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
* Unit tests for General Loader
*/

#include "general-loader.cpp"
#include "database-loader.cpp"
#include "protocol-parser.cpp"

#include <general-writer/utf8-like-int-codec.h>

#include <ktst/unit_test.hpp>

#include <sysalloc.h>

#include <klib/printf.h>
#include <klib/debug.h>

#include <kns/adapt.h>

#include <kfs/nullfile.h>
#include <kfs/file.h>
#include <kfs/ramfile.h>

#include <kdb/table.h>

#include <kfg/config.h> // KConfigPrint

#include <vfs/path.h>
#include <vfs/path-priv.h>
#include <vfs/manager.h>

#include <vdb/vdb-priv.h>

#include <time.h>
#include <cstring>
#include <stdexcept>
#include <map>
#include <fstream>
#include <cstdio>

#include "testsource.hpp"

using namespace std;
using namespace ncbi::NK;

static KLogLevel l = 4;
static rc_t argsHandler(int argc, char* argv[]) {
    Args* args = NULL;
    rc_t rc = ArgsMakeAndHandle(&args, argc, argv, 0, NULL, 0);
    ArgsWhack(args);
    KLogLevel lv = KLogLevelGet();
    if (lv != 4) {
        l = lv;
    }
    return rc;
}
TEST_SUITE_WITH_ARGS_HANDLER(GeneralLoaderTestSuite, argsHandler);

const string ScratchDir         = "./db/";
const string DefaultSchema      = ScratchDir + "default.vschema";

const string DefaultSchemaText  =
    "table table1 #1.0.0 { column ascii columnAscii; column U32 columnU32; column bool columnBool; };\n"
    "table table2 #1.0.0 { column I64 columnI64; column U8 columnU8; };\n"
    "database root_database #1 { table table1 #1 TABLE1; table table2 #1 TABLE2; };\n";

const string DefaultDatabase    = "root_database";

const string DefaultTable       = "TABLE1";
const string Table2             = "TABLE2";

const string DefaultColumn      = "columnAscii";
const string U32Column          = "columnU32";
const string BoolColumn         = "columnBool";
const string I64Column          = "columnI64";
const string U8Column           = "columnU8";

static
void
ClearScratchDir ()
{
    KDirectory* wd;
    KDirectoryNativeDir ( & wd );
    KDirectoryClearDir ( wd, true, ScratchDir . c_str() );
    KDirectoryRelease ( wd );
}


class GeneralLoaderFixture
{
public:
    static const uint32_t DefaultTableId = 100;

    static const uint32_t DefaultColumnId = 1;
    static const uint32_t Column16Id = 16;
    static const uint32_t Column32Id = 32;
    static const uint32_t Column64Id = 64;

    static std::string argv0;

public:
    GeneralLoaderFixture()
    :   m_db ( 0 ),
        m_cursor ( 0 ),
        m_wd ( 0 )
    {
        THROW_ON_RC ( KDirectoryNativeDir ( & m_wd ) );

        CreateFile ( DefaultSchema, DefaultSchemaText );
    }
    ~GeneralLoaderFixture()
    {
        RemoveDatabase();
        if (l == 4) {
            KLogLevelSet ( klogFatal );
        }
        KDirectoryRelease ( m_wd );
        if ( ! m_tempSchemaFile . empty() )
        {
            remove ( m_tempSchemaFile . c_str() );
        }
    }

    GeneralLoader* MakeLoader ( const struct KFile * p_input )
    {
        struct KStream * inStream;
        THROW_ON_RC ( KStreamFromKFilePair ( & inStream, p_input, 0 ) );

        GeneralLoader* ret = new GeneralLoader ( argv0, * inStream );
        ret -> AddSchemaIncludePath ( ScratchDir );

        THROW_ON_RC ( KStreamRelease ( inStream ) );
        THROW_ON_RC ( KFileRelease ( p_input ) );

        return ret;
    }
    bool RunLoader ( GeneralLoader& p_loader, rc_t p_rc )
    {
        rc_t rc = p_loader.Run();
        if ( rc == p_rc )
        {
            return true;
        }

        char buf[1024];
        string_printf ( buf, sizeof buf, NULL, "Expected rc='%R', actual='%R'", p_rc, rc );
        cerr << buf << endl;
        return false;
    }

    bool Run ( const struct KFile * p_input, rc_t p_rc )
    {
        struct KStream* inStream;
        THROW_ON_RC ( KStreamFromKFilePair ( & inStream, p_input, 0 ) );

        GeneralLoader gl ( argv0, *inStream );
        gl . AddSchemaIncludePath ( ScratchDir );

        rc_t rc = gl.Run();
        bool ret;
        if ( GetRCObject ( rc ) == GetRCObject ( p_rc ) &&
             GetRCState ( rc ) == GetRCState ( p_rc ) )
        {
            ret = true;
        }
        else
        {
            char buf[1024];
            string_printf ( buf, sizeof buf, NULL, "Expected rc='%R', actual='%R'", p_rc, rc );
            cerr << buf << endl;
            ret = false;
        }

        THROW_ON_RC ( KStreamRelease ( inStream ) );
        THROW_ON_RC ( KFileRelease ( p_input ) );

        return ret;
    }

    void RemoveDatabase()
    {
        CloseDatabase();
        if ( ! m_source . GetDatabaseName() . empty () )
        {
            KDirectoryRemove ( m_wd, true, m_source . GetDatabaseName() . c_str() );
        }
    }

    void OpenDatabase( const char* p_dbNameOverride = 0 )
    {
        CloseDatabase();

        VDBManager * vdb;
        THROW_ON_RC ( VDBManagerMakeUpdate ( & vdb, NULL ) );

        try
        {
            THROW_ON_RC ( VDBManagerOpenDBUpdate ( vdb, &m_db, NULL, p_dbNameOverride != 0 ? p_dbNameOverride : m_source . GetDatabaseName() . c_str() ) );
        }
        catch(...)
        {
            VDBManagerRelease ( vdb );
            throw;
        }

        THROW_ON_RC ( VDBManagerRelease ( vdb ) );
    }
    void CloseDatabase()
    {
        if ( m_db != 0 )
        {
            VDatabaseRelease ( m_db );
            m_db = 0;
        }
        if ( m_cursor != 0 )
        {
            VCursorRelease ( m_cursor );
            m_cursor = 0;
        }
    }

    void SetUpStream( const string& p_dbName, const string& p_schema = DefaultSchema, const string& p_schemaName = DefaultDatabase )
    {
        m_source . SchemaEvent ( p_schema, p_schemaName );
        string dbName = ScratchDir + p_dbName;
        if ( m_source.packed )
        {
            dbName += "-packed";
        }
        m_source . DatabaseEvent ( dbName );
    }
    void SetUpStream_OneTable( const string& p_dbName )
    {
        SetUpStream( p_dbName );
        m_source . NewTableEvent ( DefaultTableId, DefaultTable );
    }
    void OpenStream_OneTableOneColumn ( const string& p_dbName )
    {
        SetUpStream_OneTable( p_dbName );
        m_source . NewColumnEvent ( DefaultColumnId, DefaultTableId, DefaultColumn, 8 );
        m_source . OpenStreamEvent();
    }

    bool SetUpForIntegerCompression( const string& p_dbName )
    {
        if ( ! TestSource::packed )
            return false; // integer compaction is used in packed mode only

        m_tempSchemaFile = ScratchDir + string ( p_dbName ) + ".vschema";
        string schemaText =
                "table table1 #1.0.0\n"
                "{\n"
                "    column U16 column16;\n"
                "    column U32 column32;\n"
                "    column U64 column64;\n"
                "};\n"
                "database database1 #1\n"
                "{\n"
                "    table table1 #1 TABLE1;\n"
                "};\n"
            ;
        CreateFile ( m_tempSchemaFile, schemaText );
        SetUpStream ( p_dbName, m_tempSchemaFile, "database1" );

        m_source . NewTableEvent ( DefaultTableId, "TABLE1" );

        m_source . NewColumnEvent ( Column16Id, DefaultTableId, "column16", 16, true );
        m_source . NewColumnEvent ( Column32Id, DefaultTableId, "column32", 32, true );
        m_source . NewColumnEvent ( Column64Id, DefaultTableId, "column64", 64, true );

        return true;
    }

    void OpenCursor( const string& p_table, const string& p_column )
    {
        OpenDatabase();
        const VTable * tbl;
        THROW_ON_RC ( VDatabaseOpenTableRead ( m_db, &tbl, p_table.c_str() ) );
        try
        {
            THROW_ON_RC ( VTableCreateCursorRead ( tbl, & m_cursor ) );

            uint32_t idx;
            THROW_ON_RC ( VCursorAddColumn ( m_cursor, &idx, p_column.c_str() ) );
            THROW_ON_RC ( VCursorOpen ( m_cursor ) );
            THROW_ON_RC ( VTableRelease ( tbl ) );
        }
        catch (...)
        {
            VTableRelease ( tbl );
            throw;
        }
    }

    template < typename T > T GetValue ( const string& p_table, const string& p_column, uint64_t p_row )
    {
        OpenCursor( p_table, p_column );
        THROW_ON_RC ( VCursorSetRowId ( m_cursor, p_row ) );
        THROW_ON_RC ( VCursorOpenRow ( m_cursor ) );

        T ret;
        uint32_t num_read;
        THROW_ON_RC ( VCursorRead ( m_cursor, 1, 8 * sizeof ( T ), &ret, 1, &num_read ) );
        THROW_ON_RC ( VCursorCloseRow ( m_cursor ) );
        return ret;
    }

    template < typename T > bool IsNullValue ( const string& p_table, const string& p_column, uint64_t p_row )
    {
        OpenCursor( p_table, p_column );
        THROW_ON_RC ( VCursorSetRowId ( m_cursor, p_row ) );
        THROW_ON_RC ( VCursorOpenRow ( m_cursor ) );

        T ret;
        uint32_t num_read;
        THROW_ON_RC ( VCursorRead ( m_cursor, 1, 8 * sizeof ( T ), &ret, 1, &num_read ) );
        THROW_ON_RC ( VCursorCloseRow ( m_cursor ) );
        return num_read == 0;
    }

    template < typename T > T GetValueWithIndex ( const string& p_table, const string& p_column, uint64_t p_row, uint32_t p_count, size_t p_index )
    {
        OpenCursor( p_table, p_column );
        THROW_ON_RC ( VCursorSetRowId ( m_cursor, p_row ) );
        THROW_ON_RC ( VCursorOpenRow ( m_cursor ) );

        assert(1024 >= p_count);
        T ret [ 1024 ];

        uint32_t num_read;
        THROW_ON_RC ( VCursorRead ( m_cursor, 1, (uint32_t) ( 8 * sizeof ( T ) ), &ret, p_count, &num_read ) );
        THROW_ON_RC ( VCursorCloseRow ( m_cursor ) );
        return ret [  p_index ];
    }

    void FullLog()
    {
        KLogLevelSet ( klogInfo );
    }

    void CreateFile ( const string& p_name, const string& p_content )
    {
        ofstream out( p_name . c_str() );
        out << p_content;
    }

    std::string GetMetadata ( const KMetadata* p_meta, const std::string& p_node )
    {
        const KMDataNode *node;
        THROW_ON_RC ( KMetadataOpenNodeRead ( p_meta, &node, p_node.c_str() ) );

        size_t num_read;
        char buf[ 256 ];

        THROW_ON_RC ( KMDataNodeReadCString ( node, buf, sizeof buf, &num_read ) );

        THROW_ON_RC ( KMDataNodeRelease ( node ) );
        return string ( buf, num_read );

    }
    std::string GetMetadataAttr ( const KMetadata* p_meta, const std::string& p_node, const std::string& p_attr )
    {
        const KMDataNode *node;
        THROW_ON_RC ( KMetadataOpenNodeRead ( p_meta, &node, p_node.c_str() ) );

        size_t num_read;
        char buf[ 256 ];

        THROW_ON_RC ( KMDataNodeReadAttr ( node, p_attr.c_str(),
        buf, sizeof buf, &num_read ) );

        THROW_ON_RC ( KMDataNodeRelease ( node ) );
        return string ( buf, num_read );

    }

    std::string GetDbMetadata ( VDatabase* p_db, const std::string& p_node )
    {
        const KMetadata *meta;
        THROW_ON_RC ( VDatabaseOpenMetadataRead ( p_db, &meta ) );
        string ret = GetMetadata ( meta, p_node );
        THROW_ON_RC ( KMetadataRelease ( meta ) );
        return ret;
    }

    std::string GetDbMetadataAttr ( const VDatabase* p_db, const std::string& p_node, const std::string& p_attr )
    {
        const KMetadata *meta;
        THROW_ON_RC ( VDatabaseOpenMetadataRead ( p_db, &meta ) );
        string ret = GetMetadataAttr ( meta, p_node, p_attr );
        THROW_ON_RC ( KMetadataRelease ( meta ) );
        return ret;
    }
    std::string GetTblMetadataAttr ( const VTable* p_tbl, const std::string& p_node, const std::string& p_attr )
    {
        const KMetadata *meta;
        THROW_ON_RC ( VTableOpenMetadataRead ( p_tbl, &meta ) );
        string ret = GetMetadataAttr ( meta, p_node, p_attr );
        THROW_ON_RC ( KMetadataRelease ( meta ) );
        return ret;
    }

    TestSource      m_source;
    VDatabase *     m_db;
    const VCursor * m_cursor;
    KDirectory*     m_wd;
    string          m_tempSchemaFile;
	string			m_ScratchDir;
};

template<> std::string GeneralLoaderFixture::GetValue ( const string& p_table, const string& p_column, uint64_t p_row )
{
    OpenCursor( p_table, p_column );
    THROW_ON_RC ( VCursorSetRowId ( m_cursor, p_row ) );
    THROW_ON_RC ( VCursorOpenRow ( m_cursor ) );

    char buf[1024];
    uint32_t num_read;
    THROW_ON_RC ( VCursorRead ( m_cursor, 1, 8, &buf, sizeof buf, &num_read ) );
    THROW_ON_RC ( VCursorCloseRow ( m_cursor ) );
    return string ( buf, num_read );
}

std::string GeneralLoaderFixture :: argv0;

FIXTURE_TEST_CASE ( EmptyInput, GeneralLoaderFixture )
{
    const struct KFile * input;
    REQUIRE_RC ( KFileMakeNullRead ( & input ) );
    REQUIRE ( Run ( input, SILENT_RC ( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete ) ) );
}

FIXTURE_TEST_CASE ( ShortInput, GeneralLoaderFixture )
{
    char buffer[10];
    const struct KFile * input;
    REQUIRE_RC ( KRamFileMakeRead ( & input, buffer, sizeof buffer ) );

    REQUIRE ( Run ( input, SILENT_RC ( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete ) ) );
}

FIXTURE_TEST_CASE ( BadSignature, GeneralLoaderFixture )
{
    TestSource ts ( "badsigna" );
    REQUIRE ( Run (  ts . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcHeader, rcCorrupt ) ) );
}

FIXTURE_TEST_CASE ( BadEndianness, GeneralLoaderFixture )
{
    TestSource ts ( GeneralLoaderSignatureString, 3 );
    REQUIRE ( Run ( ts . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcFormat, rcInvalid) ) );
}

FIXTURE_TEST_CASE ( ReverseEndianness, GeneralLoaderFixture )
{
    TestSource ts ( GeneralLoaderSignatureString, GW_REVERSE_ENDIAN );
    REQUIRE ( Run ( ts . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcFormat, rcUnsupported ) ) );
}

FIXTURE_TEST_CASE ( LaterVersion, GeneralLoaderFixture )
{
    TestSource ts ( GeneralLoaderSignatureString, GW_GOOD_ENDIAN, GW_CURRENT_VERSION + 1 );
    REQUIRE ( Run ( ts . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcHeader, rcBadVersion ) ) );
}

//TODO: MakeTruncatedSource (stop in the middle of an event)


FIXTURE_TEST_CASE ( BadSchemaFileName, GeneralLoaderFixture )
{
    m_source . SchemaEvent ( "this file should not exist", "someSchemaName" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadSchemaFileName_Long, GeneralLoaderFixture )
{
    m_source . SchemaEvent ( string ( GeneralLoader :: MaxPackedString + 1, 'x' ), "someSchemaName" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadSchemaName, GeneralLoaderFixture )
{
    SetUpStream ( GetName(), "align/align.vschema", "bad schema name" );
    m_source . OpenStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadSchemaName_Long, GeneralLoaderFixture )
{
    SetUpStream ( GetName(), "align/align.vschema", string ( GeneralLoader :: MaxPackedString + 1, 'x' ) );
    m_source . OpenStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadTableName, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 1, "nosuchtable" );
    m_source . OpenStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadTableName_Long, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 1, string ( GeneralLoader :: MaxPackedString + 1, 'x' ) );
    m_source . OpenStreamEvent();

    // the expected return code is different here due to VDB's internal limitation on 256 characters in a table name
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcDB,rcDirectory,rcResolving,rcPath,rcExcessive ) ) );
}

FIXTURE_TEST_CASE ( DuplicateTableId, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 1, DefaultTable );
    m_source . NewTableEvent ( 1, "differentTable" ); // same Id
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcTable, rcExists ) ) );
}

FIXTURE_TEST_CASE ( BadColumnName, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( DefaultTableId, DefaultTable );
    m_source . NewColumnEvent ( 1, DefaultTableId, "nosuchcolumn", 8 );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcCursor, rcUpdating, rcColumn, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadTableId, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 1, DefaultTable );
    m_source . NewColumnEvent ( 1, 2, DefaultColumn, 8 );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcTable, rcInvalid ) ) );
}

FIXTURE_TEST_CASE ( DuplicateColumnName, GeneralLoaderFixture )
{
    SetUpStream_OneTable ( GetName() );
    m_source . NewColumnEvent ( 1, DefaultTableId, DefaultColumn, 8 );
    m_source . NewColumnEvent ( 2, DefaultTableId, DefaultColumn, 8 );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcCursor, rcUpdating, rcColumn, rcExists ) ) );
}

FIXTURE_TEST_CASE ( DuplicateColumnId, GeneralLoaderFixture )
{
    SetUpStream_OneTable ( GetName() );
    m_source . NewColumnEvent ( 1, DefaultTableId, DefaultColumn, 8 );
    m_source . NewColumnEvent ( 1, DefaultTableId, "NAME", 8 );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcColumn, rcExists ) ) );
}

FIXTURE_TEST_CASE ( NoOpenStreamEvent, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete ) ) );
}

FIXTURE_TEST_CASE ( NoCloseStreamEvent, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . OpenStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete ) ) );
}

FIXTURE_TEST_CASE ( NoColumns, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // make sure database exists and is valid
    OpenDatabase (); // did not throw => opened successfully
}

//Testing integration of software name and version input
FIXTURE_TEST_CASE ( SoftwareName, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    const string SoftwareName = "softwarename";
    const string Version = "2.1.1";
    m_source . SoftwareNameEvent ( SoftwareName, Version );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate metadata
    OpenDatabase ();
    REQUIRE_EQ ( SoftwareName,  GetDbMetadataAttr ( m_db, "SOFTWARE/formatter", "name" ) );
    REQUIRE_EQ ( Version,       GetDbMetadataAttr ( m_db, "SOFTWARE/formatter", "vers" ) );

    // extract the program name from path the same way it's done in ncbi-vdb/libs/kapp/loader-meta.c:KLoaderMeta_Write
    {
        const char* tool_name = strrchr(argv0.c_str(), '/');
        const char* r = strrchr(argv0.c_str(), '\\');
        if( tool_name != NULL && r != NULL && tool_name < r ) {
            tool_name = r;
        }
        if( tool_name++ == NULL) {
            tool_name = argv0.c_str();
        }

        REQUIRE_EQ ( string ( tool_name ), GetDbMetadataAttr ( m_db, "SOFTWARE/loader", "name" ) );
    }

    REQUIRE_EQ ( string ( __DATE__), GetDbMetadataAttr ( m_db, "SOFTWARE/loader", "date" ) );
    {
        char buf[265];
        string_printf ( buf, sizeof buf, NULL, "%V", GetKAppVersion() ); // same format as in ncbi-vdb/libs/kapp/loader-meta.c:MakeVersion()
        REQUIRE_EQ ( string ( buf ), GetDbMetadataAttr ( m_db, "SOFTWARE/loader", "vers" ) );
    }
}

FIXTURE_TEST_CASE ( SoftwareName_BadVersion, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    const string SoftwareName = "softwarename";
    const string Version = "2.1..1"; // improperly formatted
    m_source . SoftwareNameEvent ( SoftwareName, Version );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcDatabase, rcCreating, rcMessage, rcBadVersion ) ) );
}

FIXTURE_TEST_CASE ( DBAddDatabase, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database database0 #1 { table table1 #1 TABLE1; } ;"
                    "database root_database #1 { database database0 #1 SUBDB; } ;"
                 )
    );

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddDatabaseEvent ( 1, 0, "SUBDB", "subdb", kcmCreate );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate database
    OpenDatabase ();
    VDatabase * subDb;
    REQUIRE_RC ( VDatabaseOpenDBUpdate ( m_db, & subDb, "subdb" ) );
    REQUIRE_RC ( VDatabaseRelease ( subDb ) );
}

FIXTURE_TEST_CASE ( DBAddSubDatabase, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database database0 #1 { table table1 #1 TABLE1; } ;"
                    "database database1 #1 { database database0 #1 SUBSUBDB; } ;"
                    "database root_database #1 { database database1 #1 SUBDB; } ;"
                 )
    );

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddDatabaseEvent ( 1, 0, "SUBDB", "subdb", kcmCreate );
    m_source . DBAddDatabaseEvent ( 2, 1, "SUBSUBDB", "subsubdb", kcmCreate );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate database
    OpenDatabase ();
    VDatabase * subDb;
    REQUIRE_RC ( VDatabaseOpenDBUpdate ( m_db, & subDb, "subdb" ) );
    VDatabase * subSubDb;
    REQUIRE_RC ( VDatabaseOpenDBUpdate ( subDb, & subSubDb, "subsubdb" ) );
    REQUIRE_RC ( VDatabaseRelease ( subSubDb ) );
    REQUIRE_RC ( VDatabaseRelease ( subDb ) );
}

FIXTURE_TEST_CASE ( DBAddTable, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database root_database #1 { table table1 #1 TABLE1; } ;"
                 )
    );

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddTableEvent ( 1, 0, "TABLE1", "tbl", kcmCreate );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate database
    OpenDatabase ();
    const VTable *tbl;
    REQUIRE_RC ( VDatabaseOpenTableRead ( m_db, & tbl, "tbl" ) );
    REQUIRE_RC ( VTableRelease ( tbl ) );
}

FIXTURE_TEST_CASE ( DBAddTableToSubDb, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database database0 #1 { table table1 #1 TABLE1; } ;"
                    "database root_database #1 { database database0 #1 SUBDB; } ;"
                 )
    );

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddDatabaseEvent ( 2, 0, "SUBDB", "subdb", kcmCreate );
    m_source . DBAddTableEvent ( 1, 2, "TABLE1", "tbl", kcmCreate );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate database
    OpenDatabase ();
    {
        VDatabase * subDb;
        REQUIRE_RC ( VDatabaseOpenDBUpdate ( m_db, & subDb, "subdb" ) );
        const VTable *tbl;
        REQUIRE_RC ( VDatabaseOpenTableRead ( subDb, & tbl, "tbl" ) );
        REQUIRE_RC ( VTableRelease ( tbl ) );
        REQUIRE_RC ( VDatabaseRelease ( subDb ) );
    }
}

FIXTURE_TEST_CASE ( DBMetadataNode, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    const string key = "dbmetadatanode";
    const string value = "1a2b3c4d";
    m_source . OpenStreamEvent();
    m_source . DBMetadataNodeEvent ( 0, key, value );
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    OpenDatabase ();
    REQUIRE_EQ ( value, GetDbMetadata( m_db, key ) );
}

FIXTURE_TEST_CASE ( DBMetadataNodeAttr, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    const string key = "dbmetadatanode";
    const string attr = "attr";
    const string value = "1a2b3c4d";
    m_source . OpenStreamEvent();
    m_source . DBMetadataNodeEvent ( 0, key, value );
    m_source . DBMetadataNodeAttrEvent ( 0, key, attr, value );
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    OpenDatabase ();
    REQUIRE_EQ ( value, GetDbMetadataAttr( m_db, key, attr ) );
}

FIXTURE_TEST_CASE ( SubDBMetadataNode, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database database0 #1 { table table1 #1 TABLE1; } ;"
                    "database root_database #1 { database database0 #1 SUBDB; } ;"
                 )
    );
    const uint32_t subDbId = 2;
    const string key = "subdbmetadatanode";
    const string value = "1a2b3c4dsub";

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddDatabaseEvent ( subDbId, 0, "SUBDB", "subdb", kcmCreate );
    m_source . DBAddTableEvent ( 1, subDbId, "TABLE1", "tbl", kcmCreate );
    m_source . OpenStreamEvent();
    m_source . DBMetadataNodeEvent ( subDbId, key, value );
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    OpenDatabase ();
    {
        VDatabase * subDb;
        REQUIRE_RC ( VDatabaseOpenDBUpdate ( m_db, & subDb, "subdb" ) );
        REQUIRE_EQ ( value, GetDbMetadata( subDb, key ) );
        REQUIRE_RC ( VDatabaseRelease ( subDb ) );
    }
}

FIXTURE_TEST_CASE ( TblMetadataNode, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database root_database #1 { table table1 #1 TABLE1; } ;"
                 )
    );

    const char* tblName = "tbl";
    const uint32_t tblId = 2;
    const string key = "tblmetadatanode";
    const string value = "tbl1a2b3c4d";

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddTableEvent ( tblId, 0, "TABLE1", tblName, kcmCreate );

    m_source . OpenStreamEvent();
    m_source . TblMetadataNodeEvent ( tblId, key, value);
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate database
    OpenDatabase ();
    {
        const VTable *tbl;
        REQUIRE_RC ( VDatabaseOpenTableRead ( m_db, & tbl, tblName ) );

        {
            const KMetadata *meta;
            REQUIRE_RC ( VTableOpenMetadataRead ( tbl, &meta ) );
            REQUIRE_EQ ( value, GetMetadata ( meta,  key) );
            REQUIRE_RC ( KMetadataRelease ( meta ) );
        }
        REQUIRE_RC ( VTableRelease ( tbl ) );
    }
}

FIXTURE_TEST_CASE ( TblMetadataNodeAttr, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database root_database #1 { table table1 #1 TABLE1; } ;"
                 )
    );

    const char* tblName = "tbl";
    const uint32_t tblId = 2;
    const string key = "tblmetadatanode";
    const string attr = "tblattr";
    const string value = "tbl1a2b3c4d";

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddTableEvent ( tblId, 0, "TABLE1", tblName, kcmCreate );

    m_source . OpenStreamEvent();
    m_source . TblMetadataNodeEvent ( tblId, key, value);
    m_source . TblMetadataNodeAttrEvent ( tblId, key, attr, value);
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate database
    OpenDatabase ();
    {
        const VTable *tbl;
        REQUIRE_RC ( VDatabaseOpenTableRead ( m_db, & tbl, tblName ) );
        REQUIRE_EQ ( value, GetTblMetadataAttr ( tbl, key, attr ) );
        REQUIRE_RC ( VTableRelease ( tbl ) );
    }
}

FIXTURE_TEST_CASE ( ColMetadataNode, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database root_database #1 { table table1 #1 TABLE1; } ;"
                 )
    );

    const char* tblName = "tbl";
    const uint32_t tblId = 2;
    const char* colName = "column1";
    const uint32_t colId = 5;
    const string NodeName   = "colmetadatanode";
    const string NodeValue  = "1a2b3c4d";

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddTableEvent ( tblId, 0, "TABLE1", tblName, kcmCreate | kcmMD5 );
    m_source . NewColumnEvent ( colId, tblId, colName, 8 );

    m_source . OpenStreamEvent();
    m_source . ColMetadataNodeEvent ( colId, NodeName, NodeValue );
    // need at least 2 rows with different value in order for the column to become physical, otherwise column's metadata will not be stored
    m_source . CellDataEvent( colId, string("blah1") );
    m_source . NextRowEvent ( tblId );
    m_source . CellDataEvent( colId, string("brh2") );
    m_source . NextRowEvent ( tblId );
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate metadata
    OpenDatabase ();
    {
        const VTable *tbl;
        REQUIRE_RC ( VDatabaseOpenTableRead ( m_db, & tbl, tblName ) );

        {
            const KTable* ktbl;
            REQUIRE_RC ( VTableOpenKTableRead ( tbl, & ktbl ) );
            {
                const KColumn* col;
                REQUIRE_RC ( KTableOpenColumnRead ( ktbl, & col, colName ) );
                {
                    const KMetadata *meta;
                    REQUIRE_RC ( KColumnOpenMetadataRead ( col, &meta ) );

                    REQUIRE_EQ ( NodeValue, GetMetadata ( meta, NodeName ) );

                    REQUIRE_RC ( KMetadataRelease ( meta ) );
                }
                REQUIRE_RC ( KColumnRelease ( col ) );
            }
            REQUIRE_RC ( KTableRelease ( ktbl ) );
        }
        REQUIRE_RC ( VTableRelease ( tbl ) );
    }
}

FIXTURE_TEST_CASE ( ColMetadataNodeAttr, GeneralLoaderFixture )
{
    string schemaFile = ScratchDir + GetName() + ".vschema";
    CreateFile ( schemaFile,
                 string (
                    "table table1 #1.0.0 { column ascii column1; };"
                    "database root_database #1 { table table1 #1 TABLE1; } ;"
                 )
    );

    const char* tblName = "tbl";
    const uint32_t tblId = 2;
    const char* colName = "column1";
    const uint32_t colId = 5;
    const string NodeName   = "colmetadatanode";
    const string NodeValue  = "1a2b3c4d";
    const string AttrName   = "colattr";
    const string AttrValue   = "colattr";

    SetUpStream ( GetName(), schemaFile, "root_database");

    m_source . DBAddTableEvent ( tblId, 0, "TABLE1", tblName, kcmCreate | kcmMD5 );
    m_source . NewColumnEvent ( colId, tblId, colName, 8 );

    m_source . OpenStreamEvent();
    m_source . ColMetadataNodeEvent ( colId, NodeName, NodeValue );
    m_source . ColMetadataNodeAttrEvent ( colId, NodeName, AttrName, AttrValue );
    // need at least 2 rows with different value in order for the column to become physical, otherwise column's metadata will not be stored
    m_source . CellDataEvent( colId, string("blah1") );
    m_source . NextRowEvent ( tblId );
    m_source . CellDataEvent( colId, string("brh2") );
    m_source . NextRowEvent ( tblId );
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    // validate metadata
    OpenDatabase ();
    {
        const VTable *tbl;
        REQUIRE_RC ( VDatabaseOpenTableRead ( m_db, & tbl, tblName ) );

        {
            const KTable* ktbl;
            REQUIRE_RC ( VTableOpenKTableRead ( tbl, & ktbl ) );
            {
                const KColumn* col;
                REQUIRE_RC ( KTableOpenColumnRead ( ktbl, & col, colName ) );
                {
                    const KMetadata *meta;
                    REQUIRE_RC ( KColumnOpenMetadataRead ( col, &meta ) );

                    REQUIRE_EQ ( AttrValue, GetMetadataAttr ( meta, NodeName, AttrName ) );

                    REQUIRE_RC ( KMetadataRelease ( meta ) );
                }
                REQUIRE_RC ( KColumnRelease ( col ) );
            }
            REQUIRE_RC ( KTableRelease ( ktbl ) );
        }
        REQUIRE_RC ( VTableRelease ( tbl ) );
    }
}

FIXTURE_TEST_CASE ( NoData, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 2, DefaultTable ); // ids do not have to be consecutive
    m_source . NewColumnEvent ( 222, 2, DefaultColumn, 8 );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_THROW ( OpenCursor( DefaultTable, DefaultColumn ) );
}

FIXTURE_TEST_CASE ( Chunk_BadColumnId, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    m_source . CellDataEvent( /*bad*/2, string("blah") );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( WriteNoCommit, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value = "a single character string cell";
    m_source . CellDataEvent( DefaultColumnId, value );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_THROW ( OpenCursor( DefaultTable, DefaultColumn ) );
}

FIXTURE_TEST_CASE ( CommitBadTableId, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    m_source . NextRowEvent ( /*bad*/2 );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcTable, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( OneColumnOneCellOneChunk, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value = "a single character string cell";
    m_source . CellDataEvent( DefaultColumnId, value );
    m_source . NextRowEvent ( DefaultTableId );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
}

FIXTURE_TEST_CASE ( OneColumnOneCellOneChunk_Long, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value ( GeneralLoader :: MaxPackedString + 1, 'x' );
    m_source . CellDataEvent( DefaultColumnId, value );
    m_source . NextRowEvent ( DefaultTableId );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
}

FIXTURE_TEST_CASE ( OneColumnOneCellManyChunks, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value1 = "first";
    m_source . CellDataEvent( DefaultColumnId, value1 );
    string value2 = "second!";
    m_source . CellDataEvent( DefaultColumnId, value2 );
    string value3 = "third!!!";
    m_source . CellDataEvent( DefaultColumnId, value3 );
    m_source . NextRowEvent ( DefaultTableId );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value1 + value2 + value3, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
}

FIXTURE_TEST_CASE ( IntegerCompression_MinimumCompression, GeneralLoaderFixture )
{
    if ( ! SetUpForIntegerCompression ( GetName() ) )
        return;

    // no compression ( adds 1 byte per value )
    const uint16_t u16value = 0x3456;
    const uint32_t u32value = 0x789abcde;
    const uint64_t u64value = 0x0F0F0F0F0F0F0F0FLL;

    m_source . OpenStreamEvent();

    uint8_t buf[128];

    int bytes = encode_uint16 ( u16value, buf, buf + sizeof buf );
    REQUIRE_EQ ( 3, bytes );
    m_source . CellDataEventRaw ( Column16Id, 1, buf, bytes );

    bytes = encode_uint32 ( u32value, buf, buf + sizeof buf );
    REQUIRE_EQ ( 5, bytes );
    m_source . CellDataEventRaw ( Column32Id, 1, buf, bytes );

    bytes = encode_uint64 ( u64value, buf, buf + sizeof buf );
    REQUIRE_EQ ( 9, bytes );
    m_source . CellDataEventRaw ( Column64Id, 1, buf, bytes );

    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_EQ ( u16value, GetValue<uint16_t> ( "TABLE1", "column16", 1 ) );
    REQUIRE_EQ ( u32value, GetValue<uint32_t> ( "TABLE1", "column32", 1 ) );
    REQUIRE_EQ ( u64value, GetValue<uint64_t> ( "TABLE1", "column64", 1 ) );
}

FIXTURE_TEST_CASE ( IntegerCompression_MaximumCompression, GeneralLoaderFixture )
{
    if ( ! SetUpForIntegerCompression ( GetName() ) )
        return;

    // induce maximum compression ( 1 byte per value <= 0x7F )
    const uint16_t u16value = 0;
    const uint32_t u32value = 2;
    const uint64_t u64value = 0x7F;

    m_source . OpenStreamEvent();

    uint8_t buf[128];

    int bytes = encode_uint16 ( u16value, buf, buf + sizeof buf );
    REQUIRE_EQ ( 1, bytes );
    m_source . CellDataEventRaw ( Column16Id, 1, buf, bytes );

    bytes = encode_uint32 ( u32value, buf, buf + sizeof buf );
    REQUIRE_EQ ( 1, bytes );
    m_source . CellDataEventRaw ( Column32Id, 1, buf, bytes );

    bytes = encode_uint64 ( u64value, buf, buf + sizeof buf );
    REQUIRE_EQ ( 1, bytes );
    m_source . CellDataEventRaw ( Column64Id, 1, buf, bytes );

    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_EQ ( u16value, GetValue<uint16_t> ( "TABLE1", "column16", 1 ) );
    REQUIRE_EQ ( u32value, GetValue<uint32_t> ( "TABLE1", "column32", 1 ) );
    REQUIRE_EQ ( u64value, GetValue<uint64_t> ( "TABLE1", "column64", 1 ) );
}

FIXTURE_TEST_CASE ( IntegerCompression_MultipleValues, GeneralLoaderFixture )
{
    if ( ! SetUpForIntegerCompression ( GetName() ) )
        return;

    // induce maximum compression ( 1 byte per value <= 0x7F )
    const uint16_t u16value1 = 0x0001;              const uint16_t u16value2 = 0x0002;
    const uint32_t u32value1 = 0x00000003;          const uint32_t u32value2 = 0x00000004;
    const uint64_t u64value1 = 0x0000000000000006;  const uint64_t u64value2 = 0x0000000000000007;

    m_source . OpenStreamEvent();

    uint8_t buf[128];

    {
        int bytesTotal = 0;
        int bytes = encode_uint16 ( u16value1, buf, buf + sizeof buf );
        bytesTotal += bytes;
        bytes = encode_uint16 ( u16value2, buf + bytesTotal, buf + sizeof buf );
        bytesTotal += bytes;
        REQUIRE_EQ ( 2, bytesTotal );
        m_source . CellDataEventRaw ( Column16Id, 2, buf, bytesTotal );
    }

    {
        int bytesTotal = 0;
        int bytes = encode_uint32 ( u32value1, buf, buf + sizeof buf );
        bytesTotal += bytes;
        bytes = encode_uint32 ( u32value2, buf + bytesTotal, buf + sizeof buf );
        bytesTotal += bytes;
        REQUIRE_EQ ( 2, bytesTotal );
        m_source . CellDataEventRaw ( Column32Id, 2, buf, bytesTotal );
    }

    {
        int bytesTotal = 0;
        int bytes = encode_uint64 ( u64value1, buf, buf + sizeof buf );
        bytesTotal += bytes;
        bytes = encode_uint64 ( u64value2, buf + bytesTotal, buf + sizeof buf );
        bytesTotal += bytes;
        REQUIRE_EQ ( 2, bytesTotal );
        m_source . CellDataEventRaw ( Column64Id, 2, buf, bytesTotal );
    }

    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_EQ ( u16value1, GetValueWithIndex<uint16_t> ( "TABLE1", "column16", 1, 2, 0 ) );
    REQUIRE_EQ ( u16value2, GetValueWithIndex<uint16_t> ( "TABLE1", "column16", 1, 2, 1 ) );
    REQUIRE_EQ ( u32value1, GetValueWithIndex<uint32_t> ( "TABLE1", "column32", 1, 2, 0 ) );
    REQUIRE_EQ ( u32value2, GetValueWithIndex<uint32_t> ( "TABLE1", "column32", 1, 2, 1 ) );
    REQUIRE_EQ ( u64value1, GetValueWithIndex<uint64_t> ( "TABLE1", "column64", 1, 2, 0 ) );
    REQUIRE_EQ ( u64value2, GetValueWithIndex<uint64_t> ( "TABLE1", "column64", 1, 2, 1 ) );
}

// default values

FIXTURE_TEST_CASE ( OneColumnDefaultNoWrite, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value = "this be my default";
    m_source . CellDefaultEvent( DefaultColumnId, value );
    // no WriteEvent
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
}

FIXTURE_TEST_CASE ( OneColumnDefaultNoWrite_Long, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value ( GeneralLoader :: MaxPackedString + 1, 'x' );
    m_source . CellDefaultEvent( DefaultColumnId, value );
    // no WriteEvent
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
}

FIXTURE_TEST_CASE ( MoveAhead, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value = "this be my default";
    m_source . CellDefaultEvent( DefaultColumnId, value );
    m_source . MoveAheadEvent ( DefaultTableId, 3 );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 2 ) );
    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 3 ) );
    REQUIRE_THROW ( GetValue<string> ( DefaultTable, DefaultColumn, 4 ) );
}

FIXTURE_TEST_CASE ( OneColumnDefaultOverwite, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string valueDflt = "this be my default";
    m_source . CellDefaultEvent( DefaultColumnId, valueDflt );
    string value = "not the default";
    m_source . CellDataEvent( DefaultColumnId, value );
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
}

FIXTURE_TEST_CASE ( OneColumnChangeDefault, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value1 = "this be my first default";
    m_source . CellDefaultEvent( DefaultColumnId, value1 );
    m_source . NextRowEvent ( DefaultTableId  );

    string value2 = "and this be my second default";
    m_source . CellDefaultEvent( DefaultColumnId, value2 );
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value1, GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
    REQUIRE_EQ ( value2, GetValue<string> ( DefaultTable, DefaultColumn, 2 ) );
}

FIXTURE_TEST_CASE ( OneColumnDataAndDefaultsMixed, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    string value1 = "first value";
    m_source . CellDataEvent( DefaultColumnId, value1 );
    m_source . NextRowEvent ( DefaultTableId  );

    string default1 = "first default";
    m_source . CellDefaultEvent( DefaultColumnId, default1 );
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . NextRowEvent ( DefaultTableId  );

    string value2 = "second value";
    m_source . CellDefaultEvent( DefaultColumnId, value2 );
    m_source . NextRowEvent ( DefaultTableId  );

    string default2 = "second default";
    m_source . CellDefaultEvent( DefaultColumnId, default2 );
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value1,    GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
    REQUIRE_EQ ( default1,  GetValue<string> ( DefaultTable, DefaultColumn, 2 ) );
    REQUIRE_EQ ( default1,  GetValue<string> ( DefaultTable, DefaultColumn, 3 ) );
    REQUIRE_EQ ( value2,    GetValue<string> ( DefaultTable, DefaultColumn, 4 ) );
    REQUIRE_EQ ( default2,  GetValue<string> ( DefaultTable, DefaultColumn, 5 ) );
}

FIXTURE_TEST_CASE ( TwoColumnsFullRow, GeneralLoaderFixture )
{
    SetUpStream_OneTable ( GetName() );

    m_source . NewColumnEvent ( 1, DefaultTableId, DefaultColumn, 8 );
    m_source . NewColumnEvent ( 2, DefaultTableId, U32Column, 32 );
    m_source . OpenStreamEvent();

    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    uint32_t value2 = 12345;
    m_source . CellDataEvent( 2, value2 );
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value1,    GetValue<string>    ( DefaultTable, DefaultColumn, 1 ) );
    REQUIRE_EQ ( value2,    GetValue<uint32_t>  ( DefaultTable, U32Column, 1 ) );
}

FIXTURE_TEST_CASE ( TwoColumnsIncompleteRow, GeneralLoaderFixture )
{
    SetUpStream_OneTable ( GetName() );

    m_source . NewColumnEvent ( 1, DefaultTableId, DefaultColumn, 8 );
    m_source . NewColumnEvent ( 2, DefaultTableId, U32Column, 32 );
    m_source . OpenStreamEvent();

    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    // no CellData for column 2
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcColumn, rcClosing, rcRow, rcIncomplete) ) );
}

FIXTURE_TEST_CASE ( TwoColumnsPartialRowWithDefaults, GeneralLoaderFixture )
{
    SetUpStream_OneTable ( GetName() );

    m_source . NewColumnEvent ( 1, DefaultTableId, DefaultColumn, 8 );
    m_source . NewColumnEvent ( 2, DefaultTableId, U32Column, 32 );
    m_source . OpenStreamEvent();

    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    uint32_t value2 = 12345;
    m_source . CellDefaultEvent( 2, value2 );
    // no Data Event for column2
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value1,    GetValue<string>    ( DefaultTable, DefaultColumn, 1 ) );
    REQUIRE_EQ ( value2,    GetValue<uint32_t>  ( DefaultTable, U32Column, 1 ) );
}

FIXTURE_TEST_CASE ( TwoColumnsPartialRowWithDefaultsAndOverride, GeneralLoaderFixture )
{
    SetUpStream_OneTable ( GetName() );

    m_source . NewColumnEvent ( 1, DefaultTableId, DefaultColumn, 8 );
    m_source . NewColumnEvent ( 2, DefaultTableId, U32Column, 32 );
    m_source . NewColumnEvent ( 3, DefaultTableId, BoolColumn, 8 );
    m_source . OpenStreamEvent();

    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    uint32_t value2 = 12345;
    m_source . CellDefaultEvent( 2, value2 );
    m_source . CellDefaultEvent( 3, false );
    bool value3 = true;
    m_source . CellDataEvent( 3, true ); // override default
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( value1,    GetValue<string>    ( DefaultTable, DefaultColumn, 1 ) );       // explicit
    REQUIRE_EQ ( value2,    GetValue<uint32_t>  ( DefaultTable, U32Column, 1 ) );    // default
    REQUIRE_EQ ( value3,    GetValue<bool>      ( DefaultTable, BoolColumn, 1 ) );   // not the default
}

FIXTURE_TEST_CASE ( EmptyDefault_String, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );

    m_source . OpenStreamEvent();

    m_source . CellEmptyDefaultEvent( 1 );
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( string(), GetValue<string> ( DefaultTable, DefaultColumn, 1 ) );
}

FIXTURE_TEST_CASE ( EmptyDefault_Int, GeneralLoaderFixture )
{
    SetUpStream_OneTable ( GetName() );

    m_source . NewColumnEvent ( 1, DefaultTableId, U32Column, 32 );
    m_source . OpenStreamEvent();

    m_source . CellEmptyDefaultEvent( 1 );
    m_source . NextRowEvent ( DefaultTableId  );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE ( IsNullValue<uint32_t> ( DefaultTable, U32Column, 1 ) );
}

FIXTURE_TEST_CASE ( MultipleTables_Multiple_Columns_MultipleRows, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );

    m_source . NewTableEvent ( 100, DefaultTable );
    m_source . NewColumnEvent ( 1, 100, DefaultColumn, 8 );
    m_source . NewColumnEvent ( 2, 100, U32Column, 32 );

    m_source . NewTableEvent ( 200, Table2 );
    m_source . NewColumnEvent ( 3, 200, I64Column, 64 );
    m_source . NewColumnEvent ( 4, 200, U8Column, 8 );

    m_source . OpenStreamEvent();

        string t1c1v1 = "t1c1v1";
        m_source . CellDataEvent( 1, t1c1v1 );
        uint32_t t1c2v1 = 121;
        m_source . CellDataEvent( 2, t1c2v1 );
    m_source . NextRowEvent ( 100 );

        int64_t t2c1v1 = 211;
        m_source . CellDataEvent( 3, t2c1v1 );
        uint8_t t2c2v1 = 221;
        m_source . CellDataEvent( 4, t2c2v1 );
    m_source . NextRowEvent ( 200 );

        string t1c1v2 = "t1c1v2";
        m_source . CellDataEvent( 1, t1c1v2 );
        uint32_t t1c2v2 = 122;
        m_source . CellDataEvent( 2, t1c2v2 );
    m_source . NextRowEvent ( 100 );

        int64_t t2c1v2 = 212;
        m_source . CellDataEvent( 3, t2c1v2 );
        uint8_t t2c2v2 = 222;
        m_source . CellDataEvent( 4, t2c2v2 );
    m_source . NextRowEvent ( 200 );

    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );

    REQUIRE_EQ ( t1c1v1,    GetValue<string>    ( DefaultTable, DefaultColumn, 1 ) );
    REQUIRE_EQ ( t1c2v1,    GetValue<uint32_t>  ( DefaultTable, U32Column, 1 ) );
    REQUIRE_EQ ( t2c1v1,    GetValue<int64_t>   ( Table2, I64Column, 1 ) );
    REQUIRE_EQ ( t2c2v1,    GetValue<uint8_t>   ( Table2, U8Column, 1 ) );

    REQUIRE_EQ ( t1c1v2,    GetValue<string>    ( DefaultTable, DefaultColumn, 2 ) );
    REQUIRE_EQ ( t1c2v2,    GetValue<uint32_t>  ( DefaultTable, U32Column, 2 ) );
    REQUIRE_EQ ( t2c1v2,    GetValue<int64_t>   ( Table2, I64Column, 2 ) );
    REQUIRE_EQ ( t2c2v2,    GetValue<uint8_t>   ( Table2, U8Column, 2 ) );
}

FIXTURE_TEST_CASE ( AdditionalSchemaIncludePaths_Single, GeneralLoaderFixture )
{
    string schemaPath = "schema";
    string includeName = string ( GetName() ) + ".inc.vschema";
    string schemaIncludeFile = schemaPath + "/" + includeName;
    CreateFile ( schemaIncludeFile, "table table1 #1.0.0 { column ascii column1; };" );
    string schemaFile = string ( GetName() ) + ".vschema";
    CreateFile ( schemaFile, string ( "include '" ) + includeName + "'; database database1 #1 { table table1 #1 TABLE1; } ;" );

    SetUpStream ( GetName(), schemaFile, "database1" );

    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( DefaultTableId, table1 );
    m_source . NewColumnEvent ( DefaultColumnId, DefaultTableId, column1, 8 );
    m_source . OpenStreamEvent();

    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( DefaultColumnId, t1c1v1 );
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        gl -> AddSchemaIncludePath ( schemaPath );
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_EQ ( t1c1v1, GetValue<string> ( table1, column1, 1 ) );

    remove ( schemaIncludeFile . c_str() );
    remove ( schemaFile . c_str() );
}

FIXTURE_TEST_CASE ( AdditionalSchemaIncludePaths_Multiple, GeneralLoaderFixture )
{
    string schemaPath = "schema";
    string includeName = string ( GetName() ) + ".inc.vschema";
    string schemaIncludeFile = schemaPath + "/" + includeName;
    CreateFile ( schemaIncludeFile, "table table1 #1.0.0 { column ascii column1; };" );
    string schemaFile = string ( GetName() ) + ".vschema";
    CreateFile ( schemaFile, string ( "include '" ) + includeName + "'; database database1 #1 { table table1 #1 TABLE1; } ;" );

    SetUpStream ( GetName(), schemaFile, "database1" );

    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( DefaultTableId, table1 );
    m_source . NewColumnEvent ( DefaultColumnId, DefaultTableId, column1, 8 );
    m_source . OpenStreamEvent();

    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( DefaultColumnId, t1c1v1 );
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        gl -> AddSchemaIncludePath ( "path1" );
        gl -> AddSchemaIncludePath ( string ( "path2:" ) + schemaPath + ":path3" );
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_EQ ( t1c1v1, GetValue<string> ( table1, column1, 1 ) );

    remove ( schemaIncludeFile . c_str() );
    remove ( schemaFile . c_str() );
}

FIXTURE_TEST_CASE ( AdditionalSchemaFiles_Single, GeneralLoaderFixture )
{
    string schemaPath = "schema";
    string schemaFile = schemaPath + "/" + GetName() + ".vschema";
    CreateFile ( schemaFile, "table table1 #1.0.0 { column ascii column1; }; database database1 #1 { table table1 #1 TABLE1; };" );

    // here we specify a schema that does not exist but it's OK as long as we add a good schema later
    SetUpStream ( GetName(), "does not exist", "database1" );

    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( DefaultTableId, table1 );
    m_source . NewColumnEvent ( DefaultColumnId, DefaultTableId, column1, 8 );
    m_source . OpenStreamEvent();

    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( DefaultColumnId, t1c1v1 );
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        gl -> AddSchemaFile ( schemaFile );
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_EQ ( t1c1v1, GetValue<string> ( table1, column1, 1 ) );

    remove ( schemaFile . c_str() );
}

FIXTURE_TEST_CASE ( AdditionalSchemaFiles_Multiple, GeneralLoaderFixture )
{
    string schemaPath = "schema";
    string schemaFile1 = schemaPath + "/" + GetName() + "1.vschema";
    CreateFile ( schemaFile1, "table table1 #1.0.0 { column ascii column1; };" );
    string schemaFile2 = schemaPath + "/" + GetName() + "2.vschema";
    CreateFile ( schemaFile2, "database database1 #1 { table table1 #1 TABLE1; };" ); // this file uses table1 defined in schemaFile1

    string dbFile = string ( GetName() ) + ".db";

    // here we specify a schema that does not exist but it's OK as long as we add a good schema later
    m_source . SchemaEvent ( "does not exist", "database1" );
    m_source . DatabaseEvent ( dbFile );

    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( DefaultTableId, table1 );
    m_source . NewColumnEvent ( DefaultColumnId, DefaultTableId, column1, 8 );
    m_source . OpenStreamEvent();

    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( DefaultColumnId, t1c1v1 );
    m_source . NextRowEvent ( DefaultTableId  );
    m_source . CloseStreamEvent();

    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        gl -> AddSchemaFile ( schemaFile1 + ":garbage.vschema:" + schemaFile2 ); // some are good
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_EQ ( t1c1v1, GetValue<string> ( table1, column1, 1 ) );

    remove ( schemaFile1 . c_str() );
    remove ( schemaFile2 . c_str() );
}

FIXTURE_TEST_CASE ( ErrorMessage, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );
    m_source . OpenStreamEvent();
    m_source . ErrorMessageEvent ( "error message" );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), RC ( rcExe, rcFile, rcReading, rcError, rcExists ) ) );
}

FIXTURE_TEST_CASE ( ErrorMessage_Long, GeneralLoaderFixture )
{
    OpenStream_OneTableOneColumn ( GetName() );
    m_source . OpenStreamEvent();
    m_source . ErrorMessageEvent ( string ( 257, 'x' ) );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), RC ( rcExe, rcFile, rcReading, rcError, rcExists ) ) );
}

FIXTURE_TEST_CASE ( LogMessage, GeneralLoaderFixture )
{
//FullLog();
// uncomment the line above to see the message on stdout, eg:
// 2016-01-05T18:45:01 test-general-loader.1 info: general-loader: log from front-end-app: "some log message"

    OpenStream_OneTableOneColumn ( GetName() );
    const string SoftwareName = "front-end-app";
    const string Version = "2.1.1";
    m_source . SoftwareNameEvent ( SoftwareName, Version );
    m_source . OpenStreamEvent();
    m_source . LogMessageEvent ( "some log message" );
    m_source . CloseStreamEvent();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
}


FIXTURE_TEST_CASE ( ProgressMessage, GeneralLoaderFixture )
{
    // timestamp
    time_t timestamp = time ( NULL );

    OpenStream_OneTableOneColumn ( GetName() );
    m_source . OpenStreamEvent ();
    m_source . ProgMessageEvent ( 123, "progress message", timestamp, 2, 45 );
    m_source . CloseStreamEvent ();

    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
}


FIXTURE_TEST_CASE ( TargetOverride, GeneralLoaderFixture )
{
    SetUpStream ( GetName() );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();

    string newTarget = string ( GetName() ) + "_override";
    {
        GeneralLoader* gl = MakeLoader ( m_source . MakeSource () );
        gl -> SetTargetOverride ( newTarget );
        REQUIRE ( RunLoader ( *gl, 0 ) );
        delete gl;
    } // make sure loader is destroyed (= db closed) before we reopen the database for verification

    REQUIRE_THROW ( OpenDatabase() );       // the db from the instruction stream was not created
    OpenDatabase ( newTarget . c_str() );   // did not throw => overridden target db opened successfully

    // clean up
    CloseDatabase();
    KDirectoryRemove ( m_wd, true, newTarget . c_str() );
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>

int main ( int argc, char *argv [] )
{
//    TestEnv::verbosity = LogLevel::e_all;
    KConfigDisableUserSettings();

    ClearScratchDir();

    GeneralLoaderFixture :: argv0 = argv[0];

    TestSource::packed = false;
    cerr << "Unpacked protocol: ";
    int rc = GeneralLoaderTestSuite(argc, argv);
    if ( rc == 0 )
    {
        ClearScratchDir();

        TestSource::packed = true;
        cerr << "Packed protocol: ";
        rc = GeneralLoaderTestSuite(argc, argv);
    }

    ClearScratchDir();

    return rc;
}

}

