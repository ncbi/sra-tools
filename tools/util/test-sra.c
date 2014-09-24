/*==============================================================================
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
* ==============================================================================
*
*/

#include "test-sra-priv.h" /* PrintOS */

#include <kapp/main.h> /* KMain */

#include <kfg/config.h> /* KConfig */
#include <kfg/kart.h> /* Kart */
#include <kfg/repository.h> /* KRepositoryMgr */

#include <vfs/manager.h> /* VFSManager */
#include <vfs/path.h> /* VPath */
#include <vfs/resolver.h> /* VResolver */

#include <sra/sraschema.h> /* VDBManagerMakeSRASchema */

#include <vdb/database.h> /* VDatabase */
#include <vdb/dependencies.h> /* VDBDependencies */
#include <vdb/manager.h> /* VDBManager */
#include <vdb/report.h> /* ReportSetVDBManager */
#include <vdb/schema.h> /* VSchemaRelease */
#include <vdb/table.h> /* VDBManagerOpenTableRead */

#include <kns/ascp.h> /* ascp_locate */
#include <kns/http.h>
#include <kns/manager.h>
#include <kns/manager-ext.h> /* KNSManagerNewReleaseVersion */
#include <kns/stream.h>

#include <kdb/manager.h> /* kptDatabase */

#include <kfs/directory.h> /* KDirectory */
#include <kfs/file.h> /* KFile */

#include <klib/data-buffer.h> /* KDataBuffer */
#include <klib/debug.h> /* DBGMSG */
#include <klib/log.h> /* KLogHandlerSet */
#include <klib/out.h> /* KOutMsg */
#include <klib/printf.h> /* string_vprintf */
#include <klib/rc.h>
#include <klib/report.h> /* ReportForceFinalize */
#include <klib/sra-release-version.h>
#include <klib/text.h> /* String */

#include <sysalloc.h>

#include <assert.h>
#include <ctype.h> /* isprint */
#include <stdlib.h> /* calloc */
#include <string.h> /* memset */

VFS_EXTERN rc_t CC VResolverProtocols ( VResolver * self,
    VRemoteProtocols protocols );

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

typedef enum {
    eCfg = 1,
/*  eType = 2, */
    eResolve = 2,
    eDependMissing = 4,
    eDependAll = 8,
/*  eCurl = 16, */
    eVersion = 32,
    eNewVersion = 64,
    eOpenTable = 128,
    eOpenDB = 256,
    eType = 512,
    eNcbiReport = 1024,
    eOS = 2048,
    eAscp = 4096,
    eAscpVerbose = 8192,
    eAll = 16384,
    eNoTestArg = 32768
} Type;
typedef uint16_t TTest;
static const char TESTS[] = "crdDksSoOtnufFa";
typedef struct {
    KConfig *cfg;
    KDirectory *dir;
    KNSManager *knsMgr;
    const VDBManager *mgr;
    VFSManager *vMgr;
    const KRepositoryMgr *repoMgr;
    VResolver *resolver;

    TTest tests;
    bool recursive;
    bool noVDBManagerPathType;
    bool noRfs;
    bool full;
    bool xml;

    bool allowCaching;
    VResolverEnableState cacheState;
} Main;
uint32_t CC KAppVersion(void) { return 0; }

const char UsageDefaultName[] = "test-sra";

rc_t CC UsageSummary(const char *prog_name) {
    return KOutMsg(
        "Usage:\n"
        "  quick check mode:\n"
        "   %s -Q [ name... ]\n\n"
        "  full test mode:\n"
        "   %s [+crdDa] [-crdDa] [-R] [-N] [-C] [-X <type>] [options] "
                                                   "name [ name... ]\n"
        , prog_name, prog_name);
}

rc_t CC Usage(const Args *args) {
    rc_t rc2 = 0;

    const char *progname, *fullpath;
    rc_t rc = ArgsProgram(args, &fullpath, &progname);
    if (rc != 0) {
        progname = fullpath = UsageDefaultName;
    }

    rc2 = UsageSummary(progname);
    if (rc == 0 && rc2 != 0) {
        rc = rc2;
    }

    rc2 = KOutMsg("\n"
        "Test [SRA] object, resolve it, print dependencies, configuration\n\n"
        "[+tests] - add tests\n"
        "[-tests] - remove tests\n\n");
    if (rc == 0 && rc2 != 0) {
        rc = rc2;
    }
    rc2 = KOutMsg(
        "Tests:\n"
        "  s - print SRA software information\n"
        "  S - print SRA software information and last SRA toolkit versions\n"
        "  u - print operation system information\n"
        "  c - print configuration\n"
        "  n - print NCBI error report\n"
        "  f - print ascp information\n"
        "  F - print verbose ascp information\n"
        "  t - print object types\n");
//      "  k - print curl info\n"
    if (rc == 0 && rc2 != 0) {
        rc = rc2;
    }
    rc2 = KOutMsg(
        "  r - call VResolver\n"
        "  d - call ListDependencies(missing)\n"
        "  D - call ListDependencies(all)\n"
        "  o - call VDBManagerOpenTableRead(object)\n"
        "  O - call VDBManagerOpenDBRead(object)\n"
        "  a - all tests except VDBManagerOpen...Read and verbose ascp\n\n");
    if (rc == 0 && rc2 != 0) {
        rc = rc2;
    }
    rc2 = KOutMsg(
        "In quick check mode - the base checks are run;\n"
        "in full test mode (default) all the tests are available.\n\n"
        "In full mode, if no tests were specified then all tests will be run.\n"
        "\n"
        "-X < xml | text > - whether to generate well-formed XML\n"
        "-R - check objects recursively\n"
        "-N - do not call VDBManagerPathType\n"
        "-C - do not disable caching (default: from configuration)\n\n"
        "More options:\n");
    if (rc == 0 && rc2 != 0) {
        rc = rc2;
    }

    HelpOptionsStandard();

    return rc;
}

static bool testArg(const char *arg, TTest *testOn, TTest *testOff) {
    int j = 1;
    TTest *res = NULL;

    assert(arg && testOn && testOff);
    if (arg[0] != '+' && arg[0] != '-') {
        return false;
    }

    if (arg[0] == '-' &&
        arg[1] != '\0' && strchr(TESTS, arg[1]) == NULL)
    {
        return false;
    }

    res = arg[0] == '-' ? testOff : testOn;

    for (j = 1; arg[j] != '\0'; ++j) {
        char *c = strchr(TESTS, arg[j]);
        if (c != NULL) {
            int64_t offset = c - TESTS;
            *res |= 1 << offset;
        }
    }

    return true;
}

static TTest Turn(TTest in, TTest tests, bool on) {
    TTest c = 1;
    for (c = 1; c < eAll; c <<= 1) {
        if (tests & c) {
            if (on) {
                in |= c;
            }
            else {
                in &= ~c;
            }
        }
    }
    return in;
}

static TTest processTests(TTest testsOn, TTest testsOff) {
    TTest tests = 0;

    bool allOn = false;
    bool allOff = false;

    if (testsOn & eAll && testsOff & eAll) {
        testsOn &= ~eAll;
        testsOff &= ~eAll;
    }
    else if (testsOn & eAll) {
        allOn = true;
    }
    else if (testsOff & eAll) {
        allOff = true;
    }

    if (allOn) {
        tests = ~0;
        tests = Turn(tests, testsOff, false);
        tests = Turn(tests, eOpenTable, testsOn & eOpenTable);
        tests = Turn(tests, eOpenDB, testsOn & eOpenDB);
        tests = Turn(tests, eOpenDB, testsOn & eAscpVerbose);
    }
    else if (allOff) {
        tests = Turn(tests, testsOn, true);
    }
    else if (testsOn != 0 || testsOff != 0) {
        tests = Turn(tests, testsOff, false);
        tests = Turn(tests, testsOn, true);
    }
    else /* no test argument provided */ {
        tests = ~0;
        tests = Turn(tests, eOpenTable, false);
        tests = Turn(tests, eOpenDB, false);
        tests = Turn(tests, eAscpVerbose, false);
    }

    if (tests & eAscpVerbose) {
        tests = Turn(tests, eAscp, true);
    }

    if (tests & eNcbiReport) {
        tests = Turn(tests, eCfg, false);
    }

    return tests;
} 

static void MainMakeQuick(Main *self) {
    assert(self);
#if 0
    self->tests = eCurl;
#endif
}

static void MainAddTest(Main *self, Type type) {
    assert(self);
    self->tests |= type;
}

static bool MainHasTest(const Main *self, Type type) {
    return (self->tests & type) != 0;
}

static void MainPrint(const Main *self) {
    return;

    assert(self);

    if (MainHasTest(self, eCfg)) {
        KOutMsg("eCfg\n");
    }

    if (MainHasTest(self, eResolve)) {
        KOutMsg("eResolve\n");
    }

    if (MainHasTest(self, eDependMissing)) {
        KOutMsg("eDependMissing\n");
    }

    if (MainHasTest(self, eDependAll)) {
        KOutMsg("eDependAll\n");
    }

#if 0
    if (MainHasTest(self, eCurl)) {
        KOutMsg("eCurl\n");
    }
#endif

    if (MainHasTest(self, eVersion)) {
        KOutMsg("eVersion\n");
    }

    if (MainHasTest(self, eNewVersion)) {
        KOutMsg("eNewVersion\n");
    }

    if (MainHasTest(self, eOpenTable)) {
        KOutMsg("eOpenTable\n");
    }

    if (MainHasTest(self, eOpenDB)) {
        KOutMsg("eOpenDB\n");
    }

    if (MainHasTest(self, eType)) {
        KOutMsg("eType\n");
    }

    if (MainHasTest(self, eNcbiReport)) {
        KOutMsg("eNcbiReport\n");
    }

    if (MainHasTest(self, eAll)) {
        KOutMsg("eAll\n");
    }

    if (MainHasTest(self, eNoTestArg)) {
        KOutMsg("eNoTestArg\n");
    }
}

static rc_t MainInitObjects(Main *self) {
    rc_t rc = 0;

    VResolver *resolver = NULL;

    if (rc == 0) {
        rc = KDirectoryNativeDir(&self->dir);
    }

    if (rc == 0) {
        rc = VDBManagerMakeRead(&self->mgr, NULL);
        ReportSetVDBManager(self->mgr);
    }

    if (rc == 0) {
        rc = KConfigMake(&self->cfg, NULL);
    }

    if (rc == 0) {
        rc = KConfigMakeRepositoryMgrRead(self->cfg, &self->repoMgr);
    }

    if (rc == 0) {
        rc = VFSManagerMake(&self->vMgr);
    }

    if (rc == 0) {
        rc = VFSManagerGetResolver(self->vMgr, &resolver);
    }

    if (rc == 0) {
        rc = VFSManagerGetKNSMgr(self->vMgr, &self->knsMgr);
    }

    if (rc == 0) {
        if (!self->allowCaching) {
            self->cacheState = VResolverCacheEnable(resolver, vrAlwaysDisable);
        }
    }

    if (rc == 0) {
        rc = VFSManagerMakeResolver(self->vMgr, &self->resolver, self->cfg);
    }


    RELEASE(VResolver, resolver);

    return rc;
}

static
void _MainInit(Main *self, int argc, char *argv[], int *argi, char **argv2)
{
    int i = 0;

    bool hasTestArg = false;

    TTest testsOn = 0;
    TTest testsOff = 0;

    assert(self && argv && argi && argv2);

    *argi = 0;
    argv2[(*argi)++] = argv[0];

    for (i = 1; i < argc; ++i) {
        if (!testArg(argv[i], &testsOn, &testsOff)) {
            argv2[(*argi)++] = argv[i];
        }
        else {
            hasTestArg = true;
        }
    }

    self->tests = processTests(testsOn, testsOff);

    if (hasTestArg) {
        self->tests &= ~eNoTestArg;
    }
    else {
        self->tests |= eNoTestArg;
    }

    MainPrint(self);
}

static char** MainInit(Main *self, int argc, char *argv[], int *argi) {
    char **argv2 = calloc(argc, sizeof *argv2);

    assert(self);

    memset(self, 0, sizeof *self);

    if (argv2 != NULL) {
        _MainInit(self, argc, argv, argi, argv2);
    }

    return argv2;
}

static rc_t MainCallCgiImpl(const Main *self,
    const KConfigNode *node, const char *acc)
{
    rc_t rc = 0;
    String *url = NULL;
    KHttpRequest *req = NULL;
    KDataBuffer result;
    memset(&result, 0, sizeof result);
    assert(self && node && acc);
    if (rc == 0) {
        rc = KConfigNodeReadString(node, &url);
        if (url == NULL) {
            rc = RC(rcExe, rcNode, rcReading, rcString, rcNull);
        }
    }
    if (rc == 0) {
        rc = KNSManagerMakeRequest(self->knsMgr,
            &req, 0x01000000, NULL, url->addr);
    }
    if (rc == 0) {
        rc = KHttpRequestAddPostParam ( req, "acc=%s", acc );
    }
    if (rc == 0) {
        KHttpResult *rslt;
        rc = KHttpRequestPOST ( req, & rslt );
        if ( rc == 0 )
        {
            uint32_t code;
            size_t msg_size;
            char msg_buff [ 256 ];
            rc = KHttpResultStatus(rslt,
                & code, msg_buff, sizeof msg_buff, & msg_size);
            if ( rc == 0 && code == 200 )
            {
                KStream * response;
                rc = KHttpResultGetInputStream ( rslt, & response );
                if ( rc == 0 )
                {
                    size_t num_read;
                    size_t total = 0;
                    KDataBufferMakeBytes ( & result, 4096 );
                    while ( 1 )
                    {
                        uint8_t *base;
                        uint64_t avail = result . elem_count - total;
                        if ( avail < 256 )
                        {
                            rc = KDataBufferResize ( & result, result . elem_count + 4096 );
                            if ( rc != 0 )
                                break;
                        }

                        base = result . base;
                        rc = KStreamRead ( response, & base [ total ], result . elem_count - total, & num_read );
                        if ( rc != 0 )
                        {
                            if ( num_read > 0 )
                                rc = 0;
                            else
                                break;
                        }

                        if ( num_read == 0 )
                            break;

                        total += num_read;
                    }

                    KStreamRelease ( response );
                }
            }

            KHttpResultRelease ( rslt );
        }
    }
    if (rc == 0) {
        const char *start = (const void*)(result.base);
        size_t size = KDataBufferBytes(&result);
        if (*(start + size) != '\0') {
            rc = RC(rcExe, rcString, rcParsing, rcString, rcUnexpected);
        }
        if (strstr(start, "200|ok") == NULL) {
            rc = RC(rcExe, rcString, rcParsing, rcParam, rcIncorrect);
        }
    }
    KDataBufferWhack(&result);
    RELEASE(KHttpRequest, req);
    free(url);
    if (rc == 0) {
        OUTMSG(("NCBI access: ok\n"));
    }
    else {
        OUTMSG(("ERROR: cannot access NCBI Website\n"));
    }
    return rc;
}

static rc_t MainCallCgi(const Main *self,
    const KConfigNode *node, const char *acc)
{
    rc_t rc = 0;
    int i = 0, retryOnFailure = 2;
    for (i = 0; i < retryOnFailure; ++i) {
        rc = MainCallCgiImpl(self, node, acc);
        if (rc == 0) {
            break;
        }
        DBGMSG(DBG_KNS, DBG_FLAG(DBG_KNS_ERR), (
            "@@@@@@@@2: MainCallCgi %d/%d = %R\n", i + 1, retryOnFailure, rc));
    }
    return rc;
}

#define rcResolver   rcTree
static rc_t MainQuickResolveQuery(const Main *self, const char *acc) {
    rc_t rc = 0;
    VPath *query = NULL;
    const VPath *remote = NULL;
    const VPath *cache = NULL;
    assert(self && acc);
    rc = VFSManagerMakePath(self->vMgr, &query, "%s", acc);
    if (rc == 0) {
        if (!self->allowCaching) {
            VResolverCacheEnable(self->resolver, self->cacheState);
        }
        rc = VResolverQuery(self->resolver, eProtocolHttp, query,
            NULL, &remote, &cache);
        if (!self->allowCaching) {
            VResolverCacheEnable(self->resolver, vrAlwaysDisable);
        }
    }
    if (rc == 0) {
        if (remote != NULL) {
            OUTMSG(("remote location: ok\n"));
        }
        else {
            OUTMSG(("ERROR: cannot resolve remote location\n"));
            rc = RC(rcExe, rcResolver, rcResolving, rcPath, rcNotFound);
        }
        if (cache != NULL) {
            OUTMSG(("cache location: ok\n"));
        }
        else {
            OUTMSG(("ERROR: cannot resolve cache location\n"));
            rc = RC(rcExe, rcResolver, rcResolving, rcPath, rcNotFound);
        }
    }
    else {
        OUTMSG(("ERROR: cannot resolve public accession\n"));
    }
    RELEASE(VPath, cache);
    RELEASE(VPath, remote);
    RELEASE(VPath, query);
    return rc;
}

static rc_t MainQuickCheck(const Main *self) {
    rc_t rc = 0;
    rc_t rc2 = 0;
    const char acc[] = "SRR000001";
    const char path[] = "/repository/remote/protected/CGI/resolver-cgi";
    const KConfigNode *node = NULL;
    assert(self);
    rc = KConfigOpenNodeRead(self->cfg, &node, "%s", path);
    if (rc == 0) {
        OUTMSG(("configuration: found\n"));
    }
    else {
        OUTMSG(("ERROR: configuration not found or incomplete\n"));
    }
    if (rc == 0) {
        rc_t rc3 = MainCallCgi(self, node, acc);
        if (rc3 != 0 && rc2 == 0) {
            rc2 = rc3;
        }

        rc3 = MainQuickResolveQuery(self, acc);
        if (rc3 != 0 && rc2 == 0) {
            rc2 = rc3;
        }
    }
    RELEASE(KConfigNode, node);
    if (rc2 != 0 && rc == 0) {
        rc = rc2;
    }
    return rc;
}

static rc_t MainPrintConfig(const Main *self) {
    rc_t rc = 0;

    assert(self);

    if (rc == 0) {
        rc = KConfigPrint(self->cfg, 0);
        if (rc != 0) {
            OUTMSG(("KConfigPrint() = %R", rc));
        }
        OUTMSG(("\n"));
    }

    return rc;
}

static
rc_t _KDBPathTypePrint(const char *head, KPathType type, const char *tail)
{
    rc_t rc = 0;
    rc_t rc2 = 0;
    assert(head && tail);
    {
        rc_t rc2 = OUTMSG(("%s", head));
        if (rc == 0 && rc2 != 0) {
            rc = rc2;
        }
    }
    switch (type) {
        case kptNotFound:
            rc2 = OUTMSG(("NotFound"));
            break;
        case kptBadPath:
            rc2 = OUTMSG(("BadPath"));
            break;
        case kptFile:
            rc2 = OUTMSG(("File"));
            break;
        case kptDir:
            rc2 = OUTMSG(("Dir"));
            break;
        case kptCharDev:
            rc2 = OUTMSG(("CharDev"));
            break;
        case kptBlockDev:
            rc2 = OUTMSG(("BlockDev"));
            break;
        case kptFIFO:
            rc2 = OUTMSG(("FIFO"));
            break;
        case kptZombieFile:
            rc2 = OUTMSG(("ZombieFile"));
            break;
        case kptDataset:
            rc2 = OUTMSG(("Dataset"));
            break;
        case kptDatatype:
            rc2 = OUTMSG(("Datatype"));
            break;
        case kptDatabase:
            rc2 = OUTMSG(("Database"));
            break;
        case kptTable:
            rc2 = OUTMSG(("Table"));
            break;
        case kptIndex:
            rc2 = OUTMSG(("Index"));
            break;
        case kptColumn:
            rc2 = OUTMSG(("Column"));
            break;
        case kptMetadata:
            rc2 = OUTMSG(("Metadata"));
            break;
        case kptPrereleaseTbl:
            rc2 = OUTMSG(("PrereleaseTbl"));
            break;
        default:
            rc2 = OUTMSG(("unexpectedFileType(%d)", type));
            assert(0);
            break;
    }
    if (rc == 0 && rc2 != 0) {
        rc = rc2;
    }
    {
        rc_t rc2 = OUTMSG(("%s", tail));
        if (rc == 0 && rc2 != 0) {
            rc = rc2;
        }
    }
    return rc;
}

static bool isprintString(const unsigned char *s) {
    assert(s);

    while (*s) {
        int c = *(s++);
        if (!isprint(c)) {
            return false;
        }
    }

    return true;
}

static rc_t printString(const char *s) {
    rc_t rc = 0;

    const unsigned char *u = (unsigned char*)s;

    assert(u);

    if (isprintString(u)) {
        return OUTMSG(("%s", u));
    }

    while (*u) {
        rc_t rc2 = 0;
        int c = *(u++);
        if (isprint(c)) {
            rc2 = OUTMSG(("%c", c));
        }
        else {
            rc2 = OUTMSG(("\\%03o", c));
        }
        if (rc == 0 && rc2 != 0) {
            rc = rc2;
        }
    }

    return rc;
}

static rc_t _KDirectoryReport(const KDirectory *self,
    const char *name, int64_t *size, KPathType *type, bool *alias)
{
    rc_t rc = 0;
    const KFile *f = NULL;

    bool dummyB = false;
    int64_t dummy = 0;

    KPathType dummyT = kptNotFound;;
    if (type == NULL) {
        type = &dummyT;
    }

    if (alias == NULL) {
        alias = &dummyB;
    }
    if (size == NULL) {
        size = &dummy;
    }

    *type = KDirectoryPathType(self, "%s", name);

    if (*type & kptAlias) {
        OUTMSG(("alias|"));
        *type &= ~kptAlias;
        *alias = true;
    }

    rc = _KDBPathTypePrint("", *type, " ");

    if (*type == kptFile) {
        rc = KDirectoryOpenFileRead(self, &f, "%s", name);
        if (rc != 0) {
            OUTMSG(("KDirectoryOpenFileRead("));
            printString(name);
            OUTMSG((")=%R ", rc));
        }
        else {
            uint64_t sz = 0;
            rc = KFileSize(f, &sz);
            if (rc != 0) {
                OUTMSG(("KFileSize(%s)=%R ", name, rc));
            }
            else {
                OUTMSG(("%,lu ", sz));
                *size = sz;
            }
        }
    }
    else {
        OUTMSG(("- "));
    }

    RELEASE(KFile, f);

    return rc;
}

static rc_t _VDBManagerReport(const VDBManager *self,
    const char *name, KPathType *type)
{
    KPathType dummy = kptNotFound;;

    if (type == NULL) {
        type = &dummy;
    }

    *type = VDBManagerPathType(self, "%s", name);

    *type &= ~kptAlias;

    return _KDBPathTypePrint("", *type, " ");
}

static
rc_t _KDirectoryFileHeaderReport(const KDirectory *self, const char *path)
{
    rc_t rc = 0;
    char hdr[8] = "";
    const KFile *f = NULL;
    size_t num_read = 0;
    size_t i = 0;

    assert(self && path);

    rc = KDirectoryOpenFileRead(self, &f, "%s", path);
    if (rc != 0) {
        OUTMSG(("KDirectoryOpenFileRead(%s) = %R\n", path, rc));
        return rc;
    }

    rc = KFileReadAll(f, 0, hdr, sizeof hdr, &num_read);
    if (rc != 0) {
        OUTMSG(("KFileReadAll(%s, 8) = %R\n", path, rc));
    }

    for (i = 0; i < num_read && rc == 0; ++i) {
        if (isprint(hdr[i])) {
            OUTMSG(("%c", hdr[i]));
        }
        else {
            OUTMSG(("\\X%02X", hdr[i]));
        }
    }
    OUTMSG((" "));

    RELEASE(KFile, f);
    return rc;
}

static rc_t MainReport(const Main *self,
    const char *name, int64_t *size, KPathType *type, bool *alias)
{
    rc_t rc = 0;

    assert(self);

    rc = _KDirectoryReport(self->dir, name, size, type, alias);

    if (!self->noVDBManagerPathType) {
        _VDBManagerReport(self->mgr, name, type);
    }

    if (type != NULL && *type == kptFile) {
        _KDirectoryFileHeaderReport(self->dir, name);
    }

    return rc;
}

static rc_t MainOpenAs(const Main *self, const char *name, bool isDb) {
    rc_t rc = 0;
    const VTable *tbl = NULL;
    const VDatabase *db = NULL;
    VSchema *schema = NULL;
    assert(self);

    rc = VDBManagerMakeSRASchema(self->mgr, &schema);
    if (rc != 0) {
        OUTMSG(("VDBManagerMakeSRASchema() = %R\n", rc));
    }

    if (isDb) {
        rc = VDBManagerOpenDBRead(self->mgr, &db, schema, "%s", name);
        ReportResetDatabase(name, db);
        OUTMSG(("VDBManagerOpenDBRead(%s) = ", name));
    }
    else {
        rc = VDBManagerOpenTableRead(self->mgr, &tbl, schema, "%s", name);
        ReportResetTable(name, tbl);
        OUTMSG(("VDBManagerOpenTableRead(%s) = ", name));
    }
    if (rc == 0) {
        OUTMSG(("OK\n"));
    }
    else {
        OUTMSG(("%R\n", rc));
    }
    RELEASE(VDatabase, db);
    RELEASE(VTable, tbl);
    RELEASE(VSchema, schema);
    return rc;
}

#define rcResolver   rcTree
static bool NotFoundByResolver(rc_t rc) {
    if (GetRCModule(rc) == rcVFS) {
        if (GetRCTarget(rc) == rcResolver) {
            if (GetRCContext(rc) == rcResolving) {
                if (GetRCState(rc) == rcNotFound) {
                    return true;
                }
            }
        }
    }
    return false;
}

typedef enum {
      ePathLocal
    , ePathRemote
    , ePathCache
} EPathType;
static rc_t MainPathReport(const Main *self, rc_t rc, const VPath *path,
    EPathType type, const char *name, const VPath* remote, int64_t *size,
    bool fasp, const KFile *fRemote)
{
    const char *eol = "\n";

    assert(self);

    eol = self->xml ? "<br/>\n" : "\n";

    switch (type) {
        case ePathLocal:
            OUTMSG(("Local:\t\t  "));
            break;
        case ePathRemote:
            OUTMSG(("Remote %s:\t  ", fasp ? "fasp" : "http"));
            break;
        case ePathCache:
            OUTMSG(("Cache %s:\t  ", fasp ? "fasp" : "http"));
            if (remote == NULL) {
                OUTMSG(("skipped%s", eol));
                return rc;
            }
            break;
    }
    if (rc != 0) {
        if (NotFoundByResolver(rc)) {
            OUTMSG(("not found%s", eol));
            rc = 0;
        }
        else {
            switch (type) {
                case ePathLocal:
                    OUTMSG(("VResolverLocal(%s) = %R%s", name, rc, eol));
                    break;
                case ePathRemote:
                    OUTMSG(("VResolverRemote(%s) = %R%s", name, rc, eol));
                    break;
                case ePathCache:
                    OUTMSG(("VResolverCache(%s) = %R%s", name, rc, eol));
                    break;
            }
        }
    }
    else {
        const char ncbiFile[] = "ncbi-file:";
        size_t sz = sizeof ncbiFile - 1;
        const String *s = NULL;
        char buffer[PATH_MAX] = "";
        const char *fPath = buffer;
        rc_t rc = VPathMakeString(path, &s);
        if (rc != 0 || s == NULL || s->addr == NULL ||
            string_cmp(s->addr, sz, ncbiFile, sz, (uint32_t)sz) == 0)
        {
            rc = VPathReadPath(path, buffer, sizeof buffer, NULL);
        }
        else {
            fPath = s->addr;
        }
        if (rc == 0) {
            OUTMSG(("%s ", fPath));
            switch (type) {
                case ePathLocal:
                case ePathCache:
                    rc = MainReport(self, fPath, size, NULL, NULL);
                    break;
                case ePathRemote: {
                    uint64_t sz = 0;
                    if (!fasp && fRemote != NULL) {
                        rc = KFileSize(fRemote, &sz);
                        if (rc != 0) {
                            OUTMSG(("KFileSize(%s)=%R ", name, rc));
                        }
                        else {
                            OUTMSG(("%,lu ", sz));
                            *size = sz;
                        }
                    }
                    break;
                }
            }
            OUTMSG(("%s", eol));
        }
        else {
            const char *s = "";
            switch (type) {
                case ePathLocal:
                    s = "Local";
                    break;
                case ePathCache:
                    s = "Cache";
                    break;
                case ePathRemote:
                    s = "Remote";
                    break;
                default:
                    assert(0);
                    break;
            }
            OUTMSG(("VPathMakeUri(VResolver%s(%s)) = %R%s", s, name, rc, eol));
        }
        if (type == ePathCache) {
            char cachecache[PATH_MAX] = "";
            if (rc == 0) {
                rc = string_printf(cachecache,
                    sizeof cachecache, NULL, "%s.cache", fPath);
                if (rc != 0) {
                    OUTMSG(("string_printf(%s) = %R%s", fPath, rc, eol));
                }
            }

            if (rc == 0) {
                OUTMSG(("Cache.cache %s: ", fasp ? "fasp" : "http"));
                OUTMSG(("%s ", cachecache));
                rc = MainReport(self, cachecache, NULL, NULL, NULL);
                OUTMSG(("%s", eol));
            }
        }
        free((void*)s);
    }
    return rc;
}

static rc_t MainResolveLocal(const Main *self, const VResolver *resolver,
    const char *name, const VPath *acc, int64_t *size)
{
    rc_t rc = 0;

    const VPath* local = NULL;

    assert(self);

    if (resolver == NULL) {
        resolver = self->resolver;
    }
/*
    OUTMSG(("Local: "));*/

    rc = VResolverLocal(resolver, acc, &local);
    rc = MainPathReport(self,
        rc, local, ePathLocal, name, NULL, NULL, false, NULL);

    RELEASE(VPath, local);

    return rc;
}

static rc_t MainResolveRemote(const Main *self, VResolver *resolver,
    const char *name, const VPath* acc, const VPath **remote, int64_t *size,
    bool fasp)
{
    rc_t rc = 0;

    const KFile* f = NULL;

    assert(self && size && remote);

    if (resolver == NULL) {
        resolver = self->resolver;
    }

    rc = VResolverRemote(resolver,
        fasp ? eProtocolFaspHttp : eProtocolHttp, acc, remote);

    if (rc == 0) {
        rc_t rc = 0;
        String str;
        memset(&str, 0, sizeof str);
        rc = VPathGetScheme(*remote, &str);
        if (rc != 0) {
            OUTMSG(("VPathGetScheme(%S) = %R\n", *remote, rc));
        }
        else {
            String fasp;
            CONST_STRING(&fasp, "fasp");
            if (StringCompare(&str, &fasp) != 0) {
                char path_str[8192];
                rc = VPathReadUri(*remote, path_str, sizeof path_str, NULL);
                if (rc != 0) {
                    OUTMSG(("VPathReadUri(%S) = %R\n", *remote, rc));
                }
                else {
                    rc = KNSManagerMakeHttpFile
                        (self->knsMgr, &f, NULL, 0x01010000, path_str);
                }
            }
        }
    }
        
    rc = MainPathReport(self,
        rc, *remote, ePathRemote, name, NULL, size, fasp, f);
    RELEASE(KFile, f);

    return rc;
}

static rc_t MainResolveCache(const Main *self, const VResolver *resolver,
    const char *name, const VPath* remote, bool fasp)
{
    rc_t rc = 0;
    const VPath* cache = NULL;

    assert(self);
    
    if (remote == NULL) {
        rc = MainPathReport(self,
            rc, cache, ePathCache, name, remote, NULL, fasp, NULL);
    }
    else {
        if (resolver == NULL) {
            resolver = self->resolver;
        }

        if (!self->allowCaching) {
            VResolverCacheEnable(resolver, self->cacheState);
        }
        rc = VResolverQuery(resolver, fasp ? eProtocolFasp : eProtocolHttp,
            remote, NULL, NULL, &cache);
        rc = MainPathReport(self,
            rc, cache, ePathCache, name, remote, NULL, fasp, NULL);

        RELEASE(VPath, cache);
        if (!self->allowCaching) {
            VResolverCacheEnable(resolver, vrAlwaysDisable);
        }
    }

    return rc;
}

typedef enum {
      eQueryAll
    , eQueryLocal
    , eQueryRemote
} EQueryType ;
static rc_t VResolverQueryByType(const Main *self, const VResolver *resolver,
    const char *name, const VPath *query,
    bool fasp, VRemoteProtocols protocols, EQueryType type)
{
    const char *eol = "\n";

    rc_t rc = 0;

    const VPath *local = NULL;
    const VPath *remote = NULL;
    const VPath *cache = NULL;
    const VPath **pLocal = NULL;
    const VPath **pRemote = NULL;
    const VPath **pCache = NULL;

    assert(self);

    eol = self->xml ? "<br/>\n" : "\n";

    switch (type) {
        case eQueryLocal:
        case eQueryAll:
            pLocal = &local;
        default:
            break;
    }

    switch (type) {
        case eQueryRemote:
        case eQueryAll:
            pRemote = &remote;
            pCache = &cache;
        default:
            break;
    }

    if (pCache != NULL && !self->allowCaching) {
        VResolverCacheEnable(resolver, vrAlwaysEnable);
    }

    rc = VResolverQuery(resolver, protocols, query, pLocal, pRemote, pCache);
    OUTMSG(("\nVResolverQuery(%s, %s, local%s, remote%s, cache%s)= %R%s",
        name, protocols == eProtocolHttp ? "Http" : "FaspHttp", 
        pLocal == NULL ? "=NULL" : "", pRemote == NULL ? "=NULL" : "",
        pCache == NULL ? "=NULL" : "", rc, eol));
    if (rc == 0) {
        if (local != NULL) {
/*          rc2 =*/ MainPathReport(self,
                rc, local, ePathLocal, name, NULL, NULL, false, NULL);
        }
        if (remote != NULL) {
/*          rc2 =*/ MainPathReport(self,
                rc, remote, ePathRemote, name, NULL, NULL, fasp, NULL);
        }
        if (cache != NULL) {
/*          rc2 =*/ MainPathReport(self,
                rc, cache, ePathCache, name, remote, NULL, fasp, NULL);
        }
    }

    RELEASE(VPath, local);
    RELEASE(VPath, remote);
    RELEASE(VPath, cache);

    if (pCache != NULL && !self->allowCaching) {
        VResolverCacheEnable(resolver, self->cacheState);
    }

    return rc;
}

static rc_t MainResolveQuery(const Main *self, const VResolver *resolver,
    const char *name, const VPath *query, bool fasp)
{
    rc_t rc = 0;
    rc_t rc2 = 0;

    VRemoteProtocols protocols = eProtocolHttp;
    if (fasp) {
        protocols = eProtocolFaspHttp;
    }
    else {
        protocols = eProtocolHttp;
    }

    if (resolver == NULL) {
        resolver = self->resolver;
    }

    rc2 = VResolverQueryByType(self, resolver, name, query, fasp, protocols,
        eQueryAll);
    if (rc2 != 0 && rc == 0) {
        rc = rc2;
    }

    rc2 = VResolverQueryByType(self, resolver, name, query, fasp, protocols,
        eQueryLocal);
    if (rc2 != 0 && rc == 0) {
        rc = rc2;
    }

    rc2 = VResolverQueryByType(self, resolver, name, query, fasp, protocols,
        eQueryRemote);
    if (rc2 != 0 && rc == 0) {
        rc = rc2;
    }

    return rc;
}

static rc_t MainResolve(const Main *self, const KartItem *item,
    const char *name, int64_t *localSz, int64_t *remoteSz, bool refseqCtx)
{
    const char root[] = "Resolve";
    rc_t rc = 0;

    VPath* acc = NULL;
    VResolver* resolver = NULL;

    assert(self);

    if (self->xml) {
        OUTMSG(("<%s>\n", root));
    }

    if (rc == 0) {
        if (item == NULL) {
            if (refseqCtx) {
                rc = VFSManagerMakePath(self->vMgr, &acc,
                    "ncbi-acc:%s?vdb-ctx=refseq", name);
            }
            else {
                rc = VFSManagerMakePath(self->vMgr, &acc, "%s", name);
            }
            if (rc != 0) {
                OUTMSG(("VFSManagerMakePath(%s) = %R\n", name, rc));
            }
        }
        else {
            const KRepository *p_protected = NULL;
            uint64_t oid = 0;
            uint64_t project = 0;
            if (rc == 0) {
                rc = KartItemProjIdNumber(item, &project);
                if (rc != 0) {
                    OUTMSG(("KartItemProjectIdNumber = %R\n", rc));
                }
            }
            if (rc == 0) {
                rc = KartItemItemIdNumber(item, &oid);
                if (rc != 0) {
                    OUTMSG(("KartItemItemIdNumber = %R\n", rc));
                }
            }
            if (rc == 0) {
                rc = VFSManagerMakeOidPath(self->vMgr, &acc, (uint32_t)oid);
                if (rc != 0) {
                    OUTMSG(("VFSManagerMakePath(%d) = %R\n", oid, rc));
                }
            }
            if (rc == 0) {
                rc = KRepositoryMgrGetProtectedRepository(self->repoMgr, 
                    (uint32_t)project, &p_protected);
                if (rc != 0) {
                    OUTMSG((
                        "KRepositoryMgrGetProtectedRepository(%d) = %R\n",
                        project, rc));
                }
            }
            if (rc == 0) {
                rc = KRepositoryMakeResolver(p_protected, &resolver,
                    self->cfg);
                if (rc != 0) {
                    OUTMSG((
                        "KRepositoryMakeResolver(%d) = %R\n", project, rc));
                }
            }
            RELEASE(KRepository, p_protected);
        }
    }

    if (rc == 0) {
        const VPath* remote = NULL;

        rc_t rc2 = MainResolveLocal(self, resolver, name, acc, localSz);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }

        rc2 = MainResolveRemote(self, resolver, name, acc, &remote, remoteSz,
            false);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }

        rc2 = MainResolveCache(self, resolver, name, remote, false);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }

        rc2 = MainResolveRemote(self, resolver, name, acc, &remote, remoteSz,
            true);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }

        rc2 = MainResolveCache(self, resolver, name, remote, true);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }

        rc2 = MainResolveQuery(self, resolver, name, acc, false);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }

        rc2 = MainResolveQuery(self, resolver, name, acc, true);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }

        RELEASE(VPath, remote);
    }

    RELEASE(VPath, acc); 
    RELEASE(VResolver, resolver);

    if (self->xml) {
        OUTMSG(("</%s>\n", root));
    }

    return rc;
}

static
rc_t MainDepend(const Main *self, const char *name, bool missing)
{
    const char root[] = "Dependencies";
    const char *eol = "\n";
    rc_t rc = 0;

    const VDatabase *db = NULL;
    const VDBDependencies* dep = NULL;
    uint32_t count = 0;

    assert(self);
    eol = self->xml ? "<br/>\n" : "\n";

    if (self->xml) {
        OUTMSG(("<%s>\n", root));
    }

    if (rc == 0) {
        rc = VDBManagerOpenDBRead(self->mgr, &db, NULL, "%s", name);
        if (rc != 0) {
            if (rc == SILENT_RC(rcVFS,rcMgr,rcOpening,rcDirectory,rcNotFound)) {
                return 0;
            }
            OUTMSG(("VDBManagerOpenDBRead(%s) = %R\n", name, rc));
        }
    }

    if (rc == 0) {
        if (!self->allowCaching) {
            VResolverCacheEnable(self->resolver, self->cacheState);
        }
        rc = VDatabaseListDependenciesWithCaching(db,
            &dep, missing, !self->allowCaching);
        if (rc != 0) {
            OUTMSG(("VDatabaseListDependencies(%s, %s) = %R%s",
                name, missing ? "missing" : "all", rc, eol));
        }
        if (!self->allowCaching) {
            VResolverCacheEnable(self->resolver, vrAlwaysDisable);
        }
    }

    if (rc == 0) {
        rc = VDBDependenciesCount(dep, &count);
        if (rc != 0) {
            OUTMSG(("VDBDependenciesCount(%s, %s) = %R\n",
                name, missing ? "missing" : "all", rc));
        }
        else {
            OUTMSG(("VDBDependenciesCount(%s)=%d\n",
                missing ? "missing" : "all", count));
        }
    }

    if (rc == 0) {
        uint32_t i = 0;
        rc_t rc2 = 0;
        for (i = 0; i < count; ++i) {
            const char root[] = "Dependency";
            bool b = true;
            const char *s = NULL;
            const char *seqId = NULL;
            KPathType type = kptNotFound;

            if (self->xml) {
                OUTMSG(("<%s>\n", root));
            }

            OUTMSG((" %6d\t", i + 1));

            rc2 = VDBDependenciesSeqId(dep, &seqId, i);
            if (rc2 == 0) {
                OUTMSG(("seqId=%s,", seqId));
            }
            else {
                OUTMSG(("VDBDependenciesSeqId(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }

            rc2 = VDBDependenciesName(dep, &s, i);
            if (rc2 == 0) {
                OUTMSG(("name=%s,", s));
            }
            else {
                OUTMSG(("VDBDependenciesName(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }

            rc2 = VDBDependenciesCircular(dep, &b, i);
            if (rc2 == 0) {
                OUTMSG(("circular=%s,", b ? "true" : "false"));
            }
            else {
                OUTMSG(("VDBDependenciesCircular(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }

            rc2 = VDBDependenciesType(dep, &type, i);
            if (rc2 == 0) {
                rc2 = _KDBPathTypePrint("type=", type, ",");
                if (rc2 != 0 && rc == 0) {
                    rc = rc2;
                }
            }
            else {
                OUTMSG(("VDBDependenciesType(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }

            rc2 = VDBDependenciesLocal(dep, &b, i);
            if (rc2 == 0) {
                OUTMSG(("local=%s,", b ? "local" : "remote"));
            }
            else {
                OUTMSG(("VDBDependenciesLocal(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }

            rc2 = VDBDependenciesPath(dep, &s, i);
            if (rc2 == 0) {
                OUTMSG(("%s\tpathLocal=%s,", eol, s == NULL ? "notFound" : s));
            }
            else {
                OUTMSG(("VDBDependenciesPath(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }

            rc2 = VDBDependenciesPathRemote(dep, &s, i);
            if (rc2 == 0) {
                if (s == NULL) {
                    OUTMSG(("%s\tpathRemote: notFound ", eol));
                }
                else {
                    OUTMSG(("%s\tpathRemote: %s ", eol, s));
                    if (!self->noRfs) {
                        const KFile *f = NULL;
                        rc2 = KNSManagerMakeHttpFile ( self->knsMgr, & f, NULL, 0x01010000, s );
                        if (rc2 != 0) {
                            OUTMSG(("KNSManagerMakeHttpFile=%R", rc2));
                            if (rc == 0) {
                                rc = rc2;
                            }
                        }
                        if (rc2 == 0) {
                            uint64_t sz = 0;
                            rc2 = KFileSize(f, &sz);
                            if (rc2 != 0) {
                                OUTMSG(("KFileSize=%R", rc2));
                                if (rc == 0) {
                                    rc = rc2;
                                }
                            }
                            else {
                                OUTMSG(("%,lu", sz));
                            }
                        }
                        RELEASE(KFile, f);
                    }
                }
            }
            else {
                OUTMSG(("VDBDependenciesPathRemote(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }

            rc2 = VDBDependenciesPathCache(dep, &s, i);
            if (rc2 == 0) {
                OUTMSG(("%s\tpathCache: %s ", eol, s == NULL ? "notFound" : s));
                if (s != NULL) {
                    char cachecache[PATH_MAX] = "";
                    rc2 = MainReport(self, s, NULL, NULL, NULL);
                    OUTMSG(("%s", eol));
                    if (rc == 0) {
                        rc = rc2;
                    }
                    if (rc2 == 0) {
                        rc2 = string_printf(cachecache,
                            sizeof cachecache, NULL, "%s.cache", s);
                        if (rc2 != 0) {
                            if (rc == 0) {
                                rc = rc2;
                            }
                            OUTMSG(("string_printf(%s) = %R%s", s, rc2, eol));
                        }
                    }
                    if (rc == 0) {
                        OUTMSG(("\tpathCache.cache: "));
                        OUTMSG(("%s ", cachecache));
                        rc = MainReport(self, cachecache, NULL, NULL, NULL);
                    }
                }
            }
            else {
                OUTMSG(("VDBDependenciesPathCache(%s, %s, %i)=%R ",
                    name, missing ? "missing" : "all", i, rc2));
                if (rc == 0) {
                    rc = rc2;
                }
            }
            if (MainHasTest(self, eResolve) && seqId != NULL) {
                int64_t remoteSz = 0;
                OUTMSG(("%s", eol));
                rc2 = MainResolve(self, NULL, seqId, NULL, &remoteSz, true);
                if (rc == 0 && rc2 != 0) {
                    rc = rc2;
                }
            }

            if (self->xml) {
                OUTMSG(("</%s>\n", root));
            }
            else {
                OUTMSG(("%s", eol));
            }
        }
    }

    RELEASE(VDBDependencies, dep);
    RELEASE(VDatabase, db);

    if (self->xml) {
        OUTMSG(("</%s>\n", root));
    }

    return rc;
}

static rc_t MainPrintAscp(const Main *self) {
    rc_t rc = 0;
    bool status = false;
    const char *b = self->xml ? "  <Ascp>\n"  : "";
    const char *e = self->xml ? "  </Ascp>\n" : "\n";
    const char *ascp_bin = NULL;
    const char *private_file = NULL;
    AscpOptions opt;
    memset(&opt, 0, sizeof opt);
    assert(self);
    if (MainHasTest(self, eAscpVerbose)) {
        status = true;
    }
    OUTMSG(("%s", b));
    rc = ascp_locate(&ascp_bin, &private_file, true, status);
    if (rc != 0) {
        OUTMSG(("ascp_locate = %R\n", rc));
    }
    if (rc == 0) {
        const char *b = self->xml ? "    <Ascp>"       : "ascp    : ";
        const char *e = self->xml ? "</Ascp>\n" : "\n";
        OUTMSG(("%s%s%s", b, ascp_bin == NULL ? "NotFound" : ascp_bin, e));
    }
    if (rc == 0) {
        const char *b = self->xml ? "    <KeyFile>"    : "key file: ";
        const char *e = self->xml ? "</KeyFile>\n" : "\n";
        OUTMSG(("%s%s%s", b,
            private_file == NULL ? "NotFound" : private_file, e));
    }

    rc = aspera_options(&opt);
    if (rc != 0) {
        OUTMSG(("%saspera_options = %R%s", b, rc, e));
    }
    if (rc == 0) {
        const char *b = self->xml ? "    <MaxRate>"    : "max rate: ";
        const char *e = self->xml ? "</MaxRate>\n" : "\n";
        OUTMSG(("%s%s%s", b,
            opt.target_rate == NULL ? "NotFound" : opt.target_rate, e));
    }
    if (rc == 0) {
        const char *b = self->xml ? "    <Disabled>"    : "disabled: ";
        const char *e = self->xml ? "</Disabled>\n" : "\n";
        OUTMSG(("%s%s%s", b, opt.disabled ? "true" : "false", e));
    }

    if (ascp_bin != NULL) {
        size_t num_writ = 0;
        char command[PATH_MAX] = "";
        const char *b = self->xml ? "<Version>\n"  : "";
        const char *e = self->xml ? "</Version>\n" : "";
        OUTMSG(("%s", b));
        rc = string_printf(command, sizeof command, &num_writ,
            "\"%s\" -A", ascp_bin);
        if (rc != 0 || num_writ >= sizeof command) {
            OUTMSG(("cannot generate ascp command: %R\n", rc));
        }
        else {
            int s = system(command);
            if (s != 0) {
                OUTMSG(("system(%s) = %d\n", command, s));
            }
        }
        OUTMSG(("%s", e));
    }

    free((void*)ascp_bin);
    free((void*)private_file);

    OUTMSG(("%s", e));
    return 0;
}

#if 0
static rc_t PrintCurl(bool full, bool xml) {
    const char *b = xml ? "  <Curl>"  : "";
    const char *e = xml ? "</Curl>" : "";

    KNSManager *mgr = NULL;

    rc_t rc = KNSManagerMake(&mgr);
    if (rc != 0) {
        OUTMSG(("%sKNSManagerMake = %R%s\n", b, rc, e));
    }
    else {
        rc_t rc = KNSManagerAvail(mgr);
        OUTMSG(("%s", b));

        if (full) {
            OUTMSG(("KNSManagerAvail = %R. ", rc));
        }

        if (rc == 0) {
            const char *version_string = NULL;
            rc = KNSManagerCurlVersion(mgr, &version_string);
            if (rc == 0) {
                if (full) {
                    OUTMSG(("Curl Version = %s", version_string));
                }
            }
            else {
                OUTMSG(("KNSManagerCurlVersion = %R", rc));
            }
        }

        if (rc == 0 && !full) {
            OUTMSG(("libcurl: found"));
        }
        OUTMSG(("%s\n", e));
    }

    RELEASE(KNSManager, mgr);

    return rc;
}
#endif

#define kptKartITEM (kptAlias - 1)

static
rc_t MainExec(const Main *self, const KartItem *item, const char *aArg, ...)
{
    const char root[] = "Object";

    rc_t rc = 0;
    rc_t rce = 0;

    KPathType type = kptNotFound;
    bool alias = false;
    int64_t directSz = -1;
    int64_t localSz = -1;
    int64_t remoteSz = -1;
    size_t num_writ = 0;
    char arg[PATH_MAX] = "";

    va_list args;
    va_start(args, aArg);

    assert(self);

    if (self->xml) {
        OUTMSG(("<%s>\n", root));
    }

    if (item != NULL) {
        type = kptKartITEM;
    }

    else {
        const char *eol = self->xml ? "<br/>\n" : "\n";

        rc = string_vprintf(arg, sizeof arg, &num_writ, aArg, args);
        if (rc != 0) {
            OUTMSG(("string_vprintf(%s)=%R%s", aArg, rc, eol));
            return rc;
        }
        assert(num_writ < sizeof arg);

        OUTMSG(("\n"));
        rc = printString(arg);
        if (rc != 0) {
            OUTMSG(("printString=%R%s", rc, eol));
            return rc;
        }
        if (MainHasTest(self, eType)) {
            OUTMSG((" "));
            rc = MainReport(self, arg, &directSz, &type, &alias);
        }
        OUTMSG(("%s", eol));

        if (MainHasTest(self, eOpenTable)) {
            rc_t rc2 = MainOpenAs(self, arg, false);
            if (rce == 0 && rc2 != 0) {
                rce = rc2;
            }
        }
        if (MainHasTest(self, eOpenDB)) {
            rc_t rc2 = MainOpenAs(self, arg, true);
            if (rce == 0 && rc2 != 0) {
                rce = rc2;
            }
        }
    }

    if (self->recursive && type == kptDir && !alias) {
        uint32_t i = 0;
        uint32_t count = 0;
        KNamelist *list = NULL;
        rc = KDirectoryList(self->dir, &list, NULL, NULL, "%s", arg);
        if (rc != 0) {
            OUTMSG(("KDirectoryList(%s)=%R ", arg, rc));
        }
        else {
            rc = KNamelistCount(list, &count);
            if (rc != 0) {
                OUTMSG(("KNamelistCount(KDirectoryList(%s))=%R ", arg, rc));
            }
        }
        for (i = 0; i < count && rc == 0; ++i) {
            const char *name = NULL;
            rc = KNamelistGet(list, i, &name);
            if (rc != 0) {
                OUTMSG(("KNamelistGet(KDirectoryList(%s), %d)=%R ",
                    arg, i, rc));
            }
            else {
                rc_t rc2 = MainExec(self, NULL, "%s/%s", arg, name);
                if (rc2 != 0 && rce == 0) {
                    rce = rc2;
                }
            }
        }
        RELEASE(KNamelist, list);
    }
    else {
        bool isKart = false;
        Kart *kart = NULL;
        if (type == kptFile) {
            rc_t rc = KartMake(self->dir, arg, &kart, &isKart);
            if (rc != 0) {
                OUTMSG(("KartMake = %R\n", rc));
            }
        }

        if (isKart) {
            const KartItem *item = NULL;
            while (true) {
                rc_t rc2 = 0;
                RELEASE(KartItem, item);
                rc2 = KartMakeNextItem(kart, &item);
                if (rc2 != 0) {
                    OUTMSG(("KartMakeNextItem = %R\n", rc2));
                    if (rce == 0) {
                        rce = rc2;
                    }
                    break;
                }
                if (item == NULL) {
                    break;
                }
                rc2 = MainExec(self, item, NULL);
                if (rc2 != 0 && rce == 0) {
                    rce = rc2;
                }
            }
            KartRelease(kart);
            kart = NULL;
        }
        else {
            if (MainHasTest(self, eResolve)) {
                rc_t rc2 = MainResolve(self, item, arg, &localSz, &remoteSz,
                    false);
                if (rc == 0 && rc2 != 0) {
                    rc = rc2;
                }
            }

            if (item == NULL) {
                if (type == kptDatabase || type == kptNotFound) {
                    if (MainHasTest(self, eDependMissing)) {
                        rc_t rc2 = MainDepend(self, arg, true);
                        if (rc == 0 && rc2 != 0) {
                            rc = rc2;
                        }
                    }

                    if (MainHasTest(self, eDependAll)) {
                        rc_t rc2 = MainDepend(self, arg, false);
                        if (rc == 0 && rc2 != 0) {
                            rc = rc2;
                        }
                    }
                }
            }

            if (MainHasTest(self, eResolve) && (
                (directSz != -1 && localSz != -1 && directSz != localSz) ||
                (remoteSz != -1 && localSz != -1 && localSz != remoteSz))
               )
            {
                OUTMSG(("FILE SIZES DO NOT MATCH: "));
                if (directSz != -1 && localSz != -1 &&
                    directSz != remoteSz)
                {
                    OUTMSG(("direct=%ld != remote=%ld. ", directSz, remoteSz));
                }
                if (remoteSz != -1 && localSz != -1 &&
                    localSz != remoteSz)
                {
                    OUTMSG(("local=%ld != remote=%ld. ", localSz, remoteSz));
                }
                OUTMSG(("\n"));
            }

            OUTMSG(("\n"));
        }
    }

    if (rce != 0 && rc == 0) {
        rc = rce;
    }

    if (self->xml) {
        OUTMSG(("</%s>\n", root));
    }

    return rc;
}

static
rc_t _SraReleaseVersionPrint(const SraReleaseVersion *self, rc_t rc, bool xml,
    const char *error, const char *msg)
{
    assert(self && error && msg);

    if (rc != 0) {
        OUTMSG(("ERROR: %s.", error));
    }
    else {
        char version[256] = "";
        const char *eol = xml ? "<br/>\n" : "\n";
        rc = SraReleaseVersionPrint(self, version, sizeof version, NULL);
        if (rc == 0) {
            rc = OUTMSG(("%s: %s.%s", msg, version, eol));
        }
    }

    return rc;
}

static rc_t MainPrintVersion(Main *self) {
    const char root[] = "Version";

    rc_t rc = 0;

    SraReleaseVersion version;
    SraReleaseVersion newVersion;

    assert(self);

    if (MainHasTest(self, eNewVersion)) {
        MainAddTest(self, eVersion);
    }

    if (!MainHasTest(self, eVersion)) {
        return 0;
    }
    memset(&version, 0, sizeof version);
    memset(&newVersion, 0, sizeof newVersion);
    if (self->xml) {
        OUTMSG(("<%s>\n", root));
    }
    rc = SraReleaseVersionGet(&version);
    rc = _SraReleaseVersionPrint(&version, rc, self->xml,
        "cannot get SRA Toolkit release version",
        "NCBI SRA Toolkit release version");

    if (MainHasTest(self, eNewVersion)) {
        int32_t isNew = 0;
        rc = KNSManagerNewReleaseVersion(self->knsMgr, &newVersion);
        rc = _SraReleaseVersionPrint(&newVersion, rc, self->xml,
            "cannot get latest available SRA Toolkit release version",
            "Latest available NCBI SRA Toolkit release version");
        if (rc == 0) {
            rc = SraReleaseVersionCmp(&version, &newVersion, &isNew);
        }
        if (rc == 0) {
            if (isNew > 0) {
                OUTMSG((
           "New version of SRA Toolkit is available for download from\n"
           "\"http://www.ncbi.nlm.nih.gov/Traces/sra/sra.cgi?view=software\".\n"
                ));
            }
            else if (isNew == 0) {
                OUTMSG(("You already have the latest version of "
                    "SRA Toolkit.\n"));
            }
            else {
                OUTMSG(("Your version of "
                    "SRA Toolkit is more recent than the latest available.\n"));
            }
        }
    }

    if (self->xml) {
        OUTMSG(("</%s>\n", root));
    }
    else if (self->full) {
        OUTMSG(("\n"));
    }

    return rc;
}

static rc_t MainFini(Main *self) {
    rc_t rc = 0;

    assert(self);

    RELEASE(VResolver, self->resolver);
    RELEASE(KConfig, self->cfg);
    RELEASE(KNSManager, self->knsMgr);
    RELEASE(KRepositoryMgr, self->repoMgr);
    RELEASE(VFSManager, self->vMgr);
    RELEASE(VDBManager, self->mgr);
    RELEASE(KDirectory, self->dir);

    return rc;
}

#define OPTION_CACHE "allow-caching"
#define ALIAS_CACHE  "C"
static const char* USAGE_CACHE[] = { "do not disable caching", NULL };

#define OPTION_FULL "full"
#define ALIAS_FULL  NULL
static const char* USAGE_FULL[] = { "full test mode", NULL };

#define OPTION_QUICK "quick"
#define ALIAS_QUICK  "Q"
static const char* USAGE_QUICK[] = { "quick test mode", NULL };

#define OPTION_NO_RFS "no-rfs"
static const char* USAGE_NO_RFS[]
    = { "do not check remote file size for dependencies", NULL };

#define OPTION_NO_VDB "no-vdb"
#define ALIAS_NO_VDB  "N"
static const char* USAGE_NO_VDB[] = { "do not call VDBManagerPathType", NULL };

#define OPTION_REC "recursive"
#define ALIAS_REC  "R"
static const char* USAGE_REC[] = { "check object type recursively", NULL };

#define OPTION_OUT "output"
#define ALIAS_OUT  "X"
static const char* USAGE_OUT[] = { "output type: one of (xml text)", NULL };

OptDef Options[] = {                             /* needs_value, required */
    { OPTION_CACHE , ALIAS_CACHE , NULL, USAGE_CACHE , 1, false, false },
    { OPTION_FULL  , ALIAS_FULL  , NULL, USAGE_FULL  , 1, false, false },
    { OPTION_NO_RFS, NULL        , NULL, USAGE_NO_RFS, 1, false, false },
    { OPTION_NO_VDB, ALIAS_NO_VDB, NULL, USAGE_NO_VDB, 1, false, false },
    { OPTION_OUT   , ALIAS_OUT   , NULL, USAGE_OUT   , 1, true , false },
    { OPTION_QUICK , ALIAS_QUICK , NULL, USAGE_QUICK , 1, false, false },
    { OPTION_REC   , ALIAS_REC   , NULL, USAGE_REC   , 1, false, false }
};

rc_t CC KMain(int argc, char *argv[]) {
    rc_t rc = 0;
    uint32_t pcount = 0;
    uint32_t i = 0;
    Args *args = NULL;
    rc_t rc3 = 0;
    int argi = 0;

    Main prms;
    char **argv2 = MainInit(&prms, argc, argv, &argi);

    if (rc == 0) {
        rc = ArgsMakeAndHandle(&args, argi, argv2, 1,
            Options, sizeof Options / sizeof Options[0]);
    }

    if (rc == 0) {
        rc = ArgsOptionCount(args, OPTION_CACHE, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_CACHE "' argument");
        }
        else {
            if (pcount > 0) {
                prms.allowCaching = true;
            }
        }
    }

    prms.full = true;

    if (rc == 0) {
        rc = ArgsOptionCount(args, OPTION_QUICK, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_QUICK "' argument");
        }
        else {
            if (pcount > 0) {
                prms.full = false;
            }
        }
    }

    if (rc == 0) {
        rc = ArgsOptionCount(args, OPTION_FULL, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_FULL "' argument");
        }
        else {
            if (pcount > 0) {
                prms.full = true;
            }
        }
    }

    if (!prms.full) {
        MainMakeQuick(&prms);
    }

    if (rc == 0) {
        rc = MainInitObjects(&prms);
    }

    if (rc == 0) {
        const char root[] = "Test-sra";
        rc = ArgsOptionCount(args, OPTION_OUT, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_OUT "' argument");
        }
        else {
            if (pcount > 0) {
                const char* dummy = NULL;
                rc = ArgsOptionValue(args, OPTION_OUT, 0, &dummy);
                if (rc) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" OPTION_OUT "' argument");
                }
                else if (strcmp(dummy, "x") == 0 || strcmp(dummy, "X") == 0) {
                    prms.xml = true;
                }
                else {
                    prms.xml = false;
                }
            }
            else {
                prms.xml = MainHasTest(&prms, eCfg)
                    || MainHasTest(&prms, eNcbiReport);
            }
        }

        if (prms.xml) {
            OUTMSG(("<%s>\n", root));
        }

        MainPrintVersion(&prms);

        if (MainHasTest(&prms, eCfg)) {
            rc_t rc2 = MainPrintConfig(&prms);
            if (rc == 0 && rc2 != 0) {
                rc = rc2;
            }
        }

        if (MainHasTest(&prms, eOS)) {
            PrintOS(prms.xml);
        }

        if (MainHasTest(&prms, eAscp)) {
            MainPrintAscp(&prms);
        }

#if 0
        if (MainHasTest(&prms, eCurl)) {
            PrintCurl(prms.full, prms.xml);
        }
#endif

        if (!prms.full) {
            rc_t rc2 = MainQuickCheck(&prms);
            if (rc == 0 && rc2 != 0) {
                rc = rc2;
            }
        }

        if (rc == 0) {
            rc = ArgsOptionCount(args, OPTION_NO_RFS, &pcount);
            if (rc) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OPTION_NO_RFS "' argument");
            }
            else {
                if (pcount > 0) {
                    prms.noRfs = true;
                }
            }
        }

        if (rc == 0) {
            rc = ArgsOptionCount(args, OPTION_NO_VDB, &pcount);
            if (rc) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OPTION_NO_VDB "' argument");
            }
            else {
                if (pcount > 0) {
                    prms.noVDBManagerPathType = true;
                }
            }
        }

        if (rc == 0) {
            rc = ArgsOptionCount(args, OPTION_REC, &pcount);
            if (rc) {
                LOGERR(klogErr, rc, "Failure to get '" OPTION_REC "' argument");
            }
            else {
                if (pcount > 0) {
                    prms.recursive = true;
                }
            }
        }

        if (rc == 0) {
            rc = ArgsParamCount(args, &pcount);
        }

        for (i = 0; i < pcount; ++i) {
            const char *name = NULL;
            rc3 = ArgsParamValue(args, i, &name);
            if (rc3 == 0) {
                rc_t rc2 = 0;
                ReportResetObject(name);
                rc2 = MainExec(&prms, NULL, name);
                if (rc == 0 && rc2 != 0) {
                    rc = rc2;
                }
            }
        }
        if (rc == 0 && rc3 != 0) {
            rc = rc3;
        }

        if (MainHasTest(&prms, eNcbiReport)) {
            ReportForceFinalize();
        }

        if (!prms.full) {
            OUTMSG(("\nAdd -F option to try all the tests."));
        }

        if (prms.xml) {
            OUTMSG(("</%s>\n", root));
        }
    }

    RELEASE(Args, args);

    {
        rc_t rc2 = MainFini(&prms);
        if (rc == 0 && rc2 != 0) {
            rc = rc2;
        }
    }
    free(argv2);

    OUTMSG(("\n"));

    return rc;
}
