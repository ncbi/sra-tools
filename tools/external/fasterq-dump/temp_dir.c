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

#include "temp_dir.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_helper_
#include "helper.h"   /* ends_in_slash() */
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_kproc_procmgr_
#include <kproc/procmgr.h>
#endif

#define HOSTNAMELEN 128
#define DFLT_HOST_NAME "host"
#define DFLT_PATH_LEN 4096

typedef struct temp_dir_t {
    char hostname[ HOSTNAMELEN ];
    char path[ DFLT_PATH_LEN ];
    uint32_t pid;
} temp_dir_t;

void destroy_temp_dir( struct temp_dir_t * self ) {
    if ( NULL != self ) {
        free( (void *)self );
    }
}

static rc_t get_pid_and_hostname( temp_dir_t * self ) {
    struct KProcMgr * proc_mgr;
    rc_t rc = KProcMgrMakeSingleton ( &proc_mgr );
    if ( 0 != rc ) {
        ErrMsg( "cannot access process-manager" );
    } else {
        rc = KProcMgrGetPID ( proc_mgr, & self -> pid );
        if ( 0 == rc ) {
            rc = KProcMgrGetHostName ( proc_mgr, self -> hostname, sizeof self -> hostname );
            if ( 0 != rc ) {
                size_t num_writ;
                rc = string_printf( self -> hostname, sizeof self -> hostname, &num_writ, DFLT_HOST_NAME );
            }
        }
        KProcMgrRelease ( proc_mgr );
    }
    return rc;
}

static void append_slash( char * path, size_t maxlen ) {
    size_t l = strlen( path );
    if ( l < maxlen ) {
        path[ l ] = '/';
        path[ l + 1 ] = 0;
    }
}

static rc_t generate_dflt_path( temp_dir_t * self, const KDirectory * dir ) {
    rc_t rc = KDirectoryResolvePath( dir,
                                     true /* absolute */,
                                     &( self -> path[ 0 ] ),
                                     sizeof self -> path,
                                     "fasterq.tmp.%s.%u", self -> hostname, self -> pid );
    if ( 0 != rc ) {
        ErrMsg( "temp_dir.c generate_dflt_path() -> %R", rc );
    } else {
        /* we have to add a slash! ( because KDirectoryResolvePath() does not want to... ) */
        append_slash( self -> path, sizeof( self -> path ) - 2 );
    }
    /*
    size_t num_writ;
    return string_printf( self -> path, sizeof self -> path, &num_writ,
                         "fasterq.tmp.%s.%u/", self -> hostname, self -> pid );
    */
    return rc;
}

static rc_t generate_sub_path( temp_dir_t * self, const char * requested, const KDirectory * dir ) {
    rc_t rc;
    bool es = ends_in_slash( requested );
    if ( es ) {
        rc = KDirectoryResolvePath( dir,
                                    true /* absolute */,
                                    &( self -> path[ 0 ] ),
                                    sizeof self -> path,
                                    "%sfasterq.tmp.%s.%u/", requested, self -> hostname, self -> pid );
    } else {
        rc = KDirectoryResolvePath( dir,
                                    true /* absolute */,
                                    &( self -> path[ 0 ] ),
                                    sizeof self -> path,
                                    "%s/fasterq.tmp.%s.%u/", requested, self -> hostname, self -> pid );
    }
    if ( 0 != rc ) {
        ErrMsg( "temp_dir.c generate_sub_path() -> %R", rc );
    } else {
        /* we have to add a slash! ( because KDirectoryResolvePath() does not want to... ) */
        append_slash( self -> path, sizeof( self -> path ) - 2 );
    }
    return rc;
}

rc_t make_temp_dir( struct temp_dir_t ** obj, const char * requested, KDirectory * dir ) {
    rc_t rc = 0;
    if ( NULL == obj || NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c make_temp_dir() -> %R", rc );
    } else {
        temp_dir_t * o = calloc( 1, sizeof * o );
        if ( NULL == o ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "temp_dir.c make_temp_dir().calloc( %d ) -> %R", ( sizeof * o ), rc );
        } else {
            rc = get_pid_and_hostname( o );
            if ( 0 == rc ) {
                if ( requested == NULL ) {
                    rc = generate_dflt_path( o, dir );
                } else {
                    rc = generate_sub_path( o, requested, dir );
                }
            }
            if ( 0 == rc ) {
                if ( !dir_exists( dir, "%s", o -> path ) ) { /* helper.c */
                    KCreateMode create_mode = kcmCreate | kcmParents;
                    rc = KDirectoryCreateDir ( dir, 0775, create_mode, "%s", o -> path );
                    if ( 0 != rc ) {
                        ErrMsg( "temp_dir.c make_temp_dir().KDirectoryCreateDir( '%s' ) -> %R", o -> path, rc );
                    }
                }
            }
            if ( 0 == rc ) {
                *obj = o;
            } else {
                destroy_temp_dir( o );
            }
        }
    }
    return rc;
}

static const char * UNKNOWN_DIR = "unknown";

const char * get_temp_dir( struct temp_dir_t * self ) {
    if ( NULL != self ) {
        return self -> path;
    }
    return UNKNOWN_DIR;
}

rc_t generate_lookup_filename( const struct temp_dir_t * self, char * dst, size_t dst_size ) {
    rc_t rc;
    if ( NULL == self || NULL == dst || 0 == dst_size ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c generate_lookup_filename() -> %R", rc );
    } else {
        size_t num_writ;
        rc = string_printf( dst, dst_size, &num_writ,
                "%s%s.%u.lookup",
                self -> path, self -> hostname, self -> pid );
        if ( 0 != rc ) {
            ErrMsg( "temp_dir.c generate_lookup_filename().printf() -> %R", rc );
        }
    }
    return rc;
}

rc_t generate_bg_sub_filename( const struct temp_dir_t * self, char * dst, size_t dst_size, uint32_t product_id ) {
    rc_t rc;
    if ( NULL == self || NULL == dst || 0 == dst_size ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c generate_bg_sub_filename() -> %R", rc );
    } else {
        size_t num_writ;
        rc = string_printf( dst, dst_size, &num_writ,
                "%sbg_sub_%s_%u_%u.dat",
                self -> path, self -> hostname, self -> pid, product_id );
        if ( 0 != rc ) {
            ErrMsg( "temp_dir.c generate_bg_sub_filename().printf() -> %R", rc );
        }
    }
    return rc;
}

rc_t generate_bg_merge_filename( const struct temp_dir_t * self, char * dst, size_t dst_size, uint32_t product_id ) {
    rc_t rc;
    if ( NULL == self || NULL == dst || 0 == dst_size ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c generate_bg_sub_filename() -> %R", rc );
    } else {
        size_t num_writ;
        rc = string_printf( dst, dst_size, &num_writ,
                "%sbg_merge_%s_%u_%u.dat",
                self -> path, self -> hostname, self -> pid, product_id );
        if ( 0 != rc ) {
            ErrMsg( "temp_dir.c generate_bg_sub_filename().printf() -> %R", rc );
        }
    }
    return rc;
}

rc_t make_joined_filename( const struct temp_dir_t * self, char * dst, size_t dst_size,
                           const char * accession, uint32_t id ) {
    rc_t rc;
    if ( NULL == self || NULL == dst || 0 == dst_size || NULL == accession ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c make_joined_filename() -> %R", rc );
    } else {
        size_t num_writ;
        rc = string_printf( dst, dst_size, &num_writ, "%s%s.%s.%u.%.04u",
                                 self -> path,
                                 accession,
                                 self -> hostname,
                                 self -> pid,
                                 id );
        if ( 0 != rc ) {
            ErrMsg( "temp_dir.c make_joined_filename().string_printf() -> %R", rc );
        }
    }
    return rc;
}

rc_t remove_temp_dir( const struct temp_dir_t * self, KDirectory * dir ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c remove_temp_dir() -> %R", rc );
    } else {
        bool tmp_exists = dir_exists( dir, "%s", self -> path ); /* helper.c */
        if ( tmp_exists ) {
            rc = KDirectoryClearDir ( dir, true, "%s", self -> path );
            if ( 0 != rc ) {
                ErrMsg( "temp_dir.c remove_temp_dir.KDirectoryClearDir( '%s' ) -> %R", self -> path, rc );
            } else {
                tmp_exists = dir_exists( dir, "%s", self -> path ); /* helper.c */
                if ( tmp_exists ) {
                    rc = KDirectoryRemove ( dir, true, "%s", self -> path );
                    if ( 0 != rc ) {
                        ErrMsg( "temp_dir.c remove_temp_dir.KDirectoryRemove( '%s' ) -> %R", self -> path, rc );
                    }
                }
            }
        }
    }
    return rc;
}
