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


#include "NGS_ReferenceSequence.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/ReferenceSequenceItf.h>

#include "NGS_String.h"

#include <sysalloc.h>

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/rc.h>

#include <vdb/table.h>
#include <vdb/cursor.h>


/*--------------------------------------------------------------------------
 * NGS_ReferenceSequence_v1
 */

#define Self( obj ) \
    ( ( NGS_ReferenceSequence* ) ( obj ) )
    
static NGS_String_v1 * ITF_ReferenceSequence_v1_get_canon_name ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReferenceSequenceGetCanonicalName ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static bool ITF_ReferenceSequence_v1_is_circular ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_ReferenceSequenceGetIsCircular ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint64_t ITF_ReferenceSequence_v1_get_length ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_ReferenceSequenceGetLength ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * ITF_ReferenceSequence_v1_get_ref_bases ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReferenceSequenceGetBases ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_ReferenceSequence_v1_get_ref_chunk ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReferenceSequenceGetChunk ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

#undef Self


NGS_ReferenceSequence_v1_vt ITF_ReferenceSequence_vt =
{
    {
        "NGS_ReferenceSequence",
        "NGS_ReferenceSequence_v1",
        2,
        & ITF_Refcount_vt . dad
    },

    /* 1.0 */
    ITF_ReferenceSequence_v1_get_canon_name,
    ITF_ReferenceSequence_v1_is_circular,
    ITF_ReferenceSequence_v1_get_length,
    ITF_ReferenceSequence_v1_get_ref_bases,
    ITF_ReferenceSequence_v1_get_ref_chunk,
};


/*--------------------------------------------------------------------------
 * NGS_ReferenceSequence
 */
#define VT( self, msg ) \
    ( ( ( const NGS_ReferenceSequence_vt* ) ( self ) -> dad . vt ) -> msg )
    
/* Make
 *  use provided specification to create an object
 *  any error returns NULL as a result and sets error in ctx
 */
NGS_ReferenceSequence * NGS_ReferenceSequenceMake ( ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcOpening );

    if ( spec == NULL )
        USER_ERROR ( xcParamNull, "NULL reference sequence specification string" );
    else if ( spec [ 0 ] == 0 )
        USER_ERROR ( xcStringEmpty, "empty reference sequence specification string" );
    else
    {
        NGS_ReferenceSequence* ref = NGS_ReferenceSequenceMakeSRA ( ctx, spec );

        if ( FAILED() &&
            (GetRCState ( ctx->rc ) == rcNotFound || GetRCState ( ctx->rc ) == rcUnexpected) )
        {
            CLEAR();
            assert ( ref == NULL );
            ref = NGS_ReferenceSequenceMakeEBI ( ctx, spec );
        }

        return ref;
    }

    return NULL;
}


/* Init
*/
void NGS_ReferenceSequenceInit ( ctx_t ctx, 
                         struct NGS_ReferenceSequence * self, 
                         NGS_ReferenceSequence_vt * vt, 
                         const char *clsname, 
                         const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );
    
    assert ( self );
    assert ( vt );
    
    TRY ( NGS_RefcountInit ( ctx, & self -> dad, & ITF_ReferenceSequence_vt . dad, & vt -> dad, clsname, instname ) )
    {
        assert ( vt -> get_canonical_name != NULL );
        assert ( vt -> get_is_circular    != NULL );
        assert ( vt -> get_length         != NULL );
        assert ( vt -> get_bases          != NULL );
        assert ( vt -> get_chunk          != NULL );
    }
}


/* GetCanonicalName
 */
struct NGS_String * NGS_ReferenceSequenceGetCanonicalName ( NGS_ReferenceSequence * self, ctx_t ctx )
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
bool NGS_ReferenceSequenceGetIsCircular ( NGS_ReferenceSequence const* self, ctx_t ctx )
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
uint64_t NGS_ReferenceSequenceGetLength ( NGS_ReferenceSequence * self, ctx_t ctx )
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
struct NGS_String * NGS_ReferenceSequenceGetBases ( NGS_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size )
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
struct NGS_String * NGS_ReferenceSequenceGetChunk ( NGS_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size )
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

