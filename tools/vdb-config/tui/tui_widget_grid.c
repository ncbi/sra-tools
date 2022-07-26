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
#include <tui/tui_dlg.h>
#include "tui_widget.h"
#include "line_policy.h"
#include "string_cache.h"

typedef struct draw_ctx
{
    tui_rect r;
    uint32_t y, vis_col, colcount;
    uint64_t rowcount;
    uint32_t col_width[ MAX_GRID_COLS ];
} draw_ctx;


static const char * grid_get_str( TUIWGrid_data * gd, TUIWGridStr what, uint64_t col, uint64_t row, uint32_t col_width )
{
    const char * res = NULL;
    if ( gd->cb_str != NULL )
    {
        struct string_cache * sc = gd->int_string_cache;
        if ( ( sc != NULL ) && ( what == kGrid_Cell ) )
        {
            res = get_cell_from_string_cache( sc, row, (int)col );
            if ( res == NULL )
            {
                gd->cb_str( what, col, row, col_width, &res, gd->data, gd->instance );
                put_cell_to_string_cache( sc, row, (int)col, res );
            }
        }
        else
        {
            gd->cb_str( what, col, row, col_width, &res, gd->data, gd->instance );
        }
    }
    return res;
}


static uint32_t get_grid_colwidth( struct KTUIWidget * w, TUIWGrid_data * gd, uint64_t col )
{
    uint32_t res = gd->default_col_width;
    if ( gd->cb_int != NULL )
    {
        uint64_t value;
        gd->cb_int( kGrid_Get_Width, col, w->r.w, &value, gd->data, gd->instance );
        res = ( uint32_t )( value & 0xFFFFFFFF );
    }
    return res;
}


static uint32_t get_grid_colcount( struct KTUIWidget * w, TUIWGrid_data * gd )
{
    uint32_t res = 0;
    if ( gd->cb_int != NULL )
    {
        uint64_t value;
        gd->cb_int( kGrid_Get_ColCount, 0, w->r.w, &value, gd->data, gd->instance );
        res = ( uint32_t )( value & 0xFFFFFFFF );
    }
    return res;
}


static uint64_t get_grid_rowcount( struct KTUIWidget * w, TUIWGrid_data * gd )
{
    uint64_t res = 0;
    if ( gd->cb_int != NULL )
        gd->cb_int( kGrid_Get_RowCount, 0, w->r.w, &res, gd->data, gd->instance );
    return res;
}


static void set_grid_colwidth( struct KTUIWidget * w, TUIWGrid_data * gd, uint64_t col, uint32_t value )
{
    if ( gd->cb_int != NULL )
    {
        uint64_t cb_value = value;
        gd->cb_int( kGrid_Set_Width, col, w->r.w, &cb_value, gd->data, gd->instance );
    }
}


static void prepare_page( TUIWGrid_data * gd, uint64_t abs_row, uint32_t rows_per_page )
{
    if ( gd->cb_int != NULL && gd->row_offset_changed )
    {
        uint64_t cb_value;
        gd->cb_int( kGrid_Prepare_Page, abs_row, rows_per_page, &cb_value, gd->data, gd->instance );
    }
}


static void next_row( TUIWGrid_data * gd, uint64_t abs_row )
{
    if ( gd->cb_int != NULL && gd->row_offset_changed )
    {
        uint64_t cb_value;
        gd->cb_int( kGrid_Next_Row, abs_row, 0, &cb_value, gd->data, gd->instance );
    }
}


static rc_t draw_header_cell( struct KTUIWidget * w, TUIWGrid_data * gd, draw_ctx * dctx,
                              uint32_t * x, uint64_t abs_col,  uint32_t left, uint32_t ww,
                              const tui_ac * ac )
{
    rc_t rc = 0;
    uint32_t width = dctx->col_width[ abs_col ];
    if ( width > 0 )
    {
        const char * s;
        if ( *x + width > ww ) width = ww - *x;     /* clip if cell reaches beyond the widget-width */
        s = grid_get_str( gd, kGrid_Col, abs_col, 0, width );
        if ( s != NULL )
            rc = DlgWrite( w->tui, left + *x, dctx->y, ac, s, width - 1 );
    }
    *x += width;
    return rc;
}


static rc_t draw_header( struct KTUIWidget * w, TUIWGrid_data * gd, draw_ctx * dctx )
{
    rc_t rc = 0;
    uint64_t abs_col, cidx;
    uint32_t left = dctx->r.top_left.x + gd->row_hdr_width;
    uint32_t ww = dctx->r.w - gd->row_hdr_width;
    uint32_t x = 0;
    const tui_ac * ac_col_hdr = GetWidgetPaletteEntry ( w, ktuipa_grid_col_hdr );
    const tui_ac * ac_cursor  = GetWidgetPaletteEntry ( w, ktuipa_grid_cursor );

    for ( abs_col = gd->col_offset, cidx = 0;
          x < ww && cidx < dctx->vis_col && abs_col < dctx->colcount && rc == 0;
          ++abs_col, ++cidx )
    {
        if ( abs_col == gd->col )
            rc = draw_header_cell( w, gd, dctx, &x, abs_col, left, ww, ac_cursor );
        else
            rc = draw_header_cell( w, gd, dctx, &x, abs_col, left, ww, ac_col_hdr );
    }
    dctx->y++;
    return rc;
}


static rc_t draw_row( struct KTUIWidget * w, TUIWGrid_data * gd, draw_ctx * dctx, const tui_ac * ac, uint64_t row )
{
    rc_t rc = 0;
    uint32_t left = dctx->r.top_left.x;
    uint32_t x = 0;
    const tui_ac * ac2;

    if ( gd->show_row_header )
    {
        const char * s = grid_get_str( gd, kGrid_Row, 0, row, gd->row_hdr_width );
        if ( s != NULL )
        {
            if ( row == gd->row )
                ac2 = GetWidgetPaletteEntry ( w, ktuipa_grid_cursor );
            else
                ac2 = GetWidgetPaletteEntry ( w, ktuipa_grid_row_hdr );
            if ( gd->row_hdr_width > 0 )
                rc = DlgWrite( w->tui, left, dctx->y, ac2, s, gd->row_hdr_width - 1 );
        }
        x += gd->row_hdr_width;
    }

    if ( rc == 0 )
    {
        uint64_t col, cidx;
        for ( col = gd->col_offset, cidx = 0;
              x < dctx->r.w && cidx < dctx->vis_col && col < dctx->colcount && rc == 0;
              ++col, ++cidx )
        {
            uint32_t width = dctx->col_width[ col ];
            const char * s = grid_get_str( gd, kGrid_Cell, col, row, width );
            /* uint32_t width = get_grid_colwidth( w, gd, col ); */
            if ( s != NULL && width > 0 )
            {
                if ( row == gd->row && col == gd->col )
                    ac2 = GetWidgetPaletteEntry ( w, ktuipa_grid_cursor );
                else
                    ac2 = ac;
                if ( x + width > dctx->r.w ) width = dctx->r.w - x;
                rc = DlgWrite( w->tui, left + x, dctx->y, ac2, s, width - 1 );
            }
            x += width;
        }
    }
    return rc;
}


static uint32_t visible_cols_1( uint32_t width, TUIWGrid_data * gd, draw_ctx * dctx )
{
    uint32_t n = 0, ww = width;
    uint64_t col = gd->col_offset;

    if ( gd->show_row_header )
        ww -= gd->row_hdr_width;

    while( ww > 0 )
    {
        uint32_t cw = dctx->col_width[ col++ ];
        ww = ( ww >= cw ) ? ww - cw : 0;
        n++;
    }
    return n;
}


static uint32_t visible_cols_2( uint32_t width, TUIWGrid_data * gd, struct KTUIWidget * w )
{
    uint32_t n = 0, ww = width;
    uint64_t col = gd->col_offset;

    if ( gd->show_row_header )
        ww -= gd->row_hdr_width;

    while( ww > 0 )
    {
        uint32_t cw = get_grid_colwidth( w, gd, col++ );
        ww = ( ww >= cw ) ? ww - cw : 0;
        n++;
    }
    return n;
}


void enter_col_widths( struct KTUIWidget * w, TUIWGrid_data * gd, draw_ctx * dctx )
{
    uint32_t i;

    dctx->colcount = get_grid_colcount( w, gd );
    dctx->rowcount = get_grid_rowcount( w, gd );

    for( i = 0; i < dctx->colcount; ++ i )
        dctx->col_width[ i ] = get_grid_colwidth( w, gd, i );

    dctx->vis_col = visible_cols_1( dctx->r.w, gd, dctx );
}


void draw_grid( struct KTUIWidget * w )
{
    draw_ctx dctx;
    rc_t rc = KTUIDlgAbsoluteRect ( w->dlg, &dctx.r, &w->r );
    if ( rc == 0 )
    {
        tui_ac ac;
        rc = GetWidgetAc( w, ktuipa_grid, &ac );
        if ( rc == 0 )
            rc = draw_background( w->tui, w->focused, &dctx.r.top_left, dctx.r.w, dctx.r.h, ac.bg );
        if ( rc == 0 )
        {
            TUIWGrid_data * gd = w->grid_data;
            dctx.y = dctx.r.top_left.y;


            enter_col_widths( w, gd, &dctx );

            /* render the fixed row-line */
            if ( gd->show_header )
                rc = draw_header( w, gd, &dctx );


            /* render the fixed cells and row-header */
            if ( rc == 0 )
            {
                uint64_t row, ridx = ( gd->show_header ) ? 1 : 0;
                uint64_t nrow = w->r.h;
                if ( gd->show_h_scroll && dctx.colcount > dctx.vis_col ) nrow--;
                prepare_page( gd, gd->row_offset, (uint32_t)nrow );   /* give the client a hint what comes... */
                for ( row = gd->row_offset;
                       ridx < nrow && row < dctx.rowcount && rc == 0;
                       ++row, ++ridx, dctx.y++ )
                {
                    rc = draw_row( w, gd, &dctx, &ac, row );
                    next_row( gd, row );    /* give the client the opportunity to advance an iterator... */
                }
            }

            /* render the scrollbars */
            if ( rc == 0 && gd->show_v_scroll && dctx.rowcount > dctx.r.h )
                rc = DrawVScroll( w->tui, &dctx.r, dctx.rowcount, gd->row, KTUI_c_gray, ac.fg );

            if ( rc == 0 && gd->show_h_scroll && dctx.colcount > dctx.vis_col )
                rc = DrawHScroll( w->tui, &dctx.r, dctx.colcount, gd->col, KTUI_c_gray, ac.fg );

        }
    }
}


static uint32_t convert_x_to_abs_col( struct KTUIWidget * w, TUIWGrid_data * gd, uint32_t x )
{
    uint32_t i = 0, res = 0, start = 0;
    bool done = false;
    draw_ctx dctx;

    enter_col_widths( w, gd, &dctx );
    if ( gd->show_header )
        start += gd->row_hdr_width;

    do
    {
        uint32_t ww = dctx.col_width[ i ];
        if ( x >= start && x < ( start + ww ) )
        {
            res = i;
            done = true;
        }
        else
        {
            if ( i > dctx.vis_col )
                done = true;
            else
            {
                start += ww;
                i++;
            }
        }
    } while ( !done );

    res += ( uint32_t )gd->col_offset;
    return res;
}


typedef struct grid_cb_ctx
{
    struct KTUIDlg * dlg;
    struct KTUIWidget * w;
    TUIWGrid_data * gd;
} grid_cb_ctx;


static uint32_t CC grid_get_width ( uint64_t col, void * data )
{
    grid_cb_ctx * cb_ctx = data;
    return get_grid_colwidth( cb_ctx->w, cb_ctx->gd, col );
}


static void CC grid_set_width ( uint64_t col, uint32_t value, void * data )
{
    grid_cb_ctx * cb_ctx = data;
    set_grid_colwidth( cb_ctx->w, cb_ctx->gd, col, value );
}


static bool prepare_grid_context( struct KTUIWidget * w, grid_cb_ctx * cb_ctx, grid_context * gc )
{
    bool res = false;
    TUIWGrid_data * gd = w->grid_data;

    if ( gd != NULL )
    {
        cb_ctx->dlg = w->dlg;
        cb_ctx->w = w;
        cb_ctx->gd = gd;

        gc->data = cb_ctx;
        gc->on_get_width = grid_get_width;
        gc->on_set_width = grid_set_width;

        gc->col.count = get_grid_colcount( w, gd );
        gc->col.offset = &gd->col_offset;
        gc->col.curr = &gd->col;
        gc->col.visible = visible_cols_2( w->r.w, gd, w );
        if ( gd->show_row_header ) gc->col.visible--;

        gc->row.count = get_grid_rowcount( w, gd );
        gc->row.offset = &gd->row_offset;
        gc->row.curr = &gd->row;
        gc->row.visible = gd->show_header ? w->r.h -1 : w->r.h;
        if ( gd->show_h_scroll && gc->col.count > gc->col.visible ) gc->row.visible--;
        res = true;
    }
    return res;
}


bool event_grid( struct KTUIWidget * w, tui_event * event, bool hotkey )
{
    bool changed = false;
    bool enter_space = false;        
    TUIWGrid_data * gd = w->grid_data;
    
    if ( event->event_type == ktui_event_kb )
    {
        switch( event->data.kb_data.code )
        {
            case ktui_enter : enter_space = true; break;
            case ktui_alpha : enter_space = ( event->data.kb_data.key == ' ' ); break;
        }
    }

    if ( gd != NULL )
    {
        gd->row_offset_changed = false;
        if ( event->event_type == ktui_event_mouse )
        {
            gd->row = ( gd->row_offset + event->data.mouse_data.y ) - 1;
            gd->col = convert_x_to_abs_col( w, gd, event->data.mouse_data.x );
            changed = true;
        }
        else
        {
            grid_cb_ctx cb_ctx;
            grid_context gc;
            changed = prepare_grid_context( w, &cb_ctx, &gc );
            if ( changed )
            {
                uint64_t offset_before = gd->row_offset;
                changed = grid_handle_event( &gc, event ); /* in line_policy.c */
                if ( changed )
                    gd->row_offset_changed = ( offset_before != gd->row_offset );
            }
        }

        w -> int64_value = gd -> row;
        if ( changed )
            RedrawWidgetAndPushEvent( w, ktuidlg_event_changed, gd->col, gd->row, NULL );
    }
    if ( enter_space )
        KTUIDlgPushEvent( w->dlg, ktuidlg_event_select, w->id, 0, 0, NULL );
    
    return changed;
}


uint64_t get_grid_col( struct KTUIWidget * w )
{
    if ( w->grid_data != NULL )
        return w->grid_data->col;
    else
        return 0;
}


bool set_grid_col( struct KTUIWidget * w, uint64_t col )
{
    grid_cb_ctx cb_ctx;
    grid_context gc;
    bool res = prepare_grid_context( w, &cb_ctx, &gc );
    if ( res )
        res = grid_handle_set_col( &gc, col ); /* in line_policy.c */
    if ( res )
        RedrawWidgetAndPushEvent( w, ktuidlg_event_changed, cb_ctx.gd->col, cb_ctx.gd->row, NULL );
    return res;
}


uint64_t get_grid_row( struct KTUIWidget * w )
{
    if ( w->grid_data != NULL )
        return w->grid_data->row;
    else
        return 0;
}

bool set_grid_row( struct KTUIWidget * w, uint64_t row )
{
    grid_cb_ctx cb_ctx;
    grid_context gc;
    bool res = prepare_grid_context( w, &cb_ctx, &gc );
    if ( res )
        res = grid_handle_set_row( &gc, row ); /* in line_policy.c */
    if ( res )
        RedrawWidgetAndPushEvent( w, ktuidlg_event_changed, cb_ctx.gd->col, cb_ctx.gd->row, NULL );
    return res;
}
