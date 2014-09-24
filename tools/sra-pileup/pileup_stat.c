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

#include <klib/out.h>
#include <klib/sort.h>

#include "ref_walker_0.h"
#include "4na_ascii.h"

static uint32_t percent( uint32_t v1, uint32_t v2 )
{
    uint32_t sum = v1 + v2;
    uint32_t res = 0;
    if ( sum > 0 )
        res = ( ( v1 * 100 ) / sum );
    return res;
}

typedef struct tlen_array
{
    uint32_t * values;
    uint32_t capacity;
    uint32_t members;
    uint32_t zeros;
} tlen_array;


static rc_t init_tlen_array( tlen_array * a, uint32_t init_capacity )
{
    rc_t rc = 0;
    a->values = malloc( sizeof ( a->values[ 0 ] ) * init_capacity );
    if ( a->values == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted );
    else
    {
        a->capacity = init_capacity;
        a->members = 0;
        a->zeros = 0;
    }
    return rc;
}


static void finish_tlen_array( tlen_array * a )
{
    if ( a->values != NULL )
    {
        free( a->values );
        a->values = NULL;
    }
}


static rc_t realloc_tlen_array( tlen_array * a, uint32_t new_depth )
{
    rc_t rc = 0;
    if ( new_depth > a->capacity )
    {
        void * p = realloc( a->values, ( sizeof ( a->values[ 0 ] ) ) * new_depth );
        if ( a->values == NULL )
            rc = RC ( rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted );
        else
        {
            a->values = p;
            a->capacity = new_depth;
        }
    }
    return rc;
}


static void remove_from_tlen_array( tlen_array * a, uint32_t count )
{
    if ( count > 0 )
    {
        if ( a->members < count )
            a->members = 0;
        else
        {
            a->members -= count;
            memmove( &(a->values[ 0 ]), &(a->values[ count ]), a->members * ( sizeof a->values[ 0 ] ) );
        }
    }
}


static bool add_tlen_to_array( tlen_array * a, uint32_t value )
{
    bool res = ( value != 0 );
    if ( !res )
        a->zeros++;
    else
        a->values[ a->members++ ] = value;
    return res;
}


#define INIT_WINDOW_SIZE 50
#define MAX_SEQLEN_COUNT 500000

typedef struct strand
{
    uint32_t alignment_count, window_size, window_max, seq_len_accu_count;
    uint64_t seq_len_accu;
    tlen_array tlen_w;          /* tlen accumulater for all alignmnts starting/ending in window ending at current position */
    tlen_array tlen_l;          /* array holding the length of all position-slices in the window */
    tlen_array zeros;
} strand;


typedef struct stat_counters
{
    strand pos;
    strand neg;
} stat_counters;


static rc_t prepare_strand( strand * strand, uint32_t initial_size )
{
    rc_t rc = init_tlen_array( &strand->tlen_w, initial_size );
    if ( rc == 0 )
        rc = init_tlen_array( &strand->tlen_l, initial_size );
    if ( rc == 0 )
        rc = init_tlen_array( &strand->zeros, initial_size );
    if ( rc == 0 )
    {
        strand->window_size = 0;
        strand->window_max = INIT_WINDOW_SIZE;
        strand->seq_len_accu_count = 0;
        strand->seq_len_accu = 0;
    }
    return rc;
}


static rc_t prepare_stat_counters( stat_counters * counters, uint32_t initial_size )
{
    rc_t rc = prepare_strand( &counters->pos, initial_size );
    if ( rc == 0 )
        rc = prepare_strand( &counters->neg, initial_size );
    return rc;
}


static void finish_strand( strand * strand )
{
    finish_tlen_array( &strand->tlen_w );
    finish_tlen_array( &strand->tlen_l );
    finish_tlen_array( &strand->zeros );
}


static void finish_stat_counters( stat_counters * counters )
{
    finish_strand( &counters->pos );
    finish_strand( &counters->neg );
}


static rc_t realloc_strand( strand * strand, uint32_t new_depth )
{
    rc_t rc = realloc_tlen_array( &strand->tlen_w, strand->tlen_w.members + new_depth );
    if ( rc == 0 )
        rc = realloc_tlen_array( &strand->tlen_l, strand->tlen_l.members + new_depth );
    if ( rc == 0 )
        rc = realloc_tlen_array( &strand->zeros, strand->zeros.members + new_depth );
    strand->alignment_count = 0;
    return rc;
}


static void on_new_ref_position_strand( strand * strand )
{
    if ( ( strand->seq_len_accu_count < MAX_SEQLEN_COUNT ) && ( strand->seq_len_accu_count > 0 ) )
    {
        uint64_t w = ( strand->seq_len_accu / strand->seq_len_accu_count );
        if ( w > strand->window_max )
            strand->window_max = w;
    }

    if ( strand->window_size >= strand->window_max )
    {
        uint32_t to_remove = strand->tlen_l.values[ 0 ];
        remove_from_tlen_array( &strand->tlen_w, to_remove );
        remove_from_tlen_array( &strand->tlen_l, 1 );

        to_remove = strand->zeros.values[ 0 ];
        strand->tlen_w.zeros -= to_remove;
        remove_from_tlen_array( &strand->zeros, 1 );
    }
    else
        strand->window_size++;
    strand->tlen_l.values[ strand->tlen_l.members++ ] = 0;
    strand->zeros.values[ strand->zeros.members++ ] = 0;
}


static uint32_t medium( tlen_array * a )
{
    if ( a->members == 0 )
        return 0;
    else
        return a->values[ a->members >> 1 ];
}


static uint32_t percentil( tlen_array * a, uint32_t p )
{
    if ( a->members == 0 )
        return 0;
    else
        return a->values[ ( a->members * p ) / 100 ];
}


static rc_t print_header_line( void )
{
    return KOutMsg( "\nREFNAME----\tREFPOS\tREFBASE\tDEPTH\tSTRAND%%\tTL+#0\tTL+10%%\tTL+MED\tTL+90%%\tTL-#0\tTL-10%%\tTL-MED\tTL-90%%\n\n" );
}


/* ........................................................................................... */


static rc_t CC walk_stat_enter_ref_window( walk_data * data )
{
    stat_counters * counters = data->data;
    counters->pos.tlen_w.members = 0;
    counters->pos.tlen_l.members = 0;
    counters->neg.tlen_w.members = 0;
    counters->neg.tlen_l.members = 0;
    return 0;
}


static rc_t CC walk_stat_enter_ref_pos( walk_data * data )
{
    rc_t rc;
    stat_counters * counters = data->data;

    on_new_ref_position_strand( &counters->pos );
    on_new_ref_position_strand( &counters->neg );

    rc = realloc_strand( &counters->pos, data->depth );
    if ( rc == 0 )
        rc = realloc_strand( &counters->neg, data->depth );

    return rc;
}


static rc_t CC walk_stat_exit_ref_pos( walk_data * data )
{
    char c = _4na_to_ascii( data->ref_base, false );
    stat_counters * counters = data->data;

    /* REF-NAME, REF-POS, REF-BASE, DEPTH */
    rc_t rc = KOutMsg( "%s\t%u\t%c\t%u\t", data->ref_name, data->ref_pos + 1, c, data->depth );

    /* STRAND-ness */
    if ( rc == 0 )
        rc = KOutMsg( "%u%%\t", percent( counters->pos.alignment_count, counters->neg.alignment_count ) );

    /* TLEN-Statistic for sliding window, only starting/ending placements */
    if ( rc == 0 )
    {
        tlen_array * a = &counters->pos.tlen_w;
        if ( a->members > 1 )
            ksort_uint32_t ( a->values, a->members );

        rc = KOutMsg( "%u\t%u\t%u\t%u\t", a->zeros, percentil( a, 10 ), medium( a ), percentil( a, 90 ) );
        if ( rc == 0 )
        {
            a = &counters->neg.tlen_w;
            if ( a->members > 1 )
                ksort_uint32_t ( a->values, a->members );
            rc = KOutMsg( "%u\t%u\t%u\t%u\t", a->zeros, percentil( a, 10 ), medium( a ), percentil( a, 90 ) );
        }
    }

/*
    KOutMsg( "( %u,%u )\t", counters->pos.window_max, counters->neg.window_max );
    KOutMsg( "< %u.%u, %u.%u ( %u.%u, %u.%u ) >",
            counters->pos.tlen_w.members, counters->pos.tlen_w.capacity, counters->neg.tlen_w.members, counters->neg.tlen_w.capacity,
            counters->pos.tlen_l.members, counters->pos.tlen_l.capacity, counters->neg.tlen_l.members, counters->neg.tlen_l.capacity );
*/

    if ( rc == 0 )
        rc = KOutMsg( "\n" );

    return rc;
}


static void walk_strand_placement( strand * strand, int32_t tlen, INSDC_coord_len seq_len )
{
    tlen_array * a;
    uint32_t value =  ( tlen < 0 ) ? -tlen : tlen;
    if ( add_tlen_to_array( &strand->tlen_w, value ) )
        a = &strand->tlen_l;
    else
        a = &strand->zeros;
    a->values[ a->members - 1 ]++;

    if ( strand->seq_len_accu_count < MAX_SEQLEN_COUNT )
    {
        strand->seq_len_accu += seq_len;
        strand->seq_len_accu_count++;
    }
}


static rc_t CC walk_stat_placement( walk_data * data )
{
    int32_t state = data->state;
    if ( ( state & align_iter_invalid ) != align_iter_invalid )
    {
        bool reverse = data->xrec->reverse;
        stat_counters * counters = data->data;
        strand * strand = ( reverse ) ? &counters->neg : &counters->pos;

        strand->alignment_count++;

        /* for TLEN-statistic on starting/ending placements at this pos */
        if ( ( ( state & align_iter_last ) == align_iter_last )&&( reverse ) )
            walk_strand_placement( strand, data->xrec->tlen, data->rec->len );
        else if ( ( ( state & align_iter_first ) == align_iter_first )&&( !reverse ) )
            walk_strand_placement( strand, data->xrec->tlen, data->rec->len );
    }
    return 0;
}


rc_t walk_stat( ReferenceIterator *ref_iter, pileup_options *options )
{
    walk_data data;
    walk_funcs funcs;
    stat_counters counters;

    rc_t rc = print_header_line();
    if ( rc == 0 )
        rc = prepare_stat_counters( &counters, 1024 );
    if ( rc == 0 )
    {
        data.ref_iter = ref_iter;
        data.options = options;
        data.data = &counters;

        funcs.on_enter_ref = NULL;
        funcs.on_exit_ref = NULL;

        funcs.on_enter_ref_window = walk_stat_enter_ref_window;
        funcs.on_exit_ref_window = NULL;

        funcs.on_enter_ref_pos = walk_stat_enter_ref_pos;
        funcs.on_exit_ref_pos = walk_stat_exit_ref_pos;

        funcs.on_enter_spotgroup = NULL;
        funcs.on_exit_spotgroup = NULL;

        funcs.on_placement = walk_stat_placement;

        rc = walk_0( &data, &funcs );

        finish_stat_counters( &counters );
    }
    return rc;
}