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

#include <kapp/main.h> /* KMain */
#include <kapp/args.h> /* KMain */

#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>       /* OUTMSG, KOutHandlerSetStdOut */
#include <klib/status.h>    /* KStsHandlerSetStdErr */
#include <klib/debug.h>     /* KDbgHandlerSetStdErr */
#include <klib/refcount.h>
#include <klib/rc.h>
#include <klib/log.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include "delite.h"
#include "delite_k.h"

#include <sysalloc.h>
#include <stdio.h>
#include <string.h>

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Kinda legendary intentions
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/******************************************************************************/

/*)))
  \\\   Celebrity is here ...
  (((*/

const char * ProgNm = "sra-delite";  /* Application program name */

static rc_t DeLiteParamsInit ( struct DeLiteParams * Params );
static rc_t DeLiteParamsWhack ( struct DeLiteParams * Params );

static rc_t __porseAndHandle ( int Arc, char * ArgV [], struct DeLiteParams * Params );
static rc_t __runKar ( struct DeLiteParams * Params );

rc_t CC
KMain ( int ArgC, char * ArgV [] )
{
    rc_t RCt;
    struct DeLiteParams DLP;

    RCt = 0;

    RCt = __porseAndHandle ( ArgC, ArgV, & DLP );
    if ( RCt == 0 ) {
        if ( ArgC == 1 ) {
            UsageSummary ( ProgNm );
        }
        else {
                /*  Something very special
                 */
            if ( DLP . _output_stdout ) {
                KOutHandlerSetStdErr();
                KStsHandlerSetStdErr();
                KLogHandlerSetStdErr();
                KDbgHandlerSetStdErr();
            }

            RCt = __runKar ( & DLP );
        }
    }

    DeLiteParamsWhack ( & DLP );

    return RCt;
}

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Dansing from here and till the fence
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Lite
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Arguments
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
#define OPT_APPCONFIG   "appconfig"
#define ALS_APPCONFIG   "a"
#define PRM_APPCONFIG   NULL

#define OPT_OUTPUT      "output"
#define ALS_OUTPUT      "o"
#define PRM_OUTPUT      NULL
#define STDOUT_OUTPUT   "--"

#define OPT_NOEDIT      "noedit"
#define ALS_NOEDIT      "n"
#define PRM_NOEDIT      NULL

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Params
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
rc_t
DeLiteParamsInit ( struct DeLiteParams * Params )
{
    memset ( Params, 0, sizeof ( struct DeLiteParams ) );

    return 0;
}   /* DeLiteParamsInit () */

rc_t
DeLiteParamsWhack ( struct DeLiteParams * Params )
{
    if ( Params != NULL ) {
        if ( Params -> _program != NULL ) {
            free ( ( char * ) Params -> _program );
            Params -> _program = NULL;
        }
        if ( Params -> _accession != NULL ) {
            free ( ( char * ) Params -> _accession );
            Params -> _accession = NULL;
        }
        if ( Params -> _accession_path != NULL ) {
            free ( ( char * ) Params -> _accession_path );
            Params -> _accession_path = NULL;
        }
        if ( Params -> _config != NULL ) {
            free ( ( char * ) Params -> _config );
            Params -> _config = NULL;
        }
        if ( Params -> _output != NULL ) {
            free ( ( char * ) Params -> _output );
            Params -> _output = NULL;
        }
        Params -> _output_stdout = false;

        Params -> _noedit = false;

        /* NO_NO_NO free ( Params ); */
    }
    return 0;
}   /* DeLiteParamsWhack () */

static
rc_t
DeLiteParamsSetProgram (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    const char * Value;
    rc_t RCt;

    Value = NULL;
    RCt = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsProgram ( TheArgs, NULL, & Value );
    if ( RCt == 0 ) {
        RCt = copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                                & Params -> _program,
                                                Value
                                                );
    }

    return RCt;
}   /* DeLiteParamsSetProgram () */

static
rc_t
DeLiteParamsSetAccession (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    uint32_t ParamCount;
    const char * Value;
    rc_t RCt;

    ParamCount = 0;
    Value = NULL;
    RCt = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsParamCount ( TheArgs, & ParamCount );
    if ( RCt == 0 ) {
        if ( ParamCount == 1 ) {
            RCt = ArgsParamValue (
                                TheArgs,
                                0,
                                ( const void ** ) & Value
                                );
            if ( RCt == 0 ) {
                RCt = copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                                & Params -> _accession,
                                                Value
                                                );
            }
        }
        else {
            RCt = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
        }
    }

    return RCt;
}   /* DeLiteParamsSetAccession () */

static
rc_t
DeLiteParamsSetConfig (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    rc_t RCt;
    const char * Value;
    uint32_t OptCount;

    RCt = 0;
    Value = NULL;
    OptCount = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsOptionCount ( TheArgs, OPT_APPCONFIG, & OptCount );
    if ( RCt == 0 ) {
        if ( OptCount != 0 ) {
            RCt = ArgsOptionValue (
                                    TheArgs,
                                    OPT_APPCONFIG,
                                    0,
                                    ( const void ** ) & Value
                                    ); 
            if ( RCt == 0 ) {
                RCt = copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                                & Params -> _config,
                                                Value
                                                );
            }
        }
    }

    return RCt;
}   /* DeLiteParamsSetConfig () */

static
rc_t
DeLiteParamsSetOutput (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    rc_t RCt;
    const char * Value;
    uint32_t OptCount;

    RCt = 0;
    Value = NULL;
    OptCount = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsOptionCount ( TheArgs, OPT_OUTPUT, & OptCount );
    if ( RCt == 0 ) {
        if ( OptCount != 0 ) {
            RCt = ArgsOptionValue (
                                    TheArgs,
                                    OPT_OUTPUT,
                                    0,
                                    ( const void ** ) & Value
                                    ); 
            if ( RCt == 0 ) {
                RCt = copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                                & Params -> _output,
                                                Value
                                                );
            }
        }
        else {
            return RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
        }
    }

    if ( RCt == 0 ) {
        if ( strcmp ( Params -> _output, STDOUT_OUTPUT ) == 0 ) {
            Params -> _output_stdout = true;
        }
    }

    return RCt;
}   /* DeLiteParamsSetOutput () */

static
rc_t
DeLiteParamsSetNoedit (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    rc_t RCt;
    uint32_t OptCount;

    RCt = 0;
    OptCount = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsOptionCount ( TheArgs, OPT_NOEDIT, & OptCount );
    if ( RCt == 0 ) {
        if ( OptCount != 0 ) {
            Params -> _noedit = true;
        }
    }

    return RCt;
}   /* DeLiteParamsSetNoedit () */

/*)))
  \\\   KApp and Options ...
  (((*/
static const char * UsgAppConfig [] = { "Application config file delite. Usually contains mapping from old schema to new", NULL };
static const char * UsgAppOutput [] = { "Name of output file, mondatory. If name is \"--\" output will be written to STDOUT", NULL };
static const char * UsgAppNoedit [] = { "Do not delete qualities, just repack archive", NULL };

struct OptDef DeeeOpts [] = {
    {       /* Where we will read config data to resolve entries */
        OPT_APPCONFIG,      /* option name */
        ALS_APPCONFIG,      /* option alias */
        NULL,               /* help generator */
        UsgAppConfig,       /* help as text is here */
        1,                  /* max amount */
        true,               /* need value */
        false               /* is required */
    },
    {       /* Where we will dump new KAR fiel */
        OPT_OUTPUT,         /* option name */
        ALS_OUTPUT,         /* option alias */
        NULL,               /* help generator */
        UsgAppOutput,       /* help as text is here */
        1,                  /* max amount */
        true,               /* need value */
        true                /* is required, yes, it requires */
    },
    {       /* Option do not edit archive, but repack it */
        OPT_NOEDIT,         /* option name */
        ALS_NOEDIT,         /* option alias */
        NULL,               /* help generator */
        UsgAppNoedit,       /* help as text is here */
        1,                  /* max amount */
        false,              /* need value */
        false               /* is required */
    }
};  /* OptDef */

const char UsageDefaultName[] = "sra-delite";

rc_t
__porseAndHandle (
                int ArgC,
                char * ArgV [],
                struct DeLiteParams * Params
)
{
    rc_t RCt;
    struct Args * TheArgs;

    RCt = 0;
    TheArgs = NULL;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    DeLiteParamsInit ( Params );

    RCt = ArgsMakeStandardOptions ( & TheArgs );
    if ( RCt == 0 ) {
        while ( 1 ) {
            RCt = ArgsAddOptionArray (
                                TheArgs, 
                                DeeeOpts,
                                sizeof ( DeeeOpts ) / sizeof ( OptDef )
                                );
            if ( RCt != 0 ) {
                break;
            }

            RCt = ArgsParse ( TheArgs, ArgC, ArgV );
            if ( RCt != 0 ) {
                break;
            }

            RCt = ArgsHandleStandardOptions ( TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetProgram ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            if ( ArgC == 1 ) {
                break;
            }

            RCt = DeLiteParamsSetAccession ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetConfig ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetOutput ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetNoedit ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            break;
        }

        ArgsWhack ( TheArgs );
    }

    return RCt;
}   /* __porseAndHande () */

rc_t CC
UsageSummary ( const char * ProgName )
{
    return KOutMsg (
                    "\n"
                    "Usage:\n"
                    "\n"
                    "  %s [options]"
                    " <accession>"
                    "\n"
                    "\n",
                    ProgName
                    );
}   /* UsageSummary () */

rc_t CC
Usage ( const struct Args * TheArgs )
{
    rc_t RCt;
    const char * ProgName;
    const char * FullPath;

    RCt = 0;
    ProgName = NULL;
    FullPath = NULL;

    if ( TheArgs == NULL ) {
        RCt = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    }
    else {
        RCt = ArgsProgram ( TheArgs, & FullPath, & ProgName );
    }

    if ( RCt != 0 ) {
        ProgName = FullPath = UsageDefaultName;
    }

    KOutMsg (
            "\n"
            "This program will remove QUALITY column from SRA archive,"
            " it will change schema, metadata, MD5 summ and repack archive"
            " for all archives except 454 or similar"
            "\n"
            );

    UsageSummary ( ProgName );

    KOutMsg ( "Options:\n" );

    HelpOptionLine (
                ALS_APPCONFIG,
                OPT_APPCONFIG,
                PRM_APPCONFIG,
                UsgAppConfig
                );

    HelpOptionLine (
                ALS_OUTPUT,
                OPT_OUTPUT,
                PRM_OUTPUT,
                UsgAppOutput
                );

    HelpOptionLine (
                ALS_NOEDIT,
                OPT_NOEDIT,
                PRM_NOEDIT,
                UsgAppNoedit
                );

    KOutMsg ( "\n" );

    KOutMsg ( "Standard Options:\n" );
    HelpOptionsStandard ();
    HelpVersion ( FullPath, KAppVersion () );

    return RCt;
}   /* Usage () */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Sanctuary
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

static rc_t __readKar ( struct DeLiteParams * Params );

rc_t
__runKar ( struct DeLiteParams * Params )
{
    const char * KfgMsg = Params -> _config == NULL ? "default" : Params -> _config;

    KOutMsg ( "KAR [%s]\n", Params -> _accession );
    pLogMsg ( klogInfo, "KAR [$(acc)]", "acc=%s", Params -> _accession );

    KOutMsg ( "KFG [%s]\n", KfgMsg );
    pLogMsg ( klogInfo, "KFG [$(cfg)]", "cfg=%s", KfgMsg );

    if ( Params -> _noedit ) {
        KOutMsg ( "RUN [idle]\n" );
        pLogMsg ( klogInfo, "RUN [idle]", "" );
    }

    return __readKar ( Params );
}   /* __runKar () */

rc_t
__readKar ( struct DeLiteParams * Params )
{
    rc_t RCt = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArc, rcReading, rcParam, rcNull );
    }

    RCt = DeLiteResolvePath (
                            ( char ** ) & ( Params -> _accession_path ),
                            Params -> _accession
                            );
    if ( RCt == 0 ) {
        KOutMsg ( "PTH [%s]\n", Params -> _accession_path );
        pLogMsg (
                klogInfo,
                "PTH [$(path)]",
                "path=%s",
                Params -> _accession_path
                );

        RCt = Delite ( Params );
    }
    else {
        KOutMsg ( "NOT FND [%s] RC [%d]\n", Params -> _accession, RCt );
        pLogErr (
                klogErr,
                RCt,
                "NOT FOUND [$(acc)] RC [$(rc)]",
                "acc=%s,rc=%u",
                Params -> _accession,
                RCt
                );
    }

    if ( RCt == 0 ) {
        KOutMsg ( "DONE\n" );
    }

    return RCt;
}   /* __readKar () */
