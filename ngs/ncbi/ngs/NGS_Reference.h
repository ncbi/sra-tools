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

#ifndef _h_ngs_reference_
#define _h_ngs_reference_

typedef struct NGS_Reference NGS_Reference;
#ifndef NGS_REFERENCE
#define NGS_REFERENCE NGS_Reference
#endif

#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_REFERENCE
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct VCursor;
struct NGS_ReadCollection;
struct NGS_Alignment;
struct NGS_Pileup;
struct NGS_Reference_v1_vt;
extern struct NGS_Reference_v1_vt ITF_Reference_vt;

/*--------------------------------------------------------------------------
 * NGS_Reference
 */

/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_ReferenceToRefcount( self ) \
    ( & ( self ) -> dad )

/* Release
 *  release reference
 */
#define NGS_ReferenceRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_ReferenceToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_ReferenceDuplicate( self, ctx ) \
    ( ( NGS_Reference* ) NGS_RefcountDuplicate ( NGS_ReferenceToRefcount ( self ), ctx ) )

/* GetCommonName
 */
struct NGS_String * NGS_ReferenceGetCommonName ( NGS_Reference * self, ctx_t ctx );

/* GetCanonicalName
 */
struct NGS_String * NGS_ReferenceGetCanonicalName ( NGS_Reference * self, ctx_t ctx );

/* GetisCircular
 */
bool NGS_ReferenceGetIsCircular ( const NGS_Reference * self, ctx_t ctx );

/* GetLength
 */
uint64_t NGS_ReferenceGetLength ( NGS_Reference * self, ctx_t ctx );

/* GetBases
 */
struct NGS_String * NGS_ReferenceGetBases ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size );

/* GetChunk
 */
struct NGS_String * NGS_ReferenceGetChunk ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size );

/* GetAlignment
 */
struct NGS_Alignment* NGS_ReferenceGetAlignment ( NGS_Reference * self, ctx_t ctx, const char * alignmentId );

/* GetAlignments
 */
struct NGS_Alignment* NGS_ReferenceGetAlignments ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary );

/* GetFilteredAlignments
 */
struct NGS_Alignment* NGS_ReferenceGetFilteredAlignments ( NGS_Reference * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );

/* GetAlignmentCount
 */
uint64_t NGS_ReferenceGetAlignmentCount ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary );

/* GetAlignmentSlice
 */
struct NGS_Alignment* NGS_ReferenceGetAlignmentSlice ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size, bool wants_primary, bool wants_secondary );

/* GetFilteredAlignmentSlice
 */
struct NGS_Alignment* NGS_ReferenceGetFilteredAlignmentSlice ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size,
    bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );

/* GetPileups
 */
struct NGS_Pileup* NGS_ReferenceGetPileups ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary );

/* GetFilteredPileups
 */
struct NGS_Pileup* NGS_ReferenceGetFilteredPileups ( NGS_Reference * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );

/* GetPileupSlice
 */
struct NGS_Pileup* NGS_ReferenceGetPileupSlice ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size,
    bool wants_primary, bool wants_secondary );

/* GetFilteredPileupSlice
 */
struct NGS_Pileup* NGS_ReferenceGetFilteredPileupSlice ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size,
    bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );

/* GetStatistics
 */
struct NGS_Statistics* NGS_ReferenceGetStatistics ( const NGS_Reference * self, ctx_t ctx );

/* GetIsLocal
 */
bool NGS_ReferenceGetIsLocal ( const NGS_Reference * self, ctx_t ctx );

/*--------------------------------------------------------------------------
 * NGS_ReferenceIterator
 */

/* Next
 */
bool NGS_ReferenceIteratorNext ( NGS_Reference * self, ctx_t ctx );

/* FRAGMENT BLOBS
 */
struct NGS_ReferenceBlobIterator* NGS_ReferenceGetBlobs ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size );

/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_Reference
{
    NGS_Refcount dad;

    struct NGS_ReadCollection * coll;
};

typedef struct NGS_Reference_vt NGS_Reference_vt;
struct NGS_Reference_vt
{
    NGS_Refcount_vt dad;

    struct NGS_String *     ( * get_common_name    ) ( NGS_REFERENCE * self, ctx_t ctx );
    struct NGS_String *     ( * get_canonical_name ) ( NGS_REFERENCE * self, ctx_t ctx );
    bool                    ( * get_is_circular    ) ( const NGS_REFERENCE * self, ctx_t ctx );
    uint64_t                ( * get_length         ) ( NGS_REFERENCE * self, ctx_t ctx );
    struct NGS_String *     ( * get_bases          ) ( NGS_REFERENCE * self, ctx_t ctx, uint64_t offset, uint64_t size );
    struct NGS_String *     ( * get_chunk          ) ( NGS_REFERENCE * self, ctx_t ctx, uint64_t offset, uint64_t size );
    struct NGS_Alignment*   ( * get_alignment      ) ( NGS_REFERENCE * self, ctx_t ctx, const char * alignmentId );
    struct NGS_Alignment*   ( * get_alignments     ) ( NGS_REFERENCE * self, ctx_t ctx,
        bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
    uint64_t                ( * get_count          ) ( const NGS_REFERENCE * self, ctx_t ctx, bool wants_primary, bool wants_secondary );
    struct NGS_Alignment*   ( * get_slice          ) ( NGS_REFERENCE * self, ctx_t ctx, uint64_t offset, uint64_t size,
        bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
    struct NGS_Pileup*      ( * get_pileups        ) ( NGS_REFERENCE * self, ctx_t ctx, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
    struct NGS_Pileup*      ( * get_pileup_slice   ) ( NGS_REFERENCE * self, ctx_t ctx, uint64_t offset, uint64_t size,
        bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
    struct NGS_Statistics*  ( * get_statistics     ) ( const NGS_REFERENCE * self, ctx_t ctx );
    bool                    ( * get_is_local       ) ( const NGS_REFERENCE * self, ctx_t ctx );
    struct NGS_ReferenceBlobIterator* ( * get_blobs ) ( const NGS_REFERENCE * self, ctx_t ctx, uint64_t offset, uint64_t size );
    bool                    ( * next               ) ( NGS_REFERENCE * self, ctx_t ctx );
};

/* Init
*/
void NGS_ReferenceInit ( ctx_t ctx,
                         struct NGS_Reference * self,
                         NGS_Reference_vt * vt,
                         const char *clsname,
                         const char *instname,
                         struct NGS_ReadCollection * coll );

/* Whack
*/
void NGS_ReferenceWhack ( NGS_Reference * self, ctx_t ctx );

/* NullReference
 * will error out on any call
 */
struct NGS_Reference * NGS_ReferenceMakeNull ( ctx_t ctx, struct NGS_ReadCollection * coll );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_reference_ */
