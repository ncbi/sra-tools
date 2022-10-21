#ifndef BMDBG__H__INCLUDED__
#define BMDBG__H__INCLUDED__
/*
Copyright(c) 2002-2017 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

For more information please visit:  http://bitmagic.io
*/

/*! \file bmdbg.h
    \brief Debugging functions (internal)
*/


#include <cstdio>
#include <stdlib.h>
#include <cassert>
#include <memory>
#include <time.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <vector>

#include "bmalgo_similarity.h"
#include "bmsparsevec_serial.h"
#include "bmdef.h"



#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4311 4312 4127)
#endif

namespace bm
{

inline
void PrintGap(const bm::gap_word_t* gap_buf)
{
    unsigned len = (*gap_buf >> 3);
    std::cout << "[" << *gap_buf << " len=" << len << "] ";
    for (unsigned i = 0; i < len; ++i)
    {
        ++gap_buf;
        std::cout << *gap_buf << "; ";
    } 
    std::cout << std::endl;
}

inline
void PrintDGap(const bm::gap_word_t* gap_buf, unsigned gap_len=0)
{

    unsigned len = gap_len ? gap_len : (*gap_buf >> 3);
    std::cout << "[" " len=" << len << "] ";
    unsigned i = gap_len ? 0 : 1;
    for (; i < len; ++i)
    {
        std::cout << gap_buf[i] << "; ";
    } 
    std::cout << std::endl;
}

inline unsigned int iLog2(unsigned int value)
{
    unsigned int l = 0;
    while( (value >> l) > 1 ) ++l;
    return l;
}

inline
unsigned PrintGammaCode(unsigned value)
{
    unsigned bits = 0;
    // Elias gamma encode
    {
        unsigned l = iLog2(value);
        //cout << "log2=" << l << endl;
        for (unsigned i = 0; i < l; ++i)
        {
            std::cout << 0;
            ++bits;
        }
        std::cout << 1; ++bits;
        for (unsigned i = 0; i < l; ++i)
        {
            if (value & 1 << i) 
                std::cout << 1;
            else
                std::cout << 0;
            ++bits;
        }
    }
    return bits;
}

inline 
void PrintDGapGamma(const bm::gap_word_t* gap_buf, unsigned gap_len=0)
{
    unsigned total = 0;
    unsigned len = gap_len ? gap_len : (*gap_buf >> 3);
    std::cout << "[" " len=" << len << "] ";
    unsigned i = gap_len ? 0 : 1;
    for (; i < len; ++i)
    {
        unsigned v = gap_buf[i];

        unsigned bits = PrintGammaCode(v+1);
        std::cout << "; ";
        total += bits;
    } 
    std::cout << "  gamma_bits=" << total << " src_bits =" << len * 16;
    std::cout << std::endl;

}

/// Read dump file into an STL container (vector of some basic type)
///
/// @return 0 - if reading went well
///
template<class VT>
int read_dump_file(const std::string& fname, VT& data)
{
    typedef typename VT::value_type  value_type;

    size_t fsize;
    std::ifstream fin(fname.c_str(), std::ios::in | std::ios::binary);
    if (!fin.good())
    {
        return -1;
    }
    fin.seekg(0, std::ios::end);
    fsize = (size_t)fin.tellg();
    
    data.resize(fsize/sizeof(value_type));

    if (!fsize)
    {
        return 0; // empty input
    }
    fin.seekg(0, std::ios::beg);
    fin.read((char*) &data[0], std::streamsize(fsize));
    if (!fin.good())
    {
        data.resize(0);
        return -2;
    }
    return 0;    
}

template<class TBV>
void LoadBVector(const char* fname, TBV& bvector, unsigned* file_size=0)
{
    std::ifstream bv_file (fname, std::ios::in | std::ios::binary);
    if (!bv_file.good())
    {
        std::cout << "Cannot open file: " << fname << std::endl;
        exit(1);
    }
    bv_file.seekg(0, std::ios_base::end);
    unsigned length = (unsigned)bv_file.tellg();
    if (length == 0)
    {
        std::cout << "Empty file:" << fname << std::endl;
        exit(1);
    }
    if (file_size)
        *file_size = length;
        
    bv_file.seekg(0, std::ios::beg);
    
    char* buffer = new char[length];

    bv_file.read(buffer, length);
    
    bm::deserialize(bvector, (unsigned char*)buffer);
    
    delete [] buffer;
}

template<class TBV>
void SaveBVector(const char* fname, const TBV& bvector)
{
    std::ofstream bfile (fname, std::ios::out | std::ios::binary);
    if (!bfile.good())
    {
        std::cout << "Cannot open file: " << fname << std::endl;
        exit(1);
    }
    typename TBV::statistics st1;
    bvector.calc_stat(&st1);

    unsigned char* blob = new unsigned char[st1.max_serialize_mem];
    size_t blob_size = bm::serialize(bvector, blob);


    bfile.write((char*)blob, std::streamsize(blob_size));
    bfile.close();

    delete [] blob;
}

inline
void SaveBlob(const char* name_prefix, unsigned num, const char* ext,
              const unsigned char* blob, size_t blob_size)
{
    std::stringstream fname_str;
    fname_str << name_prefix << "-" << num << ext;
    
	std::string s = fname_str.str();
    const char* fname = s.c_str();
    std::ofstream bfile (fname, std::ios::out | std::ios::binary);
    if (!bfile.good())
    {
        std::cout << "Cannot open file: " << fname << std::endl;
        exit(1);
    }
    bfile.write((char*)blob, std::streamsize(blob_size));
    bfile.close();
}


template<typename V> 
void PrintBinary(V val)
{
    for (unsigned i = 0; i < sizeof(V)*8; i++)
    {
        std::cout << (unsigned)((val >> i) & 1);
        if (i == 15 && (sizeof(V)*8 > 16)) std::cout << "-";
    }
//    cout << " :" << val;
}

inline 
void PrintBits32(unsigned val)
{
    PrintBinary(val);
}

inline
void PrintDistanceMatrix(
   const unsigned distance[bm::set_block_plane_cnt][bm::set_block_plane_cnt])
{
    for (unsigned i = 0; i < bm::set_block_plane_cnt; ++i)
    {
        const unsigned* row = distance[i];
        std::cout << i << ": ";
        for (unsigned j = i; j < bm::set_block_plane_cnt; ++j)
        {
            std::cout << std::setw(4) << std::setfill('0') << row[j] << " ";
        }
        std::cout << std::endl;
    }
}

template<typename TM>
void PrintTMatrix(const TM& tmatrix, unsigned cols=0, bool binary = false)
{
    unsigned columns = cols ? cols : tmatrix.cols();
    for (unsigned i = 0; i < tmatrix.rows(); ++i)
    {
        const typename TM::value_type* row = tmatrix.row(i);
        std::cout << i << ": ";
        if (i < 10) std::cout << " ";
        for (unsigned j = 0; j < columns; ++j)
        {
            if (!binary)
            {
                std::cout << std::setw(4) << std::setfill('0') << row[j] << " ";
            }
            else
            {
                PrintBinary(row[j]);
            }
        }
        std::cout << std::endl;
    }
}

/// Binary code string converted to number
/// Bits are expected left to right
///
inline
unsigned BinStrLR(const char* str)
{
    unsigned value = 0;
    unsigned bit_idx = 0;
    for (; *str; ++str)
    {
        switch(*str)
        {
        case '0': 
            ++bit_idx;
            break;
        case '1':
            value |= (1 << bit_idx);
            ++bit_idx;
            break;
        default:
            assert(0);
        }
        if (bit_idx == sizeof(unsigned) * 8) 
            break;
    }    
    return value;
}

template<class BV>
void print_blocks_count(const BV& bv)
{
    const unsigned sz = 128000;
    unsigned* bc_arr = new unsigned[sz];
    for(unsigned x = 0; x < sz; ++x) bc_arr[x] = 0;


    unsigned last_block = bv.count_blocks(bc_arr);
    unsigned sum = 0;

    for (unsigned i = 0; i <= last_block; ++i)
    {
        std::cout << i << ":";

        unsigned j = 0;
        for (; i <= last_block; ++i)
        {
            std::cout << std::setw(5) << std::setfill('0') << bc_arr[i] << " ";
            sum += bc_arr[i];
            if (++j == 10) break;
        }
        std::cout << " | " << sum << std::endl;
    }
    std::cout << "Total=" << sum << std::endl;

    delete [] bc_arr;
}
inline 
void print_bc(unsigned i, unsigned count)
{
    static unsigned sum = 0;
    static unsigned row_idx = 0;
    static unsigned prev = 0;

    if (i == 0) 
    {
        sum = row_idx = 0;
    }
    else
    {
        if (prev +1 < i)
            print_bc(prev+1, 0);
        prev = i;
    }

    if (row_idx == 0)
    {
        std::cout << i << ":";
    }

    std::cout << std::setw(5) << std::setfill('0') << count << " ";
    sum += count;

    ++row_idx;
    if (row_idx == 10)
    {
        row_idx = 0;
        std::cout << " | " << sum << std::endl;
    }
}

template<class BV>
size_t print_bvector_stat(const BV& bvect)
{
    typename BV::statistics st;
    bvect.calc_stat(&st);

    typename serializer<BV>::buffer buf;
    bm::serializer<BV> ser;
    ser.serialize(bvect, buf, &st);
    auto ssize = buf.size();
    
    std::cout << " - Blocks: [ "
              << "B:"     << st.bit_blocks
              << ", G:"   << st.gap_blocks << "] "
              << " count() = " << bvect.count() 
              << ", mem = " << st.memory_used << " " << (st.memory_used / (1024 * 1024)) << "MB "
              << ", max smem:" << st.max_serialize_mem << " " << (st.max_serialize_mem / (1024 * 1024)) << "MB "
              << " compressed = " << ssize << " " << (ssize / (1024 * 1024)) << "MB "
              << std::endl;
    return ssize;
}


template<class BV>
void print_stat(const BV& bv, typename BV::block_idx_type blocks = 0)
{
    const typename BV::blocks_manager_type& bman = bv.get_blocks_manager();

    bm::id_t count = 0;
    int printed = 0;

    int total_gap_eff = 0;

    if (!blocks)
    {
        blocks = bm::set_total_blocks;
    }

    typename BV::block_idx_type nb;
    typename BV::block_idx_type nb_prev = 0;
    for (nb = 0; nb < blocks; ++nb)
    {
        unsigned i0, j0;
        bm::get_block_coord(nb, i0, j0);
        const bm::word_t* blk = bman.get_block(i0, j0);

        if (!blk)
           continue;

        if (IS_FULL_BLOCK(blk))
        {
           if (BM_IS_GAP(blk)) // gap block
           {
               std::cout << "[Alert!" << nb << "]";
               assert(0);
           }
           
           typename BV::block_idx_type start = nb;
           for(auto i = nb+1; i < bm::set_total_blocks; ++i, ++nb)
           {
               bm::get_block_coord(nb, i0, j0);
               blk = bman.get_block(i0, j0);
               if (IS_FULL_BLOCK(blk))
               {
                 if (BM_IS_GAP(blk)) // gap block
                 {
                     std::cout << "[Alert!" << nb << "]";
                     assert(0);
                     --nb;
                     break;
                 }

               }
               else
               {
                  --nb;
                  break;
               }
           }

           std::cout << "{F." << start << ":" << nb << "}";
           ++printed;
        }
        else
        {
            if ((nb-1) != nb_prev)
            {
                std::cout << ".." << (size_t)nb-nb_prev << "..";
            }

            if (BM_IS_GAP(blk))
            {
                unsigned bc = bm::gap_bit_count(BMGAP_PTR(blk));
                /*unsigned sum = */bm::gap_control_sum(BMGAP_PTR(blk));
                unsigned level = bm::gap_level(BMGAP_PTR(blk));
                count += bc;
               unsigned len = bm::gap_length(BMGAP_PTR(blk))-1;
               unsigned raw_size=bc*2;
               unsigned cmr_len=len*2;
               size_t mem_eff = raw_size - cmr_len;
               total_gap_eff += unsigned(mem_eff);
               
               unsigned i,j;
               bm::get_block_coord(nb, i, j);
               std::cout << " [GAP " << nb << "(" << i << "," << j << ")"
                         << "=" << bc << ":" << level << "-L" << len << "(" << mem_eff << ")]";
                ++printed;
            }
            else // bitset
            {
                unsigned bc = bm::bit_block_count(blk);

                unsigned zw = 0;
                for (unsigned i = 0; i < bm::set_block_size; ++i) 
                {
                    zw += (blk[i] == 0);
                }

                count += bc;
                std::cout << " (BIT " << nb << "=" << bc << "[" << zw << "])";
                ++printed;
            }
        }
        if (printed == 10)
        {
            printed = 0;
            printf("\n");
        }
        nb_prev = nb;
    } // for nb
    std::cout << std::endl << "gap_efficiency=" << total_gap_eff << std::endl;

}

template<class BV>
size_t compute_serialization_size(const BV& bv)
{
    BM_DECLARE_TEMP_BLOCK(tb)
    unsigned char*  buf = 0;
    typename BV::size_type blob_size = 0;
    try
    {
        bm::serializer<BV> bvs(typename BV::allocator_type(), tb);
        //bvs.set_compression_level(4);
        
        typename BV::statistics st;
        bv.calc_stat(&st);
        
        buf = new unsigned char[st.max_serialize_mem];
        blob_size = (unsigned)bvs.serialize(bv, (unsigned char*)buf, st.max_serialize_mem);
    }
    catch (...)
    {
        delete [] buf;
        throw;
    }
    
    delete [] buf;
    return blob_size;
}

#if 0
template<class SV>
void print_svector_xor_stat(const SV& sv)
{
    BM_DECLARE_TEMP_BLOCK(tb)
    typename SV::size_type sz = sv.size();
    if (!sz)
        return;
    typename SV::size_type nb_max = (sz >>  bm::set_block_shift);

    for (typename SV::size_type nb = 0; nb < nb_max; ++nb)
    {
        std::cout << "nb = " << nb << std::endl;

        unsigned i0 = unsigned(nb >> bm::set_array_shift);
        unsigned j0 = unsigned(nb &  bm::set_array_mask);

        auto planes = sv.planes();
        for (unsigned i = 0; i < planes; ++i)
        {
            const typename SV::bvector_type* bv = sv.get_plane(i);
            if (!bv)
                continue;
            const typename SV::bvector_type::blocks_manager_type& bman = bv->get_blocks_manager();
            const bm::word_t* block = bman.get_block_ptr(i0, j0);
            if (!IS_VALID_ADDR(block) || BM_IS_GAP(block))
                continue;

            // compute block complexity
            bm::block_waves_xor_descr  x_descr;
            bm::compute_complexity_descr(block, x_descr);
            unsigned gc, bc;
            bm::bit_block_change_bc32(block, &gc, &bc);
            unsigned best_metric, block_metric;
            block_metric = best_metric = gc < bc ? gc : bc;

            bool kb_found = false;
            bm::id64_t d64 = 0;
            for (unsigned k = i + 1; k < planes; ++k)
            {
                const typename SV::bvector_type* bv_x = sv.get_plane(i);
                if (!bv_x)
                    continue;
                const typename SV::bvector_type::blocks_manager_type& bman_x = bv_x->get_blocks_manager();
                const bm::word_t* block_x = bman_x.get_block_ptr(i0, j0);
                if (!IS_VALID_ADDR(block_x) || BM_IS_GAP(block_x))
                    continue;

                // evaluate potential key block as XOR filter
                bm::id64_t kb_d64 =
                    bm::compute_xor_complexity_descr(block, block_x, x_descr);
                if (kb_d64) // candidate XOR filter found
                {
                    bm::bit_block_xor_product(tb, block, block_x, kb_d64);
                    unsigned kb_bc, kb_gc;
                    bm::bit_block_change_bc32(tb, &kb_gc, &kb_bc);
                    if (kb_gc < best_metric && kb_gc < bm::bie_cut_off)
                    {
                        d64 = kb_d64;
                        best_metric = kb_gc;
                        kb_found = true;
                        //*kb_j = j0;
                    }
                    if (kb_bc < best_metric && kb_bc < bm::bie_cut_off)
                    {
                        d64 = kb_d64;
                        best_metric = kb_bc;
                        kb_found = true;
                        //*kb_j = j0;
                    }

                }

            } // for k

            if (kb_found)
            {
                std::cout << "XOR match " << "metric gain = " << std::endl;
            }


            std::cout << std::endl;

        } // for i

    } // for nb
}
#endif

template<class SV>
void print_svector_stat(const SV& svect, bool print_sim = false)
{
    typedef typename SV::bvector_type bvector_type;
    /// Functor to compute jaccard similarity
    /// \internal
    struct Jaccard_Func
    {
        unsigned operator () (distance_metric_descriptor* dmit,
                              distance_metric_descriptor* /*dmit_end*/)
        {
            double d;
            BM_ASSERT(dmit->metric == COUNT_AND);
            typename bvector_type::size_type cnt_and = dmit->result;
            ++dmit;
            BM_ASSERT(dmit->metric == COUNT_OR);
            typename bvector_type::size_type cnt_or = dmit->result;
            if (cnt_and == 0 || cnt_or == 0)
            {
                d = 0.0;
            }
            else
            {
                d = double(cnt_and) / double(cnt_or);
            }
            unsigned res = unsigned(d * 100);
            if (res > 100) res = 100;
            return res;
        }
    };

    typedef  bm::similarity_descriptor<bvector_type, 2, unsigned, unsigned, Jaccard_Func> similarity_descriptor_type;
    typedef bm::similarity_batch<similarity_descriptor_type> similarity_batch_type;
    
    similarity_batch_type sbatch;
    
    bm::build_jaccard_similarity_batch(sbatch, svect);
    
    sbatch.calculate();
    sbatch.sort();
    
    typename similarity_batch_type::vector_type& sim_vec = sbatch.descr_vect_;
    if (print_sim)
    {
        for (size_t k = 0; k < sim_vec.size(); ++k)
        {
            unsigned sim = sim_vec[k].similarity();
            if (sim > 10)
            {
                const typename SV::bvector_type* bv1 = sim_vec[k].get_first();
                const typename SV::bvector_type* bv2 = sim_vec[k].get_second();

                auto bv_size2 = compute_serialization_size(*bv2);
                
                typename SV::bvector_type bvx(*bv2);
                bvx ^= *bv1;
                
                auto bv_size_x = compute_serialization_size(bvx);
                if (bv_size_x < bv_size2) // true savings
                {
                    size_t diff = bv_size2 - bv_size_x;
                    
                    // compute 10% cut-off
                    size_t sz10p = bv_size2 / 10;
                    if (diff > sz10p)
                    {
                        std:: cout << "["  << sim_vec[k].get_first_idx()
                                   << ", " << sim_vec[k].get_second_idx()
                                   << "] = "  << sim
                                   << " size(" << sim_vec[k].get_second_idx() << ")="
                                   << bv_size2
                                   << " size(x)=" << bv_size_x
                                   << " diff=" << diff
                                   << std:: endl;
                    }
                }
            }
        } // for k
    }
    

    typename SV::statistics st;
    svect.calc_stat(&st);
    
    std::cout << "size = " << svect.size() << std::endl;
    std::cout << "Bit blocks:       " << st.bit_blocks << std::endl;
    std::cout << "Gap blocks:       " << st.gap_blocks << std::endl;
    std::cout << "Max serialize mem:" << st.max_serialize_mem << " "
              << (st.max_serialize_mem / (1024 * 1024)) << "MB" << std::endl;
    std::cout << "Memory used:      " << st.memory_used << " "
              << (st.memory_used / (1024 * 1024))       << "MB" << std::endl;
    
    auto eff_max_element = svect.effective_vector_max();
    size_t std_vect_size = sizeof(typename SV::value_type) * svect.size() * eff_max_element;
    std::cout << "Projected mem usage for vector<value_type>:"
              << std_vect_size << " "
              << std_vect_size / (1024 * 1024) << "MB"
              << std::endl;
    if (sizeof(typename SV::value_type) > 4 && (eff_max_element == 1))
    {
        std::cout << "Projected mem usage for vector<long long>:"
                  << sizeof(long long) * svect.size() << std::endl;
    }
    
    std::cout << "\nplanes:" << std::endl;

    size_t ssize(0), octet_ssize(0);

    typename SV::bvector_type bv_join; // global OR of all planes
    auto planes = svect.get_bmatrix().rows();//slices();

    unsigned octet_cnt(0), octet(0);
    for (unsigned i = 0; i < planes; ++i)
    {
        const typename SV::bvector_type* bv_plane = svect.get_slice(i);
        std::cout << i << "-" << octet_cnt << ":";
        if (bv_plane == 0)
        {
            std::cout << "NULL\n";
            bool any_else = false;
            for (unsigned j = i+1; j < planes; ++j) // look ahead
            {
                if (svect.get_slice(j))
                {
                    any_else = true;
                    break;
                }
            }
            if (!any_else)
                break;
        }
        else
        {
            bv_join |= *bv_plane;
            auto pssize = bm::print_bvector_stat(*bv_plane);
            ssize += pssize;
            octet_ssize += pssize;
        }
        if (octet_cnt == 7)
        {
            std::cout << "--------------------" << std::endl;
            std::cout << "octet N = " << octet <<
                    "  compressed = " << octet_ssize <<
                    " " << octet_ssize/(1024*1024) << "MB" << std::endl;
            octet_cnt = 0; octet_ssize = 0;
            octet++;
            std::cout << std::endl;
        }
        else
        {
            octet_cnt++;
        }
    } // for i

    const typename SV::bvector_type* bv_null = svect.get_null_bvector();
    if (bv_null)
    {
        std::cout << "(not) NULL plane:\n";
        ssize += print_bvector_stat(*bv_null);
        typename SV::size_type not_null_cnt = bv_null->count();
        std::cout << " - Bitcount: " << not_null_cnt << std::endl;

        std::cout << "Projected mem usage for std::vector<pair<unsigned, value_type> >:"
            << ((sizeof(typename SV::value_type) + sizeof(unsigned)) * not_null_cnt) << " "
            << ((sizeof(typename SV::value_type) + sizeof(unsigned)) * not_null_cnt) / (1024 * 1024) << "MB"
            << std::endl;
    }

    std::cout << " Total serialized size (planes): " << ssize
              << std::endl
              << " " << ssize / (1024 * 1024) << " MB" << std::endl;

    if (svect.size())
    {
        bm::id64_t bv_join_cnt = bv_join.count();
        double fr = double(bv_join_cnt) / double (svect.size());
        std::cout << "Non-zero elements: " << bv_join_cnt << " "
                  << "ratio=" << fr
                  << std::endl;
        size_t non_zero_mem = size_t(bv_join_cnt) * sizeof(typename SV::value_type);
        std::cout << "Projected mem usage for non-zero elements: " << non_zero_mem << " "
                  << non_zero_mem / (1024*1024) << " MB"
                  << std::endl;        
    }
}


template<class SV>
void print_str_svector_stat(const SV& str_svect)
{
    typename SV::plane_octet_matrix_type octet_stat_matr;
    
    str_svect.calc_octet_stat(octet_stat_matr);
    
    for (unsigned i = 0; i < octet_stat_matr.rows(); ++i)
    {
        const typename SV::plane_octet_matrix_type::value_type* row
                                                = octet_stat_matr.row(i);
        bool any = false;
        for (unsigned j = 0; j < octet_stat_matr.cols(); ++j)
        {
            if (row[j]) // letter is present
            {
                any = true;
                break;
            }
        }
        if (!any)
            continue;
    
        std::cout << i << " : ";
        unsigned cnt = 0;
        for (unsigned j = 0; j < octet_stat_matr.cols(); ++j)
        {
            if (row[j]) // letter is present
            {
                std::cout << char(j);
                ++cnt;
            }
        } // for j
        if (cnt)
        {
            std::cout << "\t total= " << cnt;
        }
        else
        {
            std::cout << " (empty) ";
        }
        std::cout << std::endl;
    } // for i
}



// save compressed collection to disk
//
template<class CBC>
int file_save_compressed_collection(const CBC& cbc, const std::string& fname, size_t* blob_size = 0)
{
    bm::compressed_collection_serializer<CBC > cbcs;
    typename CBC::buffer_type sbuf;

    cbcs.serialize(cbc, sbuf);

    std::ofstream fout(fname.c_str(), std::ios::binary);
    if (!fout.good())
    {
        return -1;
    }
    const char* buf = (char*)sbuf.buf();
    fout.write(buf, sbuf.size());
    if (!fout.good())
    {
        return -1;
    }

    fout.close();

    if (blob_size)
    {
        *blob_size = sbuf.size();
    }
    return 0;
}

// load compressed collection from disk
//
template<class CBC>
int file_load_compressed_collection(CBC& cbc, const std::string& fname)
{
    std::vector<unsigned char> buffer;

    // read the input buffer, validate errors
    auto ret = bm::read_dump_file(fname, buffer);
    if (ret != 0)
    {
        return -2;
    }
    if (buffer.size() == 0)
    {
        return -3;
    }
    
    const unsigned char* buf = &buffer[0];
    
    compressed_collection_deserializer<CBC> cbcd;
    cbcd.deserialize(cbc, buf);

    return 0;
}



// save sparse_vector dump to disk
//
template<class SV>
int file_save_svector(const SV& sv, const std::string& fname,
                      size_t* sv_blob_size=0, bool use_xor = true)
{
    BM_ASSERT(!fname.empty());
    
    bm::sparse_vector_serial_layout<SV> sv_lay;

    bm::sparse_vector_serializer<SV> sv_serializer;
    sv_serializer.set_xor_ref(use_xor);

    sv_serializer.serialize(sv, sv_lay);
    std::ofstream fout(fname.c_str(), std::ios::binary);
    if (!fout.good())
    {
        return -1;
    }
    const char* buf = (char*)sv_lay.buf();
    fout.write(buf, std::streamsize(sv_lay.size()));
    if (!fout.good())
    {
        return -1;
    }
    
    fout.close();
    
    if (sv_blob_size)
    {
        *sv_blob_size = sv_lay.size();
    }
    return 0;
}

template<class SV>
int file_load_svector(SV& sv, const std::string& fname)
{
    std::vector<unsigned char> buffer;

    // read the input buffer, validate errors
    auto ret = bm::read_dump_file(fname, buffer);
    if (ret != 0)
    {
        return -2;
    }
    if (buffer.size() == 0)
    {
        return -3;
    }
    
    const unsigned char* buf = &buffer[0];
    BM_DECLARE_TEMP_BLOCK(tb)
    auto res = bm::sparse_vector_deserialize(sv, buf, tb);
    if (res != 0)
    {
        return -4;
    }
    return 0;
}


// compare-check if sparse vector is excatly coresponds to vector
//
// returns 0 - if equal
//         1 - no size match
//         2 - element match fails
template<class SV, class V>
int svector_check(const SV& sv, const V& vect)
{
    if (sv.size() != vect.size())
    {
        return 1;
    }
    for (size_t i = 0; i < vect.size(); ++i)
    {
        unsigned v1 = sv[(unsigned)i];
        unsigned v2 = vect[i];
        if (v1 != v2)
            return 2;
    } // for i
    return 0;
}


template<class SV, class BV>
void convert_bv2sv(SV& sv, const BV& bv)
{
    typename SV::back_insert_iterator bit = sv.get_back_inserter();
    typename BV::enumerator en = bv.first();
    for (; en.valid(); ++en)
    {
        auto v = en.value();
        bit = v;
    }
    bit.flush();
}


} // namespace



#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif
