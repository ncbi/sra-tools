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

#include "sorter.h"
#include "lookup_writer.h"
#include "lookup_reader.h"
#include "raw_read_iter.h"
#include "helper.h"

#include <klib/vector.h>
#include <klib/printf.h>
#include <klib/progressbar.h>
#include <klib/out.h>
#include <kproc/thread.h>
#include <kproc/lock.h>

rc_t CC Quitting(); /* to avoid including kapp/main.h */

/* 
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */
#include <atomic.h>

typedef struct locked_file_list
{
    KLock * lock;
    VNamelist * files;
} locked_file_list;

static rc_t init_locked_file_list( locked_file_list * self, VNamelist * files )
{
    rc_t rc = KLockMake ( &( self -> lock ) );
    if ( rc == 0 )
        self -> files = files;
    return rc;
}

static rc_t release_locked_file_list( locked_file_list * self )
{
    return KLockRelease ( self -> lock );
}

static rc_t append_to_file_list( locked_file_list * self, const char * filename )
{
    return VNamelistAppend ( self -> files, filename );
}

static rc_t append_to_locked_file_list( locked_file_list * self, const char * filename )
{
    rc_t rc = KLockAcquire ( self -> lock );
    if ( rc == 0 )
    {
        rc = VNamelistAppend ( self -> files, filename );
        KLockUnlock ( self -> lock );
    }
    return rc;
}

typedef struct lookup_producer
{
    KDirectory * dir;
    const tmp_id * tmp_id;
    struct raw_read_iter * iter;
    KVector * store;
    atomic_t * sort_progress;   
    locked_file_list * locked_files;    
    SBuffer buf;
    uint64_t bytes_in_store;
    uint32_t chunk_id, sub_file_id;
    size_t buf_size, mem_limit;
    bool single;
} lookup_producer;


static void release_producer( lookup_producer * self )
{
    if ( self != NULL )
    {
        release_SBuffer( &( self -> buf ) ); /* helper.c */
        if ( self -> iter != NULL )
            destroy_raw_read_iter( self -> iter ); /* raw_read_iter.c */
        if ( self -> store != NULL )
            KVectorRelease( self -> store );
    }
}

static rc_t init_single_producer( lookup_producer * self,
                                  const lookup_production_params * lp,
                                  locked_file_list * locked_files )
{
    rc_t rc = KVectorMake( &self -> store );
    if ( rc != 0 )
        ErrMsg( "KVectorMake() -> %R", rc );
    else
    {
        rc = make_SBuffer( &( self -> buf ), 4096 ); /* helper.c */
        if ( rc == 0 )
        {
            self -> dir             = lp -> dir;
            self -> tmp_id          = lp -> tmp_id;
            self -> iter            = NULL;
            self -> sort_progress   = NULL;
            self -> locked_files    = locked_files;
            self -> bytes_in_store  = 0;
            self -> chunk_id        = 0;
            self -> sub_file_id     = 0;
            self -> buf_size        = lp -> buf_size;
            self -> mem_limit       = lp -> mem_limit;
            self -> single          = true;
            rc = make_raw_read_iter( lp -> cmn, &( self -> iter ) );
        }
    }
    return rc;
}

static rc_t init_multi_producer( lookup_producer * self,
                                 const lookup_production_params * lp,
                                 locked_file_list * locked_files,
                                 atomic_t * sort_progress,
                                 uint32_t chunk_id,
                                 int64_t first_row,
                                 uint64_t row_count )
{
    rc_t rc = KVectorMake( &self -> store );
    if ( rc != 0 )
        ErrMsg( "KVectorMake() -> %R", rc );
    else
    {
        rc = make_SBuffer( &( self -> buf ), 4096 ); /* helper.c */
        if ( rc == 0 )
        {
            cmn_params cmn;
            
            self -> dir             = lp -> dir;
            self -> tmp_id          = lp -> tmp_id;
            self -> iter            = NULL;
            self -> sort_progress   = sort_progress;
            self -> locked_files    = locked_files;
            self -> bytes_in_store  = 0;
            self -> chunk_id        = chunk_id;
            self -> sub_file_id     = 0;
            self -> buf_size        = lp -> buf_size;
            self -> mem_limit       = lp -> mem_limit;
            self -> single          = false;
            
            cmn . dir              = lp -> dir;
            cmn . acc              = lp -> accession;
            cmn . first_row        = first_row;
            cmn . row_count        = row_count;
            cmn . cursor_cache     = lp -> cmn -> cursor_cache;
            cmn . show_progress    = false;     /* we do that instead with the progress-thread! */

            rc = make_raw_read_iter( &cmn, &( self -> iter ) );
        }
    }
    return rc;
}


static rc_t CC on_store_entry( uint64_t key, const void *value, void *user_data )
{
    const String * bases = value;
    struct lookup_writer * writer = user_data;
    rc_t rc = write_packed_to_lookup_writer( writer, key, bases ); /* lookup_writer.c */
    StringWhack( bases );
    return rc;
}


static rc_t save_store( lookup_producer * self )
{
    rc_t rc = 0;
    if ( self -> bytes_in_store > 0 )
    {
        char buffer[ 4096 ];
        size_t num_writ;
        struct lookup_writer * writer; /* lookup_writer.h */
        const tmp_id * tmp_id = self -> tmp_id;
        
        if ( tmp_id -> temp_path_ends_in_slash )
            rc = string_printf( buffer, sizeof buffer, &num_writ, "%ssub_%s_%u_%u_%u.dat",
                    tmp_id -> temp_path, tmp_id -> hostname,
                    tmp_id -> pid, self -> chunk_id, self -> sub_file_id );
        else
            rc = string_printf( buffer, sizeof buffer, &num_writ, "%s/sub_%s_%u_%u_%u.dat",
                    tmp_id -> temp_path, tmp_id -> hostname,
                    tmp_id -> pid, self -> chunk_id, self -> sub_file_id );
        if ( rc != 0 )
            ErrMsg( "make_subfilename.string_printf() -> %R", rc );
        
        if ( rc == 0 )
            self -> sub_file_id++;
            
        if ( rc == 0 )
            rc = make_lookup_writer( self -> dir, NULL, &writer,
                                     self -> buf_size, "%s", buffer ); /* lookup_writer.c */
        
        if ( rc == 0 )
        {
            rc = KVectorVisitPtr( self -> store, false, on_store_entry, writer );
            release_lookup_writer( writer ); /* lookup_writer.c */
        }

        if ( rc == 0 )
        {
            if ( self -> single )
                rc = append_to_file_list( self -> locked_files, buffer );
            else
                rc = append_to_locked_file_list( self -> locked_files, buffer );
        }
        
        if ( rc == 0 )
        {
            self -> bytes_in_store = 0;
            rc = KVectorRelease( self -> store );
            if ( rc != 0 )
                ErrMsg( "KVectorRelease() -> %R", rc );
            else
            {
                self -> store = NULL;
                rc = KVectorMake( &self -> store );
                if ( rc != 0 )
                    ErrMsg( "KVectorMake() -> %R", rc );
            }
        }
    }
    return rc;
}


static rc_t write_to_store( lookup_producer * self, int64_t seq_spot_id, uint32_t seq_read_id,
                             const String * unpacked_bases )
{
    /* we write it to the store...*/
    rc_t rc;
    const String * to_store;    
    pack_4na( unpacked_bases, &( self -> buf ) ); /* helper.c */
    rc = StringCopy( &to_store, &( self -> buf . S ) );
    if ( rc != 0 )
        ErrMsg( "StringCopy() -> %R", rc );
    else
    {
        uint64_t key = make_key( seq_spot_id, seq_read_id ); /* helper.c */
        rc = KVectorSetPtr( self -> store, key, ( const void * )to_store );
        if ( rc != 0 )
            ErrMsg( "KVectorSetPtr() -> %R", rc );
        else
        {
            size_t item_size = ( sizeof key ) + ( sizeof *to_store ) + to_store -> size;
            self -> bytes_in_store += item_size;
        }
    }
    
    if ( rc == 0 &&
         self -> mem_limit > 0 &&
         self -> bytes_in_store >= self -> mem_limit )
        rc = save_store( self ); /* above! */
    return rc;
}

static rc_t run_producer( lookup_producer * self )
{
    rc_t rc = 0;
    raw_read_rec rec;
    while ( rc == 0 && get_from_raw_read_iter( self -> iter, &rec, &rc ) ) /* raw_read_iter.c */
    {
        rc = Quitting();
        if ( rc == 0 )
        {
            rc = write_to_store( self, rec . seq_spot_id, rec . seq_read_id, &rec . raw_read ); /* above! */
            if ( rc == 0 && self -> sort_progress != NULL )
                atomic_inc( self -> sort_progress ); /* atomic.h */
        }
    }
    
    if ( rc == 0 )
        rc = save_store( self ); /* above */

    return rc;
}

/* -------------------------------------------------------------------------------------------- */

static uint64_t find_out_row_count( const lookup_production_params * lp )
{
    rc_t rc;
    uint64_t res = 0;
    struct raw_read_iter * iter; /* raw_read_iter.c */
    cmn_params cp; /* cmn_iter.h */
    
    cp . dir            = lp -> dir;
    cp . acc            = lp -> accession;
    cp . first_row      = 0;
    cp . row_count      = 0;
    cp . cursor_cache   = lp -> cmn -> cursor_cache;
    cp . show_progress  = false;

    rc = make_raw_read_iter( &cp, &iter ); /* raw_read_iter.c */
    if ( rc == 0 )
    {
        res = get_row_count_of_raw_read( iter ); /* raw_read_iter.c */
        destroy_raw_read_iter( iter ); /* raw_read_iter.c */
    }
    return res;
}

static rc_t CC producer_thread_func( const KThread *self, void *data )
{
    rc_t rc;
    lookup_producer * producer = data;
    rc = run_producer( producer ); /* above */
    release_producer( producer ); /* above */
    free( data );
    return rc;
}

static rc_t run_producer_pool( const lookup_production_params * lp, locked_file_list * locked_files )
{
    rc_t rc = 0;
    uint64_t total_row_count = find_out_row_count( lp );
    if ( total_row_count == 0 )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "multi_threaded_make_lookup: row_count == 0!" );
    }
    else
    {
        Vector threads;
        KThread * progress_thread = NULL;
        uint32_t chunk_id = 1;
        int64_t first_row = 1;
        uint64_t row_count = ( total_row_count / lp -> num_threads ) + 1;
        multi_progress progress;
        atomic_t * sort_progress = NULL;
        
        VectorInit( &threads, 0, lp -> num_threads );
        if ( lp -> cmn -> show_progress )
        {
            init_progress_data( &progress, total_row_count );
            sort_progress = &( progress . progress_rows );
            rc = start_multi_progress( &progress_thread, &progress );
        }

        while ( rc == 0 && first_row < total_row_count )
        {
            lookup_producer * producer = calloc( 1, sizeof *producer );
            if ( producer != NULL )
            {
                rc = init_multi_producer( producer,
                                          lp,
                                          locked_files,
                                          sort_progress,
                                          chunk_id,
                                          first_row,
                                          row_count );
                if ( rc == 0 )
                {
                    KThread * thread;
                    rc = KThreadMake( &thread, producer_thread_func, producer );
                    if ( rc != 0 )
                        ErrMsg( "KThreadMake( sort-thread #%d ) -> %R", chunk_id - 1, rc );
                    else
                    {
                        rc = VectorAppend( &threads, NULL, thread );
                        if ( rc != 0 )
                            ErrMsg( "VectorAppend( sort-thread #%d ) -> %R", chunk_id - 1, rc );
                        else
                        {
                            first_row  += row_count;
                            chunk_id++;
                        }
                    }
                }
                else
                {
                    release_producer( producer ); /* above */
                    free( producer );
                }
            }
            else
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }

        /* collect all the sorter-threads */
        join_and_release_threads( &threads ); /* helper.c */
        
        /* all sorter-threads are done now, tell the progress-thread to terminate! */
        join_multi_progress( progress_thread, &progress );  /* helper.c */
    }
    return rc;
}


rc_t execute_lookup_production( lookup_production_params * lp )
{
    locked_file_list locked_files;
    
    rc_t rc = init_locked_file_list( &locked_files, lp -> files );
    if ( rc == 0 )
    {

        if ( lp -> cmn -> show_progress )
            rc = KOutMsg( "lookup :" );

        if ( rc == 0 )
        {
            if ( lp -> num_threads > 1 )
            {
                /* multi-threaded! */
                rc = run_producer_pool( lp, &locked_files ); /* above */
            }
            else
            {
                /* single-threaded! */
                lookup_producer producer;
                rc = init_single_producer( &producer, lp, &locked_files ); /* above */
                if ( rc == 0 )
                {
                    rc = run_producer( &producer ); /* above */
                    release_producer( &producer ); /* above */
                }
            }
        }
        release_locked_file_list( &locked_files );
    }
    return rc;
}
