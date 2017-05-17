#include "sraconfigmodel.h"
#include "../vdb-config-model.hpp"

#include <kfg/config.h>

#include <QMessageBox>
#include <string>


bool SRAConfigModel :: config_changed ()
{
    return model . get_config_changed ();
}

bool SRAConfigModel :: proxy_enabled ()
{
    return model . is_http_proxy_enabled ();
}

bool SRAConfigModel :: proxy_priority ()
{
    return model . has_http_proxy_env_higher_priority ();
}

bool SRAConfigModel :: remote_enabled ()
{
    return model . is_remote_enabled ();
}

bool SRAConfigModel :: site_enabled ()
{
    return model . is_site_enabled ();
}

bool SRAConfigModel :: import_ngc ( const std :: string path,  KNgcObj *ngc, uint32_t permissions, uint32_t *rslt_flags )
{
    return model . import_ngc ( path, ngc, permissions, rslt_flags );
}

bool SRAConfigModel :: path_exists ( const std :: string &path )
{
    return model . does_path_exist ( path );
}

bool SRAConfigModel :: site_workspace_exists ( const std :: string &path )
{
    return model . does_repo_exist ( path . c_str () );
}

void SRAConfigModel :: commit ()
{
    model . commit ();
}

void SRAConfigModel :: reload ()
{
    model . reload ();
}

void SRAConfigModel :: create_directory ( const KNgcObj *ngc )
{
    model . mkdir ( ngc );
}

void SRAConfigModel :: set_cache_enabled ( bool enabled )
{
    model . set_global_cache_enabled ( enabled );
}

void SRAConfigModel :: set_proxy_enabled ( bool enabled )
{
    model . set_http_proxy_enabled ( enabled );
}

void SRAConfigModel :: set_remote_enabled ( bool enabled )
{
    model . set_remote_enabled ( enabled );
}

void SRAConfigModel :: set_site_enabled ( bool enabled )
{
    model . set_site_enabled ( enabled );
}

void SRAConfigModel :: set_proxy_priority ( bool priority )
{
    model . set_http_proxy_env_higher_priority ( priority );
}

void SRAConfigModel :: set_public_path ( const std :: string &path, bool flushOld, bool reuseNew )
{
    model . set_public_location ( flushOld, path, reuseNew );
}

void SRAConfigModel :: set_workspace_path ( const std :: string & path, uint32_t id, bool flushOld, bool reuseNew )
{
    model . set_repo_location ( id, flushOld, path, reuseNew );
}

bool SRAConfigModel :: get_ngc_obj_id ( const KNgcObj *ngc, uint32_t *id )
{
    return model . get_id_of_ngc_obj ( ngc, id );
}

int32_t SRAConfigModel :: get_workspace_id ( const std :: string &name )
{
    return model . get_repo_id ( name );
}

std :: string SRAConfigModel :: get_current_path ()
{
    return model . get_current_dir ();
}

std :: string SRAConfigModel :: get_home_path ()
{
    return model . get_home_dir ();
}

std :: string SRAConfigModel :: get_proxy_path ()
{
    return model . get_http_proxy_path ();
}

std :: string SRAConfigModel :: get_public_path ()
{
    return model . get_public_location ();
}

std :: string SRAConfigModel :: get_user_default_path ()
{
    return model . get_user_default_dir ();
}

std :: string SRAConfigModel :: get_workspace_name ( uint32_t id )
{
    return model . get_repo_name ( id );
}

std :: string SRAConfigModel :: get_workspace_path ( uint32_t id )
{
    return model . get_repo_location ( id );
}

std :: string SRAConfigModel :: get_ngc_root ( std :: string &dir, KNgcObj *ngc )
{
    return model . get_ngc_root ( dir, ngc );
}

SRAConfigModel::RootState SRAConfigModel :: configure_workspace ( const std :: string &path, bool reuseNew )
{
    switch ( model . prepare_repo_directory ( path, reuseNew ) )
    {
    case eSetRootState_OK:
        return RootState_OK;             // successfully changed repository root
    case eSetRootState_NotChanged:
        return RootState_NotChanged;     // the new path is the same as the old one
    case eSetRootState_NotUnique:
        return RootState_NotUnique;      // there is another repository with the same root
    case eSetRootState_MkdirFail:
        return RootState_MkdirFail;      // failed to make new repository directory
    case eSetRootState_NewPathEmpty:
        return RootState_NewPathEmpty;   // new repository directory path is not empty
    case eSetRootState_NewDirNotEmpty:
        return RootState_NewDirNotEmpty; // new repository directory is not empty
    case eSetRootState_NewNotDir:
        return RootState_NewNotDir;      // new repository exists and is not a directory
    case eSetRootState_OldNotEmpty:
        return RootState_OldNotEmpty;    // old repository is not empty
    case eSetRootState_Error:
        return RootState_Error;          // some unusual error happened
    }
}

SRAConfigModel :: SRAConfigModel ( vdbconf_model &config_model, QObject *parent )
    : QObject ( parent )
    , model ( config_model )
{
}

