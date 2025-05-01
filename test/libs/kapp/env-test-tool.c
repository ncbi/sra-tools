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

// TODO: provide short ones
#define ALIAS_AMD64     NULL
#define OPTION_AMD64    "amd64"
static const char* USAGE_AMD64[] = { "require tool to be run under 64-bit environment", NULL };

#define ALIAS_RAM       NULL
#define OPTION_RAM      "ram"
static const char* USAGE_RAM[] = { "require system RAM to be at least B", NULL };

OptDef Options[] =
{                                         /* needs_value, required */
      { OPTION_AMD64, ALIAS_AMD64, NULL, USAGE_AMD64, 1, false, false }
    , { OPTION_RAM  , ALIAS_RAM  , NULL, USAGE_RAM  , 1, true , false }
};

ver_t CC KAppVersion ( void )
{
    return 0;
}

const char UsageDefaultName[] = "test-env";

rc_t CC UsageSummary ( const char *progname )
{
    return KOutMsg ( "\n"
                     "Usage:\n"
                     "  %s [Options]\n"
                     "\n"
                     "Summary:\n"
                     "  Test system environment.\n"
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

    HelpOptionLine (ALIAS_AMD64, OPTION_AMD64, NULL, USAGE_AMD64);
    HelpOptionLine (ALIAS_RAM  , OPTION_RAM  , NULL, USAGE_RAM);
    KOutMsg ("\n");

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion () );

    return rc;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    Args *args;
    rc_t rc;
    do {
        uint32_t paramc;
        bool requireAmd64 = false;
        uint64_t requireRam = 0;

        rc = ArgsMakeAndHandle ( & args, argc, argv, 1,
               Options, sizeof Options / sizeof (OptDef) );
        if ( rc != 0 )
        {
            LogErr ( klogErr, rc, "failed to parse arguments" );
            break;
        }

        // TODO: check if we need it
        rc = ArgsParamCount ( args, & paramc );
        if ( rc != 0 ) {
            LogErr ( klogInt, rc, "failed to obtain param count" );
            break;
        }

        {   // OPTION_AMD64
            rc = ArgsOptionCount( args, OPTION_AMD64, & paramc );
            if ( rc ) {
                LOGERR( klogErr, rc, "Failure to get '" OPTION_AMD64 "' argument" );
                break;
            }
            if ( paramc ) {
                requireAmd64 = true;
            }
        }

        {
            // OPTION_RAM
            rc = ArgsOptionCount ( args, OPTION_RAM, & paramc );
            if ( rc ) {
                LOGERR ( klogErr, rc, "Failure to get '" OPTION_RAM "' argument" );
                break;
            }
            if ( paramc ) {
                long long unsigned int sscanf_param;

                const char* dummy = NULL;
                rc = ArgsOptionValue(args, OPTION_RAM, 0, (const void **)&dummy);
                if ( rc ) {
                    LOGERR(klogErr, rc, "Failure to get '" OPTION_RAM "' argument");
                    break;
                }

                rc = sscanf(dummy, "%llu", &sscanf_param);
                requireRam = ( uint64_t ) sscanf_param;
                if ( rc != 1)
                {
                    LOGMSG(klogErr, "Failure to parse '" OPTION_RAM "' argument value");
                    break;
                }
            }

        }

        {
            rc = KAppCheckEnvironment(requireAmd64, requireRam);
            if (rc != 0 )
                printf("Invalid environment\n");
            else
                printf("Enviroment is fine!\n");
        }
    } while (false);

    ArgsWhack ( args );

    return rc;
}
