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

#define  IL_CUR_POS 0       /* cursor-position */
#define  IL_OFFSET 1        /* offset in case string is longer than visible windows */
#define  IL_INS_MODE 2      /* 0...insert-mode, 1...overwrite-mode */
#define  IL_ALPHA_MODE 3    /* 0...alpha/num-mode, 1...num-mode */

void init_inputline( struct KTUIWidget * w )
{
    if ( w->txt != NULL )
    {
        w->ints[ IL_CUR_POS ] = string_measure ( w->txt, NULL ); /* cursor at the end of it... */
        if ( w->ints[ IL_CUR_POS ] > ( w->r.w - 2 ) )
            w->ints[ IL_OFFSET ] = ( w->ints[ IL_CUR_POS ] - ( w->r.w - 2 ) + 1 );
        else
            w->ints[ IL_OFFSET ] = 0;
    }
    w -> ints[ IL_INS_MODE ] = 0;
    w -> ints[ IL_ALPHA_MODE ] = 0;
}

void set_inputline_alpha_mode( struct KTUIWidget * w, uint32_t alpha_mode )
{
    w->ints[ IL_ALPHA_MODE ] = alpha_mode;
}

static rc_t draw_inputline_focused( struct KTUI * tui, tui_rect * r, const tui_ac * ac, const tui_ac * hint_ac,
                                    const char * s, uint32_t cursor_pos, uint32_t offset, uint32_t ins_mode )
{
    const char * txt = &( s[ offset ] );
    uint32_t l = string_measure ( s, NULL );
    /* draw the visible part of the string */
    rc_t rc = DlgWrite( tui, r->top_left.x + 1, r->top_left.y, ac, txt, r->w - 2 );
    if ( rc == 0 )
    {
        /* draw the cursor */
        uint32_t x = r->top_left.x + 1 + cursor_pos - offset;
        if ( x < ( r->top_left.x + ( r->w - 1 ) ) && x >= r->top_left.x )
        {
            if ( cursor_pos >= l )
                rc = DlgWrite( tui, x, r->top_left.y, ac, "_", 1 );
            else
            {
                char tmp[ 2 ];
                tui_ac ac2;

                tmp[ 0 ] = s[ cursor_pos ];
                tmp[ 1 ] = 0;
                copy_ac( &ac2, ac );
                ac2.attr |= KTUI_a_underline;
                if ( ins_mode != 0 )
                    ac2.attr |= KTUI_a_inverse;
                rc = DlgWrite( tui, x, r->top_left.y, &ac2, tmp, 1 );
            }
        }
    }

    /* draw the hints that the content is longer than the visible part */
    if ( rc == 0 && ( l > ( r->w - 2 ) ) )
    {
        if ( offset > 0 )
            rc = DlgWrite( tui, r->top_left.x, r->top_left.y, hint_ac, "<", 1 );
        if ( rc == 0 && ( l - offset ) > ( r->w - 2 ) )
            rc = DlgWrite( tui, r->top_left.x + r->w - 1, r->top_left.y, hint_ac, ">", 1 );
    }
    return rc;
}


static rc_t draw_inputline_normal( struct KTUI * tui, tui_rect * r, const tui_ac * ac,
                                   const char * s, uint32_t offset )
{
    const char * txt = &( s[ offset ] );
    return DlgWrite( tui, r->top_left.x + 1, r->top_left.y, ac, txt, r->w - 2 );
}


void draw_inputline( struct KTUIWidget * w )
{
    tui_rect r;
    rc_t rc = KTUIDlgAbsoluteRect ( w->dlg, &r, &w->r );
    if ( rc == 0 )
    {
        tui_ac ac;
        rc = GetWidgetAc( w, ktuipa_input, &ac );
        if ( rc == 0 )
        {
            rc = draw_background( w->tui, w->focused, &r.top_left, r.w, r.h, ac.bg );
            if ( rc == 0 && w->txt != NULL )
            {
                if ( w->focused )
                {
                    const tui_ac * ac_hint = GetWidgetPaletteEntry ( w, ktuipa_input_hint );
                    rc = draw_inputline_focused( w->tui, &r, &ac, ac_hint, w->txt,
                                            (uint32_t) w->ints[ IL_CUR_POS ],
                                            (uint32_t) w->ints[ IL_OFFSET ],
                                            (uint32_t) w->ints[ IL_INS_MODE ] );
                }
                else
                    rc = draw_inputline_normal( w->tui, &r, &ac, w->txt, (uint32_t) w->ints[ IL_OFFSET ] );
            }
        }
    }
}


static bool always_handle_these_keys( tui_event * event, uint32_t h )
{
	bool res = false;
	if ( event->event_type == ktui_event_kb )
	{
		switch( event->data.kb_data.code )
		{
			case ktui_left  :
			case ktui_right : res = true; break;
			case ktui_up    :
			case ktui_down  : res = ( h > 1 ); break;
		}
	}
	return res;
}

static void clip_cursor_pos( struct KTUIWidget * w )
{
    uint32_t txt_len = string_measure ( w -> txt, NULL );
    if ( w -> ints[ IL_CUR_POS ] > txt_len )
        w -> ints[ IL_CUR_POS ] = txt_len;
}

bool event_inputline( struct KTUIWidget * w, tui_event * event, bool hotkey )
{
    bool res;

    lp_context lp;
    lp . line = w -> txt;
    lp . max_len = w -> txt_length;
    lp . visible = ( w -> r . w - 2 );
    lp . cur_pos = &( w -> ints[ IL_CUR_POS ] );
    lp . offset = &( w -> ints[ IL_OFFSET ] );
    lp . ins_mode = &( w -> ints[ IL_INS_MODE ] );
    lp . alpha_mode = &( w -> ints[ IL_ALPHA_MODE ] );
    lp . content_changed = false;
    res = lp_handle_event( &lp, event );

    if ( res )
    {
        if ( lp . content_changed )
            SetWidgetChanged ( w, true );
        RedrawWidgetAndPushEvent( w, ktuidlg_event_changed, w -> ints[ IL_CUR_POS ], 0, NULL );
    }
    else if ( event->event_type == ktui_event_kb && event->data.kb_data.code == ktui_enter )
    {
        KTUIDlgPushEvent( w->dlg, ktuidlg_event_select, w->id, 0, 0, NULL );
        res = true;
    }

	if ( !res )
		res = always_handle_these_keys( event, w->r.h );

    return res;
}

void set_carret_pos_input_line( struct KTUIWidget * w, size_t pos )
{
    w -> ints[ IL_CUR_POS ] = pos;
    clip_cursor_pos( w );
    draw_inputline( w );
}
