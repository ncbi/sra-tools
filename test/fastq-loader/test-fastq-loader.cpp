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
#include <loader/common-writer.h>
#include <loader/sequence-writer.h>
#include <loader/alignment-writer.h>
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
    }
    ~TempFileFixture() 
    {
        if ( rf != 0 && ReaderFileRelease( rf ) != 0)
            FAIL("ReaderFileRelease failed");
     
        if ( schema && VSchemaRelease(schema) != 0 )
            FAIL("VSchemaRelease failed");            
        if ( db && VDatabaseRelease(db) != 0 )
            FAIL("VDatabaseRelease failed");
        if ( mgr && VDBManagerRelease(mgr) != 0 )
            FAIL("VDBManagerRelease failed");
            
        if ( wd && ! filename.empty() && KDirectoryRemove(wd, true, filename.c_str()) != 0 )
            FAIL("KDirectoryRemove on input failed");

        if ( wd && ! dbName.empty() )
        {   // sometimes it takes several attempts to remove a non-empty dir
            while (KDirectoryRemove(wd, true, dbName.c_str()) != 0);
        }
        if ( wd )
        {   // sometimes it takes several attempts to remove a non-empty dir
            while (KDirectoryRemove(wd, true, TempDir.c_str()) != 0);
        }
             
        if ( wd && KDirectoryRelease ( wd ) != 0 )
            FAIL("KDirectoryRelease failed");
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
            file=0;
        }
        return FastqReaderFileMake(&rf, wd, p_filename, 33, 0, 0, false);
    }

    KDirectory* wd;
    string filename;
    const ReaderFile* rf;
    VDBManager* mgr;
    VSchema *schema;
    VDatabase* db;
    string dbName;
};
const string TempFileFixture::TempDir = "./tmp";
const string TempFileFixture::SchemaPath = "align/align.vschema";
const string TempFileFixture::DbType = "NCBI:align:db:alignment_unsorted";

///////////////////////////////////////////////// FASTQ-based tests for CommonWriter 
FIXTURE_TEST_CASE(CommonWriterOneFile, TempFileFixture)
{   // source: SRR006565
    CreateFile(GetName(), 
                "@G15-D_3_1_903_603_0.81\n"
                "GATTGTAGGGAGTAGGGTACAATACAGTCTGGTCTC\n"
                "+G15-D_3_1_903_603_0.81\n"
                "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\n");

    dbName = string(GetName())+".db";
    KDirectoryRemove(wd, true, dbName.c_str());
    REQUIRE_RC(VDBManagerCreateDB(mgr, &db, schema, DbType.c_str(), kcmInit + kcmMD5, dbName.c_str()));

    CommonWriterSettings settings;
    memset(&settings, 0, sizeof(settings));
    settings.numfiles = 1;
    settings.tmpfs = TempDir.c_str();
    
    CommonWriter cw;
    REQUIRE_RC(CommonWriterInit( &cw, mgr, db, &settings ));
     
    REQUIRE_RC(CommonWriterArchive( &cw, rf ));
    REQUIRE_RC(CommonWriterComplete( &cw, false, 0 ));

    REQUIRE_RC(CommonWriterWhack( &cw ));
    
    //TODO: open and validate database 
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
	// need to use user settings in order to get to the current schema
    //KConfigDisableUserSettings();
	
    rc_t rc = LoaderFastqTestSuite(argc, argv);
    return rc;
}

}

