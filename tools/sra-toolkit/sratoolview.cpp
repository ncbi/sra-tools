#include "sratoolview.h"
#include "sratoolkitglobals.h"
#include "config/sraconfigview.h"
#include "diagnostics/sradiagnosticsview.h"

#include <kfg/config.h>


#include <QPainter>

SRAToolView :: SRAToolView ( KConfig *p_config, QWidget *parent )
    : QWidget ( parent )
    , config ( p_config )
    , home ( nullptr )
    , diagnostics ( nullptr )
    , currentView ( nullptr )
{
    this -> setObjectName ( "sratool_view" );
    resize ( QSize ( parent -> size (). width () - 100, parent -> size () . height () ) );

    home = new SRAConfigView ( this );
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

#if OFFICAL_LOOKNFEEL
void SRAToolView :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    QLinearGradient gradient = sraTemplate -> getStandardBackground ();
    gradient . setStart ( size () . width () / 2, 0 );
    gradient . setFinalStop ( size () . width () / 2, size () . height () );

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


