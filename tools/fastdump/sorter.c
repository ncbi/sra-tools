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
#include "helper.h"

#include <klib/vector.h>
#include <klib/out.h>
/*#include <klib/printf.h>*/

typedef struct sorter
{
    sorter_params * params;
    KVector * store;
    SBuffer buf;
    uint64_t in_store;
    uint32_t sub_file_id;
} sorter;


void release_sorter( struct sorter * sorter )
{
    if ( sorter != NULL )
    {
        release_SBuffer( &sorter->buf );
        if ( sorter->store != NULL ) KVectorRelease( sorter->store );
        free( ( void * ) sorter );
    }
}

rc_t make_sorter( sorter_params * params, struct sorter ** sorter )
{
    rc_t rc = 0;
    struct sorter * s = calloc( 1, sizeof * s );
    if ( s == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * s ), rc );
    }
    else
    {
        rc = KVectorMake( &s->store );
        if ( rc != 0 )
            ErrMsg( "KVectorMake() -> %R", rc );
        else
        {
            rc = make_SBuffer( &s->buf, 4096 );
            if ( rc == 0 )
            {
                s->params = params;
                *sorter = s;
            }
        }
    }
    if ( rc != 0 )
        release_sorter( s );
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
    if ( sorter->in_store > 0 )
    {
        struct lookup_writer * writer;
        if ( sorter->params->use_subfiles )
        {
            if ( sorter->params->temp_path != NULL )
                rc = make_lookup_writer( sorter->params->dir, &writer, sorter->params->buf_size,
                        "%ssub_%d.dat", sorter->params->temp_path, sorter->sub_file_id++ );
            else
                rc = make_lookup_writer( sorter->params->dir, &writer, sorter->params->buf_size,
                        "sub_%d.dat", sorter->sub_file_id++ );
        }
        else
        {
            rc = make_lookup_writer( sorter->params->dir, &writer, sorter->params->buf_size,
                    "%s", sorter->params->output_filename );
        }
        
        if ( rc == 0 )
        {
            rc = KVectorVisitPtr( sorter->store, false, on_store_entry, writer );
            release_lookup_writer( writer );
        }
        if ( rc == 0 )
        {
            sorter->in_store = 0;
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


rc_t write_to_sorter( struct sorter * sorter, int64_t seq_spot_id, uint32_t seq_read_id,
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
            sorter->in_store++;
    }
    
    if ( rc == 0 &&
         sorter->params->use_subfiles &&
         sorter->in_store >= sorter->params->store_limit )
        rc = save_store( sorter );
    return rc;
}


typedef struct merge_src
{
    struct lookup_reader * reader;
    uint64_t key;
    SBuffer packed_bases;
    rc_t rc;
} merge_src;


static void release_merge_src( merge_src * src )
{
    if ( src != NULL )
    {
        release_lookup_reader( src->reader );
        release_SBuffer( &src->packed_bases );
    }
} 


static rc_t init_merge_src( merge_src * src, sorter_params * params, uint32_t id )
{
    rc_t rc;
    if ( params->temp_path != NULL )
        rc = make_lookup_reader( params->dir, &src->reader, params->buf_size, "%ssub_%d.dat", params->temp_path, id );
    else
        rc = make_lookup_reader( params->dir, &src->reader, params->buf_size, "sub_%d.dat", id );
    if ( rc == 0 )
    {
        rc = make_SBuffer( &src->packed_bases, 4096 );
        if ( rc == 0 )
            src->rc = get_packed_and_key_from_lookup_reader( src->reader, &src->key, &src->packed_bases );
    }
    return rc;
}


static merge_src * get_min_merge_src( merge_src * src, uint32_t count )
{
    merge_src * res = NULL;
    uint32_t i;
    for ( i = 0; i < count; ++ i )
    {
        merge_src * item = &src[ i ];
        if ( item->rc == 0 )
        {
            if ( res == NULL )
                res = item;
            else if ( item->key < res->key )
                res = item;
        }
    }
    return res;
} 


static rc_t delete_sub_files( sorter_params * params, uint32_t count )
{
    rc_t rc = 0;
    uint32_t i;
    for ( i = 0; rc == 0 && i < count; ++ i )
    {
        if ( params->temp_path != NULL )
            rc = KDirectoryRemove( params->dir, true, "%ssub_%d.dat", params->temp_path, i );
        else
            rc = KDirectoryRemove( params->dir, true, "sub_%d.dat", i );
        if ( rc != 0 )
            ErrMsg( "KDirectoryRemove( 'sub_%d.dat' ) -> %R", i, rc );
    }
    return rc;
}


static rc_t final_merge_sort( sorter_params * params, uint32_t count )
{
    rc_t rc = 0;
    if ( count > 0 )
    {
        struct lookup_writer * writer;
        rc = make_lookup_writer( params->dir, &writer, params->buf_size, params->output_filename );
        if ( rc == 0 )
        {
            merge_src * src = calloc( count, sizeof * src );
            if ( src == NULL )
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "calloc( %d ) -> %R", ( ( sizeof * src ) * count ), rc );
            }
            else
            {
                uint32_t i;
                /* open all previously created sub-files */
                for ( i = 0; rc == 0 && i < count; ++ i )
                    rc = init_merge_src( &src[ i ], params, i );
                    
                if ( rc == 0 )
                {
                    merge_src * to_write = get_min_merge_src( src, count );
                    while( rc == 0 && to_write != NULL )
                    {
                        rc = write_packed_to_lookup_writer( writer, to_write->key, &to_write->packed_bases.S );
                        if ( rc == 0 )
                            to_write->rc = get_packed_and_key_from_lookup_reader( to_write->reader, &to_write->key, &to_write->packed_bases );
                        to_write = get_min_merge_src( src, count );
                    }
                }

                /* close all previously created sub-files */
                for ( i = 0; i < count; ++ i )
                    release_merge_src( &src[ i ] );
                    
                rc = delete_sub_files( params, count );
            }
            release_lookup_writer( writer );
        }
    }
    return rc;
}


rc_t finalize_sorter( struct sorter * sorter )
{
    rc_t rc = save_store( sorter );
    if ( rc == 0 && sorter->params->use_subfiles )
        rc = final_merge_sort( sorter->params, sorter->sub_file_id );
    return rc;
}
