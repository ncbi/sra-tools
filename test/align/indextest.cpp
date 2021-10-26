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
#include <ktst/unit_test.hpp>

#include <klib/printf.h>

#include <stdexcept> 
#include <string>

extern "C" {
#include <klib/rc.h>
#include <align/bam.h>
}

using namespace std;

TEST_SUITE(IndexTestSuite);

class LoaderFixture
{
    BAMFile const *bam;

    static std::string BAM_FILE_NAME(void) {
        return std::string("/panfs/pan1/sra-test/bam/VDB-3148.bam");
    }
    static std::string INDEX_FILE_NAME(void) {
        return BAM_FILE_NAME() + ".bai";
    }
public:
    LoaderFixture()
    {
        rc_t const rc = BAMFileMake(&bam, BAM_FILE_NAME().c_str());
        if (rc != 0)
            throw std::runtime_error("can't open " + BAM_FILE_NAME());
    }
    ~LoaderFixture()
    {
        BAMFileRelease(bam);
    }
    void testIndex(void) const {
        rc_t const expected_rc = SILENT_RC(rcAlign, rcIndex, rcReading, rcData, rcExcessive);
        rc_t const rc = BAMFileOpenIndex(bam, INDEX_FILE_NAME().c_str());
        if (rc == 0)
            throw std::runtime_error("Index open was supposed to fail");
        if (rc != expected_rc)
            throw std::runtime_error("Index open did not fail with the expected result code; perhaps the test file is missing?");
    }
};    

FIXTURE_TEST_CASE ( LoadIndex, LoaderFixture ) 
{
    testIndex();
}

//////////////////////////////////////////// Main
#include <kapp/args.h>
#include <klib/out.h>
#include <kfg/config.h>

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}

const char UsageDefaultName[] = "test-loader";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n" "\t%s [options]\n\n", progname );
}

rc_t CC Usage( const Args* args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    return IndexTestSuite(argc, argv);
}

}

