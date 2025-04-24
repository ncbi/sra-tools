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

#include <kfs/directory.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <schema/transform-functions.h>

using namespace std;

TEST_SUITE(SraSchemaTestSuite);

const std :: string ScratchDir = "./db/";

TEST_CASE( UseSraFunction )
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
    VDBManager *    m_mgr;
    VSchema *       m_schema;
    VDatabase *     m_db;

    REQUIRE_RC ( VDBManagerMakeUpdate ( & m_mgr, NULL ) );

    REQUIRE_RC( SraLinkSchema( m_mgr ) );  // <====== this call links in sra-owned schema functions

    string m_databaseName = ScratchDir + "db";
    if ( ! m_databaseName . empty () )
    {
        KDirectory* wd;
        KDirectoryNativeDir ( & wd );
        KDirectoryRemove ( wd, true, m_databaseName . c_str () );
        KDirectoryRelease ( wd );
    }

    REQUIRE_RC ( VDBManagerMakeSchema ( m_mgr, & m_schema ) );
    REQUIRE_RC ( VSchemaParseText ( m_schema, NULL, schemaText . c_str(), schemaText . size () ) );
    REQUIRE_RC ( VDBManagerCreateDB ( m_mgr,
                                        & m_db,
                                        m_schema,
                                        "db",
                                        kcmInit + kcmMD5 + kcmParents,
                                        "%s",
                                        m_databaseName . c_str () ) );

    {
        VTable* table;
        REQUIRE_RC ( VDatabaseCreateTable ( m_db, & table, TableName, kcmCreate | kcmMD5, "%s", TableName ) );
        VCursor * cursor;
        REQUIRE_RC ( VTableCreateCursorWrite ( table, & cursor, kcmInsert ) );
        REQUIRE_RC ( VTableRelease ( table ) );

        REQUIRE_RC ( VCursorOpen ( cursor ) );
        REQUIRE_RC ( VCursorCommit ( cursor ) );
        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }

    {
        const VTable* table;
        REQUIRE_RC ( VDatabaseOpenTableRead ( m_db, & table, "%s", TableName ) );
        const VCursor * cursor;
        REQUIRE_RC ( VTableCreateCursorRead ( table, & cursor ) );
        REQUIRE_RC ( VTableRelease ( table ) );

        uint32_t column_idx;
        REQUIRE_RC ( VCursorAddColumn ( cursor, & column_idx, ColumnName ) );
        REQUIRE_RC ( VCursorOpen ( cursor ) );

        char buf[1024];
        uint32_t rowLen = 0;
        REQUIRE_RC( VCursorReadDirect ( cursor, 1, column_idx, 8, buf, sizeof ( buf ), & rowLen ) );

        REQUIRE_RC ( VCursorRelease ( cursor ) );

        REQUIRE_EQ( string("hello"), string( buf, rowLen ) ); // sra:hello() has been called
    }

    VDatabaseRelease ( m_db );
    VSchemaRelease ( m_schema );
    VDBManagerRelease ( m_mgr );
}

//////////////////////////////////////////// Main
#include <kfg/config.h>
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return SraSchemaTestSuite(argc, argv);
}
