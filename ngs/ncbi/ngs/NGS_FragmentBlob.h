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

#ifndef _h_NGS_FragmentBlob_
#define _h_NGS_FragmentBlob_

typedef struct NGS_FragmentBlob NGS_FragmentBlob;
#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_FragmentBlob
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct NGS_FragmentBlob;
struct NGS_Cursor;
struct NGS_String;

/*--------------------------------------------------------------------------
 * NGS_FragmentBlob
 *  Access to blobs in a SEQUENCE.READ column
 *  reference counted
 */

/* Make
 *  create a blob containing the given rowId
 *  run - accession name
 */
struct NGS_FragmentBlob * NGS_FragmentBlobMake ( ctx_t ctx, const struct NGS_String * run, const struct NGS_Cursor* curs, int64_t rowId );

/* Release
 *  release reference
 */
void NGS_FragmentBlobRelease ( struct NGS_FragmentBlob * self, ctx_t ctx );

/* Duplicate
 *  duplicate reference
 */
NGS_FragmentBlob * NGS_FragmentBlobDuplicate (  struct NGS_FragmentBlob * self, ctx_t ctx );

/* RowRange
 *  return the range of row Ids in the blob
 */
void NGS_FragmentBlobRowRange ( const struct NGS_FragmentBlob * self, ctx_t ctx,  int64_t* first, uint64_t* count );

/* Data
 *  returns the contents of the blob (concatenated bases of all fragments)
 */
const void* NGS_FragmentBlobData ( const struct NGS_FragmentBlob * self, ctx_t ctx );
/* Size
 *  returns the size of contents of the blob (total of bases in all fragments)
 */
uint64_t NGS_FragmentBlobSize ( const struct NGS_FragmentBlob * self, ctx_t ctx );

/* Run
 * Returns the name of the run containing the blob
 */
const struct NGS_String * NGS_FragmentBlobRun ( const struct NGS_FragmentBlob * self, ctx_t ctx );

/* InfoByOffset
 *  retrieve fragment info by offset in the blob
 *  offsetInBases - an offset into the blob
 *  rowId [OUT, NULL OK]- rowId of the row containing the base at the offset
 *  fragStart [OUT, NULL OK] - offset to the first base of the row
 *  baseCount [OUT, NULL OK]- number of bases in the row
 *  bioNumber [OUT, NULL OK]- for a biological frament, 0-based biological fragment number inside the read; for a technical number, -1
 */
void NGS_FragmentBlobInfoByOffset ( const struct NGS_FragmentBlob * self, ctx_t ctx,  uint64_t offsetInBases, int64_t* rowId, uint64_t* fragStart, uint64_t* baseCount, int32_t* bioNumber );


/* ReadFragmentCount
 *  retrieve number of fragment in a read by read's rowId
 *  rowId - rowId of the read (must be inside the blob)
 *  returns number of fragments in the read
 *
 *  NOTE: implementation TBD
 */
uint32_t NGS_FragmentBlobReadFragmentCount ( const struct NGS_FragmentBlob * self, ctx_t ctx, int64_t rowId );

/* InfoByRowId
 *  retrieve fragment info by rowId and fragment number
 *  rowId - rowId of the read (must be inside the blob)
 *  fragmentNumber  - 0-based fragment number inside the read
 *  fragStart [OUT, NULL OK] - offset to the first base of the row
 *  baseCount [OUT, NULL OK]- number of bases in the row
 *  biological [OUT, NULL OK]- true if the fragment is biological, false if technical
 *
 *  NOTE: implementation TBD
 */
void NGS_FragmentBlobInfoByRowId ( const struct NGS_FragmentBlob * self, ctx_t ctx,  int64_t rowId, uint32_t fragNumber, uint64_t* fragStart, uint64_t* baseCount, bool* biological );

/* MakeFragmentId
 *  retrieve (biological) fragment Id  by rowId and fragment number
 *  rowId - rowId of the read (must be inside the blob)
 *  fragmentNumber  - 0-based biological fragment number inside the read
 *
 *  returns the ID string of the specified fragment
 * DEPRECATED. Call NGS_FragmentBlobInfoByRowId(..., rowId, fragNumber, ...) and NGS_IdMakeFragment ( ctx, NGS_FragmentBlobRun(...), false, rowId, fragNumber );
 */
struct NGS_String* NGS_FragmentBlobMakeFragmentId ( const struct NGS_FragmentBlob * self, ctx_t ctx,  int64_t rowId, uint32_t fragNumber );

#ifdef __cplusplus
}
#endif

#endif /* _h_NGS_FragmentBlob_ */
