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

struct KCleanupTask;
#define KTASK_IMPL struct KCleanupTask
#include <kproc/impl.h>

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_kproc_procmgr_
#include <kproc/procmgr.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_cmn_
#include "cmn.h"
#endif

typedef struct KCleanupTask
{
    KTask dad;
    KTaskTicket ticket;
    char tmp_file[ 4096 ];
} KCleanupTask;


static rc_t KCleanupTask_Destroy( KCleanupTask * self )
{
    free( self );
    return 0;
}

static rc_t KCleanupTask_Execute( KCleanupTask * self )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
        ErrMsg( "cleanup_task.c KCleanupTask_Execute().KDirectoryNativeDir() -> %R", rc );
    else
    {
        /* remove the temp_file */
        rc = KDirectoryRemove ( dir, true, "%s", self -> tmp_file );
        if ( rc != 0 )
            ErrMsg( "cleanup_task.c KCleanupTask_Execute() : %R", rc );
            
        KDirectoryRelease( dir );
    }
    return rc;
}
    
static KTask_vt_v1 vtCleanupTask =
{
    /* version 1.0 */
    1, 0,

    /* start minor version 0 methods */
    KCleanupTask_Destroy,
    KCleanupTask_Execute
    /* end minor version 0 methods */
};

rc_t Make_Cleanup_Task ( KCleanupTask **task, struct KProcMgr * proc_mgr, const char * tmp_file )
{
    rc_t rc = 0;
    KCleanupTask * t = malloc ( sizeof * t );
    if ( t == NULL )
        rc = RC ( rcPS, rcMgr, rcInitializing, rcMemory, rcExhausted );
    else
    {
        rc = KTaskInit ( &t -> dad,
                        (const union KTask_vt *)&vtCleanupTask,
                        "KCleanupTask",
                        "KCleanupTask" );
        if ( rc == 0 )
        {
            /* copy the tmp-file into the structure */
            string_copy ( t -> tmp_file, sizeof t -> tmp_file, tmp_file, string_size( tmp_file ) );

            rc = KProcMgrAddCleanupTask ( proc_mgr, &( t -> ticket ), ( KTask * )t );
            if ( rc == 0 )
                *task = ( KCleanupTask * ) &t -> dad;
        }
        
        if ( rc != 0 )
            free( ( void * ) t );
    }
    return rc;
}

rc_t Terminate_Cleanup_Task ( KCleanupTask * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
    else
    {
        struct KProcMgr * proc_mgr;
        rc = KProcMgrMakeSingleton ( &proc_mgr );
        if ( rc != 0 )
            ErrMsg( "cleanup_task.c Terminate_Cleanup_Task(): cannot access process-manager" );
        else
        {
            rc = KProcMgrRemoveCleanupTask ( proc_mgr, &( self -> ticket ) );
            KProcMgrRelease ( proc_mgr );
        }
    }
    if ( rc == 0 )
        KTaskRelease ( ( KTask * )self );
    return rc;
}
