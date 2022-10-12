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
* Unit tests for SHARQ regexp matcher
*/
#include "../../../tools/loaders/sharq/regexpr.hpp"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(RegexprTestSuite);

TEST_CASE(Construction)
{
    CRegExprMatcher m( string() );
}

TEST_CASE(LastInput)
{
    CRegExprMatcher m("");
    const string Input = "qq";
    m.Matches( Input );
    REQUIRE_EQ( Input, m.GetLastInput() );
}

TEST_CASE(NoMatch)
{
    CRegExprMatcher m( "qq" );
    REQUIRE( ! m.Matches( "zz" ) );
    REQUIRE_EQ( size_t( 0 ), m.GetMatch().size() );
}

TEST_CASE(YesMatch)
{
    CRegExprMatcher m( R"([@>+]([!-~]*?)[: ]?([!-~]+?Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?|)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D|)[: ]([!-~]*?)[: ]?([!-~ ]+?_ch)_?(\d+)(_read|_file)_?(\d+)(_strand\d*.fast5|_strand\d*.*|)(\s+|$))" );
    REQUIRE( m.Matches( "@f286a4e1-fb27-4ee7-adb8-60c863e55dbb_Basecall_Alignment_template MINICOL235_20170120_FN__MN16250_sequencing_throughput_ONLL3135_25304_ch143_read16010_strand" ) );
    REQUIRE_EQ( size_t( 11 ), m.GetMatch().size() );
    REQUIRE_EQ( string(""), m.GetMatch()[0].as_string() );
    REQUIRE_EQ( string("f286a4e1-fb27-4ee7-adb8-60c863e55dbb_Basecall"), m.GetMatch()[1].as_string() );
    REQUIRE_EQ( string("_Alignment"), m.GetMatch()[2].as_string() );
    REQUIRE_EQ( string("_template"), m.GetMatch()[3].as_string() );
    REQUIRE_EQ( string(""), m.GetMatch()[4].as_string() );
    REQUIRE_EQ( string("MINICOL235_20170120_FN__MN16250_sequencing_throughput_ONLL3135_25304_ch"), m.GetMatch()[5].as_string() );
    REQUIRE_EQ( string("143"), m.GetMatch()[6].as_string() );
    REQUIRE_EQ( string("_read"), m.GetMatch()[7].as_string() );
    REQUIRE_EQ( string("16010"), m.GetMatch()[8].as_string() );
    REQUIRE_EQ( string("_strand"), m.GetMatch()[9].as_string() );
    REQUIRE_EQ( string(""), m.GetMatch()[10].as_string() );
}

int main (int argc, char *argv [])
{
    return RegexprTestSuite(argc, argv);
}
