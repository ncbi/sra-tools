#include "sratoolkit.h"
#include "sratoolkitglobals.h"
#include "sratoolbar.h"
#include "sratoolview.h"

#include <kapp/main.h>
#include <kfg/config.h>
#include <kfg/properties.h>
#include <kfs/directory.h>
#include <klib/rc.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QToolButton>
#include <QTimer>

#include <QDebug>


SRAToolkit :: SRAToolkit ( const QRect &avail_geometry, QWidget *parent )
    : QMainWindow ( parent )
    , config ( nullptr )
{
    KDirectory *dir = nullptr;
    rc_t rc = KDirectoryNativeDir ( &dir );
    if ( rc != 0 )
        qDebug () << "Failed to get native directory";
    else
    {
        rc = KConfigMake ( &config, dir );
        if ( rc != 0 )
            qDebug () << "Failed to make config";
    }

    setFixedSize ( ( avail_geometry . width () / 2.5 ) , avail_geometry . height () / 2 );

    move ( ( avail_geometry . width () - this -> width () ) / 2,
           ( avail_geometry . height () - this -> height () ) / 2 );

    init ();
}

SRAToolkit :: ~SRAToolkit ()
{
}

void SRAToolkit :: init ()
{
    init_menubar ();
    init_view ();
}

void SRAToolkit :: init_menubar ()
{
    //QMenu *file = menuBar () -> addMenu ( tr ( "&File" ) );
    //QMenu *edit = menuBar () -> addMenu ( tr ( "\u200CEdit" ) ); // \u200C was added because OSX auto-adds some unwanted menu items
    //QMenu *help = menuBar () -> addMenu ( tr ( "&Help" ) );
    //help -> addAction ( tr ( "&Diagnostics" ), this, SLOT ( open_diagnostics () ) );
}

void SRAToolkit :: init_view ()
{
    mainWidget = new QWidget ();
    mainWidget -> setObjectName ( "main_widget" );

    mainLayout = new QHBoxLayout ();
    mainLayout -> setSpacing ( 0 );
    mainLayout -> setMargin ( 0 );

    toolBar = new SRAToolBar ( this );

    toolView = new SRAToolView ( config, this );
    toolView -> setObjectName ( "sratool_view" );
    connect ( toolBar, SIGNAL ( toolSwitched ( int ) ), toolView, SLOT ( toolChanged ( int ) ) );
    connect ( toolBar, SIGNAL ( expanded ( bool ) ), this, SLOT ( expand ( bool ) ) );
    connect ( toolBar, SIGNAL ( expanded ( bool ) ), toolView, SLOT ( expand ( bool ) ) );

    mainLayout -> addWidget ( toolBar );
    mainLayout -> addWidget ( toolView );

    mainWidget -> setLayout ( mainLayout );

    setCentralWidget ( mainWidget );

}

void SRAToolkit :: expand ( bool val )
{
    if ( val )
        setFixedSize ( size () . width () + TOOLBAR_WIDTH_FACTOR, size () . height () );
    else
        setFixedSize ( size () . width () - TOOLBAR_WIDTH_FACTOR, size () . height () );
}

#if OFFICAL_LOOKNFEEL
void SRAToolkit :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    QLinearGradient gradient = sraTemplate -> getStandardBackground();
    gradient . setStart ( 0, 0 );
    gradient . setFinalStop ( size () . width (), size () . height () );

    painter.setBrush ( gradient );

    painter.drawRect ( 0, 0, size () . width (), size () . height () );
    show ();

    QWidget::paintEvent(e);
}
#elif MODERN_LOOKNFEEL
void SRAToolkit :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    QLinearGradient gradient = sraTemplate -> getBaseGradient();
    gradient . setStart ( 0, 0 );
    gradient . setFinalStop ( size () . width (), size () . height () );

    painter.setBrush ( gradient );
    //painter.setBrush ( QColor ( 255, 255, 255, 255) );

    painter.drawRect ( 0, 0, size () . width (), size () . height () );

    QPixmap pxmp ( img_path + "ncbi_helix_blue_black" );
    pxmp = pxmp. scaled ( this -> size (), Qt::KeepAspectRatio, Qt::SmoothTransformation );

    painter.drawPixmap( size () . width () / 2 - pxmp.size ().width() / 2, size () . height () / 2 - pxmp.size ().height() / 2, pxmp );

    show ();

    QWidget::paintEvent(e);
}
#elif DARKGLASS_LOOKNFEEL
void SRAToolkit :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    QLinearGradient gradient = sraTemplate -> getBaseGradient();
    gradient . setStart ( 0, 0 );
    gradient . setFinalStop ( size () . width (), size () . height () );

    painter.setBrush ( gradient );
    painter.drawRect ( 0, 0, size () . width (), size () . height () );

    QPixmap pxmp ( img_path + "ncbi_helix_blue_black" );
    pxmp = pxmp. scaled ( this -> size (), Qt::KeepAspectRatio, Qt::SmoothTransformation );

    painter.drawPixmap( size () . width () / 2 - pxmp.size ().width() / 2, size () . height () / 2 - pxmp.size ().height() / 2, pxmp );

    show ();

    QWidget::paintEvent(e);
}
#endif

