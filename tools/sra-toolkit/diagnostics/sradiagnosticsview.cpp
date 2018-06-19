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

#include "sradiagnosticsview.h"
#include "diagnosticstest.h"
#include "diagnosticsthread.h"

#include <kfg/config.h>
#include <kfg/properties.h>
#include <klib/rc.h>

#include <diagnose/diagnose.h>

#include <QAbstractEventDispatcher>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>


#include <QDebug>

extern "C"
{
    rc_t Quitting ()
    {
        return 0;
    }
}


SRADiagnosticsView :: SRADiagnosticsView ( KConfig *p_config, QWidget *parent )
    : QWidget ( parent )
    , self_layout ( new QVBoxLayout () )
    , config ( p_config )
{
    setObjectName( "diagnostics_view" );
    setLayout ( self_layout );

    self_layout -> setAlignment (  Qt::AlignCenter  );
    self_layout -> setMargin ( 30 );

    init ();

    show ();
}

SRADiagnosticsView :: ~SRADiagnosticsView ()
{
    qDebug () << this << " destroyed.";
}


void SRADiagnosticsView :: init ()
{
    char buf [ 256 ];
    rc_t rc = KConfig_Get_Home ( config, buf, sizeof ( buf ), nullptr );
    if ( rc != 0 )
        qDebug () << "Failed to get home path";
    else
        homeDir = QString ( buf );

    init_metadata_view ();
    init_tree_view ();
    init_controls ();
}

void SRADiagnosticsView :: init_metadata_view ()
{
    QWidget *metadata = new QWidget ();
    metadata -> setObjectName ( "diagnostics_metadata" );
    metadata -> setFixedHeight ( 100 );

    QGridLayout *layout = new QGridLayout ( metadata );
    layout -> setColumnStretch ( 2, 2 );

    QLabel *homeDirTag = new QLabel ( "User Home Directory: " );
    QLabel *homeDirLabel = new QLabel ( homeDir );

    layout -> addWidget ( homeDirTag, 0, 0, 1, 1, Qt::AlignLeft );
    layout -> addWidget ( homeDirLabel, 0, 1, 1, 1, Qt::AlignLeft );

    self_layout -> addWidget ( metadata );

    QFrame *separator = new QFrame ();
    separator -> setFrameShape ( QFrame::HLine );
}

void SRADiagnosticsView :: init_tree_view ()
{
    tree_view = new QTreeWidget ();
    tree_view -> setSelectionMode ( QAbstractItemView::NoSelection );
    tree_view -> setAlternatingRowColors ( true );
    tree_view -> setColumnCount ( 3 );
    tree_view -> setHeaderLabels ( QStringList () << "Test" << "Description" << "Status" );
    tree_view -> resizeColumnToContents ( 2 );

    self_layout -> addWidget ( tree_view );
}

void SRADiagnosticsView :: init_controls ()
{

    start_button = new QPushButton ("Start");
    start_button -> setObjectName ( "start_diagnostics_button" );
    start_button -> setFixedSize ( 120, 60 );

    cancel_button = new QPushButton ("Cancel");
    cancel_button -> setObjectName ( "cancel_diagnostics_button" );
    cancel_button -> setFixedSize ( 120, 60 );
    cancel_button -> hide ();

    p_bar = new QProgressBar ();
    p_bar -> hide ();
    p_bar -> setFixedWidth ( 120 );
    p_bar -> setMinimum ( 0 );
    p_bar -> setMaximum ( 130 );

    connect ( start_button, SIGNAL ( clicked () ), this, SLOT ( start_diagnostics () ) );
    connect ( cancel_button, SIGNAL ( clicked () ), this, SLOT ( cancel_diagnostics () ) );

    QHBoxLayout *controls_layout = new QHBoxLayout ();
    controls_layout -> setAlignment ( Qt::AlignRight );

    controls_layout -> addStretch ( 1 );
    controls_layout -> addWidget ( p_bar );
    controls_layout -> addWidget ( start_button );
    controls_layout -> addWidget ( cancel_button );

    self_layout -> addLayout ( controls_layout );
}


void SRADiagnosticsView :: handle_callback ( DiagnosticsTest *test )
{
    switch ( test -> getState () )
    {
    case DTS_Started:
    {
        switch ( test -> getLevel () )
        {
        case 0:
            break;
        case 1:
        {
            QTreeWidgetItem *item = new QTreeWidgetItem ( tree_view );
            item -> setText ( 0, test -> getName () );
            item -> setText ( 1, QString ("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX") );
            item -> setText ( 2, "In Progress..." );
            item -> setExpanded ( true );
            currentTest = item;
            break;
        }
        case 2:
        case 3:
        case 4:
        case 5:
        {
            QTreeWidgetItem *item = new QTreeWidgetItem ();
            item -> setText ( 0, test -> getName () );
            item -> setText ( 2, "In Progress..." );
            currentTest -> addChild ( item );
            currentTest = item;
            break;
        }
        default:
            break;
        }
        break;
    }
    case DTS_Succeed:
    {
        QTreeWidgetItem *parent = currentTest -> parent ();
        currentTest -> setText ( 2, "Passed" );
        currentTest -> setTextColor ( 2, QColor ("green") );

        switch ( test -> getLevel () )
        {
        case 0:
        case 1:
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        {
            currentTest = parent;
            break;
        }
        default:
            break;
        }

        break;
    }
    case DTS_Failed:
    {
        QTreeWidgetItem *parent = currentTest -> parent ();
        currentTest -> setText ( 2, "Failed" );
        currentTest -> setTextColor ( 2, QColor ("red") );

        switch ( test -> getLevel () )
        {
        case 0:
        case 1:
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        {
            currentTest = parent;
            break;
        }
        default:
            break;
        }

        break;
    }

    case DTS_NotStarted:
    case DTS_Skipped:
    case DTS_Warning:
    case DTS_Paused:
    case DTS_Resumed:
    case DTS_Canceled:
    default:
        break;
    }

    tree_view -> resizeColumnToContents ( 0 );
    tree_view -> resizeColumnToContents ( 1 );
    tree_view -> resizeColumnToContents ( 2 );

    progress_val += 1;
    p_bar -> setValue ( progress_val );
}

void SRADiagnosticsView :: start_diagnostics ()
{
    if ( tree_view -> children () . count () > 0 )
        tree_view -> clear ();

    thread = new QThread;
    worker = new DiagnosticsThread ();
    worker -> moveToThread ( thread );

    connect ( thread, SIGNAL ( started () ), worker, SLOT ( begin() ) );
    connect ( worker, SIGNAL ( handle ( DiagnosticsTest* ) ), this, SLOT ( handle_callback ( DiagnosticsTest * ) ) );
    connect ( worker, SIGNAL ( finished () ), worker, SLOT ( deleteLater () ) );
    connect ( worker, SIGNAL ( finished () ), this, SLOT ( worker_finished () ) );
    connect ( thread, SIGNAL ( finished () ), thread, SLOT ( deleteLater () ) );

    progress_val = 0;
    thread -> start ();

    p_bar -> show ();
    start_button -> hide ();
    cancel_button -> show ();
}

void SRADiagnosticsView :: cancel_diagnostics ()
{
    //worker -> cancel ();
    //thread -> quit ();
    //thread -> wait ();

    //self_layout -> replaceWidget ( cancel_button, start_button );
   // self_layout -> setAlignment ( start_button, Qt::AlignRight );

    cancel_button -> hide ();
    start_button -> show ();
}

void SRADiagnosticsView :: worker_finished ()
{
    p_bar -> reset ();
    p_bar -> hide ();
    cancel_button -> hide ();
    start_button -> show ();
}


