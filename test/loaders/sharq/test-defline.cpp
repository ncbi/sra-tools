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
#include "../../../tools/loaders/sharq/fastq_defline_parser.hpp"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(DefLineTestSuite);

TEST_CASE(ElementBio)
{
    CDefLineParser p;
    REQUIRE( p.Match( "@AV240401:AVT0059:2409682889:1:10602:5169:0001", true ) );
    REQUIRE_EQ( string("ElementBio"), p.GetDeflineType() );
    REQUIRE_EQ( (int)SRA_PLATFORM_ELEMENT_BIO, (int)p.GetPlatform() );
}

int main (int argc, char *argv [])
{
    return DefLineTestSuite(argc, argv);
}
