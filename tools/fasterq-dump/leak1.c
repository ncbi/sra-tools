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

#include <kapp/main.h>
#include <klib/out.h>
#include <klib/vector.h>
#include <klib/time.h>
#include <kfs/directory.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vfs/manager.h>
#include <vfs/path.h>
#include <kproc/thread.h>

#define THREAD_BIG_STACK_SIZE ((size_t)(16u * 1024u * 1024u))

static VPath * acc_2_vpath( const char * acc ) {
    VPath * res = NULL;
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    if ( 0 == rc ) {
        rc = VFSManagerMakePath( vfs_mgr, &res, "%s", acc );
        VFSManagerRelease( vfs_mgr );
    }
    return res;
}

typedef struct thread_data_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * acc;
    KThread * thread;
    uint32_t thread_num;
} thread_data_t;

static rc_t join_the_threads( Vector *threads ) {
    rc_t rc = 0;
    uint32_t i, n = VectorLength( threads );
    for ( i = VectorStart( threads ); i < n; ++i ) {
        thread_data_t * td = VectorGet( threads, i );
        if ( NULL != td ) {
            rc_t rc_thread;
            KThreadWait( td -> thread, &rc_thread );
            if ( 0 != rc_thread ) {
                rc = rc_thread;
            }
            KThreadRelease( td -> thread );
            free( td );
        }
    }
    VectorWhack( threads, NULL, NULL );
    return rc;
}

static rc_t thread_function( const KThread * self, void * data ) {
    rc_t rc = 0;
    thread_data_t * td = data;
    VPath * v_path = acc_2_vpath( td -> acc );
    if ( NULL != v_path ) {
        const VDatabase * db;
        rc_t rc = VDBManagerOpenDBReadVPath( td -> vdb_mgr, &db, NULL, v_path );
        if ( 0 == rc ) {
            KOutMsg( "the database has been opened!\n" );

            KSleepMs( 1000 );

            VDatabaseRelease( db );
        }
        VPathRelease( v_path );
    }
    return rc;
}

rc_t open_in_threads( KDirectory * dir, const VDBManager * vdb_mgr, const char * acc, uint32_t num_threads ) {
    rc_t rc = 0;
    uint32_t thread_nr;
    Vector threads;

    VectorInit( &threads, 0, num_threads );
    for ( thread_nr = 0; 0 == rc && thread_nr < num_threads; ++thread_nr ) {
        thread_data_t * td = calloc( 1, sizeof * td );
        if ( NULL != td ) {
            td -> dir = dir;
            td -> vdb_mgr = vdb_mgr;
            td -> acc = acc;
            td -> thread_num = thread_nr;
            rc = KThreadMakeStackSize( &( td -> thread ), thread_function, td, THREAD_BIG_STACK_SIZE );
            if ( 0 == rc ) {
                rc = VectorAppend( &threads, NULL, td );
            }
        }
    }
    return join_the_threads( &threads );
}

rc_t CC KMain ( int argc, char *argv [] ) {
    rc_t rc;
    if ( argc < 2 ) {
        KOutMsg( "i need an accession ( and it has to be a database ) !\n" );
        rc = 3;
    } else {
        KDirectory * dir;
        uint32_t num_threads = 6;
        
        KOutMsg( "processing '%s'\n", argv[ 1 ] );
        
        rc = KDirectoryNativeDir( &dir );
        if ( 0 == rc ) {
            const VDBManager * vdb_mgr;

            rc = VDBManagerMakeRead( &vdb_mgr, dir );
            if ( 0 == rc ) {

                rc = open_in_threads( dir, vdb_mgr, argv[ 1 ], num_threads );

                VDBManagerRelease( vdb_mgr );
            }
            KDirectoryRelease( dir );
        }
    }
    return rc;
}
