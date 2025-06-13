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
* Unit tests for the SRA schema library
*/

#include <ktst/unit_test.hpp>


#include <klib/out.h>
#include <klib/rc.h>
#include <klib/debug.h>

#include <kfs/directory.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <schema/transform-functions.h>

#define __mod__ "test-schema"
#include <../test/vdb/WVDB_Fixture.hpp>

using namespace std;

TEST_SUITE(SraSchemaTestSuite);

class WVDB_WriteReadFixture : public WVDB_Fixture
{
public:
    WVDB_WriteReadFixture()
    {
        // link in sra-owned schema functions
        THROW_ON_RC( SraLinkSchema( m_mgr ) );
    }

    // helpers for reading
    uint32_t AddColumn( const VCursor * cursor, const string & name )
    {
        uint32_t column_idx = 0;
        THROW_ON_RC ( VCursorAddColumn ( cursor, & column_idx, name.c_str() ) );
        return column_idx;
    }

    string ReadString( const VCursor * cursor, int64_t rowId, uint32_t column_idx )
    {
        char buf[1024];
        uint32_t rowLen = 0;
        THROW_ON_RC( VCursorReadDirect ( cursor, rowId, column_idx, 8, buf, sizeof ( buf ), & rowLen ) );
        return string( buf, rowLen );
    }
    uint32_t ReadU32( const VCursor * cursor, int64_t rowId, uint32_t column_idx )
    {
        uint32_t ret = 0;
        uint32_t rowLen = 0;
        THROW_ON_RC( VCursorReadDirect ( cursor, rowId, column_idx, 8, &ret, sizeof ( ret ), & rowLen ) );
        return ret;
    }
};

FIXTURE_TEST_CASE( UseSraFunction, WVDB_WriteReadFixture )
{
    const char* TableName = "tbl";
    const char* ColumnName = "label";
    const string schemaText =
"function < type T > T hello #1.0 <T val> ( * any row_len ) = sra:hello;\n"
"table T #1 \n"
"{\n"
"    column ascii label = < ascii > hello < 'label' > ();\n"
"};\n"
"database db #1\n"
"{\n"
"    table T #1 tbl;\n"
"};\n"
;

    MakeDatabase( GetName(), schemaText, "db" );

    {
        VCursor * cursor = CreateTable( TableName );
        REQUIRE_RC ( VCursorOpen ( cursor ) );
        REQUIRE_RC ( VCursorCommit ( cursor ) );
        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }

    {
        const VCursor * cursor = OpenTable( TableName );
        uint32_t column_idx = AddColumn( cursor, ColumnName );
        REQUIRE_RC ( VCursorOpen ( cursor ) );

        // verify that sra:hello() has been called
        REQUIRE_EQ( string("hello"), ReadString ( cursor, 1, column_idx ) );

        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }
}

FIXTURE_TEST_CASE( NameFormatting, WVDB_WriteReadFixture )
{
    const char* TableName = "tbl";
    const string schemaText =
"include 'ncbi/spotname.vschema';"
"include 'sra/illumina.vschema';"
"table T #1 = NCBI:SRA:tbl:skeyname #3.1 \n"
"{\n"
    "NCBI:SRA:spot_name_token in_spot_name_tok"
        "= NCBI:SRA:Illumina:tokenize_spot_name ( NAME );"
    "extern column ascii NAMEFMT = idx:text:project<\"skey\">();"
"};\n"
"database db #1\n"
"{\n"
"    table T #1 tbl;\n"
"};\n"
;
    //m_keepDb = true;
    MakeDatabase( GetName(), schemaText, "db", "../../../ncbi-vdb/interfaces/" );

    const char* ColumnName = "NAME";
    const string Name1 = "name:1:2:3:4";
    const string Name2 = "_name:5:6:7:8";

    {
        VCursor * cursor = CreateTable( TableName );
        uint32_t column_idx_Name = AddColumn( cursor, ColumnName );
        REQUIRE_RC ( VCursorOpen ( cursor ) );

        REQUIRE_RC ( VCursorOpenRow ( cursor ) );

        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_Name, 8, Name1.c_str(), 0, Name1.length() ) );

        REQUIRE_RC ( VCursorCommitRow ( cursor ) );
        REQUIRE_RC ( VCursorCloseRow ( cursor ) );

        REQUIRE_RC ( VCursorOpenRow ( cursor ) );

        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_Name, 8, Name2.c_str(), 0, Name2.length() ) );

        REQUIRE_RC ( VCursorCommitRow ( cursor ) );
        REQUIRE_RC ( VCursorCloseRow ( cursor ) );

        REQUIRE_RC ( VCursorCommit ( cursor ) );
        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }

    {
        const VCursor * cursor = OpenTable( TableName );
        uint32_t column_idx_Name = AddColumn( cursor, ColumnName );
        uint32_t column_idx_NameFmt = AddColumn( cursor, "NAMEFMT" );
        uint32_t column_idx_X = AddColumn ( cursor, "X" );
        uint32_t column_idx_Y = AddColumn ( cursor, "Y" );

        REQUIRE_RC ( VCursorOpen ( cursor ) );

        // NAME was converted into a format string
        REQUIRE_EQ( string("name:1:2:$X:$Y"), ReadString ( cursor, 1, column_idx_NameFmt ) );
        REQUIRE_EQ( string("_name:5:6:$X:$Y"), ReadString ( cursor, 2, column_idx_NameFmt ) );

        // X and Y were extracted from NAME and stored in their own numeric columns
        REQUIRE_EQ( 3u, ReadU32( cursor, 1, column_idx_X ) );
        REQUIRE_EQ( 7u, ReadU32( cursor, 2, column_idx_X ) );
        REQUIRE_EQ( 4u, ReadU32( cursor, 1, column_idx_Y ) );
        REQUIRE_EQ( 8u, ReadU32( cursor, 2, column_idx_Y ) );

        // NAME gets reassembled on read from NAME_FMT, X and Y
        REQUIRE_EQ( Name1, ReadString ( cursor, 1, column_idx_Name ) );
        REQUIRE_EQ( Name2, ReadString ( cursor, 2, column_idx_Name ) );

        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }
}

FIXTURE_TEST_CASE( NameFormatting_via_NCBI_spotname, WVDB_WriteReadFixture )
{
    // saving NAME_FMT, X and Y directly, without using schema functions
    // to parse NAME

    const char* TableName = "tbl";
    const string schemaText =
"include 'ncbi/spotname.vschema';"
"include 'sra/illumina.vschema';"
"table T #1 =  NCBI:SRA:tbl:spotname #1 \n" // <= this parent table support direct writing
"{\n"
    "NCBI:SRA:spot_name_token in_spot_name_tok"
        "= NCBI:SRA:Illumina:tokenize_spot_name ( NAME );"
    "INSDC:coord:val in_x_coord = cast ( X );"
    "INSDC:coord:val in_y_coord = cast ( Y );"
"};\n"
"database db #1\n"
"{\n"
"    table T #1 tbl;\n"
"};\n"
;
    // m_keepDb = true;
    // KDbgSetString("VDB");

    REQUIRE_RC ( VDBManagerAddSchemaIncludePath ( m_mgr, "%s", "../../libs/schema" ) );
    REQUIRE_RC ( VDBManagerAddSchemaIncludePath ( m_mgr, "%s", "../../../ncbi-vdb/interfaces" ) );
    MakeDatabase( GetName(), schemaText, "db" );

    const char* ColumnName = "NAME";
    const char* ColumnNameFmt = "NAME_FMT_2";
    const char* ColumnX = "X";
    const char* ColumnY = "Y";
    {
        VCursor * cursor = CreateTable( TableName );
        uint32_t column_idx_Name = AddColumn( cursor, ColumnNameFmt );
        uint32_t column_idx_X = AddColumn( cursor, ColumnX );
        uint32_t column_idx_Y = AddColumn( cursor, ColumnY );

        REQUIRE_RC ( VCursorOpen ( cursor ) );

        REQUIRE_RC ( VCursorOpenRow ( cursor ) );
        const string NameFmt1 = "name:1:2:$X:$Y";
        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_Name, 8, NameFmt1.c_str(), 0, NameFmt1.length() ) );
        const uint32_t x1 = 3;
        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_X, 8, &x1, 0, sizeof(x1) ) );
        const uint32_t y1 = 4;
        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_Y, 8, &y1, 0, sizeof(y1) ) );

        REQUIRE_RC ( VCursorCommitRow ( cursor ) );
        REQUIRE_RC ( VCursorCloseRow ( cursor ) );

        REQUIRE_RC ( VCursorOpenRow ( cursor ) );

        const string NameFmt2 = "_name:5:6:$X:$Y";
        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_Name, 8, NameFmt2.c_str(), 0, NameFmt2.length() ) );
        const uint32_t x2 = 7;
        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_X, 8, &x2, 0, sizeof(x2) ) );
        const uint32_t y2 = 8;
        REQUIRE_RC ( VCursorWrite ( cursor, column_idx_Y, 8, &y2, 0, sizeof(y2) ) );

        REQUIRE_RC ( VCursorCommitRow ( cursor ) );
        REQUIRE_RC ( VCursorCloseRow ( cursor ) );

        REQUIRE_RC ( VCursorCommit ( cursor ) );
        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }

    {
        const VCursor * cursor = OpenTable( TableName );
        uint32_t column_idx_Name = AddColumn( cursor, ColumnName );

        REQUIRE_RC ( VCursorOpen ( cursor ) );

        // NAME gets reassembled on read from NAME_FMT, X and Y
        const string Name1 = "name:1:2:3:4";
        REQUIRE_EQ( Name1, ReadString ( cursor, 1, column_idx_Name ) );
        const string Name2 = "_name:5:6:7:8";
        REQUIRE_EQ( Name2, ReadString ( cursor, 2, column_idx_Name ) );

        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }
}

FIXTURE_TEST_CASE( ParseNewIlluminaSchema, WVDB_WriteReadFixture )
{
    const string schemaText =
"include 'sra/illumina.vschema';"
"table T #1 =  NCBI:SRA:Illumina:tbl:phred:v2 #3 {};\n" // this one derives from NCBI:SRA:tbl:spotname
"database db #1\n"
"{\n"
"    table T #1 tbl;\n"
"};\n"
;

    REQUIRE_RC ( VDBManagerAddSchemaIncludePath ( m_mgr, "%s", "../../libs/schema" ) );
    REQUIRE_RC ( VDBManagerAddSchemaIncludePath ( m_mgr, "%s", "../../../ncbi-vdb/interfaces" ) );
    MakeDatabase( GetName(), schemaText, "db" );
}

//////////////////////////////////////////// Main
#include <kfg/config.h>
#include <kapp/main.h>
int main ( int argc, char *argv [] )
{
    VDB::Application app(argc, argv);
    KConfigDisableUserSettings();
    //KDbgSetString("VDB");
    return SraSchemaTestSuite(argc, argv);
}
