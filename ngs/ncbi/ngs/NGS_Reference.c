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


#include "NGS_Reference.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/ReferenceItf.h>

#include "NGS_String.h"
#include "NGS_ReadCollection.h"
#include "NGS_Alignment.h"
#include "NGS_Pileup.h"

#include <sysalloc.h>

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>

/*--------------------------------------------------------------------------
 * NGS_Reference_v1
 */

#define Self( obj ) \
    ( ( NGS_Reference* ) ( obj ) )

static NGS_String_v1 * ITF_Reference_v1_get_cmn_name ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReferenceGetCommonName ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Reference_v1_get_canon_name ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReferenceGetCanonicalName ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static bool ITF_Reference_v1_is_circular ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_ReferenceGetIsCircular ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint64_t ITF_Reference_v1_get_length ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_ReferenceGetLength ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * ITF_Reference_v1_get_ref_bases ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReferenceGetBases ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Reference_v1_get_ref_chunk ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReferenceGetChunk ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static uint64_t ITF_Reference_v1_get_align_count ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_ReferenceGetAlignmentCount ( Self ( self ), ctx, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static struct NGS_Alignment_v1 * ITF_Reference_v1_get_alignment ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, const char * alignmentId )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReferenceGetAlignment ( Self ( self ), ctx, alignmentId ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

static struct NGS_Alignment_v1 * ITF_Reference_v1_get_alignments ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReferenceGetAlignments ( Self ( self ), ctx, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

#if _DEBUGGING
static
uint32_t align_flags_to_filters ( uint32_t flags )
{
    static bool tested_bits;
    if ( ! tested_bits )
    {
        assert ( NGS_ReferenceAlignFlags_pass_bad >> 2 == NGS_AlignmentFilterBits_pass_bad );
        assert ( NGS_ReferenceAlignFlags_pass_dups >> 2 == NGS_AlignmentFilterBits_pass_dups );
        assert ( NGS_ReferenceAlignFlags_min_map_qual >> 2 == NGS_AlignmentFilterBits_min_map_qual );
        assert ( NGS_ReferenceAlignFlags_max_map_qual >> 2 == NGS_AlignmentFilterBits_max_map_qual);
        assert ( NGS_ReferenceAlignFlags_no_wraparound >> 2 == NGS_AlignmentFilterBits_no_wraparound);
        assert ( NGS_ReferenceAlignFlags_start_within_window >> 2 == NGS_AlignmentFilterBits_start_within_window);
        tested_bits = true;
    }
    return flags >> 2;
}
#else
#define align_flags_to_filters( flags ) \
    ( ( flags ) >> 2 )
#endif

static struct NGS_Alignment_v1 * ITF_Reference_v1_get_filtered_alignments ( const NGS_Reference_v1 * self,
                                                                            NGS_ErrBlock_v1 * err,
                                                                            enum NGS_ReferenceAlignFlags flags,
                                                                            int32_t map_qual )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );

    bool wants_primary = ( flags & NGS_ReferenceAlignFlags_wants_primary ) != 0;
    bool wants_secondary = ( flags & NGS_ReferenceAlignFlags_wants_secondary ) != 0;
    uint32_t filters = align_flags_to_filters ( flags );

    /*TODO: reject unimplemented flags */
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReferenceGetFilteredAlignments ( Self ( self ), ctx,
        wants_primary, wants_secondary, filters, map_qual ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

static struct NGS_Alignment_v1 * ITF_Reference_v1_get_align_slice ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, int64_t start, uint64_t length, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReferenceGetAlignmentSlice ( Self ( self ), ctx, start, length, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

static struct NGS_Alignment_v1 * ITF_Reference_v1_get_filtered_align_slice ( const NGS_Reference_v1 * self,
                                                                             NGS_ErrBlock_v1 * err,
                                                                             int64_t start,
                                                                             uint64_t length,
                                                                             enum NGS_ReferenceAlignFlags flags,
                                                                             int32_t map_qual )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );

    bool wants_primary = ( flags & NGS_ReferenceAlignFlags_wants_primary ) != 0;
    bool wants_secondary = ( flags & NGS_ReferenceAlignFlags_wants_secondary ) != 0;
    uint32_t filters = align_flags_to_filters ( flags );

    /*TODO: reject unimplemented flags */
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReferenceGetFilteredAlignmentSlice ( Self ( self ), ctx, start, length, wants_primary, wants_secondary, filters, map_qual ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

static struct NGS_Pileup_v1 * ITF_Reference_v1_get_pileups ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Pileup * ret = NGS_ReferenceGetPileups( Self ( self ), ctx, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Pileup_v1 * ) ret;
}

static struct NGS_Pileup_v1 * ITF_Reference_v1_get_pileup_slice ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, int64_t start, uint64_t length, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Pileup * ret = NGS_ReferenceGetPileupSlice ( Self ( self ), ctx, start, length, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Pileup_v1 * ) ret;
}

static bool ITF_Reference_v1_next ( NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_ReferenceIteratorNext ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

#if _DEBUGGING
static
uint32_t pileup_flags_to_filters ( uint32_t flags )
{
    static bool tested_bits;
    if ( ! tested_bits )
    {
        assert ( NGS_ReferenceAlignFlags_pass_bad >> 2 == NGS_PileupFilterBits_pass_bad );
        assert ( NGS_ReferenceAlignFlags_pass_dups >> 2 == NGS_PileupFilterBits_pass_dups );
        assert ( NGS_ReferenceAlignFlags_min_map_qual >> 2 == NGS_PileupFilterBits_min_map_qual );
        assert ( NGS_ReferenceAlignFlags_max_map_qual >> 2 == NGS_PileupFilterBits_max_map_qual);
        assert ( NGS_ReferenceAlignFlags_no_wraparound >> 2 == NGS_PileupFilterBits_no_wraparound);
        assert ( NGS_ReferenceAlignFlags_start_within_window >> 2 == NGS_PileupFilterBits_start_within_window);
        tested_bits = true;
    }
    return flags >> 2;
}
#else
#define pileup_flags_to_filters( flags ) \
    ( ( flags ) >> 2 )
#endif

static struct NGS_Pileup_v1 * ITF_Reference_v1_get_filtered_pileups ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
    uint32_t flags, int32_t map_qual )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );

    bool wants_primary = ( flags & NGS_ReferenceAlignFlags_wants_primary ) != 0;
    bool wants_secondary = ( flags & NGS_ReferenceAlignFlags_wants_secondary ) != 0;
    uint32_t filters = pileup_flags_to_filters ( flags );

    ON_FAIL ( struct NGS_Pileup * ret = NGS_ReferenceGetFilteredPileups ( Self ( self ), ctx, wants_primary, wants_secondary, filters, map_qual ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Pileup_v1 * ) ret;
}

static struct NGS_Pileup_v1 * ITF_Reference_v1_get_filtered_pileup_slice ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
    int64_t start, uint64_t length, uint32_t flags, int32_t map_qual )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );

    bool wants_primary = ( flags & NGS_ReferenceAlignFlags_wants_primary ) != 0;
    bool wants_secondary = ( flags & NGS_ReferenceAlignFlags_wants_secondary ) != 0;
    uint32_t filters = pileup_flags_to_filters ( flags );

    ON_FAIL ( struct NGS_Pileup * ret = NGS_ReferenceGetFilteredPileupSlice ( Self ( self ), ctx, start, length, wants_primary, wants_secondary, filters, map_qual ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Pileup_v1 * ) ret;
}

static bool ITF_Reference_v1_is_local ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_ReferenceGetIsLocal ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}


#undef Self


NGS_Reference_v1_vt ITF_Reference_vt =
{
    {
        "NGS_Reference",
        "NGS_Reference_v1",
        4,
        & ITF_Refcount_vt . dad
    },

    /* 1.0 */
    ITF_Reference_v1_get_cmn_name,
    ITF_Reference_v1_get_canon_name,
    ITF_Reference_v1_is_circular,
    ITF_Reference_v1_get_length,
    ITF_Reference_v1_get_ref_bases,
    ITF_Reference_v1_get_ref_chunk,
    ITF_Reference_v1_get_alignment,
    ITF_Reference_v1_get_alignments,
    ITF_Reference_v1_get_align_slice,
    ITF_Reference_v1_get_pileups,
    ITF_Reference_v1_get_pileup_slice,
    ITF_Reference_v1_next,

    /* 1.1 */
    ITF_Reference_v1_get_filtered_pileups,
    ITF_Reference_v1_get_filtered_pileup_slice,

    /* 1.2 */
    ITF_Reference_v1_get_align_count,

    /* 1.3 */
    ITF_Reference_v1_get_filtered_alignments,
    ITF_Reference_v1_get_filtered_align_slice,

    /* 1.4 */
    ITF_Reference_v1_is_local
};


/*--------------------------------------------------------------------------
 * NGS_Reference
 */
#define VT( self, msg ) \
    ( ( ( const NGS_Reference_vt* ) ( self ) -> dad . vt ) -> msg )

/* Init
*/
void NGS_ReferenceInit ( ctx_t ctx,
                         struct NGS_Reference * self,
                         NGS_Reference_vt * vt,
                         const char *clsname,
                         const char *instname,
                         struct NGS_ReadCollection * coll )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );

    assert ( self );
    assert ( vt );

    TRY ( NGS_RefcountInit ( ctx, & self -> dad, & ITF_Reference_vt . dad, & vt -> dad, clsname, instname ) )
    {
        assert ( vt -> get_common_name    != NULL );
        assert ( vt -> get_canonical_name != NULL );
        assert ( vt -> get_is_circular    != NULL );
        assert ( vt -> get_length         != NULL );
        assert ( vt -> get_bases          != NULL );
        assert ( vt -> get_chunk          != NULL );
        assert ( vt -> get_alignment      != NULL );
        assert ( vt -> get_alignments     != NULL );
        assert ( vt -> get_count          != NULL );
        assert ( vt -> get_slice          != NULL );
        assert ( vt -> get_pileups        != NULL );
        assert ( vt -> get_pileup_slice   != NULL );
        assert ( vt -> get_statistics     != NULL );
        assert ( vt -> next               != NULL );
        assert ( vt -> get_blobs          != NULL );
        assert ( vt -> get_is_local       != NULL );
    }

    assert ( coll );
    self -> coll = NGS_ReadCollectionDuplicate ( coll, ctx );
}

void NGS_ReferenceWhack( NGS_Reference * self, ctx_t ctx )
{
    NGS_ReadCollectionRelease ( self -> coll, ctx );
}

/*--------------------------------------------------------------------------
 * NGS_ReferenceIterator
 */

struct NGS_String * NGS_ReferenceGetCommonName ( NGS_Reference * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get common name" );
    }
    else
    {
        return VT ( self, get_common_name) ( self, ctx );
    }

    return NULL;
}

/* GetCanonicalName
 */
struct NGS_String * NGS_ReferenceGetCanonicalName ( NGS_Reference * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get canonical name" );
    }
    else
    {
        return VT ( self, get_canonical_name ) ( self, ctx );
    }

    return NULL;
}

/* GetisCircular
 */
bool NGS_ReferenceGetIsCircular ( const NGS_Reference * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get circular" );
    }
    else
    {
        return VT ( self, get_is_circular ) ( self, ctx );
    }

    return false;
}

/* GetLength
 */
uint64_t NGS_ReferenceGetLength ( NGS_Reference * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference length" );
    }
    else
    {
        return VT ( self, get_length ) ( self, ctx );
    }

    return 0;
}


/* GetBases
 */
struct NGS_String * NGS_ReferenceGetBases ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference bases" );
    }
    else
    {
        return VT ( self, get_bases ) ( self, ctx, offset, size );
    }

    return NULL;
}


/* GetChunk
 */
struct NGS_String * NGS_ReferenceGetChunk ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference chunk" );
    }
    else
    {
        return VT ( self, get_chunk ) ( self, ctx, offset, size );
    }

    return NULL;
}


/* GetAlignment
 */
struct NGS_Alignment* NGS_ReferenceGetAlignment ( NGS_Reference * self, ctx_t ctx, const char * alignmentId )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment" );
    }
    else
    {
        return VT ( self, get_alignment ) ( self, ctx, alignmentId );
    }

    return NULL;
}


/* GetAlignments
 */
struct NGS_Alignment* NGS_ReferenceGetAlignments ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignments" );
    }
    else
    {
        /* alignment iterator does not filter out bad reads and duplicates by default */
        const uint32_t filters = NGS_AlignmentFilterBits_pass_bad | NGS_AlignmentFilterBits_pass_dups;
        return VT ( self, get_alignments ) ( self, ctx, wants_primary, wants_secondary, filters, 0 );
    }

    return NULL;
}

/* GetFilteredAlignments
 */
struct NGS_Alignment* NGS_ReferenceGetFilteredAlignments ( NGS_Reference * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignments" );
    }
    else
    {
        return VT ( self, get_alignments ) ( self, ctx, wants_primary, wants_secondary, filters, map_qual );
    }

    return NULL;
}

/* GetAlignmentCount
 */
uint64_t NGS_ReferenceGetAlignmentCount ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment count" );
    }
    else
    {
        return VT ( self, get_count ) ( self, ctx, wants_primary, wants_secondary );
    }

    return 0;
}

/* GetAlignmentSlice
 */
struct NGS_Alignment* NGS_ReferenceGetAlignmentSlice ( NGS_Reference * self,
                                                       ctx_t ctx,
                                                       uint64_t offset,
                                                       uint64_t size,
                                                       bool wants_primary,
                                                       bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment slice" );
    }
    else
    {
        /* alignment iterator does not filter out bad reads and duplicates by default */
        const uint32_t filters = NGS_AlignmentFilterBits_pass_bad | NGS_AlignmentFilterBits_pass_dups;
        return VT ( self, get_slice ) ( self, ctx, offset, size, wants_primary, wants_secondary, filters, 0 );
    }

    return NULL;
}

/* GetFilteredAlignmentSlice
 */
struct NGS_Alignment* NGS_ReferenceGetFilteredAlignmentSlice ( NGS_Reference * self,
    ctx_t ctx, uint64_t offset, uint64_t size, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment slice" );
    }
    else
    {
        return VT ( self, get_slice ) ( self, ctx, offset, size, wants_primary, wants_secondary, filters, map_qual );
    }

    return NULL;
}

/* GetPileups
 */
struct NGS_Pileup* NGS_ReferenceGetPileups ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get pileups" );
    }
    else
    {
        /* pileup filters out bad reads and duplicates by default */
        return VT ( self, get_pileups ) ( self, ctx, wants_primary, wants_secondary, 0, 0 );
    }

    return NULL;
}

/* GetFilteredPileups
 */
struct NGS_Pileup* NGS_ReferenceGetFilteredPileups ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get pileups" );
    }
    else
    {
        return VT ( self, get_pileups ) ( self, ctx, wants_primary, wants_secondary, filters, map_qual );
    }

    return NULL;
}

/* GetPileupSlice
 */
struct NGS_Pileup* NGS_ReferenceGetPileupSlice ( NGS_Reference * self,
                                                 ctx_t ctx,
                                                 uint64_t offset,
                                                 uint64_t size,
                                                 bool wants_primary,
                                                 bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get pileups" );
    }
    else
    {
        return VT ( self, get_pileup_slice ) ( self, ctx, offset, size, wants_primary, wants_secondary, 0, 0 );
    }

    return NULL;
}

/* GetFilteredPileupSlice
 */
struct NGS_Pileup* NGS_ReferenceGetFilteredPileupSlice ( NGS_Reference * self,
                                                         ctx_t ctx,
                                                         uint64_t offset,
                                                         uint64_t size,
                                                         bool wants_primary,
                                                         bool wants_secondary,
                                                         uint32_t filters,
                                                         int32_t map_qual )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get pileups" );
    }
    else
    {
        return VT ( self, get_pileup_slice ) ( self, ctx, offset, size, wants_primary, wants_secondary, filters, map_qual );
    }

    return NULL;
}

/* GetStatistics
 */
struct NGS_Statistics* NGS_ReferenceGetStatistics ( const NGS_Reference * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get statistics" );
    }
    else
    {
        return VT ( self, get_statistics ) ( self, ctx );
    }

    return NULL;
}

/* GetisLocal
 */
bool NGS_ReferenceGetIsLocal ( const NGS_Reference * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get local" );
    }
    else
    {
        return VT ( self, get_is_local ) ( self, ctx );
    }

    return false;
}

/* GetBlobs
 */
struct NGS_ReferenceBlobIterator* NGS_ReferenceGetBlobs ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get blobs" );
    }
    else
    {
        return VT ( self, get_blobs ) ( self, ctx, offset, size );
    }

    return NULL;
}


/*--------------------------------------------------------------------------
 * NGS_ReferenceIterator
 */

/* Next
 */
bool NGS_ReferenceIteratorNext ( NGS_Reference * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get next reference " );
    }
    else
    {
        return VT ( self, next ) ( self, ctx );
    }

    return false;
}

 /* NullReference
 * will error out on any call
 */
static void Null_ReferenceWhack ( NGS_Reference * self, ctx_t ctx )
{
    NGS_ReadCollectionRelease ( self -> coll, ctx );
}

static NGS_String * Null_ReferenceGetCommonName ( NGS_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static NGS_String * Null_ReferenceGetCanonicalName ( NGS_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static bool Null_ReferenceGetIsCircular ( const NGS_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return false;
}

static uint64_t Null_ReferenceGetLength ( NGS_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return 0;
}

static struct NGS_String * Null_ReferenceGetBases ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static struct NGS_String * Null_ReferenceGetChunk ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static struct NGS_Alignment * Null_ReferenceGetAlignment ( NGS_Reference * self, ctx_t ctx, const char * alignmentId )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static struct NGS_Alignment * Null_ReferenceGetAlignments ( NGS_Reference * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static uint64_t Null_ReferenceGetAlignmentCount ( const NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return 0;
}

static struct NGS_Alignment* Null_ReferenceGetAlignmentSlice ( NGS_Reference * self,
                                                               ctx_t ctx,
                                                               uint64_t offset,
                                                               uint64_t size,
                                                               bool wants_primary,
                                                               bool wants_secondary,
                                                               uint32_t filters,
                                                               int32_t map_qual)
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static struct NGS_Pileup * Null_ReferenceGetPileups ( NGS_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static struct NGS_Pileup * Null_ReferenceGetPileupSlice ( NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

struct NGS_Statistics* Null_ReferenceGetStatistics ( const NGS_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static bool Null_ReferenceGetIsLocal ( const NGS_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return false;
}

static bool Null_ReferenceIteratorNext ( NGS_Reference * self, ctx_t ctx )
{
    return false;
}

static struct NGS_ReferenceBlobIterator* Null_ReferenceGetBlobs ( const NGS_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Reference accessed" );
    return NULL;
}

static NGS_Reference_vt Null_Reference_vt_inst =
{
    /* NGS_Refcount */
    { Null_ReferenceWhack },

    /* NGS_Reference */
    Null_ReferenceGetCommonName,
    Null_ReferenceGetCanonicalName,
    Null_ReferenceGetIsCircular,
    Null_ReferenceGetLength,
    Null_ReferenceGetBases,
    Null_ReferenceGetChunk,
    Null_ReferenceGetAlignment,
    Null_ReferenceGetAlignments,
    Null_ReferenceGetAlignmentCount,
    Null_ReferenceGetAlignmentSlice,
    Null_ReferenceGetPileups,
    Null_ReferenceGetPileupSlice,
    Null_ReferenceGetStatistics,
    Null_ReferenceGetIsLocal,
    Null_ReferenceGetBlobs,

    /* NGS_ReferenceIterator */
    Null_ReferenceIteratorNext
};

struct NGS_Reference * NGS_ReferenceMakeNull ( ctx_t ctx, struct NGS_ReadCollection * coll )
 {
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    NGS_Reference * ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating an empty NGS_ReferenceIterator" );
    else
    {
        TRY ( NGS_ReferenceInit ( ctx, ref, & Null_Reference_vt_inst, "NGS_Reference", "NullReference", coll ) )
        {
            return ref;
        }

        free ( ref );
    }

    return NULL;
 }
