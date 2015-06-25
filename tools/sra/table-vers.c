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

#include "table-vers.vers.h" /* TABLE_VERS_VERS */

#include <kapp/main.h>

#include <vdb/manager.h> /* VDBManager */

#include <kdb/database.h> /* KDatabase */
#include <kdb/table.h> /* KDBTable */

#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h> /* RC */

#include <assert.h>
#include <stdio.h> /* printf */
#include <string.h> /* memset */

#define DISP_RC(rc,msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

typedef enum EOutType {
    eTxt,
    eXml
} EOutType;

typedef struct CmdLine {
	const char* table;
    EOutType outType;
} CmdLine;

ver_t CC KAppVersion(void) { return TABLE_VERS_VERS; }

#define OPTION_OUTPUT  "output"
#define ALIAS_OUTPUT   "o"

static const char* output_usage[] = {
    "Output type: one of (t | x)", "where 't' - text (default), 'x' - xml", NULL
};

OptDef Options[] =  {
     { OPTION_OUTPUT, ALIAS_OUTPUT, NULL, output_usage, 1, true, false }
};

const char UsageDefaultName[] = "vable-vers";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg("Usage:\n"
            "  %s [Options] <table>\n", progname);
}

rc_t CC Usage(const Args* args)
{
    rc_t rc = 0;
    const char* progname = UsageDefaultName;
    const char* fullpath = UsageDefaultName;

    assert(args);

    rc = ArgsProgram(args, &fullpath, &progname);
    if (rc)
    {    progname = fullpath = UsageDefaultName; }

    UsageSummary(progname);

    OUTMSG(("\nOptions\n"));

    HelpOptionLine(ALIAS_OUTPUT, OPTION_OUTPUT, "type", output_usage);

    HelpOptionsStandard();

    HelpVersion(fullpath, KAppVersion());

    return rc;
}

static rc_t CmdLineInit(const Args* args, CmdLine* cmdArgs)
{
    rc_t rc = 0;

    assert(args && cmdArgs);

    memset(cmdArgs, 0, sizeof *cmdArgs);

    while (rc == 0) {
        const char* outType = NULL;
        uint32_t pcount = 0;

        /* output type */
        rc = ArgsOptionCount(args, OPTION_OUTPUT, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing output type");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many output type parameters");
            break;
        }
        else if (pcount == 1) {
            rc = ArgsOptionValue(args, OPTION_OUTPUT, 0, &outType);
            if (rc) {
                LOGERR(klogErr, rc, "Failure retrieving output type");
                break;
            }
            else if (outType != NULL) {
                if (strncmp(outType, "x", 1) == 0) {
                    cmdArgs->outType = eXml;
                }
                else if (strncmp(outType, "t", 1) == 0) {
                    cmdArgs->outType = eTxt;
                }
                else {
                    rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                    LOGERR(klogErr, rc, "Bad output type value");
                    break;
                }
            }
        }

        /* table path parameter */
        rc = ArgsParamCount(args, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing table name");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc, "Missing table parameter");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many table parameters");
            break;
        }
        rc = ArgsParamValue(args, 0, &cmdArgs->table);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving table name");
            break;
        }

        break;
    }

    if (rc != 0) {
        MiniUsage (args);
    }

    return rc;
}

rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc = 0;
    Args* args = NULL;
    const VDBManager* mgr = NULL;

    CmdLine cmdArgs;

    LogLevelSet("info");

    rc = ArgsMakeAndHandle
        (&args, argc, argv, 1, Options, sizeof Options / sizeof(Options[0]));

    if (rc == 0) {
        rc = CmdLineInit(args, &cmdArgs);
    }
    if (rc == 0) {
        DBGMSG(DBG_APP,DBG_COND_1, ("out type = '%d'\n",cmdArgs.outType));
        PLOGMSG
            (klogInfo,(klogInfo, "Checking $(tbl)", "tbl=%s", cmdArgs.table));
        rc = VDBManagerMakeRead(&mgr, NULL);
        DISP_RC(rc, "while calling VDBManagerMakeRead");
    }
    if (rc == 0) {
        ver_t version = 0;
        rc = VDBManagerGetObjVersion(mgr, &version, cmdArgs.table);
        if (rc == 0) {
            uint32_t maj =  version >> 24;
            uint32_t min = (version >> 16) & 0xFF;
            uint32_t rel =  version & 0xFFFF;
            switch (cmdArgs.outType) {
                case eTxt:
                    OUTMSG(("v%d\n", maj));
                    break;
                case eXml:
                    OUTMSG(("<Object vers=\"%u\" path=\"%s\">",
                        maj, cmdArgs.table));
                    if (min || rel) {
                        OUTMSG(("<loader vers=\"%u.%u.%u\"/>", maj, min, rel));
                    }
                    OUTMSG(("</Object>\n"));
                    break;
            }
        } else {
            PLOGERR(klogErr,
                (klogErr, rc, "'$(path)'", "path=%s", cmdArgs.table));
        }
    }

    RELEASE(VDBManager, mgr);

    ArgsWhack(args);

    return rc;
}

/************************************ EOF ****************** ******************/
