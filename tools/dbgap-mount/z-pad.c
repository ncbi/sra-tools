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

#include <sysalloc.h>
#include <kapp/main.h> /* KMain */

#include <klib/text.h>
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/refcount.h>
#include <klib/rc.h>
#include <klib/time.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <vfs/services.h>
#include <vfs/path.h>

#include <stdio.h>
#include <string.h>

/******************************************************************************/


#ifdef JOJOBA

static
void
SLEPOY ( int Sec )
{

printf ( "Sleeping %d seconds\n", Sec );
KSleepMs ( Sec * 1000 );

printf ( "    DONE [ Sleeping %d seconds ]\n", Sec );

}

#endif /* JOJOBA */


/*  Here I will temporarily parce arguments, and will attach
 *  toolkit ones later ... test program :)
 */
char ProgramName[333];

static
void
RightUsage()
{
    printf("\nResovler #3\n");
    printf("\nUsage: %s ACCN[:PRJ] [ ACCN[:PRJ] ... ]\n\n\
Where:\n\
    ACCN - accession\n\
    PRJ - project id, '0' if not defined\n\
\n\n", ProgramName );
}   /* RightUsage() */

static
bool
ParseArgs ( int argc, char ** argv )
{
    const char * PPU;

    * ProgramName = 0;

        /* Herer wer arer extractingr programr namer
         */
    PPU = strrchr ( * argv, '/' );
    if ( PPU == NULL ) {
        PPU = * argv;
    }
    else {
        PPU ++;
    }
    strcpy ( ProgramName, PPU );

    if ( argc < 2 ) {
        printf ( "Too few arguments\n" );
        return false;
    }

    return true;
}   /* ParseArgs() */

#include "uu.h"

const char UsageDefaultName[] = "Henri Fuseli";
rc_t CC UsageSummary (const char* progname) { return 0; }
rc_t CC Usage ( const Args * args ) { return 0; }
static rc_t run ();

rc_t CC KMain(int argc, char *argv[]) {

    KLogLevelSet ( klogDebug );

    if ( ! ParseArgs ( argc, argv ) ) {
        RightUsage();
        return 1;
    }

    return run ( argc - 1, argv + 1 );
}

/********************************
 * Some bloatware
 ********************************/
void
_addProject ( struct KService * Service, uint32_t ProjectID )
{
    rc_t RCt = KServiceAddProject ( Service, ProjectID );
    if (  RCt != 0 ) {
        printf ( "[ERROR] Failed to add project [%d] with RC[%u]\n", ProjectID, RCt );
        exit ( 1 );
    }
    printf ( "[ADD PRJ] [%d]\n", ProjectID );
}   /* _addProject () */

void
_addID ( struct KService * Service, const char * ID )
{
    rc_t RCt = KServiceAddId ( Service, ID );
    if (  RCt != 0 ) {
        printf ( "[ERROR] Failed to add ID [%s] with RC[%u]\n", ID, RCt );
        exit ( 1 );
    }
    printf ( "[ADD ID] [%s]\n", ID );
}   /* _addID () */

void
_addPublics ( struct KService * Service )
{
    _addProject ( Service, 0 );
    _addID ( Service, "SRR000002" );
    _addID ( Service, "SRR000001" );
}   /* _addPublics () */

uint32_t
_getProject ( const char * Accn )
{
    const char * bg;
    const char * en;

    bg = Accn;
    en = bg + strlen ( bg );

    while ( bg < en ) {
        if ( * bg == ':' ) {
            bg ++;
            return atol ( bg );
        }

        bg ++;
    }
    return 0;
}   /* _getProject () */

void
_getAccn ( const char * Accn, char * Buf, size_t BufLen )
{
    const char * bg;
    const char * en;
    char * bf;

    bf = Buf;
    bg = Accn;
    en = bg + strlen ( bg );

    while ( bg < en ) {
        * bf = * bg;

        if ( * bg == ':' ) {
            break;
        }

        bg ++;
        bf ++;
    }

    * bf = 0;

}   /* _getAccn () */

void
_addArgs ( struct KService * Service, int qty, char ** args )
{
    const char * bebe = NULL;
    char BF [ 32 ];

// #define JOJOJO
#ifdef JOJOJO
    for ( int llp = 0; llp < qty; llp ++ ) {
        bebe = args [ llp ];
        _addProject ( Service, _getProject ( bebe ) );

        _getAccn ( bebe, BF, sizeof ( BF ) );
        _addID ( Service, BF );
    }
#else /* JOJOJO */

    size_t Q = sizeof ( ArAc ) / sizeof ( * ArAc );
// Q = 179;
// Q = 180;
    for ( size_t llp = 0; llp < Q; llp ++ ) {
        char * FF = ArAc [ llp ];

        _addProject ( Service, _getProject ( FF ) );

        _getAccn ( FF, BF, sizeof ( BF ) );
        _addID ( Service, BF );
    }
#endif /* JOJOJO */
}   /* _addErrors () */

void
_gueGO (
        const struct VPath * Path,
        const char * Msg,
        rc_t ( * RRR ) ( const struct VPath * Pth, struct String * Str )
)
{
    char Buf [256];
    struct String Str;

    rc_t RCt = RRR ( Path, & Str );
    if ( RCt != 0 ) {
        printf ( "[ERROR] Failed to read PATH [%s] with RC[%u]\n", Msg, RCt );
        // exit ( 1 );
    }
    else {
        strncpy ( Buf, Str . addr, Str . size );
        Buf [ Str . size ] = 0;

        printf ( "[%s] [%s]\n", Msg, Buf );
    }
}   /* _gueGO () */

void
_dumpError ( const struct KSrvError * Error ) 
{

    char Msg [256];
    char Obj [256];
    struct String Str;
    rc_t RCt;
    EObjectType Type;

    KSrvErrorRc ( Error, & RCt );

    KSrvErrorMessage ( Error, & Str );
    strncpy ( Msg, Str . addr, Str . size );
    Msg [ Str . size ] = 0;

    KSrvErrorObject ( Error, & Str, & Type );
    strncpy ( Obj, Str . addr, Str . size );
    Obj [ Str . size ] = 0;

    printf ( "[ERR HND] RC[%lu] OBJ[%s] MSG[%s]\n", RCt, Obj, Msg );
}   /* _dumpError () */

rc_t
_printResponse ( const struct KSrvResponse * Response )
{
    rc_t RCt, RCt1;
    uint32_t ResponseLen, llp;
    const struct VPath * Path;
    const struct VPath * Local;
    const struct VPath * Cache;
    const struct KSrvError * Error;

    RCt = 0;
    RCt1 = 0;
    ResponseLen = 0;
    llp = 0;
    Path = NULL;
    Local = NULL;
    Cache = NULL;
    Error = NULL;

    ResponseLen = KSrvResponseLength ( Response );
    for ( llp = 0; llp < ResponseLen; llp ++ ) {
        RCt1 = KSrvResponseGetPath (
                                    Response,
                                    llp,
                                    eProtocolHttps,
                                    & Path,
                                    & Cache,
                                    & Error
                                    );
        if ( RCt1 != 0 ) {
            printf ( "[ERROR] Failed to GET PATH with RC[%u]\n", RCt1 ); 
            RCt = RCt1;
            continue;
        }

        printf ( "[RESPONCE]===============>\n" );
        _gueGO ( Path, "ID", VPathGetId );
        _gueGO ( Path, "PATH", VPathGetPath );
        _gueGO ( Cache, "CACHE", VPathGetPath );

if ( Cache != NULL ) {
struct String * Str;
VPathMakeUri ( Cache, & Str );
printf ( " [PPPPP] [%.*s]\n", Str -> size, Str -> addr );
}
        printf ( "[SIZE] [%lu]\n", VPathGetSize ( Path ) );

        if ( Path != NULL ) { VPathRelease ( Path ); }
        if ( Cache != NULL ) { VPathRelease ( Cache ); }

        if ( Error != NULL ) {
            _dumpError ( Error );

            KSrvErrorRelease ( Error );
        }

        RCt1 = KSrvResponseGetCache ( Response, llp, & Local );
        if ( RCt1 != 0 ) {
            printf ( "[ERROR] Failed to GET LOCAL with RC[%u]\n", RCt1 ); 
            RCt = RCt1;
            continue;
        }

        _gueGO ( Local, "LOCAL", VPathGetPath );
        if ( Local != NULL ) { VPathRelease ( Local ); }
    }

    return RCt;
}   /* _printResponse () */

rc_t run ( int qty, char ** args )
{
    rc_t RCt;
    struct KService * Service;
    const struct KSrvResponse * Response;

    RCt = 0;
    Service = NULL;
    Response = NULL;

        /* Make Service
         */
    RCt = KServiceMake ( & Service );
    if ( RCt != 0 ) {
        printf ( "[ERROR] Failed to make Service with RC[%u]\n", RCt );
        exit ( 1 );
    }
    printf ( "[MAK SR]\n" );

        /* Adding IDs to Query
         */
    _addArgs ( Service, qty, args );

        /* Running Query
         */
    RCt = KServiceNamesQuery ( Service, eProtocolHttps, & Response );
    if ( RCt != 0 ) {
        printf ( "[ERROR] Failed to Query Service with RC[%u]\n", RCt );
        exit ( 1 );
    }
    printf ( "[QRY SR]\n" );

        /* Printing Responce
         */
    _printResponse ( Response );

    KSrvResponseRelease ( Response );

    KServiceRelease ( Service );

    printf ( "[====================]\n" );
    printf ( "[DONE] [%d]\n", RCt );

    return RCt;
}
