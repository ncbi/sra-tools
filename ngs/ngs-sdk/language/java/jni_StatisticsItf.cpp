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

#include "jni_StatisticsItf.h"
#include "jni_String.hpp"
#include "jni_ErrorMsg.hpp"

#include <ngs/itf/StatisticsItf.hpp>
#include <ngs/itf/StringItf.hpp>

using namespace ngs;

inline
StatisticsItf * Self ( size_t jself )
{
    return reinterpret_cast <  StatisticsItf* > ( jself );
}

/*
 * Class:     ngs_itf_StatisticsItf
 * Method:    GetValueType
 * Signature: (JLjava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_ngs_itf_StatisticsItf_GetValueType
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jpath )
{
    try
    {
        const char * path = JStringData ( jpath, jenv );
        try
        {
            uint32_t val = Self ( jself ) -> getValueType ( path );
            JStringReleaseData ( jpath, jenv, path );
            return ( jint ) val;
        }
        catch ( ... )
        {
            JStringReleaseData ( jpath, jenv, path );
            throw;
        }
    }
    catch ( ... )
    {
    }

    return 0;
}

/*
 * Class:     ngs_itf_StatisticsItf
 * Method:    GetAsString
 * Signature: (JLjava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_StatisticsItf_GetAsString
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jpath )
{
    try
    {
        const char * path = JStringData ( jpath, jenv );
        try
        {
            StringItf * new_ref = Self ( jself ) -> getAsString ( path );
            JStringReleaseData ( jpath, jenv, path );
            return StringItfConvertToJString ( new_ref, jenv );
        }
        catch ( ... )
        {
            JStringReleaseData ( jpath, jenv, path );
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
 * Class:     ngs_itf_StatisticsItf
 * Method:    GetAsI64
 * Signature: (JLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_StatisticsItf_GetAsI64
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jpath )
{
    try
    {
        const char * path = JStringData ( jpath, jenv );
        try
        {
            int64_t val = Self ( jself ) -> getAsI64 ( path );
            JStringReleaseData ( jpath, jenv, path );
            return ( jlong ) val;
        }
        catch ( ... )
        {
            JStringReleaseData ( jpath, jenv, path );
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
 * Class:     ngs_itf_StatisticsItf
 * Method:    GetAsU64
 * Signature: (JLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_StatisticsItf_GetAsU64
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jpath )
{
    try
    {
        const char * path = JStringData ( jpath, jenv );
        try
        {
            uint64_t val = Self ( jself ) -> getAsU64 ( path );
            if ( ( int64_t ) val < 0 )
                throw ErrorMsg ( "unsigned value too large for Java long" );
            JStringReleaseData ( jpath, jenv, path );
            return ( jlong ) val;
        }
        catch ( ... )
        {
            JStringReleaseData ( jpath, jenv, path );
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
 * Class:     ngs_itf_StatisticsItf
 * Method:    GetAsDouble
 * Signature: (JLjava/lang/String;)D
 */
JNIEXPORT jdouble JNICALL Java_ngs_itf_StatisticsItf_GetAsDouble
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jpath )
{
    try
    {
        const char * path = JStringData ( jpath, jenv );
        try
        {
            double val = Self ( jself ) -> getAsDouble ( path );
            JStringReleaseData ( jpath, jenv, path );
            return ( jdouble ) val;
        }
        catch ( ... )
        {
            JStringReleaseData ( jpath, jenv, path );
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
 * Class:     ngs_itf_StatisticsItf
 * Method:    NextPath
 * Signature: (JLjava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_StatisticsItf_NextPath
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jpath )
{
    try
    {
        const char * path = jpath ? JStringData ( jpath, jenv ) : "";
        try
        {
            StringItf * new_ref = Self ( jself ) -> nextPath ( path );
            if ( jpath != NULL )
                JStringReleaseData ( jpath, jenv, path );
            return new_ref ? StringItfConvertToJString ( new_ref, jenv ) : 0;
        }
        catch ( ... )
        {
            if ( jpath != NULL )
                JStringReleaseData ( jpath, jenv, path );
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
