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
#include "lookup_reader.h"
#include "lookup_writer.h"
#include "index.h"
#include "helper.h"

#include <klib/out.h>
#include <klib/text.h>
#include <klib/status.h>
#include <klib/printf.h>
#include <klib/progressbar.h>
#include <klib/time.h>

#include <kproc/thread.h>
#include <kproc/queue.h>
#include <kproc/timeout.h>

typedef struct merge_src
{
    struct lookup_reader * reader;
    uint64_t key;
    SBuffer packed_bases;
    rc_t rc;
} merge_src;


static merge_src * get_min_merge_src( merge_src * src, uint32_t count )
{
    merge_src * res = NULL;
    uint32_t i;
    for ( i = 0; i < count; ++i )
    {
        merge_src * item = &src[ i ];
        if ( item -> rc == 0 )
        {
            if ( res == NULL )
            {
                /* pick the first one */
                res = item;
            }
            else if ( item -> key < res -> key )
            {
                /* if this src has a smaller key... */
                res = item;
            }
        }
    }
    return res;
} 

/* ================================================================================= */

typedef struct merge_sorter
{
    
    struct lookup_writer * dst; /* lookup_writer.h */
    struct index_writer * idx;  /* index.h */
    merge_src * src;            /* vector of input-files to be merged */
    struct bg_update * gap;     /* indicator of running merge */
    uint64_t total_size, total_entries;
    uint32_t num_src;
} merge_sorter;


static rc_t init_merge_sorter( merge_sorter * self,
                               KDirectory * dir,
                               const char * output,
                               const char * index,
                               VNamelist * files,
                               size_t buf_size,
                               uint32_t num_src,
                               struct bg_update * gap )
{
    rc_t rc = 0;
    uint32_t i;
    
    if ( index != NULL )
        rc = make_index_writer( dir, &( self -> idx ), buf_size,
                        DFLT_INDEX_FREQUENCY, "%s", index ); /* index.h */
    else
        self -> idx = NULL;

    self -> total_size = 0;
    self -> total_entries = 0;
    self -> num_src = num_src;
    self -> gap = gap;
    
    if ( rc == 0 )
        rc = make_lookup_writer( dir, self -> idx, &( self -> dst ), buf_size, "%s", output ); /* lookup_writer.h */
    
    if ( rc == 0 )
    {
        self -> src = calloc( self -> num_src, sizeof * self-> src );
        if ( self -> src == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "init_merge_sorter2.calloc( %d ) failed", ( ( sizeof * self -> src ) * self -> num_src ) );
        }
    }
    
    for ( i = 0; rc == 0 && i < self -> num_src; ++i )
    {
        const char * filename;
        rc = VNameListGet ( files, i, &filename );
        if ( rc == 0 )
        {
            merge_src * s = &self -> src[ i ];
            if ( rc == 0 )
                rc = make_lookup_reader( dir, NULL, &s -> reader, buf_size, "%s", filename ); /* lookup_reader.h */

            if ( rc == 0 )
            {
                rc = make_SBuffer( &s -> packed_bases, 4096 );
                if ( rc == 0 )
                    s -> rc = lookup_reader_get( s -> reader, &s -> key, &s -> packed_bases ); /* lookup_reader.h */
            }
        }
    }
    return rc;
}

static void release_merge_sorter( merge_sorter * self )
{
    release_lookup_writer( self -> dst );
    release_index_writer( self -> idx );
    if ( self -> src != NULL )
    {
        uint32_t i;    
        for ( i = 0; i < self -> num_src; ++i )
        {
            merge_src * s = &self -> src[ i ];
            release_lookup_reader( s -> reader );
            release_SBuffer( &s -> packed_bases );
        }
        free( ( void * ) self -> src );
    }
}

static rc_t run_merge_sorter( merge_sorter * self )
{
    rc_t rc = 0;
    uint64_t last_key = 0;
    uint64_t loop_nr = 0;
    merge_src * to_write = get_min_merge_src( self -> src, self -> num_src ); /* above */

    while( rc == 0 && to_write != NULL )
    {
        rc = Quitting();
        if ( rc == 0 )
        {
            if ( last_key > to_write -> key )
            {
                rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
                ErrMsg( "run_merge_sorter() %lu -> %lu in loop #%lu", last_key, to_write -> key, loop_nr );
            }
            else
            {
                loop_nr ++;
                last_key = to_write -> key;
                rc = write_packed_to_lookup_writer( self -> dst,
                                                    to_write -> key,
                                                    &to_write -> packed_bases . S ); /* lookup_writer.h */
                                                    
                if ( rc == 0 )
                    to_write -> rc = lookup_reader_get( to_write -> reader,
                                                        &to_write -> key,
                                                        &to_write -> packed_bases ); /* lookup_reader.h */

                to_write = get_min_merge_src( self -> src, self -> num_src ); /* above */
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

typedef struct background_vector_merger
{
    KDirectory * dir;               /* needed to perform the merge-sort */
    const struct temp_dir * temp_dir; /* needed to create temp. files */
    KQueue * job_q;                 /* the KVector objects arrive here from the lookup-producer */
    KThread * thread;               /* the thread that performs the merge-sort */
    struct background_file_merger * file_merger;    /* below */
    struct KFastDumpCleanupTask * cleanup_task;     /* add the produced temp_files here too */
    uint32_t product_id;            /* increased by one for each batch-run, used in temp-file-name */
    uint32_t batch_size;            /* how many KVectors have to arrive to run a batch */
    uint32_t q_wait_time;           /* timeout in milliseconds to get something out of in_q */
    size_t buf_size;                /* needed to perform the merge-sort */
    struct bg_update * gap;         /* visualize the gap after the producer finished */
    uint64_t total;                 /* how many entries have been merged... */
    uint64_t total_rowcount_prod;   /* updated by the producer, informs the vector-merger about the
                                       rowcount to be processed */
} background_vector_merger;


static rc_t wait_for_background_vector_merger( background_vector_merger * self )
{
    rc_t rc_status;
    rc_t rc = KThreadWait ( self -> thread, &rc_status );
    if ( rc == 0 )
        rc = rc_status;
    return rc;
}

static void release_background_vector_merger( background_vector_merger * self )
{
    if ( self -> job_q != NULL )
        KQueueRelease ( self -> job_q );
    free( self );
}

rc_t wait_for_and_release_background_vector_merger( background_vector_merger * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcReleasing, rcSelf, rcNull );
    else
    {
        /* while we are waiting on the background-vector-merger to finish,
           show a progress-indicator... */
        rc = wait_for_background_vector_merger( self );

        /* now we can signal to the file-merger that nothing will be pushed into the
           queue any more... */
           
        if ( rc == 0 )
            rc = seal_background_file_merger( self -> file_merger );

        if ( rc == 0 && ( self -> total != self -> total_rowcount_prod ) )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSize, rcInvalid );
            ErrMsg( "merge_sorter.c wait_for_and_release_background_vector_merger() : processed lookup rows: %lu of %lu",
                    self -> total, self -> total_rowcount_prod );
        }
        release_background_vector_merger( self );
    }
    
    if ( rc != 0 )
        ErrMsg( "merge_sorter.c wait_for_and_release_background_vector_merger()", rc );
    return rc;
}

typedef struct bg_vec_merge_src
{
    KVector * store;
    uint64_t key;
    const String * bases;
    rc_t rc;
} bg_vec_merge_src;


static rc_t init_bg_vec_merge_src( bg_vec_merge_src * src, KVector * store )
{
    src -> store = store;
    src -> rc = KVectorGetFirstPtr ( src -> store, &( src -> key ), ( void ** )&( src -> bases ) );
    return src -> rc;
}

static void release_bg_vec_merge_src( bg_vec_merge_src * src )
{
    if ( src -> store != NULL )
        KVectorRelease( src -> store );
}

static bg_vec_merge_src * get_min_bg_vec_merge_src( bg_vec_merge_src * batch, uint32_t count )
{
    bg_vec_merge_src * res = NULL;
    uint32_t i;
    for ( i = 0; i < count; ++i )
    {
        bg_vec_merge_src * item = &batch[ i ];
        if ( item -> rc == 0 )
        {
            if ( res == NULL )
                res = item;
            else if ( item -> key < res -> key )
                res = item;
        }
    }
    return res;
} 

static rc_t write_bg_vec_merge_src( bg_vec_merge_src * src, struct lookup_writer * writer )
{
    rc_t rc = src -> rc;
    if ( rc == 0 )
    {
        rc = write_packed_to_lookup_writer( writer, src -> key, src -> bases ); /* lookup_writer.c */
        StringWhack ( src -> bases );
        src -> bases = NULL;
    }
    if ( rc == 0 )
    {
        uint64_t next_key;
        src -> rc = KVectorGetNextPtr ( src -> store, &next_key, src -> key, ( void ** )&( src -> bases ) );
        src -> key = next_key;
    }
    return rc;
}

static rc_t background_vector_merger_collect_batch( background_vector_merger * self,
                                                    bg_vec_merge_src ** batch,
                                                    uint32_t * count )
{
    rc_t rc = 0;
    bg_vec_merge_src * b = calloc( self -> batch_size, sizeof * b );
    *batch = NULL;
    *count = 0;
    
    if ( b == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        bool sealed = false;
        while ( rc == 0 && *count < self -> batch_size && !sealed )
        {
            struct timeout_t tm;
            rc = TimeoutInit ( &tm, self -> q_wait_time );
            if ( rc == 0 )
            {
                KVector * store = NULL;
                rc = KQueuePop ( self -> job_q, ( void ** )&store, &tm );
                if ( rc == 0 )
                {
                    /* we pulled out a store from the Q */
                    STATUS ( STAT_USR, "KQueuePop() : store = %p", store );
                    rc = init_bg_vec_merge_src( &( b[ *count ] ), store );
                    if ( rc == 0 )
                        *count += 1;
                }
                else
                {
                    STATUS ( STAT_USR, "KQueuePop() : %R, store = %p", rc, store );
                    if ( GetRCState( rc ) == rcDone && GetRCObject( rc ) == ( enum RCObject )rcData )
                    {
                        /* the other side has sealed the Q */
                        sealed = true;
                        rc = 0;
                    }
                    else if ( GetRCState( rc ) == rcExhausted && GetRCObject( rc ) == ( enum RCObject )rcTimeout )
                    {
                        /* we had a timeout while trying to get a store from the Q */
                        rc = 0;
                    }
                }
            }
        }
    }
    if ( *count != 0 )
    {
        *batch = b;
        rc = 0;
    }
    else
        free( b );
    return rc;
}

static bool batch_valid( bg_vec_merge_src * batch, uint32_t count )
{
    bool res = false;
    uint32_t i;
    for ( i = 0; i < count && !res; ++i )
    {
        if ( batch[ i ] . rc == 0 )
            res = true;
    }
    return res;
}

static rc_t background_vector_merger_process_batch( background_vector_merger * self,
                                                    bg_vec_merge_src * batch,
                                                    uint32_t count )
{
    char buffer[ 4096 ];
    rc_t rc = generate_bg_sub_filename( self -> temp_dir, buffer, sizeof buffer, self -> product_id );
    if ( rc != 0 )
        ErrMsg( "merge_sorter.c background_vector_merger_process_batch() -> %R", rc );
    else
    {
        STATUS ( STAT_USR, "batch output filename is : %s", buffer );
        rc = Add_File_to_Cleanup_Task ( self -> cleanup_task, buffer );

        if ( rc == 0 )
        {
            struct lookup_writer * writer; /* lookup_writer.h */
            rc = make_lookup_writer( self -> dir, NULL, &writer,
                                     self -> buf_size, "%s", buffer ); /* lookup_writer.c */
            if ( rc == 0 )
                self -> product_id += 1;

            if ( rc == 0 )
            {
                bg_vec_merge_src * to_write = get_min_bg_vec_merge_src( batch, count ); /* above */
                while( rc == 0 && to_write != NULL )
                {
                    rc = Quitting();
                    if ( rc == 0 )
                    {
                        rc = write_bg_vec_merge_src( to_write, writer ); /* above */
                        if ( rc == 0 )
                        {
                            self -> total++;
                            to_write = get_min_bg_vec_merge_src( batch, count ); /* above */
                        }
                        else
                            to_write = NULL;
                        bg_update_update( self -> gap, 1 );
                    }
                }
                release_lookup_writer( writer ); /* lookup_writer.c */
            }
        }
        
        /*
        if ( rc == 0 && self -> file_merger != NULL )
            rc = lookup_check_file( self -> dir, self -> buf_size, buffer );
        */

        if ( rc == 0 && self -> file_merger != NULL )
            rc = push_to_background_file_merger( self -> file_merger, buffer ); /* below */
    }
    return rc;
}

static rc_t CC background_vector_merger_thread_func( const KThread * thread, void *data )
{
    rc_t rc = 0;
    background_vector_merger * self = data;
    bool done = false;

    STATUS ( STAT_USR, "starting background thread loop" );
    while( rc == 0 && !done )
    {
        bg_vec_merge_src * batch = NULL;
        uint32_t count = 0;
        
        /* Step 1 : get n = batch_size KVector's out of the in_q */
        STATUS ( STAT_USR, "collecting batch" );
        rc = background_vector_merger_collect_batch( self, &batch, &count );
        STATUS ( STAT_USR, "done collectin batch: rc = %R, count = %u", rc, count );
        if ( rc == 0 )
        {
            done = ( count == 0 );
            if ( !done )
            {
                if ( batch_valid( batch, count ) )
                {
                    /* Step 2 : process the batch */
                    STATUS ( STAT_USR, "processing batch of %u vectors", count );
                    rc = background_vector_merger_process_batch( self, batch, count );
                    STATUS ( STAT_USR, "finished processing: rc = %R", rc );
                }
                else
                {
                    STATUS ( STAT_USR, "we have an invalid batch!" );
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                }
            }
        }
        if ( batch != NULL )
        {
            uint32_t i;
            for ( i = 0; i < count; ++i )
                release_bg_vec_merge_src( &( batch[ i ] ) );
            free( batch );
        }
    }
    STATUS ( STAT_USR, "exiting background thread loop" );
    return rc;
}

rc_t make_background_vector_merger( struct background_vector_merger ** merger,
                             KDirectory * dir,
                             const struct temp_dir * temp_dir,
                             struct KFastDumpCleanupTask * cleanup_task,                             
                             struct background_file_merger * file_merger,
                             uint32_t batch_size,
                             uint32_t q_wait_time,
                             size_t buf_size,
                             struct bg_update * gap )
{
    rc_t rc = 0;
    background_vector_merger * b = calloc( 1, sizeof * b );
    *merger = NULL;
    if ( b == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        b -> dir = dir;
        b -> temp_dir = temp_dir;
        b -> batch_size = batch_size;
        b -> q_wait_time = q_wait_time;
        b -> buf_size = buf_size;
        b -> file_merger = file_merger;
        b -> cleanup_task = cleanup_task;
        b -> gap = gap;
        b -> total = 0;
        b -> total_rowcount_prod = 0;
        
        rc = KQueueMake ( &( b -> job_q ), batch_size );
        if ( rc == 0 )
        {
            rc = helper_make_thread( &( b -> thread ), background_vector_merger_thread_func,
                                     b, THREAD_DFLT_STACK_SIZE );
            if ( 0 != rc )
                ErrMsg( "merge_sorter.c helper_make_thread( vector-merger ) -> %R", rc );
        }

        if ( rc == 0 )
            *merger = b;
        else
            release_background_vector_merger( b );
    }
    return rc;
}

void tell_total_rowcount_to_vector_merger( background_vector_merger * self, uint64_t value )
{
    if ( self != NULL )
    {
        self -> total_rowcount_prod = value;
        tell_total_rowcount_to_file_merger( self -> file_merger, value );
    }
}

rc_t seal_background_vector_merger( background_vector_merger * self )
{
    rc_t rc = KQueueSeal ( self -> job_q );
    if ( rc != 0 )
        ErrMsg( "merge_sorter.c seal_background_vector_merger() -> %R", rc );
    return rc;
}

rc_t push_to_background_vector_merger( background_vector_merger * self, KVector * store )
{
    rc_t rc;
    bool running = true;
    while ( running )
    {
        struct timeout_t tm;
        rc = TimeoutInit ( &tm, self -> q_wait_time );
        if ( rc != 0 )
        {
            ErrMsg( "merge_sorter.c push_to_background_vector_merger().TimeoutInit( %u ) -> %R", self -> q_wait_time, rc );
            running = false;
        }
        else
        {
            rc = KQueuePush ( self -> job_q, store, &tm );
            if ( rc == 0 )
                running = false;
            else
            {
                bool timed_out = ( GetRCState( rc ) == rcExhausted && GetRCObject( rc ) == ( enum RCObject )rcTimeout );
                if ( timed_out )
                    KSleepMs( self -> q_wait_time );   
                else
                {
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
typedef struct background_file_merger
{
    KDirectory * dir;               /* needed to perform the merge-sort */
    const struct temp_dir * temp_dir;      /* needed to create temp. files */
    const char * lookup_filename;
    const char * index_filename;
    locked_file_list files;         /* a locked file-list */
    locked_value sealed;            /* flag to signal if the input is sealed */
    struct KFastDumpCleanupTask * cleanup_task;     /* add the produced temp_files here too */    
    KThread * thread;               /* the thread that performs the merge-sort */
    uint32_t product_id;            /* increased by one for each batch-run, used in temp-file-name */
    uint32_t batch_size;            /* how many KVectors have to arrive to run a batch */
    uint32_t wait_time;             /* time in milliseconds to sleep if waiting for files to process */
    size_t buf_size;                /* needed to perform the merge-sort */
    struct bg_update * gap;         /* visualize the gap after the producer finished */
    uint64_t total_rows;            /* how many rows have we processed */
    uint64_t total_rowcount_prod;   /* updated by the producer, informs the file-merger about the
                                       rowcount to be processed */
} background_file_merger;
   

static void release_background_file_merger( background_file_merger * self )
{
    if ( self != NULL )
    {
        locked_file_list_release( &( self -> files ), self -> dir );
        locked_value_release( &( self -> sealed ) );
        free( self );
    }
}

static rc_t wait_for_background_file_merger( background_file_merger * self )
{
    rc_t rc_status;
    rc_t rc = KThreadWait ( self -> thread, &rc_status );
    if ( rc == 0 )
        rc = rc_status;
    return rc;
}

rc_t wait_for_and_release_background_file_merger( background_file_merger * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcReleasing, rcSelf, rcNull );
    else
    {
        /* while we are waiting on the background-file-merger to finish,
           show a progress-indicator... */
        rc = wait_for_background_file_merger( self );
        if ( rc == 0 && ( self -> total_rows != self -> total_rowcount_prod ) )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSize, rcInvalid );
            ErrMsg( "merge_sorter.c wait_for_and_release_background_file_merger() %lu of %lu",
                     self -> total_rows, self -> total_rowcount_prod );
        }
        release_background_file_merger( self );
    }
    return rc;
}

/* called from the background-thread */
static rc_t process_background_file_merger( background_file_merger * self )
{
    char tmp_filename[ 4096 ];
    
    rc_t rc = generate_bg_merge_filename( self -> temp_dir, tmp_filename, sizeof tmp_filename,
                                          self -> product_id );
    
    if ( rc == 0 )
        rc = Add_File_to_Cleanup_Task ( self -> cleanup_task, tmp_filename );

    if ( rc == 0 )
    {
        uint32_t num_src = 0;
        VNamelist * batch_files;
        rc = VNamelistMake ( &batch_files, self -> batch_size );
        if ( rc == 0 )
        {
            uint32_t i;
            rc_t rc1 = 0;
            for ( i = 0; rc == 0 && rc1 == 0 && i < self -> batch_size; ++i )
            {
                const String * filename = NULL;
                rc1 = locked_file_list_pop( &( self -> files ), &filename );
                if ( rc1 == 0 && filename != NULL )
                {
                    rc = VNamelistAppendString ( batch_files, filename );
                    if ( rc == 0 )
                        num_src++;
                }
            }
            
            if ( rc == 0 )
            {
                merge_sorter sorter;
                rc = init_merge_sorter( &sorter,
                                        self -> dir,
                                        tmp_filename,   /* the output file */
                                        NULL,           /* opt. index_filename */
                                        batch_files,    /* the input files */    
                                        self -> buf_size,
                                        num_src,
                                        self -> gap );
                if ( rc == 0 )
                {
                    rc = run_merge_sorter( &sorter );
                    release_merge_sorter( &sorter );
                }
            }
            
            if ( rc == 0 )
                rc = delete_files( self -> dir, batch_files );

            VNamelistRelease( batch_files );
        }
    }
    
    if ( rc == 0 )
    {
        rc = locked_file_list_append( &( self -> files ), tmp_filename );
        if ( rc == 0 )
            self -> product_id += 1;
    }
    return rc;
}

static rc_t process_final_background_file_merger( background_file_merger * self, uint32_t count )
{
    VNamelist * batch_files;
    rc_t rc = VNamelistMake ( &batch_files, count );
    if ( rc == 0 )
    {
        uint32_t i;
        uint32_t num_src = 0;
        rc_t rc1 = 0;
        for ( i = 0; rc == 0 && rc1 == 0 && i < count; ++i )
        {
            const String * filename = NULL;
            rc1 = locked_file_list_pop( &( self -> files ), &filename );
            if ( rc1 == 0 && filename != NULL )
            {
                rc = VNamelistAppendString ( batch_files, filename );
                if ( rc == 0 )
                    num_src++;
            }
        }
        
        if ( rc == 0 )
            rc = Add_File_to_Cleanup_Task ( self -> cleanup_task, self -> lookup_filename );
        if ( rc == 0 )
            rc = Add_File_to_Cleanup_Task ( self -> cleanup_task, self -> index_filename );

        if ( rc == 0 )
        {
            merge_sorter sorter;
            rc = init_merge_sorter( &sorter,
                                    self -> dir,
                                    self -> lookup_filename,   /* the output file */
                                    self -> index_filename,    /* opt. index_filename */
                                    batch_files,               /* the input files */    
                                    self -> buf_size,
                                    num_src,
                                    self -> gap );
            if ( rc == 0 )
            {
                rc = run_merge_sorter( &sorter );
                if ( rc == 0 )
                    self -> total_rows += sorter . total_entries;                    
                release_merge_sorter( &sorter );
            }
        }
        
        if ( rc == 0 )
            rc = delete_files( self -> dir, batch_files );

        VNamelistRelease( batch_files );
    }
    return rc;
}

static rc_t CC background_file_merger_thread_func( const KThread * thread, void *data )
{
    rc_t rc = 0;
    background_file_merger * self = data;
    bool done = false;
    while( rc == 0 && !done )
    {
        uint64_t sealed;
        rc = locked_value_get( &( self -> sealed ), &sealed );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = locked_file_list_count( &( self -> files ), &count );
            if ( rc == 0 )
            {
                if ( sealed > 0 )
                {
                    /* we are sealed... */
                    if ( count == 0 )
                    {
                        /* this should not happen, but for the sake of completeness */
                        done = true;
                    }
                    else if ( count > ( self -> batch_size ) )
                    {
                        /* we still have more than we can open, do one batch */
                        rc = process_background_file_merger( self );
                    }
                    else
                    {
                        /* we can do the final batch */
                        rc = process_final_background_file_merger( self, count );
                        done = true;
                    }
                }
                else
                {
                    /* we are not sealed... */
                    if ( count < ( self -> batch_size ) )
                    {
                        /* let us take a little nap, until we get enough files, or get sealed */
                        KSleepMs( self -> wait_time );
                    }
                    else
                    {
                        /* we have enough files to process one batch */
                        rc = process_background_file_merger( self );
                    }
                }
            }
        }
    }
    return rc;
}

rc_t make_background_file_merger( background_file_merger ** merger,
                                KDirectory * dir,
                                const struct temp_dir * temp_dir,
                                struct KFastDumpCleanupTask * cleanup_task,
                                const char * lookup_filename,
                                const char * index_filename,
                                uint32_t batch_size,
                                uint32_t wait_time,
                                size_t buf_size,
                                struct bg_update * gap )
{
    rc_t rc = 0;
    background_file_merger * b = calloc( 1, sizeof * b );
    *merger = NULL;
    if ( b == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        b -> dir = dir;
        b -> temp_dir = temp_dir;
        b -> lookup_filename = lookup_filename;
        b -> index_filename = index_filename;
        b -> batch_size = batch_size;
        b -> wait_time = wait_time;
        b -> buf_size = buf_size;
        b -> cleanup_task = cleanup_task;
        b -> gap = gap;
        b -> total_rows = 0;
        b -> total_rowcount_prod = 0;

        rc = locked_file_list_init( &( b -> files ), 25  );
        if ( rc == 0 )
            rc = locked_value_init( &( b -> sealed ), 0 );
            
        if ( rc == 0 )
        {
            rc = helper_make_thread( &( b -> thread ), background_file_merger_thread_func,
                                     b, THREAD_DFLT_STACK_SIZE );
            if ( 0 != rc )
                ErrMsg( "merge_sorter.c helper_make_thread( file-mergerr ) -> %R", rc );
        }
        
        if ( rc == 0 )
            *merger = b;
        else
            release_background_file_merger( b );
    }
    return rc;
}

void tell_total_rowcount_to_file_merger( background_file_merger * self, uint64_t value )
{
    if ( self != NULL )
        self -> total_rowcount_prod = value;
}

rc_t push_to_background_file_merger( background_file_merger * self, const char * filename )
{
    return locked_file_list_append( &( self -> files ), filename );
}

rc_t seal_background_file_merger( background_file_merger * self )
{
    return locked_value_set( &( self -> sealed ), 1 );
}
