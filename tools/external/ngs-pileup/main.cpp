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

#include <cmath>

#include "ngs-pileup.hpp"

#include <ngs/ErrorMsg.hpp>

#include <kapp/main.h>
#include <kfg/config.h> /* KConfigSetNgcFile */
#include <klib/out.h>
#include <klib/rc.h>

#include <sysalloc.h>
#include <string.h>

#include <iostream>

#define OPTION_NGC "ngc"
#define ALIAS_NGC  NULL
static const char * ngc_usage[] = { "PATH to ngc file", NULL };

#define OPTION_REF     "aligned-region"
#define ALIAS_REF      "r"
const char * ref_usage[] = { "Filter by position on genome.",
                             "Name can either be file specific or canonical",
                             "(ex: \"chr1\" or \"1\").",
                             "\"from\" and \"to\" are 1-based coordinates",
                             NULL };

OptDef options[] =
{   /*name,           alias,         hfkt, usage-help,    maxcount, needs value, required */
    { OPTION_REF,     ALIAS_REF,     NULL, ref_usage,     0,        true,        false },
    { OPTION_NGC,     ALIAS_NGC,     NULL, ngc_usage,     0,        true,        false },
};


const char UsageDefaultName[] = "ngs-pileup";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <path> [options]\n"
                    "\n", progname);
}


rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;

    size_t i = 0;

    rc_t rc = 0;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );
    KOutMsg ( "Options:\n" );

    for (i = 0; i < sizeof options / sizeof options[0]; ++i) {
        const OptDef * opt = &options[i];

        const char * alias = opt->aliases;

        const char *param = NULL;
        if (alias != NULL) {
            if (strcmp(alias, ALIAS_REF) == 0)
                param = "region";
        }
        else if (strcmp(opt->name, OPTION_NGC) == 0)
            param = "PATH";

        HelpOptionLine(alias, opt->name, param, opt->help);
    }

    KOutMsg("\n");
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);

    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle( &args, argc, app.getArgV(), 1, options, sizeof options / sizeof options[0]);
    if ( rc == 0 )
    {
        try
        {
            NGS_Pileup::Settings settings;

            uint32_t pcount;

            void const *value = NULL;

            rc = ArgsOptionCount ( args, OPTION_REF, &pcount );
            if ( pcount > 1 )
            {
                throw ngs :: ErrorMsg ( "multiple positions are not supported at this point" );
            }
            if ( pcount == 1 )
            {
                rc = ArgsOptionValue ( args, OPTION_REF, 0, & value );
                if ( rc != 0 )
                {
                    throw ngs :: ErrorMsg ( "ArgsOptionValue (" OPTION_REF ") failed" );
                }
                settings . AddReference ( static_cast <char const*> (value) );
            }

/* OPTION_NGC */
            {
                rc = ArgsOptionCount(args, OPTION_NGC, &pcount);
                if (pcount > 1)
                    throw ngs::ErrorMsg("multiple ngc files are not supported");
                else if (pcount == 1) {
                    rc = ArgsOptionValue(args, OPTION_NGC, 0, &value);
                    if (rc != 0)
                        throw ngs::ErrorMsg(
                            "ArgsOptionValue (" OPTION_NGC ") failed");

                    KConfigSetNgcFile(static_cast <const char *>(value));
                }
            }

            rc = ArgsParamCount ( args, &pcount );
            if ( rc == 0 )
            {
                if ( pcount > 1 )
                {
                    throw ngs :: ErrorMsg ( "multiple accessions are not supported at this point" );
                }

                settings . output = & std::cout;

                rc = ArgsParamValue ( args, 0, &value );
                if ( rc == 0 )
                {
                    settings . AddInput ( static_cast <char const*> (value) );

                    NGS_Pileup ( settings ) . Run ();
                }
                else
                {
                    throw ngs :: ErrorMsg ( "ArgsParamValue failed" );
                }
            }
        }
        catch (ngs :: ErrorMsg& ex)
        {
            std :: cerr << "Program aborted: " << ex.what() << std::endl;
            exit ( -1 );
        }

        ArgsWhack( args );
    }

    return 0;
}

