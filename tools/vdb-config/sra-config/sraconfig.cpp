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

#include "sraconfig.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#include <QDebug>

SRAConfig :: SRAConfig ( const QRect &avail_geometry, QWidget *parent )
    : QMainWindow ( parent )
    , screen_geometry ( avail_geometry )
    , main_layout ( new QVBoxLayout () )
{
    main_layout -> setSpacing ( 20 );
    main_layout -> setAlignment ( Qt::AlignTop );
    main_layout -> addWidget ( setupOptionGroup () );
    main_layout -> addWidget ( setupRepositoriesGroup () );

    setupButtonLayout ();

    QWidget *main_widget = new QWidget ();
    main_widget -> setLayout ( main_layout );

    setCentralWidget ( main_widget );

    move ( ( screen_geometry . width () - ( this -> width () / 2 ) ) / 2,
           ( screen_geometry . height () - this -> height () ) / 2 );
}

SRAConfig :: ~SRAConfig ()
{

}

QGroupBox * SRAConfig::setupOptionGroup ()
{
    QGroupBox *group = new QGroupBox ( "Options" );
    group -> setFixedHeight ( 170 );

    QGridLayout *layout = new QGridLayout ();
    layout -> setAlignment ( Qt :: AlignTop );
    layout -> setSpacing ( 15 );

    //1
    QCheckBox *c_box = new QCheckBox ( "Enable Remote Access" );
    c_box -> setAutoExclusive ( false );
    c_box -> setChecked ( true );
    layout -> addWidget ( c_box, 0, 0 );

    //2
    c_box = new QCheckBox ( "Enable Local File Caching" );
    c_box -> setAutoExclusive ( false );
    layout -> addWidget ( c_box, 1, 0 );

    //3
    c_box = new QCheckBox ( "Use Site Installation" );
    c_box -> setAutoExclusive ( false );
    layout -> addWidget ( c_box, 2, 0 );

    //4
    c_box = new QCheckBox ( "Use Proxy" );
    c_box -> setAutoExclusive ( false );

    proxy_path = new QLabel ( "proxy.ncbi.nlm.nih.gov:XXXX" );

    QPalette p = proxy_path -> palette ();
    p . setColor ( QPalette::Background, Qt::white );

    proxy_path -> setPalette ( p );
    proxy_path -> setAutoFillBackground ( true );
    proxy_path -> setTextInteractionFlags ( Qt::TextEditorInteraction );
    proxy_path -> setFixedHeight ( 20 );

    layout -> addWidget ( c_box, 3, 0 );
    layout -> addWidget ( proxy_path, 3, 1, 1, 2 );

    //5
    c_box = new QCheckBox ( "Prioritize Environment Variable 'http-proxy'" );
    c_box -> setAutoExclusive ( false );
    layout -> addWidget ( c_box, 4, 0, 1, 2 );

    group -> setLayout ( layout );

    return group;
}

QGroupBox * SRAConfig :: setupRepositoriesGroup ()
{
    QGroupBox *group = new QGroupBox ( "Repositories" );

    QGridLayout *layout = new QGridLayout ();
    layout -> setAlignment ( Qt :: AlignTop );
    layout -> setSpacing ( 15 );

    //1
    QLabel *label = new QLabel ( "Import Path" );
    layout -> addWidget ( label, 0, 0 );

    import_path = new QLabel ( "/home/rodarme1/ncbi" );
    layout -> addWidget ( import_path, 0, 1, 1, 3 );

    //2
    label = new QLabel ( "Public" );
    layout -> addWidget ( label, 1, 0 );

    public_path = new QLabel ( "/home/rodarme1/ncbi/public" );

    QPalette p = public_path -> palette ();
    p . setColor ( QPalette::Background, Qt::white );

    public_path -> setPalette ( p );
    public_path -> setAutoFillBackground ( true );
    public_path -> setTextInteractionFlags ( Qt::TextEditorInteraction );
    public_path -> setFixedHeight ( 20 );

    layout -> addWidget ( public_path, 1, 1, 1, 3 );

    //3
    QPushButton *import = new QPushButton ( "Import" );
    layout -> addWidget ( import, 2, 0 );

    group -> setLayout ( layout );

    return group;
}

void SRAConfig :: setupButtonLayout ()
{
    QHBoxLayout *layout = new QHBoxLayout ();
    layout -> setAlignment ( Qt::AlignBottom | Qt::AlignRight );
    layout -> setSpacing ( 5 );

    apply = new QPushButton ( "Apply" );
    cancel = new QPushButton ( "Cancel" );
    ok = new QPushButton ( "OK" );

    layout -> addWidget ( apply );
    layout -> addWidget ( cancel );
    layout -> addWidget ( ok );

    main_layout -> addLayout ( layout );
}
