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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#include "logging.hpp"

#include <iostream>
#include <iomanip>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

namespace ncbi
{
    void LocalLogger :: resetProcessId ( pid_t pid )
    {
        if ( ! initialized )
            Logger :: init ( 0 );

        char pidbuf [ 64 ];
        int status = snprintf ( pidbuf, sizeof pidbuf, "[%d]: ", pid );
        assert ( status > 0 && ( size_t ) status < sizeof pidbuf );

        StringBuffer namebuf;
        namebuf += hostname;
        namebuf += ' ';
        namebuf += procname;
        namebuf += pidbuf;

        hostproc = namebuf . stealString ();
    }

    LocalLogger :: LocalLogger ()
        : StreamLogger ( 2 )
        , initialized ( false )
    {
    }

    LocalLogger :: ~ LocalLogger ()
    {
        initialized = false;
    }

    void LocalLogger :: init ( const char * _hostname, const char * _procname, pid_t pid )
    {
        char pidbuf [ 64 ];
        int status = snprintf ( pidbuf, sizeof pidbuf, "[%d]: ", pid );
        assert ( status > 0 && ( size_t ) status < sizeof pidbuf );

        hostname = String ( _hostname );
        procname = String ( _procname );

        StringBuffer namebuf;
        namebuf += hostname;
        namebuf += ' ';
        namebuf += procname;
        namebuf += pidbuf;

        hostproc = namebuf . stealString ();

        initialized = true;
    }

    void LocalLogger :: handleMsg ( int priority, const timespec & ts, const void * data, size_t bytes )
    {
        static const char * months [ 12 ] =
        {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };

        // timestamp
        time_t secs = ts . tv_sec;
        struct tm tm;
        localtime_r ( & secs, & tm );

        // hostname, process name & pid
        if ( ! initialized )
            Logger :: init ( 0 );

        // get month
        assert ( tm . tm_mon >= 0 && tm . tm_mon < 12 );

        // line buffer
        char buffer [ 4096 ];
        size_t marker = 0;

        // use proper way of building buffer
        int status = snprintf ( & buffer [ marker ], sizeof buffer - marker
                                , "%s %2d %02d:%02d:%02d "
                                , months [ tm . tm_mon ]
                                , tm . tm_mday
                                , tm . tm_hour
                                , tm . tm_min
                                , tm . tm_sec
            );
        assert ( status == 16 );
        marker += status;
        assert ( marker < sizeof buffer );

        status = snprintf ( & buffer [ marker ], sizeof buffer - marker
                            , "%.*s"
                            , ( int ) hostproc . size (), hostproc . data ()
            );
        assert ( status > 0 );
        marker += status;
        assert ( marker < sizeof buffer );

        // echo priority
        const char * priority_name = 0;
        switch ( LOG_PRI ( priority ) )
        {
        case LOG_EMERG:
            priority_name = "EMERG"; break;
        case LOG_ALERT:
            priority_name = "ALERT"; break;
        case LOG_CRIT:
            priority_name = "CRIT"; break;
        case LOG_ERR:
            priority_name = "ERROR"; break;
        case LOG_WARNING:
            priority_name = "WARNING"; break;
        case LOG_NOTICE:
            priority_name = "NOTICE"; break;
        case LOG_INFO:
            priority_name = "INFO"; break;
        case LOG_DEBUG:
            priority_name = "DEBUG"; break;
        default:
            status = snprintf ( & buffer [ marker ], sizeof buffer - marker,
                                "BAD_LEVEL(%u): ", LOG_PRI ( priority ) );
        }

        if ( priority_name != 0 )
            status = snprintf ( & buffer [ marker ], sizeof buffer - marker, "%s: ", priority_name );

        assert ( status > 0 );
        marker += status;
        assert ( marker < sizeof buffer );

        // just in case the message has trailing newlines
        const char * dp = ( const char * ) data;
        while ( bytes != 0 && dp [ bytes - 1 ] == '\n' )
            -- bytes;

        if ( marker + bytes < sizeof buffer )
        {
            memcpy ( & buffer [ marker ], data, bytes );
            marker += bytes;
            buffer [ marker ] = '\n';
            :: write ( log_fd, buffer, marker + 1 );
        }
        else
        {
            try
            {
                char * temp_buffer = new char [ marker + bytes + 1 ];
                memcpy ( temp_buffer, buffer, marker );
                memcpy ( & temp_buffer [ marker ], data, bytes );
                marker += bytes;
                temp_buffer [ marker ] = '\n';
                :: write ( log_fd, temp_buffer, marker + 1 );
                delete [] temp_buffer;
            }
            catch ( ... )
            {
                status = snprintf ( & buffer [ marker ], sizeof buffer - marker
                                    , "### LOG ERROR: OUT OF MEMORY ###\n" );
                assert ( status > 0 );
                marker += status;
                assert ( marker < sizeof buffer );
                :: write ( log_fd, buffer, marker );
            }
        }
    }
    
}
