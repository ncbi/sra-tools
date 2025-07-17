// ===========================================================================
//
//                            PUBLIC DOMAIN NOTICE
//               National Center for Biotechnology Information
//
//  This software/database is a "United States Government Work" under the
//  terms of the United States Copyright Act.  It was written as part of
//  the author's official duties as a United States Government employee and
//  thus cannot be copyrighted.  This software/database is freely available
//  to the public for use. The National Library of Medicine and the U.S.
//  Government have not placed any restriction on its use or reproduction.
//
//  Although all reasonable efforts have been taken to ensure the accuracy
//  and reliability of the software and data, the NLM and the U.S.
//  Government do not and cannot warrant the performance or results that
//  may be obtained by using this software or data. The NLM and the U.S.
//  Government disclaim all warranties, express or implied, including
//  warranties of performance, merchantability or fitness for any particular
//  purpose.
//
//  Please cite the author in any work or product based on this material.
//
// ===========================================================================

#include <sstream>
#include <cstdlib>

#include <kapp/main.h>

#include <vdb/manager.h> // VDBManager
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/schema.h> /* VSchemaRelease */
#include <vdb/vdb-priv.h>

#include <sra/sraschema.h> // VDBManagerMakeSRASchema

#include <kdb/meta.h>
#include <kdb/table.h>

#include <klib/rc.h>

#include <kfg/config.h>

#include <ktst/unit_test.hpp> // TEST_CASE

#include <sysalloc.h>


using namespace std;

/*
 * We will compile this test file first in read/write mode, create DB and
 * run tests using libncbi-wvdb library; then use  the database(s) it created
 * in other test suites
 */

TEST_SUITE( NgsMakeDbSuite )

const string ScratchDir = "./";

class VDB_Fixture
{
public:
    VDB_Fixture()
    : m_table ( 0 )
    {
        THROW_ON_RC ( KDirectoryNativeDir ( & m_wd ) );
        THROW_ON_RC ( VDBManagerMakeUpdate ( & m_mgr, m_wd ) );
    }
    ~VDB_Fixture()
    {
        if ( m_table != 0 )
        {
            VTableRelease ( m_table );
        }
        VDBManagerRelease ( m_mgr );
        KDirectoryRelease ( m_wd );
    }

    void RemoveDatabase()
    {
        if ( ! m_tableName . empty () )
        {
            KDirectoryRemove ( m_wd, true, m_tableName . c_str () );
        }
    }

    VCursor* MakeCursor()
    {
        const string schemaText =
            "typedef ascii INSDC:dna:text;\n"
            "typedef U32 INSDC:coord:len;\n"
            "typedef U8 INSDC:SRA:xread_type;\n"
            "table SEQUENCE #1\n"
            "{\n"
            "    column INSDC:dna:text READ;\n"
            "    column INSDC:coord:len READ_LEN;\n"
            "    column INSDC:SRA:xread_type READ_TYPE;\n"
            "};\n"
        ;
        const char * schemaSpec = "SEQUENCE";

        // make table
        VSchema* schema;
        THROW_ON_RC ( VDBManagerMakeSchema ( m_mgr, & schema ) );
        THROW_ON_RC ( VSchemaParseText(schema, NULL, schemaText . c_str(), schemaText . size () ) );

        THROW_ON_RC ( VDBManagerCreateTable ( m_mgr,
                                              & m_table,
                                              schema,
                                              schemaSpec,
                                              kcmInit + kcmMD5,
                                              "%s",
                                              m_tableName . c_str () ) );
        THROW_ON_RC ( VSchemaRelease ( schema ) );

        // make cursor
        VCursor* ret;
        THROW_ON_RC ( VTableCreateCursorWrite ( m_table, & ret, kcmInsert ) );

        THROW_ON_RC ( VCursorAddColumn ( ret, & read_colIdx, "READ" ) );
        THROW_ON_RC ( VCursorAddColumn ( ret, & read_len_colIdx, "READ_LEN" ) );
        THROW_ON_RC ( VCursorAddColumn ( ret, & read_type_colIdx, "READ_TYPE" ) ); // 1 = biological

        return ret;
    }

    void AddRow( VCursor* p_cursor, int64_t p_rowId, const string& p_read )
    {
        THROW_ON_RC ( VCursorSetRowId ( p_cursor, p_rowId ) );
        THROW_ON_RC ( VCursorOpenRow ( p_cursor ) );
        THROW_ON_RC ( VCursorWrite ( p_cursor, read_colIdx, 8, p_read.c_str(), 0, p_read.size() ) );
        uint32_t u32=p_read.size();
        THROW_ON_RC ( VCursorWrite ( p_cursor, read_len_colIdx, 32, &u32, 0, 1 ) );
        uint8_t u8=1;
        THROW_ON_RC ( VCursorWrite ( p_cursor, read_type_colIdx, 8, &u8, 0, 1 ) );
        THROW_ON_RC ( VCursorCommitRow ( p_cursor ) );
        THROW_ON_RC ( VCursorCloseRow ( p_cursor ) );
        THROW_ON_RC ( VCursorFlushPage ( p_cursor ) );
    }

    KDirectory* m_wd;

    VDBManager* m_mgr;
    string m_tableName;

    VTable* m_table;

    uint32_t read_colIdx;
    uint32_t read_len_colIdx;
    uint32_t read_type_colIdx;
};

FIXTURE_TEST_CASE ( SparseFragmentBlobs, VDB_Fixture)
{
    RemoveDatabase();
    m_tableName = ScratchDir + GetName();
    VCursor* cursor = MakeCursor();

    REQUIRE_RC ( VCursorOpen ( cursor ) );

    // add rows, with sufficient gaps in between to create NULL blobs
    AddRow ( cursor, 1, "ACGT" );
    AddRow ( cursor, 10, "AACGT" );

    REQUIRE_RC ( VCursorCommit ( cursor ) );
    REQUIRE_RC ( VCursorRelease ( cursor ) );
}

//////////////////////////////////////////// Main

int main(int argc, char* argv[])
{
    VDB::Application app(argc, argv);
    KConfigDisableUserSettings();
    return NgsMakeDbSuite(argc, app.getArgV());
}
