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

const char * ProgNm = "chkite";  /* Application program name */

static rc_t __porseAndHandle (
                            int Arc,
                            char * ArgV [],
                            const char ** ProgName,
                            const char ** PathToArchive
                            );
static rc_t __runKar ( const char * PathToArchive );

rc_t CC
KMain ( int ArgC, char * ArgV [] )
{
    rc_t RCt;
    const char * P2A;
    const char * PRG;

    RCt = 0;
    P2A = NULL;
    PRG = NULL;

    RCt = __porseAndHandle ( ArgC, ArgV, & PRG, & P2A );
    if ( RCt == 0 ) {
        if ( ArgC == 1 ) {
            UsageSummary ( ProgNm );
        }
        else {
            RCt = __runKar ( P2A );
        }
    }

    if ( P2A != NULL ) {
        free ( ( char * ) P2A );
        P2A = NULL;
    }

    if ( PRG != NULL ) {
        free ( ( char * ) PRG );
        PRG = NULL;
    }

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

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Params
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t
DeLiteParamsSetProgram (
                        const char ** ProgName,
                        const struct Args * TheArgs
)
{
    const char * Value;
    rc_t RCt;

    Value = NULL;
    RCt = 0;

    if ( ProgName != NULL ) {
        * ProgName = NULL;
    }

    if ( ProgName == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsProgram ( TheArgs, NULL, & Value );
    if ( RCt == 0 ) {
        RCt = copyStringSayNothingRelax ( ProgName, Value );
    }

    return RCt;
}   /* DeLiteParamsSetProgram () */

static
rc_t
DeLiteParamsSetPathToArchive (
                        const char ** PathToArchive,
                        const struct Args * TheArgs
)
{
    uint32_t ParamCount;
    const char * Value;
    rc_t RCt;

    ParamCount = 0;
    Value = NULL;
    RCt = 0;

    if ( PathToArchive != NULL ) {
        * PathToArchive = NULL;
    }

    if ( PathToArchive == NULL ) {
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
                RCt = copyStringSayNothingRelax ( PathToArchive, Value );
            }
        }
        else {
            RCt = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
        }
    }

    return RCt;
}   /* DeLiteParamsSetPathToArchive () */

/*)))
  \\\   KApp and Options ...
  (((*/

const char UsageDefaultName[] = "sra-chkite";

rc_t
__porseAndHandle (
                int ArgC,
                char * ArgV [],
                const char ** ProgName,
                const char ** PathToArchive
)
{
    rc_t RCt;
    struct Args * TheArgs;

    RCt = 0;
    TheArgs = NULL;

    if ( ProgName != NULL ) {
        * ProgName = NULL;
    }

    if ( PathToArchive != NULL ) {
        * PathToArchive = NULL;
    }

    if ( ProgName == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    if ( PathToArchive == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsMakeStandardOptions ( & TheArgs );
    if ( RCt == 0 ) {
        while ( 1 ) {
            RCt = ArgsParse ( TheArgs, ArgC, ArgV );
            if ( RCt != 0 ) {
                break;
            }

            RCt = ArgsHandleStandardOptions ( TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetProgram ( ProgName, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            if ( ArgC == 1 ) {
                break;
            }

            RCt = DeLiteParamsSetPathToArchive ( PathToArchive, TheArgs );
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
                    "  %s [standard_options] <path_to_archive>"
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
            "That program will do some simple checks on archive "
            "processed by sra-delite program. These check are :\n\n"
            "    check that TOC files are sorted by size\n"
            "    check that all links in TOC are resolved\n"
            "    check that Metadata file contains \"delited\" record\n"
            "    check that MD5 file is current\n"
            "\n"
            "NOTE: it does not check schema ... how could it do that, huh?\n"
            "NOTE: it requires path to archive, not accession"
            "\n"
            );

    UsageSummary ( ProgName );

    KOutMsg ( "Standard Options:\n" );
    HelpOptionsStandard ();
    HelpVersion ( FullPath, KAppVersion () );

    return RCt;
}   /* Usage () */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Sanctuary
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

static rc_t __checkKar ( const char * PathToArchive );

rc_t
__runKar ( const char * PathToArchive )
{
    if ( PathToArchive == NULL ) {
        return RC ( rcApp, rcArc, rcReading, rcParam, rcNull );
    }

    KOutMsg ( "PTH [%s]\n", PathToArchive );
    pLogMsg ( klogInfo, "KAR [$(path)]", "path=%s", PathToArchive );
    return __checkKar ( PathToArchive );
}   /* __runKar () */

rc_t
__checkPathToArchive ( const char * PathToArchive )
{
    rc_t RCt;
    struct KDirectory * NatDir;
    uint32_t PathType;

    RCt = 0;
    NatDir = NULL;
    PathType = kptNotFound;

    RCt = KDirectoryNativeDir ( & NatDir );
    if ( RCt == 0 ) {
        PathType = KDirectoryPathType ( NatDir, "%s", PathToArchive );

        switch ( PathType ) {
            case kptFile :
                break;
            case kptNotFound:
                RCt = RC ( rcApp, rcPath, rcValidating, rcItem, rcNotFound );
                break;
            default:
                RCt = RC ( rcApp, rcPath, rcValidating, rcItem, rcInvalid );
                break;
        }

        KDirectoryRelease ( NatDir );
    }

    return RCt;
}   /* __checkPathToArchive () */

rc_t
__checkKar ( const char * PathToArchive )
{
    rc_t RCt = 0;

    if ( PathToArchive == NULL ) {
        return RC ( rcApp, rcArc, rcReading, rcParam, rcNull );
    }

    RCt = __checkPathToArchive ( PathToArchive );
    if ( RCt == 0 ) {
        RCt = Checkite ( PathToArchive );
    }
    else {
        KOutMsg ( "NOT FND [%s] RC [%d]\n", PathToArchive, RCt );
        pLogErr (
                klogErr,
                RCt,
                "NOT FOUND [$(path)] RC [$(rc)]",
                "path=%s,rc=%u",
                PathToArchive,
                RCt
                );
    }

    if ( RCt == 0 ) {
        KOutMsg ( "DONE\n" );
    }

    return RCt;
}   /* __checkKar () */
