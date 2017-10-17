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

struct KFastDumpCleanupTask;
#define KTASK_IMPL struct KFastDumpCleanupTask
#include <kproc/impl.h>

#include <klib/rc.h>
#include <klib/out.h>
#include <kproc/procmgr.h>

#include "helper.h"

typedef struct KFastDumpCleanupTask
{
    KTask dad;
    locked_file_list to_clean;
    KTaskTicket ticket;
} KFastDumpCleanupTask;


static rc_t KFastDumpCleanupTask_Destroy( KFastDumpCleanupTask * self )
{
    release_locked_file_list( & ( self -> to_clean ) );
    free( self );
    return 0;
}

static rc_t KFastDumpCleanupTask_Execute( KFastDumpCleanupTask * self )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
        ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    else
    {
        KOutMsg( "\nKFastDumpCleanupTask.Execute\n" );
        rc = delete_files_in_locked_file_list( dir, &self -> to_clean );
        KDirectoryRelease( dir );
    }
    return rc;
}
    
static KTask_vt_v1 vtKFastDumpCleanupTask =
{
    /* version 1.0 */
    1, 0,

    /* start minor version 0 methods */
    KFastDumpCleanupTask_Destroy,
    KFastDumpCleanupTask_Execute
    /* end minor version 0 methods */
};

static rc_t add_to_proc_mgr_cleanup( KFastDumpCleanupTask * task )
{
    struct KProcMgr * proc_mgr;
    rc_t rc = KProcMgrMakeSingleton ( &proc_mgr );
    if ( rc != 0 )
        ErrMsg( "cannot access process-manager" );
    else
    {
        rc = KProcMgrAddCleanupTask ( proc_mgr, &( task -> ticket ), ( KTask * )task );
        if ( rc != 0 )
            KTaskRelease ( ( KTask * ) task );
        KProcMgrRelease ( proc_mgr );
    }
    return rc;
}

rc_t Make_FastDump_Cleanup_Task ( struct KFastDumpCleanupTask **task )
{
    rc_t rc = 0;
    KFastDumpCleanupTask * t = malloc ( sizeof * t );
    if ( t == NULL )
        rc = RC ( rcPS, rcMgr, rcInitializing, rcMemory, rcExhausted );
    else
    {
        rc = init_locked_file_list( &( t -> to_clean ), 25 );
        if ( rc == 0 )
        {
            rc = KTaskInit ( &t -> dad,
                            (const union KTask_vt *)&vtKFastDumpCleanupTask,
                            "KFastDumpCleanupTask",
                            "KFastDumpCleanupTask" );
            if ( rc == 0 )
                *task = ( KFastDumpCleanupTask * ) &t -> dad;
            else
                release_locked_file_list( &( t -> to_clean ) );
        }
        if ( rc != 0 )
            free( ( void * ) t );
        else
            rc = add_to_proc_mgr_cleanup( *task );
    }
    return rc;
}

rc_t Add_to_Cleanup_Task ( struct KFastDumpCleanupTask * self, const char * filename )
{
    rc_t rc = 0;
    if ( self == NULL && filename == NULL )
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
    else
        rc = append_to_locked_file_list( &( self -> to_clean ), filename );
    return rc;
}

rc_t Terminate_Cleanup_Task ( struct KFastDumpCleanupTask * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
    else
    {
        struct KProcMgr * proc_mgr;
        rc = KProcMgrMakeSingleton ( &proc_mgr );
        if ( rc != 0 )
            ErrMsg( "cannot access process-manager" );
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
