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

#include "jni_ReadItf.h"
#include "jni_ErrorMsg.hpp"
#include "jni_String.hpp"

#include <ngs/itf/FragmentItf.hpp>
#include <ngs/itf/ReadItf.hpp>
#include <ngs/itf/StringItf.hpp>

using namespace ngs;

static
FragmentItf * Frag ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( FragmentItf* ) jself;
}

#define Self( jself ) Frag ( jself )

/*
 * Class:     ngs_itf_ReadItf
 * Method:    GetFragmentId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetFragmentId
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetFragmentBases
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetFragmentBases
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetFragmentQualities
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetFragmentQualities
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
 * Class:     ngs_itf_ReadItf
 * Method:    IsPaired
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReadItf_IsPaired
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
 * Class:     ngs_itf_ReadItf
 * Method:    IsAligned
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReadItf_IsAligned
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

    return false;
}

/*
 * Class:     ngs_itf_ReadItf
 * Method:    NextFragment
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReadItf_NextFragment
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> nextFragment ();
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

    return false;
}

#undef Self

static
ReadItf * Read ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( ReadItf* ) jself;
}

#define Self( jself ) Read ( jself )

/*
 * Class:     ngs_itf_ReadItf
 * Method:    GetReadId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetReadId
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getReadId ();
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetNumFragments
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_ReadItf_GetNumFragments
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getNumFragments ();
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
 * Class:     ngs_itf_ReadItf
 * Method:    FragmentIsAligned
 * Signature: (JI)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReadItf_FragmentIsAligned
    (JNIEnv * jenv, jobject jthis, jlong jself, jint fragIdx)
{
    try
    {
        ErrorMsgAssertUnsignedInt ( jenv, fragIdx );
        return ( jboolean ) Self ( jself ) -> fragmentIsAligned ( fragIdx );
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetReadCategory
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_ReadItf_GetReadCategory
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getReadCategory ();
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetReadGroup
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetReadGroup
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getReadGroup ();
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetReadName
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetReadName
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getReadName ();
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetReadBases
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetReadBases
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length )
{
    try
    {
        ErrorMsgAssertUnsignedLong ( jenv, offset );
        StringItf * new_ref = Self ( jself ) -> getReadBases ( offset, length );
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
 * Class:     ngs_itf_ReadItf
 * Method:    GetReadQualities
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadItf_GetReadQualities
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length )
{
    try
    {
        ErrorMsgAssertUnsignedLong ( jenv, offset );
        StringItf * new_ref = Self ( jself ) -> getReadQualities ( offset, length );
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

#undef Self
