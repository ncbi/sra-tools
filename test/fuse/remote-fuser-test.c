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

#include "remote-fuser-test.vers.h" /* VDB_PASSWD_VERS */

#include <sysalloc.h>
#include <kapp/main.h> /* KMain */

#include <klib/text.h>
#include <klib/rc.h>

#include <kproc/lock.h>
#include <kproc/thread.h>

#include <kfs/directory.h>
#include <kfs/file.h>

// #include "mutli.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h> 

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*  Single File Test
 */
struct _SFTest {
    const char * path;
    uint64_t size;
};

static
rc_t CC
_SFTestDispose ( const struct _SFTest * self )
{
    struct _SFTest * Test = ( struct _SFTest * ) self;

    if ( Test != NULL ) {
        if ( Test -> path != NULL ) {
            free ( ( char * ) Test -> path );

            Test -> path = NULL;
        }

        Test -> size = 0;

        free ( Test );
    }

    return 0;
}   /* _SFTestDispose () */

static
rc_t CC
_SFTestMake ( const struct _SFTest ** Test, const char * Path )
{
    rc_t RCt;
    struct _SFTest * Ret;
    struct KDirectory * Dir;
    uint32_t PathType;
    uint64_t Size;

    RCt = 0;
    Ret = NULL;
    Dir = NULL;
    PathType = kptNotFound;
    Size = 0;

    if ( Test != NULL ) {
        * Test = NULL;
    }

    if ( Test == NULL || Path == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

        /* First we shoud check that file exist
         */
    RCt = KDirectoryNativeDir ( & Dir );
    if ( RCt == 0 ) {
        PathType = KDirectoryPathType ( Dir, Path );
        if ( PathType == kptFile ) {
            RCt = KDirectoryFileSize ( Dir, & Size, Path );
            if ( Size == 0 ) {
                return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
            }
        }
        else {
            return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
        }
        KDirectoryRelease ( Dir );
    }

    if ( RCt == 0 ) {
        Ret = calloc ( 1, sizeof ( struct _SFTest ) );
        if ( Ret == NULL ) {
            RCt = RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
        }
        else {
            Ret -> path = string_dup_measure ( Path, NULL );
            if ( Ret -> path == NULL ) {
                RCt = RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
            }
            else {
                Ret -> size = Size;

                * Test = Ret;
            }
        }
    }

    if ( RCt != 0 ) {
        * Test = NULL;

        _SFTestDispose ( Ret );
    }

    return RCt;
}   /* _SFTestMake () */

/*  I am pushed to write that test fast, so nothing special. Just
 *  reading 32M blocks at random offset
 */
static
rc_t CC
_SFTestNewJob (
                struct _SFTest * self,
                int Iteration,
                const char ** Name,
                uint64_t * Offset,
                size_t * Size
)
{
    rc_t RCt;
    size_t BS;
    uint64_t Oko;

    RCt = 0;
    BS = 33554432;
    Oko = 0;

    if ( Name != NULL ) {
        * Name = NULL;
    }

    if ( Offset != NULL ) {
        * Offset = 0;
    }

    if ( Size != NULL ) {
        * Size = 0;
    }

    if ( self == NULL || Name == NULL || Offset == NULL || Size == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    * Name = self -> path;

    if ( self -> size < BS ) {
        * Offset = 0;
        * Size = self -> size;
    }
    else {
        Oko = ( uint64_t ) ( self -> size / BS );
        * Offset = ( rand () % Oko ) * BS;
        * Size = BS;

        if ( self -> size <= * Offset + BS ) {
            * Size = self -> size - * Offset;
        }
    }

    return RCt;
}   /* _SFTestNewJob () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*  Multiply Files Test
 */
struct _MFTest {
    const struct _SFTest ** files;
    size_t files_qty;
};

static
rc_t CC
_MFTestDispose ( const struct _MFTest * self )
{
    size_t llp;
    struct _MFTest * Test;

    llp = 0;
    Test = ( struct _MFTest * ) self;

    if ( Test != NULL ) {
        if ( Test -> files != NULL ) {
            for ( llp = 0; llp < Test -> files_qty; llp ++ ) {
                _SFTestDispose ( self -> files [ llp ] );
            }

            free ( Test -> files );
        }

        Test -> files = NULL;
        Test -> files_qty = 0;

        free ( Test );
    }

    return 0;
}   /* _MFTestDispose () */

static rc_t CC _MFTestLoadEntries (
                                struct _MFTest * self,
                                const char * Path
                                );

static
rc_t CC
_MFTestMake ( const struct _MFTest ** Test, const char * Path )
{
    rc_t RCt;
    struct _MFTest * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Test != NULL ) {
        * Test = NULL;
    }

    if ( Test == NULL || Path == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( struct _MFTest ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
    }
    else {
        RCt = _MFTestLoadEntries ( Ret, Path );
        if ( RCt == 0 ) {
            * Test = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Test = NULL;

        _MFTestDispose ( Ret );
    }

    return RCt;
}   /* _MFTestMake () */

static
rc_t CC
_ReadAll ( const char * Path, char ** Buf, size_t * BufSize )
{
    rc_t RCt;
    const struct KFile * File;
    struct KDirectory * Dir;
    char * RetBuf;
    size_t Size, NumRead;

    RCt = 0;
    File = NULL;
    Dir = NULL;
    RetBuf = NULL;
    Size = 0;
    NumRead = 0;

    RCt = KDirectoryNativeDir ( & Dir );
    if ( RCt == 0 ) {
        RCt = KDirectoryFileSize ( Dir, & Size, Path );
        if ( RCt == 0 ) {
            if ( Size == 0 ) {
                return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
            }
            else {
                RetBuf = calloc ( Size, sizeof ( char ) );
                if ( RetBuf == NULL ) {
                    return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
                }
                else {
                    RCt = KDirectoryOpenFileRead ( Dir, & File, Path );
                    if ( RCt == 0 ) {
                        RCt = KFileReadAll (
                                            File,
                                            0,
                                            RetBuf,
                                            Size,
                                            & NumRead
                                            );
                        if ( RCt == 0 ) {
                            * Buf = RetBuf;
                            * BufSize = Size;
                        }
                    }
                }
            }
        }

        KDirectoryRelease ( Dir );
    }

    if ( RCt != 0 ) {
        Buf = NULL;
        BufSize = 0;

        if ( RetBuf != NULL ) {
            free ( RetBuf );
        }
    }

    return RCt;
}   /* _ReadAll () */

static
rc_t CC
_PaarseEntries ( struct _MFTest * self, char * Buf, size_t BufSize )
{
    rc_t RCt;
    size_t NumLines;
    const struct _SFTest ** Tests;
    char * pS, * pE, * pC;
    size_t llp;
    const struct _SFTest * Test;

    RCt = 0;
    NumLines = 0;
    Tests = 0;
    pS = pE = pC = NULL;
    llp = 0;
    Test = NULL;

    if ( self == NULL || Buf == NULL || BufSize == 0 ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

        /*  Fist we are going through buffer trying to define amount
         *  of lines
         */
    pS = pC = Buf;
    pE = pS + BufSize;

    while ( pC < pE ) {

        if ( * pC == '\n' ) {
            * pC = 0;
            NumLines ++;
            pS = pC + 1;
        }

        pC ++;
    }

    if ( pS < pC ) {
        * pC = 0;       /* Ha-ha, will write zero beyond boundary */
        NumLines ++;
    }

    if ( NumLines == 0 ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
    }

    Tests = calloc ( NumLines, sizeof ( struct _SFTest * ) );
    if ( Tests == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
    }

        /* Here we are doing another pass to fill a foo
         */
    pS = pC = Buf;
    pE = pS + BufSize;
    llp = 0;
    Test = 0;

    while ( pC < pE ) {

        if ( * pC == 0 ) {
            RCt = _SFTestMake ( & Test, pS );
            if ( RCt != 0 ) {
                break;
            }

            Tests [ llp ] = Test;

            llp ++;
            pS = pC + 1;
        }

        pC ++;
    }

    if ( RCt == 0 ) {
        if ( pS < pC ) {
            RCt = _SFTestMake ( & Test, pS );
            if ( RCt == 0 ) {
                Tests [ llp ] = Test;
            } 
        }
    }

    if ( RCt == 0 ) {
        self -> files = Tests;
        self -> files_qty = NumLines;
    }
    else {
        self -> files = NULL;
        self -> files_qty = 0;

        if ( Tests != NULL ) {
            for ( llp = 0; llp < NumLines; llp ++ ) {
                _SFTestDispose ( Tests [ llp ] );
            }
        }
    }

    return RCt;
}   /* _PaarseEntries () */

rc_t CC
_MFTestLoadEntries ( struct _MFTest * self, const char * Path )
{
    rc_t RCt;
    char * Buf;
    size_t BufSize;

    RCt = 0;
    Buf = NULL;
    BufSize = 0;

    if ( self == NULL || Path == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    RCt = _ReadAll ( Path, & Buf, & BufSize );
    if ( RCt == 0 ) {
        RCt = _PaarseEntries ( self, Buf, BufSize );

        free ( Buf );
    }

    return RCt;
}   /* _MFTestLoadEntries () */

/*  I am pushed to write that test fast, so nothing special. Just
 *  reading 32M blocks at random offset from random files
 */
static
rc_t CC
_MFTestNewJob (
                struct _MFTest * self,
                int Iteration,
                const char ** Name,
                uint64_t * Offset,
                size_t * Size
)
{
    rc_t RCt;
    size_t BS;
    uint64_t Oko;
    const struct _SFTest * Test;

    RCt = 0;
    BS = 33554432;
    Oko = 0;
    Test = NULL;

    if ( Name != NULL ) {
        * Name = NULL;
    }

    if ( Offset != NULL ) {
        * Offset = 0;
    }

    if ( Size != NULL ) {
        * Size = 0;
    }

    if ( self == NULL || Name == NULL || Offset == NULL || Size == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

        /*  First we are going to choose righ path
         */
    Test = self -> files [ rand () % self -> files_qty ];

    * Name = Test -> path;

    if ( Test -> size < BS ) {
        * Offset = 0;
        * Size = Test -> size;
    }
    else {
        Oko = ( uint64_t ) ( Test -> size / BS );
        * Offset = ( rand () % Oko ) * BS;
        * Size = BS;

        if ( Test -> size <= * Offset + BS ) {
            * Size = Test -> size - * Offset;
        }
    }

    return RCt;
}   /* _MFTestNewJob () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

static
rc_t run_many_files ( int NumThr, int TumeRun, const char * Path )
{
    rc_t RCt;
    const struct _MFTest * Test;
    const struct XTasker * Tasker;

    RCt = 0;
    Test = NULL;
    Tasker = NULL;

    RCt = _MFTestMake ( & Test, Path );
    if ( RCt == 0 ) {

        RCt = XTaskerMake (
                        & Tasker,
                        NumThr,
                        ( void * ) Test,
                        ( Job4Task ) _MFTestNewJob 
                        );
        if ( RCt == 0 ) {

            printf (
                    "Run test in [%d] threads on [%lu] files\n",
                    NumThr,
                    Test -> files_qty
                    );

            XTaskerRun ( Tasker, TumeRun * 60 );

            XTaskerDispose ( Tasker );
        }

        _MFTestDispose ( Test );
    }

    return RCt;
}   /* run_many_files () */

static
rc_t run_single_file ( int NumThr, int TumeRun, const char * Path )
{
    rc_t RCt;
    const struct _SFTest * Test;
    const struct XTasker * Tasker;

    RCt = 0;
    Test = NULL;
    Tasker = NULL;

    RCt = _SFTestMake ( & Test, Path );
    if ( RCt == 0 ) {

        RCt = XTaskerMake (
                        & Tasker,
                        NumThr,
                        ( void * ) Test,
                        ( Job4Task ) _SFTestNewJob 
                        );
        if ( RCt == 0 ) {
            printf (
                    "Run test in [%d] threads on [single] file\n",
                    NumThr
                    );

            XTaskerRun ( Tasker, TumeRun * 60 );

            XTaskerDispose ( Tasker );
        }

        _SFTestDispose ( Test );
    }

    return RCt;
}   /* run_single_file () */

/******************************************************************************/

static
rc_t run ( int NumThr, int TumeRun, const char * Path, bool IsList )
{
    rc_t RCt = 0;

    if ( IsList ) {
        /* Here we are reading list */
        RCt = run_many_files ( NumThr, TumeRun, Path );
    }
    else {
        RCt = run_single_file ( NumThr, TumeRun, Path );
        /* This is a single file case */
    }


    return RCt;
}

ver_t CC KAppVersion(void) { return REMOTE_FUSER_TEST_VERS; }

/*  Here I will temporarily parce arguments, and will attach
 *  toolkit ones later ... test program :)
 */
#define MAX_PATH_SIZE 1234

char ProgramName [ MAX_PATH_SIZE ];

const static char * NumThrTag = "-t";
const static char * NumThrDsc = "num_threads";
#define NUM_THR_DEF 16
static int NumThr = NUM_THR_DEF;

static char * RunTimeTag = "-r";
static char * RunTimeDsc = "run_time";
#define RUN_TIME_DEF 180
static int RunTime = RUN_TIME_DEF;

static char * ListTag    = "-l";
bool          PathIsList = false;

char FilePath [ MAX_PATH_SIZE ];

static
void
RightUsage()
{
    printf ( "\nThat program will test 'remote-fuser' application. It implements two types\n" );
    printf ( "of test : multiply access to single file and multiply acces to many files\n" );
    printf ( "\n" );
    printf ( "Ussage: %s [ %s %s ] [ %s %s ] [%s] path\n\n",
                            ProgramName, NumThrTag, NumThrDsc,
                            RunTimeTag, RunTimeDsc, ListTag
                            );
    printf ( "Where:\n\n" );
    printf ( "%s - num threads to run, optional, default %d\n", NumThrDsc, NUM_THR_DEF );
    printf ( "   %s - time minutes to run, optional, default %d\n", RunTimeDsc, RUN_TIME_DEF );
    printf ( "         %s - flag that show that path is a list file for multiply access test\n", ListTag );
    printf ( "       path - path to file ( single or list )\n" );
    printf ( "\n" );
}   /* RightUsage() */

static
bool
ParseArgs ( int argc, char ** argv )
{
    const char * PPU;
    int llp;

    PPU = NULL;
    llp = 0;

    PPU = strrchr ( * argv, '/' );
    if ( PPU == NULL ) {
        PPU = * argv;
    }
    strcpy ( ProgramName, PPU );

    * FilePath = 0;

    for ( llp = 1; llp < argc; llp ++ ) {

        PPU = argv [ llp ];
        if ( * PPU == '-' ) {

            if ( strcmp ( NumThrTag, PPU ) == 0 ) {
                if ( argc - 1 <= llp ) {
                    printf ( "ERROR : parameter [%s] requests an argument\n", NumThrTag );
                    return false;
                }

                llp ++;
                if ( ( NumThr = atol ( argv [ llp ] ) ) == 0 ) {
                    printf ( "ERROR : invalid parameter [%s] value [%s]\n", NumThrTag, argv [ llp ] );
                }

                continue;
            }

            if ( strcmp ( RunTimeTag, PPU ) == 0 ) {
                if ( argc - 1 <= llp ) {
                    printf ( "ERROR : parameter [%s] requests an argument\n", RunTimeTag );
                    return false;
                }

                llp ++;
                if ( ( RunTime = atol ( argv [ llp ] ) ) == 0 ) {
                    printf ( "ERROR : invalid parameter [%s] value [%s]\n", RunTimeTag, argv [ llp ] );
                }

                continue;
            }

            if ( strcmp ( ListTag, PPU ) == 0 ) {
                PathIsList = true;

                continue;
            }

            printf ( "ERROR : unknown parameter [%s]\n", PPU );
            return false;
        }
        else {
            strcpy ( FilePath, PPU );
            break;
        }
    }

    if ( * FilePath == 0 ) {
        printf ( "ERROR : no path passed\n" );
        return false;
    }

    if ( 48 <= NumThr ) {
        printf ( "ERROR : invalide amount of threads passed [%d], should be less than 48\n", NumThr );
        return false;
    }

    if ( RunTime < 2 ) {
        printf ( "ERROR : invalide amount of run time passed [%d], should be greater than 2\n", RunTime );
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

    if ( signal ( SIGINT, XTaskerSigIntHandler ) == SIG_ERR ) {
        printf ( "Can not instal signal handlers\n" );
        return RC ( rcExe, rcFile, rcReading, rcParam, rcInvalid );
    }

    return run ( NumThr, RunTime, FilePath, PathIsList );
}
