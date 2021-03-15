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
#include "file_printer.h"
#include "helper.h"

#include <kfs/buffile.h>

typedef struct file_printer
{
    struct KFile * f;
    SBuffer print_buffer;
    uint64_t file_pos;
} file_printer;


void destroy_file_printer( struct file_printer * printer )
{
    if ( NULL != printer )
    {
        if ( NULL != printer -> f )
        {
            rc_t rc2 = KFileRelease( printer -> f );
            if ( 0 != rc2 )
            {
                ErrMsg( "destroy_file_printer.KFileRelease() -> %R", rc2 );
            }
        }
        release_SBuffer( &( printer -> print_buffer ) );
        free( ( void * ) printer );
    }
}


rc_t make_file_printer_from_file( KFile * f, struct file_printer ** printer, size_t print_buffer_size )
{
    rc_t rc;
    file_printer * p = calloc( 1, sizeof * p );
    if ( NULL == p )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_file_printer_from_file().calloc( %d ) -> %R", ( sizeof * p ), rc );
        {
            rc_t rc2 = KFileRelease( f );
            if ( 0 != rc2 )
            {
                ErrMsg( "make_file_printer_from_file().KFileRelease().1 -> %R", rc2 );
            }
        }
    }
    else
    {
        rc = make_SBuffer( &( p -> print_buffer ), print_buffer_size );
        if ( 0 != rc )
        {
            rc_t rc2 = KFileRelease( f );
            if ( 0 != rc2 )
            {
                ErrMsg( "make_file_printer_from_file().KFileRelease().2 -> %R", rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        else
        {
            p -> f = f;
            *printer = p;
        }
    }
    return rc;
}


rc_t make_file_printer_from_filename( const KDirectory * dir, struct file_printer ** printer,
        size_t file_buffer_size, size_t print_buffer_size, const char * fmt, ... )
{
    rc_t rc;
    struct KFile * f;

    va_list args;
    va_start ( args, fmt );

    rc = KDirectoryVCreateFile( ( KDirectory * )dir, &f, false, 0664, kcmInit, fmt, args );
    va_end ( args );
    if ( 0 != rc )
    {
        ErrMsg( "make_file_printer_from_filename().KDirectoryVCreateFile() -> %R", rc );
    }
    else
    {
        struct KFile * temp_file = f;
        if ( file_buffer_size > 0 )
        {
            rc = KBufFileMakeWrite( &temp_file, f, false, file_buffer_size );
            if ( rc != 0 )
            {
                ErrMsg( "make_file_printer_from_filename().KBufFileMakeWrite() -> %R", rc );
            }
            {
                rc_t rc2 = KFileRelease( f );
                if ( 0 != rc2 )
                {
                    ErrMsg( "make_file_printer_from_filename().KFileRelease() -> %R", rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        if ( 0 == rc )
        {
            rc = make_file_printer_from_file( temp_file, printer, print_buffer_size );
        }
    }
    return rc;
}


rc_t file_print( struct file_printer * printer, const char * fmt, ... )
{
    rc_t rc = 0;
    bool done = false;

    while ( 0 == rc && !done )
    {
        va_list args;
        va_start ( args, fmt );

        rc = print_to_SBufferV( & printer -> print_buffer, fmt, args );
        va_end ( args );

        done = ( rc == 0 );
        if ( !done )
        {
            rc = try_to_enlarge_SBuffer( & printer -> print_buffer, rc );
        }
    }

    if ( 0 == rc )
    {
        size_t num_writ, to_write;
        to_write = printer -> print_buffer . S . size;
        const char * src = printer -> print_buffer . S . addr;
        rc = KFileWriteAll( printer -> f, printer -> file_pos, src, to_write, &num_writ );
        if ( 0 != rc )
        {
            ErrMsg( "file_print().KFileWriteAll( at %lu ) -> %R", printer -> file_pos, rc );
        }
        else if ( num_writ != to_write )
        {
            rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
            ErrMsg( "file_print().KFileWriteAll( at %lu ) ( %d vs %d ) -> %R", printer -> file_pos, to_write, num_writ, rc );
        }
        else
        {
            printer -> file_pos += num_writ;
        }
    }
    return rc;
}
