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

#ifndef _h_ngs_read_collection_
#define _h_ngs_read_collection_

typedef struct NGS_ReadCollection NGS_ReadCollection;
#ifndef NGS_READCOLLECTION
#define NGS_READCOLLECTION NGS_ReadCollection
#endif

#ifndef _h_NGS_Refcount_
    #ifndef NGS_REFCOUNT
    #define NGS_REFCOUNT NGS_READCOLLECTION
    #endif
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct VDatabase;
struct VTable;
struct NGS_Read;
struct NGS_String;
struct NGS_ReadCollection_v1_vt;
extern struct NGS_ReadCollection_v1_vt ITF_ReadCollection_vt;

/*--------------------------------------------------------------------------
 * NGS_ReadCollection
 */

/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_ReadCollectionToRefcount( self ) \
    ( & ( self ) -> dad )

/* Release
 *  release reference
 */
#define NGS_ReadCollectionRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_ReadCollectionToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_ReadCollectionDuplicate( self, ctx ) \
    ( ( NGS_ReadCollection* ) NGS_RefcountDuplicate ( NGS_ReadCollectionToRefcount ( self ), ctx ) )

/* Make
 *  use provided specification to create an object
 *  any error returns NULL as a result and sets error in ctx
 */
NGS_ReadCollection * NGS_ReadCollectionMake ( ctx_t ctx, const char * spec );

/* additional make functions
 *  use provided specification to create an object
 *  any error returns NULL as a result and sets error in ctx
 */
NGS_ReadCollection * NGS_ReadCollectionMakeCSRA ( ctx_t ctx, struct VDatabase const *db, const char * spec );
NGS_ReadCollection * NGS_ReadCollectionMakeVDatabase ( ctx_t ctx, struct VDatabase const *db, const char * spec );
NGS_ReadCollection * NGS_ReadCollectionMakeVTable ( ctx_t ctx, struct VTable const *tbl, const char * spec );


/* GetName
 */
struct NGS_String * NGS_ReadCollectionGetName ( NGS_ReadCollection * self, ctx_t ctx );


/* READ GROUPS
 */
struct NGS_ReadGroup * NGS_ReadCollectionGetReadGroups ( NGS_ReadCollection * self, ctx_t ctx );
bool NGS_ReadCollectionHasReadGroup ( NGS_ReadCollection * self, ctx_t ctx, const char * spec );
struct NGS_ReadGroup * NGS_ReadCollectionGetReadGroup ( NGS_ReadCollection * self, ctx_t ctx, const char * spec );


/* REFERENCES
 */
struct NGS_Reference * NGS_ReadCollectionGetReferences ( NGS_ReadCollection * self, ctx_t ctx );
bool NGS_ReadCollectionHasReference ( NGS_ReadCollection * self, ctx_t ctx, const char * spec );
struct NGS_Reference * NGS_ReadCollectionGetReference ( NGS_ReadCollection * self, ctx_t ctx, const char * spec );


/* ALIGNMENTS
 */
struct NGS_Alignment * NGS_ReadCollectionGetAlignments ( NGS_ReadCollection * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary );
struct NGS_Alignment * NGS_ReadCollectionGetAlignment ( NGS_ReadCollection * self, ctx_t ctx, const char * alignmentId );

uint64_t NGS_ReadCollectionGetAlignmentCount ( NGS_ReadCollection * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary );

struct NGS_Alignment * NGS_ReadCollectionGetAlignmentRange ( NGS_ReadCollection * self, ctx_t ctx, uint64_t first, uint64_t count,
    bool wants_primary, bool wants_secondary );

/* READS
 */
struct NGS_Read * NGS_ReadCollectionGetRead ( NGS_ReadCollection * self, ctx_t ctx, const char * readId );
struct NGS_Read * NGS_ReadCollectionGetReads ( NGS_ReadCollection * self, ctx_t ctx,
    bool wants_full, bool wants_partial, bool wants_unaligned );

uint64_t NGS_ReadCollectionGetReadCount ( NGS_ReadCollection * self, ctx_t ctx,
    bool wants_full, bool wants_partial, bool wants_unaligned );

struct NGS_Read * NGS_ReadCollectionGetReadRange ( NGS_ReadCollection * self,
                                                   ctx_t ctx,
                                                   uint64_t first,
                                                   uint64_t count,
                                                   bool wants_full,
                                                   bool wants_partial,
                                                   bool wants_unaligned );

/* STATISTICS
 */
struct NGS_Statistics* NGS_ReadCollectionGetStatistics ( NGS_ReadCollection * self, ctx_t ctx );

/* FRAGMENT BLOBS
 */
struct NGS_FragmentBlobIterator* NGS_ReadCollectionGetFragmentBlobs ( NGS_ReadCollection * self, ctx_t ctx );

/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_ReadCollection
{
    NGS_Refcount dad;
};

typedef struct NGS_ReadCollection_vt NGS_ReadCollection_vt;
struct NGS_ReadCollection_vt
{
    NGS_Refcount_vt dad;

    struct NGS_String*      ( * get_name )              ( NGS_READCOLLECTION * self, ctx_t ctx );
    struct NGS_ReadGroup*   ( * get_read_groups )       ( NGS_READCOLLECTION * self, ctx_t ctx );
           bool             ( * has_read_group )        ( NGS_READCOLLECTION * self, ctx_t ctx, const char * spec );
    struct NGS_ReadGroup*   ( * get_read_group )        ( NGS_READCOLLECTION * self, ctx_t ctx, const char * spec );
    struct NGS_Reference*   ( * get_references )        ( NGS_READCOLLECTION * self, ctx_t ctx );
           bool             ( * has_reference )         ( NGS_READCOLLECTION * self, ctx_t ctx, const char * spec );
    struct NGS_Reference*   ( * get_reference )         ( NGS_READCOLLECTION * self, ctx_t ctx, const char * spec );
    struct NGS_Alignment*   ( * get_alignments )        ( NGS_READCOLLECTION * self, ctx_t ctx, bool wants_primary, bool wants_secondary );
    struct NGS_Alignment*   ( * get_alignment )         ( NGS_READCOLLECTION * self, ctx_t ctx, const char * alignmentId );
    uint64_t                ( * get_alignment_count )   ( NGS_READCOLLECTION * self, ctx_t ctx, bool wants_primary, bool wants_secondary );
    struct NGS_Alignment*   ( * get_alignment_range )   ( NGS_READCOLLECTION * self, ctx_t ctx, uint64_t first, uint64_t count, bool wants_primary, bool wants_secondary );
    struct NGS_Read*        ( * get_reads )             ( NGS_READCOLLECTION * self, ctx_t ctx, bool wants_full, bool wants_partial, bool wants_unaligned );
    struct NGS_Read*        ( * get_read )              ( NGS_READCOLLECTION * self, ctx_t ctx, const char * readId );
    uint64_t                ( * get_read_count )        ( NGS_READCOLLECTION * self, ctx_t ctx,
        bool wants_full, bool wants_partial, bool wants_unaligned );
    struct NGS_Read*        ( * get_read_range )        ( NGS_READCOLLECTION * self, ctx_t ctx, uint64_t first, uint64_t count, bool wants_full, bool wants_partial, bool wants_unaligned );
    struct NGS_Statistics*  ( * get_statistics )        ( NGS_READCOLLECTION * self, ctx_t ctx );

    struct NGS_FragmentBlobIterator *  ( * get_frag_blobs ) ( NGS_READCOLLECTION * self, ctx_t ctx );
};


/* Init
 */
void NGS_ReadCollectionInit ( ctx_t ctx, NGS_ReadCollection * ref,
    const NGS_ReadCollection_vt *vt, const char *clsname, const char *instname );


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_read_collection_ */
