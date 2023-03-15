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
#include "temp_registry.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_concatenator_
#include "concatenator.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

typedef struct temp_registry_t {
    struct CleanupTask_t * cleanup_task;
    KLock * lock;
    size_t buf_size;
    Vector lists;
} temp_registry_t;

static void CC destroy_list( void * item, void * data ) {
    if ( NULL != item ) {
        VNamelistRelease ( item );
    }
}

void destroy_temp_registry( temp_registry_t * self ) {
    if ( NULL != self ) {
        VectorWhack ( &self -> lists, destroy_list, NULL );
        KLockRelease ( self -> lock );
        free( ( void * ) self );
    }
}

rc_t make_temp_registry( temp_registry_t ** registry, struct CleanupTask_t * cleanup_task ) {
    KLock * lock;
    rc_t rc = KLockMake ( &lock );
    if ( 0 == rc ) {
        temp_registry_t * p = calloc( 1, sizeof * p );
        if ( NULL == p ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_temp_registry().calloc( %d ) -> %R", ( sizeof * p ), rc );
            KLockRelease ( lock );
        } else {
            VectorInit ( &p -> lists, 0, 4 );
            p -> lock = lock;
            p -> cleanup_task = cleanup_task;
            *registry = p;
        }
    }
    return rc;
}

rc_t register_temp_file( temp_registry_t * self, uint32_t read_id, const char * filename ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else if ( NULL == filename ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        rc = KLockAcquire ( self -> lock );
        if ( 0 == rc ) {
            VNamelist * l = VectorGet ( &self -> lists, read_id );
            if ( NULL == l ) {
                rc = VNamelistMake ( &l, 12 );
                if ( 0 == rc ) {
                    rc = VectorSet ( &self -> lists, read_id, l );
                    if ( 0 != rc ) {
                        VNamelistRelease ( l );
                    }
                }
            }
            if ( 0 == rc && NULL != l ) {
                rc = VNamelistAppend ( l, filename );
                if ( 0 == rc ) {
                    rc = clt_add_file( self -> cleanup_task, filename );
                }
            }
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

/* -------------------------------------------------------------------- */

typedef struct on_list_ctx_t {
    KDirectory * dir;
    uint64_t res;
} on_list_ctx_t;

static void CC on_list( void *item, void *data ) {
    if ( NULL != item ) {
        on_list_ctx_t * olc = data;
        olc -> res += total_size_of_files_in_list( olc -> dir, item );
    }
}

static uint64_t total_size( KDirectory * dir, Vector * v ) {
    on_list_ctx_t olc = { dir, 0 };
    VectorForEach ( v, false, on_list, &olc );
    return olc . res;
}

/* -------------------------------------------------------------------- */

typedef struct on_count_ctx_t {
    uint32_t idx;
    uint32_t valid;
    uint32_t first;
} on_count_ctx_t;

static void CC on_count( void *item, void *data ) {
    on_count_ctx_t * occ = data;
    if ( NULL != item ) {
        if ( 0 == occ -> valid ) {
            occ -> first = occ -> idx;
        }
        occ -> valid++;
    }
    occ -> idx ++;
}

static uint32_t count_valid_entries( Vector * v, uint32_t * first ) {
    on_count_ctx_t occ = { 0, 0, 0 };
    VectorForEach( v, false, on_count, &occ );
    *first = occ . first;
    return occ . valid;
}

/* -------------------------------------------------------------------- */

typedef struct cmn_merge_t
{
    KDirectory * dir;
    const char * output_filename;
    size_t buf_size;
    struct bg_progress_t * progress;
    bool force;
    bool append;
} cmn_merge_t;

typedef struct merge_data_t
{
    cmn_merge_t * cmn;
    VNamelist * files;
    uint32_t idx;
} merge_data_t;

static rc_t CC merge_thread_func( const KThread *self, void *data ) {
    merge_data_t * md = data;
    SBuffer_t s_filename;
    rc_t rc = split_filename_insert_idx( &s_filename, 4096,
                            md -> cmn -> output_filename, md -> idx ); /* helper.c */
    if ( 0 == rc ) {
        VNamelistReorder ( md -> files, false );
        rc = execute_concat( md -> cmn -> dir,
            s_filename . S . addr,
            md -> files,
            md -> cmn -> buf_size,
            md -> cmn -> progress,
            md -> cmn -> force,
            md -> cmn -> append ); /* concatenator.c */
        release_SBuffer( &s_filename ); /* helper.c */
    }
    free( ( void * ) md );
    return rc;
}

typedef struct on_merge_ctx_t
{
    cmn_merge_t * cmn;
    uint32_t idx;
    Vector threads;
} on_merge_ctx_t;

static void CC on_merge( void *item, void *data ) {
    on_merge_ctx_t * omc = data;
    if ( NULL != item ) {
        merge_data_t * md = calloc( 1, sizeof * md );
        if ( NULL != md ) {
            rc_t rc;
            KThread * thread;

            md -> cmn = omc -> cmn;
            md -> files = item;
            md -> idx = omc -> idx;

            rc = hlp_make_thread( &thread, merge_thread_func, md, THREAD_DFLT_STACK_SIZE );
            if ( 0 != rc ) {
                ErrMsg( "temp_registry.c helper_make_thread( on_merge #%d ) -> %R", omc -> idx, rc );
            } else {
                rc = VectorAppend( &omc -> threads, NULL, thread );
                if ( 0 != rc ) {
                    ErrMsg( "temp_registry.c VectorAppend( merge-thread #%d ) -> %R", omc -> idx, rc );
                }
            }
        }
    }
    omc -> idx ++;
}

/* -------------------------------------------------------------------- */

rc_t temp_registry_merge( temp_registry_t * self,
                          KDirectory * dir,
                          const char * output_filename,
                          size_t buf_size,
                          bool show_progress,
                          bool force,
                          bool append ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else if ( NULL == output_filename || NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        struct bg_progress_t * progress = NULL;

        if ( show_progress ) {
            rc = KOutMsg( "concat :" );
            if ( 0 == rc ) {
                uint64_t total = total_size( dir, &self -> lists );
                rc = bg_progress_make( &progress, total, 0, 0 ); /* progress_thread.c */
            }
        }

        if ( 0 == rc ) {
            uint32_t first;
            uint32_t count = count_valid_entries( &self -> lists, &first ); /* above */
            /* we have MULTIPLE sets of files... */
            cmn_merge_t cmn = { dir, output_filename, buf_size, progress, force, append };
            on_merge_ctx_t omc = { &cmn, 0 };
            VectorInit( &omc . threads, 0, count );
            VectorForEach ( &self -> lists, false, on_merge, &omc );
            hlp_join_and_release_threads( &omc . threads );
            bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/
        }
    }
    return rc;
}

/* -------------------------------------------------------------------------------- */

static rc_t print_file_to_stdout( struct KFile const * f, size_t buffer_size ) {
    rc_t rc = 0;
    char * buffer = malloc( buffer_size );
    if ( NULL == buffer ) {
        rc = RC( rcApp, rcBuffer, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "print_file_to_stdout() malloc( %u )-> %R", buffer_size, rc );
    } else {
        uint64_t pos = 0;
        bool done = false;
        do {
            size_t num_read;
            rc = KFileRead ( f, pos, buffer, buffer_size - 1, &num_read );
            if ( 0 == rc ) {
                done = ( 0 == num_read );
                if ( !done ) {
                    buffer[ num_read ] = 0;
                    rc = KOutMsg( "%s", buffer );
                    pos += num_read;
                }
            }
        } while ( 0 == rc && !done );
        free( ( void * ) buffer );
    }
    return rc;
}

typedef struct print_to_stdout_ctx_t
{
    KDirectory * dir;
    size_t buf_size;
} print_to_stdout_ctx_t;

static void CC on_print_to_stdout( void * item, void * data ) {
    const VNamelist * l = ( const VNamelist * )item;
    if ( NULL != l ) {
        const print_to_stdout_ctx_t * c = ( const print_to_stdout_ctx_t * )data;
        uint32_t count;
        rc_t rc = VNameListCount ( l, &count );
        if ( 0 != rc ) {
            ErrMsg( "on_print_to_stdout().VNameListCoun() -> %R", rc );
        } else {
            uint32_t idx;
            VNamelistReorder ( ( VNamelist * )l, false );
            for ( idx = 0; 0 == rc && idx < count; ++idx ) {
                const char * filename;
                rc = VNameListGet( l, idx, &filename );
                if ( 0 != rc ) {
                    ErrMsg( "on_print_to_stdout().VNameListGet( #%d ) -> %R", idx, rc );
                } else {
                    const struct KFile * src;
                    rc = make_buffered_for_read( c -> dir, &src, filename, c -> buf_size ); /* helper.c */
                    if ( 0 == rc ) {
                        rc = print_file_to_stdout( src, 4096 * 4 );
                        if ( 0 != rc ) {
                            ErrMsg( "on_print_to_stdout().print_file_to_stdout( '%s' ) -> %R", filename, rc );
                        }
                        release_file( src, "on_print_to_stdout()" );
                    }

                    {
                        rc_t rc1 = KDirectoryRemove( c -> dir, true, "%s", filename );
                        if ( 0 != rc1 ) {
                            ErrMsg( "on_print_to_stdout.KDirectoryRemove( '%s' ) -> %R", filename, rc1 );
                        }
                    }
                }
            }
        }
    }
}

rc_t temp_registry_to_stdout( temp_registry_t * self,
                              KDirectory * dir,
                              size_t buf_size ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else if ( NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        print_to_stdout_ctx_t c;
        c . dir = dir;
        c . buf_size = buf_size;

        VectorForEach ( &( self -> lists ), false, on_print_to_stdout, &c );
    }
    return rc;
}
