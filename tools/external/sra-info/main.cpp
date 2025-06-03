/*==============================================================================
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

#include <functional>

#include <klib/out.h>
#include <klib/log.h>
#include <klib/rc.h>

#include <kapp/args.h>
#include <kapp/args-conv.h>
#include <kapp/vdbapp.h>

#include "../../shared/toolkit.vers.h"
#include <kapp/main.h>

#include "formatter.hpp"

using namespace std;
#define DISP_RC(rc, msg) if (rc != 0) { LOGERR(klogInt, rc, msg), throw VDB::Error(msg); }
#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = nullptr; } while (false)

#define OPTION_CONTENTS     "contents"
#define OPTION_DETAIL       "detail"
#define OPTION_ISALIGNED    "is-aligned"
#define OPTION_FORMAT       "format"
#define OPTION_LIMIT        "limit"
#define OPTION_PLATFORM     "platform"
#define OPTION_QUALITY      "quality"
#define OPTION_ROWS         "rows"
#define OPTION_SCHEMAVERS   "schema"
#define OPTION_SEQUENCE     "sequence"
#define OPTION_SPOTLAYOUT   "spot-layout"
#define OPTION_FINGERPRINT  "fingerprint"

#define ALIAS_ISALIGNED     "A"
#define ALIAS_SCHEMAVERS    "C"
#define ALIAS_DETAIL        "D"
#define ALIAS_FORMAT        "f"
#define ALIAS_LIMIT         "l"
#define ALIAS_PLATFORM      "P"
#define ALIAS_QUALITY       "Q"
#define ALIAS_ROWS          "R"
#define ALIAS_SPOTLAYOUT    "S"
#define ALIAS_SEQUENCE      "s"
#define ALIAS_CONTENTS      "T"
#define ALIAS_FINGERPRINT  "F"

static const char * platform_usage[]    = { "print platform(s)", nullptr };
static const char * format_usage[]      = { "output format:", nullptr };
static const char * isaligned_usage[]   = { "is data aligned", nullptr };
static const char * quality_usage[] = { "are quality scores stored or generated", nullptr };
static const char * schema_vers_usage[] = {
    "print schema version and dependencies", nullptr };
static const char * spot_layout_usage[] = { "print spot layout(s). Uses CONSENSUS table if present, SEQUENCE table otherwise", nullptr };
static const char * limit_usage[]       = { "limit output to <N> elements, e.g. <N> most popular spot layouts; <N> must be positive", nullptr };
static const char * detail_usage[]      = { "detail level, <0> the least detailed output; <N> must be zero or greater; default 3", nullptr };
static const char * sequence_usage[]    = { "use SEQUENCE table for spot layouts, even if CONSENSUS table is present", nullptr };
static const char * rows_usage[]        = { "report spot layouts for the first <N> rows of the table", nullptr };
static const char * contents_usage[]    = { "list the contents of the run: databases, tables, columns etc.", nullptr };
static const char * fingerprint_usage[] = { "show the fingerprint information. Detail level <0> (default) shows only the current run fingerprint. Description of fingerprint method available here: <LINK TBD>", nullptr };

OptDef InfoOptions[] =
{
    { OPTION_PLATFORM,      ALIAS_PLATFORM,     nullptr, platform_usage,    1, false,   false, nullptr },
    { OPTION_FORMAT,        ALIAS_FORMAT,       nullptr, format_usage,      1, true,    false, nullptr },
    { OPTION_ISALIGNED,     ALIAS_ISALIGNED,    nullptr, isaligned_usage,   1, false,   false, nullptr },
    { OPTION_QUALITY,       ALIAS_QUALITY,      nullptr, quality_usage,     1, false,   false, nullptr },
    { OPTION_SCHEMAVERS,    ALIAS_SCHEMAVERS,   nullptr, schema_vers_usage, 1, false,   false, nullptr },
    { OPTION_SPOTLAYOUT,    ALIAS_SPOTLAYOUT,   nullptr, spot_layout_usage, 1, false,   false, nullptr },
    { OPTION_LIMIT,         ALIAS_LIMIT,        nullptr, limit_usage,       1, true,    false, nullptr },
    { OPTION_DETAIL,        ALIAS_DETAIL,       nullptr, detail_usage,      1, true,    false, nullptr },
    { OPTION_SEQUENCE,      ALIAS_SEQUENCE,     nullptr, sequence_usage,    1, false,   false, nullptr },
    { OPTION_ROWS,          ALIAS_ROWS,         nullptr, rows_usage,        1, true,    false, nullptr },
    { OPTION_CONTENTS,      ALIAS_CONTENTS,     nullptr, contents_usage,    1, false,   false, nullptr },
    { OPTION_FINGERPRINT,   ALIAS_FINGERPRINT,  nullptr, fingerprint_usage,1, false,   false, nullptr },
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
    HelpOptionLine ( ALIAS_SCHEMAVERS,  OPTION_SCHEMAVERS,  nullptr, schema_vers_usage );
    HelpOptionLine ( ALIAS_SPOTLAYOUT,  OPTION_SPOTLAYOUT,  nullptr, spot_layout_usage );
    HelpOptionLine ( ALIAS_CONTENTS,    OPTION_CONTENTS,    nullptr, contents_usage );
    HelpOptionLine ( ALIAS_FINGERPRINT, OPTION_FINGERPRINT, nullptr, fingerprint_usage );

    HelpOptionLine ( ALIAS_FORMAT,   OPTION_FORMAT,     "format",   format_usage );
    KOutMsg( "      csv ..... comma separated values on one line\n" );
    KOutMsg( "      xml ..... xml-style\n" );
    KOutMsg( "      json .... json-style\n" );
    KOutMsg( "      tab ..... tab-separated values on one line\n" );
    KOutMsg( "             csv and tab formats can only be used with a single query\n" );
    KOutMsg( "             --" OPTION_SCHEMAVERS " does not support csv and tab\n" );

    HelpOptionLine ( ALIAS_LIMIT,  OPTION_LIMIT, "N", limit_usage );
    HelpOptionLine ( ALIAS_DETAIL, OPTION_DETAIL, "N", detail_usage );
    HelpOptionLine ( ALIAS_ROWS,   OPTION_ROWS,  "N", rows_usage );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, TOOLKIT_VERS );

    return rc;
}

static
void
Output( const string & text )
{
    if ( ! text.empty() )
        KOutMsg ( "%s\n", text.c_str() );
}

int
GetNumber( Args * args, const char * option, std::function<bool(int)> condition )
{
    const char* res = nullptr;
    rc_t rc = ArgsOptionValue( args, option, 0, ( const void** )&res );
    DISP_RC( rc, "ArgsOptionValue() failed" );

    try
    {
        int d = std::stoi(res);
        if ( ! condition( d ) )
        {
            throw VDB::Error("");
        }
        return d;
    }
    catch(...)
    {
        throw VDB::Error("");
    }
}

unsigned int
GetPositiveNumber( Args * args, const char * option )
{
    try
    {
        return GetNumber( args, option, [](int x) -> bool { return x > 0; } );
    }
    catch(...)
    {
        throw VDB::Error( string("invalid value for --") + option + "(not a positive number)");
    }
}
unsigned int
GetNonNegativeNumber( Args * args, const char * option )
{
    try
    {
        return GetNumber( args, option, [](int x) -> bool { return x >= 0; } );
    }
    catch(...)
    {
        throw VDB::Error( string("invalid value for --") + option + "(not a non-negative number)");
    }
}

typedef class Query {
    bool aligned;
    bool platforms;
    bool quality;
    bool schema;
    bool spots;
    bool contents;
    bool fingerprints;
    int count;

public:
    void doAligned(void) { ++count; aligned = true; }
    void doPlatforms(void) { ++count; platforms = true; }
    void doQuality(void) { ++count; quality = true; }
    void doSchema(void) { ++count; schema = true; }
    void doSpots(void) { ++count; spots = true; }
    void doContents(void) { ++count; contents = true; }
    void doFingerprints(void) { ++count; fingerprints = true; }

    int queries(void) const { return count; }

    bool needAligned(void) const { return aligned; }
    bool needPlatforms(void) const { return platforms; }
    bool needQuality(void) const { return quality; }
    bool needSchema(void) const { return schema; }
    bool needSpots(void) const { return spots; }
    bool needContents(void) const { return contents; }
    bool needFingerprints(void) const { return fingerprints; }
} Query;

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    Args * args;
    rc_t rc = ArgsMakeAndHandle( &args, argc, app.getArgV(),
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
                exit(3);
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
                    limit = GetPositiveNumber( args, OPTION_LIMIT );
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

                Query q{};

                {
                    rc = ArgsOptionCount( args, OPTION_SCHEMAVERS, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if (opt_count > 0)
                        q.doSchema();

                    rc = ArgsOptionCount( args, OPTION_PLATFORM, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                        q.doPlatforms();

                    rc = ArgsOptionCount( args, OPTION_ISALIGNED, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                        q.doAligned();

                    rc = ArgsOptionCount( args, OPTION_QUALITY, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                        q.doQuality();

                    rc = ArgsOptionCount( args, OPTION_SPOTLAYOUT, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                        q.doSpots();

                    rc = ArgsOptionCount( args, OPTION_CONTENTS, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                        q.doContents();

                    rc = ArgsOptionCount( args, OPTION_FINGERPRINT, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                        q.doFingerprints();
                }

                if (q.queries() > 1) {
                    if (formatter.getFormat() == Formatter::CSV)
                        throw VDB::Error(
                            "CVS format does not support multiple queries");
                    else if (formatter.getFormat() == Formatter::Tab)
                        throw VDB::Error(
                            "TAB format does not support multiple queries");
                }
                if (q.needSchema()) {
                    if (formatter.getFormat() == Formatter::CSV)
                        throw VDB::Error(
                            "unsupported CSV formatting option for schema");
                    else if (formatter.getFormat() == Formatter::Tab)
                        throw VDB::Error(
                            "unsupported TAB formatting option for schema");
                }

                Output(formatter.start());

                if ( q.needSchema() )
                {
                    Output( formatter.format( info.GetSchemaInfo() ) );
                }

                if ( q.needPlatforms() )
                {
                    Output( formatter.format( info.GetPlatforms() ) );
                }

                if ( q.needAligned() )
                {
                    Output( formatter.format(
                        info.IsAligned() ? "ALIGNED" : "UNALIGNED",
                        "ALIGNED" ) );
                }

                if ( q.needQuality() )
                {
                    Output( formatter.format(
                        info.QualityDescription(),
                        "QUALITY" ) );
                }

                SraInfo::Detail detail = SraInfo::Verbose;

                // detail level
                rc = ArgsOptionCount( args, OPTION_DETAIL, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                bool detail_specified = opt_count > 0;
                if ( detail_specified )
                {
                    switch( GetNonNegativeNumber( args, OPTION_DETAIL ) )
                    {
                    case 0: detail = SraInfo::Short; break;
                    case 1: detail = SraInfo::Abbreviated; break;
                    case 2: detail = SraInfo::Full; break;
                    case 3: detail = SraInfo::Verbose; break;
                    default: break; // anything higher than 2 is Verbose
                    }
                }

                if ( q.needSpots() )
                {

                    bool useConsensus = true;
                    rc = ArgsOptionCount( args, OPTION_SEQUENCE, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                    {
                        useConsensus = false;
                    }

                    rc = ArgsOptionCount( args, OPTION_ROWS, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    unsigned int topRows = 0;
                    if ( opt_count > 0 )
                    {
                        topRows = GetNonNegativeNumber( args, OPTION_ROWS );
                    }

                    Output ( formatter.format( info.GetSpotLayouts( detail, useConsensus, topRows ), detail ) );
                }

                if ( q.needContents() )
                {
                    Output( formatter.format( * info.GetContents().get(), detail ) );
                }

                if ( q.needFingerprints() )
                {   // has its own default detail level
                    SraInfo::Detail fp_detail = detail_specified ? detail : SraInfo::Short;
                    Output( formatter.format( info.GetFingerprints( fp_detail ), fp_detail ) );
                }

                Output( formatter.end() );
            }
            catch( const exception& /*ex*/)
            {
                //KOutMsg( "%s\n", ex.what() ); ? - should be in stderr already, at least for VDB::Error
                return 3;
            }
        }

        DESTRUCT(Args, args);
    }

    app.setRc( rc );
    return app.getExitCode();
}
