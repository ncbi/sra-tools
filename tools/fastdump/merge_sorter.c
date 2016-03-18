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

typedef struct merge_src
{
    struct lookup_reader * reader;
    uint64_t key;
    SBuffer packed_bases;
    rc_t rc;
} merge_src;


typedef struct merge_sorter
{
    const merge_sorter_params * params;
    merge_src * src_list;
    struct lookup_writer * dst;
    struct index_writer * idx;
} merge_sorter;


void release_merge_sorter( struct merge_sorter *ms )
{
    if ( ms != NULL )
    {
        uint32_t i;
        release_lookup_writer( ms->dst );
        release_index_writer( ms->idx );
        if ( ms->src_list != NULL )
        {
            for ( i = 0; i < ms->params->count; ++i )
            {
                merge_src * src = &ms->src_list[ i ];
                release_lookup_reader( src->reader );
                release_SBuffer( &src->packed_bases );
            }
            free( ( void * ) ms->src_list );
        }

        free( ( void * ) ms );
    }
}


rc_t make_merge_sorter( struct merge_sorter ** ms, const merge_sorter_params * params )
{
    rc_t rc = 0;
    merge_sorter * m = calloc( 1, sizeof * m );
    if ( m == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * m ), rc );
    }
    else
    {
        if ( params->index_filename != NULL )
            rc = make_index_writer( params->dir, &m->idx, params->buf_size,
                        20000, "%s", params->index_filename );

        if ( rc == 0 )
        {
            rc = make_lookup_writer( params->dir, m->idx, &m->dst, params->buf_size,
                        "%s", params->output_filename );
            if ( rc == 0 )
            {
                m->src_list = calloc( params->count, sizeof * m->src_list );
                if ( m->src_list == NULL )
                {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                    ErrMsg( "calloc( %d ) -> %R", ( ( sizeof * m->src_list ) * params->count ), rc );
                }
                else
                {
                    m->params = params;
                    *ms = m;
                }
            }
        }
    }
    if ( rc != 0 )
        release_merge_sorter( m );
    return rc;
}


rc_t add_merge_sorter_src( struct merge_sorter *ms, const char * filename, uint32_t id )
{
    rc_t rc;
    if ( id >= ms->params->count )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "add_merge_sorter_src() ... invalid id of %d", id );
    }
    else
    {
        merge_src * src = &ms->src_list[ id ];
        rc = make_lookup_reader( ms->params->dir, NULL, &src->reader, ms->params->buf_size, "%s", filename );
        if ( rc == 0 )
        {
            rc = make_SBuffer( &src->packed_bases, 4096 );
            if ( rc == 0 )
                src->rc = get_packed_and_key_from_lookup_reader( src->reader, &src->key, &src->packed_bases );
        }
    }
    return rc;
}


static merge_src * get_min_merge_src( merge_src * src, uint32_t count )
{
    merge_src * res = NULL;
    uint32_t i;
    for ( i = 0; i < count; ++i )
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


rc_t run_merge_sorter( struct merge_sorter *ms )
{
    rc_t rc = 0;
    merge_src * to_write = get_min_merge_src( ms->src_list, ms->params->count );
    while( rc == 0 && to_write != NULL )
    {
        rc = write_packed_to_lookup_writer( ms->dst, to_write->key, &to_write->packed_bases.S );
        if ( rc == 0 )
            to_write->rc = get_packed_and_key_from_lookup_reader( to_write->reader, &to_write->key, &to_write->packed_bases );
        to_write = get_min_merge_src( ms->src_list, ms->params->count );
    }
    return rc;
}
