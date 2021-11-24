#ifndef BMAGGREGATOR__H__INCLUDED__
#define BMAGGREGATOR__H__INCLUDED__
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

/*! \file bmaggregator.h
    \brief Algorithms for fast aggregation of N bvectors
*/


#ifndef BM__H__INCLUDED__
// BitMagic utility headers do not include main "bm.h" declaration 
// #include "bm.h" or "bm64.h" explicitly 
# error missing include (bm.h or bm64.h)
#endif

#include <stdio.h>
#include <string.h>


#include "bmfunc.h"
#include "bmdef.h"

#include "bmalgo_impl.h"
#include "bmbuffer.h"


namespace bm
{

/*! @name Aggregator traits and control constants
    @ingroup setalgo
*/
//@{

const bool agg_produce_result_bvectors = true;
const bool agg_disable_result_bvectors = false;
const bool agg_compute_counts = true;
const bool agg_disable_counts = false;

/**
   Aggregation options to control execution
   @ingroup setalgo
 */
template <bool OBvects=bm::agg_produce_result_bvectors,
          bool OCounts=bm::agg_disable_counts>
struct agg_run_options
{
    /// make result(target) vectors (aggregation search results) (Default: true)
    /// when false is used - means we want to only collect statistics (counts) for the targets
    static constexpr bool is_make_results() { return OBvects; }

    /// Compute counts for the target vectors, when set to true, population count is computed for
    /// each result, results itself can be omitted (make_results flag set to false)
    static constexpr bool is_compute_counts() { return OCounts; }
};

/**
    Pre-defined aggregator options to disable both intermediate results and counts
    @ingroup setalgo
 */
typedef
bm::agg_run_options<agg_disable_result_bvectors, agg_disable_counts>
agg_opt_disable_bvects_and_counts;


/**
    Pre-defined aggregator options for counts-only (results dropped) operation
    @ingroup setalgo
 */
typedef
bm::agg_run_options<agg_disable_result_bvectors, agg_compute_counts>
agg_opt_only_counts;

/**
    Pre-defined aggregator options for results plus counts operation
    @ingroup setalgo
 */
typedef
bm::agg_run_options<agg_produce_result_bvectors, agg_compute_counts>
agg_opt_bvect_and_counts;

//@}

/**
    Algorithms for fast aggregation of a group of bit-vectors

    Algorithms of this class use cache locality optimizations and efficient
    on cases, wehen we need to apply the same logical operation (aggregate)
    more than 2x vectors.
 
    TARGET = BV1 or BV2 or BV3 or BV4 ...

    Aggregator supports OR, AND and AND-MINUS (AND-SUB) operations
 
    @ingroup setalgo
*/
template<typename BV>
class aggregator
{
public:
    typedef BV                          bvector_type;
    typedef typename BV::size_type      size_type;
    typedef typename BV::allocator_type allocator_type;
    typedef const bvector_type*         bvector_type_const_ptr;
    typedef bm::id64_t                  digest_type;
    
    typedef typename bvector_type::allocator_type::allocator_pool_type allocator_pool_type;
    typedef
    bm::heap_vector<bvector_type_const_ptr, allocator_type, true> bv_vector_type;
    typedef
    bm::heap_vector<bvector_type*, allocator_type, true> bvect_vector_type;
    typedef
    bm::heap_vector<size_t, allocator_type, true> index_vector_type;


    /// Codes for aggregation operations which can be pipelined for efficient execution
    ///
    enum operation
    {
        BM_NOT_DEFINED = 0,
        BM_SHIFT_R_AND = 1
    };

    enum operation_status
    {
        op_undefined = 0,
        op_prepared,
        op_in_progress,
        op_done
    };

    /// Aggregator arg groups
    struct arg_groups
    {
        bv_vector_type     arg_bv0;            ///< arg group 0
        bv_vector_type     arg_bv1;            ///< arg group 1
        index_vector_type  arg_idx0;           ///< indexes of vectors for arg group 0
        index_vector_type  arg_idx1;

        /// Reset argument groups to zero
        void reset()
        {
            arg_bv0.resize(0); // TODO: use reset not resize(0)
            arg_bv1.resize(0);
            arg_idx0.resize(0);
            arg_idx1.resize(0);
        }

        /** Add bit-vector pointer to its aggregation group
        \param bv - input bit-vector pointer to attach
        \param agr_group - input argument group index (0 - default, 1 - fused op)

        @return current arg group size (0 if vector was not added (empty))
        */
        size_t add(const bvector_type* bv, unsigned agr_group);
    };

    typedef arg_groups*    arg_groups_type_ptr;
    typedef
    bm::heap_vector<arg_groups_type_ptr, allocator_type, true> arg_vector_type;
    typedef
    bm::heap_vector<unsigned, allocator_type, true> count_vector_type;
    typedef
    bm::heap_vector<size_type, allocator_type, true> bv_count_vector_type;
    typedef
    bm::heap_vector<bm::word_t*, allocator_type, true> blockptr_vector_type;
    typedef
    bm::heap_vector<bm::pair<unsigned, unsigned>, allocator_type, true> block_ij_vector_type;

    /**
        Block cache for pipeline execution
        @internal
     */
    struct pipeline_bcache
    {
        bv_vector_type       bv_inp_vect_; ///<  all input vectors from all groups
        count_vector_type    cnt_vect_;    ///< usage count for bv_inp (all groups)
        blockptr_vector_type blk_vect_;    ///< cached block ptrs for bv_inp_vect_
        block_ij_vector_type blk_ij_vect_; ///< current block coords
    };

    /**
       Aggregation options for runtime control
     */
    struct run_options
    {
        /// Batch size sets number of argument groups processed at a time
        /// Default: 0 means this parameter will be determined automatically
        size_t batch_size = 0;
    };

    /**
        Pipeline vector for running a group of aggregation operations on a family of vectors.
        Pipeline is used to run multiple aggregation combinations (searches) for essencially same
        set of vectors (different combinations of ANDs and SUBs for example).
        Pipeline execution improves CPU cache reuse and caches the compressed blocks to re-use it
        for more efficient execution. Essencially it is a tool to run thousads of runs at once faster.
     */
    template<class Opt = bm::agg_run_options<> >
    class pipeline
    {
    public:
        typedef Opt options_type;
    public:
        pipeline() {}
        ~pipeline() BMNOEXCEPT;

        /// Set pipeline run options
        run_options& options() BMNOEXCEPT { return options_; }

        /// Get pipeline run options
        const run_options& get_options() const BMNOEXCEPT { return options_; }

        // ------------------------------------------------------------------
        /*! @name pipeline argument groups fill-in methods */
        //@{

        /** Add new arguments group
        */
        arg_groups* add();

        /**
            Attach OR (aggregator vector).
            Pipeline results all will be OR-ed (UNION) into the OR target vector
           @param bv_or - OR target vector
         */
        void set_or_target(bvector_type* bv_or) BMNOEXCEPT
            { bv_or_target_ = bv_or; }

        /** Prepare pipeline for the execution (resize and init internal structures)
            Once complete, you cannot add() to it.
        */
        void complete();

        /** return true if pipeline is ready for execution (complete) */
        bool is_complete() const BMNOEXCEPT { return is_complete_; }

        //@}

        // ------------------------------------------------------------------

        /** Return argument vector used for pipeline execution */
        const arg_vector_type& get_args_vector() const BMNOEXCEPT
            { return arg_vect_; }

        /** Return results vector used for pipeline execution */
        bvect_vector_type& get_bv_res_vector()  BMNOEXCEPT
            { return bv_res_vect_; }

        /** Return results vector count used for pipeline execution */
        bv_count_vector_type& get_bv_count_vector()  BMNOEXCEPT
            { return count_res_vect_; }


        // ------------------------------------------------------------------
        /*! @name access to internals */
        //@{

        const bv_vector_type& get_all_input_vect() const BMNOEXCEPT
            { return bcache_.bv_inp_vect_; }
        const count_vector_type& get_all_input_cnt_vect() const BMNOEXCEPT
            { return bcache_.cnt_vect_; }

        /// Return number of unique vectors in the pipeline (after complete())
        size_t unique_vectors() const BMNOEXCEPT
            { return bcache_.bv_inp_vect_.size(); }

        /// Function looks at the pipeline to apply euristics to suggest optimal run_batch parameter
        size_t compute_run_batch() const BMNOEXCEPT;
        //@}

    protected:
        /** @internal */
        pipeline_bcache& get_bcache() BMNOEXCEPT
            { return bcache_; }
        /** Return number of top blocks after complete
            @internal
        */
        unsigned get_top_blocks() const BMNOEXCEPT { return top_blocks_; }

    private:
        void complete_arg_group(arg_groups* ag);
        void complete_arg_sub_group(index_vector_type& idx_vect,
            const bvector_type_const_ptr* bv_src, size_t size);

    protected:
        friend class aggregator;

        pipeline(const pipeline&) = delete;
        pipeline& operator=(const pipeline&) = delete;

    protected:
        run_options          options_;  ///< execution parameters
        bool                 is_complete_ = false; ///< ready to run state flag
        size_t               total_vect_= 0; ///< total number of vector mentions in all groups
        arg_vector_type      arg_vect_;    ///< input arg. groups

        bvect_vector_type    bv_res_vect_;    ///< results (bit-vector ptrs)
        bv_count_vector_type count_res_vect_; ///< results (counts)

        pipeline_bcache      bcache_;     ///< blocks cache structure
        unsigned top_blocks_ = 1;         ///< top-level structure size, max of all bvectors
        bvector_type*        bv_or_target_ = 0;  ///< OR target bit-bector ptr
    };

public:

    // -----------------------------------------------------------------------
    /*! @name Construction and setup */
    //@{
    aggregator();
    ~aggregator();

    /**
        \brief set on-the-fly bit-block compression
        By default aggregator does not try to optimize result, but in some cases
        it can be quite a lot faster than calling bvector<>::optimize() later
        (because block data sits in CPU cache).
     
        \param opt - optimization mode (full compression by default)
    */
    void set_optimization(
        typename bvector_type::optmode opt = bvector_type::opt_compress) BMNOEXCEPT
        { opt_mode_ = opt; }

    void set_compute_count(bool count_mode) BMNOEXCEPT
        { compute_count_ = count_mode; count_ = 0; }

    //@}
    
    
    // -----------------------------------------------------------------------
    
    /*! @name Methods to setup argument groups and run operations on groups */
    //@{
    
    /**
        Attach source bit-vector to a argument group (0 or 1).
        Arg group 1 used for fused operations like (AND-SUB)
        \param bv - input bit-vector pointer to attach
        \param agr_group - input argument group (0 - default, 1 - fused op)
     
        @return current arg group size (0 if vector was not added (empty))
        @sa reset
    */
    size_t add(const bvector_type* bv, unsigned agr_group = 0);
    
    /**
        Reset aggregate groups, forget all attached vectors
    */
    void reset();

    /**
        Aggregate added group of vectors using logical OR
        Operation does NOT perform an explicit reset of arg group(s)
     
        \param bv_target - target vector (input is arg group 0)
     
        @sa add, reset
    */
    void combine_or(bvector_type& bv_target);

    /**
        Aggregate added group of vectors using logical AND
        Operation does NOT perform an explicit reset of arg group(s)

        \param bv_target - target vector (input is arg group 0)
     
        @sa add, reset
    */
    void combine_and(bvector_type& bv_target);

    /**
        Aggregate added group of vectors using fused logical AND-SUB
        Operation does NOT perform an explicit reset of arg group(s)

        \param bv_target - target vector (input is arg group 0(AND), 1(SUB) )
     
        \return true if anything was found
     
        @sa add, reset
    */
    bool combine_and_sub(bvector_type& bv_target);

    /**
        Run AND-SUB: AND (groups1) AND NOT ( OR(group0)) for a pipeline
       @param pipe - pipeline to run (should be prepared, filled and complete
     */
    template<class TPipe>
    void combine_and_sub(TPipe& pipe);

    
    
    /**
        Aggregate added group of vectors using fused logical AND-SUB
        Operation does NOT perform an explicit reset of arg group(s)
        Operation can terminate early if anything was found.

        \param bv_target - target vector (input is arg group 0(AND), 1(SUB) )
        \param any - if true, search result will terminate of first found result

        \return true if anything was found

        @sa add, reset, find_first_and_sub
    */
    bool combine_and_sub(bvector_type& bv_target, bool any);

    /**
        Aggregate added group of vectors using fused logical AND-SUB,
        find the first match

        \param idx - [out] index of the first occurence
        \return true if anything was found
        @sa combine_and_sub
     */
    bool find_first_and_sub(size_type& idx);


    /**
        Aggregate added group of vectors using SHIFT-RIGHT and logical AND
        Operation does NOT perform an explicit reset of arg group(s)

        \param bv_target - target vector (input is arg group 0)
     
        @return bool if anything was found
     
        @sa add, reset
    */
    void combine_shift_right_and(bvector_type& bv_target);
    
    /**
        Set search hint for the range, where results needs to be searched
        (experimental for internal use).
    */
    void set_range_hint(size_type from, size_type to) BMNOEXCEPT;

    size_type count() const { return count_; }
    
    //@}
    
    // -----------------------------------------------------------------------
    
    /*! @name Logical operations (C-style interface) */
    //@{

    /**
        Aggregate group of vectors using logical OR
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_or(bvector_type& bv_target,
                    const bvector_type_const_ptr* bv_src, size_t src_size);

    /**
        Aggregate group of vectors using logical AND
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_and(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src, size_t src_size);

    /**
        Fusion aggregate group of vectors using logical AND MINUS another set
     
        \param bv_target     - target vector
        \param bv_src_and    - array of pointers on bit-vectors for AND
        \param src_and_size  - size of AND group
        \param bv_src_sub    - array of pointers on bit-vectors for SUBstract
        \param src_sub_size  - size of SUB group
        \param any           - flag if caller needs any results asap (incomplete results)
     
        \return true when found
    */
    bool combine_and_sub(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
                     const bvector_type_const_ptr* bv_src_sub, size_t src_sub_size,
                     bool any);
    
    bool find_first_and_sub(size_type& idx,
                     const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
                     const bvector_type_const_ptr* bv_src_sub, size_t src_sub_size);

    /**
        Fusion aggregate group of vectors using SHIFT right with AND
     
        \param bv_target     - target vector
        \param bv_src_and    - array of pointers on bit-vectors for AND masking
        \param src_and_size  - size of AND group
        \param any           - flag if caller needs any results asap (incomplete results)
     
        \return true when found
    */
    bool combine_shift_right_and(bvector_type& bv_target,
            const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
            bool any);

    
    //@}

    // -----------------------------------------------------------------------

    /*! @name Horizontal Logical operations used for tests (C-style interface) */
    //@{
    
    /**
        Horizontal OR aggregation (potentially slower) method.
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_or_horizontal(bvector_type& bv_target,
                               const bvector_type_const_ptr* bv_src,
                               size_t src_size);
    /**
        Horizontal AND aggregation (potentially slower) method.
        \param bv_target - target vector
        \param bv_src    - array of pointers on bit-vector aggregate arguments
        \param src_size  - size of bv_src (how many vectors to aggregate)
    */
    void combine_and_horizontal(bvector_type& bv_target,
                                const bvector_type_const_ptr* bv_src,
                                size_t src_size);

    /**
        Horizontal AND-SUB aggregation (potentially slower) method.
        \param bv_target - target vector
        \param bv_src_and    - array of pointers on bit-vector to AND aggregate
        \param src_and_size  - size of bv_src_and
        \param bv_src_sub    - array of pointers on bit-vector to SUB aggregate
        \param src_sub_size  - size of bv_src_sub
    */
    void combine_and_sub_horizontal(bvector_type& bv_target,
                                    const bvector_type_const_ptr* bv_src_and,
                                    size_t src_and_size,
                                    const bvector_type_const_ptr* bv_src_sub,
                                    size_t src_sub_size);

    //@}
    

    // -----------------------------------------------------------------------

    /*! @name Pipeline operations */
    //@{

    /** Get current operation code */
    int get_operation() const BMNOEXCEPT { return operation_; }

    /** Set operation code for the aggregator */
    void set_operation(int op_code) BMNOEXCEPT { operation_ = op_code; }

    /**
        Prepare operation, create internal resources, analyse dependencies.
        Prerequisites are: that operation is set and all argument vectors are added
    */
    void stage(bm::word_t* temp_block);
    
    /**
        Run a step of current arrgegation operation
    */
    operation_status run_step(unsigned i, unsigned j);
    
    operation_status get_operation_status() const { return operation_status_; }
    
    const bvector_type* get_target() const BMNOEXCEPT { return bv_target_; }
    
    bm::word_t* get_temp_block() BMNOEXCEPT { return tb_ar_->tb1; }
    
    //@}

    // -----------------------------------------------------------------------

    /*! @name Execition metrics and telemetry  */
    //@{
    bm::id64_t get_cache_gap_hits() const BMNOEXCEPT { return gap_cache_cnt_; }
    //@}

protected:
    typedef typename bvector_type::blocks_manager_type blocks_manager_type;
    typedef typename BV::block_idx_type                block_idx_type;
    typedef
    bm::heap_vector<const bm::word_t*, allocator_type, true> block_ptr_vector_type;
    typedef
    bm::heap_vector<const bm::gap_word_t*, allocator_type, true> gap_block_ptr_vector_type;
    typedef
    bm::heap_vector<unsigned char, allocator_type, true> uchar_vector_type;




    void combine_or(unsigned i, unsigned j,
                    bvector_type& bv_target,
                    const bvector_type_const_ptr* bv_src, size_t src_size);

    void combine_and(unsigned i, unsigned j,
                    bvector_type& bv_target,
                    const bvector_type_const_ptr* bv_src, size_t src_size);
    
    digest_type combine_and_sub(unsigned i, unsigned j,
                         const size_t* and_idx,
                         const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
                         const size_t* sub_idx,
                         const bvector_type_const_ptr* bv_src_sub, size_t src_sub_size,
                         int* is_result_full);
    
    void prepare_shift_right_and(bvector_type& bv_target,
                                 const bvector_type_const_ptr* bv_src,
                                 size_t src_size);

    bool combine_shift_right_and(unsigned i, unsigned j,
                                 bvector_type& bv_target,
                                 const bvector_type_const_ptr* bv_src,
                                 size_t src_size);

    static
    unsigned resize_target(bvector_type& bv_target,
                           const bvector_type_const_ptr* bv_src,
                           size_t src_size,
                           bool init_clear = true);

    static
    unsigned max_top_blocks(const bvector_type_const_ptr* bv_src,
                            size_t src_size) BMNOEXCEPT;



    /// Temporary operations vectors
    struct arena
    {
        block_ptr_vector_type     v_arg_tmp_blk;    ///< source blocks list
        block_ptr_vector_type     v_arg_or_blk;     ///< source blocks list (OR)
        gap_block_ptr_vector_type v_arg_or_blk_gap; ///< source GAP blocks list (OR)
        block_ptr_vector_type     v_arg_and_blk;     ///< source blocks list (AND)
        gap_block_ptr_vector_type v_arg_and_blk_gap; ///< source GAP blocks list (AND)
        uchar_vector_type         carry_overs;    ///<  shift carry over flags


        void reset_all_blocks()
        {
            reset_or_blocks();
            reset_and_blocks();
            carry_overs.reset();
        }
        void reset_and_blocks()
        {
            v_arg_and_blk.reset();
            v_arg_and_blk_gap.reset();
        }
        void reset_or_blocks()
        {
            v_arg_or_blk.reset();
            v_arg_or_blk_gap.reset();
        }

    };

    
    bm::word_t* sort_input_blocks_or(const size_t* src_idx,
                                     const bvector_type_const_ptr* bv_src,
                                     size_t src_size,
                                     unsigned i, unsigned j);

    bm::word_t* sort_input_blocks_and(const size_t* src_idx,
                                      const bvector_type_const_ptr* bv_src,
                                      size_t src_size,
                                      unsigned i, unsigned j);
    bm::word_t* cache_gap_block(const bm::word_t* arg_blk,
                                const size_t* src_idx,
                                size_t k,
                                unsigned i, unsigned j);


    bool process_bit_blocks_or(blocks_manager_type& bman_target,
                               unsigned i, unsigned j, const arena& ar);

    void process_gap_blocks_or(const arena& ar/*size_t block_count*/);
    
    digest_type process_bit_blocks_and(const arena& ar, /*size_t block_count,*/ digest_type digest);
    
    digest_type process_gap_blocks_and(const arena& ar, /*size_t block_count,*/ digest_type digest);

    bool test_gap_blocks_and(size_t block_count, unsigned bit_idx);
    bool test_gap_blocks_sub(size_t block_count, unsigned bit_idx);

    digest_type process_bit_blocks_sub(const arena& ar, /*size_t block_count,*/ digest_type digest);

    digest_type process_gap_blocks_sub(const arena& ar,/*size_t block_count,*/ digest_type digest);

    static
    unsigned find_effective_sub_block_size(unsigned i,
                                           const bvector_type_const_ptr* bv_src,
                                           size_t src_size,
                                           bool     top_null_as_zero) BMNOEXCEPT;

    static
    unsigned find_effective_sub_block_size(unsigned i,
                                           const bvector_type_const_ptr* bv_src1,
                                           size_t src_size1,
                                           const bvector_type_const_ptr* bv_src2,
                                           size_t src_size2) BMNOEXCEPT;

    static
    bool any_carry_overs(const unsigned char* carry_overs,
                         size_t co_size)  BMNOEXCEPT;
    
    /**
        @return carry over
    */
    static
    unsigned process_shift_right_and(bm::word_t*       BMRESTRICT blk,
                                 const bm::word_t* BMRESTRICT arg_blk,
                                 digest_type&      BMRESTRICT digest,
                                 unsigned          carry_over) BMNOEXCEPT;

    static
    const bm::word_t* get_arg_block(const bvector_type_const_ptr* bv_src,
                                unsigned k, unsigned i, unsigned j) BMNOEXCEPT;

    bvector_type* check_create_target();

    // ---------------------------------------------------------

    static arena* construct_arena()
    {
        void* p = bm::aligned_new_malloc(sizeof(arena));
        return new(p) arena();
    }
    static void free_arena(arena* ar) BMNOEXCEPT
    {
        if (!ar) return;
        ar->~arena();
        bm::aligned_free(ar);
    }

    static arg_groups* construct_arg_group()
    {
        void* p = bm::aligned_new_malloc(sizeof(arg_groups));
        return new(p) arg_groups();
    }

    static void free_arg_group(arg_groups* arg)
    {
        if (!arg) return;
        arg->~arg_groups();
        bm::aligned_free(arg);
    }

    // ---------------------------------------------------------

private:
    /// Alllocated blocka of scratch memory
    struct tb_arena
    {
        BM_DECLARE_TEMP_BLOCK(tb1)
        BM_DECLARE_TEMP_BLOCK(tb_opt)  ///< temp block for results optimization
    };


    aggregator(const aggregator&) = delete;
    aggregator& operator=(const aggregator&) = delete;
    
private:
    arg_groups           ag_;    ///< aggregator argument groups
    tb_arena*            tb_ar_; ///< data arena ptr (heap allocated)
    arena*               ar_;    ///< data arena ptr
    allocator_pool_type  pool_;  ///< pool for operations with cyclic mem.use

    bm::word_t*          temp_blk_= 0;   ///< external temp block ptr
    int                  operation_ = 0; ///< operation code (default: not defined)
    operation_status     operation_status_ = op_undefined;
    bvector_type*        bv_target_ = 0; ///< target bit-vector
    unsigned             top_block_size_ = 0; ///< operation top block (i) size
    pipeline_bcache*     bcache_ptr_ = 0; /// pipeline blocks cache ptr
    
    // search range setting (hint) [from, to]
    bool                 range_set_ = false; ///< range flag
    size_type            range_from_ = bm::id_max; ///< search from
    size_type            range_to_   = bm::id_max; ///< search to
    
    typename bvector_type::optmode opt_mode_; ///< perform search result optimization
    bool                           compute_count_; ///< compute search result count
    size_type                      count_;         ///< search result count
    //
    // execution telemetry and metrics
    bm::id64_t                     gap_cache_cnt_ = 0;
};




// ------------------------------------------------------------------------
//
// ------------------------------------------------------------------------

/**
    Experimental method ro run multiple aggregators in sync
 
    Pipeleine algorithm walts the sequence of iterators (pointers on aggregators)
    and run them all via aggregator<>::run_step() method
 
    @param first - iterator (pointer on aggregator)
    @param last - iterator (pointer on aggregator)
    @ingroup setalgo
*/
template<typename Agg, typename It>
void aggregator_pipeline_execute(It  first, It last)
{
    bm::word_t* tb = (*first)->get_temp_block();

    int pipeline_size = 0;
    for (It it = first; it != last; ++it, ++pipeline_size)
    {
        Agg& agg = *(*it);
        agg.stage(tb);
    }
    for (unsigned i = 0; i < bm::set_sub_array_size; ++i)
    {
        unsigned j = 0;
        do
        {
            // run all aggregators for the [i,j] coordinate
            unsigned w = 0;
            for (It it = first; it != last; ++it, ++w)
            {
                Agg& agg = *(*it);
                auto op_st = agg.get_operation_status();
                if (op_st != Agg::op_done)
                {
                    op_st = agg.run_step(i, j);
                    pipeline_size -= (op_st == Agg::op_done);
                }
            } // for it
            if (pipeline_size <= 0)
                return;

        } while (++j < bm::set_sub_array_size);

    } // for i
}


// ------------------------------------------------------------------------
//
// ------------------------------------------------------------------------


template<typename BV>
aggregator<BV>::aggregator()
: opt_mode_(bvector_type::opt_none),
  compute_count_(false),
  count_(0)
{
    tb_ar_ = (tb_arena*) bm::aligned_new_malloc(sizeof(tb_arena));
    ar_ = construct_arena();
}

// ------------------------------------------------------------------------

template<typename BV>
aggregator<BV>::~aggregator()
{
    BM_ASSERT(ar_ && tb_ar_);
    bm::aligned_free(tb_ar_);

    free_arena(ar_);

    delete bv_target_; 
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::reset()
{
    ag_.reset();
    ar_->reset_all_blocks();
    operation_ = top_block_size_ = 0;
    operation_status_ = op_undefined;
    range_set_ = false;
    range_from_ = range_to_ = bm::id_max;
    count_ = 0;
    bcache_ptr_ = 0;
    gap_cache_cnt_ = 0;
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::set_range_hint(size_type from, size_type to) BMNOEXCEPT
{
    range_from_ = from; range_to_ = to;
    range_set_ = true;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::bvector_type*
aggregator<BV>::check_create_target()
{
    if (!bv_target_)
    {
        bv_target_ = new bvector_type(); //TODO: get rid of "new"
        bv_target_->init();
    }
    return bv_target_;
}

// ------------------------------------------------------------------------

template<typename BV>
size_t aggregator<BV>::add(const bvector_type* bv, unsigned agr_group)
{
    return ag_.add(bv, agr_group);
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or(bvector_type& bv_target)
{
    combine_or(bv_target, ag_.arg_bv0.data(), ag_.arg_bv0.size());
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and(bvector_type& bv_target)
{
    //combine_and(bv_target, ag_.arg_bv0.data(), ag_.arg_bv0.size());
    // implemented ad AND-SUB (with an empty MINUS set)
    combine_and_sub(bv_target,
                        ag_.arg_bv0.data(), ag_.arg_bv0.size(),
                        0, 0,
                        false);
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_and_sub(bvector_type& bv_target)
{
    return combine_and_sub(bv_target,
                    ag_.arg_bv0.data(), ag_.arg_bv0.size(),
                    ag_.arg_bv1.data(), ag_.arg_bv1.size(),
                    false);
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_and_sub(bvector_type& bv_target, bool any)
{
    return combine_and_sub(bv_target,
                    ag_.arg_bv0.data(), ag_.arg_bv0.size(),
                    ag_.arg_bv1.data(), ag_.arg_bv1.size(),
                    any);
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::find_first_and_sub(size_type& idx)
{
    return find_first_and_sub(idx,
                        ag_.arg_bv0.data(), ag_.arg_bv0.size(), //arg_group0_size,
                        ag_.arg_bv1.data(), ag_.arg_bv1.size());//arg_group1_size);
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_shift_right_and(bvector_type& bv_target)
{
    count_ = 0;
    ar_->reset_all_blocks();
    combine_shift_right_and(bv_target, ag_.arg_bv0.data(), ag_.arg_bv0.size(),//arg_group0_size,
    false);
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or(bvector_type& bv_target,
                        const bvector_type_const_ptr* bv_src, size_t src_size)
{
    if (!src_size)
    {
        bv_target.clear();
        return;
    }
    ag_.reset();
    ar_->reset_or_blocks();
    unsigned top_blocks = resize_target(bv_target, bv_src, src_size);
    for (unsigned i = 0; i < top_blocks; ++i)
    {
        unsigned set_array_max = 
            find_effective_sub_block_size(i, bv_src, src_size, false);
        for (unsigned j = 0; j < set_array_max; ++j)
        {
            combine_or(i, j, bv_target, bv_src, src_size);
        } // for j
    } // for i
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and(bvector_type&                 bv_target,
                                 const bvector_type_const_ptr* bv_src, 
                                 size_t                      src_size)
{
    if (src_size == 1)
    {
        const bvector_type* bv = bv_src[0];
        bv_target = *bv;
        return;
    }
    if (!src_size)
    {
        bv_target.clear();
        return;
    }
    ag_.reset();
    ar_->reset_and_blocks();
    unsigned top_blocks = resize_target(bv_target, bv_src, src_size);
    for (unsigned i = 0; i < top_blocks; ++i)
    {
        // TODO: find range, not just size
        unsigned set_array_max = 
            find_effective_sub_block_size(i, bv_src, src_size, true);
        for (unsigned j = 0; j < set_array_max; ++j)
        {
            // TODO: use block_managers not bvectors to avoid extra indirect
            combine_and(i, j, bv_target, bv_src, src_size);
        } // for j 
    } // for i
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_and_sub(bvector_type& bv_target,
                 const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
                 const bvector_type_const_ptr* bv_src_sub, size_t src_sub_size,
                 bool any)
{
    bool global_found = false;

    if (!bv_src_and || !src_and_size)
    {
        bv_target.clear();
        return false;
    }
    
    blocks_manager_type& bman_target = bv_target.get_blocks_manager();

    unsigned top_blocks = resize_target(bv_target, bv_src_and, src_and_size);
    unsigned top_blocks2 = resize_target(bv_target, bv_src_sub, src_sub_size, false);
    
    if (top_blocks2 > top_blocks)
        top_blocks = top_blocks2;

    for (unsigned i = 0; i < top_blocks; ++i)
    {
        const unsigned set_array_max =
            find_effective_sub_block_size(i, bv_src_and, src_and_size,
                                             bv_src_sub, src_sub_size);
        for (unsigned j = 0; j < set_array_max; ++j)
        {
            int is_res_full;
            digest_type digest = combine_and_sub(i, j,
                                                0, bv_src_and, src_and_size,
                                                0, bv_src_sub, src_sub_size,
                                                &is_res_full);
            if (is_res_full)
            {
                bman_target.check_alloc_top_subblock(i);
                bman_target.set_block_ptr(i, j, (bm::word_t*)FULL_BLOCK_FAKE_ADDR);
                if (j == bm::set_sub_array_size-1)
                    bman_target.validate_top_full(i);
                if (any)
                    return any;
            }
            else
            {
                bool found = digest;
                if (found)
                {
                    bman_target.opt_copy_bit_block(i, j, tb_ar_->tb1,
                                bvector_type::opt_compress, tb_ar_->tb_opt);
                    if (any)
                        return found;
                }
                global_found |= found;
            }
        } // for j
    } // for i
    return global_found;
}

// ------------------------------------------------------------------------

template<typename BV> template<class TPipe>
void aggregator<BV>::combine_and_sub(TPipe& pipe)
{
    BM_ASSERT(pipe.is_complete());

    const arg_vector_type& pipe_args = pipe.get_args_vector();
    size_t pipe_size = pipe_args.size();
    if (!pipe_size)
        return;

    reset();

    bcache_ptr_ = &pipe.get_bcache();  // setup common cache block

    unsigned top_blocks = pipe.get_top_blocks();
    BM_ASSERT(top_blocks);

    if (pipe.bv_or_target_)
    {
        pipe.bv_or_target_->get_blocks_manager().reserve_top_blocks(top_blocks);
    }


    size_t batch_size = pipe.get_options().batch_size;
    if (!batch_size)
        batch_size = pipe.compute_run_batch();

    for (size_t batch_from(0), batch_to(0); batch_from < pipe_size;
         batch_from = batch_to)
    {
        batch_to = batch_from + batch_size;
        if (batch_to > pipe_size)
            batch_to = pipe_size;
        if (!batch_size)
            batch_size = 1;
        for (unsigned i = 0; i < top_blocks; ++i)
        {
            for (unsigned j = 0; j < bm::set_sub_array_size; ++j)
            {
                size_t p = batch_from;
                for (; p < batch_to; ++p)
                {
                    const arg_groups* ag = pipe_args[p];
                    size_t src_and_size = ag->arg_bv0.size();
                    if (!src_and_size)
                        continue;

                    const bvector_type_const_ptr* bv_src_and = ag->arg_bv0.data();
                    const size_t* bv_src_and_idx = ag->arg_idx0.data();

                    const bvector_type_const_ptr* bv_src_sub = ag->arg_bv1.data();
                    const size_t* bv_src_sub_idx = ag->arg_idx1.data();
                    size_t src_sub_size = ag->arg_bv1.size();

                    int is_res_full;
                    digest_type digest = combine_and_sub(i, j,
                                                         bv_src_and_idx,
                                                         bv_src_and, src_and_size,
                                                         bv_src_sub_idx,
                                                         bv_src_sub, src_sub_size,
                                                         &is_res_full);
                    if (digest || is_res_full)
                    {
                        if (pipe.bv_or_target_)
                        {
                            blocks_manager_type& bman =
                                pipe.bv_or_target_->get_blocks_manager();
                            const bm::word_t* arg_blk =
                                (is_res_full) ? (bm::word_t*)FULL_BLOCK_FAKE_ADDR
                                              : tb_ar_->tb1;
                            bman.check_alloc_top_subblock(i);
                            bm::word_t* blk = bman.get_block_ptr(i, j);
                            pipe.bv_or_target_->combine_operation_block_or(
                                                    i, j, blk, arg_blk);
                        }
                        if constexpr (!TPipe::options_type::is_make_results()) // drop results
                        {
                            if constexpr (TPipe::options_type::is_compute_counts())
                            {
                                if (is_res_full)
                                    pipe.count_res_vect_[p]+=bm::gap_max_bits;
                                else
                                    pipe.count_res_vect_[p]+=
                                    bm::bit_block_count(tb_ar_->tb1, digest);
                            }
                        }
                        else // results requested
                        {
                            bvect_vector_type& bv_targets_vect =
                                                pipe.get_bv_res_vector();
                            bvector_type* bv_target = bv_targets_vect[p];
                            if (!bv_target)
                            {
                                BM_ASSERT(!bv_targets_vect[p]);
                                bv_target = new bvector_type(bm::BM_GAP);
                                bv_targets_vect[p] = bv_target;
                                typename bvector_type::blocks_manager_type& bman =
                                                    bv_target->get_blocks_manager();

                                bman.reserve_top_blocks(top_blocks);
                            }
                            blocks_manager_type& bman =
                                bv_target->get_blocks_manager();
                            if (is_res_full)
                            {
                                bman.check_alloc_top_subblock(i);
                                bman.set_block_ptr(i, j,
                                            (bm::word_t*)FULL_BLOCK_FAKE_ADDR);
                                if (j == bm::set_sub_array_size-1)
                                    bman.validate_top_full(i);
                                if constexpr (TPipe::options_type::is_compute_counts())
                                    pipe.count_res_vect_[p] += bm::gap_max_bits;
                            }
                            else
                            {
                                if constexpr (TPipe::options_type::is_compute_counts())
                                    pipe.count_res_vect_[p] +=
                                        bm::bit_block_count(tb_ar_->tb1, digest);
                                bman.opt_copy_bit_block(i, j, tb_ar_->tb1,
                                   bvector_type::opt_compress, tb_ar_->tb_opt);
                            }
                        }
                    } // if
                } // for p
                // optimize OR target to save memory
                if (pipe.bv_or_target_ && p == pipe_size) // last batch is done
                {
                    blocks_manager_type& bman =
                        pipe.bv_or_target_->get_blocks_manager();
                    if (bm::word_t* blk = bman.get_block_ptr(i, j))
                        bman.optimize_block(i, j, blk,
                            tb_ar_->tb_opt, bvector_type::opt_compress, 0);
                }
            } // for j
        } // for i
    } // for batch

    bcache_ptr_ = 0;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::find_first_and_sub(size_type& idx,
                 const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
                 const bvector_type_const_ptr* bv_src_sub, size_t src_sub_size)
{
    if (!bv_src_and || !src_and_size)
        return false;

    unsigned top_blocks = max_top_blocks(bv_src_and, src_and_size);
    unsigned top_blocks2 = max_top_blocks(bv_src_sub, src_sub_size);
    
    if (top_blocks2 > top_blocks)
        top_blocks = top_blocks2;

    // compute range blocks coordinates
    //
    block_idx_type nblock_from = (range_from_ >> bm::set_block_shift);
    block_idx_type nblock_to = (range_to_ >> bm::set_block_shift);
    unsigned top_from = unsigned(nblock_from >> bm::set_array_shift);
    unsigned top_to = unsigned(nblock_to >> bm::set_array_shift);
    
    if (range_set_)
    {
        if (nblock_from == nblock_to) // one block search
        {
            int is_res_full;
            unsigned i = top_from;
            unsigned j = unsigned(nblock_from & bm::set_array_mask);
            digest_type digest = combine_and_sub(i, j,
                                                 0, bv_src_and, src_and_size,
                                                 0, bv_src_sub, src_sub_size,
                                                 &is_res_full);
            // is_res_full is not needed here, since it is just 1 block
            if (digest)
            {
                unsigned block_bit_idx = 0;
                bool found = bm::bit_find_first(tb_ar_->tb1, &block_bit_idx, digest);
                BM_ASSERT(found);
                idx = bm::block_to_global_index(i, j, block_bit_idx);
                return found;
            }
            return false;
        }

        if (top_to < top_blocks)
            top_blocks = top_to+1;
    }
    else
    {
        top_from = 0;
    }
    
    for (unsigned i = top_from; i < top_blocks; ++i)
    {
        unsigned set_array_max = bm::set_sub_array_size;
        unsigned j = 0;
        if (range_set_)
        {
            if (i == top_from)
            {
                j = nblock_from & bm::set_array_mask;
            }
            if (i == top_to)
            {
                set_array_max = 1 + unsigned(nblock_to & bm::set_array_mask);
            }
        }
        else
        {
            set_array_max = find_effective_sub_block_size(i, bv_src_and, src_and_size, true);
            if (!set_array_max)
                continue;
            if (src_sub_size)
            {
                unsigned set_array_max2 =
                        find_effective_sub_block_size(i, bv_src_sub, src_sub_size, false);
                // TODO: should it be set_array_max2 < set_array_max ????
                //if (set_array_max2 > set_array_max)
                if (set_array_max2 < set_array_max)
                    set_array_max = set_array_max2;
            }
        }
        for (; j < set_array_max; ++j)
        {
            int is_res_full;
            digest_type digest = combine_and_sub(i, j,
                                                 0, bv_src_and, src_and_size,
                                                 0, bv_src_sub, src_sub_size,
                                                 &is_res_full);
            if (digest)
            {
                unsigned block_bit_idx = 0;
                bool found = bm::bit_find_first(tb_ar_->tb1, &block_bit_idx, digest);
                BM_ASSERT(found);
                idx = bm::block_to_global_index(i, j, block_bit_idx);
                return found;
            }
        } // for j
        //while (++j < set_array_max);
    } // for i
    return false;
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned
aggregator<BV>::find_effective_sub_block_size(
                                        unsigned i,
                                        const bvector_type_const_ptr* bv_src,
                                        size_t src_size,
                                        bool     top_null_as_zero) BMNOEXCEPT
{
    // quick hack to avoid scanning large, arrays, where such scan can be quite
    // expensive by itself (this makes this function approximate)
    if (src_size > 32)
        return bm::set_sub_array_size;

    unsigned max_size = 1;
    for (size_t k = 0; k < src_size; ++k)
    {
        const bvector_type* bv = bv_src[k];
        BM_ASSERT(bv);
        const typename bvector_type::blocks_manager_type& bman_arg =
                                                    bv->get_blocks_manager();
        const bm::word_t* const* blk_blk_arg = bman_arg.get_topblock(i);
        if (!blk_blk_arg)
        {
            if (top_null_as_zero)
                return 0;
            continue;
        }
        if ((bm::word_t*)blk_blk_arg == FULL_BLOCK_FAKE_ADDR)
            return bm::set_sub_array_size;

        for (unsigned j = bm::set_sub_array_size - 1; j > max_size; --j)
        {
            if (blk_blk_arg[j])
            {
                max_size = j;
                break;
            }
        } // for j
        if (max_size == bm::set_sub_array_size - 1)
            break;
    } // for k
    ++max_size;
    BM_ASSERT(max_size <= bm::set_sub_array_size);
    
    return max_size;
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned aggregator<BV>::find_effective_sub_block_size(unsigned i,
                                       const bvector_type_const_ptr* bv_src1,
                                       size_t src_size1,
                                       const bvector_type_const_ptr* bv_src2,
                                       size_t src_size2) BMNOEXCEPT
{
    unsigned set_array_max = find_effective_sub_block_size(i, bv_src1, src_size1, true);
    if (set_array_max && src_size2)
    {
        unsigned set_array_max2 =
                find_effective_sub_block_size(i, bv_src2, src_size2, false);
        if (set_array_max2 > set_array_max) // TODO: use range intersect
            set_array_max = set_array_max2;
    }
    return set_array_max;
}


// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or(unsigned i, unsigned j,
                                bvector_type& bv_target,
                                const bvector_type_const_ptr* bv_src,
                                size_t src_size)
{
    typename bvector_type::blocks_manager_type& bman_target =
                                        bv_target.get_blocks_manager();

    ar_->reset_or_blocks();
    bm::word_t* blk = sort_input_blocks_or(0, bv_src, src_size, i, j);

    BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);

    if (blk == FULL_BLOCK_FAKE_ADDR) // nothing to do - golden block(!)
    {
        bman_target.check_alloc_top_subblock(i);
        bman_target.set_block_ptr(i, j, blk);
        if (++j == bm::set_sub_array_size)
            bman_target.validate_top_full(i);
    }
    else
    {
        size_t arg_blk_count = ar_->v_arg_or_blk.size();
        size_t arg_blk_gap_count = ar_->v_arg_or_blk_gap.size();
        if (arg_blk_count || arg_blk_gap_count)
        {
            bool all_one = process_bit_blocks_or(bman_target, i, j, *ar_);
            if (!all_one)
            {
                if (arg_blk_gap_count)
                    process_gap_blocks_or(*ar_);
                // we have some results, allocate block and copy from temp
                bman_target.opt_copy_bit_block(i, j, tb_ar_->tb1,
                                               opt_mode_, tb_ar_->tb_opt);
            }
        }
    }
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and(unsigned i, unsigned j,
                                 bvector_type& bv_target,
                                 const bvector_type_const_ptr* bv_src,
                                 size_t src_and_size)
{
    BM_ASSERT(src_and_size);

    bm::word_t* blk = sort_input_blocks_and(0, bv_src, src_and_size, i, j);
    BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);
    if (!blk) // nothing to do - golden block(!)
        return;

    size_t arg_blk_and_count = ar_->v_arg_and_blk.size();
    size_t arg_blk_and_gap_count = ar_->v_arg_and_blk_gap.size();
    if (arg_blk_and_count || arg_blk_and_gap_count)
    {
        if (!arg_blk_and_gap_count && (arg_blk_and_count == 1))
        {
            if (ar_->v_arg_and_blk[0] == FULL_BLOCK_REAL_ADDR)
            {
                // another nothing to do: one FULL block
                blocks_manager_type& bman_target = bv_target.get_blocks_manager();
                bman_target.check_alloc_top_subblock(i);
                bman_target.set_block_ptr(i, j, blk);
                if (++j == bm::set_sub_array_size)
                    bman_target.validate_top_full(i);
                return;
            }
        }
        // AND bit-blocks
        //
        bm::id64_t digest = ~0ull;
        digest = process_bit_blocks_and(*ar_, digest);
        if (!digest)
            return;

        // AND all GAP blocks (if any)
        //
        if (arg_blk_and_gap_count)
        {
            digest = process_gap_blocks_and(*ar_, digest);
        }
        if (digest) // we have results , allocate block and copy from temp
        {
            blocks_manager_type& bman_target = bv_target.get_blocks_manager();
            bman_target.opt_copy_bit_block(i, j, tb_ar_->tb1,
                                            opt_mode_, tb_ar_->tb_opt);
        }
    }
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::combine_and_sub(
             unsigned i, unsigned j,
             const size_t* and_idx,
             const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
             const size_t* sub_idx,
             const bvector_type_const_ptr* bv_src_sub, size_t src_sub_size,
             int* is_result_full)
{
    BM_ASSERT(src_and_size);
    BM_ASSERT(is_result_full);

    *is_result_full = 0;
    bm::word_t* blk = sort_input_blocks_and(and_idx, bv_src_and, src_and_size, i, j);
    BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);
    if (!blk)
        return 0; // nothing to do - golden block(!)

    {
        size_t arg_blk_and_count = ar_->v_arg_and_blk.size();
        size_t arg_blk_and_gap_count = ar_->v_arg_and_blk_gap.size();
        if (!(arg_blk_and_count | arg_blk_and_gap_count))
            return 0; // nothing to do - golden block(!)

        ar_->reset_or_blocks();
        if (src_sub_size)
        {
            blk = sort_input_blocks_or(sub_idx, bv_src_sub, src_sub_size, i, j);
            BM_ASSERT(blk == 0 || blk == FULL_BLOCK_FAKE_ADDR);
            if (blk == FULL_BLOCK_FAKE_ADDR)
                return 0; // nothing to do - golden block(!)
        }
        else
        {
            if (!arg_blk_and_gap_count && (arg_blk_and_count == 1))
            {
                if (ar_->v_arg_and_blk[0] == FULL_BLOCK_REAL_ADDR)
                {
                    *is_result_full = 1;
                    return ~0ull;
                }
            }
        }
    }
    
    digest_type digest = ~0ull;
    
    // AND-SUB bit-blocks
    //
    digest = process_bit_blocks_and(*ar_, digest);
    if (!digest)
        return digest;
    digest = process_bit_blocks_sub(*ar_, digest);
    if (!digest)
        return digest;
    
    // AND all GAP block
    //
    digest = process_gap_blocks_and(*ar_, digest);
    if (!digest)
        return digest;
    
    digest = process_gap_blocks_sub(*ar_, digest);

    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::process_gap_blocks_or(const arena& ar)//size_t arg_blk_gap_count)
{
    size_t arg_blk_gap_count = ar.v_arg_or_blk_gap.size();
    bm::word_t* blk = tb_ar_->tb1;
    for (size_t k = 0; k < arg_blk_gap_count; ++k)
        bm::gap_add_to_bitset(blk, ar.v_arg_or_blk_gap[k]);
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_gap_blocks_and(const arena& ar,
                                       digest_type digest)
{
    bm::word_t* blk = tb_ar_->tb1;
    size_t    arg_blk_gap_count = ar.v_arg_and_blk_gap.size();
    bool single_bit_found;
    unsigned single_bit_idx;
    for (size_t k = 0; k < arg_blk_gap_count; ++k)
    {
        bm::gap_and_to_bitset(blk, ar.v_arg_and_blk_gap[k], digest);
        digest = bm::update_block_digest0(blk, digest);
        if (!digest)
        {
            BM_ASSERT(bm::bit_is_all_zero(blk));
            break;
        }
        single_bit_found = bm::bit_find_first_if_1(blk, &single_bit_idx, digest);
        if (single_bit_found)
        {
            for (++k; k < arg_blk_gap_count; ++k)
            {
                bool b = bm::gap_test_unr(ar.v_arg_and_blk_gap[k], single_bit_idx);
                if (!b)
                    return 0; // AND 0 causes result to turn 0
            } // for k
            break;
        }
    }
    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_gap_blocks_sub(const arena& ar,
                                       digest_type  digest)
{
    size_t arg_blk_gap_count = ar.v_arg_or_blk_gap.size();
    bm::word_t* blk = tb_ar_->tb1;
    bool single_bit_found;
    unsigned single_bit_idx;
    for (size_t k = 0; k < arg_blk_gap_count; ++k)
    {
        bm::gap_sub_to_bitset(blk, ar.v_arg_or_blk_gap[k], digest);
        digest = bm::update_block_digest0(blk, digest);
        if (!digest)
        {
            BM_ASSERT(bm::bit_is_all_zero(blk));
            break;
        }
        // check if logical operation reduced to a corner case of one single bit
        single_bit_found = bm::bit_find_first_if_1(blk, &single_bit_idx, digest);
        if (single_bit_found)
        {
            for (++k; k < arg_blk_gap_count; ++k)
            {
                bool b = bm::gap_test_unr(ar.v_arg_or_blk_gap[k], single_bit_idx);
                if (b)
                    return 0; // AND-NOT causes search result to turn 0
            } // for k
            break;
        }
    } // for k
    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::test_gap_blocks_and(size_t arg_blk_gap_count,
                                         unsigned bit_idx)
{
    unsigned b = 1;
    for (size_t i = 0; i < arg_blk_gap_count && b; ++i)
        b = bm::gap_test_unr(ar_->v_arg_and_blk_gap[i], bit_idx);
    return b;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::test_gap_blocks_sub(size_t arg_blk_gap_count,
                                         unsigned bit_idx)
{
    unsigned b = 1;
    for (size_t i = 0; i < arg_blk_gap_count; ++i)
    {
        b = bm::gap_test_unr(ar_->v_arg_or_blk_gap[i], bit_idx);
        if (b)
            return false;
    }
    return true;
}

// ------------------------------------------------------------------------


template<typename BV>
bool aggregator<BV>::process_bit_blocks_or(blocks_manager_type& bman_target,
                                           unsigned i, unsigned j,
                                           const arena& ar)
{
    size_t arg_blk_count = ar.v_arg_or_blk.size();
    bm::word_t* blk = tb_ar_->tb1;
    bool all_one;

    size_t k = 0;

    if (arg_blk_count)  // copy the first block
        bm::bit_block_copy(blk, ar.v_arg_or_blk[k++]);
    else
        bm::bit_block_set(blk, 0);

    // process all BIT blocks
    //
    size_t unroll_factor, len, len_unr;
    
    unroll_factor = 4;
    len = arg_blk_count - k;
    len_unr = len - (len % unroll_factor);
    for( ;k < len_unr; k+=unroll_factor)
    {
        all_one = bm::bit_block_or_5way(blk,
                                        ar.v_arg_or_blk[k], ar.v_arg_or_blk[k+1],
                                        ar.v_arg_or_blk[k+2], ar.v_arg_or_blk[k+3]);
        if (all_one)
        {
            BM_ASSERT(blk == tb_ar_->tb1);
            BM_ASSERT(bm::is_bits_one((bm::wordop_t*) blk));
            bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
            return true;
        }
    } // for k

    unroll_factor = 2;
    len = arg_blk_count - k;
    len_unr = len - (len % unroll_factor);
    for( ;k < len_unr; k+=unroll_factor)
    {
        all_one = bm::bit_block_or_3way(blk, ar.v_arg_or_blk[k], ar.v_arg_or_blk[k+1]);
        if (all_one)
        {
            BM_ASSERT(blk == tb_ar_->tb1);
            BM_ASSERT(bm::is_bits_one((bm::wordop_t*) blk));
            bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
            return true;
        }
    } // for k

    for (; k < arg_blk_count; ++k)
    {
        all_one = bm::bit_block_or(blk, ar.v_arg_or_blk[k]);
        if (all_one)
        {
            BM_ASSERT(blk == tb_ar_->tb1);
            BM_ASSERT(bm::is_bits_one((bm::wordop_t*) blk));
            bman_target.set_block(i, j, FULL_BLOCK_FAKE_ADDR, false);
            return true;
        }
    } // for k

    return false;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_bit_blocks_and(const arena& ar,
                                       digest_type digest)
{
    bm::word_t* blk = tb_ar_->tb1;
    size_t k = 0;
    size_t   arg_blk_count = ar.v_arg_and_blk.size();
    
    block_idx_type nb_from = (range_from_ >> bm::set_block_shift);
    block_idx_type nb_to = (range_to_ >> bm::set_block_shift);
    if (range_set_ && (nb_from == nb_to))
    {
        unsigned nbit_from = unsigned(range_from_ & bm::set_block_mask);
        unsigned nbit_to = unsigned(range_to_ & bm::set_block_mask);
        digest_type digest0 = bm::digest_mask(nbit_from, nbit_to);
        digest &= digest0;
        bm::block_init_digest0(blk, digest);
    }
    else
    {
        switch (arg_blk_count)
        {
        case 0:
            bm::block_init_digest0(blk, digest); // 0xFF... by default
            return digest;
        case 1:
            bm::bit_block_copy(blk, ar_->v_arg_and_blk[k]);
            return bm::calc_block_digest0(blk);
        default:
            digest = bm::bit_block_and_2way(blk,
                                            ar_->v_arg_and_blk[k],
                                            ar_->v_arg_and_blk[k+1],
                                            ~0ull);
            k += 2;
            break;
        } // switch
    }

    size_t unroll_factor, len, len_unr;
    unsigned single_bit_idx;

    unroll_factor = 4;
    len = arg_blk_count - k;
    len_unr = len - (len % unroll_factor);
    for (; k < len_unr; k += unroll_factor)
    {
        digest = 
            bm::bit_block_and_5way(blk, 
                                   ar_->v_arg_and_blk[k], ar_->v_arg_and_blk[k + 1],
                                   ar_->v_arg_and_blk[k + 2], ar_->v_arg_and_blk[k + 3],
                                   digest);
        if (!digest) // all zero
            return digest;
        bool found = bm::bit_find_first_if_1(blk, &single_bit_idx, digest);
        if (found)
        {
            unsigned nword = unsigned(single_bit_idx >> bm::set_word_shift);
            unsigned mask = 1u << (single_bit_idx & bm::set_word_mask);
            for (++k; k < arg_blk_count; ++k)
            {
                const bm::word_t* arg_blk = ar_->v_arg_and_blk[k];
                if (!(mask & arg_blk[nword]))
                {
                    blk[nword] = 0;
                    return 0;
                }
            } // for k
            break;
        }
    } // for k

    for (; k < arg_blk_count; ++k)
    {
        if (ar_->v_arg_and_blk[k] == FULL_BLOCK_REAL_ADDR)
            continue;
        digest = bm::bit_block_and(blk, ar_->v_arg_and_blk[k], digest);
        if (!digest) // all zero
            return digest;
    } // for k
    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::digest_type
aggregator<BV>::process_bit_blocks_sub(const arena& ar,
                                       digest_type digest)
{
    size_t   arg_blk_count = ar.v_arg_or_blk.size();
    bm::word_t* blk = tb_ar_->tb1;
    unsigned single_bit_idx;
    const word_t** args = ar.v_arg_or_blk.data();
    for (size_t k = 0; k < arg_blk_count; ++k)
    {
        if (ar.v_arg_or_blk[k] == FULL_BLOCK_REAL_ADDR) // golden block
        {
            digest = 0;
            break;
        }
        digest = bm::bit_block_sub(blk, args[k], digest);
        if (!digest) // all zero
            break;
        
        bool found = bm::bit_find_first_if_1(blk, &single_bit_idx, digest);
        if (found)
        {
            const unsigned mask = 1u << (single_bit_idx & bm::set_word_mask);
            unsigned nword = unsigned(single_bit_idx >> bm::set_word_shift);
            for (++k; k < arg_blk_count; ++k)
            {
                if (mask & args[k][nword])
                {
                    blk[nword] = 0;
                    return 0;
                }
            } // for k
            break;
        }
    } // for k
    return digest;
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned aggregator<BV>::resize_target(bvector_type& bv_target,
                            const bvector_type_const_ptr* bv_src, size_t src_size,
                            bool init_clear)
{
    typename bvector_type::blocks_manager_type& bman_target = bv_target.get_blocks_manager();
    if (init_clear)
    {
        if (bman_target.is_init())
            bman_target.deinit_tree();
    }
    
    unsigned top_blocks = bman_target.top_block_size();
    size_type size = bv_target.size();
    bool need_realloc = false;

    // pre-scan to do target size harmonization
    for (unsigned i = 0; i < src_size; ++i)
    {
        const bvector_type* bv = bv_src[i];
        BM_ASSERT(bv);
        const typename bvector_type::blocks_manager_type& bman_arg =
                                                bv->get_blocks_manager();
        unsigned arg_top_blocks = bman_arg.top_block_size();
        if (arg_top_blocks > top_blocks)
        {
            need_realloc = true;
            top_blocks = arg_top_blocks;
        }
        size_type arg_size = bv->size();
        if (arg_size > size)
            size = arg_size;
    } // for i
    
    if (need_realloc)
        bman_target.reserve_top_blocks(top_blocks);
    if (!bman_target.is_init())
        bman_target.init_tree();
    if (size > bv_target.size())
        bv_target.resize(size);
    
    return top_blocks;
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned
aggregator<BV>::max_top_blocks(const bvector_type_const_ptr* bv_src,
                               size_t src_size) BMNOEXCEPT
{
    unsigned top_blocks = 1;

    // pre-scan to do target size harmonization
    for (unsigned i = 0; i < src_size; ++i)
    {
        const bvector_type* bv = bv_src[i];
        if (!bv)
            continue;
        const typename bvector_type::blocks_manager_type& bman_arg = bv->get_blocks_manager();
        unsigned arg_top_blocks = bman_arg.top_block_size();
        if (arg_top_blocks > top_blocks)
            top_blocks = arg_top_blocks;
    } // for i
    return top_blocks;
}

// ------------------------------------------------------------------------

template<typename BV>
bm::word_t* aggregator<BV>::sort_input_blocks_or(
                        const size_t* src_idx,
                        const bvector_type_const_ptr* bv_src,
                        size_t src_size,
                        unsigned i, unsigned j)
{
    bm::word_t* blk = 0;
    ar_->v_arg_or_blk.reset(); ar_->v_arg_or_blk_gap.reset();
    for (size_t k = 0; k < src_size; ++k)
    {
        const bm::word_t* arg_blk =
            bv_src[k]->get_blocks_manager().get_block_ptr(i, j);
        if (BM_IS_GAP(arg_blk))
        {
            (void)src_idx;
#if (0)
            if (bcache_ptr_)
            {
                BM_ASSERT(bv == bcache_ptr_->bv_inp_vect_[src_idx[k]]);
                bm::word_t* bit_blk = cache_gap_block(arg_blk, src_idx, k, i, j);
                if (bit_blk)
                {
                    ar_->v_arg_or_blk.push_back(bit_blk); // use cached bit-block for operation
                    continue;
                }
            } // bcache_ptr_
#endif
            ar_->v_arg_or_blk_gap.push_back(BMGAP_PTR(arg_blk));
        }
        else // FULL or bit block
        {
            if (!arg_blk)
                continue;
            if (arg_blk == FULL_BLOCK_FAKE_ADDR)
                return FULL_BLOCK_FAKE_ADDR;
            ar_->v_arg_or_blk.push_back(arg_blk);
        }
    } // for k
    return blk;
}

// ------------------------------------------------------------------------

template<typename BV>
bm::word_t* aggregator<BV>::sort_input_blocks_and(
                                const size_t* src_idx,
                                const bvector_type_const_ptr* bv_src,
                                size_t src_size,
                                unsigned i, unsigned j)
{
    ar_->v_arg_tmp_blk.resize_no_copy(src_size);
    auto blocks_arr = ar_->v_arg_tmp_blk.data();
    for (size_t k = 0; k < src_size; ++k)
    {
        const bm::word_t* arg_blk = blocks_arr[k] =
            bv_src[k]->get_blocks_manager().get_block_ptr(i, j);
        if (!arg_blk)
            return 0;
    }

    bool has_full_blk = false;
    bm::word_t* blk = FULL_BLOCK_FAKE_ADDR;
    auto& bit_v = ar_->v_arg_and_blk;
    auto& gap_v = ar_->v_arg_and_blk_gap;
    bit_v.resize_no_copy(src_size);
    gap_v.resize_no_copy(src_size);
    size_t bc(0), gc(0);

    auto bit_arr = bit_v.data();
    auto gap_arr = gap_v.data();
    for (size_t k = 0; k < src_size; ++k)
    {
        const bm::word_t* arg_blk = blocks_arr[k];
        if (BM_IS_GAP(arg_blk))
        {
            const bm::gap_word_t* gap_blk = BMGAP_PTR(arg_blk);
            (void)src_idx;
#if (0)
            if (bcache_ptr_)
            {
                unsigned len = bm::gap_length(gap_blk);
                size_t bv_idx = src_idx[k];
                auto cnt = bcache_ptr_->cnt_vect_[bv_idx];
                if (cnt && len > 1024 && bc < (src_size / 4))
                {
                    bm::word_t* bit_blk = cache_gap_block(arg_blk, src_idx, k, i, j);
                    if (bit_blk)
                    {
                        bit_arr[bc++] = bit_blk; // use cached bit-block for operation
                        continue;
                    }
                }
            } // bcache_ptr_
#endif
            gap_arr[gc++] = gap_blk;
            continue;
        }
        // FULL or bit block
        if (arg_blk == FULL_BLOCK_FAKE_ADDR)
        {
            has_full_blk = true;
            continue;
        }
        bit_arr[bc++] = arg_blk;

    } // for k

    bit_v.resize_no_copy(bc);
    gap_v.resize_no_copy(gc);

    if (has_full_blk && (!bc && !gc))
        bit_v.push_back(FULL_BLOCK_REAL_ADDR);

    return blk;
}

// ------------------------------------------------------------------------

template<typename BV>
bm::word_t* aggregator<BV>::cache_gap_block(const bm::word_t* arg_blk,
                            const size_t* src_idx,
                            size_t k,
                            unsigned i, unsigned j)
{
    BM_ASSERT(bcache_ptr_);
    BM_ASSERT(src_idx);

    size_t bv_idx = src_idx[k];
    auto cnt = bcache_ptr_->cnt_vect_[bv_idx];
    if (cnt > 0) // frequent bector
    {
        bm::word_t* bit_blk = bcache_ptr_->blk_vect_[bv_idx];
        bm::pair<unsigned, unsigned> pair_ij = bcache_ptr_->blk_ij_vect_[bv_idx];
        if (!bit_blk)
        {
            bit_blk = (bm::word_t*)bm::aligned_new_malloc(bm::set_block_size * sizeof(bm::word_t));
            pair_ij.first = i+1; // make it NOT match
            bcache_ptr_->blk_vect_[bv_idx] = bit_blk;
        }
        // block is allocated
        if (i != pair_ij.first || j != pair_ij.second) // not NB cached?
        {
            bm::gap_convert_to_bitset(bit_blk, BMGAP_PTR(arg_blk));
            pair_ij.first = i; pair_ij.second = j;
            bcache_ptr_->blk_ij_vect_[bv_idx] = pair_ij;
        }
        ++gap_cache_cnt_;
        return bit_blk; // use cached bit-block for operation
    }
    return 0;
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_or_horizontal(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src, size_t src_size)
{
    BM_ASSERT(src_size);
    if (src_size == 0)
    {
        bv_target.clear();
        return;
    }
    const bvector_type* bv = bv_src[0];
    bv_target = *bv;
    for (unsigned i = 1; i < src_size; ++i)
    {
        bv = bv_src[i];
        BM_ASSERT(bv);
        bv_target.bit_or(*bv);
    }
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and_horizontal(bvector_type& bv_target,
                     const bvector_type_const_ptr* bv_src, size_t src_size)
{
    BM_ASSERT(src_size);
    
    if (src_size == 0)
    {
        bv_target.clear();
        return;
    }
    const bvector_type* bv = bv_src[0];
    bv_target = *bv;
    
    for (unsigned i = 1; i < src_size; ++i)
    {
        bv = bv_src[i];
        BM_ASSERT(bv);
        bv_target.bit_and(*bv);
    }
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::combine_and_sub_horizontal(bvector_type& bv_target,
                                                const bvector_type_const_ptr* bv_src_and,
                                                size_t src_and_size,
                                                const bvector_type_const_ptr* bv_src_sub,
                                                size_t src_sub_size)
{
    BM_ASSERT(src_and_size);

    combine_and_horizontal(bv_target, bv_src_and, src_and_size);

    for (unsigned i = 0; i < src_sub_size; ++i)
    {
        const bvector_type* bv = bv_src_sub[i];
        BM_ASSERT(bv);
        bv_target -= *bv;
    }
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::prepare_shift_right_and(bvector_type& bv_target,
                                   const bvector_type_const_ptr* bv_src,
                                   size_t src_size)
{
    top_block_size_ = resize_target(bv_target, bv_src, src_size);

    // set initial carry overs all to 0
    ar_->carry_overs.resize(src_size);
    for (unsigned i = 0; i < src_size; ++i) // reset co flags
        ar_->carry_overs[i] = 0;
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_shift_right_and(
                bvector_type& bv_target,
                const bvector_type_const_ptr* bv_src_and, size_t src_and_size,
                bool any)
{
    if (!src_and_size)
    {
        bv_target.clear();
        return false;
    }
    prepare_shift_right_and(bv_target, bv_src_and, src_and_size);

    for (unsigned i = 0; i < bm::set_top_array_size; ++i)
    {
        if (i > top_block_size_)
        {
            if (!any_carry_overs(ar_->carry_overs.data(), src_and_size))
                break; // quit early if there is nothing to carry on
        }

        unsigned j = 0;
        do
        {
            bool found =
            combine_shift_right_and(i, j, bv_target, bv_src_and, src_and_size);
            if (found && any)
                return found;
        } while (++j < bm::set_sub_array_size);

    } // for i

    if (compute_count_)
        return bool(count_);

    return bv_target.any();
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::combine_shift_right_and(unsigned i, unsigned j,
                                             bvector_type& bv_target,
                                        const bvector_type_const_ptr* bv_src,
                                        size_t src_size)
{
    bm::word_t* blk = temp_blk_ ? temp_blk_ : tb_ar_->tb1;
    unsigned char* carry_overs = ar_->carry_overs.data();

    bm::id64_t digest = ~0ull; // start with a full content digest

    bool blk_zero = false;
    // first initial block is just copied over from the first AND
    {
        const blocks_manager_type& bman_arg = bv_src[0]->get_blocks_manager();
        BM_ASSERT(bman_arg.is_init());
        const bm::word_t* arg_blk = bman_arg.get_block(i, j);
        if (BM_IS_GAP(arg_blk))
        {
            bm::bit_block_set(blk, 0);
            bm::gap_add_to_bitset(blk, BMGAP_PTR(arg_blk));
        }
        else
        {
            if (arg_blk)
            {
                bm::bit_block_copy(blk, arg_blk);
            }
            else
            {
                blk_zero = true; // set flag for delayed block init
                digest = 0;
            }
        }
        carry_overs[0] = 0;
    }

    for (unsigned k = 1; k < src_size; ++k)
    {
        unsigned carry_over = carry_overs[k];
        if (!digest && !carry_over) // 0 into "00000" block >> 0
            continue;
        if (blk_zero) // delayed temp block 0-init requested
        {
            bm::bit_block_set(blk, 0);
            blk_zero = !blk_zero; // = false
        }
        const bm::word_t* arg_blk = get_arg_block(bv_src, k, i, j);
        carry_overs[k] = (unsigned char)
            process_shift_right_and(blk, arg_blk, digest, carry_over);
        BM_ASSERT(carry_overs[k] == 0 || carry_overs[k] == 1);
    } // for k

    if (blk_zero) // delayed temp block 0-init
    {
        bm::bit_block_set(blk, 0);
    }
    // block now gets emitted into the target bit-vector
    if (digest)
    {
        BM_ASSERT(!bm::bit_is_all_zero(blk));

        if (compute_count_)
        {
            unsigned cnt = bm::bit_block_count(blk, digest);
            count_ += cnt;
        }
        else
        {
            blocks_manager_type& bman_target = bv_target.get_blocks_manager();
            bman_target.opt_copy_bit_block(i, j, blk, opt_mode_, tb_ar_->tb_opt);
        }
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------

template<typename BV>
unsigned aggregator<BV>::process_shift_right_and(
                            bm::word_t*       BMRESTRICT blk,
                            const bm::word_t* BMRESTRICT arg_blk,
                            digest_type&      BMRESTRICT digest,
                            unsigned                     carry_over) BMNOEXCEPT
{
    BM_ASSERT(carry_over == 1 || carry_over == 0);

    if (BM_IS_GAP(arg_blk)) // GAP argument
    {
        if (digest)
        {
            carry_over =
                bm::bit_block_shift_r1_and_unr(blk, carry_over,
                                            FULL_BLOCK_REAL_ADDR, &digest);
        }
        else // digest == 0, but carry_over can change that
        {
            blk[0] = carry_over;
            carry_over = 0;
            digest = blk[0]; // NOTE: this does NOT account for AND (yet)
        }
        BM_ASSERT(bm::calc_block_digest0(blk) == digest);

        bm::gap_and_to_bitset(blk, BMGAP_PTR(arg_blk), digest);
        digest = bm::update_block_digest0(blk, digest);
    }
    else // 2 bit-blocks
    {
        if (arg_blk) // use fast fused SHIFT-AND operation
        {
            if (digest)
            {
                carry_over =
                    bm::bit_block_shift_r1_and_unr(blk, carry_over, arg_blk,
                                                   &digest);
            }
            else // digest == 0
            {
                blk[0] = carry_over & arg_blk[0];
                carry_over = 0;
                digest = blk[0];
            }
            BM_ASSERT(bm::calc_block_digest0(blk) == digest);
        }
        else  // arg is zero - target block => zero
        {
            carry_over = blk[bm::set_block_size-1] >> 31; // carry out
            if (digest)
            {
                bm::bit_block_set(blk, 0);  // TODO: digest based set
                digest = 0;
            }
        }
    }
    return carry_over;
}

// ------------------------------------------------------------------------

template<typename BV>
const bm::word_t* aggregator<BV>::get_arg_block(
                                const bvector_type_const_ptr* bv_src,
                                unsigned k, unsigned i, unsigned j) BMNOEXCEPT
{
    return bv_src[k]->get_blocks_manager().get_block(i, j);
}

// ------------------------------------------------------------------------

template<typename BV>
bool aggregator<BV>::any_carry_overs(const unsigned char* carry_overs,
                                     size_t co_size)  BMNOEXCEPT
{
    // TODO: loop unroll?
    unsigned acc = carry_overs[0];
    for (size_t i = 1; i < co_size; ++i)
        acc |= carry_overs[i];
    return acc;
}

// ------------------------------------------------------------------------

template<typename BV>
void aggregator<BV>::stage(bm::word_t* temp_block)
{
    bvector_type* bv_target = check_create_target(); // create target vector
    BM_ASSERT(bv_target);
    
    temp_blk_ = temp_block;

    switch (operation_)
    {
    case BM_NOT_DEFINED:
        break;
    case BM_SHIFT_R_AND:
        prepare_shift_right_and(*bv_target, ag_.arg_bv0.data(), ag_.arg_bv0.size());//arg_group0_size);
        operation_status_ = op_prepared;
        break;
    default:
        BM_ASSERT(0);
    } // switch
}

// ------------------------------------------------------------------------

template<typename BV>
typename aggregator<BV>::operation_status
aggregator<BV>::run_step(unsigned i, unsigned j)
{
    BM_ASSERT(operation_status_ == op_prepared || operation_status_ == op_in_progress);
    BM_ASSERT(j < bm::set_sub_array_size);
    
    switch (operation_)
    {
    case BM_NOT_DEFINED:
        break;
        
    case BM_SHIFT_R_AND:
        {
        if (i > top_block_size_)
        {
            if (!this->any_carry_overs(ar_->carry_overs.data(), ag_.arg_bv0.size()))//arg_group0_size))
            {
                operation_status_ = op_done;
                return operation_status_;
            }
        }
        //bool found =
           this->combine_shift_right_and(i, j, *bv_target_,
                                        ag_.arg_bv0.data(), ag_.arg_bv0.size());//arg_group0_size);
        operation_status_ = op_in_progress;
        }
        break;
    default:
        BM_ASSERT(0);
        break;
    } // switch
    
    return operation_status_;
}


// ------------------------------------------------------------------------
// aggregator::pipeline
// ------------------------------------------------------------------------

template<typename BV> template<class Opt>
aggregator<BV>::pipeline<Opt>::~pipeline() BMNOEXCEPT
{
    size_t sz = arg_vect_.size();
    arg_groups** arr = arg_vect_.data();
    for (size_t i = 0; i < sz; ++i)
        free_arg_group(arr[i]);
    sz = bv_res_vect_.size();
    bvector_type** bv_arr = bv_res_vect_.data();
    for (size_t i = 0; i < sz; ++i)
    {
        bvector_type* bv = bv_arr[i];
        delete bv;
    } // for i
    sz = bcache_.blk_vect_.size();
    bm::word_t** blk_arr = bcache_.blk_vect_.data();
    for (size_t i = 0; i < sz; ++i)
        bm::aligned_free(blk_arr[i]);
}

// ------------------------------------------------------------------------

template<typename BV> template<class Opt>
void aggregator<BV>::pipeline<Opt>::complete()
{
    if (is_complete_)
        return;

    if constexpr (Opt::is_make_results())
    {
        BM_ASSERT(!bv_res_vect_.size());
        size_t sz = arg_vect_.size();
        bv_res_vect_.resize(sz);
        bvector_type** bv_arr = bv_res_vect_.data();
        for (size_t i = 0; i < sz; ++i)
            bv_arr[i] = 0;
    }
    if constexpr (Opt::is_compute_counts())
    {
        size_t sz = arg_vect_.size();
        count_res_vect_.resize(sz);
        size_type* cnt_arr = count_res_vect_.data();
        ::memset(cnt_arr, 0, sz * sizeof(cnt_arr[0]));
    }

    const arg_vector_type& pipe_args = get_args_vector();
    size_t pipe_size = pipe_args.size();

    for (size_t p = 0; p < pipe_size; ++p)
    {
        arg_groups* ag = pipe_args[p];
        complete_arg_group(ag);

        const bvector_type_const_ptr* bv_src_and = ag->arg_bv0.data();
        size_t src_and_size = ag->arg_bv0.size();
        unsigned top_blocks1 = max_top_blocks(bv_src_and, src_and_size);
        if (top_blocks1 > top_blocks_)
            top_blocks_ = top_blocks1;

        const bvector_type_const_ptr* bv_src_sub = ag->arg_bv1.data();
        size_t src_sub_size = ag->arg_bv1.size();
        unsigned top_blocks2 = max_top_blocks(bv_src_sub, src_sub_size);
        if (top_blocks2 > top_blocks_)
            top_blocks_ = top_blocks2;

    } // for p
    is_complete_ = true;

    BM_ASSERT(bcache_.bv_inp_vect_.size() == bcache_.cnt_vect_.size());
    BM_ASSERT(bcache_.bv_inp_vect_.size() == bcache_.blk_vect_.size());
    BM_ASSERT(bcache_.bv_inp_vect_.size() == bcache_.blk_ij_vect_.size());
}

// ------------------------------------------------------------------------

template<typename BV> template<class Opt>
void aggregator<BV>::pipeline<Opt>::complete_arg_group(arg_groups* ag)
{
    BM_ASSERT(ag);
    auto sz = ag->arg_bv0.size();
    total_vect_ += sz;
    complete_arg_sub_group(ag->arg_idx0, ag->arg_bv0.data(), sz);
    sz = ag->arg_bv1.size();
    total_vect_ += sz;
    complete_arg_sub_group(ag->arg_idx1, ag->arg_bv1.data(), sz);
}

// ------------------------------------------------------------------------

template<typename BV> template<class Opt>
void aggregator<BV>::pipeline<Opt>::complete_arg_sub_group(
                index_vector_type& idx_vect,
                const bvector_type_const_ptr* bv_src, size_t size)
{
    BM_ASSERT(idx_vect.size() == 0);

    for (size_t k = 0; k < size; ++k)
    {
        bool found(false); size_t bv_idx(0);
        const bvector_type* bv = bv_src[k];
        if (bv)
        {
            const bvector_type** bv_arr = bcache_.bv_inp_vect_.data();
            found =
                bm::find_ptr((void**)bv_arr, bcache_.bv_inp_vect_.size(),
                             bv, &bv_idx);
        }
        if (found)
            bcache_.cnt_vect_[bv_idx]++; // increment vector usage counter
        else // not found (new one!)
        {
            bv_idx = bcache_.bv_inp_vect_.size();
            bcache_.bv_inp_vect_.push_back(bv); // register a new bv (0-cnt)
            bcache_.cnt_vect_.push_back(0);
            bcache_.blk_vect_.push_back(0); // NULL ptr
            bcache_.blk_ij_vect_.push_back(bm::pair(0u, 0u));
        }
        // each arg group
        idx_vect.push_back(bv_idx);
    } // for k

    BM_ASSERT(idx_vect.size() == size);
}

// ------------------------------------------------------------------------

template<typename BV> template<class Opt>
typename aggregator<BV>::arg_groups*
aggregator<BV>::pipeline<Opt>::add()
{
    BM_ASSERT(!is_complete_);
    arg_groups* arg = construct_arg_group();
    arg_vect_.push_back(arg);
    return arg;
}

// ------------------------------------------------------------------------

template<typename BV> template<class Opt>
size_t aggregator<BV>::pipeline<Opt>::compute_run_batch() const BMNOEXCEPT
{
    const size_t cache_size = 256 * 1024; // estimation-speculation (L2)
    // ad-hoc estimate (not correct!) uses 2/3 of bit-vectors size (some are GAPs)
    // TODO: need to use real sparse vector statistics (AVG block size)
    //
    const float block_size = 0.75f * (bm::set_block_size * sizeof(bm::word_t));
    const float bv_struct_overhead = (64 * 8); // 8 cache lines in bytes
    const float cached_vect = float(cache_size) / float(block_size + bv_struct_overhead);

    size_t bv_count = unique_vectors();
    size_t args_total = arg_vect_.size(); // number of arg groups
    if ((bv_count < cached_vect) || (args_total < 2)) // worst case fit in L2
        return args_total;

    size_t avg_vect_per_group = total_vect_ / args_total;

    const float reuse_coeff = 0.7f; // spec. coeff of resue of vectors
    float f_batch_size =
        (1+reuse_coeff)*(float(avg_vect_per_group) / float(cached_vect) + 0.99f);
    size_t batch_size = size_t(f_batch_size);

    return batch_size;
}


// ------------------------------------------------------------------------
// Arg Groups
// ------------------------------------------------------------------------

template<typename BV>
size_t aggregator<BV>::arg_groups::add(const bvector_type* bv,
                                       unsigned agr_group)
{
    BM_ASSERT_THROW(agr_group <= 1, BM_ERR_RANGE);
    BM_ASSERT(agr_group <= 1);
    switch (agr_group)
    {
    case 0:
        if (!bv)
            return arg_bv0.size();
        arg_bv0.push_back(bv);
        return arg_bv0.size();
    case 1:
        if (!bv)
            return arg_bv1.size();
        arg_bv1.push_back(bv);
        return arg_bv1.size();
    default:
        BM_ASSERT(0);
    }
    return 0;
}

// ------------------------------------------------------------------------


} // bm


#endif
