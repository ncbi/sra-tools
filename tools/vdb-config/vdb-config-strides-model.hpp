#ifndef _h_vdb_config_strides_model_
#define _h_vdb_config_strides_model_

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
* =========================================================================== */

#include <string>

struct vdbconf_strides_modelImpl;
struct KConfig;

class vdbconf_strides_model
{
public:
    vdbconf_strides_model(struct KConfig * config);
    ~vdbconf_strides_model(void);

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

    /* user-pay for GCP */
    std::string get_gcp_credential_file_location(void) const;
    void set_gcp_credential_file_location(const std::string & path);

    /* user-pay for AWS */
    std::string get_aws_credential_file_location(void) const;
    void set_aws_credential_file_location(const std::string & path);

    std::string get_aws_profile(void) const;
    void set_aws_profile(const std::string & name);

    bool get_aws_credentials_from_env(void) const;
    void set_aws_credentials_from_env(bool from_env);

    /* commit KConfig; returns true if commit returns 0 */
    bool commit(void);

private:
    /* disable copy */
    vdbconf_strides_model(const vdbconf_strides_model&);
    vdbconf_strides_model& operator=(vdbconf_strides_model const&);

    struct vdbconf_strides_modelImpl * m_Impl;
};

#endif // _h_vdb_config_strides_model_
