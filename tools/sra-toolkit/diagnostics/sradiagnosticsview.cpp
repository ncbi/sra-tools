#include "diagnosticsview.h"
#include "diagnosticstest.h"
#include "diagnosticsthread.h"

#include <kfg/config.h>
#include <kfg/properties.h>
#include <klib/rc.h>

#include <diagnose/diagnose.h>

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>


#include <QDebug>

const QString rsrc_path = ":/";


DiagnosticsView :: DiagnosticsView ( KConfig *p_config, QWidget *parent )
    : QWidget ( parent )
    , self_layout ( new QVBoxLayout () )
    , config ( p_config )
{
    setWindowFlags ( Qt::Window );
    setAttribute ( Qt::WA_DeleteOnClose );
    setFixedSize ( 800, 400 );
    setLayout ( self_layout );

    self_layout -> setAlignment (  Qt::AlignCenter  );
    self_layout -> setMargin ( 10 );

    setup_view ();

    show ();
}

DiagnosticsView :: ~DiagnosticsView ()
{
    qDebug () << this << " destroyed.";
}


void DiagnosticsView :: setup_view ()
{
    // Metadata
    QWidget *metadata = new QWidget ();
    metadata -> setFixedSize ( width () - 40, 120 );

    char buf [ 256 ];
    rc_t rc = KConfig_Get_Home ( config, buf, sizeof ( buf ), nullptr );
    if ( rc != 0 )
        qDebug () << "Failed to get home path";
    else
    {
        QString home = QString ( buf );
        qDebug () << home;
    }

    self_layout -> addWidget ( metadata );
    // Metadata


    QFrame *separator = new QFrame ();
    separator -> setFrameShape ( QFrame::HLine );
    separator -> setFixedSize ( width () - 40, 2 );

    self_layout -> addWidget ( separator );

    // Test view
    tree_view = new QTreeWidget ();
    tree_view -> setFixedWidth ( width () - 40 );
    tree_view -> setSelectionMode ( QAbstractItemView::NoSelection );
    tree_view -> setAlternatingRowColors ( true );
    tree_view -> setColumnCount ( 3 );
    tree_view -> setHeaderLabels ( QStringList () << "Test" << "Description" << "Status" );
    tree_view -> resizeColumnToContents ( 2 );

    self_layout -> addWidget ( tree_view );

    QPushButton *run = new QPushButton ("Run Diagnostics");
    connect ( run, SIGNAL ( clicked () ), this, SLOT ( run_diagnostics () ) );
    run -> setFixedWidth ( 140 );

    self_layout -> addWidget ( run );
    self_layout -> setAlignment ( run, Qt::AlignRight );
    // Test view
}

void DiagnosticsView :: handle_callback ( DiagnosticsTest *test )
{
    //qDebug () << test -> getName ();

    QAbstractEventDispatcher *dispatch = QApplication::eventDispatcher();

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

        switch ( test -> getLevel () )
        {
        case 0:
            break;
        case 1:
        {
            dispatch -> processEvents (QEventLoop::AllEvents);
            break;
        }
        case 2:
        {
            currentTest = parent;
            dispatch -> processEvents (QEventLoop::AllEvents);
            break;
        }
        case 3:
        case 4:
        case 5:
        {
            parent -> removeChild ( currentTest );
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
        currentTest -> setText ( 2, "Passed" );

        switch ( test -> getLevel () )
        {
        case 0:
            break;
        case 1:
        {
            dispatch -> processEvents (QEventLoop::AllEvents);
            break;
        }
        case 2:
        case 3:
        case 4:
        case 5:
        {
            currentTest = parent;
            dispatch -> processEvents (QEventLoop::AllEvents);
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
}

void DiagnosticsView :: run_diagnostics ()
{
    if ( tree_view -> children () . count () > 0 )
        tree_view -> clear ();

    QThread *thread = new QThread;
    DiagnosticsThread *worker = new DiagnosticsThread ();
    worker -> moveToThread ( thread );

    connect ( thread, SIGNAL ( started () ), worker, SLOT ( begin() ) );
    connect ( worker, SIGNAL ( handle ( DiagnosticsTest* ) ), this, SLOT ( handle_callback ( DiagnosticsTest * ) ) );
    connect ( worker, SIGNAL ( finished () ), worker, SLOT ( deleteLater () ) );
    connect ( thread, SIGNAL ( finished () ), thread, SLOT ( deleteLater () ) );

    thread -> start ();
}


