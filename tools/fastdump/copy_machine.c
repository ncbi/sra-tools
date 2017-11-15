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

static void destroy_copy_machine( copy_machine * self )
{
    uint32_t i;
    
    /* first join the 2 threads... */
    rc_t rc_status;
    
    KQueueSeal ( self -> to_write_q );
    KThreadWait ( self -> thread, &rc_status );
    
    if ( self -> empty_q != NULL )
        KQueueRelease ( self -> empty_q );
    if ( self -> to_write_q != NULL )
        KQueueRelease ( self -> to_write_q );

    for ( i = 0; i < N_COPY_MACHINE_BLOCKS; ++i )
    {
        if ( self -> blocks[ i ] . buffer != NULL )
            free( ( void * ) self -> blocks[ i ] . buffer );
    }
}

static rc_t run_copy_machine( copy_machine * self )
{
    uint32_t file_count;
    rc_t rc = VNameListCount( self -> sources, &file_count );
    if ( rc != 0 )
        ErrMsg( "copy_machine_reader_thread.VNameListCount( sources ) -> %R", rc );
    else
    {
        uint32_t idx;
        for ( idx = self -> src_list_offset; rc == 0 && idx < file_count; ++idx )
        {
            const char * filename = NULL;
            rc = VNameListGet( self -> sources, idx, &filename );
            if ( rc == 0 )
            {
                const struct KFile * src;
                rc = make_buffered_for_read( self -> dir, &src, filename, self -> buf_size );
                if ( rc == 0 )
                {
                    uint64_t src_pos = 0;
                    size_t num_read = 1;
                    while ( rc == 0 && num_read > 0 )
                    {
                        /* get a buffer out of the empty-q */
                        struct timeout_t tm;
                        rc = TimeoutInit ( &tm, self -> q_wait_time );
                        if ( rc == 0 )
                        {
                            copy_machine_block * block;
                            rc = KQueuePop ( self -> empty_q, ( void ** )&block, &tm );
                            if ( rc == 0 )
                            {
                                block -> available = 0;
                                rc = KFileRead( src, src_pos, block -> buffer, self -> buf_size, &num_read );
                                if ( rc != 0 )
                                    ErrMsg( "copy_machine_reader_thread.KFileRead( at %lu ) -> %R", src_pos, rc );
                                else if ( num_read > 0 )
                                {
                                    /* we have data for the writer-thread */
                                    block -> available = num_read;
                                    src_pos += num_read;
                                    rc = KQueuePush ( self -> to_write_q, block, NULL );
                                    /* should never block, because we have enough entries in the queue! */
                                }
                                else
                                {
                                    /* we are done with this file, put the block back... */
                                    rc = KQueuePush ( self -> empty_q, block, NULL );
                                    /* should never block, because we have enough entries in the queue! */
                                }
                            }
                            else if ( GetRCState( rc ) == rcExhausted && GetRCObject( rc ) == ( enum RCObject )rcTimeout )
                            {
                                /* we have a timeout from KQueuePop(), that means the writer is busy,
                                   all blocks are in the to_write-queue or in processing, let's try again */
                                rc = 0;
                            }
                        }
                    }
                    /* rc = copy_file( dst, src, &dst_pos, buf_size, progress ); */
                    KFileRelease( src );
                }
                if ( rc == 0 )
                {
                    rc = KDirectoryRemove( self -> dir, true, "%s", filename );
                    if ( rc != 0 )
                        ErrMsg( "copy_machine_reader_thread.KDirectoryRemove( '%s' ) -> %R", filename, rc );
                }
            }
        }
        /* we are done ... seal the to_write_q ( the one the writer is listening at ) */
        KQueueSeal ( self -> to_write_q );
    }
    return rc;
}

static rc_t CC copy_machine_thread( const KThread * thread, void *data )
{
    rc_t rc = 0;
    copy_machine * self = data;
    bool done = false;
    while( rc == 0 && !done )
    {
        /* as long as ther to_write_q is not sealed */
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( rc == 0 )
        {
            copy_machine_block * block;
            rc = KQueuePop ( self -> to_write_q, ( void ** )&block, /*&tm*/ NULL );
            if ( rc == 0 )
            {
                /* we got a block to write out of the to_write_q */
                size_t num_written;
                rc = KFileWrite( self -> dst, self -> dst_pos,
                                 block -> buffer, block -> available, &num_written );
                if ( rc == 0 )
                {
                    /* increment the write - position */
                    self -> dst_pos += num_written;
                    
                    /* inform the background-process about it */
                    bg_progress_update( self -> progress, num_written );
                    
                    /* put the block back into the empty-q */
                    block -> available = 0;
                    rc = KQueuePush ( self -> empty_q, block, NULL ); /* this might block! */
                }
            }
            else
            {
                if ( GetRCState( rc ) == rcDone && GetRCObject( rc ) == ( enum RCObject )rcData )
                {
                    /* the to_write_q has been sealed, we are done! */
                    done = true;
                    rc = 0;
                }
                else if ( GetRCState( rc ) == rcExhausted && GetRCObject( rc ) == ( enum RCObject )rcTimeout )
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
    if ( dst == NULL || buf_size == 0 || sources == NULL )
    {
        rc = RC( rcExe, rcFile, rcPacking, rcParam, rcInvalid );
        ErrMsg( "init_copy_machine() -> %R", rc );
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
        for ( i = 0; rc == 0 && i < N_COPY_MACHINE_BLOCKS; ++i )
        {
            cm . blocks[ i ] . buffer = malloc( buf_size );
            if ( cm . blocks[ i ] . buffer == NULL )
            {
                rc = RC( rcExe, rcFile, rcPacking, rcMemory, rcExhausted );
                ErrMsg( "init_copy_machine.malloc( %d ) -> %R", buf_size, rc );
            }
        }
        if ( rc == 0 )
        {
            rc = KQueueMake ( &( cm . empty_q ), N_COPY_MACHINE_BLOCKS );
            if ( rc != 0 )
                ErrMsg( "init_copy_machine.KQueueMake( empty_q ) -> %R", rc );
        }
        if ( rc == 0 )
        {
            rc = KQueueMake ( &( cm . to_write_q ), N_COPY_MACHINE_BLOCKS );
            if ( rc != 0 )
                ErrMsg( "init_copy_machine.KQueueMake( to_write_q ) -> %R", rc );
        }
        for ( i = 0; rc == 0 && i < N_COPY_MACHINE_BLOCKS; ++i )
        {
            rc = KQueuePush ( cm . empty_q, &( cm . blocks[ i ] ), NULL ); /* this might block! */
            if ( rc != 0 )
                ErrMsg( "init_copy_machine.KQueuePush( empty_q ) -> %R", rc );
        }

        if ( rc == 0 )
            rc = KThreadMake( &( cm . thread ), copy_machine_thread, &cm );
            
        if ( rc == 0 )
            rc = run_copy_machine( &cm );
            
        destroy_copy_machine( &cm );
    }
    return rc;
}
