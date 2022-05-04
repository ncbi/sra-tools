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

#include <tui/extern.h>
#include <klib/refcount.h>
#include <klib/rc.h>
#include <klib/vector.h>
#include <klib/text.h>
#include <klib/printf.h>

#include <tui/tui_dlg.h>
#include "tui_widget.h"
#include "eventring.h"
#include "line_policy.h"
#include "tui_menu.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <ctype.h>

/* ******************************************************************************************

    prototypes of all the concrete widget types ( aka 2 callbacks each )

   ****************************************************************************************** */

/* from tui_widget_label.c */
void draw_label( struct KTUIWidget * w );
bool event_label( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_button.c */
void draw_button( struct KTUIWidget * w );
bool event_button( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_checkbox.c */
void draw_checkbox( struct KTUIWidget * w );
bool event_checkbox( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_tabhdr.c */
void draw_tabhdr( struct KTUIWidget * w );
bool event_tabhdr( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_inputline.c */
void init_inputline( struct KTUIWidget * w );
void draw_inputline( struct KTUIWidget * w );
bool event_inputline( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_radiobox.c */
void draw_radiobox( struct KTUIWidget * w );
bool event_radiobox( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_string_list.c */
void draw_list( struct KTUIWidget * w );
bool event_list( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_progress.c */
void draw_progress( struct KTUIWidget * w );

/* from tui_widget_spin_edit.c */
void draw_spinedit( struct KTUIWidget * w );
bool event_spinedit( struct KTUIWidget * w, tui_event * event, bool hotkey );

/* from tui_widget_grid.c */
void draw_grid( struct KTUIWidget * w );
bool event_grid( struct KTUIWidget * w, tui_event * event, bool hotkey );

uint64_t get_grid_col( struct KTUIWidget * w );
bool set_grid_col( struct KTUIWidget * w, uint64_t col );
uint64_t get_grid_row( struct KTUIWidget * w );
bool set_grid_row( struct KTUIWidget * w, uint64_t row );


/* ****************************************************************************************** */


typedef struct KTUIDlg
{
    KRefcount refcount;

    struct KTUI * tui;
    struct KTUIDlg * parent;
    struct KTUIPalette * palette;
    struct KTUI_Menu * menu;
    char * caption;
    void * data;

    Event_Ring events;
    Vector widgets;
    tui_rect r;
    uint32_t focused, active_page;
    bool menu_active, changed, done, cursor_navigation;

} KTUIDlg;


static const char tuidlg_classname [] = "TUIDlg_Implementation";


static rc_t KTUIDlgDestroy ( struct KTUIDlg * self )
{
    uint32_t i, n;
    KTUIRelease ( self->tui );

    n = VectorLength( &self->widgets );
    for ( i = 0; i < n; ++i )
    {
        KTUIWidget * w = VectorGet ( &self->widgets, i );
        if ( w != NULL )
            TUI_DestroyWidget( w );
    }

    if ( self->caption != NULL )
        free( ( void * )self->caption );

    if ( self->menu != NULL )
        KTUI_Menu_Release ( self->menu );

    KTUIPaletteRelease ( self->palette );
    event_ring_destroy( &self->events );

    free( ( void * )self );
    return 0;
}


LIB_EXPORT rc_t CC KTUIDlgMake ( struct KTUI * tui, struct KTUIDlg ** self, struct KTUIDlg * parent,
                    struct KTUIPalette * palette, tui_rect * r )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUIDlg * td = malloc( sizeof * td );
        if ( td == NULL )
            rc = RC( rcApp, rcAttr, rcCreating, rcMemory, rcExhausted );
        else
        {
            rc = KTUIAddRef ( tui );
            if ( rc == 0 )
            {
                td->tui = tui;
                td->parent = parent;
                td->active_page = 0;
                td->cursor_navigation = true;
                
                if ( palette == NULL )
                {
                    if ( parent == NULL )
                        rc = KTUIPaletteMake ( &td->palette );
                    else
                    {
                        rc = KTUIPaletteAddRef ( parent->palette );
                        if ( rc == 0 )
                            td->palette = parent->palette;
                    }
                }
                else
                {
                    rc = KTUIPaletteAddRef ( palette );
                    if ( rc == 0 )
                        td->palette = palette;
                }

                if ( rc == 0 )
                {
                    KRefcountInit( &td->refcount, 1, "TUIDlg", "make", tuidlg_classname );
                    VectorInit ( &td->widgets, 0, 16 );
                    event_ring_init( &td->events );
                    td->caption = NULL;
                    td->focused = 0;
                    td->menu = NULL;
                    td->menu_active = td->done = td->changed = false;
                    td->data = NULL;

                    if ( r != NULL )
                    {
                        copy_rect( &td->r, r );
                        /*
                        if ( parent == NULL )
                            copy_rect( &td->r, r );
                        else
                        {
                            tui_rect pr;
                            rc = KTUIDlgGetRect ( parent, &pr );
                            if ( rc == 0 )
                            {
                                td->r.top_left.x = pr.top_left.x + r->top_left.x;
                                td->r.top_left.y = pr.top_left.y + r->top_left.y;
                                td->r.w = r->w;
                                td->r.h = r->h;
                            }
                        }
                        */
                    }
                    else
                    {
                        int w, h;
                        rc = KTUIGetExtent ( tui, &w, &h );
                        if ( rc == 0 )
                            set_rect( &td->r, 0, 0, w, h );
                    }
                }
            }

            if ( rc == 0 )
                ( * self ) = td;
            else
                KTUIDlgDestroy ( td );
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgAddRef ( const struct KTUIDlg * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        switch ( KRefcountAdd( &self->refcount, tuidlg_classname ) )
        {
        case krefOkay :
            break;
        case krefZero :
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcIncorrect );
        case krefLimit :
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcExhausted );
        case krefNegative :
            rc =  RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcInvalid );
        default :
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcUnknown );
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgRelease ( const struct KTUIDlg * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        switch ( KRefcountDrop( &self->refcount, tuidlg_classname ) )
        {
        case krefOkay :
        case krefZero :
            break;
        case krefWhack :
            rc = KTUIDlgDestroy( ( struct KTUIDlg * )self );
            break;
        case krefNegative:
            return RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcInvalid );
        default:
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcUnknown );
            break;            
        }
    }
    return rc;
}


LIB_EXPORT void CC KTUIDlgSetData ( struct KTUIDlg * self, void * data )
{
    if ( self != NULL )
        self->data = data;
}


LIB_EXPORT void * CC KTUIDlgGetData ( struct KTUIDlg * self )
{
    if ( self != NULL )
        return self->data;
    else
        return NULL;
}


LIB_EXPORT void CC KTUIDlgSetDone ( struct KTUIDlg * self, bool done )
{
    if ( self != NULL )
        self->done = done;
}


LIB_EXPORT bool CC KTUIDlgGetDone ( struct KTUIDlg * self )
{
    if ( self != NULL )
        return self->done;
    return false;
}


LIB_EXPORT void CC KTUIDlgSetChanged ( struct KTUIDlg * self )
{
    if ( self != NULL )
        self->changed = true;
}


LIB_EXPORT void CC KTUIDlgClearChanged ( struct KTUIDlg * self )
{
    if ( self != NULL )
    {
        uint32_t i, n;
        n = VectorLength( &self->widgets );
        for ( i = 0; i < n; ++i )
        {
            KTUIWidget * w = VectorGet ( &self->widgets, i );
            if ( w != NULL )
                w->changed = false;
        }
        self->changed = false;
    }
}


LIB_EXPORT bool CC KTUIDlgGetChanged ( struct KTUIDlg * self )
{
    if ( self != NULL )
        return self->changed;
    return false;
}


LIB_EXPORT struct KTUI * CC KTUIDlgGetTui ( struct KTUIDlg * self )
{
    if ( self != NULL )
        return self->tui;
    else
        return NULL;
}


LIB_EXPORT struct KTUIPalette * CC KTUIDlgGetPalette ( struct KTUIDlg * self )
{
    if ( self != NULL )
        return self->palette;
    else
        return NULL;
}


LIB_EXPORT struct KTUIDlg * CC KTUIDlgGetParent ( struct KTUIDlg * self )
{
    if ( self != NULL )
        return self->parent;
    else
        return NULL;
}


LIB_EXPORT rc_t CC KTUIDlgSetCaption ( struct KTUIDlg * self, const char * caption )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        if ( self->caption != NULL )
            free( ( void * ) self->caption );
        
        if ( caption != NULL )
            self->caption = string_dup_measure ( caption, NULL );
        else
            self->caption = NULL;
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgGetRect ( struct KTUIDlg * self, tui_rect * r )
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


LIB_EXPORT rc_t CC KTUIDlgSetRect ( struct KTUIDlg * self, const tui_rect * r, bool redraw )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else if ( r == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcParam, rcNull );
    else
    {
        copy_rect( &self->r, r );
        if ( redraw )
            rc = KTUIDlgDraw( self, true );
    }
    return rc;
}


LIB_EXPORT bool CC KTUIDlgGetMenuActive ( const struct KTUIDlg * self )
{
    bool res = false;
    if ( self != NULL )
        res = self->menu_active;
    return res;
}


LIB_EXPORT rc_t CC KTUIDlgSetMenuActive ( struct KTUIDlg * self, bool active )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else if ( self->menu == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcItem, rcInvalid );
    else
    {
        self->menu_active = active;
        rc = KTUIDlgDraw( self, false );
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgSetMenu ( struct KTUIDlg * self, struct KTUI_Menu * menu )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else if ( menu == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcParam, rcNull );
    else
    {
        if ( self->menu != NULL )
            KTUI_Menu_Release ( self->menu );
        self->menu = menu;
    }
    return rc;
}

LIB_EXPORT rc_t CC KTUIDlgSetActivePage ( struct KTUIDlg * self, uint32_t page_id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else if ( self -> active_page != page_id )
    {
        uint32_t i, n = VectorLength( &self->widgets );
        bool dlg_redraw = false;
        
        /* hide/deactivate all widgets belonging to the current active page */
        for ( i = 0; i < n; ++i )
        {
            KTUIWidget * w = VectorGet ( &self->widgets, i );
            if ( w != NULL )
            {
                if ( w -> page_id == 0 )
                {
                    // widgets on page zero are always visible
                }
                else if ( w -> page_id == self -> active_page )
                {
                    bool redraw;
                    SetWidgetCanFocus( w, false );
                    SetWidgetVisible ( w, false, &redraw );
                    dlg_redraw |= redraw;
                }
                else if ( w -> page_id == page_id )
                {
                    bool redraw;
                    SetWidgetCanFocus( w, true );
                    SetWidgetVisible ( w, true, &redraw );
                    dlg_redraw |= redraw;
                }
            }
        }
        self->active_page = page_id;
        if ( dlg_redraw )
            rc = KTUIDlgDraw( self, false );    
    }
    return rc;
}

LIB_EXPORT rc_t CC KTUIDlgGetActivePage ( struct KTUIDlg * self, uint32_t * page_id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcSelf, rcNull );
    else if ( page_id == NULL )
        rc = RC( rcApp, rcAttr, rcSelecting, rcParam, rcNull );
    else
        *page_id = self->active_page;
    return rc;
   
}

/* ****************************************************************************************** */

LIB_EXPORT void KTUIDlgPushEvent( struct KTUIDlg * self,
                       KTUIDlg_event_type event_type, uint32_t widget_id,
                       uint64_t value_1, uint64_t value_2, void * ptr_0 )
{
    tuidlg_event * ev = dlg_event_ring_get_from_stock_or_make( &self->events );
    if ( ev != NULL )
    {
        ev->event_type = event_type;
        ev->widget_id = widget_id;
        ev->value_1 = value_1;
        ev->value_2 = value_2;
        ev->ptr_0 = ptr_0;
        dlg_event_ring_put( &self->events, ev );
    }
}


static void KTUIDlgAbsolutePoint( struct KTUIDlg * self, tui_point * p )
{
    p->x += self->r.top_left.x;
    p->y += self->r.top_left.y;
    if ( self->parent != NULL )
        KTUIDlgAbsolutePoint( self->parent, p );    /* recursion ! */
}


static void KTUIDlgRelativePoint( struct KTUIDlg * self, tui_point * p )
{
    p->x -= self->r.top_left.x;
    p->y -= self->r.top_left.y;
    if ( self->parent != NULL )
        KTUIDlgRelativePoint( self->parent, p );    /* recursion ! */
}


static void KTUIDlgAbsoluteDlgRect( struct KTUIDlg * self, tui_rect * dst, tui_rect * src )
{
    dst->top_left.x = 0;
    dst->top_left.y = 0;
    KTUIDlgAbsolutePoint( self, &dst->top_left );
    dst->w = src->w;
    dst->h = src->h;
}

LIB_EXPORT rc_t KTUIDlgAbsoluteRect ( struct KTUIDlg * self, tui_rect * dst, tui_rect * src )
{
    dst->top_left.x = src->top_left.x;
    dst->top_left.y = src->top_left.y;
    KTUIDlgAbsolutePoint( self, &dst->top_left );
    dst->w = src->w;
    dst->h = src->h;
    return 0;
}


/* ****************************************************************************************** */


static KTUIWidget * KTUIDlgGetWidgetById( struct KTUIDlg * self, uint32_t id )
{
    KTUIWidget * res = NULL;

    uint32_t i, n = VectorLength( &self->widgets );
    for ( i = 0; i < n && res == NULL; ++i )
    {
        KTUIWidget * w = VectorGet ( &self->widgets, i );
        if ( w != NULL )
        {
            if ( w->id == id ) res = w;
        }
    }
    return res;
}


/* ****************************************************************************************** */


static rc_t KTUIDlgAddWidget( struct KTUIDlg * self, uint32_t id, KTUI_Widget_type wtype, const tui_rect * r,
                              draw_cb on_draw, event_cb on_event, init_cb on_init )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUIWidget * w;
        rc = TUI_MakeWidget ( &w, self, id, wtype, r, on_draw, on_event );
        if ( rc == 0 )
        {
            if ( on_init != NULL )
                on_init( w );
            VectorAppend ( &self->widgets, &w->vector_idx, w );
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgAddLabel( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_label, r, draw_label, event_label, NULL );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetCanFocus( self, id, false );
    if ( rc == 0 && caption != NULL )
        rc = KTUIDlgSetWidgetCaption ( self, id, caption );
    return rc;
}

LIB_EXPORT rc_t CC KTUIDlgAddTabHdr( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_tabhdr, r, draw_tabhdr, event_tabhdr, NULL );
    if ( rc == 0 && caption != NULL )
        rc = KTUIDlgSetWidgetCaption ( self, id, caption );
    return rc;
}

LIB_EXPORT rc_t CC KTUIDlgAddLabel2( struct KTUIDlg * self, uint32_t id,
                                     uint32_t x, uint32_t y, uint32_t w, const char * caption )
{
    tui_rect r;
    set_rect( &r, x, y, w, 1 );
    return KTUIDlgAddLabel( self, id, &r, caption );
}


LIB_EXPORT rc_t CC KTUIDlgAddBtn ( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_button, r, draw_button, event_button, NULL );
    if ( rc == 0 && caption != NULL )
        rc = KTUIDlgSetWidgetCaption ( self, id, caption );
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgAddBtn2 ( struct KTUIDlg * self, uint32_t id,
                                    uint32_t x, uint32_t y, uint32_t w, const char * caption )
{
    tui_rect r;
    set_rect( &r, x, y, w, 1 );
    return KTUIDlgAddBtn ( self, id, &r, caption );
}


LIB_EXPORT rc_t CC KTUIDlgAddCheckBox ( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_checkbox, r, draw_checkbox, event_checkbox, NULL );
    if ( rc == 0 && caption != NULL )
        rc = KTUIDlgSetWidgetCaption ( self, id, caption );
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgAddInput ( struct KTUIDlg * self, uint32_t id, const tui_rect * r,
                                     const char * txt, size_t length )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_input, r, draw_inputline, event_inputline, init_inputline );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetTextLength ( self, id, length );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetText ( self, id, txt );
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgAddRadioBox ( struct KTUIDlg * self, uint32_t id, const tui_rect * r )
{
    return KTUIDlgAddWidget( self, id, KTUIW_radiobox, r, draw_radiobox, event_radiobox, NULL );
}


LIB_EXPORT rc_t CC KTUIDlgAddList ( struct KTUIDlg * self, uint32_t id, const tui_rect * r )
{
    return KTUIDlgAddWidget( self, id, KTUIW_list, r, draw_list, event_list, NULL );
}


LIB_EXPORT rc_t CC KTUIDlgSetHScroll ( struct KTUIDlg * self, uint32_t id, bool enabled )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUIWidget *w = KTUIDlgGetWidgetById( self, id );
        if ( w != NULL )
        {
            if ( w->wtype == KTUIW_list )
            {
                w->ints[ 2 ] = enabled ? 1 : 0;
                w->ints[ 1 ] = 0;
            }
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgAddProgress ( struct KTUIDlg * self, uint32_t id,
                                        const tui_rect * r, int32_t percent, int32_t precision )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_progress, r, draw_progress, NULL, NULL );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetCanFocus( self, id, false );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetPrecision ( self, id, precision );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetPercent ( self, id, percent );
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgAddSpinEdit ( struct KTUIDlg * self, uint32_t id,
                                        const tui_rect * r, int64_t value, int64_t min, int64_t max )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_spinedit, r, draw_spinedit, event_spinedit, NULL );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetInt64Min ( self, id, min );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetInt64Max ( self, id, max );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetInt64Value ( self, id, value );

    return rc;
}


/* ****************************************************************************************** */


LIB_EXPORT rc_t CC KTUIDlgAddGrid ( struct KTUIDlg * self, uint32_t id,
                                    const tui_rect * r, TUIWGrid_data * grid_data, bool cached )
{
    rc_t rc = KTUIDlgAddWidget( self, id, KTUIW_grid, r, draw_grid, event_grid, NULL );
    if ( rc == 0 )
        rc = KTUIDlgSetWidgetGridData( self, id, grid_data, cached );
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgGetGridCol( struct KTUIDlg * self, uint32_t id, uint64_t *col )
{
    rc_t rc = 0;
    if ( col == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
    {
        KTUIWidget * w = KTUIDlgGetWidgetById( self, id );
        if ( w != NULL && w->wtype == KTUIW_grid )
            *col = get_grid_col( w );
        else
            rc = RC( rcApp, rcAttr, rcSelecting, rcId, rcInvalid );
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgSetGridCol( struct KTUIDlg * self, uint32_t id, uint64_t col )
{
    rc_t rc = 0;
    KTUIWidget * w = KTUIDlgGetWidgetById( self, id );
    if ( w != NULL && w->wtype == KTUIW_grid )
        set_grid_col( w, col );
    else
        rc = RC( rcApp, rcAttr, rcSelecting, rcId, rcInvalid );
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgGetGridRow( struct KTUIDlg * self, uint32_t id, uint64_t *row )
{
    rc_t rc = 0;
    if ( row == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
    {
        KTUIWidget * w = KTUIDlgGetWidgetById( self, id );
        if ( w != NULL && w->wtype == KTUIW_grid )
            *row = get_grid_row( w );
        else
            rc = RC( rcApp, rcAttr, rcSelecting, rcId, rcInvalid );
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgSetGridRow( struct KTUIDlg * self, uint32_t id, uint64_t row )
{
    rc_t rc = 0;
    KTUIWidget * w = KTUIDlgGetWidgetById( self, id );
    if ( w != NULL && w->wtype == KTUIW_grid )
        set_grid_row( w, row );
    else
        rc = RC( rcApp, rcAttr, rcSelecting, rcId, rcInvalid );
    return rc;
}


/* ****************************************************************************************** */


static void KTUIDlgVectorReIndex ( struct KTUIDlg * self )
{
    uint32_t i, n = VectorLength( &self->widgets );
    for ( i = 0; i < n; ++i )
    {
        KTUIWidget * w = VectorGet ( &self->widgets, i );
        if ( w != NULL )
            w->vector_idx = i;
    }
}

LIB_EXPORT rc_t CC KTUIDlgRemove ( struct KTUIDlg * self, uint32_t id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUIWidget * w = KTUIDlgGetWidgetById( self, id );
        if ( w != NULL )
        {
            void * removed = NULL;
            rc = VectorRemove ( &self->widgets, w->vector_idx, &removed );
            if ( rc == 0 )
            {
                if ( w != NULL ) TUI_DestroyWidget( w );
                KTUIDlgVectorReIndex ( self );
                rc = KTUIDlgDraw( self, false );
            }
        }
        else
            rc = RC( rcApp, rcAttr, rcSelecting, rcId, rcInvalid );
    }
    return rc;
}



/* ****************************************************************************************** */
static rc_t draw_caption( struct KTUI * tui, const tui_ac * capt_ac, const tui_rect * r, const char * caption )
{
    rc_t rc = DlgPaint( tui, r->top_left.x, r->top_left.y, r->w, 1, capt_ac->bg );
    if ( rc == 0 && caption != NULL )
    {
        rc = DlgWrite( tui, r->top_left.x + 5, r->top_left.y, capt_ac, caption, r->w - 10 );
    }
    return rc;
}

LIB_EXPORT rc_t CC KTUIDlgDrawCaption( struct KTUIDlg * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( self->caption != NULL )
    {
        tui_rect abs;
        const tui_ac * capt_ac = KTUIPaletteGet ( self->palette, ktuipa_dlg_caption );
        KTUIDlgAbsoluteDlgRect( self, &abs, &self->r );
        rc = draw_caption( self->tui, capt_ac, &abs, self->caption );
    }
    if ( rc == 0 )
        rc = KTUIFlush ( self->tui, false );
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgDraw( struct KTUIDlg * self, bool forced )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        uint32_t i, n;

        /* draw the background... */
        tui_rect abs;
        const tui_ac * dlg_ac = KTUIPaletteGet ( self->palette, ktuipa_dlg );
        KTUIDlgAbsoluteDlgRect( self, &abs, &self->r );

        rc = DlgPaint( self->tui, abs.top_left.x, abs.top_left.y, abs.w, abs.h, dlg_ac->bg );

        /* draw the caption... */
        if ( rc == 0 && self->caption != NULL )
        {
            const tui_ac * capt_ac = KTUIPaletteGet ( self->palette, ktuipa_dlg_caption );
            rc = draw_caption( self->tui, capt_ac, &abs, self->caption );
        }

        /* draw the widgets */
        n = VectorLength( &self->widgets );
        for ( i = 0; i < n; ++i )
        {
            KTUIWidget * w = VectorGet ( &self->widgets, i );
            if ( w != NULL )
                if ( w->on_draw != NULL && w->visible )
                    w->on_draw( w );
        }

        /* if the menu is activated... */
        if ( self->menu_active && self->menu != NULL )
            KTUI_Menu_Draw( self->menu, self );

        /* flush it all out... */
        rc = KTUIFlush ( self->tui, forced );
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgDrawWidget( struct KTUIDlg * self, uint32_t id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUIWidget * w = KTUIDlgGetWidgetById( self, id );
        if ( w != NULL && w->visible )
            rc = RedrawWidget( w );
    }
    return rc;
}


/* ****************************************************************************************** */


LIB_EXPORT const char * CC KTUIDlgGetWidgetCaption ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetCaption ( KTUIDlgGetWidgetById( self, id ) );   }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetCaption ( struct KTUIDlg * self, uint32_t id, const char * caption )
{   return SetWidgetCaption ( KTUIDlgGetWidgetById( self, id ), caption );  }


LIB_EXPORT struct KTUIPalette * CC KTUIDlgNewWidgetPalette ( struct KTUIDlg * self, uint32_t id )
{   return CopyWidgetPalette( KTUIDlgGetWidgetById( self, id ) );   }
LIB_EXPORT rc_t CC KTUIDlgReleaseWidgetPalette ( struct KTUIDlg * self, uint32_t id )
{   return ReleaseWidgetPalette( KTUIDlgGetWidgetById( self, id ) );    }


LIB_EXPORT rc_t CC KTUIDlgGetWidgetRect ( struct KTUIDlg * self, uint32_t id, tui_rect * r )
{   return GetWidgetRect( KTUIDlgGetWidgetById( self, id ), r );    }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetRect ( struct KTUIDlg * self, uint32_t id, const tui_rect * r, bool redraw )
{   return SetWidgetRect( KTUIDlgGetWidgetById( self, id ), r, redraw );    }

LIB_EXPORT rc_t CC KTUIDlgGetWidgetPageId ( struct KTUIDlg * self, uint32_t id, uint32_t * page_id )
{   return GetWidgetPageId( KTUIDlgGetWidgetById( self, id ), page_id );    }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetPageId ( struct KTUIDlg * self, uint32_t id, uint32_t page_id )
{   return SetWidgetPageId( KTUIDlgGetWidgetById( self, id ), page_id );    }

LIB_EXPORT rc_t CC KTUIDlgSetWidgetCanFocus ( struct KTUIDlg * self, uint32_t id, bool can_focus )
{   return SetWidgetCanFocus( KTUIDlgGetWidgetById( self, id ), can_focus );    }


LIB_EXPORT bool KTUIDlgGetWidgetVisible ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetVisible ( KTUIDlgGetWidgetById( self, id ) );   }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetVisible ( struct KTUIDlg * self, uint32_t id, bool visible )
{
    bool redraw = false;
    rc_t rc = SetWidgetVisible ( KTUIDlgGetWidgetById( self, id ), visible, &redraw );
    if ( redraw ) rc = KTUIDlgDraw( self, false );
    return rc;
}


LIB_EXPORT bool CC KTUIDlgGetWidgetChanged ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetChanged ( KTUIDlgGetWidgetById( self, id ) );   }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetChanged ( struct KTUIDlg * self, uint32_t id, bool changed )
{   return SetWidgetChanged ( KTUIDlgGetWidgetById( self, id ), changed );  }


LIB_EXPORT bool CC KTUIDlgGetWidgetBoolValue ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetBoolValue ( KTUIDlgGetWidgetById( self, id ) ); }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetBoolValue ( struct KTUIDlg * self, uint32_t id, bool value )
{   return SetWidgetBoolValue ( KTUIDlgGetWidgetById( self, id ), value );  }


LIB_EXPORT rc_t CC KTUIDlgSetWidgetBg ( struct KTUIDlg * self, uint32_t id, KTUI_color value )
{   return SetWidgetBg ( KTUIDlgGetWidgetById( self, id ), value );  }
LIB_EXPORT rc_t CC KTUIDlgReleaseWidgetBg ( struct KTUIDlg * self, uint32_t id )
{   return ReleaseWidgetBg ( KTUIDlgGetWidgetById( self, id ) );  }

LIB_EXPORT rc_t CC KTUIDlgSetWidgetFg ( struct KTUIDlg * self, uint32_t id, KTUI_color value )
{   return SetWidgetFg ( KTUIDlgGetWidgetById( self, id ), value );  }
LIB_EXPORT rc_t CC KTUIDlgReleaseWidgetFg ( struct KTUIDlg * self, uint32_t id )
{   return ReleaseWidgetFg ( KTUIDlgGetWidgetById( self, id ) );  }

LIB_EXPORT int64_t CC KTUIDlgGetWidgetInt64Value ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetInt64Value ( KTUIDlgGetWidgetById( self, id ) );    }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetInt64Value ( struct KTUIDlg * self, uint32_t id, int64_t value )
{   return SetWidgetInt64Value ( KTUIDlgGetWidgetById( self, id ), value ); }


LIB_EXPORT int64_t CC KTUIDlgGetWidgetInt64Min ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetInt64Min ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetInt64Min ( struct KTUIDlg * self, uint32_t id, int64_t value )
{   return SetWidgetInt64Min ( KTUIDlgGetWidgetById( self, id ), value ); }


LIB_EXPORT int64_t CC KTUIDlgGetWidgetInt64Max ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetInt64Max ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetInt64Max ( struct KTUIDlg * self, uint32_t id, int64_t value )
{   return SetWidgetInt64Max ( KTUIDlgGetWidgetById( self, id ), value ); }


LIB_EXPORT int32_t CC KTUIDlgGetWidgetPercent ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetPercent ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetPercent ( struct KTUIDlg * self, uint32_t id, int32_t value )
{   return SetWidgetPercent ( KTUIDlgGetWidgetById( self, id ), value ); }

LIB_EXPORT int32_t CC KTUIDlgGetWidgetPrecision ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetPrecision ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetPrecision( struct KTUIDlg * self, uint32_t id, int32_t value )
{   return SetWidgetPrecision ( KTUIDlgGetWidgetById( self, id ), value ); }

LIB_EXPORT int32_t CC KTUIDlgCalcPercent ( int64_t value, int64_t max, uint32_t precision )
{   return CalcWidgetPercent( value, max, precision ); }


LIB_EXPORT const char * CC KTUIDlgGetWidgetText( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetText ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetText ( struct KTUIDlg * self, uint32_t id, const char * value )
{   return SetWidgetText ( KTUIDlgGetWidgetById( self, id ), value ); }
LIB_EXPORT size_t CC KTUIDlgGetWidgetTextLenght( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetTextLength ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetTextLength ( struct KTUIDlg * self, uint32_t id, size_t value )
{   return SetWidgetTextLength ( KTUIDlgGetWidgetById( self, id ), value ); }

LIB_EXPORT rc_t CC KTUIDlgSetWidgetCarretPos ( struct KTUIDlg * self, uint32_t id, size_t value )
{   return SetWidgetCarretPos ( KTUIDlgGetWidgetById( self, id ), value ); }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetAlphaMode ( struct KTUIDlg * self, uint32_t id, uint32_t value )
{   return SetWidgetAlphaMode ( KTUIDlgGetWidgetById( self, id ), value ); }

LIB_EXPORT rc_t CC KTUIDlgAddWidgetString ( struct KTUIDlg * self, uint32_t id, const char * txt )
{   return AddWidgetString ( KTUIDlgGetWidgetById( self, id ), txt );  }
LIB_EXPORT rc_t CC KTUIDlgAddWidgetStrings ( struct KTUIDlg * self, uint32_t id, VNamelist * src )
{   return AddWidgetStrings ( KTUIDlgGetWidgetById( self, id ), src );  }
LIB_EXPORT const char * CC KTUIDlgGetWidgetStringByIdx ( struct KTUIDlg * self, uint32_t id, uint32_t idx )
{   return GetWidgetStringByIdx ( KTUIDlgGetWidgetById( self, id ), idx );  }
LIB_EXPORT rc_t CC KTUIDlgRemoveWidgetStringByIdx ( struct KTUIDlg * self, uint32_t id, uint32_t idx )
{   return RemoveWidgetStringByIdx ( KTUIDlgGetWidgetById( self, id ), idx );  }
LIB_EXPORT rc_t CC KTUIDlgRemoveAllWidgetStrings ( struct KTUIDlg * self, uint32_t id )
{   return RemoveAllWidgetStrings ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT uint32_t CC KTUIDlgGetWidgetStringCount ( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetStringCount ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT uint32_t CC KTUIDlgHasWidgetString( struct KTUIDlg * self, uint32_t id, const char * txt )
{   return HasWidgetString ( KTUIDlgGetWidgetById( self, id ), txt );  }
LIB_EXPORT uint32_t CC KTUIDlgGetWidgetSelectedString( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetSelectedString ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetSelectedString ( struct KTUIDlg * self, uint32_t id, uint32_t selection )
{   return SetWidgetSelectedString ( KTUIDlgGetWidgetById( self, id ), selection );  }

LIB_EXPORT bool CC KTUIDlgHasWidget ( struct KTUIDlg * self, uint32_t id )
{   return ( KTUIDlgGetWidgetById( self, id ) != NULL );  }

LIB_EXPORT TUIWGrid_data * CC KTUIDlgGetWidgetGridData( struct KTUIDlg * self, uint32_t id )
{   return GetWidgetGridData ( KTUIDlgGetWidgetById( self, id ) );  }
LIB_EXPORT rc_t CC KTUIDlgSetWidgetGridData( struct KTUIDlg * self, uint32_t id, TUIWGrid_data * grid_data, bool cached )
{   return SetWidgetGridData ( KTUIDlgGetWidgetById( self, id ), grid_data, cached );  }



/* ****************************************************************************************** */


static int32_t KTUIDlgFirstFocus( struct KTUIDlg * self )
{
    int32_t res = -1;
    uint32_t i, n = VectorLength( &self->widgets );
    for ( i = 0; i < n && res < 0; ++i )
    {
        KTUIWidget * w = VectorGet ( &self->widgets, i );
        if ( w != NULL )
        {
            if ( w->can_focus ) res = w->id;
        }
    }
    return res;
}


static int32_t KTUIDlgLastFocus( struct KTUIDlg * self )
{
    int32_t res = -1;
    uint32_t i, n = VectorLength( &self->widgets );
    for ( i = n-1; i > 0 && res < 0; --i )
    {
        KTUIWidget * w = VectorGet ( &self->widgets, i );
        if ( w != NULL )
        {
            if ( w->can_focus ) res = w->id;
        }
    }
    return res;
}


static rc_t KTUIDlgSetFocusTo( struct KTUIDlg * self, KTUIWidget * w )
{
    rc_t rc = 0;
    if ( w->can_focus && !w->focused )
    {
        KTUIWidget * has_focus  = KTUIDlgGetWidgetById( self, self->focused );
        if ( has_focus != NULL )
        {
            has_focus->focused = false;
            if ( has_focus->on_draw != NULL )
                has_focus->on_draw( has_focus );
            KTUIDlgPushEvent( self, ktuidlg_event_focus_lost, has_focus->id, 0, 0, NULL );
        }

        w->focused = true;
        if ( w->on_draw != NULL )
            w->on_draw( w );
        self->focused = w->id;
        KTUIDlgPushEvent( self, ktuidlg_event_focus, w->id, 0, 0, NULL );

        /* flush it all out... */
        rc = KTUIFlush ( self->tui, false );
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgSetFocus( struct KTUIDlg * self, uint32_t id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        if ( id == 0 )
            id = KTUIDlgFirstFocus( self );
        if ( id > 0 )
        {
            KTUIWidget * gets_focus = KTUIDlgGetWidgetById( self, id );
            if ( gets_focus != NULL )
                rc = KTUIDlgSetFocusTo( self, gets_focus );
        }
    }
    return rc;
}


static uint32_t KTUIDlgGetNextWidget( struct KTUIDlg * self, uint32_t id, bool forward )
{
    uint32_t res = 0;
    KTUIWidget * w = KTUIDlgGetWidgetById( self, id );
    if ( w != NULL )
    {
        uint32_t n = VectorLength( &self->widgets );

        int32_t vidx = w->vector_idx;
        if ( forward )
        {
            while( ( uint32_t )vidx < n && res == 0 )
            {
                vidx++;
                w = VectorGet ( &self->widgets, vidx );
                if ( w != NULL && w->can_focus )
                    res = w->id;
            }
            if ( res == 0 )
                res = KTUIDlgFirstFocus( self );
        }
        else
        {
            while( vidx > 0 && res == 0 )
            {
                vidx--;
                w = VectorGet ( &self->widgets, vidx );
                if ( w != NULL && w->can_focus )
                    res = w->id;
            }
            if ( res == 0 )
                res = KTUIDlgLastFocus( self );
        }
    }
    return res;
}


LIB_EXPORT rc_t CC KTUIDlgMoveFocus( struct KTUIDlg * self, bool forward )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        uint32_t to_focus = KTUIDlgGetNextWidget( self, self->focused, forward );
        rc = KTUIDlgSetFocus( self, to_focus );
    }
    return rc;
}


LIB_EXPORT bool CC KTUIDlgFocusValid( struct KTUIDlg * self )
{
    if ( self == NULL )
        return false;
    else
        return ( KTUIDlgGetWidgetById( self, self->focused ) != NULL );
}


LIB_EXPORT uint32_t CC KTUIDlgGetFocusId( struct KTUIDlg * self )
{
    if ( self == NULL )
        return 0;
    else
        return self->focused;
}

static KTUIWidget * KTUIDlgWidgetAtPoint( struct KTUIDlg * self, tui_point * relative_point )
{
    KTUIWidget * res = NULL;
    uint32_t i, n = VectorLength( &self->widgets );
    for ( i = 0; i < n && res == NULL; ++i )
    {
        KTUIWidget * w = VectorGet ( &self->widgets, i );
        if ( w != NULL && w -> visible && w -> wtype != KTUIW_label )
        {
            uint32_t x_left  = w -> r . top_left . x;
            uint32_t x_right = w -> r . top_left . x + w -> r . w;
            if ( relative_point -> x >= x_left && relative_point-> x < x_right )
            {
                uint32_t y_top    = w->r.top_left.y;
                uint32_t y_bottom = w->r.top_left.y + w->r.h;
                if ( relative_point->y >= y_top && relative_point->y < y_bottom )
                    res = w;
            }
        }
    }
    return res;
}

LIB_EXPORT void KTUIDlgEnableCursorNavigation( struct KTUIDlg * self, bool enabled )
{
    if ( self != NULL )
        self -> cursor_navigation = enabled;
}

/* ****************************************************************************************** */


static rc_t DlgEventHandler( struct KTUIDlg * self, tui_event * event )
{
    rc_t rc = 0;
    if ( event -> event_type == ktui_event_kb )
    {
        if ( self -> cursor_navigation )
        {
            switch( event -> data . kb_data . code )
            {
                case ktui_tab   : ;
                case ktui_down  : ;
                case ktui_right : rc = KTUIDlgMoveFocus( self, true ); break;

                case ktui_shift_tab : ;
                case ktui_up    : ;
                case ktui_left  : rc = KTUIDlgMoveFocus( self, false ); break;
            }
        }
        else
        {
            switch( event -> data . kb_data . code )
            {
                case ktui_tab   : rc = KTUIDlgMoveFocus( self, true ); break;
                case ktui_shift_tab : rc = KTUIDlgMoveFocus( self, false ); break;
            }
        }
    }
    else if ( event->event_type == ktui_event_mouse )
    {
        tui_point p;
        KTUIWidget * w;

        p . x = event -> data . mouse_data . x;
        p . y = event -> data . mouse_data . y;
        KTUIDlgRelativePoint( self, &p );
        w = KTUIDlgWidgetAtPoint( self, &p );
        if ( w != NULL )
        {
            if ( w -> can_focus )
                rc = KTUIDlgSetFocusTo( self, w );
            if ( rc == 0 && w->on_event != NULL )
            {
                /* now send the mouse-event with coordinates relative to the widget
                   to this widget */

                tui_event m_event;
                m_event . event_type = ktui_event_mouse;
                m_event . data . mouse_data . x = ( p . x - w -> r . top_left . x );
                m_event . data . mouse_data . y = ( p . y - w -> r . top_left . y );
                m_event . data . mouse_data.button = event -> data . mouse_data . button;
                w->on_event( w, &m_event, false );
            }
        }
    }
    return rc;
}

static char hotkey( const char * s )
{
    char res = 0;
    if ( s != NULL )
    {
        size_t s_cap;
        if ( string_measure( s, &s_cap ) > 0 )
        {
            char * ampersand = string_chr ( s, s_cap, '&' );
            if ( ampersand != NULL )
            {
                ampersand++;
                res = tolower( *ampersand );
            }
        }
    }
    return res;
}

static KTUIWidget * GetWidgetByHotKey( struct KTUIDlg * self, tui_event * event )
{
    KTUIWidget * res = NULL;
    if ( event -> event_type == ktui_event_kb && 
         event -> data . kb_data . code == ktui_alpha )
    {
        uint32_t i, n = VectorLength( &self->widgets );
        for ( i = 0; i < n && res == NULL; ++i )
        {
            KTUIWidget * w = VectorGet ( &self->widgets, i );
            if ( w != NULL && w -> visible )
            {
                char c = hotkey( w -> caption );
                if ( c > 0 && c == tolower( event -> data . kb_data . key ) )
                    res = w;
            }
        }
    }
    return res;
}

LIB_EXPORT rc_t CC KTUIDlgHandleEvent( struct KTUIDlg * self, tui_event * event )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( event == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    {
        if ( self -> menu_active )
        {
            bool redraw = KTUI_Menu_Event( self->menu, self, event );
            if ( redraw )
                rc = KTUIDlgDraw( self, false );
        }
        else
        {
            bool handled = false;

            KTUIWidget * w = KTUIDlgGetWidgetById( self, self -> focused );
            if ( w != NULL &&
                 w -> on_event != NULL &&
                 event -> event_type == ktui_event_kb )
            {
                handled = w -> on_event( w, event, false );
                if ( handled )
                    event->event_type = ktui_event_none;
            }

            if ( !handled )
            {
                w = GetWidgetByHotKey( self, event );
                if ( w != NULL && w -> on_event != NULL )
                    handled = w -> on_event( w, event, true );
                if ( handled )
                    event->event_type = ktui_event_none;
            }
            
            if ( !handled )
                rc = DlgEventHandler( self, event );
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIDlgGet ( struct KTUIDlg * self, tuidlg_event * event )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( event == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    {
        tuidlg_event * ev = dlg_event_ring_get( &self->events );
        if ( ev == NULL )
        {
            event->event_type = ktuidlg_event_none;
        }
        else
        {
            copy_dlg_event( ev, event );
            dlg_event_ring_put_to_stock( &self->events, ev );
        }
    }
    return rc;
}
