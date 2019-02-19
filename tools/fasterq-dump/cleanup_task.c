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
    locked_file_list files_to_clean;
    locked_file_list dirs_to_clean;    
    KTaskTicket ticket;
} KFastDumpCleanupTask;


static rc_t KFastDumpCleanupTask_Destroy( KFastDumpCleanupTask * self )
{
    locked_file_list_release( & ( self -> files_to_clean ), NULL ); /* helper.c */
    locked_file_list_release( & ( self -> dirs_to_clean ), NULL ); /* helper.c */
    free( self );
    return 0;
}

static rc_t KFastDumpCleanupTask_Execute( KFastDumpCleanupTask * self )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
        ErrMsg( "cleanup_task.c KFastDumpCleanupTask_Execute().KDirectoryNativeDir() -> %R", rc );
    else
    {
        rc = locked_file_list_delete_files( dir, &self -> files_to_clean ); /* helper.c */
        if ( rc == 0 )
            rc = locked_file_list_delete_dirs( dir, &self -> dirs_to_clean ); /* helper.c */
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
        ErrMsg( "cleanup_task.c add_to_proc_mgr_cleanup(): cannot access process-manager" );
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
        rc = locked_file_list_init( &( t -> files_to_clean ), 25 ); /* helper.c */
        if ( rc == 0 )
            rc = locked_file_list_init( &( t -> dirs_to_clean ), 5 ); /* helper.c */

        if ( rc == 0 )
        {
            rc = KTaskInit ( &t -> dad,
                            (const union KTask_vt *)&vtKFastDumpCleanupTask,
                            "KFastDumpCleanupTask",
                            "KFastDumpCleanupTask" );
            if ( rc == 0 )
                *task = ( KFastDumpCleanupTask * ) &t -> dad;
        }
        
        if ( rc != 0 )
        {
            locked_file_list_release( &( t -> files_to_clean ), NULL ); /* helper.c */
            locked_file_list_release( &( t -> dirs_to_clean ), NULL ); /* helper.c */            
            free( ( void * ) t );
        }
        else
            rc = add_to_proc_mgr_cleanup( *task ); /* above */
    }
    return rc;
}

rc_t Add_File_to_Cleanup_Task ( struct KFastDumpCleanupTask * self, const char * filename )
{
    rc_t rc = 0;
    if ( self == NULL && filename == NULL )
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
    else
        rc = locked_file_list_append( &( self -> files_to_clean ), filename ); /* helper.c */
    return rc;
}

rc_t Add_Directory_to_Cleanup_Task ( struct KFastDumpCleanupTask * self, const char * dirname )
{
    rc_t rc = 0;
    if ( self == NULL && dirname == NULL )
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
    else
        rc = locked_file_list_append( &( self -> dirs_to_clean ), dirname ); /* helper.c */
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
