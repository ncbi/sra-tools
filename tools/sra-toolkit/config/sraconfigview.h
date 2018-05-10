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

#ifndef SRACONFIG_H
#define SRACONFIG_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QHBoxLayout;
class QVBoxLayout;
class QButtonGroup;
class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

class SRAConfigModel;
class vdbconf_model;
struct KNgcObj;
struct WorkspaceItem;

class SRAConfigView : public QWidget
{
    Q_OBJECT

public:

    SRAConfigView ( QWidget *parent = 0 );
    ~SRAConfigView ();

signals:

    void dirty_config ();

private slots:

   // void advanced_settings ();
    void commit_config ();
    void reload_config ();
    void default_config ();
    void modified_config ();


    //void import_workspace ();

    //void edit_import_path ();
    void edit_proxy_path ();
    //void edit_public_path ();
    //void edit_workspace_path ();

    void toggle_remote_enabled ( int toggled );
    void toggle_local_caching ( int toggled );
    void toggle_use_site ( int toggled );
    void toggle_use_proxy ( int toggled );
    void toggle_allow_all_certs ( int toggled );

private:

    void closeEvent ( QCloseEvent *event );
    void load_settings ();

    void add_workspace ( QString name, QString val, int ngc_id, bool insert = false );

    SRAConfigModel *model;

    QAction *discard_action;
    QAction *apply_action;

    QVBoxLayout *main_layout;
    QVBoxLayout *scrollWidgetLayout;
    QVBoxLayout *workspace_layout;

    QButtonGroup *bg_remote_access;
    QButtonGroup *bg_local_caching;
    QButtonGroup *bg_use_site;
    QButtonGroup *bg_use_proxy;
    QButtonGroup *bg_allow_all_certs;

    QLineEdit *proxyEditor;

    QString *proxy_string;
    QLabel *import_path_label;

    QFrame *adv_setting_window;

    QPushButton *apply_btn;
    QPushButton *discard_btn;

    QWidget* setup_workflow_group ();
    QWidget* setup_option_group ();
    void setup_general_settings ();
    void setup_network_setting ();

    WorkspaceItem *public_workspace;
    QVector <WorkspaceItem *> protected_workspaces;

};

#endif // SRACONFIG_H
