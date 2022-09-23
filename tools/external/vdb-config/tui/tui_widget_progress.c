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

#include <math.h>

#if 0
static void calc_percent( struct KTUIWidget * w )
{
    uint64_t curr = *( ( uint64_t * )w->data_0 );
    uint64_t max  = *( ( uint64_t * )w->data_1 );
    w->ints[ 0 ] = 0;   /* int-part of result */
    w->ints[ 1 ] = 0;   /* fract-part of result ( in 1/1000 )*/
    if ( max > 0 && curr > 0 && curr <= max )
    {
        double fractpart, intpart;
        double x = (double)curr;
        x *= 100.0;
        x /= max;
        fractpart = modf ( x, &intpart );
        w->ints[ 0 ] = ( uint64_t )floor( intpart );
        fractpart *= 1000;
        w->ints[ 1 ] = ( uint64_t )floor( fractpart );
    }
}
#endif

void draw_progress( struct KTUIWidget * w )
{
    tui_rect r;
    rc_t rc = KTUIDlgAbsoluteRect ( w->dlg, &r, &w->r );
    if ( rc == 0 )
    {
        tui_ac ac;
        rc = GetWidgetAc( w, ktuipa_progress, &ac );
        if ( rc == 0 )
            rc = draw_background( w->tui, false, &r.top_left, r.w, r.h, ac.bg );
        if ( rc == 0 )
        {
            char txt[ 32 ];
            size_t num_writ;
            tui_ac ac2;
            int64_t xf = 0, x = w->percent;
            
            switch( w->precision )
            {
                case 1 :    xf = x % 10;
                            x /= 10;
                            string_printf ( txt, sizeof txt, &num_writ, " %u.%.01u %%", x, xf );
                            break;

                case 2 :    xf = x % 100;
                            x /= 100;
                            string_printf ( txt, sizeof txt, &num_writ, " %u.%.02u %%", x, xf );
                            break;

                case 3 :    xf = x % 1000;
                            x /= 1000;
                            string_printf ( txt, sizeof txt, &num_writ, " %u.%.03u %%", x, xf );
                            break;

                case 4 :    xf = x % 10000;
                            x /= 10000;
                            string_printf ( txt, sizeof txt, &num_writ, " %u.%.04u %%", x, xf );
                            break;

                default : string_printf ( txt, sizeof txt, &num_writ, " %u %%", x ); break;
            }

            x *= w->r.w;
            x /= 100;

            copy_ac( &ac2, &ac );
            ac2.attr = KTUI_a_inverse;

            if ( x >= num_writ )
            {
                rc = DlgWrite( w->tui, r.top_left.x, r.top_left.y, &ac2, txt, ( uint32_t )num_writ );
                if ( rc == 0 && x > num_writ )
                    rc = DlgPaint( w->tui, r.top_left.x + ( uint32_t )num_writ, r.top_left.y,
                                   ( uint32_t )( x - num_writ ), r.h, ac.fg );
            }
            else
            {
                rc = DlgWrite( w->tui, r.top_left.x, r.top_left.y, &ac2, txt, ( uint32_t )x );
                if ( rc == 0 )
                    rc = DlgWrite( w->tui, r.top_left.x + ( uint32_t )x, r.top_left.y,
                                   &ac, &txt[ x ], ( uint32_t )( num_writ - x ) );
            }
        }
    }
}
