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

#include <stdio.h>
#include <string.h>

#include <kapp/args.h>

#include <klib/defs.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kdb/manager.h>
#include <kdb/column.h>
#include <kapp/main.h>

const char UsageDefaultName[] = "txt2kdb";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [Options] <File> <Column>\n"
                    "\n"
                    "Summary:\n"
                    "  Create a physical database column from a text file.\n"
                    "\n", progname);
}

#define OPTION_APPEND "append"
#define OPTION_FORCE  "force"
#define OPTION_BEGIN  "begin"
#define OPTION_END    "end"
#define ALIAS_APPEND  "a"
#define ALIAS_FORCE   "f"
#define ALIAS_BEGIN   "b"
#define ALIAS_END     "e"

static const char * append_usage[] =
{
    "(no parameter) this will cause to append the",
    "text file to an existing KColumn.  If the",
    "file does not already exist it will be",
    "created.",
    NULL
};
static const char * force_usage [] =
{
    "(no parameter) this will cause to over-write",
    "existing files.  Without this option the",
    "program will fail if the KColumn already",
    "exists and append mode is not selected",
    NULL
};
static const char * begin_usage [] =
{
    "Begin include only lines starting from this",
    "line in the column.  The first line is line",
    "1 (not 0).",
    NULL
};
static const char * end_usage   [] =
{
    "Stop including lines after this line in the",
    "column.  The first line is line 1 (not 0).",
    NULL
};


OptDef Options[] =
{
    { OPTION_APPEND, ALIAS_APPEND, NULL, append_usage, 0, false, false },
    { OPTION_FORCE,  ALIAS_FORCE,  NULL, force_usage,  0, false, false },
    { OPTION_BEGIN,  ALIAS_BEGIN,  NULL, begin_usage,  1, true,  false },
    { OPTION_END,    ALIAS_END,    NULL, end_usage,    1, true,  false }
};


rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    OUTMSG (("File:\n"
             "  The text file should be ASCII or UTF-8 using LF, CR or CR-LF\n"
             "  line termination.  Each text line will be put into the\n"
             "  KColumn as a separate Row.  Each Row will be in its own\n"
             "  blob.\n"
             "\n"
             "Column:\n"
             "  The KColumn is either an existing KColumn or a path to one\n"
             "  that can be created.\n"
                 "\n"
             "  Both paths should be relative to the current directory or full\n"
             "  from root \n"
             "\n"
             "Options:\n"));

    HelpOptionLine (ALIAS_BEGIN, OPTION_BEGIN, "Start", begin_usage);

    HelpOptionLine (ALIAS_END, OPTION_END, "Stop", end_usage);

    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, force_usage);

    HelpOptionLine (ALIAS_APPEND, OPTION_APPEND, NULL, append_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


struct txt2kdbglobals
{
    uint64_t     begin;
    uint64_t     end;

    KDirectory * dir;
    const KFile * txt;
    KDBManager * mgr;
    KColumn * col;
    const char * txtpath;
    const char * colpath;

    bool force;
    bool append;
    bool begin_seen;
    bool end_seen;
} G;

void txt2kdb_release (void)
{
    KDirectoryRelease (G.dir);
    KFileRelease (G.txt);
    KDBManagerRelease (G.mgr);
    KColumnRelease (G.col);
}

rc_t txt2kdb_kfs (void)
{
    rc_t rc;

    G.dir = NULL;
    G.txt = NULL;
    G.mgr = NULL;
    G.col = NULL;

    /* -----
     * Mount the native filesystem as root
     */
    rc = KDirectoryNativeDir (&G.dir);
    if (rc != 0)
    {
        G.dir = NULL;
	LOGMSG (klogFatal, "Failed to open native KDirectory");
    }
    else
    {
        rc = KDirectoryOpenFileRead (G.dir, &G.txt, "%s", G.txtpath);
        if (rc != 0)
        {
            G.txt = NULL;
            PLOGERR (klogFatal, (klogFatal, rc, "Unable to open file at $(F)", PLOG_S(F), G.txtpath));
        }
        else
        {
            rc = KDBManagerMakeUpdate (&G.mgr, G.dir);
            if (rc)
            {
                G.mgr = NULL;
                LOGERR (klogFatal, rc, "Unable to create a KDBManager at the current directory");
            }
            else
            {
                KCreateMode kcm;
                KPathType kpt;
                const char * err = "";

                kpt = KDirectoryPathType (G.dir, "%s", G.colpath) & ~ kptAlias;
                kcm = kcmCreate;
/* Force means replace if exists */
/* Append means open in append mode if it exists */
                switch (kpt)
                {
                case kptNotFound:
                    kcm = kcmCreate;
                    break;

                default:
                    err = "Unknown";
                    rc = RC (rcExe, rcNoTarg, rcAccessing, rcPath, rcInvalid);
                    break;

                case kptBadPath:
                    err = "Bad Path";
                    rc = RC (rcExe, rcNoTarg, rcAccessing, rcPath, rcInvalid);
                    break;

                case kptFile:
                case kptCharDev:
                case kptBlockDev:
                case kptFIFO:
                    err = "Must be a Directory";
                    rc = RC (rcExe, rcNoTarg, rcAccessing, rcPath, rcInvalid);
                    break;

                case kptDir:
                    kcm = kcmCreate;

                    if (G.append)
                    {
                        kcm = kcmOpen;
                    }
                    else if (G.force)
                    {
                        kcm = kcmInit;
                    }
                    break;
                }
                if (rc == 0)
                {
                    rc = KDBManagerCreateColumn (G.mgr, &G.col, kcm, kcsNone, 0, "%s", G.colpath);
                    if (rc)
                        err = "Manager can not open column";
                }
                if (rc)
                {
                    PLOGERR (klogFatal, (klogFatal, rc, "Cannot open KColumn $(P) because $(R)", PLOG_2(PLOG_S(P),PLOG_S(R)),
                                         G.colpath, err));
                }
            }
        }
    }
    return rc;
}

/* If the begin parameter was set check if the rowid is equal or above it */
bool rowid_lower_range (uint64_t rowid)
{
    if (G.begin_seen && (rowid < G.begin))
        return false;
    return true;
}

/* If the end parameter was set check if the rowid is equal or below it */
bool rowid_upper_range (uint64_t rowid)
{
    if (G.end_seen && (rowid > G.end))
        return false;
    return true;
}
rc_t txt2kdb_io()
{
    rc_t rc = 0;
    uint64_t rowid = 1;
    uint64_t tix = 0;
    KColumnBlob * blob;
    bool blobopen = false;

    while (rc == 0)
    {
        size_t num_read;
        uint8_t buffer  [4096];
        uint8_t * limit;
        uint8_t * append_start = buffer;
        uint8_t * cursor = buffer;
        bool eol = true;

        /* quit if we are already past the end of the range */
        if ( ! rowid_upper_range(rowid))
            break;

        /* read a buffer full.  It may straddle rows. */
        rc = KFileRead (G.txt, tix, buffer, sizeof buffer, &num_read);
        if (rc)
        {
            PLOGERR (klogFatal, (klogFatal, rc, "Read failed starting $(P)", PLOG_U64(P), tix));
            break;
        }
        /* break at EOF */
        if (num_read == 0)
            break;

        /* scan across the buffer looking for lines */
        for (limit = buffer + num_read; cursor < limit; append_start = cursor)
        {
            /* if we are at the beginning of a line (end of previous line or start of first */
            if (eol)
            {
                /* if we are within the pass thru range create a blob */
                if (rowid_lower_range(rowid) && rowid_upper_range(rowid))
                {
                    rc = KColumnCreateBlob (G.col, &blob);
                    if (rc)
                    {
                        PLOGERR (klogFatal, (klogFatal, rc, "Failed to create Blob for row $(R) at $(P)",
                                             PLOG_2(PLOG_U64(R),PLOG_U64(P)), rowid, tix));
                        continue;
                    }
                    blobopen = true;
                }
                /* clear the flag */
                eol = false;
            }

            /* this blob append will go until end of buffer or end of line */
            for ( ; cursor < limit; ++ cursor, ++tix)
            {
                /* if we hit a NewLine flag it and break for append */
                if (*cursor == '\n')
                {
                    eol = true;
                    ++cursor;
                    ++tix;
                    break;
                }
            }

            /* if we are within the selected range append this to the open blob
             * ir might be the first append, a middle append, a last append or only append */
            if (blobopen)
            {
                rc = KColumnBlobAppend (blob, append_start, cursor - append_start);
                if (rc)
                {
                    PLOGERR (klogFatal, (klogFatal, rc, "Failed to append Blob for row $(R) at $(P)",
                                         PLOG_2(PLOG_U64(R),PLOG_U64(P)), rowid, tix));

                    break;
                }
            }
            /* if we hit a NewLine and are within range we will close this blob */
            if (eol)
            {
                if (blobopen)
                {
                    /* single row blobs */
                    rc = KColumnBlobAssignRange (blob, rowid, 1);
                    if (rc)
                    {
                        PLOGERR (klogFatal, (klogFatal, rc, "Failed to range assign blob for row $(R) at $(P)",
                                             PLOG_2(PLOG_U64(R),PLOG_U64(P)), rowid, tix));
                        break;
                    }
                    rc = KColumnBlobCommit (blob);
                    if (rc)
                    {
                        PLOGERR (klogFatal, (klogFatal, rc, "Failed to commit blob for row $(R) at $(P)",
                                             PLOG_2(PLOG_U64(R),PLOG_U64(P)), rowid, tix));
                        break;
                    }
                    rc = KColumnBlobRelease (blob);
                    if (rc)
                    {
                        PLOGERR (klogFatal, (klogFatal, rc, "Failed to release blob for row $(R) at $(P)",
                                             PLOG_2(PLOG_U64(R),PLOG_U64(P)), rowid, tix));
                        break;
                    }
                    blobopen = false;
                }
                ++rowid;
                if ( ! rowid_upper_range (rowid))
                    break;
            }
        }
    }

    /* if not in an error state and the last line was unterminated close the blob */
    if ((rc == 0) && blobopen)
    {
        rc = KColumnBlobAssignRange (blob, rowid, 1);
        if (rc)
        {
            PLOGERR (klogFatal, (klogFatal, rc, "Failed to range assign blob for row $(R) at $(P)",
                                 PLOG_2(PLOG_U64(R),PLOG_U64(P)), rowid, tix));
        }
        else
        {
            rc = KColumnBlobCommit (blob);
            if (rc)
            {
                PLOGERR (klogFatal, (klogFatal, rc, "Failed to commit blob for row $(R) at $(P)",
                                     PLOG_2(PLOG_U64(R),PLOG_U64(P)), rowid, tix));
            }
        }
        KColumnBlobRelease (blob);
    }
    return rc;
}

void CC ascii_to_u64_error_handler ( const char * arg, void * data )
{
    rc_t * prc = data;
    rc_t rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcIncorrect);


    PLOGERR (klogFatal, (klogFatal, rc, "numeric range option unparsable $(S)", PLOG_S(S), arg));
    *prc = rc;
}

rc_t CC NextLogLevelCommon ( const char * level_parameter );

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args;
    rc_t   rc;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof Options / sizeof (OptDef));
    if (rc == 0)
    {
        do
        {
            uint32_t pcount;

            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;

            switch (pcount)
            {
            case 0:
                MiniUsage (args);
                rc = 1;
                break;

            case 1:
                rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcNotFound);
                LOGERR (klogFatal, rc, "Missing KColumn path");
                MiniUsage (args);
                break;

            default:
                rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcExcessive);
                LOGERR (klogFatal, rc, "Too many parameters");
                MiniUsage (args);
                break;

            case 2:
                break;
            }
            if (rc)
            {
                if (rc == 1)
                    rc = 0;
                break;
            }

            rc = ArgsParamValue (args, 0, (const void **)&G.txtpath);
            if (rc)
                break;

            rc = ArgsParamValue (args, 1, (const void **)&G.colpath);
            if (rc)
                break;

            rc = ArgsOptionCount (args, OPTION_BEGIN, &pcount);
            if (rc)
                break;

            if (pcount != 1)
                G.begin_seen = false;
            else
            {
                const char * pc;

                rc = ArgsOptionValue (args, OPTION_BEGIN, 0, (const void **)&pc);
                if (rc)
                    break;

                G.begin = AsciiToU64 ( pc, ascii_to_u64_error_handler, &rc );

                if (rc)
                    break;
                G.begin_seen = true;
            }

            rc = ArgsOptionCount (args, OPTION_END, &pcount);
            if (rc)
                break;

            if (pcount != 1)
                G.end_seen = false;
            else
            {
                const char * pc;

                rc = ArgsOptionValue (args, OPTION_END, 0, (const void **)&pc);
                if (rc)
                    break;

                G.end = AsciiToU64 (pc, ascii_to_u64_error_handler, &rc);
                if (rc)
                    break;

                G.end_seen = true;
            }


            if (G.begin_seen && G.end_seen && (G.end < G.begin))
            {
                rc = RC (rcExe, rcNoTarg, rcAccessing, rcParam, rcInvalid);
                LOGERR (klogFatal, rc, "Conflicting options end before begin");
                break;
            }

            rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
            if (rc)
                break;

            G.force = (pcount != 0);

            rc = ArgsOptionCount (args, OPTION_APPEND, &pcount);
            if (rc)
                break;

            G.append = (pcount != 0);

            if (G.force && G.append)
            {
                rc = RC (rcExe, rcNoTarg, rcAccessing, rcParam, rcInvalid);
                LOGERR (klogFatal, rc, "Conflicting options force and append");
                break;
            }

            /* handle the KFS interface */
            rc = txt2kdb_kfs();

            if (rc == 0)
                rc = txt2kdb_io();

            txt2kdb_release();


        } while (0);
        ArgsWhack (args);
    }

    STSMSG (1, ("exit txt2kdb %R\n", rc));
    return VDB_TERMINATE( rc );
}


/* end of file */
