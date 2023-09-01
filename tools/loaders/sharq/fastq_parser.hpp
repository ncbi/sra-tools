#ifndef __FASTQ_PRASER_HPP__
#define __FASTQ_PRASER_HPP__

/**
 * @file fastq_parser.hpp
 * @brief FASTQ reader and parser
 *
 */


/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Andrei Shkeda
*
* File Description: FASTQ reader and parser
*
* ===========================================================================
*/


#include "fastq_utils.hpp"
#include "fastq_read.hpp"
#include "fastq_error.hpp"
#include "spot_assembly.hpp"
#include "hashing.hpp"
// input streams
#include "bxzstr/bxzstr.hpp"
#include <bm/bm64.h>
#include <bm/bmdbg.h>
#include <bm/bmtimer.h>

#include <bm/bmstrsparsevec.h>
#include <bm/bmsparsevec_algo.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <zlib.h>
#include <chrono>
#include <json.hpp>
#include <set>
#include <limits>
#include "taskflow/taskflow.hpp"
#include "taskflow/algorithm/sort.hpp"
#include "fastq_defline_parser.hpp"
#include "rwqueue/readerwriterqueue.h"
#include <condition_variable>
#include <chrono>

using namespace std::chrono_literals;



using namespace std;
using namespace moodycamel;

bm::chrono_taker<>::duration_map_type timing_map;

typedef bm::bvector<> bvector_type;
typedef bm::str_sparse_vector<char, bvector_type, 32> str_sv_type;
using json = nlohmann::json;


atomic<bool> pipeline_cancelled{false};

#define _PARALLEL_READ_
#define _PARALLEL_READ1_
#define _PARALLEL_PARSE_

static constexpr int READ_QUEUE_SIZE = 5 * 1024;
static constexpr int VALIDATE_QUEUE_SIZE = 1024;

static constexpr int ASSEMBLE_QUEUE_SIZE = 2 * 1024;
static constexpr int SAVE_SPOT_QUEUE_SIZE = 1 * 1024;
static constexpr int CLEAR_SPOT_QUEUE_SIZE = 1 * 1024;

//#define PRINT_QUEUE_T_STATS

// ReaderWriterQueue wrapper
template<typename T, int QUEUE_SIZE = 1024>
struct queue_t {
    string m_name;
    atomic<bool> is_done{false};
    atomic<bool>& is_cancelled;
    bool finished{false};
    //ReaderWriterQueue<T> queue{QUEUE_SIZE};
    BlockingReaderWriterQueue<T> queue{QUEUE_SIZE};
    
    size_t enqueue_count{0};
    size_t dequeue_count{0};

#ifdef PRINT_QUEUE_T_STATS    
    std::chrono::duration<double> enqueue_idle_time;
    spdlog::stopwatch enqueue_sw;
    std::chrono::duration<double> dequeue_idle_time;
    spdlog::stopwatch dequeue_sw;

    spdlog::stopwatch queue_sw;

    size_t enqueue_idle_count{0};
    size_t dequeue_idle_count{0};
#endif    
    queue_t(const string& name, atomic<bool>& is_cancel) 
        : m_name(name), is_cancelled(is_cancel)
    {
#ifdef PRINT_QUEUE_T_STATS    
        queue_sw.reset();
#endif        
    }
#ifdef PRINT_QUEUE_T_STATS
    ~queue_t() {
        //if (pipeline_cancelled == false && enqueue_count != dequeue_count)
        //    throw fastq_error("Queue {} count mismatch {} != {}", m_name, enqueue_count, dequeue_count);
        auto logger = spdlog::get("parser_logger");
        if (logger) {
            auto tm_count = queue_sw.elapsed().count();
            logger->info("Queue {}, time: {:.2f}", m_name, queue_sw);
            double v = enqueue_idle_time.count();
            v = v/tm_count;
            logger->info("enqueue_idle_time: {:.2f}, pct:{:.2f}, enqueue_idle_count: {:L}", enqueue_idle_time.count(), v, enqueue_idle_count);
            v = dequeue_idle_time.count();
            v = v/tm_count;
            logger->info("dequeue_idle_time: {:.2f}, pct:{:.2f}, dequeue_idle_count: {:L}", dequeue_idle_time.count(), v, dequeue_idle_count);
            logger->info("--------------------");
        }
    }
#endif    

    inline
    void enqueue(T&& item) {

#ifdef PRINT_QUEUE_T_STATS    
        auto i = enqueue_idle_count;    
        enqueue_sw.reset();
#endif        
        do {
            if (queue.try_enqueue(std::move(item))) {
#ifdef PRINT_QUEUE_T_STATS    
                if (enqueue_idle_count > i) {
                    enqueue_idle_time += enqueue_sw.elapsed();
                }
#endif                
                ++enqueue_count;
                break;
            }
#ifdef PRINT_QUEUE_T_STATS    
            ++enqueue_idle_count;
#endif            
            std::this_thread::yield();
        } while (is_cancelled == false);
    }
    inline
    bool dequeue(T& item) {
        if (finished)
            return false;
#ifdef PRINT_QUEUE_T_STATS    
        auto i = dequeue_idle_count;    
        dequeue_sw.reset();
#endif        
        do {
            if (queue.wait_dequeue_timed(item, std::chrono::milliseconds(100))) {
            //if (queue.try_dequeue(item)) {
#ifdef PRINT_QUEUE_T_STATS    
                if (dequeue_idle_count > i) {
                    dequeue_idle_time += dequeue_sw.elapsed();
                }
#endif                
                ++dequeue_count;
                return true;
            }
#ifdef PRINT_QUEUE_T_STATS    
            ++dequeue_idle_count;
#endif            
            if (is_done == true && queue.peek() == nullptr) {
                finished = true;
                break;
            }
            std::this_thread::yield();                
        } while (is_cancelled == false);
        return false;
    }

    void close() {
        is_done = true;
    }

};

#define BEGIN_MT_EXCEPTION std::exception_ptr ex_ptr = nullptr; try { 

#define END_MT_EXCEPTION } catch (exception& e) { pipeline_cancelled = true; ex_ptr = current_exception(); } return ex_ptr;

/**
 * @brief Validator types
 *
 */
enum EScoreValidator {
    eNoValidator,
    eNumeric,
    ePhred} ;

template <EScoreValidator validator_type = eNoValidator, int min_score_ = 0, int max_score_ = 0>
struct validator_options
{
    static constexpr EScoreValidator type() { return validator_type; }          ///< Flag for compact mode: counts of unique tax_id sets
    static constexpr int min_score() { return min_score_; }   ///< Flag for presence of counts
    static constexpr int max_score() { return max_score_; }   ///< Flag for presence of counts
};

// input telemetry metrics 
typedef struct {
    size_t defline_len = 0;
    size_t sequence_len = 0;
    size_t quality_len = 0;
    size_t rejected_read_count = 0;
    size_t duplicate_reads_count = 0;
    size_t duplicate_reads_len = 0;
    size_t subsequence_reads_count = 0;
    size_t subsequence_reads_len = 0;
    size_t qual_scores_added = 0;
    size_t qual_scores_removed = 0;
    array<size_t, 256> base_counts{};
    array<size_t, 256> quality_counts{};
} data_input_metrics_t;

// output telemetry metrics 
typedef struct {

    size_t sequence_len = 0;
    size_t sequence_len_bio = 0;
    size_t quality_len = 0;
    array<size_t, 256> base_counts{};
    array<size_t, 256> tech_base_counts{};
    array<size_t, 256> quality_counts{};


    size_t read_count = 0;
    size_t spot_count = 0;
} data_output_metrics_t;

class fastq_reader
/// FASTQ reader
{
public:

    /**
     * @brief Construct a new fastq reader object
     *
     * @param[in] file_name File name
     * @param[in] _stream opened input stream to read from
     * @param[in] read_type expected read type ('T', 'B', 'A')
     * @param[in] platform expected platform code
     * @param[in] match_all if True no failure on unsupported deflines
     */
    fastq_reader(const string& file_name, shared_ptr<istream> _stream, vector<char> read_type = {'B'}, int platform = 0, bool match_all = false)
        : m_file_name(file_name)
        , m_stream(_stream)
        , m_read_type(read_type)
        , m_read_type_sz(m_read_type.size())
        , m_curr_platform(platform)
        //, m_queue_finished{false}
    {
        m_stream->exceptions(std::ifstream::badbit);
        if (match_all)
            m_defline_parser.SetMatchAll();
    }

    fastq_reader(const fastq_reader& other) :
            m_defline_parser(other.m_defline_parser),
            m_file_name(other.m_file_name),
            m_stream(other.m_stream),
            m_read_type(other.m_read_type),
            m_read_type_sz(other.m_read_type_sz),
            m_curr_platform(other.m_curr_platform)
        {}


    ~fastq_reader() = default;

    /**
     * @brief Returns defline's platform code
     *
     * @return uint8_t
     */
    uint8_t platform() const { return m_platform;}

    /**
     * @brief Reads one read from the stream
     *
     * @param[in,out] read
     * @return true if read successfully parsed
     * @return false if eof reached
     *
     * @throws fastq_error on parsing failures
     */
    template<typename ScoreValidator = validator_options<>>
    bool parse_read(CFastqRead& read);

    /**
     * @brief basic read validation, throws fastq_error on invalid read
     *
     * @param read
     */
    template<typename ScoreValidator = validator_options<>>
    void validate_read(CFastqRead& read);

    /**
     * @brief Parses and validates read
     *
     * @param[in,out] read
     * @return true if read successfully parsed
     * @return false if eof reached
     *
     * @throws fastq_error on parsing failures
     */
    template<typename ScoreValidator = validator_options<>>
    bool get_read(CFastqRead& read);
    // multi-threaded version of get_read
    template<typename ScoreValidator = validator_options<>>
    bool get_read_mt(CFastqRead& read);


    /**
     * @brief Retrieves next spot in the stream
     *
     * @param[out] spot name of the next spot
     * @param[out] reads vector of reads belonging to the spot
     * @return false if readers reaches EOF
     */
    template<typename ScoreValidator = validator_options<>>
    bool get_next_spot(string& spot_name, vector<CFastqRead>& reads);

    template<typename ScoreValidator = validator_options<>>
    bool get_next_spot_mt(string& spot_name, vector<CFastqRead>& reads);

    /**
     * @brief Searches the reads for the spot
     *
     * search depth is just one spot
     * so the reads have to be either next or one spot down the stream
     *
     * @param[in] spot name
     * @param[out] reads vector of reads for the spot
     * @return true if reads are found
     */
    template<typename ScoreValidator = validator_options<>>
    bool get_spot(const string& spot_name, vector<CFastqRead>& reads);
    // multi-threaded version of get_spot
    template<typename ScoreValidator = validator_options<>>
    bool get_spot_mt(const string& spot_name, vector<CFastqRead>& reads);

    bool eof() const { return m_stream->eof();}  ///< Returns true if file is  at eof 
    // multi-threaded version of eof
    bool eof_mt() const { return m_read_queue->is_done && m_read_queue->queue.peek() == nullptr;}  ///< Returns true if file is at eof

    bool end_of_data() const { return m_buffered_spot.empty() && m_pending_spot.empty() && eof();}  ///< Returns true if file has no more reads
    // multi-threaded version of end_of_data      
    bool end_of_data_mt() const { return m_buffered_spot.empty() && m_pending_spot.empty() && eof_mt();}  ///< Returns true if file has no more reads

    size_t line_number() const { return m_line_number; } ///< Returns current line number (1-based)
    const string& file_name() const { return m_file_name; }  ///< Returns reader's file name
    const string& defline_type() const { return m_defline_parser.GetDeflineType(); }  ///< Returns current defline name

    /**
     * @brief returns True is stream is compressed
     *
     */
    bool is_compressed() const {
        auto fstream = dynamic_cast<bxz::ifstream*>(&*m_stream);
        return fstream ? fstream->compression() != bxz::plaintext : false;
    }

    /**
     * @brief returns current stream position
     *
     * @return size_t
     */
    size_t tellg() const {
        auto fstream = dynamic_cast<bxz::ifstream*>(&*m_stream);
        return fstream ? fstream->compressed_tellg() : m_stream->tellg();
    }

    const set<string>& AllDeflineTypes() const { return m_defline_parser.AllDeflineTypes(); } ///< retruns set of defline types processed by the reader

    /**
     * @brief cluster list of files into groups by common spot name
     *
     * @param[in] files list of input files
     * @param[out] batches  batches of files grouped by common top spot
     */
    static void cluster_files(const vector<string>& files, vector<vector<string>>& batches);

    template<typename ScoreValidator>
    void num_qual_validator(CFastqRead& read);   ///< Numeric quality score validatot

    template<typename ScoreValidator>
    void char_qual_validator(CFastqRead& read);  ///< Character (PHRED) quality score validator

    void set_error_handler(std::function<void(fastq_error&)> handler) { m_error_handler = handler; } ///< Sets error handler    
    function<void(fastq_error&)> m_error_handler; ///< Error handler

    // start reading in mt mode
    template<typename ScoreValidator>
    void start_reading();

    // mt mode waiting for the queue to finish
    void wait();

    // process mt errors
    void end_reading();


    data_input_metrics_t m_input_metrics;

private:
    CDefLineParser      m_defline_parser;       ///< Defline parser
    string              m_file_name;            ///< Corresponding file name
    shared_ptr<istream> m_stream;               ///< reader's stream
    vector<char>        m_read_type;            ///< Reader's readType (T|B|A), A - illumina, set based on read length
    size_t              m_line_number = 0;      ///< Line number counter (1-based)
    string              m_buffered_defline;     ///< Defline already read from the stream but not placed in a read
    vector<CFastqRead>  m_buffered_spot;        ///< Spot already read from the stream but no returned to the consumer
    vector<CFastqRead>  m_pending_spot;         ///< Partial spot with the first read only
    string              m_line;                     ///< Temporary variable to hold current line
    string_view         m_line_view;                ///< Temporary variable to hold string_view to the current line
    string              m_tmp_str;                  ///< Temporary string holder
    int                 m_read_type_sz = 0;         ///< Temporary variable yto hold readtype vector size
    int                 m_curr_platform = 0;        ///< current platform
    CFastqRead          m_read;                     ///< Temporary read holder
    vector<CFastqRead>  m_next_reads;               ///< Temporary read vector
    string              m_spot;                     ///< Temporary spot holder   
    int                 m_platform = 0;             ///< Last successfull platform

    shared_ptr<queue_t<fastq_read, READ_QUEUE_SIZE>> m_read_queue; 
    future<exception_ptr> m_read_future;
    exception_ptr m_read_exception{nullptr};

    //shared_ptr<queue_t<fastq_read, VALIDATE_QUEUE_SIZE>> m_validate_queue;
    //future<exception_ptr> m_validate_future;
    //exception_ptr m_validate_exception{nullptr};

};

//  ----------------------------------------------------------------------------
static
shared_ptr<istream> s_OpenStream(const string& filename, size_t buffer_size)
{
    shared_ptr<istream> is = (filename != "-") ? shared_ptr<istream>(new bxz::ifstream(filename, ios::in, buffer_size)) : shared_ptr<istream>(new bxz::istream(std::cin));
    if (!is->good())
        throw runtime_error("Failure to open '" + filename + "'");
    return is;
}

struct spot_read_t {
    fastq_read read;
    bool is_last;
};

typedef vector<fastq_read> spot_t;

template<typename TWriter>
class fastq_parser
/// FASTQ parser: reads from a group of readers and assembles the spots
{
    //queue_t<fastq_read, 10 * 1024> save_read_queue{"save_read_queue", pipeline_cancelled};
    unique_ptr<queue_t<spot_read_t, SAVE_SPOT_QUEUE_SIZE>> save_spot_queue;
    unique_ptr<queue_t<vector<fastq_read>, ASSEMBLE_QUEUE_SIZE>> assemble_spot_queue;
    unique_ptr<queue_t<size_t, CLEAR_SPOT_QUEUE_SIZE>> clear_spot_queue;
    unique_ptr<queue_t<spot_t, ASSEMBLE_QUEUE_SIZE>> update_telemetry_queue;


public:

    /**
     * @brief Construct a new fastq parser object
     *
     * @param[in] writer
     */
    fastq_parser(shared_ptr<TWriter> writer, vector<char> read_types = {'B'}) :
        m_writer(writer),
        m_read_types(move(read_types)),
        m_read_type_sz(m_read_types.size())
    {
        m_logger = spdlog::stderr_logger_mt("parser_logger"); // send log to stderr                    
    }

    ~fastq_parser() {
        //bm::chrono_taker<>::print_duration_map(timing_map, bm::chrono_taker<>::ct_ops_per_sec);
    }

    /**
     * @brief Set up readers from prepared digest data
     *
     * @param data[in]  digest data
     * @param update_telemetry[in]  flag to update telemetry
     */
    void set_readers(json& data, bool update_telemetry = true);

    /**
     * @brief Set allow early end
     *
     * Allow parser to procceed at early end of one of the files
     *
     * @param[in]  allow_early_end
     */
    void set_allow_early_end(bool allow_early_end = true) { m_allow_early_end = allow_early_end; }

    /**
     * @brief Set the spot_file name
     *
     * Debugging/info function: when spot_file name is set
     * the list of all spot names will be serialized
     * The seirizlized format: BitMagic str_sparse_vector
     *
     * @param spot_file[in[
     */
    void set_spot_file(const string& spot_file) { m_spot_file = spot_file; }

    /**
     * @brief Set the experiment file object
     * 
     * @param experiment_file 
     */
    void set_experiment_file(const string& experiment_file);

    /**
     * @brief read from a group of readers, assembles the spot and send it to the writer
     *
     * @tparam ErrorChecker
     * @param err_checker - lambda invoked on fastq_error during read parsing
     */
    template<typename ScoreValidator, typename ErrorChecker>
    void parse(spot_name_check& name_checker, ErrorChecker&& err_checker);

    /**
     * @brief get reads from a group of readers and apply func for each read
    */
    template<typename ScoreValidator, typename ErrorChecker, typename T>
    void for_each_read(ErrorChecker&& error_checker, T&& func);

    /**
     * @brief get spots from a group of readers and apply func for each spot
    */
    template<typename ScoreValidator, typename ErrorChecker, typename T>
    void for_each_spot(ErrorChecker&& error_checker, T&& func);

    /**
     * @brief first step of spot-assembly mode
     * collect all reads from all readers and build spot-assembly 
    */
    template<typename ScoreValidator, typename ErrorChecker>
    void first_pass(ErrorChecker&& error_checker);
    /**
     * @brief second step of spot-assembly mode
     * process spots using spot-assembly
    */
    template<typename ScoreValidator, bool is_nanopore, typename ErrorChecker>
    void second_pass(ErrorChecker&& error_checker);
    
    /**
     * @brief sort/uniq spot to remove duplicate reads
    */
    void remove_duplicate_reads(const string& spot_name,vector<fastq_read>& assembled_spot);

    /**
     * @brief remove subsequences from reads
    */
    void removeSubsequences(const string& spot_name,vector<fastq_read>& reads);

    /**
     * removes duplicate reads and assigns readType
    */
    void prepare_assemble_spot(const string& spot_name, vector<fastq_read>& assembled_spot, vector<int>& read_ids);


    /**
     * @brief thread worker 
     * reads from save_spot_queue
     * if it's the last read in the spot, gets the spot and sends it to assemble_spot_queue
     * otherwise saves the read to hot/cold storage 
     * 
    */
    template<typename ScoreValidator, bool is_nanopore>
    void save_spot_thread(); 

    /**
     * @brief thread worker
     * reads from assemble_spot_queue
     * writes spot to general-loader
     * sends spot_id to clear_spot_queue
     * sends spot to update_telemetry_queue
    */
    template<typename ScoreValidator, bool is_nanopore>
    void assemble_spot_thread(); 

    /**
     * @brief thread worker
     * reads from clear_spot_queue
     * frees memory associated with the spot
    */
    template<typename ScoreValidator, bool is_nanopore>
    void clear_spot_thread(); 

    /**
     * @brief thread worker
     * reads from update_telemetry_queue and updates telemetry
    */
    void update_telemetry_thread();

    /**
     * @brief thread worker
     * works in non spot-assembly mode
     * reads from assemble_spot_queue, writes spot to general-loader
     * and sends spot  to update_telemetry_queue
     * 
    */
    void write_spot_thread(); 


    /* structure for collation check*/
    typedef struct {
        string spot_name;
        size_t line_no;
        size_t reader_idx;
    } search_term_t;

    /**
     * @brief Collation check
     *
     * Check if more than one name exists in the collected m_spot_names dictionary
     *
     */

    template<typename ErrorChecker>
    void check_duplicate_spot_names(const vector<search_term_t>& terms, ErrorChecker&& error_checker);


    /**
     * @brief Update telemetry for the readers
    */
    template<typename ScoreValidator>
    void update_readers_telemetry();

    /**
     * @brief Reports collected telemetry via json
     *
     * @param j[out]
     */
    void report_telemetry(json& j);

    /**
     * @brief Set threshold for hot/cold reads
     * default: 10000000
    */
    void set_hot_reads_threshold(size_t threshold) { 
        m_spot_assembly.m_hot_reads_threshold = threshold; 
    }

private:

    /**
     * @brief Data structure to collect runtime statistics for each file group
     *
     */
    typedef struct {
        vector<string> files;
        set<string> defline_types;
        bool is_10x = false;
        bool is_interleaved = false;
        bool has_read_names = false;
        bool is_early_end = false;
        size_t number_of_spots = 0;
        size_t number_of_reads = 0;
        size_t rejected_spots = 0;
        int    reads_per_spot = 0;
        size_t number_of_spots_with_orphans = 0;
        size_t max_sequence_size = 0;
        size_t min_sequence_size = numeric_limits<size_t>::max();
    } group_telemetry;

    typedef struct {
        size_t number_of_far_reads = 0;
        map<int, size_t> reads_stats; // number of reads, number of spots
    } spot_assembly_metrics_t;

    /**
     * @brief Data structure to collect runtime statistics for the whole run
     *
     */
    struct m_telemetry
    {
        int platform_code = 0;
        int quality_code = 0;
        double parsing_time = 0;
        double collation_check_time = 0;
        vector<group_telemetry> groups; ///< telemetry per each group
        data_input_metrics_t input_metrics;
        data_output_metrics_t output_metrics;
        spot_assembly_metrics_t assembly_metrics;
    } m_telemetry;

    /**
     * @brief Populate telemetry from digest json
     *
     * @param[in]  group
     */
    void set_telemetry(const json& group);


    /**
     * @brief Update telemetry for the assembled spot
     *
     * @param[in] reads assembled spot
     */
    void update_telemetry(const spot_t& spot);

    shared_ptr<TWriter>  m_writer;                     ///< FASTQ writer
    vector<fastq_reader> m_readers;                    ///< List of readers
    bool                 m_IsIllumina10x{false};       ///< Parsing Illumina 10x data
    str_sv_type          m_spot_names;                 ///< Run-time collected spot name dictionary
    bool                 m_allow_early_end{false};     ///< Allow early file end flag
    string               m_spot_file;                  ///< Optional file name for spot_name dictionary
    bool                 m_sort_by_readnum{false};          ///< sort reads based on number of readers and existence of read numbers
    str_sv_type::back_insert_iterator m_spot_names_bi; ///< Internal back_inserter for spot_names collection
    vector<char>         m_read_types;                 ///< ReadTypes 
    int                  m_read_type_sz{0};            ///< ReadTypes size

    spot_assembly_t m_spot_assembly;
    std::shared_ptr<spdlog::logger> m_logger;
};


static
void s_trim(string_view& in)
{
    size_t sz = in.size();
    if (sz == 0)
        return;
    size_t pos = 0;
    auto str_data = in.data();
    for (; pos < sz; ++pos) {
        if (!isspace(str_data[pos]))
            break;
    }
    if (pos) {
        in.remove_prefix(pos);
        sz -= pos;
    }
    pos = --sz;
    for (; pos && isspace(str_data[pos]); --pos);
    if (sz - pos)
        in.remove_suffix(sz - pos);
}

#define GET_LINE(stream, line, str, count) {\
line.clear(); \
if (getline(stream, line).eof()) { \
    str = line;\
    if (!line.empty()) {\
        ++count;\
        s_trim(str); \
    } \
} else { \
    ++count;\
    s_trim(str = line); \
}\
}\

//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
bool fastq_reader::parse_read(CFastqRead& read)
{
    if (m_stream->eof())
        return false;
    read.Reset();
    if (!m_buffered_defline.empty()) {
        m_line.clear();
        swap(m_line, m_buffered_defline);
        m_line_view = m_line;
    } else {
        GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
        // skip empty lines
        while (m_line_view.empty()) {
            if (m_stream->eof())
                return false;
            GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
        }
    }

    // defline
    read.SetLineNumber(m_line_number);
    m_input_metrics.defline_len += m_line_view.size();
    m_defline_parser.Parse(m_line_view, read); // may throw

    // sequence
    GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
    while (!m_line_view.empty() && m_line_view[0] != '+') {
        if (m_line_view[0] == '@' || m_line_view[0] == '>') {
            // defline is expected to start with '@' or '>'
            // if it is not, we skip it
            m_buffered_defline = m_line;
            break;
        }
        m_input_metrics.sequence_len += m_line_view.size();
        read.AddSequenceLine(m_line_view);
        GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
    }

    if (!m_line_view.empty() && m_line_view[0] == '+') { // quality score defline
        // quality score defline is expected to start with '+'
        // we skip it
        GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
        if (!m_line_view.empty()) {
            size_t sequence_size = read.Sequence().size();
            if constexpr (ScoreValidator::type() == eNumeric) {
                sequence_size *= 4;
            }
            do {
                // attempt to detect a missing quality score
                if (m_line_view[0] == '@' && m_line_view.size() != sequence_size && m_defline_parser.MatchLast(m_line_view)) {
                    m_buffered_defline = m_line;
                    break;
                }
                m_input_metrics.quality_len += m_line_view.size();
                read.AddQualityLine(m_line_view);
                if (read.Quality().size() >= sequence_size)
                    break;
                GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
                if (m_line_view.empty())
                    break;
            } while (true);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
bool fastq_reader::get_read(CFastqRead& read)
{
    try {
        if (!parse_read<ScoreValidator>(read))
            return false;
        validate_read<ScoreValidator>(read);
        m_platform = m_defline_parser.GetPlatform();
    } catch (fastq_error& e) {
        ++m_input_metrics.rejected_read_count;
        e.set_file(m_file_name, read.LineNumber());
        if (m_error_handler) {
            m_error_handler(e);
        } else {
            throw;
        }
        return false;
    }

    return true;
}

template<typename ScoreValidator>
void fastq_reader::validate_read(CFastqRead& read)
{
    if (read.Sequence().empty())
        throw fastq_error(110, "Read {}: no sequence data", read.Spot());
    if (read.Quality().empty() && !eof())
        throw fastq_error(111, "Read {}: no quality scores", read.Spot());
    // check isalpha
    if (std::any_of(read.Sequence().begin(), read.Sequence().end(), [](const char& c) { return !isalpha(c);}))
        throw fastq_error(160, "Read {}: invalid sequence characters", read.Spot());

    if constexpr (ScoreValidator::type() == eNumeric) {
        num_qual_validator<ScoreValidator>(read);
    } else if constexpr (ScoreValidator::type() == ePhred) {
        char_qual_validator<ScoreValidator>(read);
    }

    for (const auto& c : read.Sequence()) 
        ++m_input_metrics.base_counts[c];

}

//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
bool fastq_reader::get_next_spot(string& spot_name, vector<CFastqRead>& reads)
{
    reads.clear();
    if (!m_buffered_spot.empty()) {
        reads.swap(m_buffered_spot);
        spot_name = reads.front().Spot();
        return true;
    }
    m_read.Reset();
    if (m_pending_spot.empty()) {
        if (!get_read<ScoreValidator>(m_read))
            return false;
        spot_name = m_read.Spot();
        reads.push_back(move(m_read));
    } else {
        reads.swap(m_pending_spot);
        spot_name = reads.front().Spot();
    }
    while (get_read<ScoreValidator>(m_read)) {
        if (m_read.Spot() == spot_name) {
            reads.push_back(move(m_read));
            continue;
        }
        m_pending_spot.push_back(move(m_read));
        break;
    }

    if (reads.empty())
        return false;
    // assign readTypes
    if (m_read_type_sz > 0) {
        if (m_read_type_sz < (int)reads.size())
            throw fastq_error(30, "readTypes number should match the number of reads {} != {}, Spot: '{}'", m_read_type.size(), reads.size(), spot_name);
        int i = -1;
        for (auto& read : reads) {
            read.SetType(m_read_type[++i]);
        }
    }
    return true;
}


template<typename ScoreValidator>
bool fastq_reader::get_next_spot_mt(string& spot_name, vector<CFastqRead>& reads)
{
    reads.clear();
    if (!m_buffered_spot.empty()) {
        reads.swap(m_buffered_spot);
        spot_name = reads.front().Spot();
        return true;
    }
    m_read.Reset();
    if (m_pending_spot.empty()) {
        if (!get_read_mt<ScoreValidator>(m_read))
            return false;
        spot_name = m_read.Spot();
        reads.push_back(move(m_read));
    } else {
        reads.swap(m_pending_spot);
        spot_name = reads.front().Spot();
    }
    while (get_read_mt<ScoreValidator>(m_read)) {
        if (m_read.Spot() == spot_name) {
            reads.push_back(move(m_read));
            continue;
        }
        m_pending_spot.push_back(move(m_read));
        break;
    }

    if (reads.empty())
        return false;
    // assign readTypes
    if (m_read_type_sz > 0) {
        if (m_read_type_sz < (int)reads.size())
            throw fastq_error(30, "readTypes number should match the number of reads {} != {}, Spot: '{}'", m_read_type.size(), reads.size(), spot_name);
        int i = -1;
        for (auto& read : reads) {
            read.SetType(m_read_type[++i]);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
bool fastq_reader::get_spot(const string& spot_name, vector<CFastqRead>& reads)
{

    m_next_reads.clear();
    if (!get_next_spot<ScoreValidator>(m_spot, m_next_reads))
        return false;
    if (m_spot == spot_name) {
        reads = move(m_next_reads);
        return true;
    }
    if (!m_pending_spot.empty() && m_pending_spot.front().Spot() == spot_name) {
        get_next_spot<ScoreValidator>(m_spot, reads);
        assert(!reads.empty() && reads.front().Spot() == spot_name);
        m_buffered_spot = move(m_next_reads);
        return true;
    } 
    m_buffered_spot = move(m_next_reads);
    return false;
}


template<typename ScoreValidator>
bool fastq_reader::get_spot_mt(const string& spot_name, vector<CFastqRead>& reads)
{

    m_next_reads.clear();
    if (!get_next_spot_mt<ScoreValidator>(m_spot, m_next_reads))
        return false;
    if (m_spot == spot_name) {
        reads = move(m_next_reads);
        return true;
    }
    if (!m_pending_spot.empty() && m_pending_spot.front().Spot() == spot_name) {
        get_next_spot_mt<ScoreValidator>(m_spot, reads);
        assert(!reads.empty() && reads.front().Spot() == spot_name);
        m_buffered_spot = move(m_next_reads);
        return true;
    } 
    m_buffered_spot = move(m_next_reads);
    return false;
}


//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
void fastq_reader::num_qual_validator(CFastqRead& read)
{
    m_tmp_str.clear();
    read.mQualScores.clear();
    uint8_t score;
    for (auto c : read.mQuality) {
        if (isspace(c)) {
            if (!m_tmp_str.empty()) {
                try {
                    score = stoi(m_tmp_str);
                } catch (invalid_argument&) {
                    throw fastq_error(120, "Read {}: invalid quality score value", read.Spot());
                }
                if (!(score >= ScoreValidator::min_score() && score <= ScoreValidator::max_score()))
                    throw fastq_error(120, "Read {}: unexpected quality score value '{}' ( valid range: [{}..{}] )",
                            read.Spot(), score, ScoreValidator::min_score(), ScoreValidator::max_score());
                read.mQualScores.push_back(score);
                m_tmp_str.clear();
            }
            continue;
        }
        m_tmp_str.append(1, c);
    }
    if (!m_tmp_str.empty()) {
        try {
            score = stoi(m_tmp_str);
        } catch (invalid_argument&) {
            throw fastq_error(120, "Read {}: invalid quality score value", read.Spot());
        }
        if (!(score >= ScoreValidator::min_score() && score <= ScoreValidator::max_score()))
            throw fastq_error(120, "Read {}: unexpected quality score value '{}' ( valid range: [{}..{}] )",
                    read.Spot(), score, ScoreValidator::min_score(), ScoreValidator::max_score());
        read.mQualScores.push_back(score);
    }
    int qual_size = read.mQualScores.size();
    int sz = read.mSequence.size();
    if (qual_size > sz) {
        // quality line too long; warn and truncate
        fastq_error e(130, "Read {}: quality score length exceeds sequence length", read.Spot());
        spdlog::warn(e.Message());
        read.mQualScores.resize( sz );
        m_input_metrics.qual_scores_removed += (qual_size - sz);

    }

    while (qual_size < sz) {
        read.mQuality += " ";
        read.mQuality += to_string(ScoreValidator::min_score() + 30);
        read.mQualScores.push_back(ScoreValidator::min_score() + 30);
        ++qual_size;
        ++m_input_metrics.qual_scores_added;

    }
    if (m_curr_platform != m_defline_parser.GetPlatform())
        throw fastq_error(70, "Input file has data from multiple platforms ({} != {})", m_curr_platform, m_defline_parser.GetPlatform());

    for (const auto& it : read.mQualScores)
        ++m_input_metrics.quality_counts[it];
}


//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
void fastq_reader::char_qual_validator(CFastqRead& read)
{
    m_tmp_str.clear();
    read.mQualScores.clear();

    auto qual_size = read.mQuality.size();
    auto sz = read.mSequence.size();
    if (qual_size > sz) {
        // quality line too long; warn and truncate
        fastq_error e(130, "Read {}: quality score length exceeds sequence length", read.Spot());
        spdlog::warn(e.Message());
        m_input_metrics.qual_scores_removed += (qual_size - sz);
        read.mQuality.resize( sz );
    }

    for (auto c : read.mQuality) {
        int score = int(c);
        if (!(score >= ScoreValidator::min_score() && score <= ScoreValidator::max_score()))
            throw fastq_error(120, "Read {}: unexpected quality score value '{}' ( valid range: [{}..{}] )",
                    read.Spot(), score, ScoreValidator::min_score(), ScoreValidator::max_score());
    }
    if (qual_size < sz) {
        if (qual_size == 0 && !eof())
            throw fastq_error(111, "Read {}: no quality scores", read.Spot());
        m_input_metrics.qual_scores_added += (sz - qual_size);
        read.mQuality.append(sz - qual_size,  char(ScoreValidator::min_score() + 30));
    }
    if (m_curr_platform != m_defline_parser.GetPlatform())
        throw fastq_error(70, "Input file has data from multiple platforms ({} != {})", m_curr_platform, m_defline_parser.GetPlatform());

    for (const auto& c : read.mQuality)
        ++m_input_metrics.quality_counts[c];
}


template<typename ScoreValidator>
void fastq_reader::start_reading()
{
    //m_validate_queue.reset(new queue_t<fastq_read, VALIDATE_QUEUE_SIZE>("validate_queue", pipeline_cancelled));
    m_read_queue.reset(new queue_t<fastq_read, READ_QUEUE_SIZE>("read_queue", pipeline_cancelled));

    //m_validate_future = executor.async([&]() {
    m_read_future = std::async(std::launch::async, [&]() {
        CFastqRead read;
        BEGIN_MT_EXCEPTION
        
        while (pipeline_cancelled == false) {
            try {
                if (!parse_read<ScoreValidator>(read)) 
                    break;
                validate_read<ScoreValidator>(read);
                m_platform = m_defline_parser.GetPlatform();

                //m_validate_queue->enqueue(move(read));
                m_read_queue->enqueue(move(read));

            } catch (fastq_error& e) {
                e.set_file(m_file_name, read.LineNumber());
                if (m_error_handler) {
                    m_error_handler(e);
                } else {
                    throw;
                }
            }
        }
        //m_validate_queue->close();
        m_read_queue->close();
        END_MT_EXCEPTION
    });
/*
    //m_validate_future = std::async(std::launch::async, [&]() {
    m_validate_future = executor.async([&]() {
        CFastqRead read;
        BEGIN_MT_EXCEPTION
        while (m_validate_queue->dequeue(read)) {
            try {
                validate_read<ScoreValidator>(read);
                m_read_queue->enqueue(move(read));
            } catch (fastq_error& e) {
                ++m_input_metrics.rejected_read_count;
                e.set_file(m_file_name, read.LineNumber());
                if (m_error_handler) {
                    m_error_handler(e);
                } else {
                    throw;
                }
            }
        }
        m_validate_queue->close();
        END_MT_EXCEPTION
    });
  */      
}
void fastq_reader::wait()
{
    //if (m_validate_future.valid())
    //    m_validate_future.wait();
    if (m_read_future.valid())
        m_read_future.wait();
}

void fastq_reader::end_reading()
{
    //auto val_eptr = m_validate_future.get();
    //if (val_eptr)
    //    rethrow_exception(val_eptr);
    //m_validate_queue.reset();

    auto read_eptr = m_read_future.get();
    if (read_eptr)
        rethrow_exception(read_eptr);
    m_read_queue.reset();        
}

template<typename ScoreValidator>
bool fastq_reader::get_read_mt(CFastqRead& read)
{
    return m_read_queue->dequeue(read);
}


BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block

//  ----------------------------------------------------------------------------
static
void serialize_vec(const string& file_name, str_sv_type& vec)
{
    //bm::chrono_taker<> tt1("Serialize acc list", 1, &timing_map);
    bm::sparse_vector_serializer<str_sv_type> serializer;
    bm::sparse_vector_serial_layout<str_sv_type> sv_lay;
    vec.optimize(TB);
    serializer.serialize(vec, sv_lay);
    ofstream ofs(file_name.c_str(), ofstream::out | ofstream::binary);
    const unsigned char* buf = sv_lay.data();
    ofs.write((char*)&buf[0], sv_lay.size());
    ofs.close();
}


/*!
 *  This function searches list of terms in vec using sparse_vector_scanner.
 *
 *  @param[in] vec  string vector to search in
 *
 *  @param[in] scanner  sparse_vector_scanner to use for the search
 *
 *  @param[in] term  list of terms to search
 *
 *  @return index of found term or -1
 *
 */

template<typename ErrorChecker>
int s_find_duplicates(str_sv_type& vec, bm::sparse_vector_scanner<str_sv_type>& scanner, const vector<string>& terms, ErrorChecker&& error_checker)
{
    auto sz = terms.size();
    if (sz == 0)
        return -1;
    bm::sparse_vector_scanner<str_sv_type>::pipeline<bm::agg_opt_only_counts> pipe(vec);
    pipe.options().batch_size = 0;
    for (const auto& term : terms)
        pipe.add(term.c_str());
    pipe.complete();
    scanner.find_eq_str(pipe); // run the search pipeline
    auto& cnt_vect = pipe.get_bv_count_vector();
    for (size_t i = 0; i < sz; ++i) {
        if (cnt_vect[i] > 1) {
            fastq_error e(170, "Collation check. Duplicate spot '{}' at index {}", terms[i], i);
            error_checker(e);
        }
    }
    return -1;
}


template<typename TWriter>
template<typename ErrorChecker>
void fastq_parser<TWriter>::check_duplicate_spot_names(const vector<search_term_t>& terms, ErrorChecker&& error_checker)
{
    auto sz = terms.size();
    if (sz == 0)
        return;
    static thread_local bm::sparse_vector_scanner<str_sv_type> scanner;

    bm::sparse_vector_scanner<str_sv_type>::pipeline<bm::agg_opt_only_counts> pipe(m_spot_names);
    pipe.options().batch_size = 0;
    for (const auto& term : terms)
        pipe.add(term.spot_name.c_str());
    pipe.complete();
    scanner.find_eq_str(pipe); // run the search pipeline
    auto& cnt_vect = pipe.get_bv_count_vector();
    for (size_t i = 0; i < sz; ++i) {
        if (cnt_vect[i] > 1) {
            fastq_error e(170, "Collation check. Duplicate spot '{}' at file {}, line {}", terms[i].spot_name, m_readers[terms[i].reader_idx].file_name(), terms[i].line_no);
            error_checker(e);
        }
    }
}


/**
 * gets top spot spot name for each reader
 * and retrieves next reads belonging to the same spot from other readers
 * to assembly the spot
 */
template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::parse(spot_name_check& name_checker, ErrorChecker&& error_checker)
{
    //size_t spotCount = 0;
    //size_t readCount = 0;
    //size_t currCount = 0;
    spdlog::stopwatch sw;
    spdlog::info("Parsing from {} files", m_readers.size());
    auto spot_names_bi = m_spot_names.get_back_inserter();
    
    assemble_spot_queue.reset(new queue_t<vector<fastq_read>, ASSEMBLE_QUEUE_SIZE>("assemble_spot_queue", pipeline_cancelled));
    update_telemetry_queue.reset(new queue_t<spot_t, ASSEMBLE_QUEUE_SIZE>("update_telemetry_queue", pipeline_cancelled));

    array<future<exception_ptr>, 2> futures;
    futures[1] = std::async(std::launch::async,[this](){ BEGIN_MT_EXCEPTION this->write_spot_thread(); END_MT_EXCEPTION });
    futures[0] = std::async(std::launch::async,[this](){ BEGIN_MT_EXCEPTION this->update_telemetry_thread(); END_MT_EXCEPTION });

    vector<search_term_t> search_terms;
    search_terms.reserve(10000);

    string spot_name;
    for_each_spot<ScoreValidator>(error_checker, [&](vector<fastq_read>& assembled_spot) {
        assert(assembled_spot.empty() == false);
        spot_name = assembled_spot.front().Spot();
        size_t line_no = assembled_spot.front().mLineNumber;
        size_t reader_idx = assembled_spot.front().m_ReaderIdx;
        assemble_spot_queue->enqueue(move(assembled_spot));
        spot_names_bi = spot_name; 
        if (name_checker.seen_before(spot_name.c_str(), spot_name.size())) {
            search_terms.emplace_back() = { move(spot_name), line_no, reader_idx };
            if (search_terms.size() == 10000) {
                spot_names_bi.flush();
                check_duplicate_spot_names(search_terms, error_checker);
                search_terms.clear();
            }
        }

    });
    assemble_spot_queue->close();

    for (auto& ft : futures) {
        auto eptr = ft.get();
        if (eptr)
            std::rethrow_exception(eptr);
    }

    if (!search_terms.empty()) {
        spot_names_bi.flush();
        check_duplicate_spot_names(search_terms, error_checker);
    }

    update_readers_telemetry<ScoreValidator>();

//    spdlog::info("spots: {:L}, reads: {:L}", spotCount, readCount);
    if (m_telemetry.groups.back().rejected_spots > 0)
        spdlog::info("rejected spots: {:L}", m_telemetry.groups.back().rejected_spots);
    spdlog::debug("parsing time: {}", sw);
}

template<typename TWriter>
template<typename ScoreValidator>
void fastq_parser<TWriter>::update_readers_telemetry()
{
    for(const auto& reader : m_readers) {
        m_telemetry.input_metrics.defline_len += reader.m_input_metrics.defline_len;
        m_telemetry.input_metrics.sequence_len += reader.m_input_metrics.sequence_len;
        m_telemetry.input_metrics.quality_len += reader.m_input_metrics.quality_len;
        m_telemetry.input_metrics.rejected_read_count += reader.m_input_metrics.rejected_read_count;
        m_telemetry.input_metrics.qual_scores_added += reader.m_input_metrics.qual_scores_added;
        m_telemetry.input_metrics.qual_scores_removed += reader.m_input_metrics.qual_scores_removed;

        for (size_t i = 0; i < reader.m_input_metrics.base_counts.size(); ++i) {
            m_telemetry.input_metrics.base_counts[i] += reader.m_input_metrics.base_counts[i];
        }
        for (size_t i = 0; i < reader.m_input_metrics.quality_counts.size(); ++i) {
            m_telemetry.input_metrics.quality_counts[i] += reader.m_input_metrics.quality_counts[i];
        }

        m_telemetry.input_metrics.rejected_read_count += reader.m_input_metrics.rejected_read_count;

        auto& group = m_telemetry.groups.back();
        group.defline_types.insert(reader.AllDeflineTypes().begin(), reader.AllDeflineTypes().end());
    }

    for (size_t i = 0; i < m_telemetry.input_metrics.base_counts.size(); ++i) {
        size_t out_counts = m_telemetry.output_metrics.base_counts[i] + m_telemetry.output_metrics.tech_base_counts[i];
        if (m_telemetry.input_metrics.base_counts[i] != out_counts) 
            throw fastq_error(230, "Input base counts mismatch for {} : input {} != parsed {}", char(i), m_telemetry.input_metrics.base_counts[i], out_counts);
    }       
    for (size_t i = 0; i < m_telemetry.input_metrics.quality_counts.size(); ++i) {
        size_t out_counts = m_telemetry.output_metrics.quality_counts[i];
        if (m_telemetry.input_metrics.quality_counts[i] != out_counts) 
            throw fastq_error(230, "Input quality counts mismatch for {} : input {} != parsed {}", char(i), m_telemetry.input_metrics.quality_counts[i], out_counts);
    }       
    // adjust quality counts for numeric scores to match run_stat format
    if constexpr (ScoreValidator::type() == eNumeric) {
        for (int i = (int)m_telemetry.output_metrics.quality_counts.size() - 1; i >= 0; --i) {
            if (m_telemetry.output_metrics.quality_counts[i] > 0) {
                m_telemetry.output_metrics.quality_counts[i + 33] = m_telemetry.output_metrics.quality_counts[i];
                m_telemetry.output_metrics.quality_counts[i] = 0;
            }
        }
    }

}

template<typename TWriter>
void fastq_parser<TWriter>::update_telemetry(const spot_t& reads)
{
    assert(reads.empty() == false);
    auto& telemetry = m_telemetry.groups.back();
    telemetry.number_of_spots += 1;
    telemetry.number_of_reads += reads.size();
    
    m_telemetry.output_metrics.read_count += reads.size();
    ++m_telemetry.output_metrics.spot_count;
    vector<uint8_t> qual_scores;
    for (const auto& r : reads) {
        auto sz = r.Sequence().size();
        m_telemetry.output_metrics.sequence_len += sz;
        if (r.mReadType != SRA_READ_TYPE_TECHNICAL) {
            m_telemetry.output_metrics.sequence_len_bio += sz;
            for (const auto& c : r.Sequence()) {
                ++m_telemetry.output_metrics.base_counts[c];
            }
        } else {
            for (const auto& c : r.Sequence()) {
                ++m_telemetry.output_metrics.tech_base_counts[c];
            }
        }
        qual_scores.clear();
        r.GetQualScores(qual_scores);
        for (const auto& c : qual_scores) {
            ++m_telemetry.output_metrics.quality_counts[c];
        }
        telemetry.max_sequence_size = max<int>(sz, telemetry.max_sequence_size);
        telemetry.min_sequence_size = min<size_t>(sz, telemetry.min_sequence_size);
        m_telemetry.output_metrics.quality_len += r.GetQualScores().size();
    }
    
    if ((int)reads.size() < telemetry.reads_per_spot)
        ++telemetry.number_of_spots_with_orphans;
}



/**
 * @brief Temporary structure to collect quality score stats
 *
 */
typedef struct qual_score_params
{
    qual_score_params() {
        min_score = int('~');
        max_score = int('!');
        intialized = false;
        space_delimited = false;
    }
    int min_score = int('~');
    int max_score = int('!');
    bool intialized = false;
    bool space_delimited = false;
    bool set_score(int score)
    {
        if (space_delimited) {
            if (score < -5 || score > 40)
                return false;
        } else if (score < 33 || score > 126) {
            return false;
        }
        min_score = min<int>(score, min_score);
        max_score = max<int>(score, max_score);
        return true;
    }

} qual_score_params;

//  ----------------------------------------------------------------------------
static
void s_check_qual_score(const CFastqRead& read, qual_score_params& params)
{
    const auto& quality = read.Quality();
    if (!params.intialized) {
        params.space_delimited = std::any_of(quality.begin(), quality.end(), [](const char c) { return isspace(c);});
        params.intialized = true;
    }

    if (params.space_delimited) {
        auto set_score = [&read, &params](string& str)
        {
            if (str.empty())
                return;
            int score;
            try {
                score = stoi(str);
            } catch (exception& e) {
                throw fastq_error(140, "Read {}: quality score contains unexpected character", read.Spot());
            }
            if (!params.set_score(score))
                throw fastq_error(140, "Read {}: quality score contains unexpected character '{}'", read.Spot(), score);
            str.clear();
        };
        string str;
        for (auto c : quality) {
            if (isspace(c)) {
                set_score(str);
                str.clear();
                continue;
            }
            str.append(1, c);
        }
        set_score(str);
    } else {
        for (auto c : quality) {
            if (!params.set_score(int(c)))
                throw fastq_error(140, "Read {}: quality score contains unexpected character '{}'", read.Spot(), int(c));
        }
    }
}

//  ----------------------------------------------------------------------------
/**
 * @brief Groups files by the first spot into batches
 * 
 * @param files 
 * @param batches 
 */
void fastq_reader::cluster_files(const vector<string>& files, vector<vector<string>>& batches)
{
    batches.clear();
    if (files.empty())
        return;
    if (files.size() == 1) {
        vector<string> v{files.front()};
        batches.push_back(move(v));
        return;
    }

    vector<CFastqRead> reads;
    vector<char> readTypes;
    vector<uint8_t> placed(files.size(), 0);

    for (size_t i = 0; i < files.size(); ++i) {
        if (placed[i]) continue;
        placed[i] = 1;
        string spot;
        auto reader1 = fastq_reader(files[i], s_OpenStream(files[i], 1024*1024), readTypes, 0, true);
        for (auto c = 0; c < 100; ++c) {
            try {
                if (reader1.get_next_spot<>(spot, reads)) 
                    break;     
            }  catch (fastq_error& e) {
                // skip errors during file clustering
            }
        }
        if (reads.empty())
            throw fastq_error(50 , "File '{}' has no reads", files[i]);
        vector<string> batch{files[i]};
        for (size_t j = 0; j < files.size(); ++j) {
            if (placed[j]) continue;
            auto reader2 = fastq_reader(files[j], s_OpenStream(files[j], 1024*1024), readTypes, 0, true);
            for (auto c = 0; c < 100; ++c) {
                try {
                    if (reader2.get_spot<>(spot, reads)) {
                        placed[j] = 1;
                        batch.push_back(files[j]);
                        break;
                    }
                }  catch (fastq_error& e) {
                    // skip errors during file clustering 
                }

            }
        }
        if (!batches.empty() && batches.front().size() != batch.size()) {
            string first_group = sharq::join(batches.front().begin(), batches.front().end());
            string second_group = sharq::join(batch.begin(), batch.end());
            throw fastq_error(11, "Inconsistent file sets: first group ({}), second group ({})", first_group, second_group);
        }
        spdlog::info("File group: {}", sharq::join(batch.begin(), batch.end()));
        batches.push_back(move(batch));
    }
}


//  ----------------------------------------------------------------------------
/**
 * @brief Debugging function to perform a collation check on previously saved spot_name file
 *
 * @param[in] hash_file
 */
void check_hash_file(const string& hash_file)
{
    str_sv_type vec;
    {
        vector<char> buffer;
        bm::sparse_vector_deserializer<str_sv_type> deserializer;
        bm::sparse_vector_serial_layout<str_sv_type> lay;
        bm::read_dump_file(hash_file, buffer);
        deserializer.deserialize(vec, (const unsigned char*)&buffer[0]);
    }


    spdlog::debug("Checking reads {:L} on {} threads", vec.size(), std::thread::hardware_concurrency());
    {
        str_sv_type::statistics st;
        vec.calc_stat(&st);
        spdlog::debug("st.memory_used {}", st.memory_used);
    }
    spdlog::stopwatch sw;

    bm::sparse_vector_scanner<str_sv_type> str_scan;
    spot_name_check name_check(vec.size());

    size_t index = 0, hits = 0, sz = 0, last_hits = 0;
    const char* value;
    bool hit;
    auto it = vec.begin();
    vector<string> search_terms;
    search_terms.reserve(10000);
    auto err_checker = [](fastq_error& e) -> void { throw e;};

    while (it.valid()) {
        value = it.value();
        sz = strlen(value);
        {
            bm::chrono_taker<> tt1(std::cout, "check hits", 1, &timing_map);
            hit = name_check.seen_before(value, sz);
        }
        if (hit) {
            ++hits;
            search_terms.push_back(value);
            if (search_terms.size() == 10000) {
                bm::chrono_taker<> tt1(std::cout, "scan", search_terms.size(), &timing_map);
                s_find_duplicates(vec, str_scan, search_terms, err_checker);
                search_terms.clear();
             }
        }
        it.advance();
        if (++index % 100000000 == 0) {
            spdlog::debug("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
            last_hits = hits;
            bm::chrono_taker<>::print_duration_map(std::cout, timing_map, bm::chrono_taker<>::ct_all);
            timing_map.clear();
        }
    }
    if (!search_terms.empty()) {
        bm::chrono_taker<> tt1(std::cout, "scan", search_terms.size(), &timing_map);
        s_find_duplicates(vec, str_scan, search_terms, err_checker);
    }

    {
        spdlog::debug("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
        bm::chrono_taker<>::print_duration_map(std::cout, timing_map, bm::chrono_taker<>::ct_all);
    }

    //string collisions_file = hash_file + ".dups";
    //bm::SaveBVector(collisions_file.c_str(), bv_collisions);
}

/**
 * @brief Generate digest data
 *
 * @param[in,out] j digest data
 * @param[in] input_batches input group pf files
 * @param[in] num_reads_to_check
 */
template<typename ErrorChecker>
void get_digest(json& j, const vector<vector<string>>& input_batches, ErrorChecker&& error_checker, int p_num_reads_to_check = 250000)
{
    assert(!input_batches.empty());
    // Run first num_reads_to_check to check for consistency
    // and setup read_types and platform if needed
    CFastqRead read;
    // 10x pattern
    re2::RE2 re_10x_I("[_|-]I\\d+[\\._]");
    assert(re_10x_I.ok());
    re2::RE2 re_10x_R("[_|-]R\\d+[\\._]");
    assert(re_10x_R.ok());
    bool has_I_file = false;
    bool has_R_file = false;
    bool has_non_10x_files = false;
    for (auto& files : input_batches) {
        json group_j;
        size_t estimated_spots = 0;
        int group_reads = 0;
        for (const auto& fn  : files) {
            json f;
            size_t reads_processed = 0;
            size_t spots_processed = 0;
            size_t spot_name_sz = 0;
            set<string> deflines_types;
            set<int> platforms;

            f["file_path"] = fn;
            auto file_size = fs::file_size(fn);
            f["file_size"] = file_size;
            if (re2::RE2::PartialMatch(fn, re_10x_I))
                has_I_file = true;
            else if (re2::RE2::PartialMatch(fn, re_10x_R))
                has_R_file = true;
            else
                has_non_10x_files = true;

            fastq_reader reader(fn, s_OpenStream(fn, 1024 * 4), {}, 0, true);
            vector<CFastqRead> reads;
            int max_reads = 0;
            bool has_orphans = false;
            string spot_name;
            set<string> read_names;
            bool has_reads = false;
            int num_reads_to_check = p_num_reads_to_check;

            while (has_reads == false && num_reads_to_check > 0) {
                try {
                    has_reads = reader.get_next_spot<>(spot_name, reads);
                    --num_reads_to_check;
                } catch (fastq_error& e) {
                    error_checker(e);
                }
            }
            if (!has_reads)
                throw fastq_error(50 , "File '{}' has no reads", fn);

            f["is_compressed"] = reader.is_compressed();
            max_reads = reads.size();
            reads_processed += reads.size();
            ++spots_processed;
            qual_score_params params;
            for (const auto& read : reads) {
                if (!read.ReadNum().empty())
                    read_names.insert(read.ReadNum());
                try {
                    s_check_qual_score(read, params);
                } catch (fastq_error& e) {
                    e.set_file(fn, read.LineNumber());
                    error_checker(e);
                }
            }
            string suffix = reads.empty() ? "" : reads.front().Suffix();
            spot_name += suffix;
            f["first_name"] = spot_name;
            spot_name_sz += spot_name.size();

            while (true) {
                auto def_it = deflines_types.insert(reader.defline_type());
                if (def_it.second)
                    f["defline_type"].push_back(reader.defline_type());
                auto platform_it = platforms.insert(reader.platform());
                if (platform_it.second)
                    f["platform_code"].push_back(reader.platform());
                if (num_reads_to_check != -1) {
                    --num_reads_to_check;
                    if (num_reads_to_check <=0)
                        break;
                }
                reads.clear();
                try {
                    if (!reader.get_next_spot<>(spot_name, reads))
                        break;
                } catch(fastq_error& e) {
                    error_checker(e);
                }
                ++spots_processed;
                reads_processed += reads.size();
                if (max_reads > (int)reads.size()) {
                    has_orphans = true;
                }
                string suffix = reads.empty() ? "" : reads.front().Suffix();
                spot_name_sz += spot_name.size() + suffix.size();
                max_reads = max<int>(max_reads, reads.size());
                for (const auto& read : reads) {
                    if (!read.ReadNum().empty())
                        read_names.insert(read.ReadNum());
                    try {
                        s_check_qual_score(read, params);
                    } catch (fastq_error& e) {
                        e.set_file(fn, read.LineNumber());
                        error_checker(e);
                    }
                }
                //if (platform != reader.platform())
                //    throw fastq_error(70, "Input files have deflines from different platforms {} != {}", platform, reader.platform());
            }
            if (params.space_delimited) {
                f["quality_encoding"] = 0;
            } else if (params.min_score >= 64 && params.max_score > 78) {
                f["quality_encoding"] = 64;
            } else if (params.min_score >= 33 /*&& params.max_score <= 78*/) {
                f["quality_encoding"] = 33;
            } else {
                fastq_error e("Invaid quality encoding (min: {}, max: {}), {}:{}", params.min_score, params.max_score, fn, read.LineNumber());
                e.set_file(fn, read.LineNumber());
                throw e;

            }

            f["readNums"] = read_names;
            group_reads += max_reads;
            f["max_reads"] = max_reads;
            f["has_orphans"] = has_orphans;
            f["reads_processed"] = reads_processed;
            f["spots_processed"] = spots_processed;
            f["lines_processed"] = reader.line_number();
            double bytes_read = reader.tellg();
            if (bytes_read && spots_processed) {
                double fsize = file_size;
                double bytes_per_spot = bytes_read/spots_processed;
                estimated_spots = max<size_t>(fsize/bytes_per_spot, estimated_spots);
                f["name_size_avg"] = spot_name_sz/spots_processed;
            }
            group_j["files"].push_back(f);
        }
        group_j["is_10x"] = group_reads >= 3 && has_I_file && has_R_file;
        if (has_non_10x_files && group_j["is_10x"])
            throw fastq_error(80);// "Inconsistent submission: 10x submissions are mixed with different types.");
        group_j["estimated_spots"] = estimated_spots;
        j["groups"].push_back(group_j);
    }
}

template<typename TWriter>
void fastq_parser<TWriter>::set_telemetry(const json& group)
{
   if (group["files"].empty())
        return;
    auto& first = group["files"].front();

    m_telemetry.platform_code = first["platform_code"].front();
    m_telemetry.quality_code = (int)first["quality_encoding"];
    m_telemetry.groups.emplace_back();
    auto& group_telemetry = m_telemetry.groups.back();
    group_telemetry.is_10x = group["is_10x"];
    int spot_reads = 0;
    for (auto& data : group["files"]) {
        const string& name = data["file_path"];
        group_telemetry.files.push_back(name);
        int max_reads = (int)data["max_reads"];
        if (max_reads > 1)
            group_telemetry.is_interleaved = true;
        spot_reads += max_reads;
       if (!data["readNums"].empty())
            group_telemetry.has_read_names = true;
    }
    group_telemetry.reads_per_spot = spot_reads;
}

static function<bool(const fastq_read& l, const fastq_read& r)> s_sort_by_readnum = 
[](const fastq_read& l, const fastq_read& r) 
{ 
    return l.ReadNum() < r.ReadNum();
};

static function<bool(const fastq_read& l, const fastq_read& r)> s_sort_by_file_order = 
[](const fastq_read& l, const fastq_read& r)
{
    if (l.m_ReaderIdx == r.m_ReaderIdx)
        return l.LineNumber() < r.LineNumber(); 
    else
        return l.m_ReaderIdx < r.m_ReaderIdx;
};

template<typename TWriter>
void fastq_parser<TWriter>::set_readers(json& group, bool update_telemetry)
{
    m_readers.clear();
    if (group["files"].empty())
        return;
    uint8_t files_with_read_numbers = 0;        
    vector<char> read_types;
    for (auto& data : group["files"]) {
        const string& name = data["file_path"];
        if (data.contains("readType"))
            read_types = data["readType"];
        else
            read_types.clear();  
        m_readers.emplace_back(name, s_OpenStream(name, (1024 * 1024) * 10), read_types, data["platform_code"].front());
        if (!data["readNums"].empty())
            ++files_with_read_numbers;
    }
    m_sort_by_readnum = m_readers.size() == 2 && files_with_read_numbers == m_readers.size();
    m_IsIllumina10x = group.contains("is_10x") && group["is_10x"];

    if (update_telemetry)
        set_telemetry(group);
}

template<typename TWriter>
void fastq_parser<TWriter>::report_telemetry(json& j)
{
    try {
        j["platform_code"] = m_telemetry.platform_code;
        j["quality_code"] = m_telemetry.quality_code;
        set<string> defline_types;

        for (const auto& gr : m_telemetry.groups) {
            j["groups"].emplace_back();
            auto& g = j["groups"].back();
            g["files"] = gr.files;
            g["is_10x"] = gr.is_10x;
            g["is_early_end"] = gr.is_early_end;
            g["number_of_spots"] = gr.number_of_spots;
            g["number_of_reads"] = gr.number_of_reads;
            g["rejected_spots"] = gr.rejected_spots;
            g["is_interleaved"] = gr.is_interleaved;
            g["has_read_names"] = gr.has_read_names;
            g["defline_type"] = gr.defline_types;
            g["reads_per_spot"] = gr.reads_per_spot;
            g["number_of_spots_with_orphans"] = gr.number_of_spots_with_orphans;
            g["max_sequence_size"] = gr.max_sequence_size;
            g["min_sequence_size"] = gr.min_sequence_size;
            defline_types.insert(gr.defline_types.begin(), gr.defline_types.end());
        }
        for (const auto d : defline_types)
            j["defline_types"].push_back(d);

        auto& im = j["i"];
        im["defline_len"] = m_telemetry.input_metrics.defline_len;
        im["sequence_len"] = m_telemetry.input_metrics.sequence_len;
        im["quality_len"] = m_telemetry.input_metrics.quality_len;

        if (m_telemetry.input_metrics.rejected_read_count)
            im["rejected_reads"] = m_telemetry.input_metrics.rejected_read_count;
        if (m_telemetry.input_metrics.duplicate_reads_count) {
            im["duplicate_reads"] = m_telemetry.input_metrics.duplicate_reads_count;
            im["duplicate_reads_len"] = m_telemetry.input_metrics.duplicate_reads_len;
        }
        if (m_telemetry.input_metrics.subsequence_reads_count) {
            im["subsequence_reads"] = m_telemetry.input_metrics.subsequence_reads_count;
            im["subsequence_reads_len"] = m_telemetry.input_metrics.subsequence_reads_len;
        }
        if (m_telemetry.input_metrics.qual_scores_added) 
            im["qual_scores_added"] = m_telemetry.input_metrics.qual_scores_added;
        if (m_telemetry.input_metrics.qual_scores_removed) 
            im["qual_scores_removed"] = m_telemetry.input_metrics.qual_scores_removed;

        if (j.contains("is_spot_assembly")) {
            im["far_reads"] = m_telemetry.assembly_metrics.number_of_far_reads;
        }

        auto& om = j["o"];
        om["sequence_len"] = m_telemetry.output_metrics.sequence_len;
        om["sequence_len_bio"] = m_telemetry.output_metrics.sequence_len_bio;
        om["quality_len"] = m_telemetry.output_metrics.quality_len;

        auto &bc = om["base_counts"];
        for (size_t i = 0; i < m_telemetry.output_metrics.base_counts.size(); ++i) 
            if (m_telemetry.output_metrics.base_counts[i] != 0) {
                auto s = string(1, char(i));
                bc[s] = m_telemetry.output_metrics.base_counts[i];
            }

        auto &qc = om["quality_counts"];
        for (size_t i = 0; i < m_telemetry.output_metrics.quality_counts.size(); ++i) 
            if (m_telemetry.output_metrics.quality_counts[i] != 0)
                qc[to_string(i - 33)] = m_telemetry.output_metrics.quality_counts[i];

        om["read_count"] = m_telemetry.output_metrics.read_count;               
        om["spot_count"] = m_telemetry.output_metrics.spot_count;       
        auto& qm = j["qc"];
        if (m_telemetry.input_metrics.sequence_len > 0) {
            qm["sequence_loss"] = (long long int)(m_telemetry.input_metrics.sequence_len - m_telemetry.output_metrics.sequence_len);
            qm["pct_sequence_loss"] = fmt::format("{:.2f}", 100. * (1. - float(m_telemetry.output_metrics.sequence_len)/m_telemetry.input_metrics.sequence_len));
        }
        if (m_telemetry.input_metrics.quality_len > 0) {
            qm["quality_loss"] = (long long int)(m_telemetry.input_metrics.quality_len - m_telemetry.output_metrics.quality_len);
            qm["pct_quality_loss"] = fmt::format("{:.2f}", 100. * (1. - float(m_telemetry.output_metrics.quality_len)/m_telemetry.input_metrics.quality_len));
        }
    } catch (exception& e) {
        spdlog::error("Error reporting telemetry: {}", e.what());
    }
}

template<typename TWriter>
void set_experiment_file(const string& experiment_file);


template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker, typename T>
void fastq_parser<TWriter>::for_each_read(ErrorChecker&& error_checker, T&& func)
{
    int num_readers = m_readers.size();

    try {
        for (int i = 0; i < num_readers; ++i) {
            m_readers[i].set_error_handler(error_checker);
    #ifdef _PARALLEL_READ_
            m_readers[i].template start_reading<ScoreValidator>();
    #endif        
        }

        bool has_spots;
        bool check_readers = false;
        int early_end = 0;
        CFastqRead read;
        size_t row_id = 0;

        do {
            has_spots = false;
            for (int i = 0; i < num_readers; ++i)
            try {
    #ifdef _PARALLEL_READ_
                if (!m_readers[i] . template get_read_mt<ScoreValidator>(read)) {
    #else                
                if (!m_readers[i] . template get_read<ScoreValidator>(read)) {
    #endif                
                    check_readers = ++early_end > 4 && num_readers > 1 && m_allow_early_end == false;
                    continue;
                }
                has_spots = true;
                read.m_ReaderIdx = i;
                func(row_id, read);
                ++row_id;
            } catch (fastq_error& e) {
                error_checker(e);
                //has_spots = true;  /// we don't want interrupt too early in case of an exception
                continue;
            }
            
//            if (row_id % 1000 == 0)  
//                m_logger->info("Reads: {:L}", row_id);

            if (has_spots == false)
                break;

            if (check_readers) {
                int readers_at_eof = 0;
                for (int i = 0; i < num_readers; ++i) {
    #ifdef _PARALLEL_READ_
                    if (m_readers[i].eof_mt())
    #else                
                    if (m_readers[i].eof())
    #endif                
                        ++readers_at_eof;
                }
                if ((int)readers_at_eof == num_readers) // all readers are at EOF
                    break;
                if (readers_at_eof > 0 && (int)readers_at_eof < num_readers) {
                    for (int i = 0; i < num_readers; ++i) {
    #ifdef _PARALLEL_READ_
                        if (m_readers[i].eof_mt())
    #else
                        if (m_readers[i].eof())
    #endif                    
                            throw fastq_error(180, "{} ended early at line {}. Use '--allowEarlyFileEnd' to allow load to finish.",
                                    m_readers[i].file_name(), m_readers[i].line_number());
                    }
                }
            }

    #ifdef _PARALLEL_READ_
        } while (pipeline_cancelled == false);
        for (int i = 0; i < num_readers; ++i) {
            m_readers[i].end_reading();
        }
    #else    
        } while (true);
    #endif    
    } catch (exception& e) {
        pipeline_cancelled = true;
#ifdef _PARALLEL_READ_
        for (int i = 0; i < num_readers; ++i) {
            m_readers[i].wait();
        }
#endif 
        throw;
    }
}

template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker, typename T>
void fastq_parser<TWriter>::for_each_spot(ErrorChecker&& error_checker, T&& func)
{
    int num_readers = m_readers.size();
    try {
        for (int i = 0; i < num_readers; ++i) {
            m_readers[i].set_error_handler(error_checker);
    #ifdef _PARALLEL_READ1_
            m_readers[i].template start_reading<ScoreValidator>();
    #endif        
        }

        bool has_spots;
        string spot;
        bool check_readers = false;
        CFastqRead read;

        vector<vector<CFastqRead>> spot_reads(num_readers);
        vector<CFastqRead> assembled_spot;


        do {
            has_spots = false;
            for (int i = 0; i < num_readers; ++i)
            try {
    #ifdef _PARALLEL_READ1_
                if (!m_readers[i] . template get_next_spot_mt<ScoreValidator>(spot, spot_reads[i])) {
                    if (m_readers[i].end_of_data_mt()) {
    #else                
                if (!m_readers[i] . template get_next_spot<ScoreValidator>(spot, spot_reads[i])) {
                    if (m_readers[i].end_of_data()) {
    #endif                    
                        check_readers = num_readers > 1 && m_allow_early_end == false;
                        continue;
                    }
                    has_spots = true;
                    if (spot_reads[i].empty()) {
                        ++m_telemetry.groups.back().rejected_spots;
                        continue; 
                    }
                }
                has_spots = true;
                for (int j = 0; j < num_readers; ++j) {
                    if (j == i) continue;
    #ifdef _PARALLEL_READ1_
                    m_readers[j] . template get_spot_mt<ScoreValidator>(spot, spot_reads[j]);
    #else                
                    m_readers[j] . template get_spot<ScoreValidator>(spot, spot_reads[j]);                    
    #endif                    
                }
                assembled_spot.clear();
                for (auto& r : spot_reads) {
                    if (!r.empty()) {
                        move(r.begin(), r.end(), back_inserter(assembled_spot));
                        r.clear();
                    }
                }
                assert(assembled_spot.empty() == false);
                if (assembled_spot.size() > 4) 
                    throw fastq_error(210, "Spot {} has more than 4 reads", assembled_spot.front().Spot());
                func(assembled_spot);
            } catch (fastq_error& e) {
                error_checker(e);
                ++m_telemetry.groups.back().rejected_spots;
                continue;
            }
            
            if (has_spots == false)
                break;

            if (check_readers) {
                // All readers must be at eof unless we allow early file end
                int readers_at_eof = 0;
                for (int i = 0; i < num_readers; ++i) {
    #ifdef _PARALLEL_READ1_
                    if (m_readers[i].eof_mt())
    #else                
                    if (m_readers[i].eof())
    #endif                
                        ++readers_at_eof;
                }
                if (readers_at_eof > 0 && (int)readers_at_eof < num_readers) {
                    for (int i = 0; i < num_readers; ++i) {
    #ifdef _PARALLEL_READ1_
                    if (m_readers[i].eof_mt())
    #else                
                        if (m_readers[i].eof())
    #endif                    
                            throw fastq_error(180, "{} ended early at line {}. Use '--allowEarlyFileEnd' to allow load to finish.",
                                    m_readers[i].file_name(), m_readers[i].line_number());
                    }
                }
            }
        } while (pipeline_cancelled == false);
#ifdef _PARALLEL_READ1_
        for (int i = 0; i < num_readers; ++i) {
            m_readers[i].end_reading();
        }
#endif    
    } catch (exception& e) {
        pipeline_cancelled = true;
#ifdef _PARALLEL_READ1_
        for (int i = 0; i < num_readers; ++i) {
            m_readers[i].wait();
        }
#endif
        throw;
    }
}


/**
 * gets top spot spot name for each reader
 * and retrieves next reads belonging to the same spot from other readers
 * to assembly the spot
 */
template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::first_pass(ErrorChecker&& error_checker)
{
    spdlog::stopwatch sw;
    spdlog::info("Parsing from {} files", m_readers.size());

    str_sv_type read_names;
    auto read_names_bi = read_names.get_back_inserter();
    for_each_read<ScoreValidator, ErrorChecker>(error_checker, [&](size_t row_id, CFastqRead& read) {
        read_names_bi = read.Spot();
    });
    read_names_bi.flush();
    spdlog::info("reading took: {}, {:L} reads", sw, read_names.size());
    sw.reset();
    read_names.remap();
    str_sv_type::statistics stats;
    read_names.optimize(TB, bm::bvector<>::opt_compress, &stats);
    read_names.freeze();
    spdlog::info("Remapping took {:.3}", sw);       
    sw.reset();
    auto num_rows = read_names.size();
    spdlog::info("num_row: {:L}, read_names mem: {:L}, sort_index mem: {:L}, total_mem: {:L}", num_rows, stats.memory_used, num_rows * sizeof(uint32_t), stats.memory_used + (num_rows * 2 * sizeof(uint32_t)));       

    if (num_rows > numeric_limits<uint32_t>::max())
        throw fastq_error("Number of reads {:L} exceeds the limit {:L}", num_rows, numeric_limits<uint32_t>::max());
    m_spot_assembly.assign_spot_id(read_names);
    m_telemetry.assembly_metrics.number_of_far_reads = m_spot_assembly.m_total_spots - m_spot_assembly.m_hot_spot_ids.count();
    spdlog::info("Building took {:.3}, far reads: {:L}", sw, m_telemetry.assembly_metrics.number_of_far_reads);

    int max_reads = 0;
    for (auto& it : m_spot_assembly.m_reads_counts) {
        spdlog::info("{:L} spots with {} reads", it.second, it.first);
        m_telemetry.assembly_metrics.reads_stats = m_spot_assembly.m_reads_counts;
        max_reads = max(max_reads, (int)it.first);
    }
    if (m_read_types.empty()) {
        if (m_IsIllumina10x)
            m_read_types.resize(min(max_reads, 4), 'A');
        else
            m_read_types.resize(min(max_reads, 2), 'B');
    }
    m_read_type_sz = m_read_types.size();
}


template<typename TWriter>
template<typename ScoreValidator, bool is_nanopore>
void fastq_parser<TWriter>::save_spot_thread() 
{
    // get last spot read from save_spot_queue 
    // retrieve other reads from this spot 
    // send the assembled spot assemble_spot_queue 
    spot_read_t spot_read;
    vector<fastq_read> spot;
    vector<int> read_ids;
    size_t spot_id = 0;
    string spot_name;
    //vector<uint8_t> qs1, qs2;
    while (save_spot_queue->dequeue(spot_read)) {
        auto& read = spot_read.read;
        assert(read.m_SpotId != 0);
        if (spot_read.is_last) {
            spot_name = read.Spot();
            spot_id = read.m_SpotId;
            m_spot_assembly. template get_spot_mt<ScoreValidator, is_nanopore>(spot_name, read.m_SpotId, spot);
            spot.push_back(move(read));
            prepare_assemble_spot(spot_name, spot, read_ids);

            assert(!spot.empty());
            spot.front().SetSpot(move(spot_name));
            spot.front().m_SpotId = spot_id;
            assemble_spot_queue->enqueue(move(spot));
        } else {
            m_spot_assembly. template save_read_mt<ScoreValidator, is_nanopore>(read.m_SpotId, read);               
        }
    }
    assemble_spot_queue->close();
}


template<typename TWriter>
void fastq_parser<TWriter>::removeSubsequences(const string& spot_name,vector<fastq_read>& reads) 
{
    int sz = reads.size();
    for(int i = 0; i < sz; i++) {
        auto seq_sz = reads[i].Sequence().size(); 
        for(int j = 0; j < sz; j++) {
            if(i != j && seq_sz < reads[j].Sequence().size() && reads[j].Sequence().find(reads[i].Sequence()) != string::npos) 
            {
                vector<uint8_t> qual_scores1;
                reads[i].GetQualScores(qual_scores1);
                vector<uint8_t> qual_scores2;
                reads[j].GetQualScores(qual_scores2);
                if (search(qual_scores2.begin(), qual_scores2.end(), qual_scores1.begin(), qual_scores1.end()) != qual_scores2.end()) {
                    ++m_telemetry.input_metrics.subsequence_reads_count;
                    //cerr << "Removing subsequence read: " << spot_name << endl << reads[i].Sequence() << endl;
                    m_telemetry.input_metrics.subsequence_reads_len += reads[i].GetSize();
                    for (const auto& c : reads[i].Sequence()) 
                        --m_telemetry.input_metrics.base_counts[c];
                    for (const auto& c : reads[i].GetQualScores()) 
                        --m_telemetry.input_metrics.quality_counts[c];

                    reads.erase(reads.begin() + i);
                    --i;
                    --sz;
                    break;
                }
            }
        }
    }
}


template<typename TWriter>
void fastq_parser<TWriter>::remove_duplicate_reads(const string& spot_name,vector<fastq_read>& assembled_spot)
{
    sort(assembled_spot.begin(), assembled_spot.end(), [](const auto& l, const auto& r) { 
        if (l.ReadNum() == r.ReadNum()) {
            int c = l.Sequence().compare(r.Sequence()); 
            if (c == 0) {
                if (l.GetQualScores() != r.GetQualScores()) 
                    c = -1;
            }        
            return c < 0;
        }
        return l.ReadNum() < r.ReadNum();
    });
   // same as std::unique but captures stat of the removed duplicates
    auto uniq = [&] (vector<fastq_read>::iterator first, vector<fastq_read>::iterator last) -> vector<fastq_read>::iterator
    {
        if (first == last)
            return last;
    
        auto result = first;
        while (++first != last)
            // is duplicate?
            if (result->ReadNum() == first->ReadNum() && result->Sequence().compare(first->Sequence()) == 0 && result->GetQualScores() == first->GetQualScores()) {
                // adjust input statistics 
                for (const auto& c : first->Sequence()) 
                    --m_telemetry.input_metrics.base_counts[c];
                for (const auto& c : first->GetQualScores()) 
                    --m_telemetry.input_metrics.quality_counts[c];
                m_telemetry.input_metrics.duplicate_reads_len += first->GetSize();

            } else if (++result != first) {
                *result = std::move(*first);
            }
        return ++result;
    };

    auto new_end = uniq(assembled_spot.begin(), assembled_spot.end());

    int duplicate_reads = distance(new_end, assembled_spot.end());
    if (duplicate_reads) {
        m_telemetry.input_metrics.duplicate_reads_count += duplicate_reads;
        assembled_spot.erase(new_end, assembled_spot.end());
    }
    //removeSubsequences(spot_name, assembled_spot);

}

template<typename TWriter>
void fastq_parser<TWriter>::prepare_assemble_spot(const string& spot_name, vector<fastq_read>& assembled_spot, vector<int>& read_ids)
{
    assert(!assembled_spot.empty());
    int sz = assembled_spot.size();
    if (sz > 1) {
        remove_duplicate_reads(spot_name, assembled_spot);
        sz = assembled_spot.size();
        if (m_sort_by_readnum) {
            // already sorted by read number
            // sort by read number
            //sort(assembled_spot.begin(), assembled_spot.end(), [](const fastq_read& l, const fastq_read& r) { 
            //    return l.ReadNum() < r.ReadNum(); });
            if (m_read_type_sz > 0) {
                if (m_read_type_sz < (int)sz) 
                    throw fastq_error(30, "readTypes number should match the number of reads {} != {}, Spot: '{}'", m_read_type_sz, sz, spot_name);
                // sort index by file order so that readTypes can be applied
                read_ids.resize(sz);
                iota(read_ids.begin(), read_ids.end(), 0);
                sort(read_ids.begin(), read_ids.end(), [&](int i, int j) {
                    if (assembled_spot[i].m_ReaderIdx == assembled_spot[j].m_ReaderIdx) 
                        return assembled_spot[i].LineNumber() < assembled_spot[j].LineNumber(); 
                    return assembled_spot[i].m_ReaderIdx < assembled_spot[j].m_ReaderIdx;
                });
                for (int i = 0; i < sz; ++i) {
                    assembled_spot[read_ids[i]].SetType(m_read_types[i]);
                }
            }
        } else {
            // sort by order in file
            sort(assembled_spot.begin(), assembled_spot.end(), [](const fastq_read& l, const fastq_read& r) {
                return l.m_ReaderIdx == r.m_ReaderIdx ? l.LineNumber() < r.LineNumber() : l.m_ReaderIdx < r.m_ReaderIdx;});

            if (m_read_type_sz > 0) {
                if (m_read_type_sz < (int)sz) 
                    throw fastq_error(30, "readTypes number should match the number of reads {} != {}, Spot: '{}'", m_read_type_sz, sz, spot_name);
                for (int i = 0; i < sz; ++i) {
                    assembled_spot[i].SetType(m_read_types[i]);
                }
            }
        }
    } else if (sz == 1) {
        if (m_read_type_sz > 0) {
            assembled_spot[0].SetType(m_read_types[0]);
        }
    }
} 


template<typename TWriter>
template<typename ScoreValidator, bool is_nanopore>
void fastq_parser<TWriter>::assemble_spot_thread() 
{
    // get spot from assemble_spot_queue 
    // send it to writer 
    // add spot_id to clear_spot_queue
    vector<fastq_read> assembled_spot;
    vector<int> read_ids;
    size_t readCount = 0, spotCount = 0, currCount = 0;
    while (assemble_spot_queue->dequeue(assembled_spot)) {

        m_writer->write_messages();
        m_writer->write_spot(assembled_spot.front().Spot(), assembled_spot);
        ++spotCount;
        readCount += assembled_spot.size();
        currCount += assembled_spot.size();
        clear_spot_queue->enqueue(move(assembled_spot.front().m_SpotId));
        update_telemetry_queue->enqueue(move(assembled_spot));
        if (currCount >= 10e6) {
            m_logger->info("spots: {:L}, reads: {:L}", spotCount, readCount);
            currCount = 0;
        }
    }
    m_logger->info("spots: {:L}, reads: {:L}", spotCount, readCount);
    assert(assemble_spot_queue->enqueue_count == assemble_spot_queue->dequeue_count);
    if (assemble_spot_queue->enqueue_count != assemble_spot_queue->dequeue_count)
        throw fastq_error("assemble_spot_queue->enqueue_count != assemble_spot_queue->dequeue_count {} != {}", assemble_spot_queue->enqueue_count, assemble_spot_queue->dequeue_count);
    clear_spot_queue->close();
    update_telemetry_queue->close();
}

template<typename TWriter>
template<typename ScoreValidator, bool is_nanopore>
void fastq_parser<TWriter>::clear_spot_thread() 
{
    // get spot_id from clear_spot_queue
    // and clear it from memory
    size_t spot_id;
    while (clear_spot_queue->dequeue(spot_id)) {
        assert(spot_id != 0);
        m_spot_assembly. template clear_spot_mt<is_nanopore>(spot_id);
    }
}

template<typename TWriter>
void fastq_parser<TWriter>::update_telemetry_thread()
{
    spot_t spot;
    while (update_telemetry_queue->dequeue(spot)) {
        update_telemetry(spot); 
    }

}

template<typename TWriter>
void fastq_parser<TWriter>::write_spot_thread()
{
    size_t spotCount = 0, readCount = 0, currCount = 0;
    
    vector<fastq_read> assembled_spot;
    while (assemble_spot_queue->dequeue(assembled_spot)) {
        assert(assembled_spot.empty() == false);
        auto spot_size = assembled_spot.size();
        auto& spot = assembled_spot.front().Spot();
        readCount += spot_size;
        currCount += spot_size;
        ++spotCount;
        if (m_sort_by_readnum)
            stable_sort(assembled_spot.begin(), assembled_spot.end(), [](const auto& l, const auto& r) { return l.ReadNum() < r.ReadNum();});
        m_writer->write_spot(spot, assembled_spot);
        m_writer->write_messages();
        update_telemetry_queue->enqueue(move(assembled_spot));
        if (currCount >= 10e6) {
            m_logger->info("spots: {:L}, reads: {:L}", spotCount, readCount);
            currCount = 0;
        }
    }
    update_telemetry_queue->close();
    m_logger->info("spots: {:L}, reads: {:L}", spotCount, readCount);
} 


#if defined(_PARALLEL_PARSE_)

template<typename TWriter>
template<typename ScoreValidator, bool is_nanopore, typename ErrorChecker>
void fastq_parser<TWriter>::second_pass(ErrorChecker&& error_checker)
{
    size_t readCount = 0, spotCount = 0;
    spdlog::stopwatch sw;
    spdlog::info("Parsing from {} files", m_readers.size());

    // Starting the parallel threads
    save_spot_queue.reset(new queue_t<spot_read_t, SAVE_SPOT_QUEUE_SIZE>("save_spot_queue", pipeline_cancelled));
    assemble_spot_queue.reset(new queue_t<vector<fastq_read>, ASSEMBLE_QUEUE_SIZE>("assemble_spot_queue", pipeline_cancelled));
    clear_spot_queue.reset(new queue_t<size_t, CLEAR_SPOT_QUEUE_SIZE>("clear_spot_queue", pipeline_cancelled));
    update_telemetry_queue.reset(new queue_t<spot_t, ASSEMBLE_QUEUE_SIZE>("update_telemetry_queue", pipeline_cancelled));

    array<future<exception_ptr>, 4> futures;

    futures[3] = std::async(std::launch::async, [this](){ BEGIN_MT_EXCEPTION this->template save_spot_thread<ScoreValidator, is_nanopore>(); END_MT_EXCEPTION });
    futures[2] = std::async(std::launch::async, [this](){ BEGIN_MT_EXCEPTION this->template assemble_spot_thread<ScoreValidator, is_nanopore>();END_MT_EXCEPTION }); 
    futures[1] = std::async(std::launch::async, [this](){ BEGIN_MT_EXCEPTION this->template clear_spot_thread<ScoreValidator, is_nanopore>();END_MT_EXCEPTION }); 
    futures[0] = std::async(std::launch::async, [this](){ BEGIN_MT_EXCEPTION this->update_telemetry_thread(); END_MT_EXCEPTION }); 

    spot_read_t spot_read;
    for_each_read<ScoreValidator, ErrorChecker>(error_checker, [&](size_t row_id, CFastqRead& read) {
        try {
            read.m_SpotId = m_spot_assembly.m_read_index[row_id];
            spot_read.read = move(read);
            spot_read.is_last = m_spot_assembly.is_last_spot(row_id);
            ++readCount;
            if (spot_read.is_last)
                ++spotCount;

            save_spot_queue->enqueue(move(spot_read));
            if (readCount % 10000000 == 0)  
                m_spot_assembly.optimize();
        } catch (fastq_error& e) {
            error_checker(e);
        }
    });
    save_spot_queue->close();

    for (auto& ft : futures) {
        auto eptr = ft.get();
        if (eptr)
            std::rethrow_exception(eptr);
    }

    // Second pass stats should match the first pass
    assert(m_spot_assembly.m_total_spots == spotCount && m_spot_assembly.m_read_index.size() == readCount);
    if (m_spot_assembly.m_total_spots != spotCount)
        throw fastq_error("Invalid assembly: Spot counts do not match {} != {}", m_spot_assembly.m_total_spots, spotCount);
    if (m_spot_assembly.m_read_index.size() != readCount)
        throw fastq_error("Invalid assembly: Read counts do not match {} != {}", m_spot_assembly.m_read_index.size(), readCount);        

    if (m_telemetry.groups.back().rejected_spots > 0)
        spdlog::info("rejected spots: {:L}", m_telemetry.groups.back().rejected_spots);
    spdlog::info("parsing time: {}", sw);
}
#else
template<typename TWriter>
template<typename ScoreValidator, bool is_nanopore, typename ErrorChecker>
void fastq_parser<TWriter>::second_pass(ErrorChecker&& error_checker)
{
    size_t readCount = 0;
    vector<fastq_read> assembled_spot;
    vector<int> read_ids;
    string spot;
    spdlog::stopwatch sw;
    spdlog::info("Parsing from {} files", m_readers.size());

    for_each_read<ScoreValidator, ErrorChecker>(error_checker, [&](size_t row_id, CFastqRead& read) {
        try {
            auto spot_id = m_spot_assembly.m_read_index[row_id];
            if (m_spot_assembly.is_last_spot(row_id)) {
                m_spot_assembly. template get_spot<ScoreValidator, is_nanopore>(spot_id, assembled_spot);
                spot = move(read.Spot());
                assembled_spot.push_back(move(read));

                prepare_assemble_spot(spot, assembled_spot, read_ids);
                m_writer->write_spot(spot, assembled_spot);
                m_writer->write_messages();
                m_spot_assembly.clear_spot_mt<is_nanopore>(spot_id); 
            } else {
                m_spot_assembly. template save_read<ScoreValidator, is_nanopore>(spot_id, read);
            }
            if (readCount % 10000000 == 0)  {
                m_spot_assembly.optimize();
            }

        } catch (fastq_error& e) {
            error_checker(e);
        }
    });

    // Second pass stats should match the first pass
    assert(m_spot_assembly.m_total_spots == spotCount && m_spot_assembly.m_read_index.size() == readCount);
    if (m_spot_assembly.m_total_spots != spotCount)
        throw fastq_error("Invalid assembly: Spot counts do not match {} != {}", m_spot_assembly.m_total_spots, spotCount);
    if (m_spot_assembly.m_read_index.size() != readCount)
        throw fastq_error("Invalid assembly: Read counts do not match {} != {}", m_spot_assembly.m_read_index.size(), readCount);        

    if (m_telemetry.groups.back().rejected_spots > 0)
        spdlog::info("rejected spots: {:L}", m_telemetry.groups.back().rejected_spots);
    spdlog::info("parsing time: {}", sw);
}

#endif
#endif

