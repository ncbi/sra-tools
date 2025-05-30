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
* Long-running tests for ReaderFile-related interfaces
*/

#include <cstring>
#include <ctime>

#include <ktst/unit_test.hpp>

#include <klib/out.h>
#include <klib/log.h>

#include <kapp/args.h>

#include <kfs/directory.h>
#include <kfs/impl.h>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/schema.h>

extern "C" {
#include <loader/common-reader.h>
#include <loader/sequence-writer.h>
#include <loader/alignment-writer.h>
#include "../../tools/loaders/fastq-loader/common-writer.h"
#include "../../tools/loaders/fastq-loader/fastq-reader.h"
#include "../../tools/loaders/fastq-loader/fastq-parse.h"
}

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(LoaderFastqTestSuite);

///////////////////////////////////////////////// tests for loading FASTQ files

#include <kfs/directory.h>
#include <kfs/file.h>

class TempFileFixture
{
public:
    static const string TempDir;
    static const string SchemaPath;
    static const string DbType;

public:
    TempFileFixture()
    :   wd( nullptr ),
        cur( nullptr )
    {
        if ( KDirectoryNativeDir ( & wd ) != 0 )
            FAIL("KDirectoryNativeDir failed");

        if ( VDBManagerMakeUpdate ( & mgr, wd ) != 0 )
            FAIL("VDBManagerMakeUpdate failed");

        if ( VDBManagerMakeSchema ( mgr, & schema ) != 0 )
            FAIL("VDBManagerMakeSchema failed");

        if ( VSchemaParseFile( schema, SchemaPath.c_str() ) != 0 )
            FAIL("VSchemaParseFile failed");

        if ( KDirectoryCreateDir_v1 ( wd, 0775, kcmOpen | kcmInit | kcmCreate, TempDir.c_str() ) != 0 )
            FAIL("KDirectoryOpenDirUpdate_v1 failed");

        memset(&settings, 0, sizeof(settings));
        settings.numfiles = 1;
        settings.tmpfs = TempDir.c_str();
    }
    ~TempFileFixture()
    {
        VCursorRelease( cur );
        VSchemaRelease(schema);
        VDBManagerRelease(mgr);

        if ( wd != nullptr )
        {
            if ( ! filename1.empty() )
            {
               KDirectoryRemove(wd, true, filename1.c_str());
            }
            if ( ! filename2.empty() )
            {
               KDirectoryRemove(wd, true, filename2.c_str());
            }
            if ( ! dbName.empty() )
            {
               KDirectoryRemove(wd, true, dbName.c_str());
            }
            if ( ! TempDir.empty() )
            {
               KDirectoryRemove(wd, true, TempDir.c_str());
            }
            KDirectoryRelease( wd );
        }
    }

    const ReaderFile* CreateFile( const string & p_filename,
                                  const string & p_contents,
                                  int8_t defaultReadNum = 0 )
    {   // create and open for read
        {
            KFile* file;
            THROW_ON_RC( KDirectoryCreateFile( wd, &file, true, 0664, kcmInit, p_filename.c_str() ) );
            size_t num_writ=0;
            THROW_ON_RC( KFileWrite( file, 0, p_contents.c_str(), p_contents.size(), &num_writ ) );
            THROW_ON_RC( KFileRelease(file) );
        }

        const ReaderFile* ret = nullptr;
        THROW_ON_RC( FastqReaderFileMake( &ret, wd, p_filename.c_str(), FASTQphred33, defaultReadNum, false, false ) );
        return ret;
    }

    rc_t Load( const string & p_filename, const string & p_contents )
    {
        filename1 = p_filename;
        const ReaderFile* rf = CreateFile( filename1, p_contents );
        dbName = p_filename+".db";
        KDirectoryRemove(wd, true, dbName.c_str());

        VDatabase* db;
        THROW_ON_RC(VDBManagerCreateDB(mgr, &db, schema, DbType.c_str(), kcmInit + kcmMD5, dbName.c_str()));

        CommonWriter cw;
        THROW_ON_RC(CommonWriterInit( &cw, mgr, db, &settings ));

        rc_t ret = (CommonWriterArchive( &cw, rf, nullptr ));

        THROW_ON_RC(CommonWriterComplete( &cw, false, 0 ));
        THROW_ON_RC(CommonWriterWhack( &cw ));

        THROW_ON_RC( ReaderFileRelease( rf ) );

        // close database so that it can be reopened for inspection
        THROW_ON_RC( VDatabaseRelease(db) );
        return ret;
    }

    rc_t LoadInterleaved( const string & p_filename1, const string & p_contents1,
                          const string & p_filename2, const string & p_contents2 )
    {
        filename1 = p_filename1;
        const ReaderFile* rf1 = CreateFile( p_filename1, p_contents1, 1 );
        filename2 = p_filename2;
        const ReaderFile* rf2 = CreateFile( p_filename2, p_contents2, 2 );

        dbName = p_filename1+".db";
        KDirectoryRemove(wd, true, dbName.c_str());

        VDatabase* db;
        THROW_ON_RC( VDBManagerCreateDB(mgr, &db, schema, DbType.c_str(), kcmInit + kcmMD5, dbName.c_str()) );

        CommonWriter cw;
        THROW_ON_RC( CommonWriterInit( &cw, mgr, db, &settings ) );

        rc_t ret = CommonWriterArchive( &cw, rf1, rf2 );

        THROW_ON_RC( CommonWriterComplete( &cw, false, 0 ) );

        THROW_ON_RC( CommonWriterWhack( &cw ) );

        // close database so that it can be reopened for inspection
        THROW_ON_RC( VDatabaseRelease(db) );

        THROW_ON_RC( ReaderFileRelease( rf1 ) );
        THROW_ON_RC( ReaderFileRelease( rf2 ) );

        return ret;
    }

    void OpenReadCursor()
    {   // read cursor with 1 column, READ
        const VDatabase * db;
        THROW_ON_RC ( VDBManagerOpenDBRead ( mgr, &db, NULL, "%s", dbName.c_str() ) );
        const VTable * tbl;
        THROW_ON_RC ( VDatabaseOpenTableRead ( db, & tbl, "SEQUENCE" ) );

        THROW_ON_RC ( VTableCreateCursorRead ( tbl, & cur ) );
        THROW_ON_RC ( VCursorAddColumn ( cur, & columnIdx, "READ" ) );
        THROW_ON_RC ( VCursorOpen ( cur ) );

        THROW_ON_RC ( VTableRelease ( tbl ) );
        THROW_ON_RC ( VDatabaseRelease ( db ) );
    }

    string GetRead( int64_t row = 1 )
    {
        char buf[1024];
        uint32_t row_len;
        THROW_ON_RC ( VCursorReadDirect ( cur, row, columnIdx, 8, buf, sizeof ( buf ), & row_len ) );
        return string( buf, row_len );
    }

    KDirectory* wd;

    string filename1;
    string filename2;

    VDBManager* mgr;
    VSchema *schema;
    string dbName;
    CommonWriterSettings settings;

    const VCursor * cur;
    uint32_t  columnIdx;
};
const string TempFileFixture::TempDir = "./tmp";
const string TempFileFixture::SchemaPath = "align/align.vschema";
const string TempFileFixture::DbType = "NCBI:align:db:alignment_unsorted";

///////////////////////////////////////////////// FASTQ-based tests for CommonWriter
FIXTURE_TEST_CASE(CommonWriterOneFile, TempFileFixture)
{   // source: SRR006565
    REQUIRE_RC( Load(GetName(),
         "@G15-D_3_1_903_603_0.81\nGATT\n"
         "+G15-D_3_1_903_603_0.81\nIIII\n"
    ) );
    //TODO: open and validate database
}

FIXTURE_TEST_CASE(CommonWriterDuplicateReadnames_Error, TempFileFixture)
{   // VDB-4524; reported as error
    REQUIRE_RC_FAIL( Load(GetName(),
        "@HWUSI-EAS499:1:3:9:1822\nACTT\n"
        "@HWUSI-EAS499:1:3:9:1822\nACCA\n"
    ) );
}

FIXTURE_TEST_CASE(CommonWriterDuplicateReadnames_Allowed, TempFileFixture)
{   // VDB-4524; allowed with an option
    settings.allowDuplicateReadNames = true;
    REQUIRE_RC( Load(GetName(),
        "@HWUSI-EAS499:1:3:9:1822\nACTT\n"
        "@HWUSI-EAS499:1:3:9:1822\nACCA\n"
    ) );
    //TODO: open and validate database
}

FIXTURE_TEST_CASE(CommonWriterDuplicateReadnames_SameFragment, TempFileFixture)
{   // VDB-4524; reported as error
    REQUIRE_RC_FAIL( Load(GetName(),
        "@HWUSI-EAS499:1:3:9:1822/1\nACTT\n"
        "@HWUSI-EAS499:1:3:9:1822/1\nACCA\n"
    ) );
}
FIXTURE_TEST_CASE(CommonWriterDuplicateReadnames_DiffFragment, TempFileFixture)
{   // normal spot assembly
    REQUIRE_RC( Load(GetName(),
        "@HWUSI-EAS499:1:3:9:1822/1\nACTT\n"
        "@HWUSI-EAS499:1:3:9:1822/2\nACCA\n"
    ) );
    //TODO: open and validate database
}

FIXTURE_TEST_CASE(CommonWriterDuplicateReadnames_MatedThenUnmated, TempFileFixture)
{   // error
    REQUIRE_RC_FAIL( Load(GetName(),
        "@HWUSI-EAS499:1:3:9:1822/1\nACTT\n"
        "@HWUSI-EAS499:1:3:9:1822\nACCA\n"
    ) );
}

FIXTURE_TEST_CASE(CommonWriterDuplicateReadnames_UnmatedThenMated, TempFileFixture)
{   // error
    REQUIRE_RC_FAIL( Load(GetName(),
        "@HWUSI-EAS499:1:3:9:1822\nACTT\n"
        "@HWUSI-EAS499:1:3:9:1822/2\nACCA\n"
    ) );
}

//TODO: 3d fragment after assembly

FIXTURE_TEST_CASE(NoSpotAssemply, TempFileFixture)
{   // VDB-4531
    REQUIRE_RC( Load(GetName(),
                "@V300047012L3C001R0010000001/1\nC\n+\nF\n"
                "@V300047012L3C001R0010000001/2\nA\n+\nF\n"
    ) );
    OpenReadCursor();
    REQUIRE_EQ( string( "CA" ), GetRead() );
}

FIXTURE_TEST_CASE(NoSpotAssemply_case2, TempFileFixture)
{   // VDB-4532
    REQUIRE_RC( Load(GetName(),
                "@CL100050407L1C001R001_1#224_1078_917/1 1 1\nC\n+\nF\n"
                "@CL100050407L1C001R001_1#224_1078_917/2 1 1\nA\n+\nF\n"
    ) );
    OpenReadCursor();
    REQUIRE_EQ( string( "CA" ), GetRead() );
}

// VDB-4530 Interleaving input files

FIXTURE_TEST_CASE(Interleaved_OneRead, TempFileFixture)
{
    REQUIRE_RC( LoadInterleaved(
                string( GetName() ) + ".1", "@READ1\nC\n+\nF\n",
                string( GetName() ) + ".2", "@READ1\nA\n+\nF\n"
    ) );
    OpenReadCursor();
    REQUIRE_EQ( string( "CA" ), GetRead() );
}

FIXTURE_TEST_CASE(Interleaved_MultipleReads, TempFileFixture)
{
    //KLogLevelSet ( klogDebug );
    REQUIRE_RC( LoadInterleaved(
                string( GetName() ) + ".1",
                "@READ1\nA\n+\nF\n"
                "@READ2\nC\n+\nF\n"
                "@READ3\nG\n+\nF\n"
                ,
                string( GetName() ) + ".2",
                "@READ3\nA\n+\nF\n"
                "@READ2\nG\n+\nF\n"
                "@READ1\nT\n+\nF\n"
    ) );
    OpenReadCursor();
    REQUIRE_EQ( string( "CG" ), GetRead( 1 ) ); // READ2 pairs first
    REQUIRE_EQ( string( "GA" ), GetRead( 2 ) ); // READ3 next
    REQUIRE_EQ( string( "AT" ), GetRead( 3 ) ); // READ1 last
}

FIXTURE_TEST_CASE(Interleaved_NoMatch, TempFileFixture)
{   // VDB-4530
    REQUIRE_RC( LoadInterleaved(
                string( GetName() ) + ".1", "@READ1\nC\n+\nF\n",
                string( GetName() ) + ".2", "@READ2\nA\n+\nF\n"
    ) );
    OpenReadCursor();
    REQUIRE_EQ( string( "C" ), GetRead( 1 ) );
    REQUIRE_EQ( string( "A" ), GetRead( 2 ) );
}

FIXTURE_TEST_CASE(Interleaved_1_longer, TempFileFixture)
{   // VDB-4530
    REQUIRE_RC( LoadInterleaved(
                string( GetName() ) + ".1", "@READ1\nC\n+\nF\n@READ2\nT\n+\nF\n",
                string( GetName() ) + ".2", "@READ2\nA\n+\nF\n"
    ) );
    OpenReadCursor();
    REQUIRE_EQ( string( "TA" ), GetRead( 1 ) );
    REQUIRE_EQ( string( "C" ), GetRead( 2 ) );
}
FIXTURE_TEST_CASE(Interleaved_2_longer, TempFileFixture)
{   // VDB-4530
    REQUIRE_RC( LoadInterleaved(
                string( GetName() ) + ".1", "@READ1\nA\n+\nF\n",
                string( GetName() ) + ".2", "@READ2\nG\n+\nF\n@READ1\nT\n+\nF\n"
    ) );
    OpenReadCursor();
    REQUIRE_EQ( string( "AT" ), GetRead( 1 ) );
    REQUIRE_EQ( string( "G" ), GetRead( 2 ) );
}

//////////////////////////////////////////// Main
#include <kfg/config.h>

extern "C"
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();

    KDirectory * native = NULL;
    rc_t rc = KDirectoryNativeDir( &native);

    const KDirectory * dir = NULL;
    if (rc == 0)
        rc = KDirectoryOpenDirRead ( native, &dir, false, "." );
    KConfig * cfg = NULL;
    if (rc == 0)
        rc = KConfigMake ( &cfg, dir ) ;
    if (rc == 0)
        rc=(rc_t)LoaderFastqTestSuite(argc, argv);

    KConfigRelease(cfg);
    KDirectoryRelease(dir);
    KDirectoryRelease(native);

    return (int)rc;
}
