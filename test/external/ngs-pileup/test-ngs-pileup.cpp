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
* Unit tests for NGS Pileup
*/

#include <ktst/unit_test.hpp>

#include <sysalloc.h>

#include <sstream>

#include "../../tools/external/ngs-pileup/ngs-pileup.cpp"

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsPileupTestSuite);

class NGSPileupFixture
{
public:
    NGSPileupFixture()
    {
        ps . output = & m_str;
    }

    string Run()
    {
        NGS_Pileup ( ps ) . Run ();
        return m_str . str ();
    }

    ostringstream m_str;
    NGS_Pileup::Settings ps;
};

FIXTURE_TEST_CASE ( NoInput, NGSPileupFixture )
{
    Run ();
    REQUIRE_EQ ( string(), m_str . str() );
}

FIXTURE_TEST_CASE ( BadInput, NGSPileupFixture )
{
    ps . AddInput ( "blah" );
    REQUIRE_THROW ( Run() );
    REQUIRE_EQ ( string(), m_str . str() );
}

FIXTURE_TEST_CASE ( Basic, NGSPileupFixture )
{
    ps . AddInput ( "SRR833251" ); // a small accession with primary and secondary alignments
    string expectedStart = "gi|169794206|ref|NC_010410.1|\t19376\t1\n"; //TODO: expand when pileup prints out more data
    REQUIRE_EQ ( expectedStart, Run () . substr ( 0, expectedStart . length () ) );
}

FIXTURE_TEST_CASE ( SingleReference_ByCommonName, NGSPileupFixture )
{
    ps . AddInput ( "ERR247027" );
    ps . AddReference ( "Pf3D7_13" );
    string expectedStart = "AL844509.2\t1212494\t1"; //TODO: expand when pileup prints out more data
    string actual = Run ();
    REQUIRE_EQ ( expectedStart, actual . substr ( 0, expectedStart . length () ) );
}

FIXTURE_TEST_CASE ( SingleReference_ByCanonicalName, NGSPileupFixture )
{
    ps . AddInput ( "ERR247027" );
    ps . AddReference ( "AL844509.2" );
    string expectedStart = "AL844509.2\t1212494\t1"; //TODO: expand when pileup prints out more data
    REQUIRE_EQ ( expectedStart, Run () . substr ( 0, expectedStart . length () ) );
}

FIXTURE_TEST_CASE ( SingleReference_Slice, NGSPileupFixture )
{
    ps . AddInput ( "ERR247027" );
    ps . AddReferenceSlice ( "AL844509.2", 1212492, 3 );
    string expected =
        "AL844509.2\t1212494\t1\n" /* this position is 1-based */
        "AL844509.2\t1212495\t1\n";
    REQUIRE_EQ ( expected, Run().substr(0, expected.size()) );
}

#if 0
FIXTURE_TEST_CASE ( MultipleReferences, NGSPileupFixture )
{
    ps . AddInput ( "SRR1068024" ); // 38 references
    const string ref1 = "0000000.72b.NC2_17738";
    const string ref2 = "0000000.72b.NC2_14823"; // this one should come first in the output
    ps . AddReference ( ref1 );
    ps . AddReference ( ref2 );
    string expectedStart = ref2 + "\t1\t1";

    string res = Run();

    REQUIRE_EQ ( expectedStart, res . substr ( 0, expectedStart . length () ) );
    REQUIRE_NE ( string :: npos, res. find ( ref2 ) );
}

FIXTURE_TEST_CASE ( MultipleInputs, NGSPileupFixture )
{   // ERR334733 ERR334777 align against the same reference, ERR334733's position comes first so it has to
    // be the first on the output regardless of order of accessions on the command line
    ps . AddInput ( "ERR334777" );
    ps . AddInput ( "ERR334733" );
    string expectedStart = "FN433596.1\t805952\t1"; // comes from ERR334733
    string res = Run();
    REQUIRE_EQ ( expectedStart, res . substr ( 0, expectedStart . length () ) );
    REQUIRE_NE ( string :: npos, res. find ( "2424446" ) ); // one of the positions from ERR334777
}

FIXTURE_TEST_CASE ( AllReferencses, NGSPileupFixture )
{
    ps . AddInput ( "SRR341578" );
    string expectedStart = "NC_011748.1\t1\t1";
    REQUIRE_EQ ( expectedStart, Run () . substr ( 0, expectedStart . length () ) );
}

//TODO: multiple input overlapping
#endif
//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>

int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return NgsPileupTestSuite(argc, argv);
}

}
