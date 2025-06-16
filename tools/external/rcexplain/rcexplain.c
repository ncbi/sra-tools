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

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


const char UsageDefaultName[] = "rcexplain";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [Options] rc [rc ...]\n"
                    "\n"
                    "Summary:\n"
                    "  Prints out error string to stdout for one or more return codes.\n",
                    progname);
}

rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    KOutMsg ("Options:\n");

    HelpOptionsStandard();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    rc_t rc = 0;
    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc = ArgsMakeAndHandle (&args, argc, argv, 0);
    if (rc == 0)
    {
        do
        {
            uint32_t pcount;

            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;

            if (pcount == 0) {
                MiniUsage(args);
                rc = RC(rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid);
            }
            else
            {
                const char * pc;
                rc_t exp;
                uint32_t ix;

                /* over rule any set log level */
                rc = KLogLevelSet (klogInfo);

                for (ix = 0; ix < pcount; ++ix)
                {
                    rc = ArgsParamValue (args, ix, (const void **)&pc);
                    if (rc)
                        break;

                    exp = AsciiToU32 (pc, NULL, NULL);
                    rc = LOGERR (klogInfo, exp, pc);
                }
            }

        } while (0);

        ArgsWhack (args);
    }
    return VDB_TERMINATE( rc );
}
