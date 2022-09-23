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

void draw_spinedit( struct KTUIWidget * w )
{
    tui_rect r;
    rc_t rc = KTUIDlgAbsoluteRect ( w->dlg, &r, &w->r );
    if ( rc == 0 )
    {
        tui_ac ac;
        rc = GetWidgetAc( w, ktuipa_spinedit, &ac );
        if ( rc == 0 )
        {
            tui_ac ac2;
            set_ac( &ac2, ac.attr, ac.bg, ac.fg );   /* reverse the colors */

            if ( w->focused )
            {
                rc = DlgPaint( w->tui, r.top_left.x, r.top_left.y, 1, r.h, KTUI_c_red );
                r.top_left.x += 1;
                r.w -= 1;
            }

            if ( rc == 0 )
                rc = DlgWrite( w->tui, r.top_left.x, r.top_left.y, &ac2, " + ", 3 );
            if ( rc == 0 )
                rc = DlgWrite( w->tui, r.top_left.x + r.w - 3, r.top_left.y, &ac2, " - ", 3 );

            if ( rc == 0 )
            {
                char txt[ 32 ];
                size_t num_writ;
                string_printf ( txt, sizeof txt, &num_writ, " %ld", w->int64_value );
                rc = DlgWrite( w->tui, r.top_left.x + 3, r.top_left.y, &ac, txt, r.w - 6 );
            }
        }
    }
}


static bool spinedit_inc_dec( struct KTUIWidget * w, int64_t by )
{
    SetWidgetInt64Value ( w, GetWidgetInt64Value ( w ) + by );
    return true;
}


static bool spinedit_set( struct KTUIWidget * w, int64_t v )
{
    SetWidgetInt64Value ( w, v );
    return true;
}


bool event_spinedit( struct KTUIWidget * w, tui_event * event, bool hotkey )
{
    bool res = false;

    if ( event->event_type == ktui_event_kb )
    {
        switch( event->data.kb_data.code )
        {
            case ktui_home  : res = spinedit_set( w, w->int64_min ); break;
            case ktui_end   : res = spinedit_set( w, w->int64_max ); break;
            case ktui_up    : res = spinedit_inc_dec( w, +1 ); break;
            case ktui_down  : res = spinedit_inc_dec( w, -1 ); break;
            case ktui_pgup  : res = spinedit_inc_dec( w, +10 ); break;
            case ktui_pgdn  : res = spinedit_inc_dec( w, -10 ); break;
            case ktui_alpha : switch( event->data.kb_data.key )
                                {
                                    case '+' : res = spinedit_inc_dec( w, +1 ); break;
                                    case '-' : res = spinedit_inc_dec( w, -1 ); break;
                                }
        }
    }
    else if ( event->event_type == ktui_event_mouse )
    {
        uint32_t x = event->data.mouse_data.x;
        if ( x < 4 )
            res = spinedit_inc_dec( w, +1 );
        else if ( x > ( w->r.w - 4 ) ) 
            res = spinedit_inc_dec( w, -1 );
    }

    if ( res )
        RedrawWidgetAndPushEvent( w, ktuidlg_event_changed, w->int64_value, 0, NULL );

    return res;
}
