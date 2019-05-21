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

#include "../../tools/vdb-config/vdb-config-model.cpp"

TEST_SUITE(VdbConfStridesModelTestSuite);

class VdbModelFixture
{
public:
    VdbModelFixture()
    : kfg ( nullptr )
    {
        KDirectory* wd;
        THROW_ON_RC ( KDirectoryNativeDir ( & wd ) );

        THROW_ON_RC ( KConfigMakeLocal ( & kfg, nullptr ) );

        THROW_ON_RC ( KDirectoryRelease ( wd ) );
    }
    ~VdbModelFixture()
    {
        KConfigRelease ( kfg );
    }

    struct KConfig * kfg;
};

FIXTURE_TEST_CASE( Ctor_Dtor, VdbModelFixture )
{
    vdbconf_model m ( kfg );
}

FIXTURE_TEST_CASE( prefetch_download_to_cache, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE ( m . does_prefetch_download_to_cache() ); // true by default
    m . set_prefetch_download_to_cache( false );
    REQUIRE ( ! m . does_prefetch_download_to_cache() );
}

FIXTURE_TEST_CASE( user_accept_aws_charges, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE ( ! m . does_user_accept_aws_charges() ); // false by default
    m . set_user_accept_aws_charges( true );
    REQUIRE ( m . does_user_accept_aws_charges() );
}

FIXTURE_TEST_CASE( user_accept_gcp_charges, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE ( ! m . does_user_accept_gcp_charges() ); // false by default
    m . set_user_accept_gcp_charges( true );
    REQUIRE ( m . does_user_accept_gcp_charges() );
}

FIXTURE_TEST_CASE( temp_cache_location, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string(), m . get_temp_cache_location() );
    m . set_temp_cache_location( "path" );
    REQUIRE_EQ ( string ( "path" ),  m . get_temp_cache_location() );
}

FIXTURE_TEST_CASE( gcp_credential_file_location, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string(), m . get_gcp_credential_file_location() );
    m . set_gcp_credential_file_location( "path" );
    REQUIRE_EQ ( string ( "path" ),  m . get_gcp_credential_file_location() );
}

FIXTURE_TEST_CASE( aws_credential_file_location, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string(), m . get_aws_credential_file_location() );
    m . set_aws_credential_file_location( "path" );
    REQUIRE_EQ ( string ( "path" ),  m . get_aws_credential_file_location() );
}

FIXTURE_TEST_CASE( aws_profile, VdbModelFixture )
{
    vdbconf_model m ( kfg );
    REQUIRE_EQ ( string(), m . get_aws_profile() );
    m . set_aws_profile( "path" );
    REQUIRE_EQ ( string ( "path" ),  m . get_aws_profile() );
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kapp/args.h>
#include <kfg/config.h>

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

rc_t CC Usage ( const Args * args )
{
    return 0;
}

const char UsageDefaultName[] = "wb-test-fastq";

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    rc_t rc=VdbConfStridesModelTestSuite(argc, argv);
    return rc;
}

}
