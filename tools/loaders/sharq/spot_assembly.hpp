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


using namespace std;

typedef bm::bvector<> bvector_type;
typedef bm::sparse_vector<unsigned, bvector_type> svector_u32;
typedef bm::sparse_vector<uint8_t, bvector_type> svector_u8;
typedef signed short qual_type_t;
typedef bm::sparse_vector<qual_type_t , bvector_type> svector_int;
typedef bm::rsc_sparse_vector<qual_type_t , svector_int> rsc_svector_int;

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
        return 0;
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
        return 0;
    }
}

//#define _STR_SPOT

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
//            eDF_Uint32,  // e_SeqLenId
            eDF_Uint64,  // e_SeqOffsetId
            eDF_Uint64,  // e_QualOffsetId
            eDF_Uint16,  // e_ReaderId
        };
        CreateColumns(cols);
    }
};

#define _SPOT_ASSEMBLY_MT
#if defined(_SPOT_ASSEMBLY_MT)
    static thread_local vector<uint32_t> buffer;
    static thread_local string m_tmp_str;
    static thread_local vector<uint8_t> qual_scores;
    static thread_local vector<qual_type_t> qual_buffer;
    static thread_local vector<string> str_buffer;
    static thread_local fastq_read m_tmp_read;
#endif


static constexpr int MAX_READS = 4;
// spot_assembly_t struct with constructor 
struct spot_assembly_t {
    spot_assembly_t() {
        m_reads_metadata.resize(MAX_READS);
        m_sequences.resize(MAX_READS);
        m_qualities.resize(MAX_READS);
        m_seq_offset.resize(MAX_READS);
        m_qual_offset.resize(MAX_READS);
    };
    void init(size_t num_rows) {

        spot_index.resize(num_rows);

        last_index = bvector_type();
        last_index.init();

        rows_to_clear = bvector_type();
        rows_to_clear.init();

        m_hot_spot_ids = bvector_type();
        m_hot_spot_ids.init();
        m_hot_spots.clear();

        m_reads_metadata.clear();
        m_reads_metadata.resize(MAX_READS);
        m_reads_counts.fill(0);
    } 
    ~spot_assembly_t() {}

    vector<uint32_t> spot_index;
    bvector_type last_index;

    bvector_type rows_to_clear;
    uint32_t num_rows_to_clear = 0;

    bvector_type m_hot_spot_ids;
    map<size_t, vector<fastq_read>> m_hot_spots;

    array<size_t, MAX_READS> m_reads_counts;

#if not defined(_SPOT_ASSEMBLY_MT)
    vector<uint32_t> buffer;
    string m_tmp_str;
    vector<uint8_t> qual_scores;
    vector<qual_type_t> qual_buffer;
    vector<string> str_buffer;
    fastq_read m_tmp_read;
#endif

    atomic<int> m_optimize_count = 0;

    svector_u8 m_spot_index;
    //bm::sparse_vector<uint8_t, bm::bvector<>> m_spot_index;
    vector<metadata_t> m_reads_metadata;
    mutex m_mutex;

    vector<svector_u32> m_sequences;
    vector<svector_int> m_qualities;
    vector<size_t> m_seq_offset;
    vector<size_t> m_qual_offset;

    //std::shared_ptr<spdlog::logger> stderr_logger;

    template<typename ScoreValidator, bool is_nanopore = false>
    void save_read(size_t row_id, fastq_read& read) {

        if (m_hot_spot_ids.test(row_id)) {
            m_hot_spots[row_id].push_back(move(read));
            return;
        }
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
    
        const auto& seq = read.Sequence();
        size_t sz = seq.size();
        assert(sz > 0);
        buffer.resize(sz);
        for (size_t i = 0; i < sz; ++i) 
            buffer[i] = DNA2int(seq[i]);
        size_t offset = m_seq_offset[read_idx];
        m_sequences[read_idx].import(&buffer[0], sz, offset);
        m_seq_offset[read_idx] = offset + sz;
        offset |= sz << 48;
        metadata.get<u64_t>(metadata_t::e_SeqOffsetId).set(row_id, offset);
        //metadata.get<u32_t>(metadata_t::e_SeqLenId).set(row_id, sz);
        qual_scores.clear();
        read.GetQualScores(qual_scores);
        sz = qual_scores.size();
        assert(sz == seq.size());
        qual_buffer.resize(sz);
        int mid_score = ScoreValidator::min_score() + 30;
        qual_buffer[0] = qual_scores[0] - mid_score;
        for (size_t i = 1; i < sz; ++i) {
            qual_buffer[i] = qual_scores[i] - qual_scores[i - 1];
        }
        size_t qual_offset = m_qual_offset[read_idx];
        m_qualities[read_idx].import(&qual_buffer[0], sz, qual_offset);
        m_qual_offset[read_idx] = qual_offset + sz;
        qual_offset |= sz << 48;
        metadata.get<u64_t>(metadata_t::e_QualOffsetId).set(row_id, qual_offset);
        m_spot_index.inc(row_id);
        ++m_optimize_count;
    }

    template<typename ScoreValidator, bool is_nanopore = false>
    void get_spot(size_t row_id, vector<fastq_read>& reads) 
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
            m_tmp_read.Reset();
            auto& metadata = m_reads_metadata[read_idx];
            m_tmp_read.m_ReaderIdx = metadata.get<u16_t>(metadata_t::e_ReaderId).get(row_id);

            m_tmp_str.clear();
            metadata.get<str_t>(metadata_t::e_ReadNumId).get(row_id, m_tmp_str);
            if (!m_tmp_str.empty()) {
                m_tmp_read.SetReadNum(m_tmp_str);
                m_tmp_str.clear();
            }
            metadata.get<str_t>(metadata_t::e_SpotGroupId).get(row_id, m_tmp_str);
            if (!m_tmp_str.empty()) {
                m_tmp_read.SetSpotGroup(m_tmp_str);
                m_tmp_str.clear();
            }
            metadata.get<str_t>(metadata_t::e_SuffixId).get(row_id, m_tmp_str);
            if (!m_tmp_str.empty()) {
                m_tmp_read.SetSuffix(m_tmp_str);
                m_tmp_str.clear();
            }
            if constexpr(is_nanopore) {           

                metadata.get<str_t>(metadata_t::e_ChannelId).get(row_id, m_tmp_str);   
                if (!m_tmp_str.empty()) {
                    m_tmp_read.SetChannel(m_tmp_str);
                    m_tmp_str.clear();
                }
                metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).get(row_id, m_tmp_str);
                if (!m_tmp_str.empty()) {
                    m_tmp_read.SetNanoporeReadNo(m_tmp_str);
                    m_tmp_str.clear();
                }
            }    
            if (metadata.get<bit_t>(metadata_t::e_ReadFilterId).test(row_id))        
                m_tmp_read.SetReadFilter(1);
            size_t offset = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).get(row_id);
            size_t len = offset >> 48;
            offset &= 0x0000FFFFFFFFFFFF;
            buffer.resize(len);
            m_sequences[read_idx].decode(&buffer[0], offset, len);
            m_tmp_str.resize(len);
            for (size_t j = 0; j < len; ++j)
                m_tmp_str[j] = Int2DNA(buffer[j]);
            m_tmp_read.SetSequence(m_tmp_str);

            size_t qual_offset = metadata.get<u64_t>(metadata_t::e_QualOffsetId).get(row_id);
            len = qual_offset >> 48;
            qual_offset &= 0x0000FFFFFFFFFFFF;
            qual_buffer.resize(len);
            m_qualities[read_idx].decode(&qual_buffer[0], qual_offset, len);
            int mid_score = ScoreValidator::min_score() + 30;
            qual_scores.resize(len);
            qual_scores[0] = qual_buffer[0] + mid_score;
            for (size_t i = 1; i < len; ++i) {
                qual_scores[i] = qual_buffer[i] + qual_scores[i - 1];
            }
            m_tmp_read.SetQualScores(qual_scores);
            reads[read_idx] = move(m_tmp_read);
        }
    }

    void add_spot_bits(size_t spot_id, array<uint32_t, MAX_READS>& sort_vector, size_t sz) {
        sort(sort_vector.begin(), sort_vector.begin() + sz); 
        last_index.set_bit_no_check(sort_vector[sz - 1]);
        if (sort_vector[sz - 1] - sort_vector[0] < 10000000)
            m_hot_spot_ids.set_bit_no_check(spot_id);
        ++m_reads_counts[sz - 1];
    }

    template<bool is_nanopore = false>
    void clear_spot(size_t row_id) 
    {
        if (m_hot_spot_ids.test(row_id)) {
            m_hot_spots.erase(row_id);
            m_hot_spot_ids.clear_bit_no_check(row_id);
            return;
        }
        rows_to_clear.set_bit_no_check(row_id);
        ++num_rows_to_clear;
        static const int max_rows_to_clear = 1000000;

        if (num_rows_to_clear >= max_rows_to_clear) {

            spdlog::stopwatch sw;
            vector<size_t> row_ids(max_rows_to_clear);
            size_t c = 0;
            for (auto en = rows_to_clear.first(); en.valid(); ++en) {
                row_ids[c] = *en;
                ++c;
            }                

            auto num_reads = m_spot_index.get_no_check(row_id);
            for (int read_idx = 0; read_idx < num_reads; ++read_idx) {
                auto& metadata = m_reads_metadata[read_idx];
                metadata.get<u16_t>(metadata_t::e_ReaderId).clear(rows_to_clear);

                metadata.get<str_t>(metadata_t::e_ReadNumId).clear(rows_to_clear);
                metadata.get<str_t>(metadata_t::e_SpotGroupId).clear(rows_to_clear);
                metadata.get<str_t>(metadata_t::e_SuffixId).clear(rows_to_clear);
                if constexpr(is_nanopore) {
                    metadata.get<str_t>(metadata_t::e_ChannelId).clear(rows_to_clear);   
                    metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).clear(rows_to_clear);
                }
                metadata.get<bit_t>(metadata_t::e_ReadFilterId).bit_sub(rows_to_clear);

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
                metadata.get<u64_t>(metadata_t::e_SeqOffsetId).clear(rows_to_clear);
                metadata.get<u64_t>(metadata_t::e_QualOffsetId).clear(rows_to_clear);
            }
            rows_to_clear.clear();
            m_optimize_count += num_rows_to_clear;
            num_rows_to_clear = 0;
            auto logger = spdlog::get("parser_logger"); // send log to stderr        
            if (logger) logger->info("cleanup took: {}", sw);
        }
    }

    void optimize() {
        if (m_optimize_count < 10000000)
            return;
        m_optimize_count = 0;
        spdlog::stopwatch sw;
        size_t md_mem = 0;
        for (auto& metadata : m_reads_metadata) {
            lock_guard<mutex> lock(m_mutex);
            md_mem += metadata.Optimize();
        }
        svector_u32::statistics st1;
        size_t seq_mem = 0;
        for (auto& seq : m_sequences) {
            lock_guard<mutex> lock(m_mutex);
            seq.optimize(TB1, bm::bvector<>::opt_compress, &st1);
            seq_mem += st1.memory_used;
        }
        svector_int::statistics st2;
        size_t qual_mem = 0;
        for (auto& qual : m_qualities) {
            lock_guard<mutex> lock(m_mutex);
            qual.optimize(TB1, bm::bvector<>::opt_compress, &st2);
            qual_mem += st2.memory_used;
        }
        auto logger = spdlog::get("parser_logger"); // send log to stderr        
        if (logger) logger->info("optimize took: {}, seq_mem: {:L}, qual_mem: {:L}, md_mem: {:L}", sw, seq_mem, qual_mem, md_mem);

    }
    bool is_last_spot(size_t row_id) {
        return last_index.test(row_id);
    }
    void print(ostream& out) {
        auto num_rows = spot_index.size();
        for (size_t i = 1; i < num_rows; ++i) {
            out << spot_index[i] << '\t';
            if (m_hot_spot_ids[i] != 0) {
                out << "hot";
            }
            out << '\t';
            if (last_index[i] != 0) {
                out << "last";
            }        
            out << '\t';
            out << endl;        
        }

    }

    template<typename ScoreValidator, bool is_nanopore>
    void save_read_mt(size_t row_id, fastq_read& read) {

        if (m_hot_spot_ids.test(row_id)) {
            lock_guard<mutex> lock(m_mutex);
            m_hot_spots[row_id].push_back(move(read));
            return;
        }
    
        const auto& seq = read.Sequence();
        size_t seq_sz = seq.size();
        assert(seq_sz > 0);
        buffer.resize(seq_sz);
        for (size_t i = 0; i < seq_sz; ++i) 
            buffer[i] = DNA2int(seq[i]);

        qual_scores.clear();
        read.GetQualScores(qual_scores);
        size_t qual_sz = qual_scores.size();
        assert(qual_sz == seq_sz);
        qual_buffer.resize(qual_sz);
        int mid_score = ScoreValidator::min_score() + 30;
        qual_buffer[0] = qual_scores[0] - mid_score;
        for (size_t i = 1; i < qual_sz; ++i) {
            qual_buffer[i] = qual_scores[i] - qual_scores[i - 1];
        }

        {
            lock_guard<mutex> lock(m_mutex);
            uint8_t read_idx = m_spot_index.get_no_check(row_id);
            auto& metadata = m_reads_metadata[read_idx];

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
            m_sequences[read_idx].import(&buffer[0], seq_sz, offset);
            m_seq_offset[read_idx] = offset + seq_sz;
            offset |= seq_sz << 48;
            metadata.get<u64_t>(metadata_t::e_SeqOffsetId).set(row_id, offset);

            size_t qual_offset = m_qual_offset[read_idx];
            m_qualities[read_idx].import(&qual_buffer[0], qual_sz, qual_offset);
            m_qual_offset[read_idx] = qual_offset + qual_sz;
            qual_offset |= qual_sz << 48;
            metadata.get<u64_t>(metadata_t::e_QualOffsetId).set(row_id, qual_offset);
            m_spot_index.inc(row_id);
        }

    }

    template<typename ScoreValidator, bool is_nanopore>
    void get_spot_mt(size_t row_id, vector<fastq_read>& reads) 
    {

        if (m_hot_spot_ids.test(row_id)) {
            lock_guard<mutex> lock(m_mutex);
            auto it = m_hot_spots.find(row_id);
            if (it != m_hot_spots.end()) {
                reads = move(it->second);
            } else {
                reads.clear();
            }
            return;
        } 

        lock_guard<mutex> lock(m_mutex);
        auto num_reads = m_spot_index.get_no_check(row_id);
        reads.resize(num_reads);
        for (int read_idx = 0; read_idx < num_reads; ++read_idx) {
            m_tmp_read.Reset();
            auto& metadata = m_reads_metadata[read_idx];
            m_tmp_str.clear();
            metadata.get<str_t>(metadata_t::e_ReadNumId).get(row_id, m_tmp_str);
            if (!m_tmp_str.empty()) {
                m_tmp_read.SetReadNum(m_tmp_str);
                m_tmp_str.clear();
            }
            metadata.get<str_t>(metadata_t::e_SpotGroupId).get(row_id, m_tmp_str);
            if (!m_tmp_str.empty()) {
                m_tmp_read.SetSpotGroup(m_tmp_str);
                m_tmp_str.clear();
            }
            metadata.get<str_t>(metadata_t::e_SuffixId).get(row_id, m_tmp_str);
            if (!m_tmp_str.empty()) {
                m_tmp_read.SetSuffix(m_tmp_str);
                m_tmp_str.clear();
            }
            if constexpr(is_nanopore) {
                metadata.get<str_t>(metadata_t::e_ChannelId).get(row_id, m_tmp_str);   
                if (!m_tmp_str.empty()) {
                    m_tmp_read.SetChannel(m_tmp_str);
                    m_tmp_str.clear();
                }
                metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).get(row_id, m_tmp_str);
                if (!m_tmp_str.empty()) {
                    m_tmp_read.SetNanoporeReadNo(m_tmp_str);
                    m_tmp_str.clear();
                }
            }
            if (metadata.get<bit_t>(metadata_t::e_ReadFilterId).test(row_id))        
                m_tmp_read.SetReadFilter(1);
            size_t offset = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).get(row_id);
            //size_t len = metadata.get<u32_t>(metadata_t::e_SeqLenId).get(row_id);
            size_t len = offset >> 48;
            offset &= 0x0000FFFFFFFFFFFF;
            buffer.resize(len);
            m_sequences[read_idx].decode(&buffer[0], offset, len);
            m_tmp_str.resize(len);
            for (size_t j = 0; j < len; ++j)
                m_tmp_str[j] = Int2DNA(buffer[j]);
            m_tmp_read.SetSequence(m_tmp_str);

            size_t qual_offset = metadata.get<u64_t>(metadata_t::e_QualOffsetId).get(row_id);
            len = qual_offset >> 48;
            qual_offset &= 0x0000FFFFFFFFFFFF;
            qual_buffer.resize(len);

            m_qualities[read_idx].decode(&qual_buffer[0], qual_offset, len);
            int mid_score = ScoreValidator::min_score() + 30;
            qual_scores.resize(len);
            qual_scores[0] = qual_buffer[0] + mid_score;
            for (size_t i = 1; i < len; ++i) {
                qual_scores[i] = qual_buffer[i] + qual_scores[i - 1];
            }
            m_tmp_read.SetQualScores(qual_scores);

            reads[read_idx] = move(m_tmp_read);
        }
    }

    template<bool is_nanopore>
    void clear_spot_mt(size_t row_id) 
    {
        if (m_hot_spot_ids.test(row_id)) {
            lock_guard<mutex> lock(m_mutex);
            m_hot_spots.erase(row_id);
            //m_hot_spot_ids.clear_bit_no_check(row_id);
            return;
        }

        rows_to_clear.set_bit_no_check(row_id);
        ++num_rows_to_clear;
        static const int max_rows_to_clear = 5000000;

        if (num_rows_to_clear >= max_rows_to_clear) {

            spdlog::stopwatch sw;
            vector<svector_u64::size_type> row_ids(num_rows_to_clear);
            size_t c = 0;
            for (auto en = rows_to_clear.first(); en.valid(); ++en) {
                row_ids[c] = *en;
                ++c;
            }                
            vector<svector_u64::value_type> offsets(row_ids.size());
            bvector_type clear_bv;

            assert(c == num_rows_to_clear);
            {
                lock_guard<mutex> lock(m_mutex);
                uint8_t num_reads = m_spot_index.get_no_check(row_id);
                for (int read_idx = 0; read_idx < num_reads; ++read_idx) {
                    auto& metadata = m_reads_metadata[read_idx];
                    {
                        //metadata.get<str_t>(metadata_t::e_ReadNumId).clear(rows_to_clear);
                        auto& v = metadata.get<str_t>(metadata_t::e_ReadNumId);
                        if (!v.empty()) v.clear(rows_to_clear);
                    }
                    {
                        //metadata.get<str_t>(metadata_t::e_SpotGroupId).clear(rows_to_clear);
                        auto & v = metadata.get<str_t>(metadata_t::e_SpotGroupId);
                        if (!v.empty()) v.clear(rows_to_clear);
                    }
                    {
                        //metadata.get<str_t>(metadata_t::e_SuffixId).clear(rows_to_clear);
                        auto & v = metadata.get<str_t>(metadata_t::e_SuffixId);
                        if (!v.empty()) v.clear(rows_to_clear);
                    }
                    if constexpr(is_nanopore) {
                        metadata.get<str_t>(metadata_t::e_ChannelId).clear(rows_to_clear);   
                        metadata.get<str_t>(metadata_t::e_NanoporeReadNoId).clear(rows_to_clear);
                    }
                    metadata.get<bit_t>(metadata_t::e_ReadFilterId).bit_sub(rows_to_clear);

                    clear_bv.clear();
                    auto sz = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).gather(offsets.data(), row_ids.data(), offsets.size(), bm::BM_UNSORTED);
                    for (size_t i = 0; i < sz; ++i) {
                        auto offset = offsets[i];
                        size_t len = offset >> 48;
                        if (len > 0) {
                            offset &= 0x0000FFFFFFFFFFFF;
                            clear_bv.set_range(offset, offset+(len - 1));
                        }
                    }
                    m_sequences[read_idx].clear(clear_bv);
                    metadata.get<u64_t>(metadata_t::e_SeqOffsetId).clear(rows_to_clear);

                    clear_bv.clear();
                    sz = metadata.get<u64_t>(metadata_t::e_QualOffsetId).gather(offsets.data(), row_ids.data(), offsets.size(), bm::BM_UNSORTED);
                    for (size_t i = 0; i < sz; ++i) {
                        auto offset = offsets[i];
                        size_t len = offset >> 48;
                        if (len > 0) {
                            offset &= 0x0000FFFFFFFFFFFF;
                            clear_bv.set_range(offset, offset+(len - 1));
                        }
                    }
                    m_qualities[read_idx].clear(clear_bv);
                    metadata.get<u64_t>(metadata_t::e_QualOffsetId).clear(rows_to_clear);

/*

                    for (size_t row : row_ids) {
                        size_t offset = metadata.get<u64_t>(metadata_t::e_SeqOffsetId).get(row);
                        size_t len = offset >> 48;
                        if (len > 0) {
                            offset &= 0x0000FFFFFFFFFFFF;
                            m_sequences[read_idx].clear_range(offset, offset+(len - 1));
                        }

                        size_t qual_offset = metadata.get<u64_t>(metadata_t::e_QualOffsetId).get(row);
                        len = qual_offset >> 48;
                        if (len > 0) {
                            qual_offset &= 0x0000FFFFFFFFFFFF;
                            m_qualities[read_idx].clear_range(qual_offset, qual_offset + (len - 1));
                        }
                    }
                    metadata.get<u64_t>(metadata_t::e_SeqOffsetId).clear(rows_to_clear);
                    metadata.get<u64_t>(metadata_t::e_QualOffsetId).clear(rows_to_clear);
*/
                }
                rows_to_clear.clear();
                m_optimize_count += num_rows_to_clear;
                num_rows_to_clear = 0;
            } 
            auto logger = spdlog::get("parser_logger"); // send log to stderr        
            if (logger) logger->info("cleanup took: {}", sw);
        }
    }

};

#endif

