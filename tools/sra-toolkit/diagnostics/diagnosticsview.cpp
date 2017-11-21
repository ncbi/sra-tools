#include "diagnosticsview.h"
#include "diagnosticstreemodel.h"


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


void DiagnosticsView :: run_diagnostics ()
{
    rc_t rc = 0;

    QFile file ( "/Users/rodarme1/Desktop/output.txt" );
    if ( ! file . open ( QFile :: ReadWrite | QFile :: Text ) )
    {
        qDebug () << "Could not open file";
        return;
    }

    DiagnosticsTreeModel *model = 0;

    KDiagnose *test = 0;

    rc = KDiagnoseMakeExt ( &test, nullptr, nullptr, nullptr );
    if ( rc != 0 )
        qDebug () << rc;
    else
    {
        QString text ( "Diagnosing configuration...\t" );
        model = new DiagnosticsTreeModel ( text );
        tree_view -> setModel ( model );

        rc = KDiagnoseAll ( test, 0 );
        if ( rc != 0 )
            qDebug () << rc;
        else
        {
            QString empty = QString ( "%1\t").arg("",80);
            text . append ( empty + "Pass\n" );
            model = new DiagnosticsTreeModel ( text );
            tree_view -> setModel ( model );
        }

    }

    tree_view -> resizeColumnToContents ( 0 );
    tree_view -> resizeColumnToContents ( 1 );
    tree_view -> resizeColumnToContents ( 2 );

}


