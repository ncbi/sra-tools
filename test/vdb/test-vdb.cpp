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
* Unit tests for C++ wrapper for the VDB API
*/

#include "vdb.hpp"

#include <ktst/unit_test.hpp>

#include <sstream>

using namespace std;
using namespace VDB;

TEST_SUITE(VdbTestSuite);

// VDB::Error

TEST_CASE(Error_RcToString)
{
    rc_t const rc = SILENT_RC( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete );
    auto const rcStr = Error::RcToString( rc );
    std::string const expected = rcStr.find("(null)") == std::string::npos
        ? "RC(rcNS,rcFile,rcReading,rcTransfer,rcIncomplete)"
        : "RC((null):0:(null) rcNS,rcFile,rcReading,rcTransfer,rcIncomplete)";
    REQUIRE_EQ( expected, Error::RcToString( rc ) );
}
TEST_CASE(Error_RcToString_English)
{
    rc_t rc = SILENT_RC( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete );

    REQUIRE_EQ( string("transfer incomplete while reading file within network system module"), Error::RcToString( rc, true ) );
}

// VDB::Manager

// the current default version of the schema parser
#define PARSER_VERSION "2"

TEST_CASE(Manager_Construction)
{
    Manager m;
}

TEST_CASE(Manager_Schema_Bad)
{
    const string Text = "this is a bad schema";
    REQUIRE_THROW( Manager().schema(Text.size(), Text.c_str()) );
}

TEST_CASE(Manager_Schema_Good)
{
    const string Text = "version " PARSER_VERSION ";";
    Schema s = Manager().schema(Text.size(), Text.c_str());
    ostringstream out;
    out << s;
    REQUIRE_EQ( string("version " PARSER_VERSION ";\n"), out.str() );
}

static bool check_Schema_content(Schema const &s)
{
    ostringstream out;
    out << s;

    istringstream in(out.str());
    std::string line;

    std::getline(in, line, ';');
    if (line != "version " PARSER_VERSION)
        return false;

    in >> std::ws;
    std::getline(in, line, ';');
    if (line != "typedef U32 INSDC:SRA:spotid_t")
        return false;

    return true;
}

TEST_CASE(Manager_Schema_Include)
{
    const string Text = "version " PARSER_VERSION "; include 'inc.vschema';";
    const string IncludePath = "./data";
    Schema s = Manager().schema(Text.size(), Text.c_str(), IncludePath.c_str());

    REQUIRE(check_Schema_content(s));
}

TEST_CASE(Manager_SchemaFromFile)
{
    Schema s = Manager().schemaFromFile("./data/inc.vschema");
    REQUIRE(check_Schema_content(s));
}

TEST_CASE(Manager_SchemaFromFile_Bad)
{
    REQUIRE_THROW( Manager().schemaFromFile("bad file") );
}

TEST_CASE(Manager_Database_Bad)
{
    REQUIRE_THROW( Manager().openDatabase("./data/database") );
}

const string DatabasePath = "./data/SRR6336806";
const string TablePath = "./data/ERR3487613";

TEST_CASE(Manager_OpenDatabase_Good)
{
    Database d = Manager().openDatabase(DatabasePath);
}

TEST_CASE(Manager_OpenTable_Bad)
{   // throws if given a database
    REQUIRE_THROW( Manager().openTable(DatabasePath) );
}
TEST_CASE(Manager_OpenTable_Good)
{
    Table t = Manager().openTable(TablePath);
}

TEST_CASE(Manager_PathType_Table)
{
    REQUIRE_EQ( Manager::ptTable, Manager().pathType( TablePath ) );
}
TEST_CASE(Manager_PathType_Database)
{
    REQUIRE_EQ( Manager::ptDatabase, Manager().pathType( DatabasePath ) );
}
TEST_CASE(Manager_PathType_Invalid)
{
    REQUIRE_EQ( Manager::ptInvalid, Manager().pathType( "si si je suis un garbage" ) );
}

// VDB::Database

TEST_CASE(Database_Table_Bad)
{
    Database d = Manager().openDatabase( DatabasePath );
    REQUIRE_THROW( d["NOSUCHTABLE"]);
}
TEST_CASE(Database_Table_Good)
{
    Database d = Manager().openDatabase( DatabasePath );
    Table t = d["SEQUENCE"];
}

TEST_CASE(Database_hasTable_Yes)
{
    Database d = Manager().openDatabase( DatabasePath );
    REQUIRE( d.hasTable("SEQUENCE"));
}

TEST_CASE(Database_hasTable_No)
{
    Database d = Manager().openDatabase( DatabasePath );
    REQUIRE( ! d.hasTable("SHMEQUENCE"));
}

class SequenceTableFixture
{
protected:
    SequenceTableFixture()
    : t ( Manager().openDatabase( DatabasePath )["SEQUENCE"] )
    {
    }

    Table t;
};

// VDB::Table

TEST_CASE(Table_Default)
{
    Table t;
}
FIXTURE_TEST_CASE(Table_Assign, SequenceTableFixture)
{
    Table tt;
    tt = t;
}

FIXTURE_TEST_CASE(Table_ReadCursor1_BadColumn, SequenceTableFixture)
{
    const char * cols[] = {"a", "b"};
    REQUIRE_THROW( t.read( 2, cols ) );
}

FIXTURE_TEST_CASE(Table_ReadCursor1, SequenceTableFixture)
{
    const char * cols[] = {"READ", "NAME"};
    Cursor c = t.read( 2, cols );
}

FIXTURE_TEST_CASE(Table_ReadCursor2_BadColumn, SequenceTableFixture)
{
    REQUIRE_THROW( t.read( {"a", "b"} ) );
}

FIXTURE_TEST_CASE(Table_ReadCursor2, SequenceTableFixture)
{
    Cursor c = t.read( {"READ", "NAME"} );
}

FIXTURE_TEST_CASE(Table_PhysicaColumns, SequenceTableFixture)
{
    Table::ColumnNames cols = t.physicalColumns();
    REQUIRE_EQ( size_t(10), cols.size() );
    REQUIRE_EQ( string("ALTREAD"), cols[0] );
    REQUIRE_EQ( string("SPOT_GROUP"), cols[9] );
}

// VDB::Cursor

FIXTURE_TEST_CASE(Cursor_Columns, SequenceTableFixture)
{
    Cursor c = t.read( {"READ", "NAME"} );
    REQUIRE_EQ( 2u, c.columns() );
}

FIXTURE_TEST_CASE(Cursor_RowRange, SequenceTableFixture)
{
    Cursor c = t.read( {"READ", "NAME"} );
    auto r = c.rowRange();
    REQUIRE_EQ( Cursor::RowID(1), r.first );
    REQUIRE_EQ( Cursor::RowID(2608), r.second ); // exclusive
}

FIXTURE_TEST_CASE(Cursor_ReadOne, SequenceTableFixture)
{
    Cursor c = t.read( {"READ", "NAME"} );
    Cursor::RawData rd = c.read( 1, 1 );
    REQUIRE_EQ( size_t(1), rd.size() );
    REQUIRE_EQ( string("1"), rd.asString() );
}

FIXTURE_TEST_CASE(Cursor_ReadN, SequenceTableFixture)
{
    Cursor c = t.read( {"READ", "NAME"} );
    Cursor::RawData rd[2];
    c.read( 2, 2, rd );
    REQUIRE_EQ( string("AAGTCG"), rd[0].asString().substr(0, 6) );
    REQUIRE_EQ( string("2"), rd[1].asString() );
}

FIXTURE_TEST_CASE(Cursor_ForEach, SequenceTableFixture)
{
    Cursor c = t.read( {"READ", "NAME"} );
    auto check = [&](Cursor::RowID row, const vector<Cursor::RawData>& values )
    {
        REQUIRE_LT( (size_t)0, values[0].asString().size() );
        ostringstream rowId;
        rowId << row;
        REQUIRE_EQ( rowId.str(), values[1].asString() );
    };
    uint64_t n = c.foreach( check );
    REQUIRE_EQ( (uint64_t)2607, n );
}

FIXTURE_TEST_CASE(Cursor_ForEachWithFilter, SequenceTableFixture)
{
    Cursor c = t.read( {"READ", "NAME"} );
    auto check = [&](Cursor::RowID row, bool keep, const vector<Cursor::RawData>& values )
    {
        if ( keep )
        {
            REQUIRE_EQ( 1, (int)(row % 2) );
            REQUIRE_EQ( size_t(2), values.size() );
        }
        else
        {
            REQUIRE_EQ( 0, (int)(row % 2) );
            REQUIRE_EQ( size_t(0), values.size() );
        }
    };
	auto filter = [&](const Cursor & cur, Cursor::RowID row) -> bool { UNUSED(cur);  return bool(row % 2); };
    uint64_t n = c.foreach( filter, check );
    REQUIRE_EQ( (uint64_t)2607, n );
}

FIXTURE_TEST_CASE(Cursor_IsStaticColumn_True, SequenceTableFixture)
{
    Cursor c = t.read( {"PLATFORM", "NAME"} );
    REQUIRE( c.isStaticColumn( 0 ) );
}
FIXTURE_TEST_CASE(Cursor_IsStaticColumn_False, SequenceTableFixture)
{
    Cursor c = t.read( {"PLATFORM", "NAME"} );
    REQUIRE( ! c.isStaticColumn( 1 ) );
}

// VDB::Cursor::RawData

FIXTURE_TEST_CASE( RawData_asVector_badCast, SequenceTableFixture )
{
    Cursor c = t.read( {"READ_START", "NAME"} );
    Cursor::RawData rd = c.read( 1, 1 );
    REQUIRE_THROW( rd.asVector<uint16_t>() );
}

FIXTURE_TEST_CASE( RawData_asVector, SequenceTableFixture )
{
    Cursor c = t.read( {"READ_START", "NAME"} );
    Cursor::RawData rd = c.read( 1, 0 );
    auto cv = rd.asVector<uint32_t>();
    REQUIRE_EQ( size_t(2), cv.size() );
    REQUIRE_EQ( uint32_t(0), cv[0] );
    REQUIRE_EQ( uint32_t(301), cv[1] );
}

FIXTURE_TEST_CASE( RawData_value_badCast, SequenceTableFixture )
{
    Cursor c = t.read( {"SPOT_LEN", "NAME"} );
    Cursor::RawData rd = c.read( 1, 1 );
    REQUIRE_THROW( rd.value<uint16_t>() );
}

FIXTURE_TEST_CASE( RawData_value, SequenceTableFixture )
{
    Cursor c = t.read( {"SPOT_LEN", "NAME"} );
    Cursor::RawData rd = c.read( 1, 0 );
    auto v = rd.value<uint32_t>();
    REQUIRE_EQ( uint32_t(602), v );
}

// VDB::Metadata

TEST_CASE(Database_Metadata)
{
    Database d = Manager().openDatabase( DatabasePath );
    MetadataCollection md = d.metadata();
}

TEST_CASE(Metadata_hasChild_Yes)
{
    Database d = Manager().openDatabase( DatabasePath );
    MetadataCollection md = d.metadata();
    REQUIRE( md.hasChildNode( "SOFTWARE" ) );
}

TEST_CASE(Metadata_hasChild_NO)
{
    Database d = Manager().openDatabase( DatabasePath );
    MetadataCollection md = d.metadata();
    REQUIRE( ! md.hasChildNode( "HARDWARE" ) );
}

TEST_CASE(Metadata_hasChild_Deep)
{
    Database d = Manager().openDatabase( DatabasePath );
    MetadataCollection md = d.metadata();
    REQUIRE( md.hasChildNode( "SOFTWARE/formatter" ) );
}


int main (int argc, char *argv [])
{
    return VdbTestSuite(argc, argv);
}
