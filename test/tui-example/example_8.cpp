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

using namespace tui;

/* ---------------------------------------------------------------------------

    We want it to act on window-resize and show the new size in the status bar :
    
    press ESC twice to get out of the controller-loop
    
--------------------------------------------------------------------------- */

const tui_id STATUS_ID   = 100;
const tui_id LABEL_ID    = 101;
const tui_id INPUT_ID    = 102;
const tui_id BUTTON_ID   = 103;
const tui_id CHECKBOX_ID = 104;

class ex8_model
{
    public:
        std::string caption;
        std::string input;
        
        ex8_model( const std::string cap, const std::string inp ) : caption( cap ), input( inp ) {}
};

class ex8_view : public Dlg
{
   
    public :
        
        ex8_view( ex8_model &mod ) : Dlg(), model( mod )
        {
            SetCaption( mod.caption );
            set_status_line( GetRect() );
            populate();
        }

        virtual bool Resize( Tui_Rect const &r )
        {
            set_status_line( r );
            return Dlg::Resize( r );
        }
       
    private :
        ex8_model &model;
        
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
        
        void populate( void )
        {
            tui_coord x = 5;
            tui_coord y = 3;
            tui_coord w = 20;
            tui_coord h = 1;
            AddLabel( LABEL_ID, Tui_Rect( x, y, w, h ), "a label" );
            SetWidgetBackground( LABEL_ID, KTUI_c_dark_blue );

            y += 2;
            // text will be limited to 20 characters
            AddInput( INPUT_ID, Tui_Rect( x, y, w, h ), model.input.c_str(), 20 );

            y += 2;
            AddButton( BUTTON_ID, Tui_Rect( x, y, w, h ), "press me" );

            y += 2;
            AddCheckBox( CHECKBOX_ID, Tui_Rect( x, y, w, h ), "check me", false );
        }
};

class ex8_ctrl : public Dlg_Runner
{
    public :
        ex8_ctrl( Dlg &dlg, ex8_model &mod ) : Dlg_Runner( dlg, &mod )
        {
            dlg.SetFocus( INPUT_ID );
        };
        
        // for demonstration: each time the input-field has been changed, get the text and 
        // set it as status-text
        virtual bool on_changed( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            switch( dev.get_widget_id() )
            {
                case INPUT_ID : dlg.SetWidgetCaption( STATUS_ID, dlg.GetWidgetText( INPUT_ID ) ); break;
            }
            return true;
        }

        // for demonstration: each time the button has been pressed, write 'Button' into the status line
        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            switch( dev.get_widget_id() )
            {
                case BUTTON_ID : dlg.SetWidgetCaption( STATUS_ID, "Button" ); break;               
                case CHECKBOX_ID : dlg.SetWidgetCaption( STATUS_ID, dlg.GetWidgetBoolValue( CHECKBOX_ID ) ? "checked" : "unchecked" ); break;                
            }
            return true;
        }
};

void example_8( void )
{
    try
    {
        /* (1) ... create a model */        
        ex8_model model( "example #8", "some input" );
        
        /* (2) ... create a view */
        ex8_view view( model );

        /* (3) ... create a controller, hand it the view and the model */
        ex8_ctrl crtl( view, model );

        /* (4) ... let the controller handle the events */
        crtl.run();

        /* (5) call this before leaving main() to terminate the low-level driver... */
        Tui::clean_up();
    }
    catch ( ... )
    {
        std::cerr << "problem running example 8" << std::endl;
    }
}
