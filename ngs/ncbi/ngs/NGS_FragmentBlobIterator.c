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

#include "NGS_FragmentBlobIterator.h"

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <ngs/itf/Refcount.h>

#include <klib/rc.h>

#include <vdb/cursor.h>

#include "NGS_String.h"
#include "NGS_Cursor.h"
#include "SRA_Read.h"
#include "NGS_FragmentBlob.h"

struct NGS_FragmentBlobIterator
{
    NGS_Refcount dad;

    const NGS_String* run;
    const NGS_Cursor* curs;
    int64_t last_row;
    int64_t next_row;
};

void
NGS_FragmentBlobIteratorWhack ( NGS_FragmentBlobIterator * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcDestroying );
    if ( self != NULL )
    {
        NGS_CursorRelease ( self->curs, ctx );
        NGS_StringRelease ( self->run, ctx );
    }
}

static NGS_Refcount_vt NGS_FragmentBlobIterator_vt =
{
    NGS_FragmentBlobIteratorWhack
};

NGS_FragmentBlobIterator*
NGS_FragmentBlobIteratorMake ( ctx_t ctx, const NGS_String* run, const struct VTable* tbl )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcConstructing );
    if ( tbl == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "NULL table object" );
    }
    else
    {
        NGS_FragmentBlobIterator* ret = malloc ( sizeof * ret );
        if ( ret == NULL )
        {
            SYSTEM_ERROR ( xcNoMemory, "allocating NGS_FragmentBlobIterator" );
        }
        else
        {
            TRY ( NGS_RefcountInit ( ctx, & ret -> dad, & ITF_Refcount_vt . dad, & NGS_FragmentBlobIterator_vt, "NGS_FragmentBlobIterator", "" ) )
            {   /* initialize cursor and iteration boundaries */
                TRY ( ret -> curs = NGS_CursorMake ( ctx, tbl, sequence_col_specs, seq_NUM_COLS ) )
                {
                    TRY ( ret -> run = NGS_StringDuplicate ( run, ctx ) )
                    {
                        ret -> last_row = NGS_CursorGetRowCount ( ret->curs, ctx );
                        ret -> next_row = 1;
                        return ret;
                    }
                    NGS_CursorRelease ( ret -> curs, ctx );
                }
            }
            free ( ret );
        }
    }
    return NULL;
}

/* Release
 *  release reference
 */
void
NGS_FragmentBlobIteratorRelease ( NGS_FragmentBlobIterator * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcReleasing );
    if ( self != NULL )
    {
        NGS_RefcountRelease ( & self -> dad, ctx );
    }
}

/* Duplicate
 *  duplicate reference
 */
NGS_FragmentBlobIterator *
NGS_FragmentBlobIteratorDuplicate ( NGS_FragmentBlobIterator * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    if ( self != NULL )
    {
        NGS_RefcountDuplicate( & self -> dad, ctx );
    }
    return ( NGS_FragmentBlobIterator* ) self;
}

/* HasMore
 *  return true if there are more blobs to iterate on
 */
bool
NGS_FragmentBlobIteratorHasMore ( NGS_FragmentBlobIterator * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcSelecting );
        INTERNAL_ERROR ( xcSelfNull, "NULL FragmentBlobIterator accessed" );
        return false;
    }
    return self != NULL && self -> next_row <= self -> last_row;
}

/* Next
 *  return the next blob, to be Release()d by the caller.
 *  NULL if there are no more blobs
 */
NGS_FragmentBlob*
NGS_FragmentBlobIteratorNext ( NGS_FragmentBlobIterator * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );

    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcSelecting );
        INTERNAL_ERROR ( xcSelfNull, "NULL FragmentBlobIterator accessed" );
    }
    else if ( self -> next_row <= self -> last_row )
    {   /* advance to the next non-NULL row in the READ column, retrieve its blob */
        int64_t nextRow;
        rc_t rc = VCursorFindNextRowIdDirect ( NGS_CursorGetVCursor ( self -> curs ),
                                               NGS_CursorGetColumnIndex ( self -> curs, ctx, seq_READ ),
                                               self -> next_row,
                                               & nextRow );
        if ( rc == 0 )
        {
            TRY ( NGS_FragmentBlob* ret = NGS_FragmentBlobMake ( ctx, self -> run, self -> curs, nextRow ) )
            {
                int64_t first;
                uint64_t count;
                TRY ( NGS_FragmentBlobRowRange ( ret, ctx, & first, & count ) )
                {
                    self -> next_row = first + count;
                    return ret;
                }
                NGS_FragmentBlobRelease ( ret, ctx );
            }
        }
        else if ( GetRCState ( rc ) != rcNotFound )
        {
            INTERNAL_ERROR ( xcUnexpected, "VCursorFindNextRowIdDirect(READ, row=%li ) rc = %R", self -> next_row, rc );
        }
        self -> next_row = self -> last_row + 1;
    }

    return NULL;
}
