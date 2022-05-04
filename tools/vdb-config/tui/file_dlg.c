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


typedef struct file_dlg_data
{
    uint32_t dlg_w, dlg_h, dir_h;

    /* for the inputline */
    char current_dir_internal[ 4096 ];
	char parent_dir_internal[ 4096 ];

    /* which extension... */
    char * extension;

    /* a VFS-Manager to do a path-conversion */
    VFSManager * vfs_mgr;
	
    /* a KDirectory to fill widgets with directories/files */
	KDirectory * dir;

	bool in_root;
    bool selected;
	bool done;
} file_dlg_data;


static void init_dlg_data( file_dlg_data * data, uint32_t dlg_w, uint32_t dlg_h, uint32_t dir_h,
    char * path, const char * extension )
{
    size_t written;

    data->dlg_w = dlg_w;
    data->dlg_h = dlg_h;
    data->dir_h = dir_h;

    VFSManagerMake ( &data->vfs_mgr );
	KDirectoryNativeDir ( &data->dir );
	
    native_to_internal( data->vfs_mgr, path, data->current_dir_internal, sizeof data->current_dir_internal, &written );
	
	data->in_root = ( ( data->current_dir_internal[ 0 ] == '/' ) && ( data->current_dir_internal[ 1 ] == 0 ) );
	data->parent_dir_internal[ 0 ] = 0;
    data->selected = false;
	data->done = false;
    if ( extension != NULL )
        data->extension = string_dup_measure ( extension, NULL );
    else
        data->extension = NULL;
}

static void destroy_dlg_data( file_dlg_data * data )
{
    /* we only have to destroy the Namelist's... */
    if ( data->extension != NULL )
    {
        free( ( void * ) data->extension );
        data->extension = NULL;
    }

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


enum
{
    ID_CURR = 100,
    ID_LABEL1,
    ID_DIRS,
    ID_LABEL2,
    ID_FILES,
    ID_B_OK,
    ID_B_CANCEL
};


static rc_t PopulateFileDlg ( struct KTUIDlg * dlg, file_dlg_data * data )
{
	rc_t rc = KTUIDlgAddLabel2( dlg, ID_CURR, 1, 2, data->dlg_w - 2, "" );

	if ( rc == 0 )
		rc = KTUIDlgAddLabel2( dlg, ID_LABEL1, 1, 4, data->dlg_w - 2, "directories:" );

	if ( rc == 0 )
	{
		tui_rect r;
		set_rect( &r, 1, 5, data->dlg_w - 2, data->dir_h );
		rc = KTUIDlgAddList ( dlg, ID_DIRS, &r );
	}

	if ( rc == 0 )
		rc = KTUIDlgAddLabel2( dlg, ID_LABEL2, 1, data->dir_h + 6, data->dlg_w - 2, "files:" );

	if ( rc == 0 )
	{
		tui_rect r;
		set_rect( &r, 1, data->dir_h + 7, data->dlg_w - 2, data->dlg_h - ( data->dir_h + 10 )  );
		rc = KTUIDlgAddList ( dlg, ID_FILES, &r );
	}

	if ( rc == 0 )
		rc = KTUIDlgAddBtn2( dlg, ID_B_OK, 1, data->dlg_h - 2, 12, "OK" );

	if ( rc == 0 )
		rc = KTUIDlgAddBtn2( dlg, ID_B_CANCEL, 14, data->dlg_h - 2, 22, "Cancel (ESC-ESC)" );

	if ( rc == 0 )
		rc = set_native_caption( dlg, data->vfs_mgr, ID_CURR, data->current_dir_internal );
	
	if ( rc == 0 )
		rc = fill_widget_with_dirs( dlg, data->dir, ID_DIRS, data->current_dir_internal, NULL, true );

	if ( rc == 0 )
		rc = fill_widget_with_files( dlg, data->dir, ID_FILES, data->current_dir_internal, data->extension, false );

	if ( rc == 0 )
		rc = KTUIDlgSetFocus( dlg, ID_DIRS );

    return rc;
}


static void FileDlg_Goto_Parent ( file_dlg_data * data )
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


static rc_t FileDlg_Goto_Child ( struct KTUIDlg * dlg, file_dlg_data * data, uint32_t selection )
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


static rc_t FileDlgDirectoryChanged ( struct KTUIDlg * dlg, file_dlg_data * data, uint32_t selection )
{
    rc_t rc = 0;

    if ( selection == 0 && !data->in_root )
		FileDlg_Goto_Parent ( data );
    else
		rc = FileDlg_Goto_Child ( dlg, data, selection );

    if ( rc == 0 )
		rc = set_native_caption( dlg, data->vfs_mgr, ID_CURR, data->current_dir_internal );

	if ( rc == 0 )
		rc = fill_widget_with_dirs( dlg, data->dir, ID_DIRS, data->current_dir_internal, data->parent_dir_internal, true );

	if ( rc == 0 )
		rc = fill_widget_with_files( dlg, data->dir, ID_FILES, data->current_dir_internal, data->extension, true );

    if ( rc == 0 )
        rc = KTUIDlgDraw( dlg, false );

    return rc;
}


static rc_t FileDlgFileSelected ( struct KTUIDlg * dlg, file_dlg_data * data, uint32_t selection )
{
    rc_t rc = 0;
    const char * filename = KTUIDlgGetWidgetStringByIdx ( dlg, ID_FILES, selection );
    if ( filename != NULL )
    {
        /* add path segment from selection */
        char temp[ 4096 ];
        size_t written;
        
        char * src = data -> current_dir_internal;
        size_t l = string_size( src );
        uint32_t pt = KDirectoryPathType( data -> dir, "%s", src );
        if ( pt == kptFile )
        {
            char * p = string_rchr ( src, l, '/' );
            if ( p != NULL )
            {
                l = ( p - src );
            }
        }
        
        rc = string_printf ( temp, sizeof temp, &written, "%.*s/%s", l, src, filename );
        if ( rc == 0 )
            string_copy_measure ( data->current_dir_internal, sizeof data->current_dir_internal, temp );
        data->selected = true;
		data->done = true;
    }
    return rc;
}


static rc_t FileDlgEvent ( struct KTUIDlg * dlg, tuidlg_event * dev, file_dlg_data * data )
{
    rc_t rc = 0;
    if ( dev->event_type == ktuidlg_event_select )
    {
        switch ( dev->widget_id )
        {
            case ID_DIRS     : rc = FileDlgDirectoryChanged( dlg, data, (uint32_t)dev->value_1 ); break;
            case ID_FILES    : rc = FileDlgFileSelected( dlg, data, (uint32_t)dev->value_1 ); break;
			
			case ID_B_OK     : rc = FileDlgFileSelected( dlg, data, KTUIDlgGetWidgetSelectedString( dlg, ID_FILES ) ); break;
            case ID_B_CANCEL : data->done = true; break;
        }
    }
    return rc;
}


static rc_t FileDlgLoop ( struct KTUIDlg * dlg, file_dlg_data * data )
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
            rc = KTUIDlgHandleEvent( dlg, &event );
            if ( rc == 0 )
            {
                tuidlg_event dev;
                do
                {
                    rc = KTUIDlgGet ( dlg, &dev );
                    if ( rc == 0 && dev.event_type != ktuidlg_event_none )
                        rc = FileDlgEvent( dlg, &dev, data );
                } while ( rc == 0 && dev.event_type != ktuidlg_event_none );
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


LIB_EXPORT rc_t CC FileDlg ( struct KTUI * tui,
                             struct KTUIDlg * parent,
                             char * buffer,             /* if empty ... local path */
                             uint32_t buffer_size,
                             const char * extension,    /* empty ... all files, "c" ... *.c */
                             bool * done,               /* user has selected a file */
                             tui_rect * r,              /* rectangle for dialog */
                             uint32_t dir_h,            /* how many lines for the directory part */
                             KTUI_color bg1,
                             KTUI_color bg2 )
{
    struct KTUIPalette * pal;
    struct KTUIDlg * dlg;
    rc_t rc = make_dlg_with_bg( &dlg, &pal, tui, parent, r, bg1, bg2 );
    if ( rc == 0 )
    {
        rc = KTUIDlgSetCaption ( dlg, "select file" );
        if ( rc == 0 )
        {
            file_dlg_data data;

            init_dlg_data( &data, r->w, r->h, dir_h, buffer, extension );
            rc = PopulateFileDlg ( dlg, &data );
            if ( rc == 0 )
                FileDlgLoop( dlg, &data );

            if ( done != NULL )
                *done = data.selected;

            if ( data.selected )
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
