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

#include "../../sra-tools-gui/interfaces/ktoolbaritem.h"

#include <QVBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>

KToolbarItem :: KToolbarItem ( const QString &name, const QString &icon_name, QWidget *parent )
    : QWidget ( parent )
{
    QVBoxLayout *layout = new QVBoxLayout ();
    layout -> setMargin ( 10 );
    layout -> setSpacing ( 10 );

    QPixmap pxmp = QPixmap :: fromImage ( QImage ( icon_name ) );

    QLabel *label = new QLabel ( name );
    label -> setPixmap ( pxmp . scaled ( QSize ( 50, 50 ), Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
    label -> setScaledContents ( true );
    label -> resize ( 50, 50 );

    layout -> addWidget ( label );

    QLabel *txt = new QLabel ( name );
    txt -> setAlignment ( Qt::AlignHCenter );
    layout -> addWidget ( txt );

    setLayout ( layout );

}

KToolbarItem :: ~KToolbarItem ()
{
}
