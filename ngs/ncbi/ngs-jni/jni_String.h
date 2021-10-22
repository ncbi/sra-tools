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

#ifndef _h_jni_String_
#define _h_jni_String_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#include "jni.h"

#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_String;


/*--------------------------------------------------------------------------
 * JString
 * NGS_String
 */


/* Make
 *  make with string_printf format
 */
jstring JStringMake ( ctx_t ctx, JNIEnv * jenv, const char * fmt, ... );
jstring JStringVMake ( ctx_t ctx, JNIEnv * jenv, const char * fmt, va_list args );


/* Data
 *  access Java String data
 */
const char * JStringData ( jstring jself, ctx_t ctx, JNIEnv * jenv );


/* ReleaseData
 *  release Java String data
 */
void JStringReleaseData ( jstring jself, ctx_t ctx, JNIEnv * jenv, const char * data );


/* CopyToJString
 *  copy a Java String from an NGS_String
 */
jstring NGS_StringCopyToJString ( struct NGS_String const * self, ctx_t ctx, JNIEnv * jenv  );


/* ConvertToJavaString
 *  make a Java String from an NGS_String
 */
jstring NGS_StringConvertToJString ( struct NGS_String * self, ctx_t ctx, JNIEnv * jenv );


#ifdef __cplusplus
}
#endif

#endif /* _h_jni_ErrorMsg_ */
