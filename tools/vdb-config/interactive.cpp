/*==============================================================================
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


/*
    a clean new config tool in c++ with interactive interface...
*/

#include <tui/tui.hpp>

#include <klib/rc.h>

#include <kfg/config.h>
#include <kfg/properties.h>
#include <kfg/repository.h>
#include <kfg/ngc.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <sstream>

#include "vdb-config-model.hpp"

using namespace tui;

#define BOX_COLOR		KTUI_c_dark_blue
#define STATUS_COLOR	KTUI_c_gray
#define LABEL_BG		KTUI_c_light_gray
#define LABEL_FG		KTUI_c_black
#define CB_COLOR_FG		KTUI_c_black
#define BTN_COLOR_FG	KTUI_c_black
#define CB_COLOR_BG		KTUI_c_cyan
#define BTN_COLOR_BG	KTUI_c_cyan

#define TOP_H			11
#define SRC_W		    36
#define PROXY_EN_W		15
#define PROXY_EN_W_ENV	43
#define SAVE_W		    14
#define EXIT_W		    14
#define WKSP_NAME_W		32
#define WKSP_B_LOC_W	12

#define CB_TXT_REMOTE	            "Enable Remote Access (1)"
#define CB_TXT_CACHE   	            "Enable Local File Caching (2)"
#define CB_TXT_SITE   	            "Use Site Installation (3)"
#define CB_TXT_PROXY        		"Use Proxy"
#define CB_TXT_PROXY_ENV       		"Prioritize Env. Variable \'http_proxy\'"

#define BTN_TXT_IMPORT_NGC 			"Import Repository Key (4)"
#define BTN_TXT_DFLT_IMPORT_PATH	"Set Default Import Path (5)"
#define B_SAVE_TXT                  "Save (6)"
#define B_EXIT_TXT                  "Exit (7)"
#define B_RELOAD_TXT                "Reload (8)"
#define B_FACT_DFLT_TXT             "Standard Settings (9)"

#define MAIN_CAPTION	            "vdb-config"
#define L_TXT_PUBLIC                "Public"
#define BTN_TXT_CHANGE	            "Change"
#define HDR_WKSP_NAME_TXT           "Workspace Name"
#define HDR_WKSP_LOC_TXT            "Location"
#define L_TXT_NUMBER_EXPLAIN        "Press the number in (X) as a shortcut"

#define FOCUS_TXT_CB_REMOTE         "Press SPACE | ENTER to enable/disable access to the servers at NCBI"
#define FOCUS_TXT_CB_CACHED         "Press SPACE | ENTER to enable/disable caching"
#define FOCUS_TXT_CB_SITE           "Press SPACE | ENTER to enable/disable access to site repositories"
#define FOCUS_TXT_CB_PROXY          "Press SPACE | ENTER to enable/disable http proxy"
#define FOCUS_TXT_B_PROXY			"Press SPACE | ENTER to edit proxy"
#define FOCUS_TXT_B_PROXY_ENV 	    "Press SPACE | ENTER to prioritize environment variable \'http_proxy\'"
#define FOCUS_TXT_B_PUBLIC_LOC      "Press SPACE | ENTER to change location of public data"
#define FOCUS_TXT_B_IMPORT_NGC      "Press SPACE | ENTER to import a dbGaP project"
#define FOCUS_TXT_B_USR_DFLT_PATH   "Press SPACE | ENTER to change default repository location"
#define FOCUS_TXT_B_PROT_LOC        "Press SPACE | ENTER to change location for this dbGaP repository"
#define FOCUS_TXT_B_SAVE            "Press SPACE | ENTER to save changes"
#define FOCUS_TXT_B_EXIT            "Press SPACE | ENTER to exit vdb-config"
#define FOCUS_TXT_B_RELOAD          "Press SPACE | ENTER to discard changes"
#define FOCUS_TXT_B_FACT_DFLT       "Press SPACE | ENTER to set factory defaults"

#define ID_BOX_TOP_LEFT             100
#define ID_BOX_BOTTOM               101
#define ID_BOX_TOP_RIGHT            102
#define ID_BOX_STATUS               103

#define ID_CB_REMOTE                104
#define ID_CB_CACHED                105
#define ID_CB_SITE                  106
#define ID_CB_PROXY          		107
#define ID_B_PROXY          		108
#define ID_L_PROXY          		109
#define ID_CB_PROXY_ENV        		110

#define ID_B_IMPORT_NGC             120
#define ID_B_USR_DFLT_PATH          121

#define ID_L_WKSP_NAME              122
#define ID_L_WKSP_LOC               123

#define ID_L_PUBLIC                 123
#define ID_B_PUBLIC_LOC             124
#define ID_L_PUBLIC_LOC             125

#define ID_B_SAVE                   126
#define ID_B_EXIT                   127
#define ID_B_RELOAD                 128
#define ID_B_FACT_DFLT              129

#define ID_L_NUMBER_EXPLAIN         130

#define ID_L_PROT                   200
#define ID_L_PROT_LOC               300
#define ID_B_PROT_LOC               400


class vdbconf_view : public Dlg
{
    public :
        vdbconf_view( vdbconf_model &model ) : Dlg(), priv_model( model )/*, repos_shown( 0 ), show_local_enable( false )*/
        {
            populate();
        };

        virtual bool Resize( Tui_Rect const &r )
        {
			populate( r, true );
            return Dlg::Resize( r );
        }

        void populate( void );

    private :
        vdbconf_model & priv_model;
/*      uint32_t repos_shown;
        bool show_local_enable;*/

        /* layout functions */
		uint32_t half( Tui_Rect const &r, uint32_t margins ) const { return ( ( r.get_w() - margins ) / 2 ); }
		Tui_Rect h1_rect( Tui_Rect const &r, int32_t dx, int32_t dy, int32_t dw ) const
            { return Tui_Rect( r.get_x() + dx, r.get_y() + dy, r.get_w() + dw, 1 ); }
		Tui_Rect h1_w_rect( Tui_Rect const &r, int32_t dx, int32_t dy, uint32_t w ) const
            { return Tui_Rect( r.get_x() + dx, r.get_y() + dy, w, 1 ); }
		
		Tui_Rect top_left_rect( Tui_Rect const &r ) const
            { return Tui_Rect( r.get_x() + 1, r.get_y() + 2, half( r, 3 ), TOP_H ); }

		Tui_Rect top_right_rect( Tui_Rect const &r ) const
            { return Tui_Rect( r.get_x() + half( r, 3 ) + 2, r.get_y() + 2, r.get_w() - half( r, 3 ) - 3, TOP_H ); }
																				
		Tui_Rect bottom_rect( Tui_Rect const &r ) const
            { return Tui_Rect( r.get_x() + 1, r.get_y() + TOP_H + 3, r.get_w() - 2, r.get_h() - ( TOP_H + 5 ) ); }

        Tui_Rect statust_rect( Tui_Rect const &r ) const { return h1_rect( r, 0, r.get_h() - 1, 0 ); }
        Tui_Rect remote_cb_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 1, SRC_W ); }
        Tui_Rect cache_cb_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 3, SRC_W ); }
        Tui_Rect site_cb_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 5, SRC_W ); }
        Tui_Rect proxy_cb_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 7, PROXY_EN_W ); }
        Tui_Rect proxy_cb_env_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 9, PROXY_EN_W_ENV ); }
        Tui_Rect proxy_btn_rect( Tui_Rect const &r ) const { return h1_w_rect( r, PROXY_EN_W + 2, 7, WKSP_B_LOC_W ); }
        Tui_Rect proxy_lb_rect( Tui_Rect const &r ) const
		{ return h1_w_rect( r, PROXY_EN_W + 3 + WKSP_B_LOC_W, 7, r.get_w() - ( PROXY_EN_W + 4 + WKSP_B_LOC_W ) ); }
		
        Tui_Rect import_ngc_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 1, WKSP_NAME_W ); }
        Tui_Rect usr_dflt_path_rect( Tui_Rect const &r ) const { return h1_w_rect( r, WKSP_NAME_W + 2, 1, WKSP_NAME_W + 3 ); }

		Tui_Rect hdr_wksp_name_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 3, WKSP_NAME_W ); }
		Tui_Rect hdr_wksp_loc_rect( Tui_Rect const &r ) const { return h1_rect( r, WKSP_NAME_W + 2, 3, -( WKSP_NAME_W + 3 ) ); }

		Tui_Rect wksp_name_rect( Tui_Rect const &r, uint32_t n ) const { return h1_w_rect( r, 1, 5 + ( 2 * n ), WKSP_NAME_W ); }
		Tui_Rect wksp_b_loc_rect( Tui_Rect const &r, uint32_t n ) const
            { return h1_w_rect( r, WKSP_NAME_W + 2, 5 + ( 2 * n ), WKSP_B_LOC_W ); }
		Tui_Rect wksp_loc_rect( Tui_Rect const &r, uint32_t n ) const
            { return h1_rect( r, WKSP_NAME_W + WKSP_B_LOC_W + 3, 5 + ( 2 * n ), -( WKSP_NAME_W + WKSP_B_LOC_W + 4 ) ); }
		
        Tui_Rect save_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 1, SAVE_W ); }
        Tui_Rect exit_rect( Tui_Rect const &r ) const { return h1_w_rect( r, SAVE_W + 2, 1, EXIT_W ); }
        Tui_Rect reload_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 3, SAVE_W + EXIT_W + 1 ); }
        Tui_Rect factdflt_rect( Tui_Rect const &r ) const { return h1_w_rect( r, 1, 5, SAVE_W + EXIT_W + 1 ); }
        Tui_Rect num_explain_rect( Tui_Rect const &r ) const { return h1_rect( r, 1, r.get_h() - 2, -2 ); }

		void setup_box( Tui_Rect const &r, bool resize, uint32_t id );
		void setup_label( Tui_Rect const &r, bool resize, uint32_t id, const char * txt );
		void setup_checkbox( Tui_Rect const &r, bool resize, uint32_t id, const char * txt, bool enabled );
		void setup_button( Tui_Rect const &r, bool resize, uint32_t id, const char * txt );
		
		void populate_top_left( Tui_Rect const &r, bool resize );
        void populate_protected( Tui_Rect const &r, bool resize );
		void populate_bottom( Tui_Rect const &r, bool resize );
		void populate_top_right( Tui_Rect const &r, bool resize );
        void populate( Tui_Rect const &r, bool resize );
};


void vdbconf_view::setup_box( Tui_Rect const &r, bool resize, uint32_t id )
{
	if ( resize )
		SetWidgetRect( id, r, false );
	else if ( !HasWidget( id ) )
	{
		AddLabel( id, r, "" );
		SetWidgetBackground( id, BOX_COLOR );
	}
}

void vdbconf_view::setup_label( Tui_Rect const &r, bool resize, uint32_t id, const char * txt )
{
	if ( resize )
		SetWidgetRect( id, r, false );
	else if ( HasWidget( id ) )
    {
        SetWidgetCaption( id, ( txt == NULL ) ? "" : txt );
    }
    else
	{
		AddLabel( id, r, ( txt == NULL ) ? "" : txt );
		SetWidgetBackground( id, LABEL_BG );
		SetWidgetForeground( id, LABEL_FG );
	}
}


void vdbconf_view::setup_checkbox( Tui_Rect const &r, bool resize, uint32_t id, const char * txt, bool value )
{
	if ( resize )
		SetWidgetRect( id, r, false );
	else if ( HasWidget( id ) )
    {
        SetWidgetCaption( id, ( txt == NULL ) ? "" : txt );
        SetWidgetBoolValue( id, value );
    }
    else
	{
        AddCheckBox( id, r, ( txt == NULL ) ? "" : txt, value );
		SetWidgetBackground( id, CB_COLOR_BG );
		SetWidgetForeground( id, CB_COLOR_FG );
	}
}


void vdbconf_view::setup_button( Tui_Rect const &r, bool resize, uint32_t id, const char * txt )
{
	if ( resize )
		SetWidgetRect( id, r, false );
	else if ( HasWidget( id ) )
    {
        SetWidgetCaption( id, ( txt == NULL ) ? "" : txt );
    }
    else
	{
		AddButton( id, r, ( txt == NULL ) ? "" : txt );
		SetWidgetBackground( id, BTN_COLOR_BG );
		SetWidgetForeground( id, BTN_COLOR_FG );
	}
}


void vdbconf_view::populate_top_left( Tui_Rect const &r, bool resize )
{
	setup_box( r, resize, ID_BOX_TOP_LEFT );

    /* the 3 checkboxes for remote, cached, site */
	setup_checkbox( remote_cb_rect( r ), resize, ID_CB_REMOTE, CB_TXT_REMOTE, priv_model.is_remote_enabled() );
	setup_checkbox( cache_cb_rect( r ), resize, ID_CB_CACHED, CB_TXT_CACHE, priv_model.is_global_cache_enabled() );
	if ( priv_model.does_site_repo_exist() )
		setup_checkbox( site_cb_rect( r ), resize, ID_CB_SITE, CB_TXT_SITE, priv_model.is_site_enabled() );
	setup_checkbox( proxy_cb_rect( r ), resize, ID_CB_PROXY, CB_TXT_PROXY, priv_model.is_http_proxy_enabled() );
	setup_button( proxy_btn_rect( r ), resize, ID_B_PROXY, BTN_TXT_CHANGE );
	setup_label( proxy_lb_rect( r ), resize, ID_L_PROXY, priv_model.get_http_proxy_path().c_str() );
    setup_checkbox( proxy_cb_env_rect( r ), resize, ID_CB_PROXY_ENV, CB_TXT_PROXY_ENV, priv_model.has_http_proxy_env_higher_priority() );
}


void vdbconf_view::populate_protected( Tui_Rect const &r, bool resize )
{
    uint32_t visible = ( ( r.get_h() - 9 ) / 2 );
    uint32_t i, n = priv_model.get_repo_count();
    for ( i = 0; i < n; ++i )
    {
        setup_label( wksp_name_rect( r, i + 1 ), resize, ID_L_PROT + i, priv_model.get_repo_name( i ).c_str() );
        setup_button( wksp_b_loc_rect( r, i + 1 ), resize, ID_B_PROT_LOC + i, BTN_TXT_CHANGE );
        setup_label( wksp_loc_rect( r, i + 1 ), resize, ID_L_PROT_LOC + i, priv_model.get_repo_location( i ).c_str() );

        SetWidgetVisible( ID_L_PROT + i, ( i < visible ) );
        SetWidgetVisible( ID_B_PROT_LOC + i, ( i < visible ) );
        SetWidgetVisible( ID_L_PROT_LOC + i, ( i < visible ) );
    }
}

void vdbconf_view::populate_bottom( Tui_Rect const &r, bool resize )
{
	setup_box( r, resize, ID_BOX_BOTTOM );
	
    /* the import-ngc and the set-dflt-path buttons */
	setup_button( import_ngc_rect( r ), resize, ID_B_IMPORT_NGC, BTN_TXT_IMPORT_NGC );
	setup_button( usr_dflt_path_rect( r ), resize, ID_B_USR_DFLT_PATH, BTN_TXT_DFLT_IMPORT_PATH );

    /* the header of the workspace list */	
	setup_label( hdr_wksp_name_rect( r ), resize, ID_L_WKSP_NAME, HDR_WKSP_NAME_TXT );
	setup_label( hdr_wksp_loc_rect( r ), resize, ID_L_WKSP_LOC, HDR_WKSP_LOC_TXT );

    /* the public workspace: */
    setup_label( wksp_name_rect( r, 0 ), resize, ID_L_PUBLIC, L_TXT_PUBLIC );
	setup_button( wksp_b_loc_rect( r, 0 ), resize, ID_B_PUBLIC_LOC, BTN_TXT_CHANGE );
    setup_label( wksp_loc_rect( r, 0 ), resize, ID_L_PUBLIC_LOC, priv_model.get_public_location().c_str() );

    /* the protected workspaces: */
    populate_protected( r, resize );

    setup_label( num_explain_rect( r ), resize, ID_L_NUMBER_EXPLAIN, L_TXT_NUMBER_EXPLAIN );
}


void vdbconf_view::populate_top_right( Tui_Rect const &r, bool resize )
{
	setup_box( r, resize, ID_BOX_TOP_RIGHT );

	setup_button( save_rect( r ), resize, ID_B_SAVE, B_SAVE_TXT );
	setup_button( exit_rect( r ), resize, ID_B_EXIT, B_EXIT_TXT );
	setup_button( reload_rect( r ), resize, ID_B_RELOAD, B_RELOAD_TXT );
	setup_button( factdflt_rect( r ), resize, ID_B_FACT_DFLT, B_FACT_DFLT_TXT );
}


void vdbconf_view::populate( Tui_Rect const &r, bool resize )
{
    if ( !resize ) SetCaption( MAIN_CAPTION );
    
    populate_top_left( top_left_rect( r ), resize );
    populate_bottom( bottom_rect( r ), resize );
    populate_top_right( top_right_rect( r ), resize );
    
    setup_label( statust_rect( r ), resize, ID_BOX_STATUS, NULL );
}


void vdbconf_view::populate( void )
{
    Tui_Rect r;
    GetRect( r );
    populate( r, false );
}

/* ------------------------------------------------------------------------------------------------------------------------------------- */


static void vdbconf_msg( Dlg &dlg, Tui_Rect r, const char * msg )
{
    Std_Dlg_Info_Line d;
    d.set_parent( &dlg );
    dlg.center( r );
    d.set_location( r );
    d.set_text( msg );
    d.execute();
}


static bool vdbconf_question( Dlg &dlg, Tui_Rect r, const char * msg )
{
    Std_Dlg_Question q;
    q.set_parent( &dlg );
    dlg.center( r );
    q.set_location( r );
    q.set_text( msg );
    return ( q.execute() );
}


static bool vdbconf_input( Dlg &dlg, Tui_Rect r, const char * caption, std::string & txt )
{
    bool res;
    std::string prev_txt = txt;
    Std_Dlg_Input q;
    q.set_parent( &dlg );
    dlg.center( r );
    q.set_location( r );
    q.set_caption( caption );
    q.set_text2( txt );
    res = q.execute();
    if ( res )
        res = ( prev_txt != q.get_text2() );
    if ( res )
        txt = q.get_text2();
    return res;
}


static std::string vdbconf_pick_file( Dlg &dlg, Tui_Rect r, const char * path, const char *ext )
{
    std::string res = "";
    Std_Dlg_File_Pick pick;
    pick.set_parent( &dlg );
    dlg.center( r );
    pick.set_location( r );
    pick.set_ext( ext );
    pick.set_dir_h( ( r.get_h() - 7 ) / 2 );
    pick.set_text( path );
    if ( pick.execute() )
        res.assign( pick.get_text() );
    return res;
}


bool vdbconf_pick_dir( Dlg &dlg, Tui_Rect r, std::string &path )
{
    bool res = false;
    Std_Dlg_Dir_Pick pick;

    pick.set_parent( &dlg );
    dlg.center( r );
    pick.set_location( r );
    pick.set_text( path.c_str() );
    pick.allow_dir_create();

    res = pick.execute();
    if ( res )
        path.assign( pick.get_text() );

    return res;
}


static bool make_ngc_obj( const KNgcObj ** ngc, std::string &path )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc == 0 )
    {
        const KFile * src;
        rc = KDirectoryOpenFileRead ( dir, &src, "%s", path.c_str() );
        if ( rc == 0 )
        {
            rc = KNgcObjMakeFromFile ( ngc, src );
            KFileRelease( src );
        }
        KDirectoryRelease( dir );
    }
    return ( rc == 0 );
}


std::string public_location_start_dir( vdbconf_model &model )
{
    std::string res = model.get_public_location();

    if ( !model.does_path_exist( res ) )
        res = model.get_user_default_dir();

    if ( !model.does_path_exist( res ) )
        res = model.get_home_dir() + "/ncbi";

    if ( !model.does_path_exist( res ) )
        res = model.get_home_dir();

    if ( !model.does_path_exist( res ) )
        res = model.get_current_dir();

    return res;
}


std::string protected_location_start_dir( vdbconf_model &model, uint32_t id )
{
    std::string res = model.get_repo_location( id );

    if ( !model.does_path_exist( res ) )
        res = model.get_user_default_dir();

    if ( !model.does_path_exist( res ) )
        res = model.get_home_dir() + "/ncbi";

    if ( !model.does_path_exist( res ) )
        res = model.get_home_dir();

    if ( !model.does_path_exist( res ) )
        res = model.get_current_dir();

    return res;
}

/* ------------------------------------------------------------------------------------------------------------------------------------- */


class vdbconf_controller : public Dlg_Runner
{
    private :
        vdbconf_model &priv_model;

    public :
        vdbconf_controller( Dlg &dlg, vdbconf_model &model ) : Dlg_Runner( dlg, &model ), priv_model( model ) 
        { dlg.SetFocus( ID_CB_REMOTE ); };

        virtual bool on_kb_alpha( Dlg &dlg, void * data, int code ); /* close the dialog with the 'q' character too */
        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev ); /* react to buttons pressed */
        virtual bool on_focus( Dlg &dlg, void * data, Tui_Dlg_Event &dev ) /* react to getting the focus */
        { return on_focused( dlg, dev.get_widget_id() ); };

        void update_view( Dlg &dlg );
        bool on_exit( Dlg &dlg );
        bool on_save( Dlg &dlg );
        bool on_reload( Dlg &dlg );
        bool on_fact_dflt( Dlg &dlg );

        std::string get_import_ngc_start_dir( vdbconf_model &model );
        bool import_this_ngc_into_this_location( Dlg &dlg, vdbconf_model &m, std::string &location, const KNgcObj * ngc );
        bool import_this_ngc( Dlg &dlg, vdbconf_model &m, const KNgcObj * ngc );
        bool on_import_ngc( Dlg &dlg );

        std::string change_dflt_import_path_start_dir( vdbconf_model &model );
        bool on_change_dflt_import_path( Dlg &dlg );

        bool toggle_checkbox( Dlg &dlg, uint32_t id );
        bool toggle_remote( Dlg &dlg );
        bool toggle_site( Dlg &dlg );
        bool toggle_cached( Dlg &dlg );

		bool change_proxy( Dlg &dlg );
		
        bool on_set_location_error( Dlg &dlg, ESetRootState s );
        bool on_pick_public_location( Dlg &dlg );

        std::string change_protected_location_start_dir( vdbconf_model &model, uint32_t id );
        bool on_pick_protected_location( Dlg &dlg, uint32_t id );
        bool on_protected_repo( Dlg &dlg, Tui_Dlg_Event &dev );

        void status_txt( Dlg &dlg, const char * s ) { dlg.SetWidgetCaption( ID_BOX_STATUS, s ); }
        void on_focused_protected_repo( Dlg &dlg, uint32_t widget_id );
        bool on_focused( Dlg &dlg, uint32_t widget_id );
};


void vdbconf_controller::update_view( Dlg &dlg )
{
    vdbconf_view &view = dynamic_cast<vdbconf_view &>( dlg );
    view.populate();
}


bool vdbconf_controller::on_exit( Dlg &dlg )
{
    if ( priv_model.get_config_changed() )
    {
        if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), "save changes ?" ) )
        {
            if ( priv_model.commit() )
                vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "changes successfully saved" );
            else
                vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "error saving changes" );
        }
    }
    dlg.SetDone( true );
    return true;
}


bool vdbconf_controller::on_save( Dlg &dlg )
{
    if ( priv_model.get_config_changed() )
    {
        if ( priv_model.commit() )
            vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "changes successfully saved" );
        else
            vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "error saving changes" );
    }
    else
        vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "no changes to be saved" );
    return true;
}


bool vdbconf_controller::on_reload( Dlg &dlg )
{
    priv_model.reload();
    update_view( dlg );
    return true;
}


bool vdbconf_controller::on_fact_dflt( Dlg &dlg )
{
    priv_model.set_remote_enabled( true );
    priv_model.set_global_cache_enabled( true );
    priv_model.set_site_enabled( true );
    update_view( dlg );
    return true;
}


bool vdbconf_controller::toggle_checkbox( Dlg &dlg, uint32_t id )
{
    bool value = !dlg.GetWidgetBoolValue( id );
    dlg.SetWidgetBoolValue( id, value );
    return value;
}


bool vdbconf_controller::toggle_remote( Dlg &dlg )
{
    priv_model.set_remote_enabled( toggle_checkbox( dlg, ID_CB_REMOTE ) );
    return true;
}

bool vdbconf_controller::toggle_cached( Dlg &dlg )
{
    priv_model.set_global_cache_enabled( toggle_checkbox( dlg, ID_CB_CACHED ) );
    return true;
}

bool vdbconf_controller::toggle_site( Dlg &dlg )
{
	if ( priv_model.does_site_repo_exist() )
        priv_model.set_site_enabled( toggle_checkbox( dlg, ID_CB_SITE ) );
    return true;
}


bool vdbconf_controller::on_kb_alpha( Dlg &dlg, void * data, int code )
{
    bool res = false;
    switch( code )
    {
        case '1' : res = toggle_remote( dlg ); break;
        case '2' : res = toggle_cached( dlg ); break;
        case '3' : res = toggle_site( dlg ); break;

        case '4' : res = on_import_ngc( dlg ); break;
        case '5' : res = on_change_dflt_import_path( dlg ); break;

        case '6' : res = on_save( dlg ); break;

        case '7' :
        case 'Q' :
        case 'q' : res = on_exit( dlg ); break;

        case '8' : res = on_reload( dlg ); break;
        case '9' : res = on_fact_dflt( dlg ); break;

        default  : res = false;

    }
    return res;
};


bool vdbconf_controller::import_this_ngc_into_this_location( Dlg &dlg,
    vdbconf_model &m, std::string &location, const KNgcObj * ngc )
{
    uint32_t result_flags = 0;
    bool res = m.import_ngc( location, ngc, INP_CREATE_REPOSITORY, &result_flags );
    if ( res )
    {
        /* we have it imported or it exists and no changes made */
        bool modified = false;
        if ( result_flags & INP_CREATE_REPOSITORY )
        {
            /* success is the most common outcome, the repository was created */
            vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "project successfully imported" );
            update_view( dlg );
            modified = true;
        }
        else
        {
            /* repository did exist and is completely identical to the given ngc-obj */
            vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "this project exists already, no changes made" );
        }

        std::ostringstream question; 
        question << "do you want to change the location?";
        if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) )
        {
            uint32_t id;
            if ( m.get_id_of_ngc_obj( ngc, &id ) )
                modified |= on_pick_protected_location( dlg, id );
            else
                vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "cannot find the imported repostiory" );
        }

        if ( modified )
        {
            m.commit();
            update_view( dlg );
            m.mkdir(ngc);
        }
    }
    else if ( result_flags == 0 )
    {
        /* we are here if there was an error executing one of the internal functions */
        vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "there was an internal error importing the ngc-object" );
    }
    else
    {
        bool permitted = true;

        vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "the repository does already exist!" );
        if ( result_flags & INP_UPDATE_ENC_KEY )
        {
            std::ostringstream question; 
            question << "encryption-key would change, continue ?";
            permitted = ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) );
        }

        if ( permitted && ( result_flags & INP_UPDATE_DNLD_TICKET ) )
        {
            std::ostringstream question; 
            question << "download-ticket would change, continue ?";
            permitted = ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) );
        }

        if ( permitted && ( result_flags & INP_UPDATE_DESC ) )
        {
            std::ostringstream question; 
            question << "description would change, continue ?";
            permitted = ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) );
        }

        if ( permitted )
        {
            uint32_t result_flags2 = 0;
            res = m.import_ngc( location, ngc, result_flags, &result_flags2 );
            if ( res )
            {
                vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "project successfully updated" );
                
                std::ostringstream question; 
                question << "do you want to change the location?";
                if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) )
                {
                    uint32_t id; /* we have to find out the id of the imported/existing repository */
                    if ( m.get_id_of_ngc_obj( ngc, &id ) )            
                        on_pick_protected_location( dlg, id );
                    else
                        vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "cannot find the imported repostiory" );
                }
                m.commit();
                update_view( dlg );
            }
            else
                vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "there was an internal error importing the ngc-object" );
        }
        else
            vdbconf_msg( dlg, Tui_Rect( 5, 5, 120, 6 ), "the import was canceled" );
    }
    return res;
}


bool vdbconf_controller::import_this_ngc( Dlg &dlg, vdbconf_model &m, const KNgcObj * ngc )
{
    bool res = false;

    std::string location_base = m.get_user_default_dir();
    std::string location = m.get_ngc_root( location_base, ngc );
    ESetRootState es = m.prepare_repo_directory( location );
    switch ( es )
    {
        case eSetRootState_OK               :  res = import_this_ngc_into_this_location( dlg, m, location, ngc );
                                                break;

        case eSetRootState_OldNotEmpty      :  if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ),
                                                                       "repository location is not empty, use it?" ) )
                                                {
                                                    es = m.prepare_repo_directory( location, true );
                                                    if ( es == eSetRootState_OK )
                                                        res = import_this_ngc_into_this_location( dlg, m, location, ngc );
                                                    else
                                                        res = on_set_location_error( dlg, es );
                                                }
                                                break;

        default : res = on_set_location_error( dlg, es ); break;
    }
    return res;
}


std::string vdbconf_controller::get_import_ngc_start_dir( vdbconf_model &model )
{
    std::string res = model.get_home_dir();
    if ( !model.does_path_exist( res ) )
        res = model.get_current_dir();
    return res;
}


bool vdbconf_controller::on_import_ngc( Dlg &dlg )
{
    bool res = false;
    std::string start_dir = get_import_ngc_start_dir( priv_model );
    /* ( 1 ) pick a ngc-file */
    std::string picked = vdbconf_pick_file( dlg, dlg.center( 5, 5 ), start_dir.c_str(), "ngc" );
    if ( picked.length() > 0 )
    {
        std::ostringstream question;
        question << "do you want to import '" << picked << "' ?";
        /* ( 2 ) confirm the choice */
        if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) )
        {
            const KNgcObj * ngc;
            if ( make_ngc_obj( &ngc, picked ) )
            {
                res = import_this_ngc( dlg, priv_model, ngc );
                KNgcObjRelease( ngc );
            }
        }
    }
    return res;
}


std::string vdbconf_controller::change_dflt_import_path_start_dir( vdbconf_model &model )
{
    std::string res = model.get_user_default_dir();
    if ( !model.does_path_exist( res ) )
        res = model.get_home_dir();
    if ( !model.does_path_exist( res ) )
        res = model.get_current_dir();
    return res;
}


bool vdbconf_controller::on_change_dflt_import_path( Dlg &dlg )
{
    bool res = false;

    std::string path = change_dflt_import_path_start_dir( priv_model );

    if ( priv_model.does_path_exist( path ) )
        res = vdbconf_pick_dir( dlg, dlg.center( 5, 5 ), path );
    else
        res = vdbconf_input( dlg, Tui_Rect( 5, 5, 100, 6 ), "change default import path", path );

    if ( res )
    {
        priv_model.set_user_default_dir( path.c_str() );
        vdbconf_msg( dlg, Tui_Rect( 5, 5, 100, 6 ), "default import path changed" );
    }
    return res;
}


bool vdbconf_controller::change_proxy( Dlg &dlg )
{
	std::string proxy = priv_model.get_http_proxy_path();
	bool res = vdbconf_input( dlg, Tui_Rect( 5, 5, 100, 6 ), "change proxy", proxy );
	if ( res )
	{
		priv_model.set_http_proxy_path( proxy.c_str() );
		dlg.SetWidgetCaption( ID_L_PROXY, proxy );
	}
	return res;
}

bool vdbconf_controller::on_set_location_error( Dlg &dlg, ESetRootState s )
{
    bool result = false;
    Tui_Rect r( 5, 5, 100, 6 );
    switch ( s )
    {
        case eSetRootState_NotChanged       : result = true; break;
        case eSetRootState_NotUnique        : vdbconf_msg( dlg, r, "location not unique, select a different one" ); break;
        case eSetRootState_MkdirFail        : vdbconf_msg( dlg, r, "could not created directory, maybe permisson problem" ); break;
        case eSetRootState_NewPathEmpty     : vdbconf_msg( dlg, r, "you gave me an empty path" ); break;
        case eSetRootState_NewDirNotEmpty   : vdbconf_msg( dlg, r, "the given location is not empty" ); break;
        case eSetRootState_NewNotDir        : vdbconf_msg( dlg, r, "new location is not a directory" ); break;
        case eSetRootState_Error            : vdbconf_msg( dlg, r, "error changing location" ); break;
        default                             : vdbconf_msg( dlg, r, "unknown enum" ); break;
    }
    return result;
}


bool vdbconf_controller::on_pick_public_location( Dlg &dlg )
{
    bool res = false;
	std::string path = public_location_start_dir( priv_model );
	
	if ( priv_model.does_path_exist( path ) )
		res = vdbconf_pick_dir( dlg, dlg.center( 5, 5 ), path );
	else
		res = vdbconf_input( dlg, Tui_Rect( 5, 5, 100, 6 ), "location of public cache", path );
	
    if ( res && path.length() > 0 )
    {
        std::ostringstream question;
        question << "do you want to change the location to '" << path << "' ?";
        if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) )
        {
            bool flushOld = false;
            bool reuseNew = false;
            ESetRootState s = priv_model.set_public_location( flushOld, path, reuseNew );
            switch ( s )
            {
                case eSetRootState_OK               :  dlg.SetWidgetCaption( ID_L_PUBLIC_LOC, path );
                                                        res = true;
                                                        break;

                case eSetRootState_OldNotEmpty      : if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ),
                                                                        "prev. location is not empty, flush it?" ) )
                                                        {
                                                            flushOld = true;
                                                            s = priv_model.set_public_location( flushOld, path, reuseNew );
                                                            res = ( s == eSetRootState_OK );
                                                            if ( res )
                                                                dlg.SetWidgetCaption( ID_L_PUBLIC_LOC, path );
                                                            else
                                                                res = on_set_location_error( dlg, s );
                                                        }
                                                        break;

                default : res = on_set_location_error( dlg, s ); break;
            }
        }
    }
    return res;
}


bool vdbconf_controller::on_pick_protected_location( Dlg &dlg, uint32_t id )
{
    bool res = false;
	std::string path = protected_location_start_dir( priv_model, id );

	if ( priv_model.does_path_exist( path ) )
		res = vdbconf_pick_dir( dlg, dlg.center( 5, 5 ), path );
	else
		res = vdbconf_input( dlg, Tui_Rect( 5, 5, 100, 6 ), "location of dbGaP project", path );

    if ( res && path.length() > 0 )
    {
        std::ostringstream question;
        question << "do you want to change the loction of '" << priv_model.get_repo_name( id ) <<"' to '" << path << "' ?";
        if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ), question.str().c_str() ) )
        {
            bool flushOld = false;
            bool reuseNew = false;
            ESetRootState s = priv_model.set_repo_location( id, flushOld, path, reuseNew );
            switch ( s )
            {
                case eSetRootState_OK               :  dlg.SetWidgetCaption( ID_L_PROT_LOC + id, path );
                                                        res = true;
                                                        break;

                case eSetRootState_OldNotEmpty      :  if ( vdbconf_question( dlg, Tui_Rect( 5, 5, 120, 6 ),
                                                                        "prev. location is not empty, flush it?" ) )
                                                        {
                                                            flushOld = true;
                                                            s = priv_model.set_repo_location( id, flushOld, path, reuseNew );
                                                            res = ( s == eSetRootState_OK );
                                                            if ( res )
                                                                dlg.SetWidgetCaption( ID_L_PROT_LOC + id, path );
                                                            else
                                                                res = on_set_location_error( dlg, s );
                                                        }
                                                        break;

                default : res = on_set_location_error( dlg, s ); break;
            }
        }
    }
    return res;
}


bool vdbconf_controller::on_protected_repo( Dlg &dlg, Tui_Dlg_Event &dev )
{
    bool res = false;
    uint32_t id = dev.get_widget_id();
    if ( id >= ID_B_PROT_LOC && id < ( ID_B_PROT_LOC + 100 ) )
    {
        vdbconf_model &m( priv_model );
        uint32_t repo_id = ( id - ID_B_PROT_LOC );
        if ( repo_id < m.get_repo_count() )
            res = on_pick_protected_location( dlg, repo_id );
    }
    return res;
}


bool vdbconf_controller::on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
{
    bool res = false;
    vdbconf_model &m( vdbconf_controller::priv_model );
    switch( dev.get_widget_id() )
    {
        case ID_B_SAVE          : res = on_save( dlg ); break;    
        case ID_B_EXIT          : res = on_exit( dlg ); break;    
        case ID_B_RELOAD        : res = on_reload( dlg ); break;    
        case ID_B_FACT_DFLT     : res = on_fact_dflt( dlg ); break;    

        case ID_CB_REMOTE : m.set_remote_enabled( dev.get_value_1() == 1 ); res = true; break;
        case ID_CB_CACHED : m.set_global_cache_enabled( dev.get_value_1() == 1 ); res = true; break;

        case ID_CB_SITE   : if ( priv_model.does_site_repo_exist() )
                                m.set_site_enabled( dev.get_value_1() == 1 );
                             res = true;
                             break;

		case ID_CB_PROXY     : m.set_http_proxy_enabled( dev.get_value_1() == 1 ); res = true; break;
		case ID_B_PROXY      : res = change_proxy( dlg ); break;
		
        case ID_CB_PROXY_ENV : m.set_http_proxy_env_higher_priority( dev.get_value_1() == 1 ); res = true; break;
        
        case ID_B_PUBLIC_LOC : res = on_pick_public_location( dlg ); break;

        case ID_B_IMPORT_NGC : res = on_import_ngc( dlg ); break;
        case ID_B_USR_DFLT_PATH : res = on_change_dflt_import_path( dlg ); break;

        default : res = on_protected_repo( dlg, dev ); break;
    }
    return res;
};


void vdbconf_controller::on_focused_protected_repo( Dlg &dlg, uint32_t widget_id )
{
    if ( widget_id >= ID_B_PROT_LOC && widget_id < ( ID_B_PROT_LOC + 100 ) )
        status_txt( dlg, FOCUS_TXT_B_PROT_LOC );
    else
        status_txt( dlg, "???" );
}


bool vdbconf_controller::on_focused( Dlg &dlg, uint32_t widget_id )
{
    if ( widget_id > 1000 ) return false;

    switch( widget_id )
    {
        case ID_B_SAVE          : status_txt( dlg, FOCUS_TXT_B_SAVE ); break;    
        case ID_B_EXIT          : status_txt( dlg, FOCUS_TXT_B_EXIT ); break;    
        case ID_B_RELOAD        : status_txt( dlg, FOCUS_TXT_B_RELOAD ); break;    
        case ID_B_FACT_DFLT     : status_txt( dlg, FOCUS_TXT_B_FACT_DFLT ); break;    

        case ID_CB_REMOTE       : status_txt( dlg, FOCUS_TXT_CB_REMOTE ); break;
        case ID_CB_CACHED       : status_txt( dlg, FOCUS_TXT_CB_CACHED ); break;
        case ID_CB_SITE         : status_txt( dlg, FOCUS_TXT_CB_SITE ); break;
		case ID_CB_PROXY        : status_txt( dlg, FOCUS_TXT_CB_PROXY ); break;
		case ID_B_PROXY         : status_txt( dlg, FOCUS_TXT_B_PROXY ); break;
		case ID_CB_PROXY_ENV    : status_txt( dlg, FOCUS_TXT_B_PROXY_ENV ); break;
        
        case ID_B_PUBLIC_LOC    : status_txt( dlg, FOCUS_TXT_B_PUBLIC_LOC ); break;
        case ID_B_IMPORT_NGC    : status_txt( dlg, FOCUS_TXT_B_IMPORT_NGC ); break;
        case ID_B_USR_DFLT_PATH : status_txt( dlg, FOCUS_TXT_B_USR_DFLT_PATH ); break;

        default : on_focused_protected_repo( dlg, widget_id ); break;
    }
    return false;
}

/* ------------------------------------------------------------------------------------------------------------------------------------- */

extern "C"
{
    rc_t run_interactive ( vdbconf_model & model )
    {
        try
        {
            /* (2) ... create derived view, the view creates the widgets in its constructor... */
            vdbconf_view view( model );
        
            /* (3) ... create derived controller, hand it the view and the model*/
            vdbconf_controller controller( view, model );

            /* (4) ... let the controller handle the events */
            controller.run();

            /* (5) call this before leaving main() to terminate the low-level driver... */
            Tui::clean_up();
        }
        catch ( ... )
        {
            return RC( rcExe, rcNoTarg, rcExecuting, rcNoObj, rcUnknown );
        }
        return 0;
    }
}
