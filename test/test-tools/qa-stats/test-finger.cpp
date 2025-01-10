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
    Fingerprint fp(1);
    REQUIRE_EQ( string(
        "["
        R"({"base":"A", "pos":0, "count":0},)"
        R"({"base":"C", "pos":0, "count":0},)"
        R"({"base":"G", "pos":0, "count":0},)"
        R"({"base":"T", "pos":0, "count":0},)"
        R"({"base":"N", "pos":0, "count":0},)"
        R"({"base":"OoL", "pos":0, "count":0})"
        "]"
        ), fp.toJson() );
}

TEST_CASE(OneBase)
{
    Fingerprint fp(1);
    fp.update("A");
    REQUIRE_EQ( string(
        "["
        R"({"base":"A", "pos":0, "count":1},)"
        R"({"base":"C", "pos":0, "count":0},)"
        R"({"base":"G", "pos":0, "count":0},)"
        R"({"base":"T", "pos":0, "count":0},)"
        R"({"base":"N", "pos":0, "count":0},)"
        R"({"base":"OoL", "pos":0, "count":0})"
        "]"
        ), fp.toJson() );
}

TEST_CASE(AllBases)
{
    Fingerprint fp(6);
    fp.update("ACGTN");
    REQUIRE_EQ( string(
        "["
        R"({"base":"A", "pos":0, "count":1},{"base":"A", "pos":1, "count":0},{"base":"A", "pos":2, "count":0},{"base":"A", "pos":3, "count":0},{"base":"A", "pos":4, "count":0},{"base":"A", "pos":5, "count":0},)"
        R"({"base":"C", "pos":0, "count":0},{"base":"C", "pos":1, "count":1},{"base":"C", "pos":2, "count":0},{"base":"C", "pos":3, "count":0},{"base":"C", "pos":4, "count":0},{"base":"C", "pos":5, "count":0},)"
        R"({"base":"G", "pos":0, "count":0},{"base":"G", "pos":1, "count":0},{"base":"G", "pos":2, "count":1},{"base":"G", "pos":3, "count":0},{"base":"G", "pos":4, "count":0},{"base":"G", "pos":5, "count":0},)"
        R"({"base":"T", "pos":0, "count":0},{"base":"T", "pos":1, "count":0},{"base":"T", "pos":2, "count":0},{"base":"T", "pos":3, "count":1},{"base":"T", "pos":4, "count":0},{"base":"T", "pos":5, "count":0},)"
        R"({"base":"N", "pos":0, "count":0},{"base":"N", "pos":1, "count":0},{"base":"N", "pos":2, "count":0},{"base":"N", "pos":3, "count":0},{"base":"N", "pos":4, "count":1},{"base":"N", "pos":5, "count":0},)"
        R"({"base":"OoL", "pos":0, "count":0},{"base":"OoL", "pos":1, "count":0},{"base":"OoL", "pos":2, "count":0},{"base":"OoL", "pos":3, "count":0},{"base":"OoL", "pos":4, "count":0},{"base":"OoL", "pos":5, "count":1})"
        "]"
        ), fp.toJson() );
}

TEST_CASE(MultiRead)
{
    Fingerprint fp(6);
    fp.update("ACGTN");
    fp.update("AACCGT");
    REQUIRE_EQ( string(
        "["
        R"({"base":"A", "pos":0, "count":2},{"base":"A", "pos":1, "count":1},{"base":"A", "pos":2, "count":0},{"base":"A", "pos":3, "count":0},{"base":"A", "pos":4, "count":0},{"base":"A", "pos":5, "count":0},)"
        R"({"base":"C", "pos":0, "count":0},{"base":"C", "pos":1, "count":1},{"base":"C", "pos":2, "count":1},{"base":"C", "pos":3, "count":1},{"base":"C", "pos":4, "count":0},{"base":"C", "pos":5, "count":0},)"
        R"({"base":"G", "pos":0, "count":0},{"base":"G", "pos":1, "count":0},{"base":"G", "pos":2, "count":1},{"base":"G", "pos":3, "count":0},{"base":"G", "pos":4, "count":1},{"base":"G", "pos":5, "count":0},)"
        R"({"base":"T", "pos":0, "count":0},{"base":"T", "pos":1, "count":0},{"base":"T", "pos":2, "count":0},{"base":"T", "pos":3, "count":1},{"base":"T", "pos":4, "count":0},{"base":"T", "pos":5, "count":1},)"
        R"({"base":"N", "pos":0, "count":0},{"base":"N", "pos":1, "count":0},{"base":"N", "pos":2, "count":0},{"base":"N", "pos":3, "count":0},{"base":"N", "pos":4, "count":1},{"base":"N", "pos":5, "count":0},)"
        R"({"base":"OoL", "pos":0, "count":0},{"base":"OoL", "pos":1, "count":0},{"base":"OoL", "pos":2, "count":0},{"base":"OoL", "pos":3, "count":0},{"base":"OoL", "pos":4, "count":0},{"base":"OoL", "pos":5, "count":1})"
        "]"
        ), fp.toJson() );
}

TEST_CASE(WrapAround)
{
    Fingerprint fp(3);
    fp.update("ACGTN");
             //TN
    fp.update("AACCGT");
             //CGT
    fp.update("AC");
    REQUIRE_EQ( string(
        "["
        R"({"base":"A", "pos":0, "count":3},{"base":"A", "pos":1, "count":1},{"base":"A", "pos":2, "count":0},)"
        R"({"base":"C", "pos":0, "count":1},{"base":"C", "pos":1, "count":2},{"base":"C", "pos":2, "count":1},)"
        R"({"base":"G", "pos":0, "count":0},{"base":"G", "pos":1, "count":1},{"base":"G", "pos":2, "count":1},)"
        R"({"base":"T", "pos":0, "count":1},{"base":"T", "pos":1, "count":0},{"base":"T", "pos":2, "count":1},)"
        R"({"base":"N", "pos":0, "count":0},{"base":"N", "pos":1, "count":1},{"base":"N", "pos":2, "count":0},)"
        R"({"base":"OoL", "pos":0, "count":0},{"base":"OoL", "pos":1, "count":0},{"base":"OoL", "pos":2, "count":1})"
        "]"
        ), fp.toJson() );
}

int main (int argc, char *argv [])
{
    return QaStatsFingerprintTestSuite(argc, argv);
}
