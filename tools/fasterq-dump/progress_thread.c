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
#include "progress_thread.h"

/* 
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */

#ifndef _h_progressbar_
#include <klib/progressbar.h>
#endif

#ifndef _h_atomic_
#include <atomic.h>
#endif

#ifndef _h_atomic64_
#include <atomic64.h>
#endif

#ifndef _h_kproc_thread_
#include <kproc/thread.h>
#endif

#ifndef _h_klib_time_
#include <klib/time.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#include "helper.h"

typedef struct bg_progress
{
    KThread * thread;
    struct progressbar * progressbar;
    atomic_t done;
    atomic64_t value;
    atomic64_t max_value;
    uint32_t sleep_time;
    uint32_t digits;
    uint32_t cur;
} bg_progress;

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

static void bg_progress_steps( bg_progress * self )
{
    uint64_t max_value = atomic64_read( &self -> max_value );
    uint64_t value = atomic64_read( &self -> value );
    uint32_t percent = calc_percent( max_value, value, self -> digits );
    if ( percent > self -> cur )
    {
        uint32_t i;
        for ( i = self -> cur + 1; i <= percent; ++i )
            update_progressbar( self -> progressbar, i );
        self -> cur = percent;
    }
}

static rc_t CC bg_progress_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    bg_progress * bgp = data;
    if ( bgp != NULL )
    {
        rc = make_progressbar_stderr( & bgp -> progressbar, bgp -> digits );
        if ( rc == 0 )
        {
            bgp -> cur = 0;
            update_progressbar( bgp -> progressbar, bgp -> cur );
            while ( atomic_read( &bgp -> done ) == 0 )
            {
                bg_progress_steps( bgp );
                KSleepMs( bgp -> sleep_time );
            }
            
            bg_progress_steps( bgp );
            destroy_progressbar( bgp -> progressbar );
        }
    }
    return rc;
}

rc_t bg_progress_make( bg_progress ** bgp, uint64_t max_value, uint32_t sleep_time, uint32_t digits )
{
    rc_t rc = 0;
    bg_progress * p = calloc( 1, sizeof *p );
    if ( p == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        atomic64_set( &p -> max_value, max_value );
        p -> sleep_time = sleep_time == 0 ? 200 : sleep_time;
        p -> digits = digits == 0 ? 2 : digits;
        rc = helper_make_thread( & p -> thread, bg_progress_thread_func, p, THREAD_DFLT_STACK_SIZE );
        if ( 0 != rc )
        {
            ErrMsg( "progress_thread.c helper_make_thread( bg-progress-thread ) -> %R", rc );
            free( p );
        }
        else
            *bgp = p;
    }
    return rc;
}

void bg_progress_update( bg_progress * self, uint64_t by )
{
    if ( self != NULL )
        atomic64_read_and_add( &self -> value, by );
}

void bg_progress_inc( bg_progress * self )
{
    if ( self != NULL )
        atomic64_inc( &self -> value );
}

void bg_progress_set_max( bg_progress * self, uint64_t value )
{
    if ( self != NULL )
        atomic64_set( &self -> max_value, value );
}

void bg_progress_get( bg_progress * self, uint64_t * value )
{
    if ( self != NULL && value != NULL )
    {
        *value = atomic64_read( &self -> value );
    }
}

void bg_progress_release( bg_progress * self )
{
    if ( self != NULL )
    {
        atomic_set( &self -> done, 1 );
        KThreadWait( self -> thread, NULL );
        KThreadRelease( self -> thread );
        free( ( void * ) self );
    }
}


typedef struct bg_update
{
    KThread * thread;
    atomic_t done;
    atomic_t active;    
    atomic64_t value;
    uint64_t prev_value;
    size_t digits_printed;
    uint32_t sleep_time;    
} bg_update;


static rc_t CC bg_update_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    bg_update * bga = data;
    if ( bga != NULL )
    {
        /* wait to be activated */
        while ( atomic_read( &bga -> active ) == 0 )
        {
            KSleepMs( bga -> sleep_time );
        }
        /* loop until we are done */
        while ( rc == 0 && atomic_read( &bga -> done ) == 0 )
        {
            uint64_t value = atomic64_read( &bga -> value );
            if ( value > 0 && value != bga -> prev_value )
            {
                char buffer[ 80 ];
                size_t num_writ;
                uint32_t i;                
                for ( i = 0; i < ( bga -> digits_printed ); i++ )
                    buffer[ i ] = '\b';

                rc = string_printf( &buffer[ bga -> digits_printed ],
                                    ( sizeof buffer )- ( bga -> digits_printed ),
                                    &num_writ,
                                    "%lu",
                                    value );
                if ( rc == 0 )
                {
                    KOutHandlerSetStdErr();
                    rc = KOutMsg( buffer );
                    KOutHandlerSetStdOut();
                    bga -> digits_printed = num_writ;
                }
                bga -> prev_value = value;
            }
            KSleepMs( bga -> sleep_time );
        }
        if ( rc == 0 )
        {
            KOutHandlerSetStdErr();
            rc = KOutMsg( "\n" );
            KOutHandlerSetStdOut();
        }
    }
    return rc;
}

rc_t bg_update_make( bg_update ** bga, uint32_t sleep_time )
{
    rc_t rc = 0;
    bg_update * p = calloc( 1, sizeof *p );
    if ( p == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        p -> sleep_time = sleep_time == 0 ? 200 : sleep_time;
        rc = helper_make_thread( & p -> thread, bg_update_thread_func, p, THREAD_DFLT_STACK_SIZE );
        if ( 0 != rc )
        {
            ErrMsg( "progress_thread.c helper_make_thread( bg-update-thread ) -> %R", rc );
            free( p );
        }
        else
            *bga = p;
    }
    return rc;
}

void bg_update_start( bg_update * self, const char * caption )
{
    if ( self != NULL )
    {
        rc_t rc = 0;
        if ( caption != NULL )
        {
            KOutHandlerSetStdErr();
            rc = KOutMsg( caption );
            KOutHandlerSetStdOut();
        }
        if ( rc == 0 )
            atomic_set( &self -> active, 1 );
    }
}

void bg_update_update( bg_update * self, uint64_t by )
{
    if ( self != NULL )
    {
        if ( atomic_read( &self -> active ) > 0 )
            atomic64_read_and_add( &self -> value, by );
    }
}

void bg_update_release( bg_update * self )
{
    if ( self != NULL )
    {
        atomic_set( &self -> done, 1 );
        KThreadWait( self -> thread, NULL );
        KThreadRelease( self -> thread );
        free( ( void * ) self );
    }
}
