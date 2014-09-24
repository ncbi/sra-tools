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

#include "num-gen.h"
#include <klib/printf.h>

#include <sysalloc.h>
#include <strtol.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct num_gen_node
{
    uint64_t start;
    uint64_t end;
    uint64_t count; /* 0 ... skip, >0 ... valid */
} num_gen_node;
typedef struct num_gen_node * p_num_gen_node;

/* **************************************************************************
{ start:5,count: 0 } ---> [ ]
{ start:5,count: 1 } ---> [ 5 ]
{ start:5,count: 2 } ---> [ 5, 6 ]
 ************************************************************************** */

struct num_gen
{
    Vector nodes;
};


struct num_gen_iter
{
    Vector nodes;
    uint32_t curr_node;
    uint32_t curr_node_sub_pos;
    uint64_t total;
    uint64_t progress;
};

/* forward decl. for fixing-function */
static rc_t num_gen_fix_overlaps( num_gen* self, uint32_t *count );


/* helper function to destroy a node*/
static void CC num_gen_node_destroy( void *item, void *data )
{
    free( item );
}


/* helper function to create a node from start/count */
static p_num_gen_node num_gen_make_node( const uint64_t start, const uint64_t count )
{
    p_num_gen_node p = ( p_num_gen_node )malloc( sizeof( num_gen_node ) );
    if ( p )
    {
        p->start = start;
        p->end = start + count - 1;
        p->count = count;
    }
    return p;
}


/* helper callback to compare 2 nodes, lets VectorInsert create a sorted vector */
static int CC num_gen_insert_helper( const void* item1, const void* item2 )
{
    const p_num_gen_node node1 = ( p_num_gen_node )item1;
    const p_num_gen_node node2 = ( p_num_gen_node )item2;
    int res = 0;
    if ( node1->start == node2->start )
    {
        if ( node1->count < node2->count )
            res = -1;
        else if ( node1->count > node2->count )
            res = 1;
    }
    else if ( node1->start < node2->start )
        res = -1;
    else
        res = 1;
    return res;
}


/* helper callback to create a deep and conditional copy of a node-vector */
static void CC num_gen_copy_cb( void *item, void *data )
{
    p_num_gen_node node = ( p_num_gen_node )item;
    if ( node->count > 0 )
    {
        Vector * dst = ( Vector *)data;
        p_num_gen_node new_node = num_gen_make_node( node->start, node->count );
        if ( new_node != NULL )
            VectorInsert( dst, new_node, NULL, num_gen_insert_helper );
    }
}


/* helper function that creates a deep and conditional copy of a node-vector */
static void num_gen_copy_vector( const Vector * src, Vector * dst )
{
    if ( src == NULL || dst == NULL )
        return;
    VectorForEach ( src, false, num_gen_copy_cb, dst );    
}


/* helper callback to add up all count values in the vector*/
static void CC num_gen_total_count_cb( void *item, void *data )
{
    p_num_gen_node node = ( p_num_gen_node )item;
    if ( node != NULL )
    {
        uint64_t * total = ( uint64_t *)data;
        if ( total != NULL )
            *total += node->count;
    }
}


/* helper function that adds up all count values in the vector*/
static uint64_t num_gen_total_count( const Vector * src )
{
    uint64_t res = 0;
    if ( src != NULL )
        VectorForEach ( src, false, num_gen_total_count_cb, &res );
    return res;
}


/* helper function for the parse-function */
static rc_t num_gen_add_node( num_gen* self, const uint64_t from,
                              const uint64_t to )
{
    p_num_gen_node node = NULL;
    int64_t count = ( to - from );
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcInserting, rcSelf, rcNull );

    if ( count >= 0 )
        node = num_gen_make_node( from, count + 1 );
    else
        node = num_gen_make_node( to, -( count + 1 ) );
    if ( node == NULL )
        return RC( rcVDB, rcNoTarg, rcInserting, rcMemory, rcExhausted );
    return VectorInsert( &(self->nodes), node, NULL, num_gen_insert_helper );
}


#define MAX_NUM_STR 12
/* helper-structure for num_gen_parse() */
typedef struct num_gen_parse_ctx
{
    uint32_t num_str_idx;
    bool this_is_the_first_number;
    uint64_t num1;
    uint64_t num2;
    char num_str[ MAX_NUM_STR + 1 ];
} num_gen_parse_ctx;
typedef num_gen_parse_ctx* p_num_gen_parse_ctx;


/* helper for num_gen_parse() */
static void num_gen_convert_ctx( p_num_gen_parse_ctx ctx )
{
    char *endp;
    
    ctx->num_str[ ctx->num_str_idx ] = 0;
    ctx->num1 = strtou64( ctx->num_str, &endp, 10 );
    ctx->this_is_the_first_number = false;
    ctx->num_str_idx = 0;
}


/* helper for num_gen_parse() */
static rc_t num_gen_convert_and_add_ctx( num_gen* self, p_num_gen_parse_ctx ctx )
{
    char *endp;
    
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcInserting, rcSelf, rcNull );
    if ( ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    if ( ctx->num_str_idx == 0 )
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcEmpty );

    /* terminate the source-string */
    ctx->num_str[ ctx->num_str_idx ] = 0;
    /* convert the string into a uint64_t */
    if ( ctx->this_is_the_first_number )
        {
        ctx->num1 = strtou64( ctx->num_str, &endp, 10 );
        ctx->num2 = ctx->num1;
        }
    else
        ctx->num2 = strtou64( ctx->num_str, &endp, 10 );
    /* empty the source-string to be reused */
    ctx->num_str_idx = 0;
    
    ctx->this_is_the_first_number = true;
    return num_gen_add_node( self, ctx->num1, ctx->num2 );
}


/* parse the given string and insert the found ranges 
   into the number-generator, fixes eventual overlaps */
rc_t num_gen_parse( num_gen* self, const char* src )
{
    size_t i, n;
    num_gen_parse_ctx ctx;
    rc_t rc = 0;

    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcParsing, rcSelf, rcNull );
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcParsing, rcParam, rcNull );

    n = string_measure ( src, NULL );
    if ( n == 0 )
        return RC( rcVDB, rcNoTarg, rcParsing, rcParam, rcEmpty );

    ctx.num_str_idx = 0;
    ctx.this_is_the_first_number = true;
    for ( i = 0; i < n && rc == 0; ++i )
    {
        switch ( src[ i ] )
        {
        /* a dash switches from N1-mode into N2-mode */
        case '-' :
            num_gen_convert_ctx( &ctx );
            break;

        /* a comma ends a single number or a range */
        case ',' :
            rc = num_gen_convert_and_add_ctx( self, &ctx );
            break;

        /* in both mode add the char to the temp string */
        default:
            if ( ( src[i]>='0' )&&( src[i]<='9' )&&( ctx.num_str_idx < MAX_NUM_STR ) )
                ctx.num_str[ ctx.num_str_idx++ ] = src[ i ];
            break;
        }
    }
    /* dont forget to add what is left in ctx.num_str ... */
    if ( ctx.num_str_idx > 0 )
        rc = num_gen_convert_and_add_ctx( self, &ctx );
    if ( rc == 0 )
        rc = num_gen_fix_overlaps( self, NULL );
    return rc;
}


/* inserts the given ranges into the number-generator,
   fixes eventual overlaps */
rc_t num_gen_add( num_gen* self, const uint64_t first, const uint64_t count )
{
    rc_t rc;
    uint64_t num_1 = first;
    uint64_t num_2 = first;

    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcInserting, rcSelf, rcNull );

    /* this is necessary because virtual columns which have a
       infinite row-range, get reported with first=1,count=0 */
    if ( count > 0 )
        num_2 = ( first + count - 1 );
    rc = num_gen_add_node( self, num_1, num_2 );
    if ( rc == 0 )
        rc = num_gen_fix_overlaps( self, NULL );
    return rc;
}


/* helper function for range-check */
static bool CC num_gen_check_range_start( p_num_gen_node the_node, 
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


/* helper function for range-check */
static void CC num_gen_check_range_end( p_num_gen_node the_node, 
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


/* helper function for range-check */
static void CC num_gen_check_range_callback( void *item, void *data )
{
    p_num_gen_node the_node = ( p_num_gen_node )item;
    p_num_gen_node the_range = ( p_num_gen_node )data;
    uint64_t last_tab_row = ( the_range->start + the_range->count - 1 );

    /* ignore invalid nodes... */
    if ( the_node->start == 0 || the_node->count == 0 )
        return;
        
    /* check if the start value is not out of range... */
    if ( num_gen_check_range_start( the_node, the_range->start ) )
        num_gen_check_range_end( the_node, last_tab_row );
}


/* helper function for range-check */
static void CC num_gen_count_invalid_nodes( void *item, void *data )
{
    p_num_gen_node the_node = ( p_num_gen_node )item;
    uint32_t *invalid_count = ( uint32_t * )data;
    
    if ( ( the_node->start == 0 )&&( the_node->count == 0 ) )
        ( *invalid_count )++;
}


/* helper function for range-check */
static void CC num_gen_copy_valid_nodes( void *item, void *data )
{
    p_num_gen_node node = ( p_num_gen_node )item;
    Vector *dest = ( Vector * )data;
    
    if ( ( node->start != 0 )&&( node->count != 0 ) )
        VectorInsert ( dest, node, NULL, num_gen_insert_helper );
    else
        free ( node );
}


/* helper function for range-check */
static void num_gen_remove_invalid_nodes( num_gen* self )
{
    Vector temp_nodes;
    uint32_t count = VectorLength( &(self->nodes) );
    
    if ( count < 1 )
        return;
    /* create a temp. vector */
    VectorInit( &temp_nodes, 0, count );

    /* copy all valid nodes into the temp. vector */
    VectorForEach ( &(self->nodes), false,
                    num_gen_copy_valid_nodes, &temp_nodes );

    /* clear all nodes so far...,
       DO NOT PASS num_gen_node_destroy into it */
    VectorWhack( &(self->nodes), NULL, NULL );

    /* initialize and copy (shallow) the valid nodes back
       into the generator */
    VectorCopy ( &temp_nodes, &(self->nodes) );

    /* destroy the temp-vector,
       DO NOT PASS num_gen_node_destroy into it */
    VectorWhack ( &temp_nodes, NULL, NULL );
}


/* helper function for trim */
rc_t num_gen_trim( num_gen* self, const int64_t first, const uint64_t count )
{
    num_gen_node trim_range;
    uint32_t invalid_nodes = 0;

    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcValidating, rcSelf, rcNull );
    if ( count == 0 )
        return RC( rcVDB, rcNoTarg, rcValidating, rcParam, rcNull );

    /* walk all nodes to check for boundaries... */
    trim_range.start = first;
    trim_range.count = count;

    VectorForEach ( &(self->nodes), false,
                    num_gen_check_range_callback, &trim_range );

    VectorForEach ( &(self->nodes), false,
                    num_gen_count_invalid_nodes, &invalid_nodes );
    if ( invalid_nodes > 0 )
        num_gen_remove_invalid_nodes( self );
 
    return 0;
}


rc_t num_gen_make( num_gen** self )
{
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );

    *self = calloc( 1, sizeof( num_gen ) );
    if ( *self == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );

    VectorInit( &((*self)->nodes ), 0, 5 );
    return 0;
}


rc_t num_gen_make_from_str( num_gen** self, const char *src )
{
    rc_t rc = num_gen_make( self );
    if ( rc == 0 )
    {
        rc = num_gen_parse( *self, src );
        if ( rc == 0 )
            rc = num_gen_fix_overlaps( *self, NULL );
     }
     return rc;
}


rc_t num_gen_make_from_range( num_gen** self, 
                              const int64_t first, const uint64_t count )
{
    rc_t rc = num_gen_make( self );
    if ( rc != 0 )
        return rc;
    return num_gen_add( *self, first, count );
}


rc_t num_gen_clear( num_gen* self )
{
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcClearing, rcSelf, rcNull );

    if ( VectorLength( &(self->nodes) ) > 0 )
    {
        /* clear all nodes so far... */
        VectorWhack( &(self->nodes), num_gen_node_destroy, NULL );

        /* re-init the vector */
        VectorInit( &(self->nodes ), 0, 5 );
    }
    return 0;
}


rc_t num_gen_destroy( num_gen* self )
{
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );

    VectorWhack( &(self->nodes), num_gen_node_destroy, NULL );
    free( self );
    return 0;
}


bool num_gen_empty( const num_gen* self )
{
    if ( self == NULL )
        return true;
    return ( VectorLength( &(self->nodes) ) < 1 );
}


typedef struct overlap_ctx
{
    p_num_gen_node prev;
    uint32_t overlaps;
} overlap_ctx;
typedef overlap_ctx* p_overlap_ctx;


/* static bool CC num_gen_overlap_fix_cb( void *item, void *data ) */
static bool CC num_gen_overlap_fix_cb( void *item, void *data )
{
    p_num_gen_node node = ( p_num_gen_node )item;
    p_overlap_ctx ctx = ( p_overlap_ctx )data;

    /* skip invalid nodes */
    if ( node->count ==0 || node->start == 0 || node->end == 0 )
        return false;
    /* if we do not have a previous node, take this one... */
    if ( ctx->prev == NULL )
        {
        ctx->prev = node;
        return false;
        }
    /* if we do not have an overlap,
       take this node as prev-node and continue */
    if ( ctx->prev->end < node->start )
        {
        ctx->prev = node;
        return false;
        }
    /* we have a overlap, the end of the prev-node is inside
       the current-node, we fix it by expanding the prev-node
       to the end of this node, and later declaring this
       node as invalid */
    if ( ctx->prev->end < node->end )
    {
        ctx->prev->end = node->end;
        ctx->prev->count = ( ctx->prev->end - ctx->prev->start ) + 1;
    }
    /* if the prev-node ends after this node, all we have to
       do is declaring this node as invalid */
    node->count = 0;
    node->start = 0;
    node->end = 0;
    return true;
}


static rc_t num_gen_fix_overlaps( num_gen* self, uint32_t *count )
{
    overlap_ctx ctx;
    bool fix_executed = false;
    
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    
    ctx.overlaps = 0;
    do
    {
        ctx.prev = NULL;
        fix_executed = VectorDoUntil ( &(self->nodes), false, 
                                       num_gen_overlap_fix_cb, &ctx );
    } while ( fix_executed );

    if ( count )
        *count = ctx.overlaps;
    return 0;
}


typedef struct string_ctx
{
    char *s;
    uint32_t len;
} string_ctx;
typedef string_ctx* p_string_ctx;


static void string_ctx_add( p_string_ctx ctx, char *s )
{
    uint32_t len = string_measure ( s, NULL );
    if ( len > 0 )
    {
        if ( ctx->len == 0 )
            ctx->s = malloc( len + 1 );
        else
            ctx->s = realloc( ctx->s, ctx->len + len );
        memcpy( &(ctx->s[ctx->len]), s, len );
        ctx->len += len;
    }
}


static void CC num_gen_as_string_cb( void *item, void *data )
{
    char temp[40];
    p_num_gen_node node = ( p_num_gen_node )item;
    long unsigned int start = node->start;
    long unsigned int end = ( start + node->count - 1 );
    switch( node->count )
    {
    case 0 : temp[ 0 ] = 0;
             break;
    case 1 : string_printf ( temp, sizeof temp, NULL, "%lu,", start );
             break;
    default: string_printf ( temp, sizeof temp, NULL, "%lu-%lu,", start, end );
             break;
    }
    string_ctx_add( ( p_string_ctx )data, temp );
}


rc_t num_gen_as_string( const num_gen* self, char **s )
{
    string_ctx ctx;
    
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    if ( s == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcParam, rcNull );

    ctx.s = NULL;
    ctx.len = 0;
    VectorForEach ( &(self->nodes), false, num_gen_as_string_cb, &ctx );
    if ( ctx.len == 0 )
    {
        *s = NULL;
        return RC( rcVDB, rcNoTarg, rcReading, rcData, rcEmpty );
    }
    ctx.s[ ctx.len ] = 0;
    *s = ctx.s;
    return 0;
}


static void CC num_gen_debug_cb( void *item, void *data )
{
    char temp[40];
    p_num_gen_node node = ( p_num_gen_node )item;
    long unsigned int start = node->start;
    long unsigned int count = node->count;
    string_printf ( temp, sizeof temp, NULL, "[s:%lu c:%lu]", start, count );
    string_ctx_add( ( p_string_ctx )data, temp );
}


rc_t num_gen_debug( const num_gen* self, char **s )
{
    string_ctx ctx;
    
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    if ( s == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcParam, rcNull );

    ctx.s = NULL;
    ctx.len = 0;
    VectorForEach ( &(self->nodes), false, num_gen_debug_cb, &ctx );
    if ( ctx.len == 0 )
    {
        *s = NULL;
        return RC( rcVDB, rcNoTarg, rcReading, rcData, rcEmpty );
    }
    ctx.s[ ctx.len ] = 0;
    *s = ctx.s;
    return 0;
}


static bool CC num_gen_contains_cb( void *item, void *data )
{
    bool res = false;
    p_num_gen_node node = ( p_num_gen_node )item;
    if ( node->count > 0 )
    {
        uint64_t *value = ( uint64_t * )data;
        uint64_t end = node->start + node->count - 1;
        res = ( node->start <= *value && *value <= end );
    }
    return res;
}


rc_t num_gen_contains_value( const num_gen* self, const uint64_t value )
{
    uint64_t temp = value;
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    if ( VectorDoUntil ( &(self->nodes), false, 
                         num_gen_contains_cb, &temp ) )
        return 0;
    else
        return RC( rcVDB, rcNoTarg, rcReading, rcData, rcEmpty );
}


rc_t num_gen_range_check( num_gen* self, 
                          const int64_t first, const uint64_t count )
{
    /* if the user did not specify a row-range, take all rows */
    if ( num_gen_empty( self ) )
        return num_gen_add( self, first, count );
    /* if the user did specify a row-range, check the boundaries */
    else
        return num_gen_trim( self, first, count );
}


rc_t num_gen_iterator_make( const num_gen* self, const num_gen_iter **iter )
{
    uint32_t count;
    
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    if ( iter == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcParam, rcNull );

    *iter = NULL;
    count = VectorLength( &(self->nodes) );
    if ( count < 1 )
        return RC( rcVDB, rcNoTarg, rcReading, rcParam, rcNull );
    else
    {
        num_gen_iter *temp = calloc( 1, sizeof( num_gen_iter ) );
        if ( temp == NULL )
            return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        VectorInit( &(temp->nodes), 0, count );
        num_gen_copy_vector( &(self->nodes), &(temp->nodes ) );
        temp->total = num_gen_total_count( &(temp->nodes ) );
        *iter = temp;
    }
    return 0;
}

rc_t num_gen_iterator_destroy( const num_gen_iter *self )
{
    num_gen_iter *temp;
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );

    temp = (num_gen_iter *)self;
    VectorWhack( &(temp->nodes), num_gen_node_destroy, NULL );
    free( temp );
    return 0;
}

rc_t num_gen_iterator_next( const num_gen_iter* self, uint64_t* value )
{
    num_gen_iter* temp;
    p_num_gen_node node;
    
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    if ( value == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcParam, rcNull );
    if ( self->curr_node >= VectorLength( &(self->nodes) ) )
        return RC( rcVDB, rcNoTarg, rcReading, rcId, rcInvalid );

    temp = ( num_gen_iter *)self;
    *value = 0;
    node = (p_num_gen_node)VectorGet( &(temp->nodes), temp->curr_node );
    if ( node == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcItem, rcInvalid );

    *value = node->start;
    if ( node->count < 2 )
        /* the node is a single-number-node, next node for next time */
        temp->curr_node++;
    else
    {
        /* the node is a number range, add the sub-position */
        *value += temp->curr_node_sub_pos++;
        /* if the sub-positions are use up, switch to next node */
        if ( temp->curr_node_sub_pos >= node->count )
        {
            temp->curr_node++;
            temp->curr_node_sub_pos = 0;
        }
    }
    (temp->progress)++;
    return 0;
}


rc_t num_gen_iterator_count( const num_gen_iter* self, uint64_t* count )
{
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    if ( count == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcParam, rcNull );
    *count = self->total;
    return 0;
}


rc_t num_gen_iterator_percent( const num_gen_iter* self, 
                               const uint8_t fract_digits,
                               uint32_t* value )
{
    uint32_t factor = 100;
    if ( self == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcSelf, rcNull );
    if ( value == NULL )
        return RC( rcVDB, rcNoTarg, rcReading, rcParam, rcNull );
    if ( fract_digits > 0 )
    {
        if ( fract_digits > 1 )
            factor = 10000;
        else
            factor = 1000;
    }
        
    if ( self->total > 0 )
    {
        if ( self->progress >= self->total )
            *value = factor;
        else
        {
            uint64_t temp = self->progress;
            temp *= factor;
            temp /= self->total;
            *value = (uint16_t) temp;
        }
    }
    else
        *value = 0;
    return 0;
}
