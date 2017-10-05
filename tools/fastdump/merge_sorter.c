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
#include <klib/printf.h>
#include <klib/progressbar.h>

rc_t CC Quitting();

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
                               const VNamelist * files,
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

static rc_t execute_single_threaded( const merge_sort_params * mp, uint32_t num_src )
{
    merge_sorter sorter;
    rc_t rc = init_merge_sorter( &sorter,
                            mp -> dir,
                            mp -> lookup_filename,
                            mp -> index_filename,
                            mp -> files,
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
        VNamelistRelease( self -> files );
        if ( self -> temp_output != NULL )
            free( self -> temp_output );
        free( self );
    }
}

static rc_t make_merge_batch( merge_batch ** batch,
                              KDirectory * dir,
                              const VNamelist * files,
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
                rc = VNameListGet ( files, i, &filename );
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
                              const VNamelist * files,
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

static rc_t execute_multi_threaded( const merge_sort_params * mp, uint32_t num_src )
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
                               mp -> files,
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
        VNamelistRelease( templist );
    }
    
    return rc;
    /* we loop through the given files and produce batches...*/
    return execute_single_threaded( mp, num_src );
}


/* ================================================================================= */


rc_t execute_merge_sort( const merge_sort_params * mp )
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
    

    if ( mp -> files == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "execute_merge_sort() mp -> files == NULL" );
        return rc;
    }
    
    rc = VNameListCount ( mp -> files, &num_src );
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
        rc = execute_single_threaded( mp, num_src );
    else
        rc = execute_multi_threaded( mp, num_src );

    return rc;
}
