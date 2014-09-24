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

#include <klib/vector.h> /* Vector */

#include <cstring> // memset

#include <climits> /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

using std::string;

const int32_t vdbconf_model::kPublicRepoId = -1;
const int32_t vdbconf_model::kInvalidRepoId = -2;


std::string vdbconf_model::native_to_internal( const std::string &s ) const
{
    std::string res = "";
    VPath * temp_v_path;
    rc_t rc = VFSManagerMakeSysPath ( _vfs_mgr, &temp_v_path, s.c_str() );
    if ( rc == 0 )
    {
        size_t written;
        char buffer[ 4096 ];
        rc = VPathReadPath ( temp_v_path, buffer, sizeof buffer, &written );
        if ( rc == 0 )
            res.assign( buffer, written );
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
        char buffer[ 4096 ];
        rc = VPathReadSysPath ( temp_v_path, buffer, sizeof buffer, &written );
        if ( rc == 0 )
            res.assign( buffer, written );
        VPathRelease ( temp_v_path );
    }
    return res;
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


int32_t vdbconf_model::get_repo_id( const string & repo_name ) const
{
    if ( repo_name == "public" )
        return kPublicRepoId;

    uint32_t id = 0;
    rc_t rc = KConfigGetProtectedRepositoryIdByName( _config, repo_name.c_str(), &id );
    if ( rc != 0 )
        return kInvalidRepoId;
    else
        return id;
}


std::string vdbconf_model::get_repo_name( uint32_t id ) const
{
    std::string res = "";
    if ( _config_valid )
    {
        size_t written;
        char buffer[ 1024 ];
        rc_t rc = KConfigGetProtectedRepositoryName( _config, id, buffer, sizeof buffer, &written );
        if ( rc == 0 )
            res.assign( buffer, written );
    }
    return res;
}


std::string vdbconf_model::get_repo_description(const string &repo_name) const
{
    size_t written = 0;
    char buffer[ 1024 ];
    rc_t rc = KConfigGetProtectedRepositoryDescriptionByName( _config,
        repo_name.c_str(), buffer, sizeof buffer, &written );
    if ( rc == 0 )
        return string( buffer, written );
    else
        return "";
}

std::string vdbconf_model::get_repo_location( uint32_t id ) const
{
    std::string res = "";
    if ( _config_valid )
    {
        size_t written;
        char buffer[ PATH_MAX ];
        rc_t rc = KConfigGetProtectedRepositoryPathById( _config, id, buffer, sizeof buffer, &written );
        if ( rc == 0 )
        {
            res.assign( buffer, written );
            res = internal_to_native( res );
        }
    }
    return res;
}


std::string vdbconf_model::get_public_location( void ) const
{
    std::string res = "";
    if ( _config_valid )
    {
        size_t written;
        char buffer[ PATH_MAX ];
        rc_t rc = KConfig_Get_User_Public_Cache_Location( _config, buffer, sizeof buffer, &written );
        if ( rc == 0 )
        {
            res.assign( buffer, written );
            res = internal_to_native( res );
        }
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
    if ( _config_valid )
    {
        size_t written;
        char buffer[ PATH_MAX ];
        rc_t rc = KConfig_Get_Home( _config, buffer, sizeof buffer, &written );
        if ( rc == 0 )
        {
            res.assign( buffer, written );
            res = internal_to_native( res );
        }
    }
    return res;
}


std::string vdbconf_model::get_user_default_dir( void ) const
{
    std::string res = "";
    if ( _config_valid )
    {
        size_t written;
        char buffer[ PATH_MAX ];
        rc_t rc = KConfig_Get_Default_User_Path( _config, buffer, sizeof buffer, &written );
        if ( rc == 0 )
        {
            res.assign( buffer, written );
            res = internal_to_native( res );
        }
    }
    return res;
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
        KConfig_Set_User_Public_Cache_Location( _config, newPath.c_str() );
    else if ( repoId >= 0 )
        KConfigSetProtectedRepositoryPathById( _config, repoId, newPath.c_str() );

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
    _config_changed = true;
    return res;
}

ESetRootState vdbconf_model::set_public_location(
    bool flushOld, string &path, bool reuseNew)
{
    ESetRootState res = x_ChangeRepoLocation( path, reuseNew, kPublicRepoId, flushOld );
    _config_changed = true;
    return res;
}

ESetRootState vdbconf_model::change_repo_location(bool flushOld,
    const string &newPath, bool reuseNew, int32_t repoId)
{
    ESetRootState res = x_ChangeRepoLocation( newPath, reuseNew, repoId, flushOld );
    _config_changed = true;
    return res;
}

ESetRootState vdbconf_model::prepare_repo_directory
    (const string &newPath, bool reuseNew)
{
    ESetRootState res = x_ChangeRepoLocation( newPath, reuseNew, kInvalidRepoId );
    _config_changed = true;
    return res;
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
