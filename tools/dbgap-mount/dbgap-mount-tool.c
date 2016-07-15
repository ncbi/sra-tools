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

#include <xfs/model.h>
#include <xfs/node.h>
#include <xfs/tree.h>
#include <xfs/xfs.h>

#include "dbgap-mount-tool.h"

#include <sysalloc.h>
#include <stdio.h>
#include <string.h>

/******************************************************************************/

/*)))
  \\\   Celebrity is here ...
  (((*/

XFS_EXTERN rc_t CC XFS_InitAll_MHR ( const char * ConfigFile );
XFS_EXTERN rc_t CC XFS_DisposeAll_MHR ();

static
rc_t CC
MakeModel (
            struct XFSModel ** Model,
            const char * ProjectId,
            bool ReadOnly
)
{
    rc_t RCt;
    struct XFSModel * Mod;
    struct XFSModelNode * ModNod;

    RCt = 0;
    Mod = NULL;

    RCt = XFSModelFromScratch ( & Mod, NULL );
    if ( RCt == 0 ) {
        RCt = XFSModelAddRootNode ( Mod, "gap-project" );
        if ( RCt == 0 ) {
            ModNod = ( struct XFSModelNode * ) XFSModelRootNode ( Mod );
            if ( ModNod == NULL ) {
                RCt = XFS_RC ( rcInvalid );
            }
            else {
                RCt = XFSModelNodeSetProperty (
                                        ModNod,
                                        XFS_MODEL_MODE,
                                        ( ReadOnly
                                                ? XFS_MODEL_MODE_RO
                                                : XFS_MODEL_MODE_RW
                                        )
                                        );
                if ( RCt == 0 ) {
                    RCt = XFSModelNodeSetProperty (
                                            ModNod,
                                            XFS_MODEL_PROJECTID,
                                            ProjectId
                                            );
                    if ( RCt == 0 ) {
                        * Model = Mod;
                    }
                }

            }
        }
    }

    return RCt;
}


static
rc_t CC
DoFukan (
        const char * ProjectId,
        const char * MountPoint,
        const char * LogFile,
        const char * ProgName,
        bool Daemonize,
        bool ReadOnly
)
{
    rc_t RCt;
    struct XFSModel * TheModel;
    struct XFSTree * TheTree;
    struct XFSControl * TheControl;
    char Lable [ 256 ];
    size_t NumWr;

    RCt = 0;
    TheModel = NULL;
    TheTree = NULL;
    TheControl = NULL;
    * Lable = 0;
    NumWr = 0;

    XFS_CAN ( ProjectId )
    XFS_CAN ( MountPoint )


        /*  Some messages good to say
         */
    LogMsg ( klogInfo, "Start" );
    pLogMsg ( klogInfo, "ProjectID: $(project)", "project=%s", ProjectId );
    pLogMsg ( klogInfo, "MountPoint: $(point)", "point=%s", MountPoint );
    if ( LogFile != NULL ) {
        pLogMsg ( klogInfo, "LogFile: $(file)", "file=%s", LogFile );
    }
    pLogMsg ( klogInfo, "ReadOnly: $(ro)", "ro=%s", ( ReadOnly ? "true" : "false" ) );
    pLogMsg ( klogInfo, "Daemonize: $(pokemon)", "pokemon=%s", ( Daemonize ? "true" : "false" ) );

        /*  Initializing all depots and heavy gunz
         */
    RCt = XFS_InitAll_MHR ( NULL );
    pLogMsg ( klogDebug, "[XFS_InitAll_MHR][$(rc)]", "rc=%d", RCt );
    if ( RCt == 0 ) {

        RCt = MakeModel ( & TheModel, ProjectId, ReadOnly );
        pLogMsg ( klogDebug, "[XFSModelMake][$(rc)]", "rc=%d", RCt );
        if ( RCt == 0 ) {

            RCt = XFSTreeMake ( TheModel, & TheTree );
            pLogMsg ( klogDebug, "[XFSTreeMake][$(rc)]", "rc=%d", RCt );
            if ( RCt == 0 ) {

                RCt = XFSControlMake ( TheTree, & TheControl );
                pLogMsg ( klogDebug, "[XFSControlMake][$(rc)]", "rc=%d", RCt );
                if ( RCt == 0 ) {

                    XFSControlSetMountPoint ( TheControl, MountPoint );

                    RCt = string_printf (
                                        Lable,
                                        sizeof ( Lable ) - 1,
                                        & NumWr,
                                        "dbGaP(%s)",
                                        ProjectId
                                        );
                    XFSControlSetLabel ( TheControl, Lable );
                    if ( LogFile != NULL ) {
                        XFSControlSetLogFile ( TheControl, LogFile );
                    }
                    if ( Daemonize ) {
                        XFSControlDaemonize ( TheControl );
                    }

                    LogMsg ( klogDebug, "[XFSStart]" );
                    RCt = XFSStart ( TheControl );
                    pLogMsg ( klogDebug, "[XFSStart][$(rc)]", "rc=%d", RCt );
                    if ( RCt == 0 ) {
                        LogMsg ( klogDebug, "[XFSStop]" );
                        RCt = XFSStop ( TheControl );
                        pLogMsg ( klogDebug, "[XFSStop][$(rc)]", "rc=%d", RCt );
                    }
                    else {
                        LogErr ( klogFatal, RCt, "CRITICAL ERROR: Can not start MOUNTER" );
                    }
                }

                XFSControlDispose ( TheControl );

                XFSTreeRelease ( TheTree );
            }

            XFSModelRelease ( TheModel );
        }

        XFS_DisposeAll_MHR ();

    }

        /*  Another message good to say
         */
    LogMsg ( klogDebug, "[Exiting]" );

    return RCt;
}   /* DoFukan () */

static
rc_t CC
CheckParameters (
            struct Args * TheArgs,
            const char ** ProjectId,
            const char ** MountPoint,
            bool * ReadOnly
)
{
    rc_t RCt;
    uint32_t ParamCount;
    uint32_t Idx;
    const char * ParamValue;

    RCt = 0;
    ParamCount = 0;
    Idx = 0;
    ParamValue = NULL;

    XFS_CSAN ( ProjectId )
    XFS_CSAN ( MountPoint )
    XFS_CSA ( ReadOnly, false )
    XFS_CAN ( TheArgs )
    XFS_CAN ( ProjectId )
    XFS_CAN ( MountPoint )
    XFS_CAN ( ReadOnly )

    RCt = ArgsParamCount ( TheArgs, & ParamCount );
    if ( RCt == 0 ) {
        if ( ParamCount != 2 && ParamCount != 3 ) {
            RCt = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcInsufficient );
        }
        else {
            if ( ParamCount == 3 ) {
                RCt = ArgsParamValue (
                                    TheArgs,
                                    Idx,
                                    ( const void ** ) & ParamValue
                                    );
                if ( RCt == 0 ) {
                    if ( strcmp ( PARAM_RO, ParamValue ) == 0 ) {
                        * ReadOnly = true;
                    }
                    else {
                        if ( strcmp ( PARAM_RW, ParamValue ) == 0 ) {
                            * ReadOnly = false;
                        }
                        else {
                            RCt = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcAmbiguous );
                        }
                    }
                }

                Idx ++;
            }

                /*  Next Param should be ProjectId
                 */
            if ( RCt == 0 ) {
                RCt = ArgsParamValue (
                                    TheArgs,
                                    Idx,
                                    ( const void ** ) & ParamValue
                                    );
                if ( RCt == 0 ) {
                    * ProjectId = ParamValue;
                }

                Idx ++;
            }

                /*  Next Param should be MountPoint
                 */
            if ( RCt == 0 ) {
                RCt = ArgsParamValue (
                                    TheArgs,
                                    Idx,
                                    ( const void ** ) & ParamValue
                                    );
                if ( RCt == 0 ) {
                    * MountPoint = ParamValue;
                }

                Idx ++;
            }
        }
    }

    return RCt;
}   /* CheckParameters () */

static
rc_t CC
CheckArgs (
        struct Args * TheArgs,
        const char ** LogFile,
        const char ** ProgName,
        bool * Daemonize
)
{
    rc_t RCt;
    const char * OptValue;
    uint32_t OptCount;

    RCt = 0;
    OptValue = NULL;
    OptCount = 0;

    XFS_CSAN ( LogFile )
    XFS_CSAN ( ProgName )
    XFS_CSA ( Daemonize, false )
    XFS_CAN ( TheArgs )
    XFS_CAN ( LogFile )
    XFS_CAN ( ProgName )
    XFS_CAN ( Daemonize )

    RCt = ArgsOptionCount ( TheArgs, OPT_LOGFILE, & OptCount );
    if ( RCt == 0 && OptCount == 1 ) {
        RCt = ArgsOptionValue (
                            TheArgs,
                            OPT_LOGFILE,
                            0,
                            ( const void ** ) & OptValue
                            );
        if ( RCt == 0 ) {
            * LogFile = OptValue;
        }
    }

    RCt = ArgsOptionCount ( TheArgs, OPT_DAEMONIZE, & OptCount ); 
    if ( RCt == 0 ) {
        * Daemonize = OptCount == 1;
    }

    RCt = ArgsProgram ( TheArgs, & OptValue, ProgName );

    return RCt;
}   /* CheckArgs () */

static
rc_t CC
RunApp ( struct Args * TheArgs )
{
    rc_t RCt;
    const char * ProjectId;
    const char * MountPoint;
    const char * LogFile;
    const char * ProgName;
    bool ReadOnly;
    bool Daemonize;

    RCt = 0;
    ProjectId = NULL;
    MountPoint = NULL;
    LogFile = NULL;
    ProgName = NULL;
    ReadOnly = false;
    Daemonize = false;

    XFS_CAN ( TheArgs )



        /*  First we are checking parameters
         */
    RCt = CheckParameters (
                        TheArgs,
                        & ProjectId,
                        & MountPoint,
                        & ReadOnly
                        );

    if ( RCt == 0 ) {
            /*  Second we are checking Arguments
             */
        RCt = CheckArgs ( TheArgs, & LogFile, & ProgName, & Daemonize );
    }

    if ( RCt != 0 ) {
        UsageSummary ( ProgName == NULL ? UsageDefaultName : ProgName );
    }
    else {
        RCt = DoFukan (
                    ProjectId,
                    MountPoint,
                    LogFile,
                    ProgName,
                    Daemonize,
                    ReadOnly
                    );
    }

    return RCt;
}   /* RunApp () */

static
rc_t CC
DoUnmount ( const char * MountPoint )
{
    rc_t RCt;

    RCt = 0;

    XFS_CAN ( MountPoint )

    pLogMsg ( klogDebug, "[DoUnmount] [$(point)]", "point=%s", MountPoint );

    XFSUnmountAndDestroy ( MountPoint );

    return RCt;
}   /* DoUnmount () */

/*)))
  \\\   KApp and Options ...
  (((*/

struct OptDef ToolOpts [] = {
    { OPT_DAEMONIZE, ALS_DAEMONIZE, NULL, UsgDaemonize, 1, false, false },
    { OPT_LOGFILE, ALS_LOGFILE, NULL, UsgLogFile, 1, true, false },
    { OPT_UNMOUNT, ALS_UNMOUNT, NULL, UsgUnmount, 1, true, false }
};  /* OptDef */

const char UsageDefaultName[] = "dbgap-mount-tool";

rc_t CC
UsageSummary ( const char * ProgName )
{
    return KOutMsg (
                    "\n"
                    "Usage:\n"
                    "  %s [options]"
                    " [%s|%s]"
                    " <project-id>"
                    " <mount-point>"
                    "\n"
                    "Or:\n"
                    "  %s [options] [unmount-options]"
                    "\n"
                    "\n",
                    ProgName,
                    PARAM_RO,
                    PARAM_RW,
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

    UsageSummary ( ProgName );

    KOutMsg ( "Options:\n" );

    HelpOptionLine (
                ALS_DAEMONIZE,
                OPT_DAEMONIZE,
                PRM_DAEMONIZE,
                UsgDaemonize
                );

    HelpOptionLine (
                ALS_LOGFILE,
                OPT_LOGFILE,
                PRM_LOGFILE,
                UsgLogFile
                );

    KOutMsg ( "\n" );

    KOutMsg ( "Unmount Options:\n" );

    HelpOptionLine (
                ALS_UNMOUNT,
                OPT_UNMOUNT,
                PRM_UNMOUNT,
                UsgUnmount
                );

    KOutMsg ( "\n" );
    KOutMsg ( "Standard Options:\n" );
    HelpOptionsStandard ();
    HelpVersion ( FullPath, KAppVersion () );

    return RCt;
}   /* Usage () */

rc_t CC
KMain ( int ArgC, char * ArgV [] )
{
    rc_t RCt;
    struct Args * TheArgs;
    const char * MountPoint;

    RCt = 0;
    TheArgs = NULL;

    RCt = ArgsMakeAndHandle (
                           & TheArgs,
                           ArgC,
                           ArgV,
                           1,
                           ToolOpts,
                           sizeof ( ToolOpts ) / sizeof ( OptDef )
                           );
    if ( RCt == 0 ) {
            /*  First we do check if that is unmount command 
             */
        RCt = ArgsOptionValue (
                            TheArgs,
                            OPT_UNMOUNT,
                            0,
                            ( const void ** ) & MountPoint
                            );
        if ( RCt != 0 ) {
            RCt = WrapIt ( TheArgs, RunApp );
        }
        else {
            RCt = DoUnmount ( MountPoint );
        }

        ArgsWhack ( TheArgs );
    }

    return RCt;
}
