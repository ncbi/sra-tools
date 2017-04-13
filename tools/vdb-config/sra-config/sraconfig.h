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

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QHBoxLayout;
class QVBoxLayout;
class QCheckBox;
class QFrame;
class QGroupBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

class vdbconf_model;
struct KNgcObj;
struct WorkspaceItem;

class SRAConfig : public QMainWindow
{
    Q_OBJECT

public:
    SRAConfig ( vdbconf_model &config_model, const QRect &avail_geometry, QWidget *parent = 0 );
    ~SRAConfig ();

signals:

    void dirty_config ();

private slots:

    void advanced_settings ();
    void commit_config ();
    void reload_config ();
    void default_config ();
    void modified_config ();

    void import_workspace ();

    void edit_import_path ();
    void edit_proxy_path ();
    void edit_public_path ();
    void edit_workspace_path ();

    void toggle_remote_enabled ( bool toggled );
    void toggle_local_caching ( bool toggled );
    void toggle_use_site ( bool toggled );
    void toggle_use_proxy ( bool toggled );
    void toggle_prioritize_http ( bool toggled );



private:


    void closeEvent ( QCloseEvent *event );
    void populate ();

    void add_workspace ( QString name, QString val, int ngc_id, bool insert = false );

    void setup_menubar ();
    void setup_toolbar ();

    vdbconf_model &model;

    QAction *discard_action;
    QAction *apply_action;

    QRect screen_geometry;

    QVBoxLayout *main_layout;
    QVBoxLayout *workspace_layout;

    QCheckBox *remote_enabled_cb;
    QCheckBox *local_caching_cb;
    QCheckBox *site_cb;
    QCheckBox *proxy_cb;
    QCheckBox *http_priority_cb;

    QLabel *proxy_label;
    QLabel *import_path_label;

    QFrame *adv_setting_window;

    QPushButton *apply_btn;
    QPushButton *discard_btn;

    QGroupBox* setup_option_group ();
    QGroupBox* setup_workspace_group ();
    QVBoxLayout *setup_button_layout();

    WorkspaceItem *public_workspace;
    QVector <WorkspaceItem *> protected_workspaces;

};

#endif // SRACONFIG_H
