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
#include <tui/tui.hpp>

#include <sstream>
#include <iostream>
#include <cstdarg>

#include "vdb-config-model.hpp"

using namespace tui;

/* ---------------------------------------------------------------------------

    We want it to act on window-resize and show the new size in the status bar :
    
    press ESC twice to get out of the controller-loop
    
--------------------------------------------------------------------------- */
const KTUI_color BOX_COLOR      = KTUI_c_dark_blue;
const KTUI_color STATUS_COLOR	= KTUI_c_gray;
const KTUI_color LABEL_BG		= KTUI_c_light_gray;
const KTUI_color LABEL_FG		= KTUI_c_white;
const KTUI_color CB_COLOR_FG	= KTUI_c_black;
const KTUI_color CB_COLOR_BG	= KTUI_c_cyan;
const KTUI_color BTN_COLOR_FG	= KTUI_c_black;
const KTUI_color BTN_COLOR_BG	= KTUI_c_cyan;
const KTUI_color INP_COLOR_FG	= KTUI_c_black;
const KTUI_color INP_COLOR_BG	= KTUI_c_white;

const tui_id STATUS_ID   = 100;

const tui_id SAVE_BTN_ID = 200;
const tui_id EXIT_BTN_ID = 201;
const tui_id VERIFY_BTN_ID = 202;
const tui_id RELOAD_BTN_ID = 203;
const tui_id DEFAULT_BTN_ID = 204;
const tui_id BOX_ID = 205;

const tui_id AWS_HDR_ID = 300;
const tui_id AWS_CB_ID = 302;
const tui_id AWS_KEY_ID = 303;
const tui_id AWS_CHOOSE_ID = 304;
const tui_id AWS_FILE_ID = 305;
const tui_id AWS_PROF_LBL_ID = 306;
const tui_id AWS_PROF_ID = 307;

const tui_id GCP_HDR_ID = 400;
const tui_id GCP_CB_ID = 402;
const tui_id GCP_KEY_ID = 403;
const tui_id GCP_CHOOSE_ID = 404;
const tui_id GCP_FILE_ID = 405;

const tui_id CACHE_HDR_ID = 500;
const tui_id CACHE_SEL_ID = 501;
const tui_id CACHE_REPO_LBL_ID = 502;
const tui_id CACHE_REPO_CHOOSE_ID = 503;
const tui_id CACHE_REPO_PATH_ID = 504;
const tui_id CACHE_PROC_LBL_ID = 505;
const tui_id CACHE_PROC_CHOOSE_ID = 506;
const tui_id CACHE_PROC_PATH_ID = 507;

const tui_id NETW_HDR_ID = 600;
const tui_id NETW_USE_PROXY_ID = 602;
const tui_id NETW_PROXY_LBL_ID = 603;
const tui_id NETW_PROXY_ID = 604;

const tui_id DBGAP_HDR_ID = 700;
const tui_id DBGAP_IMPORT_KEY_ID = 701;
const tui_id DBGAP_IMPORT_PATH_ID = 702;
const tui_id DBGAP_REPOS_LBL_ID = 703;
const tui_id DBGAP_REPOS_ID = 704;

const uint32_t PAGE_FIXED = 0;
const uint32_t PAGE_AWS = 1;
const uint32_t PAGE_GCP = 2;
const uint32_t PAGE_CACHE = 3;
const uint32_t PAGE_NETW = 4;
const uint32_t PAGE_DBGAP = 5;

/* the helper-model for the grid */
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

class vdbconf_view2 : public Dlg
{
    public :
        vdbconf_view2( vdbconf_model &mod ) : Dlg(), model( mod ), grid( &mod )
        {
            populate( GetRect(), false );
            SetActivePage( PAGE_AWS );
            EnableCursorNavigation( false );
        }

        virtual bool Resize( Tui_Rect const &r )
        {
            populate( r, true );            
            return Dlg::Resize( r );
        }

        virtual void onPageChanged( uint32_t old_page, uint32_t new_page )
        {
            page_changed( old_page, false );
            page_changed( new_page, true );            
        }
        
        bool set_active_page( uint32_t page )
        {
            if ( GetActivePage() == page ) return false;
            SetFocus( SAVE_BTN_ID );
            SetActivePage( page );
            return true;
        }

        void update_aws_credentials( std::string &txt ) { SetWidgetCaption( AWS_FILE_ID, txt ); }
        void update_gcp_credentials( std::string &txt ) { SetWidgetCaption( GCP_FILE_ID, txt ); }        
        
    private :
        vdbconf_model &model;
        vdbconf_grid grid;
        
        void page_changed( uint32_t page_id, bool status )
        {
            tui_id hdr_id = 0;
            switch( page_id )
            {
                case PAGE_AWS   : hdr_id = AWS_HDR_ID; break;
                case PAGE_GCP   : hdr_id = GCP_HDR_ID; break;
                case PAGE_CACHE : hdr_id = CACHE_HDR_ID; break;
                case PAGE_NETW  : hdr_id = NETW_HDR_ID; break;
                case PAGE_DBGAP : hdr_id = DBGAP_HDR_ID; break;                
            }
            if ( hdr_id > 0 )
                SetWidgetBackground( hdr_id, status ? BOX_COLOR : STATUS_COLOR );
        }
        
        Tui_Rect save_and_exit_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 2, r.get_w() - 2, 1 ); }
        Tui_Rect TAB_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 4, r.get_w() - 2, r.get_h() - 6 ); }
        Tui_Rect BODY_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +1 , r.get_w(), r.get_h() - 1 ); }
        Tui_Rect CB_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() +2 , 30, 1 ); }
        Tui_Rect lbl1_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x(), r.get_y() + y , 32, 1 ); }
        Tui_Rect choose_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +1, r.get_y() + y , 12, 1 ); }
        Tui_Rect file_rect( Tui_Rect const &r, tui_coord y ) { return Tui_Rect( r.get_x() +14, r.get_y() + y , r.get_w() -15, 1 ); }
        Tui_Rect prof_lbl_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +7 , 10, 1 ); }
        Tui_Rect CACHE_RADIO_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() + 1, r.get_y() +2 , 24, 3 ); }
        Tui_Rect prof_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +14, r.get_y() +7 , 32, 1 ); }
        Tui_Rect proxy_lbl_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +4 , 7, 1 ); }
        Tui_Rect proxy_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +8, r.get_y() +4 , 32, 1 ); }
        Tui_Rect imp_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() +2 , 30, 1 ); }
        Tui_Rect imp_path_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +32, r.get_y() +2 , 30, 1 ); }
        Tui_Rect repo_lbl_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() +4 , 21, 1 ); }
        Tui_Rect repo_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() +5 , r.get_w() -2, r.get_h() -6 ); }
        
        Tui_Rect HDR_rect( Tui_Rect const &r, uint32_t ident )
        {
            tui_coord x = r.get_x() + ( 10 * ident );
            return Tui_Rect( x, r.get_y(), 9, 1 );
        }

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

        void populate_save_and_exit( Tui_Rect const &r, bool resize )
        {
            Tui_Rect rr = Tui_Rect( r.get_x(), r.get_y(), 14, 1 );
            PopulateButton( rr, resize, SAVE_BTN_ID,  "&save", BTN_COLOR_BG, BTN_COLOR_FG, PAGE_FIXED );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, EXIT_BTN_ID,  "e&xit", BTN_COLOR_BG, BTN_COLOR_FG, PAGE_FIXED );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, VERIFY_BTN_ID, "ver&ify", BTN_COLOR_BG, BTN_COLOR_FG, PAGE_FIXED );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, RELOAD_BTN_ID, "&reload", BTN_COLOR_BG, BTN_COLOR_FG, PAGE_FIXED );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, DEFAULT_BTN_ID, "de&fault", BTN_COLOR_BG, BTN_COLOR_FG, PAGE_FIXED );
        }

        void populate_AWS( Tui_Rect const &r, bool resize )
        {
            PopulateLabel( HDR_rect( r, 0 ), resize, AWS_HDR_ID, "&AWS", STATUS_COLOR, LABEL_FG, PAGE_FIXED );
            PopulateCheckbox( CB_rect( r ), resize, AWS_CB_ID, "acc&ept charges for AWS",
                                model.does_user_accept_aws_charges(),   /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, PAGE_AWS );
            PopulateLabel( lbl1_rect( r, 4 ), resize, AWS_KEY_ID, "credentials:",
                                BOX_COLOR, LABEL_FG, PAGE_AWS );
            PopulateButton( choose_rect( r, 5 ), resize, AWS_CHOOSE_ID, "ch&oose",
                                BTN_COLOR_BG, BTN_COLOR_FG, PAGE_AWS );
            PopulateLabel( file_rect( r, 5 ), resize, AWS_FILE_ID,
                                model.get_aws_credential_file_location().c_str(), /* model-connection */
                                LABEL_BG, INP_COLOR_FG, PAGE_AWS );
            PopulateLabel( prof_lbl_rect( r ), resize, AWS_PROF_LBL_ID, "&profile:",
                                BOX_COLOR, LABEL_FG, PAGE_AWS );
            PopulateInput( prof_rect( r ), resize, AWS_PROF_ID,
                                model.get_aws_profile().c_str(), /* model-connection */
                                64, INP_COLOR_BG, INP_COLOR_FG, PAGE_AWS );
        }

        void populate_GCP( Tui_Rect const &r, bool resize )
        {
            PopulateLabel( HDR_rect( r, 1 ), resize, GCP_HDR_ID, "&GCP", STATUS_COLOR, LABEL_FG, PAGE_FIXED );
            PopulateCheckbox( CB_rect( r ), resize, GCP_CB_ID, "acc&ept charges for GCP",
                                model.does_user_accept_gcp_charges(), /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, PAGE_GCP );
            PopulateLabel( lbl1_rect( r, 4 ), resize, GCP_KEY_ID, "credentials:",
                                BOX_COLOR, LABEL_FG, PAGE_GCP );
            PopulateButton( choose_rect( r, 5 ), resize, GCP_CHOOSE_ID, "ch&oose",
                                BTN_COLOR_BG, BTN_COLOR_FG, PAGE_GCP );
            PopulateLabel( file_rect( r, 5 ), resize, GCP_FILE_ID,
                                model.get_gcp_credential_file_location().c_str(), /* model-connection */
                                LABEL_BG, INP_COLOR_FG, PAGE_GCP );
        }

        void PopulateCacheRadiobox( Tui_Rect const &r, bool resize, uint32_t id, int selection,
                    KTUI_color bg, KTUI_color fg, uint32_t page_id )
        {
            if ( resize )
                SetWidgetRect( id, r, false );
            else
            {
                if ( HasWidget( id ) )
                {   /* set selection */
                }
                else
                {
                    AddRadioBox( id, r );
                    Populate_common( id, bg, fg, page_id );                    
                    AddWidgetString( id, "in RAM" );
                    AddWidgetString( id, "in repository" );
                    AddWidgetString( id, "process local" );
                }
            }
        }

        void populate_CACHE( Tui_Rect const &r, bool resize )
        {
            PopulateLabel( HDR_rect( r, 2 ), resize, CACHE_HDR_ID, "&CACHE", STATUS_COLOR, LABEL_FG, PAGE_FIXED );
            PopulateCacheRadiobox( CACHE_RADIO_rect( r ), resize, CACHE_SEL_ID,
                                0,/* model.get_cache_select(), */ /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, PAGE_CACHE ); 
 
            PopulateLabel( lbl1_rect( r, 6 ), resize, CACHE_REPO_LBL_ID, "repository location:",
                                BOX_COLOR, LABEL_FG, PAGE_CACHE );
            PopulateButton( choose_rect( r, 7 ), resize, CACHE_REPO_CHOOSE_ID, "ch&oose",
                                BTN_COLOR_BG, BTN_COLOR_FG, PAGE_CACHE );
            PopulateLabel( file_rect( r, 7 ), resize, CACHE_REPO_PATH_ID,
                                model.get_public_location().c_str(), /* model-connection */
                                LABEL_BG, INP_COLOR_FG, PAGE_CACHE );

            PopulateLabel( lbl1_rect( r, 9 ), resize, CACHE_PROC_LBL_ID, "process local location:",
                                BOX_COLOR, LABEL_FG, PAGE_CACHE );
            PopulateButton( choose_rect( r, 10 ), resize, CACHE_PROC_CHOOSE_ID, "choos&e",
                                BTN_COLOR_BG, BTN_COLOR_FG, PAGE_CACHE );
            PopulateLabel( file_rect( r, 10 ), resize, CACHE_PROC_PATH_ID,
                                "???", /*model.get_process_loc(), */ /* model-connection */
                                LABEL_BG, INP_COLOR_FG, PAGE_CACHE );
        }

        void populate_NETW( Tui_Rect const &r, bool resize )
        {
            PopulateLabel( HDR_rect( r, 3 ), resize, NETW_HDR_ID, "&NETWORK", STATUS_COLOR, LABEL_FG, PAGE_FIXED );
            PopulateCheckbox( CB_rect( r ), resize, NETW_USE_PROXY_ID, "&use proxy",
                                model.is_http_proxy_enabled(), /* model-connection */
                                CB_COLOR_BG, CB_COLOR_FG, PAGE_NETW );
            PopulateLabel( proxy_lbl_rect( r ), resize, NETW_PROXY_LBL_ID, "&proxy:",
                                BOX_COLOR, LABEL_FG, PAGE_NETW );
            PopulateInput( proxy_rect( r ), resize, NETW_PROXY_ID,
                                model.get_http_proxy_path().c_str(), /* model-connection */
                                64, INP_COLOR_BG, INP_COLOR_FG, PAGE_NETW );
        }

        void PopulateDBGAP_Grid( Tui_Rect const &r, bool resize, uint32_t id,
                                 KTUI_color bg, KTUI_color fg, uint32_t page_id )
        {
            if ( resize )
                SetWidgetRect( id, r, false );
            else
            {
                if ( HasWidget( id ) )
                {   /* set selection */
                }
                else
                {
                    AddGrid( id, r, grid, false );
                    Populate_common( id, bg, fg, page_id );                    
                }
            }
        }

        void populate_DBGAP( Tui_Rect const &r, bool resize )
        {
            PopulateLabel( HDR_rect( r, 4 ), resize, DBGAP_HDR_ID, "&DBGAP", STATUS_COLOR, LABEL_FG, PAGE_FIXED );
            PopulateButton( imp_rect( r ), resize, DBGAP_IMPORT_KEY_ID, "I&mport Repository key",
                                BTN_COLOR_BG, BTN_COLOR_FG, PAGE_DBGAP );
            PopulateButton( imp_path_rect( r ), resize,  DBGAP_IMPORT_PATH_ID, "Set Defau&lt Import Path",
                                BTN_COLOR_BG, BTN_COLOR_FG, PAGE_DBGAP );
            PopulateLabel( repo_lbl_rect( r ), resize, DBGAP_REPOS_LBL_ID, "dbGa&p repositories:",
                           STATUS_COLOR, LABEL_FG, PAGE_DBGAP );
            PopulateDBGAP_Grid( repo_rect( r ), resize, DBGAP_REPOS_ID, BTN_COLOR_BG, BTN_COLOR_FG, PAGE_DBGAP );
        }
        
        void populate( Tui_Rect const &r, bool resize )
        {
            SetCaption( "SRA configuration" );
            set_status_line( r );
            populate_save_and_exit( save_and_exit_rect( r ), resize );
            Tui_Rect tab_rect = TAB_rect( r );            

            PopulateLabel( BODY_rect( tab_rect ), resize, BOX_ID, NULL, BOX_COLOR, LABEL_FG, PAGE_FIXED );
            populate_AWS( tab_rect, resize );
            populate_GCP( tab_rect, resize );
            populate_CACHE( tab_rect, resize );
            populate_NETW( tab_rect, resize );
            populate_DBGAP( tab_rect, resize );
        }
};

class vdbconf_ctrl2 : public Dlg_Runner
{
    public :
        vdbconf_ctrl2( Dlg &dlg, vdbconf_model &mod ) : Dlg_Runner( dlg, &mod )
        {
            dlg.SetFocus( SAVE_BTN_ID );
        };
        
        virtual bool on_changed( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            vdbconf_model * model = ( vdbconf_model * ) data;
            tui_id id = dev.get_widget_id();
            switch( id )
            {
                case AWS_PROF_ID   : model -> set_aws_profile( dlg.GetWidgetText( id ) ); break; /* model-connection */
                case NETW_PROXY_ID : model -> set_http_proxy_path( dlg.GetWidgetText( id ) ); break; /* model-connection */
            }
            return true;
        }

        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            bool res = true;
            vdbconf_model * model = static_cast< vdbconf_model * >( data );
            switch( dev.get_widget_id() )
            {
                case AWS_CB_ID  : model -> set_user_accept_aws_charges( dlg.GetWidgetBoolValue( AWS_CB_ID ) ); break; /* model-connection */
                case GCP_CB_ID  : model -> set_user_accept_gcp_charges( dlg.GetWidgetBoolValue( GCP_CB_ID ) ); break; /* model-connection */
                case NETW_USE_PROXY_ID : model -> set_http_proxy_enabled( dlg.GetWidgetBoolValue( NETW_USE_PROXY_ID ) ); break; /* model-connection */
                
                case SAVE_BTN_ID : on_save( dlg, model ); break;                
                case EXIT_BTN_ID : res = on_exit( dlg, model ); break;
                case VERIFY_BTN_ID : on_verify( dlg, model ); break;
                case AWS_CHOOSE_ID : res = on_aws_choose( dlg, model ); break;
                case GCP_CHOOSE_ID : res = on_gcp_choose( dlg, model ); break;
                case DBGAP_IMPORT_KEY_ID : res = on_import_repo_key( dlg, model ); break;
                case DBGAP_IMPORT_PATH_ID : res = on_set_dflt_import_path( dlg, model ); break;
                case DBGAP_REPOS_ID : on_edit_dbgap_repo( dlg, model ); break;
            }
            return res;
        }

        virtual bool on_kb_alpha( Dlg &dlg, void * data, int code )
        {
            bool res;
            vdbconf_view2 &view = dynamic_cast<vdbconf_view2&>( dlg );
            vdbconf_model * model = static_cast< vdbconf_model * >( data );
            int active_page = dlg . GetActivePage();
            switch( code ) {
                case 'x' :
                case 'Q' :
                case 'q' :  res = on_exit( dlg, model ); break;

                case 's' :  res = on_save( dlg, model ); break;
                case 'i' :  res = on_verify( dlg, model ); break;
                case 'r' :  res = on_reload( dlg, model ); break;
                case 'f' :  res = on_default( dlg, model ); break;
                case 'p' :  switch( active_page ) {
                                case PAGE_AWS   : dlg.SetFocus( AWS_PROF_ID ); break;
                                case PAGE_NETW  : dlg.SetFocus( NETW_PROXY_ID ); break;
                                case PAGE_DBGAP : dlg.SetFocus( DBGAP_REPOS_ID ); break;
                            } break;
                case 'e' :  res = toggle_accept_charges( view, model ); break;
                case 'u' :  res = toggle_use_proxy( view, model ); break;
                
                case 'o' :  switch( active_page ) {
                                case PAGE_AWS : res = on_aws_choose( dlg,  model ); break;
                                case PAGE_GCP : res = on_gcp_choose( dlg,  model ); break;
                                case PAGE_CACHE : res = on_repo_choose( dlg, model ); break;
                            } break;
                case 'm' :  res = on_import_repo_key( dlg, model ); break;
                case 'l' :  res = on_set_dflt_import_path( dlg, model ); break;

                case 'a' :
                case 'A' :  res = view.set_active_page( PAGE_AWS ); break;
                case 'g' :
                case 'G' :  res = view.set_active_page( PAGE_GCP ); break;
                case 'c' :
                case 'C' :  res = view.set_active_page( PAGE_CACHE ); break;
                case 'n' :
                case 'N' :  res = view.set_active_page( PAGE_NETW ); break;
                case 'd' :
                case 'D' :  res = view.set_active_page( PAGE_DBGAP ); break;
                
                default  : res = false;
            }
            return res;
        };
        
    private :

        bool show_msg( Dlg &dlg, Tui_Rect r, const char * msg )
        {
            Std_Dlg_Info_Line sub;
            sub.set_parent( &dlg );
            dlg.center( r );
            sub.set_location( r );
            sub.set_text( msg );
            sub.execute();
            return true;
        }

        bool toggle_accept_charges( vdbconf_view2 &view, vdbconf_model *model )
        {
            bool res = false;
            if ( view.GetActivePage() == PAGE_AWS )
            {
                res = view.ToggleWidgetBoolValue( AWS_CB_ID );
                //model -> set_aws_accept( view.GetWidgetBoolValue( AWS_CB_ID ) );
            }
            else if ( view.GetActivePage() == PAGE_GCP )
            {
                res = view.ToggleWidgetBoolValue( GCP_CB_ID );
                //model -> set_gcp_accept( view.GetWidgetBoolValue( GCP_CB_ID ) );
            }
            return res;
        }

        bool toggle_use_proxy( vdbconf_view2 &view, vdbconf_model *model )
        {
            bool res = false;
            if ( view.GetActivePage() == PAGE_NETW )
            {
                res = view.ToggleWidgetBoolValue( NETW_USE_PROXY_ID );
                //model -> set_use_proxy( view.GetWidgetBoolValue( NETW_USE_PROXY_ID ) );
            }
            return res;
        }

        bool pick_dir( Dlg &dlg, Tui_Rect r, std::string &path )
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

        std::string pick_file( Dlg &dlg, Tui_Rect r, const char * path /*, const char *ext*/ )
        {
            std::string res = "";
            Std_Dlg_File_Pick pick;
            pick.set_parent( &dlg );
            dlg.center( r );
            pick.set_location( r );
            //pick.set_ext( ext );
            pick.set_dir_h( ( r.get_h() - 7 ) / 2 );
            pick.set_text( path );
            if ( pick.execute() )
                res.assign( pick.get_text() );
            return res;
        }

        bool on_save( Dlg &dlg, vdbconf_model * model )
        {
            return show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "changes successfully saved" );
        }
    
        bool on_exit( Dlg &dlg, vdbconf_model * model )
        {
            dlg.SetDone( true );
            return true;
        }

        bool on_verify( Dlg &dlg, vdbconf_model * model )
        {
            return show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "verification successful" );
        }

        bool on_reload( Dlg &dlg, vdbconf_model * model )
        {
            return show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "reload successful" );
        }

        bool on_default( Dlg &dlg, vdbconf_model * model )
        {
            return show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "setting defaults successful" );
        }

        bool on_aws_choose( Dlg &dlg, vdbconf_model * model )
        {
            /* on AWS: a path is choosen */
            std::string path = model -> get_aws_credential_file_location();
            if ( path.empty() ) path = model -> get_current_dir();
            if ( pick_dir( dlg, Tui_Rect( 0, 0, 80, 40 ), path ) )
            if ( !path.empty() )
            {
                model -> set_aws_credential_file_location( path ); /* model-connection */
                vdbconf_view2 &view = dynamic_cast<vdbconf_view2&>( dlg );
                view.update_aws_credentials( path );
            }
            return true;
        }

        bool on_gcp_choose( Dlg &dlg, vdbconf_model * model )
        {
            /* on GCP: a file is choosen */
            std::string org = model ->get_gcp_credential_file_location();
            if ( org.empty() ) org = model -> get_current_dir();
            std::string file = pick_file( dlg, Tui_Rect( 0, 0, 80, 40 ), org.c_str() );
            if ( !file.empty() )
            {
                model -> set_gcp_credential_file_location( file ); /* model-connection */
                vdbconf_view2 &view = dynamic_cast<vdbconf_view2&>( dlg );
                view.update_gcp_credentials( file );
            }
            return true;
        }

        bool on_repo_choose( Dlg &dlg, vdbconf_model * model )
        {
            std::string path( model -> get_current_dir() );
            if ( pick_dir( dlg, Tui_Rect( 0, 0, 80, 40 ), path ) )
            {
                /*
                model -> set_aws_keyfile( file );
                vdbconf_view2 &view = dynamic_cast<vdbconf_view2&>( dlg );
                view.update_gcp_credentials( file );
                */
            }
            return true;
        }
        
        bool on_import_repo_key( Dlg &dlg, vdbconf_model * model )
        {
            return show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "import repo-key" );
        }
        
        bool on_set_dflt_import_path( Dlg &dlg, vdbconf_model * model )
        {
            return show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "set dflt import path" );
        }

        bool on_edit_dbgap_repo( Dlg &dlg, vdbconf_model * model )
        {
            tui_long n = dlg.GetWidgetInt64Value( DBGAP_REPOS_ID );
            std::stringstream ss;
            ss << "edit dbgap-repo #" << n + 1;
            return show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), ss.str().c_str() );
        }
};

extern "C"
{
    rc_t run_interactive2 ( vdbconf_model & model )
    {
        rc_t rc = 0;
        try
        {
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
