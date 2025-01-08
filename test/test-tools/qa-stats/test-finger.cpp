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
* Unit tests for qa-stat/fingerprint.hpp
*/

#include "../../../tools/test-tools/qa-stats/fingerprint.hpp"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(QaStatsFingerprintTestSuite);

TEST_CASE(Empty)
{
    Fingerprint fp;
    REQUIRE_EQ( string("[]"), fp.toJson() );
}

TEST_CASE(OneBase)
{
    Fingerprint fp(1);
    fp.update("A");
    REQUIRE_EQ( string(R"([{"base":"A", "pos":0, "count":1}])"), fp.toJson() );
}

TEST_CASE(AllBases)
{
    Fingerprint fp(6);
    fp.update("ACGTN");
    REQUIRE_EQ( string(
        "["
        R"({"base":"A", "pos":0, "count":1},)"
        R"({"base":"C", "pos":1, "count":1},)"
        R"({"base":"G", "pos":2, "count":1},)"
        R"({"base":"T", "pos":3, "count":1},)"
        R"({"base":"N", "pos":4, "count":1},)"
        R"({"base":"OoL", "pos":5, "count":1})"
        "]"
    ), fp.toJson() );
}

int main (int argc, char *argv [])
{
    return QaStatsFingerprintTestSuite(argc, argv);
}
