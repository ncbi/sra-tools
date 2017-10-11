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
                res = item;
            else if ( item -> key < res -> key )
                res = item;
        }
    }
    return res;
} 

/* ================================================================================= */

typedef struct merge_sorter
{
    
    struct lookup_writer * dst; /* lookup_writer.h */
    struct index_writer * idx;  /* index.h */
    struct progressbar * progressbar;
    merge_src * src;
    uint64_t total_size;
    uint32_t num_src;
} merge_sorter;


static rc_t init_merge_sorter( merge_sorter * self,
                               KDirectory * dir,
                               const char * output,
                               const char * index,
                               VNamelist * files,
                               size_t buf_size,
                               bool show_progress,
                               uint32_t num_src )
{
    rc_t rc = 0;
    uint32_t i;
    
    if ( index != NULL )
        rc = make_index_writer( dir, &( self -> idx ), buf_size,
                        DFLT_INDEX_FREQUENCY, "%s", index ); /* index.h */
    else
        self -> idx = NULL;

    self -> progressbar = NULL;
    self -> total_size = 0;
    self -> num_src = num_src;
    
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

            if ( show_progress )
            {
                uint64_t size = 0;
                rc = KDirectoryFileSize ( dir, &size, "%s", filename );
                if ( rc == 0 && size > 0 )
                    self -> total_size += size;
            }

            if ( rc == 0 )
                rc = make_lookup_reader( dir, NULL, &s -> reader, buf_size, "%s", filename ); /* lookup_reader.h */
            if ( rc == 0 )
            {
                rc = make_SBuffer( &s -> packed_bases, 4096 );
                if ( rc == 0 )
                    s -> rc = get_packed_and_key_from_lookup_reader( s -> reader,
                                    &s -> key, &s -> packed_bases ); /* lookup_reader.h */
            }
        }
    }
    
    if ( rc == 0 && show_progress )
        make_progressbar( &( self -> progressbar ), 2 );

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
    if ( self -> progressbar != NULL )
        destroy_progressbar( self -> progressbar );
}

static rc_t run_merge_sorter( merge_sorter * self )
{
    rc_t rc = 0;
    uint64_t written = 0;
    uint32_t cycle = 0;
    
    merge_src * to_write = get_min_merge_src( self -> src, self -> num_src );

    while( rc == 0 && to_write != NULL )
    {
        rc = Quitting();
        if ( rc == 0 )
        {
            rc = write_packed_to_lookup_writer( self -> dst,
                                                to_write -> key,
                                                &to_write -> packed_bases . S ); /* lookup_writer.h */
                                                
            if ( self -> progressbar != NULL )
            {
                written += ( to_write -> packed_bases . S . len + 8 );
                if ( cycle++ > 1000 )
                {
                    update_progressbar( self -> progressbar, calc_percent( self -> total_size, written, 2 ) );
                    cycle = 0;
                }
            }
            
            if ( rc == 0 )
            {
                to_write -> rc = get_packed_and_key_from_lookup_reader( to_write -> reader,
                                                                         &to_write -> key,
                                                                         &to_write -> packed_bases ); /* lookup_reader.h */
            }
            to_write = get_min_merge_src( self -> src, self -> num_src );
        }
    }
    
    if ( self -> progressbar != NULL )
        update_progressbar( self -> progressbar, calc_percent( self -> total_size, written, 2 ) );

    return rc;
}

/* ================================================================================= */

static rc_t execute_single_threaded( const merge_sort_params * mp, locked_file_list * files, uint32_t num_src )
{
    merge_sorter sorter;
    rc_t rc = init_merge_sorter( &sorter,
                            mp -> dir,
                            mp -> lookup_filename,
                            mp -> index_filename,
                            files -> files,
                            mp -> buf_size,
                            mp -> show_progress,
                            num_src );
    if ( rc == 0 )
    {
        rc = run_merge_sorter( &sorter );
        release_merge_sorter( &sorter );
    }
    return rc;
}


/* ================================================================================= */


/*
    we produce a list of batches, each batch merges a number ( > 1 ) of input-files
    each batch has one temp. output-file, a batch does not produce an index-file
    and we need a mechanism to communicate the optional progress
*/

typedef struct merge_batch
{
    KDirectory * dir;
    VNamelist * files;
    char * temp_output;
    size_t buf_size;
    uint32_t num_src;
} merge_batch;

static void release_merge_batch( merge_batch * self )
{
    if ( self != NULL )
    {
        if ( self -> files != NULL )
        {
            delete_files( self -> dir, self -> files );
            VNamelistRelease( self -> files );
        }
        if ( self -> temp_output != NULL )
            free( self -> temp_output );
        free( self );
    }
}

static rc_t make_merge_batch( merge_batch ** batch,
                              KDirectory * dir,
                              locked_file_list * files,
                              const tmp_id * tmp_id,
                              uint32_t files_offset,
                              uint32_t files_end,
                              uint32_t num_src,
                              uint32_t id,
                              size_t buf_size )
{
    rc_t rc = 0;
    merge_batch * b = calloc( 1, sizeof * b );
    *batch = NULL;
    if ( b != NULL )
    {
        b -> dir = dir;
        b -> buf_size = buf_size;
        b -> num_src = 0;
        rc = VNamelistMake( & ( b -> files ), files_end - files_offset );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = files_offset;
                  rc == 0 && i < files_end && i < num_src;
                  ++i )
            {
                const char * filename;
                rc = VNameListGet ( files -> files, i, &filename );
                if ( rc == 0 )
                {
                    rc = VNamelistAppend ( b -> files, filename );
                    if ( rc == 0 )
                        b -> num_src++;
                }
            }
        }
        if ( rc == 0 )
        {
            size_t num_writ;
            char buffer[ 4096 ];
            
            if ( tmp_id -> temp_path_ends_in_slash )
                rc = string_printf( buffer, sizeof buffer, &num_writ, "%smerge_%s_%u_%u.dat",
                        tmp_id -> temp_path, tmp_id -> hostname, tmp_id -> pid, id );
            else
                rc = string_printf( buffer, sizeof buffer, &num_writ, "%s/merge_%s_%u_%u.dat",
                        tmp_id -> temp_path, tmp_id -> hostname, tmp_id -> pid, id );

            if ( rc == 0 )
                b -> temp_output = string_dup ( buffer, num_writ );
        }
        if ( rc == 0 )
            *batch = b;
        else
            release_merge_batch( b );
    }
    else
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    
    return rc;
}

static rc_t run_merge_batch( merge_batch * self )
{
    merge_sorter sorter;
    rc_t rc = init_merge_sorter( &sorter,
                            self -> dir,
                            self -> temp_output,
                            NULL, /* index_filename */
                            self -> files,
                            self -> buf_size,
                            false, /* show_progress */
                            self -> num_src );
    if ( rc == 0 )
    {
        rc = run_merge_sorter( &sorter );
        release_merge_sorter( &sorter );
    }
    return rc;
}


/* ================================================================================= */
static rc_t create_batchlist( Vector * batches,
                              VNamelist * templist,
                              KDirectory * dir,
                              locked_file_list * files,
                              const tmp_id * tmp_id,
                              uint32_t num_src,
                              uint32_t files_per_batch,
                              size_t buf_size )
{
    rc_t rc = 0;
    uint32_t i = 0;
    uint32_t id = 0;
    
    while ( rc == 0 && i < num_src )
    {
        merge_batch * batch;
        rc = make_merge_batch( &batch,
                               dir,
                               files,
                               tmp_id,
                               i,
                               i + files_per_batch,
                               num_src,
                               id++,
                               buf_size );
        if ( rc == 0 )
            rc = VectorAppend ( batches, NULL, batch );
        if ( rc == 0 )
            rc = VNamelistAppend ( templist, batch -> temp_output );
        if ( rc == 0 )
            i += files_per_batch;
    }
    return rc;
}                              

static void CC batch_relase_cb( void * item, void * data )
{
    release_merge_batch( item );
}


static rc_t execute_multi_threaded( const merge_sort_params * mp, locked_file_list * files, uint32_t num_src )
{
    uint32_t files_per_batch = ( num_src / mp -> num_threads ) + 1;
    VNamelist * templist;
    
    rc_t rc = VNamelistMake( &templist, mp -> num_threads );
    if ( rc == 0 )
    {
        Vector batches;
        uint32_t vlen;
        
        VectorInit ( &batches, 0, num_src );
        rc = create_batchlist( &batches,
                               templist,
                               mp -> dir,
                               files,
                               mp -> tmp_id,
                               num_src,
                               files_per_batch,
                               mp -> buf_size );
        if ( rc == 0 )
        {
            vlen = VectorLength( &batches );
            uint32_t i;
            for ( i = 0; rc == 0 && i < vlen; ++i )
            {
                merge_batch * batch = VectorGet ( &batches, i );
                rc = run_merge_batch( batch );
                            }
        }
        VectorForEach ( &batches, false, batch_relase_cb, NULL );

        if ( rc == 0 )
        {
            merge_sorter sorter;
            rc = init_merge_sorter( &sorter,
                                    mp -> dir,
                                    mp -> lookup_filename,
                                    mp -> index_filename,
                                    templist,
                                    mp -> buf_size,
                                    mp -> show_progress,
                                    vlen );
            if ( rc == 0 )
            {
                rc = run_merge_sorter( &sorter );
                release_merge_sorter( &sorter );
            }
        }
        delete_files( mp -> dir, templist );
        VNamelistRelease( templist );
    }
    
    return rc;
}


/* ================================================================================= */
rc_t execute_merge_sort( const merge_sort_params * mp, locked_file_list * files )
{
    rc_t rc = 0;
    uint32_t num_src;
    
    if ( mp == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "execute_merge_sort() mp == NULL" );
        return rc;
    }
    
    if ( mp -> dir == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "execute_merge_sort() mp -> dir == NULL" );
        return rc;
    }
    
    if ( mp -> lookup_filename == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "execute_merge_sort() mp -> lookup_filename == NULL" );
        return rc;
    }
    
    if ( mp -> index_filename == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "execute_merge_sort() mp -> index_filename == NULL" );
        return rc;
    }
    

    if ( files == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "execute_merge_sort() mp -> files == NULL" );
        return rc;
    }
    
    rc = VNameListCount ( files -> files, &num_src );
    if ( rc != 0 )
    {
        ErrMsg( "execute_merge_sort() VNameListCount -> %R", rc );
        return rc;
    }

    if ( num_src == 0 )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "execute_merge_sort() mp -> files . count == 0" );
        return rc;
    }

    if ( mp -> show_progress )
        rc = KOutMsg( "\nmerge  :" );
    
    if ( mp -> num_threads == 1 || ( mp -> num_threads > ( num_src * 2 ) ) )
        rc = execute_single_threaded( mp, files, num_src );
    else
        rc = execute_multi_threaded( mp, files, num_src );

    return rc;
}


/* ================================================================================= */

typedef struct background_merger
{
    KDirectory * dir;               /* needed to perform the merge-sort */
    const tmp_id * tmp_id;          /* needed to create temp. files */
    /*KQueue * in_q;                 the KVector objects arrive here from the lookup-producer */
    locked_vector jobs;             /* the producer's put the jobs in here... */
    KThread * thread;               /* the thread that performs the merge-sort */
    const locked_file_list * produced;    /* here we keep track of the produced temp-files */
    uint32_t product_id;            /* increased by one for each batch-run, used in temp-file-name */
    uint32_t batch_size;            /* how many KVectors have to arrive to run a batch */
    uint32_t q_wait_time;           /* timeout in milliseconds to get something out of in_q */
    size_t buf_size;                /* needed to perform the merge-sort */
} background_merger;


static void CC release_job( void * item, void * data )
{
    KVectorRelease( item );
}

void release_background_merger( background_merger * self )
{
    if ( self != NULL )
    {
        /*
        if ( self -> in_q != NULL )
            KQueueRelease ( self -> in_q );
        */
        locked_vector_release( &( self -> jobs ), release_job, NULL );
        free( self );
    }
}

typedef struct bg_merge_src
{
    KVector * store;
    uint64_t key;
    const String * bases;
    rc_t rc;
} bg_merge_src;


static rc_t init_bg_merge_src( bg_merge_src * src, KVector * store )
{
    src -> store = store;
    src -> rc = KVectorGetFirstPtr ( src -> store, &( src -> key ), ( void ** )&( src -> bases ) );
    return src -> rc;
}

static void release_bg_merge_src( bg_merge_src * src )
{
    if ( src -> store != NULL )
        KVectorRelease( src -> store );
}

static bg_merge_src * get_min_bg_merge_src( bg_merge_src * batch, uint32_t count )
{
    bg_merge_src * res = NULL;
    uint32_t i;
    for ( i = 0; i < count; ++i )
    {
        bg_merge_src * item = &batch[ i ];
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

static rc_t write_bg_merge_src( bg_merge_src * src, struct lookup_writer * writer )
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

static rc_t background_merger_collect_batch( background_merger * self,
                                             bg_merge_src ** batch,
                                             uint32_t * count )
{
    rc_t rc = 0;
    bg_merge_src * b = calloc( self -> batch_size, sizeof * b );
    *batch = NULL;
    *count = 0;
    
    if ( b == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        bool sealed = false;
        while ( rc == 0 && *count < self -> batch_size && !sealed )
        {
            KVector * store = NULL;
            rc = locked_vector_pop( &( self -> jobs ), ( void ** )&store, &sealed );
            if ( rc == 0 && !sealed )
            {
                if ( store == NULL )
                    KSleepMs( self -> q_wait_time );
                else
                {
                    STATUS ( STAT_USR, "locked_vector_pop() : %R, store = %p", rc, store );
                    rc = init_bg_merge_src( &( b[ *count ] ), store );
                    if ( rc == 0 )
                        *count += 1;
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

static bool batch_valid( bg_merge_src * batch, uint32_t count )
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

static rc_t background_merger_process_batch( background_merger * self,
                                             bg_merge_src * batch,
                                             uint32_t count )
{
    rc_t rc = 0;
    char buffer[ 4096 ];
    const tmp_id * tmp_id = self -> tmp_id;
    size_t num_writ;
    
    if ( tmp_id -> temp_path_ends_in_slash )
        rc = string_printf( buffer, sizeof buffer, &num_writ, "%sbg_sub_%s_%u_%u.dat",
                tmp_id -> temp_path, tmp_id -> hostname,
                tmp_id -> pid, self -> product_id );
    else
        rc = string_printf( buffer, sizeof buffer, &num_writ, "%s/bg_sub_%s_%u_%u.dat",
                tmp_id -> temp_path, tmp_id -> hostname,
                tmp_id -> pid, self -> product_id );
    if ( rc != 0 )
        ErrMsg( "background_merger_process_batch.string_printf() -> %R", rc );
    else
    {
        STATUS ( STAT_USR, "batch output filename is : %s", buffer );
        rc = append_to_locked_file_list( self -> produced, buffer );
        if ( rc == 0 )
        {
            struct lookup_writer * writer; /* lookup_writer.h */
            rc = make_lookup_writer( self -> dir, NULL, &writer,
                                     self -> buf_size, "%s", buffer ); /* lookup_writer.c */
            if ( rc == 0 )
                self -> product_id += 1;

            if ( rc == 0 )
            {
                uint32_t entries_written = 0;
                bg_merge_src * to_write = get_min_bg_merge_src( batch, count );
                while( rc == 0 && to_write != NULL )
                {
                    rc = Quitting();
                    if ( rc == 0 )
                    {
                        rc = write_bg_merge_src( to_write, writer );
                        if ( rc == 0 )
                        {
                            entries_written++;
                            to_write = get_min_bg_merge_src( batch, count );
                        }
                        else
                            to_write = NULL;
                    }
                }
                STATUS ( STAT_USR, "%u entires written", entries_written );
                release_lookup_writer( writer ); /* lookup_writer.c */
            }
        }
    }
    return rc;
}

static rc_t CC background_merger_thread_func( const KThread * thread, void *data )
{
    rc_t rc = 0;
    background_merger * self = data;
    bool done = false;

    STATUS ( STAT_USR, "starting background thread loop" );
    while( rc == 0 && !done )
    {
        bg_merge_src * batch = NULL;
        uint32_t count = 0;
        
        /* Step 1 : get n = batch_size KVector's out of the in_q */
        STATUS ( STAT_USR, "collecting batch" );
        rc = background_merger_collect_batch( self, &batch, &count );
        STATUS ( STAT_USR, "done collectin batch: rc = %R, count = %u", rc, count );
        if ( rc == 0 )
        {
            done = count == 0;
            if ( !done )
            {
                if ( batch_valid( batch, count ) )
                {
                    /* Step 2 : process the batch */
                    STATUS ( STAT_USR, "processing batch of %u vectors", count );
                    rc = background_merger_process_batch( self, batch, count );
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
                release_bg_merge_src( &( batch[ i ] ) );
            free( batch );
        }
    }
    STATUS ( STAT_USR, "exiting background thread loop" );
    return rc;
}

rc_t make_background_merger( struct background_merger ** merger,
                             KDirectory * dir,
                             const locked_file_list * produced,
                             const tmp_id * tmp_id,
                             uint32_t batch_size,
                             uint32_t q_wait_time,
                             size_t buf_size )
{
    rc_t rc = 0;
    background_merger * b = calloc( 1, sizeof * b );
    *merger = NULL;
    if ( b != NULL )
    {
        b -> dir = dir;
        b -> tmp_id = tmp_id;
        b -> batch_size = batch_size;
        b -> q_wait_time = q_wait_time;
        b -> buf_size = buf_size;
        b -> produced = produced;
        rc = locked_vector_init( &( b -> jobs ), batch_size );
        if ( rc == 0 )
            rc = KThreadMake( &( b -> thread ), background_merger_thread_func, b );
        if ( rc == 0 )
            *merger = b;
        else
            release_background_merger( b );
    }
    else
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    return rc;
}

rc_t wait_for_background_merger( struct background_merger * self )
{
    rc_t rc_status;
    rc_t rc = KThreadWait ( self -> thread, &rc_status );
    if ( rc == 0 )
        rc = rc_status;
    return rc;
}

rc_t push_to_background_merger( struct background_merger * self, KVector * store, bool seal )
{
    return locked_vector_push( &( self -> jobs ), store, seal );
}
