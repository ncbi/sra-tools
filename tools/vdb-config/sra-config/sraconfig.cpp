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
#include "../vdb-config-model.hpp"
#include "../sra-tools-gui/interfaces/ktoolbaritem.h"

#include <klib/rc.h>
#include <kfg/config.h>
#include <kfg/properties.h>
#include <kfg/repository.h>
#include <kfg/ngc.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolBar>

#include <QDebug>

const QString rsrc_path = ":/images";

/* static functions */
static
bool location_error ( ESetRootState state, QWidget *w )
{
    QString msg;

    switch ( state )
    {
    case eSetRootState_NotChanged       : return true;
    case eSetRootState_NotUnique        : msg = QString ( "location not unique, select a different one" ); break;
    case eSetRootState_MkdirFail        : msg = QString ( "could not created directory, maybe permisson problem" ); break;
    case eSetRootState_NewPathEmpty     : msg = QString ( "you gave me an empty path" ); break;
    case eSetRootState_NewDirNotEmpty   : msg = QString ( "the given location is not empty" ); break;
    case eSetRootState_NewNotDir        : msg = QString ( "new location is not a directory" ); break;
    case eSetRootState_Error            : msg = QString ( "error changing location" ); break;
    default                             : msg = QString ( "unknown enum" ); break;
    }

    QMessageBox::critical ( w
                            , "Error"
                            , msg
                            , QMessageBox::Ok );
    return false;
}

static
std :: string protected_location_start_dir ( vdbconf_model &model, uint32_t id )
{
    std :: string s = model . get_repo_location ( id );

    if ( ! model . does_path_exist ( s ) )
        s = model . get_user_default_dir ();

    if ( ! model.does_path_exist( s ) )
        s = model.get_home_dir () + "/ncbi";

    if ( ! model.does_path_exist( s ) )
        s = model.get_home_dir ();

    if ( ! model.does_path_exist( s ) )
        s = model.get_current_dir ();

    return s;
}

static
bool select_protected_location ( vdbconf_model &model, uint32_t id, QWidget *w )
{
    QString path = protected_location_start_dir ( model, id ) . c_str ();

    if ( model . does_path_exist ( path . toStdString () ) )
    {
        path = QFileDialog :: getOpenFileName ( w
                                                , "Import Workspace"
                                                , path );
    }
    else
    {
        path = QInputDialog::getText ( w
                                       , ""
                                       , "Location of dbGaP project"
                                       , QLineEdit::Normal );
    }

    if ( path . length () > 0 )
    {
        QString repo_name = model . get_repo_name ( id ) . c_str ();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question ( w
                                        , ""
                                        , "Change the location of '" + repo_name + "' to '" + path + "'?"
                                        , QMessageBox::Yes | QMessageBox::No );
        if ( reply == QMessageBox::Yes )
        {
            bool flush_old = false;
            bool reuse_new = false;

            ESetRootState state = model . set_repo_location ( id, flush_old, path . toStdString (), reuse_new );

            switch ( state )
            {
            case eSetRootState_OK:
                return true;
            case eSetRootState_OldNotEmpty:
            {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question ( w
                                                , "Directory not empty"
                                                , "Previous location is not empty, flush it?"
                                                , QMessageBox::Yes | QMessageBox::No );
                if ( reply == QMessageBox::Yes )
                {
                    flush_old = true;
                    state = model . set_repo_location ( id, flush_old, path . toStdString (), reuse_new );
                    if ( state == eSetRootState_OK )
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
        qDebug () << "got native dir";
        qDebug () << "opening: " << QString ( path . c_str () );
        const KFile * src;
        rc = KDirectoryOpenFileRead ( dir, &src, "%s", path.c_str() );
        if ( rc == 0 )
        {
            qDebug () << "opened file for read";
            rc = KNgcObjMakeFromFile ( ngc, src ); // wont make it past here until I have a real ngs file to work with.
            KFileRelease( src );
        }
        KDirectoryRelease( dir );
    }

    return ( rc == 0 );
}

static
bool prepare_ngc ( vdbconf_model &model, const KNgcObj *ngc, QString *loc, QWidget *w )
{
    std :: string location_base = model . get_user_default_dir ();
    std :: string location = model . get_ngc_root ( location_base, ngc );

    ESetRootState state = model . prepare_repo_directory ( location );

    switch ( state )
    {
    case eSetRootState_OK:
    {
        *loc = location . c_str ();
        return true;
    }
    case eSetRootState_OldNotEmpty:
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question ( w
                                        , "Directory not empty"
                                        , "Workspace location is not empty. Use it anyway?"
                                        , QMessageBox::Yes | QMessageBox::No );
        if ( reply == QMessageBox::Yes )
        {
            state = model . prepare_repo_directory ( location, true );
            if ( state == eSetRootState_OK )
            {
                *loc = location . c_str ();
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
bool import_ngc ( vdbconf_model &model, std :: string file, QWidget *w )
{
   const KNgcObj *ngc;
   if ( make_ngc_obj ( &ngc, file ) )
   {
       qDebug () << "made ngc object";

       QString location;
       if ( prepare_ngc ( model, ngc, &location, w ) )
       {
           qDebug () << "prepared ngc object";

           uint32_t result_flags = 0;

           if ( model . import_ngc ( location . toStdString (), ngc, INP_CREATE_REPOSITORY, &result_flags ) )
           {
               /* we have it imported or it exists and no changes made */
               bool modified = false;
               if ( result_flags & INP_CREATE_REPOSITORY )
               {
                   /* success is the most common outcome, the repository was created */
                   QMessageBox::StandardButton reply;
                   reply = QMessageBox::information ( w
                                                      , "Import Successful"
                                                      , "project successfully imported" );
                   if ( reply == QMessageBox::Ok )
                       modified = true;
               }
               else
               {
                   /* repository did exist and is completely identical to the given ngc-obj */
                   QMessageBox::StandardButton reply;
                   reply = QMessageBox::information ( w
                                                      , ""
                                                      , "this project exists already, no changes made" );
               }

               QMessageBox::StandardButton reply;
               reply = QMessageBox::question ( w
                                               , ""
                                               , "Do you want to change the location?"
                                               , QMessageBox::Yes | QMessageBox::No );
               if ( reply == QMessageBox::Yes )
               {
                   uint32_t id;
                   if ( model . get_id_of_ngc_obj ( ngc, &id ) )
                       modified |= select_protected_location ( model, id, w );
                   else
                   {
                       QMessageBox::StandardButton reply;
                       reply = QMessageBox::information ( w
                                                          , ""
                                                          , "Cannot find the imported Workspace" );
                   }
               }

               if ( modified )
               {
                   model . commit ();
                   model . mkdir ( ngc );
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
                               if ( model . import_ngc ( location . toStdString () , ngc, result_flags, &result_flags2 ) )
                               {
                                   QMessageBox::StandardButton reply;
                                   reply = QMessageBox::question ( w
                                                                   , ""
                                                                   , "Project successfully updated!\nDo you want to change the location? "
                                                                   , QMessageBox::Yes | QMessageBox::No );

                                   if ( reply == QMessageBox::Yes )
                                   {
                                       uint32_t id; /* we have to find out the id of the imported/existing repository */
                                       if ( model . get_id_of_ngc_obj ( ngc, &id ) )
                                           select_protected_location ( model, id, w );
                                       else
                                           QMessageBox::information ( w, "", "the repository does already exist!" );
                                   }

                                   model . commit ();

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
       qDebug () << "failed to prepare ngc object";
   }
   qDebug () << "failed to make ngc object";

    return false;
}



/* Class methods */

SRAConfig :: SRAConfig ( vdbconf_model &config_model, const QRect &avail_geometry, QWidget *parent )
    : QMainWindow ( parent )
    , model ( config_model )
    , screen_geometry ( avail_geometry )
    , main_layout ( new QVBoxLayout () )
{
    setup_toolbar ();

    main_layout -> setSpacing ( 20 );
    main_layout -> setAlignment ( Qt::AlignTop );
    main_layout -> addSpacing ( 10 );
    main_layout -> addWidget ( setup_option_group () );
    main_layout -> addWidget ( setup_workspace_group () );
    main_layout -> addStretch ( 1 );
    main_layout -> addLayout ( setup_button_layout () );

    populate ();

    connect ( this, SIGNAL ( dirty_config () ), this, SLOT ( modified_config () ) );

    QWidget *main_widget = new QWidget ();
    main_widget -> setLayout ( main_layout );

    setCentralWidget ( main_widget );

    resize ( ( screen_geometry . width () ) / 3, this -> height () );

    move ( ( screen_geometry . width () - this -> width () ) / 2,
           ( screen_geometry . height () - this -> height () ) / 2 );
}

SRAConfig :: ~SRAConfig ()
{

}

void SRAConfig :: setup_toolbar ()
{
#ifdef Q_OS_OSX
    setUnifiedTitleAndToolBarOnMac ( true );
#endif

    QToolBar *bar = new QToolBar ( this );

    KToolbarItem *item = new KToolbarItem ( "General", rsrc_path + "/general_icon" );
    bar -> addWidget ( item );

    item = new KToolbarItem ( "AWS", rsrc_path + "/aws_icon" );
    bar -> addWidget ( item );

    item = new KToolbarItem ( "Network", rsrc_path + "/network_icon" );
    bar -> addWidget ( item );

    addToolBar ( bar );

}

void SRAConfig :: populate ()
{
    remote_enabled_cb -> setChecked ( model . is_remote_enabled () );

    local_caching_cb -> setChecked ( model . is_global_cache_enabled () );

    if ( model . does_site_repo_exist () )
        site_cb -> setChecked ( model . is_site_enabled () );
    else
        site_cb -> setDisabled ( true );

    proxy_cb -> setChecked ( model . is_http_proxy_enabled () );
    proxy_label -> setText ( model . get_http_proxy_path () . c_str () );

    http_priority_cb -> setChecked ( model . has_http_proxy_env_higher_priority () );
}

QGroupBox * SRAConfig::setup_option_group ()
{
    QGroupBox *group = new QGroupBox ( "Options" );
    group -> setFixedHeight ( 170 );

    QGridLayout *layout = new QGridLayout ();
    layout -> setAlignment ( Qt :: AlignTop );
    layout -> setSpacing ( 15 );

    //1
    remote_enabled_cb = new QCheckBox ( "Enable Remote Access" );
    remote_enabled_cb -> setAutoExclusive ( false );
    layout -> addWidget ( remote_enabled_cb, 0, 0 );
    connect ( remote_enabled_cb, SIGNAL ( clicked ( bool ) ), this, SLOT ( toggle_remote_enabled ( bool ) ) );

    //2
    local_caching_cb = new QCheckBox ( "Enable Local File Caching" );
    local_caching_cb -> setAutoExclusive ( false );
    layout -> addWidget ( local_caching_cb, 1, 0 );
     connect ( local_caching_cb, SIGNAL ( clicked ( bool ) ), this, SLOT ( toggle_local_caching ( bool ) ) );

    //3
    site_cb = new QCheckBox ( "Use Site Installation" );
    site_cb -> setAutoExclusive ( false );
    layout -> addWidget ( site_cb, 2, 0 );
    connect ( site_cb, SIGNAL ( clicked ( bool ) ), this, SLOT ( toggle_use_site ( bool ) ) );

    //4
    proxy_cb = new QCheckBox ( "Use Proxy" );
    proxy_cb -> setAutoExclusive ( false );
    layout -> addWidget ( proxy_cb, 3, 0 );
    connect ( proxy_cb, SIGNAL ( clicked ( bool ) ), this, SLOT ( toggle_use_proxy ( bool ) ) );

    proxy_label = new QLabel ();
    proxy_label -> setMargin ( 0 );
    proxy_label -> setFrameShape ( QFrame::Panel );
    proxy_label -> setFrameShadow ( QFrame::Sunken );
    proxy_label -> setFixedHeight ( 20 );
    layout -> addWidget ( proxy_label, 3, 1 );

    QPushButton *edit = new QPushButton ( "Edit" );
    edit -> setFixedSize ( 30, 20 );
    layout -> addWidget ( edit, 3, 2 );
    connect ( edit, SIGNAL ( clicked () ), this, SLOT ( edit_proxy_path () ) );

    //5
    http_priority_cb = new QCheckBox ( "Prioritize Environment Variable 'http-proxy'" );
    http_priority_cb -> setAutoExclusive ( false );
    layout -> addWidget ( http_priority_cb, 4, 0 );
    connect ( http_priority_cb, SIGNAL ( clicked ( bool ) ), this, SLOT ( toggle_prioritize_http ( bool ) ) );

    group -> setLayout ( layout );

    return group;
}


void SRAConfig :: add_workspace ( QString name, QString val, bool insert )
{
    QHBoxLayout *layout = new QHBoxLayout ();

qDebug () << name;
    QLabel *label = new QLabel ( name . append ( ':' ) );
    label -> setFixedWidth ( 150 );
    label -> setAlignment ( Qt::AlignRight );
    layout -> addWidget ( label);

qDebug () << val;
    QLabel *path = new QLabel ( val );
    path -> setFrameShape ( QFrame::Panel );
    path -> setFrameShadow ( QFrame::Sunken );
    layout -> addWidget ( path );

    QPushButton *edit = new QPushButton ( "Edit" );
    edit -> setFixedSize ( 30, 20 );
    layout -> addWidget ( edit );

    if ( insert )
    {
        qDebug () << "inserting";
        workspace_layout -> insertLayout ( workspace_layout -> count () - 1, layout );
    }
    else
        workspace_layout -> addLayout ( layout );
}


void SRAConfig :: import_workspace ()
{
    // open a file dialog to browse for the repository
    QString dir = model . get_home_dir () . c_str ();
    if ( ! model . does_path_exist ( dir . toStdString () ) )
        dir = model . get_current_dir () . c_str ();

    QString filter = tr ("NGS (*.ngc)" );
    QString file = QFileDialog :: getOpenFileName ( this
                                                    , "Import Workspace"
                                                    , dir
                                                    , tr ("NGC files (*.ngc)" ) );


    if ( ! file . isEmpty () )
    {
        QStringList list = file . split ( '/' );
        QString ext = list . last ();

        QString input = QInputDialog::getText ( this
                                                , tr ( "Name Workspace" )
                                                , tr ( "Choose a name for your workspace" )
                                                , QLineEdit::Normal
                                                , ext.split ( '.' ) . first () );
        qDebug () << "got input";
        if ( input . isEmpty () )
           return;

#if 0
        std :: string s = file . toStdString ();
        if ( import_ngc ( model, s ) )
        {
            add_workspace ( input , file, true );
            emit dirty_config ();
        }
#endif
        add_workspace ( input , file, true );
        emit dirty_config ();
    }
}


QGroupBox * SRAConfig :: setup_workspace_group ()
{
    QGroupBox *group = new QGroupBox ( "Workspaces: " );
    group -> setTitle ( group -> title () . append ( model . get_user_default_dir () . c_str ( ) ) );

    workspace_layout = new QVBoxLayout ();
    workspace_layout -> setAlignment ( Qt :: AlignTop );
    workspace_layout -> setSpacing ( 15 );

    add_workspace ( "Public", model . get_public_location () . c_str() );

    int repo_count = model . get_repo_count ();
    for ( int i = 0; i < repo_count; ++ i )
    {
        add_workspace ( model . get_repo_name ( i ) . c_str (),
                         model . get_repo_location ( i ) . c_str () );
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

    group -> setLayout ( workspace_layout );

    return group;
}

QHBoxLayout * SRAConfig::setup_button_layout ()
{
    QHBoxLayout *layout = new QHBoxLayout ();
    layout -> setAlignment ( Qt::AlignBottom | Qt::AlignRight );
    layout -> setSpacing ( 5 );

    apply = new QPushButton ( "Apply" );
    apply -> setDisabled ( true );
    connect ( apply, SIGNAL ( clicked () ), this, SLOT ( commit_config  () ) );

    revert = new QPushButton ( "Revert" );
    revert -> setDisabled ( true );
    connect ( revert, SIGNAL ( clicked () ), this, SLOT ( reload_config () ) );

    layout -> addWidget ( revert );
    layout -> addWidget ( apply );
    //layout -> addWidget ( ok );

    return layout;
}

void SRAConfig :: commit_config ()
{
    model . commit ();

    apply -> setDisabled ( true );
    revert -> setDisabled ( true );
}

void SRAConfig :: reload_config ()
{
    model . reload ();
    populate ();

   if ( ! model . get_config_changed () )
   {
       apply -> setDisabled ( true );
       revert -> setDisabled ( true );
   }
}

void SRAConfig :: modified_config ()
{
    if ( model . get_config_changed () ) // this wont trigger on workspace addition yet
    {
        apply -> setDisabled ( false );
        revert -> setDisabled ( false );
    }
}

// TBD - still needs a menu item to be triggered.
void SRAConfig :: default_config ()
{
    model . set_remote_enabled ( true );
    model . set_global_cache_enabled ( true );
    model . set_site_enabled ( true );
}

void SRAConfig :: toggle_remote_enabled ( bool toggled )
{
    model . set_remote_enabled ( toggled );
    emit dirty_config ();
}

void SRAConfig :: toggle_local_caching ( bool toggled )
{
    model . set_global_cache_enabled ( toggled );
    emit dirty_config ();
}

void SRAConfig :: toggle_use_site ( bool toggled )
{
    model . set_site_enabled ( toggled );
    emit dirty_config ();
}

void SRAConfig :: toggle_use_proxy ( bool toggled )
{
    model . set_http_proxy_enabled ( toggled );
    emit dirty_config ();
}

void SRAConfig :: toggle_prioritize_http ( bool toggled )
{
    model . set_http_proxy_env_higher_priority ( toggled );
    emit dirty_config ();
}

void SRAConfig :: edit_proxy_path ()
{
    QString input = QInputDialog::getText ( this
                                                , tr ( "Proxy Path" )
                                                , tr ( "Enter a proxy path" )
                                                , QLineEdit::Normal
                                                , proxy_label -> text () );

    if ( input . isEmpty () )
        return;

    proxy_label -> setText ( input );
    model . set_http_proxy_path ( input . toStdString () );

    emit dirty_config ();
}


