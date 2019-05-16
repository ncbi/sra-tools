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

const tui_id SAVE_AND_EXIT_ID = 200;
const tui_id SAVE_BTN_ID = 201;
const tui_id EXIT_BTN_ID = 202;
const tui_id VERIFY_BTN_ID = 203;

const tui_id AWS_BOX_ID = 300;
const tui_id AWS_HDR_ID = 301;
const tui_id AWS_CB_ID = 302;
const tui_id AWS_KEY_ID = 303;
const tui_id AWS_PICK_ID = 304;
const tui_id AWS_FILE_ID = 305;
const tui_id AWS_SEC_LBL_ID = 306;
const tui_id AWS_SEC_ID = 307;
const tui_id AWS_ENV_ID = 308;

const tui_id GCP_BOX_ID = 400;
const tui_id GCP_HDR_ID = 401;
const tui_id GCP_CB_ID = 402;
const tui_id GCP_KEY_ID = 403;
const tui_id GCP_PICK_ID = 404;
const tui_id GCP_FILE_ID = 405;

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
        void set_aws_section( const std::string &value ) { set_string_value( &aws_section, value ); }
        
        const char * get_aws_keyfile( void ) { return aws_keyfile.c_str(); }
        const char * get_gcp_keyfile( void ) { return gcp_keyfile.c_str(); }
        const char * get_aws_section( void ) { return aws_section.c_str(); }
        
    private:
        std::string caption, aws_keyfile, aws_section, gcp_keyfile;    
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

class ex10_view : public Dlg
{
   
    public :
        
        ex10_view( ex10_model &mod ) : Dlg(), model( mod )
        {
            populate( GetRect(), false );
        }

        virtual bool Resize( Tui_Rect const &r )
        {
            populate( r, true );            
            return Dlg::Resize( r );
        }
       
    private :
        ex10_model &model;
        
        Tui_Rect save_and_exit_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 2, r.get_w() - 2, 3 ); }
        Tui_Rect AWS_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 6, r.get_w() - 2, 10 ); }
        Tui_Rect GCP_rect( Tui_Rect const &r ) { return Tui_Rect( 1, 17, r.get_w() - 2, 12 ); }
        Tui_Rect HDR_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y(), 6, 1 ); }
        Tui_Rect BODY_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x(), r.get_y() +1 , r.get_w(), r.get_h() - 1 ); }
        Tui_Rect CB_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +2, r.get_y() +2 , 30, 1 ); }
        Tui_Rect key_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +2, r.get_y() +4 , 8, 1 ); }
        Tui_Rect pick_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +13, r.get_y() +4 , 10, 1 ); }
        Tui_Rect file_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +24, r.get_y() +4 , r.get_w() -25, 1 ); }
        Tui_Rect sec_lbl_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +2, r.get_y() +6 , 10, 1 ); }
        Tui_Rect sec_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +13, r.get_y() +6 , 32, 1 ); }
        Tui_Rect env_rect( Tui_Rect const &r ) { return Tui_Rect( r.get_x() +2, r.get_y() +8 , 30, 1 ); }
        
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
            PopulateLabel( r, resize, SAVE_AND_EXIT_ID, NULL, BOX_COLOR, LABEL_FG );
            Tui_Rect rr = Tui_Rect( r.get_x()+6, r.get_y()+1, 16, 1 );
            PopulateButton( rr, resize, SAVE_BTN_ID,  "save", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, EXIT_BTN_ID,  "exit", BTN_COLOR_BG, BTN_COLOR_FG );
            rr.change( rr.get_w() + 2, 0, 0, 0 );
            PopulateButton( rr, resize, VERIFY_BTN_ID, "verify", BTN_COLOR_BG, BTN_COLOR_FG );
        }

        void populate_AWS( Tui_Rect const &r, bool resize )
        {
            PopulateLabel( HDR_rect( r ), resize, AWS_HDR_ID, "AWS", BOX_COLOR, LABEL_FG );
            PopulateLabel( BODY_rect( r ), resize, AWS_BOX_ID, NULL, BOX_COLOR, LABEL_FG );
            PopulateCheckbox( CB_rect( r ), resize, AWS_CB_ID, "accept charges for AWS",
                                model.get_aws_accept(), CB_COLOR_BG, CB_COLOR_FG );
            PopulateLabel( key_rect( r ), resize, AWS_KEY_ID, "keys:", BOX_COLOR, LABEL_FG );
            PopulateButton( pick_rect( r ), resize, AWS_PICK_ID, "pick", BTN_COLOR_BG, BTN_COLOR_FG );
            PopulateLabel( file_rect( r ), resize, AWS_FILE_ID, model.get_aws_keyfile(), LABEL_BG, LABEL_FG );
            PopulateLabel( sec_lbl_rect( r ), resize, AWS_SEC_LBL_ID, "section:", BOX_COLOR, LABEL_FG );
            PopulateInput( sec_rect( r ), resize, AWS_SEC_ID, model.get_aws_section(), 64, INP_COLOR_BG, INP_COLOR_FG );
            PopulateCheckbox( env_rect( r ), resize, AWS_ENV_ID, "pick from environment",
                                model.get_aws_env(), CB_COLOR_BG, CB_COLOR_FG );
        }

        void populate_GCP( Tui_Rect const &r, bool resize )
        {
            PopulateLabel( HDR_rect( r ), resize, GCP_HDR_ID, "GCP", BOX_COLOR, LABEL_FG );
            PopulateLabel( BODY_rect( r ), resize, GCP_BOX_ID, NULL, BOX_COLOR, LABEL_FG );
            PopulateCheckbox( CB_rect( r ), resize, GCP_CB_ID, "accept charges for GCP",
                                model.get_gcp_accept(), CB_COLOR_BG, CB_COLOR_FG );
            PopulateLabel( key_rect( r ), resize, GCP_KEY_ID, "keys:", BOX_COLOR, LABEL_FG );
            PopulateButton( pick_rect( r ), resize, GCP_PICK_ID, "pick", BTN_COLOR_BG, BTN_COLOR_FG );
            PopulateLabel( file_rect( r ), resize, GCP_FILE_ID, model.get_gcp_keyfile(), LABEL_BG, LABEL_FG );
        }
        
        void populate( Tui_Rect const &r, bool resize )
        {
            SetCaption( model . get_caption() );
            set_status_line( r );
            populate_save_and_exit( save_and_exit_rect( r ), resize );
            populate_AWS( AWS_rect( r ), resize );
            populate_GCP( GCP_rect( r ), resize );
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
                case AWS_SEC_ID : model -> set_aws_section( dlg.GetWidgetText( AWS_SEC_ID ) ); break;
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
            ex10_model * model = ( ex10_model * ) data;            
            switch( code )
            {
                case 'Q' :
                case 'q' : res = on_exit( dlg, model ); break;

                case 'a' : switch_to_aws( dlg ); res = true; break;
                case 'g' : switch_to_gcp( dlg ); res = true; break;
                
                default  : res = false;
            }
            return res;
        };
        
    private :

        void on_save( Dlg &dlg, ex10_model * model )
        {
            dlg.SetWidgetCaption( STATUS_ID, "saved" );
        }
    
        bool on_exit( Dlg &dlg, ex10_model * model )
        {
            dlg.SetDone( true );
            return true;
        }

        void on_verify( Dlg &dlg, ex10_model * model )
        {
            dlg.SetWidgetCaption( STATUS_ID, "verified" );
        }

        void on_aws_pick( Dlg &dlg, ex10_model * model )
        {
            dlg.SetWidgetCaption( STATUS_ID, "aws picked" );
        }

        void on_gcp_pick( Dlg &dlg, ex10_model * model )
        {
            dlg.SetWidgetCaption( STATUS_ID, "gcp picked" );
        }

        void switch_to_aws( Dlg &dlg )
        {
            dlg.SetFocus( SAVE_BTN_ID );
            dlg.ShowWidgets( false, 400, 405 ); // gcp off
            dlg.ShowWidgets( true, 300, 308 ); // aws on
        }
        
        void switch_to_gcp( Dlg &dlg )
        {
            dlg.SetFocus( SAVE_BTN_ID );
            dlg.ShowWidgets( false, 300, 308 ); // aws off
            dlg.ShowWidgets( true, 400, 405 ); // gcp on
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
