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

#include "../../tools/general-loader/general-loader.cpp"

#include <klib/printf.h>

#include <kns/adapt.h>

#include <kfs/nullfile.h>
#include <kfs/ramfile.h>
#include <kfs/file.h>

#include <cstring>
#include <stdexcept> 
#include <map>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(GeneralLoaderTestSuite);

class TestSource
{
public:
    TestSource( const std:: string& p_signature )
    :   m_buffer(0)
    {
        strncpy ( m_header . signature, p_signature.c_str(), sizeof m_header . signature );
    }
    TestSource( uint32_t p_endness = 1, uint32_t p_version  = 1 )
    :   m_buffer(0)
    {
        strncpy ( m_header . signature, GeneralLoaderSignatureString, sizeof m_header . signature );
        m_header . endian = p_endness;
        m_header . version = p_version;
        
        m_md . md_size = 0;
        m_md . str_size =  0;
        m_md . schema_offset        = 0;
        m_md . schema_file_offset   = 0;
        m_md . database_name_offset = 0;
        m_md . num_columns  = 0; 
    }
    
    ~TestSource()
    {
        delete [] m_buffer;
        RemoveDatabase();
    }
    
    void RemoveDatabase()
    {
        if ( ! m_database . empty () )
        {
            KDirectory* wd;
            KDirectoryNativeDir ( & wd );
            KDirectoryRemove ( wd, true, m_database . c_str() );
            KDirectoryRelease ( wd );
        }
    }
    
    void SetNames ( const std:: string& p_schemaFile, const std:: string& p_schema, const std:: string& p_databaseName )
    {
        m_md . schema_offset        = AddString ( p_schema );
        m_md . schema_file_offset   = AddString ( p_schemaFile );
        m_md . database_name_offset = AddString ( p_databaseName );
        
        m_database = p_databaseName;
        RemoveDatabase();
    }
    
    const struct KFile * MakeHeaderOnly()
    {
        const struct KFile * ret;
        if ( KRamFileMakeRead ( & ret, ( char * ) & m_header, sizeof m_header ) != 0 || ret == 0 )
            throw logic_error("TestSource::MakeHeaderOnly KRamFileMakeRead failed");
            
        return ret;
    }
    
    const struct KFile * MakeTruncatedSource ( size_t p_size )
    {
        FillBuffer();
        const struct KFile * ret;
        if ( KRamFileMakeRead ( & ret, m_buffer, p_size ) != 0 || ret == 0 )
            throw logic_error("TestSource::MakeTruncatedSource KRamFileMakeRead failed");
            
        return ret;
    }
    
    const struct KFile * MakeSource ()
    {
        size_t size = FillBuffer();
        const struct KFile * ret;
        if ( KRamFileMakeRead ( & ret, m_buffer, size ) != 0 || ret == 0 )
            throw logic_error("TestSource::MakeTruncatedSource KRamFileMakeRead failed");
            
        return ret;
    }
    
    void AddColumn ( const char* p_table, const char* p_column )
    {
        m_columns . push_back ( Columns :: value_type ( AddString ( p_table ), AddString ( p_column ) ) );
    }
    
private:    
    size_t MetadataSize() const
    {
        return sizeof ( m_md ) + 
               m_columns . size () * ( sizeof ( Column::first_type ) + sizeof ( Column::second_type ) ) + 
               m_md . str_size;
    }
    
    static uint32_t Pad ( uint32_t p_value, uint32_t p_alignment )
    {
        if ( p_value % p_alignment == 0 )
        {
            return p_value;
        }
        
        return p_value + p_alignment - p_value % p_alignment;
    }
    
    static const unsigned int Alignment = 4;
    size_t DataSize()
    {
        // TBD (call Pad() on every chunk)
        return 0;
    }
    
    size_t FillBuffer()
    {
        uint32_t end_marker = 0;
        
        m_md . str_size = Pad ( m_md . str_size, Alignment ); 
        m_md . md_size = MetadataSize();
        m_md . num_columns = m_columns . size ();
        
        size_t bufSize = sizeof m_header + MetadataSize() + DataSize() + sizeof ( end_marker ); 
        m_buffer = new char [ bufSize ];
        char* writePtr = m_buffer;

        // header 
        memcpy ( writePtr, & m_header, sizeof m_header );
        writePtr += sizeof m_header;
        
        // metadata, fixed portion
        m_md . md_size = MetadataSize();
        m_md . num_columns = m_columns . size ();
        memcpy ( writePtr, & m_md, sizeof m_md);
        writePtr += sizeof m_md;
        
        // metadata, variable portion (columns table)
        for ( Columns::const_iterator it = m_columns . begin(); it != m_columns . end(); ++it )
        {
            memcpy ( writePtr, & it -> first, sizeof  it -> first );
            writePtr += sizeof it -> first;
            memcpy ( writePtr, & it -> second, sizeof  it -> second );
            writePtr += sizeof it -> second;
        }
        
        // string table
        for ( StringTable:: const_iterator it = m_strings . begin(); it != m_strings . end(); ++it )
        {
            memcpy ( writePtr, it -> c_str(), it -> size () + 1 );
            writePtr += it -> size () + 1;
        }
        // 0-pad to align
        if ( ( writePtr - m_buffer ) % Alignment != 0 )
        {
            uint32_t zero = 0;
            size_t pad = Alignment - ( writePtr - m_buffer ) % Alignment;
            memcpy ( writePtr, & zero, pad );
            writePtr += pad;
        }
        assert ( ( writePtr - m_buffer )  % Alignment == 0 );
        
        // data chunks
        //TBD
        
        // end marker
        assert ( ( writePtr - m_buffer )  % Alignment == 0 );
        
        memcpy ( writePtr, & end_marker, sizeof end_marker );
        writePtr += sizeof end_marker;
        
        assert ( writePtr == m_buffer + bufSize );
        
        return bufSize;
    }
    
    uint32_t AddString(const string& p_str)
    {
        uint32_t ret = 0;
        for ( StringTable:: const_iterator it = m_strings . begin(); it != m_strings . end(); ++it )
        {
            if ( *it == p_str )
            {
                return ret;
            }
            ret += it -> size () + 1;
        }
        m_strings . push_back ( p_str );
        m_md . str_size = ret + p_str . size () + 1;
        
        return ret;
    }
    
    string  m_database;
    
    GeneralLoader :: Header     m_header;
    GeneralLoader :: Metadata   m_md;
    
    char* m_buffer;
    
    typedef vector < string >           StringTable;
    typedef pair < uint32_t, uint32_t > Column;
    typedef vector < Column >           Columns;
    
    StringTable m_strings;
    Columns     m_columns;
};

class GeneralLoaderFixture
{
public:
    GeneralLoaderFixture()
    {
    }
    ~GeneralLoaderFixture()
    {
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
    
    TestSource m_source;
};    

FIXTURE_TEST_CASE ( EmptyInput, GeneralLoaderFixture )
{
    const struct KFile * input;
    REQUIRE_RC ( KFileMakeNullRead ( & input ) );
    REQUIRE ( Run ( input, SILENT_RC ( rcExe, rcFile, rcReading, rcFile, rcInsufficient ) ) );
}

FIXTURE_TEST_CASE ( ShortInput, GeneralLoaderFixture )
{
    char buffer[10];
    const struct KFile * input;
    REQUIRE_RC ( KRamFileMakeRead ( & input, buffer, sizeof buffer ) );
    
    REQUIRE ( Run ( input, SILENT_RC ( rcExe, rcFile, rcReading, rcFile, rcInsufficient ) ) );
}

FIXTURE_TEST_CASE ( BadSignature, GeneralLoaderFixture )
{
    TestSource ts ( "badsigna" );
    REQUIRE ( Run (  ts . MakeHeaderOnly (), SILENT_RC ( rcExe, rcFile, rcReading, rcHeader, rcCorrupt ) ) );
}

FIXTURE_TEST_CASE ( BadEndianness, GeneralLoaderFixture )
{
    TestSource ts ( 3 );
    REQUIRE ( Run ( ts . MakeHeaderOnly (), SILENT_RC ( rcExe, rcFile, rcReading, rcFormat, rcInvalid) ) );
}

FIXTURE_TEST_CASE ( ReverseEndianness, GeneralLoaderFixture )
{
    TestSource ts ( 0x10000000 );
    REQUIRE ( Run ( ts . MakeHeaderOnly (), SILENT_RC ( rcExe, rcFile, rcReading, rcFormat, rcUnsupported ) ) );
}

FIXTURE_TEST_CASE ( LaterVersion, GeneralLoaderFixture )
{
    TestSource ts ( 1, 2 );
    REQUIRE ( Run ( ts . MakeHeaderOnly (), SILENT_RC ( rcExe, rcFile, rcReading, rcHeader, rcBadVersion ) ) );
}

FIXTURE_TEST_CASE ( NoMetadata , GeneralLoaderFixture )
{
    REQUIRE ( Run ( m_source . MakeHeaderOnly (), SILENT_RC ( rcExe, rcFile, rcReading, rcMetadata, rcCorrupt ) ) );
}

FIXTURE_TEST_CASE ( TruncatedMetadata, GeneralLoaderFixture )
{   // not enough data to read the fixed part of the metadata 
    REQUIRE ( Run ( m_source . MakeTruncatedSource ( sizeof ( GeneralLoader :: Header ) + 8 ), 
                         SILENT_RC ( rcExe, rcFile, rcReading, rcMetadata, rcCorrupt ) ) );
}

FIXTURE_TEST_CASE ( BadSchemaFileName, GeneralLoaderFixture )
{   
    m_source . SetNames ( "this file should not exist", "someSchemaName", "database" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcFS, rcDirectory, rcOpening, rcPath, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadSchemaName, GeneralLoaderFixture )
{   
    m_source . SetNames ( "align/align.vschema", "bad schema name", "database" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadDatabaseName, GeneralLoaderFixture )
{   
    m_source . SetNames ( "align/align.vschema", "NCBI:align:db:alignment_sorted", "" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcDB, rcMgr, rcCreating, rcPath, rcIncorrect ) ) );
}

FIXTURE_TEST_CASE ( BadTableName, GeneralLoaderFixture )
{   
    m_source . SetNames ( "align/align.vschema", "NCBI:align:db:alignment_sorted", GetName() );
    m_source . AddColumn ( "nosuchtable", "column" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcDatabase, rcCreating, rcSchema, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( BadColumnName, GeneralLoaderFixture )
{   
    m_source . SetNames ( "align/align.vschema", "NCBI:align:db:alignment_sorted", GetName() );
    m_source . AddColumn ( "REFERENCE", "nosuchcolumn" );
    REQUIRE ( Run ( m_source . MakeSource (), SILENT_RC ( rcVDB, rcCursor, rcUpdating, rcColumn, rcNotFound ) ) );
}

FIXTURE_TEST_CASE ( NoColumns, GeneralLoaderFixture )
{   
    m_source . SetNames ( "align/align.vschema", "NCBI:align:db:alignment_sorted", GetName() );
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    // make sure database exists
    VDBManager * vdb;
    REQUIRE_RC ( VDBManagerMakeUpdate ( & vdb, NULL ) );
    VDatabase * db;
    REQUIRE_RC ( VDBManagerOpenDBUpdate ( vdb, &db, NULL, GetName() ) );
    REQUIRE_RC ( VDatabaseRelease ( db ) );
    REQUIRE_RC ( VDBManagerRelease ( vdb ) );
}

FIXTURE_TEST_CASE ( NoData, GeneralLoaderFixture )
{   
    m_source . SetNames ( "align/align.vschema", "NCBI:align:db:alignment_sorted", GetName() );
    const char* tableName = "REFERENCE";
    const char* columnName = "SPOT_ID";
    
    m_source . AddColumn ( tableName, columnName );
    REQUIRE ( Run ( m_source . MakeSource (), 0 ) );
    
    // validate the database
    VDBManager * vdb;
    REQUIRE_RC ( VDBManagerMakeUpdate ( & vdb, NULL ) );
    VDatabase * db;
    REQUIRE_RC ( VDBManagerOpenDBUpdate ( vdb, &db, NULL, GetName() ) );
    const VTable * tbl;
    REQUIRE_RC ( VDatabaseOpenTableRead ( db, &tbl, tableName ) );
    const VCursor * cur;
    REQUIRE_RC ( VTableCreateCursorRead ( tbl, & cur ) );
    
    uint32_t idx;
    REQUIRE_RC ( VCursorAddColumn ( cur, &idx, columnName ) );
    REQUIRE_RC ( VCursorOpen ( cur ) );
    
    uint64_t count;
    REQUIRE_RC ( VCursorIdRange ( cur, idx, NULL, &count ) );
    REQUIRE_EQ ( (uint64_t)0, count );
    
    REQUIRE_RC ( VCursorRelease ( cur ) );
    REQUIRE_RC ( VTableRelease ( tbl ) );
    REQUIRE_RC ( VDatabaseRelease ( db ) );
    REQUIRE_RC ( VDBManagerRelease ( vdb ) );
}

//FIXTURE_TEST_CASE ( OneColumnOneCellOneChunk, GeneralLoaderFixture )
//{   
//}

//TODO:
// corrupt header:
//  bad string offset in column table's column_name_offset 
//  bad string offset in column table's table_name_offset
//  other kinds of broken header... 
// 
// problems creating database:
//  error opening cursor (how ?)

// Populating the database:
//
//  error cases
//
//  one table, one column, write a cell in one chunk, commit, close
//  one table, one column, write a cell in several chunks, commit, close
//  one table, one column, set-default, (no write), commit, close
//  one table, one column, set-default, write (not a default), commit, close
//  one table, one column, set-default, (no write), commit, set-default (different), (no write),  commit, close
//  one table, one column, write a chunk, set-default, write another chunk, commit, (no write), commit, close
//
//  one table, multiple columns, write all cells, commit, close
//  one table, multiple columns, write some cells, commit, error (no defaults)
//  one table, multiple columns, set defaults for some cells, write others, commit, close
//  one table, multiple columns, set defaults for some cells, write others (some overwrite defaults), commit, close
//
//  multiple tables, multiple columns, write all cells, commit, write all cells, commit, close
//
//  duplicate column specs in the columns table (ok)


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
    rc_t rc=GeneralLoaderTestSuite(argc, argv);
    return rc;
}

}  

