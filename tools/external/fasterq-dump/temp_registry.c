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
    bool keep_tmp_files;
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

rc_t make_temp_registry( temp_registry_t ** registry, struct CleanupTask_t * cleanup_task,
                         bool keep_tmp_files) {
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
            p -> keep_tmp_files = keep_tmp_files;
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
        olc -> res += ft_total_size_of_files_in_list( olc -> dir, item );
    }
}

static uint64_t total_size( KDirectory * dir, Vector * v ) {
    on_list_ctx_t olc = { dir, 0 };
    VectorForEach ( v, false, on_list, &olc );
    return olc . res;
}


/* -------------------------------------------------------------------- */

/* the data common to all merge-threads */
typedef struct cmn_merge_t {
    KDirectory * dir;
    const char * output_filename;
    size_t buf_size;
    struct bg_progress_t * progress;
    bool force;
    bool append;
} cmn_merge_t;

/* the data specific to one merge-thread */
typedef struct merge_thread_data_t {
    cmn_merge_t * cmn;
    VNamelist * files;
    KThread * thread;
    uint32_t idx;
} merge_thread_data_t;

static void CC merge_thread_data_free( void * item, void * data ) {
    free( item );
}

/* the merge-thread - function */
static rc_t CC merge_thread_func( const KThread *self, void *data ) {
    merge_thread_data_t * merge_thread_data = data;
    SBuffer_t s_filename;
    rc_t rc = split_filename_insert_idx( &s_filename, 4096,
                        merge_thread_data -> cmn -> output_filename,
                        merge_thread_data -> idx ); /* helper.c */
    if ( 0 == rc ) {
        VNamelistReorder ( merge_thread_data -> files, false );
        rc = concat_execute(
            merge_thread_data -> cmn -> dir,
            s_filename . S . addr,
            merge_thread_data -> files,
            merge_thread_data -> cmn -> buf_size,
            merge_thread_data -> cmn -> progress,
            merge_thread_data -> cmn -> force,
            merge_thread_data -> cmn -> append );
        release_SBuffer( &s_filename );
    }
    return rc;
}

/* -------------------------------------------------------------------- */

rc_t temp_registry_merge( temp_registry_t * self,
                          KDirectory * dir,
                          const char * base_output_filename,
                          size_t buf_size,
                          bool show_progress,
                          bool force,
                          bool append ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else if ( NULL == base_output_filename || NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        struct bg_progress_t * progress = NULL;

        if ( show_progress ) {
            StdErrMsg( "concat :" );
            {
                uint64_t total = total_size( dir, &self -> lists );
                rc = bg_progress_make( &progress, total, 0, 0 ); /* progress_thread.c */
            }
        }

        if ( 0 == rc ) {
            /* we are iterating over the vector self->lists
             * the items of this vector are VNamelist instances, but can be NULL too */
            uint32_t start = VectorStart( &( self -> lists ) );
            uint32_t length = VectorLength( &( self -> lists ) );
            uint32_t end = start + length;
            uint32_t idx;
            Vector thread_data_vec;
            cmn_merge_t cmn = { dir, base_output_filename, buf_size, progress, force, append };
            
            /* we create a thread for each item in self->lists */
            VectorInit( &thread_data_vec, 0, length );
            for ( idx = start; rc == 0 && idx < end; ++idx ) {
                VNamelist * files = VectorGet( &( self -> lists ), idx );
                if ( NULL != files ) {
                    merge_thread_data_t * thread_data = calloc( 1, sizeof * thread_data );
                    if ( NULL != thread_data ) {
                        thread_data -> cmn = &cmn;
                        thread_data -> files = files;
                        thread_data -> idx = idx;
                        rc = hlp_make_thread( &( thread_data -> thread ),
                                              merge_thread_func,
                                              thread_data,
                                              THREAD_DFLT_STACK_SIZE );
                        if ( 0 != rc ) {
                            ErrMsg( "helper_make_thread( temp_registry_merge #%d ) -> %R", idx, rc );
                        } else {
                            rc = VectorAppend( &thread_data_vec, NULL, thread_data );
                            if ( 0 != rc ) {
                                ErrMsg( "VectorAppend( temp_registry_merge #%d ) -> %R", idx, rc );
                            }
                        }
                    }
                }
            }
            
            /* join all the threads, and capture first none-zero rc-code */
            length = VectorLength( &thread_data_vec );
            for ( idx = 0; idx < length; ++idx ) {
                merge_thread_data_t * thread_data = VectorGet( &thread_data_vec, idx );
                if ( NULL != thread_data ) {
                    rc_t rc_thread;
                    rc_t rc_wait = KThreadWait( thread_data -> thread, &rc_thread );
                    /* record the first none-zero error-code */
                    if ( 0 == rc ) {
                        if ( 0 != rc_thread ) {
                            rc = rc_thread;
                        } else if ( 0 != rc_wait ) {
                            rc = rc_wait;
                        }
                    }
                    KThreadRelease( thread_data -> thread );
                    thread_data -> thread = NULL;
                }
            }

            /* clean up the vector with the thread-specific data */
            VectorWhack( &thread_data_vec, merge_thread_data_free, NULL );
        }
        bg_progress_release( progress ); /* progress_thread.c ( ignores NULL ) */
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
    bool keep_tmp_files;
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
                    rc = ft_make_buffered_for_read( c -> dir, &src, filename, c -> buf_size );
                    if ( 0 == rc ) {
                        rc = print_file_to_stdout( src, 4096 * 4 );
                        if ( 0 != rc ) {
                            ErrMsg( "on_print_to_stdout().print_file_to_stdout( '%s' ) -> %R", filename, rc );
                        }
                        ft_release_file( src, "on_print_to_stdout()" );
                    }

                    if ( ! c -> keep_tmp_files ) {
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
                              size_t buf_size,
                              bool keep_tmp_files ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    } else if ( NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        print_to_stdout_ctx_t c;

        c . dir = dir;
        c . buf_size = buf_size;
        c . keep_tmp_files = keep_tmp_files;

        VectorForEach ( &( self -> lists ), false, on_print_to_stdout, &c );
    }
    return rc;
}
