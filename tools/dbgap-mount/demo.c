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

#include "demo.vers.h" /* VDB_PASSWD_VERS */

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

static
void
Mukashi ( const char * Path, const struct XFSTree * Tree )
{
    rc_t RCt;
    const struct XFSNode * Node;

    // printf ( "Mukashi Mukashi [%s]\n", Path );

    RCt = XFSTreeFindNode ( Tree, Path, & Node );
    if ( RCt == 0 ) {
        char BB [ 1024 ];

        printf ( "FOUND : [%s] [0x%p]\n", Path, ( void * ) Node );

        XFSNodeDescribe ( Node, BB, sizeof ( BB ) );
        printf ( "      : [0x%p][%s]\n", ( void * ) Node, BB );

        XFSNodeRelease ( Node );
    }
    else {
        printf ( "NOT FOUND : [%s] [0x%p]\n", Path, ( void * ) Node );
    }
}   /* Mukashi () */

#include <xfs/perm.h>
void
JitsuWa ( const char * String )
{
    const struct XFSPerm * Perm;
    char BB [ 100 ];

    if ( String == NULL ) {
        printf ( "ERROR : PERM MISSED\n" );
        return;
    }

    printf ( "PERM [%s]\n", String );

    if ( XFSPermMake ( String, & Perm ) == 0 ) {

        XFSPermToString ( Perm, BB, sizeof ( BB ) );

        printf ( "     [%s]\n", BB );

        XFSPermDispose ( Perm );
    }
    else {
        printf ( "ERROR : BAD PERM [%s]\n", String );
    }
}   /* JitsuWa () */

XFS_EXTERN rc_t CC XFS_InitAll_MHR ( const char * ConfigFile );
XFS_EXTERN rc_t CC XFS_DisposeAll_MHR ();

#include <xfs/path.h>
static
void
Naosu (const char * P)
{
    const struct XFSPath * Path;
    char B [ 1024 ];
    uint32_t llp, Q;

    XFSPathMake ( P, & Path );
    Q = XFSPathCount ( Path );

    printf ( "Path [%s] length %d \n", P, Q );

    for ( llp = 0; llp < Q; llp ++ ) {
        printf ( " %d : [%s]\n", llp, XFSPathGet ( Path, llp ) );

        XFSPathTo ( Path, llp, B, sizeof ( B ) );
        printf ( "    TO : [%s]\n", B );

        XFSPathFrom ( Path, llp, B, sizeof ( B ) );
        printf ( "    FR : [%s]\n", B );

    }

    XFSPathDispose ( Path );
}   /* Naosu () */

static 
rc_t 
KWrit ( const char * fname )
{
    rc_t RCt;
    struct KFile * File;
    struct KDirectory * Dir;
    size_t NWr;
    static const char * JJ = "leprocosm";

    printf ( "Writing [%s]\n", fname );


    RCt = 0;
    File = NULL;
    Dir = NULL;
    NWr = 0;

    RCt = KDirectoryNativeDir ( & Dir );
    if ( RCt == 0 ) {

        RCt = KDirectoryCreateFile ( Dir, & File, false, 0644, kcmCreate, fname );
        if ( RCt == 0 ) {
            RCt = KFileWrite ( File, 0, JJ, sizeof ( JJ ), & NWr );

            KFileRelease ( File );
        }

        KDirectoryRelease ( Dir );
    }

    printf ( "Writing [%s][R=%d]\n", fname, RCt );
    KOutMsg ( "Writing [%s][R=%R]\n", fname, RCt );

    return RCt;
}   /* KWrit () */

static
rc_t run (
        const char * MountPoint,
        const char * ConfigPoint
)
{
    rc_t RCt;
    struct XFSModel * TheModel;
    struct XFSTree * TheTree;
    struct XFSControl * TheControl;

/*
    Naosu ( "/g/kkk" );
    Naosu ( "/g/kkk/qqq" );
    return 0;
*/

/*  Testing permissions

    JitsuWa ( "rwxrw---x" );
    JitsuWa ( "---------" );
    JitsuWa ( "r-xr-xrwx user:group" );
    JitsuWa ( "r-xr-xrwx user:group:other" );
    JitsuWa ( "r-xr-xrwx user::other" );
    JitsuWa ( "r-xr-xrwx ::other" );

    if ( true ) return 1;

*/

    RCt = 0;

    OUTMSG ( ( "<<--- run()\n" ) );

    XFS_InitAll_MHR ( ConfigPoint );

/*
    RCt = XFSModelMake ( & TheModel, NULL, NULL );
*/
    RCt = XFSModelMake ( & TheModel, ConfigPoint, NULL );

/*
    XFSModelDDump ( TheModel );
*/

    printf ( "HA(XFSModelMake)[RC=%d]\n", RCt );

    if ( RCt == 0 ) {
        RCt = XFSTreeMake ( TheModel, & TheTree );

        printf ( "HA(XFSTreeMake)[RC=%d]\n", RCt );

/*
    Mukashi ( "/root", TheTree );
    Mukashi ( "/workspaces", TheTree );
    Mukashi ( "/workspaces/folder", TheTree );
    Mukashi ( "/workspaces/felder", TheTree );
    Mukashi ( "/workspaces/", TheTree );
    Mukashi ( "/", TheTree );
    Mukashi ( "/WORKSPACES", TheTree );
    Mukashi ( "/WORKSPACES/", TheTree );
    Mukashi ( "/WORKSPACES/FS", TheTree );
    Mukashi ( "/WORKSPACES/WS", TheTree );
*/


        if ( RCt == 0 ) {
            printf ( "HA(XFSControlMake)[RC=%d]\n", RCt );

            RCt = XFSControlMake ( TheTree, & TheControl );
            printf ( "HE(XFSControlMake)[0x%p][RC=%d]\n", ( void * ) TheControl, RCt );

            if ( RCt == 0 ) {

                XFSControlSetMountPoint ( TheControl, MountPoint );
                XFSControlSetLabel ( TheControl, "Olaffsen" );

                printf ( "HA(XFSStart)\n" );
                RCt = XFSStart ( TheControl );

                printf ( "HE(XFSStart)[RC=%d]\n", RCt );
                if ( RCt == 0 ) {

                    SLEPOY ( 10 );

// int u = 1 / 0; /* Kinda drop the transport */

//TT            SLEPOY ( 10 );
// KWrit ( "/home/iskhakov/PRODUCTION/MPoint/WORKSPACES/DIR/Loho.txt" );
// LOOP KWrit ( "/r/WORKSPACES/DIR/kkk/Loho.txt" );
// SLEPOY ( 10 );
// SLEPOY ( 50 );
SLEPOY ( 30 );
// SLEPOY ( 200 );

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

ver_t CC KAppVersion(void) { return DEMO_VERS; }

/*  Here I will temporarily parce arguments, and will attach
 *  toolkit ones later ... test program :)
 */
char Proga[333];
char MontP[333];
char ConfP[333];

#define MONTP_TAG   "-m"
#define CONFP_TAG   "-c"

static
void
RightUsage()
{
    printf("\ndbGaP mount tool demo program. Will mount and show content of cart files\n");
    printf("\nUsage: %s %s mount_point %s config_file\n\n\
Where:\n\
    mount_point  - point to mount\n\
    config_file - point to config\n\n\
\n\n", Proga, MONTP_TAG, CONFP_TAG);
}   /* RightUsage() */

static
bool
ParseArgs ( int argc, char ** argv )
{
    const char * PPU;
    int llp;

    MontP[0] = ConfP[0] = 0;

    PPU = strrchr ( * argv, '/' );
    if ( PPU == NULL ) {
        PPU = * argv;
    }
    else {
        PPU ++;
    }
    strcpy ( Proga, PPU );

    for ( llp = 1; llp < argc; llp ++ ) {
        const char *Arg = * ( argv + llp );

        if ( strcmp ( Arg, MONTP_TAG ) == 0 ) {
            if ( llp + 1 >= argc ) {
                printf ( "ERROR : value expected after '%s'\n", Arg );
                return false;
            }
            llp ++;

            strcpy ( MontP, * ( argv + llp ) );
            continue;
        }

        if ( strcmp ( Arg, CONFP_TAG ) == 0 ) {
            if ( llp + 1 >= argc ) {
                printf ( "ERROR : value expected after '%s'\n", Arg );
                return false;
            }
            llp ++;

            strcpy ( ConfP, * ( argv + llp ) );
            continue;
        }

        printf ( "ERROR : Invalid argument '%s'\n", Arg );
        return false;
    }

    if ( strlen ( MontP ) == 0 ) {
        printf ( "ERROR : Mount point is not defined\n" );
        return false;
    }

    if ( strlen ( ConfP ) == 0 ) {
        printf ( "ERROR : Config file is not defined\n" );
        return false;
    }

    return true;
}   /* ParseArgs() */


const char UsageDefaultName[] = "Henri Fuseli";
rc_t CC UsageSummary (const char* progname) { return 0; }
rc_t CC Usage ( const Args * args ) { return 0; }

rc_t CC KMain(int argc, char *argv[]) {


    if ( ! ParseArgs ( argc, argv ) ) {
        RightUsage();
        return 1;
    }

    return run ( MontP, ConfP );
}
