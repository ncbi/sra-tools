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

#include "jni_String.hpp"
#include "jni_ErrorMsg.hpp"

#include <ngs/itf/StringItf.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


using namespace ngs;


/*--------------------------------------------------------------------------
 * JString
 * StringItf
 */



/* Make
 *  make with string_printf format
 */
jstring JStringMake ( JNIEnv * jenv, const char * fmt, ... )
{
    jstring jstr;

    va_list args;
    va_start ( args, fmt );

    jstr = JStringVMake ( jenv, fmt, args );

    va_end ( args );

    return jstr;
}

jstring JStringVMake ( JNIEnv * jenv, const char * fmt, va_list args )
{
    assert ( jenv != 0 );

    /* if the format string is NULL or empty */
    if ( fmt == 0 || fmt [ 0 ] == 0 )
        return jenv -> NewStringUTF ( "" );

    char buffer [ 4096 ];
    int size = vsnprintf ( buffer, sizeof buffer, fmt, args );
    if ( size < 0 )
        JNI_INTERNAL_ERROR ( jenv, "failed to make a String ( bad format or string too long )" );
    else if ( ( size_t ) size >= sizeof buffer )
        JNI_INTERNAL_ERROR ( jenv, "failed to make a String ( string too long )" );
    else
        return jenv -> NewStringUTF ( buffer );

    return 0;
}


/* Data
 *  access Java String data
 */
const char * JStringData ( jstring jself, JNIEnv * jenv )
{
    jboolean is_copy;
    if ( jself != NULL )
        return jenv -> GetStringUTFChars ( jself, & is_copy );
    return NULL;
}


/* ReleaseData
 *  release Java String data
 */
void JStringReleaseData ( jstring jself, JNIEnv * jenv, const char * data )
{
    jenv -> ReleaseStringUTFChars ( jself, data );
}

/* CopyToJString
 *  copy a Java String from an NGS_String
 */
jstring StringItfCopyToJString ( const ngs :: StringItf * self, JNIEnv * jenv )
{
    assert ( jenv != 0 );

    /* if the NGS_String is NULL */
    if ( self == 0 )
        return jenv -> NewStringUTF ( "" );

    size_t size = self -> size ();
    if ( size == 0 )
        return jenv -> NewStringUTF ( "" );

    const char * data = self -> data ();

    /* the Java gods did not see fit to provide a version
       of NewString that takes a pointer and a length,
       at least when it comes to UTF-8 character sets... */

    /* an awful, but effective, test to see if the string
       is already NUL terminated:
       1. we assume that buffer SIZE is aligned to 12 bits
       2. we check if string size is less than buffer size,
          i.e. if data[size] belongs buffer of not
       3. if data[size] belongs to the buffer,
          we can check if string is followed by NULL byte */
    if ( ( ( ( size_t ) & data [ size ] ) & 0xFFF ) != 0 )
    {
        /* we can read this address without fear of a seg-fault.
           if it's NUL, then we can send the string in directly.
           NB: valgrind may complain about this line, but it is okay */
        if ( data [ size ] == 0 )
            return jenv -> NewStringUTF ( data );
    }

    /* create a copy for the benefit of our Java friends */
    char * copy = ( char* ) malloc ( size + 1 );
    if ( copy == 0 )
    {
        RuntimeExceptionThrow ( jenv, "failed to make a String ( out of memory )" );
        return 0;
    }

    memmove ( copy, data, size );
    copy [ size ] = 0;

    jstring jstr = jenv -> NewStringUTF ( copy );

    free ( ( void* ) copy );

    return jstr;
}

/* ConvertToJavaString
 *  make a Java String from an NGS_String
 */
jstring StringItfConvertToJString ( ngs :: StringItf * self, JNIEnv * jenv )
{
    jstring jstr = StringItfCopyToJString ( self, jenv );
    self -> Release ();
    return jstr;
}
