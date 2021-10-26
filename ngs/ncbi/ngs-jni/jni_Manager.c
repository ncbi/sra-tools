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

#include "jni_Manager.h"
#include "jni_ErrorMsg.h"
#include "jni_String.h"

#include <kdb/manager.h> /* KDBManager */

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <kfc/rc.h>

#include <kfc/rsrc-global.h>

#include <kns/manager.h>
#include <klib/ncbi-vdb-version.h> /* GetPackageVersion */

#include <vfs/manager.h> /* VFSManager */
#include <vfs/path.h> /* VPath */

#include "NGS_ReadCollection.h"
#include "NGS_ReferenceSequence.h"
#include "../klib/release-vers.h"

#include <assert.h>

static bool have_user_version_string;

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    Initialize
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_Initialize
    ( JNIEnv * jenv, jclass jcls )
{
    DECLARE_FUNC_LOC ( rcSRA, rcMgr, rcConstructing );

    KCtx local_ctx, * ctx = & local_ctx;
    TRY ( KRsrcGlobalInit ( & local_ctx, & s_func_loc, true ) )
    {
        return NULL;
    }

    return JStringMake ( ctx, jenv, "KRsrcGlobalInit failed with rc = %R", local_ctx . rc );
}

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    Shutdown
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_Shutdown
    ( JNIEnv * jenv, jclass jcls )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcDestroying );
    KRsrcGlobalWhack ( ctx );
}

static
void set_app_version_string ( const char * app_version )
{
    // get a KNSManager
    KNSManager * kns;
    rc_t rc = KNSManagerMake ( & kns );
    if ( rc == 0 )
    {
        have_user_version_string = true;
        KNSManagerSetUserAgent ( kns, "ncbi-ngs.%V %s", RELEASE_VERS, app_version );
        KNSManagerRelease ( kns );
    }
}

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    SetAppVersionString
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_SetAppVersionString
    ( JNIEnv * jenv, jclass jcls, jstring japp_version )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcUpdating );

    const char * app_version = JStringData ( japp_version, ctx, jenv );

    set_app_version_string ( app_version );
}

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    OpenReadCollection
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_OpenReadCollection
    ( JNIEnv * jenv, jclass jcls, jstring jspec )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcConstructing );

    NGS_ReadCollection * new_ref = NULL;
    const char * spec = JStringData ( jspec, ctx, jenv );

    if ( ! have_user_version_string )
        set_app_version_string ( "ncbi-ngs: unknown-application" );

    new_ref = NGS_ReadCollectionMake ( ctx, spec );
    if ( FAILED () )
    {
        ErrorMsgThrow ( jenv, ctx, __LINE__, "failed to create ReadCollection from spec '%s'"
                         , spec
            );
        JStringReleaseData ( jspec, ctx, jenv, spec );
        return 0;
    }

    JStringReleaseData ( jspec, ctx, jenv, spec );

    assert ( new_ref != NULL );
    return ( jlong ) ( size_t ) new_ref;
}

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    OpenReferenceSequence
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_OpenReferenceSequence
    ( JNIEnv * jenv, jclass jcls, jstring jspec )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcConstructing );

    NGS_ReferenceSequence* new_ref = NULL;
    const char * spec = JStringData ( jspec, ctx, jenv );

    if ( ! have_user_version_string )
        set_app_version_string ( "ncbi-ngs: unknown-application" );

    new_ref = NGS_ReferenceSequenceMake ( ctx, spec );
    if ( FAILED () )
    {
        ErrorMsgThrow ( jenv, ctx, __LINE__, "failed to create ReferenceSequence from spec '%s'"
                         , spec
            );
        JStringReleaseData ( jspec, ctx, jenv, spec );
        return 0;
    }

    JStringReleaseData ( jspec, ctx, jenv, spec );

    assert ( new_ref != NULL );
    return ( jlong ) ( size_t ) new_ref;
}

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    IsValid
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_IsValid
  ( JNIEnv * jenv, jclass jcls, jstring jspec )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcAccessing );

    jboolean result = false;

    VFSManager * vfs = NULL;
    rc_t rc = VFSManagerMake ( & vfs );

    if ( rc == 0 ) {
        const char * spec = JStringData ( jspec, ctx, jenv );

        VPath * path = NULL;
        rc = VFSManagerMakePath ( vfs, & path, spec );

        if ( rc == 0 ) {
            const KDBManager * kdb = NULL;
            rc = KDBManagerMakeRead ( & kdb, NULL );

            if ( rc == 0 ) {
                KPathType t = KDBManagerPathTypeVP ( kdb, path );
                if (t == kptDatabase || t == kptTable) {
                    result = true;
                }

                KDBManagerRelease ( kdb );
                kdb = NULL;
            }

            VPathRelease ( path );
            path = NULL;
        }

        VFSManagerRelease ( vfs );
        vfs = NULL;
    }

    return result;
}

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    release
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_release
    ( JNIEnv * jenv, jclass jcls, jlong jref )
{
    if ( jref != 0 )
    {
        HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcReleasing );
        NGS_RefcountRelease ( ( void* ) ( size_t ) jref, ctx );
    }
}

/*
 * Class:     gov_nih_nlm_ncbi_ngs_Manager
 * Method:    Version
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_gov_nih_nlm_ncbi_ngs_Manager_Version
  (JNIEnv *jenv, jclass jcls)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcAccessing );
    return JStringMake(ctx, jenv, GetPackageVersion());
}
