/*

  tools.common.log-db

  copyright: (C) 2014-2017 Rodarmer, Inc. All Rights Reserved

 */

#include "logging.hpp"
#include "except.hpp"
#include "text.hpp"
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

    void DBLogger :: resetProcessId ( pid_t _pid )
    {
        pid = _pid;
    }

    void DBLogger :: connect ( const std :: string & conn_info )
    {
        if ( log_db != 0 )
        {
            throw Exception ( rc_logic_err, "already connected to logdb '%s'",
                              log_db -> db () . c_str () );
        }

        log_db = new PGConn ( conn_info );
    }

    void DBLogger :: connect ( const std :: map < std :: string,
                               std :: string > params, bool expand_dbname )
    {
        if ( log_db != 0 )
        {
            throw Exception ( rc_logic_err, "already connected to logdb '%s'",
                              log_db -> db () . c_str () );
        }

        log_db = new PGConn ( params, expand_dbname );
    }


    DBLogger :: DBLogger ()
        : facility ( "user" )
        , log_db ( 0 )
        , queue_id ( -1 )
        , hostname_id ( -1 )
        , procname_id ( -1 )
        , pid ( -1 )
    {
    }

    DBLogger :: DBLogger ( const std :: string & _facility )
        : facility ( _facility )
        , log_db ( 0 )
        , queue_id ( -1 )
        , hostname_id ( -1 )
        , procname_id ( -1 )
        , pid ( -1 )
    {
    }

    DBLogger :: ~ DBLogger ()
    {
        delete log_db;
        log_db = 0;
        queue_id = hostname_id = procname_id = -1;
        pid = -1;
    }

    void DBLogger :: init ( const char * hostname, const char * procname, pid_t _pid )
    {
        TRACE ( TRACE_QA, "opening proxy logger session for ident '%s %s[%d]'\n"
                , hostname
                , procname
                , _pid
            );

        initQueue ();
        initHostname ( hostname );
        initProcname ( procname );
        pid = _pid;
    }

    void DBLogger :: initQueue ()
    {
        if ( log_db == 0 )
            throw Exception ( rc_logic_err, "unable to initialize queue - not connected to logdb" );

        std :: string fname = log_db -> escapeLiteral ( facility );

        TRACE ( TRACE_PRG, "getting id for facility \"%s\"\n", fname . c_str () );

        // create query
        log_db -> query ()
            << "select makeQueue ( cast ( "
            << fname
            << " as varchar ) );"
            << endm
            ;

        TRACE ( TRACE_PRG, "executing query: '%s'\n", log_db -> sql () . c_str () );
        PGResult rslt = log_db -> exec ();
        TRACE ( TRACE_PRG, "result status message: '%s'\n", rslt . errorMessage () . c_str () );
        if ( rslt . status () != rslt . tuples_ok )
            throw Exception ( rc_runtime_err, rslt . errorMessage () );

        // extract connection id
        assert ( rslt . numRows () == 1 );
        assert ( rslt . numColumns () == 1 );
        if ( ! rslt . nextRow () )
            throw Exception ( rc_logic_err, "no rows available" );

        I64 id = strtol ( rslt . cellData ( 1 ) );
        TRACE ( TRACE_PRG, "returned queue-id: '%lld'\n", ( long long ) id );
        assert ( id >= 0 );
        assert ( id <= INT32_MAX );
        queue_id = ( I32 ) id;
    }

    void DBLogger :: initHostname ( const char * hostname )
    {
        if ( log_db == 0 )
            throw Exception ( rc_logic_err, "unable to initialize hostname - not connected to logdb" );

        std :: string hname = log_db -> escapeLiteral ( hostname );

        TRACE ( TRACE_PRG, "getting id for hostname \"%s\"\n", hname . c_str () );

        // create query
        log_db -> query ()
            << "select makeHostname ( cast ( "
            << hname
            << " as varchar ) );"
            << endm
            ;

        TRACE ( TRACE_PRG, "executing query: '%s'\n", log_db -> sql () . c_str () );
        PGResult rslt = log_db -> exec ();
        TRACE ( TRACE_PRG, "result status message: '%s'\n", rslt . errorMessage () . c_str () );
        if ( rslt . status () != rslt . tuples_ok )
            throw Exception ( rc_runtime_err, rslt . errorMessage () );

        // extract connection id
        assert ( rslt . numRows () == 1 );
        assert ( rslt . numColumns () == 1 );
        if ( ! rslt . nextRow () )
            throw Exception ( rc_logic_err, "no rows available" );

        I64 id = strtol ( rslt . cellData ( 1 ) );
        TRACE ( TRACE_PRG, "returned hostname-id: '%lld'\n", ( long long ) id );
        assert ( id >= 0 );
        assert ( id <= INT32_MAX );
        hostname_id = ( I32 ) id;
    }

    void DBLogger :: initProcname ( const char * procname )
    {
        if ( procname == 0 || procname [ 0 ] == 0 )
            return;

        if ( log_db == 0 )
            throw Exception ( rc_logic_err, "unable to initialize procname - not connected to logdb" );

        std :: string pname = log_db -> escapeLiteral ( procname );

        TRACE ( TRACE_PRG, "getting id for procname \"%s\"\n", pname . c_str () );

        // create query
        log_db -> query ()
            << "select makeProcname ( cast ( "
            << pname
            << " as varchar ) );"
            << endm
            ;

        TRACE ( TRACE_PRG, "executing query: '%s'\n", log_db -> sql () . c_str () );
        PGResult rslt = log_db -> exec ();
        TRACE ( TRACE_PRG, "result status message: '%s'\n", rslt . errorMessage () . c_str () );
        if ( rslt . status () != rslt . tuples_ok )
            throw Exception ( rc_runtime_err, rslt . errorMessage () );

        // extract connection id
        assert ( rslt . numRows () == 1 );
        assert ( rslt . numColumns () == 1 );
        if ( ! rslt . nextRow () )
            throw Exception ( rc_logic_err, "no rows available" );

        I64 id = strtol ( rslt . cellData ( 1 ) );
        TRACE ( TRACE_PRG, "returned procname-id: '%lld'\n", ( long long ) id );
        assert ( id >= 0 );
        assert ( id <= INT32_MAX );
        procname_id = ( I32 ) id;
    }

    void DBLogger :: handleMsg ( int priority, const timespec & ts, const void * data, size_t bytes )
    {
        if ( log_db == 0 )
            throw Exception ( rc_logic_err, "unable to record message - not connected to logdb" );

        // haven't yet figured out how to handle priority
        // it either gets checked here as a filter
        // or checked on logger side, or both.
        assert ( priority >= 0 && priority < 8 );

        // crop to 64K - 1
        if ( bytes > 0xFFFF )
            bytes = 0xFFFF;

        // drop to dedicated log file
        text_write ( data, bytes );

        // break out "ts"
        TRACE ( TRACE_GEEK, "breaking out GMT timestamp\n" );
        time_t secs = ts . tv_sec;
        struct tm tm;
        gmtime_r ( & secs, & tm );

        // create microseconds for postgres
        U64 uS = ts . tv_nsec / 1000;

        // create message string
        std :: string msg ( ( const char * ) data, bytes );

        // escape string for insert
        msg = log_db -> escapeLiteral ( msg );

        // create query
        log_db -> query ()
            << "insert into log_entry_tbl ( "
               "queue_id, "
               "priority_id, "
               "proc_ts, "
               "hostname_id, "
               "procname_id, "
               "proc_id, "
               "msg"
               " ) values ( cast ( "
            << queue_id
            << " as int4 ), cast ( "
            << ( priority & 7 )
            << " as int2 ), cast ( '"
            << tm . tm_year + 1900   // 4 digits pretty certain
            << '-'
            << setw ( 2 ) << setf ( '0' ) << tm . tm_mon + 1
            << '-'
            << setw ( 2 ) << setf ( '0' ) << tm . tm_mday
            << ' '
            << setw ( 2 ) << setf ( '0' ) << tm . tm_hour
            << ':'
            << setw ( 2 ) << setf ( '0' ) << tm . tm_min
            << ':'
            << setw ( 2 ) << setf ( '0' ) << tm . tm_sec
            << '.'
            << setw ( 6 ) << setf ( '0' ) << uS
            << "+00' as timestamp with time zone ), cast ( "
            << hostname_id
            << " as int4 ), cast ( "
            << procname_id
            << " as int4 ), cast ( "
            << pid
            << " as int4 ), cast ( "
            << msg
            << " as varchar ) );"
            << endm
            ;

        TRACE ( TRACE_PRG, "executing query: '%s'\n", log_db -> sql () . c_str () );
        PGResult rslt = log_db -> exec ();
        if ( rslt . status () != rslt . cmd_ok )
        {
            TRACE ( TRACE_PRG, "result status message: '%s'\n", rslt . errorMessage () . c_str () );
            throw Exception ( rc_runtime_err, rslt . errorMessage () );
        }
    }
}
