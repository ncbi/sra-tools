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
#include "copy_machine.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_helper_
#include "helper.h"     /* helper_make_thread() */
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_klib_time_
#include <klib/time.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_kproc_queue_
#include <kproc/queue.h>
#endif

#ifndef _h_kproc_timeout_
#include <kproc/timeout.h>
#endif

typedef struct copy_machine_block_t
{
    char * buffer;
    size_t available;
} copy_machine_block_t;

#define N_COPY_MACHINE_BLOCKS 4

typedef struct copy_machine {
    KDirectory * dir;
    KFile * dst;
    struct bg_progress_t * progress;
    KThread * thread;
    KQueue * empty_q;
    KQueue * to_write_q;
    const struct VNamelist * sources;
    copy_machine_block_t blocks[ N_COPY_MACHINE_BLOCKS ];
    uint64_t dst_pos;
    size_t buf_size;
    uint32_t src_list_offset;
    uint32_t q_wait_time;
} copy_machine_t;

static rc_t destroy_copy_machine( copy_machine_t * self ) {
    uint32_t i;

    /* first join the thread... */
    rc_t rc, rc2;

    rc2 = KQueueSeal ( self -> to_write_q );
    {
        if ( 0 != rc2 ) {
            ErrMsg( "copy_machine.c destroy_copy_machine.KQueueSeal() -> %R", rc2 );
        }
    }
    KThreadWait ( self -> thread, &rc );

    if ( NULL != self -> empty_q ){
        rc2 = KQueueRelease ( self -> empty_q );
        if ( 0 != rc2 ) {
            ErrMsg( "copy_machine.c destroy_copy_machine.KQueueRelease().1 -> %R", rc2 );
        }
    }
    if ( NULL != self -> to_write_q ){
        rc2 = KQueueRelease ( self -> to_write_q );
        if ( 0 != rc2 ) {
            ErrMsg( "copy_machine.c destroy_copy_machine.KQueueRelease().2 -> %R", rc2 );
        }
    }

    for ( i = 0; i < N_COPY_MACHINE_BLOCKS; ++i ) {
        if ( NULL != self -> blocks[ i ] . buffer ) {
            free( ( void * ) self -> blocks[ i ] . buffer );
        }
    }
    return rc;
}

static rc_t push2q( KQueue * q, const void * item, uint32_t wait_time ) {
    rc_t rc;
    bool running = true;
    while ( running ) {
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, wait_time );
        if ( 0 != rc ) {
            ErrMsg( "copy_machine.c push2q().TimeoutInit( %u ) -> %R", wait_time, rc );
            running = false;
        } else {
            rc = KQueuePush ( q, item, &tm );
            if ( 0 == rc ) {
                running = false;
            } else {
                bool timed_out = ( rcExhausted != GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) );
                if ( timed_out ) {
                    rc_t rc2 = KSleepMs( wait_time );
                    if ( 0 != rc2 ) {
                        ErrMsg( "copy_machine.c push2q().KSleepMs() -> %R", rc2 );
                    }
                } else {
                    ErrMsg( "copy_machine.c push2q().KQueuePush() -> %R", rc );
                    running = false;
                }
            }
        }
    }
    return rc;
}

static rc_t copy_this_file( copy_machine_t * self, const struct KFile * src ) {
    rc_t rc = 0;
    uint64_t src_pos = 0;
    size_t num_read = 1;
    while ( 0 == rc && num_read > 0 ) {
        /* get a buffer out of the empty-q */
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( 0 != rc ) {
            ErrMsg( "copy_machine.c copy_this_file().TimeoutInit( %lu ms ) -> %R", self -> q_wait_time, rc );
        } else {
            copy_machine_block_t * block;
            rc = KQueuePop ( self -> empty_q, ( void ** )&block, &tm );
            if ( 0 == rc ) {
                block -> available = 0;
                rc = KFileRead( src, src_pos, block -> buffer, self -> buf_size, &num_read );
                if ( 0 != rc ) {
                    ErrMsg( "copy_machine.c copy_this_file().KFileRead( at %lu ) -> %R", src_pos, rc );
                }
                else if ( num_read > 0 ) {
                    /* we have data for the writer-thread */
                    block -> available = num_read;
                    src_pos += num_read;
                    rc = push2q ( self -> to_write_q, block, self -> q_wait_time );
                } else {
                    /* we are done with this file, put the block back... */
                    rc = push2q ( self -> empty_q, block, self -> q_wait_time );
                }
            }
            else if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) ) {
                /* the empty_q has been sealed, this can only happen if the writer is in trouble! */
                rc = RC( rcExe, rcFile, rcPacking, rcConstraint, rcViolated );
                ErrMsg( "you have exhausted your space" );
            } else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) ) {
                /* we have a timeout from KQueuePop(), that means the writer is busy,
                   all blocks are in the to_write-queue or in processing, let's try again */
                rc = 0;
            }
            else {
                /* something else is wrong with the empty-q */
                ErrMsg( "copy_machine.c copy_this_file().KQueuePop( empty-q ) -> %R", rc );
            }
        }
    }
    return rc;
}

static rc_t run_copy_machine( copy_machine_t * self ) {
    uint32_t file_count;
    rc_t rc = VNameListCount( self -> sources, &file_count );
    if ( 0 != rc ) {
        ErrMsg( "copy_machine.c run_copy_machine().VNameListCount( sources ) -> %R", rc );
    } else {
        uint32_t idx;
        for ( idx = self -> src_list_offset; 0 == rc && idx < file_count; ++idx ) {
            const char * filename = NULL;
            rc = VNameListGet( self -> sources, idx, &filename );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c run_copy_machine().VNameListGet( %u ) -> %R", idx, rc );
            } else {
                const struct KFile * src;
                rc = make_buffered_for_read( self -> dir, &src, filename, self -> buf_size ); /* helper.c */
                if ( 0 == rc ) {
                    rc_t rc2;
                    rc = copy_this_file( self, src ); /* above */
                    rc2 = release_file( src, "copy_machine.c run_copy_machine()[ %u ]", idx );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
                if ( 0 == rc ) {
                    rc = KDirectoryRemove( self -> dir, true, "%s", filename );
                    if ( 0 != rc ) {
                        ErrMsg( "copy_machine.c run_copy_machine().KDirectoryRemove( '%s' ) -> %R", filename, rc );
                    }
                }
            }
        }
        /* we are done ... seal the to_write_q ( the one the writer is listening at ) */
        {
            rc_t rc2 = KQueueSeal ( self -> to_write_q );
            if ( 0 != rc2 ) {
                ErrMsg( "copy_machine.c run_copy_machine().KQueueSeal() -> %R", rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static rc_t CC copy_machine_writer_thread( const KThread * thread, void *data ) {
    rc_t rc = 0;
    copy_machine_t * self = data;
    bool done = false;
    while( 0 == rc && !done ) {
        /* as long as ther to_write_q is not sealed */
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( 0 != rc ) {
            ErrMsg( "copy_machine.c copy_machine_writer_thread().TimeoutInit() -> %R", rc );
        } else {
            copy_machine_block_t * block;
            rc = KQueuePop ( self -> to_write_q, ( void ** )&block, /*&tm*/ NULL );
            if ( 0 == rc ) {
                /* we got a block to write out of the to_write_q */
                size_t num_written;
                rc = KFileWrite( self -> dst, self -> dst_pos,
                                 block -> buffer, block -> available, &num_written );
                if ( 0 == rc ) {
                    /* increment the write - position */
                    self -> dst_pos += num_written;

                    /* inform the background-process about it */
                    bg_progress_update( self -> progress, num_written ); /* progress_thread.c */

                    /* put the block back into the empty-q */
                    block -> available = 0;
                    rc = push2q ( self -> empty_q, block, self -> q_wait_time ); /* above */
                } else {
                    /* something went wrong with writing the block into the dst-file !!!
                       possibly we are running out of space to write... */

                    /* put the block back into the empty-q */
                    block -> available = 0;
                    rc = push2q ( self -> empty_q, block, self -> q_wait_time ); /* above */

                    /* we are done ... seal the empty_q ( that will tell the reader to stop... */
                    {
                        rc_t rc2 = KQueueSeal ( self -> empty_q );
                        if ( 0 != rc2 ) {
                            ErrMsg( "copy_machine.c copy_machine_writer_thread().KQueueSeal() -> %R", rc2 );
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                }
            } else {
                if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) ) {
                    /* the to_write_q has been sealed, we are done! */
                    done = true;
                    rc = 0;
                } else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) ) {
                    /* the to_write_q is still active, but so far nothing to write in it */
                    rc = 0;
                }
            }
        }
    }
    return rc;
}

rc_t make_a_copy( KDirectory * dir,
                  KFile * dst,
                  const struct VNamelist * sources,
                  struct bg_progress_t * progress,
                  uint64_t dst_offset,
                  size_t buf_size,
                  uint32_t src_list_offset,
                  uint32_t q_wait_time ) {
    rc_t rc = 0;
    if ( NULL == dst || 0 == buf_size || NULL == sources ) {
        rc = RC( rcExe, rcFile, rcPacking, rcParam, rcInvalid );
        ErrMsg( "copy_machine.c make_a_copy() -> %R", rc );
    } else {
        copy_machine_t cm;
        uint32_t i;

        cm . dir = dir;
        cm . dst = dst;
        cm . empty_q = NULL;
        cm . to_write_q = NULL;
        cm . progress = progress;
        cm . dst_pos = dst_offset;
        cm . buf_size = buf_size;
        cm . sources = sources;
        cm . src_list_offset = src_list_offset;
        cm . q_wait_time = q_wait_time;

        for ( i = 0; i < N_COPY_MACHINE_BLOCKS; ++i ) {
            cm . blocks[ i ] . buffer = NULL;
            cm . blocks[ i ] . available = 0;
        }
        for ( i = 0; 0 == rc && i < N_COPY_MACHINE_BLOCKS; ++i ) {
            cm . blocks[ i ] . buffer = malloc( buf_size );
            if ( NULL == cm . blocks[ i ] . buffer ) {
                rc = RC( rcExe, rcFile, rcPacking, rcMemory, rcExhausted );
                ErrMsg( "copy_machine.c make_a_copy().malloc( %d ) -> %R", buf_size, rc );
            }
        }
        if ( 0 == rc ) {
            rc = KQueueMake ( &( cm . empty_q ), N_COPY_MACHINE_BLOCKS );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c make_a_copy().KQueueMake( empty_q ) -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            rc = KQueueMake ( &( cm . to_write_q ), N_COPY_MACHINE_BLOCKS );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c make_a_copy().KQueueMake( to_write_q ) -> %R", rc );
            }
        }
        for ( i = 0; 0 == rc && i < N_COPY_MACHINE_BLOCKS; ++i ) {
            rc = push2q ( cm . empty_q, &( cm . blocks[ i ] ), q_wait_time );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c make_a_copy().KQueuePush( empty_q ) -> %R", rc );
            }
        }

        /* create a writer-thread */
        if ( 0 == rc ) {
            rc = hlp_make_thread( &( cm . thread ), copy_machine_writer_thread,
                                  &cm, THREAD_DFLT_STACK_SIZE );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c make_a_copy().helper_make_thread( writer-thread ) -> %R", rc );
            }
        }

        /* read the data on the current thread */
        if ( 0 == rc ) {
            rc = run_copy_machine( &cm ); /* above */
        }

        {
            rc_t rc2 = destroy_copy_machine( &cm ); /* above */
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------------------- */

typedef struct multi_writer_block_t
{
    char * data;
    size_t len;
    size_t available;
} multi_writer_block_t;

static multi_writer_block_t * create_multi_writer_block( size_t size ) {
    multi_writer_block_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        res -> data = calloc( 1, size );
        if ( NULL != res -> data ) {
            res -> available = size;
            res -> len = 0;
        }
        else {
            free( ( void * ) res );
            res = NULL;
        }
    }
    return res;
}

static void release_multi_writer_block( multi_writer_block_t * self ) {
    if ( NULL != self ) {
        free( ( void * ) self -> data );
        free( ( void * ) self );
    }
}

bool multi_writer_block_expand( multi_writer_block_t * self, size_t size ) {
    bool res = false;
    if ( NULL != self && size > self -> available ) {
        free( ( void * )( self -> data ) );
        self -> data = calloc( 1, size );
        res = ( NULL != self -> data );
        self -> available = ( res ) ? size : 0;
    }
    return res;
}

static bool multi_writer_block_write( multi_writer_block_t * self, const char * data, size_t size ) {
    bool res = false;
    if ( NULL != self && NULL != data && size > 0 ) {
        if ( size > self -> available ) {
            /* we have to make the block bigger */
            res = multi_writer_block_expand( self, size );
        } else {
            res = true;
        }
        if ( res ) {
            memmove( ( void * )self -> data, data, size );
            self -> len = size;
        }
    }
    return res;
}

bool multi_writer_block_append( multi_writer_block_t * self, const char * data, size_t size ) {
    bool res = false;
    if ( NULL != self && NULL != data && size > 0 ) {
        res = ( ( self -> len + size ) < self -> available );
        if ( res ) {
            /* it will fit ... */
            char * dst = self -> data;
            dst += self -> len;
            memmove( ( void * )dst, data, size );
            self -> len += size;
        }
    }
    return res;
}

static rc_t multi_writer_push( KQueue * q, const void * item, uint32_t wait_time ) {
    rc_t rc;
    bool running = true;
    while ( running ) {
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, wait_time );
        if ( 0 != rc ) {
            ErrMsg( "copy_machine.c multi_writer_push().TimeoutInit( %u ) -> %R", wait_time, rc );
            running = false;
        } else {
            rc = KQueuePush ( q, item, &tm );
            if ( 0 == rc ) {
                running = false;
            } else {
                bool timed_out = ( rcExhausted != GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) );
                if ( timed_out ) {
                    rc_t rc2 = KSleepMs( wait_time );
                    if ( 0 != rc2 ) {
                        ErrMsg( "copy_machine.c multi_writer_push().KSleepMs() -> %R", rc2 );
                    }
                } else {
                    ErrMsg( "copy_machine.c multi_writer_push().KQueuePush() -> %R", rc );
                    running = false;
                }
            }
        }
    }
    return rc;
}

typedef struct multi_writer_t {
    KFile * f;                          /* the file we are writing into, used by the writer-thread */
    uint64_t pos;                       /* the file-position for the writer-thread */
    KThread * thread;                   /* the thread that performs the writer */
    KQueue * empty_q;                   /* pre-allocated blocks to write to, client gets from it, thread puts to into it */
    KQueue * write_q;                   /* blocks to write, thread gets from it, client puts to into it */
    uint32_t q_wait_time;
} multi_writer_t;

static rc_t get_block( KQueue * q, uint32_t timeout, multi_writer_block_t ** block ) {
    rc_t rc = 0;
    bool running = true;
    *block = NULL;
    while ( 0 == rc && running ) {
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, timeout );
        if ( 0 != rc ) {
            ErrMsg( "copy_machine.c get_block().TimeoutInit( %lu ms ) -> %R", timeout, rc );
        } else {
            rc = KQueuePop ( q, ( void ** )block, &tm );
            if ( 0 == rc ) {
                /* nothing to do, we got a block */
                running = false;
            }
            else if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) ) {
                /* the q has been sealed, this can only happen if the writer is in trouble! */
                rc = RC( rcExe, rcFile, rcPacking, rcConstraint, rcViolated );
                ErrMsg( "get-block() called on a sealed queue" );
            } else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) ) {
                /* the queue is still empty after the timeout has elapsed, try again */
                rc = 0;
            }
            else {
                /* something else is wrong with the empty-q */
                ErrMsg( "copy_machine.c get_block, KQueuePop() -> %R", rc );
            }
        }
    }
    return rc;
}

static rc_t empty_a_queue( KQueue * q, uint32_t timeout ) {
    rc_t rc = 0;
    bool done = false;
    while( 0 == rc && !done ) {
        /* as long as ther to_write_q is not sealed */
        multi_writer_block_t * block;
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, timeout );
        if ( 0 != rc ) {
            ErrMsg( "copy_machine.c get_block().TimeoutInit( %lu ms ) -> %R", timeout, rc );
            done = true;
        } else {
            rc = KQueuePop ( q, ( void ** )&block, &tm );
            if ( 0 == rc ) {
                /* we can now release the data-block */
                release_multi_writer_block( block );
            } else {
                if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) ) {
                    /* the q has been sealed, we are done! */
                    done = true;
                    rc = 0;
                } else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) ) {
                    /* the q is still active, but so far nothing in it */
                    done = true;
                    rc = 0;
                } else {
                    ErrMsg( "copy_machine.c empty_a_queue().KQueuePop() -> %R", rc );
                }
            }
        }
    }
    return rc;
}

void release_multi_writer( struct multi_writer_t * self ) {
    if ( NULL != self ) {
        rc_t rc;
        /* first we have to wait for the thread to finish */
        if ( NULL != self -> thread ) {
            if ( NULL != self -> write_q ) {
                rc = KQueueSeal ( self -> write_q );
                if ( 0 != rc ) {
                    ErrMsg( "copy_machine.c release_multi_writer.KQueueSeal() -> %R", rc );
                }
            }
            KThreadWait ( self -> thread, &rc );
        }

        if ( NULL != self -> empty_q ) {
            /* before we can release this queue, we have to empty it... */
            empty_a_queue( self -> empty_q, self -> q_wait_time );
            rc = KQueueRelease ( self -> empty_q );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c release_multi_writer.KQueueRelease().1 -> %R", rc );
            }
        }
        if ( NULL != self -> write_q ) {
            /* this queue should not have anything in it, because we waited before for the thread to finish... */
            empty_a_queue( self -> write_q, self -> q_wait_time );
            rc = KQueueRelease ( self -> write_q );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c release_multi_writer.KQueueRelease().2 -> %R", rc );
            }
        }

        if ( NULL != self -> f ) { release_file( self -> f, "copy_machine.c release_multi_writer()" ); }
        free( ( void * ) self );
    }
}

static rc_t CC multi_writer_thread( const KThread * thread, void *data ) {
    rc_t rc = 0;
    multi_writer_t * self = data;
    bool done = false;
    while( 0 == rc && !done ) {
        /* as long as the to_write_q is not sealed */
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( 0 != rc ) {
            ErrMsg( "copy_machine.c multi_writer_thread().TimeoutInit() -> %R", rc );
        } else {
            multi_writer_block_t * block;
            rc = KQueuePop ( self -> write_q, ( void ** )&block, &tm );
            if ( 0 == rc ) {
                /* we got a block to write out of the to_write_q */

                if ( NULL != self -> f ) {
                    /* we have a file to write to... */
                    if ( NULL != block -> data && block -> len > 0 ) {
                        size_t num_written;
                        rc = KFileWrite( self -> f, self -> pos,
                                        block -> data, block -> len, &num_written );
                        if ( 0 == rc ) { self -> pos += num_written;  }
                    }
                } else {
                    /* no file to print into, write to stdout! */
                    rc = KOutMsg( "%.*s", block -> len, block -> data );
                }
                if ( 0 == rc ) {
                    /* put the block back into the empty-q */
                    rc = multi_writer_push ( self -> empty_q, block, self -> q_wait_time ); /* above */
                } else {
                    /* something went wrong with writing the block into the dst-file !!!
                       possibly we are running out of space to write... */

                    /* put the block back into the empty-q */
                    rc = multi_writer_push ( self -> empty_q, block, self -> q_wait_time ); /* above */

                    /* we are done ... seal the empty_q ( that will tell the reader to stop... */
                    {
                        rc_t rc2 = KQueueSeal ( self -> empty_q );
                        if ( 0 != rc2 ) {
                            ErrMsg( "copy_machine.c multi_writer_thread().KQueueSeal() -> %R", rc2 );
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                }
            } else {
                if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) ) {
                    /* the to_write_q has been sealed, we are done! */
                    done = true;
                    rc = 0;
                } else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) ) {
                    /* the to_write_q is still active, but so far nothing to write in it, try again */
                    rc = 0;
                }
            }
        }
    }
    return rc;
}

#define N_MULTI_WRITER_BLOCKS 16
#define MULTI_WRITER_BLOCK_SIZE ( 4 * 1024 * 1024 )
#define MULTI_WRITER_WAIT 5

static rc_t create_multi_writer_file( multi_writer_t * self, KDirectory * dir, const char * filename, size_t buf_size ) {
    rc_t rc = KDirectoryCreateFile( dir, &( self -> f ), false, 0664, kcmInit, "%s", filename );
    if ( 0 != rc ) {
        ErrMsg( "create_multi_writer().KDirectoryCreateFile( '%s' ) -> %R", filename, rc );
    } else {
        rc = wrap_file_in_buffer( &( self -> f ), buf_size, "copy_machine.c create_multi_writer()"  );
        if ( 0 != rc ) {
            ErrMsg( "create_multi_writer().wrap_file_in_buffer( '%s' ) -> %R", filename, rc );
        }
    }
    return rc;
}

struct multi_writer_t * create_multi_writer( KDirectory * dir,
                    const char * filename,
                    size_t buf_size,
                    uint32_t q_wait_time,
                    uint32_t q_num_blocks,
                    size_t q_block_size  ){
    uint32_t wait_time = ( 0 == q_wait_time ) ? MULTI_WRITER_WAIT : q_wait_time;
    uint32_t num_blocks = ( 0 == q_num_blocks ) ? N_MULTI_WRITER_BLOCKS : q_num_blocks;
    uint32_t block_size = ( 0 == q_block_size ) ? MULTI_WRITER_BLOCK_SIZE : q_block_size;
    multi_writer_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        rc_t rc = 0;
        if ( NULL != filename ) {
            rc = create_multi_writer_file( res, dir, filename, buf_size );
            if ( 0 != rc ) {
                release_multi_writer( res );
                res = NULL;
            }
        }
        if ( 0 == rc ) {
            /* create the empty queue */
            res -> q_wait_time = wait_time;
            rc = KQueueMake( &( res -> empty_q ), num_blocks );
            if ( 0 != rc ) {
                ErrMsg( "create_multi_writer().KQueueMake( '%s' ) -> %R", filename, rc );
                release_multi_writer( res );
                res = NULL;
            } else {
                /* fill it with blocks.. */
                uint32_t i;
                for ( i = 0; 0 == rc && i < num_blocks; ++i ) {
                    multi_writer_block_t * block = create_multi_writer_block( block_size );
                    if ( NULL != block ) {
                        rc = multi_writer_push( res -> empty_q, block, wait_time );
                    }
                }
                if ( 0 == rc ) {
                    rc = KQueueMake( &( res -> write_q ), num_blocks );
                }
                if ( 0 != rc ) {
                    release_multi_writer( res );
                    res = NULL;
                }
                if ( 0 == rc ) {
                    rc = hlp_make_thread( &( res -> thread ), multi_writer_thread,
                                          res, THREAD_DFLT_STACK_SIZE );
                    if ( 0 != rc ) {
                        ErrMsg( "create_multi_writer().helper_make_thread( writer-thread ) -> %R", rc );
                        release_multi_writer( res );
                        res = NULL;
                    }
                }
            }
        }
    }
    return res;
}

struct multi_writer_block_t * multi_writer_get_empty_block( struct multi_writer_t * self ) {
    struct multi_writer_block_t * block = NULL;
    if ( NULL != self ) {
        rc_t rc = get_block( self -> empty_q, self -> q_wait_time, &block );
        if ( 0 == rc ) {
            block -> len = 0;
        } else {
            block = NULL;
        }
    }
    return block;
}

bool multi_writer_submit_block( struct multi_writer_t * self, struct multi_writer_block_t * block ) {
    bool res = false;
    if ( NULL != self && NULL != block ) {
        rc_t rc =  multi_writer_push( self -> write_q, block, self -> q_wait_time );
        res = ( 0 == rc );
    }
    return res;
}

rc_t multi_writer_write( struct multi_writer_t * self,
                         const char * src,
                         size_t size )
{
    rc_t rc = 0;
    if ( NULL == self || NULL == src || 0 == size ) {
        rc = RC( rcExe, rcFile, rcPacking, rcConstraint, rcViolated );
    } else {
        /* first let us get a block from the empty pile */
        multi_writer_block_t * block;
        rc = get_block( self -> empty_q, self -> q_wait_time, &block );
        if ( 0 == rc ) {
            if ( NULL != block ) {
                if ( multi_writer_block_write( block, src, size ) ) {
                    rc =  multi_writer_push( self -> write_q, block, self -> q_wait_time );
                } else {
                    /* TBD: error... */
                }
            } else {
                /* TBD: error... */
            }
        } else {
            /* TBD: error... */
        }
    }
    return rc;
}
