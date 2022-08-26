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

typedef struct SimpleRowSet SimpleRowSet;
#define ROWSET_IMPL SimpleRowSet

typedef struct SimpleRowSetIterator SimpleRowSetIterator;
#define ROWSET_ITER_IMPL SimpleRowSetIterator

#include "row-set-priv.h"
#include "tbl-pair.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"

#include <klib/rc.h>

FILE_ENTRY ( simple-row-set );


/*--------------------------------------------------------------------------
 * SimpleRowSet
 *  implementation of RowSet based upon auto-generated serial ids
 */
struct SimpleRowSet
{
    RowSet dad;
    int64_t first, row_id, last_excl;
};

static
void SimpleRowSetWhack ( SimpleRowSet *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    MemFree ( ctx, self, sizeof *self );
}

static
size_t SimpleRowSetNext ( SimpleRowSet *self, const ctx_t *ctx,
    int64_t *row_ids, size_t max_ids )
{
    if ( row_ids != NULL )
    {
        size_t i;

        uint64_t max_avail = self -> last_excl - self -> row_id;
        if ( max_avail < ( uint64_t ) max_ids )
            max_ids = ( size_t ) max_avail;

        for ( i = 0; i < max_ids; ++ i )
            row_ids [ i ] = self -> row_id + i;

        self -> row_id += max_ids;
        return max_ids;
    }
    return 0;
}

static
void SimpleRowSetReset ( SimpleRowSet *self, const ctx_t *ctx, bool for_static )
{
    self -> row_id = self -> first;
}

static RowSet_vt SimpleRowSet_vt =
{
    SimpleRowSetWhack,
    SimpleRowSetNext,
    SimpleRowSetReset
};


/* Make
 */
static
RowSet *SimpleRowSetMake ( const ctx_t *ctx, int64_t first, int64_t last_excl )
{
    FUNC_ENTRY ( ctx );

    SimpleRowSet *rs;
    TRY ( rs = MemAlloc ( ctx, sizeof * rs, false ) )
    {
        RowSetInit ( & rs -> dad, ctx, & SimpleRowSet_vt );
        rs -> first = rs -> row_id = first;
        rs -> last_excl = last_excl;
        return & rs -> dad;
    }

    return NULL;
}


/*--------------------------------------------------------------------------
 * SimpleRowSetIterator
 *  interface to iterate RowSets
 */
struct SimpleRowSetIterator
{
    RowSetIterator dad;
    RowSet *rs;
};

static
void SimpleRowSetIteratorWhack ( SimpleRowSetIterator *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    RowSetRelease ( self -> rs, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
struct IdxMapping *SimpleRowSetIteratorGetIdxMapping ( SimpleRowSetIterator *self, const ctx_t *ctx, size_t *num_items )
{
    FUNC_ENTRY ( ctx );

    ANNOTATE ( "CALLING GetIdxMapping ON SIMPLE ROWSET ITERATOR" );

    if ( num_items != NULL )
        * num_items = 0;

    return NULL;
}

static
int64_t *SimpleRowSetIteratorGetSourceIds ( SimpleRowSetIterator *self, const ctx_t *ctx, uint32_t **opt_ord, size_t *num_items )
{
    FUNC_ENTRY ( ctx );

    ANNOTATE ( "CALLING GetSourceIds ON SIMPLE ROWSET ITERATOR" );

    if ( opt_ord != NULL )
        * opt_ord = NULL;

    if ( num_items != NULL )
        * num_items = 0;

    return NULL;
}

static
RowSet *SimpleRowSetIteratorNext ( SimpleRowSetIterator *self, const ctx_t *ctx )
{
    RowSet *rs = self -> rs;
    self -> rs = NULL;
    return rs;    
}

static RowSetIterator_vt SimpleRowSetIterator_vt =
{
    SimpleRowSetIteratorWhack,
    SimpleRowSetIteratorGetIdxMapping,
    SimpleRowSetIteratorGetSourceIds,
    SimpleRowSetIteratorNext
};


/* MakeSimpleRowSetIterator
 *  make simple row-id iterator
 *  runs from first to last id in cursor
 */
RowSetIterator *TablePairMakeSimpleRowSetIterator ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> first_id > self -> last_excl )
    {
        rc_t rc = RC ( rcExe, rcCursor, rcConstructing, rcRange, rcInvalid );
        INTERNAL_ERROR ( rc, "union of all column ranges improperly set" );
    }
    else
    {
        SimpleRowSetIterator *it;

        TRY ( it = MemAlloc ( ctx, sizeof * it, false ) )
        {
            RowSetIteratorInit ( & it -> dad, ctx, & SimpleRowSetIterator_vt );
            TRY ( it -> rs = SimpleRowSetMake ( ctx, self -> first_id, self -> last_excl ) )
            {
                return & it -> dad;
            }

            MemFree ( ctx, it, sizeof * it );
        }
    }

    return NULL;
}
