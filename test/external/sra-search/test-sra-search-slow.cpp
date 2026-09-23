/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author'm_s official duties as a United States Government employee and
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
#include <ktst/unit_test.hpp>

#include "VdbSearchFixture.hpp"

using namespace std;
using namespace ncbi::NK;
using namespace ngs;

#define SHOW_UNIMPLEMENTED 0

TEST_SUITE(SraSearchSlowTestSuite);

///////// Multi threading

FIXTURE_TEST_CASE ( SingleAccession_Threaded, VdbSearchFixture )
{
    const string Sra1 = "SRR600094";
    SetupMultiThread ( "ACGTAGGGTCC", VdbSearch :: NucStrstr, 2, Sra1 );

    REQUIRE_EQ ( Sra1 + ".FR1.101989", NextFragmentId () );
    REQUIRE_EQ ( Sra1 + ".FR0.101990", NextFragmentId () );
}

FIXTURE_TEST_CASE ( Threads_RandomCrash, VdbSearchFixture )
{   // used to crash randomly
    SetupMultiThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, 4, "SRR000001" ); // 4 blob-based threads on one run

    unsigned int count = 0;
    while ( NextMatch () )  // used to have a random crash inside VDB
    {
        ++count;
    }
    REQUIRE_EQ ( 12u, count );
}

// Reference-driven mode

FIXTURE_TEST_CASE ( ReferenceDriven_ReferenceNotFound, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_references . push_back ( ReferenceSpec ( "NOT_ME_GUV" ) );
    SetupSingleThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, "SRR600094" );
    REQUIRE ( ! NextMatch () );
}

FIXTURE_TEST_CASE ( ReferenceDriven_ReferenceNotFound_MultiThread, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_references . push_back ( ReferenceSpec ( "NOT_ME_GUV" ) );
    SetupMultiThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, 2, "SRR600094" );
    REQUIRE ( ! NextMatch () );
}

FIXTURE_TEST_CASE ( ReferenceDriven_NotCSRA, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    SetupSingleThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, "SRR000001" );
    // No references or alignments in the archive, this becomes a scan of SEQUENCE table
    REQUIRE_EQ ( string ( "SRR000001.FR0.28322" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.65088" ), NextFragmentId () );
    // etc
}

FIXTURE_TEST_CASE ( ReferenceDriven_SingleReference_SingleAccession, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = false;
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000007.13" ) );
    SetupSingleThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, "SRR600094" );

    REQUIRE_EQ ( string ( "SRR600094.FR1.1053649" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053650" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053648" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053651" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053652" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR1.1053653" ), NextFragmentId () );
    REQUIRE ( ! NextMatch () );
}

FIXTURE_TEST_CASE ( ReferenceDriven_AllReferences_NoDuplicates, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = false;
    SetupSingleThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, "SRR600094" );

/*
SRR600094.FR1.101989
SRR600094.FR0.101990
SRR600094.FR0.101991
SRR600094.FR0.324216    Not reported in reference mode b/c matches are in clipped portions of the read
SRR600094.FR1.1053649
etc
*/
    REQUIRE_EQ ( string ( "SRR600094.FR1.101989" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101990" ), NextFragmentId () );
            // REQUIRE_EQ ( string ( "SRR600094.FR1.101989" ), NextFragmentId () ); // used to be duplicates
            // REQUIRE_EQ ( string ( "SRR600094.FR0.101990" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101991" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( ReferenceDriven_AllReferences_NoDuplicates_Blobs, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = true;
    SetupSingleThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, "SRR600094" );

    REQUIRE_EQ ( string ( "SRR600094.FR1.101989" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101990" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101991" ), NextFragmentId () );
}

#if SHOW_UNIMPLEMENTED
FIXTURE_TEST_CASE ( ReferenceDriven_MatchAcrossBlobBoundary, VdbSearchFixture )
{
    const string Query = "TTGAAGAGATCCGACATCA";
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = true;
    m_settings . m_accessions . push_back("SRR600094");
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000001.10", 14936, 15011 ) ); // crosses the blob boundary (5000)

    SetupSingleThread ( Query, VdbSearch :: FgrepDumb );
    REQUIRE_EQ ( string("SRR600094.FR0.1647758"), NextFragmentId () ); // aligns into the above region, across the blob boundary
}
#endif

FIXTURE_TEST_CASE ( ReferenceDriven_Blobs_MatchAcrossEndOfCirular, VdbSearchFixture )
{
    // read=TATTGTGATGTTTTATTTAAGGGGAATGTGTGGGTTATTTAGGTTTTATGATTTTGAAGTAGGAATTAGATGTTGGATATAGTTTATTTTAGTTCCATAACACTTAAAAATAACTAAAATAAACTATATCCAACATCTAATTCCTACTTCAAAATCATAAAACCTAAATAACCCACACATTCCCCTTA
    // ref=GATCACAGG...CATCACGATG
    const string Query = "TGGAT";
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = true;
    m_settings . m_accessions . push_back("SRR1769246");
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_012920.1" ) );

    SetupSingleThread ( Query, VdbSearch :: FgrepDumb );
    while ( NextMatch () )
    {
        if ( string("SRR1769246.FR0.1638021") == m_result . m_fragmentId )
        {
            return;
        }
    }
    FAIL ("SRR1769246.FR0.1638021 not found");
}

FIXTURE_TEST_CASE ( ReferenceDriven_NoBlobs_MatchAcrossEndOfCirular, VdbSearchFixture )
{
    // read=TATTGTGATGTTTTATTTAAGGGGAATGTGTGGGTTATTTAGGTTTTATGATTTTGAAGTAGGAATTAGATGTTGGATATAGTTTATTTTAGTTCCATAACACTTAAAAATAACTAAAATAAACTATATCCAACATCTAATTCCTACTTCAAAATCATAAAACCTAAATAACCCACACATTCCCCTTA
    // ref=GATCACAGG...CATCACGATG
    const string Query = "TGGAT";
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = false;
    m_settings . m_accessions . push_back("SRR1769246");
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_012920.1" ) );

    SetupSingleThread ( Query, VdbSearch :: FgrepDumb );
    while ( NextMatch () )
    {
        if ( string("SRR1769246.FR0.1638021") == m_result . m_fragmentId )
        {
            return;
        }
    }
    FAIL ("SRR1769246.FR0.1638021 not found");
}

FIXTURE_TEST_CASE ( ReferenceDriven_MultipleReferences_SingleAccession, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = true;
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000007.13" ) );
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000001.10" ) );
    SetupSingleThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb, "SRR600094" );

    // on NC_000007.13
    REQUIRE_EQ ( string ( "SRR600094.FR1.1053649" ),  NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053650" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053648" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053651" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053652" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR1.1053653" ), NextFragmentId () );
    // NC_000001.10
    REQUIRE_EQ ( string ( "SRR600094.FR1.101989" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101990" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101991" ), NextFragmentId () );
    // there are matches on other references, not reported here
    REQUIRE ( ! NextMatch () );
}

FIXTURE_TEST_CASE ( ReferenceDriven_MultipleReferences_MultipleAccessions, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = true;
    m_settings . m_accessions . push_back ( "SRR600095" );
    m_settings . m_accessions . push_back ( "SRR600094" );
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000007.13" ) );
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000001.10" ) );
    SetupSingleThread ( "ACGTAGGGTCC", VdbSearch :: FgrepDumb );

    // SRR600095, on NC_000007.13
    REQUIRE_EQ ( string ( "SRR600095.FR1.694078" ), NextFragmentId () );
    // SRR600094, NC_000001.10
    REQUIRE_EQ ( string ( "SRR600095.FR1.69793" ), NextFragmentId () );
    // SRR600094, on NC_000007.13
    REQUIRE_EQ ( string ( "SRR600094.FR1.1053649" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053650" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053648" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053651" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1053652" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR1.1053653" ), NextFragmentId () );
    // SRR600094, NC_000001.10
    REQUIRE_EQ ( string ( "SRR600094.FR1.101989" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101990" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.101991" ), NextFragmentId () );
    // there are matches on other references, not reported here
    REQUIRE ( ! NextMatch () );
}

// Reference-driven mode on a reference slice
#if SHOW_UNIMPLEMENTED
FIXTURE_TEST_CASE ( ReferenceDriven_NoBlobs_SingleSlice_SingleAccession, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = false;
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000007.13", 81000000, 105000000 ) );
    SetupSingleThread ( "ACGTAGGGTC", VdbSearch :: FgrepDumb, "SRR600094" );

    // Match on NC_000007.13 at 104,782,835-104,782,845
    REQUIRE_EQ ( string ( "SRR600094.FR0.1125868" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1125870" ), NextFragmentId () );
    // Match on NC_000007.13 at 81,579,623-81,579,633 (reverse)
    REQUIRE_EQ ( string ( "SRR600094.FR1.1094914" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR1.1094915" ), NextFragmentId () );

    REQUIRE ( ! NextMatch () );
}

//TODO: multiple slices per reference

FIXTURE_TEST_CASE ( ReferenceDriven_Blobs_SingleSlice_SingleAccession, VdbSearchFixture )
{
    m_settings . m_referenceDriven = true;
    m_settings . m_useBlobSearch  = true;
    m_settings . m_references . push_back ( ReferenceSpec ( "NC_000007.13", 81575001, 105000000 ) );
    SetupSingleThread ( "ACGTAGGGTC", VdbSearch :: FgrepDumb, "SRR600094" );

    // Match on NC_000007.13 at 81,579,623-81,579,633 (reverse)
    REQUIRE_EQ ( string ( "SRR600094.FR1.1094914" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR1.1094915" ), NextFragmentId () );
    // Match on NC_000007.13 at 104,782,835-104,782,845
    REQUIRE_EQ ( string ( "SRR600094.FR0.1125868" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR600094.FR0.1125870" ), NextFragmentId () );

    REQUIRE ( ! NextMatch () );
}

//TODO: reference-driven, specify a single reference slice, match against different accessions
//TODO: reference-driven, specify multiple reference slices, single accession
//TODO: reference-driven, specify multiple reference slices, match against different accessions
//TODO: reference-driven search on a slice that wraps around the end of a circular reference
#endif

// Unaligned reads only
FIXTURE_TEST_CASE ( Unaligned, VdbSearchFixture )
{
    m_settings . m_unaligned  = true;
    SetupSingleThread ( "CACAG", VdbSearch :: FgrepDumb, "SRR600099" );

    REQUIRE_EQ ( string ( "SRR600099.FR0.1" ), NextFragmentId () );
    // there would be many hits on aligned fragments in between
    REQUIRE_EQ ( string ( "SRR600099.FR1.438" ), NextFragmentId () );
    //etc...
}

int
main( int argc, char *argv [] )
{
    return SraSearchSlowTestSuite(argc, argv);
}
