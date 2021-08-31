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
#include "../../tools/fastq-loader/common-writer.h"
#include "../../tools/fastq-loader/fastq-reader.h"
#include "../../tools/fastq-loader/fastq-parse.h"
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
    :   wd(0), rf(0)
    {
        if ( KDirectoryNativeDir ( & wd ) != 0 )
            FAIL("KDirectoryNativeDir failed");

        if ( VDBManagerMakeUpdate ( & mgr, wd ) != 0)
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
        if ( rf != 0 && ReaderFileRelease( rf ) != 0)
            FAIL("ReaderFileRelease failed");

        if ( schema && VSchemaRelease(schema) != 0 )
            FAIL("VSchemaRelease failed");

        if ( mgr && VDBManagerRelease(mgr) != 0 )
            FAIL("VDBManagerRelease failed");

        if ( wd )
        {
            if ( ! filename.empty() )
            {
               KDirectoryRemove(wd, true, filename.c_str());
            }
            if ( ! dbName.empty() )
            {
               KDirectoryRemove(wd, true, dbName.c_str());
            }
            if ( ! TempDir.empty() )
            {
               KDirectoryRemove(wd, true, TempDir.c_str());
            }
            if ( KDirectoryRelease ( wd ) != 0 )
            {
               FAIL("KDirectoryRelease failed");
            }
        }
    }
    rc_t CreateFile(const char* p_filename, const char* contents)
    {   // create and open for read
        KFile* file;
        filename=p_filename;
        rc_t rc=KDirectoryCreateFile(wd, &file, true, 0664, kcmInit, p_filename);
        if (rc == 0)
        {
            size_t num_writ=0;
            rc=KFileWrite(file, 0, contents, strlen(contents), &num_writ);
            if (rc == 0)
            {
                rc=KFileRelease(file);
            }
            else
            {
                KFileRelease(file);
            }
        }
        if ( rc == 0 )
        {
            rc = FastqReaderFileMake(&rf, wd, p_filename, FASTQphred33, 0, false);
        }
        return rc;
    }

    rc_t Load( const char* p_filename, const char* p_contents )
    {
        rc_t rc = CreateFile( p_filename, p_contents );
        if ( rc == 0 )
        {
            dbName = string(p_filename)+".db";
            KDirectoryRemove(wd, true, dbName.c_str());

            VDatabase* db;
            THROW_ON_RC(VDBManagerCreateDB(mgr, &db, schema, DbType.c_str(), kcmInit + kcmMD5, dbName.c_str()));

            CommonWriter cw;
            THROW_ON_RC(CommonWriterInit( &cw, mgr, db, &settings ));

            rc = (CommonWriterArchive( &cw, rf ));
            THROW_ON_RC(CommonWriterComplete( &cw, false, 0 ));

            THROW_ON_RC(CommonWriterWhack( &cw ));

            // close database so that it can be reopened for inspection
            THROW_ON_RC( VDatabaseRelease(db) );
        }
        return rc;
    }

    const VCursor * OpenDatabase()
    {
        const VDatabase * db;
        THROW_ON_RC ( VDBManagerOpenDBRead ( mgr, &db, NULL, "%s", dbName.c_str() ) );
        const VTable * tbl;
        THROW_ON_RC ( VDatabaseOpenTableRead ( db, & tbl, "SEQUENCE" ) );
        const VCursor * ret;
        THROW_ON_RC ( VTableCreateCursorRead ( tbl, & ret ) );
        THROW_ON_RC ( VTableRelease ( tbl ) );
        THROW_ON_RC ( VDatabaseRelease ( db ) );
        return ret;
    }

    string GetRead()
    {
        int64_t row = 1;

        const VCursor * cur = OpenDatabase();
        uint32_t  columnIdx;
        THROW_ON_RC ( VCursorAddColumn ( cur, & columnIdx, "READ" ) );
        THROW_ON_RC ( VCursorOpen ( cur ) );
        char buf[1024];
        uint32_t row_len;
        THROW_ON_RC ( VCursorReadDirect ( cur, row, columnIdx, 8, buf, sizeof ( buf ), & row_len ) );
        return string( buf, row_len );
        VCursorRelease ( cur );
    }


    KDirectory* wd;
    string filename;
    const ReaderFile* rf;
    VDBManager* mgr;
    VSchema *schema;
    string dbName;
    CommonWriterSettings settings;
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
    REQUIRE_EQ( string( "CA" ), GetRead() );
}

FIXTURE_TEST_CASE(NoSpotAssemply_case2, TempFileFixture)
{   // VDB-4532
    REQUIRE_RC( Load(GetName(),
                "@CL100050407L1C001R001_1#224_1078_917/1 1 1\nC\n+\nF\n"
                "@CL100050407L1C001R001_1#224_1078_917/2 1 1\nA\n+\nF\n"
    ) );
    REQUIRE_EQ( string( "CA" ), GetRead() );
}

//////////////////////////////////////////// Main
#include <kapp/args.h>
#include <kfg/config.h>

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}

const char UsageDefaultName[] = "test-fastq-loader";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n" "\t%s [options] -o path\n\n", progname );
}

rc_t CC Usage( const Args* args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
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
        rc=LoaderFastqTestSuite(argc, argv);

    KConfigRelease(cfg);
    KDirectoryRelease(dir);
    KDirectoryRelease(native);

    return rc;
}

}
