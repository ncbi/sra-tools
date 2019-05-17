#include "vdb-config-strides-model.hpp"

#include <stdexcept>

#include <kfg/config.h>
#include <kfg/properties.h>

using namespace std;

#ifndef MAX_PATH
    #define MAX_PATH 4096
#endif

#define STRIDES_THROW_ON_RC( call ) \
    do { if ( call != 0 ) throw logic_error ( string ( "vdbconf_strides_model" ) + #call + " failed" ); } while ( false )

struct vdbconf_strides_modelImpl {
    KConfig * config;
};

vdbconf_strides_model::vdbconf_strides_model(KConfig * config)
: m_Impl ( nullptr )
{
    if ( config == nullptr )
    {
        throw invalid_argument ( "vdbconf_strides_model ctor: NULL KConfig" );
    }

    m_Impl = new vdbconf_strides_modelImpl;

    if ( KConfigAddRef ( config ) != 0 )
    {
        delete m_Impl;
        throw logic_error ( "vdbconf_strides_model ctor: KConfigAddRef failed" );
    }

    m_Impl -> config = config;
}

vdbconf_strides_model::~vdbconf_strides_model()
{
    KConfigRelease ( m_Impl -> config );
    delete m_Impl;
}

//all vdbconf_strides_model::does|set|get functions call functions from ncbi-vdb/libs/properties.c

bool
vdbconf_strides_model::does_prefetch_download_to_cache(void) const
{
    bool ret;
    STRIDES_THROW_ON_RC ( KConfig_Get_Prefetch_Download_To_Cache( m_Impl -> config, & ret) );
    return ret;
}
void
vdbconf_strides_model::set_prefetch_download_to_cache(bool download_to_cache)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_Prefetch_Download_To_Cache( m_Impl -> config, download_to_cache ) );
}

bool
vdbconf_strides_model::does_user_accept_aws_charges(void) const
{
    bool ret;
    STRIDES_THROW_ON_RC ( KConfig_Get_User_Accept_Aws_Charges( m_Impl -> config, & ret) );
    return ret;
}
void
vdbconf_strides_model::set_user_accept_aws_charges(bool accepts_charges)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_User_Accept_Aws_Charges( m_Impl -> config, accepts_charges ) );
}

bool
vdbconf_strides_model::does_user_accept_gcp_charges(void) const
{
    bool ret;
    STRIDES_THROW_ON_RC ( KConfig_Get_User_Accept_Gcp_Charges( m_Impl -> config, & ret) );
    return ret;
}
void
vdbconf_strides_model::set_user_accept_gcp_charges(bool accepts_charges)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_User_Accept_Gcp_Charges( m_Impl -> config, accepts_charges ) );
}

string
vdbconf_strides_model::get_temp_cache_location(void) const
{
    char buf [ MAX_PATH ];
    size_t written;
    STRIDES_THROW_ON_RC ( KConfig_Get_Temp_Cache ( m_Impl -> config, buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_strides_model::set_temp_cache_location(const std::string & path)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_Temp_Cache ( m_Impl -> config, path . c_str() ) );
}

string
vdbconf_strides_model::get_gcp_credential_file_location(void) const
{
    char buf [ MAX_PATH ];
    size_t written;
    STRIDES_THROW_ON_RC ( KConfig_Get_Gcp_Credential_File ( m_Impl -> config, buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_strides_model::set_gcp_credential_file_location(const std::string & path)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_Gcp_Credential_File ( m_Impl -> config, path . c_str() ) );
}

string
vdbconf_strides_model::get_aws_credential_file_location(void) const
{
    char buf [ MAX_PATH ];
    size_t written;
    STRIDES_THROW_ON_RC ( KConfig_Get_Aws_Credential_File ( m_Impl -> config, buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_strides_model::set_aws_credential_file_location(const std::string & path)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_Aws_Credential_File ( m_Impl -> config, path . c_str() ) );
}

string
vdbconf_strides_model::get_aws_profile(void) const
{
    char buf [ MAX_PATH ];
    size_t written;
    STRIDES_THROW_ON_RC ( KConfig_Get_Aws_Profile ( m_Impl -> config, buf, sizeof buf, & written ) );
    return string ( buf, written );
}
void
vdbconf_strides_model::set_aws_profile(const std::string & path)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_Aws_Profile ( m_Impl -> config, path . c_str() ) );
}

bool
vdbconf_strides_model::get_aws_credentials_from_env(void) const
{
    bool ret;
    STRIDES_THROW_ON_RC ( KConfig_Get_Aws_Credentials_from_env( m_Impl -> config, & ret) );
    return ret;
}
void
vdbconf_strides_model::set_aws_credentials_from_env(bool accepts_charges)
{
    STRIDES_THROW_ON_RC ( KConfig_Set_Aws_Credentials_from_env( m_Impl -> config, accepts_charges ) );
}

bool vdbconf_strides_model::commit(void)
{
    return KConfigCommit ( m_Impl -> config ) == 0;
}
