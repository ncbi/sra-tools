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

#include "sratoolview.h"
#include "sratoolkitglobals.h"
#include "config/sraconfigview.h"
#include "diagnostics/sradiagnosticsview.h"

#include <kfg/config.h>

#include <QtWidgets>

SRAToolView :: SRAToolView ( KConfig *p_config, QWidget *parent )
    : QWidget ( parent )
    , config ( p_config )
    , home ( nullptr )
    , config_view ( nullptr )
    , diagnostics_view ( nullptr )
    , currentView ( nullptr )
{
    setObjectName ( "sra_tool_view" );
    setSizePolicy ( QSizePolicy::Ignored, QSizePolicy::Ignored);
    resize ( parent -> size () . width () - TOOLBAR_WIDTH_FACTOR, parent -> size () . height () );

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

void SRAToolView :: resizeEvent ( QResizeEvent *e )
{
    QWidget::resizeEvent ( e );
    config_view -> resize ( size () );
    diagnostics_view -> resize ( size () );
}
