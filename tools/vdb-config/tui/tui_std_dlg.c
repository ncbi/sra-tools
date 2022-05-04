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

#include <klib/text.h>
#include <klib/rc.h>
#include <klib/printf.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/filetools.h>

#include <tui/tui_dlg.h>

#include <sysalloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static rc_t Std_Dlg_Loop ( struct KTUIDlg * dlg, uint32_t * selection, bool * selected )
{
    rc_t rc = KTUIDlgDraw( dlg, false );
    if ( selected != NULL ) * selected = false;
    if ( rc == 0 )
    {
        struct KTUI * tui = KTUIDlgGetTui( dlg );
        KTUIDlgSetFocus( dlg, 0 );
        do
        {
            tui_event event;
            rc = KTUIGet ( tui, &event );
            if ( rc == 0 )
            {
                rc = KTUIDlgHandleEvent( dlg, &event );
                if ( rc == 0 )
                {
                    tuidlg_event dev;
                    do
                    {
                        rc = KTUIDlgGet ( dlg, &dev );
                        if ( rc == 0 && dev.event_type == ktuidlg_event_select )
                        {
                            if ( selection != NULL )
                                *selection = dev.widget_id;
                            KTUIDlgSetDone ( dlg, true );
                            if ( selected != NULL ) *selected = true;
                        }
                    } while ( rc == 0 && dev.event_type != ktuidlg_event_none && !KTUIDlgGetDone( dlg ) );
                }
                if ( is_alpha_key( &event, 27 ) ) 
                    KTUIDlgSetDone ( dlg, true );
            }
        } while ( rc == 0 && !KTUIDlgGetDone( dlg ) );
    }
    return rc;
}


/* ****************************************************************************************** */


static rc_t make_dlg_with_bg( struct KTUIDlg ** dlg,
                              struct KTUIPalette ** pal,
                              struct KTUI * tui_,
                              struct KTUIDlg * parent,
                              tui_rect * r,
                              KTUI_color bg1,
                              KTUI_color bg2 )
{
    rc_t rc;
    struct KTUI * tui = tui_;
    if ( tui == NULL )
        tui = KTUIDlgGetTui( parent );
    if ( tui == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
    {
        rc = KTUIPaletteMake ( pal );
        if ( rc == 0 )
        {
            KTUIPaletteSet_bg ( *pal, ktuipa_dlg, bg1 );
            KTUIPaletteSet_bg ( *pal, ktuipa_dlg_caption, bg2 );
            rc = KTUIDlgMake ( tui, dlg, parent, *pal, r );
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC TUI_ShowMessage ( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                       const char * txt, uint32_t x, uint32_t y, uint32_t w, KTUI_color bg1, KTUI_color bg2 )
{
    rc_t rc;
    struct KTUIPalette * pal;
    struct KTUIDlg * dlg;
    tui_rect r;

    set_rect( &r, x, y, w, 6 );
    rc = make_dlg_with_bg( &dlg, &pal, tui, parent, &r, bg1, bg2 );
    if ( rc == 0 )
    {
        uint32_t y1 = 1;
        r.w -= 2;

        if ( caption != NULL )
        {
            rc = KTUIDlgSetCaption ( dlg, caption );
            y1++;
        }

        if ( rc == 0 )
            rc = KTUIDlgAddLabel2( dlg, 100, 1, y1, r.w, txt );
        y1 += 2;

        if ( rc == 0 )
            rc = KTUIDlgAddBtn2( dlg, 101, 1, y1, 6, "OK" );

        if ( rc == 0 )
            rc = Std_Dlg_Loop ( dlg, NULL, NULL );

        KTUIDlgRelease ( dlg );
        KTUIPaletteRelease ( pal );
    }
    return rc;
}


/* ****************************************************************************************** */


LIB_EXPORT rc_t CC TUI_YesNoDlg ( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                    const char * question, uint32_t x, uint32_t y, uint32_t w, bool * yes, KTUI_color bg1, KTUI_color bg2 )
{
    rc_t rc;
    struct KTUIPalette * pal;
    struct KTUIDlg * dlg;
    tui_rect r;

    set_rect( &r, x, y, w, 6 );
    rc = make_dlg_with_bg( &dlg, &pal, tui, parent, &r, bg1, bg2 );
    if ( rc == 0 )
    {
        if ( caption != NULL )
            rc = KTUIDlgSetCaption ( dlg, caption );

        if ( rc == 0 )
            rc = KTUIDlgAddLabel2( dlg, 100, 1, 2, r.w - 2, question );

        if ( rc == 0 )
            rc = KTUIDlgAddBtn2( dlg, 101, 1, 4, 10, "Yes" );

        if ( rc == 0 )
            rc = KTUIDlgAddBtn2( dlg, 102, 12, 4, 10, "No" );

        if ( rc == 0 )
        {
            uint32_t selection;
            rc = Std_Dlg_Loop ( dlg, &selection, NULL );
            if ( yes != NULL )
                *yes = ( selection == 101 );
        }

        KTUIDlgRelease ( dlg );
        KTUIPaletteRelease ( pal );
    }
    return rc;
}


/* ****************************************************************************************** */


LIB_EXPORT rc_t CC TUI_ShowFile( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                   const char * filename, tui_rect *r, KTUI_color bg1, KTUI_color bg2 )
{
    struct KTUIPalette * pal;
    struct KTUIDlg * dlg;
    rc_t rc = make_dlg_with_bg( &dlg, &pal, tui, parent, r, bg1, bg2 );
    if ( rc == 0 )
    {
        VNamelist * list = NULL;

        if ( caption != NULL )
            rc = KTUIDlgSetCaption ( dlg, caption );

        if ( rc == 0 )
            rc = VNamelistMake ( &list, 100 );

        if ( rc == 0 )
            rc = LoadFileByNameToNameList( list, filename );

        if ( rc == 0 )
        {
            tui_rect r1;

            set_rect( &r1, 0, 1, r->w, r->h - 2 );
            rc = KTUIDlgAddList ( dlg, 100, &r1 );
            if ( rc == 0 )
                rc = KTUIDlgAddWidgetStrings ( dlg, 100, list );
        }

        if ( rc == 0 )
            rc = KTUIDlgAddBtn2( dlg, 101, 0, r->h - 1, r->w, "OK" );

        if ( rc == 0 )
            rc = Std_Dlg_Loop ( dlg, NULL, NULL );

        VNamelistRelease( list );
        KTUIDlgRelease ( dlg );
        KTUIPaletteRelease ( pal );
    }
    return rc;
}


/* ****************************************************************************************** */


LIB_EXPORT rc_t CC TUI_EditBuffer( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                     char * buffer, size_t buflen, uint32_t x, uint32_t y, uint32_t w, bool * selected, 
                     KTUI_color bg1, KTUI_color bg2 )
{
    rc_t rc;
    struct KTUIPalette * pal;
    struct KTUIDlg * dlg;
    tui_rect r;

    if ( selected != NULL ) *selected = false;
    set_rect( &r, x, y, w, 6 );
    rc = make_dlg_with_bg( &dlg, &pal, tui, parent, &r, bg1, bg2 );
    if ( rc == 0 )
    {
        uint32_t y1 = 1;
        uint32_t selected_id = 0;

        if ( caption != NULL )
        {
            rc = KTUIDlgSetCaption ( dlg, caption );
            y1++;
        }

        if ( rc == 0 )
        {
            tui_rect r1;
            set_rect( &r1, 1, y1, r.w - 2, 1 );
            rc = KTUIDlgAddInput ( dlg, 100, &r1, buffer, buflen );
        }

        y1 += 2;

        if ( rc == 0 )
            rc = KTUIDlgAddBtn2( dlg, 101, 1, y1, 12, "OK" );

        if ( rc == 0 )
            rc = KTUIDlgAddBtn2( dlg, 102, 14, y1, 22, "Cancel (ESC-ESC)" );

        if ( rc == 0 )
            rc = Std_Dlg_Loop ( dlg, &selected_id, selected );

        if ( rc == 0 && ( selected_id == 101 || selected_id == 100 ) )
        {
            const char * s = KTUIDlgGetWidgetText( dlg, 100 );
            if ( s != NULL )
                string_copy ( buffer, buflen, s, string_size ( s ) );
        }
        else
        {
            if ( selected != NULL )
                *selected = false;
        }

        KTUIDlgRelease ( dlg );
        KTUIPaletteRelease ( pal );
    }
    return rc;
}


/* ****************************************************************************************** */

LIB_EXPORT rc_t CC TUI_PickFromList( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
        const VNamelist * list, uint32_t * selection, tui_rect * r, bool * selected,
        KTUI_color bg1, KTUI_color bg2 )
{
    struct KTUIPalette * pal;
    struct KTUIDlg * dlg;
    rc_t rc = make_dlg_with_bg( &dlg, &pal, tui, parent, r, bg1, bg2 );
    if ( selected != NULL ) *selected = false;
    if ( rc == 0 )
    {
        uint32_t y = 1;
        uint32_t w = r->w - 2;

        if ( caption != NULL )
        {
            rc = KTUIDlgSetCaption ( dlg, caption );
            y++;
        }

        if ( rc == 0 )
        {
            tui_rect r1;
            set_rect( &r1, 1, y, w, r->h - 3 );
            rc = KTUIDlgAddRadioBox ( dlg, 100, &r1 );
            if ( rc == 0 )
            {
                uint32_t count, idx;
                rc = VNameListCount ( list, &count );
                for ( idx = 0; rc == 0 && idx < count; ++idx )
                {
                    const char * s;
                    rc = VNameListGet ( list, idx, &s );
                    if ( rc == 0 && s != NULL )
                        rc = KTUIDlgAddWidgetString ( dlg, 100, s );
                }
                if ( rc == 0 && selection != NULL )
                    rc = KTUIDlgSetWidgetSelectedString ( dlg, 100, *selection );
            }
        }

        if ( rc == 0 )
            rc = Std_Dlg_Loop ( dlg, NULL, selected );

        if ( rc == 0 && *selected && selection != NULL )
            *selection = KTUIDlgGetWidgetSelectedString( dlg, 100 );

        KTUIDlgRelease ( dlg );
        KTUIPaletteRelease ( pal );
    }
    return rc;
}
