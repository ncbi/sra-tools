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
 * Purpose:
 *  Unit tests for fingerprint.hpp
 */

#include <sstream>
#include <JSON_ostream.hpp>
#include <fingerprint.hpp>

#include <ktst/unit_test.hpp>

#define EOR_FLD eor
#define EOR_TAG "EoR"
using namespace std;

TEST_SUITE(QaStatsFingerprintTestSuite);

TEST_CASE(Empty)
{
    Fingerprint fp(1);
    ostringstream outStr;
    JSON_ostream out{outStr, true};
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":0,"A":[0],"C":[0],"G":[0],"T":[0],"N":[0],"EoR":[0]})"
    };
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(EmptyRead)
{
    Fingerprint fp(1);
    fp.record("");

    ostringstream outStr;
    JSON_ostream out(outStr, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":0,"A":[0],"C":[0],"G":[0],"T":[0],"N":[0],"EoR":[1]})"
    };
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(OneBase)
{
    Fingerprint fp(1);
    fp.record("A");

    ostringstream outStr;
    JSON_ostream out(outStr, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":1,"A":[1],"C":[0],"G":[0],"T":[0],"N":[0],"EoR":[1]})"
    };
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(AllBases)
{
    Fingerprint fp(6);
    fp.record("ACGTN");

<<<<<<< HEAD
    ostringstream outStr;
    JSON_ostream out(outStr);
    out << '[' << fp << ']';
    const string expected =
        "[\n\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 0,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 1,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 2,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 3,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 4,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 5,\n\t\t\"count\": 1\n\t}\n"
        "]";
    for( size_t i = 0 ; i < expected.size(); ++i )
    {
        if (expected[i] != outStr.str()[i] )
        {
            cout << i << " " << expected[i] << " != " << outStr.str()[i] << endl;
            cout << expected.substr(i);
            FAIL("mismatch");
        }
    }
    REQUIRE_EQ( expected.size(), outStr.str().size() );
    REQUIRE_EQ( expected, outStr.str() );
=======
    ostringstream strm;
    JSON_ostream out(strm, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":5,"A":[1,0,0,0,0,0],"C":[0,1,0,0,0,0],"G":[0,0,1,0,0,0],"T":[0,0,0,1,0,0],"N":[0,0,0,0,1,0],"EoR":[0,0,0,0,0,1]})"
    };
    REQUIRE_EQ( expected, strm.str() );
>>>>>>> VDB-5845
}

TEST_CASE(MultiRead)
{
    Fingerprint fp(6);
    //         0123456
    fp.record("ACGTN");
    fp.record("AACCGT");

    ostringstream strm;
    JSON_ostream out(strm, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":6,"A":[2,1,0,0,0,0],"C":[0,1,1,1,0,0],"G":[0,0,1,0,1,0],"T":[0,0,0,1,0,1],"N":[0,0,0,0,1,0],"EoR":[1,0,0,0,0,1]})"
    };
    REQUIRE_EQ( expected, strm.str() );
}

<<<<<<< HEAD
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 0,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"" EOR_TAG "\",\n\t\t\"pos\": 5,\n\t\t\"count\": 1\n\t}\n"
        "]";
    for( size_t i = 0 ; i < expected.size(); ++i )
    {
        if (expected[i] != outStr.str()[i] )
        {
            cout << i << " " << expected[i] << " != " << outStr.str()[i] << endl;
            cout << expected.substr(i);
            FAIL("mismatch");
=======
TEST_CASE(ReadHash_example_Fig_1_Fig_2)
{
    Fingerprint fp{9};

    // from Fig. 1
    //         01234567
    fp.record("AATGCCT");
    fp.record("AACTTNGG");
    fp.record("TATATATA");
    fp.record("GCTA");

    // from Fig. 2
    //                             0  1  2  3  4  5  6  7  8
    uint64_t const expectedA[] = { 2, 3, 0, 2, 0, 1, 0, 1, 0 };
    uint64_t const expectedC[] = { 0, 1, 1, 0, 1, 1, 0, 0, 0 };
    uint64_t const expectedG[] = { 1, 0, 0, 1, 0, 0, 1, 1, 0 };
    uint64_t const expectedT[] = { 1, 0, 3, 1, 2, 0, 2, 0, 0 };
    uint64_t const expectedN[] = { 0, 0, 0, 0, 0, 1, 0, 0, 0 };
    uint64_t const expectedE[] = { 0, 0, 0, 0, 1, 0, 0, 1, 2 };

    auto const require_eq = [&](Fingerprint::Accumulator const &stats, uint64_t const *expected) {
        REQUIRE_EQ(stats.size(), size_t{9});
        for (size_t i = 0; i < 9; ++i) {
            REQUIRE_EQ(stats[i], expected[i]);
>>>>>>> VDB-5845
        }
    };

    require_eq(fp.a(), expectedA);
    require_eq(fp.c(), expectedC);
    require_eq(fp.g(), expectedG);
    require_eq(fp.t(), expectedT);
    require_eq(fp.n(), expectedN);
    require_eq(fp.eor(), expectedE);
}

TEST_CASE(ReadHash_example_Fig_1_Fig_2)
{
    Fingerprint fp{9};

    // from Fig. 1
    //         01234567
    fp.record("AATGCCT");
    fp.record("AACTTNGG");
    fp.record("TATATATA");
    fp.record("GCTA");

    // from Fig. 2
    //                             0  1  2  3  4  5  6  7  8
    uint64_t const expectedA[] = { 2, 3, 0, 2, 0, 1, 0, 1, 0 };
    uint64_t const expectedC[] = { 0, 1, 1, 0, 1, 1, 0, 0, 0 };
    uint64_t const expectedG[] = { 1, 0, 0, 1, 0, 0, 1, 1, 0 };
    uint64_t const expectedT[] = { 1, 0, 3, 1, 2, 0, 2, 0, 0 };
    uint64_t const expectedN[] = { 0, 0, 0, 0, 0, 1, 0, 0, 0 };
    uint64_t const expectedE[] = { 0, 0, 0, 0, 1, 0, 0, 1, 2 };


    auto const require_eq = [&](Fingerprint::Accumulator const &stats, uint64_t const *expected) {
        REQUIRE_EQ(stats.size(), size_t{9});
        for (size_t i = 0; i < 9; ++i) {
            REQUIRE_EQ(stats[i], expected[i]);
        }
    };

    require_eq(fp.a, expectedA);
    require_eq(fp.c, expectedC);
    require_eq(fp.g, expectedG);
    require_eq(fp.t, expectedT);
    require_eq(fp.n, expectedN);
    require_eq(fp.EOR_FLD, expectedE);
}


TEST_CASE(WrapAround)
{
    Fingerprint fp(3);
    fp.record("ACGTN");
             //ACG
             //TN$
    fp.record("AACCGT");
             //AAC
             //CGT
             //$
    fp.record("AC");
             //AC$

    ostringstream strm;
    JSON_ostream out(strm, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":6,"A":[3,1,0],"C":[1,2,1],"G":[0,1,1],"T":[1,0,1],"N":[0,1,0],"EoR":[1,0,2]})"
    };
    REQUIRE_EQ( expected, strm.str() );
}

int main (int argc, char *argv [])
{
    return QaStatsFingerprintTestSuite(argc, argv);
}


