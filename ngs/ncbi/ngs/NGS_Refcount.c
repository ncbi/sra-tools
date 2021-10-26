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

#include "NGS_Refcount.h"

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/refcount.h>

#include <ngs/itf/Refcount.h>
#include "NGS_ErrBlock.h"

#include <stdlib.h>
#include <assert.h>

#include <sysalloc.h>


/*--------------------------------------------------------------------------
 * NGS_Refcount
 */


/* Whack
 */
static
void NGS_RefcountWhack ( NGS_Refcount * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRefcount, rcDestroying );

    assert ( self -> vt != NULL );
    assert ( self -> vt -> whack != NULL );

    ( * self -> vt -> whack ) ( self, ctx );

    free ( self );
}


/* Release
 *  release reference
 */
void NGS_RefcountRelease ( const NGS_Refcount * self, ctx_t ctx )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "NGS_Refcount" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            NGS_RefcountWhack ( ( NGS_Refcount* ) self, ctx );
            break;
        case krefNegative:
        {
            FUNC_ENTRY ( ctx, rcSRA, rcRefcount, rcReleasing );
            INTERNAL_ERROR ( xcSelfZombie, "NGS object at %#p", self );
            atomic32_set ( & ( ( NGS_Refcount* ) self ) -> refcount, 0 );
            break;
        }}
    }
}


/* Duplicate
 *  add 1 to reference count
 *  return original pointer
 */
void * NGS_RefcountDuplicate ( const NGS_Refcount * self, ctx_t ctx )
{
    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "NGS_Refcount" ) )
        {
        case krefOkay:
            break;
        case krefLimit:
        {
            FUNC_ENTRY ( ctx, rcSRA, rcRefcount, rcAttaching );
            INTERNAL_ERROR ( xcRefcountOutOfBounds, "NGS object at %#p", self );
            atomic32_set ( & ( ( NGS_Refcount* ) self ) -> refcount, 0 );
            break;
        }}
    }

    return ( void* ) self;
}


/* Init
 */
void NGS_RefcountInit ( ctx_t ctx, NGS_Refcount * ref,
    const NGS_VTable * ivt, const NGS_Refcount_vt * vt,
    const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRefcount, rcConstructing );

    if ( ref == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else if ( ivt == NULL || vt == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad vt reference" );
    else
    {
        assert ( vt -> whack != NULL );

        ref -> ivt = ivt;
        ref -> vt = vt;
        KRefcountInit ( & ref -> refcount, 1, clsname, "init", instname );
        ref -> filler = 0;
    }
}


/*--------------------------------------------------------------------------
 * NGS_Refcount_v1
 */

#define Self( obj ) \
    ( ( NGS_Refcount* ) ( obj ) )

static
void ITF_Refcount_v1_release ( NGS_Refcount_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcReleasing );
    ON_FAIL ( NGS_RefcountRelease ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
}

static
void* ITF_Refcount_v1_duplicate ( const NGS_Refcount_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcReleasing );
    ON_FAIL ( void * ref = NGS_RefcountDuplicate ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();

    return ref;
}

NGS_Refcount_v1_vt ITF_Refcount_vt =
{
    {
        "NGS_Refcount",
        "NGS_Refcount_v1"
    },

    ITF_Refcount_v1_release,
    ITF_Refcount_v1_duplicate
};

#undef Self
