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

#include "screen.h"
#include "tui-priv.h"
#include <tui/tui.h>
#include <sysalloc.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


void CC clr_tui_screen( tui_screen * screen, const tui_ac * v, bool dirty )
{
    int line_idx;
    for ( line_idx = 0; line_idx < LINES_PER_SCREEN; ++line_idx )
    {
        int column;
        tui_line * line = &screen->lines[ line_idx ];
        memset( &line->chars[ 0 ], ' ', CHR_PER_LINE );
        for ( column = 0; column < CHR_PER_LINE; ++column )
        {
            tui_ac * ac = &line->ac[ column ];
            ac->fg = v->fg;
            ac->bg = v->bg;
            ac->attr = v->attr;
            line->dirty[ column ] = dirty;
        }
        line->line_dirty = dirty;
    }
    screen->dirty = dirty;
}


void CC print_into_screen( tui_screen * screen, int x, int y, const tui_ac * v,
                           const char * s, uint32_t l )
{
    if ( y < LINES_PER_SCREEN && x < CHR_PER_LINE )
    {
        tui_line * line = &screen->lines[ y ];
        uint32_t column, idx, sl = string_measure ( s, NULL );
        bool any_chars_dirty = false;
        if ( l > CHR_PER_LINE ) l =CHR_PER_LINE;
        if ( x + l > CHR_PER_LINE ) l = ( CHR_PER_LINE - x );
        if ( sl > l ) sl = l;
        if ( x + sl > CHR_PER_LINE ) sl = ( CHR_PER_LINE - x );

        for ( column = x, idx = 0; idx < sl; ++column, ++idx )
        {
            tui_ac * ac = &line->ac[ column ];
            if ( ac->attr != v->attr ||
                 ac->fg != v->fg ||
                 ac->bg != v->bg ||
                 line->chars[ column ] != s[ idx ] )
            {
                ac->attr = v->attr;
                ac->fg = v->fg;
                ac->bg = v->bg;
                line->chars[ column ] = s[ idx ];
                line->dirty[ column ] = true;
                any_chars_dirty = true;
            }
        }

        if ( sl < l )
        {
            for ( column = x + sl, idx = sl; idx < l; ++column, ++idx )
            {
                tui_ac * ac = &line->ac[ column ];
                if ( ac->attr != v->attr ||
                     ac->fg != v->fg ||
                     ac->bg != v->bg ||
                     line->chars[ column ] != ' ' )
                {
                    ac->attr = v->attr;
                    ac->fg = v->fg;
                    ac->bg = v->bg;
                    line->chars[ column ] = ' ';
                    line->dirty[ column ] = true;
                    any_chars_dirty = true;
                }
            }
        }

        if ( any_chars_dirty )
        {
            line->line_dirty = true;
            screen->dirty = true;
        }
    }
}


void CC paint_into_screen( tui_screen * screen, const tui_rect * r,
                           const tui_ac * v, const char c )
{
    int x = r->top_left.x;
    int y = r->top_left.y;
    if ( y < LINES_PER_SCREEN && x < CHR_PER_LINE )
    {
        int line_idx, h1 = r->h;
        if ( ( y + r->h ) > LINES_PER_SCREEN ) h1 = ( LINES_PER_SCREEN - y );
        for ( line_idx = 0; line_idx < h1; ++line_idx )
        {
            int column, w1 = r->w;
            tui_line * line = &screen->lines[ y + line_idx ];
            if ( ( x + r->w ) > CHR_PER_LINE ) w1 = ( CHR_PER_LINE - x );
            for ( column = 0; column < w1; ++column )
            {
                int col_idx = x + column;
                tui_ac * ac = &line->ac[ col_idx ];
                if ( ac->attr != v->attr ||
                     ac->fg != v->fg ||
                     ac->bg != v->bg ||
                     line->chars[ col_idx ] != c )
                {
                    ac->attr = v->attr;
                    ac->fg = v->fg;
                    ac->bg = v->bg;
                    line->chars[ col_idx ] = c;
                    line->dirty[ col_idx ] = true;
                }
            }
            line->line_dirty = true;
        }
        screen->dirty = true;
    }
}


void CC send_tui_screen( tui_screen * screen, int n_lines, int n_cols )
{
    if ( screen->dirty )
    {
        int line_idx;
        for ( line_idx = 0; line_idx < n_lines; ++line_idx )
        {
            tui_line * line = &screen->lines[ line_idx ];
            if ( line->line_dirty )
            {
                tui_ac curr = { -1, -1, -1 };
                int column, start = -1;
                for ( column = 0; column < n_cols; ++column )
                {
                    if ( start < 0 )
                    {
                        if ( line->dirty[ column ] )
                            start = column;
                    }
                    else
                    {
                        tui_ac * ac = &line->ac[ start ];
                        tui_ac * cc = &line->ac[ column ];
                        if ( !line->dirty[ column ] ||
                              ac->fg != cc->fg ||
                              ac->bg != cc->bg || 
                              ac->attr != cc->attr )
                        {
                            int count = ( column - start );
                            /* in systui.c ==> platform specific */
                            tui_send_strip( start, line_idx, count,
                                            &curr, ac, &line->chars[ start ] );

                            if ( line->dirty[ column ] )
                                start = column;
                            else
                                start = -1;
                        }
                    }
                    line->dirty[ column ] = false;
                }

                if ( start >= 0 )
                {
                    int count = ( n_cols - start );
                    /* in systui.c ==> platform specific */
                    tui_send_strip( start, line_idx, count,
                                    &curr, &line->ac[ start ], &line->chars[ start ] );
                }
                line->line_dirty = false;
            }
        }
        screen->dirty = false;
    }
}
