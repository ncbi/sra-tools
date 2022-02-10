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

#include "jni_PileupItf.h"
#include "jni_ErrorMsg.hpp"
#include "jni_String.hpp"

#include <ngs/itf/PileupItf.hpp>
#include <ngs/itf/PileupEventItf.hpp>
#include <ngs/itf/StringItf.hpp>

using namespace ngs;

static
jlong Cast ( PileupEventItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

static
PileupEventItf * Event ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( PileupEventItf* ) jself;
}

#define Self( jself ) Event ( jself )

/*
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetMappingQuality
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_PileupItf_GetMappingQuality
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetAlignmentId
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_PileupItf_GetAlignmentId
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetAlignmentPosition
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_PileupItf_GetAlignmentPosition
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetFirstAlignmentPosition
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_PileupItf_GetFirstAlignmentPosition
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getFirstAlignmentPosition ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetLastAlignmentPosition
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_PileupItf_GetLastAlignmentPosition
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getLastAlignmentPosition ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetEventType
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_PileupItf_GetEventType
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getEventType ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetAlignmentBase
 * Signature: (J)C
 */
JNIEXPORT jchar JNICALL Java_ngs_itf_PileupItf_GetAlignmentBase
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jchar ) Self ( jself ) -> getAlignmentBase ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetAlignmentQuality
 * Signature: (J)C
 */
JNIEXPORT jchar JNICALL Java_ngs_itf_PileupItf_GetAlignmentQuality
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jchar ) Self ( jself ) -> getAlignmentQuality ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetInsertionBases
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_PileupItf_GetInsertionBases
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getInsertionBases ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetInsertionQualities
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_PileupItf_GetInsertionQualities
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getInsertionQualities ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetEventRepeatCount
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_PileupItf_GetEventRepeatCount
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getEventRepeatCount ();
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
 * Class:     ngs_itf_PileupEventItf
 * Method:    GetEventIndelType
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_PileupItf_GetEventIndelType
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getEventIndelType ();
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
 * Class:     ngs_itf_PileupItf
 * Method:    NextPileupEvent
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_PileupItf_NextPileupEvent
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> nextPileupEvent ();
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
 * Class:     ngs_itf_PileupEventIteratorItf
 * Method:    ResetPileupEvent
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_ngs_itf_PileupEventIteratorItf_ResetPileupEvent
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        Self ( jself ) -> resetPileupEvent ();
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
}

#undef Self

static
PileupItf * Pile ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( PileupItf* ) jself;
}

#define Self( jself ) Pile ( jself )

/*
 * Class:     ngs_itf_PileupItf
 * Method:    GetReferenceSpec
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_PileupItf_GetReferenceSpec
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
 * Class:     ngs_itf_PileupItf
 * Method:    GetReferencePosition
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_PileupItf_GetReferencePosition
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getReferencePosition ();
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
 * Class:     ngs_itf_PileupItf
 * Method:    GetReferenceBase
 * Signature: (J)C
 */
JNIEXPORT jchar JNICALL Java_ngs_itf_PileupItf_GetReferenceBase
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jchar ) Self ( jself ) -> getReferenceBase ();
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
 * Class:     ngs_itf_PileupItf
 * Method:    GetPileupDepth
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_PileupItf_GetPileupDepth
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jint ) Self ( jself ) -> getPileupDepth ();
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

