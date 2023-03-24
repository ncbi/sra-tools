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
* Unit tests for class SraInfo
*/

#include "sra-info.cpp"

#include <ktst/unit_test.hpp>

#include <klib/out.h>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(SraInfoTestSuite);

TEST_CASE(Construction)
{
    SraInfo info;
}

TEST_CASE(SetAccession)
{
    SraInfo info;
    const string Accession = "SRR000123";
    info.SetAccession(Accession);
    REQUIRE_EQ( string(Accession), info.GetAccession() );
}

TEST_CASE(ResetAccession)
{
    SraInfo info;
    const string Accession1 = "SRR000123";
    const string Accession2 = "SRR000124";
    info.SetAccession(Accession1);
    info.SetAccession(Accession2);
    REQUIRE_EQ( string(Accession2), info.GetAccession() );
}

// Platform

TEST_CASE(PlatformInInvalidAccession)
{
    SraInfo info;
    const string Accession = "i_am_groot";
    info.SetAccession(Accession);
    REQUIRE_THROW( info.GetPlatforms() );
}

TEST_CASE(NoPlatformInTable)
{
    SraInfo info;
    const string Accession = "NC_000001.10";
    info.SetAccession(Accession);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(0), p.size() );
}
TEST_CASE(NoPlatformInDatabase) // ?
{//TODO
}

TEST_CASE(SinglePlatformInTable)
{
    SraInfo info;
    const string Accession = "SRR000123";
    info.SetAccession(Accession);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(1), p.size() );
    REQUIRE_EQ( string("SRA_PLATFORM_454"), *p.begin() );
}
TEST_CASE(SinglePlatformInDatabase)
{
    SraInfo info;
    const string Accession = "ERR334733";
    info.SetAccession(Accession);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(1), p.size() );
    REQUIRE_EQ( string("SRA_PLATFORM_ILLUMINA"), *p.begin() );
}

TEST_CASE(MultiplePlatforms)
{
    SraInfo info;
    const string Accession = "./input/MultiPlatform";
    info.SetAccession(Accession);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(3), p.size() );
    REQUIRE( p.end() != p.find("SRA_PLATFORM_UNDEFINED") );
    REQUIRE( p.end() != p.find("SRA_PLATFORM_ILLUMINA") );
    REQUIRE( p.end() != p.find("SRA_PLATFORM_454") );
}

// SpotLayout

TEST_CASE(SpotLayout_SingleRow)
{
    SraInfo info;
    const string Accession = "ERR334733";
    info.SetAccession(Accession);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts();
    REQUIRE_EQ( size_t(1), sl.size() );
    REQUIRE_EQ( uint64_t(1), sl[0].count );
    REQUIRE_EQ( size_t(1), sl[0].reads.size() );
    REQUIRE_EQ( int(SRA_READ_TYPE_BIOLOGICAL | SRA_READ_TYPE_REVERSE), int(sl[0].reads[0].type) );
    REQUIRE_EQ( uint32_t(50), sl[0].reads[0].length );
}

TEST_CASE(SpotLayout_MultiRow)
{
    SraInfo info;
    const string Accession = "SRR000123";
    info.SetAccession(Accession);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts();
    REQUIRE_EQ( size_t(267), sl.size() );

    // check the total
    size_t total = 0;
    for( auto i : sl )
    {
        total += i.count;
    }
    REQUIRE_EQ( size_t(4583), total );

    // the most popular layouts are at the start of the vector
    {
        class SraInfo::SpotLayout & l = sl[0];
        REQUIRE_EQ( uint64_t(119), l.count );
        REQUIRE_EQ( size_t(2), l.reads.size() );
        REQUIRE_EQ( int(SRA_READ_TYPE_TECHNICAL), int(l.reads[0].type) );
        REQUIRE_EQ( uint32_t(4), l.reads[0].length );
        REQUIRE_EQ( int(SRA_READ_TYPE_BIOLOGICAL), int(l.reads[1].type) );
        REQUIRE_EQ( uint32_t(259), l.reads[1].length );
    }

    {
        class SraInfo::SpotLayout & l = sl[1];
        REQUIRE_EQ( uint64_t(112), l.count );
        REQUIRE_EQ( size_t(2), l.reads.size() );
        REQUIRE_EQ( int(SRA_READ_TYPE_TECHNICAL), int(l.reads[0].type) );
        REQUIRE_EQ( uint32_t(4), l.reads[0].length );
        REQUIRE_EQ( int(SRA_READ_TYPE_BIOLOGICAL), int(l.reads[1].type) );
        REQUIRE_EQ( uint32_t(256), l.reads[1].length );
    }
    //etc

}


//////////////////////////////////////////// Main
#include <kapp/args.h>
#include <kfg/config.h>

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}

const char UsageDefaultName[] = "test-sra-info";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n" "\t%s [options] -o path\n\n", progname );
}

rc_t CC Usage( const Args* args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();

    rc_t rc=SraInfoTestSuite(argc, argv);
    return rc;
}

}
