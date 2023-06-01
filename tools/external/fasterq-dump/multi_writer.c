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
#include "multi_writer.h"

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

typedef struct multi_writer_block_t
{
    char * data;
    size_t len;
    size_t available;
} multi_writer_block_t;

static multi_writer_block_t * mw_create_block( size_t size ) {
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

static void mw_release_block( multi_writer_block_t * self ) {
    if ( NULL != self ) {
        free( ( void * ) self -> data );
        free( ( void * ) self );
    }
}

bool mw_expand_block( multi_writer_block_t * self, size_t size ) {
    bool res = false;
    if ( NULL != self && size > self -> available ) {
        free( ( void * )( self -> data ) );
        self -> data = calloc( 1, size );
        res = ( NULL != self -> data );
        self -> available = ( res ) ? size : 0;
    }
    return res;
}

static bool mw_multi_writer_block_write( multi_writer_block_t * self, const char * data, size_t size ) {
    bool res = false;
    if ( NULL != self && NULL != data && size > 0 ) {
        if ( size > self -> available ) {
            /* we have to make the block bigger */
            res = mw_expand_block( self, size );
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

bool mw_append_block( multi_writer_block_t * self, const char * data, size_t size ) {
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

static rc_t mw_push( KQueue * q, const void * item, uint32_t wait_time ) {
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

static rc_t mw_get_block( KQueue * q, uint32_t timeout, multi_writer_block_t ** block ) {
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

static rc_t mw_empty_a_queue( KQueue * q, uint32_t timeout ) {
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
                mw_release_block( block );
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

void mw_release( struct multi_writer_t * self ) {
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
            mw_empty_a_queue( self -> empty_q, self -> q_wait_time );
            rc = KQueueRelease ( self -> empty_q );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c release_multi_writer.KQueueRelease().1 -> %R", rc );
            }
        }
        if ( NULL != self -> write_q ) {
            /* this queue should not have anything in it, because we waited before for the thread to finish... */
            mw_empty_a_queue( self -> write_q, self -> q_wait_time );
            rc = KQueueRelease ( self -> write_q );
            if ( 0 != rc ) {
                ErrMsg( "copy_machine.c release_multi_writer.KQueueRelease().2 -> %R", rc );
            }
        }

        if ( NULL != self -> f ) { ft_release_file( self -> f, "copy_machine.c release_multi_writer()" ); }
        free( ( void * ) self );
    }
}

static rc_t CC mw_thread( const KThread * thread, void *data ) {
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
                    rc = mw_push ( self -> empty_q, block, self -> q_wait_time ); /* above */
                } else {
                    /* something went wrong with writing the block into the dst-file !!!
                       possibly we are running out of space to write... */

                    /* put the block back into the empty-q */
                    rc = mw_push ( self -> empty_q, block, self -> q_wait_time ); /* above */

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

static rc_t mw_create_file( multi_writer_t * self, KDirectory * dir, const char * filename, size_t buf_size ) {
    rc_t rc = KDirectoryCreateFile( dir, &( self -> f ), false, 0664, kcmInit, "%s", filename );
    if ( 0 != rc ) {
        ErrMsg( "create_multi_writer().KDirectoryCreateFile( '%s' ) -> %R", filename, rc );
    } else {
        rc = ft_wrap_file_in_buffer( &( self -> f ), buf_size, "copy_machine.c create_multi_writer()"  );
        if ( 0 != rc ) {
            ErrMsg( "create_multi_writer().wrap_file_in_buffer( '%s' ) -> %R", filename, rc );
        }
    }
    return rc;
}

struct multi_writer_t * mw_create( KDirectory * dir,
                    const char * filename,
                    size_t buf_size,
                    uint32_t q_wait_time,
                    uint32_t q_num_blocks,
                    size_t q_block_size  ) {
    uint32_t wait_time = ( 0 == q_wait_time ) ? MULTI_WRITER_WAIT : q_wait_time;
    uint32_t num_blocks = ( 0 == q_num_blocks ) ? N_MULTI_WRITER_BLOCKS : q_num_blocks;
    uint32_t block_size = ( 0 == q_block_size ) ? MULTI_WRITER_BLOCK_SIZE : q_block_size;
    multi_writer_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        rc_t rc = 0;
        if ( NULL != filename ) {
            rc = mw_create_file( res, dir, filename, buf_size );
            if ( 0 != rc ) {
                mw_release( res );
                res = NULL;
            }
        }
        if ( 0 == rc ) {
            /* create the empty queue */
            res -> q_wait_time = wait_time;
            rc = KQueueMake( &( res -> empty_q ), num_blocks );
            if ( 0 != rc ) {
                ErrMsg( "mw_create().KQueueMake( '%s' ) -> %R", filename, rc );
                mw_release( res );
                res = NULL;
            } else {
                /* fill it with blocks.. */
                uint32_t i;
                for ( i = 0; 0 == rc && i < num_blocks; ++i ) {
                    multi_writer_block_t * block = mw_create_block( block_size );
                    if ( NULL != block ) {
                        rc = mw_push( res -> empty_q, block, wait_time );
                    }
                }
                if ( 0 == rc ) {
                    rc = KQueueMake( &( res -> write_q ), num_blocks );
                }
                if ( 0 != rc ) {
                    mw_release( res );
                    res = NULL;
                }
                if ( 0 == rc ) {
                    rc = hlp_make_thread( &( res -> thread ), mw_thread,
                                          res, THREAD_DFLT_STACK_SIZE );
                    if ( 0 != rc ) {
                        ErrMsg( "mw_create().helper_make_thread( writer-thread ) -> %R", rc );
                        mw_release( res );
                        res = NULL;
                    }
                }
            }
        }
    }
    return res;
}

struct multi_writer_block_t * mw_get_empty_block( struct multi_writer_t * self ) {
    struct multi_writer_block_t * block = NULL;
    if ( NULL != self ) {
        rc_t rc = mw_get_block( self -> empty_q, self -> q_wait_time, &block );
        if ( 0 == rc ) {
            block -> len = 0;
        } else {
            block = NULL;
        }
    }
    return block;
}

bool mw_submit_block( struct multi_writer_t * self, struct multi_writer_block_t * block ) {
    bool res = false;
    if ( NULL != self && NULL != block ) {
        rc_t rc =  mw_push( self -> write_q, block, self -> q_wait_time );
        res = ( 0 == rc );
    }
    return res;
}

static rc_t mw_write( struct multi_writer_t * self, const char * src, size_t size )
{
    rc_t rc = 0;
    if ( NULL == self || NULL == src || 0 == size ) {
        rc = RC( rcExe, rcFile, rcPacking, rcConstraint, rcViolated );
    } else {
        /* first let us get a block from the empty pile */
        multi_writer_block_t * block;
        rc = mw_get_block( self -> empty_q, self -> q_wait_time, &block );
        if ( 0 == rc ) {
            if ( NULL != block ) {
                if ( mw_multi_writer_block_write( block, src, size ) ) {
                    rc =  mw_push( self -> write_q, block, self -> q_wait_time );
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
