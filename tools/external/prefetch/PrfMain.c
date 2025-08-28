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
* =========================================================================== */

#include <ascp/ascp.h> /* ascp_locate */

#include <kapp/args-conv.h> /* ArgsConvFilepath */
#include <kapp/main.h> /* KAppVersion */

#include <kdb/manager.h> /* kptDatabase */

#include <kfg/config.h> /* KConfigRelease */
#include <kfg/repository.h> /* KRepositoryMgr */

#include <kfs/kfs-priv.h> /* KDirectoryPosixStringToSystemString */

#include <klib/log.h> /* PLOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/rc.h> /* RC */
#include <klib/status.h> /* STSMSG */
#include <klib/text.h> /* string_dup_measure */

#include <kns/http.h> /* KNSManagerMakeHttpFile */
#include <kns/kns-mgr-priv.h> /* KNSManagerMakeReliableHttpFile */
#include <kns/manager.h> /* KNSManagerRelease */

#include <vdb/database.h> /* VDBManagerOpenDBRead */
#include <vdb/dependencies.h> /* VDatabaseListDependencies */
#include <vdb/manager.h> /* VDBManagerPathType */
#include <vdb/vdb-priv.h> /* VDBManagerSetResolver */

#include <vfs/manager.h> /* VFSManagerMake */
#include <vfs/path.h> /* VPathGetCeRequired */
#include <vfs/resolver.h> /* VResolverRelease */

#include "PrfMain.h"
#include "PrfOutFile.h" /* PATH_MAX */

#include <time.h> /* time */

bool _StringIsXYZ(const String *self, const char **withoutScheme,
    const char * scheme, size_t scheme_size)
{
    const char *dummy = NULL;

    assert(self && self->addr);

    if (withoutScheme == NULL) {
        withoutScheme = &dummy;
    }

    *withoutScheme = NULL;

    if (string_cmp(self->addr, self->len, scheme, scheme_size,
        (uint32_t)scheme_size) == 0)
    {
        *withoutScheme = self->addr + scheme_size;
        return true;
    }
    return false;
}

bool _StringIsFasp(const String *self, const char **withoutScheme) {
    const char fasp[] = "fasp://";
    return _StringIsXYZ(self, withoutScheme, fasp, sizeof fasp - 1);
}

/********** KFile extension **********/
rc_t _KFileOpenRemote(const struct KFile **self, KNSManager *kns,
    const VPath *vpath, const struct String *path, bool reliable)
{
    rc_t rc = 0;

    bool ceRequired = false;
    bool payRequired = false;

    assert(self);

    if (*self != NULL)
        return 0;

    assert(path);

    if (_StringIsFasp(path, NULL))
        return
        SILENT_RC(rcExe, rcFile, rcConstructing, rcParam, rcWrongType);

    VPathGetCeRequired(vpath, &ceRequired);
    VPathGetPayRequired(vpath, &payRequired);

    if (reliable)
        rc = KNSManagerMakeReliableHttpFile(kns, self, NULL, 0x01010000, true,
            ceRequired, payRequired, "%S", path);
    else
        rc = KNSManagerMakeHttpFile(kns, self, NULL, 0x01010000, "%S", path);

    return rc;
}

/********** TreeNode **********/

typedef struct {
    BSTNode n;
    char *path;
} TreeNode;

static int64_t CC bstCmp(const void *item, const BSTNode *n) {
    const char* path = item;
    const TreeNode* sn = (const TreeNode*)n;

    assert(path && sn && sn->path);

    return strcmp(path, sn->path);
}

static void CC bstWhack(BSTNode* n, void* ignore) {
    TreeNode* sn = (TreeNode*)n;

    assert(sn);

    free(sn->path);

    memset(sn, 0, sizeof *sn);

    free(sn);
}

static int64_t CC bstSort(const BSTNode* item, const BSTNode* n) {
    const TreeNode* sn = (const TreeNode*)item;

    return bstCmp(sn->path, n);
}

rc_t _VDBManagerSetDbGapCtx(
    const struct VDBManager *self, struct VResolver *resolver)
{
    if (resolver == NULL) {
        return 0;
    }

    return VDBManagerSetResolver(self, resolver);
}

/********** PrfMain **********/

bool PrfMainUseAscp(PrfMain *self) {
    rc_t rc = 0;

    assert(self);

    if (self->ascpChecked) {
        return self->ascp != NULL;
    }

    self->ascpChecked = true;

    if (self->noAscp) {
        return false;
    }

    rc = ascp_locate(&self->ascp, &self->asperaKey, true, true);
    return rc == 0 && self->ascp && self->asperaKey;
}

bool PrfMainHasDownloaded(const PrfMain *self, const char *local) {
    TreeNode *sn = NULL;

    assert(self);

    sn = (TreeNode*)BSTreeFind(&self->downloaded, local, bstCmp);

    return sn != NULL;
}

rc_t PrfMainDownloaded(PrfMain *self, const char *path) {
    TreeNode *sn = NULL;

    assert(self);

    if (PrfMainHasDownloaded(self, path)) {
        return 0;
    }

    sn = calloc(1, sizeof *sn);
    if (sn == NULL) {
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    sn->path = string_dup_measure(path, NULL);
    if (sn->path == NULL) {
        bstWhack((BSTNode*)sn, NULL);
        sn = NULL;
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    BSTreeInsert(&self->downloaded, (BSTNode*)sn, bstSort);

    return 0;
}

rc_t PrfMainDependenciesList(const PrfMain *self, const Resolved *resolved,
    const struct VDBDependencies **deps)
{
    rc_t rc = 0;
    bool isDb = true;
    const VDatabase *db = NULL;
    const String *str = NULL;
    KPathType type = kptNotFound;

    assert(self && resolved && deps);

    str = resolved->path.str;
    assert(str && str->addr);

    rc = _VDBManagerSetDbGapCtx(self->mgr, resolved->resolver);

    STSMSG(STS_DBG, ("Listing '%S's dependencies...", str));

    type = VDBManagerPathType(self->mgr, "%S", str) & ~kptAlias;
    if (type != kptDatabase) {
        if (type == kptTable)
            STSMSG(STS_DBG, ("...'%S' is a table", str));
        else
            STSMSG(STS_DBG,
                ("...'%S' is not recognized as a database or a table", str));
        return 0;
    }

    rc = VDBManagerOpenDBRead(self->mgr, &db, NULL, "%S", str);
    if (rc != 0) {
        if (rc == SILENT_RC(rcDB, rcMgr, rcOpening, rcDatabase, rcIncorrect)) {
            isDb = false;
            rc = 0;
        }
        else if (rc ==
            SILENT_RC(rcKFG, rcEncryptionKey, rcRetrieving, rcItem, rcNotFound))
        {
            STSMSG(STS_TOP,
                ("Cannot open encrypted file '%s'", resolved->name));
            isDb = false;
            rc = 0;
        }
        DISP_RC2(rc, "Cannot open database", resolved->name);
    }

    if (rc == 0 && isDb) {
        bool all = self->check_all || self->force != eForceNo;
        char pResolvd[PATH_MAX] = "";
        const char * outDir = self->outDir;
        if (self->outDir != NULL) {
            rc = KDirectoryResolvePath(
                self->dir, true, pResolvd, sizeof pResolvd, "%s", self->outDir);
            outDir = pResolvd;
        }
        if (rc == 0) {
            rc = VDatabaseListDependenciesExt(db, deps, !all, outDir);
            DISP_RC2(rc, "VDatabaseListDependencies", resolved->name);
        }
    }

    RELEASE(VDatabase, db);

    return rc;
}

rc_t PrfMainOutDirCheck(PrfMain * self, bool * setAndNotExists) {
    assert(self && setAndNotExists);

    *setAndNotExists = false;

    if (self->outDir == NULL)
        return 0;

    if ((KDirectoryPathType(self->dir, self->outDir) & ~kptAlias) ==
        kptNotFound)
    {
        *setAndNotExists = true;
    }

    return 0;
}

static uint64_t _sizeFromString(const char *val) {
    uint64_t s = 0;

    for (s = 0; *val != '\0'; ++val) {
        if (*val < '0' || *val > '9') {
            break;
        }
        s = s * 10 + *val - '0';
    }

    if (*val == '\0' || *val == 'k' || *val == 'K') {
        s *= 1024L;
    }
    else if (*val == 'b' || *val == 'B') {
    }
    else if (*val == 'm' || *val == 'M') {
        s *= 1024L * 1024;
    }
    else if (*val == 'g' || *val == 'G') {
        s *= 1024L * 1024 * 1024;
    }
    else if (*val == 't' || *val == 'T') {
        s = s * 1024L * 1024 * 1024 * 1024;
    }
    else if (*val == 'u' || *val == 'U') {  /* unlimited */
        s = 0;
    }

    return s;
}

/*********** Command line arguments **********/

static ParamDef Parameters[] = { { ArgsConvFilepath } };

#define ASCP_OPTION "ascp-path"
#define ASCP_ALIAS  "a"
static const char* ASCP_USAGE[] =
{ "Path to ascp program and private key file (aspera_tokenauth_id_rsa)", NULL };

#define ASCP_PAR_OPTION "ascp-options"
#define ASCP_PAR_ALIAS  NULL
static const char* ASCP_PAR_USAGE[] =
{ "Arbitrary options to pass to ascp command line.", NULL };

#define CHECK_ALL_OPTION "check-all"
#define CHECK_ALL_ALIAS  "c"
static const char* CHECK_ALL_USAGE[] = { "Double-check all refseqs.", NULL };

#define CHECK_NEW_OPTION "check-rs"
#define CHECK_NEW_ALIAS  "S"
static const char* CHECK_NEW_USAGE[] = {
    "Check for refseqs in downloaded files: "
    "one of: no, yes, smart [default]. "
    "Smart: skip check for large encrypted non-sra files.", NULL };

#define VALIDATE_OPTION "verify"
#define VALIDATE_ALIAS  "C"
static const char* VALIDATE_USAGE[] = {
    "Verify after download: one of: no, yes [default].", NULL };

#define DRY_RUN_OPTION "dryrun"
static const char* DRY_RUN_USAGE[] = {
    "Dry run the application: don't download, only check resolving.", NULL };

#define FORCE_OPTION "force"
#define FORCE_ALIAS  "f"
static const char* FORCE_USAGE[] = {
    "Force object download: one of: no, yes, all, ALL.",
    "no [default]: skip download if the object if found and complete;",
    "yes: download it even if it is found and is complete;",
    "all: ignore lock files (stale locks or "
    "it is being downloaded by another process - "
    "use at your own risk!);",
    "ALL: ignore lock files, restart download from beginning.", NULL };

#define RESUME_OPTION "resume"
#define RESUME_ALIAS  "r"
static const char* RESUME_USAGE[] = {
    "Resume partial downloads: one of: no, yes [default].", NULL };

#define FAIL_ASCP_OPTION "FAIL-ASCP"
#define FAIL_ASCP_ALIAS  "F"
static const char* FAIL_ASCP_USAGE[] = {
    "Force ascp download fail to test ascp->http download combination.", NULL };

#define LIST_OPTION "list"
#define LIST_ALIAS  "l"
static const char* LIST_USAGE[] = {"List the content of a kart file.", NULL};

#define LOCN_OPTION "location"
#define LOCN_ALIAS  NULL
static const char* LOCN_USAGE[] = { "Location of data.", NULL };

#define NM_L_OPTION "numbered-list"
#define NM_L_ALIAS  "n"
static const char* NM_L_USAGE[] =
{ "List the content of a kart file with kart row numbers.", NULL };

#define MINSZ_ALIAS  "N"
static const char* MINSZ_USAGE[] =
{ "Minimum file size to download in KB (inclusive).", NULL };

#define ORDR_OPTION "order"
#define ORDR_ALIAS  "o"
static const char* ORDR_USAGE[] = {
    "Kart prefetch order when downloading a kart: one of: kart, size.",
    "(in kart order, by file size: smallest first), default: size.", NULL };

#define OUT_DIR_OPTION "output-directory"
#define OUT_DIR_ALIAS  "O"
static const char* OUT_DIR_USAGE[] = { "Save files to DIRECTORY/", NULL };

#define OUT_FILE_ALIAS  "o"
static const char* OUT_FILE_USAGE[] = {
    "Write file to FILE when downloading a single file.", NULL };

#define HBEAT_OPTION "heartbeat"
#define HBEAT_ALIAS  "H"
static const char* HBEAT_USAGE[] = {
    "Time period in minutes to display download progress.",
    "(0: no progress), default: 1", NULL };

#define PRGRS_OPTION "progress"
#define PRGRS_ALIAS  "p"
static const char* PRGRS_USAGE[] = { "Show progress.", NULL };

#define ROWS_OPTION "rows"
#define ROWS_ALIAS  "R"
static const char* ROWS_USAGE[] =
{ "Kart rows to download (default all).", "Row list should be ordered.", NULL };

#define SZ_L_OPTION "list-sizes"
#define SZ_L_ALIAS  "s"
static const char* SZ_L_USAGE[] =
{ "List the content of a kart file with target file sizes.", NULL };
#define TRANS_OPTION "transport"
#define TRASN_ALIAS  "t"
static const char* TRANS_USAGE[] = {
    "Transport: one of: fasp; http; both [default].",
    "(fasp only; http only; first try fasp (ascp), "
    "use http if cannot download using fasp).", NULL };

#define TYPE_OPTION "type"
#define TYPE_ALIAS  "T"
static const char* TYPE_USAGE[] = { "Specify file type to download.",
    "Default: sra", NULL };

#define DEFAULT_MAX_FILE_SIZE "20G"
#define SIZE_ALIAS  "X"
static const char* SIZE_USAGE[] = {
    "Maximum file size to download in KB (exclusive).",
    "Default: " DEFAULT_MAX_FILE_SIZE, NULL };

#if ALLOW_STRIP_QUALS
#define STRIP_QUALS_OPTION "strip-quals"
#define STRIP_QUALS_ALIAS NULL
static const char* STRIP_QUALS_USAGE[] =
{ "Remove QUALITY column from all tables.", NULL };
#endif

static const char* ELIM_QUALS_USAGE[] =
{ "Download SRA Lite files with simplified base quality scores, "
  "or fail if not available.", NULL };

#define CART_OPTION "perm"
static const char* CART_USAGE[] = { "PATH to jwt cart file.", NULL };

#define NGC_ALIAS  NULL
static const char* NGC_USAGE[] = { "PATH to ngc file.", NULL };

static const char* KART_USAGE[] = { "To read a kart file.", NULL };

#if _DEBUGGING
static const char* TEXTKART_USAGE[] =
{ "To read a textual format kart file (DEBUG ONLY).", NULL };
#endif

static OptDef OPTIONS[] = {
    /*                                          max_count needs_value required*/
 { TYPE_OPTION        , TYPE_ALIAS        , NULL, TYPE_USAGE  , 1, true, false }
,{ TRANS_OPTION       , TRASN_ALIAS       , NULL, TRANS_USAGE , 1, true, false }
,{ LOCN_OPTION        , LOCN_ALIAS        , NULL, LOCN_USAGE  , 1, true, false }
,{ MINSZ_OPTION       , MINSZ_ALIAS       , NULL, MINSZ_USAGE , 1, true ,false }
,{ SIZE_OPTION        , SIZE_ALIAS        , NULL, SIZE_USAGE  , 1, true ,false }
,{ FORCE_OPTION       , FORCE_ALIAS       , NULL, FORCE_USAGE , 1, true, false }
,{ RESUME_OPTION      , RESUME_ALIAS      , NULL, RESUME_USAGE, 1, true, false }
,{ VALIDATE_OPTION    , VALIDATE_ALIAS    , NULL,VALIDATE_USAGE,1, true, false }
,{ PRGRS_OPTION       , PRGRS_ALIAS       , NULL, PRGRS_USAGE , 1, false,false }
,{ HBEAT_OPTION       , HBEAT_ALIAS       , NULL, HBEAT_USAGE , 1, true, false }
,{ ELIM_QUALS_OPTION  , NULL             ,NULL,ELIM_QUALS_USAGE,1, false,false }
,{ CHECK_ALL_OPTION   , CHECK_ALL_ALIAS   ,NULL,CHECK_ALL_USAGE,1, false,false }
,{ CHECK_NEW_OPTION   , CHECK_NEW_ALIAS   ,NULL,CHECK_NEW_USAGE,1, true ,false }
,{ LIST_OPTION        , LIST_ALIAS        , NULL, LIST_USAGE  , 1, false,false }
,{ NM_L_OPTION        , NM_L_ALIAS        , NULL, NM_L_USAGE  , 1, false,false }
,{ SZ_L_OPTION        , SZ_L_ALIAS        , NULL, SZ_L_USAGE  , 1, false,false }
,{ ORDR_OPTION        , ORDR_ALIAS        , NULL, ORDR_USAGE  , 1, true ,false }
,{ ROWS_OPTION        , ROWS_ALIAS        , NULL, ROWS_USAGE  , 1, true, false }
,{ CART_OPTION        , NULL              , NULL, CART_USAGE  , 1, true ,false }
,{ NGC_OPTION         , NULL              , NULL, NGC_USAGE   , 1, true ,false }
,{ KART_OPTION        , NULL              , NULL, KART_USAGE  , 1, true, false }
#if _DEBUGGING
,{ TEXTKART_OPTION    , NULL              , NULL,TEXTKART_USAGE,1, true, false }
#endif
,{ ASCP_OPTION        , ASCP_ALIAS        , NULL, ASCP_USAGE  , 1, true ,false }
,{ ASCP_PAR_OPTION    , ASCP_PAR_ALIAS    , NULL, ASCP_PAR_USAGE,1,true, false}
,{ FAIL_ASCP_OPTION   , FAIL_ASCP_ALIAS  ,NULL,FAIL_ASCP_USAGE, 1, false,false }
#if ALLOW_STRIP_QUALS
,{ STRIP_QUALS_OPTION ,STRIP_QUALS_ALIAS,NULL,STRIP_QUALS_USAGE,1, false,false }
#endif
,{ OUT_FILE_OPTION    , NULL              ,NULL,OUT_FILE_USAGE ,1, true, false }
,{ OUT_DIR_OPTION     , OUT_DIR_ALIAS     , NULL, OUT_DIR_USAGE,1, true, false }
,{ DRY_RUN_OPTION     , NULL              , NULL, DRY_RUN_USAGE,1, false,false }
};

static rc_t PrfMainProcessArgs(PrfMain *self, int argc, char *argv[]) {
    rc_t rc = 0;

    assert(self);

    KStsLevelSet(STAT_USR);

    rc = ArgsMakeAndHandle2(&self->args, argc, argv,
        Parameters, sizeof Parameters / sizeof Parameters[0],
        1, OPTIONS, sizeof OPTIONS / sizeof OPTIONS[0]);
    if (rc != 0) {
        DISP_RC(rc, "ArgsMakeAndHandle");
        return rc;
    }

    do {
        const char * option_name = NULL;
        uint32_t pcount = 0;

/* FORCE_OPTION goes first */
        rc = ArgsOptionCount(self->args, FORCE_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" FORCE_OPTION "' argument");
            break;
        }

        if (pcount > 0) {
            const char *val = NULL;
            rc = ArgsOptionValue(self->args, FORCE_OPTION, 0, (const void **)&val);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" FORCE_OPTION "' argument value");
                break;
            }
            if (val == NULL || val[0] == '\0') {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                LOGERR(klogErr, rc,
                    "Unrecognized '" FORCE_OPTION "' argument value");
                break;
            }
            switch (val[0]) {
            case 'n':
            case 'N':
                self->force = eForceNo;
                break;
            case 'y':
            case 'Y':
                self->force = eForceYes;
                break;
            case 'a':
                self->force = eForceAll;
                break;
            case 'A':
                self->force = eForceALL;
                break;
            default:
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                LOGERR(klogErr, rc,
                    "Unrecognized '" FORCE_OPTION "' argument value");
                break;
            }
            if (rc != 0) {
                break;
            }
        }

/* CHECK_ALL_OPTION goes after FORCE_OPTION */
        rc = ArgsOptionCount(self->args, CHECK_ALL_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" CHECK_ALL_OPTION "' argument");
            break;
        }
        if (pcount > 0 || self->force != eForceNo)
            self->check_all = true;

option_name = CHECK_NEW_OPTION;
{
    self->check_refseqs = eDefault; /* smart check by default */
    rc = ArgsOptionCount(self->args, option_name, &pcount);
    if (rc != 0) {
        PLOGERR(klogInt, (klogInt, rc,
            "Failure to get '$(opt)' argument", "opt=%s", option_name));
        break;
    }

    if (pcount > 0) {
        const char *val = NULL;
        rc = ArgsOptionValue(
            self->args, option_name, 0, (const void **)&val);
        if (rc != 0) {
            PLOGERR(klogInt, (klogInt, rc, "Failure to get "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        if (val == NULL || val[0] == '\0') {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            PLOGERR(klogInt, (klogInt, rc, "Unrecognized "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        switch (val[0]) {
        case 'n':
        case 'N':
            self->check_refseqs = eFalse;
            break;
        case 's':
        case 'S':
            self->check_refseqs = eDefault;
            break;
        case 'y':
        case 'Y':
            self->check_refseqs = eTrue;
            break;
        default:
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            PLOGERR(klogInt, (klogInt, rc, "Unrecognized "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        if (rc != 0)
            break;
    }
}

option_name = RESUME_OPTION;
{
    self->resume = true; /* resume partial downloads by default */
    rc = ArgsOptionCount(self->args, option_name, &pcount);
    if (rc != 0) {
        PLOGERR(klogInt, (klogInt, rc,
            "Failure to get '$(opt)' argument", "opt=%s", option_name));
        break;
    }

    if (pcount > 0) {
        const char *val = NULL;
        rc = ArgsOptionValue(
            self->args, option_name, 0, (const void **)&val);
        if (rc != 0) {
            PLOGERR(klogInt, (klogInt, rc, "Failure to get "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        if (val == NULL || val[0] == '\0') {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            PLOGERR(klogInt, (klogInt, rc, "Unrecognized "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        switch (val[0]) {
        case 'n':
        case 'N':
            self->resume = false;
            break;
        case 'y':
        case 'Y':
            self->resume = true;
            break;
        default:
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            PLOGERR(klogInt, (klogInt, rc, "Unrecognized "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        if (rc != 0)
            break;
    }
}

option_name = VALIDATE_OPTION;
{
    self->validate = true; /* validate downloads by default */
    rc = ArgsOptionCount(self->args, option_name, &pcount);
    if (rc != 0) {
        PLOGERR(klogInt, (klogInt, rc,
            "Failure to get '$(opt)' argument", "opt=%s", option_name));
        break;
    }

    if (pcount > 0) {
        const char *val = NULL;
        rc = ArgsOptionValue(
            self->args, option_name, 0, (const void **)&val);
        if (rc != 0) {
            PLOGERR(klogInt, (klogInt, rc, "Failure to get "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        if (val == NULL || val[0] == '\0') {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            PLOGERR(klogInt, (klogInt, rc, "Unrecognized "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        switch (val[0]) {
        case 'n':
        case 'N':
            self->validate = false;
            break;
        case 'y':
        case 'Y':
            self->validate = true;
            break;
        default:
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            PLOGERR(klogInt, (klogInt, rc, "Unrecognized "
                "'$(opt)' argument value", "opt=%s", option_name));
            break;
        }
        if (rc != 0)
            break;
    }
}

#if 1
/******* LIST OPTIONS BEGIN ********/
/* LIST_OPTION */
        rc = ArgsOptionCount(self->args, LIST_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" LIST_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            self->list_kart = true;
        }

/* NM_L_OPTION */
        rc = ArgsOptionCount(self->args, NM_L_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" NM_L_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            self->list_kart = self->list_kart_numbered = true;
        }

/* SZ_L_OPTION */
        rc = ArgsOptionCount(self->args, SZ_L_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" SZ_L_OPTION "' argument");
            break;
        }
        if (pcount > 0) { /* self->list_kart is not set here! */
            self->list_kart_sized = true;
        }
/******* LIST OPTIONS END ********/
#endif

option_name = LOCN_OPTION;
        {
            rc = ArgsOptionCount(self->args, option_name, &pcount);
            if (rc != 0) {
                PLOGERR(klogInt, (klogInt, rc,
                    "Failure to get '$(opt)' argument", "opt=%s", option_name));
                break;
            }
            if (pcount > 0) {
                rc = ArgsOptionValue(self->args,
                    option_name, 0, (const void **)&self->location);
                if (rc != 0) {
                    PLOGERR(klogInt, (klogInt, rc, "Failure to get "
                        "'$(opt)' argument value", "opt=%s", option_name));
                    break;
                }
            }
        }

/* ASCP_OPTION */
        rc = ArgsOptionCount(self->args, ASCP_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" ASCP_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            const char *val = NULL;
            rc = ArgsOptionValue(self->args, ASCP_OPTION, 0, (const void **)&val);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" ASCP_OPTION "' argument value");
                break;
            }
            if (val != NULL) {
                char *sep = strchr(val, '|');
                if (sep == NULL) {
                    rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                    LOGERR(klogErr, rc,
                        "ascp-path expected in the following format:\n"
                        "--" ASCP_OPTION " \"<ascp-binary|private-key-file>\"\n"
                        "Examples:\n"
"--" ASCP_OPTION " \"/usr/bin/ascp|/etc/aspera_tokenauth_id_rsa\"\n"
"--" ASCP_OPTION " \"C:\\Program Files\\Aspera\\ascp.exe|C:\\Program Files\\Aspera\\etc\\aspera_tokenauth_id_rsa\"\n");
                    break;
                }
                else {
#if WINDOWS
                    rc_t r2 = 0;
                    char system[PATH_MAX] = "";
                    *sep = '\0';
                    r2 = KDirectoryPosixStringToSystemString(
                        self->dir, system, sizeof system, "%s", val);
                    if (r2 == 0)
                        self->ascp = string_dup_measure(system, NULL);
#else
                    self->ascp = string_dup(val, sep - val);
#endif
                    self->asperaKey = string_dup_measure(sep + 1, NULL);
                    self->ascpChecked = true;
                }
            }
        }

/* ASCP_PAR_OPTION */
        rc = ArgsOptionCount(self->args, ASCP_PAR_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" ASCP_PAR_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            rc = ArgsOptionValue(self->args,
                ASCP_PAR_OPTION, 0, (const void **)&self->ascpParams);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" ASCP_PAR_OPTION "' argument value");
                break;
            }
        }

/* FAIL_ASCP_OPTION */
        rc = ArgsOptionCount(self->args, FAIL_ASCP_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" FAIL_ASCP_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            self->forceAscpFail = true;
        }

        option_name = DRY_RUN_OPTION;
        {
            rc = ArgsOptionCount(self->args, option_name, &pcount);
            if (rc != 0) {
                PLOGERR(klogInt, (klogInt, rc,
                    "Failure to get '$(opt)' argument", "opt=%s", option_name));
                break;
            }
            if (pcount > 0)
                self->dryRun = true;
        }

/* PRGRS_OPTION */
        {
            rc = ArgsOptionCount(self->args, PRGRS_OPTION, &pcount);
            if (rc != 0) {
                LOGERR(klogErr, rc, "Failure to get '" PRGRS_OPTION "' argument");
                break;
            }
            if (pcount > 0)
                self->showProgress = true;
        }

/* HBEAT_OPTION */
        rc = ArgsOptionCount(self->args, HBEAT_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" HBEAT_OPTION "' argument");
            break;
        }

        if (pcount > 0) {
            double f;
            const char *val = NULL;
            rc = ArgsOptionValue(self->args, HBEAT_OPTION, 0, (const void **)&val);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" HBEAT_OPTION "' argument value");
                break;
            }
            f = atof(val) * 60000;
            self->heartbeat = (uint64_t)f;
        }

/* ROWS_OPTION */
        rc = ArgsOptionCount(self->args, ROWS_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" ROWS_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            rc = ArgsOptionValue(self->args, ROWS_OPTION, 0, (const void **)&self->rows);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" ROWS_OPTION "' argument value");
                break;
            }
        }

/* MINSZ_OPTION */
        {
            const char *val = "0";
            rc = ArgsOptionCount(self->args, MINSZ_OPTION, &pcount);
            if (rc != 0) {
                LOGERR(klogErr,
                    rc, "Failure to get '" MINSZ_OPTION "' argument");
                break;
            }
            if (pcount > 0) {
                rc = ArgsOptionValue(self->args, MINSZ_OPTION, 0, (const void **)&val);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" MINSZ_OPTION "' argument value");
                    break;
                }
            }
            self->minSize = _sizeFromString(val);
        }

/* SIZE_OPTION */
        {
            const char *val = DEFAULT_MAX_FILE_SIZE;
            rc = ArgsOptionCount(self->args, SIZE_OPTION, &pcount);
            if (rc != 0) {
                LOGERR(klogErr,
                    rc, "Failure to get '" SIZE_OPTION "' argument");
                break;
            }
            if (pcount > 0) {
                rc = ArgsOptionValue(self->args, SIZE_OPTION, 0, (const void **)&val);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" SIZE_OPTION "' argument value");
                    break;
                }
            }
            self->maxSize = _sizeFromString(val);
            if (self->maxSize == 0)
                self->maxSize = (uint64_t)~0; /* unlimited */
        }

        if (self->maxSize > 0 && self->minSize > self->maxSize) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            LOGERR(klogErr, rc, "Minimum file size is larger than maximum");
            break;
        }

/* TRANS_OPTION */
        {
            rc = ArgsOptionCount(self->args, TRANS_OPTION, &pcount);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" TRANS_OPTION "' argument");
                break;
            }

            if (pcount > 0) {
                bool ok = false;
                const char *val = NULL;
                rc = ArgsOptionValue(
                    self->args, TRANS_OPTION, 0, (const void **)&val);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" TRANS_OPTION "' argument value");
                    break;
                }
                assert(val);
                switch (val[0]) {
                case 'a':
                case 'f': {
                    const char ascp[] = "ascp";
                    const char fasp[] = "fasp";
                    if (string_cmp(val, string_measure(val, NULL),
                        ascp, sizeof ascp - 1, sizeof ascp - 1) == 0
                        ||
                        string_cmp(val, string_measure(val, NULL),
                            fasp, sizeof fasp - 1, sizeof fasp - 1) == 0
                        ||
                        (val[0] == 'a' && val[1] == '\0'))
                    {
                        self->noHttp = true;
                        ok = true;
                    }
                    break;
                }
                case 'h': {
                    const char http[] = "http";
                    if (string_cmp(val, string_measure(val, NULL),
                        http, sizeof http - 1, sizeof http - 1) == 0
                        || val[1] == '\0')
                    {
                        self->noAscp = true;
                        ok = true;
                    }
                    break;
                }
                }
                if (!ok) {
                    rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                    LOGERR(klogErr, rc,
                        "Bad '" TRANS_OPTION "' argument value");
                    break;
                }
            }
        }

option_name = TYPE_OPTION;
        {
            rc = ArgsOptionCount(self->args, option_name, &pcount);
            if (rc != 0) {
                PLOGERR(klogInt, (klogInt, rc,
                    "Failure to get '$(opt)' argument", "opt=%s", option_name));
                break;
            }
            if (pcount > 0) {
                rc = ArgsOptionValue(self->args,
                    option_name, 0, (const void **)&self->fileType);
                if (rc != 0) {
                    PLOGERR(klogInt, (klogInt, rc, "Failure to get "
                        "'$(opt)' argument value", "opt=%s", option_name));
                    break;
                }
            }
        }

#if ALLOW_STRIP_QUALS
/* STRIP_QUALS_OPTION */
        rc = ArgsOptionCount(self->args, STRIP_QUALS_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" STRIP_QUALS_OPTION "' argument");
            break;
        }

        if (pcount > 0) {
//          self->stripQuals = true;
        }
#endif

/* ELIM_QUALS_OPTION */
        rc = ArgsOptionCount(self->args, ELIM_QUALS_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" ELIM_QUALS_OPTION "' argument");
            break;
        }

        if (pcount > 0) {
            self->eliminateQuals = true;
        }

#if ALLOW_STRIP_QUALS
//      if (self->stripQuals && self->eliminateQuals) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            LOGERR(klogErr, rc, "Cannot specify both --" STRIP_QUALS_OPTION
                "and --" ELIM_QUALS_OPTION);
            break;
        }
#endif

/* OUT_DIR_OPTION */
        rc = ArgsOptionCount(self->args, OUT_DIR_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" OUT_DIR_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            rc = ArgsOptionValue(self->args,
                OUT_DIR_OPTION, 0, (const void **)&self->outDir);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OUT_DIR_OPTION "' argument value");
                break;
            }
        }

/* OUT_FILE_OPTION */
        rc = ArgsOptionCount(self->args, OUT_FILE_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr,
                rc, "Failure to get '" OUT_FILE_OPTION "' argument");
            break;
        }
        if (pcount > 0) {
            rc = ArgsOptionValue(self->args,
                OUT_FILE_OPTION, 0, (const void **)&self->outFile);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OUT_FILE_OPTION "' argument value");
                break;
            }
        }

        if (self->outFile != NULL)
            self->outDir = NULL;

/* ORDR_OPTION */
        rc = ArgsOptionCount(self->args, ORDR_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" ORDR_OPTION "' argument");
            break;
        }

        if (pcount > 0) {
            bool asAlias = false;
            const char *val = NULL;
            rc = ArgsOptionValueExt(self->args, ORDR_OPTION, 0,
                (const void **)&val, &asAlias);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" ORDR_OPTION "' argument value");
                break;
            }
            if (val != NULL && val[0] == 's')
                self->order = eOrderSize;
            else
                self->order = eOrderOrig;
            if (asAlias)
                self->orderOrOutFile = val;
        }

/* NGC_OPTION */
        {
            rc = ArgsOptionCount(self->args, NGC_OPTION, &pcount);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" NGC_OPTION "' argument");
                break;
            }

            if (pcount > 0) {
                const char *val = NULL;
                rc = ArgsOptionValue(self->args, NGC_OPTION,
                    0, (const void **)&val);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" NGC_OPTION "' argument value");
                    break;
                }
                self->ngc = val;
                KConfigSetNgcFile(self->ngc);
            }
        }

/* CART_OPTION */
        {
            rc = ArgsOptionCount(self->args, CART_OPTION, &pcount);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" CART_OPTION "' argument");
                break;
            }

            if (pcount > 0) {
                const char *val = NULL;
                rc = ArgsOptionValue(self->args, CART_OPTION,
                    0, (const void **)&val);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" CART_OPTION "' argument value");
                    break;
                }
                self->jwtCart = val;
            }
        }

/* KART_OPTION */
        {
            rc = ArgsOptionCount(self->args, KART_OPTION, &pcount);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" KART_OPTION "' argument");
                break;
            }

            if (pcount > 0) {
                const char *val = NULL;
                rc = ArgsOptionValue(self->args,
                    KART_OPTION, 0, (const void **)&val);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" KART_OPTION "' argument value");
                    break;
                }
                self->kart = val;
            }
        }

#if _DEBUGGING
/* TEXTKART_OPTION */
        rc = ArgsOptionCount(self->args, TEXTKART_OPTION, &pcount);
        if (rc != 0) {
            LOGERR(klogErr, rc,
                "Failure to get '" TEXTKART_OPTION "' argument");
            break;
        }

        if (pcount > 0) {
            const char *val = NULL;
            rc = ArgsOptionValue(self->args, TEXTKART_OPTION, 0, (const void **)&val);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" TEXTKART_OPTION "' argument value");
                break;
            }
            self->textkart = val;
        }

        if (self->kart != NULL && self->textkart != NULL) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            LOGERR(klogErr, rc, "Cannot specify both --" KART_OPTION
                "and --" TEXTKART_OPTION);
            break;
        }

#endif
    } while (false);

    STSMSG(STS_FIN, ("heartbeat = %ld Milliseconds", self->heartbeat));

    return rc;
}

rc_t CC Usage(const Args *args) {
    rc_t rc = 0;
    int i = 0;

    const char *progname = UsageDefaultName;
    const char *fullpath = UsageDefaultName;

    if (args == NULL) {
        rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    }
    else {
        rc = ArgsProgram(args, &fullpath, &progname);
    }
    if (rc != 0) {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary(progname);
    OUTMSG(("\n"));

    OUTMSG(("Options:\n"));
    for (i = 0; i < sizeof(OPTIONS) / sizeof(OPTIONS[0]); i++) {
        const OptDef * opt = &OPTIONS[i];
        const char * alias = opt->aliases;

        const char *param = NULL;

        if (alias != NULL) {
            if (strcmp(alias, FAIL_ASCP_ALIAS) == 0)
                continue; /* debug option */
            else if (strcmp(alias, ASCP_ALIAS) == 0)
                param = "ascp-binary|private-key-file";
            else if (strcmp(alias, CHECK_NEW_ALIAS) == 0)
                param = "yes|no|smart";
            else if (strcmp(alias, FORCE_ALIAS) == 0)
                param = "yes|no|all|ALL";
            else if (strcmp(alias, ORDR_ALIAS) == 0)
                param = "kart|size";
            else if (strcmp(alias, TRASN_ALIAS) == 0)
                param = "http|fasp|both";
            else if (
                strcmp(alias, RESUME_ALIAS) == 0 ||
                strcmp(alias, VALIDATE_ALIAS) == 0)
                param = "yes|no";
            else if (
                strcmp(alias, HBEAT_ALIAS) == 0 ||
                strcmp(alias, TYPE_ALIAS) == 0)
            {
                param = "value";
            }
            else if (strcmp(alias, OUT_DIR_ALIAS) == 0)
                param = "DIRECTORY";
            else if (strcmp(alias, ROWS_ALIAS) == 0)
                param = "rows";
            else if (strcmp(alias, SIZE_ALIAS) == 0
                || strcmp(alias, MINSZ_ALIAS) == 0)
            {
                param = "size";
            }
        }
        else if (
            strcmp(opt->name, ASCP_PAR_OPTION) == 0 ||
            strcmp(opt->name, LOCN_OPTION) == 0)
        {
            param = "value";
        }
        else if (
            strcmp(opt->name, CART_OPTION) == 0 ||
            strcmp(opt->name, NGC_OPTION) == 0 ||
            strcmp(opt->name, KART_OPTION) == 0
#if _DEBUGGING
         || strcmp(opt->name, TEXTKART_OPTION) == 0
#endif
        )
        {
            param = "PATH";
        }
        else if (strcmp(opt->name, OUT_FILE_OPTION) == 0) {
            param = "FILE";
            alias = OUT_FILE_ALIAS;
            continue; /* deprecated */
        }
        else if (strcmp(opt->name, DRY_RUN_OPTION) == 0)
            continue; /* debug option */

        if (alias != NULL) {
            if (strcmp(alias, ASCP_ALIAS) == 0 ||
                strcmp(alias, LIST_ALIAS) == 0 ||
                strcmp(alias, MINSZ_ALIAS) == 0 ||
                strcmp(opt->name, OUT_FILE_OPTION) == 0
                )
                OUTMSG(("\n"));
        }
        else if (strcmp(opt->name, ELIM_QUALS_OPTION) == 0)
            OUTMSG(("\n"));

        HelpOptionLine(alias, opt->name, param, opt->help);
    }

    OUTMSG(("\n"));
    HelpOptionsStandard();
    HelpVersion(fullpath, KAppVersion());

    return rc;
}

/*********** Finalize PrfMain object **********/
rc_t PrfMainFini(PrfMain *self) {
    rc_t rc = 0;

    assert(self);

    RELEASE(VResolver, self->resolver);
    RELEASE(VDBManager, self->mgr);
    RELEASE(KDirectory, self->dir);
    RELEASE(KRepositoryMgr, self->repoMgr);
    RELEASE(VFSManager, self->vfsMgr);
    RELEASE(KConfig, self->cfg);
    RELEASE(KNSManager, self->kns);
    RELEASE(Args, self->args);

    BSTreeWhack(&self->downloaded, bstWhack, NULL);

    free(self->buffer);

    free((void*)self->ascp);
    free((void*)self->asperaKey);
    free(self->ascpMaxRate);

    memset(self, 0, sizeof *self);

    return rc;
}

/*********** Initialize PrfMain object **********/
rc_t PrfMainInit(int argc, char *argv[], PrfMain *self) {
    rc_t rc = 0;

    assert(self);
    memset(self, 0, sizeof *self);

    self->heartbeat = 60000;
    /*  self->heartbeat = 69; */

    BSTreeInit(&self->downloaded);

    if (rc == 0) {
        rc = KDirectoryNativeDir(&self->dir);
        DISP_RC(rc, "KDirectoryNativeDir");
    }

    if (rc == 0) {
        rc = PrfMainProcessArgs(self, argc, argv);
    }

    if (rc == 0) {
        self->bsize = 1024 * 1024;
        self->buffer = malloc(self->bsize);
        if (self->buffer == NULL) {
            rc = RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
        }
    }

    if (rc == 0) {
        rc = VFSManagerMake(&self->vfsMgr);
        DISP_RC(rc, "VFSManagerMake");
        if (rc == 0)
            VFSManagerSetAdCaching(self->vfsMgr, true);
    }

    if (rc == 0)
    {
        rc = VFSManagerGetKNSMgr(self->vfsMgr, &self->kns);
        DISP_RC(rc, "VFSManagerGetKNSMgr");
    }

    if (rc == 0) {
        VResolver *resolver = NULL;
        rc = VFSManagerGetResolver(self->vfsMgr, &resolver);
        DISP_RC(rc, "VFSManagerGetResolver");
        VResolverRemoteEnable(resolver, vrAlwaysEnable);
        VResolverCacheEnable(resolver, vrAlwaysEnable);
        RELEASE(VResolver, resolver);
    }

    if (rc == 0) {
        rc = KConfigMake(&self->cfg, NULL);
        DISP_RC(rc, "KConfigMake");
    }

    if (rc == 0) {
        rc = KConfigMakeRepositoryMgrRead(self->cfg, &self->repoMgr);
        DISP_RC(rc, "KConfigMakeRepositoryMgrRead");
    }

    if (rc == 0) {
        rc = VFSManagerMakeResolver(self->vfsMgr, &self->resolver, self->cfg);
        DISP_RC(rc, "VFSManagerMakeResolver");
    }

    if (rc == 0) {
        VResolverRemoteEnable(self->resolver, vrAlwaysEnable);
        VResolverCacheEnable(self->resolver, vrAlwaysEnable);
    }

    if (rc == 0) {
        rc = VDBManagerMakeRead(&self->mgr, NULL);
        DISP_RC(rc, "VDBManagerMakeRead");
    }

    if (rc == 0) {
        srand((unsigned)time(NULL));
    }

    return rc;
}

