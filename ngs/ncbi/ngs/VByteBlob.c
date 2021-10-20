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

#include "VByteBlob.h"

#include <kfc/except.h>
#include <kfc/xc.h>

#include <vdb/blob.h>
#include <../libs/vdb/blob-priv.h>
#include <../libs/vdb/page-map.h>

/* Calculate the biggest available contiguous data portion of the blob:
*  starts at rowId, ends before a repeated value or at the end of the blob
*/
void
VByteBlob_ContiguousChunk ( const VBlob*    p_blob,
                            ctx_t           ctx,
                            int64_t         p_rowId,
                            uint64_t        p_maxRows,
                            bool            p_stopAtRepeat,
                            const void **   p_data,
                            uint64_t *      p_size,
                            uint64_t *      p_rowCount)
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );

    assert ( p_blob );
    assert ( p_data );
    assert ( p_size );

    {
        uint32_t elem_bits;
        const void *base;
        uint32_t boff;
        uint32_t row_len;
        TRY ( VByteBlob_CellData ( p_blob,
                                   ctx,
                                   p_rowId,
                                   & elem_bits,
                                   & base,
                                   & boff,
                                   & row_len ) )
        {
            int64_t first;
            uint64_t count;

            assert( elem_bits == 8 );
            assert( boff == 0 );
            *p_data = base;
            *p_size = 0;

            TRY ( VByteBlob_IdRange ( p_blob, ctx, & first, & count ) )
            {
                if ( p_stopAtRepeat )
                {   /* iterate until enough rows are collected, or a repeated row is seen, or we are out of rows in this blob */
                    PageMapIterator pmIt;

                    assert ( p_rowId >= first && p_rowId < first + (int64_t)count );

                    if ( p_rowId - first + 1 < (int64_t)count ) /* more rows in the blob after p_rowId */
                    {   /* *p_size is the size of value on rowId. Increase size to include subsequent rows, until we see a repeat, p_maxRows is reached or the blob ends */
                        TRY ( VByteBlob_PageMapNewIterator ( p_blob, ctx, &pmIt, p_rowId - first, count - ( p_rowId - first ) ) ) /* here, rowId is relative to the blob */
                        {   /* there will always be at least one row */
                            uint64_t rowCount = 0;
                            do
                            {
                                ++ rowCount;
                                * p_size += PageMapIteratorDataLength ( &pmIt );
                                if ( PageMapIteratorRepeatCount ( &pmIt ) > 1 )
                                {   /* repeated row found */
                                    break;
                                }
                                if ( p_maxRows != 0 && rowCount == p_maxRows )
                                {   /* this is all we were asked for  */
                                    break;
                                }
                            }
                            while ( PageMapIteratorNext ( &pmIt ) );

                            if ( p_rowCount != 0 )
                            {
                                * p_rowCount = rowCount;
                            }
                        }
                    }
                    else /* p_rowId is the last row of the blob; return the entire blob */
                    {
                        * p_size = row_len;
                        if ( p_rowCount != 0 )
                        {
                            * p_rowCount = count;
                        }
                    }
                }
                else if ( p_maxRows > 0 && p_maxRows < count - ( p_rowId - first ) )
                {   /* return the size of the first p_maxRows rows */
                    const uint8_t* firstRow = (const uint8_t*)base;
                    VByteBlob_CellData ( p_blob,
                                         ctx,
                                         p_rowId + p_maxRows,
                                         & elem_bits,
                                         & base,
                                         & boff,
                                         & row_len );
                    * p_size = (const uint8_t*)( base ) - firstRow;
                    if ( p_rowCount != 0 )
                    {
                        * p_rowCount = p_maxRows;
                    }
                }
                else
                {   /* set the size to include the rest of the blob's data */
                    * p_size = BlobBufferBytes ( p_blob ) - ( (const uint8_t*)( base ) - (const uint8_t*)( p_blob -> data . base ) );
                    if ( p_rowCount != 0 )
                    {
                        * p_rowCount = count;
                    }
                }
            }
        }
    }
}

void
VByteBlob_IdRange ( const struct VBlob * p_blob,  ctx_t ctx, int64_t * p_first, uint64_t * p_count )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    rc_t rc = VBlobIdRange ( p_blob, p_first, p_count );
    if ( rc != 0  )
    {
        INTERNAL_ERROR ( xcUnexpected, "VBlobIdRange() rc = %R", rc );
    }
}

void VByteBlob_CellData ( const struct VBlob *  p_blob,
                          ctx_t                 ctx,
                          int64_t               p_row_id,
                          uint32_t *            p_elem_bits,
                          const void **         p_base,
                          uint32_t *            p_boff,
                          uint32_t *            p_row_len )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    rc_t rc = VBlobCellData ( p_blob, p_row_id, p_elem_bits, p_base, p_boff, p_row_len );
    if ( rc != 0  )
    {
        INTERNAL_ERROR ( xcUnexpected, "VBlobCellData() rc = %R", rc );
    }
}

void VByteBlob_PageMapNewIterator ( const struct VBlob *    p_blob,
                                    ctx_t                   ctx,
                                    PageMapIterator *       p_iter,
                                    uint64_t                p_first_row,
                                    uint64_t                p_num_rows )
{
    FUNC_ENTRY ( ctx, rcSRA, rcBlob, rcAccessing );
    rc_t rc = PageMapNewIterator ( p_blob -> pm, p_iter, p_first_row, p_num_rows );
    if ( rc != 0  )
    {
        INTERNAL_ERROR ( xcUnexpected, "PageMapNewIterator() rc = %R", rc );
    }
}
