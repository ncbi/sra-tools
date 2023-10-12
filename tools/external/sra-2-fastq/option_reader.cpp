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

#include <klib/out.h>
#include <klib/log.h>

#include <kapp/main.h>

#include "option_reader.hpp"
#include "helper.hpp"

// all options
#define OPTION_MODE         "mode"
#define OPTION_FORMAT       "format"
#define OPTION_THREADS      "threads"
#define OPTION_SHOW_OPT     "show-opts"
#define OPTION_SHOW_REP     "show-report"

// all aliases of the options ( aka short options )
#define ALIAS_MODE          "m"
#define ALIAS_FORMAT        "f"
#define ALIAS_THREADS       "e"

// all usage help texts for the options
static const char * mode_usage[]    = { "modes are: ...", nullptr };
static const char * format_usage[]  = { "formats are: fastq|fasta", nullptr };
static const char * threads_usage[] = { "number of threads", nullptr };
static const char * show_opts_usage[] = { "show commandline args", nullptr };
static const char * show_rep_usage[]  = { "show inspector report", nullptr };

// the array of options, to be given to the args-module in kapp
OptDef ConverterOptions[] =
{
    { OPTION_MODE,          ALIAS_MODE,         nullptr, mode_usage,        1, true,    false },
    { OPTION_FORMAT,        ALIAS_FORMAT,       nullptr, format_usage,      1, true,    false },
    { OPTION_THREADS,       ALIAS_THREADS,      nullptr, threads_usage,     1, true,    false },
    { OPTION_SHOW_OPT,      nullptr,            nullptr, show_opts_usage,   1, false,   false },
    { OPTION_SHOW_REP,      nullptr,            nullptr, show_rep_usage,    1, false,   false },
};

static rc_t make_args( Args ** args, int argc, char *argv [] ){
    size_t option_count = sizeof ConverterOptions / sizeof ConverterOptions [ 0 ];
    rc_t rc = ArgsMakeAndHandle( args, 
                                 argc, argv,
                                 1, 
                                 ConverterOptions,
                                 option_count );
    DISP_RC( rc, "ArgsMakeAndHandle() failed" );
    return rc;
}

static std::string read_string( Args * args, const char * option, rc_t * rc_out, const char * dflt ) {
    uint32_t count;
    std::string res = dflt;
    rc_t rc = ArgsOptionCount( args, option, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( 0 == rc && count > 0 ) {
        const char* s = nullptr;
        rc = ArgsOptionValue( args, option, 0, ( const void** )&s );
        DISP_RC( rc, "ArgsOptionValue() failed" );
        if ( 0 == rc && nullptr != s ) {
            res = std::string( s );
        }
    }
    *rc_out = rc;
    return res;
}

static uint32_t read_u32( Args * args, const char * option, rc_t * rc_out, uint32_t dflt ) {
    uint32_t count;
    uint32_t res = dflt;
    rc_t rc = ArgsOptionCount( args, option, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( 0 == rc && count > 0 ) {
        const char* s = nullptr;
        rc = ArgsOptionValue( args, option, 0, ( const void** )&s );
        DISP_RC( rc, "ArgsOptionValue() failed" );
        if ( 0 == rc && nullptr != s ) {
            try {
                uint32_t tmp = std::stoi( s );
                res = tmp;
            } catch (...) {
                // nothing, res remains at default value...
            }
        }
    }
    *rc_out = rc;
    return res;
}

static bool read_bool( Args * args, const char * option, rc_t * rc_out ) {
    uint32_t count;
    bool res = false;
    rc_t rc = ArgsOptionCount( args, option, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( 0 == rc && count > 0 ) {
        res = true;
    }
    return res;
}

OptionsPtr read_options( int argc, char *argv [], rc_t * rc_out ) {
    auto res = OptionsPtr( new Options );
    Args * args;
    rc_t rc = make_args( &args, argc, argv );
    if ( 0 == rc ) {
        uint32_t param_count = 0;
        rc = ArgsParamCount( args, &param_count );
        DISP_RC( rc, "ArgsParamCount() failed" );
        if ( 0 == rc ) {
            if ( param_count == 0 || param_count > 1 ) {
                MiniUsage( args );
                rc = -1;
            } else {
                const char * accession = nullptr;
                rc = ArgsParamValue( args, 0, ( const void ** )&( accession ) );
                DISP_RC( rc, "ArgsParamValue() failed" );
                if ( 0 == rc ) {
                    res -> accession = std::string( accession );
                }
            }
        }
        if ( 0 == rc ) {
            auto s = read_string( args, OPTION_MODE, &rc, "" );
            if ( 0 == rc && !s.empty() ) {
                res -> split_mode = Options::String2SplitMode( s );
            }
        }
        if ( 0 == rc ) {
            auto s = read_string( args, OPTION_FORMAT, &rc, "" );
            if ( 0 == rc && !s.empty() ) {
                res -> format = Options::String2Format( s );
            }
        }
        if ( 0 == rc ) {
            res -> threads = read_u32( args, OPTION_THREADS, &rc, 6 );
        }
        if ( 0 == rc ) {
            res -> show_opts = read_bool( args, OPTION_SHOW_OPT, &rc );
        }
        if ( 0 == rc ) {
            res -> show_report = read_bool( args, OPTION_SHOW_REP, &rc );
        }
        
        DESTRUCT( Args, args );            
    }
    *rc_out = rc;
    return res;
}

void option_lines( void ) {
    size_t option_count = sizeof ConverterOptions / sizeof ConverterOptions [ 0 ];
    KOutMsg ( "Options:\n" );
    for ( size_t i = 0; i < option_count; ++i ) {
        HelpOptionLine( ConverterOptions[ i ] . aliases,
                        ConverterOptions[ i ] . name,
                        nullptr,
                        ConverterOptions[ i ] . help );
    }
    HelpOptionsStandard ();
}

/* ---------------------------------------------------------------------------------------- */

// mandatory by kapp/main.h
const char UsageDefaultName[] = "sra-2-fastq";

// mandatory by kapp/main.h
rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
    "Usage:\n"
    "  %s <accession> [options]\n"
    "\n", progname);
}

// mandatory by kapp/main.h
rc_t CC Usage ( const Args * args ) {
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;
    
    if ( args == nullptr ) {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    } else {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }
    
    if ( rc ) {
        progname = fullpath = UsageDefaultName;
    }
    
    UsageSummary ( progname );
    option_lines();
    HelpVersion ( fullpath, KAppVersion() );
    
    return rc;
}
