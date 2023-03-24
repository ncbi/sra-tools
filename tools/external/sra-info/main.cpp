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

#include "sra-info.hpp"

#include <klib/out.h>
#include <klib/log.h>
#include <klib/rc.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/args-conv.h>

using namespace std;
#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

#define OPTION_PLATFORM "platform"
#define ALIAS_PLATFORM  "P"
static const char * platform_usage[] = { "print platform(s)", NULL };

#define OPTION_SPOTLAYOUT "spot-layout"
#define ALIAS_SPOTLAYOUT "S"
static const char * spot_layout_usage[] = { "print spot layout(s)", NULL };

OptDef InfoOptions[] =
{
    { OPTION_PLATFORM, ALIAS_PLATFORM, NULL, platform_usage, 1, false,  false },
    { OPTION_SPOTLAYOUT, ALIAS_SPOTLAYOUT, NULL, spot_layout_usage, 1, false,  false },
};

const char UsageDefaultName[] = "sra-info";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <accession> [options]\n"
                    "\n", progname);
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL )
    {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    }
    else
    {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }

    if ( rc )
    {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );

    HelpOptionLine ( ALIAS_PLATFORM, OPTION_PLATFORM, NULL, platform_usage );
    HelpOptionLine ( ALIAS_SPOTLAYOUT, OPTION_SPOTLAYOUT, NULL, spot_layout_usage );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

void
PrintPlatforms( const SraInfo::Platforms & platforms )
{
    for( auto p : platforms )
    {
        KOutMsg ( (p+"\n").c_str() );
    }
}

void
PrintSpotLayouts( const SraInfo::SpotLayouts & layouts )
{
    for( auto l : layouts )
    {
        for ( auto r : l.reads )
        {
            KOutMsg ( "%d %d, ", r.type, r.length );
        }
        KOutMsg ( ":%d\n", l.count );
    }
}

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle( &args, argc, argv,
        1, InfoOptions, sizeof InfoOptions / sizeof InfoOptions [ 0 ] );
    DISP_RC( rc, "ArgsMakeAndHandle() failed" );
    if ( rc == 0)
    {
        SraInfo info;

        uint32_t param_count = 0;
        rc = ArgsParamCount(args, &param_count);
        DISP_RC( rc, "ArgsParamCount() failed" );
        if ( rc == 0 )
        {
            if ( param_count == 0 || param_count > 1 ) {
                MiniUsage(args);
                DESTRUCT(Args, args);
                exit(1);
            }

            const char * accession = nullptr;
            rc = ArgsParamValue( args, 0, ( const void ** )&( accession ) );
            DISP_RC( rc, "ArgsParamValue() failed" );

            try
            {
                info.SetAccession( accession );

                uint32_t opt_count;

                rc = ArgsOptionCount( args, OPTION_PLATFORM, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    PrintPlatforms( info.GetPlatforms() );
                }

                rc = ArgsOptionCount( args, OPTION_SPOTLAYOUT, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    PrintSpotLayouts( info.GetSpotLayouts() );
                }

            }
            catch( const exception& ex )
            {
                KOutMsg( (string(ex.what()) + "\n").c_str() );
            }
        }

        DESTRUCT(Args, args);
    }

    return rc;
}
