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

#include "NGS_PileupEvent.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/PileupEventItf.h>

#include "NGS_String.h"
#include "NGS_Reference.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

/*--------------------------------------------------------------------------
 * NGS_PileupEvent_v1
 */

#define Self( obj ) \
    ( ( NGS_PileupEvent* ) ( obj ) )
    
static int32_t NGS_PileupEvent_v1_get_map_qual ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int32_t ret = NGS_PileupEventGetMappingQuality ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * NGS_PileupEvent_v1_get_align_id ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_PileupEventGetAlignmentId ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static int64_t NGS_PileupEvent_v1_get_align_pos ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int64_t ret = NGS_PileupEventGetAlignmentPosition ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static int64_t NGS_PileupEvent_v1_get_first_align_pos ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int64_t ret = NGS_PileupEventGetFirstAlignmentPosition ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static int64_t NGS_PileupEvent_v1_get_last_align_pos ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int64_t ret = NGS_PileupEventGetLastAlignmentPosition ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint32_t NGS_PileupEvent_v1_get_event_type ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint32_t ret = NGS_PileupEventGetEventType ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static char NGS_PileupEvent_v1_get_align_base ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( char ret = NGS_PileupEventGetAlignmentBase ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static char NGS_PileupEvent_v1_get_align_qual ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( char ret = NGS_PileupEventGetAlignmentQuality ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * NGS_PileupEvent_v1_get_ins_bases ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_PileupEventGetInsertionBases ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * NGS_PileupEvent_v1_get_ins_quals ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_PileupEventGetInsertionQualities ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static uint32_t NGS_PileupEvent_v1_get_rpt_count ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint32_t ret = NGS_PileupEventGetRepeatCount ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint32_t NGS_PileupEvent_v1_get_indel_type ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint32_t ret = NGS_PileupEventGetIndelType ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static bool NGS_PileupEvent_v1_next ( NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcCursor, rcAccessing );
    ON_FAIL ( bool ret = NGS_PileupEventIteratorNext ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static void NGS_PileupEvent_v1_reset ( NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcCursor, rcUpdating );
    ON_FAIL ( NGS_PileupEventIteratorReset ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
}

#undef Self


NGS_PileupEvent_v1_vt ITF_PileupEvent_vt =
{
    {
        "NGS_PileupEvent",
        "NGS_PileupEvent_v1",
        0,
        & ITF_Refcount_vt . dad
    },

    NGS_PileupEvent_v1_get_map_qual,
    NGS_PileupEvent_v1_get_align_id,
    NGS_PileupEvent_v1_get_align_pos,
    NGS_PileupEvent_v1_get_first_align_pos,
    NGS_PileupEvent_v1_get_last_align_pos,
    NGS_PileupEvent_v1_get_event_type,
    NGS_PileupEvent_v1_get_align_base,
    NGS_PileupEvent_v1_get_align_qual,
    NGS_PileupEvent_v1_get_ins_bases,
    NGS_PileupEvent_v1_get_ins_quals,
    NGS_PileupEvent_v1_get_rpt_count,
    NGS_PileupEvent_v1_get_indel_type,
    NGS_PileupEvent_v1_next,
    NGS_PileupEvent_v1_reset
};

/*--------------------------------------------------------------------------
 * NGS_Pileup
 */

#define VT( self, msg ) \
    ( ( ( const NGS_PileupEvent_vt* ) ( self ) -> dad . vt ) -> msg )

void NGS_PileupEventInit ( ctx_t ctx, struct NGS_PileupEvent * obj, 
    struct NGS_VTable const * ivt, const NGS_PileupEvent_vt * vt, 
    const char *clsname, const char *instname, struct NGS_Reference * ref )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );
    
    TRY ( NGS_RefcountInit ( ctx, & obj -> dad, ivt, & vt -> dad, clsname, instname ) )
    {
        assert ( vt -> get_mapping_quality != NULL );
        assert ( vt -> get_alignment_id != NULL );
        assert ( vt -> get_alignment_position != NULL );
        assert ( vt -> get_first_alignment_position != NULL );
        assert ( vt -> get_last_alignment_position != NULL );
        assert ( vt -> get_event_type != NULL );
        assert ( vt -> get_alignment_base != NULL );
        assert ( vt -> get_alignment_quality != NULL );
        assert ( vt -> get_insertion_bases != NULL );
        assert ( vt -> get_insertion_qualities != NULL );
        assert ( vt -> get_repeat_count != NULL );
        assert ( vt -> get_indel_type != NULL );
        assert ( vt -> next != NULL );
        assert ( vt -> reset != NULL );
    
        assert ( ref != NULL );
        obj -> ref = NGS_ReferenceDuplicate ( ref, ctx );
    }
}

void NGS_PileupEventWhack( struct NGS_PileupEvent * self, ctx_t ctx )
{
    NGS_ReferenceRelease ( self -> ref, ctx );
}
    
int NGS_PileupEventGetMappingQuality( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get mapping quality " );
    }
    else
    {
        return VT ( self, get_mapping_quality ) ( self, ctx );
    }

    return 0;
}

struct NGS_String * NGS_PileupEventGetAlignmentId( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment id" );
    }
    else
    {
        return VT ( self, get_alignment_id ) ( self, ctx );
    }

    return 0;
}

int64_t NGS_PileupEventGetAlignmentPosition( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment position " );
    }
    else
    {
        return VT ( self, get_alignment_position ) ( self, ctx );
    }

    return 0;
}

int64_t NGS_PileupEventGetFirstAlignmentPosition( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get first alignment position " );
    }
    else
    {
        return VT ( self, get_first_alignment_position ) ( self, ctx );
    }

    return 0;
}

int64_t NGS_PileupEventGetLastAlignmentPosition( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to last alignment position" );
    }
    else
    {
        return VT ( self, get_last_alignment_position ) ( self, ctx );
    }

    return 0;
}

/*TODO: use uint32_t to represent all enums */
int NGS_PileupEventGetEventType( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get event type " );
    }
    else
    {
        return VT ( self, get_event_type ) ( self, ctx );
    }

    return 0;
}

char NGS_PileupEventGetAlignmentBase( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment base" );
    }
    else
    {
        return VT ( self, get_alignment_base ) ( self, ctx );
    }

    return 0;
}

char NGS_PileupEventGetAlignmentQuality( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment quality " );
    }
    else
    {
        return VT ( self, get_alignment_quality ) ( self, ctx );
    }

    return 0;
}

struct NGS_String * NGS_PileupEventGetInsertionBases( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get insertion bases" );
    }
    else
    {
        return VT ( self, get_insertion_bases) ( self, ctx );
    }

    return NULL;
}

struct NGS_String * NGS_PileupEventGetInsertionQualities( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get insertion qualities" );
    }
    else
    {
        return VT ( self, get_insertion_qualities) ( self, ctx );
    }

    return NULL;
}

unsigned int NGS_PileupEventGetRepeatCount( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get repeat count" );
    }
    else
    {
        return VT ( self, get_repeat_count ) ( self, ctx );
    }

    return 0;
}

int NGS_PileupEventGetIndelType( const NGS_PileupEvent * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get indel type " );
    }
    else
    {
        return VT ( self, get_indel_type ) ( self, ctx );
    }

    return 0;
}

/*--------------------------------------------------------------------------
 * NGS_PileupEventIterator
 */
bool NGS_PileupEventIteratorNext ( NGS_PileupEvent* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get next pileup event" );
    }
    else
    {
        return VT ( self, next ) ( self, ctx );
    }

    return false;
}

void NGS_PileupEventIteratorReset ( NGS_PileupEvent* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to reset pileup event iterator" );
    }
    else
    {
        VT ( self, reset ) ( self, ctx );
    }
}
