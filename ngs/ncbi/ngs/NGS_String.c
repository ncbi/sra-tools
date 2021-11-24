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

#include "NGS_String.h"

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/refcount.h>
#include <klib/printf.h>
#include <klib/text.h>

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/StringItf.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sysalloc.h>


/*--------------------------------------------------------------------------
 * NGS_String
 *  a reference into NGS string data
 *  reference counted, temporary
 */
struct NGS_String
{
    NGS_Refcount dad;   

    NGS_String * orig;
    char * owned;
    const char * str;
    size_t size;
};


/* Whack
 */
static
void NGS_StringWhack ( NGS_String * self, ctx_t ctx )
{
    if ( self -> orig != NULL )
        NGS_StringRelease ( self -> orig, ctx );
    if ( self -> owned != NULL )
        free ( self -> owned );
}


/* Release
 *  release reference
 */
void NGS_StringRelease ( const NGS_String * self, ctx_t ctx )
{
    if ( self != NULL )
    {
        NGS_RefcountRelease ( & self -> dad, ctx );
    }
}


/* Duplicate
 *  duplicate reference
 */
NGS_String * NGS_StringDuplicate ( const NGS_String * self, ctx_t ctx )
{
    if ( self != NULL )
    {
        NGS_RefcountDuplicate( & self -> dad, ctx );
    }
    return ( NGS_String* ) self;
}


/* Invalidate
 */
void NGS_StringInvalidate ( NGS_String * self, ctx_t ctx )
{
    if ( self != NULL )
    {
        NGS_String * orig = self -> orig;
        self -> size = 0;
        self -> str = "";
        if ( orig != NULL )
        {
            self -> orig = NULL;
            NGS_StringRelease ( orig, ctx );
        }
    }
}


/* Data
 *  retrieve data pointer
 */
const char * NGS_StringData ( const NGS_String * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcString, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "attempt to access NULL NGS_String" );
        return NULL;
    }

    return self -> str;
}


/* Size
 *  retrieve data length
 */
size_t NGS_StringSize ( const NGS_String * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcString, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "attempt to access NULL NGS_String" );
        return 0;
    }

    return self -> size;
}


/* Substr
 *  create a new allocation
 *  returns a substring, either at a simple offset
 *  or at an offset with length
 */
NGS_String * NGS_StringSubstrOffset ( const NGS_String * self, ctx_t ctx, uint64_t offset )
{
    FUNC_ENTRY ( ctx, rcSRA, rcString, rcAccessing );

    if ( self == NULL )
        INTERNAL_ERROR ( xcSelfNull, "attempt to access NULL NGS_String" );
    else if ( offset == 0 )
        return NGS_StringDuplicate ( self, ctx );
    else
    {
        NGS_String * dup;

        if ( offset > ( uint64_t ) self -> size )
            offset = self -> size;

        TRY ( dup = NGS_StringMake ( ctx, self -> str + offset, self -> size - ( size_t ) offset ) )
        {
            dup -> orig = NGS_StringDuplicate ( self, ctx );
            return dup;
        }
    }

    return NULL;
}

NGS_String * NGS_StringSubstrOffsetSize ( const NGS_String * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcString, rcAccessing );

    if ( self == NULL )
        INTERNAL_ERROR ( xcSelfNull, "attempt to access NULL NGS_String" );
    else if ( offset == 0 && size >= ( uint64_t ) self -> size )
        return NGS_StringDuplicate ( self, ctx );
    else
    {
        NGS_String * dup;

        if ( offset > ( uint64_t ) self -> size )
        {
            offset = self -> size;
            size = 0;
        }
        else
        {
            uint64_t remainder = ( uint64_t ) self -> size - offset;
            if ( size > remainder )
                size = remainder;
        }

        TRY ( dup = NGS_StringMake ( ctx, self -> str + offset, ( size_t ) size ) )
        {
            dup -> orig = NGS_StringDuplicate ( self, ctx );
            return dup;
        }
    }

    return NULL;
}

static NGS_Refcount_vt NGS_String_vt =
{
    NGS_StringWhack
};


/*--------------------------------------------------------------------------
 * NGS_String_v1
 */

#define Self( obj ) \
    ( ( NGS_String* ) ( obj ) )

static
const char* ITF_String_v1_data ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcString, rcAccessing );
    ON_FAIL ( const char * data = NGS_StringData ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();

    return data;
}

static
size_t ITF_String_v1_size ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcString, rcAccessing );
    ON_FAIL ( size_t size = NGS_StringSize ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();

    return size;
}

static
NGS_String_v1* ITF_String_v1_substr ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err,
    size_t offset, size_t size )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcString, rcAccessing );
    ON_FAIL ( NGS_String * ref = NGS_StringSubstrOffsetSize ( Self ( self ), ctx, offset, size ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();

    return ( NGS_String_v1* ) ref;
}

#undef Self

static NGS_String_v1_vt ITF_String_vt =
{
    {
        "NGS_String",
        "NGS_String_v1",
        0,
        & ITF_Refcount_vt . dad
    },

    ITF_String_v1_data,
    ITF_String_v1_size,
    ITF_String_v1_substr
};

/*--------------------------------------------------------------------------
 * NGS_String
 */

/* Make
 */
NGS_String * NGS_StringMake ( ctx_t ctx, const char * data, size_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcString, rcConstructing );

    if ( data == NULL && size > 0 ) /* string(NULL, 0) is a valid string */
        USER_ERROR ( xcParamNull, "bad input" );
    else
    {
        NGS_String * ref = calloc ( 1, sizeof * ref );
        if ( ref == NULL )
            SYSTEM_ERROR ( xcNoMemory, "allocating %zu bytes", ( size_t ) sizeof * ref );
        else
        {
            TRY ( NGS_RefcountInit ( ctx, & ref -> dad, & ITF_String_vt . dad, & NGS_String_vt, "NGS_String", "" ) )
            {
                ref -> str = data;
                ref -> size = size;
                return ref;
            }

            free ( ref );
        }
    }

    return NULL;
}

NGS_String * NGS_StringMakeOwned ( ctx_t ctx, char * owned_data, size_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcString, rcConstructing );

    if ( owned_data == NULL )
        USER_ERROR ( xcParamNull, "bad input" );
    else
    {
        NGS_String * ref = calloc ( 1, sizeof * ref );
        if ( ref == NULL )
            SYSTEM_ERROR ( xcNoMemory, "allocating %zu bytes", ( size_t ) sizeof * ref );
        else
        {
            TRY ( NGS_RefcountInit ( ctx, & ref -> dad, & ITF_String_vt . dad, & NGS_String_vt, "NGS_String", "" ) )
            {
                ref -> owned = owned_data;
                ref -> str = owned_data;
                ref -> size = size;
                return ref;
            }

            free ( ref );
        }
    }

    return NULL;
}

NGS_String * NGS_StringMakeCopy ( ctx_t ctx, const char * temp_data, size_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcString, rcConstructing );

    if ( temp_data == NULL )
        USER_ERROR ( xcParamNull, "bad input" );
    else
    {
        char * owned_data = malloc ( size + 1 );
        if ( owned_data == NULL )
            SYSTEM_ERROR ( xcNoMemory, "allocating %zu bytes", size + 1 );
        else
        {
            memmove ( owned_data, temp_data, size );
            owned_data [ size ] = 0;
            {
                TRY ( NGS_String * ref = NGS_StringMakeOwned ( ctx, owned_data, size ) )
                {
                    return ref;
                }
            }

            free ( owned_data );
        }
    }

    return NULL;
}

NGS_String * NGS_StringFromI64 ( ctx_t ctx, int64_t i )
{
    size_t size;
    char buffer [ 128 ];
    rc_t rc = string_printf ( buffer, sizeof buffer, & size, "%ld", i );

    if ( rc == 0 )
        return NGS_StringMakeCopy ( ctx, buffer, size );

    INTERNAL_ERROR ( xcStringCreateFailed, "rc = %R", rc );
    return NULL;
}


/* MakeNULTerminatedString
 *  allocates a NUL-terminated version of self
 *  returned value should be disposed using "free()"
 */
char * NGS_StringMakeNULTerminatedString ( const NGS_String * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcString, rcConstructing );

    char * nul_term = NULL;

    if ( self == NULL )
        INTERNAL_ERROR ( xcSelfNull, "attempt to access NULL NGS_String" );
    else
    {
        size_t dst_size = self -> size + 1;
        nul_term = malloc ( dst_size );
        if ( nul_term == NULL )
            SYSTEM_ERROR ( xcNoMemory, "allocating %zu bytes", dst_size );
        else
            string_copy ( nul_term, dst_size, self -> str, self -> size );
    }

    return nul_term;
}
