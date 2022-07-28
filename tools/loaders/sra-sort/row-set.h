/*===========================================================================
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
 */

#ifndef _h_sra_sort_row_set_
#define _h_sra_sort_row_set_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct MapFile;
struct TablePair;
struct IdxMapping;


/*--------------------------------------------------------------------------
 * RowSet
 *  interface to iterate row-ids
 */
typedef struct RowSet_vt RowSet_vt;

typedef struct RowSet RowSet;
struct RowSet
{
    const RowSet_vt *vt;
    KRefcount refcount;
    uint32_t align;
};

#ifndef ROWSET_IMPL
#define ROWSET_IMPL RowSet
#endif

struct RowSet_vt
{
    /* called by Release */
    void ( * whack ) ( ROWSET_IMPL *self, const ctx_t *ctx );

    /* retrieve next set of row-ids */
    size_t ( * next ) ( ROWSET_IMPL *self, const ctx_t *ctx,
        int64_t *ids, size_t max_ids );

    /* reset iterator to initial state */
    void ( * reset ) ( ROWSET_IMPL *self, const ctx_t *ctx,
        bool for_static );
};


/* Release
 *  releases reference
 */
void RowSetRelease ( const RowSet *self, const ctx_t *ctx );

/* Duplicate
 */
RowSet *RowSetDuplicate ( const RowSet *self, const ctx_t *ctx );


/* Next
 *  return next set of row-ids
 *  returns 0 if no rows are available
 */
#define RowSetNext( self, ctx, ids, max_ids ) \
    POLY_DISPATCH_INT ( next, self, ROWSET_IMPL, ctx, ids, max_ids )


/* Reset
 *  reset iterator to beginning
 */
#define RowSetReset( self, ctx, for_static ) \
    POLY_DISPATCH_VOID ( reset, self, ROWSET_IMPL, ctx, for_static )


/* Init
 */
void RowSetInit ( RowSet *self, const ctx_t *ctx, const RowSet_vt *vt );

/* Destroy
 */
#define RowSetDestroy( self, ctx ) \
    ( ( void ) 0 )


/*--------------------------------------------------------------------------
 * RowSetIterator
 *  interface to iterate RowSets
 */
typedef struct RowSetIterator_vt RowSetIterator_vt;

typedef struct RowSetIterator RowSetIterator;
struct RowSetIterator
{
    const RowSetIterator_vt *vt;
    KRefcount refcount;
    uint32_t align;
};

#ifndef ROWSET_ITER_IMPL
#define ROWSET_ITER_IMPL RowSetIterator
#endif

struct RowSetIterator_vt
{
    /* called by Release */
    void ( * whack ) ( ROWSET_ITER_IMPL *self, const ctx_t *ctx );

    /* retrieve row-id mapping pair */
    struct IdxMapping* ( * get_idx_mapping ) ( ROWSET_ITER_IMPL *self, const ctx_t *ctx,
        size_t *num_items );

    /* retrieve src row-id array */
    int64_t* ( * get_src_ids ) ( ROWSET_ITER_IMPL *self, const ctx_t *ctx,
        uint32_t **opt_ord, size_t *num_items );

    /* retrieve next row-set */
    RowSet* ( * next ) ( ROWSET_ITER_IMPL *self, const ctx_t *ctx );
};


/* MakeRowSetIterator
 *  make RowSet iterator based upon parameters
 *  each RowSet will generate row-ids in src order
 *
 *  "sort_idx" [ IN ]
 *    if NULL, the RowSet objects will auto-generate row-ids
 *    otherwise, src row-ids will be selected from MapFile
 *
 *  "pairs" [ IN ]
 *    if true, the row-ids will be generated into an IdxMapping pair
 *    if false but "idx" is not NULL, the src row-ids will be generated
 *      into a single-id map, and new ordering preserved in parallel
 *    if false and "idx" is NULL, no additional structure will be generated
 *
 *  "large" [ IN ]
 *    if true, use a reduced id-range for the row-set
 */
RowSetIterator *TablePairMakeRowSetIterator ( struct TablePair *self, const ctx_t *ctx,
    struct MapFile const *sort_idx, bool pairs, bool large );


/* Release
 *  releases reference
 */
void RowSetIteratorRelease ( const RowSetIterator *self, const ctx_t *ctx );

/* Duplicate
 */
RowSetIterator *RowSetIteratorDuplicate ( const RowSetIterator *self, const ctx_t *ctx );


/* GetIdxMapping
 *  a little bit of plumbing to get the IdxMapping map
 */
#define RowSetIteratorGetIdxMapping( self, ctx, num_items ) \
    POLY_DISPATCH_PTR ( get_idx_mapping, self, ROWSET_ITER_IMPL, ctx, num_items )


/* GetSourceIds
 *  a little bit of plumbing to get the individual source ids
 *  and optional new id ordering
 */
#define RowSetIteratorGetSourceIds( self, ctx, opt_ord, num_items ) \
    POLY_DISPATCH_PTR ( get_src_ids, self, ROWSET_ITER_IMPL, ctx, opt_ord, num_items )


/* Next
 *  return next RowSet
 *  returns NULL if no rows are available
 */
#define RowSetIteratorNext( self, ctx ) \
    POLY_DISPATCH_PTR ( next, self, ROWSET_ITER_IMPL, ctx )


/* Init
 */
void RowSetIteratorInit ( RowSetIterator *self, const ctx_t *ctx, const RowSetIterator_vt *vt );

/* Destroy
 */
#define RowSetIteratorDestroy( self, ctx ) \
    ( ( void ) 0 )


#endif /* _h_sra_sort_row_set_ */
