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

#ifndef SRATOOLBAR_H
#define SRATOOLBAR_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QPushButton;
QT_END_NAMESPACE

class SRAToolBar : public QWidget
{
    Q_OBJECT
public:

    enum SRATool
    {
        t_home,
        t_diagnostics,
    };

    explicit SRAToolBar(QWidget *parent = nullptr);

signals:
    void expanded ( bool );
    void toolSwitched ( int );

public slots:
    void expand ();
    void switchTool ( int tool );

private:

    void init ();
    void paintEvent ( QPaintEvent * );
    void updateButtonIcons ();

    bool isExpanded;

    QPushButton *home_button;
    QPushButton *config_button;
    QPushButton *diagnostics_button;
    QPushButton *expand_button;

};

#endif // SRATOOLBAR_H
