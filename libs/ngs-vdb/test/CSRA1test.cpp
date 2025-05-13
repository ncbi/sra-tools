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
* Unit tests for NGS interface, CSRA1 based implementation
*/

#include "ngsfixture.hpp"
#include <kapp/main.h>

#include <stdint.h>
#include <sstream>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsCsra1CppTestSuite);

class CSRA1_Fixture : public NgsFixture
{
public:
    static const char* CSRA1_PrimaryOnly;
    static const char* CSRA1_WithSecondary;
    static const char* CSRA1_WithGroups;
    static const char* CSRA1_NoReadGroups;
    static const char* CSRA1_WithCircularRef;
    static const char* CSRA1_WithCircularRef2;
    static const char* CSRA1_SingleFragmentPerSpot;

public:
    CSRA1_Fixture()
    {
    }
    ~CSRA1_Fixture()
    {
    }

    ngs :: ReadIterator getReads ( ngs :: Read :: ReadCategory cat = ngs :: Read :: all )
    {
        return NgsFixture :: getReads ( CSRA1_PrimaryOnly, cat );
    }
    ngs :: Read getRead ( const ngs :: String& p_id)
    {
        return NgsFixture :: getRead ( CSRA1_PrimaryOnly, p_id );
    }
    ngs :: Read getFragment (const ngs :: String& p_readId, uint32_t p_fragIdx)
    {
        return NgsFixture :: getFragment ( CSRA1_PrimaryOnly, p_readId, p_fragIdx );
    }
    ngs :: Reference getReference ( const char* spec )
    {
        return NgsFixture :: getReference ( CSRA1_PrimaryOnly, spec );
    }
    bool hasReference ( const char* spec )
    {
        return NgsFixture :: hasReference ( CSRA1_PrimaryOnly, spec );
    }
    ngs :: ReferenceIterator getReferences ()
    {
        return NgsFixture :: getReferences ( CSRA1_PrimaryOnly );
    }
    ngs :: AlignmentIterator getAlignments( ngs :: Alignment :: AlignmentCategory category = ngs :: Alignment :: all )
    {
        return open ( CSRA1_WithSecondary ) . getAlignments ( category );
    }
};
const char* CSRA1_Fixture::CSRA1_PrimaryOnly           = "SRR1063272";
const char* CSRA1_Fixture::CSRA1_WithSecondary         = "SRR833251";
const char* CSRA1_Fixture::CSRA1_WithGroups            = "SRR822962";
const char* CSRA1_Fixture::CSRA1_NoReadGroups          = "SRR1237962";
const char* CSRA1_Fixture::CSRA1_WithCircularRef       = "SRR1769246";
const char* CSRA1_Fixture::CSRA1_WithCircularRef2      = "SRR821492";
const char* CSRA1_Fixture::CSRA1_SingleFragmentPerSpot = "SRR2096940";

#include "CSRA1_ReadCollection_test.cpp"

///// Read
//TODO: error cases

FIXTURE_TEST_CASE(CSRA1_Read_ReadId, CSRA1_Fixture)
{
    REQUIRE_EQ( string ( CSRA1_PrimaryOnly ) + ".R.1", getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadId() . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Read_ReadName, CSRA1_Fixture)
{
    REQUIRE_EQ( ngs :: String( "1" ), getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadName() . toString() );
}

FIXTURE_TEST_CASE(CSRA1_Read_ReadGroup, CSRA1_Fixture)
{
    REQUIRE_EQ( ngs :: String("C1ELY.6"), getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadGroup() );
}

FIXTURE_TEST_CASE(CSRA1_Read_getNumFragments, CSRA1_Fixture)
{
    REQUIRE_EQ( ( uint32_t ) 2, getRead ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1" ) . getNumFragments() );
}

FIXTURE_TEST_CASE(CSRA1_Read_ReadCategory_FullyAligned, CSRA1_Fixture)
{
    REQUIRE_EQ( ngs :: Read :: fullyAligned, getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadCategory() );
}
FIXTURE_TEST_CASE(CSRA1_Read_ReadCategory_PartiallyAligned, CSRA1_Fixture)
{
    REQUIRE_EQ( ngs :: Read :: partiallyAligned, getRead ( string ( CSRA1_PrimaryOnly ) + ".R.3" ) . getReadCategory() );
}

FIXTURE_TEST_CASE(CSRA1_Read_getReadBases, CSRA1_Fixture)
{
    ngs :: String bases = getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadBases () . toString ();
    ngs :: String expected("ACTCGACATTCTGCCTTCGACCTATCTTTCTCCTCTCCCAGTCATCGCCCAGTAGAATTACCAGGCAATGAACCAGGGCC"
                           "TTCCATCCCAACGGCACAGCAAAGGTATGATACCTGAGATGTTGCGGGATGGTGGGTTTGTGAGGAGATGGCCACGCAGG"
                           "CAAGGTCTTTTGGAATGGTTCACTGTTGGAGTGAACCCATAT");
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(CSRA1_Read_getSubReadBases_Offset, CSRA1_Fixture)
{
    ngs :: String bases = getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadBases ( 160 ) . toString( );
    ngs :: String expected("CAAGGTCTTTTGGAATGGTTCACTGTTGGAGTGAACCCATAT");
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(CSRA1_Read_getSubReadBases_OffsetLength, CSRA1_Fixture)
{
    ngs :: String bases = getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadBases ( 160, 10 ) . toString ();
    ngs :: String expected("CAAGGTCTTT");
    REQUIRE_EQ( expected, bases );
}

FIXTURE_TEST_CASE(CSRA1_Read_getReadQualities, CSRA1_Fixture)
{
    ngs :: String quals = getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadQualities() . toString ();
    ngs :: String expected("@@CDDBDFFBFHFIEEFGIGGHIEHIGIGGFGEGAFDHIIIIIGGGDFHII;=BF@FEHGIEEH?AHHFHFFFFDC5'5="
                           "?CC?ADCD@AC??9BDDCDB<@@@DDADDFFHGHIIDHFFHDEFEHIIGHIIDGGGFHIJIGAGHAH=;DGEGEEEDDDB"
                           "<ABBD;ACDDDCBCCCDD@CCDDDCDCDBDD@ACC>A@?>C3");
    REQUIRE_EQ( expected, quals );
}

FIXTURE_TEST_CASE(CSRA1_Read_getSubReadQualities_Offset, CSRA1_Fixture)
{
    ngs :: String quals = getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadQualities( 160 ) . toString ();
    ngs :: String expected("<ABBD;ACDDDCBCCCDD@CCDDDCDCDBDD@ACC>A@?>C3");
    REQUIRE_EQ( expected, quals );
}

FIXTURE_TEST_CASE(CSRA1_Read_getSubReadQualities_OffsetLength, CSRA1_Fixture)
{
    ngs :: String quals = getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" ) . getReadQualities( 160, 10 ) . toString ();
    ngs :: String expected("<ABBD;ACDD");
    REQUIRE_EQ( expected, quals );
}

FIXTURE_TEST_CASE(CSRA1_Read_fragmentIsAligned_MultiFragmentsPerPartiallyAlignedSpot, CSRA1_Fixture)
{
    ngs :: Read read = open ( CSRA1_PrimaryOnly ) . getRead ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.3" );
    REQUIRE_EQ( true, read.fragmentIsAligned( 0 ) );
    REQUIRE_EQ( false, read.fragmentIsAligned( 1 ) );
}

///// ReadIterator
//TODO: read category selection
//TODO: range on a collection that represents a slice (read[0].id != 1)
//TODO: range and filtering
//TODO: ReadIterator over a ReadGroup (use a ReadGroup that is not immediately at the beginning of the run)
//TODO: ReadIterator over a ReadIterator, to allow creation of a sub-iterator
//TODO: ReadIterator returning less than the range requested
//TODO: error cases (?)
FIXTURE_TEST_CASE(CSRA1_ReadIterator_NoReadBeforeNext, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = getReads ();
    // accessing the read through an iterator before a call to nextRead() throws
    REQUIRE_THROW ( readIt . getReadId() );
}

FIXTURE_TEST_CASE(CSRA1_ReadIterator_Open_All, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = getReads ();
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1", readIt . getReadId() . toString () );
}

#if SHOW_UNIMPLEMENTED
FIXTURE_TEST_CASE(CSRA1_ReadIterator_Open_Filtered, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = getReads ( ngs :: Read :: partiallyAligned );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.3", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.5", readIt . getReadId() . toString () );
}
#endif

FIXTURE_TEST_CASE(CSRA1_ReadIterator_Next, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = getReads ();
    REQUIRE( readIt . nextRead () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.2", readIt . getReadId() . toString () );
}

FIXTURE_TEST_CASE(CSRA1_ReadIterator_End, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = getReads ();
    ngs :: String lastId;
    while (readIt . nextRead ())
    {
        lastId = readIt . getReadId() . toString ();
    }
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.2280633", lastId );
}

FIXTURE_TEST_CASE(CSRA1_ReadIterator_BeyondEnd, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = getReads ();
    while (readIt . nextRead ())
    {
    }
    REQUIRE_THROW( readIt . getReadId(); );
}

FIXTURE_TEST_CASE(CSRA1_ReadIterator_Range, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = open ( CSRA1_PrimaryOnly ) . getReadRange ( 10, 5 );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.10", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.11", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.12", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.13", readIt . getReadId() . toString () );
    REQUIRE( readIt . nextRead () );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.14", readIt . getReadId() . toString () );
    REQUIRE( ! readIt . nextRead () );
}

FIXTURE_TEST_CASE(CSRA1_ReadIterator_EmptyRange, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_PrimaryOnly );
    ngs :: ReadIterator readIt = run . getReadRange ( run . getReadCount() + 1, 5 );
    REQUIRE_THROW ( readIt . getReadId(); );
    REQUIRE ( ! readIt . nextRead () );
}

///// Fragment
//TODO: error cases
FIXTURE_TEST_CASE(CSRA1_Fragment_NoFragmentBeforeNext, CSRA1_Fixture)
{
    ngs :: Read read = getRead ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1" );
    REQUIRE_THROW( read . getFragmentId() );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_Id, CSRA1_Fixture)
{
    ngs :: Read read = getRead ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1" );
    REQUIRE ( read . nextFragment() );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".FR0.1", read . getFragmentId() . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_getFragmentBases, CSRA1_Fixture)
{
    ngs :: String expected("AAGGTATGATACCTGAGATGTTGCGGGATGGTGGGTTTGTGAGGAGATGGCCACGCAGGCAAGGTCTTTTGGAATGGTTCACTGTTGGAGTGAACCCATAT");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1", 2 ) . getFragmentBases () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_getSubFragmentBases_Offset, CSRA1_Fixture)
{
    ngs :: String expected("ACTGTTGGAGTGAACCCATAT");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1", 2 ) . getFragmentBases ( 80 ) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_getSubFragmentBases_OffsetLength, CSRA1_Fixture)
{
    ngs :: String expected("ACTGTT");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1", 2 ) . getFragmentBases ( 80, 6 ) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_getFragmentQualities, CSRA1_Fixture)
{
    ngs :: String expected("@@@DDADDFFHGHIIDHFFHDEFEHIIGHIIDGGGFHIJIGAGHAH=;DGEGEEEDDDB<ABBD;ACDDDCBCCCDD@CCDDDCDCDBDD@ACC>A@?>C3");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1", 2 ) . getFragmentQualities () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_getSubFragmentQualities_Offset, CSRA1_Fixture)
{
    ngs :: String expected("DDDCDCDBDD@ACC>A@?>C3");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1", 2 ) . getFragmentQualities ( 80 ) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_getSubFragmentQualities_OffsetLength, CSRA1_Fixture)
{
    ngs :: String expected("DDDCDC");
    REQUIRE_EQ( expected, getFragment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1", 2 ) . getFragmentQualities ( 80, 6 ) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_isPaired_MultiFragmentsPerAlignedSpot, CSRA1_Fixture)
{
    ngs :: Read read = open ( CSRA1_PrimaryOnly ) . getRead ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.1" );
    read.nextFragment();
    REQUIRE_EQ( true, read.isPaired() );
    read.nextFragment();
    REQUIRE_EQ( true, read.isPaired() );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_isPaired_MultiFragmentsPerPartiallyAlignedSpot, CSRA1_Fixture)
{
    ngs :: Read read = open ( CSRA1_PrimaryOnly ) . getRead ( ngs :: String ( CSRA1_PrimaryOnly ) + ".R.3" );
    read.nextFragment();
    REQUIRE_EQ( true, read.isPaired() );
    read.nextFragment();
    REQUIRE_EQ( true, read.isPaired() );
}

FIXTURE_TEST_CASE(CSRA1_Fragment_isPaired_SingleFragmentPerSpot, CSRA1_Fixture)
{
    ngs :: Read read = open ( CSRA1_SingleFragmentPerSpot ) . getRead ( ngs :: String ( CSRA1_SingleFragmentPerSpot ) + ".R.1" );
    read.nextFragment();
    REQUIRE_EQ( false, read.isPaired() );
}

//TODO: getFragmentQualities on an accession with QUALITY column of type phred_33 (and phred_64, if there can be one)

///// Reference
//TODO: getAlignmentSlice
//TODO: getPileups
//TODO: getPileupRange
//TODO: error cases
FIXTURE_TEST_CASE(CSRA1_Reference_CommonName, CSRA1_Fixture)
{   // SRR1199225
    const char* name = "supercont2.1";
    REQUIRE_EQ( ngs :: String ( name ), getReference ( name ). getCommonName () );
}
FIXTURE_TEST_CASE(CSRA1_Reference_CanonicalName, CSRA1_Fixture)
{
    const char* name = "chr7";
    const char* canoName = "NC_000007.13";
    REQUIRE_EQ( ngs :: String ( canoName ), open ( "SRR821492" ) . getReference ( name ). getCanonicalName () );
}

FIXTURE_TEST_CASE(CSRA1_Reference_IsCircular_No, CSRA1_Fixture)
{
    REQUIRE( ! getReference ( "supercont2.1" ). getIsCircular () );
}
FIXTURE_TEST_CASE(CSRA1_Reference_IsCircular_Yes, CSRA1_Fixture)
{
    REQUIRE( open ( "SRR821492" ) . getReference ( "chrM" ) . getIsCircular () );
}

FIXTURE_TEST_CASE(CSRA1_Reference_IsLocal_Yes, CSRA1_Fixture)
{
    REQUIRE( getReference ( "supercont2.1" ). getIsLocal () );
}
FIXTURE_TEST_CASE(CSRA1_Reference_IsLocal_No, CSRA1_Fixture)
{
    REQUIRE( ! open ( "SRR821492" ) . getReference ( "chrM" ) . getIsLocal () );
}

static uint64_t CSRA1_ReferenceLength = (uint64_t)2291499l;
FIXTURE_TEST_CASE(CSRA1_Reference_GetLength, CSRA1_Fixture)
{
    REQUIRE_EQ( CSRA1_ReferenceLength, getReference ( "supercont2.1" ). getLength () );
}


static uint64_t CSRA1_ReferenceLength_VDB_2832 = (uint64_t)49711204;
FIXTURE_TEST_CASE(CSRA1_Reference_GetLength_VDB_2832, CSRA1_Fixture)
{
    REQUIRE_EQ( CSRA1_ReferenceLength_VDB_2832, open ( "SRR2073063" ) . getReference ( "NC_016101.1" ). getLength () );
    REQUIRE_EQ( CSRA1_ReferenceLength_VDB_2832, open ( "SRR2073063" ) . getReference ( "Gm14" ). getLength () );
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceBases_All, CSRA1_Fixture)
{
    ngs :: String str = getReference ( "supercont2.1" ). getReferenceBases ( 0 );
    REQUIRE_EQ( (size_t)CSRA1_ReferenceLength, str . size () );
    REQUIRE_EQ( string("GAATTCT"), str . substr (0, 7) );
    REQUIRE_EQ( string("ATCTG"), str . substr (str.size() - 5, 5) );
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceBases_Offset_1, CSRA1_Fixture)
{   // offset points into the first chunk of the reference
    ngs :: String str = getReference ( "supercont2.1" ). getReferenceBases ( 1000 );
    REQUIRE_EQ( CSRA1_ReferenceLength - 1000, (uint64_t) str . size () );
    REQUIRE_EQ( string("TCCATTC"), str . substr (0, 7) );
    REQUIRE_EQ( string("ATCTG"), str . substr (str.size() - 5, 5) );
}
FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceBases_Offset_2, CSRA1_Fixture)
{   // offset points into the second chunk of the reference
    ngs :: String str = getReference ( "supercont2.1" ). getReferenceBases ( 7000 );
    REQUIRE_EQ( CSRA1_ReferenceLength - 7000, (uint64_t) str . size () );
    REQUIRE_EQ( string("CCTGTCC"), str . substr (0, 7) );
    REQUIRE_EQ( string("ATCTG"), str . substr (str.size() - 5, 5) );
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceBases_OffsetLength_1, CSRA1_Fixture)
{   // offset points into the first chunk of the reference
    ngs :: String str = getReference ( "supercont2.1" ). getReferenceBases ( 2000, 10 );
    REQUIRE_EQ( ngs::String ( "GGGCAAATGA" ), str );
}
FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceBases_OffsetLength_2, CSRA1_Fixture)
{   // offset points into the second chunk of the reference
    ngs :: Reference ref = getReference ( "supercont2.1" );
    ngs :: String str = ref . getReferenceBases ( 9000, 10 );
    REQUIRE_EQ( ngs::String ( "GCGCTATGAC" ), str );
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_All, CSRA1_Fixture)
{
    ngs :: Reference ref = getReference ( "supercont2.1" );
    ngs :: StringRef str = ref . getReferenceChunk ( 0 );
    REQUIRE_EQ( (size_t)20000, str . size () );
    REQUIRE_EQ( string("GAATTCT"), str . substr (0, 7) . toString () );
    REQUIRE_EQ( string("CATCA"), str . substr (str.size() - 5, 5) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_Offset, CSRA1_Fixture)
{
    ngs :: Reference ref = getReference ( "supercont2.1" );
    ngs :: StringRef str = ref . getReferenceChunk ( 1000 );
    REQUIRE_EQ( (size_t)19000, str . size () );
    REQUIRE_EQ( string("TCCATTC"), str . substr (0, 7) . toString () );
    REQUIRE_EQ( string("CATCA"), str . substr (str.size() - 5, 5) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_OffsetLength, CSRA1_Fixture)
{   // offset points into the first chunk of the reference
    ngs :: Reference ref = getReference ( "supercont2.1" );
    REQUIRE_EQ( ngs::String ( "GGGCAAATGA" ), ref . getReferenceChunk ( 2000, 10 ) . toString ());
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetAlignment, CSRA1_Fixture)
{
    ngs :: Alignment al = getReference ( "supercont2.1" ). getAlignment( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" );
    REQUIRE_EQ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1", al . getAlignmentId () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetAlignments, CSRA1_Fixture)
{
    ngs :: AlignmentIterator al = getReference ( "supercont2.1" ). getAlignments ( ngs :: Alignment :: all );
}

///// ReferenceIterator
FIXTURE_TEST_CASE(CSRA1_ReferenceIterator_NoObjectBeforeNextOpen, CSRA1_Fixture)
{
    ngs :: ReferenceIterator refIt = getReferences();
    // dereferencing the iterator without calling Next() throws
    REQUIRE_THROW ( refIt . getCommonName() );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceIterator_Open, CSRA1_Fixture)
{
    ngs :: ReferenceIterator refIt = getReferences();
    REQUIRE( refIt . nextReference () );
    REQUIRE_EQ( string("supercont2.1"), refIt . getCommonName() );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceIterator_Next, CSRA1_Fixture)
{
    ngs :: ReferenceIterator refIt = getReferences();
    REQUIRE( refIt . nextReference () );
    REQUIRE( refIt . nextReference () );
    REQUIRE_EQ( string("supercont2.2"), refIt . getCommonName() );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceIterator_End, CSRA1_Fixture)
{
    ngs :: ReferenceIterator refIt = getReferences();
    string lastName;
    while (refIt . nextReference ())
    {
        lastName = refIt . getCommonName();
        //cout << " lastName " << refIt . getLength () << endl;
    }
    REQUIRE_EQ( string("supercont2.14"), lastName );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceIterator_BeyondEnd, CSRA1_Fixture)
{
    ngs :: ReferenceIterator refIt = getReferences();
    while (refIt . nextReference ())
    {
    }
    REQUIRE_THROW ( refIt . getCommonName(); );
}

///// Alignment
//TODO: CG's EVIDENCE?
//TODO: add tests for missing optional columns
//TODO: getRNAOrientatation__Present

FIXTURE_TEST_CASE(CSRA1_Alignment_getAlignmentId, CSRA1_Fixture)
{
    REQUIRE_EQ( ngs :: String ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getAlignmentId() . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReferenceSpec, CSRA1_Fixture)
{
    REQUIRE_EQ( string("supercont2.1"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getReferenceSpec () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMappingQuality, CSRA1_Fixture)
{
    REQUIRE_EQ( 60,
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getMappingQuality () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReferenceBases, CSRA1_Fixture)
{
    REQUIRE_EQ( string("ACTCGACATTCTGTCTTCGACCTATCTTTCTCCTCTCCCAGTCATCGCCCAGTAGAATTACCAGGCAATGAACCACGGCCTTTCATCCCAACGGCACAGCA"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getReferenceBases () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReadGroup, CSRA1_Fixture)
{
    REQUIRE_EQ( string("C1ELY.6"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getReadGroup () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReadGroup_Empty, CSRA1_Fixture)
{
    REQUIRE_EQ( string(""),
                open ( CSRA1_NoReadGroups ) . getAlignment ( ngs :: String ( CSRA1_NoReadGroups ) + ".PA.1" ) . getReadGroup () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReadId, CSRA1_Fixture)
{
    REQUIRE_EQ( string ( CSRA1_PrimaryOnly ) + ".R.165753",
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.5" ) . getReadId () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getFragmentId, CSRA1_Fixture)
{
    REQUIRE_EQ( string ( CSRA1_PrimaryOnly ) + ".FA0.1",
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getFragmentId () . toString ());
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getFragmentBases, CSRA1_Fixture)
{
    REQUIRE_EQ( string("TGGATGCTCTGGAAAATCTGAAAAGTGGTGTTTGTAAGGTTTGCTGGCTGCCCATATACCACATGGATGATGGGGCTTTCCATTTTAATGTTGAAGGAGGA"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getFragmentBases () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getFragmentQualities, CSRA1_Fixture)
{
    REQUIRE_EQ( string("######AA>55;5(;63;;3@;A9??;6..73CDCIDA>DCB>@B=;@B?;;ADAB<DD?1*>@C9:EC?2++A3+F4EEB<E>EEIEDC2?C:;AB+==1"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getFragmentQualities () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getClippedFragmentBases, CSRA1_Fixture)
{
    REQUIRE_EQ( string("CTTCAACATTAAAATGGAAAGCCCCATCATCCATGTGGTATATGGGCAGCCAGCAAACCTTACAAACACCACTTTTCAGATTTTCCAGAGCATCCA"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getClippedFragmentBases () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getClippedFragmentQualities, CSRA1_Fixture)
{
    REQUIRE_EQ( string("#AA>55;5(;63;;3@;A9??;6..73CDCIDA>DCB>@B=;@B?;;ADAB<DD?1*>@C9:EC?2++A3+F4EEB<E>EEIEDC2?C:;AB+==1"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getClippedFragmentQualities () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getAlignedFragmentBases, CSRA1_Fixture)
{
    REQUIRE_EQ( string("AAGGTATGATACCTGAGATGTTGCGGGATGGTGGGTTTGTGAGGAGATGGCCACGCAGGCAAGGTCTTTTGGAATGGTTCACTGTTGGAGTGAACCCATAT"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.2" ) . getFragmentBases () . toString () );
    REQUIRE_EQ( string("ATATGGGTTCACTCCAACAGTGAACCATTCCAAAAGACCTTGCCTGCGTGGCCATCTCCTCACAAACCCACCATCCCGCAACATCTCAGGTATCATACCTT"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.2" ) . getAlignedFragmentBases () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getAlignmentCategory, CSRA1_Fixture)
{
    REQUIRE_EQ( ngs :: Alignment :: primaryAlignment,
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getAlignmentCategory() );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getAlignmentPosition, CSRA1_Fixture)
{
    REQUIRE_EQ( (int64_t)85, //REF_POS
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getAlignmentPosition () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReferencePositionProjectionRange, CSRA1_Fixture)
{
    uint64_t res = open ("SRR1597772").getAlignment("SRR1597772.PA.6").getReferencePositionProjectionRange(11601);
    REQUIRE( res == 1 );

    res = open("SRR1597895").getAlignment("SRR1597895.PA.26088399").getReferencePositionProjectionRange(234668879);
    REQUIRE( res == (uint64_t)-1 ); // out of bounds
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReferencePositionProjectionRangeI, CSRA1_Fixture)
{
    uint64_t res = open ("SRR1597772").getAlignment("SRR1597772.PA.6").getReferencePositionProjectionRange(11692);
    REQUIRE( res == ((uint64_t)84 << 32 | 5) );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getReferencePositionProjectionRangeD, CSRA1_Fixture)
{
    uint64_t res = open ("SRR1597772").getAlignment("SRR1597772.PA.6").getReferencePositionProjectionRange(11667);
    REQUIRE( res == ((uint64_t)65 << 32) );
    res = open ("SRR1597772").getAlignment("SRR1597772.PA.6").getReferencePositionProjectionRange(11669);
    REQUIRE( res == ((uint64_t)65 << 32) );
    res = open ("SRR1597772").getAlignment("SRR1597772.PA.6").getReferencePositionProjectionRange(11672);
    REQUIRE( res == ((uint64_t)65 << 32) );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getAlignmentLength, CSRA1_Fixture)
{
    REQUIRE_EQ( (uint64_t)101, //REF_LEN or length(RAW_READ)
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getAlignmentLength () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getIsReversedOrientation_False, CSRA1_Fixture)
{
    REQUIRE( ! open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getIsReversedOrientation () );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_getIsReversedOrientation_True, CSRA1_Fixture)
{
    REQUIRE( open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.2" ) . getIsReversedOrientation () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getSoftClip_None, CSRA1_Fixture)
{
    REQUIRE_EQ( 0, open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getSoftClip ( ngs :: Alignment :: clipLeft ) );
    REQUIRE_EQ( 0, open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getSoftClip ( ngs :: Alignment :: clipRight ) );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_getSoftClip_Left, CSRA1_Fixture)
{
    REQUIRE_EQ( 5, open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getSoftClip ( ngs :: Alignment :: clipLeft ) );
    REQUIRE_EQ( 0, open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getSoftClip ( ngs :: Alignment :: clipRight ) );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_getSoftClip_Right, CSRA1_Fixture)
{
    REQUIRE_EQ( 0,  open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.10" ) . getSoftClip ( ngs :: Alignment :: clipLeft ) );
    REQUIRE_EQ( 13, open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.10" ) . getSoftClip ( ngs :: Alignment :: clipRight ) );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getTemplateLength, CSRA1_Fixture)
{
    REQUIRE_EQ( (uint64_t)201,
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getTemplateLength () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getShortCigar_Unclipped, CSRA1_Fixture)
{
    REQUIRE_EQ( string("5S96M"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getShortCigar ( false ) . toString () );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_getShortCigar_Clipped, CSRA1_Fixture)
{
    REQUIRE_EQ( string("96M"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getShortCigar ( true ) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getLongCigar_Unclipped, CSRA1_Fixture)
{
    REQUIRE_EQ( string("5S1X8=1X39=1X46="),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getLongCigar ( false ) . toString () );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_getLongCigar_Clipped, CSRA1_Fixture)
{
    REQUIRE_EQ( string("1X8=1X39=1X46="),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.4" ) . getLongCigar ( true ) . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getRNAOrientation_Missing, CSRA1_Fixture)
{
    REQUIRE_EQ( '?',
                open ( CSRA1_NoReadGroups ) . getAlignment ( ngs :: String ( CSRA1_NoReadGroups ) + ".PA.1" ) . getRNAOrientation() );
}

// Mate alignment
FIXTURE_TEST_CASE(CSRA1_Alignment_hasMate_Primary_NoMate, CSRA1_Fixture)
{
    REQUIRE( ! open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.99" ) . hasMate () );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_hasMate_Primary_YesMate, CSRA1_Fixture)
{
    REQUIRE( open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . hasMate () );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_hasMate_Secondary, CSRA1_Fixture)
{
    REQUIRE( ! open ( CSRA1_WithSecondary ) . getAlignment ( ngs :: String ( CSRA1_WithSecondary ) + ".SA.169" ) . hasMate () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateAlignmentId_Primary, CSRA1_Fixture)
{
    REQUIRE_EQ( string ( CSRA1_PrimaryOnly ) + ".PA.2",
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getMateAlignmentId () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateAlignmentId_Missing, CSRA1_Fixture)
{
    REQUIRE_THROW ( open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.99" ) . getMateAlignmentId (); );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateAlignmentId_SecondaryThrows, CSRA1_Fixture)
{
    REQUIRE_THROW( open ( CSRA1_WithSecondary ) . getAlignment ( ngs :: String ( CSRA1_WithSecondary ) + ".SA.172" ) . getMateAlignmentId (); );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateAlignment, CSRA1_Fixture)
{
    ngs :: Alignment al = open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getMateAlignment ();
    REQUIRE_EQ ( string ( CSRA1_PrimaryOnly ) + ".PA.2", al . getAlignmentId () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateAlignment_Missing, CSRA1_Fixture)
{
    REQUIRE_THROW ( ngs :: Alignment al = open ( CSRA1_PrimaryOnly ) . getAlignment ( "99" ) . getMateAlignment (); );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateAlignment_SecondaryThrows, CSRA1_Fixture)
{
    REQUIRE_THROW( open ( CSRA1_WithSecondary ) . getAlignment ( ngs :: String ( CSRA1_WithSecondary ) + ".SA.169" ) . getMateAlignment (); );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateReferenceSpec, CSRA1_Fixture)
{
    REQUIRE_EQ( string("supercont2.1"),
                open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getMateReferenceSpec () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_getMateIsReversedOrientation_True, CSRA1_Fixture)
{
    REQUIRE( open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" ) . getMateIsReversedOrientation () );
}
FIXTURE_TEST_CASE(CSRA1_Alignment_getMateIsReversedOrientation_False, CSRA1_Fixture)
{
    REQUIRE( ! open ( CSRA1_PrimaryOnly ) . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.2" ) . getMateIsReversedOrientation () );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_isPaired_MultiFragmentsPerSpot, CSRA1_Fixture)
{
    ngs :: ReadCollection readCollection = open ( CSRA1_PrimaryOnly );
    ngs :: Alignment alignment = readCollection . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.1" );
    REQUIRE_EQ( true, alignment.isPaired() );

    alignment = readCollection . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.2" );
    REQUIRE_EQ( true, alignment.isPaired() );

    // has unaligned mate
    alignment = readCollection . getAlignment ( ngs :: String ( CSRA1_PrimaryOnly ) + ".PA.6" );
    REQUIRE_EQ( true, alignment.isPaired() );
}

FIXTURE_TEST_CASE(CSRA1_Alignment_isPaired_SingleFragmentPerSpot, CSRA1_Fixture)
{
    ngs :: ReadCollection readCollection = open ( CSRA1_SingleFragmentPerSpot );
    ngs :: Alignment alignment = readCollection . getAlignment ( ngs :: String ( CSRA1_SingleFragmentPerSpot ) + ".PA.1" );
    REQUIRE_EQ( false, alignment.isPaired() );
}


///// AlignmentIterator from ReadCollection
//TODO: CG's EVIDENCE?

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_NoAlignmentBeforeNext, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = getAlignments ();
    REQUIRE_THROW( it . getAlignmentId () );
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_PrimaryOnOpen, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = getAlignments ();
    REQUIRE( it . nextAlignment () );
    REQUIRE_EQ( ngs :: String ( CSRA1_WithSecondary ) + ".PA.1", it . getAlignmentId () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_Next, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = getAlignments ();
    REQUIRE( it . nextAlignment () );
    REQUIRE( it . nextAlignment () );
    REQUIRE_EQ( ngs :: String ( CSRA1_WithSecondary ) + ".PA.2", it . getAlignmentId () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_Next_Switch_To_Secondary, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = open ( CSRA1_WithSecondary ) . getAlignmentRange ( 168, 2, ngs :: Alignment :: all );
    REQUIRE( it . nextAlignment () );
    REQUIRE_EQ( ngs :: String ( CSRA1_WithSecondary ) + ".PA.168", it . getAlignmentId () . toString () );
    REQUIRE( it . nextAlignment () );
    REQUIRE_EQ( ngs :: String ( CSRA1_WithSecondary ) + ".SA.169", it . getAlignmentId () . toString () );
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_BeyondEnd, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = getAlignments ();
    while ( it . nextAlignment() )
    {
    }
    REQUIRE_THROW ( it . getAlignmentId (); );
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_Empty, CSRA1_Fixture)
{
    // the next line should not throw
    ngs :: AlignmentIterator it = open ( CSRA1_PrimaryOnly ) . getAlignments ( ngs :: Alignment :: secondaryAlignment );
    REQUIRE_THROW ( it . getAlignmentId(); );
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_Primary, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = getAlignments ( ngs :: Alignment :: primaryAlignment );
    REQUIRE( it . nextAlignment() );
    size_t count = 1;
    while ( it . nextAlignment() )
    {
        REQUIRE_EQ( ngs :: Alignment :: primaryAlignment, it . getAlignmentCategory() );
        ++count;
        ostringstream str;
        str << CSRA1_WithSecondary << ".PA." << count;
        REQUIRE_EQ( str . str (), it . getAlignmentId() . toString () );
    }
    REQUIRE_EQ( (size_t)168, count);
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_Secondary, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = getAlignments ( ngs :: Alignment :: secondaryAlignment );
    REQUIRE( it . nextAlignment() );
    size_t count = 0;
    while ( it . nextAlignment() )
    {
        REQUIRE_EQ( ngs :: Alignment :: secondaryAlignment, it . getAlignmentCategory() );
        ++count;
    }
    REQUIRE_EQ( (size_t)9, count);
}

FIXTURE_TEST_CASE(CSRA1_AlignmentIterator_All, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = getAlignments ( ngs :: Alignment :: all );
    REQUIRE( it . nextAlignment() );
    size_t count = 1;
    while ( it . nextAlignment() )
    {
        if ( it . getAlignmentCategory()  == ngs :: Alignment :: secondaryAlignment )
        {
            break;
        }
        ++count;
    }
    REQUIRE_EQ( (size_t)168, count);
    /* switched over to secondary alignments */
    while ( it . nextAlignment() )
    {
        REQUIRE_EQ( ngs :: Alignment :: secondaryAlignment, it . getAlignmentCategory() );
        ++count;
    }
    REQUIRE_EQ( (size_t)177, count);
}

///// AlignmentIterator from Reference (ReferenceWindow)
FIXTURE_TEST_CASE(CSRA1_ReferenceWindow, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = NgsFixture :: getReference ( CSRA1_WithSecondary, "gi|169794206|ref|NC_010410.1|" )
                                                             . getAlignments ( ngs :: Alignment :: all );
    REQUIRE ( it . nextAlignment () );

    // the first 2 secondary alignments' locations on the list: #34, #61
    size_t count = 1;
    while ( it . nextAlignment() )
    {
        if ( it . getAlignmentCategory()  == ngs :: Alignment :: secondaryAlignment )
            break;
        ++count;
    }
    REQUIRE_EQ( (size_t)34, count);
    while ( it . nextAlignment() )
    {
        if ( it . getAlignmentCategory()  == ngs :: Alignment :: secondaryAlignment )
            break;
        ++count;
    }
    REQUIRE_EQ( (size_t)61, count);
}

FIXTURE_TEST_CASE(CSRA1_ReferenceWindow_Slice, CSRA1_Fixture)
{
    ngs :: AlignmentIterator it = NgsFixture :: getReference ( CSRA1_WithSecondary, "gi|169794206|ref|NC_010410.1|" )
                                                             . getAlignmentSlice ( 516000, 100000 );

    REQUIRE ( it . nextAlignment () );
    REQUIRE_EQ ( ngs :: String ( CSRA1_WithSecondary ) + ".PA.33", it. getAlignmentId () . toString () );
    REQUIRE ( it . nextAlignment () );
    REQUIRE_EQ ( ngs :: String ( CSRA1_WithSecondary ) + ".PA.34", it. getAlignmentId () . toString () );
    REQUIRE ( it . nextAlignment () );
    REQUIRE_EQ ( ngs :: String ( CSRA1_WithSecondary ) + ".SA.169", it. getAlignmentId () . toString () );
    REQUIRE ( it . nextAlignment () );
    REQUIRE_EQ ( ngs :: String ( CSRA1_WithSecondary ) + ".PA.35", it. getAlignmentId () . toString () );

    REQUIRE ( ! it . nextAlignment () );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceWindow_Slice_Filtered_Start_Within_Slice, CSRA1_Fixture)
{
    ngs :: Reference ref = open ( CSRA1_WithCircularRef ) . getReference ( "NC_012920.1" );
    int64_t sliceStart = 1000;
    ngs :: AlignmentIterator it = ref . getFilteredAlignmentSlice ( sliceStart, 200, ngs :: Alignment :: all, ngs :: Alignment :: startWithinSlice, 0 );

    REQUIRE ( it . nextAlignment () );
    REQUIRE_LE ( sliceStart, it.getAlignmentPosition() );
}
FIXTURE_TEST_CASE(CSRA1_ReferenceWindow_Slice_Unfiltered_Start_Before_Slice, CSRA1_Fixture)
{
    ngs :: Reference ref = open ( CSRA1_WithCircularRef ) . getReference ( "NC_012920.1" );
    int64_t sliceStart = 1000;
    ngs :: AlignmentIterator it = ref . getFilteredAlignmentSlice ( sliceStart, 200, ngs :: Alignment :: all, (ngs::Alignment::AlignmentFilter)0, 0 );

    REQUIRE ( it . nextAlignment () );
    REQUIRE_GT ( sliceStart, it.getAlignmentPosition() );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceWindow_Slice_Filtered_Wraparound, CSRA1_Fixture)
{
    ngs :: Reference ref = open ( CSRA1_WithCircularRef2 ) . getReference ( "chrM" );
    int64_t sliceStart = 5;
    ngs :: AlignmentIterator it = ref . getFilteredAlignmentSlice ( sliceStart, 100, ngs :: Alignment :: all, (ngs::Alignment::AlignmentFilter)0, 0 );

    REQUIRE ( it . nextAlignment () );

    // the first returned alignment starts before the start of the circular reference, overlaps with slice
    int64_t pos = it.getAlignmentPosition();
    REQUIRE_LT ( sliceStart + 100, pos );

    // check for overlap with the slice
    uint64_t refLen = ref . getLength();
    pos -= ( int64_t ) refLen;
    REQUIRE_GT ( ( int64_t ) 0, pos );
    REQUIRE_GE ( ( int64_t ) (pos + it . getAlignmentLength()), sliceStart );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceWindow_Slice_Filtered_Wraparound_StartWithinSlice, CSRA1_Fixture)
{
    // when startWithinSlice filter is specified, it removes wraparound alignments as well
    ngs :: Reference ref = open ( CSRA1_WithCircularRef2 ) . getReference ( "chrM" );
    int64_t sliceStart = 5;
    ngs :: AlignmentIterator it = ref . getFilteredAlignmentSlice ( sliceStart, 100, ngs :: Alignment :: all, ngs :: Alignment :: startWithinSlice, 0 );

    REQUIRE ( it . nextAlignment () );

    // the first returned alignment starts inside the slice
    int64_t pos = it.getAlignmentPosition();
    REQUIRE_LE ( sliceStart, pos );
    REQUIRE_LT ( pos, sliceStart + 100 );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceWindow_Slice_Filtered_NoWraparound, CSRA1_Fixture)
{
    // when startWithinSlice filter is specified, it removes wraparound alignments as well
    ngs :: Reference ref = open ( CSRA1_WithCircularRef2 ) . getReference ( "chrM" );
    int64_t sliceStart = 5;
    ngs :: AlignmentIterator it = ref . getFilteredAlignmentSlice ( sliceStart, 100, ngs :: Alignment :: all, ngs :: Alignment :: noWraparound, 0 );

    REQUIRE ( it . nextAlignment () );

    // the first returned sliceStart starts outside the slice but does not wrap around
    REQUIRE_GT ( sliceStart, it.getAlignmentPosition() );
}

FIXTURE_TEST_CASE(CSRA1_ReferenceWindow_Slice_Filtered_NoWraparound_StartWithinSlice, CSRA1_Fixture)
{
    // when startWithinSlice filter is specified, it removes wraparound alignments as well
    ngs :: Reference ref = open ( CSRA1_WithCircularRef2 ) . getReference ( "chrM" );
    int64_t sliceStart = 5;
    ngs :: AlignmentIterator it = ref . getFilteredAlignmentSlice ( sliceStart, 100, ngs :: Alignment :: all, (ngs::Alignment::AlignmentFilter) (ngs :: Alignment :: noWraparound | ngs :: Alignment :: startWithinSlice), 0 );

    REQUIRE ( it . nextAlignment () );

    // the first returned alignment starts inside the slice
    int64_t pos = it.getAlignmentPosition();
    REQUIRE_LE ( sliceStart, pos );
    REQUIRE_LT ( pos, sliceStart + 100 );
}

FIXTURE_TEST_CASE(CSRA1_Filter_SEQ_SPOT_ID_0, CSRA1_Fixture)
{
    // we should filter out secondary alignments with SEQ_SPOT_ID == 0
    ngs :: ReadCollection run = open ( "./data/seq_spot_id_0.sra" );
    ngs :: Reference reference = run . getReference ( "NC_016088.2" );

    ngs :: AlignmentIterator alignmentIterator = run.getAlignmentRange ( 145, 2, ngs :: Alignment :: all );

    REQUIRE ( alignmentIterator.nextAlignment() );
    REQUIRE_EQ ( ngs :: String ( "seq_spot_id_0.PA.145" ), alignmentIterator . getAlignmentId() . toString () );
    REQUIRE ( !alignmentIterator.nextAlignment() );

    alignmentIterator = reference . getFilteredAlignmentSlice ( 29446335, 10, ngs :: Alignment :: all, ngs :: Alignment :: startWithinSlice, 0 );
    REQUIRE ( !alignmentIterator.nextAlignment() );

    REQUIRE_THROW ( run . getAlignment ( "seq_spot_id_0.SA.146" ) );
    REQUIRE_THROW ( reference . getAlignment ( "seq_spot_id_0.SA.146" ) );
}

/////TODO: Pileup
//TODO: getReferenceSpec
//TODO: getReferencePosition
//TODO: getPileupEvents
//TODO: getPileupDepth

/////TODO: PileupIterator

/////TODO: PileupEvent
//TODO: getReferenceSpec
//TODO: getReferencePosition
//TODO: getMappingQuality
//TODO: getAlignmentId
//TODO: getAlignment
//TODO: getAlignmentPosition
//TODO: getFirstAlignmentPosition
//TODO: getLastAlignmentPosition
//TODO: getEventType
//TODO: getAlignmentBase
//TODO: getAlignmentQuality
//TODO: getInsertionBases
//TODO: getInsertionQualities
//TODO: getDeletionCount

/////TODO: PileupEventIterator

///// ReadGroup
FIXTURE_TEST_CASE(CSRA1_ReadGroup_GetName, CSRA1_Fixture)
{
    ngs :: ReadGroup rg = open ( CSRA1_PrimaryOnly ) . getReadGroup ( "C1ELY.6" );
    REQUIRE_EQ ( ngs :: String("C1ELY.6"), rg . getName () );
}

FIXTURE_TEST_CASE(CSRA1_ReadGroup_HasReadGroup, CSRA1_Fixture)
{
    REQUIRE ( open( CSRA1_PrimaryOnly ).hasReadGroup( "C1ELY.6" ) );
    REQUIRE ( ! open( CSRA1_PrimaryOnly ).hasReadGroup( "non-existent read group" ) );
}

FIXTURE_TEST_CASE(CSRA1_ReadGroup_GetStatistics, CSRA1_Fixture)
{
    ngs :: ReadGroup rg = open ( CSRA1_WithGroups ) . getReadGroup ( "GS57510-FS3-L03" );
    ngs :: Statistics stats = rg . getStatistics ();

    REQUIRE_EQ ( (uint64_t)34164461870, stats . getAsU64 ( "BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)34164461870, stats . getAsU64 ( "BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)488063741,   stats . getAsU64 ( "SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)5368875807,  stats . getAsU64 ( "SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)4880812067,  stats . getAsU64 ( "SPOT_MIN" ) );
}

/////TODO: ReadGroupIterator
FIXTURE_TEST_CASE(CSRA1_ReadGroupIterator_AfterNext, CSRA1_Fixture)
{
    ngs :: ReadGroupIterator rg = open ( "ERR730927" ) . getReadGroups ();
    REQUIRE ( rg . nextReadGroup () );
    REQUIRE_EQ ( string ( "1#72" ),  rg . getName () );
}


//////////////////////////////////////////// Main
int main(int argc, char* argv[]) 
{
const char * p = getenv("http_proxy");
cerr << "http_proxy = '" << ( p == NULL ? "NULL" : p ) << "'\n";
    KConfigDisableUserSettings();
    int rc=NgsCsra1CppTestSuite(argc, argv);

    NgsFixture::ReleaseCache();
    return rc;
}
