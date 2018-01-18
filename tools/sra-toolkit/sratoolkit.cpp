#include "sratoolkit.h"
#include "diagnostics/diagnosticsview.h"

#include <kfg/config.h>
#include <kfg/properties.h>
#include <kfs/directory.h>
#include <klib/rc.h>

#include <QMenu>
#include <QMenuBar>

#include <QDebug>


SRAToolkit :: SRAToolkit ( const QRect &avail_geometry, QWidget *parent )
    : QMainWindow ( parent )
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
    resize ( ( avail_geometry . width () ) / 3, this -> height () );

    move ( ( avail_geometry . width () - this -> width () ) / 2,
           ( avail_geometry . height () - this -> height () ) / 2 );

    setup_menubar ();
}

SRAToolkit :: ~SRAToolkit ()
{
}

void SRAToolkit :: setup_menubar ()
{
    //QMenu *file = menuBar () -> addMenu ( tr ( "&File" ) );
    //QMenu *edit = menuBar () -> addMenu ( tr ( "\u200CEdit" ) ); // \u200C was added because OSX auto-adds some unwanted menu items
    QMenu *help = menuBar () -> addMenu ( tr ( "&Help" ) );
    help -> addAction ( tr ( "&Diagnostics" ), this, SLOT ( open_diagnostics () ) );
}

void SRAToolkit :: open_diagnostics ()
{
    diagnostics = new DiagnosticsView ( config, this );
}
