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

typedef struct SortingRowSet SortingRowSet;
#define ROWSET_IMPL SortingRowSet

typedef struct SortingRowSetIterator SortingRowSetIterator;
#define ROWSET_ITER_IMPL SortingRowSetIterator

#include "row-set-priv.h"
#include "tbl-pair.h"
#include "map-file.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <klib/rc.h>

#include <string.h>

FILE_ENTRY ( sorting-row-set );


/*--------------------------------------------------------------------------
 * SortingRowSetIterator
 *  interface to iterate RowSets
 */
struct SortingRowSetIterator
{
    RowSetIterator dad;

    /* base row-id */
    int64_t row_id;

    /* last row-id + 1 */
    int64_t last_excl;

    /* takes its input from new=>old index */
    const MapFile *idx;

    /* working data within map */
    int64_t *src_ids;
    uint32_t *new_ord;
    size_t max_elems;
    size_t num_elems;

    /* if true, use setting for large-columns */
    bool large;

    /* if true, no need to regenerate new_ord */
    bool new_ord_valid;
};

/*--------------------------------------------------------------------------
 * SortingRowSet
 *  implementation of RowSet based upon auto-generated id pairs
 */
struct SortingRowSet
{
    RowSet dad;

    /* keep-alive reference */
    SortingRowSetIterator *iter;

    /* working data within map */
    int64_t *src_ids;
    size_t num_elems;
    size_t cur_elem;
};

static
void SortingRowSetWhack ( SortingRowSet *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    RowSetIteratorRelease ( & self -> iter -> dad, ctx );
    MemFree ( ctx, self, sizeof *self );
}

static
size_t SortingRowSetNextStat ( SortingRowSet *self, const ctx_t *ctx,
    int64_t *ids, size_t max_ids )
{
    FUNC_ENTRY ( ctx );

    /* the starting row-id is taken from row-set-iterator
       and offset by our current element counter */
    int64_t row_id = self -> iter -> row_id + self -> cur_elem;

    /* limit id generation to request */
    size_t i, to_set = self -> num_elems - self -> cur_elem;
    if ( to_set > max_ids )
        to_set = max_ids;

    /* fill request with serial numbers
       could just as easily be the same number
       the important thing is to limit the count
       and produce the correct range */
    for ( i = 0; i < to_set; ++ i )
        ids [ i ] = row_id + i;

    /* advance counter */
    self -> cur_elem += to_set;
    return to_set;
}

static
size_t SortingRowSetNextPhys ( SortingRowSet *self, const ctx_t *ctx,
    int64_t *ids, size_t max_ids )
{
    FUNC_ENTRY ( ctx );

    /* limit copy to request */
    size_t to_copy = self -> num_elems - self -> cur_elem;
    if ( to_copy > max_ids )
        to_copy = max_ids;

    /* copy out old-ids */
    memcpy ( ids, & self -> src_ids [ self -> cur_elem ], to_copy * sizeof ids [ 0 ] );

    /* advance counter */
    self -> cur_elem += to_copy;
    return to_copy;
}

static
void SortingRowSetReset ( SortingRowSet *self, const ctx_t *ctx, bool for_static );

static RowSet_vt SortingRowSetPhys_vt =
{
    SortingRowSetWhack,
    SortingRowSetNextPhys,
    SortingRowSetReset
};

static RowSet_vt SortingRowSetStat_vt =
{
    SortingRowSetWhack,
    SortingRowSetNextStat,
    SortingRowSetReset
};

static
void SortingRowSetReset ( SortingRowSet *self, const ctx_t *ctx, bool for_static )
{
    FUNC_ENTRY ( ctx );

    if ( for_static )
    {
        /* go static */
        self -> dad . vt = & SortingRowSetStat_vt;
    }
    else
    {
        SortingRowSetIterator *iter = self -> iter;

        /* go physical */
        self -> dad . vt = & SortingRowSetPhys_vt;

        if ( iter -> new_ord_valid )
        {
            /* read new-ids from file, selecting old-ids within range */
            STATUS ( 3, "selecting first %,zu ids from old=>new where new_id >= %,ld",
                     iter -> max_elems, iter -> row_id );
            iter -> num_elems = MapFileSelectOldToNewSingle ( iter -> idx, ctx,
                iter -> row_id, iter -> src_ids, NULL, iter -> max_elems );
        }
        else
        {
            /* read new-ids from file, init new-ord, selecting old-ids within range */
            STATUS ( 3, "selecting first %,zu ids, new_ord from old=>new where new_id >= %,ld",
                     iter -> max_elems, iter -> row_id );
            TRY ( iter -> num_elems = MapFileSelectOldToNewSingle ( iter -> idx, ctx,
                      iter -> row_id, iter -> src_ids, iter -> new_ord, iter -> max_elems ) )
            {
                iter -> new_ord_valid = true;
            }
        }

        if ( FAILED () )
            return;

        self -> num_elems = iter -> num_elems;
        STATUS ( 3, "selected %,zu ids", self -> num_elems );
    }

    /* ready */
    self -> cur_elem = 0;
}


/*--------------------------------------------------------------------------
 * SortingRowSetIterator
 *  interface to iterate RowSets
 */

static
void SortingRowSetIteratorWhack ( SortingRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> src_ids != NULL )
        MemFree ( ctx, self -> src_ids, sizeof self -> src_ids [ 0 ] * self -> max_elems );
    if ( self -> new_ord != NULL )
        MemFree ( ctx, self -> new_ord, sizeof self -> new_ord [ 0 ] * self -> max_elems );

    MapFileRelease ( self -> idx, ctx );

    RowSetIteratorDestroy ( & self -> dad, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
RowSet *SortingRowSetIteratorMakeTheRowSet ( SortingRowSetIterator *self, const ctx_t *ctx, const RowSet_vt *vt )
{
    FUNC_ENTRY ( ctx );

    SortingRowSet *rs;
    TRY ( rs = MemAlloc ( ctx, sizeof * rs, false ) )
    {
        TRY ( RowSetInit ( & rs -> dad, ctx, vt ) )
        {
            /* initialize the row-set with map and self reference */
            rs -> src_ids = self -> src_ids;
            rs -> iter = ( SortingRowSetIterator* ) RowSetIteratorDuplicate ( & self -> dad, ctx );
            rs -> num_elems = self -> num_elems;
            rs -> cur_elem = 0;
            return & rs -> dad;
        }

        MemFree ( ctx, rs, sizeof * rs );
    }

    return NULL;
 }

static
struct IdxMapping *SortingRowSetIteratorGetIdxMapping ( SortingRowSetIterator *self, const ctx_t *ctx, size_t *num_items )
{
    FUNC_ENTRY ( ctx );

    ANNOTATE ( "CALLING GetIdxMapping ON SORTING ROWSET ITERATOR" );

    if ( num_items != NULL )
        * num_items = 0;

    return NULL;
}

static
int64_t *SortingRowSetIteratorGetSourceIds ( SortingRowSetIterator *self, const ctx_t *ctx, uint32_t **opt_ord, size_t *num_items )
{
    FUNC_ENTRY ( ctx );

    if ( opt_ord != NULL )
        * opt_ord = self -> new_ord;

    if ( self -> src_ids == NULL )
    {
        rc_t rc = RC ( rcExe, rcCursor, rcAccessing, rcSelf, rcInconsistent );
        ERROR ( rc, "source id array is NULL" );
        if ( num_items != NULL )
            * num_items = 0;
        return NULL;
    }

    if ( num_items != NULL )
        * num_items = self -> num_elems;
    return self -> src_ids;
}

static
void SortingRowSetIteratorAllocMap ( SortingRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* determine the maximum number of elements */
    const Tool *tp = ctx -> caps -> tool;
    uint64_t count = self -> last_excl - self -> row_id;
    self -> max_elems = self -> large ? tp -> max_large_idx_ids : tp -> max_idx_ids;
    if ( ( uint64_t ) self -> max_elems > count )
        self -> max_elems = ( size_t ) count;

    /* try to allocate the memory
       this may be limited by the MemBank */
    do
    {
        CLEAR ();

        TRY ( self -> src_ids = MemAlloc ( ctx, sizeof self -> src_ids [ 0 ] * self -> max_elems, false ) )
        {
            TRY ( self -> new_ord = MemAlloc ( ctx, sizeof self -> new_ord [ 0 ] * self -> max_elems, false ) )
            {
                STATUS ( 4, "allocated mapping array of %,u elements", self -> max_elems );
                return;
            }

            MemFree ( ctx, self -> src_ids, sizeof self -> src_ids [ 0 ] * self -> max_elems );
            self -> src_ids = NULL;
        }

        self -> max_elems >>= 1;
    }
    while ( self -> max_elems >= ctx -> caps -> tool -> min_idx_ids );

    ANNOTATE ( "failed to allocate map for row-set" );
}

static
RowSet *SortingRowSetIteratorNext ( SortingRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    RowSet *rs = NULL;

    /* on first invocation, allocate the map */
    if ( self -> src_ids == NULL )
    {
        ON_FAIL ( SortingRowSetIteratorAllocMap ( self, ctx ) )
            return NULL;

        assert ( self -> src_ids != NULL );
        assert ( self -> new_ord != NULL );
    }

    /* advance row_id */
    self -> row_id += self -> num_elems;
    self -> num_elems = 0;
    self -> new_ord_valid = false;

    /* create the empty row-set */
    if ( self -> row_id < self -> last_excl )
        rs = SortingRowSetIteratorMakeTheRowSet ( self, ctx, & SortingRowSetPhys_vt );

    return rs;
}


/*--------------------------------------------------------------------------
 * MapFile
 */

static RowSetIterator_vt SortingRowSetIterator_vt =
{
    SortingRowSetIteratorWhack,
    SortingRowSetIteratorGetIdxMapping,
    SortingRowSetIteratorGetSourceIds,
    SortingRowSetIteratorNext
};

RowSetIterator * MapFileMakeSortingRowSetIterator ( const MapFile *self, const ctx_t *ctx, bool large )
{
    FUNC_ENTRY ( ctx );

    SortingRowSetIterator *rsi;

    /* create the iterator */
    TRY ( rsi = MemAlloc ( ctx, sizeof * rsi, true ) )
    {
        TRY ( RowSetIteratorInit ( & rsi -> dad, ctx, & SortingRowSetIterator_vt ) )
        {
            /* initialize the row-set with map and self reference */
            TRY ( rsi -> row_id = MapFileFirst ( self, ctx ) )
            {
                TRY ( rsi -> last_excl = rsi -> row_id + MapFileCount ( self, ctx ) )
                {
                    TRY ( rsi -> idx = MapFileDuplicate ( self, ctx ) )
                    {
                        rsi -> large = large;
                        return & rsi -> dad;
                    }
                }
            }
        }

        MemFree ( ctx, rsi, sizeof * rsi );
    }

    return NULL;
}
