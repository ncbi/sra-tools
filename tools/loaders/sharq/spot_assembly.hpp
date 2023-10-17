#ifndef __SPOT_ASSEMBLY_HPP__
#define __SPOT_ASSEMBLY_HPP__

/**
 * @file spot_assembly.hpp
 * 
 */
#include "data_frame.hpp"
#include "fastq_read.hpp"
#include <string>
#include <map>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <spdlog/fmt/fmt.h>
#include <bm/bm64.h>
#include <bm/bmsparsevec_compr.h>
#include <bm/bmconst.h>
#include "taskflow/taskflow.hpp"
#include "taskflow/algorithm/sort.hpp"
#include "taskflow/algorithm/for_each.hpp"


using namespace std;

static constexpr int MAX_ROW_TO_CLEAR = 5000000;
static constexpr int MAX_ROWS_TO_OPTIMIZE = 10000000; 

static_assert(bm::id_max == bm::id_max48, "BitMagic should be compiled in 64-bit mode");

typedef bm::bvector<> bvector_type;
typedef bm::sparse_vector<unsigned, bvector_type> svector_u32;
typedef bm::sparse_vector<uint8_t, bvector_type> svector_u8;
typedef signed short qual_type_t;
typedef bm::sparse_vector<qual_type_t , bvector_type> svector_int;
typedef bm::rsc_sparse_vector<qual_type_t , svector_int> rsc_svector_int;
typedef bm::str_sparse_vector<char, bvector_type, 32> str_sv_type;


BM_DECLARE_TEMP_BLOCK(TB1) // BitMagic Temporary block

inline unsigned DNA2int(char DNA_bp)
{
    switch (DNA_bp)
    {
    case 'A':
        return 0; // 000
    case 'T':
        return 1; // 001
    case 'G':
        return 2; // 010
    case 'C':
        return 3; // 011
    case 'N':
        return 4; // 100
    default:
        assert(0);
        throw runtime_error(fmt::format("Invalid DNA base: {}", DNA_bp));
    }
}

inline char Int2DNA(uint8_t code)
{
    switch (code)
    {
    case 0:
        return 'A'; // 000
    case 1:
        return 'T'; // 001
    case 2:
        return 'G'; // 010
    case 3:
        return 'C'; // 011
    case 4:
        return 'N'; // 100
    default:
        assert(0);
        throw runtime_error(fmt::format("Invalid DNA code: {}", code));
    }
}

class metadata_t : public CDataFrame
{
public:

    enum {
        e_ReadNumId,     // str        
        e_SpotGroupId,   // str        
        e_SuffixId,      // str
        e_ChannelId,     // str
        e_NanoporeReadNoId, // str
        e_ReadFilterId,  // bit 
        e_SeqOffsetId,   // uint64 
        e_QualOffsetId,  // uint64
        e_ReaderId,      // uint16
    };

    metadata_t() 
    {
        static initializer_list<EDF_ColumnType> cols = {
            eDF_String,  // e_ReadNumId
            eDF_String,  // e_SpotGroupId
            eDF_String,  // e_SuffixId
            eDF_String,  // e_ChannelId
            eDF_String,  // e_NanoporeReadNoId
            eDF_Bit,     // e_ReadFilterId
            eDF_Uint64,  // e_SeqOffsetId -- offset in the sequence data (compound unit64_t - 16 bit len + 48 bit offset)
            eDF_Uint64,  // e_QualOffsetId -- offset in the quality data (compound unit64_t - 16 bit len + 48 bit offset)
            eDF_Uint16,  // e_ReaderId -- reader (input file) id
        };
        CreateColumns(cols);
    }
};


// temproary buffers for multi-threaded processing
static thread_local vector<svector_u32::value_type> tmp_buffer;
static thread_local string tmp_str;
static thread_local vector<uint8_t> tmp_qual_scores;
static thread_local vector<svector_int::value_type> tmp_qual_buffer;
static thread_local fastq_read tmp_read;

// --------------------------------------------------------------------------
// spot_assembly_t - class to assemble reads into spots

struct spot_assembly_t {
    spot_assembly_t() {};
    ~spot_assembly_t() {}

    // initializes data for num_rows 
    void init(size_t num_rows);

    // assign spot_ids for collected read_names
    // after this function, m_read_index will have spot_id assigned for consecutive reads from the input
    template<typename T>
    void assign_spot_id(str_sv_type& read_names, vector<T>& read_index);

    template<typename T>
    void x_assign_spot_id(tf::Executor& executor, str_sv_type& read_names, vector<T>& sort_index, vector<T>& read_index);

    // optimize/compress cold storage data
    void optimize();

    // returns true if spot is last in the file
    bool is_last_spot(size_t row_id);

    // saves read to hot or cold storage
    template<typename ScoreValidator, bool is_nanopore = false>
    void save_read(size_t row_id, fastq_read& read);
    // multi-threaded version of save_read
    template<typename ScoreValidator, bool is_nanopore>
    void save_read_mt(size_t row_id, fastq_read& read);

    // retrieves all reads for the spot from hot or cold storage
    template<typename ScoreValidator, bool is_nanopore = false>
    void get_spot(size_t row_id, vector<fastq_read>& reads);
    // multi-threaded version of get_spot
    template<typename ScoreValidator, bool is_nanopore>
    void get_spot_mt(const string& spot_name, size_t row_id, vector<fastq_read>& reads);

    // removes spot from hot or cold storage
    template<bool is_nanopore = false>
    void clear_spot(size_t row_id);
    // multi-threaded version of clear_spot
    template<bool is_nanopore>
    void clear_spot_mt(size_t row_id);
    bvector_type m_last_index;     ///< bitvector of num_reads size, 1 if read is last in the spot

    bvector_type m_rows_to_clear;  ///< bitvector, 1 if row should be cleared
    uint32_t m_num_rows_to_clear = 0; ///< counter, number of rows to clear

    atomic<int> m_num_rows_to_optimize = 0; ///< number of rows to optimize, once it reaches MAX_ROWS_TO_OPTIMIZE, optimization is performed

    size_t m_total_spots = 0; ///< total number of spots
    map<uint32_t, size_t> m_reads_counts; ///< number of reads in each spot

    mutex m_mutex; ///< lock for access hot and cold storage 

    // classes for hot storage
    bvector_type m_hot_spot_ids; ///< bitvector, 1 if spot is hot (i.e. saved in m_hot_spots)
    typedef size_t spot_id_t;
    unordered_map<spot_id_t, vector<fastq_read>> m_hot_spots; ///< hot spots storage


    // classes for cold storage 
    svector_u8 m_spot_index;             ///< spot index - num reads per spot 
    vector<metadata_t> m_reads_metadata; ///< reads metadata from defline - readNum,  spotGroup, sequence and quality offset 
    vector<svector_u32> m_sequences;     ///< sequence data
    vector<size_t> m_seq_offset;         ///< last sequence offset per read

    vector<svector_int> m_qualities;     ///< quality data
    vector<size_t> m_qual_offset;        ///< last quality offset per read

    size_t m_hot_reads_threshold = 10000000;  ///< threshold for hot reads, if read is far from the last read in the spot, it is saved in cold storage

};

void spot_assembly_t::init(size_t num_rows) 
{

    m_last_index = bvector_type();
    m_last_index.init();

    m_rows_to_clear = bvector_type();
    m_rows_to_clear.init();

    m_hot_spot_ids = bvector_type();
    m_hot_spot_ids.init();
    m_hot_spots.clear();

    m_reads_metadata.clear();
    m_reads_counts.clear();

    m_spot_index.clear();
    m_sequences.clear();
    m_seq_offset.clear();

    m_qualities.clear();
    m_qual_offset.clear();

    m_total_spots = 0;
} 

static bool file_exists(const string& name) 
{
    struct stat stat_buffer;   
    return stat(name.c_str(), &stat_buffer) == 0; 
}

struct assign_spot_page_t 
{
    size_t page_start;
    bvector_type last_index;
    bvector_type hot_spot_index;
    map<uint32_t, size_t> reads_counts;
    size_t total_spots = 0;
};


template<typename T>
void spot_assembly_t::x_assign_spot_id(tf::Executor& executor, str_sv_type& read_names, vector<T>& sort_index, vector<T>& read_index)
{

    size_t num_rows = read_names.size();
    size_t num_workers = executor.num_workers();
    if (num_workers == 0) num_workers = 1;
    size_t page_size = num_rows/num_workers + 1;
    if (page_size < 1000) {
        page_size = num_rows;
        num_workers = 1;
    }
    vector<assign_spot_page_t> pages;
    pages.resize(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        pages[i].page_start = i * page_size;
        pages[i].last_index.init();
        pages[i].hot_spot_index.init();
    }   
    assert(pages.back().page_start + page_size >= num_rows);
    atomic<size_t> spot_id{1};
    tf::Taskflow taskflow;
    taskflow.for_each(pages.begin(), pages.end(), [&](assign_spot_page_t& page) { 
        if (page.page_start >= num_rows)
            return;
        //moodycamel::ProducerToken token(*save_spot_queue->queue);
        size_t end = min(page.page_start + page_size, num_rows);
        size_t curr_spot_id = 0;
        string spot_name;
        vector<uint64_t> sort_vector;
        size_t start = page.page_start;
        size_t rows_skipped = 0, rows_borrowed = 0;

        if (start == 0) {
            curr_spot_id = 1;
            size_t row = sort_index[0];
            read_index[row] = curr_spot_id;
            read_names.get(row, spot_name);
            sort_vector.push_back(row);
        } else {
            size_t row = sort_index[start];
            read_names.get(row, spot_name);
            size_t prev_row = sort_index[start - 1];
            // skip rows that belong to the last spot of the prevoius page
            while (read_names.compare_remap(prev_row, spot_name.c_str()) == 0)  {
                ++start;
                if (start == num_rows)
                    return;
                ++rows_skipped;
                row = sort_index[start];
                read_names.get(row, spot_name);
            }
            curr_spot_id = ++spot_id;
            read_index[row] = curr_spot_id;
            sort_vector.push_back(row);
        }
        ++start;
        for (; start < end; ++start) {
            size_t row = sort_index[start];
            if (read_names.compare_remap(row, spot_name.c_str()) == 0)  {
                read_index[row] = curr_spot_id;
                sort_vector.push_back(row);
            } else {
                sort(sort_vector.begin(), sort_vector.end()); 
                page.last_index.set_bit_no_check(sort_vector.back());
                if (sort_vector.back() - sort_vector.front() < m_hot_reads_threshold)
                    page.hot_spot_index.set_bit_no_check(curr_spot_id);
                ++page.total_spots;    
                ++page.reads_counts[sort_vector.size()];
                curr_spot_id = ++spot_id;
                read_index[row] = curr_spot_id;
                sort_vector.clear();
                sort_vector.push_back(row);
                read_names.get(row, spot_name);
            }
        }
        if (!sort_vector.empty()) {
            assert(spot_name.empty() == false);
            // pick up all the rows that belong to the last spot
            while (start < num_rows) {
                size_t row = sort_index[start];
                if (read_names.compare_remap(row, spot_name.c_str()) != 0) 
                    break; 
                read_index[row] = curr_spot_id;
                sort_vector.push_back(row);
                ++start;
                ++rows_borrowed;
            }
            sort(sort_vector.begin(), sort_vector.end()); 
            page.last_index.set_bit_no_check(sort_vector.back());
            if (sort_vector.back() - sort_vector.front() < m_hot_reads_threshold)
                page.hot_spot_index.set_bit_no_check(curr_spot_id);
            ++page.total_spots;    
            ++page.reads_counts[sort_vector.size()];
        }
        if (page.total_spots > 0) {
            bvector_type::statistics st1, st2;
            BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block        
            page.last_index.optimize(TB, bm::bvector<>::opt_compress, &st1);
            page.hot_spot_index.optimize(TB, bm::bvector<>::opt_compress, &st2);
            spdlog::stopwatch sw;
            {
                lock_guard<mutex> lock(m_mutex);
                m_last_index.merge(page.last_index);
                m_hot_spot_ids.merge(page.hot_spot_index);
                m_total_spots += page.total_spots;
                for (auto& p : page.reads_counts) {
                    m_reads_counts[p.first] += p.second;
                }   
            }
            page.last_index.clear(true);
            page.hot_spot_index.clear(true);
            page.reads_counts.clear();
            //spdlog::info("page {:L} done, last_index mem {:L}, hot_index mem {:L}, mutex time {}, skipped {}, borrowed {}", page.page_start, st1.memory_used, st2.memory_used, sw, rows_skipped, rows_borrowed);
        }

        
    });
    executor.run(taskflow).wait();
}

template<typename T>
void assign_spot_id3(str_sv_type& read_names, vector<T>& sort_index, vector<T>& read_index, size_t threshold, bvector_type& last_index, bvector_type& hot_spot_ids)
{
    last_index.init();
    hot_spot_ids.init();
    size_t num_rows= read_names.size();
    size_t spot_id = 1;
    size_t prev_row = sort_index[0];
    read_index[prev_row] = spot_id;

    vector<uint64_t> sort_vector;
    sort_vector.push_back(prev_row);
    string spot_name;
    read_names.get(prev_row, spot_name);

    for (size_t i = 1; i < num_rows; ++i) {
        size_t row = sort_index[i];
        assert(row < num_rows);
        if (read_names.compare_remap(row, spot_name.c_str()) == 0)  {
            read_index[row] = spot_id;
            sort_vector.push_back(row);
        } else {
            sort(sort_vector.begin(), sort_vector.end()); 
            last_index.set_bit_no_check(sort_vector.back());

            // decide if spot is hot or cold
            if (sort_vector.back() - sort_vector.front() < threshold)
                hot_spot_ids.set_bit_no_check(spot_id);

            read_index[row] = ++spot_id;
            sort_vector.clear();
            sort_vector.push_back(row);
            read_names.get(row, spot_name);
        }
    }
    if (!sort_vector.empty()) {
        sort(sort_vector.begin(), sort_vector.end()); 
        last_index.set_bit_no_check(sort_vector.back());

        // decide if spot is hot or cold
        if (sort_vector.back() - sort_vector.front() < threshold)
            hot_spot_ids.set_bit_no_check(spot_id);
    }
}


template<typename T>
void spot_assembly_t::assign_spot_id(str_sv_type& read_names, vector<T>& read_index) 
{
    size_t num_rows = read_names.size();
    read_index.resize(num_rows);
    spdlog::stopwatch sw;
    // build and sort the index by read names
    vector<T> sort_index(num_rows);

    tf::Executor executor(min<int>(24, std::thread::hardware_concurrency())); // TODO set max number of threads baased on the number of rows
#if defined(__SAVE_FIRST_PASS__)
    if (file_exists("sort_index")) {
        ifstream is("sort_index");
        is.read((char*)&sort_index[0], num_rows * sizeof(T));
        is.close();
    } else {
#endif        
        iota(sort_index.begin(), sort_index.end(), T(0));

        tf::Taskflow taskflow;
        taskflow.sort(sort_index.begin(), sort_index.end(), [&](T  l, T  r) {
            static thread_local string last_right_str;
            static thread_local T last_right = -1;

            if (last_right != r) {
                last_right = r; 
                read_names.get(last_right, last_right_str);
            }
            return read_names.compare_remap(l, last_right_str.c_str()) < 0;
        });
        executor.run(taskflow).wait();
        spdlog::info("Sorting took {:.3}", sw);       
#if defined(__SAVE_FIRST_PASS__)        
        ofstream os("sort_index");
        os.write((char*)&sort_index[0], num_rows * sizeof(T));
    }
#endif    
    init(num_rows);
    sw.reset();
    x_assign_spot_id(executor, read_names, sort_index, read_index);
    /*
    spdlog::info("Assigning spot ids new took {:.3}", sw);
    sw.reset();

    vector<T> read_index2(num_rows);
    bvector_type last_index;
    bvector_type hot_spot_ids; 
    assign_spot_id3(read_names, sort_index, read_index2, m_hot_reads_threshold, last_index, hot_spot_ids);
    spdlog::info("Assigning spot ids old took {:.3}", sw);
    assert(last_index.count() == m_last_index.count());
    assert(hot_spot_ids.count() == m_hot_spot_ids.count());
    vector<uint64_t> rows1, rows2;
    bvector_type spot_tested1, spot_tested2;
    spot_tested1.init();
    spot_tested2.init();
    size_t spot_id_cnt1 = 0, spot_id_cnt2 = 0;
    for (size_t row = 0; row < num_rows; ++row) {
        size_t spot_id = read_index[row];
        if (spot_tested1.set_bit_conditional(spot_id, true, false)) {
            ++spot_id_cnt1;
            for (size_t i = row; i < num_rows; ++i) {
                if (read_index[i] == spot_id) {
                    rows1.push_back(i);
                    if (m_last_index.test(i)) 
                        break;                      
                }
            }
        }
        spot_id = read_index2[row];
        if (spot_tested2.set_bit_conditional(spot_id, true, false)) {
            ++spot_id_cnt2;
            for (size_t i = row; i < num_rows; ++i) {
                if (read_index2[i] == spot_id) {
                    rows2.push_back(i);
                    if (last_index.test(i)) 
                        break;                      
                }
            }
        }
        if (rows1 != rows2) {
            spdlog::error("row {} rows1 {} rows2 {}", row, rows1.size(), rows2.size());
            exit(1);
        }
        rows1.clear();
        rows2.clear();
    }
    assert(spot_tested1.count() == spot_tested2.count());
    assert(spot_id_cnt1 == spot_id_cnt2);
    assert(spot_id_cnt1 == spot_tested2.count());
    */
    m_last_index.optimize(TB1);
    m_last_index.freeze();

    m_hot_spot_ids.optimize(TB1);
    m_hot_spot_ids.freeze();

    int max_reads = 0;
    for (auto& it : m_reads_counts) 
        max_reads = max(max_reads, (int)it.first);

    m_reads_metadata.resize(max_reads);

    m_sequences.resize(max_reads);
    m_seq_offset.resize(max_reads);
    fill(m_seq_offset.begin(), m_seq_offset.end(), 0);

    m_qualities.resize(max_reads);
    m_qual_offset.resize(max_reads);
    fill(m_qual_offset.begin(), m_qual_offset.end(), 0);
}

// saves read to hot or cold storage
/*
template<typename ScoreValidator, bool is_nanopore = false>
void spot_assembly_t::save_read(size_t row_id, fastq_read& read) {

    if (m_hot_spot_ids.test(row_id)) {
        m_hot_spots[row_id].push_back(move(read));
        return;
    }
    uint8_t read_idx = m_spot_index.get_no_check(row_id);
    auto& metadata = m_reads_metadata.emplace_back();

    metadata.get<u16_t>(metadata_t::e_ReaderId).set(row_id, read.m_ReaderIdx);

    if (!read.ReadNum().empty())
        metadata.get<str_t>(metadata_t::e_ReadNumId).set(row_id, read.ReadNum().c_str());
    if (!read.SpotGroup().empty())
        metadata.get<str_t>(metadata_t::e_SpotGroupId).set(row_id, read.SpotGroup().c_str());
    if (!read.Suffix().empty())        
        metadata.get<str_t>(metadata_t::e_SuffixId).set(row_id, read.Suffix().c_str());
    if constexpr(is_nanopore) {           
        if (!read.Channel().empty())    
            metadata.get<str_t>(metadata_t::e_ChannelId).set(row_id, read.Channel().c_str());   
        if (!read.NanoporeReadNo().empty())   
            metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).set(row_id, read.NanoporeReadNo().c_str());
    }
    if (read.ReadFilter())
        metadata.get<bit_t>(metadata_t::e_ReadFilterId).set(row_id);

    const auto& seq = read.Sequence();
    size_t sz = seq.size();
    assert(sz > 0);
    tmp_buffer.resize(sz);
    for (size_t i = 0; i < sz; ++i) 
        tmp_buffer[i] = DNA2int(seq[i]);
    size_t offset = m_seq_offset[read_idx];
    m_sequences.emplace_back().import(&tmp_buffer[0], sz, offset);
    m_seq_offset.emplace_back() = offset + sz;
    offset |= sz << 48;
    metadata.get<u64_t>(metadata_t::e_SeqOffsetId).set(row_id, offset);
    tmp_qual_scores.clear();
    read.GetQualScores(tmp_qual_scores);
    sz = tmp_qual_scores.size();
    assert(sz == seq.size());
    tmp_qual_buffer.resize(sz);
    int mid_score = ScoreValidator::min_score() + 30;
    tmp_qual_buffer[0] = tmp_qual_scores[0] - mid_score;
    for (size_t i = 1; i < sz; ++i) {
        tmp_qual_buffer[i] = tmp_qual_scores[i] - tmp_qual_scores[i - 1];
    }
    size_t qual_offset = m_qual_offset[read_idx];
    m_qualities.emplace_back().import(&tmp_qual_buffer[0], sz, qual_offset);
    m_qual_offset.emplace_back() = qual_offset + sz;
    qual_offset |= sz << 48;
    metadata.get<u64_t>(metadata_t::e_QualOffsetId).set(row_id, qual_offset);
    m_spot_index.inc(row_id);
    ++m_num_rows_to_optimize;
}
*/

// retrieves all reads for the spot from hot or cold storage
template<typename ScoreValidator, bool is_nanopore>
void spot_assembly_t::get_spot(size_t row_id, vector<fastq_read>& reads) 
{

    if (m_hot_spot_ids.test(row_id)) {
        auto it = m_hot_spots.find(row_id);
        if (it != m_hot_spots.end()) {
            reads = move(it->second);
        } else {
            reads.clear();
        }
        return;
    } 

    auto num_reads = m_spot_index.get_no_check(row_id);
    reads.resize(num_reads);
    for (int read_idx = 0; read_idx < num_reads; ++read_idx) {
        tmp_read.Reset();
        auto& metadata = m_reads_metadata[read_idx];
        tmp_read.m_ReaderIdx = metadata.get<u16_t>(metadata_t::e_ReaderId).get(row_id);

        tmp_str.clear();
        metadata.get<str_t>(metadata_t::e_ReadNumId).get(row_id, tmp_str);
        if (!tmp_str.empty()) {
            tmp_read.SetReadNum(tmp_str);
            tmp_str.clear();
        }
        metadata.get<str_t>(metadata_t::e_SpotGroupId).get(row_id, tmp_str);
        if (!tmp_str.empty()) {
            tmp_read.SetSpotGroup(tmp_str);
            tmp_str.clear();
        }
        metadata.get<str_t>(metadata_t::e_SuffixId).get(row_id, tmp_str);
        if (!tmp_str.empty()) {
            tmp_read.SetSuffix(tmp_str);
            tmp_str.clear();
        }
        if constexpr(is_nanopore) {           

            metadata.get<str_t>(metadata_t::e_ChannelId).get(row_id, tmp_str);   
            if (!tmp_str.empty()) {
                tmp_read.SetChannel(tmp_str);
                tmp_str.clear();
            }
            metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).get(row_id, tmp_str);
            if (!tmp_str.empty()) {
                tmp_read.SetNanoporeReadNo(tmp_str);
                tmp_str.clear();
            }
        }    
        if (metadata.get<bit_t>(metadata_t::e_ReadFilterId).test(row_id))        
            tmp_read.SetReadFilter(1);
        size_t offset = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).get(row_id);
        size_t len = offset >> 48;
        offset &= 0x0000FFFFFFFFFFFF;
        tmp_buffer.resize(len);
        m_sequences[read_idx].decode(&tmp_buffer[0], offset, len);
        tmp_str.resize(len);
        for (size_t j = 0; j < len; ++j)
            tmp_str[j] = Int2DNA(tmp_buffer[j]);
        tmp_read.SetSequence(tmp_str);

        size_t qual_offset = metadata.get<u64_t>(metadata_t::e_QualOffsetId).get(row_id);
        len = qual_offset >> 48;
        qual_offset &= 0x0000FFFFFFFFFFFF;
        tmp_qual_buffer.resize(len);
        m_qualities[read_idx].decode(&tmp_qual_buffer[0], qual_offset, len);
        int mid_score = ScoreValidator::min_score() + 30;
        tmp_qual_scores.resize(len);
        tmp_qual_scores[0] = tmp_qual_buffer[0] + mid_score;
        for (size_t i = 1; i < len; ++i) {
            tmp_qual_scores[i] = tmp_qual_buffer[i] + tmp_qual_scores[i - 1];
        }
        tmp_read.SetQualScores(tmp_qual_scores);
        reads[read_idx] = move(tmp_read);
    }
}



// removes spot from hot or cold storage
/*
template<bool is_nanopore = false>
void spot_assembly_t::clear_spot(size_t row_id) 
{
    if (m_hot_spot_ids.test(row_id)) {
        m_hot_spots.erase(row_id);
        return;
    }
    m_rows_to_clear.set_bit_no_check(row_id);
    ++m_num_rows_to_clear;

    if (m_num_rows_to_clear >= MAX_ROW_TO_CLEAR) {

        spdlog::stopwatch sw;
        vector<size_t> row_ids(MAX_ROW_TO_CLEAR);
        size_t c = 0;
        for (auto en = m_rows_to_clear.first(); en.valid(); ++en) {
            row_ids[c] = *en;
            ++c;
        }                

        auto num_reads = m_spot_index.get_no_check(row_id);
        for (int read_idx = 0; read_idx < num_reads; ++read_idx) {
            auto& metadata = m_reads_metadata[read_idx];
            metadata.get<u16_t>(metadata_t::e_ReaderId).clear(m_rows_to_clear);

            metadata.get<str_t>(metadata_t::e_ReadNumId).clear(m_rows_to_clear);
            metadata.get<str_t>(metadata_t::e_SpotGroupId).clear(m_rows_to_clear);
            metadata.get<str_t>(metadata_t::e_SuffixId).clear(m_rows_to_clear);
            if constexpr(is_nanopore) {
                metadata.get<str_t>(metadata_t::e_ChannelId).clear(m_rows_to_clear);   
                metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).clear(m_rows_to_clear);
            }
            metadata.get<bit_t>(metadata_t::e_ReadFilterId).bit_sub(m_rows_to_clear);

            for (size_t row : row_ids) {
                size_t offset = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).get(row);
                size_t len = offset >> 48;
                offset &= 0x0000FFFFFFFFFFFF;
                m_sequences[read_idx].clear_range(offset, offset+(len - 1));

                size_t qual_offset = metadata.get<u64_t>(metadata_t::e_QualOffsetId).get(row);
                len = qual_offset >> 48;
                qual_offset &= 0x0000FFFFFFFFFFFF;
                m_qualities[read_idx].clear_range(qual_offset, qual_offset + (len - 1));
                
            }
            metadata.get<u64_t>(metadata_t::e_SeqOffsetId).clear(m_rows_to_clear);
            metadata.get<u64_t>(metadata_t::e_QualOffsetId).clear(m_rows_to_clear);
        }
        m_rows_to_clear.clear();
        m_num_rows_to_optimize += m_num_rows_to_clear;
        m_num_rows_to_clear = 0;
        auto logger = spdlog::get("parser_logger"); // send log to stderr        
        if (logger) logger->info("cleanup took: {}", sw);
    }
}
*/
// optimize/compress cold storage data
void spot_assembly_t::optimize() 
{
    if (m_num_rows_to_optimize < MAX_ROWS_TO_OPTIMIZE)
        return;
    m_num_rows_to_optimize = 0;
    spdlog::stopwatch sw;
    size_t md_mem = 0;
    {
        lock_guard<mutex> lock(m_mutex);
        for (auto& metadata : m_reads_metadata) {
            md_mem += metadata.Optimize();
        }
    }
    svector_u32::statistics st1;
    size_t seq_mem = 0;
    {
        lock_guard<mutex> lock(m_mutex);
        for (auto& seq : m_sequences) {
            seq.optimize(TB1, bm::bvector<>::opt_compress, &st1);
            seq_mem += st1.memory_used;
        }
    }
    svector_int::statistics st2;
    size_t qual_mem = 0;
    lock_guard<mutex> lock(m_mutex);
    {
        for (auto& qual : m_qualities) {
            qual.optimize(TB1, bm::bvector<>::opt_compress, &st2);
            qual_mem += st2.memory_used;
        }
    }
    auto logger = spdlog::get("parser_logger"); // send log to stderr        
    if (logger) logger->info("optimize took: {}, seq_mem: {:L}, qual_mem: {:L}, md_mem: {:L}", sw, seq_mem, qual_mem, md_mem);
}

// returns true if spot is last in the file
bool spot_assembly_t::is_last_spot(size_t row_id) {
    return m_last_index.test(row_id);
}

template<typename ScoreValidator, bool is_nanopore>
void spot_assembly_t::save_read_mt(size_t row_id, fastq_read& read) {

    if (m_hot_spot_ids.test(row_id)) {
        lock_guard<mutex> lock(m_mutex);
        m_hot_spots[row_id].push_back(move(read));
        return;
    }

    const auto& seq = read.Sequence();
    size_t seq_sz = seq.size();
    assert(seq_sz > 0);
    tmp_buffer.resize(seq_sz);  
    //std::srand(std::time(0));  // use current time as seed for random generator
    //char nucleotides[] = {'A', 'C', 'G', 'T'};
    for (size_t i = 0; i < seq_sz; ++i) 
        tmp_buffer[i] = DNA2int(seq[i]);

    //tmp_buffer[0] = DNA2int(nucleotides[std::rand() % 4]);  // get a random index between 0 and 3    


    tmp_qual_scores.clear();
    read.GetQualScores(tmp_qual_scores);
    size_t qual_sz = tmp_qual_scores.size();
    assert(qual_sz == seq_sz);
    tmp_qual_buffer.resize(qual_sz);
    int mid_score = ScoreValidator::min_score() + 30;
    tmp_qual_buffer[0] = tmp_qual_scores[0] - mid_score;
    for (size_t i = 1; i < qual_sz; ++i) {
        tmp_qual_buffer[i] = tmp_qual_scores[i] - tmp_qual_scores[i - 1];
    }

    {
        lock_guard<mutex> lock(m_mutex);
        uint8_t read_idx = m_spot_index.get_no_check(row_id);
        auto& metadata = m_reads_metadata[read_idx];

        metadata.get<u16_t>(metadata_t::e_ReaderId).set(row_id, read.m_ReaderIdx);

        if (!read.ReadNum().empty())
            metadata.get<str_t>(metadata_t::e_ReadNumId).set(row_id, read.ReadNum().c_str());
        if (!read.SpotGroup().empty())
            metadata.get<str_t>(metadata_t::e_SpotGroupId).set(row_id, read.SpotGroup().c_str());
        if (!read.Suffix().empty())        
            metadata.get<str_t>(metadata_t::e_SuffixId).set(row_id, read.Suffix().c_str());
        if constexpr(is_nanopore) {            
            if (!read.Channel().empty())    
                metadata.get<str_t>(metadata_t::e_ChannelId).set(row_id, read.Channel().c_str());   
            if (!read.NanoporeReadNo().empty())   
                metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).set(row_id, read.NanoporeReadNo().c_str());
        }
        if (read.ReadFilter())
            metadata.get<bit_t>(metadata_t::e_ReadFilterId).set(row_id);

        size_t offset = m_seq_offset[read_idx];
        if (offset + seq_sz >= bm::id_max) 
            throw runtime_error("This FASTQ cannot be processed due to far read buffer overflow");
        
        m_sequences[read_idx].import(&tmp_buffer[0], seq_sz, offset);
        m_seq_offset[read_idx] = offset + seq_sz;
        offset |= seq_sz << 48;
        metadata.get<u64_t>(metadata_t::e_SeqOffsetId).set(row_id, offset);

        size_t qual_offset = m_qual_offset[read_idx];
        m_qualities[read_idx].import(&tmp_qual_buffer[0], qual_sz, qual_offset);
        m_qual_offset[read_idx] = qual_offset + qual_sz;
        qual_offset |= qual_sz << 48;
        metadata.get<u64_t>(metadata_t::e_QualOffsetId).set(row_id, qual_offset);
        m_spot_index.inc(row_id);
        ++m_num_rows_to_optimize;
    }

}

template<typename ScoreValidator, bool is_nanopore>
void spot_assembly_t::get_spot_mt(const string& spot_name, size_t row_id, vector<fastq_read>& reads) 
{

    reads.clear();
    if (m_hot_spot_ids.test(row_id)) {
        lock_guard<mutex> lock(m_mutex);
        auto it = m_hot_spots.find(row_id);
        if (it != m_hot_spots.end()) {
            reads = move(it->second);
        }
        return;
    } 

    lock_guard<mutex> lock(m_mutex);
    auto num_reads = m_spot_index.get_no_check(row_id);
    reads.resize(num_reads);
    for (int read_idx = 0; read_idx < num_reads; ++read_idx) {
        tmp_read.Reset();
        auto& metadata = m_reads_metadata[read_idx];
        tmp_read.m_ReaderIdx = metadata.get<u16_t>(metadata_t::e_ReaderId).get(row_id);

        tmp_str.clear();
        metadata.get<str_t>(metadata_t::e_ReadNumId).get(row_id, tmp_str);
        if (!tmp_str.empty()) {
            tmp_read.SetReadNum(tmp_str);
            tmp_str.clear();
        }
        metadata.get<str_t>(metadata_t::e_SpotGroupId).get(row_id, tmp_str);
        if (!tmp_str.empty()) {
            tmp_read.SetSpotGroup(tmp_str);
            tmp_str.clear();
        }
        metadata.get<str_t>(metadata_t::e_SuffixId).get(row_id, tmp_str);
        if (!tmp_str.empty()) {
            tmp_read.SetSuffix(tmp_str);
            tmp_str.clear();
        }
        if constexpr(is_nanopore) {
            metadata.get<str_t>(metadata_t::e_ChannelId).get(row_id, tmp_str);   
            if (!tmp_str.empty()) {
                tmp_read.SetChannel(tmp_str);
                tmp_str.clear();
            }
            metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).get(row_id, tmp_str);
            if (!tmp_str.empty()) {
                tmp_read.SetNanoporeReadNo(tmp_str);
                tmp_str.clear();
            }
        }
        if (metadata.get<bit_t>(metadata_t::e_ReadFilterId).test(row_id))        
            tmp_read.SetReadFilter(1);
        size_t offset = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).get(row_id);
        size_t len = offset >> 48;
        offset &= 0x0000FFFFFFFFFFFF;
        tmp_buffer.resize(len);
        m_sequences[read_idx].decode(&tmp_buffer[0], offset, len);
        tmp_str.resize(len);
        for (size_t j = 0; j < len; ++j)
            tmp_str[j] = Int2DNA(tmp_buffer[j]);
        tmp_read.SetSequence(tmp_str);

        size_t qual_offset = metadata.get<u64_t>(metadata_t::e_QualOffsetId).get(row_id);
        len = qual_offset >> 48;
        qual_offset &= 0x0000FFFFFFFFFFFF;
        tmp_qual_buffer.resize(len);

        m_qualities[read_idx].decode(&tmp_qual_buffer[0], qual_offset, len);
        int mid_score = ScoreValidator::min_score() + 30;
        tmp_qual_scores.resize(len);
        tmp_qual_scores[0] = tmp_qual_buffer[0] + mid_score;
        for (size_t i = 1; i < len; ++i) {
            tmp_qual_scores[i] = tmp_qual_buffer[i] + tmp_qual_scores[i - 1];
        }
        tmp_read.SetQualScores(tmp_qual_scores);

        reads[read_idx] = move(tmp_read);
    }
}

template<bool is_nanopore>
void spot_assembly_t::clear_spot_mt(size_t row_id) 
{
    if (m_hot_spot_ids.test(row_id)) {
        lock_guard<mutex> lock(m_mutex);
        m_hot_spots.erase(row_id);
        return;
    }

    m_rows_to_clear.set_bit_no_check(row_id);
    ++m_num_rows_to_clear;
    if (m_num_rows_to_clear >= MAX_ROW_TO_CLEAR) {

        spdlog::stopwatch sw;
        vector<svector_u64::size_type> row_ids(m_num_rows_to_clear);
        size_t c = 0;
        for (auto en = m_rows_to_clear.first(); en.valid(); ++en) {
            row_ids[c] = *en;
            ++c;
        }                
        vector<svector_u64::value_type> offsets(row_ids.size());
        bvector_type clear_bv;

        assert(c == m_num_rows_to_clear);
        {
            lock_guard<mutex> lock(m_mutex);
            uint8_t num_reads = m_spot_index.get_no_check(row_id);
            for (int read_idx = 0; read_idx < num_reads; ++read_idx) {
                auto& metadata = m_reads_metadata[read_idx];
    
                metadata.get<u16_t>(metadata_t::e_ReaderId).clear(m_rows_to_clear);

                {
                    auto& v = metadata.get<str_t>(metadata_t::e_ReadNumId);
                    if (!v.empty()) v.clear(m_rows_to_clear);
                }
                {
                    auto & v = metadata.get<str_t>(metadata_t::e_SpotGroupId);
                    if (!v.empty()) v.clear(m_rows_to_clear);
                }
                {
                    auto & v = metadata.get<str_t>(metadata_t::e_SuffixId);
                    if (!v.empty()) v.clear(m_rows_to_clear);
                }
                if constexpr(is_nanopore) {
                    metadata.get<str_t>(metadata_t::e_ChannelId).clear(m_rows_to_clear);   
                    metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).clear(m_rows_to_clear);
                }
                metadata.get<bit_t>(metadata_t::e_ReadFilterId).bit_sub(m_rows_to_clear);

                clear_bv.clear();
                auto sz = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).gather(offsets.data(), row_ids.data(), offsets.size(), bm::BM_UNSORTED);
                for (size_t i = 0; i < sz; ++i) {
                    auto offset = offsets[i];
                    size_t len = offset >> 48;
                    if (len > 0) {
                        offset &= 0x0000FFFFFFFFFFFF;
                        clear_bv.set_range(offset, offset + (len - 1));
                    }
                }
                m_sequences[read_idx].clear(clear_bv);
                metadata.get<u64_t>(metadata_t::e_SeqOffsetId).clear(m_rows_to_clear);

                clear_bv.clear();
                sz = metadata.get<u64_t>(metadata_t::e_QualOffsetId).gather(offsets.data(), row_ids.data(), offsets.size(), bm::BM_UNSORTED);
                for (size_t i = 0; i < sz; ++i) {
                    auto offset = offsets[i];
                    size_t len = offset >> 48;
                    if (len > 0) {
                        offset &= 0x0000FFFFFFFFFFFF;
                        clear_bv.set_range(offset, offset + (len - 1));
                    }
                }
                m_qualities[read_idx].clear(clear_bv);
                metadata.get<u64_t>(metadata_t::e_QualOffsetId).clear(m_rows_to_clear);


            }
            m_rows_to_clear.clear();
            m_num_rows_to_optimize += m_num_rows_to_clear;
            m_num_rows_to_clear = 0;
        } 
        auto logger = spdlog::get("parser_logger"); // send log to stderr        
        if (logger) logger->info("cleanup took: {}", sw);
    }
}


#endif

