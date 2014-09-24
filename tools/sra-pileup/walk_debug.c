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

#include "ref_walker_0.h"
#include "4na_ascii.h"

#include <klib/out.h>

static rc_t CC walk_debug_enter_ref( walk_data * data )
{   return KOutMsg( "ENTER REF '%s' ( %u.%u )\n", data->ref_name, data->ref_start, data->ref_len );   }

static rc_t CC walk_debug_exit_ref( walk_data * data )
{   return KOutMsg( "EXIT  REF '%s' ( %u.%u )\n", data->ref_name, data->ref_start, data->ref_len );   }

static rc_t CC walk_debug_enter_ref_window( walk_data * data )
{   return KOutMsg( "  ENTER REF-WINDOW ( %u.%u )\n", data->ref_window_start, data->ref_window_len );   }

static rc_t CC walk_debug_exit_ref_window( walk_data * data )
{   return KOutMsg( "  EXIT  REF-WINDOW ( %u.%u )\n", data->ref_window_start, data->ref_window_len );   }

static rc_t CC walk_debug_enter_ref_pos( walk_data * data )
{   return KOutMsg( "    ENTER REF-POS ( %u / d=%u / '%c' )\n", data->ref_pos, data->depth, _4na_to_ascii( data->ref_base, false ) );   }

static rc_t CC walk_debug_exit_ref_pos( walk_data * data )
{   return KOutMsg( "    EXIT  REF-POS ( %u / d=%u / '%c' )\n", data->ref_pos, data->depth, _4na_to_ascii( data->ref_base, false ) );   }

static rc_t CC walk_debug_enter_sg( walk_data * data )
{   return KOutMsg( "      ENTER SPOTGROUP '%s'\n", data->spotgroup );   }

static rc_t CC walk_debug_exit_sg( walk_data * data )
{   return KOutMsg( "      EXIT SPOTGROUP '%s'\n", data->spotgroup );   }

static rc_t CC walk_debug_placement( walk_data * data )
{
    char c = ( data->xrec->reverse ? 'R' : 'F' );
    return KOutMsg( "        PLACEMENT #%lu %c ( TLEN %i )\n", data->rec->id, c, data->xrec->tlen );
}


rc_t walk_debug( ReferenceIterator *ref_iter, pileup_options *options )
{
    rc_t rc;
    walk_data data;
    walk_funcs funcs;

    data.ref_iter = ref_iter;
    data.options = options;
    
    funcs.on_enter_ref = walk_debug_enter_ref;
    funcs.on_exit_ref = walk_debug_exit_ref;

    funcs.on_enter_ref_window = walk_debug_enter_ref_window;
    funcs.on_exit_ref_window = walk_debug_exit_ref_window;

    funcs.on_enter_ref_pos = walk_debug_enter_ref_pos;
    funcs.on_exit_ref_pos = walk_debug_exit_ref_pos;

    funcs.on_enter_spotgroup = walk_debug_enter_sg;
    funcs.on_exit_spotgroup = walk_debug_exit_sg;
    
    funcs.on_placement = walk_debug_placement;

    rc = walk_0( &data, &funcs );
    return rc;
}
