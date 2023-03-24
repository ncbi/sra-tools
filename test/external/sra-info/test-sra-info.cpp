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

#include "formatter.cpp"
#include "sra-info.cpp"

#include <ktst/unit_test.hpp>

#include <klib/out.h>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(SraInfoTestSuite);

const string Accession_Table = "SRR000123";
const string Accession_CSRA = "ERR334733";
const string Run_Multiplatform = "./input/MultiPlatform";

TEST_CASE(Construction)
{
    SraInfo info;
}

class SraInfoFixture
{
protected:
    SraInfo info;
};

FIXTURE_TEST_CASE(SetAccession, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    REQUIRE_EQ( Accession_Table, info.GetAccession() );
}

FIXTURE_TEST_CASE(ResetAccession, SraInfoFixture)
{
    const string Accession2 = "SRR000124";
    info.SetAccession(Accession_Table);
    info.SetAccession(Accession2);
    REQUIRE_EQ( string(Accession2), info.GetAccession() );
}

// Platform

FIXTURE_TEST_CASE(PlatformInInvalidAccession, SraInfoFixture)
{
    const string Accession = "i_am_groot";
    info.SetAccession(Accession);
    REQUIRE_THROW( info.GetPlatforms() );
}

FIXTURE_TEST_CASE(NoPlatformInTable, SraInfoFixture)
{
    const string Accession = "NC_000001.10";
    info.SetAccession(Accession);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(0), p.size() );
}

// do such runs exist?
// FIXTURE_TEST_CASE(NoPlatformInDatabase, SraInfoFixture)
// {
// }

FIXTURE_TEST_CASE(SinglePlatformInTable, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(1), p.size() );
    REQUIRE_EQ( string("SRA_PLATFORM_454"), *p.begin() );
}
FIXTURE_TEST_CASE(SinglePlatformInDatabase, SraInfoFixture)
{
    info.SetAccession(Accession_CSRA);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(1), p.size() );
    REQUIRE_EQ( string("SRA_PLATFORM_ILLUMINA"), *p.begin() );
}

FIXTURE_TEST_CASE(MultiplePlatforms, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(3), p.size() );
    REQUIRE( p.end() != p.find("SRA_PLATFORM_UNDEFINED") );
    REQUIRE( p.end() != p.find("SRA_PLATFORM_ILLUMINA") );
    REQUIRE( p.end() != p.find("SRA_PLATFORM_454") );
}

// SpotLayout

FIXTURE_TEST_CASE(SpotLayout_SingleRow, SraInfoFixture)
{
    info.SetAccession(Accession_CSRA);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts();
    REQUIRE_EQ( size_t(1), sl.size() );
    REQUIRE_EQ( uint64_t(1), sl[0].count );
    REQUIRE_EQ( size_t(1), sl[0].reads.size() );
    REQUIRE_EQ( string("BIOLOGICAL|REVERSE"), sl[0].reads[0].type );
    REQUIRE_EQ( uint32_t(50), sl[0].reads[0].length );
}

FIXTURE_TEST_CASE(SpotLayout_MultiRow, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
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
        REQUIRE_EQ( string("TECHNICAL"), l.reads[0].type );
        REQUIRE_EQ( uint32_t(4), l.reads[0].length );
        REQUIRE_EQ( string("BIOLOGICAL"), l.reads[1].type );
        REQUIRE_EQ( uint32_t(259), l.reads[1].length );
    }

    {
        class SraInfo::SpotLayout & l = sl[1];
        REQUIRE_EQ( uint64_t(112), l.count );
        REQUIRE_EQ( size_t(2), l.reads.size() );
        REQUIRE_EQ( string("TECHNICAL"), l.reads[0].type );
        REQUIRE_EQ( uint32_t(4), l.reads[0].length );
        REQUIRE_EQ( string("BIOLOGICAL"), l.reads[1].type );
        REQUIRE_EQ( uint32_t(256), l.reads[1].length );
    }
    //etc

}

// Formatting
FIXTURE_TEST_CASE(Format_values, SraInfoFixture)
{   // case insensitive
    REQUIRE_EQ( Formatter::CSV,     Formatter::StringToFormat("cSv") );
    REQUIRE_EQ( Formatter::XML,     Formatter::StringToFormat("Xml") );
    REQUIRE_EQ( Formatter::Json,    Formatter::StringToFormat("jSON") );
    REQUIRE_EQ( Formatter::Piped,   Formatter::StringToFormat("piped") );
    REQUIRE_EQ( Formatter::Tab,     Formatter::StringToFormat("TAB") );
    REQUIRE_THROW( Formatter::StringToFormat("somethingelse") );
}

FIXTURE_TEST_CASE(Format_Platforms_Default, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    Formatter f; // default= plain
    string out = f.format( p );
    // one value per line, sorted
    REQUIRE_EQ( string("SRA_PLATFORM_454\nSRA_PLATFORM_ILLUMINA\nSRA_PLATFORM_UNDEFINED"), out );
}

FIXTURE_TEST_CASE(Format_Platforms_CSV, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    Formatter f( Formatter::CSV );
    string out = f.format( p );
    // one value per line, sorted
    REQUIRE_EQ( string("SRA_PLATFORM_454,SRA_PLATFORM_ILLUMINA,SRA_PLATFORM_UNDEFINED"), out );
}

FIXTURE_TEST_CASE(Format_Platforms_XML, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    Formatter f( Formatter::XML );
    string out = f.format( p );
    // one value per line, sorted
    REQUIRE_EQ( string("<platform>SRA_PLATFORM_454</platform>\n"
                       "<platform>SRA_PLATFORM_ILLUMINA</platform>\n"
                       "<platform>SRA_PLATFORM_UNDEFINED</platform>"), out );
}

FIXTURE_TEST_CASE(Format_Platforms_Json, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    Formatter f( Formatter::Json );
    string out = f.format( p );
    // one value per line, sorted
    REQUIRE_EQ( string("[\n"
                       "\"SRA_PLATFORM_454\",\n"
                       "\"SRA_PLATFORM_ILLUMINA\",\n"
                       "\"SRA_PLATFORM_UNDEFINED\"\n"
                       "]"), out );
}

FIXTURE_TEST_CASE(Format_Platforms_Piped, SraInfoFixture)
{   // same as default
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    Formatter f( Formatter::Piped );
    string out = f.format( p );
    REQUIRE_EQ( string("SRA_PLATFORM_454\nSRA_PLATFORM_ILLUMINA\nSRA_PLATFORM_UNDEFINED"), out );
}

FIXTURE_TEST_CASE(Format_Platforms_Tab, SraInfoFixture)
{   // same as default
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    Formatter f( Formatter::Tab );
    string out = f.format( p );
    // one line, tab separated
    REQUIRE_EQ( string("SRA_PLATFORM_454\tSRA_PLATFORM_ILLUMINA\tSRA_PLATFORM_UNDEFINED"), out );
}

// IsAligned
FIXTURE_TEST_CASE(IsAligned_No, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    REQUIRE( ! info.IsAligned() );
}
FIXTURE_TEST_CASE(IsAligned_Yes, SraInfoFixture)
{
    info.SetAccession(Accession_CSRA);
    REQUIRE( info.IsAligned() );
}

// HasPhysicalQualities
FIXTURE_TEST_CASE(HasPhysicalQualities_No, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    REQUIRE( ! info.HasPhysicalQualities() );
}
FIXTURE_TEST_CASE(HasPhysicalQualities_Yes, SraInfoFixture)
{
    info.SetAccession(Accession_CSRA);
    REQUIRE( info.HasPhysicalQualities() );
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
