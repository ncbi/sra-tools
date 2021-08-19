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

#include "jni_ErrorMsg.hpp"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>


/*--------------------------------------------------------------------------
 * ErrorMsg
 */

/* Throw
 */
static
void ErrorMsgThrow ( JNIEnv * jenv, jclass jexcept_cls, const char * fmt, va_list args )
    NGS_NOTHROW ()
{
    // expand message into buffer
    char msg [ 4096 ];
    int size = vsnprintf ( msg, sizeof msg, fmt, args );

    // ignore overruns
    if ( size < 0 || ( size_t ) size >= sizeof msg )
        strcpy ( & msg [ sizeof msg - 4 ], "..." );

    // create error object, put JVM thread into Exception state
    jenv -> ThrowNew ( jexcept_cls, msg );
}


/* Throw
 */
static
void ErrorMsgThrow ( JNIEnv * jenv, uint32_t type, const char * fmt, va_list args )
    NGS_NOTHROW ()
{
    jclass jexcept_cls = 0;

    switch ( type )
    {
    case xt_error_msg:
        jexcept_cls = jenv -> FindClass ( "ngs/ErrorMsg" );
        break;
    }

    if ( jexcept_cls == 0 )
        jexcept_cls = jenv -> FindClass ( "java/lang/RuntimeException" );

    ErrorMsgThrow ( jenv, jexcept_cls, fmt, args );
}


/* Throw
 *  throw a Java ngs.ErrorMsg object taken from the C context block
 *  may temporarily take information from point of throw
 */
void ErrorMsgThrow ( JNIEnv * jenv, uint32_t type, const char *fmt, ... )
    NGS_NOTHROW ()
{
    va_list args;
    va_start ( args, fmt );

    ErrorMsgThrow ( jenv, type, fmt, args );

    va_end ( args );
}


void RuntimeExceptionThrow ( JNIEnv * jenv, const char *fmt, ... )
    NGS_NOTHROW ()
{
    va_list args;
    va_start ( args, fmt );

    ErrorMsgThrow ( jenv, xt_runtime, fmt, args );

    va_end ( args );
}


/* INTERNAL_ERROR
 */
void JNI_INTERNAL_ERROR ( JNIEnv * jenv, const char * fmt, ... )
    NGS_NOTHROW ()
{
    va_list args;
    va_start ( args, fmt );

    char msg [ 4096 ];
    int psize = snprintf ( msg, sizeof msg, "INTERNAL ERROR: " );
    int size = vsnprintf ( & msg [ psize ], sizeof msg - psize, fmt, args );
    if ( size < 0 || ( size_t ) size >= sizeof msg - psize )
        strcpy ( & msg [ sizeof msg - 4 ], "..." );

    ErrorMsgThrow ( jenv, xt_runtime, "%s", msg );

    va_end ( args );
}


/* AssertU32
 * AssertU64
 */
void ErrorMsgAssertU32 ( JNIEnv * jenv, jint i )
    NGS_NOTHROW ()
{
    if ( i < 0 )
        ErrorMsgThrow ( jenv, xt_error_msg, "integer sign violation" );
}

void ErrorMsgAssertU64 ( JNIEnv * jenv, jlong i )
    NGS_NOTHROW ()
{
    if ( i < 0 )
        ErrorMsgThrow ( jenv, xt_error_msg, "integer sign violation" );
}
