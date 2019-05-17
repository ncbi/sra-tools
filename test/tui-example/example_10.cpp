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
#include <tui/tui.hpp>
#include <sstream>
#include <iostream>
#include <cstdarg>

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

const tui_id AWS_HDR_ID = 300;
const tui_id AWS_BOX_ID = 301;
const tui_id AWS_CB_ID = 302;
const tui_id AWS_KEY_ID = 303;
const tui_id AWS_PICK_ID = 304;
const tui_id AWS_FILE_ID = 305;
const tui_id AWS_PROF_LBL_ID = 306;
const tui_id AWS_PROF_ID = 307;
const tui_id AWS_ENV_ID = 308;

const tui_id GCP_HDR_ID = 400;
const tui_id GCP_BOX_ID = 401;
const tui_id GCP_CB_ID = 402;
const tui_id GCP_KEY_ID = 403;
const tui_id GCP_PICK_ID = 404;
const tui_id GCP_FILE_ID = 405;

const tui_id CACHE_HDR_ID = 500;
const tui_id CACHE_BOX_ID = 501;

class ex10_model
{
    public:
        ex10_model( const std::string cap ) : caption( cap ), aws_accept( false ),
            aws_env( false ), gcp_accept( false ), changed( false ) {}

        const char * get_caption() { return caption.c_str(); }
        
        void set_aws_accept( bool value ) { set_bool_value( &aws_accept, value ); }
        void set_gcp_accept( bool value ) { set_bool_value( &gcp_accept, value ); }
        void set_aws_env( bool value ) { set_bool_value( &aws_env, value ); }
        
        bool get_aws_accept( void ) { return aws_accept; }
        bool get_gcp_accept( void ) { return gcp_accept; }
        bool get_aws_env( void ) { return aws_env; }

        void set_aws_keyfile( const std::string &value ) { set_string_value( &aws_keyfile, value ); }
        void set_gcp_keyfile( const std::string &value ) { set_string_value( &gcp_keyfile, value ); }
        void set_aws_profile( const std::string &value ) { set_string_value( &aws_profile, value ); }
        
        const char * get_aws_keyfile( void ) { return aws_keyfile.c_str(); }
        const char * get_gcp_keyfile( void ) { return gcp_keyfile.c_str(); }
        const char * get_aws_profile( void ) { return aws_profile.c_str(); }
        
    private:
        std::string caption, aws_keyfile, aws_profile, gcp_keyfile;    
        bool aws_accept, aws_env, gcp_accept, changed;
        
        void set_bool_value( bool *current, bool new_value )
        {
            if ( *current != new_value )
            {
                changed = true;
                *current = new_value;
            }
        }
        
        void set_string_value( std::string *current, const std::string &new_value )
        {
            if ( *current != new_value )
            {
                changed = true;
                *current = new_value;
            }
        }
};

const uint32_t PAGE_AWS = 0;
const uint32_t PAGE_GCP = 1;
const uint32_t PAGE_CACHE = 2;

class ex10_view : public Dlg
{
   
    public :
        
        ex10_view( ex10_model &mod ) : Dlg(), model( mod ), active_page( PAGE_AWS )
        {
            populate( GetRect(), false );
        }

        virtual bool Resize( Tui_Rect const &r )
        {
            populate( r, true );            
            return Dlg::Resize( r );
        }

        bool set_active_page( uint32_t page )
        {
            if ( active_page == page ) return false;
            SetFocus( SAVE_BTN_ID );
            set_page( active_page, false );
            set_page( page, true );            
            active_page = page;
            return true;
        }

        void update_aws_credentials( std::string &txt ) { SetWidgetCaption( AWS_FILE_ID, txt ); }
        void update_gcp_credentials( std::string &txt ) { SetWidgetCaption( GCP_FILE_ID, txt ); }        
        
    private :
        ex10_model &model;
        uint32_t active_page;

        void change_items( bool on, tui_id from, tui_id to )
        {
            ShowWidgets( on, from, to );
            SetWidgetBackground( from - 1 , on ? BOX_COLOR : STATUS_COLOR );
        }

        void set_page( uint32_t page, bool on )
        {
            switch( page )
            {
                case PAGE_AWS   : change_items( on, AWS_BOX_ID, 308 ); break; // aws on/off
                case PAGE_GCP   : change_items( on, GCP_BOX_ID, 405 ); break; // gcp on/off
                case PAGE_CACHE : change_items( on, CACHE_BOX_ID, CACHE_BOX_ID ); break; // cache on/off
            }
        }
        
        Tui_Rect save_and_exit_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 2, r.get_w() - 2, 1 ); }
        Tui_Rect TAB_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 4, r.get_w() - 2, r.get_h() - 6 ); }
        Tui_Rect BODY_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +1 , r.get_w(), r.get_h() - 1 ); }
        Tui_Rect CB_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() +2 , 30, 1 ); }
        Tui_Rect cred_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +4 , 14, 1 ); }
        Tui_Rect pick_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() +5 , 10, 1 ); }
        Tui_Rect file_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +12, r.get_y() +5 , r.get_w() -13, 1 ); }
        Tui_Rect prof_lbl_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +7 , 10, 1 ); }
        Tui_Rect prof_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +12, r.get_y() +7 , 32, 1 ); }
        Tui_Rect env_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +1, r.get_y() +10 , 30, 1 ); }

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
            PopulateButton( rr, resize, SAVE_BTN_ID,  "save", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, EXIT_BTN_ID,  "exit", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, VERIFY_BTN_ID, "verify", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, RELOAD_BTN_ID, "reload", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, DEFAULT_BTN_ID, "default", BTN_COLOR_BG, BTN_COLOR_FG );
        }

        void populate_AWS( Tui_Rect const &r, bool resize, bool active )
        {
            PopulateLabel( HDR_rect( r, 0 ), resize, AWS_HDR_ID, "&AWS",
                active ? BOX_COLOR : STATUS_COLOR, LABEL_FG );
            PopulateLabel( BODY_rect( r ), resize, AWS_BOX_ID, NULL, BOX_COLOR, LABEL_FG );
            PopulateCheckbox( CB_rect( r ), resize, AWS_CB_ID, "accept charges for AWS",
                                model.get_aws_accept(), CB_COLOR_BG, CB_COLOR_FG );
            PopulateLabel( cred_rect( r ), resize, AWS_KEY_ID, "credentials:", BOX_COLOR, LABEL_FG );
            PopulateButton( pick_rect( r ), resize, AWS_PICK_ID, "pick", BTN_COLOR_BG, BTN_COLOR_FG );
            PopulateLabel( file_rect( r ), resize, AWS_FILE_ID, model.get_aws_keyfile(), LABEL_BG, LABEL_FG );
            PopulateLabel( prof_lbl_rect( r ), resize, AWS_PROF_LBL_ID, "profile:", BOX_COLOR, LABEL_FG );
            PopulateInput( prof_rect( r ), resize, AWS_PROF_ID, model.get_aws_profile(), 64, INP_COLOR_BG, INP_COLOR_FG );
            PopulateCheckbox( env_rect( r ), resize, AWS_ENV_ID, "pick from environment",
                                model.get_aws_env(), CB_COLOR_BG, CB_COLOR_FG );
            ShowWidgets( active, AWS_BOX_ID, AWS_ENV_ID );
        }

        void populate_GCP( Tui_Rect const &r, bool resize, bool active )
        {
            PopulateLabel( HDR_rect( r, 1 ), resize, GCP_HDR_ID, "&GCP",
                active ? BOX_COLOR : STATUS_COLOR, LABEL_FG );
            PopulateLabel( BODY_rect( r ), resize, GCP_BOX_ID, NULL, BOX_COLOR, LABEL_FG );
            PopulateCheckbox( CB_rect( r ), resize, GCP_CB_ID, "accept charges for GCP",
                                model.get_gcp_accept(), CB_COLOR_BG, CB_COLOR_FG );
            PopulateLabel( cred_rect( r ), resize, GCP_KEY_ID, "credentials:", BOX_COLOR, LABEL_FG );
            PopulateButton( pick_rect( r ), resize, GCP_PICK_ID, "pick", BTN_COLOR_BG, BTN_COLOR_FG );
            PopulateLabel( file_rect( r ), resize, GCP_FILE_ID, model.get_gcp_keyfile(), LABEL_BG, LABEL_FG );
            ShowWidgets( active, GCP_BOX_ID, GCP_FILE_ID );
        }

        void populate_CACHE( Tui_Rect const &r, bool resize, bool active )
        {
            PopulateLabel( HDR_rect( r, 2 ), resize, CACHE_HDR_ID, "&CACHE",
                active ? BOX_COLOR : STATUS_COLOR, LABEL_FG );
            PopulateLabel( BODY_rect( r ), resize, CACHE_BOX_ID, NULL, BOX_COLOR, LABEL_FG );
            ShowWidgets( active, CACHE_BOX_ID, CACHE_BOX_ID );            
        }
        
        void populate( Tui_Rect const &r, bool resize )
        {
            SetCaption( model . get_caption() );
            set_status_line( r );
            populate_save_and_exit( save_and_exit_rect( r ), resize );
            populate_AWS( TAB_rect( r ), resize, active_page == PAGE_AWS );
            populate_GCP( TAB_rect( r ), resize, active_page == PAGE_GCP );
            populate_CACHE( TAB_rect( r ), resize, active_page == PAGE_CACHE );
        }
};

class ex10_ctrl : public Dlg_Runner
{
    public :
        ex10_ctrl( Dlg &dlg, ex10_model &mod ) : Dlg_Runner( dlg, &mod )
        {
            dlg.SetFocus( SAVE_BTN_ID );
        };
        
        // for demonstration: each time the input-field has been changed, get the text and 
        // set it as status-text
        virtual bool on_changed( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            ex10_model * model = ( ex10_model * ) data;
            switch( dev.get_widget_id() )
            {
                case AWS_PROF_ID : model -> set_aws_profile( dlg.GetWidgetText( AWS_PROF_ID ) ); break;
            }
            return true;
        }

        // for demonstration: each time the button has been pressed, write 'Button' into the status line
        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            bool res = true;
            ex10_model * model = ( ex10_model * ) data;
            switch( dev.get_widget_id() )
            {
                case AWS_CB_ID  : model -> set_aws_accept( dlg.GetWidgetBoolValue( AWS_CB_ID ) ); break;
                case AWS_ENV_ID : model -> set_aws_env( dlg.GetWidgetBoolValue( AWS_ENV_ID ) ); break;
                case GCP_CB_ID  : model -> set_gcp_accept( dlg.GetWidgetBoolValue( GCP_CB_ID ) ); break;
                
                case SAVE_BTN_ID : on_save( dlg, model ); break;                
                case EXIT_BTN_ID : res = on_exit( dlg, model ); break;
                case VERIFY_BTN_ID : on_verify( dlg, model ); break;
                case AWS_PICK_ID : on_aws_pick( dlg, model ); break;
                case GCP_PICK_ID : on_gcp_pick( dlg, model ); break;                
            }
            return res;
        }

        virtual bool on_kb_alpha( Dlg &dlg, void * data, int code )
        {
            bool res;
            ex10_view &view = dynamic_cast<ex10_view&>( dlg );
            ex10_model *model = ( ex10_model * ) data; 

            switch( code )
            {
                case 'Q' :
                case 'q' : res = on_exit( dlg, model ); break;

                case 'a' :
                case 'A' : res = view.set_active_page( PAGE_AWS ); break;
                case 'g' :
                case 'G' : res = view.set_active_page( PAGE_GCP ); break;
                case 'c' :
                case 'C' : res = view.set_active_page( PAGE_CACHE ); break;
                
                default  : res = false;
            }
            return res;
        };
        
    private :

        void show_msg( Dlg &dlg, Tui_Rect r, const char * msg )
        {
            Std_Dlg_Info_Line sub;
            sub.set_parent( &dlg );
            dlg.center( r );
            sub.set_location( r );
            sub.set_text( msg );
            sub.execute();
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

        void on_save( Dlg &dlg, ex10_model * model )
        {
            show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "changes successfully saved" );
        }
    
        bool on_exit( Dlg &dlg, ex10_model * model )
        {
            dlg.SetDone( true );
            return true;
        }

        void on_verify( Dlg &dlg, ex10_model * model )
        {
            show_msg( dlg, Tui_Rect( 0, 0, 80, 6 ), "verification successful" );
        }

        void on_aws_pick( Dlg &dlg, ex10_model * model )
        {
            std::string file = pick_file( dlg, Tui_Rect( 0, 0, 80, 40 ), "/home" );
            if ( !file.empty() )
            {
                model -> set_aws_keyfile( file );                
                ex10_view &view = dynamic_cast<ex10_view&>( dlg );
                view.update_aws_credentials( file );
            }
        }

        void on_gcp_pick( Dlg &dlg, ex10_model * model )
        {
            std::string file = pick_file( dlg, Tui_Rect( 0, 0, 80, 40 ), "/home" );
            if ( !file.empty() )
            {
                model -> set_aws_keyfile( file );
                ex10_view &view = dynamic_cast<ex10_view&>( dlg );
                view.update_gcp_credentials( file );
            }
        }
};

void example_10( void )
{
    try
    {
        /* (1) ... create a model */        
        ex10_model model( "SRA cloud settings" );
        
        /* (2) ... create a view */
        ex10_view view( model );

        /* (3) ... create a controller, hand it the view and the model */
        ex10_ctrl crtl( view, model );

        /* (4) ... let the controller handle the events */
        crtl.run();

        /* (5) call this before leaving main() to terminate the low-level driver... */
        Tui::clean_up();
    }
    catch ( ... )
    {
        std::cerr << "problem running example 10" << std::endl;
    }
}
