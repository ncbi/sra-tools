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

#include "jni_Refcount.h"
#include "jni_ErrorMsg.hpp"

#include <ngs/itf/Refcount.hpp>

using namespace ngs;

inline
OpaqueRefcount * Self ( size_t jself )
{
    return reinterpret_cast <  OpaqueRefcount* > ( jself );
}

inline
jlong Cast ( void * obj )
{
    return ( jlong ) ( size_t ) obj;
}

/*
 * Class:     ngs_itf_Refcount
 * Method:    Duplicate
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_ngs_itf_Refcount_Duplicate
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    if ( jself != 0 )
    {
        try
        {
            OpaqueRefcount * self = Self ( jself );
            void * val = self -> Duplicate ();
            return Cast ( val );
        }
        catch ( ErrorMsg & x )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
        }
        catch ( std :: exception & x )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
        }
        catch ( ... )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, "unknown error" );
        }
    }

    return 0;
}

/*
 * Class:     ngs_itf_Refcount
 * Method:    Release
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_ngs_itf_Refcount_Release
    ( JNIEnv * jenv, jobject jthis, jlong jself )
{
    if ( jself != 0 )
    {
        try
        {
            OpaqueRefcount * self = Self ( jself );
            self -> Release ();
        }
        catch ( ErrorMsg & x )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
        }
        catch ( std :: exception & x )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
        }
        catch ( ... )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, "unknown error" );
        }
    }
}

/*
 * Class:     ngs_itf_Refcount
 * Method:    ReleaseRef
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_ngs_itf_Refcount_ReleaseRef
    ( JNIEnv * jenv, jclass jcls, jlong jref )
{
    if ( jref != 0 )
    {
        try
        {
            OpaqueRefcount * ref = Self ( jref );
            ref -> Release ();
        }
        catch ( ErrorMsg & x )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
        }
        catch ( std :: exception & x )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, x . what () );
        }
        catch ( ... )
        {
            ErrorMsgThrow ( jenv, xt_error_msg, "unknown error" );
        }
    }
}
