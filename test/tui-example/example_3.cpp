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

using namespace tui;

/* ---------------------------------------------------------------------------

    We want it to act on window-resize and show the new size in the status bar :
    
    press ESC twice to get out of the controller-loop
    
--------------------------------------------------------------------------- */

const tui_id STATUS_ID = 100;

class ex3_view : public Dlg
{
    public :
        ex3_view( const char * a_caption ) : Dlg()
        {
            SetCaption( a_caption );
            set_status_line( GetRect() );
        }
        
        virtual bool Resize( Tui_Rect const &r )
        {
            set_status_line( r );
            return Dlg::Resize( r );
        }
        
        void set_status_line( Tui_Rect r )
        {
            std::stringstream ss;
            ss << " w:" << r.get_w() << " h:" << r.get_h();
            
            r.change( 1, r.get_h() - 1, -2, 1 );
            if ( HasWidget( STATUS_ID ) )
            {
                SetWidgetRect( STATUS_ID, r, false );
                SetWidgetCaption( STATUS_ID, ss.str().c_str() );
            }
            else
                AddLabel( STATUS_ID, r, ss.str().c_str() );
        }
};


void example_3( void )
{
    /* (1) ... create a view */
    ex3_view view( "example #3" );

    /* (2) ... create a controller, hand it the view and the model */
    Dlg_Runner crtl( view, NULL );

    /* (3) ... let the controller handle the events */
    crtl.run();

    /* (4) call this before leaving main() to terminate the low-level driver... */
    Tui::clean_up();
}
