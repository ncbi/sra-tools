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

#include "kfile_stream/kfile_stream.hpp"

#include <ktst/unit_test.hpp>

//using namespace std;

TEST_SUITE(KFileStreamTestSuite);

static size_t consume_line_by_line( std::istream& stream, bool show = false, size_t max = SIZE_MAX ) {
    size_t res = 0;
    std::string s;
    while( res < max && std::getline( stream, s ) ) {
        if ( show ) { std::cout << s << std::endl; }
        res++;
    }
    return res;
}

static size_t consume_numbers( std::istream& s, bool show = false ) {
    size_t res = 0;
    while ( s . good() ) {
        int n;
        s >> n;
        if ( show ) { std::cout << n << std::endl; }
        res++;
    }
    return res;
}

TEST_CASE( ConsumeLineByLine_FromString )
{
    auto stream = custom_istream::custom_istream::make_from_string(
        "this is a very long \nlong long string" );
    REQUIRE_EQ ( 2, (int) consume_line_by_line( stream ) );
}
TEST_CASE( ConsumeNumbers )
{
    auto stream = custom_istream::custom_istream::make_from_string( "100 200 300 400" );
    REQUIRE_EQ ( 4, (int) consume_numbers( stream ) );
}

TEST_CASE( ConsumeLineByLine_FromKFile )
{
    auto src = vdb::KFileFactory::make_from_path( "Makefile" );
    auto stream = custom_istream::custom_istream::make_from_kfile( src );
    REQUIRE_LT( 0, (int) consume_line_by_line( stream ) );
}

TEST_CASE( ConsumeLineByLine_FromURL )
{
    const std::string url{ "https://sra-downloadb.be-md.ncbi.nlm.nih.gov/sos5/sra-pub-zq-11/SRR000/000/SRR000001/SRR000001.lite.1" };
    auto src = vdb::KFileFactory::make_from_vpath( url );
    REQUIRE_NOT_NULL( src );
    auto stream = custom_istream::custom_istream::make_from_kfile( src );
    REQUIRE_LT( 0, (int) consume_line_by_line( stream, false, 100 ) );
}

TEST_CASE( ConsumeLineByLine_FromURI )
{
    const std::string uri{ "https://www.nih.gov" };
    auto src = vdb::KStreamFactory::make_from_uri( uri );
    REQUIRE_NOT_NULL( src );
    auto stream = custom_istream::custom_istream::make_from_kstream( src );
    REQUIRE_LT( 0, (int) consume_line_by_line( stream, false, 20 ) );
}

TEST_CASE( ConsumeLineByLine_FromInvalidURI )
{
    const std::string uri{ "https://www.nih.gov/an_invalid_path" };
    REQUIRE_THROW( vdb::KStreamFactory::make_from_uri( uri ) );
}

int main (int argc, char *argv [])
{
    return KFileStreamTestSuite(argc, argv);
}

