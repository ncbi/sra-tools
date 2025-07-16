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

TEST_CASE(Fastq_DecimalQualitiesOnly)
{
    auto &&source = Input::Source::StringLiteralType{
        "+B:8:2:212:211\n"  // warning, skip
        "40 40\n"           // skip
        ">1\n"              // recover, parse normally
        "AAAG\n"
    };
    auto i = Input::newSource( source, false ); // should give out a warning
    char const *const expected[] = {
        "",
        "AAAG",
        nullptr
    };
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

/// There are 3 forms of records from `make-input.sh`:
/// 1. `READ,REF_NAME,REF_POS,REF_ORIENTATION,CIGAR_SHORT,SEQ_SPOT_GROUP` from an alignment table.
/// 2. `CMP_READ,READ_LEN,READ_START,READ_TYPE,PRIMARY_ALIGNMENT_ID,SPOT_GROUP` from the sequence table.
/// 3. `READ,READ_LEN,READ_START,READ_TYPE,SPOT_GROUP` from the sequence table.

/// form 1
TEST_CASE(make_input_alignment)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "GCATCCTGCACAGCTAGAGAT\tHUMAN_1\t14358\ttrue\t21M\tA\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 21);
    REQUIRE_EQ((int)reads.size(), 1);
    REQUIRE_EQ(reads[0].length, 21);
    REQUIRE_EQ(reads[0].type, Input::ReadType::aligned);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::reverse);
    REQUIRE_EQ((int)reads[0].cigar.operations.size(), 1);
    REQUIRE_EQ((int)reads[0].cigar.operations[0].length, 21);
    REQUIRE_EQ((int)reads[0].cigar.operations[0].opcode, 0);
    REQUIRE_EQ((int)reads[0].cigar.operations[0].sequenceLength(), 21);
    REQUIRE_EQ((int)reads[0].cigar.operations[0].referenceLength(), 21);
    REQUIRE_NE(reads[0].reference, -1);
    REQUIRE_EQ(std::string{"HUMAN_1"}, input.references[reads[0].reference]);
    REQUIRE_NE(input.group, -1);
    REQUIRE_EQ(std::string{"A"}, input.groups[input.group]);
}

/// form 2, fully aligned
TEST_CASE(make_input_fully_aligned)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE\t11, 12\tGROUP_1\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[0].type, Input::ReadType::aligned);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[1].type, Input::ReadType::aligned);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_NE(input.group, -1);
    REQUIRE_EQ(std::string{"GROUP_1"}, input.groups[input.group]);
}

/// form 2, fully aligned, empty spot group
TEST_CASE(make_input_fully_aligned_no_sg)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE\t11, 12\t\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[0].type, Input::ReadType::aligned);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[1].type, Input::ReadType::aligned);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_EQ(input.group, -1);
}

/// form 2, half aligned
TEST_CASE(make_input_half_aligned)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "NTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE\t0, 12\tGROUP_1\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[0].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[1].type, Input::ReadType::aligned);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_NE(input.group, -1);
    REQUIRE_EQ(std::string{"GROUP_1"}, input.groups[input.group]);
}

/// form 2, half aligned, no spot group
TEST_CASE(make_input_half_aligned_no_sg)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "NTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE\t0, 12\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[0].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[1].type, Input::ReadType::aligned);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_EQ(input.group, -1);
}

/// form 2, fully unaligned
TEST_CASE(make_input_fully_unaligned)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE\t0, 0\tGROUP_1\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[0].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[1].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_NE(input.group, -1);
    REQUIRE_EQ(std::string{"GROUP_1"}, input.groups[input.group]);
}

/// form 2, fully unaligned, no spot group
TEST_CASE(make_input_fully_unaligned_no_sg)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE\t0, 0\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[0].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[1].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_EQ(input.group, -1);
}

/// form 3 with a spot group
TEST_CASE(make_input_3)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_REVERSE\tGROUP_100\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[0].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[1].type, Input::ReadType::technical);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_NE(input.group, -1);
    REQUIRE_EQ(std::string{"GROUP_100"}, input.groups[input.group]);
}

/// form 3 with no spot group
TEST_CASE(make_input_3_no_sg)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD, SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_REVERSE\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)input.sequence.size(), 293);
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[0].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[1].type, Input::ReadType::technical);
    REQUIRE_EQ(reads[0].orientation, Input::ReadOrientation::forward);
    REQUIRE_EQ(reads[1].orientation, Input::ReadOrientation::reverse);
    REQUIRE_EQ(input.group, -1);
}

/// form 3 with a numeric spot group (could be confused with form 2 with no spot group
TEST_CASE(make_input_3_num_sg)
{
    auto && sourcetext = Input::Source::StringLiteralType{
        "GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL, SRA_READ_TYPE_BIOLOGICAL\t11\n"
    };
    auto source = Input::newSource(sourcetext, false);
    auto const &input = source->get();
    auto const &reads = input.reads;
    REQUIRE_EQ((int)reads.size(), 2);
    REQUIRE_EQ(reads[0].length, 142);
    REQUIRE_EQ(reads[1].length, 151);
    REQUIRE_EQ(reads[0].type, Input::ReadType::biological);
    REQUIRE_EQ(reads[1].type, Input::ReadType::biological);
    REQUIRE_NE(input.group, -1);
    REQUIRE_EQ(std::string{"11"}, input.groups[input.group]);
}

int main (int argc, char *argv [])
{
    return QaStatsInputTestSuite(argc, argv);
}
