#include "sratoolview.h"
#include "sratoolkitglobals.h"
#include "config/sraconfigview.h"
#include "diagnostics/sradiagnosticsview.h"

#include <QPropertyAnimation>

#include <kfg/config.h>


#include <QPainter>

SRAToolView :: SRAToolView ( KConfig *p_config, QWidget *parent )
    : QWidget ( parent )
    , config ( p_config )
    , home ( nullptr )
    , config_view ( nullptr )
    , diagnostics_view ( nullptr )
    , currentView ( nullptr )
{
    setObjectName ( "sra_tool_view" );
    resize ( QSize ( parent -> size (). width () - TOOLBAR_WIDTH_FACTOR, parent -> size () . height () ) );


    home = new QWidget ( this );
    currentView = home;

    config_view = new SRAConfigView ( this );
    config_view -> hide ();

    diagnostics_view = new SRADiagnosticsView ( config, this );
    diagnostics_view -> hide ();
}

void SRAToolView :: toolChanged ( int p_view )
{
    currentView -> hide ();
    currentView = nullptr;

    switch ( p_view )
    {
    case 0:
        home -> show ();
        currentView = home;
        break;
    case 1:
        config_view -> show ();
        currentView = config_view;
        break;
    case 2:
        diagnostics_view -> show ();
        currentView = diagnostics_view;
        break;
    }
}

void SRAToolView :: expand ( bool val )
{
}


#if OFFICAL_LOOKNFEEL
void SRAToolView :: paintEvent ( QPaintEvent *e )
{
    bool vertical = false;
    QPainter painter ( this );

    QLinearGradient gradient = sraTemplate -> getStandardBackground ();
    if ( vertical )
    {
        gradient . setStart ( size () . width () / 2, 0 );
        gradient . setFinalStop ( size () . width () / 2, size () . height () );
    }
    else
    {
        gradient . setStart ( 0, size () . height () / 2 );
        gradient . setFinalStop ( size () . width (), size () . height () / 2 );
    }

    painter.setBrush ( gradient );

    painter.drawRect ( 0, 0, size () . width (), size () . height () );

    /*
    QPixmap pxmp ( img_path + "ncbi_helix_blue_black" );
    pxmp = pxmp. scaled ( this -> size (), Qt::KeepAspectRatio, Qt::SmoothTransformation );

    painter.drawPixmap( size () . width () / 2 - pxmp.size ().width() / 2, size () . height () / 2 - pxmp.size ().height() / 2, pxmp );
*/
    show ();

    QWidget::paintEvent(e);
}
#elif MODERN_LOOKNFEEL
void SRAToolView :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    painter.setBrush ( QColor ( 255, 255, 255, 255) );

    painter.drawRect ( 0, 0, size () . width (), size () . height () );

    QPixmap pxmp ( img_path + "ncbi_helix_blue_black" );
    pxmp = pxmp. scaled ( this -> size (), Qt::KeepAspectRatio, Qt::SmoothTransformation );

    painter.drawPixmap( size () . width () / 2 - pxmp.size ().width() / 2, size () . height () / 2 - pxmp.size ().height() / 2, pxmp );

    show ();

    QWidget::paintEvent(e);
}
#elif DARKGLASS_LOOKNFEEL
void SRAToolView :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    show ();

    QWidget::paintEvent(e);
}
#endif


