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

template<typename metadata_t>
struct spot_batch 
{
    unique_ptr<array_map_t> m_spot_map;
    size_t m_offset = 0;
    size_t m_batch_size = 0;
    unique_ptr<str_sv_type> m_data;    ///<< sorted succinct spot names 
    unique_ptr<svector_u32> m_index;   ///<< succint spot name index
    unique_ptr<bm::sparse_vector_scanner<str_sv_type, 32>> m_scanner;
    tf::Future<void> m_pack_job;
    atomic<bool> m_data_ready{false};
    bool m_use_scanner{false};
    size_t m_memory_used = 0;
    unique_ptr<metadata_t> m_metadata;

    spot_batch(size_t offset, size_t batch_size) 
        : m_offset(offset)
        , m_batch_size(batch_size)
    {

    }
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
    
    bool find_row(const char* term, int term_len, uint64_t hash, size_t& row_id) 
    {
        str_sv_type::size_type pos = 0;
        bool found = false;
        if (m_use_scanner) {
            if (m_scanner->bfind_eq_str(term, term_len, pos)) { 
                row_id = m_index->get(pos);
                found = true;
            } 

        } else {
            auto it = m_spot_map->find_ks(term, term_len, hash);
            if (it != m_spot_map->end()) {
                row_id = *it;
                found = true;
            } 
            if (m_data_ready) {
                m_use_scanner = true;
                m_spot_map.reset();
            }
        } 
        return found;
    }

};

template<typename metadata_t>
struct spot_assembly 
{
    tf::Executor& m_executor;           ///< Taskflow executor (initialized in constructor)
    tf::Taskflow m_taskflow;            ///< taskflow used for searches
    shared_ptr<spot_name_filter> m_key_filter;                         ///< Spot name bloom filter
    const unsigned m_group_id;            ///< unique spot assembly (for reporting)
    vector<unique_ptr<spot_batch<metadata_t>>> m_batches; ///< List of search batches
    unique_ptr<array_map_t> m_spot_map;  ///< Current search map
    unique_ptr<metadata_t> m_metadata;    ///< Current metdata
    size_t m_offset = 0;                 ///< Current data offset  
    size_t m_curr_row = 0;               ///< Current row 
    size_t m_total_spots = 0;            ///< Total number of spots 
    unsigned m_platform = 0;               ///< Assembly platform 
    std::optional<tuple<uint64_t, metadata_t*, uint32_t>> m_search_result;
    atomic<bool> m_stop_packing{false};
    atomic<bool> m_search_done;
    
    size_t m_key_filter_total = 0;
    size_t m_key_filter_miss = 0;

/**
 * @brief Release memory realted to spot name searches, keep metadata
 * 
 * 
 */
    void release_search_memory() {
        m_stop_packing = true;
        for (auto& batch : m_batches) {
            batch->release_search_memory();
        }
        m_spot_map.reset(nullptr);
    }

/**
 * @brief Add batch and run background packing job
 * 
 */
    void pack_batch() 
    {
        m_batches.push_back(make_unique<spot_batch<metadata_t>>(m_offset, m_curr_row));

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
            new_batch.m_scanner.reset(new bm::sparse_vector_scanner<str_sv_type, 32>);
            new_batch.m_scanner->bind(*new_batch.m_data, true);
            new_batch.m_data_ready = true;
            spdlog::info("{} Batch done in : {:.3}", batch_idx, sw1); 

        });
    } 

    /**
     * @brief report batch memory usage 
     * 
     * @return size_t - used memory in bytes
     */
    size_t memory_used() {
        size_t m = 0;
        for (auto& b : m_batches) {
            m += b->m_memory_used;
            m += b->m_metadata->memory_used;
        }
        return m;
    }


    spot_assembly(tf::Executor& executor, shared_ptr<spot_name_filter> key_filter, unsigned group_id, size_t batch_size ) 
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

    ~spot_assembly() 
    {
        m_stop_packing = true;
        for_each(m_batches.begin(), m_batches.end(), [] (auto& batch) { 
            if (batch->m_pack_job.valid())
                batch->m_pack_job.get();
        });
    }

    typedef struct {
        size_t pos{};
        bool wasInserted{true};
        metadata_t* metadata{nullptr};
        size_t row_id{0};
    } spot_rec_t;

    spot_rec_t rec;

    const spot_rec_t& find(const char* name, int namelen) 
    {
        static size_t count = 0;
        static size_t new_rec = 0;
        static size_t bloom_collisions = 0;
        static size_t hot_found = 0;
        static size_t batch_found = 0;
        rec.pos = 0;
        rec.wasInserted = true;
        if (m_key_filter->seen_before(name, namelen)) {
            auto it = m_spot_map->find_ks(name, namelen, m_key_filter->get_name_hash());

            if (it != m_spot_map->end()) {
                ++hot_found;
                rec.pos = m_offset + *it;
                rec.wasInserted = false;
                rec.row_id = *it;
                rec.metadata = m_metadata.get();
            } else {
                m_taskflow.clear();
                m_search_result.reset();
                m_search_done = false;
#ifndef TWO_THREAD_PER_BACTH                
                m_taskflow.for_each(m_batches.rbegin(), m_batches.rend(), [this, &name, &namelen] (auto& batch) { 
                    if (m_search_done)
                        return;
                    size_t row_id;
                    if (batch->find_row(name, namelen, m_key_filter->get_name_hash(), row_id)) {
                        m_search_done = true;
                        rec.wasInserted = false;
                        rec.pos = row_id + batch->m_offset;
                        rec.metadata = batch->m_metadata.get();
                        rec.row_id = row_id;
                    }    
                });
#else
                int sz = m_batches.size();
                m_taskflow.for_each_index(sz, 0, -2, [this, &name, &namelen](int i) { 
                    if (m_search_done)
                        return;
                    auto& b1 = *m_batches.at(i - 1);
                    size_t row_id;
                    if (b1.find_row(name, namelen, m_key_filter->get_name_hash(), row_id)) {
                        m_search_done = true;
                        rec.wasInserted = false;
                        rec.pos = row_id + b1.m_offset;
                        rec.metadata = b1.m_metadata.get();
                        rec.row_id = row_id;
                        return;
                    }    
                    if (m_search_done || i < 2)
                        return;
                    auto& b2 = *m_batches.at(i - 2);
                    if (b2.find_row(name, namelen, m_key_filter->get_name_hash(), row_id)) {
                        m_search_done = true;
                        rec.wasInserted = false;
                        rec.pos = row_id + b2.m_offset;
                        rec.metadata = b2.m_metadata.get();
                        rec.row_id = row_id;
                    }    
                }); 
#endif
                m_executor.run(m_taskflow).wait();                
                if (m_search_done)
                    ++batch_found;
            }
            if (rec.wasInserted) 
                ++bloom_collisions;
        } 
        if (rec.wasInserted) {
            m_spot_map->insert_ks_hash(m_key_filter->get_name_hash(), name, namelen, m_curr_row); 
            rec.row_id = m_curr_row;
            rec.metadata = m_metadata.get();
            rec.pos = m_curr_row + m_offset;
            ++m_total_spots;
            ++m_curr_row;
            ++new_rec;
        }
        if (++count % 10000000 == 0) {
            spdlog::info("New: {:L}, collisions: {:L}, found hot: {:L}, found batch: {:L}", new_rec, bloom_collisions, hot_found, batch_found);
            new_rec = bloom_collisions = hot_found = batch_found = 0;
        }
        return rec;
    }

    template<typename F>
    void visit_metadata(F&& f, unsigned group_id) {
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

    template<typename F>
    void visit_keyId(F&& f, unsigned group_id, unsigned GROUPID_SHIFT, unsigned col_index) {
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

    template<typename F>
    void visit_spots(F&& f) {
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

    template<typename T>
    void clear_column(unsigned col_index) 
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


    template<typename TSource, typename TDest>
    void extract_column_ext(unsigned col_index, vector<TDest>& data, bool clear = false) 
    {
        data.resize(m_total_spots);
        vector<typename TSource::value_type> buffer;
        size_t offset = 0;
        for (auto& b : m_batches) {
            assert(b->m_metadata);
            if (!b->m_metadata) 
                continue;
            auto md = b->m_metadata->template get<TSource>(col_index);
            assert(offset == b->m_offset);
            buffer.resize(b->m_batch_size);
            md.extract(buffer.data(), b->m_batch_size, 0);
            for (size_t i = 0; i < b->m_batch_size; ++i) {
                data[b->m_offset + i].set(buffer[i]);
            }
            //md.extract(&data[b->m_offset], b->m_batch_size, 0);
            offset += b->m_batch_size;
            if (clear)
                md.clear();
        }
        if (m_metadata) {
            auto md = m_metadata->template get<TSource>(col_index);
            assert(offset == m_offset);
            buffer.resize(m_curr_row);
            md.extract(buffer.data(), m_curr_row, 0);
            for (size_t i = 0; i < m_curr_row; ++i) {
                data[m_offset + i].set(buffer[i]);
            }
            //md.extract(&data[m_offset], m_curr_row, 0);
            offset += m_curr_row;
            if (clear)
                md.clear();
        }

        if (offset != m_total_spots)
            spdlog::error("col: {}, offset != m_total_spots, {} != {}", col_index, offset, m_total_spots);
    }

    void extract_64bit_column(unsigned col_index, vector<uint32_t>& values, vector<uint8_t>& ext, bool clear = false) 
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

    void extract_16bit_column(unsigned col_index, vector<uint8_t>& values, bool clear = false) 
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


    template<typename T>
    void extract_column(unsigned col_index, vector<typename T::value_type>& data, bool clear = false) 
    {
        data.resize(m_total_spots);
        size_t offset = 0;
        for (auto& b : m_batches) {
            assert(b->m_metadata);
            if (!b->m_metadata) 
                continue;
            auto md = b->m_metadata->template get<T>(col_index);
            assert(offset == b->m_offset);
            md.extract(&data[b->m_offset], b->m_batch_size, 0);
            offset += b->m_batch_size;
            if (clear)
                md.clear();
        }
        if (m_metadata) {
            auto md = m_metadata->template get<T>(col_index);
            assert(offset == m_offset);
            md.extract(&data[m_offset], m_curr_row, 0);
            offset += m_curr_row;
            if (clear)
                md.clear();
        }

        if (offset != m_total_spots)
            spdlog::error("col: {}, offset != m_total_spots, {} != {}", col_index, offset, m_total_spots);
    }

    pair<metadata_t*, size_t> metadata_by_key(uint64_t keyId) {
        //keyId &= KEYID_MASK;
        if (keyId >= m_offset) {
            return make_pair<metadata_t*, size_t>(m_metadata.get(), keyId - m_offset);
        }
        
        //assert(m_batches.empty() == false);
        //size_t batch_idx = keyId / m_batch_size; 
        //assert(batch_idx < m_batches.size());
        //assert(keyId >= m_batches[batch_idx]->m_offset);
        //auto rowId = keyId - m_batches[batch_idx]->m_offset;
        //return make_pair<metadata_t*, size_t>(m_batches[batch_idx]->m_metadata.get(), keyId - m_batches[batch_idx]->m_offset);

        auto it = m_batches.rbegin();
        auto it_end = m_batches.rend();
        while (it != it_end) {
            if (keyId >= (*it)->m_offset) {
                //if (rowId != (keyId - (*it)->m_offset)) {
                //    spdlog::error("rowId mismatch {} != {}", rowId, (keyId - (*it)->m_offset));
                //}
                return make_pair<metadata_t*, size_t>((*it)->m_metadata.get(), keyId - (*it)->m_offset);
            }
            ++it;
        }
        assert(false);
        return make_pair<metadata_t*, size_t>(nullptr, 0);

    }

};

#endif /* __SPOT_ASSEMBLY_HPP_ */

