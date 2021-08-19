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

#include "jni_ErrorMsg.h"

/* prevent generation of static block
   since this file doesn't participate in stack
 */
#define SRC_LOC_DEFINED 1

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/printf.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>


/*--------------------------------------------------------------------------
 * ErrorMsg
 */

/* VThrow
 */
static
void ErrorVThrow ( JNIEnv * jenv, jclass jexcept_cls,
    ctx_t ctx, uint32_t lineno, const char * fmt, va_list args )
{
    const char * msg;

    /* if the error was from C code, get message */
    if ( FAILED () )
        msg = WHAT ();
    else
    {
        /* otherwise, use provided message */
        rc_t rc;
        size_t msg_size = 0;
        char msg_buffer [ 4096 ];

#if _DEBUGGING
        size_t i;
        const char * fname = ctx -> loc -> func;
        assert ( fname != NULL );
        if ( memcmp ( fname, "Java_", sizeof "Java_" - 1 ) == 0 )
            fname += sizeof "Java_" - 1;
        rc = string_printf ( msg_buffer, sizeof msg_buffer, & msg_size, "%s:%u ", fname, lineno );
        for ( i = 0; i < msg_size; ++ i )
        {
            if ( msg_buffer [ i ] == '_' )
                msg_buffer [ i ] = '.';
        }
#endif

        rc = string_vprintf ( & msg_buffer [ msg_size ], sizeof msg_buffer - msg_size, NULL, fmt, args );
        if ( rc != 0 )
            string_printf ( & msg_buffer [ msg_size ], sizeof msg_buffer - msg_size, NULL, "** BAD MESSAGE STRING **" );

        msg = msg_buffer;
    }

    /* create error object, put JVM thread into Exception state */
    ( ( * jenv ) -> ThrowNew ) ( jenv, jexcept_cls, msg );

    /* if error was from C code, pull out stack trace */
    if ( FAILED () )
    {
        jthrowable x = ( * jenv ) -> ExceptionOccurred ( jenv );
        if ( x != NULL )
        {
            /* get the stack depth */
            /* allocate array of StackTraceElement */
            /* access stack trace from C */
            /* walk stack trace, filling in array */
            /* set StackTraceElement on "x" */
        }

        /* don't leave exception on C thread state */
        CLEAR ();
    }
}


/* Throw
 *  throw a Java ngs.ErrorMsg object taken from the C context block
 *  may temporarily take information from point of throw
 */
void ErrorMsgThrow ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, const char *fmt, ... )
{
    va_list args;

    /* locate ErrorMsg class */
    jclass jexcept_cls = ( ( * jenv ) -> FindClass ) ( jenv, "ngs/ErrorMsg" );
    if ( jexcept_cls == NULL )
    {
        /* turn it into a RuntimeException */
        jexcept_cls = ( ( * jenv ) -> FindClass ) ( jenv, "java/lang/RuntimeException" );
    }

    /* package up arguments */
    va_start ( args, fmt );

    /* throw the exception */
    ErrorVThrow ( jenv, jexcept_cls, ctx, lineno, fmt, args );

    /* probably never get here */
    va_end ( args );
}


void RuntimeExceptionThrow ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, const char *fmt, ... )
{
    va_list args;

    /* locate RuntimeException class */
    jclass jexcept_cls = ( ( * jenv ) -> FindClass ) ( jenv, "java/lang/RuntimeException" );

    /* package up arguments */
    va_start ( args, fmt );

    /* throw the exception */
    ErrorVThrow ( jenv, jexcept_cls, ctx, lineno, fmt, args );

    /* probably never get here */
    va_end ( args );
}


/* AssertU32
 * AssertU64
 */
void ErrorMsgAssertU32 ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, jint i )
{
    if ( i < 0 )
    {
        USER_ERROR ( xcIntegerOutOfBounds, "expected unsigned integer but found %d", i );
        ErrorMsgThrow ( jenv, ctx, lineno, "integer sign violation" );
    }
}

void ErrorMsgAssertU64 ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, jlong i )
{
    if ( i < 0 )
    {
        USER_ERROR ( xcIntegerOutOfBounds, "expected unsigned integer but found %ld", i );
        ErrorMsgThrow ( jenv, ctx, lineno, "integer sign violation" );
    }
}
