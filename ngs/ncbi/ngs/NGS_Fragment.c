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

#include "NGS_Fragment.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/FragmentItf.h>
 
#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <kfc/rc.h>

#include <assert.h>


/*--------------------------------------------------------------------------
 * NGS_Fragment_v1
 */

#define Self( obj ) \
    ( ( NGS_Fragment* ) ( obj ) )
    
static    
NGS_String_v1 * ITF_Fragment_v1_get_id ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_FragmentGetId ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static    
NGS_String_v1 * ITF_Fragment_v1_get_bases ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_FragmentGetSequence ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static    
NGS_String_v1 * ITF_Fragment_v1_get_quals ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_FragmentGetQualities ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static    
bool ITF_Fragment_v1_is_paired ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_FragmentIsPaired ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static    
bool ITF_Fragment_v1_is_aligned ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_FragmentIsAligned ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static    
bool ITF_Fragment_v1_next ( NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_FragmentIteratorNext ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

#undef Self

NGS_Fragment_v1_vt ITF_Fragment_vt =
{
    {
        "NGS_Fragment",
        "NGS_Fragment_v1",
        1,
        & ITF_Refcount_vt . dad
    },

    ITF_Fragment_v1_get_id,
    ITF_Fragment_v1_get_bases,
    ITF_Fragment_v1_get_quals,
    ITF_Fragment_v1_next,
    ITF_Fragment_v1_is_paired,
    ITF_Fragment_v1_is_aligned
};


/*--------------------------------------------------------------------------
 * NGS_Fragment
 */

#define VT( self, msg ) \
    ( ( ( const NGS_Fragment_vt* ) ( self ) -> dad . vt ) -> msg )


/* GetId
 *  returns an unique id within the context of the entire ReadCollection
 */
struct NGS_String * NGS_FragmentGetId ( NGS_Fragment* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get id" );
    }
    else
    {
        return VT ( self, get_id ) ( self, ctx );
    }

    return 0;
}


/* GetSequence
 *  read the Fragment sequence
 *  offset is zero based
 *  size is limited to bases available
 */
struct NGS_String * NGS_FragmentGetSequence ( NGS_Fragment * self, ctx_t ctx, uint64_t offset, uint64_t length )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get sequence" );
    }
    else
    {
        return VT ( self, get_sequence ) ( self, ctx, offset, length );
    }

    return NULL;
}


/* GetQualities
 *  read the Fragment qualities as phred-33
 *  offset is zero based
 *  size is limited to qualities available
 */
struct NGS_String * NGS_FragmentGetQualities ( NGS_Fragment * self, ctx_t ctx, uint64_t offset, uint64_t length )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get qualities" );
    }
    else
    {
        return VT ( self, get_qualities ) ( self, ctx, offset, length );
    }

    return NULL;
}


/* IsPaired
 */
bool NGS_FragmentIsPaired ( NGS_Fragment * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to test alignment" );
    }
    else
    {
        return VT ( self, is_paired ) ( self, ctx );
    }

    return false;
}


/* IsAligned
 */
bool NGS_FragmentIsAligned ( NGS_Fragment * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to test alignment" );
    }
    else
    {
        return VT ( self, is_aligned ) ( self, ctx );
    }

    return false;
}


/* Next
 *  advance to next Fragment
 *  returns false if no more Fragments are available.
 *  throws exception if more Fragments should be available,
 *  but could not be accessed.
 */
bool NGS_FragmentIteratorNext ( NGS_Fragment * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcSelecting );
        INTERNAL_ERROR ( xcSelfNull, "failed to advance to next Fragment" );
    }
    else
    {
        return VT ( self, next ) ( self, ctx );
    }

    return false;
}


/* Init
 */
void NGS_FragmentInit ( ctx_t ctx, NGS_Fragment * frag, const NGS_VTable * ivt, 
    const NGS_Fragment_vt * vt, const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );

    TRY ( NGS_RefcountInit ( ctx, & frag -> dad, ivt, & vt -> dad, clsname, instname ) )
    {
        assert ( vt -> get_id != NULL );
        assert ( vt -> get_sequence != NULL );
        assert ( vt -> get_qualities != NULL );
    }
}
