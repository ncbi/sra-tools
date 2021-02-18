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

#include "helper.h"

#include <kfs/buffile.h>
#include <klib/time.h>

#include <kproc/thread.h>
#include <kproc/queue.h>
#include <kproc/timeout.h>

typedef struct copy_machine_block
{
    char * buffer;
    size_t available;
} copy_machine_block;

#define N_COPY_MACHINE_BLOCKS 4

typedef struct copy_machine
{
    KDirectory * dir;
    KFile * dst;
    struct bg_progress * progress;
    KThread * thread;
    KQueue * empty_q;
    KQueue * to_write_q;
    const struct VNamelist * sources;
    copy_machine_block blocks[ N_COPY_MACHINE_BLOCKS ];
    uint64_t dst_pos;
    size_t buf_size;
    uint32_t src_list_offset;
    uint32_t q_wait_time;
} copy_machine;

static rc_t destroy_copy_machine( copy_machine * self )
{
    uint32_t i;

    /* first join the 2 threads... */
    rc_t rc, rc2;

    rc2 = KQueueSeal ( self -> to_write_q );
    {
        if ( 0 != rc2 )
        {
            ErrMsg( "copy_machine.c destroy_copy_machine.KQueueSeal() -> %R", rc2 );
        }
    }
    KThreadWait ( self -> thread, &rc );
    
    if ( NULL != self -> empty_q )
    {
        rc2 = KQueueRelease ( self -> empty_q );
        if ( 0 != rc2 )
        {
            ErrMsg( "copy_machine.c destroy_copy_machine.KQueueRelease().1 -> %R", rc2 );
        }
    }
    if ( NULL != self -> to_write_q )
    {
        rc2 = KQueueRelease ( self -> to_write_q );
        if ( 0 != rc2 )
        {
            ErrMsg( "copy_machine.c destroy_copy_machine.KQueueRelease().2 -> %R", rc2 );
        }
    }

    for ( i = 0; i < N_COPY_MACHINE_BLOCKS; ++i )
    {
        if ( NULL != self -> blocks[ i ] . buffer )
        {
            free( ( void * ) self -> blocks[ i ] . buffer );
        }
    }
    return rc;
}

static rc_t push2q( KQueue * q, const void * item, uint32_t wait_time )
{
    rc_t rc;
    bool running = true;
    while ( running )
    {
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, wait_time );
        if ( 0 != rc )
        {
            ErrMsg( "copy_machine.c push2q().TimeoutInit( %u ) -> %R", wait_time, rc );
            running = false;
        }
        else
        {
            rc = KQueuePush ( q, item, &tm );
            if ( 0 == rc )
            {
                running = false;
            }
            else
            {
                bool timed_out = ( rcExhausted != GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) );
                if ( timed_out )
                {
                    rc_t rc2 = KSleepMs( wait_time );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "copy_machine.c push2q().KSleepMs() -> %R", rc2 );
                    }
                }
                else
                {
                    ErrMsg( "copy_machine.c push2q().KQueuePush() -> %R", rc );
                    running = false;
                }
            }
        }
    }
    return rc;
}

static rc_t copy_this_file( copy_machine * self, const struct KFile * src )
{
    rc_t rc = 0;
    uint64_t src_pos = 0;
    size_t num_read = 1;
    while ( 0 == rc && num_read > 0 )
    {
        /* get a buffer out of the empty-q */
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( 0 != rc )
        {
            ErrMsg( "copy_machine.c copy_this_file().TimeoutInit( %lu ms ) -> %R", self -> q_wait_time, rc );
        }
        else
        {
            copy_machine_block * block;
            rc = KQueuePop ( self -> empty_q, ( void ** )&block, &tm );
            if ( 0 == rc )
            {
                block -> available = 0;
                rc = KFileRead( src, src_pos, block -> buffer, self -> buf_size, &num_read );
                if ( 0 != rc )
                {
                    ErrMsg( "copy_machine.c copy_this_file().KFileRead( at %lu ) -> %R", src_pos, rc );
                }
                else if ( num_read > 0 )
                {
                    /* we have data for the writer-thread */
                    block -> available = num_read;
                    src_pos += num_read;
                    rc = push2q ( self -> to_write_q, block, self -> q_wait_time );
                }
                else
                {
                    /* we are done with this file, put the block back... */
                    rc = push2q ( self -> empty_q, block, self -> q_wait_time );
                }
            }
            else if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) )
            {
                /* the empty_q has been sealed, this can only happen if the writer is in trouble! */
                rc = RC( rcExe, rcFile, rcPacking, rcConstraint, rcViolated );
                ErrMsg( "you have exhausted your space" );
            }
            else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) )
            {
                /* we have a timeout from KQueuePop(), that means the writer is busy,
                   all blocks are in the to_write-queue or in processing, let's try again */
                rc = 0;
            }
            else
            {
                /* something else is wrong with the empty-q */
                ErrMsg( "copy_machine.c copy_this_file().KQueuePop( empty-q ) -> %R", rc );
            }
        }
    }
    return rc;
}

static rc_t run_copy_machine( copy_machine * self )
{
    uint32_t file_count;
    rc_t rc = VNameListCount( self -> sources, &file_count );
    if ( 0 != rc )
    {
        ErrMsg( "copy_machine.c run_copy_machine().VNameListCount( sources ) -> %R", rc );
    }
    else
    {
        uint32_t idx;
        for ( idx = self -> src_list_offset; 0 == rc && idx < file_count; ++idx )
        {
            const char * filename = NULL;
            rc = VNameListGet( self -> sources, idx, &filename );
            if ( 0 != rc )
            {
                ErrMsg( "copy_machine.c run_copy_machine().VNameListGet( %u ) -> %R", idx, rc );
            }
            else
            {
                const struct KFile * src;
                rc = make_buffered_for_read( self -> dir, &src, filename, self -> buf_size ); /* helper.c */
                if ( 0 == rc )
                {
                    rc_t rc2;
                    rc = copy_this_file( self, src ); /* above */
                    rc2 = KFileRelease( src );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "copy_machine.c run_copy_machine().KFileRelease()[ %u ] -> %R", idx, rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
                if ( 0 == rc )
                {
                    rc = KDirectoryRemove( self -> dir, true, "%s", filename );
                    if ( 0 != rc )
                    {
                        ErrMsg( "copy_machine.c run_copy_machine().KDirectoryRemove( '%s' ) -> %R", filename, rc );
                    }
                }
            }
        }
        /* we are done ... seal the to_write_q ( the one the writer is listening at ) */
        {
            rc_t rc2 = KQueueSeal ( self -> to_write_q );
            if ( 0 != rc2 )
            {
                ErrMsg( "copy_machine.c run_copy_machine().KQueueSeal() -> %R", rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static rc_t CC copy_machine_writer_thread( const KThread * thread, void *data )
{
    rc_t rc = 0;
    copy_machine * self = data;
    bool done = false;
    while( 0 == rc && !done )
    {
        /* as long as ther to_write_q is not sealed */
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( 0 != rc )
        {
            ErrMsg( "copy_machine.c copy_machine_writer_thread().TimeoutInit() -> %R", rc );
        }
        else
        {
            copy_machine_block * block;
            rc = KQueuePop ( self -> to_write_q, ( void ** )&block, /*&tm*/ NULL );
            if ( 0 == rc )
            {
                /* we got a block to write out of the to_write_q */
                size_t num_written;
                rc = KFileWrite( self -> dst, self -> dst_pos,
                                 block -> buffer, block -> available, &num_written );
                if ( 0 == rc )
                {
                    /* increment the write - position */
                    self -> dst_pos += num_written;

                    /* inform the background-process about it */
                    bg_progress_update( self -> progress, num_written ); /* progress_thread.c */

                    /* put the block back into the empty-q */
                    block -> available = 0;
                    rc = push2q ( self -> empty_q, block, self -> q_wait_time ); /* above */
                }
                else
                {
                    /* something went wrong with writing the block into the dst-file !!!
                       possibly we are running out of space to write... */

                    /* put the block back into the empty-q */
                    block -> available = 0;
                    rc = push2q ( self -> empty_q, block, self -> q_wait_time ); /* above */
    
                    /* we are done ... seal the empty_q ( that will tell the reader to stop... */
                    {
                        rc_t rc2 = KQueueSeal ( self -> empty_q );
                        if ( 0 != rc2 )
                        {
                            ErrMsg( "copy_machine.c copy_machine_writer_thread().KQueueSeal() -> %R", rc2 );
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                }
            }
            else
            {
                if ( rcDone == GetRCState( rc ) && ( enum RCObject )rcData == GetRCObject( rc ) )
                {
                    /* the to_write_q has been sealed, we are done! */
                    done = true;
                    rc = 0;
                }
                else if ( rcExhausted == GetRCState( rc ) && ( enum RCObject )rcTimeout == GetRCObject( rc ) )
                {
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
                  struct bg_progress * progress,
                  uint64_t dst_offset,
                  size_t buf_size,
                  uint32_t src_list_offset,
                  uint32_t q_wait_time )
{
    rc_t rc = 0;
    if ( NULL == dst || 0 == buf_size || NULL == sources )
    {
        rc = RC( rcExe, rcFile, rcPacking, rcParam, rcInvalid );
        ErrMsg( "copy_machine.c make_a_copy() -> %R", rc );
    }
    else
    {
        copy_machine cm;
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
            
        for ( i = 0; i < N_COPY_MACHINE_BLOCKS; ++i )
        {
            cm . blocks[ i ] . buffer = NULL;
            cm . blocks[ i ] . available = 0;
        }
        for ( i = 0; 0 == rc && i < N_COPY_MACHINE_BLOCKS; ++i )
        {
            cm . blocks[ i ] . buffer = malloc( buf_size );
            if ( NULL == cm . blocks[ i ] . buffer )
            {
                rc = RC( rcExe, rcFile, rcPacking, rcMemory, rcExhausted );
                ErrMsg( "copy_machine.c make_a_copy().malloc( %d ) -> %R", buf_size, rc );
            }
        }
        if ( 0 == rc )
        {
            rc = KQueueMake ( &( cm . empty_q ), N_COPY_MACHINE_BLOCKS );
            if ( 0 != rc )
            {
                ErrMsg( "copy_machine.c make_a_copy().KQueueMake( empty_q ) -> %R", rc );
            }
        }
        if ( 0 == rc )
        {
            rc = KQueueMake ( &( cm . to_write_q ), N_COPY_MACHINE_BLOCKS );
            if ( 0 != rc )
            {
                ErrMsg( "copy_machine.c make_a_copy().KQueueMake( to_write_q ) -> %R", rc );
            }
        }
        for ( i = 0; 0 == rc && i < N_COPY_MACHINE_BLOCKS; ++i )
        {
            rc = push2q ( cm . empty_q, &( cm . blocks[ i ] ), q_wait_time );
            if ( 0 != rc )
            {
                ErrMsg( "copy_machine.c make_a_copy().KQueuePush( empty_q ) -> %R", rc );
            }
        }

        /* create a writer-thread */
        if ( 0 == rc )
        {
            rc = helper_make_thread( &( cm . thread ), copy_machine_writer_thread,
                                    &cm, THREAD_DFLT_STACK_SIZE );
            if ( 0 != rc )
            {
                ErrMsg( "copy_machine.c make_a_copy().helper_make_thread( writer-thread ) -> %R", rc );
            }
        }

        /* read the data on the current thread */
        if ( 0 == rc )
        {
            rc = run_copy_machine( &cm ); /* above */
        }

        {
            rc_t rc2 = destroy_copy_machine( &cm ); /* above */
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}
