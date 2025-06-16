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
* ===========================================================================
*
*/

#include <vdb/manager.h> /* VDBManager */
#include <vdb/database.h> /* VDatabase */
#include <vdb/dependencies.h> /* VDBDependencies */
#include <vdb/table.h> /* VTable */
#include <vdb/cursor.h> /* VCursor */
#include <vdb/vdb-priv.h> /* VDBManagerOpenKDBManagerRead */

#include <kapp/main.h>
#include <kapp/vdbapp.h>

#include <kfg/config.h> /* KConfig */

#include <kdb/manager.h> /* KDBManagerRelease */
#include <kdb/namelist.h> /* KMDataNodeListChild */
#include <kdb/meta.h> /* KMetadata */

#include <klib/container.h> /* BSTree */
#include <klib/sort.h> /* ksort */
#include <klib/out.h> /* OUTMSG */
#include <klib/log.h> /* LOGERR */
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h> /* RC */

#include <assert.h>
#include <stdio.h> /* sscanf */
#include <stdlib.h> /* free */
#include <string.h> /* strcmp */

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DISP_RC2(rc, name, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt,rc, "$(msg): $(name)","msg=%s,name=%s",msg,name)))
#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

typedef struct Params {
    const char* dbPath;

    bool paramBamHeader;
    bool paramQuality;
    bool paramRef;

    bool paramHeaders;
} Params;

#define ALIAS_ALL    "a"
#define OPTION_ALL   "all"
static const char* USAGE_ALL[] = { "print all information", NULL };

#define ALIAS_BAM    "b"
#define OPTION_BAM   "bam"
static const char* USAGE_BAM[] = { "print bam header (if present)", NULL };

#define ALIAS_QUA    "Q"
#define OPTION_QUA   "qual"
static const char* USAGE_QUA[]
                       = { "print quality statistics (if present)", NULL };

#define ALIAS_REF    "r"
#define OPTION_REF   "ref"
static const char* USAGE_REF[] = { "print refseq information [default]", NULL };

#define ALIAS_HEA   "H"
#define OPTION_HEA  "headers"
static const char* USAGE_HEA[] = { "print headers for output blocks", NULL };

#define ALIAS_NGC   NULL
#define OPTION_NGC  "ngc"
static const char* USAGE_NGC[] = { "path to ngc file", NULL };

OptDef Options[] =
{
      { OPTION_ALL, ALIAS_ALL, NULL, USAGE_ALL, 1, false, false }
    , { OPTION_BAM, ALIAS_BAM, NULL, USAGE_BAM, 1, false, false }
    , { OPTION_QUA, ALIAS_QUA, NULL, USAGE_QUA, 1, false, false }
    , { OPTION_REF, ALIAS_REF, NULL, USAGE_REF, 1, false, false }
    , { OPTION_HEA, ALIAS_HEA, NULL, USAGE_HEA, 1, false, false }
    , { OPTION_NGC, ALIAS_NGC, NULL, USAGE_NGC, 1, true , false }
};

rc_t CC UsageSummary (const char * progname) {
    return KOutMsg (
        "Usage:\n"
        "  %s [options] <db-path>\n"
        "\n"
        "Summary:\n"
        "  Print database alignment information\n"
        "\n", progname);
 }

static const char* param_usage[] = { "Path to the database", NULL };

const char UsageDefaultName[] = "align-info";

rc_t CC Usage(const Args* args) {
    rc_t rc = 0 ;

    const char* progname = UsageDefaultName;
    const char* fullpath = UsageDefaultName;

    if (args == NULL)
    {    rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull); }
    else
    {    rc = ArgsProgram(args, &fullpath, &progname); }

    UsageSummary(progname);

    KOutMsg("Parameters:\n");

    HelpParamLine ("db-path", param_usage);

    KOutMsg ("\nOptions:\n");

    HelpOptionLine (ALIAS_ALL, OPTION_ALL, NULL, USAGE_ALL);
    HelpOptionLine (ALIAS_REF, OPTION_REF, NULL, USAGE_REF);
    HelpOptionLine (ALIAS_BAM, OPTION_BAM, NULL, USAGE_BAM);
    HelpOptionLine (ALIAS_QUA, OPTION_QUA, NULL, USAGE_QUA);
    HelpOptionLine (ALIAS_HEA, OPTION_HEA, NULL, USAGE_HEA);
    HelpOptionLine (ALIAS_NGC, OPTION_NGC, "path",USAGE_NGC);

    KOutMsg ("\n");

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

static rc_t bam_header(const VDatabase* db) {
    rc_t rc = 0;
    const char path[] = "BAM_HEADER";
    const KMetadata* meta = NULL;
    const KMDataNode* node = NULL;
    char* buffer = NULL;
    assert(db);
    if (rc == 0) {
        rc = VDatabaseOpenMetadataRead(db, &meta);
        DISP_RC(rc, "while calling VDatabaseOpenMetadataRead");
    }
    if (rc == 0) {
        rc = KMetadataOpenNodeRead(meta, &node, "%s", path);
        if (GetRCState(rc) == rcNotFound)
        {   rc = 0; }
        else {
            DISP_RC2(rc, path, "while calling KMetadataOpenNodeRead");
            if (rc == 0) {
                int i = 0;
                size_t bsize = 0;
                size_t size = 1024;
                for (i = 0; i < 2; ++i) {
                    free(buffer);
                    bsize = size + 1;
                    buffer = malloc(bsize);
                    if (buffer == NULL) {
                        rc = RC(rcExe, rcStorage, rcAllocating,
                            rcMemory, rcExhausted);
                    }
                    else {
                        rc = KMDataNodeReadCString(node, buffer, bsize, &size);
                        if (rc == 0) {
                            break;
                        }
                        else if (i == 0
                            && GetRCObject(rc) == (enum RCObject)rcBuffer
                            && GetRCState (rc) ==          rcInsufficient)
                        {
                            rc = 0;
                        }
                    }
                    DISP_RC2(rc, path, "while calling KMDataNodeReadCString");
                }
            }
        }
    }
    if (rc == 0 && buffer)
    {   OUTMSG(("BAM_HEADER: {\n%s}\n\n", buffer)); }
    DESTRUCT(KMDataNode, node);
    DESTRUCT(KMetadata, meta);
    free(buffer);
    return rc;
}

static int64_t CC sort_callback(const void* p1, const void* p2, void* data) {
    int i1 = *(int*) p1;
    int i2 = *(int*) p2;
    return i1 - i2;
}
static rc_t qual_stats(const Params* prm, const VDatabase* db) {
    rc_t rc = 0;
    const char tblName[] = "SEQUENCE";
    const VTable* tbl = NULL;
    const KMetadata* meta = NULL;
    const KMDataNode* node = NULL;
    assert(prm && db);
    if (rc == 0) {
        rc = VDatabaseOpenTableRead(db, &tbl, "%s", tblName);
        DISP_RC2(rc, tblName, "while calling VDatabaseOpenTableRead");
    }
    if (rc == 0) {
        rc = VTableOpenMetadataRead(tbl, &meta);
        DISP_RC2(rc, tblName, "while calling VTableOpenMetadataRead");
    }
    if (rc == 0) {
        bool found = false;
        const char path[] = "STATS/QUALITY";
        rc = KMetadataOpenNodeRead(meta, &node, path);
        if (rc == 0)
        {   found = true; }
        else if (GetRCState(rc) == rcNotFound)
        {   rc = 0; }
        DISP_RC2(rc, path, "while calling KMetadataOpenNodeRead");
        if (found) {
            uint32_t i = 0;
            int nbr = 0;
            uint32_t count = 0;
            KNamelist* names = NULL;
            int* quals = NULL;
            if (rc == 0) {
                rc = KMDataNodeListChild(node, &names);
                DISP_RC2(rc, path, "while calling KMDataNodeListChild");
            }
            if (rc == 0) {
                rc = KNamelistCount(names, &count);
                DISP_RC2(rc, path, "while calling KNamelistCount");
                if (rc == 0 && count > 0) {
                    quals = calloc(count, sizeof *quals);
                    if (quals == NULL) {
                        rc = RC(rcExe,
                            rcStorage, rcAllocating, rcMemory, rcExhausted);
                    }
                }
            }
            for (i = 0; i < count && rc == 0; ++i) {
             /* uint64_t u = 0;
                const KMDataNode* n = NULL; */
                const char* nodeName = NULL;
                const char* name = NULL;
                rc = KNamelistGet(names, i, &nodeName);
                DISP_RC2(rc, path, "while calling KNamelistGet");
                if (rc)
                {   break; }
                name = nodeName;
             /* rc = KMDataNodeOpenNodeRead(node, &n, name);
                DISP_RC(rc, name);
                if (rc == 0) {
                    rc = KMDataNodeReadAsU64(n, &u);
                    DISP_RC(rc, name);
                } */
                if (rc == 0) {
                    char* c = strchr(name, '_');
                    if (c != NULL && *(c + 1) != '\0') {
                        name = c + 1;
                        if (sscanf(name, "%d", &quals[i]) != 1) {
                            rc = RC(rcExe,
                                rcNode, rcParsing, rcName, rcUnexpected);
                            PLOGERR(klogInt,
                                (klogInt, rc, "$(name)", "name=%s", nodeName));
                        }
                    }
                    /* OUTMSG(("QUALITY %s %lu\n", name, u)); */
                }
             /* DESTRUCT(KMDataNode, n); */
            }
            if (rc == 0 && count > 0)
            {   ksort(quals, count, sizeof *quals, sort_callback, NULL); }

            if (rc == 0) {
                if (prm->paramHeaders) {
                    OUTMSG(("Quality statistics - rows per value\n"));
                    OUTMSG(("Quality values:"));
                    for (i = 0; i <= 40; ++i) {
                        OUTMSG(("\t%d", i));
                    }
                    OUTMSG(("\n"));
                }
                OUTMSG(("%s", prm->dbPath));
            }

            for (i = 0, nbr = 0; i < count && rc == 0; ++i, ++nbr) {
                uint64_t u = 0;
                char name[64];
                const KMDataNode* n = NULL;
                int len = snprintf(name, sizeof(name), "PHRED_%d", quals[i]);
                assert(len < sizeof(name));
                rc = KMDataNodeOpenNodeRead(node, &n, "%s", name);
                DISP_RC(rc, name);
                if (rc == 0) {
                    rc = KMDataNodeReadAsU64(n, &u);
                    DISP_RC(rc, name);
                    if (rc == 0) {
                        while (nbr < quals[i]) {
                            OUTMSG(("\t0"));
                            ++nbr;
                        }
                        OUTMSG(("\t%lu", u));
                    /*  OUTMSG(("QUALITY %d %lu\n", quals[i], u)); */
                    }
                }
                DESTRUCT(KMDataNode, n);
            }
            while (rc == 0 && nbr <= 40) {
                OUTMSG(("\t0"));
                nbr++;
            }
            if (rc == 0) {
                OUTMSG(("\n"));
            }
            DESTRUCT(KNamelist, names);
        }
    }
    DESTRUCT(KMDataNode, node);
    DESTRUCT(KMetadata, meta);
    DESTRUCT(VTable, tbl);
    return rc;
}

static rc_t align_info(const Params* prm) {
    rc_t rc = 0;

    const VDatabase* db = NULL;
    const VDBManager* mgr = NULL;
    const KDBManager *kmgr = NULL;
    bool is_db = false;

    if (prm == NULL)
    {   return RC(rcExe, rcQuery, rcExecuting, rcParam, rcNull); }

    if (rc == 0) {
        rc = VDBManagerMakeRead(&mgr, NULL);
        DISP_RC(rc, "while calling VDBManagerMakeRead");
    }

    if (rc == 0) {
        rc = VDBManagerOpenKDBManagerRead(mgr, &kmgr);
        DISP_RC(rc, "while calling VDBManagerOpenKDBManagerRead");
    }

    if (rc == 0) {
        rc = VDBManagerOpenDBRead(mgr, &db, NULL, "%s", prm->dbPath);
        if (rc == 0) {
            is_db = true;
        }
        else if (rc == SILENT_RC(rcDB, rcMgr, rcOpening, rcDatabase, rcIncorrect)) {
            PLOGMSG(klogWarn, (klogWarn,
                "'$(path)' is not a database", "path=%s", prm->dbPath));
            rc = 0;
        }
        else {
            PLOGERR(klogErr,
                (klogErr, rc, "$(path)", "path=%s", prm->dbPath));
        }
    }

    if (is_db) {
        if (rc == 0) {
            if (prm->paramRef) {
                const VDBDependencies* dep = NULL;
                uint32_t count = 0;
                int i = 0;
                if (prm->paramHeaders) {
                    OUTMSG(("Alignments:\n"));
                }
                rc = VDatabaseFindDependencies(db, &dep);
                DISP_RC2(rc, prm->dbPath,
                    "while calling VDatabaseFindDependencies");
                if (rc == 0) {
                    rc = VDBDependenciesCount(dep, &count);
                    DISP_RC2(rc, prm->dbPath,
                        "while calling VDBDependenciesCount");
                }
                for (i = 0; i < count && rc == 0; ++i) {
                    bool circular = false;
                    const char* name = NULL;
                    const char* path = NULL;
                    const char* remote = NULL;
                    bool local = false;
                    const char* seqId = NULL;
                    rc = VDBDependenciesCircular(dep, &circular, i);
                    if (rc != 0) {
                        DISP_RC2(rc, prm->dbPath,
                            "while calling VDBDependenciesCircular");
                        break;
                    }
                    rc = VDBDependenciesName(dep, &name, i);
                    if (rc != 0) {
                        DISP_RC2(rc, prm->dbPath,
                            "while calling VDBDependenciesName");
                        break;
                    }
                    rc = VDBDependenciesPath(dep, &path, i);
                    if (rc != 0) {
                        DISP_RC2(rc, prm->dbPath,
                            "while calling VDBDependenciesPath");
                        break;
                    }
                    rc = VDBDependenciesLocal(dep, &local, i);
                    if (rc != 0) {
                        DISP_RC2(rc, prm->dbPath,
                            "while calling VDBDependenciesLocal");
                        break;
                    }
                    rc = VDBDependenciesSeqId(dep, &seqId, i);
                    if (rc != 0) {
                        DISP_RC2(rc, prm->dbPath,
                            "while calling VDBDependenciesSeqId");
                        break;
                    }
                    rc = VDBDependenciesPathRemote(dep, &remote, i);
                    if (rc != 0) {
                        DISP_RC2(rc, prm->dbPath,
                            "while calling VDBDependenciesRemote");
                        break;
                    }
                    OUTMSG(("%s,%s,%s,%s", seqId, name,
                        (circular ? "true" : "false"),
                        (local ? "local" : "remote")));
                    if (path && path[0]) {
                        OUTMSG((":%s", path));
                    }
                    else if (remote && remote[0]) {
                        OUTMSG(("::%s", remote));
                    }
                    OUTMSG(("\n"));
                }
                DESTRUCT(VDBDependencies, dep);
                if (prm->paramHeaders) {
                    OUTMSG(("\n"));
                }
            }

            if (prm->paramBamHeader) {
                rc_t rc3 = bam_header(db);
                if (rc3 != 0 && rc == 0)
                {   rc = rc3; }
            }

            if (prm->paramQuality) {
                rc_t rc3 = qual_stats(prm, db);
                if (rc3 != 0 && rc == 0)
                {   rc = rc3; }
            }
        }

    }

    DESTRUCT(KDBManager, kmgr);
    DESTRUCT(VDBManager, mgr);
    DESTRUCT(VDatabase, db);

    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args* args = NULL;

    Params prm;
    memset(&prm, 0, sizeof prm);

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = 0;
    do {
        uint32_t pcount = 0;

        rc = ArgsMakeAndHandle(&args, argc, argv, 1,
            Options, sizeof Options / sizeof (OptDef));
        if (rc) {
            LOGERR(klogErr, rc, "While calling ArgsMakeAndHandle");
            break;
        }
        rc = ArgsParamCount(args, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "While calling ArgsParamCount");
            break;
        }
        if (pcount < 1) {
            MiniUsage(args);
            DESTRUCT(Args, args);
            exit(1);
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many database parameters");
            break;
        }
        rc = ArgsParamValue(args, 0, (const void **)&prm.dbPath);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving database name");
            break;
        }

        rc = ArgsOptionCount (args, OPTION_ALL, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_ALL "' argument");
            break;
        }
        if (pcount)
        {   prm.paramBamHeader = prm.paramQuality = prm.paramRef = true; }

        rc = ArgsOptionCount (args, OPTION_BAM, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_BAM "' argument");
            break;
        }
        if (pcount)
        {   prm.paramBamHeader = true; }

        rc = ArgsOptionCount (args, OPTION_QUA, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_QUA "' argument");
            break;
        }
        if (pcount)
        {   prm.paramQuality = true; }

        rc = ArgsOptionCount (args, OPTION_REF, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_REF "' argument");
            break;
        }
        if (pcount)
        {   prm.paramRef = true; }

        if (!prm.paramBamHeader && !prm.paramQuality && !prm.paramRef)
        {   prm.paramRef = true; }

        rc = ArgsOptionCount (args, OPTION_HEA, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_HEA "' argument");
            break;
        }
        if (pcount) {
            prm.paramHeaders = true;
        }

/* OPTION_NGC */
        {
            const char * dummy = NULL;
            rc = ArgsOptionCount(args, OPTION_NGC, &pcount);
            if (rc != 0) {
                LOGERR(klogErr, rc, "Failure to get '" OPTION_NGC "' argument");
                break;
            }
            if (pcount != 0) {
                rc = ArgsOptionValue(args, OPTION_NGC, 0,
                    (const void **)&dummy);
                if (rc != 0) {
                    LOGERR(klogErr, rc,
                        "Failure to get '" OPTION_NGC "' argument");
                    break;
                }
                KConfigSetNgcFile(dummy);
            }
        }
    } while (false);

    if (rc == 0)
    {   rc = align_info(&prm); }

    DESTRUCT(Args, args);
    return VDB_TERMINATE( rc );
}

/************************************* EOF ************************************/
