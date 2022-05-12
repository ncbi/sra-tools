#ifndef __DATA_FRAME_HPP_
#define __DATA_FRAME_HPP_

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

#include <bm/bm.h>
#include <bm/bmsparsevec.h>
#include <bm/bmstrsparsevec.h>
#include <bm/bmsparsevec_serial.h>
#include <bm/bmsparsevec_compr.h>
#include <bm/bmintervals.h>
#include <vector>
#include <ostream>
#include <memory>
#include <algorithm>
#include <variant>

using namespace std;

typedef enum {
    eDF_String,
    eDF_Uint16,
    eDF_Uint32,
    eDF_Uint64,
    eDF_Bit
} EDF_ColumnType;

 

/*!
    CDataFrame implements succinct columnar data frame 
    It supports three data types : String, Uint and Bit 
    The data types correspond to BitMagic's vector types:
        str_sparse_vector, sparse_vector<unsigned>, bvector
    
    Template parameter MAX_SIZE defines the max size for string columns

    Constructor's initializer_list describes the number and the types of columns 

    get()
    GetColumns() 
    provide access to the columns

    Column is a union structure with pointer to the corresponding data types
    The pointer provides access to underlying vectors for further manipulations
    The caller is responsible for invoking correct pointer to the corresponding data type


 */

typedef bm::sparse_vector<uint16_t, bm::bvector<> > svector_u16;
typedef bm::sparse_vector<unsigned, bm::bvector<> > svector_u32;
typedef bm::sparse_vector<uint64_t, bm::bvector<> > svector_u64;

typedef svector_u16 u16_t; 
typedef svector_u32 u32_t;
typedef svector_u64 u64_t;
typedef bm::bvector<> bit_t;
typedef typename bm::str_sparse_vector<char,  bm::bvector<>, 32> str_t;

BM_DECLARE_TEMP_BLOCK(TB)

class CDataFrame 
{

public:



    /**
     * Column provides access to the vector data via union of pointers 
     * to the specific data type
     * Caller is responsible for accesing the correct pointer 
     * Usage:
     * df.get<str_t>(0)->set(0, "1"); // access to str_sparse_vector
     * df.get<u32_t>(1)->set(0, 1);  // access to sparse_vector<unsigned>
     * df.get<bit_t>(1)->set(0);      // access to bit vector 
     * 
    */
   
    using Column = std::variant<str_t, u16_t, u32_t, u64_t, bit_t>;
    typedef vector<Column> TColumns;

    /** Default constructor */
    CDataFrame() {
    }
    
    /** Constructor with column initiazlization
     * @param[in] columns initializer list of column types
     *   Usage:
     *   CDataFrame df({eDF_String, eDF_Uint, eDF_Bit});
    */
    CDataFrame(initializer_list<EDF_ColumnType>& columns) 
    {
        CreateColumns(columns);
    }

    CDataFrame(const CDataFrame&) = delete;
    CDataFrame& operator=(const CDataFrame&) = delete;
    virtual ~CDataFrame() = default;

    void CreateColumns(initializer_list<EDF_ColumnType> columns) 
    {
        for (const auto& t : columns) {
            switch (t) {
            case eDF_String: {
                m_Columns.emplace_back().emplace<str_t>();
                break;
            }
            case eDF_Uint16: {
                m_Columns.emplace_back().emplace<u16_t>();
                break;
            }
            case eDF_Uint32: {
                m_Columns.emplace_back().emplace<u32_t>();
                break;
            }
            case eDF_Uint64: {
                m_Columns.emplace_back().emplace<u64_t>();
                break;
            }
            case eDF_Bit: {
                m_Columns.emplace_back().emplace<bit_t>();
                break;
            }
            }
        }
    }

    template<typename T>
    vector<typename T::value_type> gather(const vector<typename T::size_type>& rows, unsigned col_idx, unsigned num_rows) 
    {
        vector<typename T::size_type> tmp_buf;
        tmp_buf.resize(num_rows, 0);
        vector<typename T::value_type> values;
        values.resize(num_rows, 0);
        auto& data = *get<T>(col_idx);
        data.gather(values.data(), rows.data(), tmp_buf.data(), num_rows, bm::BM_UNSORTED);
        return values;
    }

    /** 
     * Access to a column by type and index with no boundary checks
    */
    template<typename T> 
    T& get(unsigned idx) 
    {
        return std::get<T>(m_Columns[idx]);
    }


    /** 
     * Access to all columns 
    */
    const TColumns& GetColumns() const noexcept
    {
        return m_Columns;
    }
    TColumns& GetColumns() noexcept
    {
        return m_Columns;
    }

    /** 
     * Optimize internal data after adding or updating new rows 
    */
    size_t Optimize() {
        size_t total_memory = 0;
        for (auto& col : m_Columns) {
            std::visit([&total_memory, &TB](auto&& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same<str_t, T>::value) {
                    str_t::statistics st;
                    c.optimize(TB, bm::bvector<>::opt_compress, &st);
                    total_memory += st.memory_used;
                } else if constexpr (std::is_same<u16_t, T>::value) {
                    u16_t::statistics st;
                    c.optimize(TB, bm::bvector<>::opt_compress, &st);
                    total_memory += st.memory_used;
                } else if constexpr (std::is_same<u32_t, T>::value) {
                    u32_t::statistics st;
                    c.optimize(TB, bm::bvector<>::opt_compress, &st);
                    total_memory += st.memory_used;
                } else if constexpr (std::is_same<u64_t, T>::value) {
                    u64_t::statistics st;
                    c.optimize(TB, bm::bvector<>::opt_compress, &st);
                    total_memory += st.memory_used;
                } else if constexpr (std::is_same<bit_t, T>::value) {
                    bit_t::statistics st;
                    c.optimize(TB, bm::bvector<>::opt_compress, &st);
                    total_memory += st.memory_used;

                }
            }, col);        
        }
        return total_memory;
    }

protected:
    TColumns m_Columns;
};

#endif /* __DATA_FRAME_HPP_ */

