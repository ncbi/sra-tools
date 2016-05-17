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

#ifndef _utils_h_
#define _utils_h_

#include <xfs/xfs-defs.h>

#ifdef __cplusplus 
extern "C" {
#endif /* __cplusplus */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/**
 ** Lyrics:
 ** That file contains stupid things
 **
 **/

/*) Fowards, and others
 (*/

/*) Something about time im MicroSeconds
 (*/
uint64_t CC XTmNow ( );

/*) Statistics module
 (*/
struct XStats;

rc_t CC XStatsMake ( const struct XStats ** Stats );
rc_t CC XStatsDispose ( const struct XStats * self );

rc_t CC XStatsReset ( const struct XStats * self );

rc_t CC XStatsGood (
                    const struct XStats * self,
                    uint64_t Size,
                    uint64_t Time
                    );

rc_t CC XStatsBad ( const struct XStats * self );

rc_t CC XStatsReport ( const struct XStats * self ); 

/*) Tasks
 (*/
struct XTasker;

    /*  Supposed to return NULL Name and 0 RC if there are no
     *  tasks anymore
     */
typedef rc_t ( CC * Job4Task ) (
                            void * Data,
                            int Iteration,
                            const char ** Name,
                            uint64_t * Offset,
                            size_t * Size
                            );

rc_t CC XTaskerMake (
                    const struct XTasker ** Tasker,
                    size_t NumThreads,
                    void * Data,
                    Job4Task Jobber
                    );
rc_t CC XTaskerDispose ( const struct XTasker * self );

rc_t CC XTaskerRun ( const struct XTasker * self, size_t Seconds );
bool CC XTaskerIsRunning ( const struct XTasker * self );

    /* We suppose if FileName returned as NULL, there are no new jobs
     */
rc_t CC XTaskerNewJob (
                    const struct XTasker * self,
                    const char ** FileName,
                    uint64_t * Offset,
                    size_t * Size
                    );

rc_t CC XTaskerSetDone ( const struct XTasker * self );
bool CC XTaskerIsDoneSet ( const struct XTasker * self );
bool CC XTaskerIsDone ( const struct XTasker * self );

rc_t CC XTaskerWait ( const struct XTasker * self );

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _utils_h_ */
