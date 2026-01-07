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
* Unit tests for SHARQ loader
*/
#include <ktst/unit_test.hpp>

#include "../../tools/loaders/sharq/fastq_parser.hpp"
#include "../../tools/loaders/sharq/fastq_writer.hpp"

using namespace std;

TEST_SUITE(SharQParserTestSuite);

class LoaderFixture
{
public:
    LoaderFixture()
    {
    }
    ~LoaderFixture()
    {
    }

    shared_ptr<istream> create_stream(const string& str = "") {
        shared_ptr<stringstream> ss(new stringstream);
        *ss << str;
        return ss;
    }
    string filename;
};

FIXTURE_TEST_CASE(TestSequence, LoaderFixture)
{   // parser registers the fingerprints of incoming reads with the writer
    json readers = json::parse(
        R"( {
                "files" : [
                    { "file_path" : "input/r1.fastq", "platform_code" : 1, "quality_encoding" : 33, "max_reads" : 1 },
                    { "file_path" : "input/r2.fastq", "platform_code" : 1, "quality_encoding" : 33, "max_reads" : 1 }
                ],
                "is_10x" : false
            }
        )"
    );
    auto wr = make_shared<fastq_writer_debug>();
    fastq_parser<fastq_writer_debug> parser( wr );
    wr->open();
    parser.set_readers( readers ); // will open files

    auto err_checker = [this](fastq_error& e) -> void {};
    spot_name_check name_checker(10);
    parser.parse< validator_options<eNumeric, 33, 126> > (
        name_checker,
        err_checker
    );
    // this will not read anything but should save readers' (empty) fingerprints
    // on the writer

    REQUIRE_EQ( string("input/r1.fastq"), wr->m_source_sums[0].first);
    REQUIRE_EQ( string("input/r2.fastq"), wr->m_source_sums[1].first);

    wr->close();
}

FIXTURE_TEST_CASE(ReaderMd5, LoaderFixture)
{   // parser calculates md5 of inputs
    json readers = json::parse(
        R"( {
                "files" : [
                    { "file_path" : "input/r1.fastq", "platform_code" : 1, "quality_encoding" : 33, "max_reads" : 1 }
                ],
                "is_10x" : false
            }
        )"
    );
    auto wr = make_shared<fastq_writer_debug>();
    fastq_parser<fastq_writer_debug> parser( wr );
    //wr->open();
    parser.set_readers( readers ); // will open files

    auto err_checker = [this](fastq_error& e) -> void {};
    spot_name_check name_checker(10);
    parser.parse< validator_options<eNumeric, 33, 126> > (
        name_checker,
        err_checker
    );

    auto in0 = dynamic_cast<const istreambuf_holder*>( & parser.get_readers()[0].get_stream() );
    REQUIRE_NOT_NULL( in0);

    uint8_t digest [ 16 ];
    in0 -> get_md5( digest );
    ostringstream md5str;
    for( auto b: digest )
    {
        md5str << setfill('0') << setw(2) << hex << (int)b;
    }
    REQUIRE_EQ( string("29eb1d7f05c030570fa3671ee13feb7b"), md5str.str() );

    wr->close();
}

////////////////////////////////////////////

int main (int argc, char *argv [])
{
    return SharQParserTestSuite(argc, argv);
}
