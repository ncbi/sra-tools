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
#include <sra/srapath.h>
#include <vdb/manager.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/rc.h>

#include <assert.h>

const char UsageDefaultName[] = "vdb-lock";

rc_t CC UsageSummary ( const char *progname )
{
    return KOutMsg ( "\n"
                     "Usage:\n"
                     "  %s [Options] <target>\n"
                     "\n"
                     "Summary:\n"
                     "  Lock a VDB database, table or column.\n"
                     , progname
        );
}

rc_t CC Usage ( const Args *args )
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

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion () );

    return rc;
}


MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args *args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle ( & args, argc, argv, 0 );
    if ( rc != 0 )
        LogErr ( klogErr, rc, "failed to parse arguments" );
    else
    {
        uint32_t paramc;
        rc = ArgsParamCount ( args, & paramc );
        if ( rc != 0 )
            LogErr ( klogInt, rc, "failed to obtain param count" );
        else
        {
            if ( paramc == 0 )
            {
                rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
                LogErr ( klogErr, rc, "missing target object" );
                MiniUsage ( args );
            }
            else if ( paramc > 1 )
            {
                rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcExcessive );
                LogErr ( klogErr, rc, "expected single target object" );
                MiniUsage ( args );
            }
            else
            {
                const char *target;
                rc = ArgsParamValue ( args, 0, (const void **)& target );
                if ( rc != 0 )
                    LogErr ( klogInt, rc, "failed to obtain param value" );
                else
                {
                    VDBManager *mgr;

#if TOOLS_USE_SRAPATH != 0
                    char full [ 4096 ];
                    SRAPath *sra_path;
                    rc = SRAPathMake ( & sra_path, NULL );
                    if ( rc == 0 )
                    {
                        if ( ! SRAPathTest ( sra_path, target ) )
                        {
                            rc = SRAPathFind ( sra_path, target, full, sizeof full );
                            if ( rc == 0 )
                                target = full;
                        }
                        SRAPathRelease ( sra_path );
                    }
#endif

                    rc = VDBManagerMakeUpdate ( & mgr, NULL );
                    if ( rc != 0 )
                        LogErr ( klogInt, rc, "failed to open VDB manager" );
                    else
                    {
                        rc = VDBManagerLock ( mgr, "%s", target );
                        if ( rc == 0 )
                            pLogMsg ( klogInfo, "locked '$(target)'", "target=%s", target );
                        else switch ( GetRCState ( rc ) )
                        {
                        case rcLocked:
                            pLogMsg ( klogInfo, "'$(target)' was already locked", "target=%s", target );
                            break;
                        default:
                            pLogErr ( klogErr, rc, "failed to lock '$(target)'", "target=%s", target );
                        }

                        VDBManagerRelease ( mgr );
                    }
                }
            }
        }

        ArgsWhack ( args );
    }

    return VDB_TERMINATE( rc );
}
