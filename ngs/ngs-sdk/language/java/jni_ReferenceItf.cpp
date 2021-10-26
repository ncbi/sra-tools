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

#include "jni_ReferenceItf.h"
#include "jni_ErrorMsg.hpp"
#include "jni_String.hpp"

#include <ngs/itf/ReferenceItf.hpp>
#include <ngs/itf/AlignmentItf.hpp>
#include <ngs/itf/PileupItf.hpp>
#include <ngs/itf/StringItf.hpp>

using namespace ngs;

static
ReferenceItf * Self ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( ReferenceItf* ) jself;
}

inline
jlong Cast ( AlignmentItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

inline
jlong Cast ( PileupItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

/*
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetCommonName
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReferenceItf_GetCommonName
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getCommonName ();
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetCanonicalName
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReferenceItf_GetCanonicalName
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getCanonicalName ();
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetIsCircular
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReferenceItf_GetIsCircular
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> getIsCircular ();
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetIsLocal
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReferenceItf_GetIsLocal
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jboolean ) Self ( jself ) -> getIsLocal ();
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetLength
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetLength
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getLength ();
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetReferenceBases
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReferenceItf_GetReferenceBases
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length )
{
    try
    {
        ErrorMsgAssertUnsignedLong ( jenv, offset );
        StringItf * new_ref = Self ( jself ) -> getReferenceBases ( offset, length );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetReferenceChunk
 * Signature: (JJJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReferenceItf_GetReferenceChunk
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length )
{
    try
    {
        ErrorMsgAssertUnsignedLong ( jenv, offset );
        StringItf * new_ref = Self ( jself ) -> getReferenceChunk ( offset, length );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetAlignmentCount
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetAlignmentCount
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint categories )
{
    try
    {
        return Self ( jself ) -> getAlignmentCount ( categories );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetAlignment
 * Signature: (JLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetAlignment
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring alignmentId )
{
    try
    {
        const char * id = JStringData ( alignmentId, jenv );
        try
        {
            AlignmentItf * new_ref = Self ( jself ) -> getAlignment ( id );
            JStringReleaseData ( alignmentId, jenv, id );
            return Cast ( new_ref );
        }
        catch ( ... )
        {
            JStringReleaseData ( alignmentId, jenv, id );
            throw;
        }
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetAlignments
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetAlignments
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint categories )
{
    try
    {
        AlignmentItf * new_ref = Self ( jself ) -> getAlignments ( categories );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetAlignmentSlice
 * Signature: (JJJI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetAlignmentSlice
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length, jint categories )
{
    try
    {
        AlignmentItf * new_ref = Self ( jself ) -> getAlignmentSlice ( offset, length, categories );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetFilteredAlignmentSlice
 * Signature: (JJJIII)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetFilteredAlignmentSlice
( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length, jint categories, jint filters, jint mappingQuality )
{
    try
    {
        AlignmentItf * new_ref = Self ( jself ) -> getFilteredAlignmentSlice ( offset, length, categories, filters, mappingQuality );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetPileups
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetPileups
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint categories )
{
    try
    {
        PileupItf * new_ref = Self ( jself ) -> getPileups ( categories );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetFilteredPileups
 * Signature: (JIII)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetFilteredPileups
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint categories, jint filters, jint map_qual )
{
    try
    {
        PileupItf * new_ref = Self ( jself ) -> getFilteredPileups ( categories, filters, map_qual );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetPileupSlice
 * Signature: (JIJJ)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetPileupSlice
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length, jint categories )
{
    try
    {
        PileupItf * new_ref = Self ( jself ) -> getPileupSlice ( offset, length, categories );
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
 * Class:     ngs_itf_ReferenceItf
 * Method:    GetFilteredPileupSlice
 * Signature: (JJJIII)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReferenceItf_GetFilteredPileupSlice
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong offset, jlong length, jint categories, jint filters, jint map_qual )
{
    try
    {
        PileupItf * new_ref = Self ( jself ) -> getFilteredPileupSlice ( offset, length, categories, filters, map_qual );
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

