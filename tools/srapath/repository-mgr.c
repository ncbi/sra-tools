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

#include "repository-mgr.h" /* KRepositoryMgr_DownloadTicket */

#include <kfg/config.h> /* KConfigRelease */

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

rc_t KRepositoryMgr_Make ( const KRepositoryMgr ** mgr ) {
    rc_t rc = 0;

    KConfig * kfg = NULL;

    rc = KConfigMake ( & kfg, NULL );

    if ( rc == 0 )
        rc = KConfigMakeRepositoryMgrRead ( kfg, mgr );

    RELEASE ( KConfig, kfg );

    return rc;
}

rc_t KRepositoryMgr_DownloadTicket ( const KRepositoryMgr * self,
    uint32_t projectId, char * buffer, size_t bsize, size_t * ticket_size )
{
    rc_t rc = 0;

    const KRepository * repo = NULL;

    assert ( self );

    rc = KRepositoryMgrGetProtectedRepository ( self, projectId, & repo );

    if ( rc == 0 )
         rc = KRepositoryDownloadTicket ( repo, buffer, bsize, ticket_size );

    RELEASE ( KRepository, repo );

    return rc;
}
