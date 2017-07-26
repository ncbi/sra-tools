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
    , cb_all ( new QCheckBox ( "Run All" ) )
    , cb_config ( new QCheckBox ( "Configuration" ) )
    , cb_network ( new QCheckBox ( "Network" ) )
    , cb_fail ( new QCheckBox ( "Fail" ) )
{
    setWindowFlags ( Qt::Window );
    setAttribute ( Qt::WA_DeleteOnClose );
    setFixedSize ( 800, 400 );
    setLayout ( self_layout );

    self_layout -> setAlignment ( Qt::AlignTop | Qt:: AlignLeft );
    self_layout -> setMargin ( 20 );
    self_layout -> setSpacing ( 10 );

    setup_view ();

    show ();
}

DiagnosticsView :: ~DiagnosticsView ()
{
    qDebug () << this << " destroyed.";
}

void DiagnosticsView :: init_metadata_view ()
{
    QWidget *view = new QWidget ();
    view -> setFixedSize ( QSize ( this -> width (), 100 ) );

    QHBoxLayout *layout = new QHBoxLayout ();
    layout -> setMargin ( 10 );
    layout -> setSpacing ( 0 );
    layout -> setAlignment ( Qt::AlignLeft );

    QPixmap pxmp = QPixmap :: fromImage ( QImage ( rsrc_path + "images/network_icon" ) );
    QLabel *icon = new QLabel ();
    icon -> setPixmap ( pxmp . scaled ( QSize ( 80, 80 ), Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
    icon -> setScaledContents ( true );
    icon -> setFixedSize ( 80, 80 );

    layout -> addWidget ( icon, 0, Qt::AlignLeft );

    // 1.1
    QVBoxLayout *label_layout = new QVBoxLayout ();
    label_layout -> setMargin ( 5 );
    label_layout -> setSpacing ( 10 );
    label_layout -> setAlignment ( Qt::AlignTop );

    QLabel *title = new QLabel ( "Workspace" );
    QFont f = title -> font ();
    f . setPointSize ( 24 );
    title -> setFont ( f );

    QLabel *subtitle = new QLabel ( "Subtitle with metadata" );
    f = title -> font ();
    f . setPointSize ( 14 );
    subtitle -> setFont ( f );

    QPalette p = subtitle -> palette ();
    p . setColor ( QPalette::WindowText, Qt::gray );
    subtitle -> setPalette ( p );

    label_layout -> addWidget ( title );
    label_layout -> addWidget ( subtitle );

    layout -> addLayout ( label_layout );
    layout -> addSpacing ( 70 );

    // 2
    QTableWidget *table = new QTableWidget ( view );
    table -> setFixedSize ( 445, 80 );
    table -> setFocusPolicy ( Qt::NoFocus );
    table -> setFrameShape ( QFrame::NoFrame );
    table -> setShowGrid ( false );
    table -> setSelectionMode ( QAbstractItemView::NoSelection );
    table -> setStyleSheet ( "QTableView { background-color: white; }" );
    table -> setEditTriggers ( QAbstractItemView::NoEditTriggers );
    table -> setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );

    table -> setColumnCount ( 4 );
    table -> horizontalHeader () -> setVisible ( false );

    table -> setRowCount ( 2 );
    table -> verticalHeader () -> setVisible ( false );
    table -> verticalHeader () -> setSectionResizeMode ( QHeaderView::Fixed );
    table -> verticalHeader () -> setDefaultSectionSize ( 30 );

    table -> setItem ( 0, 0, new QTableWidgetItem ( "Location:" ) );
    table -> setItem ( 0, 1, new QTableWidgetItem ( "/home/rodarme1/ncbi" ) );
    table -> setItem ( 1, 0, new QTableWidgetItem ( "Size:" ) );
    table -> setItem ( 1, 1, new QTableWidgetItem ( "1 TB" ) );
    table -> setItem ( 0, 2, new QTableWidgetItem ( "Network:" ) );
    table -> setItem ( 0, 3, new QTableWidgetItem ( "Connected" ) );

    table -> item ( 0,1 ) -> setTextAlignment ( Qt::AlignRight | Qt::AlignVCenter );
    table -> item ( 1,1 ) -> setTextAlignment ( Qt::AlignRight | Qt::AlignVCenter );
    table -> item ( 0,3 ) -> setTextAlignment ( Qt::AlignRight | Qt::AlignVCenter );
    //table -> item ( 1,3 ) -> setTextAlignment ( Qt::AlignRight | Qt::AlignVCenter );

    table -> resizeColumnToContents ( 0 );
    table -> setColumnWidth ( 1, ( table -> width () / 2 ) - table -> columnWidth ( 0 ) );
    table -> resizeColumnToContents ( 2 );
    table -> setColumnWidth ( 3, ( table -> width () / 2 ) - table -> columnWidth ( 0 ) );
    table -> resizeRowsToContents ();

    table -> setAlternatingRowColors ( true );

    layout -> addWidget ( table, 0, Qt::AlignRight);

    view -> setLayout ( layout );

    self_layout -> addWidget ( view );
}

void DiagnosticsView :: init_test_view ()
{
    QWidget *w = new QWidget ();
    w -> setFixedSize ( QSize ( this -> width() - 40, 200 ) );

    QHBoxLayout *layout = new QHBoxLayout ();
    layout -> setMargin ( 0 );
    layout -> setSpacing ( 15 );

    QGroupBox *test_selection_box = new QGroupBox ( "Test Selection" );
    test_selection_box -> setFixedWidth ( 200 );

    QVBoxLayout *cb_layout = new QVBoxLayout ();
    test_selection_box -> setLayout ( cb_layout );

    cb_layout -> addWidget ( cb_all );
    cb_layout -> addWidget ( cb_config );
    cb_layout -> addWidget ( cb_network );
    cb_layout -> addWidget ( cb_fail );

    layout -> addWidget ( test_selection_box, 0, Qt::AlignLeft );

    tree_view = new QTreeView ();
    tree_view -> clearFocus ();
    tree_view -> setSelectionMode ( QAbstractItemView::NoSelection );
    tree_view -> setAlternatingRowColors ( true );
    layout -> addWidget ( tree_view );

    w -> setLayout ( layout );

    self_layout -> addWidget ( w );
}

void DiagnosticsView :: setup_view ()
{
    // 1
    init_metadata_view ();

    // 2
    QFrame *separator = new QFrame ();
    separator -> setFrameShape ( QFrame::HLine );
    separator -> setFixedSize ( this -> width () - 40, 2 );

    self_layout -> addWidget ( separator );

    // 3
    init_test_view ();

    // 4
    QPushButton *run_diagnostics_button = new QPushButton ( "Run Diagnostics" );
    connect ( run_diagnostics_button, SIGNAL ( clicked () ), this, SLOT ( run_diagnostics () ) );
    self_layout -> addWidget ( run_diagnostics_button, 0, Qt::AlignRight );
}


void DiagnosticsView :: run_diagnostics ()
{
    rc_t rc = 0;

    KDiagnose *test = 0;

    QFile file ( "/Users/rodarme1/Desktop/output.txt" );
    if ( ! file . open ( QFile :: ReadWrite | QFile :: Text ) )
    {
        qDebug () << "Could not open file";
        return;
    }

    DiagnosticsTreeModel *model = 0;

    if ( cb_all -> isChecked () )
    {
        cb_config -> setChecked ( true );
        cb_network -> setChecked ( true );
        cb_fail -> setChecked ( true );
    }

    if ( cb_config -> isChecked () )
    {
    /*
        const char *text = "Diagnosing configuration...";
        file . write ( text, qstrlen ( text ) );
        */
        QString text ( "Diagnosing configuration...\t" );
        model = new DiagnosticsTreeModel ( text );
        tree_view -> setModel ( model );
        //file . close ();
        rc = KDiagnoseRun ( test, DIAGNOSE_CONFIG );
        qDebug () << rc;
        if ( rc == 0 )
        {
            QString empty = QString ( "%1\t").arg("",80);
            text . append ( empty + "Pass\n" );
            model = new DiagnosticsTreeModel ( text );
            tree_view -> setModel ( model );
        }
    }

    if ( cb_network -> isChecked () )
        rc = KDiagnoseRun ( test, DIAGNOSE_NETWORK );

    if ( cb_fail -> isChecked () )
        rc = KDiagnoseRun ( test, DIAGNOSE_FAIL );



    tree_view -> resizeColumnToContents ( 0 );
    tree_view -> resizeColumnToContents ( 1 );
    tree_view -> resizeColumnToContents ( 2 );

}


