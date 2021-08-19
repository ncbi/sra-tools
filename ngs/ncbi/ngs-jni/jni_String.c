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

#include "jni_String.h"
#include "jni_ErrorMsg.h"
#include "NGS_String.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/text.h>
#include <klib/printf.h>

#include <sysalloc.h>

#include <string.h>
#include <assert.h>


/*--------------------------------------------------------------------------
 * JString
 * NGS_String
 */



/* Make
 *  make with string_printf format
 */
jstring JStringMake ( ctx_t ctx, JNIEnv * jenv, const char * fmt, ... )
{
    jstring jstr;

    va_list args;
    va_start ( args, fmt );

    jstr = JStringVMake ( ctx, jenv, fmt, args );

    va_end ( args );

    return jstr;
}

jstring JStringVMake ( ctx_t ctx, JNIEnv * jenv, const char * fmt, va_list args )
{
    FUNC_ENTRY ( ctx, rcSRA, rcString, rcConstructing );

    rc_t rc;
    size_t size;
    char buffer [ 4096 ];

    assert ( jenv != NULL );

    /* if the format string is NULL or empty */
    if ( fmt == NULL || fmt [ 0 ] == 0 )
        return ( ( * jenv ) -> NewStringUTF ) ( jenv, "" );

    rc = string_vprintf ( buffer, sizeof buffer, & size, fmt, args );
    if ( rc != 0 )
        INTERNAL_ERROR ( xcStringCreateFailed, "string_printf: rc = %R", rc );
    else
    {
        return ( ( * jenv ) -> NewStringUTF ) ( jenv, buffer );
    }

    /* something is really bad with the string */
    RuntimeExceptionThrow ( jenv, ctx, __LINE__, "failed to make a String" );

    /* should never reach here */
    return NULL;
}


/* Data
 *  access Java String data
 */
const char * JStringData ( jstring jself, ctx_t ctx, JNIEnv * jenv )
{
    jboolean is_copy;
    if ( jself != NULL )
        return ( ( * jenv ) -> GetStringUTFChars ) ( jenv, jself, & is_copy );
    return NULL;
}


/* ReleaseData
 *  release Java String data
 */
void JStringReleaseData ( jstring jself, ctx_t ctx, JNIEnv * jenv, const char * data )
{
    ( ( * jenv ) -> ReleaseStringUTFChars ) ( jenv, jself, data );
}

/* CopyToJString
 *  copy a Java String from an NGS_String
 */
jstring NGS_StringCopyToJString ( const NGS_String * self, ctx_t ctx, JNIEnv * jenv )
{
    size_t size;

    assert ( jenv != NULL );

    /* if the NGS_String is NULL */
    if ( self == NULL )
        return ( ( * jenv ) -> NewStringUTF ) ( jenv, "" );

    TRY ( size = NGS_StringSize ( self, ctx ) )
    {
        const char * data;

        /* if NGS_String is empty */
        if ( size == 0 )
            return ( ( * jenv ) -> NewStringUTF ) ( jenv, "" );

        TRY ( data = NGS_StringData ( self, ctx ) )
        {
            char * copy;

            /* the Java gods did not see fit to provide a version
               of NewString that takes a pointer and a length,
               at least when it comes to UTF-8 character sets... */

            /* an awful, but effective, test to see if the string
               is already NUL terminated. */
            if ( ( ( ( size_t ) & data [ size ] ) & 0xFFF ) != 0 )
            {
                /* we can read this address without fear of a seg-fault.
                   if it's NUL, then we can send the string in directly. */
                if ( data [ size ] == 0 )
                    return ( ( * jenv ) -> NewStringUTF ) ( jenv, data );
            }

            /* create a copy for the benefit of our Java friends */
            copy = malloc ( size + 1 );
            if ( copy == NULL )
                SYSTEM_ERROR ( xcNoMemory, "out of memory allocating a string copy to plug in a NUL byte" );
            else
            {
                jstring jstr;

                memmove ( copy, data, size );
                copy [ size ] = 0;

                jstr = ( ( * jenv ) -> NewStringUTF ) ( jenv, copy );

                free ( copy );

                return jstr;
            }
        }
    }

    /* something is really bad with the string */
    {
        FUNC_ENTRY ( ctx, rcSRA, rcString, rcConstructing );
        RuntimeExceptionThrow ( jenv, ctx, __LINE__, "failed to make a String" );
    }

    /* should never reach here */
    return NULL;
}

/* ConvertToJavaString
 *  make a Java String from an NGS_String
 */
jstring NGS_StringConvertToJString ( NGS_String * self, ctx_t ctx, JNIEnv * jenv )
{
    jstring jstr = NGS_StringCopyToJString ( self, ctx, jenv );
    NGS_StringRelease ( self, ctx );
    return jstr;
}
