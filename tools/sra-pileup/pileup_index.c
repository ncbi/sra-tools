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


typedef struct index_counters
{
    uint32_t base_counts[ 4 ];   /* 0...A, 1...C, 2...G, 3...T */
    uint32_t inserts;
    uint32_t deletes;
    uint32_t forward;
    uint32_t reverse;
} index_counters;


static rc_t CC walk_index_enter_ref_pos( walk_data * data )
{
    index_counters * ic = data->data;
    memset( ic, 0, sizeof *ic );
    return 0;
}


static rc_t CC walk_index_exit_ref_pos( walk_data * data )
{
    index_counters * ic = data->data;
    if ( ic->forward + ic->reverse == 0 )
        return 0;
    else
        return KOutMsg( "%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n", 
                     data->ref_name, data->ref_pos + 1, 
                     ic->base_counts[ 0 ], ic->base_counts[ 1 ], ic->base_counts[ 2 ], ic->base_counts[ 3 ],
                     ic->inserts, ic->deletes, percent( ic->forward, ic->reverse ) );
}


static rc_t CC walk_index_placement( walk_data * data )
{
    int32_t state = data->state;
    if ( ( state & align_iter_invalid ) != align_iter_invalid )
    {
        index_counters * ic = data->data;

        if ( ( state & align_iter_skip ) == align_iter_skip )
            ( ic->deletes ) ++;
        else
            ic->base_counts[ _4na_to_index( state ) ] ++;

        if ( data->xrec->reverse )
            ( ic->reverse )++;
        else
            ( ic->forward )++;

        if ( ( state & align_iter_insert ) == align_iter_insert )
            ( ic->inserts )++;
    }
    return 0;
}


rc_t walk_index( ReferenceIterator *ref_iter, pileup_options * options )
{
    walk_data data;
    walk_funcs funcs;
    index_counters i_counters;

    data.ref_iter = ref_iter;
    data.options = options;
    data.data = &i_counters;

    funcs.on_enter_ref = NULL;
    funcs.on_exit_ref = NULL;

    funcs.on_enter_ref_window = NULL;
    funcs.on_exit_ref_window = NULL;

    funcs.on_enter_ref_pos = walk_index_enter_ref_pos;
    funcs.on_exit_ref_pos = walk_index_exit_ref_pos;

    funcs.on_enter_spotgroup = NULL;
    funcs.on_exit_spotgroup = NULL;

    funcs.on_placement = walk_index_placement;

    return walk_0( &data, &funcs );
}
