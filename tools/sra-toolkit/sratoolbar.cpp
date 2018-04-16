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

#include "sratoolbar.h"
#include "sratoolkitglobals.h"

#include <QtWidgets>

#define TOOLBAR_BUTTON_WIDTH_SHORT 96
#define TOOLBAR_BUTTON_WIDTH_LONG 192
#define TOOLBAR_BUTTON_HEIGHT 64

SRAToolBar::SRAToolBar ( QWidget *parent )
    : QWidget ( parent )
    , isExpanded ( false )
{
    setObjectName ( "tool_bar_view" );
    setFixedWidth ( TOOLBAR_WIDTH_FACTOR );

    init ();
}

void SRAToolBar::init ()
{
    QButtonGroup *toolBar =  new QButtonGroup ( this );
    toolBar -> setExclusive ( true );
    connect ( toolBar, SIGNAL ( buttonClicked ( int ) ), this , SLOT ( switchTool ( int ) ) );

    home_button = new QPushButton ();
    home_button -> setObjectName ( "tool_bar_button" );
    home_button -> setProperty ( "type", "home" );
    home_button -> setFixedHeight ( 72 );
    home_button -> setCheckable ( true );
    home_button -> setChecked ( true );

    config_button = new QPushButton ();
    config_button -> setObjectName ( "tool_bar_button" );
    config_button -> setIcon ( QIcon ( img_path + "ncbi_config_button.svg" ) );
    config_button -> setFixedHeight ( TOOLBAR_BUTTON_HEIGHT );
    config_button -> setCheckable ( true );

    diagnostics_button = new QPushButton ();
    diagnostics_button -> setObjectName ( "tool_bar_button" );
    diagnostics_button -> setIcon ( QIcon ( img_path + "ncbi_diagnostics_button.svg" ) );
    diagnostics_button -> setFixedHeight ( TOOLBAR_BUTTON_HEIGHT );
    diagnostics_button -> setCheckable ( true );

    expand_button = new QPushButton ();
    connect ( expand_button, SIGNAL ( clicked () ), this, SLOT ( expand () ) );
    expand_button -> setObjectName ( "tool_bar_button" );
    expand_button -> setProperty ( "type", "expand" );
    expand_button -> setFixedHeight ( 48 );

    updateButtonIcons ();

    QFrame *line = new QFrame ();
    line -> setObjectName ( "spacer_line" );
    line -> setFrameShape ( QFrame::HLine );
    line -> setFixedHeight ( 2 );
    line -> setMinimumWidth ( width () * .8 );

    QFrame *line2= new QFrame ();
    line2 -> setObjectName ( "spacer_line" );
    line2 -> setFrameShape ( QFrame::HLine );
    line2 -> setFixedHeight ( 2 );
    line2 -> setMinimumWidth ( width () * .8 );

    toolBar -> addButton ( home_button );
    toolBar -> setId ( home_button, 0 );

    toolBar -> addButton ( config_button );
    toolBar -> setId ( config_button, 1 );

    toolBar -> addButton ( diagnostics_button );
    toolBar -> setId ( diagnostics_button, 2 );

    QVBoxLayout *layout = new QVBoxLayout ( );
    layout -> setMargin ( 0 );
    layout -> setSpacing ( 0 );

    layout -> addWidget ( home_button );
    layout -> addSpacing ( 20 );
    layout -> addWidget ( line );
    layout -> addSpacing ( 12 );
    layout -> addWidget ( config_button );
    layout -> addSpacing ( 8 );
    layout -> addWidget ( diagnostics_button );
    layout -> addSpacing ( 20 );
    layout -> addWidget ( line2 );

    layout -> addWidget ( expand_button, 0, Qt::AlignBottom );

    setLayout ( layout );
}

void SRAToolBar :: updateButtonIcons ()
{
    if ( isExpanded )
    {
        home_button -> setIcon ( QIcon ( img_path + "ncbi_logo_long.svg") . pixmap ( QSize ( 176, 48 ) ) );
        home_button -> setIconSize ( QSize ( TOOLBAR_BUTTON_WIDTH_LONG, 70 ) );

        config_button -> setText ( "Configuration" );
        diagnostics_button -> setText ("Diagnostics     ");

        expand_button -> setIcon ( QIcon ( img_path + "ncbi_expand_button_close.svg" ) );
        expand_button -> setIconSize ( QSize ( TOOLBAR_BUTTON_WIDTH_LONG, 48 ) );
    }
    else
    {
        home_button -> setIcon ( QIcon ( img_path + "ncbi_helix_vertical.svg") );
        home_button -> setIconSize ( QSize ( TOOLBAR_BUTTON_WIDTH_SHORT, TOOLBAR_BUTTON_HEIGHT ) );

        config_button -> setText ( "" );
        config_button -> setIconSize ( QSize ( TOOLBAR_BUTTON_WIDTH_SHORT, TOOLBAR_BUTTON_HEIGHT ) );

        diagnostics_button -> setText ( "" );
        diagnostics_button -> setIconSize ( QSize ( TOOLBAR_BUTTON_WIDTH_SHORT, TOOLBAR_BUTTON_HEIGHT ) );

        expand_button -> setIcon ( QIcon ( img_path + "ncbi_expand_button_open.svg" ) );
        expand_button -> setIconSize ( QSize ( TOOLBAR_BUTTON_WIDTH_SHORT, 48 ) );
    }
}

void SRAToolBar :: switchTool ( int tool )
{
    emit toolSwitched ( tool );
}

void SRAToolBar :: expand ()
{
    int s = size () . width ();
    if ( isExpanded )
        s = s - TOOLBAR_WIDTH_FACTOR;
    else
        s = s + TOOLBAR_WIDTH_FACTOR;;

    isExpanded = !isExpanded;
    resize ( QSize ( s, size () . height () ) );
    setFixedWidth ( s );

    updateButtonIcons ();

    emit expanded ( isExpanded );
}

void SRAToolBar :: paintEvent ( QPaintEvent *e )
{
    QPainter painter ( this );

    QLinearGradient gradient = sraTemplate -> getBaseGradient();
    gradient . setStart ( size () . width () / 2, 0 );
    gradient . setFinalStop ( size () . width () / 2, size () . height () );

    painter.setBrush ( gradient );

    painter.drawRect ( 0, 0, size () . width (), size () . height () );

    show ();

    QWidget::paintEvent(e);
}


