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

#include <sysalloc.h>

#include <ktst/unit_test.hpp>

#include <klib/printf.h>

#include <vdb/database.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <insdc/sra.h>

using namespace std;

TEST_SUITE( WVdbTestSuite )

const string ScratchDir = "./db/";

class WVDB_Fixture
{
public:
    WVDB_Fixture()
    :   m_mgr ( 0 ),
        m_schema ( 0 ),
        m_db ( 0 ),
        m_keepDb ( false )
    {
    }
    ~WVDB_Fixture()
    {
        if ( m_db != 0 )
        {
            VDatabaseRelease ( m_db );
        }
        if ( m_schema != 0 )
        {
            VSchemaRelease ( m_schema );
        }
        if ( m_mgr != 0 )
        {
            VDBManagerRelease ( m_mgr );
        }

        if ( ! m_keepDb )
        {
            RemoveDatabase();
        }
    }

    void RemoveDatabase ()
    {
        if ( ! m_databaseName . empty () )
        {
            KDirectory* wd;
            KDirectoryNativeDir ( & wd );
            KDirectoryRemove ( wd, true, m_databaseName . c_str () );
            KDirectoryRelease ( wd );
        }
    }

    virtual void ParseSchema ( VSchema * p_schema, const string & p_schemaText, const string & p_includes )
    {
        THROW_ON_RC ( VSchemaAddIncludePaths ( m_schema, p_includes.length(), p_includes.c_str() ) );
        THROW_ON_RC ( VSchemaParseText ( p_schema, NULL, p_schemaText . c_str(), p_schemaText . size () ) );
    }

    void MakeDatabase ( const string & p_testName,
                        const string & p_schemaText,
                        const string & p_schemaSpec,
                        const char * includes )
    {
        m_databaseName = ScratchDir + p_testName;
        RemoveDatabase();

        THROW_ON_RC ( VDBManagerMakeUpdate ( & m_mgr, NULL ) );
        THROW_ON_RC ( VDBManagerMakeSchema ( m_mgr, & m_schema ) );
        ParseSchema ( m_schema, p_schemaText, includes );
        THROW_ON_RC ( VDBManagerCreateDB ( m_mgr,
                                          & m_db,
                                          m_schema,
                                          p_schemaSpec . c_str (),
                                          kcmInit + kcmMD5 + kcmParents,
                                          "%s",
                                          m_databaseName . c_str () ) );
    }

    /// @brief Create table, specifying column parameters
    /// @param p_tableName name of the table to create.
    /// @param checksum type of blob checksums.
    /// @param create create mode for columns.
    /// @param pgsize size of pages.
    /// @return write cursor.
    VCursor * CreateTable ( const char * p_tableName, KChecksum checksum, KCreateMode create = kcmCreate, size_t pgsize = 0 ) // returns write cursor
    {
        VTable * table = nullptr;
        THROW_ON_RC ( VDatabaseCreateTable ( m_db, & table, p_tableName, kcmCreate | kcmMD5, "%s", p_tableName ) );
        THROW_ON_RC ( VTableColumnCreateParams ( table, create, checksum, pgsize ) );

        VCursor * ret = nullptr;
        THROW_ON_RC ( VTableCreateCursorWrite ( table, & ret, kcmInsert ) );
        THROW_ON_RC ( VTableRelease ( table ) );

        return ret;
    }

    void WriteRow ( VCursor * p_cursor, uint32_t p_colIdx, const string & p_value )
    {
        THROW_ON_RC ( VCursorOpenRow ( p_cursor ) );
        THROW_ON_RC ( VCursorWrite ( p_cursor, p_colIdx, 8, p_value . c_str (), 0, p_value . length () ) );
        THROW_ON_RC ( VCursorCommitRow ( p_cursor ) );
        THROW_ON_RC ( VCursorCloseRow ( p_cursor ) );
    }

    string   m_databaseName;
    VDBManager *    m_mgr;
    VSchema *       m_schema;
    VDatabase *     m_db;
    bool            m_keepDb;
};

FIXTURE_TEST_CASE ( PlatformNames, WVDB_Fixture )
{
    const char * schemaText =
        "include 'ncbi/sra.vschema';"
        "table foo #1 = NCBI:SRA:tbl:sra #2 {"
        "   INSDC:SRA:platform_id out_platform = < INSDC:SRA:platform_id > echo < %s > ();"
        "};";

    static const char *platform_symbolic_names[] = { INSDC_SRA_PLATFORM_SYMBOLS };
    size_t platform_count = sizeof( platform_symbolic_names ) / sizeof( * platform_symbolic_names );
    for ( size_t platformId = 0; platformId < platform_count; ++ platformId )
    {
        char schema[4096];
        string_printf( schema, sizeof( schema ), nullptr, schemaText, platform_symbolic_names[ platformId ] );
        {
            REQUIRE_RC ( VDBManagerMakeUpdate ( & m_mgr, NULL ) );
            REQUIRE_RC ( VDBManagerMakeSchema ( m_mgr, & m_schema ) );
            ParseSchema ( m_schema, schema, "../../libs/schema:../../../ncbi-vdb/interfaces" );

            VTable * table;
            REQUIRE_RC ( VDBManagerCreateTable ( m_mgr,
                                                & table,
                                                m_schema,
                                                "foo",
                                                kcmInit + kcmMD5 + kcmParents,
                                                "%s%u",
                                                (ScratchDir + GetName()).c_str(), platformId ) );

            {
                VCursor * cursor;
                REQUIRE_RC ( VTableCreateCursorWrite ( table, & cursor, kcmInsert ) );
                uint32_t column_idx;
                REQUIRE_RC ( VCursorAddColumn ( cursor, & column_idx, "LABEL" ) );
                REQUIRE_RC ( VCursorOpen ( cursor ) );

                WriteRow ( cursor, column_idx, "L1" );
                WriteRow ( cursor, column_idx, "L2" );

                REQUIRE_RC ( VCursorCommit ( cursor ) );
                REQUIRE_RC ( VCursorRelease ( cursor ) );
            }
            REQUIRE_RC ( VTableRelease ( table ) );
        }

        {   // reopen
            VDBManager * mgr;
            REQUIRE_RC ( VDBManagerMakeUpdate ( & mgr, NULL ) );
            const VTable * table;
            REQUIRE_RC ( VDBManagerOpenTableRead ( mgr, & table, NULL, "%s%u",
                                                (ScratchDir + GetName()).c_str(), platformId ) );
            const VCursor* cursor;
            REQUIRE_RC ( VTableCreateCursorRead ( table, & cursor ) );

            uint32_t column_idx;
            REQUIRE_RC ( VCursorAddColumn ( cursor, & column_idx, "PLATFORM" ) );
            REQUIRE_RC ( VCursorOpen ( cursor ) );

            // verify platform id
            INSDC_SRA_platform_id id = SRA_PLATFORM_UNDEFINED;
            uint32_t row_len = 0;
            REQUIRE_RC ( VCursorReadDirect(cursor, 1, column_idx, 8, (void*) & id, 1, & row_len ) );
            REQUIRE_EQ ( 1u, row_len );
            REQUIRE_EQ ( platformId, (size_t)id );

            REQUIRE_RC ( VCursorRelease ( cursor ) );
            REQUIRE_RC ( VTableRelease ( table ) );
            REQUIRE_RC ( VDBManagerRelease ( mgr ) );
        }

        REQUIRE_RC ( VSchemaRelease ( m_schema ) ); m_schema = nullptr;
        REQUIRE_RC ( VDBManagerRelease ( m_mgr ) ); m_mgr = nullptr;
    }
}

FIXTURE_TEST_CASE ( ProductionSchema, WVDB_Fixture)
{
    string const schemaText = R"(include "sra/generic-fastq.vschema";)";
    m_keepDb = true;
    MakeDatabase ( GetName(), schemaText, "NCBI:SRA:GenericFastq:db", "../../libs/schema:../../../ncbi-vdb/interfaces" );

    const char* TableName = "SEQUENCE";
    const char* ColumnName = "(INSDC:dna:text)READ";
    {
        uint32_t column_idx = 0;
        auto const cursor = CreateTable ( TableName, kcsNone );

        REQUIRE_RC ( VCursorAddColumn ( cursor, & column_idx, ColumnName ) );
        REQUIRE_RC ( VCursorOpen ( cursor ) );
        // insert some rows
        WriteRow ( cursor, column_idx, string("AAAAA") );
        WriteRow ( cursor, column_idx, string("CCCCCC") );
        WriteRow ( cursor, column_idx, string("GGGG") );
        WriteRow ( cursor, column_idx, string("TT") );
        REQUIRE_RC ( VCursorCommit ( cursor ) );
        REQUIRE_RC ( VCursorRelease ( cursor ) );
    }
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return WVdbTestSuite(argc, argv);
}

}
