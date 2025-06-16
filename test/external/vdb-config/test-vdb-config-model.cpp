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

/**
* Unit tests for FASTQ loader
*/
#include <ktst/unit_test.hpp>

#include <kfs/directory.h>

#include <kfg/kfg-priv.h>

#include "../../tools/external/vdb-config/vdb-config-model.cpp"
#include "../../tools/external/vdb-config/util.cpp"

TEST_SUITE(VdbConfStridesModelTestSuite);

class VdbModelFixture
{
public:
    VdbModelFixture()
    {
    }
    ~VdbModelFixture()
    {
    }

    CKConfig kfg;
};

FIXTURE_TEST_CASE( get_config_changed, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE ( ! m . get_config_changed () );
}

FIXTURE_TEST_CASE( prefetch_download_to_cache, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE ( m . does_prefetch_download_to_cache() ); // true by default
    m . set_prefetch_download_to_cache( false );
    REQUIRE ( ! m . does_prefetch_download_to_cache() );
    REQUIRE ( m . get_config_changed () );
}

FIXTURE_TEST_CASE( user_accept_aws_charges, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE ( ! m . does_user_accept_aws_charges() ); // false by default
    m . set_user_accept_aws_charges( true );
    REQUIRE ( m . does_user_accept_aws_charges() );
    REQUIRE ( m . get_config_changed () );
}

FIXTURE_TEST_CASE( user_accept_gcp_charges, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE ( ! m . does_user_accept_gcp_charges() ); // false by default
    m . set_user_accept_gcp_charges( true );
    REQUIRE ( m . does_user_accept_gcp_charges() );
    REQUIRE ( m . get_config_changed () );
}

FIXTURE_TEST_CASE( temp_cache_location, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string(), m . get_temp_cache_location() );
    m . set_temp_cache_location( "path" ); // converts to an absolute path
    REQUIRE_NE ( string::npos, m . get_temp_cache_location().find( "path" ) );
    REQUIRE ( m . get_config_changed () );
}

FIXTURE_TEST_CASE( gcp_credential_file_location, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string(), m . get_gcp_credential_file_location() );
    m . set_gcp_credential_file_location( "path" );
    REQUIRE_EQ ( string ( "path" ),  m . get_gcp_credential_file_location() );
    REQUIRE ( m . get_config_changed () );
}

FIXTURE_TEST_CASE( aws_credential_file_location, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string(), m . get_aws_credential_file_location() );
    m . set_aws_credential_file_location( "path" );
    REQUIRE_EQ ( string ( "path" ),  m . get_aws_credential_file_location() );
    REQUIRE ( m . get_config_changed () );
}

FIXTURE_TEST_CASE( aws_profile, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string( "default" ), m . get_aws_profile() );
    m . set_aws_profile( "path" );
    REQUIRE_EQ ( string ( "path" ),  m . get_aws_profile() );
    REQUIRE ( m . get_config_changed () );
}

FIXTURE_TEST_CASE( noCommit, VdbModelFixture )
{
    bool saved = vdbconf_model( kfg ) . is_http_proxy_enabled ();

    {
        CKConfig konfig;
        vdbconf_model model ( konfig );
        model . set_http_proxy_enabled ( ! saved );
        // no commit
    }

    {   // did not change on disk
        CKConfig konfig;
        vdbconf_model model ( konfig );
        REQUIRE_EQ ( saved, model . is_http_proxy_enabled() );
    }
}

FIXTURE_TEST_CASE( Commit, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    bool saved = m . has_http_proxy_env_higher_priority ();

    {
        CKConfig konfig;
        vdbconf_model model ( konfig );
        model . set_http_proxy_env_higher_priority ( ! saved );
        REQUIRE ( model . commit () );
        REQUIRE ( ! model . get_config_changed () );
    }

    {   // did change on disk
        CKConfig konfig;
        vdbconf_model model ( konfig );
        REQUIRE_EQ ( ! saved, model . has_http_proxy_env_higher_priority() );
    }

    // clean up
    m . set_http_proxy_env_higher_priority ( saved );
    REQUIRE ( m . commit () );
}

FIXTURE_TEST_CASE( Reload, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    bool saved = m . has_http_proxy_env_higher_priority ();
    m . set_http_proxy_env_higher_priority ( ! saved );
    REQUIRE_EQ ( ! saved, m . has_http_proxy_env_higher_priority() );
    REQUIRE ( m . get_config_changed () );

    m.reload();

    REQUIRE ( ! m . get_config_changed () );
    REQUIRE_EQ ( saved, m . has_http_proxy_env_higher_priority() );
}

//////////////////////////////////////////// Main
extern "C"
int main( int argc, char *argv [] )
{
    // do not disable user settings as we need to update them as part of the testing
    //KConfigDisableUserSettings();

    return VdbConfStridesModelTestSuite(argc, argv);
}
