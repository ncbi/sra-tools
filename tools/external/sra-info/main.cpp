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

#include "formatter.hpp"


using namespace std;
#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = nullptr; } while (false)

#define OPTION_PLATFORM     "platform"
#define OPTION_FORMAT       "format"
#define OPTION_ISALIGNED    "is-aligned"

#define ALIAS_PLATFORM      "P"
#define ALIAS_FORMAT        "f"
#define ALIAS_ISALIGNED     "A"

static const char * platform_usage[]    = { "print platform(s)", nullptr };
static const char * format_usage[]      = { "output format:", nullptr };
static const char * isaligned_usage[]   = { "is data aligned", nullptr };

OptDef InfoOptions[] =
{
    { OPTION_PLATFORM,  ALIAS_PLATFORM,     nullptr, platform_usage,   1, false,   false, nullptr },
    { OPTION_FORMAT,    ALIAS_FORMAT,       nullptr, format_usage,     1, true,    false, nullptr },
    { OPTION_ISALIGNED, ALIAS_ISALIGNED,    nullptr, isaligned_usage,  1, false,   false, nullptr },
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

    if ( args == nullptr )
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

    HelpOptionLine ( ALIAS_PLATFORM, OPTION_PLATFORM,   nullptr,    platform_usage );
    HelpOptionLine ( ALIAS_FORMAT,   OPTION_FORMAT,     "format",   format_usage );
    KOutMsg( "      csv ..... comma separated values on one line\n" );
    KOutMsg( "      xml ..... xml-style without complete xml-frame\n" );
    KOutMsg( "      json .... json-style\n" );
    KOutMsg( "      piped ... 1 line per cell: row-id, column-name: value\n" );
    KOutMsg( "      tab ..... 1 line per row: tab-separated values only\n" );
    HelpOptionLine ( ALIAS_ISALIGNED,   OPTION_ISALIGNED, nullptr, isaligned_usage );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion() );

    return rc;
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

                // formatting
                Formatter::Format fmt = Formatter::Default;
                rc = ArgsOptionCount( args, OPTION_FORMAT, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {   
                    const char* res = nullptr;
                    rc = ArgsOptionValue( args, OPTION_FORMAT, 0, ( const void** )&res );
                    fmt = Formatter::StringToFormat( res );
                }
                Formatter formatter( fmt );

                rc = ArgsOptionCount( args, OPTION_PLATFORM, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    KOutMsg ( "%s\n", formatter.format( info.GetPlatforms() ).c_str() );
                }

                rc = ArgsOptionCount( args, OPTION_ISALIGNED, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    KOutMsg ( "%s\n", formatter.format( info.IsAligned() ? "ALIGNED" : "UNALIGNED" ).c_str() );
                }
            }
            catch( const exception& ex )
            {
                KOutMsg( "%s\n", ex.what() );
            }
        }

        DESTRUCT(Args, args);
    }

    return rc;
}
