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
#include <tui/tui.h>
#include <klib/refcount.h>
#include <klib/rc.h>
#include <klib/log.h>

#include "eventring.h"
#include "screen.h"
#include "tui-priv.h"

#include <sysalloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* this is the generic - not platform specific code of KTUI */

/* static const char tuimanager_classname [] = "TUIManager"; */
static const char tui_classname [] = "TUI_Implementation";

LIB_EXPORT rc_t CC KTUIMake ( const KTUIMgr * mgr, struct KTUI ** self, uint32_t timeout )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUI * tui = malloc( sizeof * tui );
        if ( tui == NULL )
            rc = RC( rcApp, rcAttr, rcCreating, rcMemory, rcExhausted );
        else
        {
            tui_ac v = { KTUI_a_none, KTUI_c_black, KTUI_c_green };
            event_ring_init( &tui->ev_ring );

            clr_tui_screen( &tui->screen, &v, false );

            rc = KTUI_Init_platform( tui ); /* call the platform specific init */
            if ( rc == 0 )
            {
                tui->mgr = mgr;
                tui->timeout = timeout;
                if ( rc == 0 )
                    KRefcountInit( &tui->refcount, 1, "TUI", "make", tui_classname );
            }
            if ( rc != 0 )
                free( ( void * ) tui );
            else
                ( * self ) = tui;
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIAddRef ( const struct KTUI * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        switch ( KRefcountAdd( &self->refcount, tui_classname ) )
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


static rc_t CC KTUIDestroy ( struct KTUI * self )
{
    KTUI_Destroy_platform( self->pf );

    event_ring_destroy( &self->ev_ring );
    KRefcountWhack( &self->refcount, tui_classname );
    free( self );
    return 0;
}


/* not platform specific */
LIB_EXPORT rc_t CC KTUIRelease ( const struct KTUI * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        switch ( KRefcountDrop( &self->refcount, tui_classname ) )
        {
        case krefOkay :
        case krefZero :
            break;
        case krefWhack :
            rc = KTUIDestroy( ( struct KTUI * )self );
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


LIB_EXPORT rc_t CC KTUISetTimeout ( struct KTUI * self, uint32_t timeout )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
        self->timeout = timeout;

    return rc;
}


LIB_EXPORT rc_t CC KTUIPrint( struct KTUI * self, const tui_point * p,
                              const tui_ac * ac, const char * s, uint32_t l )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( ac == NULL || p == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
        /* in screen.c */
        print_into_screen( &self->screen, p->x, p->y, ac, s, l );

    return rc;
}


LIB_EXPORT rc_t CC KTUIRect ( struct KTUI * self, const tui_rect * r,
                              const tui_ac * ac, const char c )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( ac == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
    {
        if ( r != NULL )
            paint_into_screen( &self->screen, r, ac, c );
        else
            clr_tui_screen( &self->screen, ac, true );
    }

    return rc;
}


LIB_EXPORT rc_t CC KTUIFlush ( struct KTUI * self, bool forced )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        if ( forced )
        {
            tui_ac ac;
            set_ac( &ac, KTUI_a_none, KTUI_c_white, KTUI_c_white );
            clr_tui_screen( &self->screen, &ac, true );
        }
		
		/* this is platform specific! in systui.c */
        send_tui_screen( &self->screen, self->lines, self->cols );
    }

    return rc;
}


/* not platform specific, except the call to platform specific read_from_stdin() */
LIB_EXPORT rc_t CC KTUIGet ( struct KTUI * self, tui_event * event )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( event == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
    {
        tui_event * ev;
        read_from_stdin( self, self->timeout ); /* that is the platform specific state-machine */
        ev = event_ring_get( &self->ev_ring );
        if ( ev == NULL )
        {
            event->event_type = ktui_event_none;
        }
        else
        {
            copy_event( ev, event );
            event_ring_put_to_stock( &self->ev_ring, ev );
        }
    }
    return rc;
}


/* not platform specific, just returns values from self-struct */
LIB_EXPORT rc_t CC KTUIGetExtent ( struct KTUI * self, int * cols, int * lines )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( lines == NULL || cols == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
    {
        *lines = self->lines;
        *cols = self->cols;
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIClrScr( struct KTUI * self, KTUI_color bg )
{
    tui_ac ac;
    set_ac( &ac, KTUI_a_none, bg, bg );
    clr_tui_screen( &self->screen, &ac, true );
    return 0;
}


LIB_EXPORT void set_ac( tui_ac * dst, KTUI_attrib attr, KTUI_color fg, KTUI_color bg )
{
    if ( dst != NULL )
    {
        dst->attr = attr;
        dst->fg = fg;
        dst->bg = bg;
    }
}


LIB_EXPORT void copy_ac( tui_ac * dst, const tui_ac * src )
{
    if ( dst != NULL && src != NULL )
    {
        dst->attr = src->attr;
        dst->fg = src->fg;
        dst->bg = src->bg;
    }
}


LIB_EXPORT void inverse_ac( tui_ac * dst, const tui_ac * src )
{
    if ( dst != NULL && src != NULL )
    {
        dst->attr = src->attr;
        dst->fg = src->bg;
        dst->bg = src->fg;
    }
}


LIB_EXPORT void set_rect( tui_rect * dst, int x, int y, int w, int h )
{
    if ( dst != NULL )
    {
        dst->top_left.x = x;
        dst->top_left.y = y;
        dst->w = w;
        dst->h = h;
    }
}


LIB_EXPORT void copy_rect( tui_rect * dst, const tui_rect * src )
{
    if ( dst != NULL && src != NULL )
    {
        dst->top_left.x = src->top_left.x;
        dst->top_left.y = src->top_left.y;
        dst->w = src->w;
        dst->h = src->h;
    }
}

void CC put_window_event( struct KTUI * self, int cols, int lines )
{
    tui_event * event = event_ring_get_from_stock_or_make( &self->ev_ring );
    if ( event != NULL )
    {
        event->event_type = ktui_event_window;
        event->data.win_data.w = cols;
        event->data.win_data.h = lines;
        event_ring_put( &self->ev_ring, event );
    }
}


void CC put_kb_event( struct KTUI * self, int key, KTUI_key code )
{
    tui_event * event = event_ring_get_from_stock_or_make( &self->ev_ring );
    if ( event != NULL )
    {
        event->event_type = ktui_event_kb;
        event->data.kb_data.key = key;
        event->data.kb_data.code = code;
        event_ring_put( &self->ev_ring, event );
    }
}

void CC put_mouse_event( struct KTUI * self, unsigned int x, unsigned int y, 
                         KTUI_mouse_button button, KTUI_mouse_action action, uint32_t raw_event )
{
    tui_event * event = event_ring_get_from_stock_or_make( &self->ev_ring );
    if ( event != NULL )
    {
        event->event_type = ktui_event_mouse;
        event->data.mouse_data.button = button;
        event->data.mouse_data.action = action;	
        event->data.mouse_data.x = x;
        event->data.mouse_data.y = y;
		event->data.mouse_data.raw_event = raw_event;
        event_ring_put( &self->ev_ring, event );
    }
}


LIB_EXPORT bool CC is_alpha_key( tui_event * event, char c )
{
    return ( event != NULL &&
              event->event_type == ktui_event_kb &&
              event->data.kb_data.code == ktui_alpha &&
              event->data.kb_data.key == c );
}


LIB_EXPORT bool CC is_key_code( tui_event * event, KTUI_key k )
{
    return ( event != NULL &&
              event->event_type == ktui_event_kb &&
              event->data.kb_data.code == k );
}
