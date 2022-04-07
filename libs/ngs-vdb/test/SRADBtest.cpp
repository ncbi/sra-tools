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
* Unit tests for NGS interface, SRA DB based implementation
*/

#include "ngsfixture.hpp"

#include <ngs/Statistics.hpp>

#include <klib/text.h>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsSradbCppTestSuite);

class SRADBFixture : public NgsFixture
{
public:
    static const char* SRADB_Accession;

public:
    SRADBFixture()
    {
    }
    ~SRADBFixture()
    {
    }

    ngs :: ReadIterator getReads ( ngs :: Read :: ReadCategory cat = ngs :: Read :: all )
    {
        return NgsFixture :: getReads ( SRADB_Accession, cat );
    }
    ngs :: Read getRead ( const ngs :: String& p_id)
    {
        return NgsFixture :: getRead ( SRADB_Accession, p_id );
    }
    ngs :: Read getFragment (const ngs :: String& p_readId, uint32_t p_fragIdx)
    {
        return NgsFixture :: getFragment ( SRADB_Accession, p_readId, p_fragIdx );
    }
    ngs :: Reference getReference ( const char* spec )
    {
        return NgsFixture :: getReference ( SRADB_Accession, spec );
    }
};
const char* SRADBFixture::SRADB_Accession = "SRR600096";

///// ReadCollection
//TODO: getReadRange (categories)
//TODO: getReadCount (categories)
//TODO: getReadGroups
//TODO: error cases


FIXTURE_TEST_CASE(SRADB_ReadCollection_GetName, SRADBFixture)
{
    ngs::String name1, name2;
    {
        ngs::ReadCollection run = open ("data/SRR600096");
        name1 = run.getName();
        char const* pNoSlash = string_rchr( name1.c_str(), name1.length(), '/' );
        REQUIRE ( pNoSlash == NULL );
    }
    {
        ngs::ReadCollection run = open ("SRR600096");
        name2 = run.getName();
    }
    REQUIRE_EQ ( name1, name2 );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_Open, SRADBFixture)
{
    ngs :: ReadCollection run = open ( SRADB_Accession );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_Open_Failed, SRADBFixture)
{
    REQUIRE_THROW ( ngs :: ReadCollection run = open ( "idonotopen" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads, SRADBFixture)
{
    ngs :: ReadIterator readIt = open ( SRADB_Accession ) . getReads ( ngs :: Read :: all );
}
FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads_Filtered, SRADBFixture)
{
    ngs :: ReadIterator readIt = open ( SRADB_Accession ) . getReads ( ngs :: Read :: unaligned );
}
FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReadRange, SRADBFixture)
{
    ngs :: ReadIterator readIt = open ( SRADB_Accession ) . getReadRange ( 100, 200 );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetRead, SRADBFixture)
{
    ngs :: Read read = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetRead_Failed, SRADBFixture)
{
    REQUIRE_THROW ( ngs :: Read read = open ( SRADB_Accession ) . getRead ( "notthere" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReference_Failed, SRADBFixture)
{
    REQUIRE_THROW ( ngs :: Reference ref = getReference ( "anything" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_ReadCount, SRADBFixture)
{
    ngs :: ReadCollection run = open ( SRADB_Accession );
    REQUIRE_EQ( (uint64_t)16, run . getReadCount() );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_Getreferences, SRADBFixture)
{
    ngs :: ReferenceIterator refIt = NgsFixture :: getReferences ( SRADB_Accession );  // always an empty iterator
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetAlignment, SRADBFixture)
{
    ngs :: ReadCollection run = open ( SRADB_Accession );
    REQUIRE_THROW ( ngs :: Alignment align = run . getAlignment ( "1" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetAlignmentCount, SRADBFixture)
{
    ngs :: ReadCollection run = open ( SRADB_Accession );
    REQUIRE_EQ( (uint64_t)0, run . getAlignmentCount() );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetAlignmentCountFiltered, SRADBFixture)
{
    ngs :: ReadCollection run = open ( SRADB_Accession );
    REQUIRE_EQ( (uint64_t)0, run . getAlignmentCount( ngs :: Alignment :: all ) );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetAlignmentRange, SRADBFixture)
{
    ngs :: ReadCollection run = open ( SRADB_Accession );
    REQUIRE ( ! run . getAlignmentRange ( 1, 2 ) . nextAlignment () );
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetAlignmentRangeFiltered, SRADBFixture)
{
    ngs :: ReadCollection run = open ( SRADB_Accession );
    REQUIRE ( ! run . getAlignmentRange ( 1, 2, ngs :: Alignment :: all ) . nextAlignment () );
}

///// Read
//TODO: error cases

FIXTURE_TEST_CASE(SRADB_Read_ReadId, SRADBFixture)
{
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.1", getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadId() . toString () );
}

FIXTURE_TEST_CASE(SRADB_Read_ReadName, SRADBFixture)
{
    REQUIRE_EQ( string ( "1" ), getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadName() . toString() );
}

FIXTURE_TEST_CASE(SRADB_Read_ReadGroup, SRADBFixture)
{
    REQUIRE_EQ( ngs :: String("A1DLC.1"), getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ). getReadGroup() );
}

FIXTURE_TEST_CASE(SRADB_Read_getNumFragments, SRADBFixture)
{
    REQUIRE_EQ( ( uint32_t ) 2, getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getNumFragments() );
}

FIXTURE_TEST_CASE(SRADB_Read_ReadCategory, SRADBFixture)
{
    REQUIRE_EQ( ngs :: Read :: unaligned, getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ). getReadCategory() );
}

FIXTURE_TEST_CASE(SRADB_Read_getReadBases, SRADBFixture)
{
    ngs :: String bases = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadBases () . toString ();
    ngs :: String expected(
        "TACGGAGGGGGCTAGCGTTGCTCGGAATTACTGGGCGTAAAGGGCGCGTAGGCGGACAGTTAAGTCGGGGGTGAAAGCCCCGGGCTCAACCTCGGAATTG"
        "CCTTCGATACTGGCTGGCTTGAGTACGGTAGAGGGGGGTGGAACTCCTAGTGTAGAGGTGAAATTCGTAGAGATTCCTGTTTGCTCCCCACGCTTTCGCG"
        "CCTCAGCGTCAGTAACGGTCCAGTGTGTCGCCTTCGCCACTGGTACTCTTCCTGCTATCTACGCATCTCATTCTACACACGTCGCGCGCCACACCTCTCT"
        "AACACACGTGACACAGCCACTCTCTGCCGTTACTTCGCTGCTCTGCCGCC"
    );
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(SRADB_Read_getSubReadBases_Offset, SRADBFixture)
{
    ngs :: String bases = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadBases ( 300 ) . toString( );
    ngs :: String expected("AACACACGTGACACAGCCACTCTCTGCCGTTACTTCGCTGCTCTGCCGCC");
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(SRADB_Read_getSubReadBases_OffsetLength, SRADBFixture)
{
    ngs :: String bases = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadBases ( 300, 6 ) . toString ();
    ngs :: String expected("AACACA");
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(SRADB_Read_getReadQualities, SRADBFixture)
{
    ngs :: String quals = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadQualities() . toString ();
    ngs :: String expected(
        "====,<==@7<@<@@@CEE=CCEECCEEEEFFFFEECCCEEF=EEDDCCDCEEDDDDDEEEEDDEE*=))9;;=EE(;?E(4;<<;<<EEEE;--'9<;?"
        "EEE=;E=EE<E=EE<9(9EE;(6<?E#################################################?????BBBBDDDDDBBDEEFFFEE7"
        "CHH58E=EECCEG///75,5CF-5A5-5C@FEEDFDEE:E--55----5,@@@,,5,5@=?7?#####################################"
        "##################################################"
    );
    REQUIRE_EQ( expected, quals );
}

FIXTURE_TEST_CASE(SRADB_Read_getSubReadQualities_Offset, SRADBFixture)
{
    ngs :: String quals = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadQualities( 300 ) . toString ();
    ngs :: String expected("##################################################");
    REQUIRE_EQ( expected, quals );
}

FIXTURE_TEST_CASE(SRADB_Read_getSubReadQualities_OffsetLength, SRADBFixture)
{
    ngs :: String quals = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" ) . getReadQualities( 200, 10 ) . toString ();
    ngs :: String expected("CHH58E=EEC");
    REQUIRE_EQ( expected, quals );
}

///// ReadIterator
//TODO: read category selection
//TODO: range on a collection that represents a slice (read[0].id != 1)
//TODO: range and filtering
//TODO: empty range
//TODO: ReadIterator over a ReadGroup (use a ReadGroup that is not immediately at the beginning of the run)
//TODO: ReadIterator over a ReadIterator, to allow creation of a sub-iterator
//TODO: ReadIterator returning less than the range requested
//TODO: error cases (?)
FIXTURE_TEST_CASE(SRADB_ReadIterator_NoReadBeforeNext, SRADBFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    // accessing the read through an iterator before a call to nextRead() throws
    REQUIRE_THROW ( readIt . getReadId() );
}

FIXTURE_TEST_CASE(SRADB_ReadIterator_Open_All, SRADBFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    REQUIRE( readIt . nextRead () );

    REQUIRE_EQ( ngs :: String ( ngs :: String ( SRADB_Accession ) + ".R.1" ), readIt . getReadId() . toString() );
}

#if SHOW_UNIMPLEMENTED
FIXTURE_TEST_CASE(SRADB_ReadIterator_Open_Filtered, SRADBFixture)
{
    ngs :: ReadIterator readIt = getReads ( ngs :: Read :: partiallyAligned );
    REQUIRE( readIt . nextRead () );

    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.5", readIt . getReadId() . toString () );
}
#endif

FIXTURE_TEST_CASE(SRADB_ReadIterator_Next, SRADBFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    REQUIRE( readIt . nextRead () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.2", readIt . getReadId() . toString() );
}

FIXTURE_TEST_CASE(SRADB_ReadIterator_End, SRADBFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    ngs :: String lastId;
    while (readIt . nextRead ())
    {
        lastId = readIt . getReadId() . toString ();
    }
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.16", lastId );
}

FIXTURE_TEST_CASE(SRADB_ReadIterator_BeyondEnd, SRADBFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    while (readIt . nextRead ())
    {
    }
    REQUIRE_THROW ( readIt . getReadId() );
}

FIXTURE_TEST_CASE(SRADB_ReadIterator_Range, SRADBFixture)
{
    ngs :: ReadIterator readIt = open ( SRADB_Accession ) . getReadRange ( 10, 5 );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.10", readIt . getReadId() . toString() );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.11", readIt . getReadId() . toString() );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.12", readIt . getReadId() . toString() );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.13", readIt . getReadId() . toString() );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".R.14", readIt . getReadId() . toString() );
    REQUIRE( ! readIt . nextRead () );
}


///// Fragment
//TODO: error cases
FIXTURE_TEST_CASE(SRADB_Fragment_NoFragmentBeforeNext, SRADBFixture)
{
    ngs :: Read read = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" );
    REQUIRE_THROW( read . getFragmentId() );
}

FIXTURE_TEST_CASE(SRADB_Fragment_Id, SRADBFixture)
{
    ngs :: Read read = getRead ( ngs :: String ( SRADB_Accession ) + ".R.1" );
    REQUIRE ( read . nextFragment() );
    REQUIRE_EQ( ngs :: String ( SRADB_Accession ) + ".FR0.1",  read . getFragmentId() . toString () );
}

FIXTURE_TEST_CASE(SRADB_WGS_Fragment_Id, SRADBFixture)
{
    ngs :: Read read = NgsFixture :: getRead ( "ALWZ01", ngs :: String ( "ALWZ01" ) + ".R.1" );
    REQUIRE ( read . nextFragment() );
    REQUIRE_EQ( ngs :: String ( "ALWZ01" ) + ".FR0.1",  read . getFragmentId() . toString () );
}

FIXTURE_TEST_CASE(SRADB_Fragment_getFragmentBases, SRADBFixture)
{
    ngs :: String expected(
        "CCTGTTTGCTCCCCACGCTTTCGCGCCTCAGCGTCAGTAACGGTCCAGTGTGTCGCCTTCGCCACTGGTACTCTTCCTGCTATCTACGCATCTCATTCTA"
        "CACACGTCGCGCGCCACACCTCTCTAACACACGTGACACAGCCACTCTCTGCCGTTACTTCGCTGCTCTGCCGCC"
    );
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRADB_Accession ) + ".R.1", 2 ) . getFragmentBases () . toString () );
}

FIXTURE_TEST_CASE(SRADB_Fragment_getSubFragmentBases_Offset, SRADBFixture)
{
    ngs :: String expected("CACACGTCGCGCGCCACACCTCTCTAACACACGTGACACAGCCACTCTCTGCCGTTACTTCGCTGCTCTGCCGCC");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRADB_Accession ) + ".R.1", 2 ) . getFragmentBases ( 100 ) . toString () );
}

FIXTURE_TEST_CASE(SRADB_Fragment_getSubFragmentBases_OffsetLength, SRADBFixture)
{
    ngs :: String expected("CACACG");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRADB_Accession ) + ".R.1", 2 ) . getFragmentBases ( 100, 6 ) . toString () );
}

FIXTURE_TEST_CASE(SRADB_Fragment_getFragmentQualities, SRADBFixture)
{
    ngs :: String expected(
        "?????BBBBDDDDDBBDEEFFFEE7CHH58E=EECCEG///75,5CF-5A5-5C@FEEDFDEE:E--55----5,@@@,,5,5@=?7?############"
        "###########################################################################"
    );
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRADB_Accession ) + ".R.1", 2 ) . getFragmentQualities () . toString () );
}

FIXTURE_TEST_CASE(SRADB_Fragment_getSubFragmentQualities_Offset, SRADBFixture)
{
    ngs :: String expected("###########################################################################");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRADB_Accession ) + ".R.1", 2 ) . getFragmentQualities ( 100 ) . toString () );
}

FIXTURE_TEST_CASE(SRADB_Fragment_getSubFragmentQualities_OffsetLength, SRADBFixture)
{
    ngs :: String expected("5,5@=?");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRADB_Accession ) + ".R.1", 2 ) . getFragmentQualities ( 80, 6 ) . toString () );
}

/////ReferenceIterator
FIXTURE_TEST_CASE(SRADB_ReferenceIterator_Open, SRADBFixture)
{
    ngs :: ReferenceIterator refIt = NgsFixture :: getReferences ( SRADB_Accession );  // always an empty iterator
    REQUIRE_THROW ( refIt . getCommonName() );
}

FIXTURE_TEST_CASE(SRADB_ReferenceIterator_Next, SRADBFixture)
{
    ngs :: ReferenceIterator refIt = NgsFixture :: getReferences ( SRADB_Accession );  // always an empty iterator
    REQUIRE( ! refIt . nextReference () );
}

/////AlignmentIterator
FIXTURE_TEST_CASE(SRADB_AlignmentIterator_Open, SRADBFixture)
{
    ngs :: AlignmentIterator alIt = open ( SRADB_Accession ) .  getAlignments ( ngs :: Alignment :: all );  // always an empty iterator
    REQUIRE_THROW ( alIt . getAlignmentId () );
}

FIXTURE_TEST_CASE(SRADB_AlignmentIterator_Next, SRADBFixture)
{
    ngs :: AlignmentIterator alIt = open ( SRADB_Accession ) .  getAlignments ( ngs :: Alignment :: all );  // always an empty iterator
    REQUIRE( ! alIt . nextAlignment () );
}

/////TODO: PileupIterator

///// ReadGroup
FIXTURE_TEST_CASE(SRADB_ReadGroup_GetName, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    REQUIRE_EQ ( ngs :: String("A1DLC.1"), rg . getName () );
}
FIXTURE_TEST_CASE(SRADB_HasReadGroup, SRADBFixture)
{
    REQUIRE ( open( SRADB_Accession ).hasReadGroup ( "A1DLC.1" ) );
    REQUIRE ( ! open( SRADB_Accession ).hasReadGroup ( "non-existent read group" ) );
}
FIXTURE_TEST_CASE(SRADB_ReadGroup_GetStatistics, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    ngs :: Statistics stats = rg . getStatistics ();
}

/////TODO: ReadGroupIterator

///// Statistics
FIXTURE_TEST_CASE(SRADB_ReadGroup_GetStatistics_getValueType, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    ngs :: Statistics stats = rg . getStatistics ();

    REQUIRE_EQ ( ngs :: Statistics :: uint64, stats . getValueType ( "BASE_COUNT" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadGroup_GetStatistics_getAsU64, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    ngs :: Statistics stats = rg . getStatistics ();

    REQUIRE_EQ ( (uint64_t)5600, stats . getAsU64 ( "BASE_COUNT" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadGroup_GetStatistics_getAsI64, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    ngs :: Statistics stats = rg . getStatistics ();

    REQUIRE_EQ ( (int64_t)5600, stats . getAsI64 ( "BASE_COUNT" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadGroup_GetStatistics_getAsDouble, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    ngs :: Statistics stats = rg . getStatistics ();

    REQUIRE_EQ ( 5600.0, stats . getAsDouble ( "BASE_COUNT" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadGroup_GetStatistics_nextPath, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    ngs :: Statistics stats = rg . getStatistics ();

    REQUIRE_EQ ( string("BIO_BASE_COUNT"), stats . nextPath ( "BASE_COUNT" ) );
}

FIXTURE_TEST_CASE(SRADB_ReadGroup_GetStatistics_iteration, SRADBFixture)
{
    ngs :: ReadGroup rg = open ( SRADB_Accession ) . getReadGroup ( "A1DLC.1" );
    ngs :: Statistics stats = rg . getStatistics ();

	string path = stats . nextPath ( "" );
    REQUIRE_EQ ( string("BASE_COUNT"), path );
	path = stats . nextPath ( path );
    REQUIRE_EQ ( string("BIO_BASE_COUNT"), path );
	path = stats . nextPath ( path );
    REQUIRE_EQ ( string("SPOT_COUNT"), path );
	path = stats . nextPath ( path );
    REQUIRE_EQ ( string("SPOT_MAX"), path );
	path = stats . nextPath ( path );
    REQUIRE_EQ ( string("SPOT_MIN"), path );
	path = stats . nextPath ( path );
    REQUIRE_EQ ( string(), path );
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kapp/args.h>

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

const char UsageDefaultName[] = "test-ngs_sradb-c++";

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    rc_t rc=NgsSradbCppTestSuite(argc, argv);
    NgsFixture::ReleaseCache();
    return rc;
}

}


