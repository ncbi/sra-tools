/*

  tools.common.log-proxy

  copyright: (C) 2015-2018 Rodarmer, Inc. All Rights Reserved

 */

#include "logging.hpp"
#include "log-protocol.hpp"
#include "debug.hpp"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

namespace cmn
{
#if ! _DEBUGGING

#define text_write( data, bytes ) \
    ( void ) 0

#else

    static
    void text_write ( const void * data, size_t bytes )
    {
        if ( dbg_dedicated_log >= 0 )
        {
            char buffer [ 4096 ];
            if ( bytes < sizeof buffer )
            {
                memcpy ( buffer, data, bytes );
                buffer [ bytes ] = '\n';
                :: write ( dbg_dedicated_log, buffer, bytes + 1 );
            }
            else
            {
                try
                {
                    char * temp_buffer = new char [ bytes + 1 ];
                    memcpy ( temp_buffer, data, bytes );
                    temp_buffer [ bytes ] = '\n';
                    :: write ( dbg_dedicated_log, temp_buffer, bytes + 1 );
                    delete [] temp_buffer;
                }
                catch ( ... )
                {
                    buffer [ 0 ] = '\n';
                    :: write ( dbg_dedicated_log, data, bytes );
                    :: write ( dbg_dedicated_log, buffer, 1 );
                }
            }
        }
    }
#endif

    static inline
    void log_write ( int log_fd, const void * data, size_t bytes )
    {
        :: write ( log_fd, data, bytes );
    }

    ProxyLogger :: ProxyLogger ()
        : StreamLogger ( 2 )
        , facility ( "user" )
    {
    }

    ProxyLogger :: ProxyLogger ( const std :: string & _facility )
        : StreamLogger ( 2 )
        , facility ( _facility )
    {
    }

    ProxyLogger :: ~ ProxyLogger ()
    {
    }

    void ProxyLogger :: resetProcessId ( pid_t pid )
    {
        LogPIDEvent evt;
        evt . event_code = ( uint8_t ) logPID;
        memcpy ( & evt . pid, & pid, sizeof evt . pid );

        // write
        TRACE ( TRACE_QA, "going to send %u bytes for logPID event\n", sizeof evt );
        log_write ( log_fd, & evt, sizeof evt );
    }

    void ProxyLogger :: init ( const char * hostname, const char * procname, pid_t pid )
    {
        TRACE ( TRACE_QA, "opening proxy logger session for ident '%s %s[%d]'\n"
                , hostname
                , procname
                , pid
            );

        // measure strings
        LogOpenEvent * evt = 0;
        size_t queue_bytes = facility . size ();
        size_t hostname_bytes = strlen ( hostname );
        size_t procname_bytes = strlen ( procname );
        size_t evt_size = evt -> size ( queue_bytes + hostname_bytes + procname_bytes );

        // allocate an event
        evt = ( LogOpenEvent * ) malloc ( evt_size );
        if ( evt == 0 )
            throw std :: bad_alloc ();

        // initialize
        evt -> event_code = ( uint8_t ) logOpen;
        assert ( queue_bytes < 256 );
        evt -> queue = ( uint8_t ) queue_bytes;
        assert ( hostname_bytes < 256 );
        evt -> hostname = ( uint8_t ) hostname_bytes;
        assert ( procname_bytes < 256 );
        evt -> procname = ( uint8_t ) procname_bytes;
        evt -> pid = pid;

        memcpy ( evt -> str, facility . data (), queue_bytes );
        memcpy ( & evt -> str [ queue_bytes ], hostname, hostname_bytes );
        memcpy ( & evt -> str [ queue_bytes + hostname_bytes ],
                 procname, procname_bytes );

        // write the event to log fd
        TRACE ( TRACE_QA, "going to send %lu bytes for logOpen event ( %u )\n"
                , ( unsigned long ) evt_size
                , evt -> event_code
            );
        log_write ( log_fd, evt, evt_size );

        // done with event
        free ( ( void * ) evt );
    }

    void ProxyLogger :: handleMsg ( int priority, const timespec & ts, const void * data, size_t bytes )
    {
        // haven't yet figured out how to handle priority
        // it either gets checked here as a filter
        // or checked on logger side, or both.
        assert ( priority >= 0 && priority < 8 );

        // crop to 64K - 1
        if ( bytes > 0xFFFF )
            bytes = 0xFFFF;

        // drop to dedicated log file
        text_write ( data, bytes );

        // measure
        LogMsgEvent * evt = 0;
        size_t evt_size = evt -> size ( bytes );

        // allocate
        evt = ( LogMsgEvent * ) malloc ( evt_size );
        if ( evt == 0 )
            throw std :: bad_alloc ();

        // initialize
        evt -> event_code = ( uint8_t ) logMsg;
        evt -> priority = ( uint8_t ) LOG_PRI ( priority );
        evt -> bytes = ( uint16_t ) bytes;
        evt -> ts_sec = ts . tv_sec;
        evt -> ts_nsec = ts . tv_nsec;
        memcpy ( evt -> msg, data, bytes );

        // write
        TRACE ( TRACE_QA, "going to send %lu bytes for logMsg event\n"
                , ( unsigned long ) evt_size
            );
        log_write ( log_fd, evt, evt_size );

        // done
        free ( ( void * ) evt );
    }
}
