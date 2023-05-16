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

#include <sstream>
#include <iostream>
#include <cstdarg>

#include <klib/rc.h>
#include <tui/tui.hpp>

#include "vdb-config-model.hpp"

using namespace tui;

const bool With_DbGaP = false;
const bool With_Verfify = false;

const KTUI_color BOX_COLOR      = KTUI_c_dark_blue;
const KTUI_color STATUS_COLOR   = KTUI_c_gray;
const KTUI_color LABEL_BG       = KTUI_c_light_gray;
const KTUI_color LABEL_FG       = KTUI_c_white;
const KTUI_color CB_COLOR_FG    = KTUI_c_black;
const KTUI_color CB_COLOR_BG    = KTUI_c_cyan;
const KTUI_color BTN_COLOR_FG   = KTUI_c_black;
const KTUI_color BTN_COLOR_BG   = KTUI_c_cyan;
const KTUI_color INP_COLOR_FG   = KTUI_c_black;
const KTUI_color INP_COLOR_BG   = KTUI_c_white;

// #define TELEMETRY_VISIBLE

enum e_id
{
    STATUS_ID = 100,
    SAVE_BTN_ID, EXIT_BTN_ID, VERIFY_BTN_ID, DISCARD_BTN_ID, DEFAULT_BTN_ID, BOX_ID,

    MAIN_HDR_ID = 200,
    MAIN_USE_REMOTE_ID, MAIN_USE_SITE_ID, MAIN_FULL_QUALITY,
#ifdef TELEMETRY_VISIBLE
    MAIN_TELEMETRY,
#endif
    MAIN_GUID_ID,

    CACHE_HDR_ID = 300,
    CACHE_USE_CACHE_ID,
    CACHE_REPO_LBL_ID, CACHE_REPO_CHOOSE_ID, CACHE_REPO_PATH_ID, CACHE_REPO_CLEAR_ID,
    CACHE_PROC_LBL_ID, CACHE_PROC_CHOOSE_ID, CACHE_PROC_PATH_ID, CACHE_PROC_CLEAR_ID,
    CACHE_RAM_LBL_ID, CACHE_RAM_ID, CACHE_RAM_UNIT_ID,

    AWS_HDR_ID = 400,
    AWS_CHARGES_ID, AWS_REPORT_ID, AWS_KEY_ID, AWS_CHOOSE_ID, AWS_CLEAR_ID,
    AWS_FILE_ID, AWS_PROF_LBL_ID, AWS_PROF_ID,

    GCP_HDR_ID = 500,
    GCP_CHARGES_ID, GCP_REPORT_ID, GCP_KEY_ID, GCP_CHOOSE_ID, GCP_CLEAR_ID, GCP_FILE_ID,

    NETW_HDR_ID = 600,
    NETW_USE_PROXY_ID, NETW_PROXY_LBL_ID, NETW_PROXY_ID,
    NETW_PROXY_PORT_LBL_ID, NETW_PROXY_PORT_ID,

    DBGAP_HDR_ID = 700,
    DBGAP_IMPORT_KEY_ID, DBGAP_IMPORT_PATH_ID, DBGAP_REPOS_LBL_ID, DBGAP_REPOS_ID,

    TOOLS_HDR_ID = 800,
    TOOLS_PREFETCH_LBL_ID, TOOLS_PREFETCH_BOX_ID, TOOLS_PREFETCH_ID
};

enum e_page
{
    PAGE_FIXED, PAGE_MAIN, PAGE_CACHE, PAGE_AWS, PAGE_GCP, PAGE_NETW, PAGE_DBGAP, PAGE_TOOLS
};

/* ==== pick a directory ========================================================================== */
static bool pick_dir( Dlg &dlg, Tui_Rect r, std::string &path )
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

/* ==== pick a file ========================================================================== */
static std::string pick_file( Dlg &dlg, Tui_Rect r, const char * path, const char *ext )
{
    std::string res = "";
    Std_Dlg_File_Pick pick;

    pick.set_parent( &dlg );
    dlg.center( r );
    pick.set_location( r );
    if ( ext != NULL ) pick.set_ext( ext );
    pick.set_dir_h( ( r.get_h() - 7 ) / 2 );
    pick.set_text( path );

    if ( pick.execute() )
        res.assign( pick.get_text() );

    return res;
}

/* ==== helper to explain the error =============================================================== */
static bool on_set_location_error( Dlg &dlg, ESetRootState s )
{
    bool res = false;
    switch ( s )
    {
        case eSetRootState_NotChanged       : res = true; break;
        case eSetRootState_NotUnique        : msg_ctrl::show_msg( dlg, "location not unique, select a different one" ); break;
        case eSetRootState_MkdirFail        : msg_ctrl::show_msg( dlg, "could not created directory, maybe permisson problem" ); break;
        case eSetRootState_NewPathEmpty     : msg_ctrl::show_msg( dlg, "you gave me an empty path" ); break;
        case eSetRootState_NewDirNotEmpty   : msg_ctrl::show_msg( dlg, "the given location is not empty" ); break;
        case eSetRootState_NewNotDir        : msg_ctrl::show_msg( dlg, "new location is not a directory" ); break;
        case eSetRootState_Error            : msg_ctrl::show_msg( dlg, "error changing location" ); break;
        default                             : msg_ctrl::show_msg( dlg, "unknown enum" ); break;
    }
    return res;
}

/* ==== pick public location ================================================================= */
static std::string public_location_start_dir( vdbconf_model * model )
{
    std::string res = model -> get_public_location();

    if ( !model -> does_path_exist( res ) )
        res = model -> get_user_default_dir();

    if ( !model -> does_path_exist( res ) )
        res = model -> get_home_dir() + "/ncbi";

    if ( !model -> does_path_exist( res ) )
        res = model -> get_home_dir();

    if ( !model -> does_path_exist( res ) )
        res = model -> get_current_dir();

    return res;
}

static bool pick_public_location( Dlg &dlg, vdbconf_model * model )
{
    bool res = false;
	std::string path = public_location_start_dir( model );

	if ( model -> does_path_exist( path ) )
		res = pick_dir( dlg, dlg.center( 5, 5 ), path );
	else
		res = input_ctrl::input( dlg, "location of public cache", path, 256 );

    if ( res && path.length() > 0 )
    {
        std::ostringstream q;
        q << "do you want to change the location to '" << path << "' ?";
        if ( question_ctrl::question( dlg, q.str().c_str() ) )
        {
            bool flushOld = false;
            bool reuseNew = false;
            ESetRootState s = model -> set_public_location( flushOld, path, reuseNew );
            switch ( s )
            {
                case eSetRootState_OK               : res = true; break;

                case eSetRootState_OldNotEmpty      : if ( question_ctrl::question( dlg, "prev. location is not empty, flush it?" ) )
                                                      {
                                                            flushOld = true;
                                                            s = model -> set_public_location( flushOld, path, reuseNew );
                                                            res = ( s == eSetRootState_OK );
                                                            if ( !res )
                                                                res = on_set_location_error( dlg, s );
                                                      }
                                                      break;

                default : res = on_set_location_error( dlg, s ); break;
            }
        }
    }
    return res;
}

/* ==== pick protected location ================================================================= */
static std::string protected_location_start_dir( vdbconf_model * model, uint32_t id )
{
    std::string res = model -> get_repo_location( id );

    if ( !model -> does_path_exist( res ) )
        res = model -> get_user_default_dir();

    if ( !model -> does_path_exist( res ) )
        res = model -> get_home_dir() + "/ncbi";

    if ( !model -> does_path_exist( res ) )
        res = model -> get_home_dir();

    if ( !model -> does_path_exist( res ) )
        res = model -> get_current_dir();

    return res;
}

static bool pick_protected_location( Dlg &dlg, vdbconf_model * model, uint32_t id )
{
    bool res = false;
	std::string path = protected_location_start_dir( model, id ); // above

	if ( model -> does_path_exist( path ) )
		res = pick_dir( dlg, dlg.center( 5, 5 ), path );
	else
		res = input_ctrl::input( dlg, "location of dbGaP project", path, 256 );

    if ( res && path.length() > 0 )
    {
        std::ostringstream q;
        q << "do you want to change the loction of '" << model -> get_repo_name( id ) << "' to '" << path << "' ?";
        if ( question_ctrl::question( dlg, q.str().c_str() ) )
        {
            bool flushOld = false;
            bool reuseNew = false;
            ESetRootState s = model -> set_repo_location( id, flushOld, path, reuseNew );
            switch ( s )
            {
            case eSetRootState_OK               :  res = true; break;

            case eSetRootState_OldNotEmpty      :  if ( question_ctrl::question( dlg, "prev. location is not empty, flush it?" ) )
                                                   {
                                                        flushOld = true;
                                                        s = model -> set_repo_location( id, flushOld, path, reuseNew );
                                                        res = ( s == eSetRootState_OK );
                                                        if ( !res )
                                                            res = on_set_location_error( dlg, s );
                                                   }
                                                   break;

            default : res = on_set_location_error( dlg, s ); break;
            }
        }
    }
    return res;
}

/* ==== set default import path for protected repositories ======================================= */
static bool set_dflt_import_path( Dlg &dlg, vdbconf_model * model )
{
    bool res = false;
    std::string path = model -> get_dflt_import_path_start_dir();

    if ( model -> does_path_exist( path ) )
        res = pick_dir( dlg, dlg.center( 5, 5 ), path );
    else
        res = input_ctrl::input( dlg, "change default import path", path, 128 );

    if ( res )
    {
        model -> set_user_default_dir( path.c_str() );
        msg_ctrl::show_msg( dlg, "default import path changed" );
    }
    return res;
}

/* ==== import ngs file ========================================================================= */
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

static bool import_this_ngc_into_this_location( Dlg &dlg, vdbconf_model * model,
                                            std::string &location, const KNgcObj * ngc )
{
    uint32_t result_flags = 0;
    bool res = model -> import_ngc( location, ngc, INP_CREATE_REPOSITORY, &result_flags );
    if ( res )
    {
        /* we have it imported or it exists and no changes made */
        bool modified = false;
        if ( result_flags & INP_CREATE_REPOSITORY )
        {
            /* success is the most common outcome, the repository was created */
            msg_ctrl::show_msg( dlg, "project successfully imported" );
            modified = true;
        }
        else
        {
            /* repository did exist and is completely identical to the given ngc-obj */
            msg_ctrl::show_msg( dlg, "this project exists already, no changes made" );
        }

        if ( question_ctrl::question( dlg, "do you want to change the location?" ) )
        {
            uint32_t id;
            if ( model -> get_id_of_ngc_obj( ngc, &id ) )
                modified |= pick_protected_location( dlg, model, id );
            else
                msg_ctrl::show_msg( dlg, "cannot find the imported repostiory" );
        }

        if ( modified )
        {
            model -> commit();
            model -> mkdir( ngc );
        }
    }
    else if ( result_flags == 0 )
    {
        /* we are here if there was an error executing one of the internal functions */
        msg_ctrl::show_msg( dlg, "there was an internal error importing the ngc-object" );
    }
    else
    {
        bool permitted = true;

        msg_ctrl::show_msg( dlg, "the repository does already exist!" );
        if ( result_flags & INP_UPDATE_ENC_KEY )
            permitted = question_ctrl::question( dlg, "encryption-key would change, continue ?" );

        if ( permitted && ( result_flags & INP_UPDATE_DNLD_TICKET ) )
            permitted = question_ctrl::question( dlg, "download-ticket would change, continue ?" );

        if ( permitted && ( result_flags & INP_UPDATE_DESC ) )
            permitted = question_ctrl::question( dlg, "description would change, continue ?" );

        if ( permitted )
        {
            uint32_t result_flags2 = 0;
            res = model -> import_ngc( location, ngc, result_flags, &result_flags2 );
            if ( res )
            {
                msg_ctrl::show_msg( dlg, "project successfully updated" );
                if ( question_ctrl::question( dlg, "do you want to change the location?" ) )
                {
                    uint32_t id; /* we have to find out the id of the imported/existing repository */
                    if ( model -> get_id_of_ngc_obj( ngc, &id ) )
                        pick_protected_location( dlg, model, id );
                    else
                        msg_ctrl::show_msg( dlg, "cannot find the imported repostiory" );
                }
                model ->commit();
            }
            else
                msg_ctrl::show_msg( dlg, "there was an internal error importing the ngc-object" );
        }
        else
            msg_ctrl::show_msg( dlg, "the import was canceled" );
    }
    return res;
}

static bool import_this_ngc( Dlg &dlg, vdbconf_model * model, const KNgcObj * ngc )
{
    bool res = false;

    std::string location_base = model -> get_user_default_dir();
    std::string location = model -> get_ngc_root( location_base, ngc );
    ESetRootState es = model -> prepare_repo_directory( location );
    switch ( es )
    {
        case eSetRootState_OK           :  res = import_this_ngc_into_this_location( dlg, model, location, ngc );
                                           break;

        case eSetRootState_OldNotEmpty  :  if ( question_ctrl::question( dlg, "repository location is not empty, use it?" ) )
                                           {
                                                es = model -> prepare_repo_directory( location, true );
                                                if ( es == eSetRootState_OK )
                                                    res = import_this_ngc_into_this_location( dlg, model, location, ngc );
                                                else
                                                    res = on_set_location_error( dlg, es );
                                            }
                                            break;

        default : res = on_set_location_error( dlg, es ); break;
    }
    return res;
}


static std::string get_import_ngc_start_dir( vdbconf_model * model )
{
    std::string res = model -> get_home_dir();
    if ( !model -> does_path_exist( res ) )
        res = model -> get_current_dir();
    return res;
}

static bool import_ngc( Dlg &dlg, vdbconf_model * model )
{
    bool res = false;
    std::string start_dir = get_import_ngc_start_dir( model );
    /* ( 1 ) pick a ngc-file */
    std::string picked = pick_file( dlg, dlg.center( 5, 5 ), start_dir.c_str(), "ngc" );
    if ( picked.length() > 0 )
    {
        std::ostringstream q;
        q << "do you want to import '" << picked << "' ?";
        /* ( 2 ) confirm the choice */
        if ( question_ctrl::question( dlg, q.str().c_str() ) )
        {
            const KNgcObj * ngc;
            if ( make_ngc_obj( &ngc, picked ) )
            {
                res = import_this_ngc( dlg, model, ngc );
                KNgcObjRelease( ngc );
            }
        }
    }
    return res;
}

/* ==== helper-model for the dbGaP - grid ======================================================= */
class vdbconf_grid : public Grid
{
    public:
        vdbconf_grid( void * data ) : Grid( data )
        {
            show_header( true );
            show_row_header( 16 );
            show_h_scroll( false );
            show_v_scroll( true );
        }

        virtual tui_long Get_Col_Count( uint32_t widget_width, void * data ) { return 1; };
        virtual tui_long Get_Row_Count( uint32_t widget_width, void * data )
        {
            return get_model( data ) -> get_repo_count(); /* model connection */
        };

        virtual tui_long Get_Col_Width( tui_long col, uint32_t widget_width, void * data ) { return widget_width - 17; };

        virtual void Col_Hdr_Request( tui_long col, uint32_t col_width, void * data, char * buffer, size_t buffer_size )
        {
            string_printf ( buffer, buffer_size, NULL, "location" );
        };

        virtual void Row_Hdr_Request( tui_long row, uint32_t col_width, void * data, char * buffer, size_t buffer_size )
        {
            std::string name = get_model( data ) -> get_repo_name( ( uint32_t )row );
            string_printf ( buffer, buffer_size, NULL, "%s", name.c_str() );
        };

        virtual void Cell_Request( tui_long col, tui_long row, uint32_t col_width, void * data, char * buffer, size_t buffer_size )
        {
            std::string path = get_model( data ) -> get_repo_location( ( uint32_t )row );
            string_printf ( buffer, buffer_size, NULL, "%s", path.c_str() ) ;
        }

    private:
        vdbconf_model * get_model( void * data ) { return static_cast< vdbconf_model * >( data ); }
};

/* ==== the view, just presenting the values, no logic ================================================ */
class vdbconf_view2 : public Dlg
{
    public :
        // constructor
        vdbconf_view2( vdbconf_model &mod ) : Dlg(), model( mod ), grid( &mod )
        {
            populate( GetRect(), false );
            SetActivePage( PAGE_MAIN );
            EnableCursorNavigation( false );
        }

        // called by controller if user has resized the windows
        virtual bool Resize( Tui_Rect const &r )
        {
            populate( r, true );
            return Dlg::Resize( r );
        }

        // called by controller if user has switched the active page
        virtual void onPageChanged( uint32_t old_page, uint32_t new_page )
        {
            tui_id hdr_id = page_id_2_hdr_id( old_page );
            if ( hdr_id > 0 )
                SetWidgetBackground( hdr_id, STATUS_COLOR );
            hdr_id = page_id_2_hdr_id( new_page );
            if ( hdr_id > 0 )
            {
                SetWidgetBackground( hdr_id, BOX_COLOR );
                SetFocus( hdr_id );
            }
            update();
        }

        // called by controller after reload/default/set credentials ...
        bool update( void ) { populate( GetRect(), false ); Draw( false ); return true; }

        // called by controller to update the status-line text
        void status_txt( const char * txt ) { SetWidgetCaption( STATUS_ID, txt ); }

    private :
        vdbconf_model &model;   // store model
        vdbconf_grid grid;      // store the intermediate grid-model

        std::string proxy_of_proxyport( const std::string proxy_port )
        {
            std::string res = "";
            std::string delim = ":";
            size_t start = 0;
            size_t end = proxy_port.find( delim );
            if ( end != std::string::npos )
                res = proxy_port.substr( start, end - start );
            return res;
        }

        std::string port_of_proxyport( const std::string proxy_port )
        {
            std::string res;
            std::string delim = ":";
            size_t start = 0U;
            size_t end = proxy_port.find( delim );
            while ( end != std::string::npos )
            {
                start = end + delim.length();
                end = proxy_port.find( delim, start );
            }
            res = proxy_port.substr( start, end );
            if ( res.empty() ) res = "0";
            return res;
        }

        tui_id page_id_2_hdr_id( uint32_t page_id )
        {
            tui_id hdr_id = 0;
            switch( page_id )
            {
                case PAGE_MAIN  : hdr_id = MAIN_HDR_ID; break;
                case PAGE_CACHE : hdr_id = CACHE_HDR_ID; break;
                case PAGE_AWS   : hdr_id = AWS_HDR_ID; break;
                case PAGE_GCP   : hdr_id = GCP_HDR_ID; break;
                case PAGE_NETW  : hdr_id = NETW_HDR_ID; break;
                case PAGE_DBGAP : hdr_id = DBGAP_HDR_ID; break;
                case PAGE_TOOLS : hdr_id = TOOLS_HDR_ID; break;
            }
            return hdr_id;
        }

        // block of rectangles for populating widgets below
        Tui_Rect save_and_exit_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 2, r.get_w() - 2, 1 ); }
        Tui_Rect TAB_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 4, r.get_w() - 2, r.get_h() - 6 ); }
        Tui_Rect BODY_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +1 , r.get_w(), r.get_h() - 1 ); }
        Tui_Rect CB_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , 36, 1 ); }
        Tui_Rect lbl1_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x(), r.get_y() + y , r.get_w() - 2, 1 ); }
        Tui_Rect choose_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , 12, 1 ); }
        Tui_Rect use_repo_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , 34, 1 ); }
        Tui_Rect zero_qual_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , r.get_w() - 2, 1 ); }
        Tui_Rect file_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +14, r.get_y() + y , r.get_w() -15, 1 ); }
        Tui_Rect prof_lbl_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x(), r.get_y() + y , 10, 1 ); }
        Tui_Rect CACHE_RADIO_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() + 1, r.get_y() +2 , 24, 3 ); }
        Tui_Rect prof_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +14, r.get_y() + y , 32, 1 ); }
        Tui_Rect proxy_lbl_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x(), r.get_y() + y , 7, 1 ); }
        Tui_Rect proxy_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +8, r.get_y() + y , 32, 1 ); }
        Tui_Rect proxy_port_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +8, r.get_y() + y , 10, 1 ); }
        Tui_Rect imp_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , 30, 1 ); }
        Tui_Rect imp_path_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +32, r.get_y() + y , 30, 1 ); }
        Tui_Rect repo_lbl_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , 21, 1 ); }
        Tui_Rect ram_pages_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +14, r.get_y() + y , 15, 1 ); }
        Tui_Rect ram_unit_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +30, r.get_y() + y , 8, 1 ); }
        Tui_Rect repo_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , r.get_w() -2, r.get_h() - ( y + 1 ) ); }
        Tui_Rect pf_lbl_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , r.get_w() -2, 1 ); }
        Tui_Rect pf_box_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , r.get_w() -2, 4 ); }
        Tui_Rect pf_cb_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +2, r.get_y() + y , 32, 2 ); }
        Tui_Rect bottom( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() + r.get_h() -2 , r.get_w() -2, 1 ); }

        Tui_Rect HDR_rect( Tui_Rect const &r, uint32_t ident )
        {
            tui_coord x = r.get_x() + ( 10 * ident );
            return Tui_Rect( x, r.get_y(), 9, 1 );
        }

        // used in populate...
        void set_status_line( Tui_Rect r )
        {
            std::stringstream ss;
            ss << " w:" << r.get_w() << " h:" << r.get_h();

            r.change( 0, r.get_h() - 1, 0, - ( r.get_h() - 1 ) );
            if ( HasWidget( STATUS_ID ) )
            {
                SetWidgetRect( STATUS_ID, r, false );
                SetWidgetCaption( STATUS_ID, ss.str().c_str() );
            }
            else
                AddLabel( STATUS_ID, r, ss.str().c_str() );
        }

        // populate the top row of switches
        void populate_save_and_exit( Tui_Rect const &r, bool resize )
        {
            Tui_Rect rr = Tui_Rect( r.get_x(), r.get_y(), 14, 1 );
            PopulateButton( rr, resize, SAVE_BTN_ID,  "&save", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, EXIT_BTN_ID,  "e&xit", BTN_COLOR_BG, BTN_COLOR_FG );
            if ( With_Verfify )
            {
                rr.change( rr.get_w() + 2, 0, 0, 0 );
                PopulateButton( rr, resize, VERIFY_BTN_ID, "&verify", BTN_COLOR_BG, BTN_COLOR_FG );
            }
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, DISCARD_BTN_ID, "&discard", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, DEFAULT_BTN_ID, "de&fault", BTN_COLOR_BG, BTN_COLOR_FG );
        }

        void populate_tab_headers( Tui_Rect const &r, bool resize )
        {
            uint32_t ident = 0;
            PopulateTabHdr( HDR_rect( r, ident++ ), resize, MAIN_HDR_ID, "&MAIN", STATUS_COLOR, LABEL_FG );
            PopulateTabHdr( HDR_rect( r, ident++ ), resize, CACHE_HDR_ID, "&CACHE", STATUS_COLOR, LABEL_FG );
            PopulateTabHdr( HDR_rect( r, ident++ ), resize, AWS_HDR_ID, "&AWS", STATUS_COLOR, LABEL_FG );
            PopulateTabHdr( HDR_rect( r, ident++ ), resize, GCP_HDR_ID, "&GCP", STATUS_COLOR, LABEL_FG );
            PopulateTabHdr( HDR_rect( r, ident++ ), resize, NETW_HDR_ID, "&NET", STATUS_COLOR, LABEL_FG );

            if ( With_DbGaP )
                PopulateTabHdr( HDR_rect( r, ident++ ), resize, DBGAP_HDR_ID, "d&bGaP", STATUS_COLOR, LABEL_FG );

            PopulateTabHdr( HDR_rect( r, ident ), resize, TOOLS_HDR_ID, "&TOOLS", STATUS_COLOR, LABEL_FG );
        }

        // populate the MAIN page
        void populate_MAIN( Tui_Rect const &r, bool resize, uint32_t page_id )
        {
            tui_coord y = 2;
            PopulateCheckbox( use_repo_rect( r, y ), resize, MAIN_USE_REMOTE_ID, "&Enable Remote Access",
                              model.is_remote_enabled(), // model-connection
                              CB_COLOR_BG, CB_COLOR_FG, page_id );
            if ( model.does_site_repo_exist() )
            {
                y += 2;
                PopulateCheckbox( use_repo_rect( r, y ), resize, MAIN_USE_SITE_ID, "&Use Site Installation",
                              model.is_site_enabled(), // model-connection
                              CB_COLOR_BG, CB_COLOR_FG, page_id );
            }

            y += 2;
            PopulateCheckbox( zero_qual_rect( r, y ), resize, MAIN_FULL_QUALITY,
            "Prefer SRA Lite files with simplified base &Quality scores",
                              model.get_full_quality(), // model-connection
                              CB_COLOR_BG, CB_COLOR_FG, page_id );

#ifdef TELEMETRY_VISIBLE
            y += 2;
            PopulateCheckbox( zero_qual_rect( r, y ), resize, MAIN_TELEMETRY,
            "Send te&lemetry ( options used for command-line tools ) to NCBI",
                              model.get_telemetry(), // model-connection
                              CB_COLOR_BG, CB_COLOR_FG, page_id );
#endif

            /* the GUID-label at the bottom */
            std::stringstream ss;
            ss << "GUID: " << model.get_guid();
            PopulateLabel( bottom( r ), resize, MAIN_GUID_ID, ss.str().c_str(), BOX_COLOR, LABEL_FG, page_id );
        }

        // populate the CACHE page
        void populate_CACHE( Tui_Rect const &r, bool resize, uint32_t page_id )
        {
            tui_coord y = 2;
            PopulateCheckbox( use_repo_rect( r, y ), resize, CACHE_USE_CACHE_ID, "enable local f&ile-caching",
                              model.is_user_cache_enabled(), // model-connection
                              CB_COLOR_BG, CB_COLOR_FG, page_id );
            y += 2;
            PopulateLabel( lbl1_rect( r, y ), resize, CACHE_REPO_LBL_ID, "location of user-repository:",
                                BOX_COLOR, LABEL_FG, page_id );
            y++;
            PopulateButton( choose_rect( r, y ), resize, CACHE_REPO_CHOOSE_ID, "ch&oose",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            PopulateLabel( file_rect( r, y ), resize, CACHE_REPO_PATH_ID,
                                model.get_public_location().c_str(), /* model-connection */
                                LABEL_BG, INP_COLOR_FG, page_id );
            y += 2;
            PopulateButton( choose_rect( r, y ), resize, CACHE_REPO_CLEAR_ID, "c&lear",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            y += 2;
            PopulateLabel( lbl1_rect( r, y ), resize, CACHE_PROC_LBL_ID, "process-local location:",
                                BOX_COLOR, LABEL_FG, page_id );
            y++;
            PopulateButton( choose_rect( r, y ), resize, CACHE_PROC_CHOOSE_ID, "choos&e",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            PopulateLabel( file_rect( r, y ), resize, CACHE_PROC_PATH_ID,
                                model.get_temp_cache_location().c_str(), /* model-connection */
                                LABEL_BG, INP_COLOR_FG, page_id );
            y += 2;
            PopulateButton( choose_rect( r, y ), resize, CACHE_PROC_CLEAR_ID, "clea&r",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            y += 2;
            PopulateLabel( lbl1_rect( r, y ), resize, CACHE_RAM_LBL_ID, "RAM &used:",
                                BOX_COLOR, LABEL_FG, page_id );
            PopulateSpinEdit( ram_pages_rect( r, y ), resize, CACHE_RAM_ID,
                                model.get_cache_amount_in_MB(), /* model-connection */
                                1 /* min */, 1024 * 16 /* max */, STATUS_COLOR, LABEL_FG, page_id );
            PopulateLabel( ram_unit_rect( r, y ), resize, CACHE_RAM_UNIT_ID, "MB",
                                BOX_COLOR, LABEL_FG, page_id );
        }

        // populate the AWS page
        void populate_AWS( Tui_Rect const &r, bool resize, uint32_t page_id )
        {
            tui_coord y = 2;
            PopulateCheckbox( CB_rect( r, y ), resize, AWS_CHARGES_ID, "acc&ept charges for AWS",
                                model.does_user_accept_aws_charges(),   /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, page_id );

            y += 2;
            PopulateCheckbox( CB_rect( r, y ), resize, AWS_REPORT_ID, "&report cloud instance identity",
                                model.report_cloud_instance_identity(),   /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, page_id );

            y += 2;
            PopulateLabel( lbl1_rect( r, y ), resize, AWS_KEY_ID, "credentials:",
                                BOX_COLOR, LABEL_FG, page_id );
            y++;
            PopulateButton( choose_rect( r, y ), resize, AWS_CHOOSE_ID, "ch&oose",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            PopulateLabel( file_rect( r, y ), resize, AWS_FILE_ID,
                                model.get_aws_credential_file_location().c_str(), /* model-connection */
                                LABEL_BG, INP_COLOR_FG, page_id );
            y += 2;
            PopulateButton( choose_rect( r, y ), resize, AWS_CLEAR_ID, "c&lear",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            y += 2;
            PopulateLabel( prof_lbl_rect( r, y ), resize, AWS_PROF_LBL_ID, "&profile:",
                                BOX_COLOR, LABEL_FG, page_id );
            PopulateInput( prof_rect( r, y ), resize, AWS_PROF_ID,
                                model.get_aws_profile().c_str(), /* model-connection */
                                64, INP_COLOR_BG, INP_COLOR_FG, page_id );
        }

        // populate the GCP page
        void populate_GCP( Tui_Rect const &r, bool resize, uint32_t page_id )
        {
            tui_coord y = 2;
            PopulateCheckbox( CB_rect( r, y ), resize, GCP_CHARGES_ID, "acc&ept charges for GCP",
                                model.does_user_accept_gcp_charges(), /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, page_id );

            y += 2;
            PopulateCheckbox( CB_rect( r, y ), resize, GCP_REPORT_ID, "&report cloud instance identity",
                                model.report_cloud_instance_identity(),   /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, page_id );

            y += 2;
            PopulateLabel( lbl1_rect( r, y ), resize, GCP_KEY_ID, "credentials:",
                                BOX_COLOR, LABEL_FG, page_id );
            y++;
            PopulateButton( choose_rect( r, y ), resize, GCP_CHOOSE_ID, "ch&oose",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            PopulateLabel( file_rect( r, y ), resize, GCP_FILE_ID,
                                model.get_gcp_credential_file_location().c_str(), /* model-connection */
                                LABEL_BG, INP_COLOR_FG, page_id );
            y += 2;
            PopulateButton( choose_rect( r, y ), resize, GCP_CLEAR_ID, "c&lear",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
        }

        // populate the NETWORK page
        void populate_NETW( Tui_Rect const &r, bool resize, uint32_t page_id )
        {
            tui_coord y = 2;
            PopulateCheckbox( CB_rect( r, y ), resize, NETW_USE_PROXY_ID, "&use http-proxy",
                                model.is_http_proxy_enabled(), /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, page_id );
            y += 2;
            PopulateLabel( proxy_lbl_rect( r, y ), resize, NETW_PROXY_LBL_ID, "&proxy:",
                                BOX_COLOR, LABEL_FG, page_id );
            PopulateInput( proxy_rect( r, y ), resize, NETW_PROXY_ID,
                                proxy_of_proxyport( model.get_http_proxy_path() ).c_str(), /* model-connection */
                                64, INP_COLOR_BG, INP_COLOR_FG, page_id );
            y += 2;
            PopulateLabel( proxy_lbl_rect( r, y ), resize, NETW_PROXY_PORT_LBL_ID, "p&ort:",
                                BOX_COLOR, LABEL_FG, page_id );
            PopulateInput( proxy_port_rect( r, y ), resize, NETW_PROXY_PORT_ID,
                                port_of_proxyport( model.get_http_proxy_path() ).c_str(), /* model-connection */
                                12, INP_COLOR_BG, INP_COLOR_FG, page_id );
            SetWidgetAlphaMode( NETW_PROXY_PORT_ID, 1 ); /* turn it into numbers only */
        }

        // populate the DBGAP page
        void populate_DBGAP( Tui_Rect const &r, bool resize, uint32_t page_id )
        {
            tui_coord y = 2;
            PopulateButton( imp_rect( r, y ), resize, DBGAP_IMPORT_KEY_ID, "&Import Repository key",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            PopulateButton( imp_path_rect( r, y ), resize,  DBGAP_IMPORT_PATH_ID, "Set Defau&lt Import Path",
                                BTN_COLOR_BG, BTN_COLOR_FG, page_id );
            y += 2;
            PopulateLabel( repo_lbl_rect( r, y ), resize, DBGAP_REPOS_LBL_ID, "dbGa&p repositories:",
                           STATUS_COLOR, LABEL_FG, page_id );
            y++;
            PopulateGrid( repo_rect( r, y ), resize, DBGAP_REPOS_ID, grid, BTN_COLOR_BG, BTN_COLOR_FG, page_id );
        }

        // populate the TOOLS page
        void populate_TOOLS( Tui_Rect const &r, bool resize, uint32_t page_id )
        {
            tui_coord y = 2;
            PopulateLabel( pf_lbl_rect( r, y ), resize, TOOLS_PREFETCH_LBL_ID, "&prefetch downloads to",
                                CB_COLOR_FG, LABEL_FG, page_id );
            y++;
            PopulateLabel( pf_box_rect( r, y ), resize, TOOLS_PREFETCH_BOX_ID, NULL,
                                STATUS_COLOR, LABEL_FG, page_id );
            y++;
            PopulateRadioBox( pf_cb_rect( r, y ), resize, TOOLS_PREFETCH_ID, CB_COLOR_BG, CB_COLOR_FG, page_id );
            if ( GetWidgetStringCount( TOOLS_PREFETCH_ID ) == 0 )
            {
                AddWidgetStringN( TOOLS_PREFETCH_ID, 2,
                                  "user-repository", "current directory" );
                bool value = model.does_prefetch_download_to_cache();
                SetWidgetSelectedString( TOOLS_PREFETCH_ID, value ? 0 : 1 );
            }
        }

        // populate all widgets
        void populate( Tui_Rect const &r, bool resize )
        {
            SetCaption( "SRA configuration" );
            set_status_line( r );
            populate_save_and_exit( save_and_exit_rect( r ), resize );
            Tui_Rect tab_rect = TAB_rect( r );

            PopulateLabel( BODY_rect( tab_rect ), resize, BOX_ID, NULL, BOX_COLOR, LABEL_FG );
            populate_tab_headers( tab_rect, resize );
            populate_MAIN( tab_rect, resize, PAGE_MAIN );
            populate_CACHE( tab_rect, resize, PAGE_CACHE );
            populate_AWS( tab_rect, resize, PAGE_AWS );
            populate_GCP( tab_rect, resize, PAGE_GCP );
            populate_NETW( tab_rect, resize, PAGE_NETW );
            if ( With_DbGaP )
                populate_DBGAP( tab_rect, resize, PAGE_DBGAP );
            populate_TOOLS( tab_rect, resize, PAGE_TOOLS );
        }
};

/* ==== the controller, reacting to user-input ================================================ */
class vdbconf_ctrl2 : public Dlg_Runner
{
    public :
        // constructor
        vdbconf_ctrl2( Dlg &dlg, vdbconf_model &mod ) : Dlg_Runner( dlg, &mod )
        {
            dlg.SetFocus( SAVE_BTN_ID );
        };

        // called by base-class if user has changed the value of an input-field
        virtual bool on_changed( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            vdbconf_model * model = ( vdbconf_model * ) data;
            tui_id id = dev.get_widget_id();
            switch( id )
            {
                case AWS_PROF_ID   : model -> set_aws_profile( dlg.GetWidgetText( id ) ); break; /* model-connection */
                case NETW_PROXY_ID      : on_port( dlg, model ); break;
                case NETW_PROXY_PORT_ID : on_port( dlg, model ); break;
                case CACHE_RAM_ID  : model->set_cache_amount_in_MB( dlg.GetWidgetInt64Value( CACHE_RAM_ID ) ); break;
                case TOOLS_PREFETCH_ID : on_prefetch_dnld( dlg, model ); break;
            }
            return true;
        }

        // called by base-class if user has changed a checkbox or pressed a button
        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            bool res = true;
            vdbconf_model * model = static_cast< vdbconf_model * >( data );
            switch( dev.get_widget_id() )
            {
                case SAVE_BTN_ID    : res = on_save( dlg, model ); break;
                case EXIT_BTN_ID    : res = on_exit( dlg, model ); break;
                case VERIFY_BTN_ID  : res = on_verify( dlg, model ); break;
                case DISCARD_BTN_ID : res = on_discard( dlg, model ); break;
                case DEFAULT_BTN_ID : res = on_default( dlg, model ); break;

                case MAIN_HDR_ID        : res = dlg.SetActivePage( PAGE_MAIN ); break;
                case MAIN_USE_REMOTE_ID : res = on_remote_repo( dlg, model ); break;
                case MAIN_USE_SITE_ID   : res = on_site_repo( dlg, model ); break;
                case MAIN_FULL_QUALITY  : res = on_full_quality( dlg, model ); break;

#ifdef TELEMETRY_VISIBLE
                case MAIN_TELEMETRY     : res = on_telemetry( dlg, model ); break;
#endif

                case CACHE_USE_CACHE_ID  : res = on_use_cache( dlg, model ); break;
                case CACHE_HDR_ID         : res = dlg.SetActivePage( PAGE_CACHE ); break;
                case CACHE_REPO_CHOOSE_ID : res = choose_main_local( dlg, model ); break;
                case CACHE_REPO_CLEAR_ID  : res = clear_main_local( dlg, model ); break;
                case CACHE_PROC_CHOOSE_ID : res = choose_main_proc( dlg, model ); break;
                case CACHE_PROC_CLEAR_ID  : res = clear_main_proc( dlg, model ); break;
                case CACHE_RAM_LBL_ID     : dlg.SetFocus( CACHE_RAM_ID ); break;

                case AWS_HDR_ID      : res = dlg.SetActivePage( PAGE_AWS ); break;
                case AWS_CHARGES_ID  : res = on_accept_aws_charges( dlg, model ); break;
                case AWS_REPORT_ID   : res = on_report_aws_identity( dlg, model ); break;
                case AWS_CHOOSE_ID   : res = on_aws_choose( dlg, model ); break;
                case AWS_CLEAR_ID    : res = on_aws_clear( dlg, model ); break;
                case AWS_PROF_LBL_ID : dlg.SetFocus( AWS_PROF_ID ); break;

                case GCP_HDR_ID     : res = dlg.SetActivePage( PAGE_GCP ); break;
                case GCP_CHARGES_ID : res = on_accept_gcp_charges( dlg, model ); break;
                case GCP_REPORT_ID  : res = on_report_gcp_identity( dlg, model ); break;
                case GCP_CHOOSE_ID  : res = on_gcp_choose( dlg, model ); break;
                case GCP_CLEAR_ID   : res = on_gcp_clear( dlg, model ); break;

                case NETW_HDR_ID            : res = dlg.SetActivePage( PAGE_NETW ); break;
                case NETW_USE_PROXY_ID      : res = on_use_proxy( dlg, model ); break;
                case NETW_PROXY_LBL_ID      : dlg.SetFocus( NETW_PROXY_ID ); break;
                case NETW_PROXY_PORT_LBL_ID : dlg.SetFocus( NETW_PROXY_PORT_ID ); break;

                case DBGAP_HDR_ID         : res = dlg.SetActivePage( PAGE_DBGAP ); break;
                case DBGAP_IMPORT_KEY_ID  : res = on_import_repo_key( dlg, model ); break;
                case DBGAP_IMPORT_PATH_ID : res = on_set_dflt_import_path( dlg, model ); break;
                case DBGAP_REPOS_LBL_ID   : dlg.SetFocus( DBGAP_REPOS_ID ); break;
                case DBGAP_REPOS_ID       : res = on_edit_dbgap_repo( dlg, model ); break;

                case TOOLS_HDR_ID          : res = dlg.SetActivePage( PAGE_TOOLS ); break;
                case TOOLS_PREFETCH_LBL_ID : dlg.SetFocus( TOOLS_PREFETCH_ID ); break;
            }
            return res;
        }

        // called by base-class if widget gets focus
        virtual bool on_focus( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            vdbconf_view2 &view = dynamic_cast<vdbconf_view2 &>( dlg );
            switch( dev.get_widget_id() )
            {
                case SAVE_BTN_ID    : view.status_txt( "save configuration" ); break;
                case EXIT_BTN_ID    : view.status_txt( "exit vdb-config tool" ); break;
                case VERIFY_BTN_ID  : view.status_txt( "verify configuration settings" ); break;
                case DISCARD_BTN_ID  : view.status_txt( "discard current changes" ); break;
                case DEFAULT_BTN_ID : view.status_txt( "load default settings" ); break;

                case MAIN_USE_REMOTE_ID : view.status_txt( "use remote repository" ); break;
                case MAIN_USE_SITE_ID   : view.status_txt( "use site repository" ); break;
                case MAIN_FULL_QUALITY  : view.status_txt( "request full qualities" ); break;

#ifdef TELEMETRY_VISIBLE
                case MAIN_TELEMETRY     : view.status_txt( "set telemetry usage" ); break;
#endif

                case CACHE_USE_CACHE_ID  : view.status_txt( "use local cache" ); break;
                case CACHE_REPO_CHOOSE_ID : view.status_txt( "choose loacation of local repository" ); break;
                case CACHE_REPO_CLEAR_ID  : view.status_txt( "clear loacation of local repository" ); break;
                case CACHE_PROC_CHOOSE_ID : view.status_txt( "choose loacation of process local storage" ); break;
                case CACHE_PROC_CLEAR_ID  : view.status_txt( "clear loacation of process local storage" ); break;

                case AWS_CHARGES_ID : view.status_txt( "do accept charges for AWS usage" ); break;
                case AWS_REPORT_ID  : view.status_txt( "report cloud instance identity to ..." ); break;
                case AWS_CHOOSE_ID  : view.status_txt( "choose location of credentials for AWS" ); break;
                case AWS_CLEAR_ID   : view.status_txt( "clear location of credentials for AWS" ); break;
                case AWS_PROF_ID    : view.status_txt( "enter name of profile to use for AWS" ); break;

                case GCP_CHARGES_ID : view.status_txt( "do accept charges for GCP usage" ); break;
                case GCP_REPORT_ID  : view.status_txt( "report cloud instance identity to ..." ); break;
                case GCP_CHOOSE_ID  : view.status_txt( "choose location of credentials for GCP" ); break;
                case GCP_CLEAR_ID   : view.status_txt( "clear location of credentials for GCP" ); break;

                case NETW_USE_PROXY_ID  : view.status_txt( "use a network proxy to access remote data" ); break;
                case NETW_PROXY_ID      : view.status_txt( "specify the proxy ( host-name/ip )to use" ); break;
                case NETW_PROXY_PORT_ID : view.status_txt( "specify the proxy-port to use" ); break;

                case DBGAP_IMPORT_KEY_ID  : view.status_txt( "import a ngc-file" ); break;
                case DBGAP_IMPORT_PATH_ID : view.status_txt( "set default import path for ngc-files" ); break;
                case DBGAP_REPOS_ID       : view.status_txt( "list of available dbGaP repositories, press ENTER to edit" ); break;

                case TOOLS_PREFETCH_ID   : view.status_txt( "choose where prefetch downloads files to" ); break;
            }
            return false;
        }

    private :
        // helper function to signal the view to update itself
        bool update_view( Dlg &dlg )
        {
            vdbconf_view2 &view = dynamic_cast<vdbconf_view2 &>( dlg );
            return view.update();
        }

        // ================ the save, exit, verify, reload and default - buttons :

        // user has pressed the save-button
        bool on_save( Dlg &dlg, vdbconf_model * model )
        {
            if ( model -> get_config_changed() )
            {
                if ( model -> commit() )
                    msg_ctrl::show_msg( dlg, "changes successfully saved" );
                else
                    msg_ctrl::show_msg( dlg, "error saving changes" );
            }
            else
                msg_ctrl::show_msg( dlg, "no changes to be saved" );
            return true;
        }

        // user has pressed the exit-button
        bool on_exit( Dlg &dlg, vdbconf_model * model )
        {
            if ( model -> get_config_changed() )
            {
                if ( question_ctrl::question( dlg, "save changes ?" ) )
                {
                    if ( model -> commit() )
                        msg_ctrl::show_msg( dlg, "changes successfully saved" );
                    else
                        msg_ctrl::show_msg( dlg, "error saving changes" );
                }
            }
            dlg.SetDone( true );
            return true;
        }

        // user has pressed the verify-button
        bool on_verify( Dlg &dlg, vdbconf_model * model )
        {
            return msg_ctrl::show_msg( dlg, "not yet implemented" );
            /*
            std::string p = model -> get_current_dir();
            bool picked = pick_path_ctrl::pick( dlg, "pick a path", p );
            return msg_ctrl::show_msg( dlg, picked ? "true" : "false" );
            */
        }

        // user has pressed the reload-button
        bool on_discard( Dlg &dlg, vdbconf_model * model )
        {
            if ( model -> get_config_changed() )
            {
                if ( question_ctrl::question( dlg, "discard changes ?" ) )
                {
                    model -> reload();
                    update_view( dlg );
                    msg_ctrl::show_msg( dlg, "changes discarded" );
                }
            }
            else
                msg_ctrl::show_msg( dlg, "There are no changes to be discarded." );
            return true;
        }

        // user has pressed the default-button
        bool on_default( Dlg &dlg, vdbconf_model * model )
        {
            if ( question_ctrl::question( dlg, "revert to default-values ?" ) )
            {
                model -> set_defaults();
                update_view( dlg );
                msg_ctrl::show_msg( dlg, "default values set" );
            }
            return true;
        }

        // ================ the MAIN-page: choose/clear for local repo and process local

        // user has pressed the 'use remote repo' checkbox
        bool on_remote_repo( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_remote_enabled( dlg.GetWidgetBoolValue( MAIN_USE_REMOTE_ID ) ); /* model-connection */
            return true;
        }

        // user has pressed the 'use site repo' checkbox
        bool on_site_repo( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_site_enabled( dlg.GetWidgetBoolValue( MAIN_USE_SITE_ID ) ); /* model-connection */
            return true;
        }

        // user has pressed the 'use full qualities' checkbox
        bool on_full_quality( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_full_quality( dlg.GetWidgetBoolValue( MAIN_FULL_QUALITY ) ); /* model-connection */
            return true;
        }

#ifdef TELEMETRY_VISIBLE
        // user has pressed the 'telemetry' checkbox
        bool on_telemetry( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_telemetry( dlg.GetWidgetBoolValue( MAIN_TELEMETRY ) ); /* model-connection */
            return true;
        }
#endif

        // user has pressed the 'prefetch download to cache' checkbox
        bool on_prefetch_dnld( Dlg &dlg, vdbconf_model *model )
        {
            bool value = ( dlg.GetWidgetSelectedString( TOOLS_PREFETCH_ID ) == 0 );
            model -> set_prefetch_download_to_cache( value ); /* model-connection */
            return true;
        }

        // ================ the CACHE-page:

        // user has pressed the 'use local cache' checkbox
        bool on_use_cache( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_user_cache_enabled( dlg.GetWidgetBoolValue( CACHE_USE_CACHE_ID ) ); /* model-connection */
            return true;
        }

        // user has pressed the choose-button for the public user repo location
        bool choose_main_local( Dlg &dlg, vdbconf_model * model )
        {
            if ( pick_public_location( dlg, model ) )
                return update_view( dlg );
            return false;
        }

        // user has pressed the clear-button for the public user repo location
        bool clear_main_local( Dlg &dlg, vdbconf_model * model )
        {
            model -> unset_public_repo_location();
            return update_view( dlg );
        }

        // user has pressed the choose-button for the process local cache location
        bool choose_main_proc( Dlg &dlg, vdbconf_model * model )
        {
            std::string path = model -> get_temp_cache_location();
            if ( path.empty() ) path = model -> get_current_dir();
            if ( pick_dir( dlg, Tui_Rect( 0, 0, 80, 40 ), path ) && !path.empty() )
                model -> set_temp_cache_location( path );
            return update_view( dlg );
        }

        // user has pressed the clear-button for the process local cache location
        bool clear_main_proc( Dlg &dlg, vdbconf_model * model )
        {
            std::string path( "" );
            model -> set_temp_cache_location( path );
            return update_view( dlg );
        }

        // ================ the AWS-page:

        // user has changed the 'accept AWS charges' checkbox
        bool on_accept_aws_charges( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_user_accept_aws_charges( dlg.GetWidgetBoolValue( AWS_CHARGES_ID ) ); /* model-connection */
            return true;
        }

        // user has changed the 'report cloud identity' checkbox
        bool on_report_aws_identity( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_report_cloud_instance_identity( dlg.GetWidgetBoolValue( AWS_REPORT_ID ) ); /* model-connection */
            return true;
        }

        // common code for on_aws_choose() and on_aws_clear() to set the credential path
        bool upate_aws_credentials( Dlg &dlg, vdbconf_model * model, std::string &path )
        {
            model -> set_aws_credential_file_location( path ); /* model-connection */
            return update_view( dlg );
        }

        // user has pressed the button to choose a path to AWS credentials
        bool on_aws_choose( Dlg &dlg, vdbconf_model * model )
        {
            std::string path = model -> get_aws_credential_file_location();
            if ( path.empty() ) path = model -> get_current_dir();
            if ( pick_dir( dlg, Tui_Rect( 0, 0, 80, 40 ), path ) && !path.empty() )
                upate_aws_credentials( dlg, model, path );
            return true;
        }

        // user has pressed the button to clear the path to AWS credentials
        bool on_aws_clear( Dlg &dlg, vdbconf_model * model )
        {
            std::string path( "" );
            return upate_aws_credentials( dlg, model, path );
        }


        // ================ the GCP-page:

        // user has changed the 'accept GCP charges' checkbox
        bool on_accept_gcp_charges( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_user_accept_gcp_charges( dlg.GetWidgetBoolValue( GCP_CHARGES_ID ) ); /* model-connection */
            return true;
        }

        // user has changed the 'report cloud identity' checkbox
        bool on_report_gcp_identity( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_report_cloud_instance_identity( dlg.GetWidgetBoolValue( GCP_REPORT_ID ) ); /* model-connection */
            return true;
        }

        // common for on_gcp_choose() and on_gcp_clear() to set credential file
        bool upate_gcp_credentials( Dlg &dlg, vdbconf_model * model, std::string &file )
        {
            model -> set_gcp_credential_file_location( file ); /* model-connection */
            return update_view( dlg );
        }

        // user has pressed the button to choose a GCP credentials file
        bool on_gcp_choose( Dlg &dlg, vdbconf_model * model )
        {
            /* on GCP: a file is choosen */
            std::string org = model ->get_gcp_credential_file_location();
            if ( org.empty() ) org = model -> get_current_dir();
            std::string file = pick_file( dlg, Tui_Rect( 0, 0, 80, 40 ), org.c_str(), NULL );
            if ( !file.empty() )
                upate_gcp_credentials( dlg, model, file );
            return true;
        }

        // user has pressed the button to clear the GCP credentials file
        bool on_gcp_clear( Dlg &dlg, vdbconf_model * model )
        {
            std::string file( "" );
            return upate_gcp_credentials( dlg, model, file );
        }

        // ================ the NETWORK-page:

        // the user has pressed the 'use proxy' checkbox
        bool on_use_proxy( Dlg &dlg, vdbconf_model *model )
        {
            model -> set_http_proxy_enabled( dlg.GetWidgetBoolValue( NETW_USE_PROXY_ID ) ); /* model-connection */
            return true;
        }

        bool on_port( Dlg &dlg, vdbconf_model * model )
        {
            std::string proxy = dlg.GetWidgetText( NETW_PROXY_ID );
            std::string port = dlg.GetWidgetText( NETW_PROXY_PORT_ID );
            std::stringstream ss;
            ss << proxy << ":" << port;
            model -> set_http_proxy_path( ss.str() );
            return true;
        }

        // ================ the DBGAP-page:

        // user has pressed the 'Import Repository key'-button on the DBGAP-page
        bool on_import_repo_key( Dlg &dlg, vdbconf_model * model )
        {
            if ( import_ngc( dlg, model ) )
                return update_view( dlg );
            return false;
        }

        // user has pressed the 'Set Default Import Path'-button on the DBGAP-page
        bool on_set_dflt_import_path( Dlg &dlg, vdbconf_model * model )
        {
            return set_dflt_import_path( dlg, model );
        }

        // user has pressed enter on one of the repository entries in the DBGAP-grid
        bool on_edit_dbgap_repo( Dlg &dlg, vdbconf_model * model )
        {
            tui_long n = dlg.GetWidgetInt64Value( DBGAP_REPOS_ID );
            if ( pick_protected_location( dlg, model, ( uint32_t )n ) )
                return update_view( dlg );
            return false;
        }
};

extern "C"
{
    rc_t run_interactive2 ( vdbconf_model & model )
    {
        rc_t rc = 0;
        try
        {
            /* try to get a GUID from config, create and store if none found */
            model.check_guid();

            /* (1) ... create a view */
            vdbconf_view2 view( model );

            /* (2) ... create derived controller, hand it the view and the model*/
            vdbconf_ctrl2 controller( view, model );

            /* (3) ... let the controller handle the events */
            controller.run();

            /* (4) call this before leaving main() to terminate the low-level driver... */
            Tui::clean_up();
        }
        catch ( ... )
        {
            rc = RC( rcExe, rcNoTarg, rcExecuting, rcNoObj, rcUnknown );
        }
        return rc;
    }
}
