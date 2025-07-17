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

struct CleanupTask_t;
#define KTASK_IMPL struct CleanupTask_t

#include <kproc/impl.h>

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_kproc_procmgr_
#include <kproc/procmgr.h>
#endif

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_locked_file_list_
#include "locked_file_list.h"
#endif

typedef struct CleanupTask_t {
    KTask dad;
    locked_file_list_t files_to_clean;
    locked_file_list_t dirs_to_clean;
    KTaskTicket ticket;
    bool details;
    bool keep_tmp_files;
} CleanupTask_t;

static rc_t clt_destroy( CleanupTask_t * self ) {
    locked_file_list_release( & ( self -> files_to_clean ), NULL, self -> details ); /* helper.c */
    locked_file_list_release( & ( self -> dirs_to_clean ), NULL, self -> details ); /* helper.c */
    free( self );
    return 0;
}

static rc_t clt_execute( CleanupTask_t * self ) {
    KDirectory * dir;
    rc_t rc;
    if ( self -> details ) { InfoMsg( "CleanupTask: executing..." ); }
    rc = KDirectoryNativeDir( &dir );
    if ( 0 != rc ) {
        ErrMsg( "clt_execute().KDirectoryNativeDir() -> %R", rc );
    } else {
        if ( ! self -> keep_tmp_files ) {
            rc = locked_file_list_delete_files( dir, &self -> files_to_clean, self -> details ); /* helper.c */
            if ( 0 == rc ) {
                rc = locked_file_list_delete_dirs( dir, &self -> dirs_to_clean, self -> details ); /* helper.c */
            }
        }

        {
            rc_t rc2 = KDirectoryRelease( dir );
            if ( 0 != rc2 ) {
                ErrMsg( "clt_execute().KDirectoryRelease() -> %R", rc );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}
    
static KTask_vt_v1 vtCleanupTask = {
    /* version 1.0 */
    1, 0,

    /* start minor version 0 methods */
    clt_destroy,
    clt_execute
    /* end minor version 0 methods */
};

static rc_t clt_add_task_to_proc_mgr( CleanupTask_t * task ) {
    struct KProcMgr * proc_mgr;
    rc_t rc = KProcMgrMakeSingleton ( &proc_mgr );
    if ( 0 != rc ) {
        ErrMsg( "clt_add_task_to_proc_mgr(): cannot access process-manager" );
    } else {
        rc = KProcMgrAddCleanupTask ( proc_mgr, &( task -> ticket ), ( KTask * )task );
        if ( 0 != rc ) {
            rc_t rc2 = KTaskRelease ( ( KTask * ) task );
            if ( 0 != rc2 ) {
                ErrMsg( "clt_add_task_to_proc_mgr().KTaskRelease() -> %R", rc2 );
            }
        } else if ( task -> details ) {
            InfoMsg( "CleanupTask: added to ProcManager" );
        }
        {
            rc_t rc2 = KProcMgrRelease ( proc_mgr );
            if ( 0 != rc2 ) {
                ErrMsg( "clt_add_task_to_proc_mgr().KProcMgrRelease() -> %R", rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

rc_t clt_create( struct CleanupTask_t **task, bool details, bool keep_tmp_files ) {
    rc_t rc = 0;
    CleanupTask_t * t = malloc ( sizeof * t );
    if ( NULL == t ) {
        rc = RC ( rcPS, rcMgr, rcInitializing, rcMemory, rcExhausted );
    } else {
        t -> details = details;
        t -> keep_tmp_files = keep_tmp_files;
        if ( details ) { InfoMsg( "CleanupTask: creating..." ); }
        rc = locked_file_list_init( &( t -> files_to_clean ), 25 ); /* helper.c */
        if ( 0 == rc ) {
            rc = locked_file_list_init( &( t -> dirs_to_clean ), 5 ); /* helper.c */
        }

        if ( 0 == rc ) {
            rc = KTaskInit ( &t -> dad,
                            (const union KTask_vt *)&vtCleanupTask,
                            "CleanupTask",
                            "CleanupTask" );
            if ( 0 == rc ) {
                *task = ( CleanupTask_t * ) &t -> dad;
            } else {
                ErrMsg( "clt_create().KTaskInit() -> %R", rc );
            }
        }

        if ( 0 != rc ) {
            locked_file_list_release( &( t -> files_to_clean ), NULL, details ); /* helper.c */
            locked_file_list_release( &( t -> dirs_to_clean ), NULL, details ); /* helper.c */
            free( ( void * ) t );
        } else {
            rc = clt_add_task_to_proc_mgr( *task ); /* above */
        }
    }
    return rc;
}

rc_t clt_add_file( struct CleanupTask_t * self, const char * filename ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == filename ) {
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
        ErrMsg( "clt_add_file() : %R", rc );
    } else {
        rc = locked_file_list_append( &( self -> files_to_clean ), filename ); /* helper.c */
        if ( self -> details ) { InfoMsg( "clt_add_file( '%s' )", filename ); }
    }
    return rc;
}

rc_t clt_add_directory( struct CleanupTask_t * self, const char * dirname ) {
    rc_t rc = 0;
    if ( self == NULL || dirname == NULL ) {
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
        ErrMsg( "clt_add_directory() : %R", rc );
    } else {
        rc = locked_file_list_append( &( self -> dirs_to_clean ), dirname ); /* helper.c */
        if ( self -> details ) { InfoMsg( "clt_add_directory( '%s' )", dirname ); }
    }
    return rc;
}

rc_t clt_terminate( struct CleanupTask_t * self ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC ( rcPS, rcMgr, rcInitializing, rcParam, rcInvalid );
        ErrMsg( "clt_terminate() : %R", rc );
    } else {
        struct KProcMgr * proc_mgr;
        rc = KProcMgrMakeSingleton ( &proc_mgr );
        if ( rc != 0 ) {
            ErrMsg( "clt_terminate(): cannot access process-manager" );
        } else {
            rc = KProcMgrRemoveCleanupTask ( proc_mgr, &( self -> ticket ) );
            if ( 0 != rc ) {
                ErrMsg( "clt_terminate().KProcMgrRemoveCleanupTask() -> %R", rc );
            } else {
                bool details = self -> details;
                locked_file_list_release( &( self -> files_to_clean ), NULL, details ); /* helper.c */
                locked_file_list_release( &( self -> dirs_to_clean ), NULL, details ); /* helper.c */
                if ( details ) {
                    InfoMsg( "CleanupTask: terminating ..." );
                }
            }

            {
                rc_t rc2 = KProcMgrRelease ( proc_mgr );
                if ( 0 != rc2 ) {
                    ErrMsg( "clt_terminate().KProcMgrRelease() -> %R", rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    if ( 0 == rc ) {
        rc = KTaskRelease ( ( KTask * )self );
        if ( 0 != rc ) {
            ErrMsg( "clt_terminate().KTaskRelease() -> %R", rc );
        }
    }
    return rc;
}
