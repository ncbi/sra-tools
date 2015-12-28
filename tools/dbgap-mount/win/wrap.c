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
#include <kapp/args.h> /* KMain */

#include <klib/text.h>
#include <klib/out.h> /* OUTMSG */
#include <klib/rc.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <xfs/xfs-defs.h>
#include <xfs/xlog.h>

#include "../mount-tool.h"

#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*  There is some inconvinience. Before KMain is called, some filthy
 *  converts all cool WCHAR paths to uncool linux-style char paths
 *  so we had to go throught all parameters and regenerate command
 *  line with converting into path everything what looks like a path.
 *  or ... what starts with \
 */

static
rc_t CC
_CharToWChar ( WCHAR * Buf, size_t BufSz, const char * Str )
{
    XFS_CAN ( Str )
    XFS_CAN ( Buf )


    return  mbstowcs ( Buf, Str, strlen ( Str ) ) == - 1
                                            ? XFS_RC ( rcInvalid )
                                            : 0
                                            ;
}   /* _CharToWChar () */

LIB_EXPORT
rc_t CC
XFSPathInnerToNative (
                    WCHAR * NativePathBuffer,
                    size_t NativePathBufferSize,
                    const char * InnerPath,
                    ...
                    );

static
rc_t CC
_CheckConvert ( const char * Str, WCHAR * Buf, size_t BufSz )
{
    rc_t RCt = 0;

    XFS_CAN ( Str )
    XFS_CAN ( Buf )

    if ( * Str == '/' || ( * Str == '.' && * ( Str + 1 ) == '/' ) ) {
            /* That is candidate for a path */
        RCt = XFSPathInnerToNative ( Buf, BufSz, Str );
    }
    else {
            /* Just converting to WCHAR */
        RCt = _CharToWChar ( Buf, BufSz, Str );
    }

    return RCt;
}   /* _CheckConvert () */

static
bool CC
_IsDaemonizeToken ( const char * Param )
{
    const char * Str = Param;

    if ( Str != NULL ) {
        if ( * Str == '-' ) {
            Str ++;
            if ( * Str == '-' ) {
                Str ++;
                return strcmp ( Str, OPT_DAEMONIZE ) == 0;
            }
            else {
                return strcmp ( Str, ALS_DAEMONIZE ) == 0;
            }
        }
    }

    return false;
}   /* _IsDaemonizeToken () */

static
rc_t CC
_GetProgName ( struct Args * TheArgs, WCHAR * Buf, size_t BufSz )
{
    rc_t RCt;
    const char * FullPath;
    const char * ProgName;

    RCt = 0;
    FullPath = NULL;
    ProgName = NULL;

    XFS_CAN ( TheArgs )
    XFS_CAN ( Buf )

    RCt = ArgsProgram ( TheArgs, & FullPath, & ProgName );
    if ( RCt == 0 ) {
        RCt = _CharToWChar ( Buf, BufSz, ProgName );
    }

    return RCt;
}   /* _GetProgName () */

static
rc_t CC
_MakeCommandString (
                struct Args * TheArgs,
                WCHAR ** Str,
                WCHAR ** Prog
)
{
    rc_t RCt;
    uint32_t Cnt;
    uint32_t Idx;
    const char * Val;
    WCHAR BF [ 4096 ];
    WCHAR BF1 [ 1096 ];

    RCt = 0;
    Cnt = 0;
    Idx = 0;
    Val = NULL;
    * BF = 0;
    * BF1 = 0;

    XFS_CSAN ( Str )
    XFS_CAN ( TheArgs )
    XFS_CAN ( Str )

    RCt = ArgsArgvCount ( TheArgs, & Cnt );
    if ( RCt == 0 ) {
        for ( Idx = 0; Idx < Cnt; Idx ++ ) {
            ZeroMemory ( BF1, sizeof ( BF1 ) );

            RCt = ArgsArgvValue ( TheArgs, Idx, & Val );
            if ( RCt != 0 ) {
                break;
            }

            if ( _IsDaemonizeToken ( Val ) ) {
                continue;
            }

            RCt = _CheckConvert ( Val, BF1, sizeof ( BF1 ) );
            if ( RCt != 0 ) {
                break;
            }

            if ( 0 < Idx ) {
                wcscat ( BF, L" " );
            }
            wcscat ( BF, BF1 );
        }
    }

    if ( RCt == 0 ) {
        ZeroMemory ( BF1, sizeof ( BF1 ) );
        RCt = _GetProgName ( TheArgs, BF1, sizeof ( BF1 ) );
    }

    * Str = _wcsdup ( BF );
    * Prog = _wcsdup ( BF1 );

    if ( * Str == NULL || * Prog == NULL ) {
        RCt = XFS_RC ( rcExhausted );

        if ( * Str != NULL ) {
            free ( * Str );
            * Str = NULL;
        }

        if ( * Prog != NULL ) {
            free ( * Prog );
            * Prog = NULL;
        }
    }

    return 0;
}   /* _MakeCommandString () */

static
rc_t CC
_CreateDetached ( LPCTSTR AppName, LPTSTR Cmd )
{
    rc_t RCt;
    BOOL Ret;
    STARTUPINFO StartInfo;
    PROCESS_INFORMATION Process;
    int Err;

    RCt = 0;
    Ret = FALSE;
    ZeroMemory ( & StartInfo, sizeof( StartInfo ) );
    ZeroMemory ( & Process, sizeof( Process ) );
    Err = 0;

    Ret = CreateProcessW (
                AppName,            // application name
                Cmd,                // command line
                NULL,               // process attributes
                NULL,               // trhead attributes
                FALSE,              // no file handler inheritance
                DETACHED_PROCESS,   // Creation Flags
                NULL,               // Inherit environment
                NULL,               // Inherit CWD
                & StartInfo,        // Startup Info
                & Process           // Process Information
                );
    if ( Ret == FALSE ) {
        Err = GetLastError ();
        wprintf ( L"CRITICAL ERROR: Can not run in background [%s] Error[%d]\n", AppName, Err );
        RCt = XFS_RC ( rcInvalid );
    }
    else {
        wprintf ( L"RUN DETACHED\n" );
    }

    return RCt;
}   /* _CreateDetached () */

static
rc_t CC
RunDaemon ( struct Args * TheArgs )
{
    rc_t RCt;
    WCHAR * CmdLine;
    WCHAR * ProgName;

    RCt = 0;
    CmdLine = NULL;
    ProgName = NULL;

    RCt = _MakeCommandString ( TheArgs, & CmdLine, & ProgName );
    if ( RCt == 0 ) {
        RCt = _CreateDetached ( ProgName, CmdLine );        

        free ( CmdLine );
        free ( ProgName );
    }

    return 0;
}   /* RunDaemon () */

static
rc_t CC
_SetLog ( struct Args * TheArgs )
{
    rc_t RCt;
    const char * LogFile;
    uint32_t OptCount;

    RCt = 0;
    LogFile = NULL;
    OptCount = 0;

    XFS_CAN ( TheArgs )

    if ( TheArgs != NULL ) {
        RCt = ArgsOptionCount ( TheArgs, OPT_LOGFILE, & OptCount );
        if ( RCt == 0 && OptCount == 1 ) {
            RCt = ArgsOptionValue (
                                TheArgs,
                                OPT_LOGFILE,
                                0,
                                ( const void ** ) & LogFile
                                );
        }
    }

    if ( RCt == 0 ) {
        if ( LogFile != NULL ) {
            printf ( "Log File [%s]\n", LogFile );
        }
        else {
            printf ( "Log File [NULL]\n" );
        }
        RCt = XFSLogInit ( LogFile );
    }

    return RCt;
}   /* _SetLog () */

/* By default should call DoFukan ()
 */
rc_t CC
WrapIt (
        struct Args * TheArgs, 
        rc_t ( CC * runner ) ( struct Args * TheArgs )
)
{
    rc_t RCt;
    uint32_t OptCount;

    RCt = 0;
    OptCount = 0;

    XFS_CAN ( TheArgs );
    XFS_CAN ( runner );

    RCt = ArgsOptionCount ( TheArgs, OPT_DAEMONIZE, & OptCount );
    if ( RCt == 0 ) {
        if ( OptCount == 1 ) {
                /* Here we are daemonizing
                 */
            RCt = RunDaemon ( TheArgs );
        }
        else {
                /* Setting log file
                 */
            RCt = _SetLog ( TheArgs );
            if ( RCt == 0 ) {
                RCt = runner ( TheArgs );
            }
            else {
                XFSLogErr ( RCt, "CRITICAL ERROR: Can not initialize log file\n" );
            }
        }
    }

        /* TODO : that is soo stupid - remove 
         */
    return RCt;
}   /* WrapIt () */
