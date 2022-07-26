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
#include "tui_widget.h"
#include "string_cache.h"

#include <sysalloc.h>
#include <math.h>

rc_t DlgPaint( struct KTUI * tui, int x, int y, int w, int h, KTUI_color c )
{
    tui_rect r;
    tui_ac ac;

    r.top_left.x = x;
    r.top_left.y = y;
    r.w = w;
    r.h = h;
    set_ac( &ac, KTUI_a_none, c, c );

    return KTUIRect ( tui, &r, &ac, ' ' );
}


rc_t DlgWrite( struct KTUI * tui, int x, int y, const tui_ac * ac, const char * s, uint32_t l )
{
    tui_point p;

    p.x = x;
    p.y = y;

    return KTUIPrint( tui, &p, ac, s, l );
}


static rc_t draw_hl_label( struct KTUI * tui, uint32_t x, uint32_t y, uint32_t w,
                           tui_ac * ac, tui_ac * hl_ac, size_t offset, const char * caption )
{
    rc_t rc;
    
    if ( offset == 0 )
    {
        rc = DlgWrite( tui, x, y, ac, caption + 1, w );
        if ( rc == 0 )
            /* one highlighted char */
            rc = DlgWrite( tui, x, y, hl_ac, caption + 1, 1 );
    }
    else if ( offset < w )
    {
        rc = DlgWrite( tui, x, y, ac, caption, offset );
        if ( rc == 0 )
        {
            const char * cap_ofs = caption + offset + 1;
            /* one highlighted char */
            rc = DlgWrite( tui, x + offset, y, hl_ac, cap_ofs, 1 );
            if ( rc == 0 )
                rc = DlgWrite( tui, x + offset + 1, y, ac, cap_ofs + 1, w - ( offset + 1 ) );
        }
    }
    else
        rc = DlgWrite( tui, x, y, ac, caption, w );
    return rc;
}


rc_t draw_highlighted( struct KTUI * tui, uint32_t x, uint32_t y, uint32_t w,
                       tui_ac * ac, tui_ac * hl_ac, const char * caption )
{
    rc_t rc = 0;
    size_t s_cap;
    uint32_t l_cap = string_measure( caption, &s_cap );
    if ( l_cap > 0 )
    {
        char * ampersand = string_chr ( caption, s_cap, '&' );
        if ( ampersand != NULL )
            rc = draw_hl_label( tui, x, y, w, ac, hl_ac, ( ampersand - caption ), caption );
        else
            rc = DlgWrite( tui, x, y, ac, caption, w );
    }
    return rc;
}


rc_t DrawVScroll( struct KTUI * tui, tui_rect * r, uint64_t count, uint64_t value,
                  KTUI_color c_bar, KTUI_color c_sel )
{
    uint32_t x = r->top_left.x + r->w - 1;
    rc_t rc = DlgPaint( tui, x, r->top_left.y, 1, r->h, c_bar );
    if ( rc == 0 && count > 0 )
    {
        uint64_t v = ( r->h * value ) / count;
        uint32_t y = r->top_left.y + (uint32_t)v;
        rc = DlgPaint( tui, x, y, 1, 1, c_sel );
    }
    return rc;
}


rc_t DrawHScroll( struct KTUI * tui, tui_rect * r, uint64_t count, uint64_t value,
                  KTUI_color c_bar, KTUI_color c_sel )
{
    uint32_t x = r->top_left.x + 1;
    uint32_t y = r->top_left.y + r->h - 1;
    uint32_t w = r->w - 3;
    rc_t rc = DlgPaint( tui, x, y, w, 1, c_bar );
    if ( rc == 0 && count > 0 )
    {
        uint64_t v = ( w * value ) / count;
        rc = DlgPaint( tui, x + (uint32_t)v, y, 1, 1, c_sel );
    }
    return rc;
}


rc_t draw_background( struct KTUI * tui, bool focused, tui_point * p,
                      int w, int h, KTUI_color bg )
{
    rc_t rc;
    if ( focused )
    {
        rc = DlgPaint( tui, p->x, p->y, 1, h, KTUI_c_red );
        if ( rc == 0 )
            rc = DlgPaint( tui, p->x + 1, p->y, w - 1, h, bg );
    }
    else
        rc = DlgPaint( tui, p->x, p->y, w, h, bg );
    return rc;
}


/* ****************************************************************************************** */


static void init_widget( KTUIWidget * w, uint32_t id, KTUI_Widget_type wtype, const tui_rect * r )
{
    copy_rect( &w->r, r );

    w->caption = NULL;
    w->can_focus = true;
    w->visible = true;
    w->focused = false;
    w->id = id;
    w->wtype = wtype;
    w->ints[ 0 ] = 0;   /* widget_string_list.DL_V_OFFSET */
    w->ints[ 1 ] = 0;   /* widget_string_list.DL_H_OFFSET */
    w->ints[ 2 ] = 0;
    w->ints[ 3 ] = 0;
    w->changed = false;
    w->bool_value = false;
    w->bg_override_flag = false;
    w->fg_override_flag = false;

    w->txt = NULL;
    w->txt_length = 0;
    w->strings = NULL;
    w->selected_string = 0;
}


rc_t TUI_MakeWidget ( KTUIWidget ** self, struct KTUIDlg * dlg, uint32_t id,
                      KTUI_Widget_type wtype, const tui_rect * r,
                      draw_cb on_draw, event_cb on_event )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        *self = NULL;
        if ( dlg == NULL )
            rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
        else
        {
            struct KTUIPalette * pal = KTUIDlgGetPalette ( dlg );
            if ( pal == NULL )
                rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcInvalid );
            else
            {
                rc = KTUIPaletteAddRef ( pal );
                if ( rc == 0 )
                {
                    KTUIWidget * w = malloc( sizeof * w );
                    if ( w == NULL )
                        rc = RC( rcApp, rcAttr, rcCreating, rcMemory, rcExhausted );
                    else
                    {
                        w->dlg = dlg;
                        w->tui = KTUIDlgGetTui ( dlg );
                        w->palette = pal;

                        init_widget( w, id, wtype, r );
                        w->on_draw = on_draw;
                        w->on_event = on_event;

                        ( * self ) = w;
                    }
                }
            }
        }
    }
    return rc;
}


void TUI_DestroyWidget( struct KTUIWidget * self )
{
    if ( self != NULL )
    {
        if ( self->caption != NULL )
            free( ( void * ) self->caption );
        if ( self->txt != NULL )
            free( ( void * ) self->txt );
        if ( self->strings != NULL )
            VNamelistRelease ( self->strings );

        KTUIPaletteRelease ( self->palette );

        free( ( void * ) self );
    }
}


const tui_ac * GetWidgetPaletteEntry ( struct KTUIWidget * self, KTUIPa_entry what )
{
    return KTUIPaletteGet ( self->palette, what );
}


struct KTUIPalette * CopyWidgetPalette( struct KTUIWidget * self )
{
    struct KTUIPalette * res = NULL;
    if ( self != NULL )
    {
        rc_t rc = KTUIPaletteMake ( &res );
        if ( rc == 0 )
        {
            rc = KTUIPaletteCopy ( res, self->palette );
            if ( rc == 0 )
            {
                KTUIPaletteRelease ( self->palette );
                self->palette = res;
            }
        }
    }
    return res;
}


rc_t ReleaseWidgetPalette( struct KTUIWidget * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        KTUIPaletteRelease ( self->palette );
        self->palette = KTUIDlgGetPalette ( self->dlg );
        KTUIPaletteAddRef ( self->palette );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcSelf, rcNull );
    return rc;
}

/* the attribute and color of regular text */
rc_t GetWidgetAc( struct KTUIWidget * self, KTUIPa_entry pa_entry, tui_ac * ac )
{
    rc_t rc = 0;
    if ( self != NULL && ac != NULL )
    {
        copy_ac( ac, GetWidgetPaletteEntry ( self, pa_entry ) );
        if ( self->bg_override_flag )
            ac->bg = self->bg_override;
        if ( self->fg_override_flag )
            ac->fg = self->fg_override;
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcSelf, rcNull );
    return rc;
}

/* the attribute and color of highlighted text */
rc_t GetWidgetHlAc( struct KTUIWidget * self, KTUIPa_entry pa_entry, tui_ac * ac )
{
    tui_ac norm;
    rc_t rc = GetWidgetAc( self, pa_entry, &norm );
    if ( rc == 0  )
    {
        const tui_ac * palette_hl = KTUIPaletteGet ( self -> palette, ktuipa_hl );
        // determine the highlighted style...
        ac -> attr = palette_hl -> attr;
        ac -> fg = palette_hl -> fg;
        ac -> bg = norm . bg;
    }
    return rc;
}

rc_t RedrawWidget( KTUIWidget * w )
{
    rc_t rc = 0;
    if ( w->on_draw != NULL )
    {
        w->on_draw( w );

        /* flush it out... */
        rc = KTUIFlush ( w->tui, false );
    }
    return rc;
}

rc_t RedrawWidgetAndPushEvent( KTUIWidget * w,
           KTUIDlg_event_type ev_type, uint64_t value_1, uint64_t value_2, void * ptr_0 )
{
    rc_t rc = RedrawWidget( w );
    if ( rc == 0 )
        KTUIDlgPushEvent( w->dlg, ev_type, w->id, value_1, value_2, ptr_0 );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


rc_t GetWidgetRect ( struct KTUIWidget * self, tui_rect * r )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else if ( r == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcParam, rcNull );
    else
        copy_rect( r, &self->r );
    return rc;
}


rc_t SetWidgetRect ( struct KTUIWidget * self, const tui_rect * r, bool redraw )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcUpdating, rcSelf, rcNull );
    else if ( r == NULL )
        rc = RC( rcApp, rcAttr, rcUpdating, rcParam, rcNull );
    else
    {
        if ( redraw )
        {
            struct KTUIPalette * dlg_palette = KTUIDlgGetPalette ( self->dlg );
            const tui_ac * dlg_ac = KTUIPaletteGet ( dlg_palette, ktuipa_dlg );
            rc = DlgPaint( self->tui, self->r.top_left.x, self->r.top_left.y,
                            self->r.w, self->r.h, dlg_ac->bg );
        }
        if ( rc == 0 )
        {
            copy_rect( &self->r, r );
            if ( redraw )
                rc = RedrawWidget( self );
        }
    }
    return rc;
}

/* ---------------------------------------------------------------------------------------------- */

rc_t GetWidgetPageId ( struct KTUIWidget * self, uint32_t * id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else if ( id == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcParam, rcNull );
    else
        *id = self -> page_id;
    return rc;
}

rc_t SetWidgetPageId ( struct KTUIWidget * self, uint32_t id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else
        self -> page_id = id;
    return rc;
}

/* ---------------------------------------------------------------------------------------------- */


const char * GetWidgetCaption ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->caption;
    else
        return NULL;
}


rc_t SetWidgetCaption ( struct KTUIWidget * self, const char * caption )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->caption != NULL )
            free( ( void * ) self->caption );
            
        if ( caption != NULL )
            self->caption = string_dup_measure ( caption, NULL );
        else
            self->caption = NULL;

        rc = RedrawWidget( self );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcSelf, rcNull );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */



rc_t SetWidgetCanFocus( struct KTUIWidget * self, bool can_focus )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self -> wtype != KTUIW_label )
            self->can_focus = can_focus;
        else
            self->can_focus = false; /* a label should never be able to get a focus */
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcSelf, rcNull );
    return rc;

}


/* ---------------------------------------------------------------------------------------------- */


bool GetWidgetVisible ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->visible;
    else
        return false;
}


rc_t SetWidgetVisible ( struct KTUIWidget * self, bool visible, bool * redraw )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        *redraw = ( self->visible != visible );
        self->visible = visible;
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcSelf, rcNull );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


bool GetWidgetChanged ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->changed;
    else
        return false;
}


rc_t SetWidgetChanged ( struct KTUIWidget * self, bool changed )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        self->changed = changed;
        if ( changed )
            KTUIDlgSetChanged ( self->dlg );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcSelf, rcNull );
    return rc;
}


/* called if the value of a widged has changed */
static rc_t Widget_Value_Changed( struct KTUIWidget * self )
{
    self->changed = true;
    return RedrawWidget( self );
}


/* ---------------------------------------------------------------------------------------------- */


rc_t SetWidgetBg ( struct KTUIWidget * self, KTUI_color value )
{
    rc_t rc;
    if ( self != NULL )
    {
        self->bg_override = value;
        self->bg_override_flag = true;
        rc = RedrawWidget( self );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


rc_t ReleaseWidgetBg ( struct KTUIWidget * self )
{
    rc_t rc;
    if ( self != NULL )
    {
        self->bg_override_flag = false;
        rc = RedrawWidget( self );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


rc_t SetWidgetFg ( struct KTUIWidget * self, KTUI_color value )
{
    rc_t rc;
    if ( self != NULL )
    {
        self->fg_override = value;
        self->fg_override_flag = true;
        rc = RedrawWidget( self );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


rc_t ReleaseWidgetFg ( struct KTUIWidget * self )
{
    rc_t rc;
    if ( self != NULL )
    {
        self->fg_override_flag = false;
        rc = RedrawWidget( self );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}

/* ---------------------------------------------------------------------------------------------- */


bool GetWidgetBoolValue ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->bool_value;
    else
        return false;
}


rc_t SetWidgetBoolValue ( struct KTUIWidget * self, bool value )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->bool_value != value )
        {
            self->bool_value = value;
            rc = Widget_Value_Changed( self );
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


int64_t GetWidgetInt64Value ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->int64_value;
    else
        return 0;
}


rc_t SetWidgetInt64Value ( struct KTUIWidget * self, int64_t value )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->int64_value != value )
        {
            int64_t prev = self->int64_value;
            if ( value < self->int64_min )
                self->int64_value = self->int64_min;
            else if ( value > self->int64_max )
                self->int64_value = self->int64_max;
            else
                self->int64_value = value;

            if ( prev != self->int64_value )
                rc = Widget_Value_Changed( self );
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


int64_t GetWidgetInt64Min ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->int64_min;
    else
        return 0;
}


rc_t SetWidgetInt64Min ( struct KTUIWidget * self, int64_t value )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->int64_min != value )
        {
            self->int64_min = value;
            if ( self->int64_value < value )
                rc = SetWidgetInt64Value ( self, value );
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


int64_t GetWidgetInt64Max ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->int64_max;
    else
        return 0;
}


rc_t SetWidgetInt64Max ( struct KTUIWidget * self, int64_t value )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->int64_max != value )
        {
            self->int64_max = value;
            if ( self->int64_value > value )
                rc = SetWidgetInt64Value ( self, value );
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


int32_t GetWidgetPercent ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->percent;
    else
        return 0;
}


int32_t CalcWidgetPercent( int64_t value, int64_t max, uint32_t precision );

rc_t SetWidgetPercent ( struct KTUIWidget * self, int32_t value )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->percent != value )
        {
            if ( value >= 0 && value <= CalcWidgetPercent( 100, 100, self->precision ) )
            {
                self->percent = value;
                rc = Widget_Value_Changed( self );
            }
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


int32_t GetWidgetPrecision ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->precision;
    else
        return 0;
}


rc_t SetWidgetPrecision ( struct KTUIWidget * self, int32_t value )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->precision != value )
        {
            if ( value >= 0 && value < 5 )
            {
                self->precision = value;
                rc = RedrawWidget( self );
            }
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}

/*
    
    result      %           precision
    50          50%         0
    515         51.5%       1
    5274        52.74%      2
    53112       53.112%     3
    540072      54.0072%    4   ( max. precision )
*/

int32_t CalcWidgetPercent( int64_t value, int64_t max, uint32_t precision )
{
    int32_t res = 0;
    if ( precision > 4 ) precision = 4;
    if ( max > 0 && value > 0 && value <= max )
    {
        double x = (uint32_t)value;
        switch( precision )
        {
            case 0 : x *= 100.0; break;
            case 1 : x *= 1000.0; break;
            case 2 : x *= 10000.0; break;
            case 3 : x *= 100000.0; break;
            case 4 : x *= 1000000.0; break;
            default: x *= 100.0; break;
        }
        x /= max;
        res = ( int32_t )floor( x );
    }
    return res;
}


/* ---------------------------------------------------------------------------------------------- */


const char * GetWidgetText ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->txt;
    else
        return 0;
}


rc_t SetWidgetText ( struct KTUIWidget * self, const char * txt )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->txt != NULL )
        {
            if ( txt != NULL )
            {
                size_t l = string_size ( txt );
                if ( l >= self->txt_length )
                    rc = SetWidgetTextLength ( self, l + 1 );
                if ( rc == 0 )
                    string_copy ( self->txt, self->txt_length, txt, l );
            }
            else
                self->txt[ 0 ] = 0;
            if ( rc == 0 )
                rc = Widget_Value_Changed( self );
        }
        else
        {
            if ( txt != NULL )
            {
                size_t l = string_size ( txt );
                rc = SetWidgetTextLength ( self, l + 1 );
                if ( rc == 0 )
                    rc = string_copy ( self->txt, self->txt_length, txt, l );
                if ( rc == 0 )
                    rc = Widget_Value_Changed( self );
            }
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


size_t GetWidgetTextLength ( struct KTUIWidget * self )
{
    if ( self != NULL )
        return self->txt_length;
    else
        return 0;
}


rc_t SetWidgetTextLength ( struct KTUIWidget * self, size_t new_length )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->txt != NULL )
        {
            if ( self->txt_length != new_length )
            {
                char * tmp = malloc( new_length );
                if ( tmp != NULL )
                {
                    string_copy ( tmp, new_length, self->txt, string_size ( self->txt ) );
                    free( ( void * )self->txt );
                    self->txt = tmp;
                    if ( new_length < self->txt_length )
                    {
                        self->txt_length = new_length;
                        rc = Widget_Value_Changed( self );
                    }
                    else
                        self->txt_length = new_length;
                }
                else
                    rc = RC( rcApp, rcAttr, rcUpdating, rcMemory, rcExhausted );
            }
        }
        else
        {
            self->txt = malloc( new_length );
            if ( self->txt != NULL )
            {
                self->txt_length = new_length;
                self->txt[ 0 ] = 0;
            }
            else
            {
                self->txt_length = 0;
                rc = RC( rcApp, rcAttr, rcUpdating, rcMemory, rcExhausted );
            }
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}

/* code is in tui_widget_inputline.c */
void set_carret_pos_input_line( struct KTUIWidget * w, size_t pos );

rc_t SetWidgetCarretPos ( struct KTUIWidget * self, size_t new_pos )
{
    if ( self != NULL && self -> wtype == KTUIW_input )
        set_carret_pos_input_line( self, new_pos );
    return 0;
}

/* code is in tui_widget_inputline.c */
void set_inputline_alpha_mode( struct KTUIWidget * w, uint32_t alpha_mode );

rc_t SetWidgetAlphaMode ( struct KTUIWidget * self, uint32_t alpha_mode )
{
    if ( self != NULL && self -> wtype == KTUIW_input )
        set_inputline_alpha_mode( self, alpha_mode );
    return 0;
}

/* ---------------------------------------------------------------------------------------------- */


rc_t AddWidgetString ( struct KTUIWidget * self, const char * txt )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->strings == NULL )
            rc = VNamelistMake ( &self->strings, 32 );
        if ( rc == 0 )
        {
            rc = VNamelistAppend ( self->strings, txt );
            if ( rc == 0 )
                rc = Widget_Value_Changed( self );
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


rc_t AddWidgetStrings ( struct KTUIWidget * self, VNamelist * src )
{
    rc_t rc = 0;
    if ( self != NULL && src != NULL )
    {
        if ( self->strings == NULL )
            rc = VNamelistMake ( &self->strings, 32 );
        if ( rc == 0 )
        {
            uint32_t count, idx;
            rc = VNameListCount ( src, &count );
            for ( idx = 0; rc == 0 && idx < count; ++idx )
            {
                const char * s;
                rc = VNameListGet ( src, idx, &s );
                if ( rc == 0 && s != NULL )
                    rc = VNamelistAppend ( self->strings, s );
            }
            if ( rc == 0 && count > 0 )
                rc = Widget_Value_Changed( self );
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


const char * GetWidgetStringByIdx ( struct KTUIWidget * self, uint32_t idx )
{
    const char * res = NULL;
    if ( self != NULL && self->strings != NULL )
        VNameListGet ( self->strings, idx, &res );
    return res;
}


rc_t RemoveWidgetStringByIdx ( struct KTUIWidget * self, uint32_t idx )
{
    rc_t rc;
    if ( self != NULL )
    {
        if ( self->strings != NULL )
        {
            rc = VNamelistRemoveIdx( self->strings, idx );
            if ( rc == 0 )
            {
                uint32_t count;
                rc = VNameListCount ( self->strings, &count );
                if ( rc == 0 )
                {
                    if ( count > 0 )
                    {
                        if ( self->selected_string >= count )
                            self->selected_string = count - 1;
                    }
                    else
                        self->selected_string = 0;
                    rc = Widget_Value_Changed( self );
                }
            }
        }
        else
            rc = RC( rcApp, rcAttr, rcUpdating, rcItem, rcInvalid );
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


rc_t RemoveAllWidgetStrings ( struct KTUIWidget * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( self->strings != NULL )
        {
            rc = VNamelistRemoveAll( self->strings );
            if ( rc == 0 )
            {
                self->selected_string = 0;
                self->ints[ 0 ] = 0;   /* widget_string_list.DL_V_OFFSET */
                self->ints[ 1 ] = 0;   /* widget_string_list.DL_H_OFFSET */
                rc = Widget_Value_Changed( self );
            }
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;

}


uint32_t GetWidgetStringCount ( struct KTUIWidget * self )
{
    uint32_t res = 0;
    if ( self != NULL && self->strings != NULL )
        VNameListCount ( self->strings, &res );
    return res;
}


uint32_t HasWidgetString( struct KTUIWidget * self, const char * txt )
{
    uint32_t res = 0xFFFFFFFF;
    if ( self != NULL && self->strings != NULL )
        VNamelistIndexOf( self->strings, txt, &res );
    return res;
}


uint32_t GetWidgetSelectedString( struct KTUIWidget * self )
{
    uint32_t res = 0;
    if ( self != NULL )
        res = (uint32_t)self->selected_string;
    return res;
}


rc_t SetWidgetSelectedString ( struct KTUIWidget * self, uint32_t selection )
{
    rc_t rc = 0;
    if ( self != NULL && self->strings != NULL )
    {
        uint32_t count;
        rc = VNameListCount ( self->strings, &count );
        if ( rc == 0 )
        {
            uint32_t prev = (uint32_t)self->selected_string;
            if ( count > 0 )
            {
                if ( selection >= count )
                    self->selected_string = count - 1;
                else
                    self->selected_string = selection;
                /* here we have to adjust self->ints[ 0 ] ... the vertical offset ! */
            }
            else
                self->selected_string = 0; 
            if ( prev != self->selected_string )
                rc = Widget_Value_Changed( self );
        }
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}


/* ---------------------------------------------------------------------------------------------- */


TUIWGrid_data * GetWidgetGridData( struct KTUIWidget * self )
{
    TUIWGrid_data * res = NULL;
    if ( self != NULL )
        res = self->grid_data;
    return res;
}


rc_t SetWidgetGridData( struct KTUIWidget * self, TUIWGrid_data * grid_data, bool cached )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( grid_data != NULL )
        {
            grid_data->int_string_cache = make_string_cache( 200, 128 );
            grid_data->row_offset_changed = true;
        }
        self->grid_data = grid_data;
    }
    else
        rc = RC( rcApp, rcAttr, rcUpdating, rcId, rcInvalid );
    return rc;
}
