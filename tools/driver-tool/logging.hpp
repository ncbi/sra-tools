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

#pragma once

#include <ncbi/secure/string.hpp>

#include "fmt.hpp"

#include <sys/syslog.h>
#include <time.h>

// syslog for some reason uses LOG_ERR ( not LOG_ERROR )
// but insists on full LOG_WARNING...
const int LOG_WARN = LOG_WARNING;

namespace ncbi
{

    /*-------------------------------------------------------------------
     * Logger
     *  base class for log message processing engine
     */
    class Logger
    {
    public:

        // initialize with executable path ( i.e. argv[0] )
        void init ( const char * exe_path );

        // manipulate levels that are active
        int setLogLevel ( int priority );

        // after a fork, reset pid
        virtual void resetProcessId ( pid_t pid ) = 0;

        // receive a message
        void msg ( int priority, const timespec & ts, const void * data, size_t bytes );

        Logger ();
        virtual ~ Logger ();

    protected:

        // initialize from hostname, procname, and pid
        virtual void init ( const char * hostname, const char * procname, pid_t pid ) = 0;

        // handle a message being sent
        virtual void handleMsg ( int priority, const timespec & ts, const void * data, size_t bytes ) = 0;

        int level;
    };


    /*-------------------------------------------------------------------
     * StreamLogger
     *  a Logger that writes to a stream
     *  such as a file or socket or to a logging daemon
     */
    class StreamLogger : public Logger
    {
    public:

        // if moving stderr fd to somewhere else
        void resetLogFileDescriptor ( int log_fd );

        StreamLogger ();
        StreamLogger ( int log_fd );
        virtual ~ StreamLogger ();

    protected:

        int log_fd;
    };


    /*-------------------------------------------------------------------
     * LocalLogger
     *  a Logger that writes simple text format
     *  to a local file or stream
     */
    class LocalLogger : public StreamLogger
    {
    public:

        inline void init ( const char * exe_path )
        { Logger :: init ( exe_path ); }
        virtual void resetProcessId ( pid_t pid );

        LocalLogger ();
        ~ LocalLogger ();

    protected:

        virtual void init ( const char * hostname, const char * procname, pid_t pid );
        virtual void handleMsg ( int priority, const timespec & ts, const void * data, size_t bytes );

        String hostproc;
        String hostname;
        String procname;
        bool initialized;
    };


    /*-------------------------------------------------------------------
     * ProxyLogger
     *  a Logger that serializes structured format
     *  to a logging daemon process across a socket
     */
    class ProxyLogger : public StreamLogger
    {
    public:

        inline void init ( const char * exe_path )
        { Logger :: init ( exe_path ); }
        virtual void resetProcessId ( pid_t pid );

        ProxyLogger ();
        ProxyLogger ( const String & facility );
        ~ ProxyLogger ();

    protected:

        virtual void init ( const char * hostname, const char * procname, pid_t pid );
        virtual void handleMsg ( int priority, const timespec & ts, const void * data, size_t bytes );

    private:

        String facility;
    };


    /*-------------------------------------------------------------------
     * DBLogger
     *  a Logger that writes structured format
     *  directly to a database
     */
#if 0
    class DBLogger : public Logger
    {
    public:

        inline void init ( const char * exe_path )
        { Logger :: init ( exe_path ); }
        virtual void resetProcessId ( pid_t pid );

        void connect ( const String & conn_info );
        void connect ( const std :: map < String, String > params,
                       bool expand_dbname = false );

        DBLogger ();
        DBLogger ( const String & facility );
        virtual ~ DBLogger ();

    protected:

        // initialize from hostname, procname, and pid
        virtual void init ( const char * hostname, const char * procname, pid_t pid );

        // handle a message being sent
        virtual void handleMsg ( int priority, const timespec & ts, const void * data, size_t bytes );

    private:

        void initQueue ();
        void initHostname ( const char * hostname );
        void initProcname ( const char * procname );

        String facility;
        PGConn * log_db;
        I32 queue_id;
        I32 hostname_id;
        I32 procname_id;
        pid_t pid;
    };
#endif


    /*-------------------------------------------------------------------
     * LoggerScope
     *  an object that fits into C++ stack cleanup
     *  that can be used to use a Logger while it is in scope
     *  and restore the previous logger once it goes out of scope.
     *
     *  the most obvious use case is when switching from LocalLogger
     *  to DBLogger, all of which may fail in interesting ways.
     */
    class LoggerScope
    {
    public:

        // capture current log.logger, use new logger
        LoggerScope ( Logger & logger );

        // restore old logger
        ~ LoggerScope ();

    private:

        Logger * logger;
    };


    /*-------------------------------------------------------------------
     * Log
     *  the front-end formatter for driving a Logger
     *  caller starts message via "msg()", writes to returned Fmt,
     *  and ends with a "Fmt::endm".
     *
     *  the process id may be reset as the result of a "fork()"
     *
     *  the logging engine may be reset to "use()" another Logger.
     */
    class Log : FmtWriter
    {
    public:

        // useful at times after forking
        void resetProcessId ( pid_t pid );

        // creates the start of a message
        // returns a Fmt so that the details can be sent
        Fmt & msg ( int priority );

        // connect to another Logger
        void use ( Logger & logger );

        Log ();
        Log ( Logger & logger );
        ~ Log ();

    private:

        // handle endm
        void write ( const char * data, size_t bytes );

        Fmt fmt;
        Logger * logger;
        timespec timestamp;
        int priority;

        friend class LoggerScope;
    };

    extern Log log;
}
