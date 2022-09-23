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
#include <klib/namelist.h>
#include <tui/tui_dlg.h>
#include "tui_widget.h"
#include "line_policy.h"

void draw_radiobox( struct KTUIWidget * w )
{
    tui_rect r;
    rc_t rc = KTUIDlgAbsoluteRect ( w->dlg, &r, &w->r );
    if ( rc == 0 )
    {
        tui_ac ac;
        rc = GetWidgetAc( w, ktuipa_radiobox, &ac );
        if ( rc == 0 )
            rc = draw_background( w->tui, w->focused, &r.top_left, r.w, r.h, ac.bg );
        if ( rc == 0 && w->strings != NULL )
        {
            uint32_t count;
            rc = VNameListCount ( w->strings, &count );
            if ( rc == 0 )
            {
                uint32_t idx, offset = (uint32_t)w->ints[ 0 ];
                for ( idx = 0; idx < w->r.h; ++idx )
                {
                    uint32_t nl_idx = offset + idx;
                    if ( nl_idx < count )
                    {
                        const char * s;
                        rc = VNameListGet ( w->strings, nl_idx, &s );
                        if ( rc == 0 && s != NULL )
                        {
                            size_t num_writ;
                            char txt[ 265 ];
                            if ( nl_idx == w->selected_string )
                                rc = string_printf ( txt, sizeof txt, &num_writ, "(*) %s", s );
                            else
                                rc = string_printf ( txt, sizeof txt, &num_writ, "( ) %s", s );
                            if ( rc == 0 )
                                rc = DlgWrite( w->tui, r.top_left.x + 1, r.top_left.y + idx,
                                               &ac, txt, r.w - 2 );
                        }
                    }
                }

                /* draw the scrollbar on the right of the list */
                if ( rc == 0 && count > r.h )
                {
                    rc = DlgPaint( w->tui, r.top_left.x + r.w - 1,
                                   r.top_left.y, 1, r.h, KTUI_c_gray );
                    if ( rc == 0 && count > 0 )
                    {
                        uint32_t y = r.top_left.y + ( ( ( uint32_t )w->selected_string * r.h ) / count );
                        rc = DlgPaint( w->tui, r.top_left.x + r.w - 1,
                                       y, 1, 1, ac.fg );
                    }
                }
            }
        }
    }
}


bool event_radiobox( struct KTUIWidget * w, tui_event * event, bool hotkey )
{
    bool res = false;

    if ( w->strings != NULL )
    {
        uint32_t count;
        rc_t rc = VNameListCount ( w->strings, &count );
        if ( rc == 0 )
        {
            gen_context ctx;
            ctx.count = count;
            ctx.visible = w->r.h;
            ctx.curr = &w->selected_string;
            ctx.offset = &( w->ints[ 0 ] );

            res = list_handle_event( &ctx, event, 4 );
            if ( res )
            {
                SetWidgetChanged ( w, true );
                RedrawWidgetAndPushEvent( w, ktuidlg_event_changed, *ctx.curr, 0, ( void * )w->strings );
            }
            else if ( event->event_type == ktui_event_kb )
            {
                switch( event->data.kb_data.code )
                {
                    case ktui_enter : res = true; break;
                    case ktui_alpha : res = ( event->data.kb_data.key == ' ' ); break;
                }
                if ( res )
                {
                    SetWidgetChanged ( w, true );
                    RedrawWidgetAndPushEvent( w, ktuidlg_event_select, *ctx.curr, 0, ( void * )w->strings );
                }
            }
        }
    }
    return res;
}
