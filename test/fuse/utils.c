/*===========================================================================
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

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kproc/lock.h>
#include <kproc/thread.h>

#include <atomic32.h>

#include "utils.h"

#include <sysalloc.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

 /*))))
   |||| XStats implementation
   ((((*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*))
 || XTmXXXX stuff
((*/

/*)))
 ///  Mesuring time in ... microzecoidz from :
 \\\     # date -d @1380000000
 ///     Tue Sep 24 01:20:00 EDT 2013
(((*/
const uint64_t _DateFrom = 1380000000;
const uint64_t _DateCoeff = 1000000;

uint64_t CC
XTmNow ()
{
    struct timeval TV;
    uint64_t RetVal;

    gettimeofday ( & TV, NULL );
    RetVal = ( ( TV.tv_sec - _DateFrom ) * _DateCoeff ) + TV.tv_usec;


    return RetVal;
}   /* XTmNow () */

/*))
 || XStats struct
((*/
struct XStats {
    struct KLock * mutabor;

    uint64_t start_time;    /* Time of starting stat */

    uint64_t qty;           /* all Reads Qty */
    uint64_t err_qty;       /* bad Reads Qty */
    uint64_t time;          /* Time of Reads cumulative */
    uint64_t size;          /* all Reads Qty */
};

static rc_t CC _XStatsReset_NoLock ( const struct XStats * self );

rc_t CC
XStatsDispose ( const struct XStats * self )
{
    struct XStats * Stats = ( struct XStats * ) self;

    if ( Stats != NULL ) {
        if ( Stats -> mutabor != NULL ) {
            KLockRelease ( Stats -> mutabor );
            Stats -> mutabor = NULL;
        }

        _XStatsReset_NoLock ( self );

        free ( Stats );
    }

    return 0;
}   /* XStatDispose () */

rc_t CC
XStatsMake ( const struct XStats ** Stats )
{
    rc_t RCt;
    struct XStats * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Stats != NULL ) {
        * Stats = NULL;
    }

    Ret = calloc ( 1, sizeof ( struct XStats ) );
    if ( Ret == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
    }
    else {
        RCt = KLockMake ( & ( Ret -> mutabor ) );
        if ( RCt == 0 ) {
            RCt = XStatsReset ( Ret );
            if ( RCt == 0 ) {
                * Stats = ( const struct XStats * ) Ret;
            }
        }
    }

    if ( RCt != 0 ) {
        * Stats = NULL;

        if ( Ret != NULL ) {
            XStatsDispose ( Ret );
        }
    }

    return RCt;
}   /* XStatMake () */

static
rc_t CC
_XStatsReset_NoLock ( const struct XStats * self )
{
    struct XStats * Stats = ( struct XStats * ) self;

    if ( Stats != NULL ) {
        Stats -> start_time = XTmNow ();

        Stats -> qty = 0;
        Stats -> err_qty = 0;
        Stats -> time = 0;
        Stats -> size = 0;
    }

    return 0;
}   /* _XStatsReset_NoLock () */

rc_t CC
XStatsReset ( const struct XStats * self )
{
    rc_t RCt = 0;

    if ( self != NULL ) {
        RCt = KLockAcquire ( self -> mutabor );
        if ( RCt == 0 ) {
            RCt = _XStatsReset_NoLock ( self );
            KLockUnlock ( self -> mutabor );
        }
    }

    return RCt;
}   /* XStatReset () */

rc_t CC
XStatsGood ( const struct XStats * self, uint64_t Size, uint64_t Time )
{
    rc_t RCt;
    struct XStats * Stats;

    RCt = 0;
    Stats = ( struct XStats * ) self;

    if ( Stats != NULL ) {
        RCt = KLockAcquire ( Stats -> mutabor );
        if ( RCt == 0 ) {
            Stats -> qty ++;
            Stats -> time += Time;
            Stats -> size += Size;

            KLockUnlock ( Stats -> mutabor );
        }
    }

    return RCt;
}   /* XStatDispose () */

rc_t CC
XStatsBad ( const struct XStats * self )
{
    rc_t RCt;
    struct XStats * Stats;

    RCt = 0;
    Stats = ( struct XStats * ) self;

    if ( Stats != NULL ) {
        RCt = KLockAcquire ( Stats -> mutabor );
        if ( RCt == 0 ) {
            Stats -> err_qty ++;

            KLockUnlock ( Stats -> mutabor );
        }
    }

    return RCt;
}   /* XStatDispose () */

rc_t CC
XStatsReport ( const struct XStats * self )
{
    rc_t RCt;
    uint64_t Per;
    uint64_t Tim;

    RCt = 0;
    Per = 0;
    Tim = 0;

    if ( self != NULL ) {
        RCt = KLockAcquire ( self -> mutabor );
        if ( RCt == 0 ) {
            printf ( "<<== Read Stats\n" );
            printf ( "  READ QTY : %lu\n", self -> qty );
            Per = self -> qty == 0 ? 0 : ( ( int ) ( ( ( float ) self -> err_qty * 100.0f ) / ( float ) ( self -> qty ) ) );
            printf ( "    ERRORS : %lu [%lu%%]\n", self -> err_qty, Per );
            Tim = ( XTmNow () - self -> start_time ) / 1000000;
            printf ( "      TIME : %lu sec\n", Tim );
            printf ( "  ACC TIME : %lu sec\n", self -> time / 1000000 );
            printf ( " READ SIZE : %lu but\n", self -> size );

            Per = self -> time == 0 ? 0 : ( self -> size / Tim ) ;
            printf ( " THOUGHOUT : %lu bit per sec\n", Per );

            KLockUnlock ( self -> mutabor );
        }
    }

    return RCt;
}   /* XStatDispose () */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct XTasker;

struct XTask {
    const struct XTasker * tasker;

    struct KThread * thread;

    size_t task_no;

    size_t buffer_size;
    char * buffer;
};

struct XTasker {
    size_t task_qty;
    const struct XTask ** tasks;

    atomic32_t iteration;
    atomic32_t is_run;
    atomic32_t is_done;

    uint64_t stop_at;

    const struct XStats * stat;

    void * job_4_task_data;
    Job4Task job_4_task;
};

rc_t CC _TaskStop ( const struct XTask * self );

static
rc_t CC
_TaskDispose ( const struct XTask * self )
{
    struct XTask * Task = ( struct XTask * ) self;

    if ( Task != NULL ) {
        if ( Task -> thread != NULL ) {
            _TaskStop ( Task );
        }

        Task -> task_no = 0;

        Task -> buffer_size = 0;
        if ( Task -> buffer != NULL ) {
            free ( Task -> buffer );
            Task -> buffer = NULL;
        }

        Task -> tasker = NULL;
    }

    return 0;
}   /* _TaskDispose () */

static
rc_t CC
_TaskMake (
        const struct XTask ** Task,
        const struct XTasker * Tasker,
        size_t TaskNo
)
{
    rc_t RCt;
    struct XTask * Ret;

    RCt = 0;
    Ret = NULL;

    if ( * Task != NULL ) {
        * Task = NULL;
    }

    if ( Tasker == NULL || Task == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( struct XTask ) );
    if ( Ret == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
    }
    else {
        Ret -> tasker = Tasker;
        Ret -> task_no = TaskNo;

        * Task = ( const struct XTask * ) Ret;
    }

    if ( RCt != 0 ) {
        * Task = NULL;

        if ( Ret != NULL ) {
            _TaskDispose ( Ret );
        }
    }

    return RCt;
}   /* _TaskMake () */

static
rc_t CC
_TaskRealloc ( const struct XTask * self, size_t BufferSize )
{
    struct XTask * Task = ( struct XTask * ) self;

    if ( Task -> buffer_size < BufferSize ) {
        if ( Task -> buffer != 0 ) {
            free ( Task -> buffer );

            Task -> buffer = NULL;
            Task -> buffer_size = 0;
        }

        Task -> buffer_size = ( ( BufferSize / 1024 ) + 1 ) * 1024;
        Task -> buffer = calloc (
                                Task -> buffer_size,
                                sizeof ( char )
                                );
        if ( Task -> buffer == NULL ) {
            return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
        }
    }

    return 0;
}   /* _TaskRealloc () */


static
rc_t CC
_TaskRead ( const struct XTask * self )
{
    rc_t RCt;
    const char * Name;
    uint64_t Offset;
    size_t Size, NumRead;
    struct KDirectory * Directory;
    const struct KFile * File;
    uint64_t Time;

    RCt = 0;
    Name = NULL;
    Offset = 0;
    Size = NumRead = 0;
    Directory = NULL;
    File = NULL;
    Time = 0;

    if ( self == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    if ( self -> tasker == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
    }

    RCt = XTaskerNewJob ( self -> tasker, & Name, & Offset, & Size );
    if ( RCt == 0 ) {
        if ( Name != NULL ) {
            RCt = _TaskRealloc ( self, Size );
            if ( RCt == 0 ) {
                RCt = KDirectoryNativeDir ( & Directory );
                if ( RCt == 0 ) {
                    RCt = KDirectoryOpenFileRead (
                                                Directory,
                                                & File,
                                                Name
                                                );
                    if ( RCt == 0 ) {
                        Time = XTmNow ();

                        RCt = KFileReadAll (
                                            File,
                                            Offset,
                                            self -> buffer,
                                            Size,
                                            & NumRead
                                            );
                        if ( RCt == 0 ) {
                            XStatsGood (
                                        self -> tasker -> stat,
                                        NumRead,
                                        ( XTmNow () - Time )
                                        );
                        }
                        else {
                            XStatsBad ( self -> tasker -> stat );
                        }

                        KFileRelease ( File );
                    }
                    KDirectoryRelease ( Directory );
                }
            }
        }
    }

    return RCt;
}   /* _TaskRead () */

static
bool CC
_TaskDone ( const struct XTask * self )
{
    if ( self != NULL ) {
        return XTaskerIsDoneSet ( self -> tasker );
    }

    return true;
}   /* _TaskDone () */

static
rc_t CC
_TaskThreadProc ( const struct KThread * Thread, void * Data )
{
    rc_t RCt;
    struct XTask * Task;

    RCt = 0;
    Task = ( struct XTask * ) Data;

    if ( Thread == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    if ( Data == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    while ( true ) {
        /* We are ignoring the return code, cuz there is statistic */
        /* RCt = */
        _TaskRead ( Task );

        if ( _TaskDone ( Task ) ) {
            break;
        }
    }

    atomic32_dec ( ( atomic32_t * ) & ( Task -> tasker -> is_run ) );

    return RCt;
}   /* _TaskThreadProc () */

static
rc_t CC
_TaskRun ( const struct XTask * self )
{
    struct XTask * Task = ( struct XTask * ) self;

    if ( self == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    if ( self -> tasker == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
    }

    if ( self -> thread != NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
    }

    atomic32_read_and_add (
                ( atomic32_t * ) & ( Task -> tasker -> is_run ), 1
                );

    return KThreadMake ( & ( Task -> thread ), _TaskThreadProc, Task );
}   /* _TaskRun () */

rc_t CC
_TaskStop ( const struct XTask * self )
{
    rc_t RCt;
    struct XTask * Task;

    RCt = 0;
    Task = ( struct XTask * ) self;

    if ( self == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    KThreadCancel ( Task -> thread );
    KThreadWait ( Task -> thread, NULL );
    KThreadRelease ( Task -> thread );

    Task -> thread = NULL;

    return RCt;
}   /* TaskStop () */

rc_t CC
XTaskerDispose ( const struct XTasker * self )
{
    struct XTasker * Tasker;
    size_t llp;

    Tasker = ( struct XTasker * ) self;
    llp = 0;

    if ( Tasker != NULL ) {
        if ( Tasker -> stat != NULL ) {
            XStatsDispose ( Tasker -> stat );
            Tasker -> stat = NULL;
        }

        if ( Tasker -> tasks != NULL ) {
            for ( llp = 0; llp < Tasker -> task_qty; llp ++ ) {
                if ( Tasker -> tasks [ llp ] != NULL ) {
                    _TaskDispose ( Tasker -> tasks [ llp ] );
                    Tasker -> tasks [ llp ] = NULL;
                }
            }
        }
        Tasker -> tasks = NULL;
        Tasker -> task_qty = 0;

        atomic32_set ( & ( Tasker -> iteration ), 0 );
        atomic32_set ( & ( Tasker -> is_run ), 0 );
        atomic32_set ( & ( Tasker -> is_done ), 0 );

        Tasker -> stop_at = 0;

        Tasker -> job_4_task_data = NULL;
        Tasker -> job_4_task = NULL;

        free ( Tasker );
    }

    return 0;
}   /* XTaskerDispose () */

/* JOJOBA here will be variable part */
rc_t CC
XTaskerMake (
            const struct XTasker ** Tasker,
            size_t NumThreads,
            void * Data,
            Job4Task Jobber
)
{
    rc_t RCt;
    struct XTasker * Ret;
    size_t llp;

    RCt = 0;
    Ret = NULL;
    llp = 0;

    if ( Tasker != NULL ) {
        * Tasker = NULL;
    }

    if ( Tasker == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    if ( Jobber == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    if ( NumThreads == 0 ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
    }

    Ret = calloc ( 1, sizeof ( struct XTasker ) );
    if ( Ret == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
    }
    else {
        Ret -> tasks = calloc ( NumThreads, sizeof ( struct XTask * ) );
        if ( Ret -> tasks == NULL ) {
            RCt = RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcExhausted );
        }
        else {
            if ( RCt == 0 ) {
                Ret -> task_qty = NumThreads;
                for ( llp = 0; llp < NumThreads; llp ++ ) {
                    RCt = _TaskMake ( Ret -> tasks + llp, Ret, llp );
                    if ( RCt != 0 ) {
                        break;
                    }
                }
                if ( RCt == 0 ) {
                    RCt = XStatsMake ( & ( Ret -> stat ) );
                    if ( RCt == 0 ) { 
                        atomic32_set ( & ( Ret -> iteration ), 0 );
                        atomic32_set ( & ( Ret -> is_run ), 0 );
                        atomic32_set ( & ( Ret -> is_done ), 0 );
                        Ret -> stop_at = 0;
                        Ret -> job_4_task_data = Data;
                        Ret -> job_4_task = Jobber;

                        * Tasker = Ret;
                    }
                }
            }
        }
    }

    if ( RCt != 0 ) {
        * Tasker = NULL;

        if ( Ret != NULL ) {
            XTaskerDispose ( Ret );
        }
    }

    return RCt;
}   /* XTaskerMake () */

rc_t CC
XTaskerRun ( const struct XTasker * self, size_t Seconds )
{
    rc_t RCt;
    size_t llp;

    RCt = 0;
    llp = 0;

    if ( self == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    if ( self -> tasks == NULL || self -> task_qty == 0 ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcInvalid );
    }

    ( ( struct XTasker * ) self ) -> stop_at =
                                    XTmNow () + ( Seconds * 1000000 );

    for ( llp = 0; llp < self -> task_qty; llp ++ ) {
        if ( self -> tasks [ llp ] != NULL ) {
            RCt = _TaskRun ( self -> tasks [ llp ] );
        }

        if ( RCt != 0 ) {
            break;
        }
    }

    RCt = XTaskerWait ( self );

    return RCt;
}   /* XTaskerRun () */

bool CC
XTaskerIsRunning ( const struct XTasker * self )
{
    if ( self != NULL ) {
        return atomic32_read ( & ( self -> is_run ) ) != 0;
    }

    return false;
}   /* XTaskerIsRunning () */

rc_t CC
XTaskerNewJob (
            const struct XTasker * self,
            const char ** FileName,
            uint64_t * Offset,
            size_t * Size
)
{
    int Iteration = 0;

    if ( FileName != NULL ) {
        * FileName = NULL;
    }

    if ( Offset != NULL ) {
        * Offset = 0;
    }

    if ( Size != NULL ) {
        * Size = 0;
    }

    if ( self == NULL || FileName == NULL || Offset == NULL || Size == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    Iteration = atomic32_read_and_add (
                            ( atomic32_t * ) & ( self -> iteration ),
                            1
                            );

    if ( XTaskerIsDoneSet ( self ) ) {
        return 0;
    }

    return self -> job_4_task (
                            self -> job_4_task_data,
                            Iteration,
                            FileName,
                            Offset,
                            Size
                            );
}   /* XTaskerNewJob () */

rc_t CC
XTaskerSetDone ( const struct XTasker * self )
{
    if ( self == NULL ) {
        return RC ( rcExe, rcNoTarg, rcProcessing, rcParam, rcNull );
    }

    atomic32_set ( ( atomic32_t * ) & ( self -> is_done ), 1 );

    return 0;
}   /* XTaskerSetDone () */

bool CC
XTaskerIsDoneSet ( const struct XTasker * self )
{
    if ( self != NULL ) {
        return atomic32_read ( & ( self -> is_done ) ) != 0;
    }

    return false;
}   /* XTaskerIsDoneSet () */

bool CC
XTaskerIsDone ( const struct XTasker * self )
{
    return XTaskerIsDoneSet ( self ) && ! XTaskerIsRunning ( self );
}   /* XTaskerIsDone () */

rc_t CC
XTaskerWait ( const struct XTasker * self )
{
    uint64_t Next = 0; 
    uint64_t Now = 0; 
    size_t llp = 0;
    uint64_t Pardon = 600 * 1000000;
    struct timespec t_spec;
    struct timespec t_rem;

    t_spec . tv_sec = 0;
    t_spec . tv_nsec = 100000;

    t_rem . tv_sec = 0;
    t_rem . tv_nsec = 0;

    if ( XTaskerIsRunning ( self ) ) {
        Next = XTmNow () + Pardon;

        while ( true ) {
            nanosleep ( & t_spec, & t_rem );

            Now = XTmNow ();

            if ( self -> stop_at < Now ) {
                if ( ! XTaskerIsDoneSet ( self ) ) {
                    XTaskerSetDone ( self );
                }
            }

            if ( XTaskerIsDone ( self ) ) {
                break;
            }

            if ( Next < Now ) {
                Next += Pardon;

                XStatsReport ( self -> stat );
            }
        }

        XStatsReport ( self -> stat );

        for ( llp = 0; llp < self -> task_qty; llp ++ ) {
            if ( self -> tasks [ llp ] != NULL ) {
                _TaskStop ( self -> tasks [ llp ] );
            }
        }
    }

    return 0;
}   /* XTaskerWait () */
