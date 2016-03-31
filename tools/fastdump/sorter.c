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
#include "merge_sorter.h"
#include "helper.h"

#include <klib/vector.h>
#include <klib/printf.h>
#include <klib/progressbar.h>
#include <kproc/thread.h>

/* 
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */
#include <atomic.h>


typedef struct sorter
{
    sorter_params params;
    KVector * store;
    SBuffer buf;
    uint64_t bytes_in_store;
    uint32_t sub_file_id;
} sorter;


static void release_sorter( struct sorter * sorter )
{
    if ( sorter != NULL )
    {
        release_SBuffer( &sorter->buf );
        if ( sorter->params.src != NULL )
            destroy_raw_read_iter( sorter->params.src );
        if ( sorter->store != NULL )
            KVectorRelease( sorter->store );
    }
}

static rc_t init_sorter( struct sorter * sorter, const sorter_params * params )
{
    rc_t rc = KVectorMake( &sorter->store );
    if ( rc != 0 )
        ErrMsg( "KVectorMake() -> %R", rc );
    else
    {
        rc = make_SBuffer( &sorter->buf, 4096 );
        if ( rc == 0 )
        {
            sorter->params.dir = params->dir;
            sorter->params.output_filename = params->output_filename;
            sorter->params.index_filename = NULL;
            sorter->params.temp_path = params->temp_path;
            sorter->params.src = params->src;
            sorter->params.buf_size = params->buf_size;
            sorter->params.mem_limit = params->mem_limit;
            sorter->params.prefix = params->prefix;
            sorter->bytes_in_store = 0;
            sorter->sub_file_id = 0;
        }
    }
    return rc;
}


static rc_t make_subfilename( const sorter_params * params, uint32_t id, char * buffer, size_t buflen )
{
    rc_t rc;
    size_t num_writ;
    if ( params->temp_path != NULL )
    {
        uint32_t l = string_measure( params->temp_path, NULL );
        if ( l == 0 )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            ErrMsg( "make_subfilename.string_measure() = 0 -> %R", rc );
        }
        else
        {
            if ( params->temp_path[ l-1 ] == '/' )
                rc = string_printf( buffer, buflen, &num_writ, "%ssub_%d_%d.dat",
                        params->temp_path, params->prefix, id );
            else
                rc = string_printf( buffer, buflen, &num_writ, "%s/sub_%d_%d.dat",
                        params->temp_path, params->prefix, id );
        }
    }
    else
        rc = string_printf( buffer, buflen, &num_writ, "sub_%d_%d.dat",
                params->prefix, id );

    if ( rc != 0 )
        ErrMsg( "make_subfilename.string_printf() -> %R", rc );
    return rc;
}


static rc_t make_dst_filename( const sorter_params * params, char * buffer, size_t buflen )
{
    rc_t rc;
    size_t num_writ;
    if ( params->prefix > 0 )
    {
        if ( params->temp_path != NULL )
        {
            uint32_t l = string_measure( params->temp_path, NULL );
            if ( l == 0 )
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                ErrMsg( "make_subfilename.string_measure() = 0 -> %R", rc );
            }
            else
            {
                if ( params->temp_path[ l-1 ] == '/' )
                    rc = string_printf( buffer, buflen, &num_writ, "%stmp_%d.dat",
                            params->temp_path, params->prefix );
                else
                    rc = string_printf( buffer, buflen, &num_writ, "%s/tmp_%d.dat",
                            params->temp_path, params->prefix );
            }
        }
        else
            rc = string_printf( buffer, buflen, &num_writ, "tmp_%d.dat", params->prefix );
    }
    else
        rc = string_printf( buffer, buflen, &num_writ, "%s", params->output_filename );

    if ( rc != 0 )
        ErrMsg( "make_dst_filename.string_printf() -> %R", rc );
    return rc;
}


static rc_t CC on_store_entry( uint64_t key, const void *value, void *user_data )
{
    const String * bases = value;
    struct lookup_writer * writer = user_data;
    rc_t rc = write_packed_to_lookup_writer( writer, key, bases );
    StringWhack( bases );
    return rc;
}


static rc_t save_store( struct sorter * sorter )
{
    rc_t rc = 0;
    if ( sorter->bytes_in_store > 0 )
    {
        char buffer[ 4096 ];
        struct lookup_writer * writer;
        
        if ( sorter->params.mem_limit > 0 )
        {
            rc = make_subfilename( &sorter->params, sorter->sub_file_id, buffer, sizeof buffer );
            if ( rc == 0 )
                sorter->sub_file_id++;
        }
        else
            rc = make_dst_filename( &sorter->params, buffer, sizeof buffer );

        if ( rc == 0 )
            rc = make_lookup_writer( sorter->params.dir, NULL, &writer, sorter->params.buf_size, "%s", buffer );
        
        if ( rc == 0 )
        {
            rc = KVectorVisitPtr( sorter->store, false, on_store_entry, writer );
            release_lookup_writer( writer );
        }
        if ( rc == 0 )
        {
            sorter->bytes_in_store = 0;
            rc = KVectorRelease( sorter->store );
            if ( rc != 0 )
                ErrMsg( "KVectorRelease() -> %R", rc );
            else
            {
                sorter->store = NULL;
                rc = KVectorMake( &sorter->store );
                if ( rc != 0 )
                    ErrMsg( "KVectorMake() -> %R", rc );
            }
        }
    }
    return rc;
}


static rc_t write_to_sorter( struct sorter * sorter, int64_t seq_spot_id, uint32_t seq_read_id,
        const String * unpacked_bases )
{
    /* we write it to the store...*/
    rc_t rc;
    const String * to_store;    
    pack_4na( unpacked_bases, &sorter->buf );
    rc = StringCopy( &to_store, &sorter->buf.S );
    if ( rc != 0 )
        ErrMsg( "StringCopy() -> %R", rc );
    else
    {
        uint64_t key = make_key( seq_spot_id, seq_read_id );
        rc = KVectorSetPtr( sorter->store, key, (const void *)to_store );
        if ( rc != 0 )
            ErrMsg( "KVectorSetPtr() -> %R", rc );
        else
        {
            size_t item_size = ( sizeof key ) + ( sizeof *to_store ) + to_store->size;
            sorter->bytes_in_store += item_size;
        }
    }
    
    if ( rc == 0 &&
         sorter->params.mem_limit > 0 &&
         sorter->bytes_in_store >= sorter->params.mem_limit )
        rc = save_store( sorter );
    return rc;
}


static rc_t delete_sub_files( const sorter_params * params, uint32_t count )
{
    rc_t rc = 0;
    char buffer[ 4096 ];
    uint32_t i;
    for ( i = 0; rc == 0 && i < count; ++ i )
    {
        rc = make_subfilename( params, i, buffer, sizeof buffer );
        if ( rc == 0 )
            rc = KDirectoryRemove( params->dir, true, "%s", buffer );
        if ( rc != 0 )
            ErrMsg( "KDirectoryRemove( 'sub_%d.dat' ) -> %R", i, rc );
    }
    return rc;
}


static rc_t final_merge_sort( const sorter_params * params, uint32_t count )
{
    rc_t rc = 0;
    if ( count > 0 )
    {
        char buffer[ 4096 ];
        rc = make_dst_filename( params, buffer, sizeof buffer );
        if ( rc == 0 )
        {
            merge_sorter_params msp;
            struct merge_sorter * ms;
            uint32_t i;
            
            msp.dir = params->dir;
            msp.output_filename = buffer;
            msp.index_filename = params->index_filename;
            msp.count = count;
            msp.buf_size = params->buf_size;
            
            rc = make_merge_sorter( &ms, &msp );
            for ( i = 0; rc == 0 && i < count; ++i )
            {
                char buffer2[ 4096 ];
                rc = make_subfilename( params, i, buffer2, sizeof buffer2 );
                if ( rc == 0 )
                    rc = add_merge_sorter_src( ms, buffer2, i );
            }
            if ( rc == 0 )
                rc = run_merge_sorter( ms );
                
            release_merge_sorter( ms );
        }

        if ( rc == 0 )
            rc = delete_sub_files( params, count );
    }
    return rc;
}

rc_t CC Quitting();

rc_t run_sorter( const sorter_params * params )
{
    sorter sorter;
    rc_t rc = init_sorter( &sorter, params );
    if ( rc == 0 )
    {
        raw_read_rec rec;
        while ( rc == 0 && get_from_raw_read_iter( sorter.params.src, &rec, &rc ) )
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                rc = write_to_sorter( &sorter, rec.seq_spot_id, rec.seq_read_id, &rec.raw_read );
                if ( rc == 0 && params->sort_progress != NULL )
                    atomic_inc( params->sort_progress );
            }
        }
        
        if ( rc == 0 )
            rc = save_store( &sorter );

        if ( rc == 0 && sorter.params.mem_limit > 0 )
            rc = final_merge_sort( params, sorter.sub_file_id );
            
        release_sorter( &sorter );
    }
    return rc;
}

/* -------------------------------------------------------------------------------------------- */

static uint64_t find_out_row_count( const sorter_params * params )
{
    rc_t rc;
    uint64_t res = 0;
    struct raw_read_iter * iter;
    cmn_params cp;
    
    cp.dir = params->dir;
    cp.acc = params->acc;
    cp.row_range = NULL;
    cp.first = 0;
    cp.count = 0;
    cp.cursor_cache = params->cursor_cache;
    cp.show_progress = false;

    rc = make_raw_read_iter( &cp, &iter );
    if ( rc == 0 )
    {
        res = get_row_count_of_raw_read( iter );
        destroy_raw_read_iter( iter );
    }
    return res;
}


static rc_t make_pool_src_filename( const sorter_params * params, uint32_t id,
            char * buffer, size_t buflen )
{
    rc_t rc;
    size_t num_writ;
    if ( params->temp_path != NULL )
    {
        uint32_t l = string_measure( params->temp_path, NULL );
        if ( l == 0 )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            ErrMsg( "make_subfilename.string_measure() = 0 -> %R", rc );
        }
        else
        {
            if ( params->temp_path[ l - 1 ] == '/' )
                rc = string_printf( buffer, buflen, &num_writ, "%stmp_%d.dat",
                        params->temp_path, id );
            else
                rc = string_printf( buffer, buflen, &num_writ, "%s/tmp_%d.dat",
                        params->temp_path, id );
        }
    }
    else
        rc = string_printf( buffer, buflen, &num_writ, "tmp_%d.dat", id );

    if ( rc != 0 )
        ErrMsg( "make_pool_src_filename.string_printf() -> %R", rc );
    return rc;
}


static rc_t delete_tmp_files( const sorter_params * params, uint32_t count )
{
    rc_t rc = 0;
    char buffer[ 4096 ];
    uint32_t i;
    for ( i = 0; rc == 0 && i < count; ++ i )
    {
        make_pool_src_filename( params, i + 1, buffer, sizeof buffer );
        if ( rc == 0 )
            rc = KDirectoryRemove( params->dir, true, "%s", buffer );
        if ( rc != 0 )
            ErrMsg( "KDirectoryRemove( 'tmp_%d.dat' ) -> %R", rc );
    }
    return rc;
}


static rc_t merge_pool_files( const sorter_params * params )
{
    rc_t rc;
    merge_sorter_params msp;
    struct merge_sorter * ms;
    
    msp.dir = params->dir;
    msp.output_filename = params->output_filename;
    msp.index_filename = params->index_filename;
    msp.count = params->num_threads;
    msp.buf_size = params->buf_size;

    rc = make_merge_sorter( &ms, &msp );
    if ( rc == 0 )
    {
        uint32_t i;
        for ( i = 0; rc == 0 && i < params->num_threads; ++i )
        {
            char buffer[ 4096 ];
            rc = make_pool_src_filename( params, i + 1, buffer, sizeof buffer );
            if ( rc == 0 )
                rc = add_merge_sorter_src( ms, buffer, i );
        }
        if ( rc == 0 )
            rc = run_merge_sorter( ms );
        
        release_merge_sorter( ms );
    }

    if ( rc == 0 ) 
       rc = delete_tmp_files( params, params->num_threads );

    return rc;
}

static void init_sorter_params( sorter_params * dst, const sorter_params * params, uint32_t prefix )
{
    dst->dir = params->dir;
    dst->output_filename = NULL;
    dst->index_filename = params->index_filename;
    dst->temp_path = params->temp_path;
    dst->src = NULL;
    dst->prefix = prefix;
    dst->mem_limit = params->mem_limit;
    dst->buf_size = params->buf_size;
}

static void init_cmn_params( cmn_params * dst, const sorter_params * params, uint64_t row_count )
{
    dst->dir = params->dir;
    dst->acc = params->acc;
    dst->row_range = NULL;
    dst->first = 1;
    dst->count = ( row_count / params->num_threads ) + 1;
    dst->cursor_cache = params->cursor_cache;
    dst->show_progress = false;
}


static rc_t CC sort_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    sorter_params * params = data;
    params->index_filename = NULL;
    rc = run_sorter( params );
    free( data );
    return rc;
}


rc_t run_sorter_pool( const sorter_params * params )
{
    rc_t rc = 0;
    uint64_t row_count = find_out_row_count( params );
    if ( row_count == 0 )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "multi_threaded_make_lookup: row_count == 0!" );
    }
    else
    {
        cmn_params cp;
        Vector threads;
        KThread * progress_thread = NULL;
        uint32_t prefix = 1;
        multi_progress progress;

        init_progress_data( &progress, row_count );
        VectorInit( &threads, 0, params->num_threads );
        init_cmn_params( &cp, params, row_count );
        
        if ( params->show_progress )
            rc = start_multi_progress( &progress_thread, &progress );
            
        while ( rc == 0 && cp.first < row_count )
        {
            sorter_params * sp = calloc( 1, sizeof *sp );
            if ( sp != NULL )
            {
                init_sorter_params( sp, params, prefix++ );
                rc = make_raw_read_iter( &cp, &sp->src );
                
                if ( rc == 0 )
                {
                    KThread * thread;
                    
                    if ( params->show_progress )
                        sp->sort_progress = &progress.progress_rows;
                    rc = KThreadMake( &thread, sort_thread_func, sp );
                    if ( rc != 0 )
                        ErrMsg( "KThreadMake( sort-thread #%d ) -> %R", prefix - 1, rc );
                    else
                    {
                        rc = VectorAppend( &threads, NULL, thread );
                        if ( rc != 0 )
                            ErrMsg( "VectorAppend( sort-thread #%d ) -> %R", prefix - 1, rc );
                    }
                }
                cp.first  += cp.count;
            }
        }

        join_and_release_threads( &threads );
        /* all sorter-threads are done now, tell the progress-thread to terminate! */
        join_multi_progress( progress_thread, &progress );
        rc = merge_pool_files( params );
    }
    return rc;
}
