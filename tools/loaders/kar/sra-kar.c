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

#define USE_FORCE 0

#include <klib/rc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/status.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <sra/sradb.h>
#include <sra/sradb-priv.h>

#include <kapp/args.h>


#include <assert.h>

typedef struct srakar_parms srakar_parms;
struct srakar_parms
{
    KDirectory * dir;
    const char *src_path;
    const char *dst_path;
    bool lite;
    bool force;
};

static
rc_t copy_file (const KFile * fin, KFile *fout)
{
    rc_t rc;
    uint8_t	buff	[64 * 1024];
    size_t	num_read;
    uint64_t	inpos;
    uint64_t	outpos;
    uint64_t    fsize;

    assert (fin != NULL);
    assert (fout != NULL);

    inpos = 0;
    outpos = 0;

    rc = KFileSize (fin, &fsize);
    if (rc != 0)
        return rc;

    do
    {
        rc = KFileRead (fin, inpos, buff, sizeof (buff), &num_read);
        if (rc != 0)
        {
            PLOGERR (klogErr, (klogErr, rc,
                               "Failed to read from directory structure in creating archive at $(P)",
                               PLOG_U64(P), inpos));
            break;
        }
        else if (num_read > 0)
        {
            size_t to_write;

            inpos += (uint64_t)num_read;

            to_write = num_read;
            while (to_write > 0)
            {
                size_t num_writ;
                rc = KFileWrite (fout, outpos, buff, num_read, &num_writ);
                if (rc != 0)
                {
                    PLOGERR (klogErr, (klogErr, rc,
                                       "Failed to write to archive in creating archive at $(P)",
                                       PLOG_U64(P), outpos));
                    break;
                }
                outpos += num_writ;
                to_write -= num_writ;
            }
        }
        if (rc != 0)
            break;
    } while (num_read != 0);
    return rc;
}
static
rc_t run ( srakar_parms *pb )
{
    KFile * outfile;
    rc_t rc;

    const SRAMgr *mgr;

    rc = SRAMgrMakeRead ( & mgr );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to open SRAMgr" );
    else
    {
        const SRATable *tbl;
        rc = SRAMgrOpenTableRead ( mgr, & tbl, "%s", pb -> src_path );
        if ( rc != 0 )
            PLOGERR ( klogInt, (klogInt, rc,
                "failed to open SRATable '$(spec)'", "spec=%s",
                pb -> src_path ));
        else
        {
            rc = KDirectoryCreateFile (pb->dir, &outfile, false, 0446,
                                       kcmParents | ( pb->force ? kcmInit : kcmCreate) , "%s", pb->dst_path);
            if (rc == 0)
            {
                const KFile * archive;

                rc = SRATableMakeSingleFileArchive (tbl, &archive, pb->lite,
                    NULL);
                if (rc == 0)
                {
                    rc = copy_file (archive, outfile);
                    KFileRelease (archive);
                }
                KFileRelease (outfile);
            }
            SRATableRelease ( tbl );
        }
        SRAMgrRelease (mgr);
    }
/*
    rc = KDirectoryCreateFile (pb->dir, &outfile, false, 0446, kcmParents | ( pb->force ? kcmInit : kcmCreate) , "%s", pb->dst_path);

    if (rc == 0)
    {
        const SRAMgr *mgr;

        rc = SRAMgrMakeRead ( & mgr );
        if ( rc != 0 )
            LOGERR ( klogInt, rc, "failed to open SRAMgr" );
        else
        {
            const SRATable *tbl;
            rc = SRAMgrOpenTableRead ( mgr, & tbl, "%s", pb -> src_path );
            if ( rc != 0 )
                PLOGERR ( klogInt, (klogInt, rc, "failed to open SRATable '$(spec)'", "spec=%s", pb -> src_path ));
            else
            {
                const KFile * archive;

                rc = SRATableMakeSingleFileArchive (tbl, &archive, pb->lite, NULL);
                if (rc == 0)
                {
                    rc = copy_file (archive, outfile);
                    KFileRelease (archive);
                }
                SRATableRelease ( tbl );
            }
            SRAMgrRelease (mgr);
        }
        KFileRelease (outfile);
    }
*/
    return rc;
}

/* Usage
 */
#define OPTION_LITE "lite"
#define ALIAS_LITE "l"
static const char * lite_usage[] = { "output an abbreviated version of the table", NULL };

#if USE_FORCE
#define OPTION_FORCE "force"
#define ALIAS_FORCE "f"
static const char * force_usage[] = { "replace an existing archive if present", NULL };
#endif
static
OptDef Options[] =
{
#if USE_FORCE
    { OPTION_FORCE, ALIAS_FORCE, NULL, force_usage, 1, false, false },
#endif
    { OPTION_LITE, ALIAS_LITE, NULL, lite_usage, 1, false, false }
};


static
KLogLevel default_log_level;



rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [options] table [archive]\n"
                    "\n"
                    "Summary:\n"
                    "  Create a single file archive from an SRA database table.\n"
                    "  The archive name can be given or will be derived from the\n"
                    "  name of the table.\n"
                    "\n", progname);
}

const char UsageDefaultName[] = "sra-kar";

rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg ("Options:\n");

    HelpOptionLine (ALIAS_LITE, OPTION_LITE, NULL, lite_usage);
#if USE_FORCE
    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, force_usage);
#endif
    HelpOptionsStandard ();
    HelpVersion (fullpath, KAppVersion());
    return rc;
}


MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
        return VDB_INIT_FAILED;

    Args * args;
    rc_t rc;

    default_log_level = KLogLevelGet();

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof Options / sizeof (OptDef));
    if (rc == 0)
    {
        do
        {
            srakar_parms pb;
            KDirectory * pwd;
            const char * name;
            uint32_t pcount;
            char archive_name [256];

            rc = KDirectoryNativeDir (&pwd);
            if (rc)
                break;

            pb.lite = false;
            pb.force = false;
            pb.dir = pwd;

            rc = ArgsOptionCount (args, OPTION_LITE, &pcount);
            if (rc)
                break;

            if (pcount == 1)
            {
                STSMSG (1, ("Using lite option"));
                pb.lite = true;
            }

#if USE_FORCE
            rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
            if (rc)
                break;

            if (pcount == 1)
            {
                STSMSG (1, ("Using force option"));
                pb.force = true;
            }
#endif
            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;

            if (pcount == 0)
            {
                KOutMsg ("missing source table\n");
                MiniUsage (args);
                rc = RC (rcExe, rcArgv, rcParsing, rcPath, rcInsufficient);
                break;
            }
            else if (pcount > 2)
            {
                KOutMsg ("Too many parameters\n");
                MiniUsage (args);
                rc = RC (rcExe, rcArgv, rcParsing, rcPath, rcExcessive);
                break;
            }

            rc = ArgsParamValue (args, 0, (const void **)&pb.src_path);
            if (rc)
            {
                KOutMsg ("failure to get source path/name\n");
                break;
            }

            pb.dst_path = archive_name;

            name = string_rchr (pb.src_path, string_size (pb.src_path), '/');
            if (name == NULL)
                name = pb.src_path;

            if (pcount == 1)
            {
                size_t size;

                rc = string_printf (archive_name, sizeof (archive_name), & size,
                                 "%s%s", name, pb.lite ? ".lite.sra" : ".sra");
                if ( rc != 0 )
                {
                    rc = RC (rcExe, rcArgv, rcParsing, rcPath, rcInsufficient);
                    PLOGERR (klogFatal, (klogFatal, rc, "Failure building archive name $(P)", "P=%s", archive_name));
                    break;
                }
            }
            else
            {
                rc = ArgsParamValue (args, 1, (const void **)&pb.dst_path);
                if (rc)
                {
                    LOGERR (klogInt, rc, "failure to get destination path");
                    break;
                }
            }

            if (rc == 0)
            {
                KPathType kpt;

                kpt = KDirectoryPathType (pwd, "%s", pb.dst_path);

                switch (kpt & ~kptAlias)
                {
                case kptNotFound:
                    /* found nothing so assume its a valid new file name
                     * not gonna tweak extensions at this point but that can be upgraded */
                    break;
                case kptFile:
                    /* got a file name, use it  - would need force... */
                    if (pb.force == false)
                    {
                        rc = RC (rcExe, rcArgv, rcParsing, rcPath, rcBusy);
                        PLOGERR (klogFatal, (klogFatal, rc,
                                             "Output file already exists $(P)",
                                             "P=%s", archive_name));
                    }
                    break;

                case kptDir:
                {
                    size_t size;

                    rc = string_printf (archive_name, sizeof (archive_name), & size,
                                     "%s/%s%s", pb.dst_path, name, pb.lite ? ".lite.sra" : ".sra");
                    if ( rc != 0 )
                    {
                        rc = RC (rcExe, rcArgv, rcParsing, rcPath, rcInsufficient);
                        PLOGERR (klogFatal, (klogFatal, rc, "Failure building archive name $(P)", "P=%s", archive_name));
                        break;
                    }
                    pb.dst_path = archive_name;
                    break;
                }
                default:
                    rc = RC (rcExe, rcArgv, rcParsing, rcPath, rcInvalid);
                    break;
                }
                if (rc == 0)
                {
                    STSMSG (1,("Creating archive (%s) from table (%s)\n", pb.dst_path, pb.src_path));

                    rc = run (&pb);
                    STSMSG (5, ("Run exits with %d %R\n", rc, rc));
                }
            }

        } while (0);
    }
    STSMSG (1, ("Exit status %u %R", rc, rc));
    return VdbTerminate( rc );
}
