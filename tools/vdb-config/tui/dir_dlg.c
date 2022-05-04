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

#include <klib/rc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <kfs/directory.h>

#include <vfs/manager.h>
#include <vfs/path.h>

#include <tui/tui_dlg.h>
#include "tui-priv.h"

#include <sysalloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct dir_dlg_data
{
    uint32_t dlg_w, dlg_h;

    /* for the inputline */
    char current_dir_internal[ 4096 ];
    char parent_dir_internal[ 4096 ];
	
    /* a VFS-Manager to do a path-conversion */
    VFSManager * vfs_mgr;

	/* a native KDirectory to fill the path-list */
	KDirectory * dir;

    KTUI_color bg1;
    KTUI_color bg2;
	
    bool in_root;
    bool ok_pressed;
    bool done;
    bool allow_dir_create;

} dir_dlg_data;


enum
{
    ID_CURR = 100,
    ID_LABEL1,
    ID_DIRS,
    ID_B_OK,
    ID_B_CANCEL,
    ID_B_CREATE,
    ID_B_GOTO
};


static void init_dlg_data( dir_dlg_data * data, uint32_t dlg_w, uint32_t dlg_h,
                           char * path, KTUI_color bg1, KTUI_color bg2, bool allow_dir_create )
{
    size_t written;

    data->dlg_w = dlg_w;
    data->dlg_h = dlg_h;

    VFSManagerMake ( &data->vfs_mgr );
	KDirectoryNativeDir ( &data->dir );

    native_to_internal( data->vfs_mgr, path, data->current_dir_internal, sizeof data->current_dir_internal, &written );

	data->in_root = ( ( data->current_dir_internal[ 0 ] == '/' ) && ( data->current_dir_internal[ 1 ] == 0 ) );
	data->parent_dir_internal[ 0 ] = 0;
    data->done = false;
    data->ok_pressed = false;
    data->bg1 = bg1;
    data->bg2 = bg2;
    data->allow_dir_create = allow_dir_create;
}


static void destroy_dlg_data( dir_dlg_data * data )
{
    if ( data->vfs_mgr != NULL )
    {
        VFSManagerRelease ( data->vfs_mgr );
        data->vfs_mgr = NULL;
    }
	if ( data->dir != NULL )
	{
		KDirectoryRelease ( data->dir );
		data->dir = NULL;
	}
}


static bool create_allowed( dir_dlg_data * data )
{
#ifdef WINDOWS
    return !data->in_root;
#else
    return true;
#endif
}

static rc_t PopulateDirDlg ( struct KTUIDlg * dlg, dir_dlg_data * data )
{
    uint32_t x;
    rc_t rc = KTUIDlgAddLabel2( dlg, ID_CURR, 1, 2, data->dlg_w - 2, "" );

    if ( rc == 0 )
        rc = KTUIDlgAddLabel2( dlg, ID_LABEL1, 1, 4, data->dlg_w - 2, "directories:" );

    if ( rc == 0 )
    {
        tui_rect r;
        set_rect( &r, 1, 5, data->dlg_w - 2, data->dlg_h - 8 );
        rc = KTUIDlgAddList ( dlg, ID_DIRS, &r );
    }

    x = 1;
    if ( rc == 0 )
        rc = KTUIDlgAddBtn2( dlg, ID_B_OK, x, data->dlg_h - 2, 12, "OK" );
    x += ( 12 + 1 );

    if ( rc == 0 )
        rc = KTUIDlgAddBtn2( dlg, ID_B_CANCEL, x, data->dlg_h - 2, 22, "Cancel (ESC-ESC)" );
    x += ( 22 + 1 );

    if ( rc == 0 )
        rc = KTUIDlgAddBtn2( dlg, ID_B_GOTO, x, data->dlg_h - 2, 12, "Goto" );
    x += ( 12 + 1 );

    if ( rc == 0 && data->allow_dir_create )
    {
        rc = KTUIDlgAddBtn2( dlg, ID_B_CREATE, x, data->dlg_h - 2, 18, "Create Dir" );
        if ( rc == 0 )
            rc = KTUIDlgSetWidgetVisible ( dlg, ID_B_CREATE, create_allowed( data ) );
    }

	if ( rc == 0 )
		rc = set_native_caption( dlg, data->vfs_mgr, ID_CURR, data->current_dir_internal );
		
    if ( rc == 0 )
        rc = fill_widget_with_dirs( dlg, data->dir, ID_DIRS, data->current_dir_internal, data->parent_dir_internal, true );

    if ( rc == 0 )
        rc = KTUIDlgSetFocus( dlg, ID_DIRS );
    return rc;
}


static void DirDlg_Goto_Parent ( dir_dlg_data * data )
{
	char * right_slash = string_rchr ( data->current_dir_internal, string_size ( data->current_dir_internal ), '/' );
	if ( right_slash != NULL )
	{
		size_t written;

		data->in_root = ( right_slash == data->current_dir_internal );
		if ( data->in_root )
			data->current_dir_internal[ 1 ] = 0;
		else
			*right_slash = 0;
			
		string_printf( data->parent_dir_internal, sizeof data->parent_dir_internal, &written, "%s", right_slash + 1 );
	}
}


static rc_t DirDlg_Goto_Child ( struct KTUIDlg * dlg, dir_dlg_data * data, uint32_t selection )
{
	rc_t rc = 0;
	/* we are goint one directory forward in the tree: */
	const char * s = KTUIDlgGetWidgetStringByIdx ( dlg, ID_DIRS, selection );
	if ( s != NULL )
	{
		/* add path segment from selection */
		char temp[ 4096 ];
		size_t written;
		
		if ( data->in_root )
#ifdef WINDOWS
			rc = string_printf ( temp, sizeof temp, &written, "/%c", s[ 0 ] );
#else
			rc = string_printf ( temp, sizeof temp, &written, "/%s", s );
#endif
		else
			rc = string_printf ( temp, sizeof temp, &written, "%s/%s", data->current_dir_internal, s );
		
		if ( rc == 0 )
			string_copy_measure ( data->current_dir_internal, sizeof data->current_dir_internal, temp );

		data->parent_dir_internal[ 0 ] = 0;
		data->in_root = false;
	}
	return rc;
}


static rc_t DirDlgDirectoryChanged ( struct KTUIDlg * dlg, dir_dlg_data * data, uint32_t selection )
{
    rc_t rc = 0;

    if ( selection == 0 && !data->in_root )
        DirDlg_Goto_Parent ( data );
    else
        rc = DirDlg_Goto_Child ( dlg, data, selection );

    if ( rc == 0 )
        rc = set_native_caption( dlg, data->vfs_mgr, ID_CURR, data->current_dir_internal ); 
    
    if ( rc == 0 )
        rc = fill_widget_with_dirs( dlg, data->dir, ID_DIRS, data->current_dir_internal, data->parent_dir_internal, true );

    if ( rc == 0 )
        rc = KTUIDlgSetWidgetVisible ( dlg, ID_B_CREATE, create_allowed( data ) );

    if ( rc == 0 )
        rc = KTUIDlgDraw( dlg, false );

    return rc;
}


static rc_t Present_Input ( struct KTUIDlg * dlg, dir_dlg_data * data, const char * caption,
                            char * buffer, size_t buffer_size, bool * selected )
{
    tui_rect r;
    rc_t rc = KTUIDlgGetRect ( dlg, &r );
    *selected = false;
    if ( rc == 0 )
    {
        uint32_t w = 40, h = 6;
        uint32_t x = ( r.w - w ) / 2;
        uint32_t y = ( r.h - h ) / 2;
        struct KTUI * tui = KTUIDlgGetTui( dlg );

        rc = TUI_EditBuffer( tui, dlg, caption, buffer, buffer_size, x, y, w, selected, data->bg1, data->bg2 );

        /* redraw in any case, even creating a directory failed, because we have to erase the sub-dialog from the screen */
        KTUIDlgDraw( dlg, false );
    }
    return rc;
}

static rc_t DirDlgCreate ( struct KTUIDlg * dlg, dir_dlg_data * data )
{
    rc_t rc = 0;
    if ( create_allowed( data ) )
    {
        char leafname[ 1024 ];
        bool selected;

        leafname[ 0 ] = 0;
        rc = Present_Input ( dlg, data, "new sub-directory", leafname, sizeof leafname, &selected );
        if ( rc == 0 )
        {
            if ( rc == 0 && selected && leafname[ 0 ] != 0 )
            {
                rc = KDirectoryCreateDir ( data->dir, 0775, kcmCreate | kcmParents,
                                           "%s/%s", data->current_dir_internal, leafname );
                if ( rc == 0 )
                    rc = fill_widget_with_dirs( dlg, data->dir, ID_DIRS, data->current_dir_internal, leafname, true );
                if ( rc == 0 )
                    rc = KTUIDlgDraw( dlg, false );
            }
            /* set the focus to the directory-list */
            KTUIDlgSetFocus( dlg, ID_DIRS );
        }
    }
    return rc;
}


static rc_t DirDlgGotoDir ( struct KTUIDlg * dlg, dir_dlg_data * data )
{
    char new_path[ 1024 ];
    size_t written;
    rc_t rc = internal_to_native( data->vfs_mgr, data->current_dir_internal, new_path, sizeof new_path, &written );
    if ( rc == 0 )
    {
        bool selected;
        rc = Present_Input ( dlg, data, "goto path", new_path, sizeof new_path, &selected );
        if ( rc == 0 && selected )
        {
            rc = native_to_internal( data->vfs_mgr, new_path, data->current_dir_internal,
                                     sizeof data->current_dir_internal, &written );
            if ( rc == 0 )
                rc = fill_widget_with_dirs( dlg, data->dir, ID_DIRS, data->current_dir_internal, NULL, true );
            if ( rc == 0 )
                rc = set_native_caption( dlg, data->vfs_mgr, ID_CURR, data->current_dir_internal );	

            KTUIDlgSetFocus( dlg, ID_DIRS );
        }
    }
    return rc;
}


static rc_t DirDlgEvent ( struct KTUIDlg * dlg, tuidlg_event * dev, dir_dlg_data * data )
{
    rc_t rc = 0;
    if ( dev->event_type == ktuidlg_event_select )
    {
        switch ( dev->widget_id )
        {
            case ID_DIRS     : rc = DirDlgDirectoryChanged( dlg, data, ( uint32_t )dev->value_1 ); break;
            case ID_B_OK     : data->ok_pressed = data->done = true; break;
            case ID_B_CANCEL : data->done = true; break;
            case ID_B_CREATE : rc = DirDlgCreate( dlg, data ); break;
            case ID_B_GOTO   : rc = DirDlgGotoDir( dlg, data ); break;
        }
    }
    return rc;
}


static rc_t DirDlgTuiEvent( struct KTUIDlg * dlg, dir_dlg_data * data, tui_event * ev, bool * handled )
{
    rc_t rc = 0;
    *handled = false;
    if ( ev->event_type == ktui_event_kb )
    {
        switch( ev->data.kb_data.code )
        {
            case ktui_F7 : rc = DirDlgCreate ( dlg, data ); *handled = true; break;
            case ktui_F3 : rc = DirDlgGotoDir ( dlg, data ); *handled = true; break;
            case ktui_F2 : data->ok_pressed = data->done = true; *handled = true; break;
            case ktui_alpha : switch ( ev->data.kb_data.key )
                               {
                                    case 'c' :
                                    case 'C' : rc = DirDlgCreate ( dlg, data ); *handled = true; break;
                                    case 'g' :
                                    case 'G' : rc = DirDlgGotoDir ( dlg, data ); *handled = true; break;
                               }
                               break;
        }
    }
    return rc;
}


static rc_t DirDlgLoop ( struct KTUIDlg * dlg, dir_dlg_data * data )
{
    rc_t rc;
    tui_event event;
    struct KTUI * tui = KTUIDlgGetTui( dlg );

    KTUIDlgDraw( dlg, false );  /* draw this dialog */
    do
    {
        rc = KTUIGet ( tui, &event );
        if ( rc == 0 )
        {
            bool handled = false;
            rc = DirDlgTuiEvent( dlg, data, &event, &handled );
            if ( rc == 0 && !handled )
            {
                rc = KTUIDlgHandleEvent( dlg, &event );
                if ( rc == 0 )
                {
                    tuidlg_event dev;
                    do
                    {
                        rc = KTUIDlgGet ( dlg, &dev );
                        if ( rc == 0 && dev.event_type != ktuidlg_event_none )
                            rc = DirDlgEvent( dlg, &dev, data );
                    } while ( rc == 0 && dev.event_type != ktuidlg_event_none );
                }
            }
        }
    } while ( rc == 0 && !is_alpha_key( &event, 27 ) && !data->done );
    return rc;
}


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


LIB_EXPORT rc_t CC DirDlg ( struct KTUI * tui,
                            struct KTUIDlg * parent,
                            char * buffer,             /* if empty ... local path */
                            uint32_t buffer_size,
                            bool * done,               /* user has selected a directory */
                            tui_rect * r,              /* rectangle for dialog */
                            KTUI_color bg1,
                            KTUI_color bg2,
                            bool allow_dir_create )
{
    struct KTUIPalette * pal;
    struct KTUIDlg * dlg;
    rc_t rc = make_dlg_with_bg( &dlg, &pal, tui, parent, r, bg1, bg2 );
    if ( rc == 0 )
    {
        rc = KTUIDlgSetCaption ( dlg, "select directory" );
        if ( rc == 0 )
        {
            dir_dlg_data data;

            init_dlg_data( &data, r->w, r->h, buffer, bg1, bg2, allow_dir_create );
            rc = PopulateDirDlg ( dlg, &data );
            if ( rc == 0 )
                DirDlgLoop( dlg, &data );

            if ( done != NULL )
                *done = data.ok_pressed;

            if ( data.ok_pressed )
            {
                size_t written;
                internal_to_native( data.vfs_mgr, data.current_dir_internal, buffer, buffer_size, &written );
            }

            destroy_dlg_data( &data );
        }
        KTUIDlgRelease ( dlg );
        KTUIPaletteRelease ( pal );
    }
    return rc;
}
