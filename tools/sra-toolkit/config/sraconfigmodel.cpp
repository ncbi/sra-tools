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

#include "sraconfigmodel.h"
#include "../vdb-config/vdb-config-model.hpp"

#include <kfg/config.h>

#include <QMessageBox>
#include <string>

static
RootState translate_state ( ESetRootState state )
{
    switch ( state )
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

bool SRAConfigModel::commit()
{
    return model . commit ();
}

bool SRAConfigModel :: config_changed ()
{
    return model . get_config_changed ();
}


bool SRAConfigModel :: path_exists ( std :: string &path )
{
    return model . does_path_exist ( path );
}

void SRAConfigModel :: create_directory ( const KNgcObj *ngc )
{
    model . mkdir ( ngc );
}

void SRAConfigModel :: reload ()
{
    model . reload ();
}

// cache
bool SRAConfigModel :: global_cache_enabled ()
{
    return model . is_global_cache_enabled ();
}

bool SRAConfigModel :: user_cache_enabled ()
{
    return model . is_user_cache_enabled ();
}

void SRAConfigModel :: set_global_cache_enabled ( bool enabled )
{
    model . set_global_cache_enabled ( enabled );
}

void SRAConfigModel :: set_user_cache_enabled ( bool enabled )
{
    model . set_user_cache_enabled ( enabled );
}

// network
bool SRAConfigModel :: remote_enabled ()
{
    return model . is_remote_enabled ();
}

void SRAConfigModel :: set_remote_enabled ( bool enabled )
{
    model . set_remote_enabled ( enabled );
}

bool SRAConfigModel :: site_enabled ()
{
    return model . is_site_enabled ();
}

void SRAConfigModel :: set_site_enabled ( bool enabled )
{
    model . set_site_enabled ( enabled );
}

bool SRAConfigModel :: allow_all_certs ()
{
    return model . allow_all_certs ();
}

void SRAConfigModel :: set_allow_all_certs ( bool allow )
{
    model . set_allow_all_certs ( allow );
}

// ngc
bool SRAConfigModel :: import_ngc ( const std :: string path, const KNgcObj *ngc, uint32_t permissions, uint32_t *rslt_flags )
{
    return model . import_ngc ( path, ngc, permissions, rslt_flags );
}

bool SRAConfigModel :: get_ngc_obj_id ( const KNgcObj *ngc, uint32_t *id )
{
    return model . get_id_of_ngc_obj ( ngc, id );
}

std :: string SRAConfigModel :: get_ngc_root ( std :: string &dir, const KNgcObj *ngc )
{
    return model . get_ngc_root ( dir, ngc );
}

// proxy
bool SRAConfigModel :: proxy_enabled ()
{
    return model . is_http_proxy_enabled ();
}

void SRAConfigModel :: set_proxy_enabled ( bool enabled )
{
    model . set_http_proxy_enabled ( enabled );
}

std :: string SRAConfigModel :: get_proxy_path ()
{
    return model . get_http_proxy_path ();
}

void SRAConfigModel :: set_proxy_path ( const std :: string &path )
{
    model . set_http_proxy_path ( path );
}

bool SRAConfigModel :: proxy_priority ()
{
    return model . has_http_proxy_env_higher_priority ();
}

void SRAConfigModel :: set_proxy_priority ( bool priority )
{
    model . set_http_proxy_env_higher_priority ( priority );
}

// settings
std :: string SRAConfigModel :: get_current_path ()
{
    return model . get_current_dir ();
}

std :: string SRAConfigModel :: get_home_path ()
{
    return model . get_home_dir ();
}

std :: string SRAConfigModel :: get_public_path ()
{
    return model . get_public_location ();
}

RootState SRAConfigModel :: set_public_path ( std :: string &path, bool flushOld, bool reuseNew )
{
    return translate_state ( model . set_public_location ( flushOld, path, reuseNew ) );
}

std :: string SRAConfigModel :: get_user_default_path ()
{
    return model . get_user_default_dir ();
}

void SRAConfigModel :: set_user_default_path ( const char *path )
{
    model . set_user_default_dir ( path );
}

// workspace

bool SRAConfigModel :: site_workspace_exists ()
{
    return model . does_site_repo_exist ();
}

RootState SRAConfigModel :: configure_workspace ( const std :: string &path, bool reuseNew )
{
    return translate_state ( model . prepare_repo_directory ( path, reuseNew ) );
}

uint32_t SRAConfigModel :: workspace_count ()
{
    return model . get_repo_count ();
}

int32_t SRAConfigModel :: get_workspace_id ( const std :: string &name )
{
    return model . get_repo_id ( name );
}

std :: string SRAConfigModel :: get_workspace_name ( uint32_t id )
{
    return model . get_repo_name ( id );
}

std :: string SRAConfigModel :: get_workspace_path ( uint32_t id )
{
    return model . get_repo_location ( id );
}

RootState SRAConfigModel :: set_workspace_path ( const std :: string & path, uint32_t id, bool flushOld, bool reuseNew )
{
    return translate_state ( model . set_repo_location ( id, flushOld, path, reuseNew ) );
}

SRAConfigModel :: SRAConfigModel ( vdbconf_model &config_model, QObject *parent )
    : QObject ( parent )
    , model ( config_model )
{
}

