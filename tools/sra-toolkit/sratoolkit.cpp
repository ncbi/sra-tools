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


#include "sratoolkit.h"
#include "sratoolkitglobals.h"
#include "sratoolbar.h"
#include "sratoolview.h"
#include "config/sraconfigview.h"
#include "config/testwidget.h"

#include <kapp/main.h>
#include <kfg/config.h>
#include <kfg/properties.h>
#include <kfs/directory.h>
#include <klib/rc.h>

#include <QtWidgets>


SRAToolkit :: SRAToolkit ( QWidget *parent )
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

    QSize avail_size = QGuiApplication::primaryScreen () -> availableSize ();
    if ( avail_size . isNull () ||
         avail_size . width () < 1000 ||
         avail_size . height () < 600 )
    {
        // Minimum size if no size can be determined or screen is too small
        // 1440 * 900
        setMinimumSize ( 1440 / 2, 900 / 1.75  );
    }
    else
    {
        setMinimumSize ( avail_size . width () / 2.5, avail_size . height () / 2 );
    }

    init ();
}

SRAToolkit :: ~SRAToolkit ()
{
}

void SRAToolkit :: init ()
{
    mainWidget = new QWidget ();
    mainWidget -> setObjectName ( "main_widget" );
    setCentralWidget ( mainWidget );

    mainLayout = new QHBoxLayout ();
    mainLayout -> setSpacing ( 0 );
    mainLayout -> setMargin ( 0 );
    mainWidget -> setLayout ( mainLayout );

    toolBar = new SRAToolBar ( this );

    toolView = new SRAToolView ( config, mainWidget );
    toolView -> setObjectName ( "sratool_view" );

    connect ( toolBar, SIGNAL ( toolSwitched ( int ) ), toolView, SLOT ( toolChanged ( int ) ) );
    connect ( toolBar, SIGNAL ( expanded ( bool ) ), this, SLOT ( expand ( bool ) ) );

    mainLayout -> addWidget ( toolBar );
    mainLayout -> addWidget ( toolView );
}


void SRAToolkit :: expand ( bool val )
{
    if ( val )
        resize ( size () . width () + TOOLBAR_WIDTH_FACTOR, size () . height () );
    else
        resize ( size () . width () - TOOLBAR_WIDTH_FACTOR, size () . height () );
}

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


