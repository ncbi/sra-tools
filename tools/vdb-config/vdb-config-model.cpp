/*==============================================================================
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

#include "vdb-config-model.hpp" // vdbconf_model

#include <klib/text.h> /* string_cmp */
#include <klib/vector.h> /* Vector */
#include <klib/rc.h>

#include <kfg/kfg-priv.h>

#include <cstring> // memset

#include <climits> /* PATH_MAX */
#include <stdexcept>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

using namespace std;

const int32_t vdbconf_model::kPublicRepoId = -1;
const int32_t vdbconf_model::kInvalidRepoId = -2;

// error reporting for KConfig_Get/KConfigSet calls: ignore NotFound, throw otherwise
static
void
HandleRC( rc_t rc, const char * call )
{
    if ( rc != 0 && GetRCState ( rc ) != rcNotFound )
    {
        throw logic_error ( string ( "vdbconf_model: " ) + call + " failed" );
    }
}

#define MODEL_THROW_ON_RC( call ) HandleRC ( call, #call )

vdbconf_model::vdbconf_model( CKConfig & config )
    : _config( config )
    , _dir( NULL )
    , _mgr( NULL )
    , _vfs_mgr( NULL )
{
    rc_t rc = KDirectoryNativeDir(&_dir);
    if ( rc != 0 ) throw rc;

    rc = KConfigMakeRepositoryMgrRead( _config.Get(), &_mgr );
    if ( rc != 0 ) throw rc;

    rc = VFSManagerMake ( &_vfs_mgr );
}

vdbconf_model::~vdbconf_model( void )
{
    KRepositoryMgrRelease(_mgr);
    _mgr = NULL;

    KDirectoryRelease(_dir);
    _dir = NULL;

    VFSManagerRelease ( _vfs_mgr );
    _vfs_mgr = NULL;
}

bool vdbconf_model::commit( void )
{
    return _config.Commit() == 0;
}

void vdbconf_model::reload( void )
{
    _config.Reload();
}

std::string vdbconf_model::native_to_internal( const std::string &s ) const
{
    std::string res = "";

    VPath * temp_v_path;
    rc_t rc = VFSManagerMakeSysPath ( _vfs_mgr, &temp_v_path, s.c_str() );
    if ( rc == 0 )
    {
        size_t written;
        char buffer[ PATH_MAX ];
        rc = VPathReadPath ( temp_v_path, buffer, sizeof buffer, &written );
        if ( rc == 0 ) {
            char resolved [ PATH_MAX ] = "";
            rc_t rc = KDirectoryResolvePath
                ( _dir, true, resolved, sizeof resolved, buffer );
            if ( rc == 0 ) {
                if ( string_cmp ( buffer, written, resolved,
                            string_measure ( resolved, NULL ), PATH_MAX ) != 0 )
                {   // make sure the path is canonic
                    res = resolved;
                }
                else {
                    res.assign( buffer, written );
                }
            }
        }
        VPathRelease ( temp_v_path );
    }

    return res;
}

std::string vdbconf_model::internal_to_native( const std::string &s ) const
{
    std::string res = "";
    VPath * temp_v_path;
    rc_t rc = VFSManagerMakePath ( _vfs_mgr, &temp_v_path, "%s", s.c_str() );
    if ( rc == 0 )
    {
        size_t written;
        char buffer[ PATH_MAX ];
        rc = VPathReadSysPath ( temp_v_path, buffer, sizeof buffer, &written );
        if ( rc == 0 )
            res.assign( buffer, written );
        VPathRelease ( temp_v_path );
    }
    return res;
}

bool vdbconf_model::is_http_proxy_enabled( void ) const
{
    bool enabled = true;
    KConfig_Get_Http_Proxy_Enabled(_config.Get(), &enabled, true);
    if ( enabled )
    {
        std::string path = get_http_proxy_path();
        if ( path.empty() ) enabled = false;
    }
    return enabled;
}

void vdbconf_model::set_http_proxy_enabled( bool enabled )
{
    KConfig_Set_Http_Proxy_Enabled(_config.Get(), enabled);
    _config.Updated();
}

std::string vdbconf_model::get_http_proxy_path( void ) const
{
    char buffer[ PATH_MAX ] = "";
    rc_t rc = KConfig_Get_Http_Proxy_Path(_config.Get(), buffer, sizeof buffer, NULL);
    if (rc == 0) {
        return buffer;
    }
    else {
        return "";
    }
}

void vdbconf_model::set_http_proxy_path(const std::string &path)
{
    KConfig_Set_Http_Proxy_Path(_config.Get(), path.c_str());
    _config.Updated();
}

bool vdbconf_model::has_http_proxy_env_higher_priority( void ) const
{
    bool enabled = false;
    KConfig_Has_Http_Proxy_Env_Higher_Priority(_config.Get(), &enabled);
    return enabled;
}
void vdbconf_model::set_http_proxy_env_higher_priority( bool value )
{
    KConfig_Set_Http_Proxy_Env_Higher_Priority(_config.Get(), value);
    _config.Updated();
}

bool vdbconf_model::is_remote_enabled( void ) const
{
    bool res = false;

    rc_t rc = KConfig_Get_Remote_Access_Enabled( _config.Get(), &res );
    if (rc == 0) {
        return res;
    }

    KConfig_Get_Remote_Main_Cgi_Access_Enabled( _config.Get(), &res );
    if (!res) {
        return res;
    }

    KConfig_Get_Remote_Aux_Ncbi_Access_Enabled( _config.Get(), &res );

    return res;
}

void vdbconf_model::set_remote_enabled( bool enabled )
{
    KConfig_Set_Remote_Access_Enabled( _config.Get(), enabled );
    _config.Updated();
}

bool vdbconf_model::does_site_repo_exist( void ) const
{
    KRepositoryVector repositories;
    memset( &repositories, 0, sizeof repositories );
    rc_t rc = KRepositoryMgrSiteRepositories( _mgr, &repositories );
    bool res = ( ( rc == 0 ) && ( VectorLength( &repositories ) > 0 ) );
    KRepositoryVectorWhack( &repositories );
    return res;
}

bool vdbconf_model::is_site_enabled( void ) const
{
    bool res = false;
    KConfig_Get_Site_Access_Enabled( _config.Get(), &res );
    return res;
}
void vdbconf_model::set_site_enabled( bool enabled )
{
    if ( does_site_repo_exist() )
    {
        KConfig_Set_Site_Access_Enabled( _config.Get(), enabled );
        _config.Updated();
    }
}

bool vdbconf_model::allow_all_certs( void ) const
{
    bool res = false;
    KConfig_Get_Allow_All_Certs( _config.Get(), &res );
    return res;
}
void vdbconf_model::set_allow_all_certs( bool enabled )
{
    KConfig_Set_Allow_All_Certs( _config.Get(), enabled );
    _config.Updated();
}

/* THIS IS NEW AND NOT YET IMPLEMENTED IN CONFIG: global cache on/off !!! */
bool vdbconf_model::is_global_cache_enabled( void ) const
{
    bool res = true;
    bool is_disabled;
    rc_t rc = KConfigReadBool ( _config.Get(), "/repository/user/cache-disabled", &is_disabled );
    if ( rc == 0 )
        res = !is_disabled;
    return res;
}

void vdbconf_model::set_global_cache_enabled( bool enabled )
{
    KConfigWriteBool( _config.Get(), "/repository/user/cache-disabled", !enabled );
    _config.Updated();
}

bool vdbconf_model::is_user_cache_enabled( void ) const
{
    bool res = false;
    KConfig_Get_User_Public_Cached( _config.Get(), &res );
    return res;
}
void vdbconf_model::set_user_cache_enabled( bool enabled )
{
    KConfig_Set_User_Public_Cached( _config.Get(), enabled );
    _config.Updated();
}

uint32_t vdbconf_model::get_repo_count( void ) const
{
    uint32_t res = 0;
    KConfigGetProtectedRepositoryCount( _config.Get(), &res );
    return res;
}

int32_t vdbconf_model::get_repo_id( const string & repo_name ) const
{
    if ( repo_name == "public" )
        return kPublicRepoId;

    uint32_t id = 0;
    rc_t rc = KConfigGetProtectedRepositoryIdByName( _config.Get(), repo_name.c_str(), &id );
    if ( rc != 0 )
        return kInvalidRepoId;
    else
        return id;
}


std::string vdbconf_model::get_repo_name( uint32_t id ) const
{
    std::string res = "";
    size_t written;
    char buffer[ 1024 ];
    rc_t rc = KConfigGetProtectedRepositoryName( _config.Get(), id, buffer, sizeof buffer, &written );
    if ( rc == 0 )
        res.assign( buffer, written );
    return res;
}


std::string vdbconf_model::get_repo_description(const string &repo_name) const
{
    size_t written = 0;
    char buffer[ 1024 ];
    rc_t rc = KConfigGetProtectedRepositoryDescriptionByName( _config.Get(),
        repo_name.c_str(), buffer, sizeof buffer, &written );
    if ( rc == 0 )
        return string( buffer, written );
    else
        return "";
}

bool vdbconf_model::is_protected_repo_cached( uint32_t id ) const
{
    bool res = true;
    KConfigGetProtectedRepositoryCachedById( _config.Get(), id, &res );
    return res;
}

void vdbconf_model::set_protected_repo_cached( uint32_t id, bool enabled )
{
    KConfigSetProtectedRepositoryCachedById( _config.Get(), id, enabled );
    _config.Updated();
}

bool vdbconf_model::does_repo_exist( const char * repo_name )
{
    bool res = false;
    KConfigDoesProtectedRepositoryExist( _config.Get(), repo_name, &res );
    return res;
}

std::string vdbconf_model::get_repo_location( uint32_t id ) const
{
    std::string res = "";
    size_t written;
    char buffer[ PATH_MAX ];
    rc_t rc = KConfigGetProtectedRepositoryPathById( _config.Get(), id, buffer, sizeof buffer, &written );
    if ( rc == 0 )
    {
        res.assign( buffer, written );
        res = internal_to_native( res );
    }
    return res;
}


std::string vdbconf_model::get_public_location( void ) const
{
    std::string res = "";
    size_t written;
    char buffer[ PATH_MAX ];
    rc_t rc = KConfig_Get_User_Public_Cache_Location( _config.Get(), buffer, sizeof buffer, &written );
    if ( rc == 0 )
    {
        res.assign( buffer, written );
        res = internal_to_native( res );
    }
    return res;
}


std::string vdbconf_model::get_current_dir( void ) const
{
    std::string res = "./";
    char buffer[ PATH_MAX ];
    rc_t rc = KDirectoryResolvePath ( _dir, true, buffer, sizeof buffer, "./" );
    if ( rc == 0 )
    {
        res.assign( buffer );
        res = internal_to_native( res );
    }
    return res;
}


std::string vdbconf_model::get_home_dir( void ) const
{
    std::string res = "";
    size_t written;
    char buffer[ PATH_MAX ];
    rc_t rc = KConfig_Get_Home( _config.Get(), buffer, sizeof buffer, &written );
    if ( rc == 0 )
    {
        res.assign( buffer, written );
        res = internal_to_native( res );
    }
    return res;
}


std::string vdbconf_model::get_user_default_dir( void ) const
{
    std::string res = "";
    size_t written;
    char buffer[ PATH_MAX ];
    rc_t rc = KConfig_Get_Default_User_Path( _config.Get(), buffer, sizeof buffer, &written );
    if ( rc == 0 )
    {
        res.assign( buffer, written );
        res = internal_to_native( res );
    }
    return res;
}

void vdbconf_model::set_user_default_dir( const char * new_default_dir )
{
    std::string tmp( new_default_dir );
    tmp = native_to_internal( tmp );
    KConfig_Set_Default_User_Path( _config.Get(), tmp.c_str() );
    _config.Updated();
}

std::string vdbconf_model::get_ngc_root( std::string &base, const KNgcObj * ngc ) const
{
    std::string res = "";
    size_t written;
    char buffer[ PATH_MAX ];
    rc_t rc = KNgcObjGetProjectName ( ngc, buffer, sizeof buffer, &written );
    if ( rc == 0 )
    {
        res = native_to_internal( base ) + '/' + buffer;
        res = internal_to_native( res );
    }
    return res;
}

static rc_t CC s_IsEmpty( const KDirectory * dir, uint32_t type, const char * name, void * data )
{
    if ( ( type & ~kptAlias) != kptDir )
        return 1;
    else
        return 0;
}

// repoId == kPublicRepoId is for the public repository
ESetRootState vdbconf_model::x_ChangeRepoLocation( const string &native_newPath,
    bool reuseNew, int32_t repoId, bool flushOld )
{
/*rc_t CC KDirectoryResolvePath_v1 ( const KDirectory_v1 *self, bool absolute,
    char *resolved, size_t rsize, const char *path, ... )
    resolve ~
    and ~user */
    // old root path

    if ( native_newPath.size() == 0 )
        return eSetRootState_NewPathEmpty;

    std::string newPath = native_to_internal( native_newPath );

    string oldPath;

    if ( repoId != kInvalidRepoId )
    {
        if ( repoId == kPublicRepoId )
            oldPath = get_public_location();
        else if ( repoId >= 0 )
            oldPath = get_repo_location( repoId );
        else
            return eSetRootState_Error;

        if ( oldPath.size() == newPath.size() )
        {
            // make sure new path is different from the old one
            if ( oldPath == newPath )
                return eSetRootState_NotChanged;
        }

        // old root path should not be empty - just ignore it now
        if ( oldPath.size() > 0 )
        {
            KPathType type = KDirectoryPathType( _dir, oldPath.c_str() );
            if ( ( type & ~kptAlias ) == kptDir )
            {
                rc_t rc = KDirectoryVisit ( _dir, true, s_IsEmpty, NULL, oldPath.c_str() );
                if ( rc != 0 && !flushOld )
                {
                    // warn if the old repo is not empty and flush was not asked
                    return eSetRootState_OldNotEmpty;
                }
            }
        }
    }

    KPathType type = KDirectoryPathType( _dir, newPath.c_str() );
    uint32_t access = 0775;
    switch ( type & ~kptAlias )
    {
        case kptNotFound :
            {
                // create non existing new repository directory
                rc_t rc = KDirectoryCreateDir( _dir, access, (kcmCreate | kcmParents), newPath.c_str() );
                if ( rc != 0 )
                    return eSetRootState_MkdirFail;
            }
            break;

        case kptDir :
            {
                rc_t rc = KDirectoryVisit( _dir, true, s_IsEmpty, NULL, newPath.c_str() );
                if ( rc != 0 && !reuseNew )
                    // warn if the new repo is not empty and resuse was not asked
                    return eSetRootState_NewDirNotEmpty;
            }
            break;

        // error: new repository exists and it is not a directory
        default : return eSetRootState_NewNotDir;
    }

    // create apps subdirectories
    const char *apps[] = { "files", "nannot", "refseq", "sra", "wgs", NULL };
    for ( const char **p = apps; *p; ++p )
    {
        KPathType type = KDirectoryPathType( _dir, "%s/%s", newPath.c_str(), *p );
        switch ( type & ~kptAlias )
        {
            case kptNotFound : KDirectoryCreateDir( _dir, access, kcmCreate, "%s/%s", newPath.c_str(), *p );
            case kptDir : break;
            default : return eSetRootState_Error;
        }
    }

    // update repository root configiration
    if ( repoId == kPublicRepoId )
        KConfig_Set_User_Public_Cache_Location( _config.Get(), newPath.c_str() );
    else if ( repoId >= 0 )
        KConfigSetProtectedRepositoryPathById( _config.Get(), repoId, newPath.c_str() );

    if ( repoId != kInvalidRepoId )
    {
        // flush the old repository
        for ( const char **p = apps; *p; ++p )
        {
            // completely remove all old apps subdirectories
            KDirectoryRemove( _dir, true, "%s/%s", oldPath.c_str(), *p );
        }
        // remove all old repository directory if it is empty now
        KDirectoryRemove( _dir, false, oldPath.c_str() );
    }

    return eSetRootState_OK;
}

ESetRootState vdbconf_model::set_repo_location(uint32_t id,
    bool flushOld, const string &path, bool reuseNew)
{
    ESetRootState res = x_ChangeRepoLocation( path, reuseNew, id, flushOld );
    _config.Updated();
    return res;
}

ESetRootState vdbconf_model::set_public_location(
    bool flushOld, string &path, bool reuseNew)
{
    ESetRootState res = x_ChangeRepoLocation( path, reuseNew, kPublicRepoId, flushOld );
    _config.Updated();
    return res;
}

ESetRootState vdbconf_model::change_repo_location(bool flushOld,
    const string &newPath, bool reuseNew, int32_t repoId)
{
    ESetRootState res = x_ChangeRepoLocation( newPath, reuseNew, repoId, flushOld );
    _config.Updated();
    return res;
}

ESetRootState vdbconf_model::prepare_repo_directory
    (const string &newPath, bool reuseNew)
{
    ESetRootState res = x_ChangeRepoLocation( newPath, reuseNew, kInvalidRepoId );
    _config.Updated();
    return res;
}

bool vdbconf_model::is_user_public_enabled( void ) const
{
    bool res = true;
    KConfig_Get_User_Public_Enabled( _config.Get(), &res );
    return res;
}
void vdbconf_model::set_user_public_enabled( bool enabled )
{
    KConfig_Set_User_Public_Enabled( _config.Get(), enabled );
    _config.Updated();
}

bool vdbconf_model::is_user_public_cached( void ) const
{
    bool res = true;
    KConfig_Get_User_Public_Cached( _config.Get(), &res );
    return res;
}

void vdbconf_model::set_user_public_cached( bool enabled )
{
    KConfig_Set_User_Public_Cached( _config.Get(), enabled );
    _config.Updated();
}

#if TDB
bool check_locations_unique(KRepositoryVector *nonUniqueRepos,
    const string &newRootPath)
{
    assert(nonUniqueRepos);
    KRepositoryVectorWhack(nonUniqueRepos);

    KRepositoryVector repositories;
    memset(&repositories, 0, sizeof repositories);
    rc_t rc = KRepositoryMgrUserRepositories(_mgr, &repositories);
    uint32_t len = 0;
    if (rc == 0) {
        len = VectorLength(&repositories);
    }
    std::map<const string, const KRepository*> roots;
    typedef std::map<const string, const KRepository*>::const_iterator TCI;
    if (len > 0) {
        for (uint32_t i = 0; i < len; ++i) {
            const KRepository *repo = static_cast<const KRepository*>
                (VectorGet(&repositories, i));
            if (repo != NULL) {
                char buffer[PATH_MAX] = "";
                size_t size = 0;
                rc = KRepositoryRoot(repo, buffer, sizeof buffer, &size);
                if (rc == 0) {
                    const string root(buffer);
                    TI it = find(root);
                    if (it == end()) {
                        insert(std::pair<const string, const KRepository*>
                            (root, repo));
                    }
                    else {
                        if (VectorLength(nonUniqueRepos) == 0) {
                            const KRepository *r = KRepositoryAddRef(repo);
                            if (r != NULL) {
              /*ignored rc = */ VectorAppend(repositories, NULL, r);
                            }
                        }
                        const KRepository *found = (*it);
                        const KRepository *r = KRepositoryAddRef(found);
                    }
                }
            }
        }
    }
    KRepositoryVectorWhack( &repositories );
}
#endif

bool vdbconf_model::import_ngc( const std::string &native_location,
    const KNgcObj *ngc, uint32_t permissions, uint32_t * result_flags )
{
    bool res = false;

    KRepositoryMgr * repo_mgr;
    rc_t rc = KConfigMakeRepositoryMgrUpdate ( _config.Get(), &repo_mgr );
    if ( rc == 0 )
    {
        std::string location = native_to_internal( native_location);

        rc = KRepositoryMgrImportNgcObj( repo_mgr, ngc,
            location.c_str(), permissions, result_flags );
        res = ( rc == 0 );
        KRepositoryMgrRelease( repo_mgr );
    }

    return res;
}

bool vdbconf_model::get_id_of_ngc_obj( const KNgcObj *ngc, uint32_t * id )
{
    bool res = false;
    size_t written;
    char proj_id[ 512 ];
    rc_t rc = KNgcObjGetProjectName( ngc, proj_id, sizeof proj_id, &written );
    if ( rc == 0 )
    {
        rc = KConfigGetProtectedRepositoryIdByName( _config.Get(), proj_id, id );
        res = ( rc == 0 );
    }
    return res;
}

bool vdbconf_model::mkdir(const KNgcObj *ngc)
{
    uint32_t id = 0;
    if (!get_id_of_ngc_obj(ngc, &id)) {
        return false;
    }

    const std::string root(get_repo_location(id));
    if (root.size() == 0) {
        return false;
    }

    if (KDirectoryPathType(_dir, root.c_str()) != kptNotFound) {
        return false;
    }

    return KDirectoryCreateDir(_dir, 0775,
        kcmCreate | kcmParents, root.c_str()) == 0;
}

bool vdbconf_model::does_path_exist( std::string &path )
{
    bool res = false;
    if ( _dir != NULL )
    {
        KPathType type = KDirectoryPathType( _dir, path.c_str() );
        res = ( ( type & ~kptAlias ) == kptDir );
    }
    return res;
}



bool
vdbconf_model::does_prefetch_download_to_cache(void) const
{
    bool ret;
    MODEL_THROW_ON_RC ( KConfig_Get_Prefetch_Download_To_Cache( _config.Get(), & ret) );
    return ret;
}
void
vdbconf_model::set_prefetch_download_to_cache(bool download_to_cache)
{
    MODEL_THROW_ON_RC ( KConfig_Set_Prefetch_Download_To_Cache( _config.Get(), download_to_cache ) );
    _config.Updated();
}

bool
vdbconf_model::does_user_accept_aws_charges(void) const
{
    bool ret;
    MODEL_THROW_ON_RC ( KConfig_Get_User_Accept_Aws_Charges( _config.Get(), & ret) );
    return ret;
}
void
vdbconf_model::set_user_accept_aws_charges(bool accepts_charges)
{
    MODEL_THROW_ON_RC ( KConfig_Set_User_Accept_Aws_Charges( _config.Get(), accepts_charges ) );
    _config.Updated();
}

bool
vdbconf_model::does_user_accept_gcp_charges(void) const
{
    bool ret;
    MODEL_THROW_ON_RC ( KConfig_Get_User_Accept_Gcp_Charges( _config.Get(), & ret) );
    return ret;
}
void
vdbconf_model::set_user_accept_gcp_charges(bool accepts_charges)
{
    MODEL_THROW_ON_RC ( KConfig_Set_User_Accept_Gcp_Charges( _config.Get(), accepts_charges ) );
    _config.Updated();
}

string
vdbconf_model::get_temp_cache_location(void) const
{
    char buf [ PATH_MAX ];
    size_t written;
    MODEL_THROW_ON_RC ( KConfig_Get_Temp_Cache ( _config.Get(), buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_model::set_temp_cache_location(const std::string & path)
{
    MODEL_THROW_ON_RC ( KConfig_Set_Temp_Cache ( _config.Get(), path . c_str() ) );
    _config.Updated();
}

string
vdbconf_model::get_gcp_credential_file_location(void) const
{
    char buf [ PATH_MAX ];
    size_t written;
    MODEL_THROW_ON_RC ( KConfig_Get_Gcp_Credential_File ( _config.Get(), buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_model::set_gcp_credential_file_location(const std::string & path)
{
    MODEL_THROW_ON_RC ( KConfig_Set_Gcp_Credential_File ( _config.Get(), path . c_str() ) );
    _config.Updated();
}

string
vdbconf_model::get_aws_credential_file_location(void) const
{
    char buf [ PATH_MAX ];
    size_t written;
    MODEL_THROW_ON_RC ( KConfig_Get_Aws_Credential_File ( _config.Get(), buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_model::set_aws_credential_file_location(const std::string & path)
{
    MODEL_THROW_ON_RC ( KConfig_Set_Aws_Credential_File ( _config.Get(), path . c_str() ) );
    _config.Updated();
}

string
vdbconf_model::get_aws_profile(void) const
{
    char buf [ PATH_MAX ];
    size_t written;
    MODEL_THROW_ON_RC ( KConfig_Get_Aws_Profile ( _config.Get(), buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_model::set_aws_profile(const std::string & path)
{
    MODEL_THROW_ON_RC ( KConfig_Set_Aws_Profile ( _config.Get(), path . c_str() ) );
    _config.Updated();
}

uint32_t vdbconf_model::get_cache_amount_in_MB( void ) const
{
    uint32_t value;
    MODEL_THROW_ON_RC ( KConfig_Get_Cache_Amount ( _config.Get(), &value ) );
    return value;
}

void vdbconf_model::set_cache_amount_in_MB( uint32_t value )
{
    MODEL_THROW_ON_RC ( KConfig_Set_Cache_Amount ( _config.Get(), value ) );
    _config.Updated();
}

void vdbconf_model::set_defaults( void )
{
    set_remote_enabled( true );
    set_global_cache_enabled( true );
    set_site_enabled( true );

    set_http_proxy_enabled( false );
    
    set_user_accept_aws_charges( false );
    set_user_accept_gcp_charges( false );

    std::string dflt_path( "" );
    set_gcp_credential_file_location( dflt_path );
    set_aws_credential_file_location( dflt_path );

    std::string dflt_profile( "default" );
    set_aws_profile( dflt_profile );
}

std::string vdbconf_model::get_dflt_import_path_start_dir( void )
{
    std::string res = get_user_default_dir();
    if ( !does_path_exist( res ) )
        res = get_home_dir();
    if ( !does_path_exist( res ) )
        res = get_current_dir();
    return res;
}

