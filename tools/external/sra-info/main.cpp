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
#define OPTION_QUALITY      "quality"
#define OPTION_SPOTLAYOUT   "spot-layout"
#define OPTION_LIMIT        "limit"

#define ALIAS_PLATFORM      "P"
#define ALIAS_FORMAT        "f"
#define ALIAS_ISALIGNED     "A"
#define ALIAS_QUALITY       "Q"
#define ALIAS_SPOTLAYOUT    "S"
#define ALIAS_LIMIT         "l"

static const char * platform_usage[]    = { "print platform(s)", nullptr };
static const char * format_usage[]      = { "output format:", nullptr };
static const char * isaligned_usage[]   = { "is data aligned", nullptr };
static const char * quality_usage[]     = { "are quality scores stored or generated", nullptr };
static const char * spot_layout_usage[] = { "print spot layout(s)", nullptr };
static const char * limit_usage[]       = { "limit output to <N> elements, e.g. <N> most popular spot layouts; <N> must be >0", nullptr };

OptDef InfoOptions[] =
{
    { OPTION_PLATFORM,      ALIAS_PLATFORM,     nullptr, platform_usage,    1, false,   false, nullptr },
    { OPTION_FORMAT,        ALIAS_FORMAT,       nullptr, format_usage,      1, true,    false, nullptr },
    { OPTION_ISALIGNED,     ALIAS_ISALIGNED,    nullptr, isaligned_usage,   1, false,   false, nullptr },
    { OPTION_QUALITY,       ALIAS_QUALITY,      nullptr, quality_usage,     1, false,   false, nullptr },
    { OPTION_SPOTLAYOUT,    ALIAS_SPOTLAYOUT,   nullptr, spot_layout_usage, 1, false,   false, nullptr },
    { OPTION_LIMIT,         ALIAS_LIMIT,        nullptr, limit_usage,       1, true,    false, nullptr },
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

    HelpOptionLine ( ALIAS_PLATFORM,    OPTION_PLATFORM,    nullptr, platform_usage );
    HelpOptionLine ( ALIAS_QUALITY,     OPTION_QUALITY,     nullptr, quality_usage );
    HelpOptionLine ( ALIAS_ISALIGNED,   OPTION_ISALIGNED,   nullptr, isaligned_usage );
    HelpOptionLine ( ALIAS_SPOTLAYOUT,  OPTION_SPOTLAYOUT,  nullptr, spot_layout_usage );

    HelpOptionLine ( ALIAS_FORMAT,   OPTION_FORMAT,     "format",   format_usage );
    KOutMsg( "      csv ..... comma separated values on one line\n" );
    KOutMsg( "      xml ..... xml-style without complete xml-frame\n" );
    KOutMsg( "      json .... json-style\n" );
    KOutMsg( "      tab ..... tab-separated values on one line\n" );

    HelpOptionLine ( ALIAS_LIMIT,  OPTION_LIMIT, "N", limit_usage );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

static
void
Output( const string & text )
{
    KOutMsg ( "%s\n", text.c_str() );
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
                uint32_t limit = 0;

                rc = ArgsOptionCount( args, OPTION_LIMIT, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    const char* res = nullptr;
                    rc = ArgsOptionValue( args, OPTION_LIMIT, 0, ( const void** )&res );
                    std::stringstream ss(res);
                    ss >> limit;
                    if ( limit == 0 )
                    {
                        throw VDB::Error("invalid value for --limit (not a positive number)");
                    }
                }

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
                Formatter formatter( fmt, limit );

                rc = ArgsOptionCount( args, OPTION_PLATFORM, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    Output( formatter.format( info.GetPlatforms() ) );
                }

                rc = ArgsOptionCount( args, OPTION_ISALIGNED, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    Output( formatter.format( info.IsAligned() ? "ALIGNED" : "UNALIGNED" ) );
                }

                rc = ArgsOptionCount( args, OPTION_QUALITY, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    Output( formatter.format( info.HasPhysicalQualities() ? "STORED" : "GENERATED" ) );
                }

                rc = ArgsOptionCount( args, OPTION_SPOTLAYOUT, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    Output ( formatter.format( info.GetSpotLayouts() ) );
                }

            }
            catch( const exception& ex )
            {
                //KOutMsg( "%s\n", ex.what() ); ? - should be in stderr already, at least for VDB::Error
                rc = 3;
            }
        }

        DESTRUCT(Args, args);
    }

    return rc;
}
