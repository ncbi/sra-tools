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

#include "jni_ReadCollectionItf.h"
#include "jni_ErrorMsg.hpp"
#include "jni_String.hpp"

#include <ngs/itf/ReadCollectionItf.hpp>
#include <ngs/itf/ReferenceItf.hpp>
#include <ngs/itf/AlignmentItf.hpp>
#include <ngs/itf/ReadGroupItf.hpp>
#include <ngs/itf/ReadItf.hpp>
#include <ngs/itf/StringItf.hpp>

using namespace ngs;

static
ReadCollectionItf * Self ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( ReadCollectionItf* ) jself;
}

inline
jlong Cast ( ReferenceItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

inline
jlong Cast ( AlignmentItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

inline
jlong Cast ( ReadGroupItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

inline
jlong Cast ( ReadItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

/*
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetName
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadCollectionItf_GetName
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StringItf * new_ref = Self ( jself ) -> getName ();
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetReadGroups
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetReadGroups
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        ReadGroupItf * new_ref = Self ( jself ) -> getReadGroups ();
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    HasReadGroup
 * Signature: (JLjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReadCollectionItf_HasReadGroup
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jspec )
{
    try
    {
        const char * spec = JStringData ( jspec, jenv );
        bool ret = Self ( jself ) -> hasReadGroup ( spec );
        JStringReleaseData ( jspec, jenv, spec );
        return ret;
    }
    catch ( ... )
    {
    }

    return 0;
}

/*
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetReadGroup
 * Signature: (JLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetReadGroup
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jspec )
{
    try
    {
        const char * spec = JStringData ( jspec, jenv );
        try
        {
            ReadGroupItf * new_ref = Self ( jself ) -> getReadGroup ( spec );
            JStringReleaseData ( jspec, jenv, spec );
            return Cast ( new_ref );
        }
        catch ( ... )
        {
            JStringReleaseData ( jspec, jenv, spec );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetReferences
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetReferences
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        ReferenceItf * new_ref = Self ( jself ) -> getReferences ();
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    HasReference
 * Signature: (JLjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_ngs_itf_ReadCollectionItf_HasReference
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jspec )
{
    try
    {
        const char * spec = JStringData ( jspec, jenv );
        bool ret = Self ( jself ) -> hasReference ( spec );
        JStringReleaseData ( jspec, jenv, spec );
        return ret;
    }
    catch ( ... )
    {
    }

    return 0;
}

/*
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetReference
 * Signature: (JLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetReference
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jspec )
{
    try
    {
        const char * spec = JStringData ( jspec, jenv );
        try
        {
            ReferenceItf * new_ref = Self ( jself ) -> getReference ( spec );
            JStringReleaseData ( jspec, jenv, spec );
            return Cast ( new_ref );
        }
        catch ( ... )
        {
            JStringReleaseData ( jspec, jenv, spec );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetAlignment
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetAlignment
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jspec )
{
    try
    {
        const char * spec = JStringData ( jspec, jenv );
        try
        {
            AlignmentItf * new_ref = Self ( jself ) -> getAlignment ( spec );
            JStringReleaseData ( jspec, jenv, spec );
            return Cast ( new_ref );
        }
        catch ( ... )
        {
            JStringReleaseData ( jspec, jenv, spec );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetAlignments
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetAlignments
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetAlignmentCount
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetAlignmentCount
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint categories )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getAlignmentCount ( categories );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetAlignmentRange
 * Signature: (JIJJ)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetAlignmentRange
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong first, jlong count, jint categories )
{
    try
    {
        ErrorMsgAssertU64 ( jenv, first );
        ErrorMsgAssertU64 ( jenv, count );

        AlignmentItf * new_ref = Self ( jself ) -> getAlignmentRange ( first, count, categories );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetRead
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetRead
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jspec )
{
    try
    {
        const char * spec = JStringData ( jspec, jenv );
        try
        {
            ReadItf * new_ref = Self ( jself ) -> getRead ( spec );
            JStringReleaseData ( jspec, jenv, spec );
            return Cast ( new_ref );
        }
        catch ( ... )
        {
            JStringReleaseData ( jspec, jenv, spec );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetReads
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetReads
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint categories )
{
    try
    {
        ReadItf * new_ref = Self ( jself ) -> getReads ( categories );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetReadCount
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetReadCount
    ( JNIEnv * jenv, jobject jthis, jlong jself, jint categories )
{
    try
    {
        return ( jlong ) Self ( jself ) -> getReadCount ( categories );
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
 * Class:     ngs_itf_ReadCollectionItf
 * Method:    GetRead2Range
 * Signature: (JIJJ)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadCollectionItf_GetReadRange
    ( JNIEnv * jenv, jobject jthis, jlong jself, jlong first, jlong count, jint categories )
{
    try
    {
        ErrorMsgAssertU64 ( jenv, first );
        ErrorMsgAssertU64 ( jenv, count );

        ReadItf * new_ref = Self ( jself ) -> getReadRange ( first, count, categories );
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
