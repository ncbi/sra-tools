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
#include "progress.h"

/* 
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */

#ifndef _h_atomic_
#include <atomic.h>
#endif

#ifndef _h_atomic64_
#include <atomic64.h>
#endif

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_progressbar_
#include <klib/progressbar.h>
#endif

#ifndef _h_kproc_thread_
#include <kproc/thread.h>
#endif

#ifndef _h_klib_time_
#include <klib/time.h>
#endif

typedef enum proc_state { ps_idle, ps_init, ps_running, ps_stop, ps_term } t_proc_state;

typedef struct progress
{
    struct progressbar * progressbar; /* from klib/progressbar.h */
    uint32_t sleep_time;
    uint32_t cur;
    
    atomic64_t value;
    atomic64_t max_value;
    atomic_t digits;
    atomic_t state;
} progress;


static uint32_t calc_percent( uint64_t max, uint64_t value, uint16_t digits )
{
    uint64_t res = value;
    switch ( digits )
    {
        case 1 : res *= 1000; break;
        case 2 : res *= 10000; break;
        default : res *= 100; break;
    }
    if ( max > 0 )
        res /= max;
    return ( uint32_t )res;
}

static bool progress_steps( progress * self )
{
    bool res = ( self -> progressbar != NULL );
    if ( res )
    {
        uint32_t percent = calc_percent( atomic64_read( &self -> max_value ),
                                         atomic64_read( &self -> value ),
                                         atomic_read( &self -> digits ) );
        if ( percent > self -> cur )
        {
            uint32_t i;
            for ( i = self -> cur + 1; i <= percent; ++i )
                update_progressbar( self -> progressbar, i );
            self -> cur = percent;
        }
    }
    return res;
}

static void progress_steps_and_destroy( progress * self )
{
    if ( progress_steps( self ) )
    {
        destroy_progressbar( self -> progressbar );
        self -> progressbar = NULL;
    }
    atomic_set( &self -> state, ps_idle );
}

static rc_t CC progress_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    progress * prog = data;
    if ( prog != NULL )
    {
        bool running = true;
        while ( running )
        {
            switch( atomic_read( &prog -> state ) )
            {
                case ps_idle    :   break;

                case ps_init    :   progress_steps_and_destroy( prog );
                                    if ( 0 == make_progressbar( & prog -> progressbar, atomic_read( &prog -> digits ) ) )
                                        atomic_set( &prog -> state, ps_running );
                                    break;
                                    
                case ps_running :   if ( !progress_steps( prog ) )
                                        atomic_set( &prog -> state, ps_idle );
                                    break;

                case ps_stop    :   progress_steps_and_destroy( prog );
                                    break;

                case ps_term    :   progress_steps_and_destroy( prog );
                                    running = false;
                                    break;
            }
            
            if ( running )
                KSleepMs( prog -> sleep_time );
        }
        free( ( void * ) data );
    }
    return rc;
}

rc_t make_progress( struct thread_runner * threads, progress ** prog, uint32_t sleep_time )
{
    rc_t rc = 0;
    progress * p = calloc( 1, sizeof * p );
    if ( p == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_progress().calloc( %d ) -> %R", ( sizeof * p ), rc );
    }
    else
    {
        p -> sleep_time = sleep_time;
        atomic64_set( &p -> value, 0 );
        atomic64_set( &p -> max_value, 0 );
        atomic_set( &p -> digits, 0 );
        atomic_set( &p -> state, ps_idle );
        
        rc = thread_runner_add( threads, progress_thread_func, p );
        if ( rc != 0 )
            free( ( void * ) p );
        else
            *prog = p;
    }
    return rc;
}

void stop_progress( progress * self )
{
    if ( self != NULL )
        atomic_set( &self -> state, ps_stop );
}

void terminate_progress( progress * self )
{
    if ( self != NULL )
        atomic_set( &self -> state, ps_term );
}

void start_progress( progress * self, uint32_t digits, uint64_t max_value )
{
    if ( self != NULL )
    {
        atomic64_set( &self -> value, 0 );
        atomic64_set( &self -> max_value, max_value );
        atomic_set( &self -> digits, digits );
        atomic_set( &self -> state, ps_init );
    }
}

void update_progress( progress * self, uint64_t by )
{
    if ( self != NULL )
        atomic64_read_and_add( &self -> value, by );
}
