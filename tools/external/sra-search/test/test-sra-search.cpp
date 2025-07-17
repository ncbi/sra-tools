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

#include <kfg/config.h> /* KConfigDisableUserSettings */
#include <ktst/unit_test.hpp>

#include "VdbSearchFixture.hpp"

using namespace std;
using namespace ncbi::NK;
using namespace ngs;

#define SHOW_UNIMPLEMENTED 0

TEST_SUITE(SraSearchTestSuite);

//TODO: bad query string
//TODO: bad accession name
FIXTURE_TEST_CASE ( Create_Destroy, VdbSearchFixture )
{
    VdbSearch :: Settings s;
    s . m_query = "ACGT";
    m_s = new VdbSearch ( s );
}

FIXTURE_TEST_CASE ( SupportedAlgorithms, VdbSearchFixture )
{
    VdbSearch :: SupportedAlgorithms algs = m_s -> GetSupportedAlgorithms ();
    REQUIRE_LT ( ( size_t ) 0, algs . size () );
}

FIXTURE_TEST_CASE ( SingleAccession_FirstMatches, VdbSearchFixture )
{
    const string Accession = "SRR000001";
    SetupSingleThread ( "A", VdbSearch :: FgrepDumb, Accession ); // will hit (almost) every fragment

    REQUIRE_EQ ( string ( "SRR000001.FR0.1" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.2" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR1.2" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.3" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR1.3" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( SingleAccession_DefaultFormat, VdbSearchFixture )
{
    const string Accession = "SRR000001";
    SetupSingleThread ( "A", VdbSearch :: FgrepDumb, Accession ); // will hit (almost) every fragment
    REQUIRE ( NextMatch () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.1" ), m_result . m_formatted );
}

FIXTURE_TEST_CASE ( SingleAccession_FastaFormat, VdbSearchFixture )
{
    const string Accession = "SRR000001";
    m_settings . m_fasta = true;
    SetupSingleThread ( "A", VdbSearch :: FgrepDumb, Accession ); // will hit (almost) every fragment
    REQUIRE ( NextMatch () );
    REQUIRE_EQ (
        string (
            ">SRR000001.FR0.1\n"
            "ATTCTCCTAGCCTACATCCGTACGAGTTAGCGTGGGATTACGAGGTGCACACCATTTCATTCCGTACGGG\n"
            "TAAATTTTTGTATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCT\n"
            "GACCTCAAGTGATTTGCCTGCCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTG\n"
            "CCCAAGCCTTATAAGTAAATTTATAAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG\n"
        ),
        m_result . m_formatted );
}

FIXTURE_TEST_CASE ( SingleAccession_FastaLineLength, VdbSearchFixture )
{
    const string Accession = "SRR000001";
    m_settings . m_fasta = true;
    m_settings . m_fastaLineLength = 140;
    SetupSingleThread ( "A", VdbSearch :: FgrepDumb, Accession ); // will hit (almost) every fragment
    REQUIRE ( NextMatch () );
    REQUIRE_EQ (
        string (
            ">SRR000001.FR0.1\n"
            "ATTCTCCTAGCCTACATCCGTACGAGTTAGCGTGGGATTACGAGGTGCACACCATTTCATTCCGTACGGGTAAATTTTTGTATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCT\n"
            "GACCTCAAGTGATTTGCCTGCCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTATAAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG\n"
        ),
        m_result . m_formatted );
}

FIXTURE_TEST_CASE ( SingleAccession_MaxMatches, VdbSearchFixture )
{
    const string Accession = "SRR000001";
    m_settings . m_maxMatches = 3;
    SetupSingleThread ( "A", VdbSearch :: FgrepDumb, Accession ); // will hit (almost) every fragment, only return the first 3

    REQUIRE_EQ ( string ( "SRR000001.FR0.1" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.2" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR1.2" ), NextFragmentId () );
    REQUIRE ( ! NextMatch () );
}

FIXTURE_TEST_CASE ( FgrepDumb_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: FgrepDumb, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( FgrepBoyerMoore_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: FgrepBoyerMoore, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( FgrepAho_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: FgrepAho, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( AgrepDP_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{   /* VDB-2681: AgrepDP algorithm is broken */
    SetupSingleThread ( "ATTAGC", VdbSearch :: AgrepDP, "SRR000001" );
    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( AgrepWuManber_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: AgrepWuManber, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( AgrepMyers_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: AgrepMyers, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( AgrepMyersUnltd_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: AgrepMyersUnltd, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( NucStrstr_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: NucStrstr, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( SmithWaterman_SingleAccession_HitsAcrossFragments, VdbSearchFixture )
{
    SetupSingleThread ( "ATTAGC", VdbSearch :: SmithWaterman, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( MultipleAccessions, VdbSearchFixture )
{
    const string Sra1 = "SRR600096";
    const string Sra2 = "SRR000001";
    m_settings . m_accessions . push_back(Sra1);
    m_settings . m_accessions . push_back(Sra2);
    SetupSingleThread ( "ACGTACG", VdbSearch :: NucStrstr );

    REQUIRE_EQ ( Sra1 + ".FR1.5", NextFragmentId () );
    REQUIRE_EQ ( Sra2 + ".FR0.26",   NextFragmentId () );
    REQUIRE_EQ ( Sra2 + ".FR0.717",   NextFragmentId () );
    REQUIRE_EQ ( Sra2 + ".FR0.951",  NextFragmentId () );
}

FIXTURE_TEST_CASE ( NucStrstr_Expression, VdbSearchFixture )
{
    m_settings . m_isExpression = true;
    SetupSingleThread ( "AAAAAAACCCCCCC||ATTAGC", VdbSearch :: NucStrstr, "SRR000001" );

    REQUIRE_EQ ( string ( "SRR000001.FR0.23" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.36" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( Expression_OnlyForNucStrstr, VdbSearchFixture )
{
    m_settings . m_isExpression = true;
    REQUIRE_THROW ( SetupSingleThread ( "AAAAAAA||ATTAGC", VdbSearch :: SmithWaterman, "SRR000001" ) );
}
// Imperfect matches
FIXTURE_TEST_CASE ( FgrepDumb_ImperfectMatch_Unsupported, VdbSearchFixture )
{
    m_settings . m_minScorePct =  90;
    REQUIRE_THROW ( SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: FgrepDumb, "SRR000001" ) );
}
FIXTURE_TEST_CASE ( FgrepBoyerMoore_ImperfectMatch_Unsupported, VdbSearchFixture )
{
    m_settings . m_minScorePct =  90;
    REQUIRE_THROW ( SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: FgrepBoyerMoore, "SRR000001" ) );
}
FIXTURE_TEST_CASE ( FgrepAho_ImperfectMatch_Unsupported, VdbSearchFixture )
{
    m_settings . m_minScorePct =  90;
    REQUIRE_THROW ( SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: FgrepAho, "SRR000001" ) );
}

FIXTURE_TEST_CASE ( AgrepDP_ImperfectMatch, VdbSearchFixture )
{
    m_settings . m_minScorePct =  90;
    SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: AgrepDP, "SRR000001" );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.2944" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.3608" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( AgrepWuManber_ImperfectMatch, VdbSearchFixture )
{
    m_settings . m_minScorePct =  90;
    SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: AgrepWuManber, "SRR000001" );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.2944" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.3608" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( AgrepMyers_ImperfectMatch, VdbSearchFixture )
{
    m_settings . m_minScorePct =  90;
    SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: AgrepMyers, "SRR000001" );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.2944" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.3608" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( AgrepMyersUnltd_ImperfectMatch, VdbSearchFixture )
{
    m_settings . m_minScorePct =  90;
    SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: AgrepMyersUnltd, "SRR000001" );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.2944" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.3608" ), NextFragmentId () );
}

FIXTURE_TEST_CASE ( SmithWaterman_ImperfectMatch, VdbSearchFixture )
{   // SW scoring function is different from Agrep's, so the results are slightly different
    m_settings . m_minScorePct =  90;
    SetupSingleThread ( "ATTAGCATTAGC", VdbSearch :: SmithWaterman, "SRR000001" );
    REQUIRE_EQ ( string ( "SRR000001.FR0.141" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.183" ), NextFragmentId () );
    REQUIRE_EQ ( string ( "SRR000001.FR0.2944" ), NextFragmentId () );
}

int
main( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return SraSearchTestSuite(argc, argv);
}
