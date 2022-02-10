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

#include "NGS_Alignment.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/FragmentItf.h>
#include <ngs/itf/AlignmentItf.h>

#include "NGS_Refcount.h"
#include "NGS_String.h"

#include <sysalloc.h>

#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>

#include <klib/printf.h>

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

/*--------------------------------------------------------------------------
 * NGS_Alignment_v1
 */

#define Self( obj ) \
    ( ( NGS_Alignment* ) ( obj ) )

static NGS_String_v1 * ITF_Alignment_v1_get_id ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetAlignmentId ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_ref_spec ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetReferenceSpec ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static int32_t ITF_Alignment_v1_get_map_qual ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int32_t ret = NGS_AlignmentGetMappingQuality ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_ref_bases ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetReferenceBases ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_read_group ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetReadGroup ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_read_id ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetReadId ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_clipped_frag_bases ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetClippedFragmentBases ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_clipped_frag_quals ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetClippedFragmentQualities ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_aligned_frag_bases ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetAlignedFragmentBases ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static bool ITF_Alignment_v1_is_primary ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_AlignmentIsPrimary ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static int64_t ITF_Alignment_v1_get_align_pos ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int64_t ret = NGS_AlignmentGetAlignmentPosition ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint64_t ITF_Alignment_v1_get_ref_pos_projection_range ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, int64_t ref_pos )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_AlignmentGetReferencePositionProjectionRange ( Self ( self ), ctx, ref_pos ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint64_t ITF_Alignment_v1_get_align_length ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_AlignmentGetAlignmentLength ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static bool ITF_Alignment_v1_get_is_reversed ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_AlignmentGetIsReversedOrientation ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static int32_t ITF_Alignment_v1_get_soft_clip ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, uint32_t edge )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int32_t ret = NGS_AlignmentGetSoftClip ( Self ( self ), ctx, edge == 0 ) ) /* TODO: use an enum from <ngs/itf/AlignmentItf.h> */
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint64_t ITF_Alignment_v1_get_template_len ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_AlignmentGetTemplateLength ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_short_cigar ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, bool clipped )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetShortCigar ( Self ( self ), ctx, clipped ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_long_cigar ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, bool clipped )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetLongCigar ( Self ( self ), ctx, clipped ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static char ITF_Alignment_v1_get_rna_orientation ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( char ret = NGS_AlignmentGetRNAOrientation ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static bool ITF_Alignment_v1_has_mate ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_AlignmentHasMate ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_mate_id ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetMateAlignmentId ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_Alignment_v1 * ITF_Alignment_v1_get_mate_alignment ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_Alignment * ret = NGS_AlignmentGetMateAlignment ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_Alignment_v1 * ) ret;
}

static NGS_String_v1 * ITF_Alignment_v1_get_mate_ref_spec ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_AlignmentGetMateReferenceSpec ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static bool ITF_Alignment_v1_get_mate_is_reversed ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_AlignmentGetMateIsReversedOrientation ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static bool ITF_Alignment_v1_next ( NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_AlignmentIteratorNext ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

#undef Self


NGS_Alignment_v1_vt ITF_Alignment_vt =
{
    {
        "NGS_Alignment",
        "NGS_Alignment_v1",
        2,
        & ITF_Fragment_vt . dad
    },

    /* v1.0 */
    ITF_Alignment_v1_get_id,
    ITF_Alignment_v1_get_ref_spec,
    ITF_Alignment_v1_get_map_qual,
    ITF_Alignment_v1_get_ref_bases,
    ITF_Alignment_v1_get_read_group,
    ITF_Alignment_v1_get_read_id,
    ITF_Alignment_v1_get_clipped_frag_bases,
    ITF_Alignment_v1_get_clipped_frag_quals,
    ITF_Alignment_v1_get_aligned_frag_bases,
    ITF_Alignment_v1_is_primary,
    ITF_Alignment_v1_get_align_pos,
    ITF_Alignment_v1_get_align_length,
    ITF_Alignment_v1_get_is_reversed,
    ITF_Alignment_v1_get_soft_clip,
    ITF_Alignment_v1_get_template_len,
    ITF_Alignment_v1_get_short_cigar,
    ITF_Alignment_v1_get_long_cigar,
    ITF_Alignment_v1_has_mate,
    ITF_Alignment_v1_get_mate_id,
    ITF_Alignment_v1_get_mate_alignment,
    ITF_Alignment_v1_get_mate_ref_spec,
    ITF_Alignment_v1_get_mate_is_reversed,
    ITF_Alignment_v1_next,

    /* v1.1 */
    ITF_Alignment_v1_get_rna_orientation,

    /* v1.2 */
    ITF_Alignment_v1_get_ref_pos_projection_range
};


#define VT( self, msg ) \
    ( ( ( const NGS_Alignment_vt* ) ( self ) -> dad . dad . vt ) -> msg )

/* Init
 */
void NGS_AlignmentInit ( ctx_t ctx, NGS_Alignment * ref, const NGS_Alignment_vt *vt, const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    if ( ref == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_FragmentInit ( ctx, & ref -> dad, & ITF_Alignment_vt . dad, & vt -> dad, clsname, instname ) )
        {
        }
    }
}

struct NGS_String * NGS_AlignmentGetAlignmentId( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetAlignmentId failed" );
    }
    else
    {
        return VT ( self, getId ) ( self, ctx );
    }
    return 0;
}

struct NGS_String* NGS_AlignmentGetReferenceSpec( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetReferenceSpec failed" );
    }
    else
    {
        return VT ( self, getReferenceSpec ) ( self, ctx );
    }
    return NULL;
}

int NGS_AlignmentGetMappingQuality( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetMappingQuality failed" );
    }
    else
    {
        return VT ( self, getMappingQuality ) ( self, ctx );
    }
    return 0;
}

INSDC_read_filter NGS_AlignmentGetReadFilter( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetReadFilter failed" );
    }
    else
    {
        return VT ( self, getReadFilter ) ( self, ctx );
    }
    return 0;
}

struct NGS_String* NGS_AlignmentGetReferenceBases( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetMappingQuality failed" );
    }
    else
    {
        return VT ( self, getReferenceBases ) ( self, ctx );
    }
    return NULL;
}

struct NGS_String* NGS_AlignmentGetReadGroup( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetMappingQuality failed" );
    }
    else
    {
        return VT ( self, getReadGroup ) ( self, ctx );
    }
    return NULL;
}

struct NGS_String * NGS_AlignmentGetReadId( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetReadId failed" );
    }
    else
    {
        return VT ( self, getReadId ) ( self, ctx );
    }
    return 0;
}

struct NGS_String* NGS_AlignmentGetClippedFragmentBases( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetFragmentBases failed" );
    }
    else
    {
        return VT ( self, getClippedFragmentBases ) ( self, ctx );
    }
    return NULL;
}

struct NGS_String* NGS_AlignmentGetClippedFragmentQualities( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetFragmentQualities failed" );
    }
    else
    {
        return VT ( self, getClippedFragmentQualities ) ( self, ctx );
    }
    return NULL;
}

struct NGS_String* NGS_AlignmentGetAlignedFragmentBases( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetAlignedFragmentBases failed" );
    }
    else
    {
        return VT ( self, getAlignedFragmentBases ) ( self, ctx );
    }
    return NULL;
}

bool NGS_AlignmentIsPrimary( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentIsPrimary failed" );
    }
    else
    {
        return VT ( self, isPrimary ) ( self, ctx );
    }
    return false;
}

int64_t NGS_AlignmentGetAlignmentPosition( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetAlignmentPosition failed" );
    }
    else
    {
        return VT ( self, getAlignmentPosition ) ( self, ctx );
    }
    return 0;
}

uint64_t NGS_AlignmentGetReferencePositionProjectionRange( NGS_Alignment* self, ctx_t ctx, int64_t ref_pos )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetReferencePositionProjectionRange failed" );
    }
    else
    {
        return VT ( self, getReferencePositionProjectionRange ) ( self, ctx, ref_pos );
    }
    return 0;
}

uint64_t NGS_AlignmentGetAlignmentLength( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetAlignmentLength failed" );
    }
    else
    {
        return VT ( self, getAlignmentLength ) ( self, ctx );
    }
    return 0;
}

bool NGS_AlignmentGetIsReversedOrientation( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetIsReversedOrientation failed" );
    }
    else
    {
        return VT ( self, getIsReversedOrientation ) ( self, ctx );
    }
    return false;
}

int NGS_AlignmentGetSoftClip( NGS_Alignment* self, ctx_t ctx, bool left )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetSoftClip failed" );
    }
    else
    {
        return VT ( self, getSoftClip ) ( self, ctx, left );
    }
    return 0;
}

uint64_t NGS_AlignmentGetTemplateLength( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetTemplateLength failed" );
    }
    else
    {
        return VT ( self, getTemplateLength ) ( self, ctx );
    }
    return 0;
}

struct NGS_String* NGS_AlignmentGetShortCigar( NGS_Alignment* self, ctx_t ctx, bool clipped )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetShortCigar failed" );
    }
    else
    {
        return VT ( self, getShortCigar ) ( self, ctx, clipped );
    }
    return NULL;
}

struct NGS_String* NGS_AlignmentGetLongCigar( NGS_Alignment* self, ctx_t ctx, bool clipped )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetLongCigar failed" );
    }
    else
    {
        return VT ( self, getLongCigar ) ( self, ctx, clipped );
    }
    return NULL;
}

char NGS_AlignmentGetRNAOrientation( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetRNAOrientation failed" );
    }
    else
    {
        return VT ( self, getRNAOrientation ) ( self, ctx );
    }
    return '?';
}

bool NGS_AlignmentHasMate ( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentHasMate failed" );
    }
    else
    {
        return VT ( self, hasMate ) ( self, ctx );
    }
    return false;
}

struct NGS_String* NGS_AlignmentGetMateAlignmentId( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetMateAlignmentId failed" );
    }
    else
    {
        return VT ( self, getMateAlignmentId ) ( self, ctx );
    }
    return 0;
}

NGS_Alignment* NGS_AlignmentGetMateAlignment( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetMateAlignment failed" );
    }
    else
    {
        return VT ( self, getMateAlignment ) ( self, ctx );
    }
    return NULL;
}

struct NGS_String* NGS_AlignmentGetMateReferenceSpec( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetMateReferenceSpec failed" );
    }
    else
    {
        return VT ( self, getMateReferenceSpec ) ( self, ctx );
    }
    return NULL;
}

bool NGS_AlignmentGetMateIsReversedOrientation( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentGetMateIsReversedOrientation failed" );
    }
    else
    {
        return VT ( self, getMateIsReversedOrientation ) ( self, ctx );
    }
    return false;
}

bool NGS_AlignmentIsFirst ( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "NGS_AlignmentIsFirst failed" );
    }
    else
    {
        return VT ( self, isFirst ) ( self, ctx );
    }
    return false;
}

/*--------------------------------------------------------------------------
 * NGS_AlignmentIterator
 */
bool NGS_AlignmentIteratorNext( NGS_Alignment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to increment iterator" );
    }
    else
    {
        return VT ( self, next ) ( self, ctx );
    }

    return false;
}


/*--------------------------------------------------------------------------
 * Null_Alignment
 */
static
void NullAlignmentWhack ( NGS_Alignment * self, ctx_t ctx )
{
}

static
struct NGS_String * NullAlignment_FragmentToString ( NGS_Alignment * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static
struct NGS_String * NullAlignment_FragmentOffsetLenToString ( NGS_Alignment * self, ctx_t ctx, uint64_t offset, uint64_t length )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static
bool NullAlignment_FragmentToBool ( NGS_Alignment * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}


static int64_t NullAlignment_toI64 ( NGS_ALIGNMENT* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static struct NGS_String* NullAlignment_toString ( NGS_ALIGNMENT* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return NULL;
}

static int NullAlignment_toInt ( NGS_ALIGNMENT* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static struct NGS_String* NullAlignment_boolToString ( NGS_ALIGNMENT* self, ctx_t ctx, bool b )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return NULL;
}

static bool NullAlignment_toBool ( NGS_ALIGNMENT* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return false;
}

static uint8_t NullAlignment_toU8 ( NGS_ALIGNMENT* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static uint64_t NullAlignment_toU64 ( NGS_ALIGNMENT* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static int NullAlignment_boolToInt ( NGS_ALIGNMENT* self, ctx_t ctx, bool b )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static NGS_ALIGNMENT* NullAlignment_toAlignment ( NGS_ALIGNMENT* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return NULL;
}

static bool NullAlignment_noNext( NGS_ALIGNMENT* self, ctx_t ctx )
{   /* trying to advance an empty iterator - not an error */
    return false;
}

static char NullAlignment_RNAOrientation ( NGS_ALIGNMENT * self, ctx_t ctx )
{
    return '?';
}

static uint64_t NullAlignment_ReferencePositionProjectionRange ( NGS_ALIGNMENT * self, ctx_t ctx, int64_t ref_pos )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return -1 ^ (uint64_t)0xffffffff;
}

static NGS_Alignment_vt NullAlignment_vt_inst =
{
    {
        {
             /* NGS_Refcount */
            NullAlignmentWhack
        },

        /* NGS_Fragment */
        NullAlignment_FragmentToString,
        NullAlignment_FragmentOffsetLenToString,
        NullAlignment_FragmentOffsetLenToString,
        NullAlignment_FragmentToBool,
        NullAlignment_FragmentToBool,
        NullAlignment_FragmentToBool
    },

    NullAlignment_toString,        /* getId                        */
    NullAlignment_toString,        /* getReferenceSpec             */
    NullAlignment_toInt,           /* getMappingQuality            */
    NullAlignment_toU8,            /* getReadFilter                */
    NullAlignment_toString,        /* getReferenceBases            */
    NullAlignment_toString,        /* getReadGroup                 */
    NullAlignment_toString,        /* getReadId                    */
    NullAlignment_toString,        /* getClippedFragmentBases      */
    NullAlignment_toString,        /* getClippedFragmentQualities  */
    NullAlignment_toString,        /* getAlignedFragmentBases      */
    NullAlignment_toBool,          /* isPrimary                    */
    NullAlignment_toI64,           /* getAlignmentPosition         */
    NullAlignment_ReferencePositionProjectionRange, /* getReferencePositionProjectionRange */
    NullAlignment_toU64,           /* getAlignmentLength           */
    NullAlignment_toBool,          /* getIsReversedOrientation     */
    NullAlignment_boolToInt,       /* getSoftClip                  */
    NullAlignment_toU64,           /* getTemplateLength            */
    NullAlignment_boolToString,    /* getShortCigar                */
    NullAlignment_boolToString,    /* getLongCigar                 */
    NullAlignment_RNAOrientation,  /* getRNAOrientation            */
    NullAlignment_toBool,          /* hasMate                      */
    NullAlignment_toString,        /* getMateAlignmentId           */
    NullAlignment_toAlignment,     /* getMateAlignment             */
    NullAlignment_toString,        /* getMateReferenceSpec         */
    NullAlignment_toBool,          /* getMateIsReversedOrientation */
    NullAlignment_toBool,          /* isFirst                      */

    /* Iterator */
    NullAlignment_noNext           /* next                         */
};

struct NGS_Alignment * NGS_AlignmentMakeNull ( ctx_t ctx, char const * run_name, size_t run_name_size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    NGS_Alignment * ref;

    assert ( run_name != NULL );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating NullAlignment on '%.*s'", run_name_size, run_name );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s(NULL)", run_name_size, run_name );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( NGS_AlignmentInit ( ctx, ref, & NullAlignment_vt_inst, "NullAlignment", instname ) )
        {
            return ref;
        }
        free ( ref );
    }

    return NULL;
}


