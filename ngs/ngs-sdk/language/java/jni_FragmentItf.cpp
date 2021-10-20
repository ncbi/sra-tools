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

#include "jni_FragmentItf.h"
#include "jni_ErrorMsg.hpp"
#include "jni_String.hpp"

#include <ngs/itf/FragmentItf.hpp>
#include <ngs/itf/StringItf.hpp>

using namespace ngs;

static
FragmentItf * Self ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( FragmentItf* ) jself;
}

/*
 * Class:     ngs_itf_FragmentItf
 * Method:    GetFragmentId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_FragmentItf_GetFragmentId
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getFragmentId ();
        return StringItfConvertToJString ( new_ref, jenv );
    }
    catch ( ErrorMsg & x )
    {
        ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
    }
    catch ( std :: exception & x )
    {
        ErrorMsgThrow ( jenv, xt_runtime, x . what () );
    }
    catch ( ... )
    {
        JNI_INTERNAL_ERROR ( jenv, "%s", __func__ );
    }

    return 0;
}

/*
 * Class:     ngs_itf_FragmentItf
 * Method:    GetFragmentBases
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_FragmentItf_GetFragmentBases
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length )
{
    try
    {
        ErrorMsgAssertUnsignedLong ( jenv, offset );
        StringItf * new_ref = Self ( jself ) -> getFragmentBases ( offset, length );
        return StringItfConvertToJString ( new_ref, jenv );
    }
    catch ( ErrorMsg & x )
    {
        ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
    }
    catch ( std :: exception & x )
    {
        ErrorMsgThrow ( jenv, xt_runtime, x . what () );
    }
    catch ( ... )
    {
        JNI_INTERNAL_ERROR ( jenv, "%s", __func__ );
    }

    return 0;
}

/*
 * Class:     ngs_itf_FragmentItf
 * Method:    GetFragmentQualities
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_FragmentItf_GetFragmentQualities
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length )
{
    try
    {
        ErrorMsgAssertUnsignedLong ( jenv, offset );
        StringItf * new_ref = Self ( jself ) -> getFragmentQualities ( offset, length );
        return StringItfConvertToJString ( new_ref, jenv );
    }
    catch ( ErrorMsg & x )
    {
        ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
    }
    catch ( std :: exception & x )
    {
        ErrorMsgThrow ( jenv, xt_runtime, x . what () );
    }
    catch ( ... )
    {
        JNI_INTERNAL_ERROR ( jenv, "%s", __func__ );
    }

    return 0;
}

/*
 * Class:     ngs_itf_FragmentItf
 * Method:    IsPaired
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_FragmentItf_IsPaired
    (JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> isPaired ();
    }
    catch ( ErrorMsg & x )
    {
        ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
    }
    catch ( std :: exception & x )
    {
        ErrorMsgThrow ( jenv, xt_runtime, x . what () );
    }
    catch ( ... )
    {
        JNI_INTERNAL_ERROR ( jenv, "%s", __func__ );
    }
    
    return 0;
}

/*
 * Class:     ngs_itf_FragmentItf
 * Method:    IsAligned
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_FragmentItf_IsAligned
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> isAligned ();
    }
    catch ( ErrorMsg & x )
    {
        ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
    }
    catch ( std :: exception & x )
    {
        ErrorMsgThrow ( jenv, xt_runtime, x . what () );
    }
    catch ( ... )
    {
        JNI_INTERNAL_ERROR ( jenv, "%s", __func__ );
    }

    return ( jboolean ) 0;
}

