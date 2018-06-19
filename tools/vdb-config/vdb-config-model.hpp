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

#ifndef _h_vdb_config_model_
#define _h_vdb_config_model_

#include <kfg/config.h>
#include <kfg/properties.h>
#include <kfg/repository.h>
#include <kfg/ngc.h>
#include <kfs/directory.h>
#include <vfs/manager.h>
#include <vfs/path.h>

#include <string>

enum ESetRootState {
    eSetRootState_OK,            // successfully changed repository root
    eSetRootState_NotChanged,    // the new path is the same as the old one
    eSetRootState_NotUnique,   // there is another repository with the same root
    eSetRootState_MkdirFail,     // failed to make new repository directory
    eSetRootState_NewPathEmpty,  // new repository directory path is not empty
    eSetRootState_NewDirNotEmpty,// new repository directory is not empty
    eSetRootState_NewNotDir,     // new repository exists and is not a directory
    eSetRootState_OldNotEmpty,   // old repository is not empty
    eSetRootState_Error,         // some unusual error happened
};

/*
    a c++ class the encapsulates the connection to 
*/

class vdbconf_model
{
    public :
        static const int32_t kPublicRepoId;
        static const int32_t kInvalidRepoId;

        vdbconf_model( KConfig * config )
            : _config( config )
            , _config_valid( _config != NULL )
            , _config_changed( false )
            , _dir( NULL )
            , _mgr( NULL )
            , _vfs_mgr( NULL )
        {
            if ( KConfigAddRef( config ) != 0 )
            {
                _config = NULL;
                _config_valid = false;
            }

            assert(_config && _config_valid);

            rc_t rc = KDirectoryNativeDir(&_dir);
            if ( rc != 0 ) throw rc;

            rc = KConfigMakeRepositoryMgrRead( _config, &_mgr );
            if ( rc != 0 ) throw rc;

            rc = VFSManagerMake ( &_vfs_mgr );
        }

        ~vdbconf_model( void )
        {
            if ( _config_valid ) {
                KConfigRelease ( _config );
                _config = NULL;
            }

            KRepositoryMgrRelease(_mgr);
            _mgr = NULL;

            KDirectoryRelease(_dir);
            _dir = NULL;

            VFSManagerRelease ( _vfs_mgr );
            _vfs_mgr = NULL;
        }

        // ----------------------------------------------------------------
        std::string native_to_internal( const std::string &s ) const;
        std::string internal_to_native( const std::string &s ) const;

        bool get_config_changed( void ) const { return _config_changed; }

        // ----------------------------------------------------------------

        bool is_http_proxy_enabled( void ) const {
            bool enabled = true;
            KConfig_Get_Http_Proxy_Enabled(_config, &enabled, true);
			if ( enabled )
			{
				std::string path = get_http_proxy_path();
				if ( path.empty() ) enabled = false;
			}
            return enabled;
        }

        void set_http_proxy_enabled( bool enabled ) {
            KConfig_Set_Http_Proxy_Enabled(_config, enabled);
			_config_changed = true;
        }

        std::string get_http_proxy_path( void ) const;

        void set_http_proxy_path(const std::string &path) {
            KConfig_Set_Http_Proxy_Path(_config, path.c_str());
			_config_changed = true;
        }

        bool has_http_proxy_env_higher_priority( void ) const {
            bool enabled = false;
            KConfig_Has_Http_Proxy_Env_Higher_Priority(_config, &enabled);
            return enabled;
        }
        void set_http_proxy_env_higher_priority( bool value ) {
            KConfig_Set_Http_Proxy_Env_Higher_Priority(_config, value);
			_config_changed = true;
        }

        // ----------------------------------------------------------------
        bool is_remote_enabled( void ) const
        {
            bool res = false;

            rc_t rc = KConfig_Get_Remote_Access_Enabled( _config, &res );
            if (rc == 0) {
                return res;
            }

            KConfig_Get_Remote_Main_Cgi_Access_Enabled( _config, &res );
            if (!res) {
                return res;
            }

            KConfig_Get_Remote_Aux_Ncbi_Access_Enabled( _config, &res );

            return res;
        }

        void set_remote_enabled( bool enabled )
        {
            if ( _config_valid )
            {
                KConfig_Set_Remote_Access_Enabled( _config, enabled );
                _config_changed = true;
            }
        }

        // ----------------------------------------------------------------
        bool does_site_repo_exist( void ) const;

        bool is_site_enabled( void ) const
        {
            bool res = false;
            if ( _config_valid ) KConfig_Get_Site_Access_Enabled( _config, &res ); 
            return res;
        }
        void set_site_enabled( bool enabled )
        {
            if ( does_site_repo_exist() && _config_valid )
            {
                KConfig_Set_Site_Access_Enabled( _config, enabled );
                _config_changed = true;
            }
        }

        // ----------------------------------------------------------------
        bool allow_all_certs( void ) const
        {
            bool res = false;
            if ( _config_valid ) KConfig_Get_Allow_All_Certs( _config, &res );
            return res;
        }
        void set_allow_all_certs( bool enabled )
        {
            KConfig_Set_Allow_All_Certs( _config, enabled );
            _config_changed = true;
        }

        // ----------------------------------------------------------------
        /* THIS IS NEW AND NOT YET IMPLEMENTED IN CONFIG: global cache on/off !!! */
        bool is_global_cache_enabled( void ) const
        {
            bool res = true;
            if ( _config_valid )
            {
                bool is_disabled;
                rc_t rc = KConfigReadBool ( _config, "/repository/user/cache-disabled", &is_disabled );
                if ( rc == 0 )
                    res = !is_disabled;
            }
            return res;
        }

        void set_global_cache_enabled( bool enabled )
        {
            if ( _config_valid )
            {
                KConfigWriteBool( _config, "/repository/user/cache-disabled", !enabled );
                _config_changed = true;
            }
        }

  // ----------------------------- //
  // ADD DEFINE IF YOU NEED IT !!! //
  // ----------------------------- //
        bool is_user_enabled( void ) const
        {
            bool res = true;
#ifdef ALLOW_USER_REPOSITORY_DISABLING
            if ( _config_valid ) KConfig_Get_User_Access_Enabled( _config, &res ); 
#endif
            return res;
        }
        void set_user_enabled( bool enabled )
        {
#ifdef ALLOW_USER_REPOSITORY_DISABLING
            if ( _config_valid ) KConfig_Set_User_Access_Enabled( _config, enabled );
#endif
        }

        // ----------------------------------------------------------------
        bool is_user_cache_enabled( void ) const
        {
            bool res = false;
            if ( _config_valid ) KConfig_Get_User_Public_Cached( _config, &res ); 
            return res;
        }
        void set_user_cache_enabled( bool enabled )
        {
            if ( _config_valid )
            {
                KConfig_Set_User_Public_Cached( _config, enabled );
                _config_changed = true;
            }
        }

        // ----------------------------------------------------------------
        uint32_t get_repo_count( void ) const
        {
            uint32_t res = 0;
            if ( _config_valid ) KConfigGetProtectedRepositoryCount( _config, &res );
            return res;
        }


        /* Returns:
         *  kInvalidRepoId if not found,
         *  kPublicRepoId for the user public repository
         *  protected repository id otherwise
         */
        int32_t get_repo_id( const std::string & repo_name ) const;

        std::string get_repo_name( uint32_t id ) const;
        std::string get_repo_description( const std::string & repo_name ) const;

        bool is_protected_repo_enabled( uint32_t id ) const
        {
            bool res = true;
#ifdef ALLOW_USER_REPOSITORY_DISABLING
            if ( _config_valid ) KConfigGetProtectedRepositoryEnabledById( _config, id, &res ); 
#endif
            return res;
        }
        void set_protected_repo_enabled( uint32_t id, bool enabled )
        {
#ifdef ALLOW_USER_REPOSITORY_DISABLING
            if ( _config_valid ) KConfigSetProtectedRepositoryEnabledById( _config, id, enabled ); 
#endif
        }

        bool is_protected_repo_cached( uint32_t id ) const
        {
            bool res = true;
            if ( _config_valid )
                KConfigGetProtectedRepositoryCachedById( _config, id, &res );
            return res;
        }

        void set_protected_repo_cached( uint32_t id, bool enabled )
        {
            if ( _config_valid )
            {
                KConfigSetProtectedRepositoryCachedById( _config, id, enabled );
                _config_changed = true;
            }
        }

        bool does_repo_exist( const char * repo_name )
        {
            bool res = false;
            if ( _config_valid ) KConfigDoesProtectedRepositoryExist( _config, repo_name, &res );
            return res;
        }

        std::string get_repo_location( uint32_t id ) const;

        ESetRootState set_repo_location( uint32_t id,
            bool flushOld, const std::string &path, bool reuseNew );

/* ----------------------------------------------------------------
 * flushOld repository
 * reuseNew repository: whether to refuse to change location on existing newPath
 */
        ESetRootState change_repo_location(bool flushOld,
            const std::string &newPath, bool reuseNew, int32_t repoId);

        ESetRootState prepare_repo_directory(const std::string &newPath,
            bool reuseNew = false);

        // ----------------------------------------------------------------

        std::string get_public_location( void ) const;

        ESetRootState set_public_location( bool flushOld, std::string &path, bool reuseNew );

        bool is_user_public_enabled( void ) const
        {
            bool res = true;
            if ( _config_valid ) KConfig_Get_User_Public_Enabled( _config, &res ); 
            return res;
        }
        void set_user_public_enabled( bool enabled )
        {
            if ( _config_valid )
            {
                KConfig_Set_User_Public_Enabled( _config, enabled ); 
                _config_changed = true;
            }
        }

        bool is_user_public_cached( void ) const
        {
            bool res = true;
            if ( _config_valid ) KConfig_Get_User_Public_Cached( _config, &res ); 
            return res;
        }

        void set_user_public_cached( bool enabled )
        {
            if ( _config_valid )
            {
                KConfig_Set_User_Public_Cached( _config, enabled ); 
                _config_changed = true;
            }
        }

        // ----------------------------------------------------------------
        std::string get_current_dir( void ) const;
        std::string get_home_dir( void ) const;

        std::string get_user_default_dir( void ) const;
        std::string get_ngc_root( std::string &base, const KNgcObj * ngc ) const;

        void set_user_default_dir( const char * new_default_dir )
        {
            if ( _config_valid )
            {
                std::string tmp( new_default_dir );
                tmp = native_to_internal( tmp );
                KConfig_Set_Default_User_Path( _config, tmp.c_str() );
                _config_changed = true;
            }
        }

        // ----------------------------------------------------------------
        bool commit( void )
        {
            bool res = false;
            if ( _config_valid )
            {
                res = ( KConfigCommit ( _config ) == 0 );
                if ( res ) _config_changed = false;
            }
            return res;
        }

        // ----------------------------------------------------------------
        bool import_ngc( const std::string &native_location,
            const KNgcObj *ngc, uint32_t permissions,
            uint32_t * result_flags );

        bool get_id_of_ngc_obj( const KNgcObj *ngc, uint32_t * id )
        {
            bool res = false;
            if ( _config_valid )
            {
                size_t written;
                char proj_id[ 512 ];
                rc_t rc = KNgcObjGetProjectName( ngc, proj_id, sizeof proj_id, &written );
                if ( rc == 0 )
                {
                    rc = KConfigGetProtectedRepositoryIdByName( _config, proj_id, id );
                    res = ( rc == 0 );
                }
            }
            return res;
        }

        bool mkdir(const KNgcObj *ngc) {
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

        bool does_path_exist( std::string &path )
        {
            bool res = false;
            if ( _dir != NULL )
            {
                KPathType type = KDirectoryPathType( _dir, path.c_str() );
                res = ( ( type & ~kptAlias ) == kptDir );
            }
            return res;
        }

        bool reload( void )
        {
            if ( _config_valid )
            {
                KRepositoryMgrRelease ( _mgr );
                _mgr = NULL;

                KConfigRelease ( _config );
                _config_valid = ( KConfigMake ( &_config, NULL ) == 0 );

                if ( _config_valid )
                    KConfigMakeRepositoryMgrRead( _config, &_mgr );

                _config_changed = false;
            }
            return _config_valid;
        }

    private :
        KConfig * _config;
        bool _config_valid;
        bool _config_changed;

        KDirectory * _dir;
        const KRepositoryMgr *_mgr;
        VFSManager *_vfs_mgr;

        ESetRootState x_ChangeRepoLocation(const std::string &native_newPath,
            bool reuseNew, int32_t repoId, bool flushOld = false);
};

#endif
