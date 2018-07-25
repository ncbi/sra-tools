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
#include <klib/out.h> /* OUTMSG */
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

const char * ProgNm = "delite";  /* Application program name */

struct DeLiteParams {
    const char * program;
    const char * accession;
    const char * config;
    const char * output;
};

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
        if ( Params -> program != NULL ) {
            free ( ( char * ) Params -> program );
            Params -> program = NULL;
        }
        if ( Params -> accession != NULL ) {
            free ( ( char * ) Params -> accession );
            Params -> accession = NULL;
        }
        if ( Params -> config != NULL ) {
            free ( ( char * ) Params -> config );
            Params -> config = NULL;
        }
        if ( Params -> output != NULL ) {
            free ( ( char * ) Params -> output );
            Params -> output = NULL;
        }

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
        RCt = _copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                                    & Params -> program,
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
                RCt = _copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                            & Params -> accession,
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
                RCt = _copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                            & Params -> config,
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
                RCt = _copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                            & Params -> config,
                                            Value
                                            );
            }
        }
    }

    return RCt;
}   /* DeLiteParamsSetOutput () */

/*)))
  \\\   KApp and Options ...
  (((*/
static const char * UsgAppConfig [] = { "Application config file delite", NULL };
static const char * UsgAppOutput [] = { "Name of output file, optional, by default it send data to STDOUT" };

struct OptDef DeeeOpts [] = {
    {       /* Mystical Application Config option */
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
        UsgAppConfig,       /* help as text is here */
        1,                  /* max amount */
        true,               /* need value */
        false               /* is required */
    }
};  /* OptDef */

const char UsageDefaultName[] = "delite";

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
            "Hola, amigo ... "
            "This programm will doo it well\n"
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

    KOutMsg ( "\n" );

    KOutMsg ( "Standard Options:\n" );
    HelpOptionsStandard ();
    HelpVersion ( FullPath, KAppVersion () );

    return RCt;
}   /* Usage () */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Sanctuary
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

static rc_t __readKar ( const char * Accession);

rc_t
__runKar ( struct DeLiteParams * Params )
{
    if ( Params -> config != NULL ) {
        printf ( " KAR [%s] with KFG [%s]\n", Params -> accession, Params -> config ); 
    }
    else {
        printf ( " KAR [%s] without KFG\n", Params -> accession ); 
    }

    return __readKar ( Params -> accession );
}   /* __runKar () */

rc_t
__readKar ( const char * Accession )
{
    rc_t RCt;
    char * Path;
    const struct karChive * Chive;

    RCt = 0;
    Path = NULL;
    Chive = NULL;

    if ( Accession == NULL ) {
        return RC ( rcApp, rcArc, rcReading, rcParam, rcNull );
    }

    RCt = DeLiteResolvePath ( & Path, Accession );
    if ( RCt == 0 ) {
        printf ( "FOUND [%s] -> [%s]\n", Accession, Path );

        RCt = karChiveOpen ( & Chive, ( const char * ) Path );
        if ( RCt == 0 ) {
            karChiveDump ( Chive, true );

            RCt = karChiveEdit ( Chive );
            if ( RCt == 0 ) {
                RCt = karChiveWrite ( Chive, "jojo", true );
            }

            karChiveRelease ( Chive );
        }
    }
    else {
        printf ( "NOT FOUND [%s] RC [%u]\n", Accession, RCt );
    }

    free ( Path );

    return RCt;
}   /* __readKar () */
