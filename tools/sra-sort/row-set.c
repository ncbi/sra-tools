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

#include "row-set-priv.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"

#include <vdb/cursor.h>
#include <klib/rc.h>

FILE_ENTRY ( row-set );


/*--------------------------------------------------------------------------
 * RowSet
 *  interface to iterate row-ids
 */


/* Release
 *  releases reference
 */
void RowSetRelease ( const RowSet *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "RowSet" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release RowSet" );
        }
    }
}

/* Duplicate
 */
RowSet *RowSetDuplicate ( const RowSet *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "RowSet" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate RowSet" );
            return NULL;
        }
    }

    return ( RowSet* ) self;
}


/* Init
 */
void RowSetInit ( RowSet *self, const ctx_t *ctx, const RowSet_vt *vt )
{
    self -> vt = vt;
    KRefcountInit ( & self -> refcount, 1, "RowSet", "init", "" );
    self -> align = 0;
}


/*--------------------------------------------------------------------------
 * RowSetIterator
 *  interface to iterate RowSets
 */

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
 */
RowSetIterator *TablePairMakeRowSetIterator ( struct TablePair *self, const ctx_t *ctx,
    struct MapFile const *sort_idx, bool pairs, bool large )
{
    FUNC_ENTRY ( ctx );

    if ( sort_idx != NULL )
    {
        if ( pairs )
            return MapFileMakeMappingRowSetIterator ( sort_idx, ctx, large );

        return MapFileMakeSortingRowSetIterator ( sort_idx, ctx, large );
    }

    if ( pairs )
        return TablePairMakeMappingRowSetIterator ( self, ctx, large );

    return TablePairMakeSimpleRowSetIterator ( self, ctx );
}


/* Release
 *  releases reference
 */
void RowSetIteratorRelease ( const RowSetIterator *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "RowSetIterator" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release RowSetIterator" );
        }
    }
}

/* Duplicate
 */
RowSetIterator *RowSetIteratorDuplicate ( const RowSetIterator *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "RowSetIterator" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate RowSetIterator" );
            return NULL;
        }
    }

    return ( RowSetIterator* ) self;
}


/* Init
 */
void RowSetIteratorInit ( RowSetIterator *self, const ctx_t *ctx, const RowSetIterator_vt *vt )
{
    self -> vt = vt;
    KRefcountInit ( & self -> refcount, 1, "RowSetIterator", "init", "" );
    self -> align = 0;
}
