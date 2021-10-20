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

#include "NGS_ReferenceBlob.h"

#include <ngs/itf/Refcount.h>

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <vdb/blob.h>

#include "NGS_Cursor.h"
#include "CSRA1_Reference.h"
#include "VByteBlob.h"

#include "../vdb/blob-priv.h"
#include <../libs/vdb/page-map.h>

const int64_t ChunkSize = 5000;

struct NGS_ReferenceBlob
{
    NGS_Refcount dad;

    const VBlob* blob;

    int64_t     refFirst;  /* rowId of the first row in the reference/slice */
    int64_t     rowId;  /* rowId of the first row in the blob */
    uint64_t    count;  /* number of rows in the blob */

    int64_t     first;  /* rowId of the first row in VBlob (may differ from rowId) */

    const void* data;   /* start of the first row */
    uint64_t    size;   /* from the start of the first row until the end of the blob */
};

void
NGS_ReferenceBlobWhack ( NGS_ReferenceBlob * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcDestroying );
    if ( self != NULL )
    {
        VBlobRelease ( ( VBlob * ) self -> blob );
    }
}

static NGS_Refcount_vt NGS_ReferenceBlob_vt =
{
    NGS_ReferenceBlobWhack
};

struct NGS_ReferenceBlob * NGS_ReferenceBlobMake ( ctx_t ctx, const NGS_Cursor* p_curs, int64_t p_firstRowId, int64_t p_refFirstRowId, int64_t p_refLastRowId )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcConstructing );
    if ( p_curs == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "NULL cursor object" );
    }
    else if ( p_refFirstRowId < 1 )
    {
        INTERNAL_ERROR ( xcParamNull, "Invalid refFirstRowId: %li", p_refFirstRowId );
    }
    else if ( p_firstRowId < p_refFirstRowId )
    {
        INTERNAL_ERROR ( xcParamNull, "Invalid rowId: %li (less than refFirstRowId=%li)", p_firstRowId, p_refFirstRowId );
    }
    else
    {
        NGS_ReferenceBlob * ret = calloc ( 1, sizeof * ret );
        if ( ret == NULL )
        {
            SYSTEM_ERROR ( xcNoMemory, "allocating NGS_ReferenceBlob" );
        }
        else
        {
            TRY ( NGS_RefcountInit ( ctx, & ret -> dad, & ITF_Refcount_vt . dad, & NGS_ReferenceBlob_vt, "NGS_ReferenceBlob", "" ) )
            {
                TRY ( ret -> blob = NGS_CursorGetVBlob ( p_curs, ctx, p_firstRowId, reference_READ ) )
                {
                    ret -> refFirst = p_refFirstRowId;
                    ret -> rowId = p_firstRowId;
                    TRY ( VByteBlob_ContiguousChunk ( ret -> blob,
                                                      ctx,
                                                      ret -> rowId,
                                                      p_refLastRowId - p_firstRowId + 1,
                                                      false,
                                                      & ret -> data,
                                                      & ret -> size,
                                                      & ret -> count ) )
                    {
                        TRY ( VByteBlob_IdRange ( ret -> blob, ctx, & ret -> first, NULL ) )
                        {
                            assert ( ret -> first <= ret -> rowId );
                            return ret;
                        }
                    }
                    VBlobRelease ( ( VBlob * ) ret -> blob );
                }
            }
            free ( ret );
        }
    }
    return NULL;
}

void NGS_ReferenceBlobRowRange ( const struct NGS_ReferenceBlob * self, ctx_t ctx, int64_t * p_first, uint64_t * p_count )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );

    if ( self == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    }
    else
    {
        if ( p_first != NULL )
        {
            *p_first = self -> rowId;
        }
        if ( p_count != NULL )
        {
            *p_count = self -> count;
        }
    }
}

void NGS_ReferenceBlobRelease ( struct NGS_ReferenceBlob * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    if ( self != NULL )
    {
        NGS_RefcountRelease ( & self -> dad, ctx );
    }
}

NGS_ReferenceBlob * NGS_ReferenceBlobDuplicate (  struct NGS_ReferenceBlob * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    if ( self != NULL )
    {
        NGS_RefcountDuplicate ( & self -> dad, ctx );
    }
    return self;
}

const void* NGS_ReferenceBlobData ( const struct NGS_ReferenceBlob * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );

    if ( self == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    }
    else
    {
        return self -> data;
    }
    return 0;
}

uint64_t NGS_ReferenceBlobSize ( const struct NGS_ReferenceBlob * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );

    if ( self == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    }
    else
    {
        return self -> size;
    }
    return 0;
}

uint64_t NGS_ReferenceBlobUnpackedSize ( const struct NGS_ReferenceBlob * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    uint64_t ret = 0;
    if ( self == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    }
    else
    {
        PageMapIterator pmIt;
        TRY ( VByteBlob_PageMapNewIterator ( self->blob, ctx, & pmIt, self -> rowId - self -> first, self -> count ) )
        {
            row_count_t  repeat;
            do
            {
                repeat = PageMapIteratorRepeatCount ( &pmIt );
                ret += repeat * PageMapIteratorDataLength ( &pmIt );
            }
            while ( PageMapIteratorAdvance ( &pmIt, repeat ) );
        }
    }
    return ret;
}

void NGS_ReferenceBlobResolveOffset ( const struct NGS_ReferenceBlob * self, ctx_t ctx, uint64_t p_inBlob, uint64_t* p_inReference, uint32_t* p_repeatCount, uint64_t* p_increment )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    if ( self == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    }
    else if ( p_inBlob >= self -> size )
    {
        INTERNAL_ERROR ( xcParamNull, "offset %lu is out of range (0-%lu)", p_inBlob, self -> size );
    }
    else if ( p_inReference == NULL )
    {
        INTERNAL_ERROR ( xcParamNull, "NULL return parameter" );
    }
    else
    {   /* Iterate through the blob's page map to locate the chuink containing position p_inBlob. Once found, translate */
        /* the offset inside the blob into offset on the reference. */
        PageMapIterator pmIt;
        TRY ( VByteBlob_PageMapNewIterator ( self->blob, ctx, &pmIt, self -> rowId - self -> first, self -> count ) )
        {
            uint64_t inUnrolledBlob = 0; /* offset from the starting position of self->rowId if all repetitions in the blob were unrolled */
            while ( true )
            {
                row_count_t  repeat = PageMapIteratorRepeatCount ( &pmIt );
                int64_t size = PageMapIteratorDataLength ( &pmIt );
                elem_count_t offset = PageMapIteratorDataOffset ( &pmIt );
                if (inUnrolledBlob == 0)
                {   /* the first offset is not always 0! */
                    inUnrolledBlob = offset;
                }
                assert ( size <= ChunkSize ); /* this may be the last chunk, shorter than ChunkSize */
                if ( p_inBlob < offset + size )
                {
                    /* assume all the prior chunks of this reference were ChunkSize long */
                    * p_inReference =  ( self -> rowId - self -> refFirst ) * ChunkSize + inUnrolledBlob + p_inBlob % ChunkSize;
                    if ( p_repeatCount != NULL )
                    {
                        * p_repeatCount = repeat;
                    }
                    if ( p_increment != NULL )
                    {   /* p_increment is only used when there is a repetition, specifies how much to increase p_inBlob by */
                        /* in order to start the next call to NGS_ReferenceBlobResolveOffset() past the repeated rows */
                        * p_increment = repeat > 1 ? size : 0;
                    }
                    return;
                }
                if ( ! PageMapIteratorAdvance ( &pmIt, repeat ) )
                {
                    INTERNAL_ERROR ( xcParamNull, "offset %lu is not found in (row=%li, count=%lu)", p_inBlob, self -> rowId, self -> count );
                    return;
                }
                inUnrolledBlob += repeat * size;
            }
        }
    }
}
