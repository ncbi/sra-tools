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

#include "vdb-dump-num-gen.h"

#include <klib/vector.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/out.h>
#include <klib/text.h>
#include <strtol.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void CC vdn_node_destroy( void *item, void *data )
{
    free( item );
}

static p_num_gen_node vdn_make_node( const uint64_t start, const uint64_t count )
{
    p_num_gen_node p = ( p_num_gen_node )malloc( sizeof( num_gen_node ) );
    if ( p )
    {
        p->start = start;
        p->count = count;
    }
    return p;
}

static int CC vdn_insert_helper( const void* item1, const void* item2 )
{
    int res = 0;
    p_num_gen_node node1 = (p_num_gen_node)item1;
    p_num_gen_node node2 = (p_num_gen_node)item2;
    if ( node1->start < node2->start )
        res = -1;
    else if ( node1->start > node2->start )
        res = 1;
    return res;
}

static bool vdn_add_node( num_gen* generator, const uint64_t num_1,
                          const uint64_t num_2 )
{
    rc_t res;
    p_num_gen_node node = NULL;
    if ( generator == NULL ) return false;
    if ( num_1 == num_2 )
        {
        node = vdn_make_node( num_1, 1 );
        }
    else if ( num_1 < num_2 )
        {
        node = vdn_make_node( num_1, ( num_2 - num_1 ) + 1 );
        }
    else
        {
        node = vdn_make_node( num_2, ( num_1 - num_2 ) + 1 );
        }
    if ( node == NULL ) return false;
    res = VectorInsert( &(generator->nodes), node, NULL, vdn_insert_helper );
    if ( res == 0 ) generator->node_count++;
    return ( res == 0 );
}

#define MAX_NUM_STR 12
/* helper-structure for vdn_parse() */
typedef struct num_gen_parse_ctx
{
    size_t num_str_idx, num_count;
    uint64_t num[ 2 ];
    char num_str[ MAX_NUM_STR + 1 ];
} num_gen_parse_ctx;
typedef num_gen_parse_ctx* p_num_gen_parse_ctx;

/* helper for vdn_parse() */
static void vdb_convert_ctx( p_num_gen_parse_ctx ctx )
{
    char *endp;
    
    ctx->num_str[ ctx->num_str_idx ] = 0;
    ctx->num[ 0 ] = strtou64( ctx->num_str, &endp, 10 );
    ctx->num_count = 1;
    ctx->num_str_idx = 0;
}

/* helper for vdn_parse() */
static uint32_t vdb_convert_and_add_ctx( num_gen* generator, p_num_gen_parse_ctx ctx )
{
    uint32_t res = 0;
    char *endp;
    
    if ( ctx->num_str_idx > 0 )
    {
        ctx->num_str[ ctx->num_str_idx ] = 0;
        ctx->num[ ctx->num_count ] = strtou64( ctx->num_str, &endp, 10 );
        ctx->num_str_idx = 0;
        if ( ctx->num_count == 0 )
        {
            ctx->num[1] = ctx->num[0];
        }
        if ( vdn_add_node( generator, ctx->num[0], ctx->num[1] ) )
        {
            res++;
        }
        ctx->num_count = 0;
    }
    return res;
}

uint32_t vdn_parse( num_gen* generator, const char* src )
{
    size_t i, n;
    num_gen_parse_ctx ctx;
    uint32_t res = 0;
    
    if ( generator == NULL ) return res;
    if ( src == NULL ) return res;
    n = string_size( src );
    generator->node_count = 0;
    generator->curr_node = 0;
    generator->curr_node_sub_pos = 0;
    if ( n == 0 ) return res;

    ctx.num_str_idx = 0;
    ctx.num_count = 0;
    
    for ( i=0; i<n; ++i )
    {
        switch ( src[i] )
        {
        /* a dash switches from N1-mode into N2-mode */
        case '-' :
            vdb_convert_ctx( &ctx );
            break;

        /* a comma ends a single number or a range */
        case ',' :
            res += vdb_convert_and_add_ctx( generator, &ctx );
            break;

        /* in both mode add the char to the temp string */
        default:
            if ( ( src[i]>='0' )&&( src[i]<='9' )&&( ctx.num_str_idx < MAX_NUM_STR ) )
            {
                ctx.num_str[ ctx.num_str_idx++ ] = src[ i ];
            }
            break;
        }
    }
    res += vdb_convert_and_add_ctx( generator, &ctx );
    return res;
}

bool vdn_set_range( num_gen* generator,
                    const int64_t first, const uint64_t count )
{
    bool res = ( generator != NULL );
    if ( res )
    {
        uint64_t num_1 = first;
        uint64_t num_2;

        /* this is necessary because virtual columns which have a
           infinite row-range, get reported with first=1,count=0 */
        if ( count > 0 )
        {
            num_2 = first + count - 1;
        }
        else
        {
            num_2 = first;
        }
        
        /* clear all nodes so far... */
        VectorWhack( &(generator->nodes), vdn_node_destroy, NULL );
        /* re-init the vector */
        VectorInit( &(generator->nodes ), 0, 5 );
        generator->node_count = 0;
        generator->curr_node = 0;
        generator->curr_node_sub_pos = 0;

        res = vdn_add_node( generator, num_1, num_2 );
    }
    return res;
}

static bool CC vdn_check_range_start( p_num_gen_node the_node, 
                               const uint64_t range_start )
{
    bool res = true;
    uint64_t last_node_row = ( the_node->start + the_node->count - 1 );
    
    if ( the_node->start < range_start )
    {
        the_node->start = range_start;
        if ( the_node->start <= last_node_row )
        {
            the_node->count = ( last_node_row - the_node->start ) + 1;
        }
        else
        {
            /* the node becomes invalid ... */
            the_node->start = 0;
            the_node->count = 0;
            res = false;
        }
    }
    return res;
}

static void CC vdn_check_range_end( p_num_gen_node the_node, 
                             const uint64_t last_tab_row )
{
    uint64_t last_node_row = ( the_node->start + the_node->count - 1 );

    if ( last_node_row > last_tab_row )
    {
        last_node_row = last_tab_row;
        if ( the_node->start <= last_node_row )
        {
            the_node->count = ( last_node_row - the_node->start ) + 1;
        }
        else
        {
            /* the node becomes invalid ... */
            the_node->start = 0;
            the_node->count = 0;
        }
    }
}

static void CC vdn_check_range_callback( void *item, void *data )
{
    p_num_gen_node the_node = ( p_num_gen_node )item;
    p_num_gen_node the_range = ( p_num_gen_node )data;
    uint64_t last_tab_row = ( the_range->start + the_range->count - 1 );

    /* check if the start value is not out of range... */
    if ( vdn_check_range_start( the_node, the_range->start ) )
        vdn_check_range_end( the_node, last_tab_row );
}

static void CC vdn_count_invalid_nodes( void *item, void *data )
{
    p_num_gen_node the_node = ( p_num_gen_node )item;
    uint32_t *invalid_count = ( uint32_t * )data;
    
    if ( ( the_node->start == 0 )&&( the_node->count == 0 ) )
        ( *invalid_count )++;
}

static void CC vdn_copy_valid_nodes( void *item, void *data )
{
    p_num_gen_node the_node = ( p_num_gen_node )item;
    Vector *p_temp_nodes = ( Vector * )data;
    
    if ( ( the_node->start != 0 )&&( the_node->count != 0 ) )
    {
        p_num_gen_node new_node = vdn_make_node( the_node->start, the_node->count );
        if ( new_node )
        {
            VectorInsert( p_temp_nodes, new_node, NULL, vdn_insert_helper );
        }
    }
}

static void vdn_remove_invalid_nodes( num_gen* generator, const uint32_t invalid_nodes )
{
    Vector temp_nodes;
            
    /* create a temp. vector */
    VectorInit( &temp_nodes, 0, 5 );

    /* copy all valid nodes into the temp. vector */
    VectorForEach ( &(generator->nodes), false,
                    vdn_copy_valid_nodes, &temp_nodes );

    /* clear all nodes so far... */
    VectorWhack( &(generator->nodes), vdn_node_destroy, NULL );

    /* re-init the vector */
    VectorInit( &(generator->nodes ), 0, 5 );

    /* copy (swallow) the valid nodes back into the generator */
    VectorCopy ( &temp_nodes, &(generator->nodes) );

    /* correct the node count */
    generator->node_count -= invalid_nodes;

    /* destroy the temp-vector, DO NOT PASS vdn_node_destroy into it */            
    VectorWhack ( &temp_nodes, NULL, NULL );
}


bool vdn_check_range( num_gen* generator,
                      const int64_t first, const uint64_t count )
{
    bool res = ( generator != NULL );
    if ( res )
    {
        num_gen_node defined_range;
        uint32_t invalid_nodes = 0;

        if ( count > 0 )
        {
            /* walk all nodes to check for boundaries... */
            defined_range.start = first;
            defined_range.count = count;

            VectorForEach ( &(generator->nodes), false,
                        vdn_check_range_callback, &defined_range );

            VectorForEach ( &(generator->nodes), false,
                        vdn_count_invalid_nodes, &invalid_nodes );

            if ( invalid_nodes > 0 )
            {
                vdn_remove_invalid_nodes( generator, invalid_nodes );
            }
        }
    }
    return res;
}


rc_t vdn_make( num_gen** generator )
{
    if ( generator == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    (*generator) = calloc( 1, sizeof(num_gen) );
    if ( *generator == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    VectorInit( &((*generator)->nodes ), 0, 5 );
    return 0;
}


rc_t vdn_destroy( num_gen* generator )
{
    if ( generator == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );
    }
    VectorWhack( &(generator->nodes), vdn_node_destroy, NULL );
    free( generator );
    return 0;
}


bool vdn_start( num_gen* generator )
{
    if ( generator == NULL ) return false;
    generator->curr_node = ( generator->node_count > 0 ? 0 : 1 );
    generator->curr_node_sub_pos = 0;
    return true;
}


static bool vdn_next_node( num_gen* generator, uint64_t* value )
{
    bool res = false;
    if ( generator->curr_node < generator->node_count )
    {
        p_num_gen_node node = (p_num_gen_node)VectorGet( &(generator->nodes), 
                                        (uint32_t)generator->curr_node );
        if ( node != NULL )
        {
            uint64_t ret_value = node->start;
            if ( node->count < 2 )
            {
                /* the node is a single-number-node */
                generator->curr_node++;
            }
            else
            {
                /* the node is a number range */
                ret_value += generator->curr_node_sub_pos++;
                if ( generator->curr_node_sub_pos >= node->count )
                {
                    generator->curr_node++;
                    generator->curr_node_sub_pos = 0;
                }
            }
            if ( value ) *value = ret_value;
            res = true;
        }
    }
    return res;
}


bool vdn_range_defined( num_gen* generator )
{
    bool res = false;
    if ( generator != NULL )
    {
        res = ( generator->node_count > 0 );
    }
    return res;
}


bool vdn_next( num_gen* generator, uint64_t* value )
{
    bool res = false;
    if ( generator != NULL )
    {
        uint64_t ret_value = 0;
        if ( vdn_range_defined( generator ) )
        {
            /* there are nodes (number-ranges) defined */
            res = vdn_next_node( generator, &ret_value );
        }
        else
        {
            /* endless mode, there are NO nodes (number-ranges) defined */
            ret_value = generator->curr_node++;
            res = true;
        }
        if ( value ) *value = ret_value;
    }
    return res;
}


static void CC vdn_detect_max( void *item, void *data )
{
    p_num_gen_node the_node = ( p_num_gen_node )item;
    uint64_t * max_value = ( uint64_t * )data;
    uint64_t node_max = ( the_node->start + the_node->count ) - 1;
    if ( node_max > * max_value )
    {
        * max_value = node_max;
    }
}


uint64_t vdn_max( num_gen* generator )
{
    uint64_t res = 0;
    if ( generator != NULL )
        VectorForEach ( &(generator->nodes), false, vdn_detect_max, &res );
    return res;
}