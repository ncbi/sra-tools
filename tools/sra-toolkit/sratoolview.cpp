#include "sratoolview.h"
#include "config/sraconfigview.h"
#include "diagnostics/sradiagnosticsview.h"

#include <kfg/config.h>

SRAToolView :: SRAToolView ( KConfig *p_config, QWidget *parent )
    : QWidget ( parent )
    , config ( p_config )
    , home ( nullptr )
    , diagnostics ( nullptr )
    , currentView ( nullptr )
{
    resize ( QSize ( parent -> size (). width () - 100, parent -> size () . height () ) );

    home = new SRAConfigView ( this );
    //home = new QWidget ( this );
    //home -> resize ( size () );
    home -> setObjectName ( "home_view" );
    currentView = home;

    diagnostics = new SRADiagnosticsView ( config, this );
    diagnostics -> hide ();
}

void SRAToolView :: toolChanged ( int p_view )
{
    currentView -> hide ();

    switch ( p_view )
    {
    case 0:
        home -> show ();
        currentView = home;
        break;
    case 1:
        diagnostics -> show ();
        currentView = diagnostics;
        break;
    }
}
