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
* Unit tests for qa-stat/Input.[hc]pp
*/

#include "../../tools/test-tools/qa-stats/input.hpp"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(QaStatsInputTestSuite);

TEST_CASE(Empty)
{
    string text;
    auto i = Input::newSource(Input::Source::StringLiteralType{ text }, false );
    try
    {
        auto input = i->get();
    }
    catch(const ios_base::failure & ex)
    {
        REQUIRE_EQ( string{"no input:"}, string{ex.what()}.substr(0, 9) );
    }

    REQUIRE( i->eof() );
}

TEST_CASE(Fastq_1)
{
    auto &&source = Input::Source::StringLiteralType{
        "@BILLIEHOLIDAY_1_FC20F3DAAXX:8:2:342:540/1\n"
        "GTCGCTTCTCGGAAGNGTGAAAGACAANAATNTTNN\n"
        "+BILLIEHOLIDAY_1_FC20F3DAAXX:8:2:342:540\n"
        "&.<77478889998776776:997974354774779\n"
    };
    auto i = Input::newSource( source, false );
    auto input = i->get();
    REQUIRE_EQ( string{"GTCGCTTCTCGGAAGNGTGAAAGACAANAATNTTNN"}, input.sequence );
    try { i->get(); }
    catch(const ios_base::failure & ex)
    {
        REQUIRE_EQ( string{"no input:"}, string{ex.what()}.substr(0, 9) );
    }
    REQUIRE( i->eof() );
}

TEST_CASE(Fastq_2)
{
    auto &&source = Input::Source::StringLiteralType{
        R"###(
@NB501550:336:H75GGAFXY:2:11101:10137:1038 1:N:0:CTAGGTGA
NCTATCTAGAATTCCCTACTACTCCC
+
#AAAAEEEEEEEEEEEEEEEEEEEEE
)###"
    };
    auto i = Input::newSource( source, false );
    auto input = i->get();
    REQUIRE_EQ( string{"NCTATCTAGAATTCCCTACTACTCCC"}, input.sequence );
    try { i->get(); }
    catch(const ios_base::failure & ex)
    {
        REQUIRE_EQ( string{"no input:"}, string{ex.what()}.substr(0, 9) );
    }
    REQUIRE( i->eof() );
}

TEST_CASE(Fastq_File)
{
    char const *const expected[] = {
        "NCTATCTAGAATTCCCTACTACTCCC",
        "NAGCCGCGTAAGGGAATTAGGCAGCA",
        "NAATAAGGTAAAGTCACGTCAGTGTT",
        "NAAGATGTCATCTGTTGTAAGTCCTG",
        nullptr
    };
    auto &&source = Input::Source::FilePathType{"input/001.R1.fastq", false};
    auto i = Input::newSource( source, false );
    for (auto e = expected; *e; ++e) {
        auto const expect = string_view{ *e };
        auto const input = i->get();
        REQUIRE_EQ( expect, string_view{ input.sequence } );
    }
    try { i->get(); }
    catch(const ios_base::failure & ex)
    {
        REQUIRE_EQ( string{"no input:"}, string{ex.what()}.substr(0, 9) );
    }
    REQUIRE( i->eof() );
}

TEST_CASE(Fasta_File)
{ 
    auto &&source = Input::Source::StringLiteralType{ ">1\nAAAG\nTC\n>2\nTCGT\nCG\n" };
    char const *const expected[] = {
        "AAAGTC",
        "TCGTCG",
        nullptr
    };
    auto i = Input::newSource( source, false );
    for (auto e = expected; *e; ++e) {
        auto const expect = string_view{ *e };
        auto const input = i->get();
        REQUIRE_EQ( expect, string_view{ input.sequence } );
    }
}

TEST_CASE(Fasta_File_missing_LF)
{ 
    auto &&source = Input::Source::StringLiteralType{ ">1\nAAAG\nTC\n>2\nTCGT\nCG" };
    char const *const expected[] = {
        "AAAGTC",
        "TCGT",
        nullptr
    };
    auto i = Input::newSource( source, false );
    for (auto e = expected; *e; ++e) {
        auto const expect = string_view{ *e };
        auto const input = i->get();
        REQUIRE_EQ( expect, string_view{ input.sequence } );
    }
}

TEST_CASE(Input_tests)
{
    Input::runTests(); // will abort if anything fails
}

int main (int argc, char *argv [])
{
    return QaStatsInputTestSuite(argc, argv);
}
