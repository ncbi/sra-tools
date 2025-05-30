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
* Unit tests for search
*/

#include <ktst/unit_test.hpp>
#include <kapp/main.h>

//#include <../libs/search/search-priv.h>
#include <search/nucstrstr.h>
#include <search/smith-waterman.h>

#include <stdexcept>
#include <limits>

#include <stdio.h>

#include "search-vdb.h"
#include <search/ref-variation.h>

#undef max

using namespace std;

static rc_t argsHandler(int argc, char* argv[]);
TEST_SUITE_WITH_ARGS_HANDLER(TestSuiteSearch, argsHandler);

static rc_t argsHandler(int argc, char* argv[]) {
    Args* args = NULL;
    rc_t rc = ArgsMakeAndHandle(&args, argc, argv, 0, NULL, 0);
    ArgsWhack(args);
    return rc;
}

void trim_eol(char* psz)
{
    size_t len = strlen(psz);
    if (psz[len - 2] == '\n' || psz[len - 2] == '\r')
        psz[len - 2] = '\0';
    else if (psz[len - 1] == '\n' || psz[len - 1] == '\r')
        psz[len - 1] = '\0';
}

#if 0
#include "PerfCounter.h"
#include <klib/checksum.h>

TEST_CASE(TempCRC)
{
    std::cout << "Testing crc32 speed..." << std::endl;
    size_t const size = (size_t)10 << 20;
    char* buf = new char [size];
    CPerfCounter counter( "CRC32" );

    for ( size_t i = 0; i < size; ++i )
    {
        buf[i] = i;
    }

    size_t const ALIGN_BYTES = _ARCH_BITS / 8;

    printf ("allocated %saligned (address=%p)\n", (int)((size_t)buf % ALIGN_BYTES) ? "un" : "", buf);

    size_t offset = 2;
    uint32_t crc32;

    {
        CPCount count(counter);
        crc32 = ::CRC32 ( 0, buf + offset, size - offset );
    }

    printf ("Caclulated CRC32: 0x%08X (%.2lf MB/s)\n", crc32, (size >> 20)/counter.GetSeconds());

    delete[]buf;
}
#endif

class AgrepFixture
{
public:
	AgrepFixture()
	: agrep_params ( 0 )
	{
	}
	~AgrepFixture()
	{
		::AgrepWhack ( agrep_params );
	}

	rc_t Setup ( const char* pattern, AgrepFlags flags )
	{
		pattern_len = strlen ( pattern );
		return ::AgrepMake ( & agrep_params, AGREP_MODE_ASCII | flags, pattern );
	}

	bool FindFirst ( const string& text, int32_t threshold )
	{
		return ::AgrepFindFirst ( agrep_params, threshold, text . c_str (), text . size (), & match_info ) != 0;
	}

	bool FindBest ( const string& text, int32_t threshold )
	{
		return ::AgrepFindBest ( agrep_params, threshold, text . c_str (), text . size (), & match_info ) != 0;
	}


public:
	size_t pattern_len;
	::AgrepParams* agrep_params;
    ::AgrepMatch match_info;
};

FIXTURE_TEST_CASE ( AgrepDPTest, AgrepFixture )
{
    REQUIRE_RC ( Setup ( "MATCH", AGREP_ALG_DP ) );

    // Complete match
    {
        REQUIRE ( FindFirst ( "MATCH", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }

    // Complete substring match
    {
        REQUIRE ( FindFirst ( "xxMATCHvv", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 2, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }
    // 1 Deletion
    {
        REQUIRE ( FindFirst ( "xxxMACHvv", 1 ) );
        REQUIRE_EQ ( pattern_len - 1, (size_t)match_info.length );
        REQUIRE_EQ ( 3, match_info.position );
        REQUIRE_EQ ( 1, match_info.score );
    }

    // 2 Insertions
    {
        REQUIRE ( FindFirst ( "xxxMAdTCaHvv", 2 ) );
        REQUIRE_EQ ( pattern_len + 2, (size_t)match_info.length );
        REQUIRE_EQ ( 3, match_info.position );
        REQUIRE_EQ ( 2, match_info.score );
    }

    // 3 Mismatches
    {
        REQUIRE ( FindFirst ( "xATxx", 5 ) );
        REQUIRE_EQ ( pattern_len /*2*/, (size_t)match_info.length ); // FIXME: 2 looks correct here
        REQUIRE_EQ ( 0, match_info.position );                       // 1
        REQUIRE_EQ ( 3, match_info.score );                          // 2
    }

    // Best match
    {
        REQUIRE ( FindBest ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 18, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }
    // First match
    {
        REQUIRE ( FindFirst ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE_EQ ( pattern_len - 1, (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( 1, match_info.score );
    }

    // Match anything
    {
        // threshold >= pattern_len seems to specify that a complete mismatch is acceptable
        // by implementation, the algorithm reports the result to be "found" at the tail portion of the reference string
        // for this degenerate case, the expected behavior is not clear, so I'll just document the reality here:
        const string text = "xyzvuwpiuuuu";
        REQUIRE ( FindFirst ( "xyzvuwpiuuuu", pattern_len ) );
        REQUIRE_EQ ( pattern_len + 1, (size_t)match_info.length );
        REQUIRE_EQ ( text . size () - ( pattern_len + 1 ), (size_t)match_info.position );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.score );
    }

    // Not found
    {
        REQUIRE ( ! FindFirst ( "xyzvuwpiu", 4 ) );
    }
}

FIXTURE_TEST_CASE ( AgrepWumanberTest, AgrepFixture )
{
    REQUIRE_RC ( Setup ( "MATCH", AGREP_ALG_WUMANBER ) );

    // Complete match
    {
        REQUIRE ( FindFirst ( "MATCH", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }

    // Complete substring match
    {
        REQUIRE ( FindFirst ( "xxMATCHvv", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 2, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }
    // 1 Deletion
    {
        REQUIRE ( FindFirst ( "xxxMACHvv", 1 ) );
        REQUIRE_EQ ( pattern_len - 1, (size_t)match_info.length );
        REQUIRE_EQ ( 3, match_info.position );
        REQUIRE_EQ ( 1, match_info.score );
    }

    // 2 Insertions
    {
        REQUIRE ( FindFirst ( "xxxMAdTCaHvv", 2 ) );
        REQUIRE_EQ ( pattern_len + 2, (size_t)match_info.length );
        REQUIRE_EQ ( 3, match_info.position );
        REQUIRE_EQ ( 2, match_info.score );
    }

    // 3 Mismatches
    {
        REQUIRE ( FindFirst ( "xATxx", 5 ) );
        REQUIRE_EQ ( pattern_len /*2*/, (size_t)match_info.length ); // FIXME: 2 looks correct here
        REQUIRE_EQ ( 0, match_info.position );                       // 1
        REQUIRE_EQ ( 3, match_info.score );                          // 2
    }

    // Best match
    {
        REQUIRE ( FindBest ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 18, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }
    // First match
    {
        REQUIRE ( FindFirst ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE_EQ ( pattern_len - 1, (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( 1, match_info.score );
    }

    // Match anything
    {   // threshold >= pattern_len seems to specify that a complete mismatch is acceptable
        const string text = "xyzvuwpiuuuu";
        REQUIRE ( FindFirst ( text, pattern_len ) );
        REQUIRE_EQ ( text . size (), (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.score );
    }

    // Not found
    {
        REQUIRE ( ! FindFirst ( "xyzvuwpiu", 4 ) );
    }
}

FIXTURE_TEST_CASE ( AgrepMyersTest, AgrepFixture )
{
    REQUIRE_RC ( Setup ( "MATCH", AGREP_ALG_MYERS ) );

    // Complete match
    {
        REQUIRE ( FindFirst ( "MATCH", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }

    // Complete substring match
    {
        REQUIRE ( FindFirst ( "xxMATCHvv", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 2, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }
    // 1 Deletion
    {
        REQUIRE ( FindFirst ( "xxxMACHvv", 1 ) );
        REQUIRE_EQ ( pattern_len - 1, (size_t)match_info.length );
        REQUIRE_EQ ( 3, match_info.position );
        REQUIRE_EQ ( 1, match_info.score );
    }

    // 2 Insertions
    {
        REQUIRE ( FindFirst ( "xxxMAdTCaHvv", 2 ) );
//        REQUIRE_EQ ( pattern_len + 2, (size_t)match_info.length );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length ); //FIXME: different from other algorithms
        REQUIRE_EQ ( 3, match_info.position );
        REQUIRE_EQ ( 2, match_info.score );
    }

    // 3 Mismatches
    {
        REQUIRE ( FindFirst ( "xATxx", 5 ) );
        REQUIRE_EQ ( 2, match_info.length  );
        REQUIRE_EQ ( 1, match_info.position );
        REQUIRE_EQ ( 3, match_info.score );
    }

    // Best match
    {
        REQUIRE ( FindBest ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 18, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }
    // First match
    {
        REQUIRE ( FindFirst ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE_EQ ( pattern_len - 1, (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( 1, match_info.score );
    }

    // Match anything
    {   // threshold >= pattern_len seems to specify that a complete mismatch is acceptable
        const string text = "xyzvuwpiuuuu";
        REQUIRE ( FindFirst ( text, pattern_len ) );
//        REQUIRE_EQ ( text . size (), (size_t)match_info.length );
        REQUIRE_EQ ( 1, match_info.length ); //FIXME: different from other algorithms
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.score );
    }

    // Not found
    {
        REQUIRE ( ! FindFirst ( "xyzvuwpiu", 4 ) );
    }
}

FIXTURE_TEST_CASE ( AgrepMyersUnltdTest, AgrepFixture )
{
    REQUIRE_RC ( Setup ( "MATCH", AGREP_ALG_MYERS_UNLTD ) );

    // Complete match
    {
        REQUIRE ( FindFirst ( "MATCH", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }

    // Complete substring match
    {
        REQUIRE ( FindFirst ( "xxMATCHvv", 0 ) );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.length );
        REQUIRE_EQ ( 2, match_info.position );
        REQUIRE_EQ ( 0, match_info.score );
    }
    // 1 Deletion
    {
        REQUIRE ( FindFirst ( "xxxMACHvv", 1 ) );
        REQUIRE_EQ ( pattern_len - 1, (size_t)match_info.length );
        REQUIRE_EQ ( 3, match_info.position );
        REQUIRE_EQ ( 1, match_info.score );
    }

    // 2 Insertions
    {
        REQUIRE ( FindFirst ( "xxxMAdTCaHvv", 2 ) );
//        REQUIRE_EQ ( pattern_len + 2, (size_t)match_info.length );
        REQUIRE_EQ ( (size_t)pattern_len - 1, (size_t)match_info.length ); //FIXME: different from other algorithms
        REQUIRE_EQ ( (int32_t)3, match_info.position );
        REQUIRE_EQ ( (int32_t)2, match_info.score );
    }

    // 3 Mismatches
    {
        REQUIRE ( FindFirst ( "xATxx", 5 ) );
        REQUIRE_EQ ( 2, match_info.length );
        REQUIRE_EQ ( 1, match_info.position );
        REQUIRE_EQ ( 3, match_info.score );
    }

    // Best match
    {
        REQUIRE ( FindBest ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE ( (size_t)match_info.length == pattern_len );
        REQUIRE ( match_info.position == 18 );
        REQUIRE ( match_info.score == 0 );
    }
    // First match
    {
        REQUIRE ( FindFirst ( "MTCH__MITCH_MTACH_MATCH_MATCH", 1 ) );
        REQUIRE ( (size_t)match_info.length == pattern_len - 1 );
        REQUIRE ( match_info.position == 0 );
        REQUIRE ( match_info.score == 1 );
    }

    // Match anything
    {   // threshold >= pattern_len seems to specify that a complete mismatch is acceptable
        const string text = "xyzvuwpiuuuu";
        REQUIRE ( FindFirst ( text, pattern_len ) );
//        REQUIRE_EQ ( text . size (), (size_t)match_info.length );
        REQUIRE_EQ ( 1, match_info.length ); //FIXME: different from other algorithms
        REQUIRE_EQ ( 0, match_info.position );
        REQUIRE_EQ ( pattern_len, (size_t)match_info.score );
    }

    // Not found
    {
        REQUIRE ( ! FindFirst ( "xyzvuwpiu", 4 ) );
    }
}


TEST_CASE(SearchCompare)
{
    //std::cout << "This is search algorithm time comparison test" << std::endl << std::endl;
    std::cout << "SearchCompare test is currently OFF" << std::endl;

    /*
    TimeTest("SRR067432", SearchTimeTest::ALGORITHM_AGREP);
    TimeTest("SRR067432", SearchTimeTest::ALGORITHM_LCS_DP);
    TimeTest("SRR067432", SearchTimeTest::ALGORITHM_NUCSTRSTR);
    TimeTest("SRR067432", SearchTimeTest::ALGORITHM_LCS_SAIS);*/

    //std::cout << std::endl << "Time comparison test acomplished" << std::endl;
}

TEST_CASE(TestCaseAgrep)
{
    std::cout << "TestCaseAgrep test is currently OFF" << std::endl;
    //VDBSearchTest::CFindLinker().Run(AGREP_MODE_ASCII|AGREP_ALG_MYERS, "SRR067432");//"SRR068408");
    //VDBSearchTest::CFindLinker().Run(AGREP_MODE_ASCII|AGREP_ALG_MYERS, "SRR067408_extr");

/*
    char const pszRunNamesFile[] = "454Runs.txt";
    FILE* fRuns = fopen(pszRunNamesFile, "r");
    if (!fRuns)
    {
        printf("Failed to open file: %s\n", pszRunNamesFile);
        return;
    }
    bool bSkipping = true;
    for (; !feof(fRuns); )
    {
        char szRuns[512] = "";
        char* psz = fgets(szRuns, sizeof(szRuns), fRuns);
        if (!psz)
            break;
        trim_eol(psz);
        if (bSkipping && !strcmp(psz, "SRR067408"))
        {
            bSkipping = false;
        }
        if (bSkipping)
            continue;
        VDBSearchTest::CFindLinker().Run(AGREP_MODE_ASCII|AGREP_ALG_MYERS, psz);
    }*/
}

// Fgrep

static void RunFgrep ( FgrepFlags p_alg )
{   // VDB-2669: creates uninitialized memory
    Fgrep* fg;
    const char* queries[] = { "RRRRRAH" };
    if ( FgrepMake ( & fg, FGREP_MODE_ASCII | p_alg, queries, 1 ) ) // this used to leave uninitialized memory ...
        throw logic_error ( "RunFgrep: FgrepMake() failed" );

    const std::string str ( 1000, 'A' );
    FgrepMatch matchinfo;
    if ( 0 != FgrepFindFirst ( fg, str . data (), str . size (), & matchinfo ) ) // ... the use of which showed up in valgrind here, and sometimes caused a crash
        throw logic_error ( "RunFgrep: FgrepFindFirst() found a false match" );

    FgrepFree ( fg );
}

TEST_CASE ( DumbGrep_Crash )
{
    RunFgrep ( FGREP_ALG_DUMB );
}

TEST_CASE ( BoyerMooreGrep_Crash )
{
    RunFgrep ( FGREP_ALG_BOYERMOORE );
}

TEST_CASE ( AhoGrep_Crash )
{
    RunFgrep ( FGREP_ALG_AHOCORASICK );
}

// Smith-Waterman

class SmithWatermanFixture
{
public:
	SmithWatermanFixture()
	:  m_self ( 0 ),
       m_pattern_len ( 0 )
	{
	}
	~SmithWatermanFixture()
	{
		::SmithWatermanWhack ( m_self );
	}

	rc_t Setup ( const string& p_query )
	{
        m_pattern_len = p_query . size ();
		return ::SmithWatermanMake ( & m_self, p_query . data() );
	}

	bool FindFirst ( const string& p_text, uint32_t p_threshold = numeric_limits<uint32_t>::max() )
	{
		return ::SmithWatermanFindFirst ( m_self,  p_threshold, p_text . c_str (), p_text . size (), & m_match_info ) == 0;
	}

public:
	::SmithWaterman*       m_self;
	size_t                 m_pattern_len;
    ::SmithWatermanMatch   m_match_info;
};

FIXTURE_TEST_CASE ( SmithWatermanTest, SmithWatermanFixture )
{
    REQUIRE_RC ( Setup ( "MATCH" ) );

    // VDB-3034: crash on query of length 0, when called right after initialization of the search object
    {
        REQUIRE(!FindFirst(""));
    }

	// threshold in FirstMatch varies from 0 (a complete mismatch is acceptable) to 2*m_pattern_len (perfect match)

    // Complete match
    {
        REQUIRE ( FindFirst ( "MATCH" ) );
        REQUIRE_EQ ( m_pattern_len,  ( size_t ) m_match_info.length );
        REQUIRE_EQ ( 0, m_match_info.position );
        REQUIRE_EQ ( 2 * m_pattern_len, ( size_t ) m_match_info.score );
    }
    // Complete substring match
    {
        REQUIRE ( FindFirst ( "xxMATCHvv", 0 ) );
        REQUIRE_EQ ( m_pattern_len, (size_t)m_match_info.length );
        REQUIRE_EQ ( 2, m_match_info.position );
        REQUIRE_EQ ( 2 * m_pattern_len, ( size_t ) m_match_info.score );
    }
    // 1 Deletion
    {
        REQUIRE ( FindFirst ( "xxxMACHvv", 7 ) );
        REQUIRE_EQ ( m_pattern_len - 1, (size_t)m_match_info.length );
        REQUIRE_EQ ( 3, m_match_info.position );
        REQUIRE_EQ ( 7, m_match_info.score );
    }

    // 2 Insertions
    {
        REQUIRE ( FindFirst ( "xxxMAdTCaHvv", 8 ) );
        REQUIRE_EQ ( m_pattern_len + 2, (size_t)m_match_info.length );
        REQUIRE_EQ ( 3, m_match_info.position );
        REQUIRE_EQ ( 8, m_match_info.score );
    }

    // 3 Mismatches
    {
        REQUIRE ( FindFirst ( "xATxx", 4 ) );
        REQUIRE_EQ ( 2, m_match_info.length );
        REQUIRE_EQ ( 1, m_match_info.position );
        REQUIRE_EQ ( 4, m_match_info.score );
    }

    // First match
    {
        REQUIRE ( FindFirst ( "MTCH__MITCH_MTACH_MATCH_MATCH" ) );
        REQUIRE_EQ ( m_pattern_len, (size_t)m_match_info.length );
        REQUIRE_EQ ( 18, m_match_info.position );
        REQUIRE_EQ ( 2 * m_pattern_len,  (size_t)m_match_info.score );
    }

    // Match anything
    {   // threshold is from 0 (a complete mismatch is acceptable) to 2*m_pattern_len (perfect match)
        const string text = "xyzvuwpiuuuu";
        REQUIRE ( FindFirst ( text, 0 ) );
        REQUIRE_EQ ( 0, m_match_info.length );
        REQUIRE_EQ ( 0, m_match_info.position );
        REQUIRE_EQ ( 0, m_match_info.score );
    }

    // Not found
    {
        REQUIRE ( ! FindFirst ( "xyzvuwpiu", 1 ) );
    }

}

// Ref-Variation
#if 0
static
void
PrintMatrix ( const INSDC_dna_text* p_ref, const INSDC_dna_text* p_query, const int p_matrix[], size_t p_rows, size_t p_cols )
{
    cout << "    ";
    while ( *p_query )
    {
        cout << " " << *p_query;
        ++p_query;
    }
    cout << endl;
    for ( size_t i = 0; i < p_rows - 1; ++i ) // skip row 0 ( all 0s )
    {
        cout << i << " " << p_ref[i] << ": ";
        for ( size_t j = 0; j < p_cols - 1; ++j ) // skip the 0 at the start
        {
            cout << p_matrix [ ( i + 1 ) * p_cols + j + 1 ] << " ";
        }
        cout << endl;
    }
}
#endif

TEST_CASE ( RefVariation_crash)
{
    RefVariation* self;
    INSDC_dna_text ref[] = "ACGTACGTACGTACGTACGTACGTACGTACGT";
    REQUIRE_RC_FAIL ( RefVariationIUPACMake ( &self, ref, string_size ( ref ), 0, 0, "", 0, ::refvarAlgSW ) );
}

#ifdef _WIN32
#define PRSIZET "I"
#else
#define PRSIZET "z"
#endif

string print_refvar_obj (::RefVariation const* obj)
{
    size_t allele_len = 0, allele_start = 0, allele_len_on_ref = 0;
    char const* allele = NULL;
    ::RefVariationGetAllele( obj, & allele, & allele_len, & allele_start );
    ::RefVariationGetAlleleLenOnRef ( obj, & allele_len_on_ref );

    //printf ("<no ref name>:%"PRSIZET"u:%"PRSIZET"u:%.*s\n", allele_start, allele_len_on_ref, (int)allele_len, allele);

    return string ( allele, allele_len );
}

#undef PRSIZET


string vrefvar_bounds (::RefVarAlg alg, char const* ref,
    size_t ref_len, size_t pos, size_t len_on_ref,
    char const* query, size_t query_len)
{

    ::RefVariation* obj;

    rc_t rc = ::RefVariationIUPACMake ( & obj, ref, ref_len, pos, len_on_ref, query, query_len, alg );

    string ret = print_refvar_obj ( obj );

    if ( rc == 0 )
        ::RefVariationRelease( obj );

    return ret;
}

string vrefvar_bounds_n(::RefVarAlg alg)
{
    //                  01234567890123456789
    char const ref[] = "NNNNNNNNNNTAACCCTAAC";
    //                       CCCCTTAGG-

    size_t pos = 5, len_on_ref = 10;
    char const query[] = "CCCCTTAGG";

    return vrefvar_bounds ( alg, ref, strlen(ref), pos, len_on_ref, query, strlen(query) );
}

string vrefvar_bounds_0(::RefVarAlg alg)
{
    //                  01234567890123456789
    char const ref[] = "TAACCCTAAC";
    //                  TTAGG-

    size_t pos = 0, len_on_ref = 5;
    char const query[] = "TAGG";

    return vrefvar_bounds ( alg, ref, strlen(ref), pos, len_on_ref, query, strlen(query) );
}

string vrefvar_bounds_N0(::RefVarAlg alg)
{
    //                  01234567890123456789
    char const ref[] = "NNNNNTAACCCTAAC";
    //                  CCCCTTTAGG-

    size_t pos = 0, len_on_ref = 10;
    char const query[] = "CCCCTTAGG";

    return vrefvar_bounds ( alg, ref, strlen(ref), pos, len_on_ref, query, strlen(query) );
}

TEST_CASE ( RefVariation_bounds_N )
{
    //printf ("TODO: this test is derived from the real example which hangs up now (2015-12-14):\n");
    //printf ("echo \"67068302 NC_000001.10:9995:10:CCCCTTAGG\" | var-expand --algorithm=sw\n");

    REQUIRE_EQ ( string ( "NNNNNCCCCTTAGGCTAA" ), vrefvar_bounds_n ( ::refvarAlgSW ) );
    REQUIRE_EQ ( string ( "CCCCTTAGGC" ), vrefvar_bounds_n ( ::refvarAlgRA ) );

    REQUIRE_EQ ( string ( "AGGC" ), vrefvar_bounds_0 ( ::refvarAlgSW ) );
    REQUIRE_EQ ( string ( "TAGG" ), vrefvar_bounds_0 ( ::refvarAlgRA ) );

    REQUIRE_EQ ( string ( "CCCCTTAGGCTAA" ), vrefvar_bounds_N0 ( ::refvarAlgSW ) );
    REQUIRE_EQ ( string ( "CCCCTTAGGC" ), vrefvar_bounds_N0 ( ::refvarAlgRA ) );
}

// Nucstrstr
static
void
ConvertAsciiTo2NAPacked ( const string& p_read, unsigned char* pBuf2NA, size_t nBuf2NASize )
{
    static unsigned char map [ 1 << ( sizeof ( char ) * 8 ) ];
    map[(unsigned)'A'] = map[(unsigned)'a'] = 0;
    map[(unsigned)'C'] = map[(unsigned)'c'] = 1;
    map[(unsigned)'G'] = map[(unsigned)'g'] = 2;
    map[(unsigned)'T'] = map[(unsigned)'t'] = 3;

    static size_t shiftLeft [ 4 ] = { 6, 4, 2, 0 };

    fill ( pBuf2NA, pBuf2NA + nBuf2NASize, (unsigned char)0 );

    for ( size_t iChar = 0; iChar < p_read . size (); ++iChar )
    {
        size_t iByte = iChar / 4;
        if ( iByte > nBuf2NASize )
        {
            assert ( false );
            break;
        }

        pBuf2NA[iByte] |= map [ size_t ( p_read [ iChar ] ) ] << shiftLeft [ iChar % 4 ];
    }
}

static
int
RunNucStrtr ( const string& p_ref, const string& p_query, bool p_positional )
{
    unsigned char buf2na [ 1024 ];
    ConvertAsciiTo2NAPacked ( p_ref, buf2na, sizeof ( buf2na ) );

    NucStrstr *nss;
    if ( NucStrstrMake ( & nss, p_positional ? 1 : 0, p_query . c_str (), p_query . size () ) != 0 )
        throw logic_error ( "RunNucStrtr: NucStrstrMake() failed" );
    unsigned int selflen = 0u;
    // NB: for now, all searches start are from the beginning ( nucstrstr.h says "may be >= 4"; not sure what that means )
    const unsigned int pos = 0;
    int ret = NucStrstrSearch ( nss, ( const void * ) buf2na, pos, p_ref . size () - pos, & selflen );
    NucStrstrWhack ( nss );
    return ret;
}

TEST_CASE ( Nucstrstr_NonPositional_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "ACTA", false ) );
}

TEST_CASE ( Nucstrstr_NonPositional_Found_AtStart )
{
    REQUIRE_NE ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "ACGTACGTACG", false ) );
}

TEST_CASE ( Nucstrstr_NonPositional_Found_InMiddle )
{
    REQUIRE_NE ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "GTACGTACG", false ) );
}

TEST_CASE ( Nucstrstr_Positional_NoExpr_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "ACCGT", true ) );
}

TEST_CASE ( Nucstrstr_Positional_NoExpr_Found_AtStart )
{
    REQUIRE_EQ ( 1, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "ACGTACGTACG", true ) );
}

TEST_CASE ( Nucstrstr_Positional_NoExpr_Found_InMiddle )
{
    REQUIRE_EQ ( 4, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "TACGTACG", true ) );
}

TEST_CASE ( Nucstrstr_Positional_Not_False )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "!TACGTACG", true ) );
}
TEST_CASE ( Nucstrstr_Positional_Not_True )
{   // "!" returns only 0 or 1, not the position
    REQUIRE_EQ ( 1, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "!AG", true ) );
}

TEST_CASE ( Nucstrstr_Positional_Caret_Found )
{
    REQUIRE_EQ ( 1, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "^AC", true ) );
}
TEST_CASE ( Nucstrstr_Positional_Caret_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "^C", true ) );
}

TEST_CASE ( Nucstrstr_Positional_Dollar_Found )
{
    REQUIRE_EQ ( 33, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGTTGCA", "TGCA$", true ) );
}
TEST_CASE ( Nucstrstr_Positional_Dollar_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGTTGCAA", "TGCA$", true ) );
}

TEST_CASE ( Nucstrstr_Positional_OR_Found_1 )
{
    REQUIRE_EQ ( 4, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGTTGCA", "TGAA|TACG", true ) );
}
TEST_CASE ( Nucstrstr_Positional_OR_Found_2 )
{
    REQUIRE_EQ ( 4, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGTTGCA", "TGAA||TACG", true ) );
}
TEST_CASE ( Nucstrstr_Positional_OR_Found_FirstMatchPositionReported )
{
    REQUIRE_EQ ( 2, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGTTGCA", "TGAA|CGT|TACG", true ) );
}
TEST_CASE ( Nucstrstr_Positional_OR_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGTTGCA", "TGAA|TACA", true ) );
}

TEST_CASE ( Nucstrstr_Positional_AND_Found_LastMatchPositionReported )
{
    REQUIRE_EQ ( 4, RunNucStrtr ( "ACGTACGT", "CGTA&TACG", true ) );
}
TEST_CASE ( Nucstrstr_Positional_AND_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGT", "TACG&TACA", true ) );
}

TEST_CASE ( Nucstrstr_Positional_AND_OR_Found_1 )
{
    REQUIRE_EQ ( 3, RunNucStrtr ( "ACGTACGT", "CGTA&TACC|GTAC", true ) );
}
TEST_CASE ( Nucstrstr_Positional_AND_OR_Found_2 )
{
    REQUIRE_EQ ( 3, RunNucStrtr ( "ACGTACGT", "CGTA&(TACC|GTAC)", true ) );
}
TEST_CASE ( Nucstrstr_Positional_AND_OR_Found_3 )
{
    REQUIRE_EQ ( 2, RunNucStrtr ( "ACGTACGT", "(TACC|GTAC)&&CGTA", true ) );
}

TEST_CASE ( Nucstrstr_Positional_AND_OR_NOT_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGT", "(TACC|GTAC)&&!CGTA", true ) );
}
TEST_CASE ( Nucstrstr_Positional_AND_OR_NOT_Found )
{
    REQUIRE_EQ ( 1, RunNucStrtr ( "ACGTACGT", "(TACC|GTAC)&&!CATA", true ) );
}

TEST_CASE ( Nucstrstr_Positional_Error )
{
    REQUIRE_THROW ( RunNucStrtr ( "ACGTACGT", "(TACC", true ) );
}

TEST_CASE ( Nucstrstr_NonPositional_4NA_NotFound )
{
    REQUIRE_EQ ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "ACTN", false ) );
}

TEST_CASE ( Nucstrstr_NonPositional_4NA_Found_AtStart )
{
    REQUIRE_NE ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "ACGTACGTACN", false ) );
}

TEST_CASE ( Nucstrstr_NonPositional_4NA_Found_InMiddle )
{
    REQUIRE_NE ( 0, RunNucStrtr ( "ACGTACGTACGTACGTACGTACGTACGTACGT", "GTACGTACN", false ) );
}



//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return TestSuiteSearch(argc, argv);
}

} // end of extern "C"
