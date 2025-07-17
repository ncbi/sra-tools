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
#include <klib/rc.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

const char UsageDefaultName[] = "sig-core";

rc_t CC UsageSummary ( const char *progname )
{
    return KOutMsg ( "\n"
                     "Usage:\n"
                     "  %s [Options]\n"
                     "\n"
                     "Summary:\n"
                     "  Test core generation by killing it with signal.\n"
		             "  The tool enter into infinite loop after start .\n"
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
    HelpVersion ( fullpath, GetKAppVersion () );

    return rc;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    Args *args;
    SetUsage( Usage );
    SetUsageSummary( UsageSummary );
    rc_t rc;
    do {
        rc = ArgsMakeAndHandle ( & args, argc, argv, 0 );
        if ( rc != 0 )
        {
            LogErr ( klogErr, rc, "failed to parse arguments" );
            break;
        }

        while ( true ) {
            sleep ( 1000 );
        }
    } while (false);

    ArgsWhack ( args );

    return rc;
}
