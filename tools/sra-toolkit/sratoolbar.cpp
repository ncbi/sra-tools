#include "sratoolbar.h"

#include <QVBoxLayout>
#include <QBrush>
#include <QButtonGroup>
#include <QColor>
#include <QLabel>
#include <QPainter>
#include <QPushButton>

#include <QDebug>

SRAToolBar::SRAToolBar ( QWidget *parent )
    : QWidget ( parent )
{
    resize ( QSize ( 100, parent -> size () . height () ) );
    setFixedWidth ( 100 );

    init ();
}

void SRAToolBar::init ()
{
    QButtonGroup *toolBar =  new QButtonGroup ( this );
    toolBar -> setExclusive ( true );
    connect ( toolBar, SIGNAL ( buttonClicked ( int ) ), this , SLOT ( switchTool ( int ) ) );

    QPixmap pxmp (":/images/ncbi_logo_vertical_white_blue");
    QIcon icon ( pxmp );
    home_button = new QPushButton ();
    home_button -> setObjectName ( "diagnostics_button" );
    home_button -> setCheckable ( true );
    home_button -> setChecked ( true );
    home_button -> setIconSize ( QSize ( 90, 90 ) );
    home_button -> setIcon ( icon );

    pxmp = QPixmap ( ":/images/ncbi_diagnostics_button" );
    icon = QIcon ( pxmp );
    diagnostics_button = new QPushButton ();
    diagnostics_button -> setObjectName ( "diagnostics_button" );
    diagnostics_button -> setCheckable ( true );
    diagnostics_button -> setIconSize ( QSize ( 90, 90 ) );
    diagnostics_button -> setIcon ( icon );

    toolBar -> addButton ( home_button );
    toolBar -> setId ( home_button, 0 );

    toolBar -> addButton ( diagnostics_button );
    toolBar -> setId ( diagnostics_button, 1 );

    QVBoxLayout *layout = new QVBoxLayout ();
    layout -> setMargin ( 0 );
    layout -> setSpacing ( 12 );
    layout -> setAlignment ( Qt::AlignTop );

    layout -> addWidget ( home_button );
    layout -> addWidget ( diagnostics_button );

    setLayout ( layout );
}

void SRAToolBar :: switchTool ( int tool )
{
    emit toolSwitched ( tool );
}

void SRAToolBar :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    QColor color ( 70, 102, 152, 255 );

    painter.setBrush ( color );

    painter.drawRect ( 0, 0, 100, size () . height () );

    show ();

    QWidget::paintEvent(e);
}
