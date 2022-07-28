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
#include <tui/tui_dlg.h>
#include "tui_widget.h"

void draw_tabhdr( struct KTUIWidget * w )
{
    tui_rect r;
    rc_t rc = KTUIDlgAbsoluteRect ( w->dlg, &r, &w->r );
    if ( rc == 0 )
    {
        tui_ac ac;
        rc = GetWidgetAc( w, ktuipa_label, &ac );
        if ( rc == 0 )
        {
            struct KTUI * tui = w->tui;
            if ( tui != NULL )
            {
                uint32_t x = r.top_left.x;
                uint32_t y = r.top_left.y;
                
                rc = draw_background( tui, w -> focused, &( r . top_left ), r. w, r . h, ac . bg );
                
                //rc = DlgPaint( tui, x, y, r.w, r.h, ac.bg );
                if ( rc == 0 && w->caption != NULL )
                {
                    tui_ac hl_ac;   // the highlighted style
                    rc = GetWidgetHlAc( w, ktuipa_label, &hl_ac );
                    if ( rc == 0 )
                        draw_highlighted( tui, x + 2, y, r.w - 3, &ac, &hl_ac, w->caption );
                }
            }
        }
    }
}

bool event_tabhdr( struct KTUIWidget * w, tui_event * event, bool hotkey )
{
    bool res = hotkey;
    if ( !res && event -> event_type == ktui_event_kb )
    {
        switch( event -> data . kb_data . code )
        {
            case ktui_enter : res = true; break;
            case ktui_alpha : res = event -> data . kb_data . key == ' '; break;
        }
    }

    if ( res )
        KTUIDlgPushEvent( w -> dlg, ktuidlg_event_select, w -> id, 0, 0, NULL );
    return res;
}
