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

#include <iostream>
#include <typeinfo>
#include <sstream>
#include <JSON_ostream.hpp>

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
        out << 1;
    }
    REQUIRE_EQ( string("1"), outStr.str() );
}

TEST_CASE(InsertFloat)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out << 1.5;
    }
    REQUIRE_EQ( string("1.5"), outStr.str() );
}

TEST_CASE(InsertTrue)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out << true;
    }
    REQUIRE_EQ( string("true"), outStr.str() );
}

TEST_CASE(InsertFalse)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out << false;
    }
    REQUIRE_EQ( string("false"), outStr.str() );
}

TEST_CASE(InsertChar)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out << 'c';
    }
    REQUIRE_EQ( string("c"), outStr.str() );
}

TEST_CASE(InsertCString)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out << "cstring";
    }
    REQUIRE_EQ( string(R"("cstring")"), outStr.str() );
}

TEST_CASE(InsertString)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out << string("string");
    }
    REQUIRE_EQ( string(R"("string")"), outStr.str() );
}

TEST_CASE(InsertStringView)
{
    ostringstream outStr;
    {
        JSON_ostream out(outStr);
        out << string_view("string");
    }
    REQUIRE_EQ( string(R"("string")"), outStr.str() );
}

static void insertArrayElem(JSON_ostream &&out)
{ // expect ["elem1"]
    out << '[';
    out << "elem1";
    out << ']';
}

static void insertArrayElems(JSON_ostream &&out)
{ // expect [1,[],{},true,false,"Hello",0.5]
   // commas between array elements are the caller's responsibility
    out << '[' << 1
        << ',' << '[' << ']' // empty array
        << ',' << '{' << '}' // empty object
        << ',' << true
        << ',' << false
        << ',' << "Hello"
        << ',' << 0.5
        << ']';
}

static void insertArrayElems_Strings(JSON_ostream &&out)
{ // expect ["12","3"]
    out << '[' << '"' << 1 << 2 << '"'  // build a string with multiple insertions
        << ',' << "3"
        << ','                          // dangling comma does nothing
        << ']';
}

static void insertMember(JSON_ostream &&out)
{ // expect {"name":"value"}
    out << '{';
    out << JSON_Member{"name"};
    out << "value";
    out << '}';
}

static void insertMember2(JSON_ostream &&out)
{ // expect {"name1":"value1","name2":"value2","emptyArray":[],"emptyObject":{}}
   // commas between value members appear automagically
    out << '{';
    out << JSON_Member{"name1"};
    out << std::string{"value1"};
    out << JSON_Member{"name2"};
    out << std::string_view{"value2"};
    out << JSON_Member{"emptyArray"};
    out << '[' << ']';
    out << JSON_Member{"emptyObject"};
    out << '{' << '}';
    out << '}';
}

TEST_CASE(InsertArrayElem)
{
    ostringstream outStr;
    insertArrayElem(JSON_ostream{outStr});
    REQUIRE_EQ( string("[\n\t\"elem1\"\n]"), outStr.str() );
}

TEST_CASE(InsertArrayElems)
{
    ostringstream outStr;
    insertArrayElems(JSON_ostream{outStr});
    REQUIRE_EQ( string("[\n\t1,\n\t[],\n\t{},\n\ttrue,\n\tfalse,\n\t\"Hello\",\n\t0.5\n]"), outStr.str() );
}

TEST_CASE(InsertArrayElems_Strings)
{
    ostringstream outStr;
    insertArrayElems_Strings(JSON_ostream{outStr});
    REQUIRE_EQ( string("[\n\t\"12\",\n\t\"3\"\n]"), outStr.str() );
}

TEST_CASE(InsertMember)
{
    ostringstream outStr;
    insertMember(JSON_ostream{outStr});
    REQUIRE_EQ( string("{\n\t\"name\": \"value\"\n}"), outStr.str() );
}

TEST_CASE(InsertMembers)
{   // commas between value members appear automagically
    ostringstream outStr;
    insertMember2(JSON_ostream{outStr});
    REQUIRE_EQ( string("{\n\t\"name1\": \"value1\",\n\t\"name2\": \"value2\",\n\t\"emptyArray\": [],\n\t\"emptyObject\": {}\n}"), outStr.str() );
}

TEST_CASE(InsertArrayElem_compact)
{
    ostringstream outStr;
    insertArrayElem(JSON_ostream{outStr, true});
    REQUIRE_EQ( string{R"(["elem1"])"}, outStr.str() );
}

TEST_CASE(InsertArrayElems_compact)
{
    ostringstream outStr;
    insertArrayElems(JSON_ostream{outStr, true});
    REQUIRE_EQ( string{R"([1,[],{},true,false,"Hello",0.5])"}, outStr.str() );
}

TEST_CASE(InsertArrayElems_Strings_compact)
{
    ostringstream outStr;
    insertArrayElems_Strings(JSON_ostream{outStr, true});
    REQUIRE_EQ( string{R"(["12","3"])"}, outStr.str() );
}

TEST_CASE(InsertMember_compact)
{
    ostringstream outStr;
    insertMember(JSON_ostream{outStr, true});
    REQUIRE_EQ( string{R"({"name":"value"})"}, outStr.str() );
}

TEST_CASE(InsertMembers_compact)
{
    ostringstream outStr;
    insertMember2(JSON_ostream{outStr, true});
    REQUIRE_EQ( string{R"({"name1":"value1","name2":"value2","emptyArray":[],"emptyObject":{}})"}, outStr.str() );
}

static void set_the_locale(std::ostream &strm)
{
    char const *locales[] = { "en_US.UTF-8", "en_US.utf8", "en_US.utf-8", "en_US.UTF8", "en_US", nullptr };
    char const **cur = &locales[0];
    while (*cur) {
        try {
            strm.imbue(std::locale(*cur));
            return;
        }
        catch (std::runtime_error const &e) { (void)(e); }
        ++cur;
    }
    throw ncbi::NK::test_skipped{"no appropriate locale"};
}

TEST_CASE(Test_locale)
{
    ostringstream test;
    set_the_locale(test);
    test << 1234.56;
    
    auto const expected = string{R"([")"} + test.str() + string{R"(",1234.56])"};
    
    ostringstream outStr;
    
    outStr.imbue(test.getloc());
    
    auto jso = JSON_ostream{outStr, true};
    jso << '['
        << '"'
            << 1234.56 // this will use the set locale (it is inside a string)
        << '"'
        << ','
        << 1234.56 // this will use the "C" locale, i.e. no commas
    << ']';

    REQUIRE_EQ(expected, outStr.str());
}

int main (int argc, char *argv [])
{
    return QaStatsOutputTestSuite(argc, argv);
}
