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
* Unit tests for the Loader module
*/

#include <loader/loader-meta.h>

#include <ktst/unit_test.hpp>

#include <klib/printf.h>

#include <kdb/meta.h>

#include <kapp/main.h>

#include <vdb/manager.h> // VDBManager
#include <vdb/database.h>
#include <vdb/schema.h> /* VSchemaRelease */

#include <stdexcept>
#include <fstream>

extern "C" {
#include <loader/sequence-writer.h>
}

using namespace std;

TEST_SUITE(LoaderTestSuite);

class LoaderFixture
{
public:
    LoaderFixture()
    :   m_db ( 0 ),
        m_cursor ( 0 ),
        m_keepDatabase ( false )
    {
    }
    ~LoaderFixture()
    {
        RemoveDatabase();
    }

    void RemoveDatabase()
    {
        CloseDatabase();
        if ( ! m_databaseName . empty () && ! m_keepDatabase )
        {
            KDirectory* wd;
            KDirectoryNativeDir ( & wd );
            KDirectoryRemove ( wd, true, m_databaseName . c_str() );
            KDirectoryRelease ( wd );
        }
    }

    void OpenDatabase()
    {
        CloseDatabase();

        VDBManager * vdb;
        THROW_ON_RC ( VDBManagerMakeUpdate ( & vdb, NULL ) );
        THROW_ON_RC ( VDBManagerOpenDBUpdate ( vdb, &m_db, NULL, m_databaseName . c_str() ) );
        THROW_ON_RC( VDBManagerRelease ( vdb ) );
    }

    void InitDatabase ( const char * schemaFile, const char * schemaSpec )
    {
        RemoveDatabase();

        VDBManager* mgr;
        THROW_ON_RC ( VDBManagerMakeUpdate ( & mgr, NULL ) );
        VSchema* schema;
        THROW_ON_RC ( VDBManagerMakeSchema ( mgr, & schema ) );
        THROW_ON_RC ( VSchemaParseFile(schema, "%s", schemaFile ) );

        THROW_ON_RC ( VDBManagerCreateDB ( mgr,
                                          & m_db,
                                          schema,
                                          schemaSpec,
                                          kcmInit + kcmMD5,
                                          "%s",
                                          m_databaseName . c_str() ) );

        THROW_ON_RC ( VSchemaRelease ( schema ) );
        THROW_ON_RC ( VDBManagerRelease ( mgr ) );
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

    void OpenCursor( const char* p_table, const char* p_column )
    {
        OpenDatabase();
        const VTable * tbl;
        THROW_ON_RC ( VDatabaseOpenTableRead ( m_db, &tbl, p_table ) );
        THROW_ON_RC ( VTableCreateCursorRead ( tbl, & m_cursor ) );

        uint32_t idx;
        THROW_ON_RC ( VCursorAddColumn ( m_cursor, &idx, p_column ) );
        THROW_ON_RC ( VCursorOpen ( m_cursor ) );
        THROW_ON_RC ( VTableRelease ( tbl ) );
    }

    template < typename T > T GetValue ( const char* p_table, const char* p_column, uint64_t p_row )
    {
        OpenCursor( p_table, p_column );
        THROW_ON_RC ( VCursorSetRowId ( m_cursor, p_row ) );
        THROW_ON_RC ( VCursorOpenRow ( m_cursor ) );

        T ret;
        uint32_t num_read;
        THROW_ON_RC ( VCursorRead ( m_cursor, 1, 8 * sizeof ret, &ret, 1, &num_read ) );
        THROW_ON_RC ( VCursorCloseRow ( m_cursor ) );
        return ret;
    }

    template < typename T, int Count > bool GetValue ( const char* p_table, const char* p_column, uint64_t p_row, T ret[Count] )
    {
        OpenCursor( p_table, p_column );
        THROW_ON_RC ( VCursorSetRowId ( m_cursor, p_row ) );
        THROW_ON_RC ( VCursorOpenRow ( m_cursor ) );

        uint32_t num_read;
        THROW_ON_RC ( VCursorRead ( m_cursor, 1, 8 * sizeof ( T ), ret, Count, &num_read ) );
        if ( num_read != Count )
            throw logic_error("LoaderFixture::GetValueU32(): VCursorRead failed");

        THROW_ON_RC ( VCursorCloseRow ( m_cursor ) );
        return ret;
    }

    std::string GetMetadata ( const std::string& p_node, const std::string& p_attr )
    {
        const KMetadata *meta;
        THROW_ON_RC ( VDatabaseOpenMetadataRead ( m_db, &meta ) );

        const KMDataNode *node;
        THROW_ON_RC ( KMetadataOpenNodeRead ( meta, &node, p_node.c_str() ) );

        size_t num_read;
        char attr [ 256 ];
        THROW_ON_RC ( KMDataNodeReadAttr ( node, p_attr.c_str(), attr, sizeof attr, & num_read ) );
        THROW_ON_RC ( KMDataNodeRelease ( node ) );
        THROW_ON_RC ( KMetadataRelease ( meta ) );
        return string ( attr, num_read );
    }

    void CreateFile ( const string& p_name, const string& p_content )
    {
        ofstream out( p_name . c_str() );
        out << p_content;
    }


    string          m_databaseName;
    VDatabase *     m_db;
    const VCursor * m_cursor;
    bool            m_keepDatabase;
};

template<> std::string LoaderFixture::GetValue ( const char* p_table, const char* p_column, uint64_t p_row )
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

#if SHOW_UNIMPLEMENTED
FIXTURE_TEST_CASE ( SequenceWriter_Write, LoaderFixture )
{
    m_databaseName = GetName();
    InitDatabase ( "./sequencewriter.vschema", "NCBI:align:db:fastq";);

    const string Sequence = "AC";
    const string SpotName = "name1";
    const string SpotGroup = "spotgroup1";
    const uint8_t qual[2] = {10,20};
//m_keepDatabase = true;

    {
        SequenceWriter wr;

        REQUIRE_NOT_NULL ( SequenceWriterInit ( & wr, m_db ) );

        uint32_t readStart = 0;
        uint32_t readLen = Sequence . size();
        uint8_t orientation = 0;
        uint8_t is_bad = 0;
        uint8_t alignmentCount = 0;
        bool aligned = false;
        uint64_t ti = 0;

        SequenceRecord rec;
        rec . seq = (char*) Sequence . c_str ();
        rec . qual = (uint8_t*) qual;
        rec . readStart = & readStart;
        rec . readLen = & readLen;
        rec . orientation = & orientation;
        rec . is_bad = & is_bad;
        rec . alignmentCount = & alignmentCount;
        rec . spotGroup = (char*) SpotGroup . c_str();
        rec . aligned = & aligned;
        rec . cskey = (char*)"";
        rec . ti = & ti;
        rec . spotName = (char*) SpotName . c_str();
        rec . keyId = 1;
        rec . spotGroupLen = SpotGroup . size();;
        rec . spotNameLen = SpotName . size();
        rec . numreads = 1;

        REQUIRE_RC ( KDataBufferMake ( & rec.storage, 8, 0 ) );

        REQUIRE_RC ( SequenceWriteRecord ( & wr,
                                           & rec,
                                           false,
                                           false,
                                           SRA_PLATFORM_454,
                                           false,
                                           false,
                                           false,
                                           "0",
                                           false,
                                           true
                                          ) );
        REQUIRE_RC ( SequenceDoneWriting ( & wr ) );
        SequenceWhack ( & wr, true );
        REQUIRE_RC ( SequenceRecordWhack ( & rec ) );

        CloseDatabase();
    }

    // read, validate
    REQUIRE_EQ ( Sequence, GetValue<string> ( "SEQUENCE", "CMP_READ", 1 ) );
    REQUIRE_EQ ( SpotName, GetValue<string> ( "SEQUENCE", "NAME", 1 ) );
    REQUIRE_EQ ( SpotGroup, GetValue<string> ( "SEQUENCE", "SPOT_GROUP", 1 ) );
    {
        uint8_t q[2] = { 0, 0 };
        bool req = GetValue < uint8_t, 2 > ( "SEQUENCE", "QUALITY", 1, q );
        REQUIRE ( req );
        REQUIRE_EQ ( (int)qual[0], (int)q[0] );
        REQUIRE_EQ ( (int)qual[1], (int)q[1] );
    }
}
#endif

FIXTURE_TEST_CASE ( LoaderMeta_Write, LoaderFixture )
{
    m_databaseName = GetName();

    string schemaFile = string ( GetName() ) + ".vschema";
    CreateFile ( schemaFile, string ( "table table1 #1.0.0 { column ascii column1; }; database database1 #1 { table table1 #1 TABLE1; } ;" ).c_str() );
    InitDatabase ( schemaFile.c_str(), "database1");

    const string FormatterName = "fmt_name";
    const string LoaderName = "test-loader";

    {
        struct KMetadata* meta;
        REQUIRE_RC ( VDatabaseOpenMetadataUpdate ( m_db, &meta ) );
        KMDataNode *node;
        REQUIRE_RC ( KMetadataOpenNodeUpdate ( meta, &node, "/" ) );

        // this is the one we are testing
        REQUIRE_RC ( KLoaderMeta_Write ( node, LoaderName.c_str(), __DATE__, FormatterName.c_str(), KAppVersion() ) );

        REQUIRE_RC ( KMDataNodeRelease ( node ) );
        REQUIRE_RC ( KMetadataRelease ( meta ) );

        CloseDatabase();
    }

    // read, validate
    OpenDatabase ();
    REQUIRE_EQ ( FormatterName,  GetMetadata ( "SOFTWARE/formatter", "name" ) );

    // extract the program name from path the same way it's done in ncbi-vdb/libs/kapp/loader-meta.c:KLoaderMeta_Write
    REQUIRE_EQ ( LoaderName, GetMetadata ( "SOFTWARE/loader", "name" ) );
    REQUIRE_EQ ( string ( __DATE__), GetMetadata ( "SOFTWARE/loader", "date" ) );
    {   // the "old" KLoaderMeta_Write sets versions of loader and formatter to the same value
        char buf[265];
        string_printf ( buf, sizeof buf, NULL, "%V", KAppVersion() ); // same format as in ncbi-vdb/libs/kapp/loader-meta.c:MakeVersion()
        REQUIRE_EQ ( string ( buf ), GetMetadata ( "SOFTWARE/formatter", "vers" ) );
        REQUIRE_EQ ( string ( buf ), GetMetadata ( "SOFTWARE/loader", "vers" ) );
    }

    std::remove ( schemaFile . c_str() );
}

FIXTURE_TEST_CASE ( LoaderMeta_WriteWithVersion, LoaderFixture )
{
    m_databaseName = GetName();
    string schemaFile = string ( GetName() ) + ".vschema";
    CreateFile ( schemaFile, string ( "table table1 #1.0.0 { column ascii column1; }; database database1 #1 { table table1 #1 TABLE1; } ;" ).c_str() );
    InitDatabase ( schemaFile.c_str(), "database1");

    const string FormatterName = "fmt_name";
    const string LoaderName = "test-loader";
    const ver_t  LoaderVersion = ( 1 << 24 ) | ( 2 << 16 ) | 0x0003;

    {
        struct KMetadata* meta;
        REQUIRE_RC ( VDatabaseOpenMetadataUpdate ( m_db, &meta ) );
        KMDataNode *node;
        REQUIRE_RC ( KMetadataOpenNodeUpdate ( meta, &node, "/" ) );

        // this is the one we are testing
        REQUIRE_RC ( KLoaderMeta_WriteWithVersion ( node, LoaderName.c_str(), __DATE__, LoaderVersion, FormatterName.c_str(), KAppVersion() ) );

        REQUIRE_RC ( KMDataNodeRelease ( node ) );
        REQUIRE_RC ( KMetadataRelease ( meta ) );

        CloseDatabase();
    }

    // read, validate
    OpenDatabase ();
    REQUIRE_EQ ( FormatterName,  GetMetadata ( "SOFTWARE/formatter", "name" ) );

    // extract the program name from path the same way it's done in ncbi-vdb/libs/kapp/loader-meta.c:KLoaderMeta_Write
    REQUIRE_EQ ( LoaderName, GetMetadata ( "SOFTWARE/loader", "name" ) );
    REQUIRE_EQ ( string ( __DATE__), GetMetadata ( "SOFTWARE/loader", "date" ) );
    {   // KLoaderMeta_WriteWithVersion sets versions of loader and formatter apart
        char buf[265];
        string_printf ( buf, sizeof buf, NULL, "%V", KAppVersion() );
        REQUIRE_EQ ( string ( buf ), GetMetadata ( "SOFTWARE/formatter", "vers" ) );
        string_printf ( buf, sizeof buf, NULL, "%V", LoaderVersion );
        REQUIRE_EQ ( string ( buf ), GetMetadata ( "SOFTWARE/loader", "vers" ) );
    }

    std::remove ( schemaFile . c_str() );
}

//////////////////////////////////////////// Main
#include <kfg/config.h>

int main(int argc, char* argv[])
{
    VDB::Application app(argc, argv);
    KConfigDisableUserSettings();
    return LoaderTestSuite(argc, app.getArgV());
}
