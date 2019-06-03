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

#include "util.hpp"

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

        vdbconf_model( CKConfig & config );
        ~vdbconf_model( void );

        // ----------------------------------------------------------------
        std::string native_to_internal( const std::string &s ) const;
        std::string internal_to_native( const std::string &s ) const;

        bool get_config_changed( void ) const { return _config.IsUpdated(); }

        // ----------------------------------------------------------------

        bool is_http_proxy_enabled( void ) const;

        void set_http_proxy_enabled( bool enabled );

        std::string get_http_proxy_path( void ) const;

        void set_http_proxy_path(const std::string &path);

        bool has_http_proxy_env_higher_priority( void ) const;
        void set_http_proxy_env_higher_priority( bool value );

        // ----------------------------------------------------------------
        bool is_remote_enabled( void ) const;
        void set_remote_enabled( bool enabled );

        // ----------------------------------------------------------------
        bool does_site_repo_exist( void ) const;

        bool is_site_enabled( void ) const;
        void set_site_enabled( bool enabled );

        // ----------------------------------------------------------------
        bool allow_all_certs( void ) const;
        void set_allow_all_certs( bool enabled );

        // ----------------------------------------------------------------
        /* THIS IS NEW AND NOT YET IMPLEMENTED IN CONFIG: global cache on/off !!! */
        bool is_global_cache_enabled( void ) const;

        void set_global_cache_enabled( bool enabled );

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
        bool is_user_cache_enabled( void ) const;
        void set_user_cache_enabled( bool enabled );

        // ----------------------------------------------------------------
        uint32_t get_repo_count( void ) const;


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

        bool is_protected_repo_cached( uint32_t id ) const;

        void set_protected_repo_cached( uint32_t id, bool enabled );

        bool does_repo_exist( const char * repo_name );

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

        bool is_user_public_enabled( void ) const;
        void set_user_public_enabled( bool enabled );

        bool is_user_public_cached( void ) const;

        void set_user_public_cached( bool enabled );

        // ----------------------------------------------------------------
        std::string get_current_dir( void ) const;
        std::string get_home_dir( void ) const;

        std::string get_user_default_dir( void ) const;
        std::string get_ngc_root( std::string &base, const KNgcObj * ngc ) const;

        void set_user_default_dir( const char * new_default_dir );

        // ----------------------------------------------------------------
        bool import_ngc( const std::string &native_location,
            const KNgcObj *ngc, uint32_t permissions,
            uint32_t * result_flags );

        bool get_id_of_ngc_obj( const KNgcObj *ngc, uint32_t * id );

        bool mkdir(const KNgcObj *ngc);

        bool does_path_exist( std::string &path );

        /* does prefetch download ETL to output directory or cache? */
        bool does_prefetch_download_to_cache(void) const;
        void set_prefetch_download_to_cache(bool download_to_cache);

        /* does user agree to accept charges? */
        bool does_user_accept_aws_charges(void) const;
        bool does_user_accept_gcp_charges(void) const;
        void set_user_accept_aws_charges(bool accepts_charges);
        void set_user_accept_gcp_charges(bool accepts_charges);

        /* preferred temporary cache location */
        std::string get_temp_cache_location(void) const;
        void set_temp_cache_location(const std::string & path);

        /* user-pay for GCP, a file name */
        std::string get_gcp_credential_file_location(void) const;
        void set_gcp_credential_file_location(const std::string & path);

        /* user-pay for AWS, can be a directory or a file */
        std::string get_aws_credential_file_location(void) const;
        void set_aws_credential_file_location(const std::string & path);

        /* "default" if not present or empty */
        std::string get_aws_profile(void) const;
        void set_aws_profile(const std::string & name);

        /* how much memory to use for caching */
        uint32_t get_cache_amount_in_MB( void ) const;
        void set_cache_amount_in_MB( uint32_t value );
        
        // ----------------------------------------------------------------
        bool commit( void );
        void reload( void ); // throws on error
        void set_defaults( void );
        std::string get_dflt_import_path_start_dir( void );
        
    private :
        CKConfig & _config;

        KDirectory * _dir;
        const KRepositoryMgr *_mgr;
        VFSManager *_vfs_mgr;

        ESetRootState x_ChangeRepoLocation(const std::string &native_newPath,
            bool reuseNew, int32_t repoId, bool flushOld = false);
};

#endif
