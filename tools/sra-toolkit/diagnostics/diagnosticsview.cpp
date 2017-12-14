#include "diagnosticsview.h"
#include "diagnosticstreemodel.h"
#include "diagnosticstreeitem.h"


#include <klib/rc.h>
#include <diagnose/diagnose.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
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
#include <QVector>

#include <QDebug>

const QString rsrc_path = ":/";


DiagnosticsView :: DiagnosticsView ( QWidget *parent )
    : QWidget ( parent )
    , self_layout ( new QVBoxLayout () )
    , model ( new DiagnosticsTreeModel () )
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
    QWidget *metadata = new QWidget ();
    metadata -> setFixedSize ( width () - 40, 160 );

    self_layout -> addWidget ( metadata );

    QFrame *separator = new QFrame ();
    separator -> setFrameShape ( QFrame::HLine );
    separator -> setFixedSize ( width () - 40, 2 );

    self_layout -> addWidget ( separator );

    tree_view = new QTreeView ();
    tree_view -> setFixedWidth ( width () - 40 );
    tree_view -> setSelectionMode ( QAbstractItemView::NoSelection );
    tree_view -> setAlternatingRowColors ( true );

    self_layout -> addWidget ( tree_view );

    QPushButton *run = new QPushButton ("Run Diagnostics");
    connect ( run, SIGNAL ( clicked () ), this, SLOT ( run_diagnostics () ) );
    run -> setFixedWidth ( 140 );

    self_layout -> addWidget ( run );
    self_layout -> setAlignment ( run, Qt::AlignRight );
}

static
void CC test_callback ( EKDiagTestState state, const KDiagnoseTest *test, void *data )
{
    DiagnosticsView *self = static_cast <DiagnosticsView *> (data);
    self -> handle_results ( state, test );
}

void DiagnosticsView :: handle_results ( EKDiagTestState state, const KDiagnoseTest *test )
{

    const char *name = 0;
    rc_t rc = KDiagnoseTestName ( test, &name );
    if ( rc != 0 )
        qDebug () << "Failed to get test name";
    else
    {
        uint32_t level;
        QString test_name = QString ( name );

        rc = KDiagnoseTestLevel ( test, &level );
        if ( rc != 0 )
            qDebug () << "failed to get test level for test: " << test_name;
        else
        {
            if ( level == 0 )
                return;

           DiagnosticsTreeItem *item = new DiagnosticsTreeItem ( QString ( name ), QString (""), level );

           model -> updateModelData ( item );

           tree_view -> setModel ( model );
            qDebug () << "Test: " << name << " State: " << state << " Test level: " << level;
        }
    }
}

void DiagnosticsView :: run_diagnostics ()
{

    QFile file ( "/Users/rodarme1/Desktop/output.txt" );
    if ( ! file . open ( QFile :: ReadWrite | QFile :: Text ) )
    {
        qDebug () << "Could not open file";
        return;
    }


    KDiagnose *diagnose = 0;

    rc_t rc = KDiagnoseMakeExt ( &diagnose, nullptr, nullptr, nullptr );
    if ( rc != 0 )
        qDebug () << rc;
    else
    {
        //test_results = "Beginning diagnosis...\t";
        //model = new DiagnosticsTreeModel ( test_results );
        tree_view -> setModel ( model );

        KDiagnoseTestHandlerSet ( diagnose, test_callback, this );

        rc = KDiagnoseAll ( diagnose, 0 );
        if ( rc != 0 )
                qDebug () << "failed";
        else
        {
            qDebug () << " didnt fail";
        }
    }

   // tree_view -> resizeColumnToContents ( 0 );
    //tree_view -> resizeColumnToContents ( 1 );
    //tree_view -> resizeColumnToContents ( 2 );

}


