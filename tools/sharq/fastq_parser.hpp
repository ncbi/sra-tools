#ifndef __FASTQ_PRASER_HPP__
#define __FASTQ_PRASER_HPP__
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
* Author:  Many, by the time it's done.
*
* File Description:
*
* ===========================================================================
*/

#define LOCALDEBUG

#include "fastq_read.hpp"
#include "fastq_error.hpp"
#include "fastq_defline_parser.hpp"
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

using namespace std;

bm::chrono_taker::duration_map_type timing_map;
typedef bm::bvector<> bvector_type;
typedef bm::str_sparse_vector<char, bvector_type, 32> str_sv_type;


//  ============================================================================
class fastq_reader
//  ============================================================================
{
public:    
    fastq_reader(const string& file_name, shared_ptr<istream> _stream, char read_type = 'B') 
        : m_file_name(file_name)
        , m_stream(_stream)
        , m_read_type(read_type)
    {
        //m_stream->exceptions(std::ifstream::failbit | std::ifstream::badbit);
        m_stream->exceptions(std::ifstream::badbit);
    }
    ~fastq_reader() = default;
    uint8_t platform() const { return m_defline_parser.GetPlatform();}
    char read_type() const { return m_read_type;}
    bool parse_read(CFastqRead& read);
    bool get_read(CFastqRead& read);

    /**
     * Retrieves next spot in the stream 
     *
     * @param[out] spot name of the next spot 
     * @param[out] reads vector of reads belonging to the spot 
     * @return false if readers reaches EOF
     */
    bool get_next_spot(string& spot_name, vector<CFastqRead>& reads);

    /**
     * Searches the reads for the spot 
     * 
     * search depth is just one spot 
     * so the reads have to be either next or one spot down the stream
     * 
     * @param[in] spot name
     * @param[out] reads vector of reads for the spot 
     * @return true if reads are found 
     */

    bool get_spot(const string& spot_name, vector<CFastqRead>& reads);
    void set_qual_validator(bool is_numeric, int min_score, int max_score);

    bool eof() const { return m_buffered_spot.empty() && m_stream->eof();}  ///< Returns true file has no more reads
    size_t line_number() const { return m_line_number; } ///< Returns current line number (1-based)
    const string& file_name() const { return m_file_name; }  ///< Returns reader's file name

    static void cluster_files(const vector<string>& files, vector<vector<string>>& batches);
private:
    static void dummy_validator(CFastqRead& read, pair<int, int>& expected_range);
    static void num_qual_validator(CFastqRead& read, pair<int, int>& expected_range);
    static void char_qual_validator(CFastqRead& read, pair<int, int>& expected_range);

    CDefLineParser      m_defline_parser;       ///< Defline parser 
    string              m_file_name;            ///< Corresponding file name
    shared_ptr<istream> m_stream;               ///< reader's stream
    char                m_read_type = 'B';      ///< Reader's readType (T|B|A), A - illumina, set based on read length
    size_t              m_line_number = 0;      ///< Line number counter (1-based)
    string              m_buffered_defline;     ///< Defline already read from the stream but not placed in a read
    vector<CFastqRead>  m_buffered_spot;        ///< Spot already read from the stream but no returned to the consumer
    vector<CFastqRead>  m_pending_spot;         ///< Partial spot with the first read only 
    bool                m_is_qual_score_numeric = true; ///< Quality score is numeric and qual score size == sequence size
    pair<int, int> m_qual_score_range = {33, 74};       ///< Expected quality score range
    function<void(CFastqRead&, pair<int,int>&)> m_qual_validator = dummy_validator; ///< Quality score validator function
    string              m_line;
    string_view         m_line_view;
};

//  ----------------------------------------------------------------------------
static
shared_ptr<istream> s_OpenStream(const string& filename)
//  ----------------------------------------------------------------------------
{
    shared_ptr<istream> is = (filename != "-") ? shared_ptr<istream>(new bxz::ifstream(filename)) : shared_ptr<istream>(new bxz::istream(std::cin)); 
    if (!is->good())
        throw runtime_error("Failure to open '" + filename + "'");
    return is;        
}


//  ============================================================================
template<typename TWriter>
class fastq_parser
//  ============================================================================
{
public:    
    fastq_parser(shared_ptr<TWriter> writer) :
        m_writer(writer) {}
    ~fastq_parser() {
//        bm::chrono_taker::print_duration_map(timing_map, bm::chrono_taker::ct_ops_per_sec);
    }
    void setup_readers(const vector<string>& files, const vector<char>& read_types, uint8_t platform = 0, int num_reads_to_check = 100000);
    void set_spot_file(const string& spot_file) { m_spot_file = spot_file; }
    void set_allow_early_end(bool allow_early_end = true) { m_allow_early_end = allow_early_end; }
    void parse();
    void check_duplicates();
private:

    shared_ptr<TWriter>  m_writer;
    vector<fastq_reader> m_readers;
    str_sv_type          m_spot_names;
    bool                 m_allow_early_end{false};     ///< Allow early file end
    string               m_spot_file;
    str_sv_type::back_insert_iterator m_spot_names_bi;
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
/*
static 
void TruncateSpacesInPlace(string& str)
{
    auto length = str.length();
    if (length == 0) {
        return;
    }
    size_t  beg = 0;
    while ( isspace((unsigned char) str.data()[beg]) ) {
        if (++beg == length) {
            str.erase();
            return;
        }
    }

    auto end = length;
    while (isspace((unsigned char) str.data()[--end])) {
        if (beg == end) {
            str.erase();
            return;
        }
    }
    ++end;
    if ( beg | (end - length) ) { // if either beg != 0 or end != length
        str.replace(0, length, str, beg, end - beg);
    }
}
*/
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
bool fastq_reader::parse_read(CFastqRead& read) 
//  ----------------------------------------------------------------------------    
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
    auto sequence_size = read.Sequence().size();
    if (sequence_size == 0) 
        throw fastq_error(110, "Read {}: no sequence data", read.Spot());

    if (!m_line_view.empty() && m_line_view[0] == '+') { // quality score defline
        // quality score defline is expected to start with '+'
        // we skip it
        GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
        if (!m_line_view.empty()) {
            do {
                read.AddQualityLine(m_line_view);
                if (m_is_qual_score_numeric && read.Quality().size() >= sequence_size)
                    break;
                GET_LINE(*m_stream, m_line, m_line_view, m_line_number);
                if (m_line_view.empty())
                    break;
                if (m_line_view[0] == '@' && m_defline_parser.Match(m_line_view)) {
                    m_buffered_defline = m_line;
                    break;
                } 
            } while (true);
        }
    }
    read.SetType(m_read_type);
    return true;
}

//  ----------------------------------------------------------------------------    
inline
bool fastq_reader::get_read(CFastqRead& read) 
//  ----------------------------------------------------------------------------    
{
    try {
        if (!parse_read(read))
            return false;
        m_qual_validator(read, m_qual_score_range);
    } catch (fastq_error& e) {
        e.set_file(m_file_name, read.LineNumber());
        throw;
    }
    return true;
}

//  ----------------------------------------------------------------------------    
bool fastq_reader::get_next_spot(string& spot_name, vector<CFastqRead>& reads)
//  ----------------------------------------------------------------------------    
{
    reads.clear();
    if (!m_buffered_spot.empty()) {
        reads.swap(m_buffered_spot);
        spot_name = reads.front().Spot();
        return true;
    }
    CFastqRead read;
    if (m_pending_spot.empty()) {
        if (!get_read(read))
            return false;
        spot_name = read.Spot();
        reads.emplace_back(move(read));
    } else {
        reads.swap(m_pending_spot);
        spot_name = reads.front().Spot();
    }

    while (get_read(read)) {
        if (read.Spot() == spot_name) {
            reads.emplace_back(move(read));
            continue;
        }
        m_pending_spot.emplace_back(move(read));
        break;
    }
    return reads.empty() == false;
}

//  ----------------------------------------------------------------------------    
bool fastq_reader::get_spot(const string& spot_name, vector<CFastqRead>& reads)
//  ----------------------------------------------------------------------------    
{
    
    string spot;
    vector<CFastqRead> next_reads;
    if (!get_next_spot(spot, next_reads)) 
        return false;
    if (spot == spot_name) {
        swap(next_reads, reads);
        return true; 
    }
    if (!m_pending_spot.empty() && m_pending_spot.front().Spot() == spot_name) {
        get_next_spot(spot, reads);
        swap(m_buffered_spot, next_reads);
        return true;
    }
    return false;
}

void fastq_reader::dummy_validator(CFastqRead& read, pair<int, int>& expected_range)
{
    return;
}

//  ----------------------------------------------------------------------------    
void fastq_reader::char_qual_validator(CFastqRead& read, pair<int, int>& expected_range)
//  ----------------------------------------------------------------------------    
{
    string str;
    int qual_size = 0;
    for (auto c : read.mQuality) {
        if (isspace(c)) {
            if (!str.empty()) {
                int score = stoi(str);
                if (!(score >= expected_range.first && score <= expected_range.second)) 
                    throw fastq_error(120, "Read {}: unexpected quality score value '{}'", read.Spot(), score);
                ++qual_size;
                str.clear();
            }
            continue;
        }
        str.append(1, c);
    }
    if (!str.empty()) {
        int score = stoi(str);
        if (!(score >= expected_range.first && score <= expected_range.second)) 
            throw fastq_error(120, "Read {}: unexpected quality score value '{}'", read.Spot(), score);
        ++qual_size;    
    }
    int sz = read.mSequence.size();
    if (qual_size > sz) {
        throw fastq_error(130, "Read {}: quality score length exceeds sequence length", read.Spot());
    }

    while (qual_size < sz) {
        read.mQuality += " ";
        read.mQuality += to_string(expected_range.first + 30);
        ++qual_size;
    }
}


//  ----------------------------------------------------------------------------    
void fastq_reader::num_qual_validator(CFastqRead& read, pair<int, int>& expected_range)
//  ----------------------------------------------------------------------------    
{
    auto qual_size = read.mQuality.size();
    if (qual_size == 0)
        throw fastq_error(111, "Read {}: no quality scores", read.Spot());

    auto sz = read.mSequence.size();
    if (qual_size > sz) 
        throw fastq_error(130, "Read {}: quality score length exceeds sequence length", read.Spot());

    for (auto c : read.mQuality) {
        int score = int(c);
        if (!(score >= expected_range.first && score <= expected_range.second)) 
            throw fastq_error(120, "Read {}: unexpected quality score value '{}'", read.Spot(), score);
    }
    if (qual_size < sz) 
        read.mQuality.append(sz - qual_size,  char(expected_range.first + 30));
}


//  ----------------------------------------------------------------------------    
void fastq_reader::set_qual_validator(bool is_numeric, int min_score, int max_score) 
//  ----------------------------------------------------------------------------    
{
    m_qual_validator = is_numeric ? num_qual_validator : char_qual_validator;
    m_qual_score_range = pair<int, int>(min_score, max_score);
    m_is_qual_score_numeric = is_numeric;
}




BM_DECLARE_TEMP_BLOCK(TB)

//  ----------------------------------------------------------------------------    
static 
void serialize_vec(const string& file_name, str_sv_type& vec)
//  ----------------------------------------------------------------------------    
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


//  ----------------------------------------------------------------------------    
template<typename TWriter>
void fastq_parser<TWriter>::parse() 
//  ----------------------------------------------------------------------------    
{
#ifdef LOCALDEBUG
    size_t spotCount = 0;
    size_t readCount = 0;
    size_t currCount = 0;
    spdlog::stopwatch sw;
    spdlog::info("Parsing from {} files", m_readers.size());
#endif

    int num_readers = m_readers.size();
    vector<CFastqRead> curr_reads;
    auto spot_names_bi = m_spot_names.get_back_inserter();    
    vector<vector<CFastqRead>> spot_reads(num_readers);
    vector<CFastqRead> assembled_spot;

    string spot;
    bool has_spots;
    bool check_readers = false;
    do {
        has_spots = false;
        for (int i = 0; i < num_readers; ++i) {
            if (!m_readers[i].get_next_spot(spot, spot_reads[i])) {
                check_readers = num_readers > 1 && m_allow_early_end == false;
                continue;
            }
    
            has_spots = true;
            for (int j = 0; j < num_readers; ++j) {
                if (j == i) continue;
                m_readers[j].get_spot(spot, spot_reads[j]);
            }

            assembled_spot.clear();
            for (auto& r : spot_reads) {
                if (!r.empty()) {
                    move(r.begin(), r.end(), back_inserter(assembled_spot));
#ifdef LOCALDEBUG
                    readCount += r.size();
                    currCount += r.size();
#endif  
                    r.clear();
                }
            }
            assert(assembled_spot.empty() == false);
            if (!assembled_spot.empty()) {
                m_writer->write_spot(assembled_spot);
                spot_names_bi = assembled_spot.front().Spot();
#ifdef LOCALDEBUG
                ++spotCount;
                if (currCount >= 10e6) {
                    //m_writer->progressMessage(fmt::format("spots: {:L}, reads: {:L}", spotCount, readCount));
                    spdlog::info("spots: {:L}, reads: {:L}", spotCount, readCount);
                    currCount = 0;
                }
#endif  
            }
        }
        if (has_spots == false)
            break;
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

#ifdef LOCALDEBUG
    spdlog::info("spots: {:L}, reads: {:L}", spotCount, readCount);
    spdlog::info("parsing time: {}", sw);
#endif
    spot_names_bi.flush();
}


//  ----------------------------------------------------------------------------    
template<typename TWriter>
void fastq_parser<TWriter>::check_duplicates() 
//  ----------------------------------------------------------------------------    
{
    spdlog::stopwatch sw;
    spdlog::info("spot_name check: {} spots", m_spot_names.size());
    str_sv_type sv;
    sv.remap_from(m_spot_names);
    m_spot_names.swap(sv);
    spdlog::info("remapping done...");

    if (!m_spot_file.empty()) 
        serialize_vec(m_spot_file, m_spot_names);

    spot_name_check name_check(m_spot_names.size());

    bm::bvector<> bv_collisions;
    bm::sparse_vector_scanner<str_sv_type> str_scan;

    str_sv_type::size_type pos = 0;
    size_t idx_pos = 0, index = 0, hits = 0, sz = 0, last_hits = 0;
    static const char empty_str[] = "";
    const char* value;
    auto it = m_spot_names.begin();

    while (it.valid()) {
        value = it.value();
        sz = strlen(value);
        if (name_check.seen_before(value, sz)) {
            ++hits;
            idx_pos = it.pos();
            if (!m_spot_file.empty())
                bv_collisions.set(idx_pos);
            m_spot_names.set(idx_pos, empty_str);
            if (str_scan.find_eq_str(m_spot_names, value, pos))
                throw fastq_error(170, "[line:{}] Collation check. Duplicate spot '{}' at line {}", pos * 4 + 1, value, (idx_pos * 4) + 1);
            it = m_spot_names.get_const_iterator(idx_pos + 1);
        } else {
            it.advance();
        }
        if (++index % 100000000 == 0) {
            spdlog::info("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
            last_hits = hits;
        }
    }    

    spdlog::info("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{:L}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());

    if (hits > 0 && !m_spot_file.empty()) {
        string collisions_file = m_spot_file + ".dups";
        bm::SaveBVector(collisions_file.c_str(), bv_collisions);
    }
    spdlog::info("spot_name check time:{}, num_hits: {:L}", sw, hits);
}

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
            if (score < -5 || score > 126)
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
//  ----------------------------------------------------------------------------    
{
    const auto& quality = read.Quality();
    if (!params.intialized) {
        params.space_delimited = std::any_of(quality.begin(), quality.end(), [](const char c) { return isspace(c);});
        params.intialized = true;
    }

    if (params.space_delimited) {
        string str;
        for (auto c : quality) {
            if (isspace(c)) {
                if (!str.empty()) {
                    if (!params.set_score(stoi(str)))
                        throw fastq_error(140, "Read {}: quality score contains unexpected character '{}'", read.Spot(), stoi(str));
                    str.clear();
                }
                continue;
            }
            str.append(1, c);
        }
        if (!str.empty()) 
            if (!params.set_score(stoi(str))) 
                throw fastq_error(140, "Read {}: quality score contains unexpected character '{}'", read.Spot(), stoi(str));
    } else {
        for (auto c : quality) {
            if (!params.set_score(int(c)))
                throw fastq_error(140, "Read {}: quality score contains unexpected character '{}'", read.Spot(), int(c));
        }
    }
}


//  ----------------------------------------------------------------------------    
template<typename TWriter>
void fastq_parser<TWriter>::setup_readers(const vector<string>& files, const vector<char>& read_types, uint8_t platform, int num_reads_to_check) 
//  ----------------------------------------------------------------------------    
{
    assert(!files.empty());
    assert(read_types.empty() || read_types.size() == files.size());
    m_readers.clear();
    // Run first num_reads_to_check to check for consistency 
    // and setup read_types and platform if needed
    CFastqRead read;
    map<string, qual_score_params> reader_qual_params;
    for (const auto& fn  : files) {
        fastq_reader reader(fn, s_OpenStream(fn));
        try {
            if (!reader.parse_read(read))
                throw fastq_error(50 , "File '{}' has no reads", fn);
        } catch (fastq_error& e) {
            e.set_file(fn, read.LineNumber());
            throw;
        }

        qual_score_params& params  = reader_qual_params[fn];
        s_check_qual_score(read, params);

        platform = reader.platform();

        //if (platform == 0)
        //    platform = reader.platform();
        //else if (reader.platform() != platform)    
        //    throw fastq_error(60, "Platform detected from defline '{}' does not match paltform passed as parameter '{}'", reader.platform(), platform);

        for (int i = 0; i < num_reads_to_check; ++i) {
            try {
                if (!reader.parse_read(read))
                    break;
            } catch (fastq_error& e) {
                e.set_file(fn, read.LineNumber());
                throw;
            }
            s_check_qual_score(read, params);
            if (platform != reader.platform()) 
                throw fastq_error(70, "Input files have deflines from different platforms");
        }

        //RANGES = {
        //'Sanger': (33, 73),
        //'Illumina-1.8': (33, 74),
        //'Solexa': (59, 104),
        //'Illumina-1.3': (64, 104),
        //'Illumina-1.5': (66, 105)
        if (params.min_score < 0) {
            params.min_score = -5;
            params.max_score = 40;
        } else if (params.min_score >= 33 && params.max_score <= 74) {
            params.min_score = 33;
            params.max_score = 74;
        //} else if (params.min_score >= 59 && params.min_score <= 104) {
        //    params.min_score = 59;
        //    params.max_score = 104;
        } else if (params.min_score >= 64 && params.min_score <= 105) {
            params.min_score = 64;
            params.max_score = 105;
        }
    }
    size_t idx = 0;
    for (const auto& fn  : files) {
        m_readers.emplace_back(fn, s_OpenStream(fn), (idx < read_types.size()) ? read_types[idx] : 'A');
        const qual_score_params& params = reader_qual_params[fn];
        auto& reader = m_readers.back();
        reader.set_qual_validator(params.space_delimited == false, params.min_score, params.max_score);
        ++idx;
    }
    m_writer->set_platform(platform);
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
//  ----------------------------------------------------------------------------    
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
    for (const auto& fn : files)
        readers.emplace_back(fn, s_OpenStream(fn));
    vector<uint8_t> placed(files.size(), 0);

    for (size_t i = 0; i < files.size(); ++i) {
        if (placed[i]) continue;
        placed[i] = 1;
        string spot;
        if (!readers[i].get_next_spot(spot, reads))
            throw fastq_error(50 , "File '{}' has no reads", readers[i].file_name());
        vector<string> batch{readers[i].file_name()};
        for (size_t j = 0; j < files.size(); ++j) {
            if (placed[j]) continue;
            if (readers[j].get_spot(spot, reads)) {
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
void check_hash_file(const string& hash_file) 
//  ----------------------------------------------------------------------------    
{
    str_sv_type vec;
    {
        vector<char> buffer;
        bm::sparse_vector_deserializer<str_sv_type> deserializer;
        bm::sparse_vector_serial_layout<str_sv_type> lay;
        bm::read_dump_file(hash_file, buffer);        
        deserializer.deserialize(vec, (const unsigned char*)&buffer[0]);
    }


    spdlog::info("Checking reads {:L} on {} threads", vec.size(), std::thread::hardware_concurrency());
    {
        str_sv_type::statistics st;
        vec.calc_stat(&st);
        spdlog::info("st.memory_used {}", st.memory_used);
    }
    spdlog::stopwatch sw;

    bm::sparse_vector_scanner<str_sv_type> str_scan;
    str_sv_type::size_type pos = 0;
    spot_name_check name_check(vec.size());

    //bm::bvector<> bv_collisions;

    size_t idx_pos = 0, index = 0, hits = 0, sz = 0, last_hits = 0;
    static const char empty_str[] = "";
    const char* value;
    bool hit;
    auto it = vec.begin();

    while (it.valid()) {
        value = it.value();
        sz = strlen(value);
        {
            bm::chrono_taker tt1("check hits", 1, &timing_map);
            hit = name_check.seen_before(value, sz);
        }
        if (hit) {
            ++hits;
            idx_pos = it.pos();
            //bv_collisions.set(idx_pos);
            vec.set(idx_pos, empty_str);
            {
                bm::chrono_taker tt1("scan", 1, &timing_map);
                if (str_scan.find_eq_str(vec, value, pos))
                    throw fastq_error(170, "Duplicate spot '{}'", value);
            }
            it = vec.get_const_iterator(idx_pos + 1);
        } else {
            it.advance();
        }
        if (++index % 100000000 == 0) {
            spdlog::info("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
            last_hits = hits;
            bm::chrono_taker::print_duration_map(timing_map, bm::chrono_taker::ct_all);
            timing_map.clear();
        }
    }
    {
        spdlog::info("index:{:L}, elapsed:{:.2f}, hits:{}, total_hits:{}, memory_used:{:L}", index, sw, hits - last_hits, hits, name_check.memory_used());
        bm::chrono_taker::print_duration_map(timing_map, bm::chrono_taker::ct_all);
    }

    //string collisions_file = hash_file + ".dups";
    //bm::SaveBVector(collisions_file.c_str(), bv_collisions);
}


#endif

