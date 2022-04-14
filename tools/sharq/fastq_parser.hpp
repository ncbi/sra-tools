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


#include "fastq_read.hpp"
#include "fastq_error.hpp"
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
#include <zlib.h>
#include <chrono>
#include <json.hpp>
#include <set>
#include <limits>

#include "fastq_defline_parser.hpp"

using namespace std;

bm::chrono_taker::duration_map_type timing_map;
typedef bm::bvector<> bvector_type;
typedef bm::str_sparse_vector<char, bvector_type, 32> str_sv_type;
using json = nlohmann::json;

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
     * @param[in] platform expected platfrom code
     * @param[in] match_all if True no failure on unspoorted deflines
     */
    fastq_reader(const string& file_name, shared_ptr<istream> _stream, vector<char> read_type = {'B'}, int platform = 0, bool match_all = false)
        : m_file_name(file_name)
        , m_stream(_stream)
        , m_read_type(read_type)
        , m_read_type_sz(m_read_type.size())
        , m_curr_platform(platform)
    {
        m_stream->exceptions(std::ifstream::badbit);
        if (match_all)
            m_defline_parser.SetMatchAll();
    }
    ~fastq_reader() = default;

    /**
     * @brief Returns defline's platform code
     *
     * @return uint8_t
     */
    uint8_t platform() const { return m_defline_parser.GetPlatform();}

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


    /**
     * @brief Retrieves next spot in the stream
     *
     * @param[out] spot name of the next spot
     * @param[out] reads vector of reads belonging to the spot
     * @return false if readers reaches EOF
     */
    template<typename ScoreValidator = validator_options<>>
    bool get_next_spot(string& spot_name, vector<CFastqRead>& reads);

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


    bool eof() const { return m_buffered_spot.empty() && m_stream->eof();}  ///< Returns true if file has no more reads

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


template<typename TWriter>
class fastq_parser
/// FASTQ parser: reads from a group of readers and assembles the spots
{
public:

    /**
     * @brief Construct a new fastq parser object
     *
     * @param[in] writer
     */
    fastq_parser(shared_ptr<TWriter> writer) :
        m_writer(writer) {}

    ~fastq_parser() {
//        bm::chrono_taker::print_duration_map(timing_map, bm::chrono_taker::ct_ops_per_sec);
    }

    /**
     * @brief Set up readers from prepared digest data
     *
     * @param data[in[
     */
    void set_readers(json& data);

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
     * @brief read from a group of readers, assembles the spot and send it to the writer
     *
     * @tparam ErrorChecker
     * @param err_checker - lambda invoked on fastq_error during read parsing
     */
    template<typename ScoreValidator, typename ErrorChecker>
    void parse(ErrorChecker&& err_checker);

    /**
     * @brief Collation check
     *
     * Check if more than one name exists in the collected m_spot_names dictionary
     *
     */
    void check_duplicates();

    /**
     * @brief Reports collected telemetry via json
     *
     * @param j[out]
     */
    void report_telemetry(json& j);
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
    void update_telemetry(const vector<CFastqRead>& reads);

    shared_ptr<TWriter>  m_writer;                     ///< FASTQ writer
    vector<fastq_reader> m_readers;                    ///< List of readers
    str_sv_type          m_spot_names;                 ///< Run-time collected spot name dictionary
    bool                 m_allow_early_end{false};     ///< Allow early file end flag
    string               m_spot_file;                  ///< Optional file name for spot_name dictionary
    str_sv_type::back_insert_iterator m_spot_names_bi; ///< Internal back_inserter for spot_names collection
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
    m_defline_parser.Parse(m_line_view, read); // may throw

    // sequence
    GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
    while (!m_line_view.empty() && m_line_view[0] != '+' && m_line_view[0] != '@' && m_line_view[0] != '>') {
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
                read.AddQualityLine(m_line_view);
                if (read.Quality().size() >= sequence_size)
                    break;
                GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
                if (m_line_view.empty())
                    break;
                if (m_line_view[0] == '@' && m_defline_parser.Match(m_line_view, true)) {
                    m_buffered_defline = m_line;
                    break;
                }
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
    } catch (fastq_error& e) {
        e.set_file(m_file_name, read.LineNumber());
        throw;
    }
    return true;
}

template<typename ScoreValidator>
void fastq_reader::validate_read(CFastqRead& read)
{
    if (read.Sequence().empty())
        throw fastq_error(110, "Read {}: no sequence data", read.Spot());
    if (read.Quality().empty())
        throw fastq_error(111, "Read {}: no quality scores", read.Spot());
    // check isalpha
    if (std::any_of(read.Sequence().begin(), read.Sequence().end(), [](const char& c) { return !isalpha(c);}))
        throw fastq_error(160, "Read {}: invalid sequence characters", read.Spot());

    if constexpr (ScoreValidator::type() == eNumeric) {
        num_qual_validator<ScoreValidator>(read);
    } else if constexpr (ScoreValidator::type() == ePhred) {
        char_qual_validator<ScoreValidator>(read);
    }
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
    CFastqRead read;
    if (m_pending_spot.empty()) {
        if (!get_read<ScoreValidator>(read))
            return false;
        spot_name = read.Spot();
        reads.push_back(move(read));
    } else {
        reads.swap(m_pending_spot);
        spot_name = reads.front().Spot();
    }

    while (get_read<ScoreValidator>(read)) {
        if (read.Spot() == spot_name) {
            reads.push_back(move(read));
            continue;
        }
        m_pending_spot.push_back(move(read));
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

    string spot;
    vector<CFastqRead> next_reads;
    if (!get_next_spot<ScoreValidator>(spot, next_reads))
        return false;
    if (spot == spot_name) {
        swap(next_reads, reads);
        return true;
    }
    if (!m_pending_spot.empty() && m_pending_spot.front().Spot() == spot_name) {
        get_next_spot<ScoreValidator>(spot, reads);
        swap(m_buffered_spot, next_reads);
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
void fastq_reader::num_qual_validator(CFastqRead& read)
{
    if (m_curr_platform != m_defline_parser.GetPlatform())
        throw fastq_error(70, "Input file has data from multiple platforms ({} != {})", m_curr_platform, m_defline_parser.GetPlatform());

    m_tmp_str.clear();
    read.mQualScores.clear();
    uint8_t score;
    for (auto c : read.mQuality) {
        if (isspace(c)) {
            if (!m_tmp_str.empty()) {
                try {
                    score = stoi(m_tmp_str);
                } catch (invalid_argument&) {
                    throw fastq_error(120, "Read {}: invalid quality score value '{}'", read.Spot(), m_tmp_str);
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
            throw fastq_error(120, "Read {}: invalid quality score value '{}'", read.Spot(), m_tmp_str);
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
}


//  ----------------------------------------------------------------------------
template<typename ScoreValidator>
void fastq_reader::char_qual_validator(CFastqRead& read)
{
    if (m_curr_platform != m_defline_parser.GetPlatform())
        throw fastq_error(70, "Input file has data from multiple platforms ({} != {})", m_curr_platform, m_defline_parser.GetPlatform());

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
        if (qual_size == 0)
            throw fastq_error(111, "Read {}: no quality scores", read.Spot());
        read.mQuality.append(sz - qual_size,  char(ScoreValidator::min_score() + 30));
    }
}



BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block

//  ----------------------------------------------------------------------------
static
void serialize_vec(const string& file_name, str_sv_type& vec)
{
    //bm::chrono_taker tt1("Serialize acc list", 1, &timing_map);
    bm::sparse_vector_serializer<str_sv_type> serializer;
    bm::sparse_vector_serial_layout<str_sv_type> sv_lay;
    vec.optimize(TB);
    serializer.serialize(vec, sv_lay);
    ofstream ofs(file_name.c_str(), ofstream::out | ofstream::binary);
    const unsigned char* buf = sv_lay.data();
    ofs.write((char*)&buf[0], sv_lay.size());
    ofs.close();
}


/**
 * gets top spot spot name for each reader
 * and retrieves next reads belonging to the same spot from other readers
 * to assembly the spot
 */
template<typename TWriter>
template<typename ScoreValidator, typename ErrorChecker>
void fastq_parser<TWriter>::parse(ErrorChecker&& error_checker)
{
    size_t spotCount = 0;
    size_t readCount = 0;
    size_t currCount = 0;
    spdlog::stopwatch sw;
    spdlog::info("Parsing from {} files", m_readers.size());

    int num_readers = m_readers.size();
    vector<CFastqRead> curr_reads;
    auto spot_names_bi = m_spot_names.get_back_inserter();
    vector<vector<CFastqRead>> spot_reads(num_readers);
    vector<CFastqRead> assembled_spot;

    string spot;
    bool has_spots;
    bool check_readers = false;
    int early_end = 0;
    do {
        has_spots = false;
        early_end = 0;
        for (int i = 0; i < num_readers; ++i)
        try {
            if (!m_readers[i] . template get_next_spot<ScoreValidator>(spot, spot_reads[i])) {
                ++early_end;
                check_readers = num_readers > 1 && m_allow_early_end == false;
                continue;
            }

            has_spots = true;

            for (int j = 0; j < num_readers; ++j) {
                if (j == i) continue;
                m_readers[j] . template get_spot<ScoreValidator>(spot, spot_reads[j]);
            }

            assembled_spot.clear();
            for (auto& r : spot_reads) {
                if (!r.empty()) {
                    move(r.begin(), r.end(), back_inserter(assembled_spot));
                    readCount += r.size();
                    currCount += r.size();
                    r.clear();
                }
            }
            assert(assembled_spot.empty() == false);
            auto spot_size = assembled_spot.size();
            if (spot_size > 0) {
                if (spot_size > 4) {
                    throw fastq_error(210, "Spot {} has more than 4 reads", assembled_spot.front().Spot());
                } else {
                    m_writer->write_spot(assembled_spot);
                    spot_names_bi = assembled_spot.front().Spot();
                    update_telemetry(assembled_spot);
                    ++spotCount;
                    if (currCount >= 10e6) {
                        //m_writer->progressMessage(fmt::format("spots: {:L}, reads: {:L}", spotCount, readCount));
                        spdlog::info("spots: {:L}, reads: {:L}", spotCount, readCount);
                        currCount = 0;
                    }
                }
            }
        } catch (fastq_error& e) {
            error_checker(e);
            has_spots = true;  /// we don't want interrupt too early in case of an exception
            ++m_telemetry.groups.back().rejected_spots;
       }

        if (has_spots == false)
            break;

        if (early_end && early_end < num_readers)
            m_telemetry.groups.back().is_early_end = true;

        if (check_readers) {
            vector<int> reader_idx;
            for (int i = 0; i < num_readers; ++i) {
                const auto& reader = m_readers[i];
                if (reader.eof())
                    reader_idx.push_back(i);
            }
            if ((int)reader_idx.size() == num_readers) // all readers are at EOF
                break;
            if (!reader_idx.empty() && (int)reader_idx.size() < num_readers)
                throw fastq_error(180, "{} ended early at line {}. Use '--allowEarlyFileEnd' to allow load to finish.",
                        m_readers[reader_idx.front()].file_name(), m_readers[reader_idx.front()].line_number());
        }

    } while (true);


    spdlog::info("spots: {:L}, reads: {:L}", spotCount, readCount);
    if (m_telemetry.groups.back().rejected_spots > 0)
        spdlog::info("rejected spots: {:L}", m_telemetry.groups.back().rejected_spots);
    spdlog::debug("parsing time: {}", sw);
    spot_names_bi.flush();
    // update telemetry
    for(const auto& reader : m_readers) {
        m_telemetry.groups.back().defline_types
            .insert(reader.AllDeflineTypes().begin(), reader.AllDeflineTypes().end());
    }
}

template<typename TWriter>
void fastq_parser<TWriter>::update_telemetry(const vector<CFastqRead>& reads)
{
    auto& telemetry = m_telemetry.groups.back();
    telemetry.number_of_spots += 1;
    telemetry.number_of_reads += reads.size();
    for (const auto& r : reads) {
        auto sz = r.Sequence().size();
        telemetry.max_sequence_size = max<int>(sz, telemetry.max_sequence_size);
        telemetry.min_sequence_size = min<size_t>(sz, telemetry.min_sequence_size);
    }
    if ((int)reads.size() < telemetry.reads_per_spot)
        ++telemetry.number_of_spots_with_orphans;
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
static
int s_search_terms(str_sv_type& vec, bm::sparse_vector_scanner<str_sv_type>& scanner, const vector<string>& terms)
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
            return i;
        }
    }
    return -1;
}


//  ----------------------------------------------------------------------------
template<typename TWriter>
void fastq_parser<TWriter>::check_duplicates()
{
    spdlog::stopwatch sw;
    spdlog::info("spot_name check: {} spots", m_spot_names.size());
    str_sv_type sv;
    sv.remap_from(m_spot_names);
    m_spot_names.swap(sv);
    spdlog::debug("remapping done...");

    if (!m_spot_file.empty())
        serialize_vec(m_spot_file, m_spot_names);

    spot_name_check name_check(m_spot_names.size());

    bm::sparse_vector_scanner<str_sv_type> str_scan;

    size_t index = 0, hits = 0, sz = 0, last_hits = 0;
    const char* value;
    auto it = m_spot_names.begin();
    vector<string> search_terms;
    search_terms.reserve(10000);

    while (it.valid()) {
        value = it.value();
        sz = strlen(value);
        if (name_check.seen_before(value, sz)) {
            ++hits;
            search_terms.push_back(value);
            if (search_terms.size() == 10000) {
                int idx = s_search_terms(m_spot_names, str_scan, search_terms);
                if (idx >= 0)
                    throw fastq_error(170, "Collation check. Duplicate spot '{}' at index {}", search_terms[idx], idx);
                search_terms.clear();
             }
        }
        it.advance();
        if (++index % 100000000 == 0) {
            spdlog::debug("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
            last_hits = hits;
        }
    }
    if (!search_terms.empty()) {
        int idx = s_search_terms(m_spot_names, str_scan, search_terms);
        if (idx >= 0)
            throw fastq_error(170, "Collation check. Duplicate spot '{}' at index {}", search_terms[idx], idx);
    }
    spdlog::debug("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{:L}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
    spdlog::debug("spot_name check time:{}, num_hits: {:L}", sw, hits);
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
                throw fastq_error(140, "Read {}: quality score contains unexpected character '{}'", read.Spot(), str);
            }
            if (!params.set_score(score))
                throw fastq_error(140, "Read {}: quality score contains unexpected character '{}'", read.Spot(), str);
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


template<typename T>
static string s_join(const T& b, const T& e, const string& delimiter = ", ")
{
    stringstream ss;
    auto it = b;
    ss << *it++;
    while (it != e) {
        ss << delimiter << *it++;
    }
    return ss.str();
}


//  ----------------------------------------------------------------------------
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
    vector<fastq_reader> readers;
    vector<char> readTypes;
    for (const auto& fn : files)
        readers.emplace_back(fn, s_OpenStream(fn, 1024*1024), readTypes, 0, true);
    vector<uint8_t> placed(files.size(), 0);

    for (size_t i = 0; i < files.size(); ++i) {
        if (placed[i]) continue;
        placed[i] = 1;
        string spot;
        if (!readers[i].get_next_spot<>(spot, reads))
            throw fastq_error(50 , "File '{}' has no reads", readers[i].file_name());
        vector<string> batch{readers[i].file_name()};
        for (size_t j = 0; j < files.size(); ++j) {
            if (placed[j]) continue;
            if (readers[j].get_spot<>(spot, reads)) {
                placed[j] = 1;
                batch.push_back(readers[j].file_name());
            }
        }
        if (!batches.empty() && batches.front().size() != batch.size()) {
            string first_group = s_join(batches.front().begin(), batches.front().end());
            string second_group = s_join(batch.begin(), batch.end());
            throw fastq_error(11, "Inconsistent file sets: first group ({}), second group ({})", first_group, second_group);
        }
        spdlog::info("File group: {}", s_join(batch.begin(), batch.end()));
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

    while (it.valid()) {
        value = it.value();
        sz = strlen(value);
        {
            bm::chrono_taker tt1("check hits", 1, &timing_map);
            hit = name_check.seen_before(value, sz);
        }
        if (hit) {
            ++hits;
            search_terms.push_back(value);
            if (search_terms.size() == 10000) {
                bm::chrono_taker tt1("scan", search_terms.size(), &timing_map);
                int idx = s_search_terms(vec, str_scan, search_terms);
                if (idx >= 0)
                    throw fastq_error(170, "Duplicate spot '{}'", search_terms[idx]);
                search_terms.clear();
             }
        }
        it.advance();
        if (++index % 100000000 == 0) {
            spdlog::debug("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
            last_hits = hits;
            bm::chrono_taker::print_duration_map(timing_map, bm::chrono_taker::ct_all);
            timing_map.clear();
        }
    }
    if (!search_terms.empty()) {
        bm::chrono_taker tt1("scan", search_terms.size(), &timing_map);
        int idx = s_search_terms(vec, str_scan, search_terms);
        if (idx >= 0)
            throw fastq_error(170, "Duplicate spot '{}'", search_terms[idx]);
    }

    {
        spdlog::debug("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
        bm::chrono_taker::print_duration_map(timing_map, bm::chrono_taker::ct_all);
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
void get_digest(json& j, const vector<vector<string>>& input_batches, ErrorChecker&& error_checker, int num_reads_to_check = 250000)
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
            } else if (params.min_score >= 64 && params.max_score <= 126) {
                f["quality_encoding"] = 64;
            } else if (params.min_score >= 33 && params.max_score <= 126) {
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


template<typename TWriter>
void fastq_parser<TWriter>::set_readers(json& group)
{
    m_readers.clear();
    if (group["files"].empty())
        return;
    for (auto& data : group["files"]) {
        const string& name = data["file_path"];
        m_readers.emplace_back(name, s_OpenStream(name, (1024 * 1024) * 10), data["readType"], data["platform_code"].front());
    }
    set_telemetry(group);
}

template<typename TWriter>
void fastq_parser<TWriter>::report_telemetry(json& j)
{
    j["platform_code"] = m_telemetry.platform_code;
    j["quality_code"] = m_telemetry.quality_code;

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
    }
}


#endif

