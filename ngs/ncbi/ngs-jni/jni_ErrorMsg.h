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

#ifndef _h_jni_ErrorMsg_
#define _h_jni_ErrorMsg_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * ErrorMsg
 */


/* Throw
 *  throw a Java ngs.ErrorMsg object taken from the C context block
 */
void ErrorMsgThrow ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, const char *msg, ... );


/* AssertUnsignedInt
 */
void ErrorMsgAssertU32 ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, jint i );
#define ErrorMsgAssertUnsignedInt( jenv, ctx, i ) \
    if ( ( i ) < 0 )                               \
        ErrorMsgAssertU32 ( jenv, ctx, __LINE__, i )

/* AssertUnsignedLong
 */
void ErrorMsgAssertU64 ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, jlong i );
#define ErrorMsgAssertUnsignedLong( jenv, ctx, i ) \
    if ( ( i ) < 0 )                               \
        ErrorMsgAssertU64 ( jenv, ctx, __LINE__, i )


/*--------------------------------------------------------------------------
 * RuntimeException
 */


/* Throw
 *  throw a Java RuntimeException object taken from the C context block
 */
void RuntimeExceptionThrow ( JNIEnv * jenv, ctx_t ctx, uint32_t lineno, const char *msg, ... );


/* UNIMPLEMENTED
 *  while the stubs are being brought up
 */
#define JNI_UNIMPLEMENTED( jenv, ctx )                                  \
    RuntimeExceptionThrow ( jenv, ctx, __LINE__, "UNIMPLEMENTED" )


#ifdef __cplusplus
}
#endif

#endif /* _h_jni_ErrorMsg_ */
