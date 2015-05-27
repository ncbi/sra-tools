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

typedef struct indel_counters
{
    uint32_t coverage;
    uint32_t deletes;
    uint32_t inserts;
} indel_counters;


static rc_t CC walk_indels_enter_ref_pos( walk_data * data )
{
    indel_counters * vc = data->data;
    memset( vc, 0, sizeof *vc );
    return 0;
}


static rc_t CC walk_indels_exit_ref_pos( walk_data * data )
{
	rc_t rc = 0;
    if ( data->depth > 0 )
    {
        indel_counters * vc = data->data;
		if ( ( vc -> deletes + vc -> inserts ) > 0 )
		{
			char ref_base = _4na_to_ascii( data->ref_base, false );

/*
        A ... ref-name
        B ... ref-pos
        C ... ref-base
        D ... coverage

        E ... total deletes
        F ... total insertes
                          A   B   C   D   E   F
*/
			rc = KOutMsg( "%s\t%u\t%c\t%u\t%u\t%u\n", 
                     data->ref_name, data->ref_pos + 1, ref_base, data->depth,
                     vc->deletes, vc->inserts );
		}
    }
	return rc;
}


static rc_t CC walk_indels_placement( walk_data * data )
{
    int32_t state = data->state;
    if ( ( state & align_iter_invalid ) != align_iter_invalid )
    {
        indel_counters * vc = data->data;

        if ( ( state & align_iter_skip ) == align_iter_skip )
            ( vc->deletes ) ++;

        if ( ( state & align_iter_insert ) == align_iter_insert )
            ( vc->inserts )++;
    }
    return 0;
}


rc_t walk_indels( ReferenceIterator *ref_iter, pileup_options * options )
{
    walk_data data;
    walk_funcs funcs;

    indel_counters v_counters;

    data.ref_iter = ref_iter;
    data.options = options;
    data.data = &v_counters;

    funcs.on_enter_ref = NULL;
    funcs.on_exit_ref = NULL;

    funcs.on_enter_ref_window = NULL;
    funcs.on_exit_ref_window = NULL;

    funcs.on_enter_ref_pos = walk_indels_enter_ref_pos;
    funcs.on_exit_ref_pos = walk_indels_exit_ref_pos;

    funcs.on_enter_spotgroup = NULL;
    funcs.on_exit_spotgroup = NULL;

    funcs.on_placement = walk_indels_placement;

    return walk_0( &data, &funcs );
}
