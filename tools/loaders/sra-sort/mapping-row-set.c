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

typedef struct MappingRowSet MappingRowSet;
#define ROWSET_IMPL MappingRowSet

typedef struct MappingRowSetIterator MappingRowSetIterator;
#define ROWSET_ITER_IMPL MappingRowSetIterator

#include "row-set-priv.h"
#include "tbl-pair.h"
#include "idx-mapping.h"
#include "map-file.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <klib/rc.h>

FILE_ENTRY ( mapping-row-set );


/*--------------------------------------------------------------------------
 * MappingRowSetIterator
 *  interface to iterate RowSets
 */
struct MappingRowSetIterator
{
    RowSetIterator dad;

    /* base row-id */
    int64_t row_id;

    /* last row-id + 1 */
    int64_t last_excl;

    /* takes its input from new=>old index */
    const MapFile *idx;

    /* working data within map */
    IdxMapping *map;
    size_t max_elems;
    size_t num_elems;

    bool large;
};

/*--------------------------------------------------------------------------
 * MappingRowSet
 *  implementation of RowSet based upon auto-generated id pairs
 */
struct MappingRowSet
{
    RowSet dad;

    /* keep-alive reference */
    MappingRowSetIterator *iter;

    /* working data within map */
    IdxMapping *map;
    size_t num_elems;
    size_t cur_elem;
};

static
void MappingRowSetWhack ( MappingRowSet *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    RowSetIteratorRelease ( & self -> iter -> dad, ctx );
    MemFree ( ctx, self, sizeof *self );
}

static
size_t MappingRowSetNextStat ( MappingRowSet *self, const ctx_t *ctx,
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
size_t MappingRowSetNextPhys ( MappingRowSet *self, const ctx_t *ctx,
    int64_t *ids, size_t max_ids )
{
    FUNC_ENTRY ( ctx );

    /* limit copy to request */
    size_t i, to_copy = self -> num_elems - self -> cur_elem;
    if ( to_copy > max_ids )
        to_copy = max_ids;

    /* copy out old-ids */
    for ( i = 0; i < to_copy; ++ i )
        ids [ i ] = self -> map [ self -> cur_elem + i ] . old_id;

    /* advance counter */
    self -> cur_elem += to_copy;
    return to_copy;
}

static
void MappingRowSetReset ( MappingRowSet *self, const ctx_t *ctx, bool for_static );

static RowSet_vt MappingRowSetPhys_vt =
{
    MappingRowSetWhack,
    MappingRowSetNextPhys,
    MappingRowSetReset
};

static RowSet_vt MappingRowSetStat_vt =
{
    MappingRowSetWhack,
    MappingRowSetNextStat,
    MappingRowSetReset
};

static
void MappingRowSetReset ( MappingRowSet *self, const ctx_t *ctx, bool for_static )
{
    FUNC_ENTRY ( ctx );

    if ( for_static )
    {
        /* go static */
        self -> dad . vt = & MappingRowSetStat_vt;
    }
    else
    {
        size_t i, max_elems;
        MappingRowSetIterator *iter = self -> iter;

        /* go physical */
        self -> dad . vt = & MappingRowSetPhys_vt;

        /* limit id generation */
        max_elems = iter -> max_elems;
        if ( iter -> row_id + max_elems >= iter -> last_excl )
            max_elems = ( size_t ) ( iter -> last_excl - iter -> row_id );

        STATUS ( 4, "auto-generating %,zu ( old_id, new_id ) pairs where new_id >= %ld",
                 max_elems, iter -> row_id );
        for ( i = 0; i < max_elems; ++ i )
            iter -> map [ i ] . old_id = iter -> map [ i ] . new_id = iter -> row_id + i;

        self -> num_elems = iter -> num_elems = max_elems;
    }

    /* ready */
    self -> cur_elem = 0;
}

static
void MapFileMappingRowSetReset ( MappingRowSet *self, const ctx_t *ctx, bool for_static );

static RowSet_vt MapFileMappingRowSetPhys_vt =
{
    MappingRowSetWhack,
    MappingRowSetNextPhys,
    MapFileMappingRowSetReset
};

static RowSet_vt MapFileMappingRowSetStat_vt =
{
    MappingRowSetWhack,
    MappingRowSetNextStat,
    MapFileMappingRowSetReset
};

static
void MapFileMappingRowSetReset ( MappingRowSet *self, const ctx_t *ctx, bool for_static )
{
    FUNC_ENTRY ( ctx );

    if ( for_static )
    {
        /* go static */
        self -> dad . vt = & MapFileMappingRowSetStat_vt;
    }
    else
    {
        MappingRowSetIterator *iter = self -> iter;

        /* go physical */
        self -> dad . vt = & MapFileMappingRowSetPhys_vt;

        /* read new-ids from file, selecting only those within range */
        STATUS ( 3, "selecting first %,zu ( old_id, new_id ) pairs from old=>new where new_id >= %,ld",
                 iter -> max_elems, iter -> row_id );
        ON_FAIL ( iter -> num_elems = MapFileSelectOldToNewPairs ( iter -> idx, ctx,
                      iter -> row_id, iter -> map, iter -> max_elems ) )
            return;

        self -> num_elems = iter -> num_elems;
        STATUS ( 3, "selected %,zu ( old_id, new_id ) pairs", self -> num_elems );
    }

    /* ready */
    self -> cur_elem = 0;
}


/*--------------------------------------------------------------------------
 * MappingRowSetIterator
 *  interface to iterate RowSets
 */

static
void MappingRowSetIteratorWhack ( MappingRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> map != NULL )
        MemFree ( ctx, self -> map, sizeof self -> map [ 0 ] * self -> max_elems );

    RowSetIteratorDestroy ( & self -> dad, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
void MapFileMappingRowSetIteratorWhack ( MappingRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> map != NULL )
        MemFree ( ctx, self -> map, sizeof self -> map [ 0 ] * self -> max_elems );

    MapFileRelease ( self -> idx, ctx );

    RowSetIteratorDestroy ( & self -> dad, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
RowSet *MappingRowSetIteratorMakeTheRowSet ( MappingRowSetIterator *self, const ctx_t *ctx, const RowSet_vt *vt )
{
    FUNC_ENTRY ( ctx );

    MappingRowSet *rs;
    TRY ( rs = MemAlloc ( ctx, sizeof * rs, false ) )
    {
        TRY ( RowSetInit ( & rs -> dad, ctx, vt ) )
        {
            /* initialize the row-set with map and self reference */
            rs -> map = self -> map;
            rs -> iter = ( MappingRowSetIterator* ) RowSetIteratorDuplicate ( & self -> dad, ctx );
            rs -> num_elems = self -> num_elems;
            rs -> cur_elem = 0;
            return & rs -> dad;
        }

        MemFree ( ctx, rs, sizeof * rs );
    }

    return NULL;
 }

static
struct IdxMapping *MappingRowSetIteratorGetIdxMapping ( MappingRowSetIterator *self, const ctx_t *ctx, size_t *num_items )
{
    FUNC_ENTRY ( ctx );

    if ( self -> map == NULL )
    {
        rc_t rc = RC ( rcExe, rcCursor, rcAccessing, rcSelf, rcInconsistent );
        ERROR ( rc, "IdxMapping array is NULL" );
        if ( num_items != NULL )
            * num_items = 0;
        return NULL;
    }

    if ( num_items != NULL )
        * num_items = self -> num_elems;
    return self -> map;
}

static
int64_t *MappingRowSetIteratorGetSourceIds ( MappingRowSetIterator *self, const ctx_t *ctx, uint32_t **opt_ord, size_t *num_items )
{
    FUNC_ENTRY ( ctx );

    ANNOTATE ( "CALLING GetSourceIds ON MAPPING ROWSET ITERATOR" );

    if ( opt_ord != NULL )
        * opt_ord = NULL;

    if ( num_items != NULL )
        * num_items = 0;

    return NULL;
}

static
void MappingRowSetIteratorAllocMap ( MappingRowSetIterator *self, const ctx_t *ctx )
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

        TRY ( self -> map = MemAlloc ( ctx, sizeof self -> map [ 0 ] * self -> max_elems, false ) )
        {
            STATUS ( 4, "allocated mapping array of %,u elements", self -> max_elems );
            return;
        }

        self -> max_elems >>= 1;
    }
    while ( self -> max_elems >= ctx -> caps -> tool -> min_idx_ids );

    ANNOTATE ( "failed to allocate map for row-set" );
}

static
RowSet *MappingRowSetIteratorNextInt ( MappingRowSetIterator *self, const ctx_t *ctx, const RowSet_vt *vt )
{
    /* no FUNC_ENTRY */

    RowSet *rs = NULL;

    /* on first invocation, allocate the map */
    if ( self -> map == NULL )
    {
        ON_FAIL ( MappingRowSetIteratorAllocMap ( self, ctx ) )
            return NULL;
    }

    /* advance row_id */
    self -> row_id += self -> num_elems;
    self -> num_elems = 0;

    /* create the empty row-set */
    if ( self -> row_id < self -> last_excl )
        rs = MappingRowSetIteratorMakeTheRowSet ( self, ctx, vt );

    return rs;
}

static
RowSet *MappingRowSetIteratorNext ( MappingRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    return MappingRowSetIteratorNextInt ( self, ctx, & MappingRowSetPhys_vt );
}

static
RowSet *MapFileMappingRowSetIteratorNext ( MappingRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    return MappingRowSetIteratorNextInt ( self, ctx, & MapFileMappingRowSetPhys_vt );
}


/*--------------------------------------------------------------------------
 * TablePair
 */

static RowSetIterator_vt MappingRowSetIterator_vt =
{
    MappingRowSetIteratorWhack,
    MappingRowSetIteratorGetIdxMapping,
    MappingRowSetIteratorGetSourceIds,
    MappingRowSetIteratorNext
};

/* MakeMappingRowSetIterator
 *  runs from first to last id in cursor
 */
RowSetIterator *TablePairMakeMappingRowSetIterator ( TablePair *self, const ctx_t *ctx, bool large )
{
    FUNC_ENTRY ( ctx );

    if ( self -> first_id > self -> last_excl )
    {
        rc_t rc = RC ( rcExe, rcCursor, rcConstructing, rcRange, rcInvalid );
        INTERNAL_ERROR ( rc, "union of all column ranges improperly set" );
    }
    else
    {
        MappingRowSetIterator *rsi;

        TRY ( rsi = MemAlloc ( ctx, sizeof * rsi, true ) )
        {
            TRY ( RowSetIteratorInit ( & rsi -> dad, ctx, & MappingRowSetIterator_vt ) )
            {
                rsi -> row_id = self -> first_id;
                rsi -> last_excl = self -> last_excl;
                rsi -> large = large;
                return & rsi -> dad;
            }

            MemFree ( ctx, rsi, sizeof * rsi );
        }
    }

    return NULL;
}


/*--------------------------------------------------------------------------
 * MapFile
 */

static RowSetIterator_vt MapFileMappingRowSetIterator_vt =
{
    MapFileMappingRowSetIteratorWhack,
    MappingRowSetIteratorGetIdxMapping,
    MappingRowSetIteratorGetSourceIds,
    MapFileMappingRowSetIteratorNext
};

RowSetIterator * MapFileMakeMappingRowSetIterator ( const MapFile *self, const ctx_t *ctx, bool large )
{
    FUNC_ENTRY ( ctx );

    MappingRowSetIterator *rsi;

    /* create the iterator */
    TRY ( rsi = MemAlloc ( ctx, sizeof * rsi, true ) )
    {
        TRY ( RowSetIteratorInit ( & rsi -> dad, ctx, & MapFileMappingRowSetIterator_vt ) )
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
