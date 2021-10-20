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

#include "NGS_Pileup.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/PileupItf.h>
#include <ngs/itf/PileupEventItf.h>

#include "NGS_String.h"
#include "NGS_Reference.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

/*--------------------------------------------------------------------------
 * NGS_Pileup_v1
 */

#define Self( obj ) \
    ( ( NGS_Pileup* ) ( obj ) )
    
static NGS_String_v1 * NGS_Pileup_v1_get_ref_spec ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcCursor, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_PileupGetReferenceSpec ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static int64_t NGS_Pileup_v1_get_ref_pos ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcCursor, rcAccessing );
    ON_FAIL ( int64_t ret = NGS_PileupGetReferencePosition ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static char NGS_Pileup_v1_get_ref_base ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcCursor, rcAccessing );
    ON_FAIL ( char ret = NGS_PileupGetReferenceBase ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint32_t NGS_Pileup_v1_get_pileup_depth ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcCursor, rcAccessing );
    ON_FAIL ( uint32_t ret = NGS_PileupGetPileupDepth ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static bool NGS_Pileup_v1_next ( NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcCursor, rcAccessing );
    ON_FAIL ( bool ret = NGS_PileupIteratorNext ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

#undef Self


NGS_Pileup_v1_vt ITF_Pileup_vt =
{
    {
        "NGS_Pileup",
        "NGS_Pileup_v1",
        0,
        & ITF_PileupEvent_vt . dad
    },

    NGS_Pileup_v1_get_ref_spec,
    NGS_Pileup_v1_get_ref_pos,
    NGS_Pileup_v1_get_ref_base,
    NGS_Pileup_v1_get_pileup_depth,
    NGS_Pileup_v1_next
};

/*--------------------------------------------------------------------------
 * NGS_Pileup
 */

#define VT( self, msg ) \
    ( ( ( const NGS_Pileup_vt* ) ( self ) -> dad . dad . vt ) -> msg )

void NGS_PileupInit ( ctx_t ctx, 
                      struct NGS_Pileup * obj, 
                      const NGS_Pileup_vt * vt, 
                      const char *clsname, 
                      const char *instname, 
                      struct NGS_Reference* ref )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );
    
    TRY ( NGS_PileupEventInit ( ctx, & obj -> dad, & ITF_Pileup_vt . dad, & vt -> dad, clsname, instname, ref ) )
    {
        assert ( vt -> get_reference_spec != NULL );
        assert ( vt -> get_reference_position != NULL );
        assert ( vt -> get_reference_base != NULL );
        assert ( vt -> get_pileup_depth != NULL );
        assert ( vt -> next != NULL );
    }
}

/* Whack
*/                         
void NGS_PileupWhack ( struct NGS_Pileup * self, ctx_t ctx )
{
    NGS_PileupEventWhack ( & self -> dad, ctx );
}
    
struct NGS_String* NGS_PileupGetReferenceSpec ( const NGS_Pileup* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference spec" );
    }
    else
    {
        return VT ( self, get_reference_spec ) ( self, ctx );
    }

    return NULL;
}

int64_t NGS_PileupGetReferencePosition ( const NGS_Pileup* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference position" );
    }
    else
    {
        return VT ( self, get_reference_position ) ( self, ctx );
    }

    return 0;
}

char NGS_PileupGetReferenceBase ( const NGS_Pileup* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference base" );
    }
    else
    {
        return VT ( self, get_reference_base ) ( self, ctx );
    }

    return 0;
}

unsigned int NGS_PileupGetPileupDepth ( const NGS_Pileup* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get pileup depth" );
    }
    else
    {
        return VT ( self, get_pileup_depth) ( self, ctx );
    }

    return 0;
}

bool NGS_PileupIteratorNext ( NGS_Pileup* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get next pileup" );
    }
    else
    {
        return VT ( self, next ) ( self, ctx );
    }

    return false;
}

