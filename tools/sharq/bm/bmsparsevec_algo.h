#ifndef BMSPARSEVEC_ALGO__H__INCLUDED__
#define BMSPARSEVEC_ALGO__H__INCLUDED__
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
/*! \file bmsparsevec_algo.h
    \brief Algorithms for bm::sparse_vector
*/

#ifndef BM__H__INCLUDED__
// BitMagic utility headers do not include main "bm.h" declaration 
// #include "bm.h" or "bm64.h" explicitly 
# error missing include (bm.h or bm64.h)
#endif

#include "bmdef.h"
#include "bmsparsevec.h"
#include "bmaggregator.h"
#include "bmbuffer.h"
#include "bmalgo.h"
#include "bmdef.h"

#ifdef _MSC_VER
# pragma warning( disable: 4146 )
#endif



/** \defgroup svalgo Sparse vector algorithms
    Sparse vector algorithms
    \ingroup svector
 */


namespace bm
{


/*!
    \brief Clip dynamic range for signal higher than specified
    
    \param  svect - sparse vector to do clipping
    \param  high_bit - max bit (inclusive) allowed for this signal vector
    
    
    \ingroup svalgo
    \sa dynamic_range_clip_low
*/
template<typename SV>
void dynamic_range_clip_high(SV& svect, unsigned high_bit)
{
    unsigned sv_planes = svect.slices();
    
    BM_ASSERT(sv_planes > high_bit && high_bit > 0);
    
    typename SV::bvector_type bv_acc;
    unsigned i;
    
    // combine all the high bits into accumulator vector
    for (i = high_bit+1; i < sv_planes; ++i)
    {
        if (typename SV::bvector_type* bv = svect.slice(i))
        {
            bv_acc.bit_or(*bv);
            svect.free_slice(i);
        }
    } // for i
    
    // set all bits ON for all low vectors, which happen to be clipped
    for (i = high_bit; true; --i)
    {
        typename SV::bvector_type* bv = svect.get_create_slice(i);
        bv->bit_or(bv_acc);
        if (!i)
            break;
    } // for i
}


/*!
    \brief Clip dynamic range for signal lower than specified (boost)
    
    \param  svect - sparse vector to do clipping
    \param  low_bit - low bit (inclusive) allowed for this signal vector
    
    \ingroup svalgo
    \sa dynamic_range_clip_high 
*/
template<typename SV>
void dynamic_range_clip_low(SV& svect, unsigned low_bit)
{
    if (low_bit == 0) return; // nothing to do
    BM_ASSERT(svect.slices() > low_bit);
    
    unsigned sv_planes = svect.slices();
    typename SV::bvector_type bv_acc1;
    unsigned i;
    
    // combine all the high bits into accumulator vector
    for (i = low_bit+1; i < sv_planes; ++i)
    {
        if (typename SV::bvector_type* bv = svect.slice(i))
            bv_acc1.bit_or(*bv);
    } // for i
    
    // accumulate all vectors below the clipping point
    typename SV::bvector_type bv_acc2;
    typename SV::bvector_type* bv_low_plane = svect.get_create_slice(low_bit);
    
    for (i = low_bit-1; true; --i)
    {
        if (typename SV::bvector_type* bv = svect.slice(i))
        {
            bv_acc2.bit_or(*bv);
            svect.free_slice(i);
            if (i == 0)
                break;
        }
    } // for i
    
    // now we want to set 1 in the clipping low plane based on
    // exclusive or (XOR) between upper and lower parts)
    // as a result high signal (any bits in the upper planes) gets
    // slightly lower value (due to clipping) low signal gets amplified
    // (lower contrast algorithm)
    
    bv_acc1.bit_xor(bv_acc2);
    bv_low_plane->bit_or(bv_acc1);
}

/**
    Find first mismatch (element which is different) between two sparse vectors
    (uses linear scan in bit-vector planes)

    Function works with both NULL and NOT NULL vectors
    NULL means unassigned (uncertainty), so first mismatch NULL is a mismatch
    even if values in vectors can be formally the same (i.e. 0)

    @param sv1 - vector 1
    @param sv2 - vector 2
    @param midx - mismatch index
    @param null_proc - defines if we want to include (not) NULL
                  vector into comparison (bm::use_null) or not.
                  By default search takes NULL vector into account 

    @return true if mismatch found

    @sa sparse_vector_find_mismatch

    \ingroup svalgo
*/
template<typename SV>
bool sparse_vector_find_first_mismatch(const SV& sv1,
                                       const SV& sv2,
                                       typename SV::size_type& midx,
                                       bm::null_support  null_proc = bm::use_null)
{
    typename SV::size_type mismatch = bm::id_max;
    bool found = false;

    unsigned sv_idx = 0;

    unsigned planes1 = (unsigned)sv1.get_bmatrix().rows();
    BM_ASSERT(planes1);

    typename SV::bvector_type_const_ptr bv_null1 = sv1.get_null_bvector();
    typename SV::bvector_type_const_ptr bv_null2 = sv2.get_null_bvector();

    // for RSC vector do NOT compare NULL planes

    if (bm::conditional<SV::is_rsc_support::value>::test())
    {
        //--planes1;
    }
    else // regular sparse vector - may have NULL planes
    {
        if (null_proc == bm::use_null)
        {
            if (bv_null1 && bv_null2) // both (not) NULL vectors present
            {
                bool f = bv_null1->find_first_mismatch(*bv_null2, midx, mismatch);
                if (f && (midx < mismatch)) // better mismatch found
                {
                    found = f; mismatch = midx;
                }
            }
            else // one or both NULL vectors are not present
            {
                if (bv_null1)
                {
                    typename SV::bvector_type bv_tmp; // TODO: get rid of temp bv
                    bv_tmp.resize(sv2.size());
                    bv_tmp.invert(); // turn into true NULL vector

                    // find first NULL value (mismatch)
                    bool f = bv_null1->find_first_mismatch(bv_tmp, midx, mismatch);
                    if (f && (midx < mismatch)) // better mismatch found
                    {
                        found = f; mismatch = midx;
                    }
                }
                if (bv_null2)
                {
                    typename SV::bvector_type bv_tmp; // TODO: get rid of temp bv
                    bv_tmp.resize(sv1.size());
                    bv_tmp.invert();

                    bool f = bv_null2->find_first_mismatch(bv_tmp, midx, mismatch);
                    if (f && (midx < mismatch)) // better mismatch found
                    {
                        found = f; mismatch = midx;
                    }
                }
            }
        } // null_proc
    }

    for (unsigned i = 0; mismatch && (i < planes1); ++i)
    {
        typename SV::bvector_type_const_ptr bv1 = sv1.get_slice(i);
        if (bv1 == bv_null1)
            bv1 = 0;
        typename SV::bvector_type_const_ptr bv2 = sv2.get_slice(i);
        if (bv2 == bv_null2)
            bv2 = 0;

        if (!bv1)
        {
            if (!bv2)
                continue;
            bool f = bv2->find(midx);
            if (f && (midx < mismatch))
            {
                found = f; sv_idx = 2; mismatch = midx;
            }
            continue;
        }
        if (!bv2)
        {
            BM_ASSERT(bv1);
            bool f = bv1->find(midx);
            if (f && (midx < mismatch))
            {
                found = f; sv_idx = 1; mismatch = midx;
            }
            continue;
        }
        // both planes are not NULL
        //
        bool f = bv1->find_first_mismatch(*bv2, midx, mismatch);
        if (f && (midx < mismatch)) // better mismatch found
        {
            found = f; mismatch = midx;
            // which vector has '1' at mismatch position?
            if (bm::conditional<SV::is_rsc_support::value>::test())
                sv_idx = (bv1->test(mismatch)) ? 1 : 2;
        }

    } // for i

    // RSC address translation here
    //
    if (bm::conditional<SV::is_rsc_support::value>::test())
    {
        if (found) // RSC address translation
        {
            BM_ASSERT(sv1.is_compressed());
            BM_ASSERT(sv2.is_compressed());

            switch (sv_idx)
            {
            case 1:
                found = sv1.find_rank(midx + 1, mismatch);
                break;
            case 2:
                found = sv2.find_rank(midx + 1, mismatch);
                break;
            default: // unknown, try both
                BM_ASSERT(0);
            }
            BM_ASSERT(found);
            midx = mismatch;
        }
        if (null_proc == bm::use_null)
        {
            // no need for address translation in this case
            bool f = bv_null1->find_first_mismatch(*bv_null2, midx, mismatch);
            if (f && (midx < mismatch)) // better mismatch found
            {
                found = f; mismatch = midx;
            }
        }

/*
        else // search for mismatch in the NOT NULL vectors
        {
            if (null_proc == bm::use_null)
            {
                // no need for address translation in this case
                typename SV::bvector_type_const_ptr bv_null1 = sv1.get_null_bvector();
                typename SV::bvector_type_const_ptr bv_null2 = sv2.get_null_bvector();
                found = bv_null1->find_first_mismatch(*bv_null2, mismatch);
            }
        }
*/
    }

    midx = mismatch; // minimal mismatch
    return found;
}

/**
    Find mismatch vector, indicating positions of mismatch between two sparse vectors
    (uses linear scan in bit-vector planes)

    Function works with both NULL and NOT NULL vectors

    @param bv - [out] - bit-ector with mismatch positions indicated as 1s
    @param sv1 - vector 1
    @param sv2 - vector 2
    @param null_proc - rules of processing for (not) NULL plane
      bm::no_null - NULLs from both vectors are treated as uncertainty
                    and NOT included into final result
      bm::use_null - difference in NULLs accounted into the result

    @sa sparse_vector_find_first_mismatch

    \ingroup svalgo
*/
template<typename SV1, typename SV2>
void sparse_vector_find_mismatch(typename SV1::bvector_type& bv,
                                 const SV1&                  sv1,
                                 const SV2&                  sv2,
                                 bm::null_support            null_proc)
{
    typedef typename SV1::bvector_type       bvector_type;
    typedef typename bvector_type::allocator_type        allocator_type;
    typedef typename allocator_type::allocator_pool_type allocator_pool_type;

    allocator_pool_type  pool; // local pool for blocks
    typename bvector_type::mem_pool_guard mp_guard_bv;
    mp_guard_bv.assign_if_not_set(pool, bv);


    if (bm::conditional<SV1::is_rsc_support::value>::test())
    {
        BM_ASSERT(0); // TODO: fixme
    }
    if (bm::conditional<SV2::is_rsc_support::value>::test())
    {
        BM_ASSERT(0); // TODO: fixme
    }

    bv.clear();

    unsigned planes = sv1.effective_slices();; // slices();
    if (planes < sv2.slices())
        planes = sv2.slices();

    for (unsigned i = 0; i < planes; ++i)
    {
        typename SV1::bvector_type_const_ptr bv1 = sv1.get_slice(i);
        typename SV2::bvector_type_const_ptr bv2 = sv2.get_slice(i);

        if (!bv1)
        {
            if (!bv2)
                continue;
            bv |= *bv2;
            continue;
        }
        if (!bv2)
        {
            BM_ASSERT(bv1);
            bv |= *bv1;
            continue;
        }

        // both planes are not NULL, compute XOR diff
        //
        bvector_type bv_xor;
        typename bvector_type::mem_pool_guard mp_guard;
        mp_guard.assign_if_not_set(pool, bv_xor);

        bv_xor.bit_xor(*bv1, *bv2, SV1::bvector_type::opt_none);
        bv |= bv_xor;

    } // for i

    // size mismatch check
    {
        typename SV1::size_type sz1 = sv1.size();
        typename SV2::size_type sz2 = sv2.size();
        if (sz1 != sz2)
        {
            if (sz1 < sz2)
            {
            }
            else
            {
                bm::xor_swap(sz1, sz2);
            }
            bv.set_range(sz1, sz2-1);
        }
    }

    // NULL processings
    //
    typename SV1::bvector_type_const_ptr bv_null1 = sv1.get_null_bvector();
    typename SV2::bvector_type_const_ptr bv_null2 = sv2.get_null_bvector();

    switch (null_proc)
    {
    case bm::no_null:
        // NULL correction to exclude all NULL (unknown) values from the result set
        //  (AND with NOT NULL vector)
        if (bv_null1 && bv_null2)
        {
            bvector_type bv_or;
            typename bvector_type::mem_pool_guard mp_guard;
            mp_guard.assign_if_not_set(pool, bv_or);

            bv_or.bit_or(*bv_null1, *bv_null2, bvector_type::opt_none);
            bv &= bv_or;
        }
        else
        {
            if (bv_null1)
                bv &= *bv_null1;
            if (bv_null2)
                bv &= *bv_null2;
        }
    break;
    case bm::use_null:
        if (bv_null1 && bv_null2)
        {
            bvector_type bv_xor;
            typename bvector_type::mem_pool_guard mp_guard;
            mp_guard.assign_if_not_set(pool, bv_xor);

            bv_xor.bit_xor(*bv_null1, *bv_null2, bvector_type::opt_none);
            bv |= bv_xor;
        }
        else
        {
            bvector_type bv_null;
            typename bvector_type::mem_pool_guard mp_guard;
            mp_guard.assign_if_not_set(pool, bv_null);
            if (bv_null1)
            {
                bv_null = *bv_null1;
                bv_null.resize(sv1.size());
            }
            if (bv_null2)
            {
                bv_null = *bv_null2;
                bv_null.resize(sv2.size());
            }
            bv_null.invert();
            bv |= bv_null;
        }
    break;
    default:
        BM_ASSERT(0);
    }
}


/**
    \brief algorithms for sparse_vector scan/search
 
    Scanner uses properties of bit-vector planes to answer questions
    like "find all sparse vector elements equivalent to XYZ".

    Class uses fast algorithms based on properties of bit-planes.
    This is NOT a brute force, direct scan, scanner uses search space pruning and cache optimizations
    to run the search.
 
    @ingroup svalgo
    @ingroup setalgo
*/
template<typename SV>
class sparse_vector_scanner
{
public:
    typedef typename SV::bvector_type       bvector_type;
    typedef const bvector_type*             bvector_type_const_ptr;
    typedef bvector_type*                   bvector_type_ptr;
    typedef typename SV::value_type         value_type;
    typedef typename SV::size_type          size_type;
    
    typedef typename bvector_type::allocator_type        allocator_type;
    typedef typename allocator_type::allocator_pool_type allocator_pool_type;

    typedef bm::aggregator<bvector_type>    aggregator_type;
    typedef
    bm::heap_vector<value_type, typename bvector_type::allocator_type, true>
                                                    remap_vector_type;
    typedef
    bm::heap_vector<bvector_type*, allocator_type, true> bvect_vector_type;
    typedef
    typename aggregator_type::bv_count_vector_type bv_count_vector_type;

    /**
       Scanner options to control execution
     */
    typedef
    typename aggregator_type::run_options run_options;

public:


    /**
        Pipeline to run multiple searches against a particular SV faster using cache optimizations.
        USAGE:
        - setup the pipeline (add searches)
        - run all searches at once
        - get the search results (in the order it was added)
     */
    template<class Opt = bm::agg_run_options<> >
    class pipeline
    {
    public:
        typedef
        typename aggregator_type::template pipeline<Opt> aggregator_pipeline_type;
    public:
        pipeline(const SV& sv)
        : sv_(sv)
        {}

        /// Set pipeline run options
        run_options& options() BMNOEXCEPT
            { return agg_pipe_.options(); }

        /// Get pipeline run options
        const run_options& get_options() const BMNOEXCEPT
            { return agg_pipe_.options(); }

        /**
            Attach OR (aggregator vector).
            Pipeline results all will be OR-ed (UNION) into the OR target vector
           @param bv_or - OR target vector
         */
        void set_or_target(bvector_type* bv_or) BMNOEXCEPT
            { agg_pipe_.set_or_target(bv_or); }

        /*! @name pipeline fill-in methods */
        //@{

        void add(const typename SV::value_type* str);

        /** Prepare pipeline for the execution (resize and init internal structures)
            Once complete, you cannot add() to it.
        */
        void complete() { agg_pipe_.complete(); }
        
        bool is_complete() const BMNOEXCEPT
            { return agg_pipe_.is_complete(); }
        //@}

        /** Return results vector used for pipeline execution */
        bvect_vector_type& get_bv_res_vector()  BMNOEXCEPT
            { return agg_pipe_.get_bv_res_vector(); }
        /** Return results vector count used for pipeline execution */

        bv_count_vector_type& get_bv_count_vector()  BMNOEXCEPT
            { return agg_pipe_.get_bv_count_vector(); }


        /**
            get aggregator pipeline (access to compute internals)
            @internal
         */
         aggregator_pipeline_type& get_aggregator_pipe() BMNOEXCEPT
            { return agg_pipe_; }

    protected:
        friend class sparse_vector_scanner;

        pipeline(const pipeline&) = delete;
        pipeline& operator=(const pipeline&) = delete;
    protected:
        const SV&                    sv_; ///< target sparse vector ref
        aggregator_pipeline_type     agg_pipe_; ///< traget aggregator pipeline
        remap_vector_type            remap_value_vect_; ///< remap buffer
    };

public:
    sparse_vector_scanner();

    /**
        \brief bind sparse vector for all searches
        
        \param sv - input sparse vector to bind for searches
        \param sorted - source index is sorted, build index for binary search
    */
    void bind(const SV& sv, bool sorted);

    /**
        \brief reset sparse vector binding
    */
    void reset_binding() BMNOEXCEPT;


    /*! @name Find in scalar vector */
    //@{

    /**
        \brief find all sparse vector elements EQ to search value

        Find all sparse vector elements equivalent to specified value

        \param sv - input sparse vector
        \param value - value to search for
        \param bv_out - search result bit-vector (search result masks 1 elements)
    */
    void find_eq(const SV&                  sv,
                 typename SV::value_type    value,
                 typename SV::bvector_type& bv_out);

    /**
        \brief find first sparse vector element

        Find all sparse vector elements equivalent to specified value.
        Works well if sperse vector represents unordered set

        \param sv - input sparse vector
        \param value - value to search for
        \param pos - output found sparse vector element index
     
        \return true if found
    */
    bool find_eq(const SV&                  sv,
                 typename SV::value_type    value,
                 typename SV::size_type&    pos);
    /**
        \brief binary search for position in the sorted sparse vector

        Method assumes the sparse array is sorted, if value is found pos returns its index,
        if not found, pos would contain index where it could be inserted to maintain the order

        \param sv - input sparse vector
        \param val - value to search for
        \param pos - output sparse vector element index (actual index if found or insertion
                    point if not found)

        \return true if value found
    */
    bool bfind(const SV&                      sv,
               const typename SV::value_type  val,
               typename SV::size_type&        pos);

    //@}


    // --------------------------------------------------
    //
    /*! @name Find in bit-transposed string vector */
    //@{


    /**
        \brief find sparse vector elements (string)
        @param sv  - sparse vector of strings to search
        @param str - string to search for
        @param bv_out - search result bit-vector (search result masks 1 elements)
    */
    bool find_eq_str(const SV&                      sv,
                     const typename SV::value_type* str,
                     typename SV::bvector_type& bv_out);

    /**
        \brief find sparse vector elementa (string) in the attached SV
        @param str - string to search for
        @param bv_out - search result bit-vector (search result masks 1 elements)
    */
    bool find_eq_str(const typename SV::value_type* str,
                     typename SV::bvector_type& bv_out);

    /**
        \brief find first sparse vector element (string)
        @param sv  - sparse vector of strings to search
        @param str - string to search for
        @param pos - [out] index of the first found
    */
    bool find_eq_str(const SV&                      sv,
                     const typename SV::value_type* str,
                     typename SV::size_type&        pos);

    /**
        \brief binary find first sparse vector element (string)
        Sparse vector must be attached (bind())
        @param str - string to search for
        @param pos - [out] index of the first found
        @sa bind
    */
    bool find_eq_str(const typename SV::value_type* str,
                     typename SV::size_type&        pos);

    /**
        \brief find sparse vector elements  with a given prefix (string)
        @param sv  - sparse vector of strings to search
        @param str - string prefix to search for
                     "123" is a prefix for "1234567"
        @param bv_out - search result bit-vector (search result masks 1 elements)
    */
    bool find_eq_str_prefix(const SV&               sv,
                     const typename SV::value_type* str,
                     typename SV::bvector_type&     bv_out);

    /**
        \brief find sparse vector elements using search pipeline
        @param pipe  - pipeline to run
     */
    template<class TPipe>
    void find_eq_str(TPipe& pipe);

    /**
        \brief binary find first sparse vector element (string)     
        Sparse vector must be sorted.
    */
    bool bfind_eq_str(const SV&                      sv,
                      const typename SV::value_type* str,
                      typename SV::size_type&        pos);

    /**
        \brief lower bound search for an array position
     
        Method assumes the sparse array is sorted
     
        \param sv - input sparse vector
        \param str - value to search for
        \param pos - output sparse vector element index

        \return true if value found
    */
    bool lower_bound_str(const SV&                      sv,
                         const typename SV::value_type* str,
                         typename SV::size_type&        pos);

    /**
        \brief binary find first sparse vector element (string)
        Sparse vector must be sorted and attached
        @sa bind
    */
    bool bfind_eq_str(const typename SV::value_type* str,
                      typename SV::size_type&        pos);

    //@}

    /**
        \brief find all sparse vector elements EQ to 0
        \param sv - input sparse vector
        \param bv_out - output bit-vector (search result masks 1 elements)
    */
    void find_zero(const SV&                  sv,
                   typename SV::bvector_type& bv_out);

    /*!
        \brief Find non-zero elements
        Output vector is computed as a logical OR (join) of all planes

        \param  sv - input sparse vector
        \param  bv_out - output bit-bector of non-zero elements
    */
    void find_nonzero(const SV& sv, typename SV::bvector_type& bv_out);

    /**
        \brief invert search result ("EQ" to "not EQ")

        \param  sv - input sparse vector
        \param  bv_out - output bit-bector of non-zero elements
    */
    void invert(const SV& sv, typename SV::bvector_type& bv_out);

    /**
        \brief find all values A IN (C, D, E, F)
        \param  sv - input sparse vector
        \param  start - start iterator (set to search) 
        \param  end   - end iterator (set to search)
        \param  bv_out - output bit-bector of non-zero elements
     */
    template<typename It>
    void find_eq(const SV&  sv,
                 It    start, 
                 It    end,
                 typename SV::bvector_type& bv_out)
    {
        typename bvector_type::mem_pool_guard mp_guard;
        mp_guard.assign_if_not_set(pool_, bv_out); // set algorithm-local memory pool to avoid heap contention

        bvector_type bv1;
        typename bvector_type::mem_pool_guard mp_guard1(pool_, bv1);
        bool any_zero = false;
        for (; start < end; ++start)
        {
            value_type v = *start;
            any_zero |= (v == 0);
            bool found = find_eq_with_nulls(sv, v, bv1);
            if (found)
                bv_out.bit_or(bv1);
            else
            {
                BM_ASSERT(!bv1.any());
            }
        } // for
        if (any_zero)
            correct_nulls(sv, bv_out);
    }


    /// For testing purposes only
    ///
    /// @internal
    void find_eq_with_nulls_horizontal(const SV&   sv,
                 typename SV::value_type           value,
                 typename SV::bvector_type&        bv_out);

    /** Exclude possible NULL values from the result vector
        \param  sv - input sparse vector
        \param  bv_out - output bit-bector of non-zero elements
    */
    void correct_nulls(const SV&   sv, typename SV::bvector_type& bv_out);

    /// Return allocator pool for blocks
    ///  (Can be used to improve performance of repeated searches with the same scanner)
    ///
    allocator_pool_type& get_bvector_alloc_pool() BMNOEXCEPT { return pool_; }
    
protected:

    /// Remap input value into SV char encodings
    static
    bool remap_tosv(remap_vector_type& remap_vect_target,
                    const typename SV::value_type* str,
                    const SV& sv);

    /// set search boundaries (hint for the aggregator)
    void set_search_range(size_type from, size_type to);
    
    /// reset (disable) search range
    void reset_search_range();

    /// find value (may include NULL indexes)
    bool find_eq_with_nulls(const SV&   sv,
                            typename SV::value_type         value,
                            typename SV::bvector_type&      bv_out,
                            typename SV::size_type          search_limit = 0);

    /// find first value (may include NULL indexes)
    bool find_first_eq(const SV&   sv,
                       typename SV::value_type         value,
                       size_type&                      idx);
    
    /// find first string value (may include NULL indexes)
    bool find_first_eq(const SV&                       sv,
                       const typename SV::value_type*  str,
                       size_type&                      idx,
                       bool                            remaped);

    /// find EQ str / prefix impl
    bool find_eq_str_impl(const SV&                      sv,
                     const typename SV::value_type* str,
                     typename SV::bvector_type& bv_out,
                     bool prefix_sub);

    /// Prepare aggregator for AND-SUB (EQ) search
    bool prepare_and_sub_aggregator(const SV&   sv,
                                    typename SV::value_type   value);

    /// Prepare aggregator for AND-SUB (EQ) search (string)
    bool prepare_and_sub_aggregator(const SV&  sv,
                                    const typename SV::value_type*  str,
                                    unsigned octet_start,
                                    bool prefix_sub);

    /// Rank-Select decompression for RSC vectors
    void decompress(const SV&   sv, typename SV::bvector_type& bv_out);

    /// compare sv[idx] with input str
    int compare_str(const SV& sv, size_type idx, const value_type* str);

    /// compare sv[idx] with input value
    int compare(const SV& sv, size_type idx, const value_type val) BMNOEXCEPT;


protected:
    sparse_vector_scanner(const sparse_vector_scanner&) = delete;
    void operator=(const sparse_vector_scanner&) = delete;

protected:
    enum search_algo_params
    {
        linear_cutoff1 = 16,
        linear_cutoff2 = 128
    };

    typedef bm::dynamic_heap_matrix<value_type, allocator_type> heap_matrix_type;
    typedef bm::dynamic_heap_matrix<value_type, allocator_type> matrix_search_buf_type;

    typedef
    bm::heap_vector<bm::id64_t, typename bvector_type::allocator_type, true>
                                            mask_vector_type;

protected:

    /// Addd char  to aggregator (AND-SUB)
    template<typename AGG>
    static
    void add_agg_char(AGG& agg,
                      const SV& sv,
                      int octet_idx, bm::id64_t planes_mask,
                      unsigned value)
    {
        unsigned char bits[64];
        unsigned short bit_count_v = bm::bitscan(value, bits);
        for (unsigned i = 0; i < bit_count_v; ++i)
        {
            unsigned bit_idx = bits[i];
            BM_ASSERT(value & (value_type(1) << bit_idx));
            unsigned plane_idx = (unsigned(octet_idx) * 8) + bit_idx;
            const bvector_type* bv = sv.get_slice(plane_idx);
            agg.add(bv, 0); // add to the AND group
        } // for i

        unsigned vinv = unsigned(value);
        if constexpr (sizeof(value_type) == 1)
            vinv ^= 0xFF;
        else // 2-byte char
        {
            BM_ASSERT(sizeof(value_type) == 2);
            vinv ^= 0xFFFF;
        }
        // exclude the octet bits which are all not set (no vectors)
        vinv &= unsigned(planes_mask);
        for (unsigned octet_plane = (unsigned(octet_idx) * 8); vinv; vinv &= vinv-1)
        {
            bm::id_t t = vinv & -vinv;
            unsigned bit_idx = bm::word_bitcount(t - 1);
            unsigned plane_idx = octet_plane + bit_idx;
            const bvector_type* bv = sv.get_slice(plane_idx);
            BM_ASSERT(bv);
            agg.add(bv, 1); // add to SUB group
        } // for
    }

private:
    allocator_pool_type                pool_;
    bvector_type                       bv_tmp_;
    aggregator_type                    agg_;
    bm::rank_compressor<bvector_type>  rank_compr_;
    
    size_type                          mask_from_;
    size_type                          mask_to_;
    bool                               mask_set_;
    
    const SV*                          bound_sv_;
    heap_matrix_type                   block0_elements_cache_; ///< cache for elements[0] of each block
    heap_matrix_type                   block3_elements_cache_; ///< cache for elements[16384x] of each block
    size_type                          effective_str_max_;
    
    remap_vector_type                  remap_value_vect_; ///< remap buffer
    /// masks of allocated bit-planes (1 - means there is a bit-plane)
    mask_vector_type                   vector_plane_masks_;
    matrix_search_buf_type             hmatr_; ///< heap matrix for string search linear stage
};


/*!
    \brief Integer set to set transformation (functional image in groups theory)
    https://en.wikipedia.org/wiki/Image_(mathematics)
 
    Input sets gets translated through the function, which is defined as
    "one to one (or NULL)" binary relation object (sparse_vector).
    Also works for M:1 relationships.
 
    \ingroup svalgo
    \ingroup setalgo
*/
template<typename SV>
class set2set_11_transform
{
public:
    typedef typename SV::bvector_type       bvector_type;
    typedef typename SV::value_type         value_type;
    typedef typename SV::size_type          size_type;
    typedef typename bvector_type::allocator_type::allocator_pool_type allocator_pool_type;
public:

    set2set_11_transform();
    ~set2set_11_transform();


    /**  Get read access to zero-elements vector
         Zero vector gets populated after attach_sv() is called
         or as a side-effect of remap() with immediate sv param
    */
    const bvector_type& get_bv_zero() const { return bv_zero_; }

    /** Perform remapping (Image function) (based on attached translation table)

    \param bv_in  - input set, defined as a bit-vector
    \param bv_out - output set as a bit-vector

    @sa attach_sv
    */
    void remap(const bvector_type&        bv_in,
               bvector_type&              bv_out);

    /** Perform remapping (Image function)
   
     \param bv_in  - input set, defined as a bit-vector
     \param sv_brel   - binary relation (translation table) sparse vector
     \param bv_out - output set as a bit-vector
    */
    void remap(const bvector_type&        bv_in,
               const    SV&               sv_brel,
               bvector_type&              bv_out);
    
    /** Remap single set element
   
     \param id_from  - input value
     \param sv_brel  - translation table sparse vector
     \param id_to    - out value
     
     \return - true if value was found and remapped
    */
    bool remap(size_type id_from, const SV& sv_brel, size_type& id_to);


    /** Run remap transformation
   
     \param bv_in  - input set, defined as a bit-vector
     \param sv_brel   - translation table sparse vector
     \param bv_out - output set as a bit-vector
     
     @sa remap
    */
    void run(const bvector_type&        bv_in,
             const    SV&               sv_brel,
             bvector_type&              bv_out)
    {
        remap(bv_in, sv_brel, bv_out);
    }
    
    /** Attach a translation table vector for remapping (Image function)

        \param sv_brel   - binary relation sparse vector pointer
                          (pass NULL to detach)
        \param compute_stats - flag to perform computation of some statistics
                               later used in remapping. This only make sense
                               for series of remappings on the same translation
                               vector.
        @sa remap
    */
    void attach_sv(const SV* sv_brel, bool compute_stats = false);

    
protected:
    void one_pass_run(const bvector_type&        bv_in,
                      const    SV&               sv_brel,
                      bvector_type&              bv_out);
    
    /// @internal
    template<unsigned BSIZE>
    struct gather_buffer
    {
        size_type   BM_VECT_ALIGN gather_idx_[BSIZE] BM_VECT_ALIGN_ATTR;
        value_type  BM_VECT_ALIGN buffer_[BSIZE] BM_VECT_ALIGN_ATTR;
    };
    
    enum gather_window_size
    {
        sv_g_size = 1024 * 8
    };
    typedef gather_buffer<sv_g_size>  gather_buffer_type;
    

protected:
    set2set_11_transform(const set2set_11_transform&) = delete;
    void operator=(const set2set_11_transform&) = delete;
    
protected:
    const SV*              sv_ptr_;    ///< current translation table vector
    gather_buffer_type*    gb_;        ///< intermediate buffers
    bvector_type           bv_product_;///< temp vector
    
    bool                   have_stats_; ///< flag of statistics presense
    bvector_type           bv_zero_;   ///< bit-vector for zero elements
    
    allocator_pool_type    pool_;
};



//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

template<typename SV>
set2set_11_transform<SV>::set2set_11_transform()
: sv_ptr_(0), gb_(0), have_stats_(false)
{
    // set2set_11_transform for signed int is not implemented yet
    static_assert(std::is_unsigned<value_type>::value,
                  "BM: unsigned sparse vector is required for transform");
    gb_ = (gather_buffer_type*)::malloc(sizeof(gather_buffer_type));
    if (!gb_)
    {
        SV::throw_bad_alloc();
    }
}

//----------------------------------------------------------------------------

template<typename SV>
set2set_11_transform<SV>::~set2set_11_transform()
{
    if (gb_)
        ::free(gb_);
}


//----------------------------------------------------------------------------

template<typename SV>
void set2set_11_transform<SV>::attach_sv(const SV*  sv_brel, bool compute_stats)
{
    sv_ptr_ = sv_brel;
    if (!sv_ptr_)
    {
        have_stats_ = false;
    }
    else
    {
        if (sv_brel->empty() || !compute_stats)
            return; // nothing to do
        const bvector_type* bv_non_null = sv_brel->get_null_bvector();
        if (bv_non_null)
            return; // already have 0 elements vector
        

        typename bvector_type::mem_pool_guard mp_g_z;
        mp_g_z.assign_if_not_set(pool_, bv_zero_);

        bm::sparse_vector_scanner<SV> scanner;
        scanner.find_zero(*sv_brel, bv_zero_);
        have_stats_ = true;
    }
}

//----------------------------------------------------------------------------

template<typename SV>
bool set2set_11_transform<SV>::remap(size_type  id_from,
                                     const SV&  sv_brel,
                                     size_type& id_to)
{
    if (sv_brel.empty())
        return false; // nothing to do

    const bvector_type* bv_non_null = sv_brel.get_null_bvector();
    if (bv_non_null)
    {
        if (!bv_non_null->test(id_from))
            return false;
    }
    size_type idx = sv_brel.translate_address(id_from);
    id_to = sv_brel.get(idx);
    return true;
}

//----------------------------------------------------------------------------

template<typename SV>
void set2set_11_transform<SV>::remap(const bvector_type&        bv_in,
                                     const    SV&               sv_brel,
                                     bvector_type&              bv_out)
{
    typename bvector_type::mem_pool_guard mp_g_out, mp_g_p, mp_g_z;
    mp_g_out.assign_if_not_set(pool_, bv_out);
    mp_g_p.assign_if_not_set(pool_, bv_product_);
    mp_g_z.assign_if_not_set(pool_, bv_zero_);

    attach_sv(&sv_brel);
    
    remap(bv_in, bv_out);

    attach_sv(0); // detach translation table
}

template<typename SV>
void set2set_11_transform<SV>::remap(const bvector_type&        bv_in,
                                     bvector_type&              bv_out)
{
    BM_ASSERT(sv_ptr_);

    bv_out.clear();

    if (sv_ptr_ == 0 || sv_ptr_->empty())
        return; // nothing to do

    bv_out.init(); // just in case to "fast set" later

    typename bvector_type::mem_pool_guard mp_g_out, mp_g_p, mp_g_z;
    mp_g_out.assign_if_not_set(pool_, bv_out);
    mp_g_p.assign_if_not_set(pool_, bv_product_);
    mp_g_z.assign_if_not_set(pool_, bv_zero_);


    const bvector_type* enum_bv;

    const bvector_type * bv_non_null = sv_ptr_->get_null_bvector();
    if (bv_non_null)
    {
        // TODO: optimize with 2-way ops
        bv_product_ = bv_in;
        bv_product_.bit_and(*bv_non_null);
        enum_bv = &bv_product_;
    }
    else // non-NULL vector is not available
    {
        enum_bv = &bv_in;
        // if we have any elements mapping into "0" on the other end
        // we map it once (chances are there are many duplicates)
        //
        
        if (have_stats_) // pre-attached translation statistics
        {
            bv_product_ = bv_in;
            size_type cnt1 = bv_product_.count();
            bv_product_.bit_sub(bv_zero_);
            size_type cnt2 = bv_product_.count();
            
            BM_ASSERT(cnt2 <= cnt1);
            
            if (cnt1 != cnt2) // mapping included 0 elements
                bv_out.set_bit_no_check(0);
            
            enum_bv = &bv_product_;
        }
    }

    

    size_type buf_cnt, nb_old, nb;
    buf_cnt = nb_old = 0;
    
    typename bvector_type::enumerator en(enum_bv->first());
    for (; en.valid(); ++en)
    {
        typename SV::size_type idx = *en;
        idx = sv_ptr_->translate_address(idx);
        
        nb = unsigned(idx >> bm::set_block_shift);
        if (nb != nb_old) // new blocks
        {
            if (buf_cnt)
            {
                sv_ptr_->gather(&gb_->buffer_[0], &gb_->gather_idx_[0], buf_cnt, BM_SORTED_UNIFORM);
                bv_out.set(&gb_->buffer_[0], buf_cnt, BM_SORTED);
                buf_cnt = 0;
            }
            nb_old = nb;
            gb_->gather_idx_[buf_cnt++] = idx;
        }
        else
        {
            gb_->gather_idx_[buf_cnt++] = idx;
        }
        
        if (buf_cnt == sv_g_size)
        {
            sv_ptr_->gather(&gb_->buffer_[0], &gb_->gather_idx_[0], buf_cnt, BM_SORTED_UNIFORM);
            bv_out.set(&gb_->buffer_[0], buf_cnt, bm::BM_SORTED);
            buf_cnt = 0;
        }
    } // for en
    if (buf_cnt)
    {
        sv_ptr_->gather(&gb_->buffer_[0], &gb_->gather_idx_[0], buf_cnt, BM_SORTED_UNIFORM);
        bv_out.set(&gb_->buffer_[0], buf_cnt, bm::BM_SORTED);
    }
}


//----------------------------------------------------------------------------

template<typename SV>
void set2set_11_transform<SV>::one_pass_run(const bvector_type&        bv_in,
                                            const    SV&               sv_brel,
                                            bvector_type&              bv_out)
{
    if (sv_brel.empty())
        return; // nothing to do

    bv_out.init();

    typename SV::const_iterator it = sv_brel.begin();
    for (; it.valid(); ++it)
    {
        typename SV::value_type t_id = *it;
        size_type idx = it.pos();
        if (bv_in.test(idx))
        {
            bv_out.set_bit_no_check(t_id);
        }
    } // for
}


//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

template<typename SV>
sparse_vector_scanner<SV>::sparse_vector_scanner()
{
    mask_set_ = false;
    mask_from_ = mask_to_ = bm::id_max;

    bound_sv_ = 0;
    effective_str_max_ = 0;
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::bind(const SV&  sv, bool sorted)
{
    (void)sorted; // MSVC warning over if constexpr variable "not-referenced"
    bound_sv_ = &sv;

    if constexpr (SV::is_str()) // bindings for the string sparse vector
    {
        effective_str_max_ = sv.effective_vector_max();
        if (sorted)
        {
            size_type sv_sz = sv.size();
            BM_ASSERT(sv_sz);
            size_type total_nb = sv_sz / bm::gap_max_bits + 1;

            block0_elements_cache_.resize(total_nb, effective_str_max_+1);
            block0_elements_cache_.set_zero();

            block3_elements_cache_.resize(total_nb * 3, effective_str_max_+1);
            block3_elements_cache_.set_zero();

            // fill in elements cache
            for (size_type i = 0; i < sv_sz; i+= bm::gap_max_bits)
            {
                size_type nb = (i >> bm::set_block_shift);
                value_type* s0 = block0_elements_cache_.row(nb);
                sv.get(i, s0, size_type(block0_elements_cache_.cols()));

                for (size_type k = 0; k < 3; ++k)
                {
                    value_type* s1 = block3_elements_cache_.row(nb * 3 + k);
                    size_type idx = i + (k+1) * bm::sub_block3_size;
                    sv.get(idx, s1, size_type(block3_elements_cache_.cols()));
                } // for k
            } // for i
        }
        // pre-calculate vector plane masks
        //
        vector_plane_masks_.resize(effective_str_max_);
        for (unsigned i = 0; i < effective_str_max_; ++i)
        {
            vector_plane_masks_[i] = sv.get_slice_mask(i);
        } // for i
    }
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::reset_binding() BMNOEXCEPT
{
    bound_sv_ = 0;
    effective_str_max_ = 0;
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::find_zero(const SV&                  sv,
                                          typename SV::bvector_type& bv_out)
{
    if (sv.size() == 0)
    {
        bv_out.clear();
        return;
    }
    find_nonzero(sv, bv_out);
    if (sv.is_compressed())
    {
        bv_out.invert();
        bv_out.set_range(sv.effective_size(), bm::id_max - 1, false);
        decompress(sv, bv_out);
    }
    else
    {
        invert(sv, bv_out);
    }
    correct_nulls(sv, bv_out);
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::invert(const SV& sv, typename SV::bvector_type& bv_out)
{
    if (sv.size() == 0)
    {
        bv_out.clear();
        return;
    }
    // TODO: find a better algorithm (NAND?)
    bv_out.invert();
    const bvector_type* bv_null = sv.get_null_bvector();
    if (bv_null) // correct result to only use not NULL elements
        bv_out &= *bv_null;
    else
    {
        // TODO: use the shorter range to clear the tail
        bv_out.set_range(sv.size(), bm::id_max - 1, false);
    }
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::correct_nulls(const SV&   sv,
                           typename SV::bvector_type& bv_out)
{
    const bvector_type* bv_null = sv.get_null_bvector();
    if (bv_null) // correct result to only use not NULL elements
        bv_out.bit_and(*bv_null);
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq_with_nulls(const SV&    sv,
                                    typename SV::value_type     value,
                                    typename SV::bvector_type&  bv_out,
                                    typename SV::size_type      search_limit)
{
    if (sv.empty())
        return false; // nothing to do

    if (!value)
    {
        find_zero(sv, bv_out);
        return bv_out.any();
    }
    agg_.reset();
    
    bool found = prepare_and_sub_aggregator(sv, value);
    if (!found)
    {
        bv_out.clear();
        return found;
    }

    bool any = (search_limit == 1);
    found = agg_.combine_and_sub(bv_out, any);
    agg_.reset();
    return found;
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_first_eq(const SV&   sv,
                               typename SV::value_type    value,
                               size_type&                 idx)
{
    if (sv.empty())
        return false; // nothing to do
    
    BM_ASSERT(value); // cannot handle zero values
    if (!value)
        return false;

    agg_.reset();
    bool found = prepare_and_sub_aggregator(sv, value);
    if (!found)
        return found;
    found = agg_.find_first_and_sub(idx);
    agg_.reset();
    return found;
}


//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_first_eq(
                                const SV&                       sv,
                                const typename SV::value_type*  str,
                                size_type&                      idx,
                                bool                            remaped)
{
    if (sv.empty())
        return false; // nothing to do
    BM_ASSERT(*str);

    if (!*str)
        return false;

    agg_.reset();
    unsigned common_prefix_len = 0;
    if (mask_set_)
    {
        agg_.set_range_hint(mask_from_, mask_to_);
        common_prefix_len = sv.common_prefix_length(mask_from_, mask_to_);
    }
    
    if (remaped)
    {
        str = remap_value_vect_.data();
    }
    else
    {
        if (sv.is_remap() && (str != remap_value_vect_.data()))
        {
            bool r = sv.remap_tosv(
                remap_value_vect_.data(), sv.effective_max_str(), str);
            if (!r)
                return r;
            str = remap_value_vect_.data();
        }
    }
    
    bool found = prepare_and_sub_aggregator(sv, str, common_prefix_len, true);
    if (!found)
        return found;
    
    found = agg_.find_first_and_sub(idx);
    agg_.reset();
    return found;
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::prepare_and_sub_aggregator(const SV&  sv,
                                      const typename SV::value_type*  str,
                                      unsigned octet_start,
                                      bool prefix_sub)
{
    int len = 0;
    for (; str[len] != 0; ++len)
    {}
    BM_ASSERT(len);

    // use reverse order (faster for sorted arrays)
    for (int octet_idx = len-1; octet_idx >= 0; --octet_idx)
    {
        if (unsigned(octet_idx) < octet_start) // common prefix
            break;

        unsigned value = unsigned(str[octet_idx]) & 0xFF;
        BM_ASSERT(value != 0);
        
        bm::id64_t planes_mask;
        if (&sv == bound_sv_)
            planes_mask = vector_plane_masks_[unsigned(octet_idx)];
        else
            planes_mask = sv.get_slice_mask(unsigned(octet_idx));

        if ((value & planes_mask) != value) // pre-screen for impossible values
            return false; // found non-existing plane

        add_agg_char(agg_, sv, octet_idx, planes_mask, value); // AND-SUB aggregator
     } // for octet_idx
    
    // add all vectors above string len to the SUB aggregation group
    //
    if (prefix_sub)
    {
        unsigned plane_idx = unsigned(len * 8);
        typename SV::size_type planes = sv.get_bmatrix().rows();
        for (; plane_idx < planes; ++plane_idx)
        {
            if (bvector_type_const_ptr bv = sv.get_slice(plane_idx))
                agg_.add(bv, 1); // agg to SUB group
        } // for
    }
    return true;
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::prepare_and_sub_aggregator(const SV&   sv,
                                           typename SV::value_type   value)
{
    using unsigned_value_type = typename SV::unsigned_value_type;

    agg_.reset();

    unsigned char bits[sizeof(value) * 8];
    unsigned_value_type uv = sv.s2u(value);

    unsigned short bit_count_v = bm::bitscan(uv, bits);
    BM_ASSERT(bit_count_v);
    const unsigned_value_type mask1 = 1;

    // prep the lists for combined AND-SUB aggregator
    //   (backward order has better chance for bit reduction on AND)
    //
    for (unsigned i = bit_count_v; i > 0; --i)
    {
        unsigned bit_idx = bits[i-1];
        BM_ASSERT(uv & (mask1 << bit_idx));
        if (const bvector_type* bv = sv.get_slice(bit_idx))
            agg_.add(bv);
        else
            return false;
    }
    unsigned sv_planes = sv.effective_slices();
    for (unsigned i = 0; i < sv_planes; ++i)
    {
        unsigned_value_type mask = mask1 << i;
        if (bvector_type_const_ptr bv = sv.get_slice(i); bv && !(uv & mask))
            agg_.add(bv, 1); // agg to SUB group
    } // for i
    return true;
}


//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::find_eq_with_nulls_horizontal(const SV&  sv,
    typename SV::value_type    value,
    typename SV::bvector_type& bv_out)
{
    if (sv.empty())
        return; // nothing to do

    if (!value)
    {
        find_zero(sv, bv_out);
        return;
    }

    unsigned char bits[sizeof(value) * 8];
    typename SV::unsigned_value_type uv = sv.s2u(value);
    unsigned short bit_count_v = bm::bitscan(uv, bits);
    BM_ASSERT(bit_count_v);

    // aggregate AND all matching vectors
    if (const bvector_type* bv_plane = sv.get_slice(bits[--bit_count_v]))
        bv_out = *bv_plane;
    else // plane not found
    {
        bv_out.clear();
        return;
    }
    for (unsigned i = 0; i < bit_count_v; ++i)
    {
        const bvector_type* bv_plane = sv.get_slice(bits[i]);
        if (bv_plane)
            bv_out &= *bv_plane;
        else // mandatory plane not found - empty result!
        {
            bv_out.clear();
            return;
        }
    } // for i

    // SUB all other planes
    unsigned sv_planes = sv.effective_slices();
    for (unsigned i = 0; (i < sv_planes) && uv; ++i)
    {
        if (const bvector_type* bv = sv.get_slice(i); bv && !(uv & (1u << i)))
            bv_out -= *bv;
    } // for i
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq_str_prefix(const SV&   sv,
                                const typename SV::value_type* str,
                                typename SV::bvector_type&     bv_out)
{
    return find_eq_str_impl(sv, str, bv_out, false);
}


//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq_str(const typename SV::value_type* str,
                                            typename SV::size_type&        pos)
{
    BM_ASSERT(bound_sv_);
    return this->find_eq_str(*bound_sv_, str, pos);
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq_str(const SV&                      sv,
                                            const typename SV::value_type* str,
                                            typename SV::size_type&        pos)
{
    bool found = false;
    if (sv.empty())
        return found;
    if (*str)
    {
        bool remaped = false;
        if constexpr (SV::is_remap_support::value) // test remapping trait
        {
            if (sv.is_remap() && str != remap_value_vect_.data())
            {
                auto str_len = sv.effective_vector_max();
                remap_value_vect_.resize(str_len);
                remaped = sv.remap_tosv(
                    remap_value_vect_.data(), str_len, str);
                if (!remaped)
                    return remaped;
                str = remap_value_vect_.data();
            }
        }
    
        size_type found_pos;
        found = find_first_eq(sv, str, found_pos, remaped);
        if (found)
        {
            pos = found_pos;
            if constexpr (SV::is_rsc_support::value) // test rank/select trait
            {
                if (sv.is_compressed()) // if compressed vector - need rank translation
                    found = sv.find_rank(found_pos + 1, pos);
            }
        }
    }
    else // search for zero(empty str) value
    {
        // TODO: implement optimized version which would work witout temp vector
        typename SV::bvector_type bv_zero;
        find_zero(sv, bv_zero);
        found = bv_zero.find(pos);
    }
    return found;
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq_str(const typename SV::value_type* str,
                                            typename SV::bvector_type& bv_out)
{
    BM_ASSERT(bound_sv_);
    return find_eq_str(*bound_sv_, str, bv_out);
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq_str(const SV&                      sv,
                                            const typename SV::value_type* str,
                                            typename SV::bvector_type& bv_out)
{
    return find_eq_str_impl(sv, str, bv_out, true);
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::remap_tosv(
                                remap_vector_type& remap_vect_target,
                                const typename SV::value_type* str,
                                const SV& sv)
{
    auto str_len = sv.effective_vector_max();
    remap_vect_target.resize(str_len);
    bool remaped = sv.remap_tosv(remap_vect_target.data(), str_len, str);
    return remaped;
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq_str_impl(const SV&  sv,
                             const typename SV::value_type* str,
                             typename SV::bvector_type& bv_out,
                             bool prefix_sub)
{
    bool found = false;
    bv_out.clear(true);
    if (sv.empty())
        return false; // nothing to do
    if constexpr (SV::is_rsc_support::value) // test rank/select trait
    {
        // search in RS compressed string vectors is not implemented
        BM_ASSERT(0);
    }

    if (*str)
    {
        if constexpr (SV::is_remap_support::value) // test remapping trait
        {
            if (sv.is_remap() && (str != remap_value_vect_.data()))
            {
                bool remaped = remap_tosv(remap_value_vect_, str, sv);
                if (!remaped)
                    return false; // not found because
                str = remap_value_vect_.data();
            }
        }
        /// aggregator search
        agg_.reset();

        const unsigned common_prefix_len = 0;
        found = prepare_and_sub_aggregator(sv, str, common_prefix_len,
                                           prefix_sub);
        if (!found)
            return found;
        found = agg_.combine_and_sub(bv_out);

        agg_.reset();
    }
    else // search for zero(empty str) value
    {
        find_zero(sv, bv_out);
        found = bv_out.any();
    }
    return found;

}

//----------------------------------------------------------------------------

template<typename SV> template<class TPipe>
void sparse_vector_scanner<SV>::find_eq_str(TPipe& pipe)
{
    agg_.combine_and_sub(pipe.agg_pipe_);
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::bfind_eq_str(
                                    const SV&                      sv,
                                    const typename SV::value_type* str,
                                    typename SV::size_type&        pos)
{
    bool found = false;
    if (sv.empty())
        return found;

    if (*str)
    {
        bool remaped = false;
        // test search pre-condition based on remap tables
        if constexpr (SV::is_remap_support::value)
        {
            if (sv.is_remap() && (str != remap_value_vect_.data()))
            {
                auto str_len = sv.effective_vector_max();
                remap_value_vect_.resize(str_len);
                remaped = sv.remap_tosv(remap_value_vect_.data(), str_len, str);
                if (!remaped)
                    return remaped;
            }
        }
        
        reset_search_range();
        
        // narrow down the search
        const unsigned min_distance_cutoff = bm::gap_max_bits + bm::gap_max_bits / 2;
        size_type l, r, dist;
        l = 0; r = sv.size()-1;
        size_type found_pos;
        
        // binary search to narrow down the search window
        while (l <= r)
        {
            dist = r - l;
            if (dist < min_distance_cutoff)
            {
                // we are in an narrow <2 blocks window, but still may be in two
                // different neighboring blocks, lets try to narrow
                // to exactly one block
                
                size_type nb_l = (l >> bm::set_block_shift);
                size_type nb_r = (r >> bm::set_block_shift);
                if (nb_l != nb_r)
                {
                    size_type mid = nb_r * bm::gap_max_bits;
                    if (mid < r)
                    {
                        int cmp = this->compare_str(sv, mid, str);
                        if (cmp < 0) // mid < str
                            l = mid;
                        else
                            r = mid-(cmp!=0); // branchless if (cmp==0) r=mid;
                        BM_ASSERT(l < r);
                    }
                    nb_l = unsigned(l >> bm::set_block_shift);
                    nb_r = unsigned(r >> bm::set_block_shift);
                }
                
                if (nb_l == nb_r)
                {
                    size_type max_nb = sv.size() >> bm::set_block_shift;
                    if (nb_l != max_nb)
                    {
                        // linear in-place fixed depth scan to identify the sub-range
                        size_type mid = nb_r * bm::gap_max_bits + bm::sub_block3_size;
                        int cmp = this->compare_str(sv, mid, str);
                        if (cmp < 0)
                        {
                            l = mid;
                            mid = nb_r * bm::gap_max_bits + bm::sub_block3_size * 2;
                            cmp = this->compare_str(sv, mid, str);
                            if (cmp < 0)
                            {
                                l = mid;
                                mid = nb_r * bm::gap_max_bits + bm::sub_block3_size * 3;
                                cmp = this->compare_str(sv, mid, str);
                                if (cmp < 0)
                                    l = mid;
                                else
                                    r = mid;
                            }
                            else
                            {
                                r = mid;
                            }
                        }
                        else
                        {
                            r = mid;
                        }
                    }
                }
                
                set_search_range(l, r);
                break;
            }

            typename SV::size_type mid = dist/2+l;
            size_type nb = (mid >> bm::set_block_shift);
            mid = nb * bm::gap_max_bits;
            if (mid <= l)
            {
                if (nb == 0 && r > bm::gap_max_bits)
                    mid = bm::gap_max_bits;
                else
                    mid = dist / 2 + l;
            }
            BM_ASSERT(mid > l);

            int cmp = this->compare_str(sv, mid, str);
            if (cmp == 0)
            {
                found_pos = mid;
                found = true;
                set_search_range(l, mid);
                break;
            }
            if (cmp < 0)
                l = mid+1;
            else
                r = mid-1;
        } // while

        // use linear search (range is set)
        found = find_first_eq(sv, str, found_pos, remaped);
        if (found)
        {
            pos = found_pos;
            if (bm::conditional<SV::is_rsc_support::value>::test()) // test rank/select trait
            {
                if (sv.is_compressed()) // if compressed vector - need rank translation
                    found = sv.find_rank(found_pos + 1, pos);
            }
        }
        reset_search_range();
    }
    else // search for zero value
    {
        // TODO: implement optimized version which would work without temp vector
        typename SV::bvector_type bv_zero;
        find_zero(sv, bv_zero);
        found = bv_zero.find(pos);
    }
    return found;
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::bfind_eq_str(const typename SV::value_type* str,
                                             typename SV::size_type&        pos)
{
    BM_ASSERT(bound_sv_);
    return bfind_eq_str(*bound_sv_, str, pos);
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::bfind(const SV&                      sv,
                                      const typename SV::value_type  val,
                                      typename SV::size_type&        pos)
{
    int cmp;
    size_type l = 0, r = sv.size();
    if (l == r) // empty vector
    {
        pos = 0;
        return false;
    }
    --r;
    
    // check initial boundary conditions if insert point is at tail/head
    cmp = this->compare(sv, l, val); // left (0) boundary check
    if (cmp > 0) // vect[x] > str
    {
        pos = 0;
        return false;
    }
    if (cmp == 0)
    {
        pos = 0;
        return true;
    }
    cmp = this->compare(sv, r, val); // right(size-1) boundary check
    if (cmp == 0)
    {
        pos = r;
        // back-scan to rewind all duplicates
        // TODO: adapt one-sided binary search to traverse large platos
        for (; r >= 0; --r)
        {
            cmp = this->compare(sv, r, val);
            if (cmp != 0)
                return true;
            pos = r;
        } // for i
        return true;
    }
    if (cmp < 0) // vect[x] < str
    {
        pos = r+1;
        return false;
    }
    
    size_type dist = r - l;
    if (dist < linear_cutoff1)
    {
        for (; l <= r; ++l)
        {
            cmp = this->compare(sv, l, val);
            if (cmp == 0)
            {
                pos = l;
                return true;
            }
            if (cmp > 0)
            {
                pos = l;
                return false;
            }
        } // for
    }
    
    while (l <= r)
    {
        size_type mid = (r-l)/2+l;
        cmp = this->compare(sv, mid, val);
        if (cmp == 0)
        {
            pos = mid;
            // back-scan to rewind all duplicates
            for (size_type i = mid-1; i >= 0; --i)
            {
                cmp = this->compare(sv, i, val);
                if (cmp != 0)
                    return true;
                pos = i;
            } // for i
            pos = 0;
            return true;
        }
        if (cmp < 0)
            l = mid+1;
        else
            r = mid-1;

        dist = r - l;
        if (dist < linear_cutoff2) // do linear scan here
        {
            typename SV::const_iterator it(&sv, l);
            for (; it.valid(); ++it, ++l)
            {
                typename SV::value_type sv_value = it.value();
                if (sv_value == val)
                {
                    pos = l;
                    return true;
                }
                if (sv_value > val) // vect[x] > val
                {
                    pos = l;
                    return false;
                }
            } // for it
            BM_ASSERT(0);
            pos = l;
            return false;
        }
    } // while
    
    BM_ASSERT(0);
    return false;
}


//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::lower_bound_str(
                                        const SV&  sv,
                                        const typename SV::value_type* str,
                                        typename SV::size_type&        pos)
{
    int cmp;
    size_type l = 0, r = sv.size();
    
    if (l == r) // empty vector
    {
        pos = 0;
        return false;
    }
    --r;
    
    // check initial boundary conditions if insert point is at tail/head
    cmp = this->compare_str(sv, l, str); // left (0) boundary check
    if (cmp > 0) // vect[x] > str
    {
        pos = 0;
        return false;
    }
    if (cmp == 0)
    {
        pos = 0;
        return true;
    }
    cmp = this->compare_str(sv, r, str); // right(size-1) boundary check
    if (cmp == 0)
    {
        pos = r;
        // back-scan to rewind all duplicates
        // TODO: adapt one-sided binary search to traverse large platos
        for (; r >= 0; --r)
        {
            cmp = this->compare_str(sv, r, str);
            if (cmp != 0)
                return true;
            pos = r;
        } // for i
        return true;
    }
    if (cmp < 0) // vect[x] < str
    {
        pos = r+1;
        return false;
    }
    
    size_type dist = r - l;
    if (dist < linear_cutoff1)
    {
        for (; l <= r; ++l)
        {
            cmp = this->compare_str(sv, l, str);
            if (cmp == 0)
            {
                pos = l;
                return true;
            }
            if (cmp > 0)
            {
                pos = l;
                return false;
            }
        } // for
    }
    while (l <= r)
    {
        size_type mid = (r-l)/2+l;
        cmp = this->compare_str(sv, mid, str);
        if (cmp == 0)
        {
            pos = mid;
            // back-scan to rewind all duplicates
            for (size_type i = mid-1; i >= 0; --i)
            {
                cmp = this->compare_str(sv, i, str);
                if (cmp != 0)
                    return true;
                pos = i;
            } // for i
            pos = 0;
            return true;
        }
        if (cmp < 0)
            l = mid+1;
        else
            r = mid-1;

        dist = r - l;
        if (dist < linear_cutoff2) // do linear scan here
        {
            if (!hmatr_.is_init())
            {
                unsigned max_str = (unsigned)sv.effective_max_str();
                hmatr_.resize(linear_cutoff2, max_str+1, false);
            }

            dist = sv.decode(hmatr_, l, dist+1);
            for (unsigned i = 0; i < dist; ++i, ++l)
            {
                const typename SV::value_type* hm_str = hmatr_.row(i);
                cmp = ::strcmp(hm_str, str);
                if (cmp == 0)
                {
                    pos = l;
                    return true;
                }
                if (cmp > 0) // vect[x] > str
                {
                    pos = l;
                    return false;
                }
            }
            cmp = this->compare_str(sv, l, str);
            if (cmp > 0) // vect[x] > str
            {
                pos = l;
                return false;
            }
            BM_ASSERT(0);
            pos = l;
            return false;
        }
    } // while
    
    BM_ASSERT(0);
    return false;
}


//----------------------------------------------------------------------------

template<typename SV>
int sparse_vector_scanner<SV>::compare_str(const SV& sv,
                                           size_type idx,
                                           const value_type* str)
{
    if (bound_sv_ == &sv)
    {
        size_type nb = (idx >> bm::set_block_shift);
        size_type nbit = (idx & bm::set_block_mask);
        if (nbit == 0) // access to sentinel, first block element
        {
            value_type* s0 = block0_elements_cache_.row(nb);
            if (*s0 == 0) // uninitialized element
            {
                sv.get(idx, s0, size_type(block0_elements_cache_.cols()));
            }
            int res = 0;
            for (unsigned i = 0; i < block0_elements_cache_.cols(); ++i)
            {
                char octet = str[i]; char value = s0[i];
                res = (value > octet) - (value < octet);
                if (res || !octet)
                    break;
            } // for i
            return res;
        }
        else
        {
            if (nbit % bm::sub_block3_size == 0) // TODO: use AND mask here
            {
                size_type k = nbit / bm::sub_block3_size - 1;
                value_type* s1 = block3_elements_cache_.row(nb * 3 + k);
                int res = 0;
                for (unsigned i = 0; i < block3_elements_cache_.cols(); ++i)
                {
                    char octet = str[i]; char value = s1[i];
                    res = (value > octet) - (value < octet);
                    if (res || !octet)
                        break;
                } // for i
                return res;
            }
        }
    }
    return sv.compare(idx, str);
}

//----------------------------------------------------------------------------

template<typename SV>
int sparse_vector_scanner<SV>::compare(const SV& sv,
                                       size_type idx,
                                       const value_type val) BMNOEXCEPT
{
    // TODO: implement sentinel elements cache (similar to compare_str())
    return sv.compare(idx, val);
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::find_eq(const SV&                  sv,
                                        typename SV::value_type    value,
                                        typename SV::bvector_type& bv_out)
{
    if (sv.empty())
    {
        bv_out.clear();
        return; // nothing to do
    }
    if (!value)
    {
        find_zero(sv, bv_out);
        return;
    }

    find_eq_with_nulls(sv, value, bv_out, 0);
    
    decompress(sv, bv_out);
    correct_nulls(sv, bv_out);
}

//----------------------------------------------------------------------------

template<typename SV>
bool sparse_vector_scanner<SV>::find_eq(const SV&                  sv,
                                        typename SV::value_type    value,
                                        typename SV::size_type&    pos)
{
    if (!value) // zero value - special case
    {
        bvector_type bv_zero;
        find_eq(sv, value, bv_zero);
        bool found = bv_zero.find(pos);
        return found;
    }

    size_type found_pos;
    bool found = find_first_eq(sv, value, found_pos);
    if (found)
    {
        pos = found_pos;
        if (bm::conditional<SV::is_rsc_support::value>::test()) // test rank/select trait
        {
            if (sv.is_compressed()) // if compressed vector - need rank translation
                found = sv.find_rank(found_pos + 1, pos);
        }
    }
    return found;
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::find_nonzero(const SV& sv, 
                                             typename SV::bvector_type& bv_out)
{
    agg_.reset(); // in case if previous scan was interrupted
    for (unsigned i = 0; i < sv.slices(); ++i)
        agg_.add(sv.get_slice(i));
    agg_.combine_or(bv_out);
    agg_.reset();
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::decompress(const SV&   sv,
                                           typename SV::bvector_type& bv_out)
{
    if (!sv.is_compressed())
        return; // nothing to do
    const bvector_type* bv_non_null = sv.get_null_bvector();
    BM_ASSERT(bv_non_null);

    // TODO: implement faster decompressor for small result-sets
    rank_compr_.decompress(bv_tmp_, *bv_non_null, bv_out);
    bv_out.swap(bv_tmp_);
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::set_search_range(size_type from, size_type to)
{
    BM_ASSERT(from < to);
    mask_from_ = from;
    mask_to_ = to;
    mask_set_ = true;
}

//----------------------------------------------------------------------------

template<typename SV>
void sparse_vector_scanner<SV>::reset_search_range()
{
    mask_set_ = false;
    mask_from_ = mask_to_ = bm::id_max;
}


//----------------------------------------------------------------------------
// sparse_vector_scanner<SV>::pipeline<Opt>
//----------------------------------------------------------------------------

template<typename SV> template<class Opt>
void
sparse_vector_scanner<SV>::pipeline<Opt>::add(const typename SV::value_type* str)
{
    BM_ASSERT(str);

    typename aggregator_type::arg_groups* arg = agg_pipe_.add();
    BM_ASSERT(arg);

    if constexpr (SV::is_remap_support::value) // test remapping trait
    {
        if (sv_.is_remap() && (str != remap_value_vect_.data()))
        {
            bool remaped = remap_tosv(remap_value_vect_, str, sv_);
            if (!remaped)
                return; // not found (leaves the arg(arg_group) empty
            str = remap_value_vect_.data();
        }
    }
    int len = 0;
    for (; str[len] != 0; ++len)
    {}
    BM_ASSERT(len);


    // use reverse order (faster for sorted arrays)
    for (int octet_idx = len-1; octet_idx >= 0; --octet_idx)
    {
        //if (unsigned(octet_idx) < octet_start) // common prefix
        //    break;

        unsigned value = unsigned(str[octet_idx]) & 0xFF;
        BM_ASSERT(value != 0);

        bm::id64_t planes_mask;
        /*
        if (&sv == bound_sv_)
            planes_mask = vector_plane_masks_[unsigned(octet_idx)];
        else */
        planes_mask = sv_.get_slice_mask(unsigned(octet_idx));

        if ((value & planes_mask) != value) // pre-screen for impossible values
            return; // found non-existing plane

        sparse_vector_scanner<SV>::add_agg_char(
            *arg, sv_, octet_idx, planes_mask, value); // AND-SUB aggregator
     } // for octet_idx

    // add all vectors above string len to the SUB aggregation group
    //
    //if (prefix_sub)
    {
        unsigned plane_idx = unsigned(len * 8);
        typename SV::size_type planes = sv_.get_bmatrix().rows();
        for (; plane_idx < planes; ++plane_idx)
        {
            if (bvector_type_const_ptr bv = sv_.get_slice(plane_idx))
                arg->add(bv, 1); // agg to SUB group
        } // for
    }


    //return true;

}

//----------------------------------------------------------------------------


} // namespace bm


#endif
