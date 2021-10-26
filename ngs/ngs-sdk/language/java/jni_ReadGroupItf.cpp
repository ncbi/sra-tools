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

#include "jni_ReadGroupItf.h"
#include "jni_ErrorMsg.hpp"
#include "jni_String.hpp"

#include <ngs/itf/ReadGroupItf.hpp>
#include <ngs/itf/ReadItf.hpp>
#include <ngs/itf/StatisticsItf.hpp>
#include <ngs/itf/StringItf.hpp>

using namespace ngs;

static
ReadGroupItf * Self ( size_t jself )
{
    if ( jself == 0 )
        throw ErrorMsg ( "NULL self parameter" );

    return ( ReadGroupItf* ) jself;
}

inline
jlong Cast ( StatisticsItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}

#if READ_GROUP_SUPPORTS_READS
inline
jlong Cast ( ReadItf * obj )
{
    return ( jlong ) ( size_t ) obj;
}
#endif
    
/*
 * Class:     ngs_itf_ReadGroupItf
 * Method:    GetName
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_ngs_itf_ReadGroupItf_GetName
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
 * Class:     ngs_itf_ReadGroupItf
 * Method:    GetStatistics
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadGroupItf_GetStatistics
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    try
    {
        StatisticsItf * new_ref = Self ( jself ) -> getStatistics ();
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

#if READ_GROUP_SUPPORTS_READS

/*
 * Class:     ngs_itf_ReadGroupItf
 * Method:    GetRead
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadGroupItf_GetRead
    ( JNIEnv * jenv, jobject jthis, jlong jself, jstring jreadId )
{
    try
    {
        const char * readId = JStringData ( jreadId, jenv );
        try
        {
            ReadItf * new_ref = Self ( jself ) -> getRead ( readId );
            JStringReleaseData ( jreadId, jenv, readId );
            return Cast ( new_ref );
        }
        catch ( ... )
        {
            JStringReleaseData ( jreadId, jenv, readId );
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
 * Class:     ngs_itf_ReadGroupItf
 * Method:    GetReads
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_ReadGroupItf_GetReads
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

#endif
