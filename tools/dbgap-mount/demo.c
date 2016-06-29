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

#include <kfs/directory.h>
#include <kfs/file.h>

#include <xfs/model.h>
#include <xfs/node.h>
#include <xfs/tree.h>
#include <xfs/xfs.h>

#include <stdio.h>
#include <string.h>

#ifdef WINDOWS

#include <windows.h>    /* Sleep () */

#else

#include <unistd.h>     /* sleep () */

#endif /* WINDOWS */

/******************************************************************************/


#ifdef JOJOBA

static
void
SLEPOY ( int Sec )
{

printf ( "Sleeping %d seconds\n", Sec );
#ifdef WINDOWS
    Sleep ( Sec * 1000 );
#else
    sleep ( Sec );
#endif

printf ( "    DONE [ Sleeping %d seconds ]\n", Sec );

}

#endif /* JOJOBA */

XFS_EXTERN rc_t CC XFS_InitAll_MHR ( const char * ConfigFile );
XFS_EXTERN rc_t CC XFS_DisposeAll_MHR ();

static
rc_t 
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
rc_t run (
        const char * ProjectId,
        const char * MountPoint,
        bool ReadOnly,
        bool Daemonize
)
{
    rc_t RCt;
    struct XFSModel * TheModel;
    struct XFSTree * TheTree;
    struct XFSControl * TheControl;

    RCt = 0;
    TheModel = NULL;
    TheTree = NULL;
    TheControl = NULL;

    OUTMSG ( ( "<<--- run()\n" ) );

    XFS_InitAll_MHR ( NULL );

    RCt = MakeModel ( & TheModel, ProjectId, ReadOnly );

    printf ( "HA(XFSModelMake)[RC=%d]\n", RCt );

    if ( RCt == 0 ) {
        RCt = XFSTreeMake ( TheModel, & TheTree );

        printf ( "HA(XFSTreeMake)[RC=%d]\n", RCt );

        if ( RCt == 0 ) {
            printf ( "HA(XFSControlMake)[RC=%d]\n", RCt );

            RCt = XFSControlMake ( TheTree, & TheControl );
            printf ( "HE(XFSControlMake)[0x%p][RC=%d]\n", ( void * ) TheControl, RCt );

            if ( RCt == 0 ) {

                XFSControlSetMountPoint ( TheControl, MountPoint );
                XFSControlSetLabel ( TheControl, "Olaffsen" );
                XFSControlSetLogFile ( TheControl, NULL );

                if ( ! Daemonize ) {
                    XFSControlSetArg ( TheControl, "-f", "-f" );
                }

                printf ( "HA(XFSStart)\n" );
                RCt = XFSStart ( TheControl );

                printf ( "HE(XFSStart)[RC=%d]\n", RCt );
                if ( RCt == 0 ) {
                    printf ( "HA(XFSStop)\n" );
                    RCt = XFSStop ( TheControl );
                    printf ( "HE(XFSStop)[RC=%d]\n", RCt );
                }
                else {
                    printf ( "Can not start FUSE\n" );
                }
            }

            XFSControlDispose ( TheControl );

            XFSTreeRelease ( TheTree );
        }

        XFSModelRelease ( TheModel );
    }

    XFS_DisposeAll_MHR ();

    OUTMSG ( ( "--->> run()\n" ) );

    return RCt;
}

/*  Here I will temporarily parce arguments, and will attach
 *  toolkit ones later ... test program :)
 */
char ProgramName[333];
char ProjectId [33];
int ProjectIdInt = 0;
char MountPoint[333];
bool ReadOnly = true;
bool Daemonize = false;
bool LogToFile = false;
char LogFile [ 777 ];

#define RO_TAG   "ro"
#define RW_TAG   "rw"
#define DM_TAG   "-d"

#define LF_TAG   "-l"

static
void
RightUsage()
{
    printf("\ndbGaP mount tool demo program. Will mount and show content of cart files\n");
    printf("\nUsage: %s [%s|%s] [%s log_file] [%s] project_id mount_point\n\n\
Where:\n\
    project_id - usually integer greater that zero and less than twelve\n\
    %s - mount in read only mode\n\
    %s - mount in read-write mode\n\
    %s - run mounter as daemon\n\
    mount_point - point to mount\n\
    log_file - file to log logs\n\
\n\n", ProgramName, RO_TAG, RW_TAG, LF_TAG, DM_TAG, RO_TAG, RW_TAG, DM_TAG );
}   /* RightUsage() */

static
bool
ParseArgs ( int argc, char ** argv )
{
    const char * PPU;
    const char * Arg;
    int llp;

    Arg = NULL;
    llp = 0;
    * ProgramName = 0;
    * ProjectId = 0;
    ProjectIdInt = 0;
    * MountPoint = 0;

    ReadOnly = true;
    Daemonize = false;

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

        /* Herer shouldr ber atr leastr oner argumentr - projectr idr
         */
    if ( argc <= 2 ) {
        printf ( "ERROR : too few arguments\n" );
        return false;
    }

    llp = 1;
    Arg = * ( argv + llp );

        /* firstr paramr couldr ber "ro|rw" orr "-d"
         */
    if ( strcmp ( Arg, RO_TAG ) == 0 ) {
        ReadOnly = true;

        llp ++;
    }
    else {
        ReadOnly = false;
        if ( strcmp ( Arg, RW_TAG ) == 0 ) {

            llp ++;
        }
    }

        /* secondr paramr "-d" orr projectr idr
         */
    if ( argc <= llp ) {
        printf ( "ERROR : too few arguments\n" );
        return false;
    }
    Arg = * ( argv + llp );
    if ( strcmp ( Arg, DM_TAG ) == 0 ) {
        Daemonize = true;

        llp ++;
    }

        /* andr nowr itr isr projectr idr
         */
    if ( argc <= llp ) {
        printf ( "ERROR : too few arguments\n" );
        return false;
    }
    Arg = * ( argv + llp );
    strcpy ( ProjectId, Arg );
        /* checkr thatr integerr
         */
    ProjectIdInt = atol ( ProjectId );
    if ( ProjectIdInt <= 0 ) {
        printf ( "ERROR : invalid project_id '%s'\n", ProjectId );
        return false;
    }
    llp ++;


        /* mountr pointr ifr existsr
         */
    if ( llp < argc ) {
        Arg = * ( argv + llp );
        strcpy ( MountPoint, Arg );
        llp ++;
    }

    printf ( "PrI [%d]\n", ProjectIdInt );
    printf ( "MnP [%s]\n", MountPoint );


    return true;
}   /* ParseArgs() */


const char UsageDefaultName[] = "Henri Fuseli";
rc_t CC UsageSummary (const char* progname) { return 0; }
rc_t CC Usage ( const Args * args ) { return 0; }

rc_t CC KMain(int argc, char *argv[]) {

    // KLogLevelSet ( klogInfo );
    KLogLevelSet ( klogDebug );
    // XFSLogInit ( "log.log" );

    if ( ! ParseArgs ( argc, argv ) ) {
        RightUsage();
        return 1;
    }

    return run (
                ProjectId,
                MountPoint,
                ReadOnly,
                Daemonize
                );
}
