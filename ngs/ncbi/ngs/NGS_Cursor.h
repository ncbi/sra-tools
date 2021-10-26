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

#ifndef _h_ngs_cursor_
#define _h_ngs_cursor_

typedef struct NGS_Cursor NGS_Cursor;
#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_Cursor
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct VDatabase;
struct VTable;
struct VCursor;
struct VBlob;
struct NGS_String;

/*--------------------------------------------------------------------------
 * NGS_Cursor
 *  Shareable cursor with lazy adding of columns
 *  reference counted
 */

/* Make
 */
const NGS_Cursor* NGS_CursorMake ( ctx_t ctx, const struct VTable* table, const char * col_specs[], uint32_t num_cols );
const NGS_Cursor* NGS_CursorMakeDb ( ctx_t ctx,
                                     const struct VDatabase* db,
                                     const struct NGS_String* run_name,
                                     const char* tableName,
                                     const char * col_specs[],
                                     uint32_t num_cols );

/* Release
 *  release reference
 */
void NGS_CursorRelease ( const NGS_Cursor * self, ctx_t ctx );

/* Duplicate
 *  duplicate reference
 */
const NGS_Cursor * NGS_CursorDuplicate ( const NGS_Cursor * self, ctx_t ctx );

/* CellDataDirect
 * Adds requested column if necessary and calls VCursorCellDataDirect
*/
void NGS_CursorCellDataDirect ( const NGS_Cursor *self,
                                ctx_t ctx,
                                int64_t rowId,
                                uint32_t colIdx,
                                uint32_t *elem_bits,
                                const void **base,
                                uint32_t *boff,
                                uint32_t *row_len );

/* GetString
*/
struct NGS_String * NGS_CursorGetString ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx );

/* GetInt64
*/
int64_t NGS_CursorGetInt64 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx );

/* GetUInt64
*/
uint64_t NGS_CursorGetUInt64 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx );

/* GetInt32
*/
int32_t NGS_CursorGetInt32 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx );

/* GetUInt32
*/
uint32_t NGS_CursorGetUInt32 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx );

/* GetBool
*/
bool NGS_CursorGetBool ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx );

/* GetChar
*/
char NGS_CursorGetChar ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx );

/* GetRowCount
 */
uint64_t NGS_CursorGetRowCount ( const NGS_Cursor * self, ctx_t ctx );

/* GetRowRange
 */
void NGS_CursorGetRowRange ( const NGS_Cursor * self, ctx_t ctx, int64_t* first, uint64_t* count );

/* GetTable
 */
const struct VTable* NGS_CursorGetTable ( const NGS_Cursor * self, ctx_t ctx );

/* GetVCursor
 */
const struct VCursor* NGS_CursorGetVCursor ( const NGS_Cursor * self );

/* GetColumnIndex
 */
uint32_t NGS_CursorGetColumnIndex ( const NGS_Cursor * self, ctx_t ctx, uint32_t column_id );

/* GetVBlob
 */
const struct VBlob* NGS_CursorGetVBlob ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t column_id );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_cursor_ */
