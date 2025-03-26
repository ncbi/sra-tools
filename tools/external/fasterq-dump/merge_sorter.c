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
#include "merge_sorter.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_lookup_reader_
#include "lookup_reader.h"
#endif

#ifndef _h_lookup_writer_
#include "lookup_writer.h"
#endif

#ifndef _h_locked_file_list_
#include "locked_file_list.h"
#endif

#ifndef _h_locked_value_
#include "locked_value.h"
#endif

#ifndef _h_klib_status_
#include <klib/status.h>
#endif

#ifndef _h_klib_time_
#include <klib/time.h>
#endif

#ifndef _h_kproc_queue_
#include <kproc/queue.h>
#endif

#ifndef _h_kproc_timeout_
#include <kproc/timeout.h>
#endif

typedef struct merge_src {
    struct lookup_reader_t * reader;
    uint64_t key;
    SBuffer_t packed_bases;
    rc_t rc;
} merge_src_t;

static merge_src_t * get_min_merge_src( merge_src_t * src, uint32_t count ) {
    merge_src_t * res = NULL;
    uint32_t i;
    for ( i = 0; i < count; ++i ) {
        merge_src_t * item = &src[ i ];
        if ( 0 == item -> rc ) {
            if ( NULL == res ) {
                /* pick the first one */
                res = item;
            } else if ( item -> key < res -> key ) {
                /* if this src has a smaller key... */
                res = item;
            }
        }
    }
    return res;
}

/* ================================================================================= */

typedef struct merge_sorter_t {
    struct lookup_writer_t * dst; /* lookup_writer.h */
    struct index_writer_t * idx;  /* index.h */
    merge_src_t * src;            /* vector of input-files to be merged */
    struct bg_update_t * gap;     /* indicator of running merge */
    uint64_t total_size, total_entries;
    uint32_t num_src;
} merge_sorter_t;

static rc_t init_merge_sorter( merge_sorter_t * self,
                               KDirectory * dir,
                               const char * output,
                               const char * index,
                               VNamelist * files,
                               size_t buf_size,
                               uint32_t num_src,
                               struct bg_update_t * gap ) {
    rc_t rc = 0;
    uint32_t i;

    if ( NULL != index ) {
        rc = make_index_writer( dir, &( self -> idx ), buf_size,
                        DFLT_INDEX_FREQUENCY, "%s", index ); /* index.h */
    } else {
        self -> idx = NULL;
    }

    self -> total_size = 0;
    self -> total_entries = 0;
    self -> num_src = num_src;
    self -> gap = gap;

    if ( 0 == rc ) {
        rc = make_lookup_writer( dir, self -> idx, &( self -> dst ), buf_size, "%s", output ); /* lookup_writer.h */
    }

    if ( 0 == rc ) {
        self -> src = calloc( self -> num_src, sizeof * self-> src );
        if ( NULL == self -> src ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "init_merge_sorter2.calloc( %d ) failed", ( ( sizeof * self -> src ) * self -> num_src ) );
        }
    }

    for ( i = 0; 0 == rc && i < self -> num_src; ++i ) {
        const char * filename;
        rc = VNameListGet ( files, i, &filename );
        if ( 0 == rc ) {
            merge_src_t * s = &self -> src[ i ];
            if ( 0 == rc ) {
                rc = make_lookup_reader( dir, NULL, &s -> reader, buf_size, "%s", filename ); /* lookup_reader.h */
            }
            if ( 0 == rc ) {
                rc = make_SBuffer( &s -> packed_bases, 4096 );
                if ( 0 == rc ) {
                    s -> rc = lookup_reader_get( s -> reader, &s -> key, &s -> packed_bases ); /* lookup_reader.h */
                }
            }
        }
    }
    return rc;
}

static void release_merge_sorter( merge_sorter_t * self ) {
    release_lookup_writer( self -> dst );
    release_index_writer( self -> idx );
    if ( NULL != self -> src ) {
        uint32_t i;
        for ( i = 0; i < self -> num_src; ++i ) {
            merge_src_t * s = &self -> src[ i ];
            release_lookup_reader( s -> reader );
            release_SBuffer( &s -> packed_bases );
        }
        free( ( void * ) self -> src );
    }
}

static rc_t run_merge_sorter( merge_sorter_t * self ) {
    rc_t rc = 0;
    uint64_t last_key = 0;
    uint64_t loop_nr = 0;
    merge_src_t * to_write = get_min_merge_src( self -> src, self -> num_src ); /* above */

    while( 0 == rc && NULL != to_write ) {
        rc = hlp_get_quitting();    /* helper.c */
        if ( 0 == rc ) {
            if ( last_key > to_write -> key ) {
                rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
                ErrMsg( "run_merge_sorter() %lu -> %lu in loop #%lu", last_key, to_write -> key, loop_nr );
            } else {
                loop_nr ++;
                last_key = to_write -> key;
                rc = write_packed_to_lookup_writer( self -> dst,
                                                    to_write -> key,
                                                    &to_write -> packed_bases . S ); /* lookup_writer.h */
                if ( 0 == rc ) {
                    to_write -> rc = lookup_reader_get( to_write -> reader,
                                                        &to_write -> key,
                                                        &to_write -> packed_bases ); /* lookup_reader.h */
                }
                to_write = get_min_merge_src( self -> src, self -> num_src ); /* above */
            }
            if ( 0 != rc ) {
                hlp_set_quitting();     /* helper.c */
            }
            bg_update_update( self -> gap, 1 ); /* signal to gap-update */
        }
    }
    self -> total_entries += loop_nr;
    return rc;
}

/* =================================================================================
    The background-merger is composed from 1 background-thread, which is the consumer
    of a job_q. The producer-pool in sorter.c puts KVector-instances into the queue.
    The background-merger pops the jobs out of the queue until it has assembled
    a batch of jobs. It then processes this batch by merge-sorting the content of
    the KVector's into a temporary file. The entries are key-value pairs with a 64-bit
    key which is composed from the SEQID and one bit: first or second read in a spot.
    The value is the packed READ ( pack_4na() in helper.c ).
    The background-merger terminates when it's input-queue is sealed in perform_fastdump()
    in fastdump.c after all sorter-threads ( producers ) have been joined.
    The final output of the background-merger is a list of temporary files produced
    in the temp-directory.
   ================================================================================= */

typedef struct background_vector_merger_t {
    KDirectory * dir;               /* needed to perform the merge-sort */
    const struct temp_dir_t * temp_dir; /* needed to create temp. files */
    KQueue * job_q;                 /* the KVector objects arrive here from the lookup-producer */
    KThread * thread;               /* the thread that performs the merge-sort */
    struct background_file_merger_t * file_merger;    /* below */
    struct CleanupTask_t * cleanup_task;     /* add the produced temp_files here too */
    uint32_t product_id;            /* increased by one for each batch-run, used in temp-file-name */
    uint32_t batch_size;            /* how many KVectors have to arrive to run a batch */
    uint32_t q_wait_time;           /* timeout in milliseconds to get something out of in_q */
    size_t buf_size;                /* needed to perform the merge-sort */
    struct bg_update_t * gap;       /* visualize the gap after the producer finished */
    uint64_t total;                 /* how many entries have been merged... */
    uint64_t total_rowcount_prod;   /* updated by the producer, informs the vector-merger about the
                                       rowcount to be processed */
    bool details;
} background_vector_merger_t;

static rc_t wait_for_background_vector_merger( background_vector_merger_t * self ) {
    rc_t rc_status;
    rc_t rc = KThreadWait ( self -> thread, &rc_status );
    if ( 0 == rc ) {
        rc = rc_status;
    }
    return rc;
}

static void release_background_vector_merger( background_vector_merger_t * self ) {
    if ( NULL != self -> job_q ) {
        KQueueRelease ( self -> job_q );
    }
	KThreadRelease( self -> thread );
    free( self );
}

rc_t wait_for_and_release_background_vector_merger( background_vector_merger_t * self ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcReleasing, rcSelf, rcNull );
    } else {
        /* while we are waiting on the background-vector-merger to finish,
           show a progress-indicator... */
        rc = wait_for_background_vector_merger( self );

        /* now we can signal to the file-merger that nothing will be pushed into the
           queue any more... */
        if ( 0 == rc ) {
            rc = seal_background_file_merger( self -> file_merger );
        }

        if ( 0 == rc && ( self -> total != self -> total_rowcount_prod ) ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSize, rcInvalid );
            ErrMsg( "merge_sorter.c wait_for_and_release_background_vector_merger() : processed lookup rows: %lu of %lu",
                    self -> total, self -> total_rowcount_prod );
        }
        release_background_vector_merger( self );
    }

    if ( 0 != rc ) {
        ErrMsg( "merge_sorter.c wait_for_and_release_background_vector_merger()", rc );
    }
    return rc;
}

typedef struct bg_vec_merge_src_t {
    KVector * store;
    uint64_t key;
    const String * bases;
    rc_t rc;
} bg_vec_merge_src_t;

static rc_t init_bg_vec_merge_src( bg_vec_merge_src_t * src, KVector * store ) {
    src -> store = store;
    src -> rc = KVectorGetFirstPtr ( src -> store, &( src -> key ), ( void ** )&( src -> bases ) );
    return src -> rc;
}

static void release_bg_vec_merge_src( bg_vec_merge_src_t * src ) {
    if ( NULL != src -> store ) {
        KVectorRelease( src -> store );
    }
}

static bg_vec_merge_src_t * get_min_bg_vec_merge_src( bg_vec_merge_src_t * batch, uint32_t count ) {
    bg_vec_merge_src_t * res = NULL;
    uint32_t i;
    for ( i = 0; i < count; ++i ) {
        bg_vec_merge_src_t * item = &batch[ i ];
        if ( 0 == item -> rc ) {
            if ( NULL == res ) {
                res = item;
            } else if ( item -> key < res -> key ) {
                res = item;
            }
        }
    }
    return res;
}

static rc_t write_bg_vec_merge_src( bg_vec_merge_src_t * src, struct lookup_writer_t * writer ) {
    rc_t rc = src -> rc;
    if ( 0 == rc ) {
        rc = write_packed_to_lookup_writer( writer, src -> key, src -> bases ); /* lookup_writer.c */
        StringWhack ( src -> bases );
        src -> bases = NULL;
    }
    if ( 0 == rc ) {
        uint64_t next_key;
        src -> rc = KVectorGetNextPtr ( src -> store, &next_key, src -> key, ( void ** )&( src -> bases ) );
        src -> key = next_key;
    }
    return rc;
}

static rc_t background_vector_merger_collect_batch( background_vector_merger_t * self,
                                                    bg_vec_merge_src_t ** batch,
                                                    uint32_t * count ) {
    rc_t rc = 0;
    bg_vec_merge_src_t * b = calloc( self -> batch_size, sizeof * b );
    *batch = NULL;
    *count = 0;

    if ( NULL == b ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    } else {
        bool sealed = false;
        while ( 0 == rc && *count < self -> batch_size && !sealed ) {
            struct timeout_t tm;
            rc = TimeoutInit ( &tm, self -> q_wait_time );
            if ( 0 == rc ) {
                KVector * store = NULL;
                rc = KQueuePop ( self -> job_q, ( void ** )&store, &tm );
                if ( 0 == rc ) {
                    /* we pulled out a store from the Q */
                    STATUS ( STAT_USR, "KQueuePop() : store = %p", store );
                    rc = init_bg_vec_merge_src( &( b[ *count ] ), store );
                    if ( 0 == rc ) {
                        *count += 1;
                    }
                } else {
                    STATUS ( STAT_USR, "KQueuePop() : %R, store = %p", rc, store );
                    if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) ) {
                        /* the other side has sealed the Q */
                        sealed = true;
                        rc = 0;
                    } else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) ) {
                        /* we had a timeout while trying to get a store from the Q */
                        rc = 0;
                    }
                }
            }
        }
    }
    if ( 0 != *count ) {
        *batch = b;
        rc = 0;
    } else {
        free( b );
    }
    return rc;
}

static bool batch_valid( bg_vec_merge_src_t * batch, uint32_t count ) {
    bool res = false;
    uint32_t i;
    for ( i = 0; i < count && !res; ++i ) {
        if ( 0 == batch[ i ] . rc ) {
            res = true;
        }
    }
    return res;
}

static rc_t background_vector_merger_process_batch( background_vector_merger_t * self,
                                                    bg_vec_merge_src_t * batch,
                                                    uint32_t count ) {
    char buffer[ 4096 ];
    rc_t rc = generate_bg_sub_filename( self -> temp_dir, buffer, sizeof buffer, self -> product_id );
    if ( 0 != rc ) {
        ErrMsg( "merge_sorter.c background_vector_merger_process_batch() -> %R", rc );
    } else {
        STATUS ( STAT_USR, "batch output filename is : %s", buffer );
        rc = clt_add_file( self -> cleanup_task, buffer );

        if ( 0 == rc ) {
            struct lookup_writer_t * writer; /* lookup_writer.h */
            rc = make_lookup_writer( self -> dir, NULL, &writer,
                                     self -> buf_size, "%s", buffer ); /* lookup_writer.c */
            if ( 0 == rc ) {
                self -> product_id += 1;
            }
            if ( 0 == rc ) {
                bg_vec_merge_src_t * to_write = get_min_bg_vec_merge_src( batch, count ); /* above */
                while( 0 == rc && NULL != to_write ) {
                    rc = hlp_get_quitting();    /* helper.c */
                    if ( 0 == rc ) {
                        rc = write_bg_vec_merge_src( to_write, writer ); /* above */
                        if ( 0 == rc ) {
                            self -> total++;
                            to_write = get_min_bg_vec_merge_src( batch, count ); /* above */
                        } else {
                            to_write = NULL;
                        }
                        bg_update_update( self -> gap, 1 );
                        if ( 0 != rc ) {
                            hlp_set_quitting();     /* helper.c */
                        }
                    }
                }
                release_lookup_writer( writer ); /* lookup_writer.c */
            }
        }

        /*
        if ( rc == 0 && self -> file_merger != NULL )
            rc = lookup_check_file( self -> dir, self -> buf_size, buffer );
        */

        if ( 0 == rc && NULL != self -> file_merger ) {
            rc = push_to_background_file_merger( self -> file_merger, buffer ); /* below */
        }
    }
    return rc;
}

static rc_t CC background_vector_merger_thread_func( const KThread * thread, void *data ) {
    rc_t rc = 0;
    background_vector_merger_t * self = data;
    bool done = false;

    STATUS ( STAT_USR, "starting background thread loop" );
    while( 0 == rc && !done ) {
        bg_vec_merge_src_t * batch = NULL;
        uint32_t count = 0;

        /* Step 1 : get n = batch_size KVector's out of the in_q */
        STATUS ( STAT_USR, "collecting batch" );
        rc = background_vector_merger_collect_batch( self, &batch, &count );
        STATUS ( STAT_USR, "done collectin batch: rc = %R, count = %u", rc, count );
        if ( 0 == rc ) {
            done = ( 0 == count );
            if ( !done ) {
                if ( batch_valid( batch, count ) ) {
                    /* Step 2 : process the batch */
                    STATUS ( STAT_USR, "processing batch of %u vectors", count );
                    rc = background_vector_merger_process_batch( self, batch, count );
                    STATUS ( STAT_USR, "finished processing: rc = %R", rc );
                } else {
                    STATUS ( STAT_USR, "we have an invalid batch!" );
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                }
            }
        }
        if ( NULL != batch ) {
            uint32_t i;
            for ( i = 0; i < count; ++i ) {
                release_bg_vec_merge_src( &( batch[ i ] ) );
            }
            free( batch );
        }
    }
    STATUS ( STAT_USR, "exiting background thread loop" );
    return rc;
}

rc_t make_background_vector_merger( struct background_vector_merger_t ** merger,
                                    vector_merger_args_t * args ) {
    rc_t rc = 0;
    background_vector_merger_t * b = calloc( 1, sizeof * b );
    *merger = NULL;
    if ( NULL == b ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    } else {
        b -> dir = args -> dir;
        b -> temp_dir = args -> temp_dir;
        b -> batch_size = args -> batch_size;
        b -> q_wait_time = args -> q_wait_time;
        b -> buf_size = args -> buf_size;
        b -> file_merger = args -> file_merger;
        b -> cleanup_task = args -> cleanup_task;
        b -> gap = args -> gap;
        b -> total = 0;
        b -> total_rowcount_prod = 0;
        b -> details = args -> details;

        rc = KQueueMake ( &( b -> job_q ), args -> batch_size );
        if ( 0 == rc ) {
            rc = hlp_make_thread( &( b -> thread ),
                                  background_vector_merger_thread_func,
                                  b,
                                  THREAD_DFLT_STACK_SIZE );
            if ( 0 != rc ) {
                ErrMsg( "merge_sorter.c helper_make_thread( vector-merger ) -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            *merger = b;
        } else {
            release_background_vector_merger( b );
        }
    }
    return rc;
}

void tell_total_rowcount_to_vector_merger( background_vector_merger_t * self, uint64_t value ) {
    if ( NULL != self ) {
        self -> total_rowcount_prod = value;
        tell_total_rowcount_to_file_merger( self -> file_merger, value );
    }
}

rc_t seal_background_vector_merger( background_vector_merger_t * self ) {
    rc_t rc = KQueueSeal ( self -> job_q );
    if ( 0 != rc ) {
        ErrMsg( "merge_sorter.c seal_background_vector_merger() -> %R", rc );
    }
    return rc;
}

rc_t push_to_background_vector_merger( background_vector_merger_t * self, KVector * store ) {
    rc_t rc;
    bool running = true;
    while ( running ) {
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( 0 != rc ) {
            ErrMsg( "merge_sorter.c push_to_background_vector_merger().TimeoutInit( %u ) -> %R", self -> q_wait_time, rc );
            running = false;
        } else {
            rc = KQueuePush ( self -> job_q, store, &tm );
            if ( 0 == rc ) {
                running = false;
            } else {
                bool timed_out = ( GetRCState( rc ) == rcExhausted && GetRCObject( rc ) == ( enum RCObject )rcTimeout );
                if ( timed_out ) {
                    KSleepMs( self -> q_wait_time );
                } else {
                    ErrMsg( "merge_sorter.c push_to_background_vector_merger().KQueuePush() -> %R", rc );
                    running = false;
                }
            }
        }
    }
    return rc;
}

/* =================================================================================
    The background-file is composed from 1 background-thread, which is the consumer
    of a job_q. The background_vector_merger above puts strings into the queue.
    The background-merger pops the jobs out of the queue until it has assembled
    a batch of jobs. It then processes this batch by merge-sorting the content of
    the the files into a temporary file. The file-entries are key-value pairs with a 64-bit
    key which is composed from the SEQID and one bit: first or second read in a spot.
    The value is the packed READ ( pack_4na() in helper.c ).
    The background-merger terminates when it's input-queue is sealed in perform_fastdump()
    in fastdump.c after all background-vector-merger-threads ( producers ) have been joined.
    The final output of the background-merger is a list of temporary files produced
    in the temp-directory.
   ================================================================================= */
typedef struct background_file_merger_t {
    KDirectory * dir;               /* needed to perform the merge-sort */
    const struct temp_dir_t * temp_dir;      /* needed to create temp. files */
    const char * lookup_filename;
    const char * index_filename;
    locked_file_list_t files;        /* a locked file-list */
    locked_value_t sealed;           /* flag to signal if the input is sealed */
    struct CleanupTask_t * cleanup_task;     /* add the produced temp_files here too */
    KThread * thread;               /* the thread that performs the merge-sort */
    uint32_t product_id;            /* increased by one for each batch-run, used in temp-file-name */
    uint32_t batch_size;            /* how many KVectors have to arrive to run a batch */
    uint32_t wait_time;             /* time in milliseconds to sleep if waiting for files to process */
    size_t buf_size;                /* needed to perform the merge-sort */
    struct bg_update_t * gap;       /* visualize the gap after the producer finished */
    uint64_t total_rows;            /* how many rows have we processed */
    uint64_t total_rowcount_prod;   /* updated by the producer, informs the file-merger about the
                                       rowcount to be processed */
    bool details;
} background_file_merger_t;

static void release_background_file_merger( background_file_merger_t * self ) {
    if ( NULL != self ) {
        locked_file_list_release( &( self -> files ), self -> dir, self -> details );
        locked_value_release( &( self -> sealed ) );
		KThreadRelease( self -> thread );
        free( self );
    }
}

static rc_t wait_for_background_file_merger( background_file_merger_t * self ) {
    rc_t rc_status;
    rc_t rc = KThreadWait ( self -> thread, &rc_status );
    if ( 0 == rc ) {
        rc = rc_status;
    }
    return rc;
}

rc_t wait_for_and_release_background_file_merger( background_file_merger_t * self ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcReleasing, rcSelf, rcNull );
    } else {
        /* while we are waiting on the background-file-merger to finish,
           show a progress-indicator... */
        rc = wait_for_background_file_merger( self );
        if ( 0 == rc && ( self -> total_rows != self -> total_rowcount_prod ) ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSize, rcInvalid );
            ErrMsg( "merge_sorter.c wait_for_and_release_background_file_merger() %lu of %lu",
                     self -> total_rows, self -> total_rowcount_prod );
        }
        release_background_file_merger( self );
    }
    return rc;
}

/* called from the background-thread */
static rc_t process_background_file_merger( background_file_merger_t * self ) {
    char tmp_filename[ 4096 ];

    rc_t rc = generate_bg_merge_filename( self -> temp_dir, tmp_filename, sizeof tmp_filename,
                                          self -> product_id );
    if ( 0 == rc ) {
        rc = clt_add_file( self -> cleanup_task, tmp_filename );
    }
    if ( 0 == rc ) {
        uint32_t num_src = 0;
        VNamelist * batch_files;
        rc = VNamelistMake ( &batch_files, self -> batch_size );
        if ( 0 == rc ) {
            uint32_t i;
            rc_t rc1 = 0;
            for ( i = 0; 0 == rc && 0 == rc1 && i < self -> batch_size; ++i ) {
                const String * filename = NULL;
                rc1 = locked_file_list_pop( &( self -> files ), &filename );
                if ( 0 == rc1 && NULL != filename ) {
                    rc = VNamelistAppendString ( batch_files, filename );
                    if ( 0 == rc ) {
                        num_src++;
                    }
                }
            }
            if ( 0 == rc ) {
                merge_sorter_t sorter;
                rc = init_merge_sorter( &sorter,
                                        self -> dir,
                                        tmp_filename,   /* the output file */
                                        NULL,           /* opt. index_filename */
                                        batch_files,    /* the input files */
                                        self -> buf_size,
                                        num_src,
                                        self -> gap );
                if ( 0 == rc ) {
                    rc = run_merge_sorter( &sorter );
                    release_merge_sorter( &sorter );
                }
            }
            if ( 0 == rc ) {
                rc = ft_delete_files( self -> dir, batch_files, self -> details );
            }
            VNamelistRelease( batch_files );
        }
    }
    if ( 0 == rc ) {
        rc = locked_file_list_append( &( self -> files ), tmp_filename );
        if ( 0 == rc ) {
            self -> product_id += 1;
        }
    }
    return rc;
}

static rc_t process_final_background_file_merger( background_file_merger_t * self, uint32_t count ) {
    VNamelist * batch_files;
    rc_t rc = VNamelistMake ( &batch_files, count );
    if ( 0 == rc ) {
        uint32_t i;
        uint32_t num_src = 0;
        rc_t rc1 = 0;
        for ( i = 0; 0 == rc && 0 == rc1 && i < count; ++i ) {
            const String * filename = NULL;
            rc1 = locked_file_list_pop( &( self -> files ), &filename );
            if ( 0 == rc1 && filename != NULL ) {
                rc = VNamelistAppendString ( batch_files, filename );
                if ( 0 == rc ) {
                    num_src++;
                }
				StringWhack( filename );
            }
        }

        if ( 0 == rc ) {
            rc = clt_add_file( self -> cleanup_task, self -> lookup_filename );
        }
        if ( 0 == rc ) {
            rc = clt_add_file( self -> cleanup_task, self -> index_filename );
        }
        if ( 0 == rc ) {
            merge_sorter_t sorter;
            rc = init_merge_sorter( &sorter,
                                    self -> dir,
                                    self -> lookup_filename,   /* the output file */
                                    self -> index_filename,    /* opt. index_filename */
                                    batch_files,               /* the input files */
                                    self -> buf_size,
                                    num_src,
                                    self -> gap );
            if ( 0 == rc ) {
                rc = run_merge_sorter( &sorter );
                if ( 0 == rc ) {
                    self -> total_rows += sorter . total_entries;
                }
                release_merge_sorter( &sorter );
            }
        }
        if ( 0 == rc ) {
            rc = ft_delete_files( self -> dir, batch_files, self -> details );
        }
        VNamelistRelease( batch_files );
    }
    return rc;
}

static rc_t CC background_file_merger_thread_func( const KThread * thread, void *data ) {
    rc_t rc = 0;
    background_file_merger_t * self = data;
    bool done = false;
    while( 0 == rc && !done ) {
        uint64_t sealed;
        rc = locked_value_get( &( self -> sealed ), &sealed );
        if ( 0 == rc ) {
            uint32_t count;
            rc = locked_file_list_count( &( self -> files ), &count );
            if ( 0 == rc ) {
                if ( sealed > 0 ) {
                    /* we are sealed... */
                    if ( 0 == count ) {
                        /* this should not happen, but for the sake of completeness */
                        done = true;
                    } else if ( count > ( self -> batch_size ) ) {
                        /* we still have more than we can open, do one batch */
                        rc = process_background_file_merger( self );
                    } else {
                        /* we can do the final batch */
                        rc = process_final_background_file_merger( self, count );
                        done = true;
                    }
                } else {
                    /* we are not sealed... */
                    if ( count < ( self -> batch_size ) ) {
                        /* let us take a little nap, until we get enough files, or get sealed */
                        KSleepMs( self -> wait_time );
                    } else {
                        /* we have enough files to process one batch */
                        rc = process_background_file_merger( self );
                    }
                }
            }
        }
    }
    return rc;
}

rc_t make_background_file_merger( background_file_merger_t ** merger,
                                  file_merger_args_t * args ) {
    rc_t rc = 0;
    background_file_merger_t * b = calloc( 1, sizeof * b );
    *merger = NULL;
    if ( NULL == b ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    } else {
        b -> dir = args -> dir;
        b -> temp_dir = args -> temp_dir;
        b -> lookup_filename = args -> lookup_filename;
        b -> index_filename = args -> index_filename;
        b -> batch_size = args -> batch_size;
        b -> wait_time = args -> wait_time;
        b -> buf_size = args -> buf_size;
        b -> cleanup_task = args -> cleanup_task;
        b -> gap = args -> gap;
        b -> total_rows = 0;
        b -> total_rowcount_prod = 0;
        b -> details = args -> details;

        rc = locked_file_list_init( &( b -> files ), 25  );
        if ( 0 == rc ) {
            rc = locked_value_init( &( b -> sealed ), 0 );
        }
        if ( 0 == rc ) {
            rc = hlp_make_thread( &( b -> thread ),
                                  background_file_merger_thread_func,
                                  b,
                                  THREAD_DFLT_STACK_SIZE );
            if ( 0 != rc ) {
                ErrMsg( "merge_sorter.c helper_make_thread( file-mergerr ) -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            *merger = b;
        } else {
            release_background_file_merger( b );
        }
    }
    return rc;
}

void tell_total_rowcount_to_file_merger( background_file_merger_t * self, uint64_t value ) {
    if ( NULL != self ) {
        self -> total_rowcount_prod = value;
    }
}

rc_t push_to_background_file_merger( background_file_merger_t * self, const char * filename ) {
    return locked_file_list_append( &( self -> files ), filename );
}

rc_t seal_background_file_merger( background_file_merger_t * self ) {
    return locked_value_set( &( self -> sealed ), 1 );
}
