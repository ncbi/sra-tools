#include "sratoolkit.h"
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
/*
extern "C"
{
    rc_t run_interactive ( vdbconf_model &m )
    {
        const QRect avail_geometry = QApplication :: desktop () -> availableGeometry ( QPoint ( 0, 0 ) );
         SRAConfig config_window (  m, avail_geometry );
         config_window . show ();
        int status = app -> exec ();
        if ( status != 0 )
            exit ( status );
        return 0;
    }

    rc_t Quitting ( void )
    {
        // TBD - fill this out with a call to whatever QT uses to indicate
        // that the app has received a ^C or SIGTERM
        // the appropriate value to return if actually quitting is
        // RC ( rcExe, rcProcess, rcExecuting, rcProcess, rcCanceled );

        return 0;
    }
}
*/

const QString rsrc_path = ":/";

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
    mainWidget -> setObjectName ( "mainWidget" );

    mainLayout = new QHBoxLayout ();
    mainLayout -> setSpacing ( 0 );
    mainLayout -> setMargin ( 0 );

    toolBar = new SRAToolBar ( this );
    toolBar -> setObjectName ( "toolBar" );

    toolView = new SRAToolView ( config, this );
    toolView -> setObjectName ( "toolView" );

    connect ( toolBar, SIGNAL ( toolSwitched ( int ) ), toolView, SLOT ( toolChanged ( int ) ) );

    mainLayout -> addWidget ( toolBar );
    mainLayout -> addWidget ( toolView );

    mainWidget -> setLayout ( mainLayout );

    setCentralWidget ( mainWidget );

}

void SRAToolkit :: paintEvent ( QPaintEvent *e )
{
    QPainter painter(this);

    QPixmap pxmp ( rsrc_path + "images/ncbi_helix_blue_black" );
    pxmp = pxmp. scaled ( this -> size (), Qt::KeepAspectRatio, Qt::SmoothTransformation );

    painter.drawPixmap( size () . width () / 2 - pxmp.size ().width() / 2, size () . height () / 2 - pxmp.size ().height() / 2, pxmp );

    QWidget::paintEvent(e);
}
