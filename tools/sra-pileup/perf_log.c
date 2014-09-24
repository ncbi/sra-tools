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
#include <klib/time.h>
#include <klib/printf.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <stdarg.h>

typedef struct perf_log perf_log;
struct perf_log
{
    KFile * perf_log_file;
    KTime time_stamp;
    const char * tool_name;
    const char * section_name;
    const char * sub_section_name;

    KTime_t tool_start;
    KTime_t section_start;
    KTime_t sub_section_start;
    KTime_t chunk_start;

    uint64_t file_pos;
    uint64_t counter;
    uint64_t limit;
};


const char * perf_log_unknown = "unknown";

static const char * value_or_unknown( const char * value )
{
    const char * s;
    if ( value == NULL ) s = perf_log_unknown; else s = value;
    return s;
}


static void perf_log_write_args( struct perf_log * pl, const char *fmt, va_list vargs )
{
    if ( pl != NULL )
    {
        char buffer[ 1024 ];
        size_t num_writ;
        rc_t rc = string_vprintf ( buffer, sizeof buffer, &num_writ, fmt, vargs );
        if ( rc == 0 )
        {
            size_t num_writ_2;
            rc = KFileWriteAll ( pl->perf_log_file, pl->file_pos, buffer, num_writ, &num_writ_2 );
            if ( rc == 0 )
                pl->file_pos += num_writ_2;
        }
    }
}


static void perf_log_write( struct perf_log * pl, const char *fmt, ... )
{
    if ( pl != NULL )
    {
        va_list args;

        va_start ( args, fmt );
        perf_log_write_args ( pl, fmt, args );
        va_end ( args );
    }
}


static void write_tool_start( struct perf_log * pl )
{
    KTime t;
    KTimeLocal ( &t, pl->tool_start );
    perf_log_write( pl, "start >%s< at %T\n", value_or_unknown( pl->tool_name ), &t );
}


static void write_tool_end( struct perf_log * pl )
{
    KTime_t tool_end = KTimeStamp();
    KTime t;
    KTimeLocal ( &t, tool_end );
    perf_log_write( pl, "end.. >%s< at %T ( %lu seconds )\n",
                    value_or_unknown( pl->tool_name ), &t, ( tool_end - pl->tool_start ) );
}

struct perf_log * make_perf_log( const char * filename, const char * toolname )
{
    struct perf_log * res = NULL;
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir ( &dir );
    if ( rc == 0 )
    {
        KFile * f;
        rc = KDirectoryCreateFile ( dir, &f, false, 0664, kcmInit, "%s", filename );
        if ( rc == 0 )
        {
            res = malloc( sizeof * res );
            if ( res != NULL )
            {
                res->perf_log_file = f;
                KTimeLocal ( &res->time_stamp, KTimeStamp() );
                if ( toolname != NULL )
                    res->tool_name = string_dup_measure ( toolname, NULL );
                else
                    res->tool_name = NULL;
                res->section_name = NULL;
                res->sub_section_name = NULL;
                res->tool_start = KTimeStamp();
                res->section_start = res->tool_start;
                res->sub_section_start = res->tool_start;
                res->chunk_start = res->tool_start;

                res->file_pos = 0;
                res->counter = 0;
                res->limit = 10000;
                write_tool_start( res );
            }
            else
                KFileRelease ( f );
        }
        KDirectoryRelease ( dir );
    }
    return res;
}


void free_perf_log( struct perf_log * pl )
{
    if ( pl != NULL )
    {
        write_tool_end( pl );
        KFileRelease ( pl->perf_log_file );
        if ( pl->tool_name != NULL ) free( ( void * )pl->tool_name );
        free( ( void * ) pl );
    }
}


void perf_log_start_section( struct perf_log * pl, const char * section_name )
{
    if ( pl != NULL )
    {
        KTime t;
        pl->section_start = KTimeStamp();
        KTimeLocal ( &t, pl->section_start );

        if ( pl->section_name != NULL ) free( ( void * ) pl->section_name );
        if ( section_name != NULL )
            pl->section_name = string_dup_measure ( section_name, NULL );
        else
            pl->section_name = NULL;

        perf_log_write( pl, "start [%s] at %T\n",
                        value_or_unknown( pl->section_name ), &t );
    }
}


void perf_log_end_section( struct perf_log * pl )
{
    if ( pl != NULL )
    {
        KTime_t section_end = KTimeStamp();
        KTime t;
        KTimeLocal ( &t, section_end );
        perf_log_write( pl, "end.. [%s] at %T ( %lu seconds )\n",
                        value_or_unknown( pl->section_name ), &t, ( section_end - pl->section_start ) );

        if ( pl->section_name != NULL )
        {
            free( ( void * ) pl->section_name );
            pl->section_name = NULL;
        }
    }
}


void perf_log_start_sub_section( struct perf_log * pl, const char * sub_section_name )
{
    if ( pl != NULL )
    {
        KTime t;
        pl->sub_section_start = KTimeStamp();
        pl->chunk_start = pl->sub_section_start;
        pl->counter = 0;
        KTimeLocal ( &t, pl->sub_section_start );

        if ( pl->sub_section_name != NULL ) free( ( void * ) pl->sub_section_name );
        if ( sub_section_name != NULL )
            pl->sub_section_name = string_dup_measure ( sub_section_name, NULL );
        else
            pl->sub_section_name = NULL;

        perf_log_write( pl, "start (%s) at %T\n",
                        value_or_unknown( pl->sub_section_name ), &t );
    }
}


void perf_log_end_sub_section( struct perf_log * pl )
{
    if ( pl != NULL )
    {
        KTime_t sub_section_end = KTimeStamp();
        KTime t;
        KTimeLocal ( &t, sub_section_end );
        perf_log_write( pl, "end.. (%s) at %T ( %lu seconds )\n",
                        value_or_unknown( pl->sub_section_name ), &t, ( sub_section_end - pl->sub_section_start ) );

        if ( pl->sub_section_name != NULL )
        {
            free( ( void * ) pl->sub_section_name );
            pl->sub_section_name = NULL;
        }
    }
}


static uint64_t lpm( uint64_t seconds, uint64_t written )
{
    uint64_t res = 0;
    if ( seconds > 0 )
        res = ( written * 60 ) / seconds;
    return res;
}


void perf_log_line( struct perf_log * pl, uint64_t pos )
{
    if ( pl != NULL )
    {
        pl->counter++;
        if ( pl->counter > pl->limit )
        {
            KTime_t chunk_end = KTimeStamp();
            uint64_t seconds = ( chunk_end - pl->chunk_start );
            
            perf_log_write( pl, "%lu lines in %lu seconds, at %lu, lpm = %lu\n",
                            pl->limit, seconds, pos, lpm( seconds, pl->counter ) );

            pl->counter = 0;
            pl->chunk_start = chunk_end;
        }
    }
}