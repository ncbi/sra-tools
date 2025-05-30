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

const string Accession_Table    = "SRR000123";
const string Accession_CSRA     = "ERR334733";
const string Accession_Refseq   = "NC_000001.10";
const string Accession_WGS      = "AAAAAA01";
const string Run_Multiplatform  = "./input/MultiPlatform.sra";
const string Accession_Pacbio   = "DRR032985";
const string Run_Fingerprints   = "./input/Test_Sra_Info_Fingerprint"; // a copy of db created by sharq for test cases DbMetaMultipleFiles_*

TEST_CASE(Construction)
{
    SraInfo info;
}

class SraInfoFixture
{
protected:
    string
    FormatSpotLayout( const string & accession, SraInfo::Detail det, Formatter::Format fmt )
    {
        info.SetAccession(accession);
        SraInfo::SpotLayouts sl = info.GetSpotLayouts( det );
        const Formatter f( fmt, 2 );
        return f.format( sl, det );
    }

    void verifyFingerprint( const SraInfo::Fingerprints::value_type & object, const string& exp_key, const string& exp_value )
    {
        if ( exp_key != object.key || exp_value != object.value )
        {
            cerr << "SraInfoFixture::verifyFingerprint() expected " << exp_key << " " << exp_value << " got " << object.key << " " << object.value << endl;
            THROW_ON_FALSE( false );
        }
    }

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

FIXTURE_TEST_CASE(SetInvalidAccession, SraInfoFixture)
{
    const string Accession = "i_am_groot";
    REQUIRE_THROW( info.SetAccession(Accession) );
}

// Platform

FIXTURE_TEST_CASE(NoPlatformInTable, SraInfoFixture)
{
    info.SetAccession(Accession_Refseq);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(1), p.size() );
    REQUIRE_EQ( string("SRA_PLATFORM_UNDEFINED"), *p.begin() );
}

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
FIXTURE_TEST_CASE(SinglePlatformWGS, SraInfoFixture)
{   // no PLATFORM column
    info.SetAccession(Accession_WGS);
    SraInfo::Platforms p = info.GetPlatforms();
    REQUIRE_EQ( size_t(1), p.size() );
    REQUIRE_EQ( string("none provided"), *p.begin() );
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

// Formatting
FIXTURE_TEST_CASE(Format_values, SraInfoFixture)
{   // case insensitive
    REQUIRE_EQ( Formatter::CSV,     Formatter::StringToFormat("cSv") );
    REQUIRE_EQ( Formatter::XML,     Formatter::StringToFormat("Xml") );
    REQUIRE_EQ( Formatter::Json,    Formatter::StringToFormat("jSON") );
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
    REQUIRE_EQ( string("PLATFORM: SRA_PLATFORM_454\n"
        "PLATFORM: SRA_PLATFORM_ILLUMINA\n"
        "PLATFORM: SRA_PLATFORM_UNDEFINED"), out );
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
    REQUIRE_EQ( string(" <PLATFORMS>\n"
                       "  <platform>SRA_PLATFORM_454</platform>\n"
                       "  <platform>SRA_PLATFORM_ILLUMINA</platform>\n"
                       "  <platform>SRA_PLATFORM_UNDEFINED</platform>\n"
                       " </PLATFORMS>"), out );
}

FIXTURE_TEST_CASE(Format_Platforms_Json, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    SraInfo::Platforms p = info.GetPlatforms();
    Formatter f( Formatter::Json );
    string out = f.format( p );
    // one value per line, sorted
    REQUIRE_EQ( string(" \"PLATFORMS\": [\n"
                       "  \"SRA_PLATFORM_454\",\n"
                       "  \"SRA_PLATFORM_ILLUMINA\",\n"
                       "  \"SRA_PLATFORM_UNDEFINED\"\n"
                       " ]"), out );
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

FIXTURE_TEST_CASE(Format_MultiRow_Limited, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    const SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Verbose );
    REQUIRE_EQ( size_t(267), sl.size() );
    const Formatter f( Formatter::Default, 2 ); // 2 top elements
    const string out = f.format( sl, SraInfo::Short );
    const string expected("SPOT: 119 spots: 2 reads\n"
        "SPOT: 112 spots: 2 reads");
    REQUIRE_EQ( expected, out );
}

// SpotLayout
FIXTURE_TEST_CASE(SpotLayout_ReadStructure_Encode_Short, SraInfoFixture)
{   // nothing
    SraInfo::ReadStructure rs( 0, 0 );
    REQUIRE_EQ( string(), rs.Encode( SraInfo::Short ) );
}
FIXTURE_TEST_CASE(SpotLayout_ReadStructure_Encode_Abbreviated, SraInfoFixture)
{   // T or B only, no length
    SraInfo::ReadStructure rs( SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_FORWARD, 1 );
    REQUIRE_EQ( string("T"), rs.Encode( SraInfo::Abbreviated ) );
}
FIXTURE_TEST_CASE(SpotLayout_ReadStructure_Encode_Full, SraInfoFixture)
{
    SraInfo::ReadStructure rs( SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE, 2 );
    REQUIRE_EQ( string("BR2"), rs.Encode( SraInfo::Full ) );
}
FIXTURE_TEST_CASE(SpotLayout_ReadStructure_Encode_Verbose, SraInfoFixture)
{
    SraInfo::ReadStructure rs ( SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE, 2 );
    REQUIRE_EQ( string("BIOLOGICAL|REVERSE(length=2)"), rs.Encode( SraInfo::Verbose ) );
}

FIXTURE_TEST_CASE(SpotLayout_SingleRow, SraInfoFixture)
{
    info.SetAccession(Accession_CSRA);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Verbose );
    REQUIRE_EQ( size_t(1), sl.size() );
    REQUIRE_EQ( uint64_t(1), sl[0].count );
    REQUIRE_EQ( size_t(1), sl[0].reads.size() );
    REQUIRE_EQ( string("BIOLOGICAL|REVERSE"), sl[0].reads[0].TypeAsString() );
    REQUIRE_EQ( uint32_t(50), sl[0].reads[0].length );
}

FIXTURE_TEST_CASE(SpotLayout_UsingConsensus, SraInfoFixture)
{
    info.SetAccession(Accession_Pacbio);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Verbose, true );
    REQUIRE_EQ( size_t(924), sl.size() );
    REQUIRE_EQ( uint64_t(80297), sl[0].count );
    REQUIRE_EQ( size_t(1), sl[0].reads.size() );
    REQUIRE_EQ( string("BIOLOGICAL"), sl[0].reads[0].TypeAsString() );
    REQUIRE_EQ( uint32_t(0), sl[0].reads[0].length );
}
FIXTURE_TEST_CASE(SpotLayout_NotUsingConsensus, SraInfoFixture)
{
    info.SetAccession(Accession_Pacbio);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Verbose, false );
    REQUIRE_EQ( size_t(29146), sl.size() );
    REQUIRE_EQ( uint64_t(47), sl[0].count );
    REQUIRE_EQ( size_t(1), sl[0].reads.size() );
    REQUIRE_EQ( string("BIOLOGICAL"), sl[0].reads[0].TypeAsString() );
    REQUIRE_EQ( uint32_t(378), sl[0].reads[0].length );
}

// SpotLayout, detail levels
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Full, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Full );
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
        SraInfo::SpotLayout & l = sl[0];
        REQUIRE_EQ( uint64_t(119), l.count );
        REQUIRE_EQ( size_t(2), l.reads.size() );
        REQUIRE_EQ( string("TECHNICAL"), l.reads[0].TypeAsString() );
        REQUIRE_EQ( uint32_t(4), l.reads[0].length );
        REQUIRE_EQ( string("BIOLOGICAL"), l.reads[1].TypeAsString() );
        REQUIRE_EQ( uint32_t(259), l.reads[1].length );
    }

    {
        SraInfo::SpotLayout & l = sl[1];
        REQUIRE_EQ( uint64_t(112), l.count );
        REQUIRE_EQ( size_t(2), l.reads.size() );
        REQUIRE_EQ( string("TECHNICAL"), l.reads[0].TypeAsString() );
        REQUIRE_EQ( uint32_t(4), l.reads[0].length );
        REQUIRE_EQ( string("BIOLOGICAL"), l.reads[1].TypeAsString() );
        REQUIRE_EQ( uint32_t(256), l.reads[1].length );
    }
    //etc
}

FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Abbreviated, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Abbreviated ); // ignore read lengths
    REQUIRE_EQ( size_t(1), sl.size() );

    SraInfo::SpotLayout & l = sl[0];
    REQUIRE_EQ( uint64_t(4583), l.count );
    REQUIRE_EQ( size_t(2), l.reads.size() );
    REQUIRE_EQ( string("TECHNICAL"), l.reads[0].TypeAsString() );
    REQUIRE_EQ( uint32_t(0), l.reads[0].length );
    REQUIRE_EQ( string("BIOLOGICAL"), l.reads[1].TypeAsString() );
    REQUIRE_EQ( uint32_t(0), l.reads[1].length );
}

FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Short, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Short ); // ignore read types
    REQUIRE_EQ( size_t(1), sl.size() );

    SraInfo::SpotLayout & l = sl[0];
    REQUIRE_EQ( uint64_t(4583), l.count );
    REQUIRE_EQ( size_t(2), l.reads.size() ); // only the size is used here
    REQUIRE_EQ( string("TECHNICAL"), l.reads[0].TypeAsString() ); // the default
    REQUIRE_EQ( uint32_t(0), l.reads[0].length );
    REQUIRE_EQ( string("TECHNICAL"), l.reads[0].TypeAsString() );
    REQUIRE_EQ( uint32_t(0), l.reads[1].length );
}

// SpotLayout, detail levels vs formatting
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Full_Json, SraInfoFixture)
{
    const string expected(
" \"SPOTS\": [\n"
"  { \"count\": 119, \"reads\": [{ \"type\": \"T\", \"length\": 4 }, { \"type\": \"B\", \"length\": 259 }] },\n"
"  { \"count\": 112, \"reads\": [{ \"type\": \"T\", \"length\": 4 }, { \"type\": \"B\", \"length\": 256 }] }\n"
" ]\n");
    const string out( FormatSpotLayout(
        Accession_Table, SraInfo::Full, Formatter::Json ) );
    REQUIRE_EQ( expected, out );
}

FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Abbreviated_Json, SraInfoFixture)
{
    const string expected(" \"SPOTS\": [\n"
        "  { \"count\": 4583, \"reads\": \"TB\" }\n ]\n");
    const string out( FormatSpotLayout(
        Accession_Table, SraInfo::Abbreviated, Formatter::Json ) );
    REQUIRE_EQ( expected, out );
}
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Short_Json, SraInfoFixture)
{
    const string expected(" \"SPOTS\": [\n"
        "  { \"count\": 4583, \"reads\": 2 }\n ]\n");
    const string out( FormatSpotLayout(
        Accession_Table, SraInfo::Short, Formatter::Json ) );
    REQUIRE_EQ( expected, out );
}

FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Full_CSV, SraInfoFixture)
{
    const string expected("119, T, 4, B, 259, \n112, T, 4, B, 256, \n");
    REQUIRE_EQ( expected, FormatSpotLayout( Accession_Table, SraInfo::Full, Formatter::CSV ) );
}
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Abbreviated_CSV, SraInfoFixture)
{
    const string expected("4583, TB\n");
    REQUIRE_EQ( expected, FormatSpotLayout( Accession_Table, SraInfo::Abbreviated, Formatter::CSV ) );
}
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Short_CSV, SraInfoFixture)
{
    const string expected("4583, 2, \n");
    REQUIRE_EQ( expected, FormatSpotLayout( Accession_Table, SraInfo::Short, Formatter::CSV ) );
}

FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Full_Tab, SraInfoFixture)
{
    const string expected("119\tT\t4\tB\t259\t\n112\tT\t4\tB\t256\t\n");
    REQUIRE_EQ( expected, FormatSpotLayout( Accession_Table, SraInfo::Full, Formatter::Tab ) );
}
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Abbreviated_Tab, SraInfoFixture)
{
    const string expected("4583\tTB\n");
    REQUIRE_EQ( expected, FormatSpotLayout( Accession_Table, SraInfo::Abbreviated, Formatter::Tab ) );
}
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Short_Tab, SraInfoFixture)
{
    const string expected("4583\t2\t\n");
    REQUIRE_EQ( expected, FormatSpotLayout( Accession_Table, SraInfo::Short, Formatter::Tab ) );
}

FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Full_XML, SraInfoFixture)
{
    const string expected(
" <SPOTS>\n"
"  <layout><count>119</count><read><type>T</type><length>4</length></read><read><type>B</type><length>259</length></read></layout>\n"
"  <layout><count>112</count><read><type>T</type><length>4</length></read><read><type>B</type><length>256</length></read></layout>\n"
" </SPOTS>");
    const string out( FormatSpotLayout(
        Accession_Table, SraInfo::Full, Formatter::XML ) );
    REQUIRE_EQ( expected, out );
}
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Abbreviated_XML, SraInfoFixture)
{
    const string expected(" <SPOTS>\n"
        "  <layout><count>4583</count><reads>TB</reads></layout>\n"
        " </SPOTS>");
    const string out( FormatSpotLayout(
        Accession_Table, SraInfo::Abbreviated, Formatter::XML ) );
    REQUIRE_EQ( expected, out );
}
FIXTURE_TEST_CASE(SpotLayout_MultiRow_Detail_Short_XML, SraInfoFixture)
{
    const string expected(" <SPOTS>\n"
        "  <layout><count>4583</count><reads>2</reads></layout>\n"
        " </SPOTS>");
    const string out( FormatSpotLayout(
        Accession_Table, SraInfo::Short, Formatter::XML ) );
    REQUIRE_EQ( expected, out );
}

// SpotLayout, limited to N top rows
FIXTURE_TEST_CASE(SpotLayout_TopRows, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    const uint64_t TOP_ROWS = 5;
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Full, true, TOP_ROWS );
    REQUIRE_EQ( size_t( TOP_ROWS ), sl.size() );

    // the top 5 rows are all different layouts
    {
        SraInfo::SpotLayout & l = sl[0];
        REQUIRE_EQ( uint64_t(1), l.count );
        REQUIRE_EQ( size_t(2), l.reads.size() );
        REQUIRE_EQ( string("TECHNICAL"), l.reads[0].TypeAsString() );
        REQUIRE_EQ( uint32_t(4), l.reads[0].length );
        REQUIRE_EQ( string("BIOLOGICAL"), l.reads[1].TypeAsString() );
        REQUIRE_EQ( uint32_t(297), l.reads[1].length );
    }
    //etc
}

FIXTURE_TEST_CASE(SpotLayout_TopRows_MoreThanPresent, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    const uint64_t TOTAL_ROWS = 4583;
    const uint64_t TOP_ROWS = TOTAL_ROWS + 10000;
    SraInfo::SpotLayouts sl = info.GetSpotLayouts( SraInfo::Full, true, TOP_ROWS );
    // check the total
    size_t total = 0;
    for( auto i : sl )
    {
        total += i.count;
    }
    REQUIRE_EQ( size_t( TOTAL_ROWS ), total );
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
    info.SetAccession(Accession_Refseq);
    REQUIRE( ! info.HasPhysicalQualities() );
}
FIXTURE_TEST_CASE(HasPhysicalQualities_Yes, SraInfoFixture)
{
    info.SetAccession(Accession_CSRA);
    REQUIRE( info.HasPhysicalQualities() );
}
FIXTURE_TEST_CASE(HasPhysicalQualities_Original, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    REQUIRE( info.HasPhysicalQualities() );
}

// Contents
FIXTURE_TEST_CASE(Contents_Table, SraInfoFixture)
{
    info.SetAccession(Accession_Table);
    SraInfo::Contents cnt = info.GetContents();
    REQUIRE_EQ( Accession_Table, string(cnt -> name) );
    REQUIRE_EQ( (int)kptTable, (int)(cnt -> dbtype) );
    REQUIRE_EQ( 0, (int)(cnt -> fstype) );
    REQUIRE_EQ( cca_HasChecksum_CRC | cca_HasLock | cca_HasMD5_File | cca_HasMetadata | cta_HasIndices,
                (int)(cnt -> attributes) );
    REQUIRE_NULL( cnt -> parent );
    REQUIRE_NULL( cnt -> nextSibling );
    REQUIRE_NULL( cnt -> prevSibling );

    const KDBContents * child1 = cnt -> firstChild;
    REQUIRE_NOT_NULL( child1 );
    REQUIRE_EQ( string("ALTREAD"), string(child1 -> name) );
    REQUIRE_EQ( (int)kptColumn, (int)(child1 -> dbtype) );
    REQUIRE_EQ( (int)kptDir, (int)(child1 -> fstype) );
    REQUIRE_EQ( cca_HasChecksum_CRC | cca_HasMD5_File | cca_HasMetadata,
                (int)(child1 -> attributes) );
    REQUIRE_EQ( (const KDBContents *)cnt.get(), child1 -> parent );
    REQUIRE_NULL( child1 -> firstChild );
    REQUIRE_NULL( child1 -> prevSibling );

    const KDBContents * child2 = child1 -> nextSibling;
    REQUIRE_NOT_NULL( child2 );
    REQUIRE_EQ( string("CLIP_QUALITY_RIGHT"), string(child2 -> name) );
    REQUIRE_EQ( (int)kptColumn, (int)(child2 -> dbtype) );
    REQUIRE_EQ( (int)kptDir, (int)(child2 -> fstype) );
    REQUIRE_EQ( cca_HasChecksum_CRC | cca_HasMD5_File | cca_HasMetadata,
                (int)(child2 -> attributes) );
    REQUIRE_EQ( (const KDBContents *)cnt.get(), child2 -> parent );
    REQUIRE_NULL( child2 -> firstChild );
    REQUIRE_EQ( child1, child2 -> prevSibling );

    //etc.
}

// Contents
FIXTURE_TEST_CASE(Contents_SRA, SraInfoFixture)
{
    info.SetAccession(Accession_CSRA);
    SraInfo::Contents cnt = info.GetContents();
    REQUIRE_EQ( Accession_CSRA, string(cnt -> name) );
    REQUIRE_EQ( (int)kptDatabase, (int)(cnt -> dbtype) );
    REQUIRE_EQ( 0, (int)(cnt -> fstype) ); // correct?
    REQUIRE_EQ( cca_HasChecksum_CRC | cca_HasLock | cca_HasMD5_File | cca_HasMetadata,
                (int)(cnt -> attributes) ); // correct?
    REQUIRE_NULL( cnt -> parent );
    REQUIRE_NULL( cnt -> nextSibling );
    REQUIRE_NULL( cnt -> prevSibling );

    const KDBContents * child1 = cnt -> firstChild;
    REQUIRE_NOT_NULL( child1 );
    REQUIRE_EQ( string("PRIMARY_ALIGNMENT"), string(child1 -> name) );
    //etc.
}

// Fingerprints
FIXTURE_TEST_CASE(Fingerprint_Empty, SraInfoFixture)
{
    info.SetAccession(Run_Multiplatform);
    SraInfo::Fingerprints fp = info.GetFingerprints( SraInfo::Verbose );
    REQUIRE( fp.empty() );
}

FIXTURE_TEST_CASE(Fingerprint_Short, SraInfoFixture)
{   // output fingerprint only
    info.SetAccession(Run_Fingerprints);
    SraInfo::Fingerprints fp = info.GetFingerprints( SraInfo::Short );
    REQUIRE_EQ( 3, (int)fp.size() ); // fingerprint + digest + algorithm
    REQUIRE_EQ( SraInfo::TreeNode::Value, fp[0].type );
    verifyFingerprint(
        fp[0],
        "fingerprint",
        R"({"maximum-position":4,"A":[0,1,1,0,0],"C":[1,1,0,0,0],"G":[1,0,0,1,0],"T":[0,0,1,1,0],"N":[0,0,0,0,0],"EoR":[0,0,0,0,2]})");
    verifyFingerprint(
        fp[1],
        "digest",
        "67e4aef5339fee30de2f22d909494e19cffeefd900ba150bd0ed2ecf187879c5");
    verifyFingerprint(fp[2], "algorithm", "SHA-256");
    }

FIXTURE_TEST_CASE(Fingerprint_Abbreviated, SraInfoFixture)
{   // output fingerprint with history
    info.SetAccession(Run_Fingerprints);
    SraInfo::Fingerprints fp = info.GetFingerprints( SraInfo::Abbreviated );

    REQUIRE_EQ( 7, (int)fp.size() ); // fingerprint, digest, algorithm, timestamp, history, version, format

    verifyFingerprint( fp[3], "timestamp", "1743618335" );
    verifyFingerprint( fp[4], "version", "1.0.0" );
    verifyFingerprint( fp[5], "format", "json utf-8 compact" );

    // history
    const auto & h = fp[6];
    REQUIRE_EQ( SraInfo::TreeNode::Array, h.type );
    verifyFingerprint( h, "history", "" );
    REQUIRE_EQ( 2, (int)h.subnodes.size() ); // 2 history entries

    const auto & h1 = h.subnodes[0];
    REQUIRE_EQ( SraInfo::TreeNode::Element, h1.type );
    verifyFingerprint( h1, "update_1", "" );
    REQUIRE_EQ( 7, (int)h1.subnodes.size() );  // update#, fingerprint, digest, algorithm, timestamp, version, format

    verifyFingerprint( h1.subnodes[0], "update", "1" );
    verifyFingerprint( h1.subnodes[1], "fingerprint", R"({"A":[1],"C":[1],"G":[1],"T":[1],"N":[1],"EoR":[1]})" );
    verifyFingerprint( h1.subnodes[2], "digest", "qwer" );
    verifyFingerprint( h1.subnodes[3], "algorithm", "SHA-256" );
    verifyFingerprint( h1.subnodes[4], "timestamp", "1743618305" );
    verifyFingerprint( h1.subnodes[5], "version", "1.0.0" );
    verifyFingerprint( h1.subnodes[6], "format", "json utf-8 compact" );

    const auto & h2 = h.subnodes[1];
    verifyFingerprint( h2, "update_2", "" );
    REQUIRE_EQ( 7, (int)h2.subnodes.size() );
    verifyFingerprint( h2.subnodes[0], "update", "2" );
    verifyFingerprint( h2.subnodes[1], "fingerprint", R"({"A":[2],"C":[2],"G":[2],"T":[2],"N":[2],"EoR":[2]})" );
    verifyFingerprint( h2.subnodes[2], "digest", "asdf" );
    verifyFingerprint( h2.subnodes[3], "algorithm", "SHA-256" );
    verifyFingerprint( h2.subnodes[4], "timestamp", "1743618339" );
    verifyFingerprint( h2.subnodes[5], "version", "1.0.0" );
    verifyFingerprint( h2.subnodes[6], "format", "json utf-8 compact" );
}

FIXTURE_TEST_CASE(Fingerprint_Full, SraInfoFixture)
{   // output fingerprint with history, input fingerprints
    info.SetAccession(Run_Fingerprints);
    SraInfo::Fingerprints fp = info.GetFingerprints( SraInfo::Full );

    REQUIRE_EQ( 8, (int)fp.size() ); // fingerprint, digest, algorithm, timestamp, history, version, format, inputs

    const auto & ins = fp[7];
    REQUIRE_EQ( SraInfo::TreeNode::Array, ins.type );
    verifyFingerprint( ins, "inputs", "" );
    REQUIRE_EQ( 2, (int)ins.subnodes.size() ); // 2 input files

    // inputs
    const auto & in1 = ins.subnodes[0];

    REQUIRE_EQ( SraInfo::TreeNode::Element, in1.type );
    verifyFingerprint( in1, "file_1", "" );
    REQUIRE_EQ( 7, (int)in1.subnodes.size() ); // file#, fingerprint, digest, algorithm, timestamp, version, format
    verifyFingerprint( in1.subnodes[0], "file", "1" );
    verifyFingerprint( in1.subnodes[1], "name", "input/r1.fastq" );
    verifyFingerprint( in1.subnodes[2], "fingerprint", R"({"maximum-position":4,"A":[0,1,0,0,0],"C":[0,0,0,0,0],"G":[1,0,0,0,0],"T":[0,0,1,1,0],"N":[0,0,0,0,0],"EoR":[0,0,0,0,1]})" );
    verifyFingerprint( in1.subnodes[3], "algorithm", "SHA-256" );
    verifyFingerprint( in1.subnodes[4], "digest", "123" );
    verifyFingerprint( in1.subnodes[5], "version", "1.0.0" );
    verifyFingerprint( in1.subnodes[6], "format", "json utf-8 compact" );

    const auto & in2 = ins.subnodes[1];
    verifyFingerprint( in2, "file_2", "" );
    REQUIRE_EQ( 7, (int)in2.subnodes.size() ); // fingerprint, digest, algorithm, timestamp, version, format
    verifyFingerprint( in2.subnodes[0], "file", "2" );
    verifyFingerprint( in2.subnodes[1], "name", "input/r2.fastq" );
    verifyFingerprint( in2.subnodes[2], "fingerprint", R"({"maximum-position":4,"A":[0,0,1,0,0],"C":[1,1,0,0,0],"G":[0,0,0,1,0],"T":[0,0,0,0,0],"N":[0,0,0,0,0],"EoR":[0,0,0,0,1]})" );
    verifyFingerprint( in2.subnodes[3], "algorithm", "SHA-256" );
    verifyFingerprint( in2.subnodes[4], "digest", "456" );
    verifyFingerprint( in2.subnodes[5], "version", "1.0.0" );
    verifyFingerprint( in2.subnodes[6], "format", "json utf-8 compact" );
}

//////////////////////////////////////////// Main
#include <kapp/args.h>
#include <kfg/config.h>

extern "C"
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return SraInfoTestSuite(argc, argv);
}
