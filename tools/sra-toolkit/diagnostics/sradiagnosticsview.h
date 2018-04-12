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

#ifndef DIAGNOSTICSVIEW_H
#define DIAGNOSTICSVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

struct KConfig;
struct KDiagnose;
struct KDiagnoseTest;
class DiagnosticsTest;
class DiagnosticsThread;


class SRADiagnosticsView : public QWidget
{
    Q_OBJECT
public:
    explicit SRADiagnosticsView( KConfig *p_config, QWidget *parent = 0);
    ~SRADiagnosticsView ();

signals:

public slots:
    void handle_callback ( DiagnosticsTest *test );

private slots:
    void start_diagnostics ();
    void cancel_diagnostics ();
    void worker_finished ();

private:

    void init ();
    void init_metadata_view ();
    void init_tree_view ();
    void init_controls ();

    // Visual
    QVBoxLayout *self_layout;

    QTreeWidget *tree_view;
    QTreeWidgetItem *currentTest;

    QPushButton *start_button;
    QPushButton *cancel_button;
    QProgressBar *p_bar;
    int progress_val;

    // Data
    const KConfig *config;
    QString homeDir;

    // Event

    QThread *thread;
    DiagnosticsThread *worker;
};

#endif // DIAGNOSTICSVIEW_H
