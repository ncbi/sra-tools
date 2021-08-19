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

#ifndef _h_NGS_FragmentBlobIterator_
#define _h_NGS_FragmentBlobIterator_

typedef struct NGS_FragmentBlobIterator NGS_FragmentBlobIterator;
#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_FragmentBlobIterator
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct VTable;
struct NGS_String;
struct NGS_FragmentBlob;

/*--------------------------------------------------------------------------
 * NGS_FragmentBlobIterator
 *  Iteration over blobs in the SEQUENCE.READ column
 *  reference counted
 */

/* Make
 */
NGS_FragmentBlobIterator* NGS_FragmentBlobIteratorMake ( ctx_t ctx, const struct NGS_String* run, const struct VTable* sequence );

/* Release
 *  release reference
 */
void NGS_FragmentBlobIteratorRelease ( NGS_FragmentBlobIterator * self, ctx_t ctx );

/* Duplicate
 *  duplicate reference
 */
NGS_FragmentBlobIterator * NGS_FragmentBlobIteratorDuplicate ( NGS_FragmentBlobIterator * self, ctx_t ctx );

/* HasMore
 *  return true if there are more blobs to iterate on
 */
bool NGS_FragmentBlobIteratorHasMore ( NGS_FragmentBlobIterator * self, ctx_t ctx );

/* Next
 *  return the next blob, to be Release()d by the caller.
 *  NULL if there are no more blobs
 */
struct NGS_FragmentBlob* NGS_FragmentBlobIteratorNext ( NGS_FragmentBlobIterator * self, ctx_t ctx );

#ifdef __cplusplus
}
#endif

#endif /* _h_NGS_FragmentBlob_ */
