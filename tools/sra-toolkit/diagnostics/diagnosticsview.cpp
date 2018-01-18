#include "diagnosticsview.h"
#include "diagnosticstest.h"

#include <kfg/config.h>
#include <kfg/properties.h>
#include <klib/rc.h>

#include <diagnose/diagnose.h>

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QEventLoop>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStringList>
#include <QVector>

#include <QDebug>

const QString rsrc_path = ":/";


DiagnosticsView :: DiagnosticsView ( KConfig *p_config, QWidget *parent )
    : QWidget ( parent )
    , self_layout ( new QVBoxLayout () )
    , config ( p_config )
    , diagnose ( 0 )
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

static
void CC diagnose_callback ( EKDiagTestState state, const KDiagnoseTest *diagnose_test, void *data )
{
    const char *name;

    DiagnoseStruct *ds = static_cast <DiagnoseStruct *> ( data );

    rc_t rc = KDiagnoseTestName ( diagnose_test, &name );
    if ( rc ==  0 )
    {
        uint32_t test_level = 0;

        rc = KDiagnoseTestLevel ( diagnose_test, &test_level );
        if ( rc == 0 )
        {
            DiagnosticsTest *test = new DiagnosticsTest ( QString ( name ) );
            test -> setLevel ( test_level );
            test -> setState ( state );

/*
            const KDiagnoseTestDesc *desc;
            rc = KDiagnoseGetDesc ( ds -> diagnose, &desc );
            if ( rc != 0 )
                qDebug () << "Failed to get test description object for test: " << QString ( name );
            else
            {
                const char *buf;
                rc = KDiagnoseTestDescDesc ( desc, &buf );
                if ( rc != 0 )
                    qDebug () << "failed to get description for test: " << QString ( name );
                else
                   // test -> setDescription ( QString ( buf ) );
                    qDebug () << QString ( buf );
            }
*/

           // DiagnosticsView *self = static_cast <DiagnosticsView *> (data);

            ds -> self -> handle_callback ( test );
        }
    }
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
    rc_t rc = 0;

    rc = KDiagnoseMakeExt ( &diagnose, nullptr, nullptr, nullptr );
    if ( rc != 0 )
        qDebug () << rc;
    else
    {
        if ( tree_view -> children () . count () > 0 )
            tree_view -> clear ();

        DiagnoseStruct *ds = new DiagnoseStruct ();
        ds -> self = this;
        ds -> diagnose = diagnose;

        KDiagnoseTestHandlerSet ( diagnose, diagnose_callback, ds );

        rc = KDiagnoseAll ( diagnose, 0 );
        if ( rc != 0 )
           qDebug () << rc;
    }
}


