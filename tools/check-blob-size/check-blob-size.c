/*******************************************************************************
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
 * =============================================================================
 */

#include "check-blob-size.vers.h" /* CHECK_BLOB_SIZE_VERS */
#include <kapp/main.h>
#include <kdb/manager.h>
#include <kdb/table.h>
#include <kdb/column.h>
#include <kdb/namelist.h> /* KTableListCol */
#include <klib/namelist.h> /* KNamelistRelease */
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h>
#include <assert.h>
#include <string.h> /* memset */
#include <stdio.h>  /* printf */
#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DISP_RC2(rc, name, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt,rc, "$(msg): $(name)","msg=%s,name=%s",msg,name)))
typedef struct CmdArgs {
    Args* args;
    uint32_t count;

    uint32_t i;
} CmdArgs;
ver_t CC KAppVersion(void) { return CHECK_BLOB_SIZE_VERS; }
static void summary(const char* progname) {
    OUTMSG(("Usage:\n"
            "  %s [Options] <table>\n", progname));
}
const char UsageDefaultName[] = "check-blob-size";
static const char* def_name = UsageDefaultName;
 
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

rc_t CC Usage(const Args* args)
{
    rc_t rc = 0;
    const char* progname = def_name;
    const char* fullpath = def_name;
    assert(args);
    rc = ArgsProgram(args, &fullpath, &progname);
    if (rc)
    {    progname = fullpath = def_name; }
    summary(progname);
    OUTMSG(("\nOptions\n"));
    HelpOptionsStandard();
    HelpVersion(fullpath, KAppVersion());
    return rc;
}
/* MINIUSAGE(def_name) */
static rc_t CmdArgsInit(int argc, char** argv, CmdArgs* cmdArgs)
{
    rc_t rc = 0;

    assert(argv && cmdArgs);

    memset(cmdArgs, 0, sizeof *cmdArgs);

    rc = ArgsMakeAndHandle(&cmdArgs->args, argc, argv, 0, NULL, 0);
    DISP_RC(rc, "while calling ArgsMakeAndHandle");

    if (rc == 0) {
        do {
            Args* args = cmdArgs->args;
            rc = ArgsParamCount(args, &cmdArgs->count);
            if (rc) {
                DISP_RC(rc, "while calling ArgsParamCount");
                break;
            }
            if (cmdArgs->count < 1) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
                LOGERR(klogErr, rc, "Missing table parameter");
                break;
            }
        } while (false);
    }

    if (rc != 0)
    {   MiniUsage(cmdArgs->args); }

    return rc;
}
static rc_t CmdArgsGetNextTable(CmdArgs* args, const char** table)
{
    rc_t rc = 0;
    assert(args && table);
    *table = NULL;
    if (args->i >= args->count) /* no more tables to give :( */
    {   return rc; }
    rc = ArgsParamValue(args->args, args->i, table);
    if (rc) {
        PLOGERR(klogInt, (klogInt, rc,
            "while calling ArgsParamValue($(i))", "i=%d", args->i));
    }
    ++args->i;
    return rc;
}
static rc_t CmdArgsDestroy(CmdArgs* args)
{
    rc_t rc = 0;
    assert(args);
    rc = ArgsWhack(args->args);
    DISP_RC(rc, "while calling ArgsWhack");
    args->args = NULL;
    return rc;
}
static rc_t CheckBlob(const char* name, const KColumn* col, int64_t id,
    int64_t* lastId, size_t* blobSize)
{
    rc_t rc = 0;
    int64_t first = 0;
    uint32_t count = 0;
    const KColumnBlob* blob = NULL;
    assert(col && lastId && blobSize);
    rc = KColumnOpenBlobRead(col, &blob, id);
    if (rc != 0) {
        PLOGERR(klogInt, (klogInt, rc,
            "while calling KColumnOpenBlobRead($(col), $(id))",
            "col=%s,id=%d", name, id));
    }
    else {
        rc = KColumnBlobIdRange(blob, &first, &count);
        if (rc != 0) {
            PLOGERR(klogInt, (klogInt, rc,
                "while calling KColumnBlobIdRange($(col), $(id))",
                "col=%s,id=%d", name, id));
        }
    }
    if (rc == 0) {
        size_t num_read = 0;
        size_t remaining = 0;
        rc = KColumnBlobRead(blob, 0, NULL, 0, &num_read, &remaining);
        if (rc != 0) {
            PLOGERR(klogInt, (klogInt, rc,
                "while calling KColumnBlobRead($(col), $(id))",
                "col=%s,id=%d", name, id));
        }
        else {
            assert(num_read == 0 && remaining);
            *lastId = first + count - 1;
            *blobSize = remaining;
            DBGMSG(DBG_APP, DBG_COND_2,
                ("first %d, count %d, size %d\n", first, count, remaining));
        }
    }
    {
        rc_t rc2 = KColumnBlobRelease(blob);
        DISP_RC(rc2, "while calling KColumnBlobRelease");
        if (rc == 0)
        {   rc = rc2; }
        blob = NULL;
    }
    return rc;
}
static rc_t CheckCol(const KTable* tbl,
    const char* name, size_t* maxBlobSize)
{
    rc_t rc = 0;
    uint64_t first = 0;
    uint64_t count = 0;
    const KColumn* col = NULL;
    assert(tbl && name && maxBlobSize);
    if (rc == 0) {
        rc = KTableOpenColumnRead(tbl, &col, "%s", name);
        DISP_RC2(rc, name, "while opening column");
    }
    if (rc == 0) {
        rc = KColumnIdRange(col, &first, &count);
        DISP_RC2(rc, name, "while calling KColumnIdRange");
    }
    if (rc == 0) {
        uint64_t nBlobs = 0;
        uint64_t i = 0;
        *maxBlobSize = 0;
        DBGMSG(DBG_APP, DBG_COND_1,
            ("Column %18s: %d(%d)\n", name, first, count));
        for (i = first; i < first + count && rc == 0;) {
            int64_t lastId = 0;
            rc = Quitting();
            if (rc)
            {   LOGMSG(klogWarn, "Interrupted"); }
            else {
                size_t blobSize = 0;
                rc = CheckBlob(name, col, i, &lastId, &blobSize);
                i = lastId + 1;
                ++nBlobs;
                if (rc == 0 && blobSize > *maxBlobSize)
                {   *maxBlobSize = blobSize; }
            }
        }
        if (rc == 0) {
            PLOGMSG(klogInfo, (klogInfo,
                "'$(col): $(blobs) blobs (max $(max))",
                "col=%s,blobs=%d,max=%d", name, nBlobs, *maxBlobSize));
        }
    }
    {
        rc_t rc2 = KColumnRelease(col);
        DISP_RC(rc2, "while calling KColumnRelease");
        if (rc == 0)
        {   rc = rc2; }
        col = NULL;
    }
    return rc;
}
static rc_t Check(const KTable* tbl, size_t* maxBlobSize)
{
    rc_t rc = 0;
    uint32_t cCount = 0;
    struct KNamelist* columns = NULL;
    assert(tbl && maxBlobSize);
    rc = KTableListCol(tbl, &columns);
    DISP_RC(rc, "while calling KTableListCol");
    if (rc == 0) {
        rc = KNamelistCount(columns, &cCount);
        DISP_RC(rc, "while calling KTableColNamelistCount");
    }
    if (rc == 0) {
        uint32_t iCol = 0;
        *maxBlobSize = 0;
        for (iCol = 0; iCol < cCount && rc == 0; ++iCol) {
            const char* name = NULL;
            if (rc == 0) {
                rc = KNamelistGet(columns, iCol, &name);
                if (rc) {
                    PLOGERR(klogInt, (klogInt, rc,
                        "while calling KTableColNamelistGet($(i))",
                        "i=%d", iCol));
                }
            }
            if (rc == 0) {
                size_t maxColBlobSize = 0;
                rc = CheckCol(tbl, name, &maxColBlobSize);
                if (rc == 0 && maxColBlobSize > *maxBlobSize)
                {   *maxBlobSize = maxColBlobSize; }
            }
        }
    }
    {
        rc_t rc2 = KNamelistRelease(columns);
        DISP_RC(rc2, "while calling KNamelistRelease");
        if (rc == 0)
        {   rc = rc2; }
        columns = NULL;
    }
    return rc;
}
rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc = 0;

    CmdArgs args;
    const KDBManager* mgr = NULL;

    assert(argc && argv);

    def_name = argv[0];

    rc = LogLevelSet("info");
    DISP_RC(rc, "while calling LogLevelSet");

    rc = CmdArgsInit(argc, argv, &args);
    if (rc == 0) {
        rc = KDBManagerMakeRead(&mgr, NULL);
        DISP_RC(rc, "while calling KDBManagerMakeRead");
    }
    while (rc == 0) {
        const char* table = NULL;
        const KTable* tbl = NULL;
        rc = CmdArgsGetNextTable(&args, &table);
        if (table == NULL)
        {   break; }
        if (rc == 0) {
            rc = KDBManagerOpenTableRead(mgr, &tbl, "%s", table);
            DISP_RC2(rc, table, "while opening table");
        }
        if (rc == 0) {
            size_t maxBlobSize = 0;
            PLOGMSG(klogInfo, (klogInfo, "'$(table)'", "table=%s", table));
            rc = Check(tbl, &maxBlobSize);
            if (rc == 0)
            {   printf("%lu\t%s\n", maxBlobSize, table); }
        }
        {
            rc_t rc2 = KTableRelease(tbl);
            DISP_RC(rc2, "while calling KTableRelease");
            if (rc == 0)
            {   rc = rc2; }
            tbl = NULL;
        }
    }
    {
        rc_t rc2 = KDBManagerRelease(mgr);
        DISP_RC(rc2, "while calling KDBManagerRelease");
        if (rc == 0)
        {   rc = rc2; }
        mgr = NULL;
    }
    {
        rc_t rc2 = CmdArgsDestroy(&args);
        if (rc == 0)
        {   rc = rc2; }
    }
    return rc;
}
