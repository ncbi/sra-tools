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

#include <ktst/unit_test.hpp> 

#include <sysalloc.h>

#include <klib/printf.h>
#include <klib/debug.h>

#include <kns/adapt.h>

#include <kfs/nullfile.h>
#include <kfs/file.h>

#include <cstring>
#include <stdexcept> 
#include <map>
#include <fstream>
#include <cstdio>

#include "testsource.hpp"

#include "../../tools/general-loader/general-loader.cpp"

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(GeneralLoaderTestSuite);

class GeneralLoaderFixture
{
public:
    GeneralLoaderFixture()
    :   m_db ( 0 ),
        m_cursor ( 0 ),
        m_keepDatabase ( false )
    {
    }
    ~GeneralLoaderFixture()
    {
        RemoveDatabase();
    }
    
    GeneralLoader* MakeLoader ( const struct KFile * p_input )
    {
        struct KStream * inStream;
        if ( KStreamFromKFilePair ( & inStream, p_input, 0 ) != 0 )
            throw logic_error("GeneralLoaderFixture::Run KStreamFromKFilePair failed");
            
        GeneralLoader* ret = new GeneralLoader ( * inStream );
        
        if ( KStreamRelease ( inStream)  != 0 )
            throw logic_error("GeneralLoaderFixture::MakeLoader: KStreamRelease failed");
        if ( KFileRelease ( p_input)  != 0 )
            throw logic_error("GeneralLoaderFixture::Run: KFileRelease failed");
            
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
        struct KStream * inStream;
        if ( KStreamFromKFilePair ( & inStream, p_input, 0 ) != 0 )
            throw logic_error("GeneralLoaderFixture::Run KStreamFromKFilePair failed");
            
        GeneralLoader gl ( * inStream );
        rc_t rc = gl.Run();
        bool ret;
        if ( rc == p_rc )
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
        
        if ( KStreamRelease ( inStream)  != 0 )
            throw logic_error("GeneralLoaderFixture::Run: KStreamRelease failed");
            
        if ( KFileRelease ( p_input)  != 0 )
            throw logic_error("GeneralLoaderFixture::Run: KFileRelease failed");
            
        return ret;
    }
    
    bool DatabaseExists ()
    {
        if ( m_source . GetDatabaseName() . empty ()  )
        {
            return false;
        }
        KDirectory* wd;
        KDirectoryNativeDir ( & wd );
        bool ret = KDirectoryPathType_v1 ( wd, m_source . GetDatabaseName() . c_str() ) != kptNotFound;
        KDirectoryRelease ( wd );
        return ret;
    }
    
    void RemoveDatabase()
    {
        CloseDatabase();
        if ( ! m_source . GetDatabaseName() . empty () && ! m_keepDatabase )
        {
            KDirectory* wd;
            KDirectoryNativeDir ( & wd );
            KDirectoryRemove ( wd, true, m_source . GetDatabaseName() . c_str() );
            KDirectoryRelease ( wd );
        }
    }
    
    void OpenDatabase()
    {
        CloseDatabase();
        
        VDBManager * vdb;
        if ( VDBManagerMakeUpdate ( & vdb, NULL ) != 0 )
            throw logic_error("GeneralLoaderFixture::OpenDatabase(" + m_source . GetDatabaseName() + "): VDBManagerMakeUpdate failed");
           
        if ( VDBManagerOpenDBUpdate ( vdb, &m_db, NULL, m_source . GetDatabaseName() . c_str() ) != 0 )
            throw logic_error("GeneralLoaderFixture::OpenDatabase(" + m_source . GetDatabaseName() + "): VDBManagerOpenDBUpdate failed");
        
        if ( VDBManagerRelease ( vdb ) != 0 )
            throw logic_error("GeneralLoaderFixture::OpenDatabase(" + m_source . GetDatabaseName() + "): VDBManagerRelease failed");
    }
    void CloseDatabase()
    {
        if ( m_db != 0 )
        {
            VDatabaseRelease ( m_db );
        }
        if ( m_cursor != 0 )
        {
            VCursorRelease ( m_cursor );
        }
    }
    
    void OpenCursor( const char* p_table, const char* p_column )
    {
        OpenDatabase();
        const VTable * tbl;
        if ( VDatabaseOpenTableRead ( m_db, &tbl, p_table ) != 0 )
            throw logic_error(string ( "GeneralLoaderFixture::OpenCursor(" ) + p_table + "): VDatabaseOpenTableRead failed");
        if ( VTableCreateCursorRead ( tbl, & m_cursor ) != 0 )
            throw logic_error(string ( "GeneralLoaderFixture::OpenCursor(" ) + p_table + "): VTableCreateCursorRead failed");
        
        uint32_t idx;
        if ( VCursorAddColumn ( m_cursor, &idx, p_column ) != 0 ) 
            throw logic_error(string ( "GeneralLoaderFixture::OpenCursor(" ) + p_column + "): VCursorAddColumn failed");
        
        if ( VCursorOpen ( m_cursor ) != 0 )
            throw logic_error(string ( "GeneralLoaderFixture::OpenCursor(" ) + p_table + "): VCursorOpen failed");
        
        if ( VTableRelease ( tbl ) != 0 )
            throw logic_error(string ( "GeneralLoaderFixture::OpenCursor(" ) + p_table + "): VTableRelease failed");
    }
    
    template < typename T > T GetValue ( const char* p_table, const char* p_column, uint64_t p_row )
    {
        OpenCursor( p_table, p_column ); 
        if ( VCursorSetRowId ( m_cursor, p_row ) ) 
            throw logic_error("GeneralLoaderFixture::GetValueU32(): VCursorSetRowId failed");
        
        if ( VCursorOpenRow ( m_cursor ) != 0 )
            throw logic_error("GeneralLoaderFixture::GetValueU32(): VCursorOpenRow failed");
            
        T ret;
        uint32_t num_read;
        if ( VCursorRead ( m_cursor, 1, 8 * sizeof ret, &ret, 1, &num_read ) != 0 )
            throw logic_error("GeneralLoaderFixture::GetValueU32(): VCursorRead failed");
        
        if ( VCursorCloseRow ( m_cursor ) != 0 )
            throw logic_error("GeneralLoaderFixture::GetValueU32(): VCursorCloseRow failed");

         return ret;
    }
    
    
    void SetUpStream( const char* p_dbName )
    {
        m_source . SetNames ( "align/align.vschema", "NCBI:align:db:alignment_sorted", p_dbName );
    }
    
    void SetUpStream_OneTable( const char* p_dbName, const char* p_tableName )
    {
        SetUpStream( p_dbName );
        m_source . NewTableEvent ( 1, p_tableName ); 
    }
    void OpenStream_OneTableOneColumn ( const char* p_dbName, const char* p_tableName, const char* p_columnName )
    {
        SetUpStream_OneTable( p_dbName, p_tableName ); 
        m_source . NewColumnEvent ( 1, 1, p_columnName );
        m_source . OpenStreamEvent();
    }

    TestSource      m_source;
    VDatabase *     m_db;
    const VCursor * m_cursor;
    bool            m_keepDatabase;
};    

template<> std::string GeneralLoaderFixture::GetValue ( const char* p_table, const char* p_column, uint64_t p_row )
{
    OpenCursor( p_table, p_column ); 
    if ( VCursorSetRowId ( m_cursor, p_row ) ) 
        throw logic_error("GeneralLoaderFixture::GetValue(): VCursorSetRowId failed");
    
    if ( VCursorOpenRow ( m_cursor ) != 0 )
        throw logic_error("GeneralLoaderFixture::GetValue(): VCursorOpenRow failed");
        
    char buf[1024];
    uint32_t num_read;
    if ( VCursorRead ( m_cursor, 1, 8, &buf, sizeof buf, &num_read ) != 0 )
        throw logic_error("GeneralLoaderFixture::GetValue(): VCursorRead failed");
    
    if ( VCursorCloseRow ( m_cursor ) != 0 )
        throw logic_error("GeneralLoaderFixture::GetValue(): VCursorCloseRow failed");

     return string ( buf, num_read );
}



const char* tableName = "REFERENCE";
const char* columnName = "SPOT_GROUP";
  
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
    TestSource ts ( GeneralLoaderSignatureString, 0x10000000 );
    REQUIRE ( Run ( ts . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcFormat, rcUnsupported ) ) );
}

FIXTURE_TEST_CASE ( LaterVersion, GeneralLoaderFixture )
{
    TestSource ts ( GeneralLoaderSignatureString, 1, 2 );
    REQUIRE ( Run ( ts . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcHeader, rcBadVersion ) ) );
}

FIXTURE_TEST_CASE ( TruncatedMetadata , GeneralLoaderFixture )
{   // not enough data to read the fixed part of the metadata 
    SetUpStream ( GetName() );
    REQUIRE ( Run ( m_source . MakeTruncatedSource ( sizeof ( GeneralLoader :: Header ) + 8 ), 
              SILENT_RC ( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( BadSchemaFileName, GeneralLoaderFixture )
{   
    m_source . SetNames ( "this file should not exist", "someSchemaName", "database" );
    m_source . OpenStreamEvent ();
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( BadSchemaName, GeneralLoaderFixture )
{   
    m_source . SetNames ( "align/align.vschema", "bad schema name", "database" );
    m_source . OpenStreamEvent ();
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( BadDatabaseName, GeneralLoaderFixture )
{   
    SetUpStream ( "" );
    m_source . OpenStreamEvent ();
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcDB, rcMgr, rcCreating, rcPath, rcIncorrect ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( BadTableName, GeneralLoaderFixture )
{   
    SetUpStream_OneTable ( GetName(), "nosuchtable" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcDatabase, rcCreating, rcSchema, rcNotFound ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( DuplicateTableId, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 1, "REFERENCE" );
    m_source . NewTableEvent ( 1, "SEQUENCE" ); // same Id
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcTable, rcExists ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( BadColumnName, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 1, "REFERENCE" );
    m_source . NewColumnEvent ( 1, 1, "nosuchcolumn" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcCursor, rcUpdating, rcColumn, rcNotFound ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( BadTableId, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 1, "REFERENCE" );
    m_source . NewColumnEvent ( 1, 2, "SPOT_GROUP" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcTable, rcInvalid ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( DuplicateColumnName, GeneralLoaderFixture )
{   
    SetUpStream_OneTable ( GetName(), "REFERENCE" );
    m_source . NewColumnEvent ( 1, 1, "SPOT_GROUP" );
    m_source . NewColumnEvent ( 2, 1, "SPOT_GROUP" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcCursor, rcUpdating, rcColumn, rcExists ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( DuplicateColumnId, GeneralLoaderFixture )
{   
    SetUpStream_OneTable ( GetName(), "REFERENCE" );
    m_source . NewColumnEvent ( 1, 1, "SPOT_GROUP" );
    m_source . NewColumnEvent ( 1, 1, "NAME" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcColumn, rcExists ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( NoOpenStreamEvent, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( NoCloseStreamEvent, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    m_source . OpenStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcNS, rcFile, rcReading, rcTransfer, rcIncomplete ) ) );
    REQUIRE ( ! DatabaseExists() );
}

FIXTURE_TEST_CASE ( NoColumns, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    // make sure database exists and is valid
    REQUIRE ( DatabaseExists() );
    OpenDatabase (); // did not throw => opened successfully
}

FIXTURE_TEST_CASE ( NoData, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 2, tableName ); // ids do not have to be consecutive
    m_source . NewColumnEvent ( 300, 2, columnName );   
    m_source . OpenStreamEvent();
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    OpenCursor( tableName, columnName ); 
    uint64_t count;
    REQUIRE_RC ( VCursorIdRange ( m_cursor, 1, NULL, &count ) );
    REQUIRE_EQ ( (uint64_t)0, count );
}

FIXTURE_TEST_CASE ( Chunk_BadColumnId, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );
    
    m_source . CellDataEvent( /*bad*/2, string() );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( WriteNoCommit, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );

    string value = "a single character string cell";
    m_source . CellDataEvent( 1, value );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    OpenCursor( tableName, columnName ); 
    uint64_t count;
    REQUIRE_RC ( VCursorIdRange ( m_cursor, 1, NULL, &count ) );
    REQUIRE_EQ ( (uint64_t)0, count );
}

FIXTURE_TEST_CASE ( CommitBadTableId, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );

    m_source . NextRowEvent ( /*bad*/2 );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcExe, rcFile, rcReading, rcTable, rcNotFound ) ) );
}    

FIXTURE_TEST_CASE ( OneColumnOneCellOneChunk, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );

    string value = "a single character string cell";
    m_source . CellDataEvent( 1, value );
    m_source . NextRowEvent ( 1 );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value, GetValue<string> ( tableName, columnName, 1 ) ); 
}

FIXTURE_TEST_CASE ( OneColumnOneCellManyChunks, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );

    string value1 = "first";
    m_source . CellDataEvent( 1, value1 );
    string value2 = "second!";
    m_source . CellDataEvent( 1, value2 );
    string value3 = "third!!!";
    m_source . CellDataEvent( 1, value3 );
    m_source . NextRowEvent ( 1 );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value1 + value2 + value3, GetValue<string> ( tableName, columnName, 1 ) ); 
}

FIXTURE_TEST_CASE ( OneColumnDefaultNoWrite, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );

    string value = "this be my default";
    m_source . CellDefaultEvent( 1, value );
    // no WriteEvent
    m_source . NextRowEvent ( 1 );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value, GetValue<string> ( tableName, columnName, 1 ) ); 
}

FIXTURE_TEST_CASE ( OneColumnDefaultOverwite, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );

    string valueDflt = "this be my default";
    m_source . CellDefaultEvent( 1, valueDflt );
    string value = "not the default";
    m_source . CellDataEvent( 1, value );
    m_source . NextRowEvent ( 1 );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value, GetValue<string> ( tableName, columnName, 1 ) ); 
}

FIXTURE_TEST_CASE ( OneColumnChangeDefault, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );
    
    string value1 = "this be my first default";
    m_source . CellDefaultEvent( 1, value1 );
    m_source . NextRowEvent ( 1 );
    
    string value2 = "and this be my second default";
    m_source . CellDefaultEvent( 1, value2 );
    m_source . NextRowEvent ( 1 );
    
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value1, GetValue<string> ( tableName, columnName, 1 ) ); 
    REQUIRE_EQ ( value2, GetValue<string> ( tableName, columnName, 2 ) ); 
}

FIXTURE_TEST_CASE ( OneColumnDataAndDefaultsMixed, GeneralLoaderFixture )
{   
    OpenStream_OneTableOneColumn ( GetName(), tableName, columnName );
    
    string value1 = "first value";
    m_source . CellDataEvent( 1, value1 );
    m_source . NextRowEvent ( 1 );
    
    string default1 = "first default";
    m_source . CellDefaultEvent( 1, default1 );
    m_source . NextRowEvent ( 1 );
    m_source . NextRowEvent ( 1 );
    
    string value2 = "second value";
    m_source . CellDefaultEvent( 1, value2 );
    m_source . NextRowEvent ( 1 );
    
    string default2 = "second default";
    m_source . CellDefaultEvent( 1, default2 );
    m_source . NextRowEvent ( 1 );
    
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value1,    GetValue<string> ( tableName, columnName, 1 ) ); 
    REQUIRE_EQ ( default1,  GetValue<string> ( tableName, columnName, 2 ) ); 
    REQUIRE_EQ ( default1,  GetValue<string> ( tableName, columnName, 3 ) ); 
    REQUIRE_EQ ( value2,    GetValue<string> ( tableName, columnName, 4 ) ); 
    REQUIRE_EQ ( default2,  GetValue<string> ( tableName, columnName, 5 ) ); 
}

FIXTURE_TEST_CASE ( TwoColumnsFullRow, GeneralLoaderFixture )
{   
    SetUpStream_OneTable ( GetName(), tableName );

    const char* columnName1 = "SPOT_GROUP";
    const char* columnName2 = "MAX_SEQ_LEN";
    m_source . NewColumnEvent ( 1, 1, columnName1 );
    m_source . NewColumnEvent ( 2, 1, columnName2 );
    m_source . OpenStreamEvent();
    
    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    uint32_t value2 = 12345;
    m_source . CellDataEvent( 2, value2 );
    m_source . NextRowEvent ( 1 );
    
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value1,    GetValue<string>    ( tableName, columnName1, 1 ) ); 
    REQUIRE_EQ ( value2,    GetValue<uint32_t>  ( tableName, columnName2, 1 ) ); 
}

FIXTURE_TEST_CASE ( TwoColumnsIncompleteRow, GeneralLoaderFixture )
{   
    SetUpStream_OneTable ( GetName(), tableName );

    const char* columnName1 = "SPOT_GROUP";
    const char* columnName2 = "MAX_SEQ_LEN";
    m_source . NewColumnEvent ( 1, 1, columnName1 );
    m_source . NewColumnEvent ( 2, 1, columnName2 );
    m_source . OpenStreamEvent();
    
    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    // no CellData for column 2
    m_source . NextRowEvent ( 1 );
    
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcColumn, rcClosing, rcRow, rcIncomplete) ) );
}

FIXTURE_TEST_CASE ( TwoColumnsPartialRowWithDefaults, GeneralLoaderFixture )
{   
    SetUpStream_OneTable ( GetName(), tableName );

    const char* columnName1 = "SPOT_GROUP";
    const char* columnName2 = "MAX_SEQ_LEN";
    m_source . NewColumnEvent ( 1, 1, columnName1 );
    m_source . NewColumnEvent ( 2, 1, columnName2 );
    m_source . OpenStreamEvent();
    
    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    uint32_t value2 = 12345;
    m_source . CellDefaultEvent( 2, value2 );
    // no Data Event for column2
    m_source . NextRowEvent ( 1 );
    
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value1,    GetValue<string>    ( tableName, columnName1, 1 ) ); 
    REQUIRE_EQ ( value2,    GetValue<uint32_t>  ( tableName, columnName2, 1 ) ); 
}

FIXTURE_TEST_CASE ( TwoColumnsPartialRowWithDefaultsAndOverride, GeneralLoaderFixture )
{   
    SetUpStream_OneTable ( GetName(), tableName );

    const char* columnName1 = "SPOT_GROUP";
    const char* columnName2 = "MAX_SEQ_LEN";
    const char* columnName3 = "CIRCULAR";
    m_source . NewColumnEvent ( 1, 1, columnName1 );
    m_source . NewColumnEvent ( 2, 1, columnName2 );
    m_source . NewColumnEvent ( 3, 1, columnName3 );
    m_source . OpenStreamEvent();
    
    string value1 = "value1";
    m_source . CellDataEvent( 1, value1 );
    uint32_t value2 = 12345;
    m_source . CellDefaultEvent( 2, value2 );
    m_source . CellDefaultEvent( 3, false );
    bool value3 = true;
    m_source . CellDataEvent( 3, true ); // override default
    m_source . NextRowEvent ( 1 );
    
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( value1,    GetValue<string>    ( tableName, columnName1, 1 ) );       // explicit
    REQUIRE_EQ ( value2,    GetValue<uint32_t>  ( tableName, columnName2, 1 ) );    // default
    REQUIRE_EQ ( value3,    GetValue<bool>      ( tableName, columnName3, 1 ) );   // not the default
}

FIXTURE_TEST_CASE ( MultipleTables_Multiple_Columns_MultipleRows, GeneralLoaderFixture )
{   
    SetUpStream ( GetName() );
    
    const char* table1 = "REFERENCE";
    const char* table1column1 = "SPOT_GROUP";           // ascii
    const char* table1column2 = "MAX_SEQ_LEN";          // u32
    m_source . NewTableEvent ( 1, table1 );
    m_source . NewColumnEvent ( 1, 1, table1column1 );
    m_source . NewColumnEvent ( 2, 1, table1column2 );

    const char* table2 = "SEQUENCE";
    const char* table2column1 = "PRIMARY_ALIGNMENT_ID"; // i64
    const char* table2column2 = "ALIGNMENT_COUNT";      // u8
    m_source . NewTableEvent ( 2, table2 );
    m_source . NewColumnEvent ( 3, 2, table2column1 );
    m_source . NewColumnEvent ( 4, 2, table2column2 );
    
    m_source . OpenStreamEvent();
    
        string t1c1v1 = "t1c1v1";
        m_source . CellDataEvent( 1, t1c1v1 );
        uint32_t t1c2v1 = 121;
        m_source . CellDataEvent( 2, t1c2v1 );
    m_source . NextRowEvent ( 1 );

        int64_t t2c1v1 = 211;
        m_source . CellDataEvent( 3, t2c1v1 );
        uint8_t t2c2v1 = 221;
        m_source . CellDataEvent( 4, t2c2v1 );
    m_source . NextRowEvent ( 2 );
    
        string t1c1v2 = "t1c1v2";
        m_source . CellDataEvent( 1, t1c1v2 );
        uint32_t t1c2v2 = 122;
        m_source . CellDataEvent( 2, t1c2v2 );
    m_source . NextRowEvent ( 1 );

        int64_t t2c1v2 = 212;
        m_source . CellDataEvent( 3, t2c1v2 );
        uint8_t t2c2v2 = 222;
        m_source . CellDataEvent( 4, t2c2v2 );
    m_source . NextRowEvent ( 2 );
    
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    REQUIRE_EQ ( t1c1v1,    GetValue<string>    ( table1, table1column1, 1 ) );      
    REQUIRE_EQ ( t1c2v1,    GetValue<uint32_t>  ( table1, table1column2, 1 ) );   
    REQUIRE_EQ ( t2c1v1,    GetValue<int64_t>   ( table2, table2column1, 1 ) );      
    REQUIRE_EQ ( t2c2v1,    GetValue<uint8_t>   ( table2, table2column2, 1 ) );   
    
    REQUIRE_EQ ( t1c1v2,    GetValue<string>    ( table1, table1column1, 2 ) );      
    REQUIRE_EQ ( t1c2v2,    GetValue<uint32_t>  ( table1, table1column2, 2 ) );   
    REQUIRE_EQ ( t2c1v2,    GetValue<int64_t>   ( table2, table2column1, 2 ) );      
    REQUIRE_EQ ( t2c2v2,    GetValue<uint8_t>   ( table2, table2column2, 2 ) );   
}

FIXTURE_TEST_CASE ( AdditionalSchemaIncludePaths_Single, GeneralLoaderFixture )
{   
    string schemaPath = "schema";
    string includeName = string ( GetName() ) + ".inc.vschema";
    string schemaIncludeFile = schemaPath + "/" + includeName;
    { 
        ofstream out ( schemaIncludeFile . c_str() );
        out << 
            "table table1 #1.0.0\n"
            "{\n"
            "    column ascii column1;\n"
            "};\n"
        ;
    }
    
    string schemaFile = string ( GetName() ) + ".vschema";
    { 
        string schemaText = 
            string ( "include '" ) + includeName + "';\n"
            "database database1 #1\n"
            "{\n"
            "    table table1 #1 TABLE1;\n"
            "};\n"
        ;
        ofstream out( schemaFile . c_str() );
        out << schemaText;
    }
    
    string dbFile = string ( GetName() ) + ".db";
    m_source . SetNames ( schemaFile, "database1", dbFile );
    
    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( 1, table1 );
    m_source . NewColumnEvent ( 1, 1, column1 );
    m_source . OpenStreamEvent();
    
    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( 1, t1c1v1 );
    m_source . NextRowEvent ( 1 );
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
    { 
        ofstream out ( schemaIncludeFile . c_str() );
        out << 
            "table table1 #1.0.0\n"
            "{\n"
            "    column ascii column1;\n"
            "};\n"
        ;
    }
    
    string schemaFile = string ( GetName() ) + ".vschema";
    { 
        string schemaText = 
            string ( "include '" ) + includeName + "';\n"
            "database database1 #1\n"
            "{\n"
            "    table table1 #1 TABLE1;\n"
            "};\n"
        ;
        ofstream out( schemaFile . c_str() );
        out << schemaText;
    }
    
    string dbFile = string ( GetName() ) + ".db";
    m_source . SetNames ( schemaFile, "database1", dbFile );
    
    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( 1, table1 );
    m_source . NewColumnEvent ( 1, 1, column1 );
    m_source . OpenStreamEvent();
    
    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( 1, t1c1v1 );
    m_source . NextRowEvent ( 1 );
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
    { 
        string schemaText = 
            "table table1 #1.0.0\n"
            "{\n"
            "    column ascii column1;\n"
            "};\n"
            "database database1 #1\n"
            "{\n"
            "    table table1 #1 TABLE1;\n"
            "};\n"
        ;
        ofstream out( schemaFile . c_str() );
        out << schemaText;
    }
    
    string dbFile = string ( GetName() ) + ".db";
    // here we do not specify a schema but it's OK as long as we add a good schema later
    m_source . SetNames ( "", "database1", dbFile );
    
    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( 1, table1 );
    m_source . NewColumnEvent ( 1, 1, column1 );
    m_source . OpenStreamEvent();
    
    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( 1, t1c1v1 );
    m_source . NextRowEvent ( 1 );
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
    { 
        string schemaText = 
            "table table1 #1.0.0\n"
            "{\n"
            "    column ascii column1;\n"
            "};\n"
        ;
        ofstream out( schemaFile1 . c_str() );
        out << schemaText;
    }
    string schemaFile2 = schemaPath + "/" + GetName() + "2.vschema";
    {  // this file uses table 1 defined in schemaFile1
        string schemaText = 
            "database database1 #1\n"
            "{\n"
            "    table table1 #1 TABLE1;\n"
            "};\n"
        ;
        ofstream out( schemaFile2 . c_str() );
        out << schemaText;
    }
    
    string dbFile = string ( GetName() ) + ".db";
    
    // here we specify a schema that does not exist but it's OK as long as we add a good schema later
    m_source . SetNames ( "doesnotexist", "database1", dbFile ); 
    
    const char* table1 = "TABLE1";
    const char* column1 = "column1";
    m_source . NewTableEvent ( 1, table1 );
    m_source . NewColumnEvent ( 1, 1, column1 );
    m_source . OpenStreamEvent();
    
    string t1c1v1 = "t1c1v1";
    m_source . CellDataEvent( 1, t1c1v1 );
    m_source . NextRowEvent ( 1 );
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
    SetUpStream ( GetName() );
    m_source . NewTableEvent ( 2, tableName ); 
    m_source . NewColumnEvent ( 300, 2, columnName );   
    m_source . OpenStreamEvent();
    m_source . ErrorMessageEvent ( "error message" );
    m_source . CloseStreamEvent();
    
    REQUIRE ( Run ( m_source . MakeSource (), RC ( rcExe, rcFile, rcReading, rcError, rcExists ) ) );
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kapp/args.h>
#include <kfg/config.h>

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

rc_t CC Usage ( const Args * args )
{
    return 0;
}

const char UsageDefaultName[] = "test-general-loader";

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    KLogLevelSet ( klogFatal );
    rc_t rc=GeneralLoaderTestSuite(argc, argv);
    return rc;
}

}  

