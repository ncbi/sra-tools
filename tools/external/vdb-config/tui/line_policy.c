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

#include <klib/rc.h>
#include <klib/text.h>
#include <klib/printf.h>

#include <tui/tui.h>
#include "line_policy.h"

#include <sysalloc.h>
#include <stdlib.h>


static void lp_adjust( lp_context * lp )
{
    if ( *lp->cur_pos < *lp->offset )
        *lp->offset = *lp->cur_pos;
    else if ( *lp->cur_pos >= ( *lp->offset + lp->visible ) )
        *lp->offset = ( *lp->cur_pos - lp->visible ) + 1;
    else if ( lp->len == 0 && ( *lp->cur_pos > 0 ) )
        *lp->cur_pos = 0;
}


static bool lp_alpha( lp_context * lp, char c )
{
    bool res = ( lp->len < ( lp->max_len - 1 ) && ( c != 27 ) );
    if ( res && lp -> alpha_mode != NULL && * lp -> alpha_mode > 0 )
            res = ( c >= '0' && c <= '9' );
    if ( res )
    {
        if ( *lp->cur_pos < lp->len )
        {
            if ( *lp->ins_mode == 0 )
            {
                uint64_t idx;
                lp->line[ lp->len + 1 ] = 0;
                for ( idx = lp->len; idx > *lp->cur_pos; --idx )
                lp->line[ idx ] = lp->line[ idx - 1 ];
            }
            lp->line[ ( *lp->cur_pos )++ ] = c;
        }
        else
        {
            lp->line[ ( *lp->cur_pos )++ ] = c;
            lp->line[ *lp->cur_pos ] = 0;
            lp->len = string_measure ( lp->line, NULL );
        }
        lp_adjust( lp );
        lp->content_changed = true;
    }
    return res;
}


static bool lp_home( lp_context * lp )
{
    bool res = ( *lp->cur_pos > 0 );
    if ( res )
    {
        *lp->cur_pos = 0;
        *lp->offset = 0;
    }
    return res;
}


static bool lp_end( lp_context * lp )
{
    bool res = ( *lp->cur_pos != lp->len );
    if ( res )
    {
        *lp->cur_pos = lp->len;
        lp_adjust( lp );
    }
    return res;
}


static bool lp_right( lp_context * lp )
{
    bool res = ( *lp->cur_pos < lp->len );
    if ( res )
    {
        ( *lp->cur_pos )++;
        lp_adjust( lp );
    }
    return res;
}


static bool lp_left( lp_context * lp )
{
    bool res = ( *lp->cur_pos > 0 );
    if ( res )
    {
        ( *lp->cur_pos )--;
        lp_adjust( lp );
    }
    return res;
}


static bool lp_bksp( lp_context * lp )
{
    bool res = ( *lp->cur_pos > 0 );
    if ( res )
    {
        if ( *lp->cur_pos < lp->len  )
        {
            uint64_t idx;
            for ( idx = *lp->cur_pos - 1; idx < lp->len; ++idx )
                lp->line[ idx ] = lp->line[ idx + 1 ];
            ( *lp->cur_pos )--;
        }
        else
            lp->line[ --( *lp->cur_pos ) ] = 0;
        lp_adjust( lp );
        lp->content_changed = true;
    }
    return res;
}


static bool lp_del( lp_context * lp )
{
    bool res = ( *lp->cur_pos < lp->len  );
    if ( res  )
    {
        uint64_t idx;
        for ( idx = *lp->cur_pos; idx < lp->len; ++idx )
            lp->line[ idx ] = lp->line[ idx + 1 ];
        lp_adjust( lp );
        lp->content_changed = true;
    }
    return res;
}


static bool lp_ins( lp_context * lp )
{
    if ( *lp->ins_mode == 0 )
        *lp->ins_mode = 1;
    else
        *lp->ins_mode = 0;
    return true;
}


bool lp_handle_event( lp_context * lp, tui_event * event )
{
    bool res = false;
    lp->len = string_measure ( lp->line, NULL );
    if ( event->event_type == ktui_event_kb )
    {
        switch( event->data.kb_data.code )
        {
            case ktui_home  : res = lp_home( lp ); break;
            case ktui_end   : res = lp_end( lp ); break;
            case ktui_left  : res = lp_left( lp ); break;
            case ktui_right : res = lp_right( lp ); break;
            case ktui_bksp  : res = lp_bksp( lp ); break;
            case ktui_del   : res = lp_del( lp ); break;
            case ktui_ins   : res = lp_ins( lp ); break;
            case ktui_alpha : res = lp_alpha( lp, event->data.kb_data.key ); break;
        }
    }
    else if ( event->event_type == ktui_event_mouse )
    {
        *lp->cur_pos = *lp->offset + event->data.mouse_data.x - 1;
        if ( *lp->cur_pos > lp->len )
            *lp->cur_pos = lp->len;
        lp_adjust( lp );
        res = true;
    }
    return res;
}


/* ****************************************************************************************** */

static void gen_adjust( gen_context * ctx )
{
    if ( *ctx->curr >= ctx->count )
        *ctx->curr = ctx->count - 1;

    if ( *ctx->curr < *ctx->offset )
        *ctx->offset = *ctx->curr;
    else if ( *ctx->curr >= ( *ctx->offset + ctx->visible ) )
        *ctx->offset = *ctx->curr - ctx->visible + 1;
}


static bool gen_home( gen_context * ctx )
{
    bool res = ( *ctx->curr > 0 );
    if ( res )
    {
        *ctx->curr = 0;
        *ctx->offset = 0;
    }
    return res;
}


static bool gen_end( gen_context * ctx )
{
    bool res = ( *ctx->curr < ctx->count );
    if ( res )
    {
        *ctx->curr = ctx->count - 1;
        gen_adjust( ctx );
    }
    return res;
}


static bool gen_move( gen_context * ctx, int32_t by )
{
    bool res = false;

    if ( by < 0 )
    {
        res = ( *ctx->curr > 0 );
        if ( res )
        {
            uint32_t dist = -by;
            if ( *ctx->curr >= dist )
                *ctx->curr -= dist;
            else
                *ctx->curr = 0;
        }
    }
    else if ( by > 0 )
    {
        res = ( *ctx->curr < ( ctx->count - 1 ) );
        if ( res )
            *ctx->curr += by;
    }

    if ( res )
        gen_adjust( ctx );
    return res;
}


static bool gen_set( gen_context * ctx, uint64_t value )
{
    bool res = ( *ctx->curr != value );
    if ( res )
    {
        *ctx->curr = value;
        gen_adjust( ctx );
    }
    return res;
}

/* ****************************************************************************************** */


bool list_handle_event( gen_context * ctx, tui_event * event, uint32_t x_max )
{
    bool res = false;
    if ( event->event_type == ktui_event_kb )
    {
        switch( event->data.kb_data.code )
        {
            case ktui_home  : res = gen_home( ctx ); break;
            case ktui_end   : res = gen_end( ctx ); break;
            case ktui_up    : res = gen_move( ctx, -1 ); break;
            case ktui_down  : res = gen_move( ctx, 1 ); break;
            case ktui_pgup  : res = gen_move( ctx, -7 ); break;
            case ktui_pgdn  : res = gen_move( ctx, 7 ); break;
        }
    }
    else if ( event->event_type == ktui_event_mouse )
    {
        uint32_t x = event->data.mouse_data.x;
        bool flag = ( x_max > 0 ) ? ( x > 0 && x < x_max ) : true;
        if ( flag )
        {
            *ctx->curr = *ctx->offset + event->data.mouse_data.y;
            res = true;
        }
    }
    return res;
}


/* ****************************************************************************************** */

static bool grid_handle_width( grid_context * gp, tui_event * event, int32_t by )
{
    bool res = false;
    if ( gp->on_get_width != NULL && gp->on_set_width != NULL )
    {
        uint64_t col = *( gp->col.curr );
        int32_t width = gp->on_get_width( col, gp->data );
        if ( by > 0 )
        {
            gp->on_set_width( col, width + by, gp->data );
            res = true;
        }
        else if ( by < 0 )
        {
            if ( ( width - 1 ) > ( -by ) )
            {
                gp->on_set_width( col, width + by, gp->data );
                res = true;
            }
        }
    }
    return res;
}

static bool grid_handle_alpha( grid_context * gp, tui_event * event )
{
    bool res = false;
    switch( event->data.kb_data.key )
    {
        case '+' : res = grid_handle_width( gp, event, +1 ); break;
        case '-' : res = grid_handle_width( gp, event, -1 ); break;
    }
    return res;
}

bool grid_handle_event( grid_context * gp, tui_event * event )
{
    bool res = false;

    if ( event->event_type == ktui_event_kb )
    {
        switch( event->data.kb_data.code )
        {
            case ktui_home  : res = gen_home( &gp->row ); break;
            case ktui_end   : res = gen_end( &gp->row ); break;
            case ktui_up    : res = gen_move( &gp->row, -1 ); break;
            case ktui_down  : res = gen_move( &gp->row, 1 ); break;
            case ktui_left  : res = gen_move( &gp->col, -1 ); break;
            case ktui_right : res = gen_move( &gp->col, 1 ); break;
            case ktui_pgup  : res = gen_move( &gp->row, -7 ); break;
            case ktui_pgdn  : res = gen_move( &gp->row, 7 ); break;
            case ktui_alpha : res = grid_handle_alpha( gp, event ); break;
        }
    }

    return res;
}


bool grid_handle_set_col( grid_context * gp, uint64_t col )
{
    return gen_set( &gp->col, col );
}

bool grid_handle_set_row( grid_context * gp, uint64_t row )
{
    return gen_set( &gp->row, row );
}
