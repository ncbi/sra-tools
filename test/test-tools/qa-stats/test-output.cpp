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
* Unit tests for qa-stat/output.[hc]pp
*/

#include "../../tools/test-tools/qa-stats/output.hpp"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(QaStatsOutputTestSuite);

TEST_CASE(Empty)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
    }
    REQUIRE_EQ( string(), outStr.str() );
}

TEST_CASE(InsertInt)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out.insert(1);
    }
    REQUIRE_EQ( string("1"), outStr.str() );
}

TEST_CASE(InsertBool)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out.insert(true);
        out.insert(false);
    }
    REQUIRE_EQ( string("truefalse"), outStr.str() );
}

TEST_CASE(InsertChar)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out.insert('c');
    }
    REQUIRE_EQ( string("c"), outStr.str() );
}

TEST_CASE(InsertCString)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out.insert("cstring");
    }
    REQUIRE_EQ( string(R"("cstring")"), outStr.str() );
}

TEST_CASE(InsertString)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out.insert(string("string"));
    }
    REQUIRE_EQ( string(R"("string")"), outStr.str() );
}

TEST_CASE(InsertMember)
{
    ostringstream outStr;
    {
        JSON_Member mem = { "name" };
        JSON_ostream out(outStr);
        out.insert('{');
        out.insert(mem);
        out.insert(string("value"));
        out.insert('}');
    }
    REQUIRE_EQ( string("{\n\t\"name\": \"value\"\n}"), outStr.str() );
}

TEST_CASE(InsertMembers)
{   // commas between value members appear automagically
    ostringstream outStr;
    {
        JSON_Member mem1 = { "name1" };
        JSON_Member mem2 = { "name2" };
        JSON_ostream out(outStr);
        out.insert('{');
        out.insert(mem1);
        out.insert(string("value1"));
        out.insert(mem2);
        out.insert(string("value2"));
        out.insert('}');
    }
    REQUIRE_EQ( string("{\n\t\"name1\": \"value1\",\n\t\"name2\": \"value2\"\n}"), outStr.str() );
}

TEST_CASE(InsertArrayElem)
{
    ostringstream outStr;
    {
        JSON_Member mem = { "name" };
        JSON_ostream out(outStr);
        out.insert('[');
        out.insert(string("elem1"));
        out.insert(']');
    }
    REQUIRE_EQ( string("[\n\t\"elem1\"\n]"), outStr.str() );
}

TEST_CASE(InsertArrayElems)
{   // commas between array elements seem to be the caller's responsibility
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out.insert('[');
        out<<"elem1"<<','<<"elem2"<<','<<"elem3";
        out.insert(']');
    }
    REQUIRE_EQ( string("[\n\t\"elem1\",\n\t\"elem2\",\n\t\"elem3\"\n]"), outStr.str() );
}

int main (int argc, char *argv [])
{
    return QaStatsOutputTestSuite(argc, argv);
}
