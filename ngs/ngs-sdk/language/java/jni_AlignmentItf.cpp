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

#include "jni_AlignmentItf.h"
#include "jni_ErrorMsg.hpp"
#include "jni_String.hpp"

#include <ngs/itf/FragmentItf.hpp>
#include <ngs/itf/AlignmentItf.hpp>
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetFragmentId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetFragmentId
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetFragmentBases
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetFragmentBases
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetFragmentQualities
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetFragmentQualities
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    IsPaired
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_AlignmentItf_IsPaired
    (JNIEnv * jenv, jobject jthis, jlong jself)
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

#undef Self

static
AlignmentItf * Align ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( AlignmentItf* ) jself;
}

#define Self( jself ) Align ( jself )

static
jlong Cast ( AlignmentItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

/*
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetAlignmentId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetAlignmentId
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getAlignmentId ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetReferenceSpec
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetReferenceSpec
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getReferenceSpec ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetMappingQuality
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_AlignmentItf_GetMappingQuality
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getMappingQuality ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetReferenceBases
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetReferenceBases
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getReferenceBases ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetReadGroup
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetReadGroup
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetReadId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetReadId
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetClippedFragmentBases
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetClippedFragmentBases
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getClippedFragmentBases ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetClippedFragmentQualities
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetClippedFragmentQualities
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getClippedFragmentQualities ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetAlignedFragmentBases
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetAlignedFragmentBases
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getAlignedFragmentBases ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetAlignmentCategory
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_AlignmentItf_GetAlignmentCategory
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getAlignmentCategory ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetAlignmentPosition
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_AlignmentItf_GetAlignmentPosition
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getAlignmentPosition ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetAlignmentLength
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_AlignmentItf_GetAlignmentLength
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getAlignmentLength ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetIsReversedOrientation
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_AlignmentItf_GetIsReversedOrientation
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> getIsReversedOrientation ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetSoftClip
 * Signature: (JI)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_AlignmentItf_GetSoftClip
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint edge )
{
    try
    {
        return ( jint ) Self ( jself ) -> getSoftClip ( edge );
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetTemplateLength
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_AlignmentItf_GetTemplateLength
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getTemplateLength ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetShortCigar
 * Signature: (JZ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetShortCigar
    ( JNIEnv * jenv, jobject jthis, jlong jself, jboolean clipped )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getShortCigar ( clipped ? true : false );
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetLongCigar
 * Signature: (JZ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetLongCigar
    ( JNIEnv * jenv, jobject jthis, jlong jself, jboolean clipped )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getLongCigar ( clipped ? true : false );
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetRNAOrientation
 * Signature: (J)C
 */
JNIEXPORT jchar JNICALL Java_ngs_itf_AlignmentItf_GetRNAOrientation
    ( JNIEnv * jenv, jobject jthis , jlong jself )
{
    try
    {
        return Self ( jself ) -> getRNAOrientation ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    HasMate
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_AlignmentItf_HasMate
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> hasMate ();
    }
    catch ( ... )
    {
    }

    return false;
}

/*
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetMateAlignmentId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetMateAlignmentId
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getMateAlignmentId ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetMateAlignment
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_AlignmentItf_GetMateAlignment
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        AlignmentItf * new_ref = Self ( jself ) -> getMateAlignment ();
        return Cast ( new_ref );
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetMateReferenceSpec
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_AlignmentItf_GetMateReferenceSpec
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getMateReferenceSpec ();
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
 * Class:     ngs_itf_AlignmentItf
 * Method:    GetMateIsReversedOrientation
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_AlignmentItf_GetMateIsReversedOrientation
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> getMateIsReversedOrientation ();
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
