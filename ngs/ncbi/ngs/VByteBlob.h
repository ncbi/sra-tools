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

#ifndef _h_vbyteblob_
#define _h_vbyteblob_

#include <kfc/ctx.h>

struct VBlob;
struct PageMapIterator;

#ifdef __cplusplus
extern "C" {
#endif

/* Calculate the biggest available contiguous data portion of the blob:
 *  starts at rowId, ends at MaxRows if not 0, before a repeated value or at the end of the blob
 *
 * "rowId" [ IN ] - starting rowId
 *
 * "maxRows" [ IN ] - if not 0, do not extend to include more rows than maxRows
 *
 * "stopAtRepeat"" [ IN ] - if true, will stop after the first repetition of the first row repeated more than once.
 *
 * "data" [ OUT ] and "size"" [ OUT] - the returned data portion
 *
 * "rowCount" [ OUT, NULL OK ] - number of rows included in the chunk
 */
void VByteBlob_ContiguousChunk ( const struct VBlob *   self,
                                 ctx_t                  ctx,
                                 int64_t                rowId,
                                 uint64_t               maxRows,
                                 bool                   stopAtRepeat,
                                 const void **          data,
                                 uint64_t *             size,
                                 uint64_t *             rowCount );

/* IdRange
 *  returns id range for blob
 *
 *  "first" [ OUT, NULL OKAY ] and "count" [ OUT, NULL OKAY ] -
 *  id range is returned in these output parameters, where
 *  at least ONE must be NOT-NULL
 */
void VByteBlob_IdRange ( const struct VBlob* self, ctx_t ctx, int64_t *first, uint64_t *count );

/* CellData
 *  access pointer to single cell of potentially bit-aligned cell data
 *
 *  "elem_bits" [ OUT, NULL OKAY ] - optional return parameter for
 *  element size in bits
 *
 *  "base" [ OUT ] and "boff" [ OUT, NULL OKAY ] -
 *  compound return parameter for pointer to row starting bit
 *  where "boff" is in BITS
 *
 *  "row_len" [ OUT, NULL OKAY ] - the number of elements in cell
 */
void VByteBlob_CellData ( const struct VBlob *self,
                          ctx_t ctx,
                          int64_t row_id,
                          uint32_t *elem_bits,
                          const void **base,
                          uint32_t *boff,
                          uint32_t *row_len );

/* PageMapNewIterator
 *  Initialize an iterator for the blob's page map
 *
 *  "elem_bits" [ OUT, NULL OKAY ] - optional return parameter for
 *  element size in bits
 *
 *  "base" [ OUT ] and "boff" [ OUT, NULL OKAY ] -
 *  compound return parameter for pointer to row starting bit
 *  where "boff" is in BITS
 *
 *  "row_len" [ OUT, NULL OKAY ] - the number of elements in cell
 */
void VByteBlob_PageMapNewIterator ( const struct VBlob *self,
                                    ctx_t ctx,
                                    struct PageMapIterator *iter,
                                    uint64_t first_row,
                                    uint64_t num_rows);

#ifdef __cplusplus
}
#endif

#endif /* _h_vbyteblob_ */
