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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

namespace ncbi
{
    /*-------------------------------------------------------------------
     * Logger
     *  base class for log message processing engine
     */
    const size_t BSIZE = 256;

    void Logger :: init ( const char * ident )
    {
        // use a local buffer
        char hostname [ BSIZE ];

        // hostname
        int status = gethostname ( hostname, sizeof hostname );
        if ( status != 0 )
        {
            switch ( errno )
            {
            case ENAMETOOLONG:
                strncpy ( hostname, "<HOSTNAME-TOO-LONG>", sizeof hostname );
                break;
            default:
                strncpy ( hostname, "<GETHOSTNAME-FAILED>", sizeof hostname );
            }
            status = 0;
        }

        // process name
        const char * procname = "";
        if ( ident != 0 )
        {
            // find leaf name
            procname = strrchr ( ident, '/' );
            if ( procname ++ == 0 )
                procname = ident;
        }

        // initialize implementation
        init ( hostname, procname, getpid () );
    }

    int Logger :: setLogLevel ( int priority )
    {
        int prior = level;
        level = LOG_PRI ( priority );
        return prior;
    }

    void Logger :: msg ( int priority, const timespec & ts, const void * data, size_t bytes )
    {
        if ( LOG_PRI ( priority ) <= level )
            handleMsg ( priority, ts, data, bytes );
    }

    Logger :: Logger ()
#if _DEBUGGING
        : level ( LOG_DEBUG )
#else
        : level ( LOG_INFO )
#endif
    {
    }

    Logger :: ~ Logger ()
    {
        level = 0;
    }


    /*-------------------------------------------------------------------
     * StreamLogger
     *  a Logger that writes to a stream
     *  such as a file or socket or to a logging daemon
     */

    void StreamLogger :: resetLogFileDescriptor ( int _log_fd )
    {
        log_fd = _log_fd;
    }

    StreamLogger :: StreamLogger ()
        : log_fd ( 2 )
    {
    }

    StreamLogger :: StreamLogger ( int _log_fd )
        : log_fd ( _log_fd )
    {
    }

    StreamLogger :: ~ StreamLogger ()
    {
        log_fd = -1;
    }


    /*-------------------------------------------------------------------
     * LoggerScope
     *  an object that fits into C++ stack cleanup
     *  that can be used to use a Logger while it is in scope
     *  and restore the previous logger once it goes out of scope.
     *
     *  the most obvious use case is when switching from LocalLogger
     *  to DBLogger, all of which may fail in interesting ways.
     */

    LoggerScope :: LoggerScope ( Logger & _logger )
        : logger ( log . logger )
    {
        log . use ( _logger );
    }

    LoggerScope :: ~ LoggerScope ()
    {
        if ( logger != 0 )
        {
            log . use ( * logger );
            logger = 0;
        }
    }
}
