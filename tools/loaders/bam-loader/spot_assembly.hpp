#ifndef __SPOT_ASSEMBLY_HPP_
#define __SPOT_ASSEMBLY_HPP_

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
 * Authors:  Andrei Shkeda
 *
 * File Description:
 *
 */
#include "hashing.hpp"
#include "data_frame.hpp"
#include <tsl/array_map.h>
#include <bm/bm.h>
#include <bm/bmsparsevec.h>
#include <bm/bmstrsparsevec.h>
#include <bm/bmsparsevec_algo.h>
#include <bm/bmsparsevec_serial.h>
#include <bm/bmsparsevec_compr.h>
#include <bm/bmintervals.h>
#include <bm/bmdbg.h>


#include <vector>
#include <ostream>
#include <memory>
#include <algorithm>
#include <variant>

using namespace std;



typedef tsl::array_map<char, uint32_t, 
hashing::fnv_1a_hash, 
tsl::ah::str_equal<char>,
true,
std::uint16_t,
std::uint32_t,
tsl::ah::prime_growth_policy> array_map_t;

typedef bm::bvector<> bvector_type;
typedef bm::str_sparse_vector<char, bvector_type, 32> str_sv_type;
typedef bm::sparse_vector<uint32_t, bm::bvector<> > svector_u32;



/**
 * @brief BAM Alignment succinct metadata
 * 
 * 
 */
class metadata_t : public CDataFrame
{
public:    
    enum {
        e_primaryId1, //uint64
        e_primaryId2, //uint64
        e_spotId,     //uint64
        e_fragmentId, //uint32 
        e_fragment_len1, //uint32
        e_fragment_len2, //uint32
        e_alignmentCount1, //uint32
        e_alignmentCount2, //uint32
        e_platform,  //uint16
        e_unmated, //bit
        e_pcr_dup, //bit
        e_unaligned_1, //bit
        e_unaligned_2, //bit
        e_hardclipped, //bit
        e_primary_is_set //bit
    };

    metadata_t() {
        static initializer_list<EDF_ColumnType> cols = {
            eDF_Uint64,  // e_primaryId1
            eDF_Uint64,  // e_primaryId2
            eDF_Uint64,  // e_spotId
            eDF_Uint32,  // e_fragmentId
            eDF_Uint16,  // e_fragment_len1
            eDF_Uint16,  // e_fragment_len2
            eDF_Uint16,  // e_alignmentCount1
            eDF_Uint16,  // e_alignmentCount2
            eDF_Uint16,  // e_platform
            eDF_Bit,     // e_unmated
            eDF_Bit,     // e_pcr_dup
            eDF_Bit,     // e_unaligned_1
            eDF_Bit,     // e_unaligned_2
            eDF_Bit,     // e_hardclipped
            eDF_Bit     // e_primary_is_set
        };
        CreateColumns(cols);
    }
    bool need_optimize{false};  ///< Flag to indicate if metadata need to be optimized (compressed)
    size_t memory_used{0};      ///< Used memory in bytes

    static constexpr std::array<int, 2> E_PRIM_ID = {metadata_t::e_primaryId1, metadata_t::e_primaryId2};
    static constexpr std::array<int, 2> E_FRAG_LEN = {metadata_t::e_fragment_len1, metadata_t::e_fragment_len2};
    static constexpr std::array<int, 2> E_ALN_COUNT = {metadata_t::e_alignmentCount1, metadata_t::e_alignmentCount2};

};


/**
 * @brief Volume of compressed spot names and corresponding metadata
 * 
 * Volume has two states: hot and cold. While it's hot (m_use_scanner == false), 
 * m_pack_job is running and the spot search is using m_spot_map.
 * Once m_pack_job is finished (m_data_ready == true), 
 * the search is switched (m_use_scanner == false) to use svector scanner (m_scanner)
 * and m_spot_map memory is released
 * The switch is done by spot_assembly instance which handles the list of spot_batches and their states
 * 
 */
struct spot_batch 
{
    typedef bm::sparse_vector_scanner<str_sv_type, 64> scanner_t;
    unique_ptr<array_map_t> m_spot_map;  ///< 'hot' spot name map while pack_job is running
    size_t m_offset = 0;                 ///< Metadata global offset
    size_t m_batch_size = 0;             ///< Number of spots in the batch 
    unique_ptr<str_sv_type> m_data;      ///< sorted succinct spot names 
    unique_ptr<svector_u32> m_index;     ///< succinct spot name index
    unique_ptr<scanner_t> m_scanner;     ///< scanner implements binary search in m_data
    tf::Future<void> m_pack_job;         ///< Pack job future
    atomic<bool> m_data_ready{false};    ///< Flag to indicate the end of the pack job
    bool m_use_scanner{false};           ///< Flag to indicate which search to use (scanner or spot_map)
    size_t m_memory_used = 0;            ///< Memory used by metadata (for diagnostics) 
    unique_ptr<metadata_t> m_metadata;   ///< Pointer to metadata
    bool m_data_saved{false};

    /**
     * @brief Construct a new spot batch object
     * 
     * @param offset 
     * @param batch_size 
     */
    spot_batch(size_t offset, size_t batch_size) 
        : m_offset(offset)
        , m_batch_size(batch_size)
    {

    }

    /**
     * @brief Release memory related to spot name searches, keep metadata
     * 
     */
    void release_search_memory() 
    {
        if (m_pack_job.valid())
            m_pack_job.get();
        m_spot_map.reset(nullptr);
        m_data.reset(nullptr);
        m_index.reset(nullptr);
        m_scanner.reset(nullptr);
        m_memory_used = 0;
    }

};

/**
 * @brief Implements spot assembly
 * 
 * find() is the main method that finds or else adds a new spot and returns its metadata
 * 
 * @tparam metadata_t 
 */

struct spot_assembly 
{
    tf::Executor& m_executor;           ///< Taskflow executor (initialized in constructor)
    tf::Taskflow m_taskflow;            ///< taskflow used for searches
    shared_ptr<spot_name_filter> m_key_filter; ///< Spot name bloom filter
    const unsigned m_group_id;           ///< unique spot assembly (or reporting)
    unique_ptr<array_map_t> m_spot_map;  ///< Current search map
    unique_ptr<metadata_t> m_metadata;   ///< Current metdata

    vector<unique_ptr<spot_batch>> m_batches; ///< List of search batches
    size_t m_offset = 0;                 ///< Current data offset  
    size_t m_curr_row = 0;               ///< Current row 
    size_t m_total_spots = 0;            ///< Total number of spots 
    unsigned m_platform = 0;             ///< Assembly platform 
    atomic<bool> m_stop_packing{false};  ///< Flag to interrupt bach packing jobs
    atomic<bool> m_search_done;          ///< Flag to interrupt the current search 
    
    size_t m_key_filter_total = 0;
    size_t m_key_filter_miss = 0;

    /**
     * @brief Construct a new spot assembly object
     * 
     * @param executor -- TaskFlow executor
     * @param key_filter -- Bloom filter
     * @param group_id -- numeric cgroup_id
     * @param batch_size -- default batch size
     */
    spot_assembly(tf::Executor& executor, shared_ptr<spot_name_filter> key_filter, unsigned group_id, size_t batch_size); 

    /**
     * @brief Destroy the spot assembly object
     * 
     */
    ~spot_assembly(); 
    

    /**
     * @brief Release memory related to spot name searches, keep metadata
    */
    void release_search_memory();

    /**
     * @brief Add batch and run background packing job
     * 
     */
    void pack_batch();

    /**
     * @brief report batch memory usage 
     * 
     * @return size_t - used memory in bytes
     */
    size_t memory_used();

    /**
     * @brief Structure for teh search results 
     * 
     */
    typedef struct {
        size_t pos{0}; // Global spot index
        bool wasInserted{true}; ///< Indicates if spot inserted (new spot)
        metadata_t* metadata{nullptr}; ///< Metadata for the spot
        size_t row_id{0}; ///< Metadata row
    } spot_rec_t;

    spot_rec_t m_rec;

    /**
     * @brief Implements spot search and returns populate spot_rec_t
     * 
     * @param name 
     * @param namelen 
     * @return const spot_rec_t& 
     */
    const spot_rec_t& find(const char* name, int namelen);

    /**
     * @brief Applies F to all metadata in the group
     * 
     * @tparam F 
     * @param f - Visitor (metadata_t& metadata, unsigned group_id, size_t offset) 
     *        params: metadata's group id and metadata's offset
     *        for row_id in 0 >= row_id < metadata.size()
     *        global keyId can be generated using the following formula
     *        ((uint64_t)group_id << GROUPID_SHIFT) | (offset + row_id);
     */

    template<typename F>
    void visit_metadata(F&& f, unsigned group_id);

#if defined(HAS_CTX_VALUE)
    //**
     * @brief Apply F to all keyId
     * 
     * @tparam F 
     * @param f 
     * @param group_id 
     * @param GROUPID_SHIFT 
     * @param col_index 
     */
    template<typename F>
    void visit_keyId(F&& f, unsigned group_id, unsigned GROUPID_SHIFT, unsigned col_index);
#endif


    /**
     * @brief Apply F to each spot name 
     * 
     * @tparam F 
     * @param f 
     */
    template<typename F>
    void visit_spots(F&& f);


    /**
     * @brief Clears specific metadata column memory
     * 
     * @tparam T 
     * @param col_index -- Column index
     */
    template<typename T>
    void clear_column(unsigned col_index);


    /**
     * @brief Extracts all values from 64-bit column into 32 and 8 bit arrays
     * 
     * @param col_index -- column index
     * @param values - 32-bit part of the value
     * @param ext -- 8-bit part of the value
     * @param clear -- if true clears column after extraction 
     */
    void extract_64bit_column(unsigned col_index, vector<uint32_t>& values, vector<uint8_t>& ext, bool clear = false);


    /**
     * @brief Extracts all values from 16-bit column into unit8_t array 
     * 
     * @param col_index -- column index
     * @param values - unit_8 array 
     * @param clear -- if true clears column after extraction 
     */
    void extract_16bit_column(unsigned col_index, vector<uint8_t>& values, bool clear = false);


    /**
     * @brief Finds metadata by global KeyId (expected to always find it)
     * 
     * @param keyId 
     * @return pair<metadata_t*, size_t> 
     */
    pair<metadata_t*, size_t> metadata_by_key(uint64_t keyId);

};

void spot_assembly::release_search_memory() 
{
    m_stop_packing = true;
    for (auto& batch : m_batches) {
        batch->release_search_memory();
    }
    m_spot_map.reset(nullptr);
}



void spot_assembly::pack_batch() 
{
    m_batches.push_back(make_unique<spot_batch>(m_offset, m_curr_row));

    string batch_idx = fmt::format("{}.{}", m_group_id, m_batches.size());
    auto batch = m_batches.back().get();
    batch->m_spot_map.swap(m_spot_map); 
    m_spot_map.reset(new array_map_t);
    m_spot_map->max_load_factor(64.);
    assert(m_curr_row > 0);
    m_spot_map->reserve(ceil((float)m_curr_row/10e6) * 10e6);

    m_offset += m_curr_row;
    m_curr_row = 0;

    batch->m_metadata.swap(m_metadata);
    batch->m_metadata->need_optimize = true;
    m_metadata.reset(new metadata_t);
    batch->m_pack_job = m_executor.async([this, batch, batch_idx]() {
        auto& new_batch = *batch;
        spdlog::stopwatch sw1;
        spdlog::stopwatch sw;
        // Get the list of current spot name and sort them
        vector<const char*> sss;
        sss.reserve(new_batch.m_spot_map->size());
        for(auto it = new_batch.m_spot_map->begin(); it != new_batch.m_spot_map->end(); ++it) {
            sss.push_back(it.key());
        }
        if (m_stop_packing)
            return;
        spdlog::info("{} Batch insert: {:.3}, {:L} reads", batch_idx, sw, new_batch.m_spot_map->size()); 
        sw.reset();
        tf::Taskflow taskflow;
        taskflow.sort(sss.begin(), sss.end(), [](const char* s1, const char* s2) {
            return strcmp(s1, s2) < 0;
        });
        m_executor.run(taskflow).wait();
        if (m_stop_packing)
            return;
        spdlog::info("{} Batch sort: {:.3}", batch_idx, sw); 
        sw.reset();
        // Populate succinct data and index with sorted spot names
        new_batch.m_data.reset(new str_sv_type); 
        new_batch.m_index.reset(new svector_u32); 
        {
            str_sv_type::back_insert_iterator sv_it = new_batch.m_data->get_back_inserter();
            svector_u32::back_insert_iterator idx_it = new_batch.m_index->get_back_inserter();
            for (auto& s : sss) {
                sv_it = s;
                idx_it = (uint32_t)new_batch.m_spot_map->at(s);
            }
            idx_it.flush();
            sv_it.flush();
        }
        
        sss.clear(); // We don't need spot_names anymore
        sss.shrink_to_fit();
        spdlog::info("{} Batch vector insert: {:.3}", batch_idx, sw); 
        sw.reset();

        new_batch.m_data->remap(); // Remap to optimize spot_name alphabet
        spdlog::info("{} Batch vector remap: {:.3}", batch_idx, sw); 
        sw.reset();
        if (m_stop_packing)
            return;
        // optimize and freeze succinct structures
        BM_DECLARE_TEMP_BLOCK(TB) // BitMagic Temporary block
        str_sv_type::statistics st1;
        new_batch.m_data->optimize(TB);
        new_batch.m_data->calc_stat(&st1);
        new_batch.m_memory_used = st1.memory_used;
        svector_u32::statistics st2;
        new_batch.m_index->optimize(TB, bm::bvector<>::opt_compress, &st2);
        new_batch.m_memory_used += st2.memory_used;

        spdlog::info("{} Batch vector optimize: {:.3}, sv {:L}, idx: {:L}", batch_idx, sw, st1.memory_used, st2.memory_used); 
        if (m_stop_packing)
            return;
        new_batch.m_data->freeze();
        new_batch.m_index->freeze();
        if (m_stop_packing)
            return;
        // Create and link scanner to spot_name vector    
        new_batch.m_scanner.reset(new spot_batch::scanner_t);
        new_batch.m_scanner->bind(*new_batch.m_data, true);
        new_batch.m_data_ready = true;
        spdlog::info("{} Batch done in : {:.3}", batch_idx, sw1); 

    });
} 

size_t spot_assembly::memory_used() 
{
    size_t m = 0;
    for (auto& b : m_batches) {
        m += b->m_memory_used;
        m += b->m_metadata->memory_used;
    }
    return m;
}


spot_assembly::spot_assembly(tf::Executor& executor, shared_ptr<spot_name_filter> key_filter, unsigned group_id, size_t batch_size ) 
    : m_executor{executor}
    , m_key_filter(key_filter)
    , m_group_id(group_id)
{
    m_batches.reserve(256);
    m_spot_map.reset(new array_map_t);
    m_spot_map->max_load_factor(64.);
    m_spot_map->reserve(batch_size);
    m_metadata.reset(new metadata_t);
}

spot_assembly::~spot_assembly() 
{
    m_stop_packing = true;
    for_each(m_batches.begin(), m_batches.end(), [] (auto& batch) { 
        if (batch->m_pack_job.valid())
            batch->m_pack_job.get();
    });
}

const spot_assembly::spot_rec_t& spot_assembly::find(const char* name, int namelen) 
{
#if defined (COLLECT_STATS)    
    static size_t count = 0;
    static size_t new_rec = 0;
    static size_t bloom_collisions = 0;
    static size_t hot_found = 0;
    static size_t batch_found = 0;
#endif    
    m_rec.wasInserted = true;
    if (m_key_filter->seen_before(name, namelen)) {
        auto it = m_spot_map->find_ks(name, namelen, m_key_filter->get_name_hash());

        if (it != m_spot_map->end()) {
#if defined (COLLECT_STATS)    
            ++hot_found;
#endif            
            m_rec.wasInserted = false;
            m_rec.row_id = *it;
            m_rec.pos = m_rec.row_id + m_offset;
            m_rec.metadata = m_metadata.get();
            return m_rec;
        } else if (m_batches.size() > 1) {
            m_taskflow.clear();
            m_search_done = false;
            m_taskflow.for_each(m_batches.rbegin(), m_batches.rend(), [this, &name, &namelen] (auto& batch) { 
                if (m_search_done)
                    return;
                if (batch->m_use_scanner) {
                    str_sv_type::size_type pos = 0;
                    if (batch->m_scanner->bfind_eq_str(name, namelen, pos)) { 
                        m_search_done = true;
                        m_rec.wasInserted = false;
                        m_rec.row_id = batch->m_index->get(pos);
                        m_rec.pos = m_rec.row_id + batch->m_offset;
                        m_rec.metadata = batch->m_metadata.get();
                    } 

                } else {
                    auto it = batch->m_spot_map->find_ks(name, namelen, m_key_filter->get_name_hash());
                    if (it != batch->m_spot_map->end()) {
                        m_search_done = true;
                        m_rec.wasInserted = false;
                        m_rec.row_id = *it;
                        m_rec.pos = m_rec.row_id + batch->m_offset;
                        m_rec.metadata = batch->m_metadata.get();
                    } 
                    if (batch->m_data_ready) {
                        batch->m_use_scanner = true;
                        batch->m_spot_map.reset();
                    }
                } 
            });
            m_executor.run(m_taskflow).wait();
    #if defined (COLLECT_STATS)    
                if (m_search_done)
                    ++batch_found;
    #endif                    
        } else if (m_batches.size() == 1) {
            auto& b = *m_batches.front();
            if (b.m_use_scanner) {
                str_sv_type::size_type pos = 0;
                if (b.m_scanner->bfind_eq_str(name, namelen,  pos)) { 
                    m_rec.wasInserted = false;
                    m_rec.row_id = b.m_index->get(pos);
                    m_rec.pos = m_rec.row_id + b.m_offset;
                    m_rec.metadata = b.m_metadata.get();
                    return m_rec;
                } 
            } else {
                auto it = b.m_spot_map->find_ks(name, namelen, m_key_filter->get_name_hash());
                if (it != b.m_spot_map->end()) {
                    m_rec.wasInserted = false;
                    m_rec.row_id = *it;
                    m_rec.pos = m_rec.row_id + b.m_offset;
                    m_rec.metadata = b.m_metadata.get();
                } 
                if (b.m_data_ready) {
                    b.m_use_scanner = true;
                    b.m_spot_map.reset();
                }
            } 
        }
#if defined (COLLECT_STATS)    
        if (rec.wasInserted) 
            ++bloom_collisions;
#endif            
    } 
    if (m_rec.wasInserted) {
        m_spot_map->insert_ks_hash(m_key_filter->get_name_hash(), name, namelen, m_curr_row); 
        m_rec.row_id = m_curr_row;
        m_rec.pos = m_curr_row + m_offset;
        m_rec.metadata = m_metadata.get();
        ++m_total_spots;
        ++m_curr_row;
#if defined (COLLECT_STATS)    
        ++new_rec;
#endif        
    }
#if defined (COLLECT_STATS)    
    if (++count % 10000000 == 0) {
        spdlog::info("New: {:L}, collisions: {:L}, found hot: {:L}, found batch: {:L}", new_rec, bloom_collisions, hot_found, batch_found);
        new_rec = bloom_collisions = hot_found = batch_found = 0;
    }
#endif    
    return m_rec;
}


template<typename F>
void spot_assembly::visit_metadata(F&& f, unsigned group_id) 
{
    if (m_metadata) {
        f(*m_metadata, group_id, m_offset);
    }
    for (auto& b : m_batches) {
        assert(b->m_metadata);
        if (b->m_metadata) {
            f(*b->m_metadata, group_id, b->m_offset);
        }
    }
}
#if defined(HAS_CTX_VALUE)

template<typename F>
void spot_assembly::visit_keyId(F&& f, unsigned group_id, unsigned GROUPID_SHIFT, unsigned col_index) 
{
    if (m_metadata) {
        auto sz = m_metadata->template get<u32_t>(col_index).size();
        for (auto row_id = 0; row_id < sz; ++row_id) {
            uint64_t keyId = (((uint64_t)group_id) << GROUPID_SHIFT) | (row_id + m_offset);
            f(keyId);
        }
    }
    for (auto& b : m_batches) {
        assert(b->m_metadata);
        if (b->m_metadata) {
            auto sz = b->m_metadata->template get<u32_t>(col_index).size();
            for (auto row_id = 0; row_id < sz; ++row_id) {
                uint64_t keyId = (((uint64_t)group_id) << GROUPID_SHIFT) | (row_id + b->m_offset);
                f(keyId);
            }
        }
    }
}
#endif


template<typename F>
void spot_assembly::visit_spots(F&& f) 
{
    for(auto it = m_spot_map->begin(); it != m_spot_map->end(); ++it) {
        f(it.key());
    }
    for (auto& b : m_batches) {
        if (b->m_use_scanner) {
            auto data_it = b->m_data->begin();
            while (data_it.valid()) {
                f(data_it.value());
                data_it.advance();
            }
        } else {
            for(auto it = b->m_spot_map->begin(); it != b->m_spot_map->end(); ++it) {
                f(it.key());
            }
        }
    }

}


//template<typename metadata_t>
//template<typename T>
//void spot_assembly<metadata_t>::clear_column(unsigned col_index) 
template<typename T>
void spot_assembly::clear_column(unsigned col_index) 
{
    if (m_metadata) {
        auto md = m_metadata->template get<T>(col_index);
        md.clear();
    }
    for (auto& b : m_batches) {
        assert(b->m_metadata);
        if (!b->m_metadata) 
            continue;
        auto md = b->m_metadata->template get<T>(col_index);
        md.clear();
    }
}


//template<typename metadata_t>
//void spot_assembly<metadata_t>::extract_64bit_column(unsigned col_index, vector<uint32_t>& values, vector<uint8_t>& ext, bool clear) 
void spot_assembly::extract_64bit_column(unsigned col_index, vector<uint32_t>& values, vector<uint8_t>& ext, bool clear) 
{
    values.resize(m_total_spots);
    ext.resize(m_total_spots);
    vector<u64_t::value_type> buffer;
    size_t offset = 0;
    for (auto& b : m_batches) {
        assert(b->m_metadata);
        if (!b->m_metadata) 
            continue;
        auto md = b->m_metadata->template get<u64_t>(col_index);
        assert(offset == b->m_offset);
        buffer.resize(b->m_batch_size);
        md.extract(buffer.data(), b->m_batch_size, 0);
        for (size_t i = 0; i < b->m_batch_size; ++i) {
            values[b->m_offset + i] = uint32_t(buffer[i]);
            ext[b->m_offset + i] = uint8_t(buffer[i] >> 32);
        }
        //md.extract(&data[b->m_offset], b->m_batch_size, 0);
        offset += b->m_batch_size;
        if (clear)
            md.clear();
    }
    if (m_metadata) {
        auto md = m_metadata->template get<u64_t>(col_index);
        assert(offset == m_offset);
        buffer.resize(m_curr_row);
        md.extract(buffer.data(), m_curr_row, 0);
        for (size_t i = 0; i < m_curr_row; ++i) {
            values[m_offset + i] = uint32_t(buffer[i]);
            ext[m_offset + i] = uint8_t(buffer[i] >> 32);
        }
        //md.extract(&data[m_offset], m_curr_row, 0);
        offset += m_curr_row;
        if (clear)
            md.clear();
    }

    if (offset != m_total_spots)
        spdlog::error("col: {}, offset != m_total_spots, {} != {}", col_index, offset, m_total_spots);
}


//template<typename metadata_t>
//void spot_assembly<metadata_t>::extract_16bit_column(unsigned col_index, vector<uint8_t>& values, bool clear) 
void spot_assembly::extract_16bit_column(unsigned col_index, vector<uint8_t>& values, bool clear) 
{
    values.resize(m_total_spots);
    vector<u16_t::value_type> buffer;
    size_t offset = 0;
    for (auto& b : m_batches) {
        assert(b->m_metadata);
        if (!b->m_metadata) 
            continue;
        auto md = b->m_metadata->template get<u16_t>(col_index);
        assert(offset == b->m_offset);
        buffer.resize(b->m_batch_size);
        md.extract(buffer.data(), b->m_batch_size, 0);
        for (size_t i = 0; i < b->m_batch_size; ++i) {
            values[b->m_offset + i] = uint8_t(buffer[i]);
        }
        //md.extract(&data[b->m_offset], b->m_batch_size, 0);
        offset += b->m_batch_size;
        if (clear)
            md.clear();
    }
    if (m_metadata) {
        auto md = m_metadata->template get<u16_t>(col_index);
        assert(offset == m_offset);
        buffer.resize(m_curr_row);
        md.extract(buffer.data(), m_curr_row, 0);
        for (size_t i = 0; i < m_curr_row; ++i) {
            values[m_offset + i] = uint8_t(buffer[i]);
        }
        //md.extract(&data[m_offset], m_curr_row, 0);
        offset += m_curr_row;
        if (clear)
            md.clear();
    }

    if (offset != m_total_spots)
        spdlog::error("col: {}, offset != m_total_spots, {} != {}", col_index, offset, m_total_spots);
}

//template<typename metadata_t>
//pair<metadata_t*, size_t> spot_assembly<metadata_t>::metadata_by_key(uint64_t keyId) 
pair<metadata_t*, size_t> spot_assembly::metadata_by_key(uint64_t keyId) 
{
    //keyId &= KEYID_MASK;
    if (keyId >= m_offset) {
        return make_pair<metadata_t*, size_t>(m_metadata.get(), keyId - m_offset);
    }
    auto it = m_batches.rbegin();
    auto it_end = m_batches.rend();
    while (it != it_end) {
        if (keyId >= (*it)->m_offset) {
            return make_pair<metadata_t*, size_t>((*it)->m_metadata.get(), keyId - (*it)->m_offset);
        }
        ++it;
    }
    assert(false);
    return make_pair<metadata_t*, size_t>(nullptr, 0);

}


#endif /* __SPOT_ASSEMBLY_HPP_ */

