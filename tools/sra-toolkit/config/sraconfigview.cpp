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

#include "sraconfigview.h"
#include "sraconfigmodel.h"
#include "../vdb-config/configure.h"
#include "../vdb-config/interactive.h"
#include "../vdb-config/vdb-config-model.hpp"
//#include "../sra-tools-gui/interfaces/ktoolbaritem.h"

#include <klib/rc.h>
#include <kfg/config.h>
#include <kfg/properties.h>
#include <kfg/repository.h>
#include <kfg/ngc.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <QAction>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolBar>

#include <QDebug>

extern "C"
{
    rc_t run_interactive ( vdbconf_model &m )
    {
        (void) m;
        return 0;
    }
}

struct WorkspaceItem
{
    WorkspaceItem ( QString name, QString path, uint32_t id )
        : name_label ( new QLabel ( name ) )
        , path_label ( new QLabel ( path ) )
        , edit_button ( new QPushButton ( "Edit" ) )
        , ngc_id ( id )
    {
        name_label -> setObjectName ( "workspace_name_label");
        name_label -> setFixedWidth ( 150 );
        name_label -> setAlignment ( Qt::AlignRight );

        path_label -> setObjectName ( "workspace_path_label");
        path_label -> setFrameShape ( QFrame::Panel );
        path_label -> setFrameShadow ( QFrame::Sunken );

        edit_button -> setObjectName ( "workspace_edit_button" );
        edit_button -> setFixedSize ( 30, 20 );
    }

    WorkspaceItem ()
        : name_label ( nullptr )
        , path_label ( nullptr )
        , edit_button ( nullptr )
        , ngc_id ( -1 )
    {}

    QLabel *name_label;
    QLabel *path_label;
    QPushButton *edit_button;

    int ngc_id;
};


/* static functions */
static
bool location_error ( RootState state, QWidget *w )
{
    QString msg;

    switch ( state )
    {
    case RootState_NotChanged       : return true;
    case RootState_NotUnique        : msg = QString ( "location not unique, select a different one" ); break;
    case RootState_MkdirFail        : msg = QString ( "could not created directory, maybe permisson problem" ); break;
    case RootState_NewPathEmpty     : msg = QString ( "you gave me an empty path" ); break;
    case RootState_NewDirNotEmpty   : msg = QString ( "the given location is not empty" ); break;
    case RootState_NewNotDir        : msg = QString ( "new location is not a directory" ); break;
    case RootState_Error            : msg = QString ( "error changing location" ); break;
    default                             : msg = QString ( "unknown enum" ); break;
    }

    QMessageBox::critical ( w
                            , "Error"
                            , msg
                            , QMessageBox::Ok );
    return false;
}

static
std :: string public_location_start_dir ( SRAConfigModel *model )
{
    std :: string s = model -> get_public_path ();

    if ( ! model -> path_exists ( s ) )
        s = model -> get_user_default_path ();

    if ( ! model -> path_exists ( s ) )
        s = model -> get_home_path () + "/ncbi";

    if ( ! model -> path_exists ( s ) )
        s = model -> get_home_path ();

    if ( ! model -> path_exists ( s ) )
        s = model -> get_current_path ();

    return s;
}

static
bool select_public_location ( SRAConfigModel *model, QWidget *w )
{
    std ::string path = public_location_start_dir ( model ) . c_str ();

    if ( model -> path_exists ( path ) )
    {
        QString p = QFileDialog :: getOpenFileName ( w
                                                , "Import Workspace"
                                                , path . c_str () );
        path = p . toStdString ();
    }
    else
    {
        QString p = QInputDialog::getText ( w
                                       , ""
                                       , "Location of public cache"
                                       , QLineEdit::Normal );
        path = p . toStdString ();
    }

    if ( path . length () > 0 )
    {
        QString p = path . c_str ();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question ( w
                                        , ""
                                        , "Change the location to '" + p + "'?"
                                        , QMessageBox::Yes | QMessageBox::No );
        if ( reply == QMessageBox::Yes )
        {
            bool flush_old = false;
            bool reuse_new = false;

            RootState state = model -> set_public_path ( path, flush_old, reuse_new );

            switch ( state )
            {
            case RootState_OK:
                return true;
            case RootState_OldNotEmpty:
            {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question ( w
                                                , "Directory not empty"
                                                , "Previous location is not empty, flush it?"
                                                , QMessageBox::Yes | QMessageBox::No );
                if ( reply == QMessageBox::Yes )
                {
                    flush_old = true;
                    state = model -> set_public_path ( path, flush_old, reuse_new );
                    if ( state == RootState_OK )
                        return true;
                    else
                        return location_error ( state, w );
        }
            }
            default:
                return location_error ( state, w );
            }
        }
    }

    return false;
}

static
std :: string protected_location_start_dir ( SRAConfigModel *model, uint32_t id )
{
    std :: string s = model -> get_workspace_path ( id );

    if ( ! model -> path_exists ( s ) )
        s = model -> get_user_default_path ();

    if ( ! model -> path_exists ( s ) )
        s = model -> get_home_path () + "/ncbi";

    if ( ! model -> path_exists ( s ) )
        s = model -> get_home_path ();

    if ( ! model -> path_exists ( s ) )
        s = model -> get_current_path ();

    return s;
}

static
bool select_protected_location ( SRAConfigModel *model, int id, QWidget *w )
{
    std :: string path = protected_location_start_dir ( model, id );
    qDebug () << "Protect location path: " << QString ( path . c_str () ) << " [" << id << "]";

    if ( model -> path_exists ( path ) )
    {
        QString p = QFileDialog :: getOpenFileName ( w
                                                , "Import Workspace"
                                                , path . c_str () );
        path = p . toStdString ();
    }
    else
    {
        QString p = QInputDialog::getText ( w
                                       , ""
                                       , "Location of dbGaP project"
                                       , QLineEdit::Normal );
        path = p . toStdString ();
    }

    if ( path . length () > 0 )
    {
        QString repo_name = model -> get_workspace_name ( id ) . c_str ();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question ( w
                                        , ""
                                        , "Change the location of '" + repo_name + "' to '" + QString ( path . c_str () ) + "'?"
                                        , QMessageBox::Yes | QMessageBox::No );
        if ( reply == QMessageBox::Yes )
        {
            bool flush_old = false;
            bool reuse_new = false;

            RootState state = model -> set_workspace_path ( path, id, flush_old, reuse_new );

            switch ( state )
            {
            case RootState_OK:
                return true;
            case RootState_OldNotEmpty:
            {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question ( w
                                                , "Directory not empty"
                                                , "Previous location is not empty, flush it?"
                                                , QMessageBox::Yes | QMessageBox::No );
                if ( reply == QMessageBox::Yes )
                {
                    flush_old = true;
                    state = model -> set_workspace_path ( path, id, flush_old, reuse_new );
                    if ( state == RootState_OK )
                        return true;
                    else
                        return location_error ( state, w );
                }
            }
            default:
                return location_error ( state, w );
            }
        }
    }

    return false;
}

static
bool make_ngc_obj ( const KNgcObj ** ngc, std::string &path )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc == 0 )
    {
        const KFile * src;
        rc = KDirectoryOpenFileRead ( dir, &src, "%s", path.c_str() );
        if ( rc == 0 )
        {
            rc = KNgcObjMakeFromFile ( ngc, src );
            KFileRelease( src );
        }
        KDirectoryRelease( dir );
    }

    return ( rc == 0 );
}

static
bool prepare_ngc ( SRAConfigModel *model, const KNgcObj *ngc, QString *loc, QWidget *w )
{
    std :: string base_path = model -> get_user_default_path ();
    std :: string path = model -> get_ngc_root ( base_path, ngc );

    RootState state = model -> configure_workspace ( path );

    switch ( state )
    {
    case RootState_OK:
    {
        *loc = path . c_str ();
        return true;
    }
    case RootState_OldNotEmpty:
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question ( w
                                        , "Directory not empty"
                                        , "Workspace location is not empty. Use it anyway?"
                                        , QMessageBox::Yes | QMessageBox::No );
        if ( reply == QMessageBox::Yes )
        {
            state = model -> configure_workspace ( path, true );
            if ( state == RootState_OK )
            {
                *loc = path . c_str ();
                return true;
            }
            else
                return location_error ( state, w );
        }
    }
    default:
        return location_error ( state, w );
    }

    return false;
}

static
bool import_ngc ( SRAConfigModel *model, std :: string file, uint32_t &ngc_id, QWidget *w )
{
   const KNgcObj *ngc;
   if ( ! make_ngc_obj ( &ngc, file ) )
   {
       QMessageBox::information ( w
                                  , "Import Error"
                                  , "Unable to import NGC file" );
   }
   else
   {
       QString path;
       if ( ! prepare_ngc ( model, ngc, &path, w ) )
           qDebug () << "failed to prepare ngc object";
       else
       {
           qDebug () << "prepared ngc object";

           uint32_t result_flags = 0;

           if ( model -> import_ngc ( path . toStdString (), ngc, INP_CREATE_REPOSITORY, &result_flags ) )
           {
               /* we have it imported or it exists and no changes made */
               bool modified = false;
               if ( result_flags & INP_CREATE_REPOSITORY )
               {
                   /* success is the most common outcome, the repository was created */
                   QMessageBox::information ( w
                                              , "Import Successful"
                                              , "project successfully imported" );

                   modified = true;
               }
               else
               {
                   /* repository did exist and is completely identical to the given ngc-obj */
                   QMessageBox::information ( w
                                              , ""
                                              , "this project exists already, no changes made" );

                   modified = false;
               }

               if ( model -> get_ngc_obj_id ( ngc, &ngc_id ) )
               {
                   qDebug () << "NGC ID: " << ngc_id;

                   QMessageBox::StandardButton reply;
                   reply = QMessageBox::question ( w
                                                   , ""
                                                   , "Do you want to change the location?"
                                                   , QMessageBox::Yes | QMessageBox::No );
                   if ( reply == QMessageBox::Yes )
                   {
                       modified |= select_protected_location ( model, ngc_id, w );
                   }
               }
               else
               {
                   QMessageBox::information ( w
                                              , ""
                                              , "Cannot find the imported Workspace" );
               }

               if ( modified )
               {
                   model -> commit (); // TBD - on import of NGC files, do we automaically commit, or allow for revert and require apply button?
                   model -> create_directory ( ngc );
                   return true;
               }
           }
           else if ( result_flags == 0 )
           {
               QMessageBox::critical ( w
                                       , "Error"
                                       , "Internal Error: Failed to impport the ngc-object"
                                       , QMessageBox::Ok );
           }
           else
           {
               QMessageBox::information ( w, "", "the repository does already exist!" );

               if ( result_flags & INP_UPDATE_ENC_KEY )
               {
                   QMessageBox::StandardButton reply;
                   reply = QMessageBox::question ( w
                                                   , ""
                                                   , "Encryption key would change, continue?"
                                                   , QMessageBox::Yes | QMessageBox::No );
                   if ( reply == QMessageBox::Yes  && ( result_flags & INP_UPDATE_DNLD_TICKET ) )
                   {
                       QMessageBox::StandardButton reply;
                       reply = QMessageBox::question ( w
                                                       , ""
                                                       , "Download ticket would change, continue?"
                                                       , QMessageBox::Yes | QMessageBox::No );
                       if ( reply == QMessageBox::Yes  && ( result_flags & INP_UPDATE_DESC ) )
                       {
                           QMessageBox::StandardButton reply;
                           reply = QMessageBox::question ( w
                                                           , ""
                                                           , "Description would change, continue?"
                                                           , QMessageBox::Yes | QMessageBox::No );
                           if ( reply == QMessageBox::Yes )
                           {
                               uint32_t result_flags2 = 0;
                               if ( model -> import_ngc ( path . toStdString () , ngc, result_flags, &result_flags2 ) )
                               {
                                   QMessageBox::StandardButton reply;
                                   reply = QMessageBox::question ( w
                                                                   , ""
                                                                   , "Project successfully updated!\nDo you want to change the location? "
                                                                   , QMessageBox::Yes | QMessageBox::No );

                                   if ( reply == QMessageBox::Yes )
                                   {
                                       /* we have to find out the id of the imported/existing repository */
                                       if ( model -> get_ngc_obj_id ( ngc, &ngc_id ) )
                                       {
                                           qDebug () << "NGC ID: " << ngc_id;
                                           select_protected_location ( model, ngc_id, w );
                                       }
                                       else
                                           QMessageBox::information ( w, "", "the repository does already exist!" );
                                   }

                                   model -> commit ();

                                   return true;
                               }
                               else
                               {
                                   QMessageBox::critical ( w
                                                           , "Error"
                                                           , "Internal Error: Failed to impport the ngc-object"
                                                           , QMessageBox::Ok );
                               }
                           }
                           else
                               QMessageBox::information ( w, "", "The import was canceled" );
                       }
                   }
               }
           }
       }

       KNgcObjRelease ( ngc );
   }

    return false;
}



/* Class methods */

SRAConfigView :: SRAConfigView ( QWidget *parent )
    : QWidget ( parent )
    , model ( new SRAConfigModel ( configure_model (), this ) )
    , main_layout ( new QVBoxLayout () )
{
    setObjectName ( "config_view" );
    connect ( this, SIGNAL ( dirty_config () ), this, SLOT ( modified_config () ) );

    main_layout -> setAlignment ( Qt::AlignTop );
    main_layout -> setMargin ( 0 );
    main_layout -> setSpacing ( 0 );

    main_layout -> addWidget ( setup_workflow_group () );
    main_layout -> addWidget ( setup_option_group () );

    setLayout ( main_layout );

    load_settings ();

    show ();
}

SRAConfigView :: ~SRAConfigView ()
{

}

void SRAConfigView :: load_settings ()
{
    if ( ! model -> remote_enabled () )
        bg_remote_access -> button ( 0 ) -> setChecked ( true );
     else
        bg_remote_access -> button ( 1 ) -> setChecked ( true );

    if ( ! model -> global_cache_enabled () )
        bg_local_caching -> button ( 0 ) -> setChecked ( true );
     else
        bg_local_caching -> button ( 1 ) -> setChecked ( true );

    if ( model -> site_workspace_exists () ) // TBD - Possible bug
    {
        if ( ! model -> site_enabled () )
            bg_use_site -> button ( 0 ) -> setChecked ( true );
        else
            bg_use_site -> button ( 1 ) -> setChecked ( true );
    }
    else
    {
        bg_use_site -> button ( 0 ) -> setDisabled ( true );
        bg_use_site -> button ( 1 ) -> setDisabled ( true );
    }

    if ( ! model -> proxy_enabled () )
    {
        bg_use_proxy -> button ( 0 ) -> setChecked ( true );
        proxyEditor -> setDisabled ( true );
    }
     else
    {
        bg_use_proxy -> button ( 1 ) -> setChecked ( true );
        proxyEditor -> setText ( QString ( model -> get_proxy_path () . c_str () ) );
    }

    if ( ! model -> allow_all_certs () )
        bg_allow_all_certs -> button ( 0 ) -> setChecked ( true );
     else
        bg_allow_all_certs -> button ( 1 ) -> setChecked ( true );
}

QWidget * SRAConfigView::setup_workflow_group ()
{

    QWidget *widget = new QWidget ();
    widget -> setObjectName ( "workflow_widget" );
    widget -> setFixedHeight ( 70 );

    QHBoxLayout *layout = new QHBoxLayout ();
    layout -> setAlignment ( Qt::AlignBottom | Qt::AlignRight );
    layout -> setSpacing ( 5 );

    apply_btn = new QPushButton ( "Apply" );
    apply_btn -> setDisabled ( true );
    connect ( apply_btn, SIGNAL ( clicked () ), this, SLOT ( commit_config  () ) );

    discard_btn = new QPushButton ( "Revert" );
    discard_btn -> setDisabled ( true );
    connect ( discard_btn, SIGNAL ( clicked () ), this, SLOT ( reload_config () ) );

    layout -> addWidget ( discard_btn );
    layout -> addWidget ( apply_btn );

    widget -> setLayout ( layout );

    return widget;
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

static
QWidget * make_editor_button_option_row ( QString name, QString desc, QLineEdit *editor,
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


    editor -> setFocusPolicy ( Qt::FocusPolicy::ClickFocus );
    editor -> setFixedHeight ( 40 );
    editor -> setAlignment ( Qt::AlignCenter );

    QHBoxLayout *row_h_layout = new QHBoxLayout ();
    row_h_layout -> setSpacing ( 10 );
    row_h_layout -> addWidget ( name_l );
    row_h_layout -> setAlignment ( name_l, Qt::AlignLeft );
    row_h_layout -> addWidget ( editor );
    row_h_layout -> setAlignment ( editor, Qt::AlignRight );
    row_h_layout -> addWidget ( b1 );
    row_h_layout -> addWidget ( b2 );

    QVBoxLayout *row_v_layout = new QVBoxLayout ();
    row_v_layout -> setSpacing ( 5 );
    row_v_layout -> addLayout ( row_h_layout );
    row_v_layout -> addWidget ( desc_l, 0, Qt::AlignLeft );

    row -> setLayout ( row_v_layout );

    return row;
}

void SRAConfigView::setup_general_settings ()
{
    QLabel *label = new QLabel ( "General Settings" );
    QPushButton *no = nullptr;
    QPushButton *yes = nullptr;
    QString name;
    QString desc;

    scrollWidgetLayout -> addWidget ( label );
    scrollWidgetLayout -> addSpacing ( 5 );

    // row 1
    name = QString ("Enable Remote Access");
    desc = QString ("Connect to NCBI over http or https. Connect to NCBI over http or https."
                            "Connect to NCBI over http or https.Connect to NCBI over http or https."
                            "Connect to NCBI over http or https.Connect to NCBI over http or https.");
    connect ( bg_remote_access = make_no_yes_button_group ( &no, &yes ),
              SIGNAL ( buttonClicked ( int ) ), this, SLOT ( toggle_remote_enabled ( int ) ) );

    scrollWidgetLayout -> addWidget ( make_button_option_row ( name, desc, no, yes ) );
    // row 1

    // row 2
    name = QString ("Enable Local File Caching");
    connect ( bg_local_caching = make_no_yes_button_group ( &no, &yes ),
              SIGNAL ( buttonClicked ( int ) ), this, SLOT ( toggle_local_caching ( int ) ) );

    scrollWidgetLayout -> addWidget ( make_button_option_row ( name, desc, no, yes ) );
    // row 2
}

void SRAConfigView::setup_network_setting ()
{
    QLabel *label = new QLabel ( "Network Settings" );
    QPushButton *no = nullptr;
    QPushButton *yes = nullptr;
    QString name;
    QString desc;

    scrollWidgetLayout -> addWidget ( label );
    scrollWidgetLayout -> addSpacing ( 5 );

    // row 1
    name = QString ("Use Site Installation") ;
    desc = QString ("Connect to NCBI over http or https. Connect to NCBI over http or https."
                            "Connect to NCBI over http or https.Connect to NCBI over http or https."
                            "Connect to NCBI over http or https.Connect to NCBI over http or https.");

    connect ( bg_use_site = make_no_yes_button_group ( &no, &yes ),
              SIGNAL ( buttonClicked ( int ) ), this, SLOT ( toggle_use_site ( int ) ) );

    scrollWidgetLayout -> addWidget ( make_button_option_row ( name, desc, no, yes ) );
    // row 1

    // row 2
    proxyEditor = new QLineEdit ();
    proxyEditor -> setPlaceholderText ( "xxx.xxx.xxx.xxx" );
    connect ( proxyEditor, SIGNAL ( editingFinished () ), this, SLOT ( edit_proxy_path () ) );

    name = QString ("Use Proxy");
    connect ( bg_use_proxy = make_no_yes_button_group ( &no, &yes ),
              SIGNAL ( buttonClicked ( int ) ), this, SLOT ( toggle_use_proxy ( int ) ) );

    scrollWidgetLayout -> addWidget ( make_editor_button_option_row ( name, desc, proxyEditor,
                                                          no, yes ) );

    // row 2

    //row 3
    connect ( bg_allow_all_certs = make_no_yes_button_group ( &no, &yes ),
              SIGNAL ( buttonClicked ( int ) ), this, SLOT ( toggle_allow_all_certs ( int ) ) );

    name = QString ( "Allow All Certificates" );

    scrollWidgetLayout -> addWidget ( make_button_option_row ( name, desc, no, yes) );
    // row 3
}

QWidget * SRAConfigView::setup_option_group ()
{
    scrollWidgetLayout = new QVBoxLayout ();
    scrollWidgetLayout -> setAlignment ( Qt::AlignTop );
    scrollWidgetLayout -> setSpacing ( 0 );

    setup_general_settings ();
    scrollWidgetLayout -> addSpacing ( 20 );
    setup_network_setting ();

    QWidget *scrollWidget = new QWidget ();
    scrollWidget -> setObjectName ("scroll_widget");
    scrollWidget -> setLayout ( scrollWidgetLayout );
    scrollWidget -> setSizePolicy ( QSizePolicy::Ignored, QSizePolicy::Ignored);

    QScrollArea *scrollArea = new QScrollArea ();
    scrollArea -> setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );
    scrollArea -> setWidget ( scrollWidget );
    scrollArea -> setWidgetResizable ( true );

    return scrollArea;
}

void SRAConfigView :: closeEvent ( QCloseEvent *ev )
{
#if ALLOW_SLOTS
    if ( model -> config_changed () )
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question ( this
                                        , ""
                                        , "Save changes? "
                                        , QMessageBox::No | QMessageBox::Yes );

        if ( reply == QMessageBox::Yes )
            commit_config ();
    }

    ev -> accept ();
#endif
}

void SRAConfigView :: add_workspace (QString name, QString val, int ngc_id, bool insert )
{
    QHBoxLayout *layout = new QHBoxLayout ();

    WorkspaceItem *ws = new WorkspaceItem ( name . append ( ':' ), val, ngc_id );

    if ( ngc_id == -1 )
    {
        public_workspace = ws;
        connect ( ws -> edit_button, SIGNAL ( clicked () ), this, SLOT ( edit_public_path () ) );
    }
    else
    {
        protected_workspaces . append ( ws );
        connect ( ws -> edit_button, SIGNAL ( clicked () ), this, SLOT ( edit_workspace_path () ) );
    }

    layout -> addWidget ( ws -> name_label );
    layout -> addWidget ( ws -> path_label );
    layout -> addWidget ( ws -> edit_button );

    if ( insert )
    {
        workspace_layout -> insertLayout ( workspace_layout -> count () - 1, layout );
    }
    else
        workspace_layout -> addLayout ( layout );
}

/*
void SRAConfigView :: import_workspace ()
{
    // open a file dialog to browse for the repository
    std :: string path = model -> get_home_path ();
    if ( ! model -> path_exists ( path ) )
        path = model -> get_current_path ();

    QString filter = tr ("NGS (*.ngc)" );
    QString file = QFileDialog :: getOpenFileName ( this
                                                    , "Import Workspace"
                                                    , path . c_str ()
                                                    , tr ( "NGC files (*.ngc)" ) );

    if ( ! file . isEmpty () )
    {
        std :: string s = file . toStdString ();
        uint32_t ngc_id;
        if ( import_ngc ( model, s, ngc_id, this ) )
        {
            QString name = model -> get_workspace_name ( ngc_id ) . c_str ();

            name = QInputDialog::getText ( this
                                                   , tr ( "Name Workspace" )
                                                   , tr ( "Choose a name for your workspace" )
                                                   , QLineEdit::Normal
                                                   , name );

            if ( name . isEmpty () )
                name = model -> get_workspace_name ( ngc_id ) . c_str ();

            add_workspace ( name, file, ngc_id, true );
        }
    }
}


QGroupBox * SRAConfigView :: setup_workspace_group ()
{
    QGroupBox *group = new QGroupBox ();
    group -> setObjectName ( "config_options_group" );

    workspace_layout = new QVBoxLayout ();
    workspace_layout -> setAlignment ( Qt :: AlignTop );
    workspace_layout -> setSpacing ( 15 );

    add_workspace ( "Public", model -> get_public_path () . c_str(), -1 );

    int ws_count = model -> workspace_count ();
    qDebug () << "Setup workspace group: repo-count: " << ws_count;
    for ( int i = 0; i < ws_count; ++ i )
    {
        std :: string name = model -> get_workspace_name ( i );
        add_workspace ( name . c_str () ,
                        model -> get_workspace_path ( i ) . c_str (),
                        model -> get_workspace_id ( name ) );
    }

    //3
    QHBoxLayout *i_layout = new QHBoxLayout ();

    i_layout -> addSpacing ( 125 );

    QPushButton *import = new QPushButton ( "+" );
    import -> setFixedSize ( 30, 25 );
    connect ( import, SIGNAL ( clicked () ), this, SLOT ( import_workspace () ) );
    i_layout -> addWidget ( import );

    i_layout -> addSpacing ( 5 );

    QLabel *label = new QLabel ();
    label -> setFrameShape ( QFrame::Panel );
    label -> setFrameShadow ( QFrame::Sunken );

    i_layout -> addWidget ( label );

    workspace_layout -> addLayout ( i_layout );

    //group -> setLayout ( workspace_layout );

    return group;
}
*/


/*
void SRAConfigView :: advanced_settings ()
{
    adv_setting_window = new QFrame ();
    adv_setting_window -> resize ( this -> width () * .7, this -> height () / 2 );
    adv_setting_window -> setWindowTitle ( "Advanced Settings" );

    QVBoxLayout *v_layout = new QVBoxLayout ();

    // 1
    QHBoxLayout *layout = new QHBoxLayout ();

    QLabel *label = new QLabel ( "Default import path:" );
    label -> setFixedWidth ( 150 );
    label -> setAlignment ( Qt::AlignRight );
    layout -> addWidget ( label);

    import_path_label = new QLabel ( model -> get_user_default_path () . c_str () );
    import_path_label -> setFrameShape ( QFrame::Panel );
    import_path_label -> setFrameShadow ( QFrame::Sunken );
    layout -> addWidget ( import_path_label );

    QPushButton *edit = new QPushButton ( "Edit" );
    connect ( edit, SIGNAL ( clicked () ), this, SLOT ( edit_import_path () ) );
    edit -> setFixedSize ( 30, 20 );
    layout -> addWidget ( edit );

    v_layout -> addLayout ( layout );
    v_layout -> addStretch ( 1 );

    // last
    layout = new QHBoxLayout ();
    layout -> setAlignment ( Qt::AlignBottom | Qt::AlignRight );

    QPushButton *done = new QPushButton ( "Done" );
    connect ( done, SIGNAL ( clicked () ), adv_setting_window, SLOT ( close () ) );

    layout -> addWidget ( done );
    v_layout -> addLayout ( layout );

    adv_setting_window -> setLayout ( v_layout );

    adv_setting_window -> show ();
}
*/

void SRAConfigView :: commit_config ()
{
    if ( ! model -> commit () )
        QMessageBox::information ( this, "", "Error saving changes" );

    apply_btn -> setDisabled ( true );
    discard_btn -> setDisabled ( true );
}

void SRAConfigView :: reload_config ()
{
    model -> reload ();
    load_settings ();

   if ( ! model -> config_changed () )
   {
       apply_btn -> setDisabled ( true );
       discard_btn -> setDisabled ( true );
   }
}

void SRAConfigView :: modified_config ()
{
    if ( model -> config_changed () ) // this wont trigger on workspace addition yet
    {
        apply_btn -> setDisabled ( false );
        discard_btn -> setDisabled ( false );
    }
}

void SRAConfigView :: default_config ()
{
    model -> set_remote_enabled ( true );
    model -> set_global_cache_enabled ( true );
    model -> set_site_enabled ( true );

    load_settings ();

    emit dirty_config ();
}

void SRAConfigView :: toggle_remote_enabled ( int toggled )
{
    if ( toggled == 1 )
    {
        qDebug () << "remote_enabled: yes";
        model -> set_remote_enabled ( true );
    }
    else
    {
        qDebug () << "remote_enabled: no";
        model -> set_remote_enabled ( false );
    }

    emit dirty_config ();
}

void SRAConfigView :: toggle_local_caching ( int toggled )
{
    if ( toggled == 1 )
    {
        qDebug () << "local_caching: yes";
        model -> set_global_cache_enabled ( true );
    }
    else
    {
        qDebug () << "local_caching: no";
        model -> set_global_cache_enabled ( false );
    }

    emit dirty_config ();
}

void SRAConfigView :: toggle_use_site ( int toggled )
{
    if ( toggled == 1 )
    {
        qDebug () << "use_site: yes";
        model -> set_site_enabled ( true );

    }
    else
    {
        qDebug () << "use_site: no";
        model -> set_site_enabled ( false );
    }

    emit dirty_config ();
}

void SRAConfigView :: toggle_use_proxy ( int toggled )
{
    if ( toggled == 1 )
    {
        qDebug () << "use_proxy: yes";
        model -> set_proxy_enabled ( true );
        proxyEditor -> setDisabled ( false );
    }
    else
    {
        qDebug () << "use_proxy: no";
        model -> set_proxy_enabled ( false );
        proxyEditor -> setDisabled ( true );
    }

    emit dirty_config ();
}

void SRAConfigView :: toggle_allow_all_certs ( int toggled )
{
    if ( toggled == 1 )
    {
        qDebug () << "set_allow_all_certs: yes";
        model -> set_allow_all_certs ( true );
    }
    else
    {
        qDebug () << "set_allow_all_certs: no";
        model -> set_allow_all_certs ( false );
    }

    emit dirty_config ();
}

void SRAConfigView :: edit_proxy_path ()
{
    QString text = proxyEditor -> text ();

    if ( text . isEmpty () )
        return;

    proxy_string = &text;

    if ( proxyEditor -> hasFocus () )
        proxyEditor -> clearFocus ();

    qDebug () << "set new proxy path: " << text;
    //model -> set_proxy_path ( proxy_string -> toStdString () );

    //emit dirty_config ();
}

/*
void SRAConfigView :: edit_import_path ()
{
    std :: string path = model -> get_user_default_path () . c_str ();

    if ( ! model -> path_exists ( path ) )
        path = model -> get_home_path ();

    if ( ! model -> path_exists ( path ) )
        path = model -> get_current_path ();

    QString e_path = QFileDialog :: getOpenFileName ( adv_setting_window
                                                    , ""
                                                    , path . c_str () );


    if ( e_path . isEmpty () )
        return;

    import_path_label -> setText ( e_path );
    model -> set_user_default_path ( e_path . toStdString () . c_str () );

    emit dirty_config ();
}


void SRAConfigView :: edit_public_path ()
{
    qDebug () << public_workspace -> ngc_id;
    if ( select_public_location ( model, this ) )
    {
        public_workspace -> path_label -> setText ( model -> get_public_path () . c_str () );

        emit dirty_config ();
    }
}

void SRAConfigView :: edit_workspace_path ()
{
    foreach ( WorkspaceItem *item,  protected_workspaces )
    {
        if ( sender () == item -> edit_button )
        {
            qDebug () << item -> ngc_id;

            if ( select_protected_location ( model, item -> ngc_id, this ) )
            {
                QString path =  model -> get_workspace_path ( item -> ngc_id ) . c_str ();
                item -> path_label -> setText ( path );
                import_path_label -> setText ( path );

                emit dirty_config ();
            }

            return;
        }
    }
}

*/
