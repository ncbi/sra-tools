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

typedef struct var_counters
{
    uint32_t coverage;
    uint32_t base_counts[ 4 ];      /* 0...A, 1...C, 2...G, 3...T */
    uint32_t deletes;
    uint32_t inserts;
    uint32_t insert_after[ 4 ];     /* 0...A, 1...C, 2...G, 3...T */
} var_counters;


static rc_t CC walk_varcount_enter_ref_pos( walk_data * data )
{
    var_counters * vc = data->data;
    memset( vc, 0, sizeof *vc );
    return 0;
}


static rc_t CC walk_varcount_exit_ref_pos( walk_data * data )
{
    if ( data->depth == 0 )
        return 0;
    else
    {
        var_counters * vc = data->data;
        char ref_base = _4na_to_ascii( data->ref_base, false );

/*
        A ... ref-name
        B ... ref-pos
        C ... ref-base
        D ... coverage

        E ... A ( match or mismatch )
        F ... C ( match or mismatch )
        G ... G ( match or mismatch )
        H ... T ( match or mismatch )

        I ... total deletes
        J ... total insertes

        K ... inserts after A
        L ... inserts after C
        M ... inserts after G
        N ... inserts after T

                          A   B   C   D   E   F   G   H   I   J   K   L   M   N
*/                         
        return KOutMsg( "%s\t%u\t%c\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n", 
                     data->ref_name, data->ref_pos + 1, ref_base, data->depth,

                     vc->base_counts[ 0 ], vc->base_counts[ 1 ], vc->base_counts[ 2 ], vc->base_counts[ 3 ],
                     vc->deletes, vc->inserts,
                     vc->insert_after[ 0 ], vc->insert_after[ 1 ], vc->insert_after[ 2 ], vc->insert_after[ 3 ] );
    }
}


static rc_t CC walk_varcount_placement( walk_data * data )
{
    int32_t state = data->state;
    if ( ( state & align_iter_invalid ) != align_iter_invalid )
    {
        var_counters * vc = data->data;
        uint32_t idx = _4na_to_index( state );

        if ( ( state & align_iter_skip ) == align_iter_skip )
            ( vc->deletes ) ++;
        else if ( ( state & align_iter_match ) != align_iter_match )
            vc->base_counts[ idx ] ++;

        if ( ( state & align_iter_insert ) == align_iter_insert )
        {
            ( vc->inserts )++;
            vc->insert_after[ idx ] ++;
        }
    }
    return 0;
}


rc_t walk_varcount( ReferenceIterator *ref_iter, pileup_options * options )
{
    walk_data data;
    walk_funcs funcs;

    var_counters v_counters;

    data.ref_iter = ref_iter;
    data.options = options;
    data.data = &v_counters;

    funcs.on_enter_ref = NULL;
    funcs.on_exit_ref = NULL;

    funcs.on_enter_ref_window = NULL;
    funcs.on_exit_ref_window = NULL;

    funcs.on_enter_ref_pos = walk_varcount_enter_ref_pos;
    funcs.on_exit_ref_pos = walk_varcount_exit_ref_pos;

    funcs.on_enter_spotgroup = NULL;
    funcs.on_exit_spotgroup = NULL;

    funcs.on_placement = walk_varcount_placement;

    return walk_0( &data, &funcs );
}
