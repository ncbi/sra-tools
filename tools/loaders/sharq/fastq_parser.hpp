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
#define LIBSAIS_OPENMP
#define LIBSAIS_OPENMP
//#include "libsais64.h"
#include "libsais.h"
// input streams
#include "bxzstr/bxzstr.hpp"
#include <bm/bm64.h>
#include <bm/bmdbg.h>
#include <bm/bmtimer.h>
#include <bm/bmbmatrix.h>
#include <bm/bmstrsparsevec.h>
#include <bm/bmsparsevec_algo.h>
#include <bm/bmsparsevec_compr.h>
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
#include "taskflow/algorithm/for_each.hpp"
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

    bool eof() const { return m_buffered_spot.empty() && m_stream->eof();}  ///< Returns true if file has no more reads
    // multi-threaded version of eof
    bool eof_mt() const { return m_read_queue->is_done && m_read_queue->queue.peek() == nullptr;}  ///< Returns true if file has no more reads

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
    void remove_duplicate_reads(vector<fastq_read>& assembled_spot);

    /**
     * removes duplicate reads and assigns readType
    */
    void prepare_assemble_spot(vector<fastq_read>& assembled_spot, vector<int>& read_ids);


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


    template<typename ScoreValidator, typename ErrorChecker>
    void parse_and_dump(const string& mode, ErrorChecker&& err_checker);

    template<typename ScoreValidator, typename ErrorChecker>
    void parse_and_dump_na(ErrorChecker&& err_checker);

    template<typename ScoreValidator, typename ErrorChecker>
    void parse_and_dump_sort(ErrorChecker&& err_checker);

    template<typename ScoreValidator, typename ErrorChecker>
    void parse_and_dump_rotate(ErrorChecker&& err_checker);

    template<typename ScoreValidator, typename ErrorChecker>
    void test_sort_create(const string& case_name, ErrorChecker&& error_checker);

    template<typename ScoreValidator, typename ErrorChecker, typename T>
    void test_each_batch(const string& case_name, ErrorChecker&& error_checker, size_t batch_size, T&& func);

    template<typename ScoreValidator, typename ErrorChecker, typename T>
    void test_each_spot(const string& case_name, ErrorChecker&& error_checker, size_t batch_size, T&& func);


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
            throw fastq_error(30, "readTypes number should match the number of reads {} != {}", m_read_type.size(), reads.size());
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
            throw fastq_error(30, "readTypes number should match the number of reads {} != {}", m_read_type.size(), reads.size());
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
    }

    while (qual_size < sz) {
        read.mQuality += " ";
        read.mQuality += to_string(ScoreValidator::min_score() + 30);
        read.mQualScores.push_back(ScoreValidator::min_score() + 30);
        ++qual_size;
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
        read.mQuality.append(sz - qual_size,  char(ScoreValidator::min_score() + 30));
    }
    if (m_curr_platform != m_defline_parser.GetPlatform())
        throw fastq_error(70, "Input file has data from multiple platforms ({} != {})", m_curr_platform, m_defline_parser.GetPlatform());

    for (auto c : read.mQuality)
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

    update_readers_telemetry();

//    spdlog::info("spots: {:L}, reads: {:L}", spotCount, readCount);
    if (m_telemetry.groups.back().rejected_spots > 0)
        spdlog::info("rejected spots: {:L}", m_telemetry.groups.back().rejected_spots);
    spdlog::debug("parsing time: {}", sw);
}

template<typename TWriter>
void fastq_parser<TWriter>::update_readers_telemetry()
{
    for(const auto& reader : m_readers) {
        m_telemetry.input_metrics.defline_len += reader.m_input_metrics.defline_len;
        m_telemetry.input_metrics.sequence_len += reader.m_input_metrics.sequence_len;
        m_telemetry.input_metrics.quality_len += reader.m_input_metrics.quality_len;
        m_telemetry.input_metrics.rejected_read_count += reader.m_input_metrics.rejected_read_count;

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
        for (auto c : qual_scores) {
            ++m_telemetry.output_metrics.quality_counts[c];
        }
        telemetry.max_sequence_size = max<int>(sz, telemetry.max_sequence_size);
        telemetry.min_sequence_size = min<size_t>(sz, telemetry.min_sequence_size);
        m_telemetry.output_metrics.quality_len += r.Quality().size();
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
        if (m_telemetry.output_metrics.sequence_len > 0) {
            qm["sequence_loss"] = m_telemetry.input_metrics.sequence_len - m_telemetry.output_metrics.sequence_len;
            qm["pct_sequence_loss"] = fmt::format("{:.2f}", 100. * float(m_telemetry.input_metrics.sequence_len - m_telemetry.output_metrics.sequence_len)/m_telemetry.output_metrics.sequence_len);
        }
        if (m_telemetry.output_metrics.quality_len > 0) {
            qm["quality_loss"] = m_telemetry.input_metrics.quality_len - m_telemetry.output_metrics.quality_len;
            qm["pct_quality_loss"] = fmt::format("{:.2f}", 100. * float(m_telemetry.input_metrics.quality_len - m_telemetry.output_metrics.quality_len)/m_telemetry.output_metrics.quality_len);
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
        int early_end = 0;
        CFastqRead read;

        vector<vector<CFastqRead>> spot_reads(num_readers);
        vector<CFastqRead> assembled_spot;


        do {
            has_spots = false;
            for (int i = 0; i < num_readers; ++i)
            try {
    #ifdef _PARALLEL_READ1_
                if (!m_readers[i] . template get_next_spot_mt<ScoreValidator>(spot, spot_reads[i])) {
                    if (m_readers[i].eof_mt()) {
    #else                
                if (!m_readers[i] . template get_next_spot<ScoreValidator>(spot, spot_reads[i])) {
                    if (m_readers[i].eof()) {
    #endif                    
                        ++early_end;
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
                int readers_at_eof = 0;
                for (int i = 0; i < num_readers; ++i) {
    #ifdef _PARALLEL_READ1_
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
            m_spot_assembly. template get_spot_mt<ScoreValidator, is_nanopore>(read.m_SpotId, spot);
            spot_name = read.Spot();
            spot_id = read.m_SpotId;
            spot.push_back(move(read));
            prepare_assemble_spot(spot, read_ids);

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
void fastq_parser<TWriter>::remove_duplicate_reads(vector<fastq_read>& assembled_spot)
{
    sort(assembled_spot.begin(), assembled_spot.end(), [](const auto& l, const auto& r) { 
        if (l.ReadNum() == r.ReadNum()) {
            int c = l.Sequence().compare(r.Sequence()); 
            if (c == 0) 
                c = l.Quality().compare(r.Quality());
            return c < 0;
        }
        return l.ReadNum() < r.ReadNum();
    });
    auto new_end = unique(assembled_spot.begin(), assembled_spot.end(), [](const auto& l, const auto& r){
        return l.ReadNum() == r.ReadNum() && l.Sequence().compare(r.Sequence()) == 0 && l.Quality().compare(r.Quality()) == 0;
    });
    int duplicate_reads = distance(new_end, assembled_spot.end());
    if (duplicate_reads) {
        m_telemetry.input_metrics.duplicate_reads_count += duplicate_reads;
        for (auto it = new_end; it != assembled_spot.end(); ++it) {
            m_telemetry.input_metrics.duplicate_reads_len += it->Sequence().size();
        }
        assembled_spot.erase(new_end, assembled_spot.end());
    }
}

template<typename TWriter>
void fastq_parser<TWriter>::prepare_assemble_spot(vector<fastq_read>& assembled_spot, vector<int>& read_ids)
{
    assert(!assembled_spot.empty());
    int sz = assembled_spot.size();
    if (sz > 1) {
        remove_duplicate_reads(assembled_spot);
        sz = assembled_spot.size();
        if (m_sort_by_readnum) {
            // already sorted by read number
            // sort by read number
            //sort(assembled_spot.begin(), assembled_spot.end(), [](const fastq_read& l, const fastq_read& r) { 
            //    return l.ReadNum() < r.ReadNum(); });
            if (m_read_type_sz > 0) {
                if (m_read_type_sz < (int)sz)
                    throw fastq_error(30, "readTypes number should match the number of reads {} != {}", m_read_type_sz, sz);
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
                    throw fastq_error(30, "readTypes number should match the number of reads {} != {}", m_read_type_sz, sz);
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

                prepare_assemble_spot(assembled_spot, read_ids);
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

template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker, typename T>
void fastq_parser<TWriter>::test_each_spot(const string& case_name, ErrorChecker&& error_checker, size_t batch_size, T&& func)
{
    size_t spot_id = 0;
    vector<string> v_seqs;
    size_t v_max_size = 0;
    tf::Executor executor(min<int>(48, std::thread::hardware_concurrency())); 
    tf::Taskflow taskflow;
    size_t batch_id = 0;
    string seq;
    ofstream seq_in(case_name + ".seq.in");
    atomic<size_t> total_size = 0;
    size_t curr_batch_size = 0;
    size_t MAX_SIZE = 2147483648;
    for_each_spot<ScoreValidator>(error_checker, [&](vector<fastq_read>& spot) {
        seq.clear();
        for (size_t i = 0; i < spot.size(); ++i) {
            seq += spot[i].Sequence();
        }
        if (curr_batch_size + spot.size() >= MAX_SIZE) {
            total_size += func(executor, 0, v_max_size, v_seqs, case_name + "_" + to_string(batch_id));
            v_max_size = 0;
            v_seqs.clear();
            curr_batch_size = 0;
            ++batch_id;
        }
        seq_in << seq << endl;
        curr_batch_size += seq.size();
        v_max_size = max(v_max_size, seq.size());
        v_seqs.push_back(move(seq));
        ++spot_id;
        if (spot_id % 1000000 == 0) 
            spdlog::info("Processed {} spots", spot_id);
    });
    if (!v_seqs.empty())
        total_size += func(executor, 0, v_max_size, v_seqs, case_name + "_" + to_string(batch_id));
    spdlog::info("Total size {}", total_size);
}


template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker, typename T>
void fastq_parser<TWriter>::test_each_batch(const string& case_name, ErrorChecker&& error_checker, size_t batch_size, T&& func)
{
    size_t spot_id = 0;
    vector<vector<string>> v_seqs;
    vector<size_t> v_max_sizes;
    tf::Executor executor(min<int>(48, std::thread::hardware_concurrency())); 
    tf::Taskflow taskflow;
    size_t batch_id = 0;
    string seq;
    ofstream seq_in(case_name + ".seq.in");
    atomic<size_t> total_size = 0;
    size_t curr_batch_size = 0;
    size_t MAX_SIZE = 2147483648;
    for_each_spot<ScoreValidator>(error_checker, [&](vector<fastq_read>& spot) {
        for (size_t i = 0; i < spot.size(); ++i) {
            if (curr_batch_size + spot[i].Sequence().size() >= MAX_SIZE) {
                int first = 0;
                int last = v_seqs.size();
                
                taskflow.clear();
                taskflow.for_each_index(ref(first), ref(last), 1, [&](int i) { 
                    spdlog::info("Processing batch {}, read {} with {} sequences of length {}", batch_id, i, v_seqs[i].size(), v_max_sizes[i]);
                    total_size += func(executor, i, v_max_sizes[i], v_seqs[i], case_name + "_" + to_string(batch_id));
                });
                executor.run(taskflow).wait();
                
                v_max_sizes.clear();
                v_seqs.clear();
                curr_batch_size = 0;
                ++batch_id;
            }
        }
        size_t max_seq = 0;
        for (size_t i = 0; i < spot.size(); ++i) 
            max_seq = max(spot[i].Sequence().size(), max_seq);
        curr_batch_size += max_seq;            

        if (spot.size() > v_seqs.size()) {
            auto n = v_seqs.size();
            v_seqs.resize(spot.size());
            v_max_sizes.resize(spot.size(), 0);
            if (n > 0) {
                for (size_t i = n; i < spot.size(); ++i) {
                    v_seqs[i].resize(spot_id - (batch_id * batch_size));
                    v_max_sizes[i] = 0;
                }
            }
        }
        seq.clear();
        for (size_t i = 0; i < v_seqs.size(); ++i) {

            if (i < spot.size()) {
                v_max_sizes[i] = max(v_max_sizes[i], spot[i].Sequence().size());
                v_seqs[i].emplace_back(spot[i].Sequence());
                seq += spot[i].Sequence();

            } else {
                v_seqs[i].emplace_back();
            }
        }
        seq_in << seq << endl;
        ++spot_id;
        /*
        if (spot_id % batch_size == 0) { 
            int first = 0;
            int last = v_seqs.size();
            
            taskflow.clear();
            taskflow.for_each_index(ref(first), ref(last), 1, [&](int i) { 
                spdlog::info("Processing batch {}, read {} with {} sequences of length {}", batch_id, i, v_seqs[i].size(), v_max_sizes[i]);
                total_size += func(executor, i, v_max_sizes[i], v_seqs[i], case_name + "_" + to_string(batch_id));
            });
            executor.run(taskflow).wait();
            
            v_max_sizes.clear();
            v_seqs.clear();
            ++batch_id;
        }
        */
        if (spot_id % 1000000 == 0) 
            spdlog::info("Processed {} spots", spot_id);
    });
    seq_in.close();
    if (!v_seqs.empty()) {
        int first = 0;
        int last = v_seqs.size();
        taskflow.clear();
        taskflow.for_each_index(ref(first), ref(last), 1, [&](int i) { 
            spdlog::info("Processing read {} with {} sequences of length {}", i, v_seqs[i].size(), v_max_sizes[i]);
            total_size += func(executor, i, v_max_sizes[i], v_seqs[i], case_name + "_" + to_string(batch_id));
        });
        executor.run(taskflow).wait();
        
    }
    spdlog::info("Total size {}", total_size);

}

template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::parse_and_dump(const string& mode, ErrorChecker&& error_checker)
{
    spdlog::stopwatch sw;
    spdlog::info("Parsing from {} files", m_readers.size());
    if (mode == "na")
        parse_and_dump_na<ScoreValidator>(error_checker);
    else if (mode == "sort")        
        parse_and_dump_sort<ScoreValidator>(error_checker);
    else if (mode == "rotate")        
        parse_and_dump_rotate<ScoreValidator>(error_checker);

    spdlog::debug("parsing time: {}", sw);
}


vector<string> slice_str(const string& str, size_t len) {
    vector<string> vec;
    for (size_t i = 0; i < str.size(); i += len) {
        vec.push_back(str.substr(i, len));
    }
    return vec;
}

static 
int64_t bwt_vector(int max_size, vector<string>& seqs)
{
    if (seqs.empty())
        return 0;
    string str;
    auto sz = seqs.front().size();
    str.reserve(seqs.size() * sz);
    for (auto& s : seqs) {
        str += s;
        if (s.size() != sz) {
            spdlog::error("Variable length");
            throw runtime_error("Variable length");
        }
        s.clear();
    }
#if defined(LIBSAIS64_H)
    vector<int64_t> sa;
    sa.resize(str.size(), 0);

    auto sais_key = libsais64_bwt_omp(
        (uint8_t*)&str[0], 
        (uint8_t*)&str[0], 
        (int64_t*)&sa[0],
        str.size(), 
        0, nullptr,
        24);
#else
    vector<int32_t> sa;
    sa.resize(str.size(), 0);

    auto sais_key = libsais_bwt_omp(
        (uint8_t*)&str[0], 
        (uint8_t*)&str[0], 
        (int32_t*)&sa[0],
        str.size(), 
        0, nullptr,
        24);
#endif        
    seqs = slice_str(str, max_size);
/*
    size_t offset = 0;        
    seqs.clear();
    for (size_t i = 0; i < lens.size(); ++i) {
        seqs.push_back(str.substr(offset, len[i]));        
        offset += len[i];
    }
*/
    return sais_key;        
}


static void unbwt_vector(int64_t key, int max_size, vector<string>& seqs)
{
    string str;
    for (auto& s : seqs) {
        str += s;
    }
#if defined(LIBSAIS64_H)

    vector<int64_t> sa;
    sa.resize(str.size(), 0);

    libsais64_unbwt_omp((uint8_t*)&str[0], (uint8_t*)&str[0], (int64_t*)&sa[0], str.size(), nullptr, key, 24);
#else
    vector<int32_t> sa;
    sa.resize(str.size(), 0);

    libsais_unbwt_omp((uint8_t*)&str[0], (uint8_t*)&str[0], (int32_t*)&sa[0], str.size(), nullptr, key, 24);
#endif
    seqs = slice_str(str, max_size);
    /*
    seqs.clear();
    size_t offset = 0;        
    for (size_t i = 0; i < lens.size(); ++i) {
        seqs.push_back(str.substr(offset, len[i]));        
        offset += len[i];
    }
    */
}


static
size_t test_na_batch(tf::Executor& executor, size_t offset, size_t max_size, vector<string>& seqs, const string& case_name)
{
        BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block
        string name = case_name + "_" + to_string(offset);

        auto logger = spdlog::get("parser_logger");
        logger->info("Starting bwt for {}, seq {}, max_size {}", name, seqs.size(), max_size);
        auto bwt_key = bwt_vector(max_size, seqs);
        logger->info("Finish bwt for {}", name);

        svector_u32 sequence;     ///< sequence data
        bm::sparse_vector<uint64_t, bvector_type> offsets;
        auto o_bi = offsets.get_back_inserter();
        vector<uint32_t> tmp_buffer;
        size_t seq_offset = 0;
        size_t base_count = 0;
        for (const auto& seq : seqs) {
            size_t sz = seq.size();
            base_count += sz;
            tmp_buffer.resize(sz);
            for (size_t i = 0; i < sz; ++i) {
                tmp_buffer[i] = DNA2int(seq[i]);
            }
            size_t offset = seq_offset;
            sequence.import(&tmp_buffer[0], sz, offset);
            seq_offset = offset + sz;
            offset |= sz << 48;
            o_bi = offset;
        }
        o_bi.flush();

        offsets.optimize(TB);
        bm::file_save_svector(offsets, name + ".seq_offset");
        sequence.optimize(TB);
        bm::file_save_svector(sequence, name + ".seq");

        auto batch_size = fs::file_size(name + ".seq") + fs::file_size(name + ".seq_offset");
        float bits_per_base = 0;
        if (base_count) {
            bits_per_base = float(batch_size * 8)/base_count;
        }
        spdlog::info("{} Batch size {}, num_rows {}, base_count {}, bits/base {}", offset, batch_size, max_size, base_count, bits_per_base);
        {
            json f;
            f["batch-size"] = batch_size;
            f["max-seq-size"] = max_size;
            f["num-bases"] = base_count;
            f["bits-per-base"] = bits_per_base;
            f["bwt_key"] = bwt_key;

            ofstream os(name + ".info");
            os << f.dump(4, ' ', true) << endl;

        }
        return batch_size;
}



static
size_t test_na_batch_rle(tf::Executor& executor, size_t offset, size_t max_size, vector<string>& seqs, const string& case_name)
{
        BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block
        string name = case_name + "_" + to_string(offset);

        auto logger = spdlog::get("parser_logger");
        size_t bwt_key = 0;
        logger->info("Starting bwt for {}, seq {}, max_size {}", name, seqs.size(), max_size);
        bwt_key = bwt_vector(max_size, seqs);
        logger->info("Finish bwt for {}", name);

        svector_u32 sequence;     ///< sequence data
        bm::rsc_sparse_vector<unsigned, svector_u32 > rle;
        //svector_u32 rle;     
        bm::sparse_vector<uint64_t, bvector_type> offsets;
        auto o_bi = offsets.get_back_inserter();
        auto rle_bi = rle.get_back_inserter();
        vector<uint32_t> tmp_buffer;
        tmp_buffer.reserve(max_size * seqs.size());

        size_t seq_offset = 0;
        size_t base_count = 0;
        //ofstream os(name + ".rle.txt");
        char prev = ' ';
        char last = ' ';
        int cnt = 0;
        size_t j = 0;

        for (const auto& seq : seqs) {
            size_t sz = seq.size();
            base_count += sz;
            for (size_t i = 0; i < sz; ++i) {
                if (seq[i] == last) {
                    ++cnt;
                } else if (cnt > 0) {
                    //tmp_buffer.push_back(dna2int(prev, last));
                    tmp_buffer.push_back(DNA2int(last));
                    //os << prev << ":" << cnt << endl;
                    (cnt - 1 == 0) ? rle_bi.add_null() : rle_bi.add(cnt - 1);    
                    //rle_bi = (cnt - 1);                        
                    cnt = 1;
                    prev = last;
                    last = seq[i];
                    ++j;    
                } else {
                    prev = last;
                    last = seq[i];
                    cnt = 1;
                }
            }
            //sz = j;
            size_t offset = seq_offset;
            //sequence.import(&tmp_buffer[0], sz, offset);
            seq_offset = offset + sz;
            offset |= sz << 48;
            o_bi = offset;
        }
        if (cnt > 0) {
            //tmp_buffer.push_back(dna2int(prev, last));
            tmp_buffer.push_back(DNA2int(last));
            //os << prev << ":" << cnt << endl;
            (cnt - 1 == 0) ? rle_bi.add_null() : rle_bi.add(cnt - 1);    
            //rle_bi = (cnt - 1);                        
        }

        uint8_t prev_code = tmp_buffer[0];
        array<array<size_t, 5>, 5> base_stats = {};

        for (size_t i = 1; i < tmp_buffer.size(); ++i) {
            ++base_stats[prev_code][tmp_buffer[i]];
            prev_code = tmp_buffer[i];
        }
        vector<int> idx(5);
        for (size_t i = 0; i < base_stats.size(); ++i) {
            iota(idx.begin(), idx.end(), 0);
            sort(idx.begin(), idx.end(), [&](const auto& l, const auto& r) {
                return base_stats[i][l] >  base_stats[i][r];
            });
            base_stats[i][idx[0]] = 0;
            base_stats[i][idx[1]] = 1;
            base_stats[i][idx[2]] = 2;
            base_stats[i][idx[3]] = 3;
            base_stats[i][idx[4]] = 4;
        }
        prev_code = tmp_buffer[0];
        for (size_t i = 1; i < tmp_buffer.size(); ++i) {
            uint8_t curr_code = tmp_buffer[i];
            tmp_buffer[i] = base_stats[prev_code][curr_code];
            prev_code = curr_code;
        }


        sequence.import(&tmp_buffer[0], tmp_buffer.size(), 0);

        rle_bi.flush();
        //rle.sync();
        rle.optimize(TB);
        bm::file_save_svector(rle, name + ".rle");

        o_bi.flush();
        offsets.optimize(TB);
        bm::file_save_svector(offsets, name + ".seq_offset");
        sequence.optimize(TB);
        bm::file_save_svector(sequence, name + ".seq");

        auto batch_size = fs::file_size(name + ".seq") + fs::file_size(name + ".seq_offset") + fs::file_size(name + ".rle");
        float bits_per_base = 0;
        if (base_count) {
            bits_per_base = float(batch_size * 8)/base_count;
        }
        spdlog::info("{} Batch size {}, num_rows {}, base_count {}, bits/base {}", offset, batch_size, max_size, base_count, bits_per_base);
        {
            json f;
            f["batch-size"] = batch_size;
            f["max-seq-size"] = max_size;
            f["num-bases"] = base_count;
            f["bits-per-base"] = bits_per_base;
            f["bwt-key"] = bwt_key;
            array<array<char, 5>, 5> char_stats = {};
            for (size_t i = 0; i < base_stats.size(); ++i) {
                for (size_t j = 0; j < base_stats[i].size(); ++j) {
                    char_stats[i][base_stats[i][j]] = Int2DNA(j);
                }
            }
            f["char-stats"] = char_stats;
            f["base-stats"] = base_stats;
            
            ofstream os(name + ".info");
            os << f.dump(4, ' ', true) << endl;

        }
        return batch_size;
}


static 
tuple<int64_t, size_t> s_get_info(const string& file_name)
{
    int64_t bwt_key = 0;
    size_t max_size = 0;
    {
        ifstream is(file_name);
        string line;
        while (is.good()) {
            getline(is, line);
            if (line.rfind("bwt_key", 0) == 0) {
                auto pos = line.find(":");
                assert(pos != string::npos);
                ++pos;
                while (line[pos] != ' ') ++pos;
                bwt_key = stoi(line.substr(pos));
                assert(bwt_key);
            } else if (line.rfind("max_seq_size", 0) == 0) {
                auto pos = line.find(":");
                assert(pos != string::npos);
                ++pos;
                while (line[pos] != ' ') ++pos;
                max_size = stoi(line.substr(pos));
                assert(max_size);
            }
        }
    }
    return make_tuple(bwt_key, max_size);
}


static 
void test_na_dump_rle(const string& case_name)
{
    size_t batch_id = 0;

    ofstream seq_out(case_name + ".seq.out");
    vector<uint32_t> tmp_buffer;
    vector<uint64_t> offsets;
    vector<uint32_t> rle;
    vector<string> seqs;

    while (true) {
        string batch_name = case_name + "_" + to_string(batch_id);
        if (!fs::exists(batch_name + "_0.seq"))
            break;
        size_t case_idx = 0;
        vector<vector<string>> spots;
        while (true) {
            string name = batch_name + "_" + to_string(case_idx);
            if (!fs::exists(name + ".seq"))
                break;
            ++case_idx;
            json info;
            {
                std::ifstream f(name + ".info");
                info = json::parse(f);
            }
            size_t bwt_key = info["bwt-key"];
            size_t max_size  = info["max-seq-size"];
            array<array<size_t, 5>, 5> char_stats = info["char-stats"];


            svector_u32 sv_sequence;     ///< sequence data
            bm::rsc_sparse_vector<unsigned, svector_u32 > sv_rle;
            //svector_u32 sv_rle;
            bm::sparse_vector<uint64_t, bvector_type> sv_offsets;
            seqs.clear();
            string str;
            rle.clear();
            {
                bm::file_load_svector(sv_rle, name + ".rle");
                //rle.resize(sv_rle.size());
                //sv_rle.decode(&rle[0], 0, sv_rle.size());
            }
            auto rle_it = sv_rle.begin();
            {
                bm::file_load_svector(sv_sequence, name + ".seq");
                tmp_buffer.resize(sv_sequence.size());
                sv_sequence.decode(&tmp_buffer[0], 0, sv_sequence.size());
                uint8_t prev_code = tmp_buffer[0];
                str.append(rle_it.is_null() ? 1 : rle_it.value() + 1, Int2DNA(prev_code));
                for (size_t i = 1; i < tmp_buffer.size(); ++i) {
                    char code = char_stats[prev_code][tmp_buffer[i]];
                    rle_it.advance();
                    str.append(rle_it.is_null() ? 1 : rle_it.value() + 1, code);
                    prev_code = DNA2int(code);
                }
                tmp_buffer.resize(0);
                rle.resize(0);
            }            

            offsets.clear();
            {
                bm::file_load_svector(sv_offsets, name + ".seq_offset");
                offsets.resize(sv_offsets.size());
                sv_offsets.decode(&offsets[0], 0, sv_offsets.size());
            }
            for (auto offset : offsets) {
                size_t len = offset >> 48;
                offset &= 0x0000FFFFFFFFFFFF;
                string& seq = seqs.emplace_back();
                seq = str.substr(offset, len);
                //for (size_t j = 0; j < len; ++j) 
                //    seq.append(rle[j + offset] + 1, Int2DNA(tmp_buffer[j + offset]));
            }

            //auto [bwt_key, max_size] = s_get_info(name + ".info");
            auto logger = spdlog::get("parser_logger");
            logger->info("Unbwt start bwt_key {}, max_size {}", bwt_key, max_size);
            unbwt_vector(bwt_key, max_size, seqs);
            logger->info("Unbwt finish");
            
            spots.push_back(move(seqs));
        }
        size_t read_id = 0;

        string s;
        while (true) {
            for (const auto& reads : spots) {
                if (read_id < reads.size()) {
                    s+= reads[read_id];
                }
            }
            if (s.empty()) break;
            seq_out << s << endl;
            s.clear();
            ++read_id;
        }
        ++batch_id;
    }
    seq_out.close();
    auto cmd = fmt::format("diff -q --text {} {}", case_name + ".seq.in", case_name + ".seq.out");
    int result = system(cmd.c_str());
    // Check the result, if you want to get the result of the diff command
    if (result == 0)
        spdlog::info("Files are identical");
    else
        spdlog::info("Files are different");
}



static 
void test_na_dump(const string& case_name)
{
    size_t batch_id = 0;

    ofstream seq_out(case_name + ".seq.out");
    vector<uint32_t> tmp_buffer;
    vector<uint64_t> offsets;
    vector<string> seqs;

    while (true) {
        string batch_name = case_name + "_" + to_string(batch_id);
        if (!fs::exists(batch_name + "_0.seq"))
            break;
        size_t case_idx = 0;
        vector<vector<string>> spots;
        while (true) {
            string name = batch_name + "_" + to_string(case_idx);
            if (!fs::exists(name + ".seq"))
                break;
            ++case_idx;

            svector_u32 sv_sequence;     ///< sequence data
            bm::sparse_vector<uint64_t, bvector_type> sv_offsets;
            seqs.clear();
            {
                bm::file_load_svector(sv_sequence, name + ".seq");
                tmp_buffer.resize(sv_sequence.size());
                sv_sequence.decode(&tmp_buffer[0], 0, sv_sequence.size());
            }            
            offsets.clear();
            {
                bm::file_load_svector(sv_offsets, name + ".seq_offset");
                offsets.resize(sv_offsets.size());
                sv_offsets.decode(&offsets[0], 0, sv_offsets.size());
            }
            for (auto offset : offsets) {
                size_t len = offset >> 48;
                offset &= 0x0000FFFFFFFFFFFF;
                //tmp_buffer.resize(len);
                //sv_sequence.decode(&tmp_buffer[0], offset, len);
                string& seq = seqs.emplace_back();
                seq.resize(len);
                for (size_t j = 0; j < len; ++j) 
                    seq[j] = Int2DNA(tmp_buffer[j + offset]);
            }


            auto [bwt_key, max_size] = s_get_info(name + ".info");
            auto logger = spdlog::get("parser_logger");
            logger->info("Unbwt start bwt_key {}, max_size {}", bwt_key, max_size);
            unbwt_vector(bwt_key, max_size, seqs);
            logger->info("Unbwt finish");
            
            spots.push_back(move(seqs));
        }
        size_t read_id = 0;

        string s;
        while (true) {
            for (const auto& reads : spots) {
                if (read_id < reads.size()) {
                    s+= reads[read_id];
                }
            }
            if (s.empty()) break;
            seq_out << s << endl;
            s.clear();
            ++read_id;
        }
        ++batch_id;
    }
    seq_out.close();
    auto cmd = fmt::format("diff -q --text {} {}", case_name + ".seq.in", case_name + ".seq.out");
    int result = system(cmd.c_str());
    // Check the result, if you want to get the result of the diff command
    if (result == 0)
        spdlog::info("Files are identical");
    else
        spdlog::info("Files are different");
}




template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::parse_and_dump_na(ErrorChecker&& error_checker)
{
    test_each_batch<ScoreValidator>("test", error_checker, 10000000, test_na_batch_rle);
    test_na_dump_rle("test");
    return;
    string name  = "test";
    spot_compress_t sc(name);
    sc.m_mid_score = ScoreValidator::min_score() + 30;
    sc.open();
    size_t spot_id = 0;
    for_each_spot<ScoreValidator>(error_checker, [&](vector<fastq_read>& spot) {
        sc.save_spot(spot_id, spot);
        ++spot_id;
    });
    sc.close();
    sc.save_sequence();
    spot_compress_t sc1(name);
    sc1.load_sequence();
    spot_id = 0;
    sc.for_each_fasta([&](size_t idx, const string& seq) {
        string saved_seq = sc1.load_spot_sequence(spot_id); 
        if (seq != saved_seq) {
            spdlog::error("seq mismatch {} != {}", seq, saved_seq);  
            exit(0);
        }
        ++spot_id;          
    });
    spdlog::info("{} spots matched", spot_id);

    auto batch_size = fs::file_size(name + ".seq") + fs::file_size(name + ".seq_offset");
    size_t base_count = sc.m_base_count;
    float bits_per_base = 0;
    if (base_count) {
        bits_per_base = float(batch_size * 8)/base_count;
    }
    spdlog::info("Batch size {}, base_count {}, bits/base {}", batch_size, base_count, bits_per_base);
    {
        ofstream os(name + ".info");
        os << "Batch size (bytes)  : " << batch_size << endl;
        os << "Number of bases     : " << base_count << endl;
        os << "bits/base ratio     : " << bits_per_base << endl;
    }
}

typedef bm::basic_bmatrix< bvector_type >         bmatr_32;
typedef bm::sparse_vector_serializer<bmatr_32>   bmb_serializer_type;
typedef bm::sparse_vector_deserializer<bmatr_32> bmb_deserializer_type;
typedef bm::sparse_vector_serial_layout<bmatr_32> bmatr_lay_t;

static int save_matrix(const string& fname, bmatr_32& bmatr)
{
    bmatr_lay_t sv_lay;
    bmb_serializer_type bmbser;     // serializer class
    // set bookmarks for faster range deserialization
    //bmbser.set_bookmarks(true, 16);
    // enable XOR filter
    bmbser.enable_xor_compression();

    // run serialization, layout now contains BLOB
    bmbser.serialize(bmatr, sv_lay);

    std::ofstream fout(fname.c_str(), std::ios::binary);
    if (!fout.good()) return -1;
    const char* buf = (char*)sv_lay.buf();
    fout.write(buf, std::streamsize(sv_lay.size()));
    if (!fout.good()) return -1;
    fout.close();
    return 0;
}
static int load_matrix(const string& fname, bmatr_32& bmatr)
{
    std::vector<unsigned char> buffer;
    // read the input buffer, validate errors
    auto ret = bm::read_dump_file(fname, buffer);
    if (ret != 0) return -2;
    if (buffer.size() == 0) return -3;

    bmb_deserializer_type bmatr_deserial_ro; // deserializer instance
    bmatr_deserial_ro.set_finalization(bm::finalization::READONLY);

    const unsigned char* buf = &buffer[0];
    bmatr_deserial_ro.deserialize(bmatr, buf, TB);
    return 0;
}


static
size_t test_sort_save_batch(tf::Executor& executor, size_t offset, size_t max_size, vector<string>& seqs, const string& case_name)
{
    BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block
    string name = case_name + "_" + to_string(offset);
    auto logger = spdlog::get("parser_logger");

    logger->info("Starting bwt for {}", name);
    auto bwt_key = bwt_vector(max_size, seqs);
    logger->info("Finish bwt for {}", name);

    vector<size_t> idx(seqs.size());
    iota(idx.begin(), idx.end(), 0);

    tf::Taskflow taskflow;
    taskflow.sort(idx.begin(), idx.end(), [&](uint32_t  l, uint32_t  r) {
        return seqs[l] < seqs[r];
    });
    executor.corun(taskflow);
    svector_u32 len_vec;
    auto bi = len_vec.get_back_inserter();

    svector_u32 idx_vec;
    auto idx_bi = idx_vec.get_back_inserter();

    size_t base_count = 0;
    //vector<unsigned> lens(idx.size());
    for (size_t j = 0; j < idx.size(); ++j) {
        auto& seq = seqs[idx[j]];
        base_count += seq.size();
        bi = seq.size();
        idx_bi = idx[j];
        //lens[j] = seq.size();
    }
    bi.flush();
    idx_bi.flush();
    len_vec.optimize(TB);
    bm::file_save_svector(len_vec, name + ".len");        

    idx_vec.optimize(TB);
    bm::file_save_svector(idx_vec, name + ".idx");        
    //spdlog::info("Creating matrix");
    int first = 0, last = max_size*3;
    bmatr_32 bmatr0(max_size * 3); // create sample row major bit-matrix
    taskflow.clear();
    taskflow.for_each_index(std::ref(first), std::ref(last), 1, 
        [&] (int i) {
            bmatr_32::bvector_type* bv1 = bmatr0.construct_row(i);
            size_t seq_idx = i/3;
            size_t bv_idx = i % 3;
            for (size_t j = 0; j < idx.size(); ++j) {
                auto& seq = seqs[idx[j]];
                if (seq_idx >= seq.size()) continue;
                auto v = DNA2int(seq[seq_idx]);
                v >>= bv_idx;
                if (v & 1) bv1->set(j, v & 1);
            }
        }
    );
    //executor.run(taskflow).wait();
    executor.corun(taskflow);
    bmatr0.optimize(TB);
    save_matrix(name + ".matrix", bmatr0);

    auto batch_size = fs::file_size(name + ".len") + fs::file_size(name + ".idx") + fs::file_size(name + ".matrix");
    float bits_per_base = 0;
    if (base_count) {
        bits_per_base = float(batch_size * 8)/base_count;
    }
    spdlog::info("{} Batch size {}, num_rows {}, base_count {}, bits/base {}", offset, batch_size, max_size, base_count, bits_per_base);
    {
        ofstream os(name + ".info");
        os << "Batch size (bytes)  : " << batch_size << endl;
        os << "max_seq_size        : " << max_size << endl;
        os << "Number of bases     : " << base_count << endl;
        os << "bits/base ratio     : " << bits_per_base << endl;
        os << "bwt_key             : " << bwt_key << endl;
    }
    return batch_size;
}

template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::test_sort_create(const string& case_name, ErrorChecker&& error_checker)
{

    test_each_batch<ScoreValidator>(case_name, error_checker, 20000000, test_sort_save_batch);
}


static 
void test_sort_dump(const string& case_name)
{
    size_t batch_id = 0;
    ofstream seq_out(case_name + ".seq.out");

    while (true) {
        string batch_name = case_name + "_" + to_string(batch_id);
        if (!fs::exists(batch_name + "_0.matrix"))
            break;
        size_t case_idx = 0;
        vector<vector<string>> spots;
        while (true) {
            string name = batch_name + "_" + to_string(case_idx);
            if (!fs::exists(name + ".matrix"))
                break;
            ++case_idx;
            vector<uint32_t> lens;
            {
                svector_u32 len_vec;
                bm::file_load_svector(len_vec, name + ".len");        
                lens.resize(len_vec.size());
                len_vec.decode(&lens[0], 0, len_vec.size());
            }

            vector<uint32_t> idx;
            {
                svector_u32 idx_vec;
                bm::file_load_svector(idx_vec, name + ".idx");
                idx.resize(idx_vec.size());
                idx_vec.decode(&idx[0], 0, idx.size());
            }

            bmatr_32 bmatr(0); // create sample row major bit-matrix
            load_matrix(name + ".matrix", bmatr);
            size_t num_seq = idx.size();
            vector<string> seqs(num_seq);
            for (size_t i = 0; i < num_seq; ++i) 
                seqs[idx[i]].resize(lens[i]);
        
            size_t num_rows = bmatr.rows();
            for (size_t i = 0; i < num_rows; ++i) {
                bmatr_32::bvector_type* bv1 = bmatr.get_row(i * 3);
                bmatr_32::bvector_type* bv2 = bmatr.get_row(i * 3 + 1);
                bmatr_32::bvector_type* bv3 = bmatr.get_row(i * 3 + 2);

                for (size_t j = 0; j < num_seq; ++j) {
                    auto& seq = seqs[idx[j]];
                    if (i >= seq.size()) continue;
                    uint8_t v = 0;
                    if (bv3->test(j))
                        v = 1 << 2;
                    if (bv2->test(j)) 
                        v |= 1 << 1;
                    if (bv1->test(j))
                    v |= 1; 
                    seq[i] = Int2DNA(v);
                }
            }
            int64_t bwt_key = 0;
            size_t max_size = 0;
            {
                ifstream is(name + ".info");
                string line;
                while (is.good()) {
                    getline(is, line);
                    if (line.rfind("bwt_key", 0) == 0) {
                        auto pos = line.find(":");
                        assert(pos != string::npos);
                        ++pos;
                        while (line[pos] != ' ') ++pos;
                        bwt_key = stoi(line.substr(pos));
                        assert(bwt_key);
                    } else if (line.rfind("max_seq_size", 0) == 0) {
                        auto pos = line.find(":");
                        assert(pos != string::npos);
                        ++pos;
                        while (line[pos] != ' ') ++pos;
                        max_size = stoi(line.substr(pos));
                        assert(max_size);
                    }
                }
            }
            auto logger = spdlog::get("parser_logger");
            logger->info("bwt_key {}, max_size {}", bwt_key, max_size);

            unbwt_vector(bwt_key, max_size, seqs);

            spots.push_back(move(seqs));
        }

        size_t read_id = 0;
        string s;
        while (true) {
            for (const auto& reads : spots) {
                if (read_id < reads.size()) {
                    s+= reads[read_id];
                }
            }
            if (s.empty()) break;
            seq_out << s << endl;
            s.clear();
            ++read_id;
        }
        ++batch_id;
    }
    seq_out.close();

    auto cmd = fmt::format("diff -q {} {}", case_name + ".seq.in", case_name + ".seq.out");
    int result = system(cmd.c_str());
    // Check the result, if you want to get the result of the diff command
    if (result == 0)
        spdlog::info("Files are identical");
    else
        spdlog::info("Files are different");
}

template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::parse_and_dump_sort(ErrorChecker&& error_checker)
{

    test_sort_create<ScoreValidator>("test", error_checker);
    spdlog::info("test_sort_create done");
    test_sort_dump("test");
}


typedef bm::dynamic_heap_matrix<unsigned, bvector_type::allocator_type> DPMatrix;

/**
  Result of longest common substring search
 */
struct LCS_result
{
  unsigned pos1 = 0;   ///< pos in string 1
  unsigned pos2 = 0;   ///< pos in string 2
  unsigned len = 0; /// LCS length

  void set(unsigned p1, unsigned p2, unsigned l) noexcept
    { pos1 = p1; pos2 = p2; len = l; }
};

/**
  Find longest common substring
 */
template<typename DPM>
LCS_result find_LCS(const std::string& in_str1, const std::string& in_str2, DPM& dp)
{
    auto len1 = in_str1.length();
    auto len2 = in_str2.length();

    dp.resize(typename DPM::size_type(len1)+1, typename DPM::size_type(len2)+1, false);
    dp.set_zero();

    const char* s1 = in_str1.c_str();
    const char* s2 = in_str2.c_str();

    unsigned max_len = 0;
    LCS_result result;

    for (size_t i = 1; i <= len1; ++i) {
        unsigned* row = dp.row(i);
        const unsigned* row_prev = dp.row(i-1);
        for (size_t j = 1; j <= len2; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                row[j] = row_prev[j-1] + 1;
                if (row[j] > max_len) {
                    max_len = row[j];
                    result.set(unsigned(i - max_len), unsigned(j - max_len), max_len);
                }
            }
        } // for j
    } // for i
    return result;
}

inline
size_t compute_match_count(const string& str1, const string& str2,
              size_t max_match_len,
              const LCS_result& lcs_res) noexcept
{
    const char* s1 = str1.c_str() + lcs_res.pos1;
    const char* s2 = str2.c_str() + lcs_res.pos2;
    size_t match_cnt = lcs_res.len;
    for (size_t i = lcs_res.len; i < max_match_len; ++i) {
        match_cnt += (*s1 == *s2);
        ++s1; ++s2;
        if (!s2)
        break;
    } // for i
    return match_cnt;
}

void cyclicRotateString(std::string& str, int N) 
{
    int len = str.length();

    // Adjust the rotation value to be within the string length
    N = N % len;
    if (N < 0) {
        // Reverse rotation
        N = len + N;
    }

    // Perform the rotation in place by swapping substrings
    std::reverse(str.begin(), str.begin() + N);
    std::reverse(str.begin() + N, str.end());
    std::reverse(str.begin(), str.end());
}


typedef bm::dynamic_heap_matrix<unsigned long long, bvector_type::allocator_type> LCS_ResultsMatrix;

/// Codes for heuristical decision maker for best set to pick
enum E_LCS_BestSet
{
  e_LCS,    ///< max length LCS substring (and all associalted mate subjects)
  e_LCS_maxsum, ///< maximum sum of all LCS substrings
  e_LCS_best_avg, ///< best LCS average: sum(lcs)/N
  e_LCS_Best_none  ///< no good variant to follow
};

inline
E_LCS_BestSet find_best_subject_set(size_t LCS_sum, size_t LCS_count,
                  size_t LCS_max_sum, size_t LCS_max_sum_cnt,
                  float LCS_best_avg, size_t LCS_avg_cnt) noexcept
                   
{
    const size_t min_set_count = 32; // to prevent really small sets of subjects from winning
    E_LCS_BestSet ret = (LCS_max_sum_cnt > min_set_count/2) ? e_LCS_maxsum : e_LCS_Best_none; // default choice

    float lcs_sum_avg = (LCS_count > min_set_count) ? float(LCS_sum) / float(LCS_count) : 0.0f;
    float lcs_max_sum_avg = (LCS_max_sum_cnt > min_set_count) ? float(LCS_max_sum) / float(LCS_max_sum_cnt) : 0.0f;

    // evaluate best average
    //
    // if BEST avg (SUM(lcs)/N is better than SUM(lcs)
    if ((LCS_best_avg > lcs_max_sum_avg) && (LCS_avg_cnt > min_set_count)) {
        float d = LCS_best_avg - lcs_max_sum_avg;
        if (d > 0.7f) { // gain is substantial enough to warrant a switch
            ret = e_LCS_best_avg; // new best established
            // evaluate longest LCS beats it
            if ((lcs_sum_avg > LCS_best_avg) && (LCS_count > min_set_count)) {
                d = lcs_sum_avg - LCS_best_avg;
                if (d > 0.7f) {
                    ret = e_LCS;
                }
            }
        }
    } else {
        // evaluate longest LCS beats sum(lcs)
        if ((lcs_sum_avg > lcs_max_sum_avg) && (LCS_count > min_set_count)) {
            float d = lcs_sum_avg - lcs_max_sum_avg;
            if (d > 0.7f) {
                ret = e_LCS;
            }
        }
    }
    return ret;
}


typedef struct {
    vector<string> seqs;
    vector<uint16_t> rotations;
    vector<uint32_t> idx;
    vector<uint16_t> distance;

} cluster_t;


int LevenshteinDistance(const std::string &s, const std::string &t) {
    if (s.empty()) return t.length();
    if (t.empty()) return s.length();

    std::vector<int> prev(t.length() + 1);
    std::vector<int> curr(t.length() + 1);

    for (size_t i = 0; i <= t.length(); ++i) {
        prev[i] = i;
    }
    
    for (size_t i = 0; i < s.length(); ++i) {
        curr[0] = i + 1;
        for (size_t j = 0; j < t.length(); ++j) {
            int add = (s[i] == t[j]) ? 0 : 1;
            curr[j + 1] = std::min({prev[j + 1] + 1, curr[j] + 1, prev[j] + add});
        }
        std::swap(prev, curr);
    }

    return prev[t.length()];
}


size_t rotate_seqs(tf::Executor& executor, vector<string>& seq_vect, size_t max_ssize, size_t offset, vector<cluster_t>& clusters)
{
    auto logger = spdlog::get("parser_logger");
    logger->info("rotating {} sequences", seq_vect.size());
    vector<uint16_t> rotations;
    rotations.resize(seq_vect.size());
    fill(rotations.begin(), rotations.end(), UINT16_MAX);

    std::vector<LCS_result> row_results;
    row_results.resize(seq_vect.size());    

    unsigned min_match = 6;
    //if (max_ssize > 300)
    //    min_match = 20;
    //else if (max_ssize > 200)
    //    min_match = 16;
    //else if (max_ssize > 100)
    //    min_match = 12;
    size_t num_seq = seq_vect.size();
    vector<char> freqs_chr(num_seq, ' ');
    vector<uint16_t> freqs_cnt(num_seq, 0);
    for (size_t i{0}; i < num_seq; ++i) {
        char prev_char = ' ';
        int cnt = 0;
        auto& s = seq_vect[i];
        for (size_t j = 0; j < s.size(); ++j) {
            if (s[j] == prev_char) {
                ++cnt;
            } else {
                if ( cnt > 12 && cnt > freqs_cnt[i]) {
                    freqs_chr[i] = prev_char;
                    freqs_cnt[i] = cnt;
                }
                cnt = 1;
                prev_char = s[j];
            }
        }
        if ( cnt > 12 && cnt > freqs_cnt[i]) {
            freqs_chr[i] = prev_char;
            freqs_cnt[i] = cnt;
        }
    }
    vector<size_t> v[6];
    {
        for (size_t i= 0; i < num_seq; ++i) {
            if (freqs_cnt[i] > 12) {
                size_t idx = -1;
                switch (freqs_chr[i]) {
                    case 'A': idx = 0; 
                    break;
                    case 'C': idx = 1; break;
                    case 'G': idx = 2; break;
                    case 'T': idx = 3; break;
                    case 'N': idx = 4; break;
                    default:
                        assert(false);
                }
                v[idx].push_back(i);
            } else {
                v[5].push_back(i);
            }
        }
    }
    
    
    size_t found_count = 0;
    tf::Taskflow taskflow;
    std::mutex best_match_mutex;

    std::vector<size_t> LCS_sum_vect;
    LCS_sum_vect.resize(max_ssize, 0); // resize and set to 0

    std::vector<size_t> LCS_cnt_vect;
    LCS_cnt_vect.resize(max_ssize, 0); // resize and set to 0
    //vector<cluster_t> clusters;
    size_t cluster_leader;
    cluster_t leftover_cluster;

    for (size_t v_idx = 0; v_idx < 6; ++v_idx) {
        auto& seq_v = v[v_idx];
        //if (found_count >= seq_vect.size())
        //    break;

    size_t LCS_cnt{0}, LCS_max_sum_cnt{0}, LCS_best_avg_cnt{0}, LCS_none{0};
    //for (size_t i{0}, c{0}; i < seq_vect.size(); ++i) {
    for (size_t idx = 0; idx < seq_v.size(); ++idx) {
        size_t i = seq_v[idx];
        assert(i < num_seq);
        if (rotations[i] != UINT16_MAX) 
            continue;
        //if (found_count >= seq_vect.size())
        //    break;
        fill(row_results.begin(), row_results.end(), LCS_result());
        const string& s_i = seq_vect[i];
        unsigned best_sim = 0;
        size_t best_mate_idx;
        //int first = i + 1;
        //int last = seq_vect.size();
        int first = idx + 1;
        int last = seq_v.size();
        taskflow.clear();
        taskflow.for_each_index(ref(first), ref(last), 1, [&](int j_idx) {
            size_t j = seq_v[j_idx];
            assert(j < num_seq);

            if (rotations[j] != UINT16_MAX) // we have a greedy algo here, it is ok to skip some
                return;
            const string& s_j = seq_vect[j];
            static thread_local DPMatrix dp;
            LCS_result result = find_LCS(s_i, s_j, dp);
            row_results[j] = result;
            if (result.len > min_match) { // minimal cut-off
                const lock_guard<mutex> lock(best_match_mutex);
                if (result.len > best_sim) {
                    best_sim = result.len; 
                    best_mate_idx = j;
                }
            }

        }); // for j
        //executor.run(taskflow).wait();
        executor.corun(taskflow);
            
        // find the max-sum position
        //
        size_t best_LCS_sum = 0;
        size_t best_LCS_pos = 0;

        float best_LCS_avg = 0;
        size_t best_LCS_avg_pos = 0;
        size_t LCS_bestsum_pos1 = 0;

        const LCS_result& max_LCS_res = row_results[best_mate_idx];
        auto LCS_max_pos1 = max_LCS_res.pos1;
        E_LCS_BestSet best_subj_set = e_LCS_Best_none;
        if (v_idx < 5 && best_sim > min_match) {           
            best_subj_set = e_LCS;
        } else {

            fill(LCS_sum_vect.begin(), LCS_sum_vect.end(), 0);
            fill(LCS_cnt_vect.begin(), LCS_cnt_vect.end(), 0);
    
            //for (size_t j = i + 1; j < seq_vect.size(); ++j) {

            for (size_t j_idx = v_idx + 1; j_idx < seq_v.size(); ++j_idx) {              
                size_t j = seq_v[j_idx];
                if (rotations[j] != UINT16_MAX) 
                    continue;
                const LCS_result& result = row_results[j];
                if (result.len < min_match)
                    continue;
                auto query_pos = result.pos1;
                assert(query_pos < LCS_sum_vect.size());
                LCS_sum_vect[query_pos] += result.len; // sum of all possible gains for a particular position on the query
                LCS_cnt_vect[query_pos]++; // register a hit
            } // for j

            for (size_t k = 0; k < LCS_sum_vect.size(); ++k) {
                if (LCS_cnt_vect[k]) {
                    if (LCS_sum_vect[k] > best_LCS_sum) {
                        best_LCS_sum = LCS_sum_vect[k];
                        best_LCS_pos = k;
                    }
                    float LCS_avg = float(LCS_sum_vect[k]) / float(LCS_cnt_vect[k]);
                    float d = LCS_avg - best_LCS_avg;
                    if (d > 0.1f ) {
                        best_LCS_avg = LCS_avg;
                        best_LCS_avg_pos = k;
                    }
                }
            } // for k
            LCS_bestsum_pos1 = best_LCS_pos;

            best_subj_set = find_best_subject_set(
                LCS_sum_vect[LCS_max_pos1], LCS_cnt_vect[LCS_max_pos1],
                LCS_sum_vect[LCS_bestsum_pos1], LCS_cnt_vect[LCS_bestsum_pos1],
                best_LCS_avg, LCS_cnt_vect[best_LCS_avg_pos]);
        }

        size_t query_pos = 0;
        bool result_flag = true;
        switch(best_subj_set) {
            case e_LCS: query_pos = LCS_max_pos1; break;
            case e_LCS_maxsum: query_pos = LCS_bestsum_pos1; break;
            case e_LCS_best_avg: query_pos = best_LCS_avg_pos; break;
            default:
                result_flag = false;
            break;
        } // switch
        if (result_flag) {
            //auto& cluster = clusters.emplace_back();
            auto& cluster = clusters.emplace_back();

            size_t cnt = 0;
            for (size_t j_idx = v_idx + 1; j_idx < seq_v.size(); ++j_idx) {              
                size_t j = seq_v[j_idx];
                assert(j < num_seq);

            //for (size_t j = i + 1; j < seq_vect.size(); ++j) {
                if (rotations[j] != UINT16_MAX)
                    continue;
                const LCS_result& sim_res = row_results[j];
                if ( sim_res.len && query_pos == sim_res.pos1 && sim_res.len >= min_match) { //&& sim_res.len >= min_match) { // same similarity position!
                    if (rotations[i] == UINT16_MAX) {
                        cluster.seqs.push_back(seq_vect[i]);
                        cluster.rotations.push_back(sim_res.pos1);
                        assert(i < num_seq);

                        cluster.idx.push_back(i);

                        //cluster.distance.push_back(0);
                        cluster_leader = i;

                        rotations[i] = sim_res.pos1;
                        ++found_count;
                        ++cnt;
                        //logger->info("Cluster leader: {}", seq_vect[i].substr(sim_res.pos1, min<int>(20, seq_vect[i].size() - sim_res.pos1));
                    }
                    rotations[j] = sim_res.pos2;
                    cluster.seqs.push_back(seq_vect[j]);
                    cluster.rotations.push_back(sim_res.pos2);
                    assert(j < num_seq);
                    cluster.idx.push_back(j);
                    //cluster.distance.push_back(LevenshteinDistance(seq_vect[j], cluster.seqs[0]));

                    ++cnt;
                    ++found_count;
                }
                //logger->info( "{}: Found={}/{}, min_match: {}", offset, found_count, seq_vect.size(), min_match);
            } // for j
        }
        if (rotations[i] == UINT16_MAX) {
            rotations[i] = 0;
            cluster_leader = i;

            leftover_cluster.seqs.push_back(seq_vect[i]);
            leftover_cluster.rotations.push_back(0);
            assert(i < num_seq);
            leftover_cluster.idx.push_back(i);
            ++found_count;
        }

    } // for i
    } // for v_idx
    if (leftover_cluster.seqs.size()) {
        clusters.push_back(move(leftover_cluster));
    }
    return cluster_leader;
}    

/*
vector<cluster_t> rotate_seqs(tf::Executor& executor, vector<string>& seq_vect, size_t max_ssize, size_t offset)
{
    auto logger = spdlog::get("parser_logger");
    logger->info("rotating {} sequences", seq_vect.size());
    vector<uint16_t> rotations;
    rotations.resize(seq_vect.size());
    fill(rotations.begin(), rotations.end(), UINT16_MAX);

    std::vector<LCS_result> row_results;
    row_results.resize(seq_vect.size());    

    //DPMatrix dp;
    //bvector_type bv_sim_found; // elements already picked for rotation
    //bv_sim_found.init();
    unsigned min_match = 4;
    if (max_ssize > 300)
        min_match = 20;
    else if (max_ssize > 200)
        min_match = 16;
    else if (max_ssize > 100)
        min_match = 12;
        
    size_t found_count = 0;
    tf::Taskflow taskflow;
    std::mutex best_match_mutex;
    std::vector<size_t> LCS_sum_vect;

    LCS_sum_vect.resize(max_ssize); // resize and set to 0     
    size_t cluster_id = 0;
    map<size_t, size_t> cluster_stat;
    vector<cluster_t> clusters;

    for (size_t i = 0; i < seq_vect.size(); ++i) {

        if (rotations[i] != UINT16_MAX) 
            continue;
        if (found_count >= seq_vect.size())
            break;
        //cout << i << ":" << endl;
        const string& s_i = seq_vect[i];
        unsigned best_sim = 0;
        size_t best_mate_idx;
        int first = i + 1;
        int last = seq_vect.size();
        //fill(row_results.begin(), row_results.end(), LCS_result());
        taskflow.clear();
        taskflow.for_each_index(ref(first), ref(last), 1, [&](int j) {
        //for (size_t j = i+1; j < seq_vect.size(); ++j) {
            if (rotations[j] != UINT16_MAX) // we have a greedy algo here, it is ok to skip some
                return;
            const string& s_j = seq_vect[j];
            static thread_local DPMatrix dp;
            LCS_result result = find_LCS(s_i, s_j, dp);
            row_results[j] = result;
            //if (result.len > min_match) { // minimal cut-off
            //    const lock_guard<mutex> lock(best_match_mutex);
            //    if (result.len > best_sim) {
            //        best_sim = result.len; 
            //        best_mate_idx = j;
            //    }
            //}

        }); // for j
        //executor.run(taskflow).wait();
        executor.corun(taskflow);
        //cout << " best-sim=" << best_sim << " ";

        fill(LCS_sum_vect.begin(), LCS_sum_vect.end(), 0);   

        for (size_t j = first; j < last; ++j) {
            if (rotations[j] != UINT16_MAX) 
                continue;

            const LCS_result& result = row_results[j];
            if (result.len == 0)
                continue;
            auto query_pos = result.pos1;
            //if (result.len < min_match) // minimal cut-off
              //  continue;
            if  (query_pos >= LCS_sum_vect.size()) {
                logger->error("query_pos {} out of range {}", query_pos, LCS_sum_vect.size());
                throw runtime_error("query_pos out of range");
            }
            assert(query_pos < LCS_sum_vect.size());
            LCS_sum_vect[query_pos] += result.len; // sum of all possible gains for a particular position on the query
        } // for j
        // find the max-sum position
        //
        //size_t best_LCS_sum = 0;
        //size_t best_LCS_pos = -1;
        auto maxElement = std::max_element(LCS_sum_vect.begin(), LCS_sum_vect.end());        
        size_t best_LCS_pos = maxElement - LCS_sum_vect.begin();
        // find other similar cases in the row
        //
        //if (best_LCS_pos != -1) { // any similarity found?
            auto& cluster = clusters.emplace_back();
            size_t cnt = 0;
            for (size_t j = i + 1; j < seq_vect.size(); ++j) {
                if (rotations[j] != UINT16_MAX)
                    continue;
                const LCS_result& sim_res = row_results[j];
                if ( sim_res.len && best_LCS_pos == sim_res.pos1 ) { //&& sim_res.len >= min_match) { // same similarity position!
                    if (rotations[i] == UINT16_MAX) {
                        cluster.seqs.push_back(seq_vect[i]);
                        cluster.rotations.push_back(sim_res.pos1);
                        cluster.idx.push_back(i);

                        rotations[i] = sim_res.pos1;
                        ++found_count;
                        ++cnt;
                    }
                    rotations[j] = sim_res.pos2;
                    cluster.seqs.push_back(seq_vect[j]);
                    cluster.rotations.push_back(sim_res.pos2);
                    cluster.idx.push_back(j);

                    ++cnt;
                    ++found_count;
                }
            } // for j
            logger->info( "{}: Found={}/{}, min_match: {}", offset, found_count, seq_vect.size(), min_match);
            cluster_stat[cluster_id++] = cnt;
        //}
    } // for i

//    for (int i = 0; i < clusters.size(); ++i) {
//        ofstream ofs("cluster_" + to_string(i));
//        for (int j = 0; j < clusters[i].seqs.size(); ++j) {
//            cyclicRotateString(clusters[i].seqs[j], clusters[i].rotations[j]);
//            ofs << clusters[i].seqs[j] << endl;
//        }
//    }
        
//    for (const auto& it : cluster_stat) {
//        logger->info("Cluster: {} size: {}", it.first, it.second);
//    }
    
    return clusters;
}    
*/



static
size_t test_rotate_save_batch(tf::Executor& executor, size_t offset, size_t max_size, vector<string>& seqs, const string& case_name)
{

    BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block    
    string name = case_name + "_" + to_string(offset);
    auto logger = spdlog::get("parser_logger");
    {
        ofstream os(name + ".input");
        for (const auto& s : seqs)
            os << s << endl;
    }
    size_t num_seqs = seqs.size();
    //max_size =max<int>(max_size/ 3, 50);
    size_t bwt_key = 0;

    logger->info("Starting bwt for {}", name);
    size_t orig_max_size = max_size;
    {
        size_t i = 2;
        while (true) {
            if ( orig_max_size * i > 100)
                break;
            max_size = orig_max_size * i; 
            ++i;       
        }
    }
    bwt_key = bwt_vector(max_size, seqs);
    logger->info("Finish bwt for {}", name);
    {
        ofstream os(name + ".bwt");
        for (const auto& s : seqs)
            os << s << endl;
    }
    
    vector<cluster_t> clusters;
    size_t cluster_leader = rotate_seqs(executor, seqs, max_size, offset, clusters);
    svector_u32 rotate_vec;
    auto r_bi = rotate_vec.get_back_inserter();

    svector_u32 len_vec;
    auto len_bi = len_vec.get_back_inserter();

    svector_u32 idx_vec;
    auto idx_bi = idx_vec.get_back_inserter();

    bmatr_32 len_matr(max_size); // create sample row major bit-matrix
    size_t base_count = 0;


    bmatr_32 bmatr0(max_size * 3); // create sample row major bit-matrix
    int first = 0, last = max_size*3;
    tf::Taskflow taskflow;
    size_t cluster_offset = 0;
    size_t cluster_id = 0;
    size_t num_clustered_seqs = 0;

    for (auto& cluster : clusters) {
        if (cluster.seqs.empty())
            continue;
        num_clustered_seqs += cluster.seqs.size();            
        for (size_t i = 0; i < cluster.seqs.size(); ++i) {
            assert(cluster.idx[i] < num_seqs);
            base_count += cluster.seqs[i].size();
            if (cluster.rotations[i] != 0) 
                cyclicRotateString(cluster.seqs[i], cluster.rotations[i]);                 
        }
        taskflow.clear();
        vector<size_t> idx(cluster.idx.size());
        iota(idx.begin(), idx.end(), 0);
        
        taskflow.sort(idx.begin(), idx.end(), [&](uint32_t  l, uint32_t  r) {
            return cluster.seqs[l] < cluster.seqs[r];
        });
        executor.corun(taskflow);
        //ofstream ofs(name + ".cluster_" + to_string(cluster_id++));       
        for (size_t i = 0; i < idx.size(); ++i) {
            *r_bi = cluster.rotations[idx[i]];
            len_bi = cluster.seqs[idx[i]].size();
            assert (cluster.idx[idx[i]] < seqs.size());
            idx_bi = cluster.idx[idx[i]];
           // ofs << cluster.seqs[idx[i]] << '\n';
        }

        taskflow.clear();
        taskflow.for_each_index(std::ref(first), std::ref(last), 1, [&] (int i) {
                bmatr_32::bvector_type* bv1 = bmatr0.construct_row(i);
                size_t seq_idx = i/3;
                size_t bv_idx = i % 3;
                for (size_t j = 0; j < cluster.seqs.size(); ++j) {
                    auto& seq = cluster.seqs[idx[j]];
                    if (seq_idx >= seq.size()) continue;
                    auto v = DNA2int(seq[seq_idx]);
                    v >>= bv_idx;
                    if (v & 1) bv1->set_bit_no_check(j + cluster_offset);
                }
            }
        );
        executor.corun(taskflow);
        cluster_offset += cluster.seqs.size();
    }
    assert(num_clustered_seqs == num_seqs);
    r_bi.flush();
    rotate_vec.optimize(TB);
    bm::file_save_svector(rotate_vec, name + ".rot");        

    idx_bi.flush();
    idx_vec.optimize(TB);
    bm::file_save_svector(idx_vec, name + ".idx");        

    len_bi.flush();
    len_vec.optimize(TB);
    bm::file_save_svector(len_vec, name + ".len");        

    bmatr0.optimize(TB);
    save_matrix(name + ".matrix", bmatr0);
    auto batch_size = fs::file_size(name + ".rot") + fs::file_size(name + ".len") + fs::file_size(name + ".idx") + fs::file_size(name + ".matrix");
    float bits_per_base = 0;
    if (base_count) {
        bits_per_base = float(batch_size * 8)/base_count;
    }
    spdlog::info("{} Batch size {}, num_rows {}, base_count {}, bits/base {}", offset, batch_size, max_size, base_count, bits_per_base);
    {
        ofstream os(name + ".info");
        os << "Batch size (bytes)  : " << batch_size << endl;
        os << "max_seq_size        : " << orig_max_size << endl;
        os << "Number of bases     : " << base_count << endl;
        os << "bits/base ratio     : " << bits_per_base << endl;
        os << "bwt_key             : " << bwt_key << endl;   
     }
    return batch_size;
}


/*
static
size_t test_rotate_save_batch(tf::Executor& executor, size_t offset, size_t max_size, vector<string>& seqs, const string& case_name)
{

    BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block    
    string name = case_name + "_" + to_string(offset);
    size_t base_count = 0;
    {
        auto rotations = rotate_seqs(executor, seqs, max_size, offset);
        svector_u32 rotate_vec;
        auto r_bi = rotate_vec.get_back_inserter();
        ofstream ofs1(name + ".original");
        ofstream ofs2(name + ".rotated");

        for (size_t i = 0; i < seqs.size(); ++i) {
            base_count += seqs[i].size();
            ofs1 << seqs[i] << endl;
            if (rotations[i] != UINT16_MAX && rotations[i] != 0) {
                *r_bi = rotations[i];
                cyclicRotateString(seqs[i], rotations[i]);                 
            } else {
                *r_bi = 0;
            }
            ofs2 << seqs[i] << endl;
        }
        ofs1.close();
        ofs2.close();
        r_bi.flush();
        rotate_vec.optimize(TB);
        bm::file_save_svector(rotate_vec, name + ".rot");        
    }

    vector<size_t> idx(seqs.size());
    iota(idx.begin(), idx.end(), 0);
    tf::Taskflow taskflow;
    taskflow.sort(idx.begin(), idx.end(), [&](uint32_t  l, uint32_t  r) {
        return seqs[l] < seqs[r];
    });
    //executor.run(taskflow).wait();
    executor.corun(taskflow);
    svector_u32 len_vec;
    auto len_bi = len_vec.get_back_inserter();

    svector_u32 idx_vec;
    auto idx_bi = idx_vec.get_back_inserter();

    bmatr_32 len_matr(max_size); // create sample row major bit-matrix
    //ofstream ofs(name + ".rotated");

    for (size_t j = 0; j < idx.size(); ++j) {
        auto& seq = seqs[idx[j]];
        len_bi = seq.size();
        idx_bi = idx[j];
        //ofs << seq << endl;
    }
    //ofs.close();
    len_bi.flush();
    idx_bi.flush();
    len_vec.optimize(TB);
    bm::file_save_svector(len_vec, name + ".len");        

    idx_vec.optimize(TB);
    bm::file_save_svector(idx_vec, name + ".idx");        
    //spdlog::info("Creating matrix");
    bmatr_32 bmatr0(max_size * 3); // create sample row major bit-matrix

    int first = 0, last = max_size*3;

    taskflow.clear();
    taskflow.for_each_index(std::ref(first), std::ref(last), 1, [&] (int i) {
            bmatr_32::bvector_type* bv1 = bmatr0.construct_row(i);
            size_t seq_idx = i/3;
            size_t bv_idx = i % 3;
            for (size_t j = 0; j < idx.size(); ++j) {
                auto& seq = seqs[idx[j]];
                if (seq_idx >= seq.size()) continue;
                auto v = DNA2int(seq[seq_idx]);
                v >>= bv_idx;
                if (v & 1) bv1->set_bit_no_check(j);
            }
        }
    );
    //executor.run(taskflow).wait();
    executor.corun(taskflow);
    bmatr0.optimize(TB);
    save_matrix(name + ".matrix", bmatr0);

    auto batch_size = fs::file_size(name + ".rot") + fs::file_size(name + ".len") + fs::file_size(name + ".idx") + fs::file_size(name + ".matrix");
    float bits_per_base = 0;
    if (base_count) {
        bits_per_base = float(batch_size * 8)/base_count;
    }
    spdlog::info("{} Batch size {}, num_rows {}, base_count {}, bits/base {}", offset, batch_size, max_size, base_count, bits_per_base);
    {
        ofstream os(name + ".info");
        os << "Batch size (bytes)  : " << batch_size << endl;
        os << "Max sequence length : " << max_size << endl;
        os << "Number of bases     : " << base_count << endl;
        os << "bits/base ratio     : " << bits_per_base << endl;
    }
    return batch_size;
}
*/

static 
void test_rotate_dump(const string& case_name)
{
    size_t batch_id = 0;

    tf::Executor executor(min<int>(12, std::thread::hardware_concurrency())); 
    tf::Taskflow taskflow;

    ofstream seq_out(case_name + ".seq.out");

    while (true) {
        string batch_name = case_name + "_" + to_string(batch_id);
        if (!fs::exists(batch_name + "_0.matrix"))
            break;
        size_t case_idx = 0;
        vector<vector<string>> spots;
        while (true) {
            string name = batch_name + "_" + to_string(case_idx);
            if (!fs::exists(name + ".matrix"))
                break;
            ++case_idx;
            vector<uint32_t> lens;
            {
                svector_u32 len_vec;
                bm::file_load_svector(len_vec, name + ".len");        
                lens.resize(len_vec.size());
                len_vec.decode(&lens[0], 0, len_vec.size());
            }
            vector<uint32_t> idx;
            {
                svector_u32 idx_vec;
                bm::file_load_svector(idx_vec, name + ".idx");
                idx.resize(idx_vec.size());
                idx_vec.decode(&idx[0], 0, idx.size());

            }
            vector<uint32_t> rotations;
            {
                svector_u32 rot_vec;
                bm::file_load_svector(rot_vec, name + ".rot");
                rotations.resize(rot_vec.size());
                rot_vec.decode(&rotations[0], 0, rotations.size());
            }

            bmatr_32 bmatr(0); // create sample row major bit-matrix
            load_matrix(name + ".matrix", bmatr);
            size_t num_seq = idx.size();
            
            vector<string> seqs(num_seq);
            for (size_t i = 0; i < num_seq; ++i) 
                seqs[idx[i]].resize(lens[i]);

            size_t num_rows = bmatr.rows() / 3;
            int first = 0;
            int last = num_rows;
            taskflow.clear();
            taskflow.for_each_index(std::ref(first), std::ref(last), 1, [&] (int i) {

            //for (size_t i = 0; i < num_rows; ++i) {
                bmatr_32::bvector_type* bv1 = bmatr.get_row(i * 3);
                bmatr_32::bvector_type* bv2 = bmatr.get_row(i * 3 + 1);
                bmatr_32::bvector_type* bv3 = bmatr.get_row(i * 3 + 2);

                for (size_t j = 0; j < num_seq; ++j) {
                    auto& seq = seqs[idx[j]];
                    if (i >= seq.size()) continue;
                    uint8_t v = 0;
                    if (bv3->test(j))
                        v = 1 << 2;
                    if (bv2->test(j)) 
                        v |= 1 << 1;
                    if (bv1->test(j))
                    v |= 1; 
                    seq[i] = Int2DNA(v);
                }
            });
            executor.run(taskflow).wait();
            

            vector<uint32_t> idx1(idx.size());
            iota(idx1.begin(), idx1.end(), 0);
            sort(idx1.begin(), idx1.end(), [&](size_t l, size_t r) {
                return idx[l] < idx[r];
            });

            for (size_t i = 0; i < num_seq; ++i) {
                auto k = idx1[i];
                if (rotations[k]) 
                    cyclicRotateString(seqs[i], -rotations[k]);                 
            }

            auto [bwt_key, max_size] = s_get_info(name + ".info");
            auto logger = spdlog::get("parser_logger");
            logger->info("bwt_key {}, max_size {}", bwt_key, max_size);
            unbwt_vector(bwt_key, max_size, seqs);
            
            spots.push_back(move(seqs));
        }
        size_t read_id = 0;

        string s;
        while (true) {
            for (const auto& reads : spots) {
                if (read_id < reads.size()) {
                    s+= reads[read_id];
                }
            }
            if (s.empty()) break;
            seq_out << s << endl;
            s.clear();
            ++read_id;
        }
        ++batch_id;
    }
    seq_out.close();
    auto cmd = fmt::format("diff -q --text {} {}", case_name + ".seq.in", case_name + ".seq.out");
    int result = system(cmd.c_str());
    // Check the result, if you want to get the result of the diff command
    if (result == 0)
        spdlog::info("Files are identical");
    else
        spdlog::info("Files are different");
}


template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::parse_and_dump_rotate(ErrorChecker&& error_checker)
{
    test_each_batch<ScoreValidator>("test", error_checker, 10000000, test_rotate_save_batch);
    spdlog::info("parse_and_dump_rotate done");
    test_rotate_dump("test");

}





#endif

