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

typedef struct indel_fragment
{
    BSTNode node;
    const char * bases;
    uint32_t len;
    uint32_t count;
} indel_fragment;


static indel_fragment * make_indel_fragment( const char * bases, uint32_t len )
{
    indel_fragment * res = malloc( sizeof * res );
    if ( res != NULL )
    {
        res->bases = string_dup ( bases, len );
        if ( res->bases == NULL )
        {
            free( res );
            res = NULL;
        }
        else
        {
            res->len = len;
            res->count = 1;
        }
    }
    return res;
}


static void CC free_indel_fragment( BSTNode * n, void * data )
{
    indel_fragment * fragment = ( indel_fragment * ) n;
    if ( fragment != NULL )
    {
        free( ( void * ) fragment->bases );
        free( fragment );
    }
}


static void free_fragments( BSTree * fragments )
{    
    BSTreeWhack ( fragments, free_indel_fragment, NULL );
}


typedef struct find_fragment_ctx
{
    const char * bases;
    uint32_t len;
} find_fragment_ctx;


static int CC cmp_fragment_vs_find_ctx( const void *item, const BSTNode *n )
{
    const indel_fragment * fragment = ( const indel_fragment * )n;
    const find_fragment_ctx * fctx = ( const find_fragment_ctx * )item;
    return string_cmp ( fctx->bases, fctx->len, fragment->bases, fragment->len, -1 );
}


static int CC cmp_fragment_vs_fragment( const BSTNode *item, const BSTNode *n )
{
    const indel_fragment * f1 = ( const indel_fragment * )item;
    const indel_fragment * f2 = ( const indel_fragment * )n;
    return string_cmp ( f1->bases, f1->len, f2->bases, f2->len, -1 );
}


static void count_indel_fragment( BSTree * fragments, const INSDC_4na_bin *bases, uint32_t len )
{
    find_fragment_ctx fctx;

    fctx.bases = malloc( len );
    if ( fctx.bases != NULL )
    {
        indel_fragment * fragment;
        uint32_t i;

        fctx.len = len;
        for ( i = 0; i < len; ++i )
            ( ( char * )fctx.bases )[ i ] = _4na_to_ascii( bases[ i ], false );

        fragment = ( indel_fragment * ) BSTreeFind ( fragments, &fctx, cmp_fragment_vs_find_ctx );
        if ( fragment == NULL )
        {
            fragment = make_indel_fragment( fctx.bases, len );
            if ( fragment != NULL )
            {
                rc_t rc = BSTreeInsert ( fragments, ( BSTNode * )fragment, cmp_fragment_vs_fragment );
                if ( rc != 0 )
                    free_indel_fragment( ( BSTNode * )fragment, NULL );
            }
        }
        else
            fragment->count++;

        free( ( void * ) fctx.bases );
    }
}


typedef struct walk_fragment_ctx
{
    rc_t rc;
    uint32_t n;
} walk_fragment_ctx;


static void CC on_fragment( BSTNode *n, void *data )
{
    walk_fragment_ctx * wctx = data;
    const indel_fragment * fragment = ( const indel_fragment * )n;
    if ( wctx->rc == 0 )
    {
        if ( wctx->n == 0 )
            wctx->rc = KOutMsg( "%u-%.*s", fragment->count, fragment->len, fragment->bases );
        else
            wctx->rc = KOutMsg( "|%u-%.*s", fragment->count, fragment->len, fragment->bases );
        wctx->n++;
    }
}


static rc_t print_fragments( BSTree * fragments )
{
    walk_fragment_ctx wctx;
    wctx.rc = 0;
    wctx.n = 0;
    BSTreeForEach ( fragments, false, on_fragment, &wctx );
    return wctx.rc;
}

/* =========================================================================================== */

typedef struct pileup_counters
{
    uint32_t matches;
    uint32_t mismatches[ 4 ];
    uint32_t inserts;
    uint32_t deletes;
    uint32_t forward;
    uint32_t reverse;
    uint32_t starting;
    uint32_t ending;
    BSTree insert_fragments;
    BSTree delete_fragments;
} pileup_counters;


static void clear_counters( pileup_counters * counters )
{
    uint32_t i;

    counters->matches = 0;
    for ( i = 0; i < 4; ++i )
        counters->mismatches[ i ] = 0;
    counters->inserts = 0;
    counters->deletes = 0;
    counters->forward = 0;
    counters->reverse = 0;
    counters->starting = 0;
    counters->ending = 0;
    BSTreeInit( &(counters->insert_fragments) );
    BSTreeInit( &(counters->delete_fragments) );
}


static void walk_counter_state( ReferenceIterator *ref_iter, int32_t state, bool reverse,
                                pileup_counters * counters )
{
    if ( ( state & align_iter_invalid ) == align_iter_invalid )
        return;

    if ( ( state & align_iter_skip ) != align_iter_skip )
    {
        if ( ( state & align_iter_match ) == align_iter_match )
            (counters->matches)++;
        else
        {
            char c = _4na_to_ascii( state, false );
            switch( c )
            {
                case 'A' : ( counters->mismatches[ 0 ] )++; break;
                case 'C' : ( counters->mismatches[ 1 ] )++; break;
                case 'G' : ( counters->mismatches[ 2 ] )++; break;
                case 'T' : ( counters->mismatches[ 3 ] )++; break;
            }
        }
    }

    if ( reverse )
        (counters->reverse)++;
    else
        (counters->forward)++;

    if ( ( state & align_iter_insert ) == align_iter_insert )
    {
        const INSDC_4na_bin *bases;
        uint32_t n = ReferenceIteratorBasesInserted ( ref_iter, &bases );
        (counters->inserts) += n;
        count_indel_fragment( &(counters->insert_fragments), bases, n );
    }

    if ( ( state & align_iter_delete ) == align_iter_delete )
    {
        const INSDC_4na_bin *bases;
        INSDC_coord_zero ref_pos;
        uint32_t n = ReferenceIteratorBasesDeleted ( ref_iter, &ref_pos, &bases );
        if ( bases != NULL )
        {
            (counters->deletes) += n;
            count_indel_fragment( &(counters->delete_fragments), bases, n );
            free( (void *) bases );
        }
    }

    if ( ( state & align_iter_first ) == align_iter_first )
        ( counters->starting)++;

    if ( ( state & align_iter_last ) == align_iter_last )
        ( counters->ending)++;
}


static rc_t print_counter_line( const char * ref_name,
                                INSDC_coord_zero ref_pos,
                                INSDC_4na_bin ref_base,
                                uint32_t depth,
                                pileup_counters * counters )
{
    char c = _4na_to_ascii( ref_base, false );

    rc_t rc = KOutMsg( "%s\t%u\t%c\t%u\t", ref_name, ref_pos + 1, c, depth );

    if ( rc == 0 && counters->matches > 0 )
        rc = KOutMsg( "%u", counters->matches );

    if ( rc == 0 /* && counters->mismatches[ 0 ] > 0 */ )
        rc = KOutMsg( "\t%u-A", counters->mismatches[ 0 ] );

    if ( rc == 0 /* && counters->mismatches[ 1 ] > 0 */ )
        rc = KOutMsg( "\t%u-C", counters->mismatches[ 1 ] );

    if ( rc == 0 /* && counters->mismatches[ 2 ] > 0 */ )
        rc = KOutMsg( "\t%u-G", counters->mismatches[ 2 ] );

    if ( rc == 0 /* && counters->mismatches[ 3 ] > 0 */ )
        rc = KOutMsg( "\t%u-T", counters->mismatches[ 3 ] );

    if ( rc == 0 )
        rc = KOutMsg( "\tI:" );
    if ( rc == 0 )
        rc = print_fragments( &(counters->insert_fragments) );

    if ( rc == 0 )
        rc = KOutMsg( "\tD:" );
    if ( rc == 0 )
        rc = print_fragments( &(counters->delete_fragments) );

    if ( rc == 0 )
        rc = KOutMsg( "\t%u%%", percent( counters->forward, counters->reverse ) );

    if ( rc == 0 && counters->starting > 0 )
        rc = KOutMsg( "\tS%u", counters->starting );

    if ( rc == 0 && counters->ending > 0 )
        rc = KOutMsg( "\tE%u", counters->ending );

    if ( rc == 0 )
        rc = KOutMsg( "\n" );

    free_fragments( &(counters->insert_fragments) );
    free_fragments( &(counters->delete_fragments) );

    return rc;
}


/* ........................................................................................... */


static rc_t CC walk_counters_enter_ref_pos( walk_data * data )
{
    clear_counters( data->data );
    return 0;
}

static rc_t CC walk_counters_exit_ref_pos( walk_data * data )
{
    rc_t rc = print_counter_line( data->ref_name, data->ref_pos, data->ref_base, data->depth, data->data );
    return rc;
}

static rc_t CC walk_counters_placement( walk_data * data )
{
    walk_counter_state( data->ref_iter, data->state, data->xrec->reverse, data->data );
    return 0;
}

rc_t walk_counters( ReferenceIterator *ref_iter, pileup_options *options )
{
    walk_data data;
    walk_funcs funcs;
    pileup_counters counters;

    data.ref_iter = ref_iter;
    data.options = options;
    data.data = &counters;

    funcs.on_enter_ref = NULL;
    funcs.on_exit_ref = NULL;

    funcs.on_enter_ref_window = NULL;
    funcs.on_exit_ref_window = NULL;

    funcs.on_enter_ref_pos = walk_counters_enter_ref_pos;
    funcs.on_exit_ref_pos = walk_counters_exit_ref_pos;

    funcs.on_enter_spotgroup = NULL;
    funcs.on_exit_spotgroup = NULL;

    funcs.on_placement = walk_counters_placement;

    return walk_0( &data, &funcs );
}


/* =========================================================================================== */


static rc_t print_mismatches_line( const char * ref_name,
                                   INSDC_coord_zero ref_pos,
                                   uint32_t depth,
                                   uint32_t min_mismatch_percent,
                                   pileup_counters * counters )
{
    rc_t rc = 0;
    if ( depth > 0 )
    {
        uint32_t total_mismatches = counters->mismatches[ 0 ] +
                                    counters->mismatches[ 1 ] +
                                    counters->mismatches[ 2 ] +
                                    counters->mismatches[ 3 ];
	if ( total_mismatches * 100 >= min_mismatch_percent * depth) 
        {
                rc = KOutMsg( "%s\t%u\t%u\t%u\n", ref_name, ref_pos + 1, depth, total_mismatches );
        }
    }
    
    free_fragments( &(counters->insert_fragments) );
    free_fragments( &(counters->delete_fragments) );

    return rc;
}


/* ........................................................................................... */


static rc_t CC walk_mismatches_enter_ref_pos( walk_data * data )
{
    clear_counters( data->data );
    return 0;
}

static rc_t CC walk_mismatches_exit_ref_pos( walk_data * data )
{
    rc_t rc = print_mismatches_line( data->ref_name, data->ref_pos,
                                     data->depth, data->options->min_mismatch, data->data );
    return rc;
}

static rc_t CC walk_mismatches_placement( walk_data * data )
{
    walk_counter_state( data->ref_iter, data->state, data->xrec->reverse, data->data );
    return 0;
}


rc_t walk_mismatches( ReferenceIterator *ref_iter, pileup_options * options )
{
    walk_data data;
    walk_funcs funcs;
    pileup_counters counters;

    data.ref_iter = ref_iter;
    data.options = options;
    data.data = &counters;

    funcs.on_enter_ref = NULL;
    funcs.on_exit_ref = NULL;

    funcs.on_enter_ref_window = NULL;
    funcs.on_exit_ref_window = NULL;

    funcs.on_enter_ref_pos = walk_mismatches_enter_ref_pos;
    funcs.on_exit_ref_pos = walk_mismatches_exit_ref_pos;

    funcs.on_enter_spotgroup = NULL;
    funcs.on_exit_spotgroup = NULL;

    funcs.on_placement = walk_mismatches_placement;

    return walk_0( &data, &funcs );
}
