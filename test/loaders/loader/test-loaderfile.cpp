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

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <ktst/unit_test.hpp>

#include <loader/queue-file.h>
#include <loader/loader-file.h>
#include <kfs/file.h>
#define KFILE_IMPL struct KLoaderFile
#include <kfs/impl.h>

#include <klib/rc.h>
#include <kapp/args.h>
#include <kfs/directory.h>

#include <kfg/config.h>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(KAppTestSuite);

const char UsageDefaultName[] = "Test_LOADERFILE";

extern "C"
{
    rc_t CC UsageSummary ( const char *progname )
    {
        return TestEnv::UsageSummary ( progname );
    }

    rc_t CC Usage ( const Args *args )
    {
        const char* progname = UsageDefaultName;
        const char* fullpath = UsageDefaultName;

        rc_t rc = (args == NULL) ?
            RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull):
            ArgsProgram(args, &fullpath, &progname);
        if ( rc == 0 )
            rc = TestEnv::Usage ( progname );
        return rc;
    }
}

TEST_CASE(KQueueFile_ReadTimeout_FGsleeps)
{
    KDirectory *dir;
    REQUIRE_RC(KDirectoryNativeDir(&dir));

    KFile const* src;
    REQUIRE_RC(KDirectoryOpenFileRead(dir, &src, "queuefile.data"));

    const size_t BlockSize=10;
    struct KFile const* qf;
    const uint32_t timeoutSec=1;
    // create a queue of 1 short block
    REQUIRE_RC(KQueueFileMakeRead(&qf, 0, src, BlockSize*1, BlockSize, timeoutSec*1000));

    char buffer[BlockSize];
    size_t num_read;
    uint64_t pos=0;
    REQUIRE_RC(KFileRead(qf, pos, buffer, BlockSize, &num_read)); REQUIRE_EQ(num_read, BlockSize); pos+=num_read;
    // now sleep longer than the timeout and make sure the background thread does not seal the queue
    TestEnv::Sleep(timeoutSec*2);
    // read the block that has been populated while we slept:
    REQUIRE_RC(KFileRead(qf, pos, buffer, BlockSize, &num_read)); REQUIRE_EQ(num_read, BlockSize); pos+=num_read;
    // if queue has been sealed because of the timeout, the second read would return 0 bytes:
    REQUIRE_RC(KFileRead(qf, pos, buffer, BlockSize, &num_read)); REQUIRE_EQ(num_read, BlockSize); pos+=num_read;

    REQUIRE_RC(KFileRelease(qf));
    REQUIRE_RC(KFileRelease(src));
    REQUIRE_RC(KDirectoryRelease(dir));
}

//// A mock KFile object for imitating slow read on a background thread. Sends its thread to sleep after reading each block.
struct SleepyReader
{
    const static uint64_t Size=10000;
    const static int SleepSec=1;

    static rc_t MakeFileRead(KFile const** f)
    {
        KFile* ret=(KFile*)calloc(1, sizeof(KFile));
        ret-> vt = (const KFile_vt*)&vt;
        ret-> dir = NULL;
        atomic32_set ( & ret-> refcount, 1 );
        ret-> read_enabled = 1;
        ret-> write_enabled = 0;
        *f = ret;
        return 0;
    }
    static rc_t CC get_size ( const KFILE_IMPL *self, uint64_t *size )
    {
        *size=Size;
        return 0;
    }
    static rc_t CC read( const KFILE_IMPL *self, uint64_t pos, void *buffer, size_t bsize, size_t *num_read )
    {
        memset(buffer, 1, bsize);
        *num_read=bsize;
        TestEnv::Sleep(SleepSec);
        return 0;
    }
    static rc_t CC destroy( KFILE_IMPL *self )
    {
        free(self);
        return 0;
    }

    // the rest of the functions do not matter
    static struct KSysFile* CC get_sysfile ( const KFILE_IMPL *self, uint64_t *offset ) { *offset=0; return 0; }
    static rc_t CC random_access ( const KFILE_IMPL *self ) { return 0; }
    static rc_t CC set_size ( KFILE_IMPL *self, uint64_t size ) { return 0; }
    static rc_t CC write( KFILE_IMPL *self, uint64_t pos, const void *buffer, size_t size, size_t *num_writ ) { *num_writ=0; return 0; }

    static KFile_vt_v1 vt;
};
KFile_vt_v1 SleepyReader::vt=
{   1, 0,
    SleepyReader::destroy,
    SleepyReader::get_sysfile,
    SleepyReader::random_access,
    SleepyReader::get_size,
    SleepyReader::set_size,
    SleepyReader::read,
    SleepyReader::write
};

TEST_CASE(KQueueFile_ReadTimeout_BGsleeps)
{
    KFile const* src;
    REQUIRE_RC(SleepyReader::MakeFileRead(&src)); // this reader will sleep for a second after each block

    const size_t BlockSize=10;
    struct KFile const* qf;
    // create a queue of 1 short block
    REQUIRE_RC(KQueueFileMakeRead(&qf, 0, src, BlockSize*1, BlockSize, 100)); // a short timeout on the foreground thread

    char buffer[BlockSize];
    size_t num_read;
    uint64_t pos=0;
    REQUIRE_RC(KFileRead(qf, pos, buffer, BlockSize, &num_read)); REQUIRE_EQ(num_read, BlockSize); pos+=num_read;
    // the background read thread will read the bytes and go to sleep for 1 second
    // now, read the next block to make sure the foreground read thread recovers from the timeouts
    REQUIRE_RC(KFileRead(qf, pos, buffer, BlockSize, &num_read)); REQUIRE_EQ(num_read, BlockSize); pos+=num_read;

    REQUIRE_RC(KFileRelease(qf));
    REQUIRE_RC(KFileRelease(src));
}

TEST_CASE(KQueueFile_WriteTimeout)
{
    KDirectory *dir;
    REQUIRE_RC(KDirectoryNativeDir(&dir));

    const char* fileName="queuefile.temp";
    KFile *dest;
    REQUIRE_RC(KDirectoryCreateFile(dir, (KFile**)&dest, false, 0664, kcmInit, fileName));

    const size_t BlockSize=10;
    struct KFile* qf;
    const uint32_t timeoutSec=1;
    // create a queue of 1 short block
    REQUIRE_RC(KQueueFileMakeWrite(&qf, dest, BlockSize*1, BlockSize, timeoutSec*1000));

    char buffer[BlockSize*2];
    memset(buffer, 0, sizeof(buffer));
    size_t num_writ;
    uint64_t pos=0;
    REQUIRE_RC(KFileWrite(qf, pos, buffer, BlockSize, &num_writ)); REQUIRE_EQ(num_writ, BlockSize); pos+=num_writ;
    // now sleep longer than the timeout and make sure the background thread does not seal the queue
    TestEnv::Sleep(timeoutSec*2);
    // make sure we can continue writing:
    REQUIRE_RC(KFileWrite(qf, pos, buffer, BlockSize, &num_writ)); REQUIRE_EQ(num_writ, BlockSize);

    REQUIRE_RC(KFileRelease(qf));
    REQUIRE_RC(KFileRelease(dest));
    REQUIRE_RC(KDirectoryRemove(dir, true, fileName));
    REQUIRE_RC(KDirectoryRelease(dir));
}

class LoaderFileFixture
{
public:
    LoaderFileFixture()
    :   wd(0), lf(0)
    {
        if ( KDirectoryNativeDir ( & wd ) != 0 )
            FAIL("KDirectoryNativeDir failed");
    }
    ~LoaderFileFixture()
    {
        if ( lf != 0 && KLoaderFile_Release( lf, true ) != 0)
            FAIL("KLoaderFile_Release failed");

        if ( !filename.empty() && KDirectoryRemove(wd, true, filename.c_str()) != 0)
            FAIL("KDirectoryRemove failed");

        if ( KDirectoryRelease ( wd ) != 0 )
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
        return KLoaderFile_Make(&lf, wd, p_filename, 0, true);
    }
    KDirectory* wd;
    string filename;
    const KLoaderFile* lf;
};

FIXTURE_TEST_CASE(KLoaderFile_eolBeforeEof, LoaderFileFixture)
{
    string input="qqq abcd\n";
    CreateFile(GetName(), input.c_str());
    const char* buf = 0;
    size_t length = 0;
    REQUIRE_RC(KLoaderFile_Readline(lf, (const void**)&buf, &length));
    REQUIRE_NOT_NULL(buf);
    REQUIRE_EQ(input, string(buf, length + 1)); // \n is not included in length but should be in the buffer
}


FIXTURE_TEST_CASE(KLoaderFile_noEolBeforeEof, LoaderFileFixture)
{   // formerly a bug: if no \n on the last line of a file, the line was lost
    string input="qqq abcd";
    CreateFile(GetName(), input.c_str());
    const char* buf = 0;
    size_t length = 0;
    REQUIRE_RC(KLoaderFile_Readline(lf, (const void**)&buf, &length));
    REQUIRE_NOT_NULL(buf);
    REQUIRE_EQ(input, string(buf, length));
}

//////////////////////////////////////////// Main

extern "C"
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return KAppTestSuite(argc, argv);
}
