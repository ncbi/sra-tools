#include "testwidget.h"

#include <QVBoxLayout>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>

#include <QDebug>

TestWidget :: TestWidget ( QWidget *parent )
: QWidget(parent)
{
    layout = new QVBoxLayout ();
    layout -> setMargin ( 5 );
    setLayout ( layout );

    QWidget *w = new QWidget ();
    w -> setFixedHeight ( 100 );
    w -> setStyleSheet("background-color: blue");

    layout -> addWidget ( w );

    setupScrollView ();
}

static
QButtonGroup * make_no_yes_button_group ( QPushButton **p_no, QPushButton **p_yes )
{
    QButtonGroup *group = new QButtonGroup ();

    QPushButton *no = new QPushButton ( "No" );
    no -> setObjectName ( "button_no" );
    no -> setCheckable ( true );
    no -> setFixedSize ( 60, 40 );

    QPushButton *yes = new QPushButton ( "Yes" );
    yes -> setObjectName ( "button_yes" );
    yes -> setCheckable ( true );
    yes -> setFixedSize ( 60, 40 );

    group -> addButton ( no, 0 );
    group -> addButton ( yes, 1 );

    *p_no = no;
    *p_yes = yes;

    return group;
}

static
QWidget * make_button_option_row ( QString name, QString desc,
                                   QPushButton *b1, QPushButton *b2 )
{
    QWidget *row = new QWidget ();
    row -> setObjectName ( "row_widget" );

    QLabel *name_l = new QLabel ( name );
    name_l -> setObjectName ( "name_label" );
    name_l -> setFixedHeight ( 40 );

    QLabel *desc_l = new QLabel ( desc );
    desc_l -> setObjectName ( "desc_label" );
    desc_l -> setFixedWidth ( ( row -> size () . width () ) );
    desc_l -> setWordWrap ( true );

    QHBoxLayout *row_h_layout = new QHBoxLayout ();
    row_h_layout -> setSpacing ( 10 );
    row_h_layout -> addWidget ( name_l );
    row_h_layout -> setAlignment ( name_l, Qt::AlignLeft );
    row_h_layout -> addWidget ( b1 );
    row_h_layout -> setAlignment ( b1, Qt::AlignRight );
    row_h_layout -> addWidget ( b2 );

    QVBoxLayout *row_v_layout = new QVBoxLayout ();
    row_v_layout -> setSpacing ( 5 );
    row_v_layout -> addLayout ( row_h_layout );
    row_v_layout -> addWidget ( desc_l, 0, Qt::AlignLeft );

    row -> setLayout ( row_v_layout );

    return row;
}

void TestWidget :: setupScrollView ()
{
    QVBoxLayout *scrollLayout = new QVBoxLayout ();
    scrollLayout -> setAlignment ( Qt::AlignTop );
    scrollLayout -> setSpacing ( 0 );

    QWidget *scrollView = new QWidget ();
    scrollView -> setStyleSheet("background-color: yellow");
    scrollView -> setLayout ( scrollLayout );
    scrollView -> setSizePolicy ( QSizePolicy::Ignored, QSizePolicy::Ignored);

    QScrollArea *scrollArea = new QScrollArea ();
    scrollArea -> setWidget ( scrollView );
    scrollArea -> setWidgetResizable ( true );

    {
        QLabel *label = new QLabel ( "General Settings" );
        QPushButton *no = nullptr;
        QPushButton *yes = nullptr;
        QString name;
        QString desc;

        scrollLayout -> addWidget ( label );
        scrollLayout -> addSpacing ( 5 );

        // row 1
        name = QString ("Enable Remote Access");
        desc = QString ("Connect to NCBI over http or https. Connect to NCBI over http or https."
                        "Connect to NCBI over http or https.Connect to NCBI over http or https."
                        "Connect to NCBI over http or https.Connect to NCBI over http or https.");
        connect ( bg_remote_access = make_no_yes_button_group ( &no, &yes ),
                  SIGNAL ( buttonClicked ( int ) ), this, SLOT ( toggle_remote_enabled ( int ) ) );

        scrollLayout -> addWidget ( make_button_option_row ( name, desc, no, yes ) );
        // row 1

        // row 2
        name = QString ("Enable Local File Caching");
        connect ( bg_local_caching = make_no_yes_button_group ( &no, &yes ),
                  SIGNAL ( buttonClicked ( int ) ), this, SLOT ( toggle_local_caching ( int ) ) );

        scrollLayout -> addWidget ( make_button_option_row ( name, desc, no, yes ) );
        // row 2
    }

    layout -> addWidget ( scrollArea );
}

