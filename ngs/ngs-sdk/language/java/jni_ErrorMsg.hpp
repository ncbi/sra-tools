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

#ifndef _hpp_jni_ErrorMsg_
#define _hpp_jni_ErrorMsg_

#ifndef _h_ngs_itf_err_block_
#include <ngs/itf/ErrBlock.h>
#endif

#include "jni.h"


/*--------------------------------------------------------------------------
 * ErrorMsg
 */


/* Throw
 *  throw a Java ngs.ErrorMsg object taken from the C++ exception
 *
 *  NB - this doesn't actually "throw" anything, but rather
 *  sets state within "jenv" such that an exception is thrown
 *  upon return from JNI
 */
void ErrorMsgThrow ( JNIEnv * jenv, uint32_t type, const char *msg, ... )
    NGS_NOTHROW ();


/* AssertUnsignedInt
 *  since Java doesn't support unsigned integers,
 *  use this to assert that a signed integer is >= 0
 */
void ErrorMsgAssertU32 ( JNIEnv * jenv, jint i )
    NGS_NOTHROW ();

inline
void ErrorMsgAssertUnsignedInt ( JNIEnv * jenv, jint i )
    NGS_NOTHROW ()
{
    if ( i < 0 )
        ErrorMsgAssertU32 ( jenv, i );
};

/* AssertUnsignedLong
 *  since Java doesn't support unsigned integers,
 *  use this to assert that a signed integer is >= 0
 */
void ErrorMsgAssertU64 ( JNIEnv * jenv, jlong i )
    NGS_NOTHROW ();

inline
void ErrorMsgAssertUnsignedLong ( JNIEnv * jenv, jlong i )
    NGS_NOTHROW ()
{
    if ( i < 0 )
        ErrorMsgAssertU64 ( jenv, i );
}


/*--------------------------------------------------------------------------
 * RuntimeException
 */


/* Throw
 *  throw a Java RuntimeException object taken from the C context block
 */
void RuntimeExceptionThrow ( JNIEnv * jenv, const char *msg, ... )
    NGS_NOTHROW ();


/* INTERNAL_ERROR
 */
void JNI_INTERNAL_ERROR ( JNIEnv * jenv, const char * fmt, ... )
    NGS_NOTHROW ();


/* UNIMPLEMENTED
 *  while the stubs are being brought up
 */
inline
void JNI_UNIMPLEMENTED ( JNIEnv * jenv )
    NGS_NOTHROW ()
{
    RuntimeExceptionThrow ( jenv, "UNIMPLEMENTED" );
}

#endif /* _hpp_jni_ErrorMsg_ */
