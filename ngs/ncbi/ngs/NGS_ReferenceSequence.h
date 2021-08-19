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

#ifndef _h_NGS_ReferenceSequencesequence_
#define _h_NGS_ReferenceSequencesequence_

typedef struct NGS_ReferenceSequence NGS_ReferenceSequence;
#ifndef NGS_REFERENCESEQUENCE
#define NGS_REFERENCESEQUENCE NGS_ReferenceSequence
#endif

#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_REFERENCESEQUENCE
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_ReferenceSequence_v1_vt;
extern struct NGS_ReferenceSequence_v1_vt ITF_ReferenceSequence_vt;

/*--------------------------------------------------------------------------
 * NGS_ReferenceSequence
 */

/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_ReferenceSequenceToRefcount( self ) \
    ( & ( self ) -> dad )

/* Release
 *  release reference
 */
#define NGS_ReferenceSequenceRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_ReferenceSequenceToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_ReferenceSequenceDuplicate( self, ctx ) \
    ( ( NGS_ReferenceSequence* ) NGS_RefcountDuplicate ( NGS_ReferenceSequenceToRefcount ( self ), ctx ) )
    
/* GetCanonicalName
 */
struct NGS_String * NGS_ReferenceSequenceGetCanonicalName ( NGS_ReferenceSequence* self, ctx_t ctx );

/* GetisCircular
 */
bool NGS_ReferenceSequenceGetIsCircular ( NGS_ReferenceSequence const* self, ctx_t ctx );

/* GetLength
 */
uint64_t NGS_ReferenceSequenceGetLength ( NGS_ReferenceSequence* self, ctx_t ctx );

/* GetBases
 */
struct NGS_String * NGS_ReferenceSequenceGetBases ( NGS_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size );

/* GetChunk
 */
struct NGS_String * NGS_ReferenceSequenceGetChunk ( NGS_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size );

/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_ReferenceSequence
{
    NGS_Refcount dad;
};

typedef struct NGS_ReferenceSequence_vt NGS_ReferenceSequence_vt;
struct NGS_ReferenceSequence_vt
{
    NGS_Refcount_vt dad;
    
    struct NGS_String * ( * get_canonical_name ) ( NGS_REFERENCESEQUENCE * self, ctx_t ctx );
    bool                ( * get_is_circular    ) ( NGS_REFERENCESEQUENCE const* self, ctx_t ctx );
    uint64_t            ( * get_length         ) ( NGS_REFERENCESEQUENCE * self, ctx_t ctx );
    struct NGS_String * ( * get_bases          ) ( NGS_REFERENCESEQUENCE * self, ctx_t ctx, uint64_t offset, uint64_t size );
    struct NGS_String * ( * get_chunk          ) ( NGS_REFERENCESEQUENCE * self, ctx_t ctx, uint64_t offset, uint64_t size );
};

/* Make
 *  use provided specification to create an object
 *  any error returns NULL as a result and sets error in ctx
 */
NGS_ReferenceSequence * NGS_ReferenceSequenceMake ( ctx_t ctx, const char * spec );

/* additional make functions
 *  use provided specification to create an object
 *  any error returns NULL as a result and sets error in ctx
 */
NGS_ReferenceSequence * NGS_ReferenceSequenceMakeSRA ( ctx_t ctx, const char * spec );
NGS_ReferenceSequence * NGS_ReferenceSequenceMakeEBI ( ctx_t ctx, const char * spec );

/* Init
*/
void NGS_ReferenceSequenceInit ( ctx_t ctx, 
                         struct NGS_ReferenceSequence * self, 
                         NGS_ReferenceSequence_vt * vt, 
                         const char *clsname, 
                         const char *instname );
                         
                        
#ifdef __cplusplus
}
#endif

#endif /* _h_NGS_ReferenceSequencesequence_ */
