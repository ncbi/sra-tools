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

#define DL_V_OFFSET 0
#define DL_H_OFFSET 1

void draw_list( struct KTUIWidget * w )
{
    tui_rect r;
    rc_t rc = KTUIDlgAbsoluteRect ( w->dlg, &r, &w->r );
    if ( rc == 0 )
    {
        tui_ac ac;
        rc = GetWidgetAc( w, ktuipa_list, &ac );
        if ( rc == 0 )
            rc = draw_background( w->tui, w->focused, &r.top_left, r.w, r.h, ac.bg );
        if ( rc == 0 && w->strings != NULL )
        {
            uint32_t count;
            rc = VNameListCount ( w->strings, &count );
            if ( rc == 0 )
            {
                uint32_t idx, offset = ( uint32_t )w->ints[ DL_V_OFFSET ];
                for ( idx = 0; idx < w->r.h; ++idx )
                {
                    uint32_t nl_idx = offset + idx;
                    if ( nl_idx < count )
                    {
                        const char * s;
                        rc = VNameListGet ( w->strings, nl_idx, &s );
                        if ( rc == 0 && s != NULL )
                        {
                            uint32_t length = string_measure ( s, NULL );
                            if ( w->ints[ DL_H_OFFSET ] < length )
                                s += w->ints[ DL_H_OFFSET ];
                            else
                                s = " ";
                            if ( nl_idx == w->selected_string )
                            {
                                tui_ac ac2;
                                copy_ac( &ac2, &ac );
                                ac2.attr = KTUI_a_inverse;
                                rc = DlgWrite( w->tui, r.top_left.x + 1, r.top_left.y + idx,
                                               &ac2, s, r.w - 2 );
                            }
                            else
                                rc = DlgWrite( w->tui, r.top_left.x + 1, r.top_left.y + idx,
                                               &ac, s, r.w - 2 );
                        }
                    }
                }

                /* draw the scrollbar on the right of the list */
                if ( rc == 0 && count > r.h )
                    rc = DrawVScroll( w->tui, &r, count, w->selected_string, KTUI_c_gray, ac.fg );
            }
        }
    }
}


static uint32_t longest_string( VNamelist * list, uint32_t count )
{
	rc_t rc = 0;
	uint32_t idx, res = 0;
	for ( idx = 0; idx < count && rc == 0; ++idx )
	{
		const char * s;
		rc = VNameListGet ( list, idx, &s );
		if ( rc == 0 && s != NULL )
		{
			uint32_t length = string_measure ( s, NULL );
			if ( length > res ) res = length;
		}
	}
	return res;
}


static bool always_handle_these_keys( tui_event * event )
{
	bool res = false;
	if ( event->event_type == ktui_event_kb )
	{
		switch( event->data.kb_data.code )
		{
			case ktui_left : ;
			case ktui_right : ;
			case ktui_up : ;
			case ktui_down : res = true; break;
		}
	}
	return res;
}


bool event_list( struct KTUIWidget * w, tui_event * event, bool hotkey )
{
    bool res = false;
    if ( w->strings != NULL )
    {
        uint32_t count;
        rc_t rc = VNameListCount ( w->strings, &count );
        if ( rc == 0 )
        {
            gen_context ctx;
            uint64_t prev_selection;
            ctx.count = count;
            ctx.visible = w->r.h;
            ctx.curr = &w->selected_string;
            ctx.offset = &( w->ints[ DL_V_OFFSET ] );

            prev_selection = *ctx.curr;
            res = list_handle_event( &ctx, event, 0 );
            if ( res )
			{
                RedrawWidgetAndPushEvent( w, ktuidlg_event_changed, *ctx.curr, prev_selection, (void *)w->strings );
			}
            else if ( event->event_type == ktui_event_kb )
            {
                switch( event->data.kb_data.code )
                {
                    case ktui_enter :  RedrawWidgetAndPushEvent( w, ktuidlg_event_select, *ctx.curr, 0, (void *)w->strings );
										res = true;
                                        break;

                    case ktui_left :   if ( w->ints[ DL_H_OFFSET ] > 0 )
                                        {
                                            w->ints[ DL_H_OFFSET ]--;
                                            RedrawWidget( w );
                                        }
										res = true;
                                        break;

                    case ktui_right :  if ( longest_string( w->strings, count ) > w->r.w )
										{
											w->ints[ DL_H_OFFSET ]++;
											RedrawWidget( w );
										}
										res = true;
                                        break;

                }
            }
        }
    }

	if ( !res )
		res = always_handle_these_keys( event );

    return res;
}
