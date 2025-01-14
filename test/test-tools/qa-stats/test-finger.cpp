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
    ostringstream outStr;
    JSON_ostream out(outStr);
    out << '[' << fp << ']';
    const string expected =
        "[\n\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t}\n]";
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(EmptyRead)
{
    Fingerprint fp(1);
    fp.record("");

    ostringstream outStr;
    JSON_ostream out(outStr);
    out << '[' << fp << ']';
    const string expected =
        "[\n\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 0,\n\t\t\"count\": 1\n\t}\n]";
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(OneBase)
{
    Fingerprint fp(1);
    fp.record("A");

    ostringstream outStr;
    JSON_ostream out(outStr);
    out << '[' << fp << ']';
    const string expected =
        "[\n\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 0,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t}\n]";
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(AllBases)
{
    Fingerprint fp(6);
    fp.record("ACGTN");

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

           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 5,\n\t\t\"count\": 1\n\t}\n"
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
}

TEST_CASE(MultiRead)
{
    Fingerprint fp(6);
    fp.record("ACGTN");
    fp.record("AACCGT");

    ostringstream outStr;
    JSON_ostream out(outStr);
    out << '[' << fp << ']';
    const string expected =
        "[\n\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 0,\n\t\t\"count\": 2\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 1,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 1,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 2,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 3,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 2,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 4,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 3,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 5,\n\t\t\"count\": 1\n\t},\n"

           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 4,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 5,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 3,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 4,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 5,\n\t\t\"count\": 1\n\t}\n"
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
}

TEST_CASE(WrapAround)
{
    Fingerprint fp(3);
    fp.record("ACGTN");
             //TN
    fp.record("AACCGT");
             //CGT
    fp.record("AC");

    ostringstream outStr;
    JSON_ostream out(outStr);
    out << '[' << fp << ']';
    const string expected =
        "[\n\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 0,\n\t\t\"count\": 3\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 1,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"A\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 0,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 1,\n\t\t\"count\": 2\n\t},\n"
           "\t{\n\t\t\"base\": \"C\",\n\t\t\"pos\": 2,\n\t\t\"count\": 1\n\t},\n"

           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 1,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"G\",\n\t\t\"pos\": 2,\n\t\t\"count\": 1\n\t},\n"

           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 0,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"T\",\n\t\t\"pos\": 2,\n\t\t\"count\": 1\n\t},\n"

           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 1,\n\t\t\"count\": 1\n\t},\n"
           "\t{\n\t\t\"base\": \"N\",\n\t\t\"pos\": 2,\n\t\t\"count\": 0\n\t},\n"

           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 0,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 1,\n\t\t\"count\": 0\n\t},\n"
           "\t{\n\t\t\"base\": \"OoL\",\n\t\t\"pos\": 2,\n\t\t\"count\": 1\n\t}\n"
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
}

int main (int argc, char *argv [])
{
    return QaStatsFingerprintTestSuite(argc, argv);
}


