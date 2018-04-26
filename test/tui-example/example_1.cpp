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

using namespace tui;

/* ---------------------------------------------------------------------------

    This one adds a caption! :
    
    we have to derive our own view-class from tui::Dlg
    
    press ESC twice to get out of the controller-loop
    
--------------------------------------------------------------------------- */

class ex1_view : public Dlg
{
    public :
        ex1_view( const char * a_caption ) : Dlg()
        {
            SetCaption( a_caption );
        }
};


void example_1( void )
{
    /* (1) ... create a view ( an empty window in this case ) */
    ex1_view view( "example #1" );

    /* (2) ... create a controller, hand it the view and the model */
    Dlg_Runner crtl( view, NULL );

    /* (3) ... let the controller handle the events */
    crtl.run();

    /* (4) call this before leaving main() to terminate the low-level driver... */
    Tui::clean_up();
}
