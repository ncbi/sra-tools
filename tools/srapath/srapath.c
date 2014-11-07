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

#include "srapath.vers.h"

#include <vfs/resolver.h>
#include <vfs/path.h>
#include <vfs/manager.h>
#include <kfs/directory.h>
#include <kfs/defs.h>
#include <kfg/config.h>
#include <sra/impl.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h> /* STSMSG */
#include <klib/rc.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <limits.h> /* PATH_MAX */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion ( void )
{
    return SRAPATH_VERS;
}

const char UsageDefaultName[] = "srapath";

rc_t CC UsageSummary (const char * progname)
{
    return OUTMSG(("\n"
        "Usage:\n"
        "  %s [options] <accession> ...\n\n"
        "Summary:\n"
        "  Tool to produce a list of full paths to files\n"
        "  (SRA and WGS runs, refseqs: reference sequences)\n"
        "  from list of NCBI accessions.\n"
        "\n", progname));
}

rc_t CC Usage(const Args *args) {
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL) {
        rc = RC (rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    }
    else {
        rc = ArgsProgram(args, &fullpath, &progname);
    }

    if (rc != 0) {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary(progname);

    OUTMSG((
        "  Output paths are ordered according to accession list.\n"
        "\n"
        "  The accession search path will be determined according to the\n"
        "  configuration. It will attempt to find files in local and site\n"
        "  repositories, and will also check remote repositories for run\n"
        "  location.\n"));
    OUTMSG((
        "  This tool produces a path that is 'likely' to be a run, in that\n"
        "  an entry exists in the file system at the location predicted.\n"
        "  It is possible that this path will fail to produce success upon\n"
        "  opening a run if the path does not point to a valid object.\n\n"));

    OUTMSG(("Options:\n"));

    HelpOptionsStandard();

    HelpVersion(fullpath, KAppVersion());

    return rc;
}

/* KMain
 */
rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc;

    rc = ArgsMakeAndHandle (&args, argc, argv, 0);
    if (rc)
        LOGERR (klogInt, rc, "failed to parse arguments");
    else do
    {
        uint32_t acount;
        rc = ArgsParamCount (args, &acount);
        if (rc)
        {
            LOGERR (klogInt, rc, "failed to count parameters");
            break;
        }

        if (acount == 0)
        {
            rc = MiniUsage (args);
            break;
        }
        else
        {
            VFSManager* mgr;
            rc = VFSManagerMake(&mgr);
            if (rc)
                LOGERR ( klogErr, rc, "failed to create VFSManager object" );
            else
            {
                VResolver * resolver;

                rc = VFSManagerGetResolver (mgr, &resolver);
                if (rc == 0)
                {
                    uint32_t ix;
                    for ( ix = 0; ix < acount; ++ ix )
                    {
                        const char * pc;
                        rc = ArgsParamValue (args, ix, &pc );
                        if (rc)
                            LOGERR (klogInt, rc,
                                    "failed to retrieve parameter value");
                        else
                        {
                            const VPath * upath;
                            rc = VFSManagerMakePath ( mgr, (VPath**)&upath, "%s", pc);
                            if (rc == 0)
                            {
                                const VPath * local;
                                const VPath * remote;

                                rc = VResolverQuery (resolver, eProtocolHttp, upath, &local, &remote, NULL);

                                if (rc == 0)
                                {
                                    const String * s;

                                    if (local != NULL)
                                        rc = VPathMakeString (local, &s);
                                    else 
                                        rc = VPathMakeString (remote, &s);
                                    if (rc == 0)
                                    {
                                        OUTMSG (("%S\n", s));
                                        free ((void*)s);
                                    }
                                    VPathRelease (local);
                                    VPathRelease (remote);
                                }
                                else
                                {
                                    KDirectory * cwd;
                                    rc_t orc = VFSManagerGetCWD (mgr, &cwd);
                                    if (orc == 0)
                                    {
                                        KPathType kpt
                                            = KDirectoryPathType(cwd, "%s", pc);
                                        switch (kpt &= ~kptAlias)
                                        {
                                        case kptNotFound:
                                            STSMSG(1, ("'%s': not found while "
                                                "searching the file system",
                                                pc));
                                            break;
                                        case kptBadPath:
                                            STSMSG(1, ("'%s': bad path while "
                                                "searching the file system",
                                                pc));
                                            break;
                                        default:
                                            STSMSG(1, ("'%s': "
                                                "found in the file system",
                                                pc));
                                            rc = 0;
                                            break;
                                        }
                                    }
                                    if (orc == 0 && rc == 0) {
                                        if (rc != 0) {
                                            PLOGMSG(klogErr, (klogErr,
                                                              "'$(name)': not found",
                                                              "name=%s", pc));
                                        }
                                        else {
                                            char resolved[PATH_MAX] = "";
                                            rc = KDirectoryResolvePath(cwd, true,
                                                                       resolved, sizeof resolved, "%s", pc);
                                            if (rc == 0) {
                                                STSMSG(1, ("'%s': found in "
                                                           "the current directory at '%s'",
                                                           pc, resolved));
                                                OUTMSG (("%s\n", resolved));
                                            }
                                            else {
                                                STSMSG(1, ("'%s': cannot resolve "
                                                           "in the current directory",
                                                           pc));
                                                OUTMSG (("./%s\n", pc));
                                            }
                                        }
                                    }
                                    KDirectoryRelease(cwd);
                                }
                            }
                        }
                    }
                    VResolverRelease (resolver);
                }
                VFSManagerRelease(mgr);
            }
        }
        ArgsWhack (args);

    } while (0);

    return rc;
}
