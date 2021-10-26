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

#include "NGS_ReferenceBlobIterator.h"

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <klib/rc.h>

#include <ngs/itf/Refcount.h>

#include <vdb/cursor.h>

#include "NGS_String.h"
#include "NGS_Cursor.h"
#include "SRA_Read.h"
#include "NGS_ReferenceBlob.h"

struct NGS_ReferenceBlobIterator
{
    NGS_Refcount dad;

    const NGS_Cursor* curs;
    int64_t ref_start;
    int64_t next_row;
    int64_t last_row;
};

void
NGS_ReferenceBlobIteratorWhack ( NGS_ReferenceBlobIterator * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcDestroying );
    if ( self != NULL )
    {
        NGS_CursorRelease ( self->curs, ctx );
    }
}

static NGS_Refcount_vt NGS_ReferenceBlobIterator_vt =
{
    NGS_ReferenceBlobIteratorWhack
};

NGS_ReferenceBlobIterator*
NGS_ReferenceBlobIteratorMake ( ctx_t ctx, const struct NGS_Cursor* p_curs, int64_t p_refStartId, int64_t p_firstRowId, int64_t p_lastRowId )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcConstructing );
    if ( p_curs == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "NULL cursor object" );
    }
    else
    {
        NGS_ReferenceBlobIterator* ret = malloc ( sizeof * ret );
        if ( ret == NULL )
        {
            SYSTEM_ERROR ( xcNoMemory, "allocating NGS_ReferenceBlobIterator" );
        }
        else
        {
            TRY ( NGS_RefcountInit ( ctx, & ret -> dad, & ITF_Refcount_vt . dad, & NGS_ReferenceBlobIterator_vt, "NGS_ReferenceBlobIterator", "" ) )
            {   /* set up iteration boundaries */
                TRY ( ret -> curs = NGS_CursorDuplicate ( p_curs, ctx ) )
                {
                    ret -> ref_start = p_refStartId;
                    ret -> next_row = p_firstRowId;
                    ret -> last_row = p_lastRowId;
                    return ret;
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
NGS_ReferenceBlobIteratorRelease ( NGS_ReferenceBlobIterator * self, ctx_t ctx )
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
NGS_ReferenceBlobIterator *
NGS_ReferenceBlobIteratorDuplicate ( NGS_ReferenceBlobIterator * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    if ( self != NULL )
    {
        NGS_RefcountDuplicate( & self -> dad, ctx );
    }
    return ( NGS_ReferenceBlobIterator* ) self;
}

/* HasMore
 *  return true if there are more blobs to iterate on
 */
bool
NGS_ReferenceBlobIteratorHasMore ( NGS_ReferenceBlobIterator * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcSelecting );
        INTERNAL_ERROR ( xcSelfNull, "NULL ReferenceBlobIterator accessed" );
        return false;
    }
    return self -> next_row <= self -> last_row;
}

/* Next
 *  return the next blob, to be Release()d by the caller.
 *  NULL if there are no more blobs
 */
NGS_ReferenceBlob*
NGS_ReferenceBlobIteratorNext ( NGS_ReferenceBlobIterator * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );

    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcSelecting );
        INTERNAL_ERROR ( xcSelfNull, "NULL ReferenceBlobIterator accessed" );
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
            TRY ( NGS_ReferenceBlob* ret = NGS_ReferenceBlobMake ( ctx, self -> curs, nextRow, self -> ref_start, self -> last_row ) )
            {
                int64_t first;
                uint64_t count;
                TRY ( NGS_ReferenceBlobRowRange ( ret, ctx, & first, & count ) )
                {
                    self -> next_row = first + count;
                    return ret;
                }
                NGS_ReferenceBlobRelease ( ret, ctx );
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
