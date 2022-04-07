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
* Unit tests for NGS interface, SRA table based implementation
*/

#include "ngsfixture.hpp"

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsSraCppTestSuite);

class SRAFixture : public NgsFixture
{
public:
    static const char* SRA_Accession;

public:
    SRAFixture()
    {
    }
    ~SRAFixture()
    {
    }

    ngs :: ReadIterator getReads ( ngs :: Read :: ReadCategory cat = ngs :: Read :: all )
    {
        return NgsFixture :: getReads ( SRA_Accession, cat );
    }
    ngs :: Read getRead ( const ngs :: String& p_id)
    {
        return NgsFixture :: getRead ( SRA_Accession, p_id );
    }
    ngs :: Reference getReference ( const char* spec )
    {
        return NgsFixture :: getReference ( SRA_Accession, spec );
    }
    ngs :: Fragment getFragment ( const ngs :: String& p_readId, uint32_t p_fragIdx )
    {
        return NgsFixture :: getFragment ( SRA_Accession, p_readId, p_fragIdx );
    }
};
const char* SRAFixture::SRA_Accession = "SRR000001";

///// ReadCollection
//TODO: getReadRange (categories)
//TODO: getReadCount (categories)
//TODO: getReadGroups
//TODO: error cases

FIXTURE_TEST_CASE(SRA_ReadCollection_Open, SRAFixture)
{
    ngs :: ReadCollection run = open ( SRA_Accession );
}
FIXTURE_TEST_CASE(SRA_ReadCollection_Open_Failed, SRAFixture)
{
    REQUIRE_THROW ( ngs :: ReadCollection run = open ( "idonotopen" ) );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetName, SRAFixture)
{
    REQUIRE_EQ ( ngs :: String ( SRA_Accession ), open ( SRA_Accession ) . getName () );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads, SRAFixture)
{
    ngs :: ReadIterator readIt = open ( SRA_Accession ) . getReads ( ngs :: Read :: all );
}
FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads_Filtered, SRAFixture)
{
    ngs :: ReadIterator readIt = open ( SRA_Accession ) . getReads ( ngs :: Read :: unaligned );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReadRange, SRAFixture)
{
    ngs :: ReadIterator readIt = open ( SRA_Accession ) . getReadRange ( 100, 200 );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetRead, SRAFixture)
{
    ngs :: Read read = getRead ( string ( SRA_Accession ) + ".R.1" );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetRead_Failed, SRAFixture)
{
    REQUIRE_THROW ( ngs :: Read read = open ( SRA_Accession ) . getRead ( "notaread" ) );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_ReadCount, SRAFixture)
{
    ngs :: ReadCollection run = open ( SRA_Accession );
    REQUIRE_EQ( (uint64_t)470985, run . getReadCount() );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReference_Failed, SRAFixture)
{
    REQUIRE_THROW ( ngs :: Reference ref = getReference ( "supercnut2.1" ) );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReferences, SRAFixture)
{
    ngs :: ReferenceIterator refIt = NgsFixture :: getReferences ( SRA_Accession );  // always an empty iterator
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetAlignment, SRAFixture)
{
    ngs :: ReadCollection run = open ( SRA_Accession );
    REQUIRE_THROW ( ngs :: Alignment align = run . getAlignment ( "1" ) );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetAlignmentCount, SRAFixture)
{
    ngs :: ReadCollection run = open ( SRA_Accession );
    REQUIRE_EQ( (uint64_t)0, run . getAlignmentCount() );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetAlignmentCountFiltered, SRAFixture)
{
    ngs :: ReadCollection run = open ( SRA_Accession );
    REQUIRE_EQ( (uint64_t)0, run . getAlignmentCount( ngs :: Alignment :: all ) );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetAlignmentRange, SRAFixture)
{
    ngs :: ReadCollection run = open ( SRA_Accession );
    REQUIRE ( ! run . getAlignmentRange ( 1, 2 ) . nextAlignment () );
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetAlignmentRangeFiltered, SRAFixture)
{
    ngs :: ReadCollection run = open ( SRA_Accession );
    REQUIRE ( ! run . getAlignmentRange ( 1, 2, ngs :: Alignment :: all ) . nextAlignment () );
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
FIXTURE_TEST_CASE(SRA_ReadIterator_NoReadBeforeNext, SRAFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    // accessing the read through an iterator before a call to nextRead() throws
    REQUIRE_THROW ( readIt . getReadId() );
}

FIXTURE_TEST_CASE(SRA_ReadIterator_Open_All, SRAFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    REQUIRE( readIt . nextRead () );
    ngs :: StringRef sref = readIt . getReadId();
    ngs :: String id =  sref . toString ();
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.1", id );
}

#if SHOW_UNIMPLEMENTED
FIXTURE_TEST_CASE(SRA_ReadIterator_Open_Filtered, SRAFixture)
{
    ngs :: ReadIterator readIt = getReads ( ngs :: Read :: partiallyAligned );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( (int64_t)5, readIt . getReadId() );
}
#endif

FIXTURE_TEST_CASE(SRA_ReadIterator_Next, SRAFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    REQUIRE( readIt . nextRead () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.2", readIt . getReadId() . toString () );
}

FIXTURE_TEST_CASE(SRA_ReadIterator_End, SRAFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    ngs :: String lastId;
    while (readIt . nextRead ())
    {
        lastId = readIt . getReadId() . toString ();
    }
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.470985", lastId );
}

FIXTURE_TEST_CASE(SRA_ReadIterator_BeyondEnd, SRAFixture)
{
    ngs :: ReadIterator readIt = getReads ();
    while (readIt . nextRead ())
    {
    }
    REQUIRE_THROW ( readIt . getReadId() );
}

FIXTURE_TEST_CASE(SRA_ReadIterator_Range, SRAFixture)
{
    ngs :: ReadIterator readIt = open ( SRA_Accession ) . getReadRange ( 10, 5 );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.10", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.11", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.12", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.13", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.14", readIt . getReadId() . toString () );
    REQUIRE( ! readIt . nextRead () );
}

/////TODO: Read
//TODO: error cases

FIXTURE_TEST_CASE(SRA_Read_ReadId, SRAFixture)
{
    REQUIRE_EQ( ngs :: String ( SRA_Accession ) + ".R.1", getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadId() . toString () );
}

FIXTURE_TEST_CASE(SRA_Read_getNumFragments, SRAFixture)
{
    REQUIRE_EQ( ( uint32_t ) 2, getRead ( ngs :: String ( SRA_Accession ) + ".R.2" ) . getNumFragments() );
}

FIXTURE_TEST_CASE(SRA_Read_ReadName, SRAFixture)
{
    REQUIRE_EQ( ngs :: String("EM7LVYS02FOYNU"), getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadName() . toString() );
}

FIXTURE_TEST_CASE(SRA_Read_ReadGroup, SRAFixture)
{   //TODO: find an accession with non-empty read groups
    REQUIRE_EQ( ngs :: String(""), getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ). getReadGroup() );
}

FIXTURE_TEST_CASE(SRA_Read_ReadCategory, SRAFixture)
{
    REQUIRE_EQ( ngs :: Read :: unaligned, getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ). getReadCategory() );
}

FIXTURE_TEST_CASE(SRA_Read_getReadBases, SRAFixture)
{
    ngs :: String bases = getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadBases () . toString ();
    ngs :: String expected(
        "TCAGATTCTCCTAGCCTACATCCGTACGAGTTAGCGTGGGATTACGAGGTGCACACCATTTCATTCCGTACGGGTAAATT"
        "TTTGTATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCTGACCTCAAGTGATTTG"
        "CCTGCCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTATAA"
        "ATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG"
    );
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(SRA_Read_getReadBases_Offset, SRAFixture)
{
    ngs :: String bases = getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadBases ( 200 ) . toString( );
    ngs :: String expected(
        "TGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTATAAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG"
    );
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(SRA_Read_getReadBases_OffsetLength, SRAFixture)
{
    ngs :: String bases = getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadBases ( 200, 6 ) . toString ();
    ngs :: String expected("TGAGCG");
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(SRA_Read_getReadQualities, SRAFixture)
{
    ngs :: String quals = getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadQualities() . toString ();
    ngs :: String expected(
        "=<8<85)9=9/3-8?68<7=8<3657747==49==+;FB2;A;5:'*>69<:74)9.;C?+;<B<B;(<';FA/;C>*GC"
        "8/%9<=GC8.#=2:5:16D==<EA2EA.;5=44<;2C=5;@73&<<2;5;6+9<?776+:24'26:7,<9A;=:;0C>*6"
        "?7<<C=D=<52?:9CA2CA23<2<;3CA12:A<9414<7<<6;99<2/=9#<;9B@27.;=6>:77>:1<A>+CA138?<"
        ")C@2166:A<B?->:%<<9<;33<;6?9;<;4=:%<$CA1+1%1");
    REQUIRE_EQ( expected, quals );
}

FIXTURE_TEST_CASE(SRA_Read_getReadQualities_Offset, SRAFixture)
{
    ngs :: String quals = getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadQualities( 200 ) . toString ();
    ngs :: String expected(
        "<6;99<2/=9#<;9B@27.;=6>:77>:1<A>+CA138?<)C@2166:A<B?->:%<<9<;33<;6?9;<;4=:%<$CA1+1%1"
    );
    REQUIRE_EQ( expected, quals );
}

FIXTURE_TEST_CASE(SRA_Read_getReadQualities_OffsetLength, SRAFixture)
{
    ngs :: String quals = getRead ( ngs :: String ( SRA_Accession ) + ".R.1" ) . getReadQualities( 200, 10 ) . toString ();
    ngs :: String expected("<6;99<2/=9");
    REQUIRE_EQ( expected, quals );
}

///// FragmentIterator
FIXTURE_TEST_CASE(SRA_FragmentIterator_Next_and_Beyond, SRAFixture)
{
    ngs :: Read read = getRead ( ngs :: String ( SRA_Accession ) + ".R.2" );
    // 2 fragments total
    REQUIRE ( read . nextFragment () ); // still on first fragment
    REQUIRE ( read . nextFragment () );
    REQUIRE ( ! read . nextFragment () );
    REQUIRE ( ! read . nextFragment () ); // past the end
}

///// Fragment
//TODO: error cases

FIXTURE_TEST_CASE(SRA_Fragment_Id, SRAFixture)
{
    ngs :: Read read = getRead ( ngs :: String ( SRA_Accession ) + ".R.2" );

    REQUIRE ( read . nextFragment () );
    REQUIRE_EQ ( string ( SRA_Accession ) + ".FR0.2", read . getFragmentId() . toString () );

    REQUIRE ( read . nextFragment () );
    REQUIRE_EQ ( string ( SRA_Accession ) + ".FR1.2", read . getFragmentId() . toString () );
}

FIXTURE_TEST_CASE(SRA_Fragment_getFragmentBases, SRAFixture)
{
    ngs :: String expected(
        "ACAGACTCAACCTGCATAATAAATAACATTGAAACTTAGTTTCCTTCTTGGGCTTTCGGTGAGAAAACATAAGTTAAAAC"
        "TGAGCGGGCTGGCAAGGCN"
    );
    REQUIRE_EQ( expected,
                getFragment ( ngs :: String ( SRA_Accession ) + ".R.2", 2 ) . getFragmentBases () . toString ()
    );
}

FIXTURE_TEST_CASE(SRA_Fragment_getSubFragmentBases_Offset, SRAFixture)
{
    ngs :: String expected("GGCAAGGCN");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRA_Accession ) + ".R.2", 2 ) . getFragmentBases ( 90 ) . toString () );
}

FIXTURE_TEST_CASE(SRA_Fragment_getSubFragmentBases_OffsetLength, SRAFixture)
{
    ngs :: String expected("GGCAAG");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRA_Accession ) + ".R.2", 2 ) . getFragmentBases ( 90, 6 ) . toString () );
}

FIXTURE_TEST_CASE(SRA_Fragment_getFragmentQualities, SRAFixture)
{
    ngs :: String expected(
        "<=::=<8=D=C<<<<<<A=;CA1<=7<;A<;CA1<@:<9>;&>7;4<>7CA0<C@0:<5<;:<CA7+:7<@9<B=CA7+<"
        "<99<:B?.<;:2<A<A;:!"
    );
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRA_Accession ) + ".R.2", 2 ) . getFragmentQualities () . toString () );
}

FIXTURE_TEST_CASE(SRA_Fragment_getSubFragmentQualities_Offset, SRAFixture)
{
    ngs :: String expected(":2<A<A;:!");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRA_Accession ) + ".R.2", 2 ) . getFragmentQualities ( 90 ) . toString () );
}

FIXTURE_TEST_CASE(SRA_Fragment_getSubFragmentQualities_OffsetLength, SRAFixture)
{
    ngs :: String expected(":2<A<A");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( SRA_Accession ) + ".R.2", 2 ) . getFragmentQualities ( 90, 6 ) . toString () );
}

/////ReferenceIterator
FIXTURE_TEST_CASE(SRA_ReferenceIterator_Open, SRAFixture)
{
    ngs :: ReferenceIterator refIt = open ( SRA_Accession ) . getReferences ();  // always an empty iterator
    REQUIRE_THROW ( refIt . getCommonName() );
}

FIXTURE_TEST_CASE(SRA_ReferenceIterator_Next, SRAFixture)
{
    ngs :: ReferenceIterator refIt = open ( SRA_Accession ) .  getReferences ();  // always an empty iterator
    REQUIRE( ! refIt . nextReference () );
}

/////AlignmentIterator
FIXTURE_TEST_CASE(SRA_AlignmentIterator_Open, SRAFixture)
{
    ngs :: AlignmentIterator alIt = open ( SRA_Accession ) .  getAlignments ( ngs :: Alignment :: all );  // always an empty iterator
    REQUIRE_THROW ( alIt . getAlignmentId () );
}

FIXTURE_TEST_CASE(SRA_AlignmentIterator_Next, SRAFixture)
{
    ngs :: AlignmentIterator alIt = open ( SRA_Accession ) .  getAlignments ( ngs :: Alignment :: all );  // always an empty iterator
    REQUIRE( ! alIt . nextAlignment () );
}

/////TODO: ReadGroup
//TODO: getName
//TODO: getRead
//TODO: getReads

/////TODO: ReadGroupIterator

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

const char UsageDefaultName[] = "test-ngs_sra-c++";

rc_t CC KMain ( int argc, char *argv [] )
{
/*const char * p = getenv("http_proxy");
cerr << "http_proxy = '" << ( p == NULL ? "NULL" : p ) << "'\n";*/
    KConfigDisableUserSettings();
    rc_t rc=NgsSraCppTestSuite(argc, argv);
    NgsFixture::ReleaseCache();
    return rc;
}

}


