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
#include "join_results.h"
#include "helper.h"
#include <klib/vector.h>
#include <klib/printf.h>
#include <kfs/buffile.h>

typedef struct join_printer
{
    struct KFile * f;
    uint64_t file_pos;
} join_printer;

typedef struct join_results
{
    KDirectory * dir;
    struct temp_registry * registry;
    const char * output_base;
    SBuffer print_buffer;   /* we have only one print_buffer... */
    Vector printers;
    size_t buffer_size;
} join_results;

static void CC destroy_join_printer( void * item, void * data )
{
    if ( item != NULL )
    {
        join_printer * p = item;
        KFileRelease( p -> f );
        free( item );
    }
}

void destroy_join_results( join_results * self )
{
    if ( self != NULL )
    {
        VectorWhack ( &self -> printers, destroy_join_printer, NULL );
        release_SBuffer( &self -> print_buffer );
        free( ( void * ) self );
    }
}

rc_t make_join_results( struct KDirectory * dir,
                        struct join_results ** results,
                        struct temp_registry * registry,
                        const char * output_base,
                        size_t file_buffer_size,
                        size_t print_buffer_size )
{
    rc_t rc;
    join_results * p = calloc( 1, sizeof * p );
    *results = NULL;
    if ( p == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_join_results().calloc( %d ) -> %R", ( sizeof * p ), rc );
    }
    else
    {
        p -> dir = dir;
        p -> output_base = output_base;
        p -> buffer_size = file_buffer_size;
        p -> registry = registry;
        rc = make_SBuffer( &( p -> print_buffer ), print_buffer_size ); /* helper.c */
        if ( rc == 0 )
        {
            VectorInit ( &p -> printers, 0, 4 );
            *results = p;
        }
    }
    return rc;
}

static rc_t make_join_printer( join_results * self, uint32_t read_id, join_printer ** printer )
{
    char filename[ 4096 ];
    size_t num_writ;
    
    rc_t rc = string_printf( filename, sizeof filename, &num_writ, "%s.%u", self -> output_base, read_id );
    *printer = NULL;
    if ( rc != 0 )
        ErrMsg( "make_join_printer().string_vprintf() -> %R", rc );
    else
    {
        struct KFile * f;
        rc = KDirectoryCreateFile( self -> dir, &f, false, 0664, kcmInit, "%s", filename );
        if ( rc != 0 )
            ErrMsg( "make_join_printer().KDirectoryVCreateFile() -> %R", rc );
        else
        {
            if ( self -> buffer_size > 0 )
            {
                struct KFile * temp_file = f;
                rc = KBufFileMakeWrite( &temp_file, f, false, self -> buffer_size );
                KFileRelease( f );
                f = temp_file;
                if ( rc != 0 )
                    ErrMsg( "KBufFileMakeWrite() -> %R", rc );
            }
            if ( rc == 0 )
            {
                join_printer * p = calloc( 1, sizeof * p );
                if ( p == NULL )
                {
                    KFileRelease( f );
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                    ErrMsg( "make_join_printer().calloc( %d ) -> %R", ( sizeof * p ), rc );
                }
                else
                {
                    rc = register_temp_file( self -> registry, read_id, filename );
                    if ( rc != 0 )
                    {
                        free( p );
                        KFileRelease( f );
                    }
                    else
                    {
                        p -> f = f;
                        *printer = p;
                    }
                }
            }
        }
    }
    return rc;
}

rc_t join_results_print( struct join_results * self, uint32_t read_id, const char * fmt, ... )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcSelf, rcNull );
    else if ( fmt == NULL )
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    {
        join_printer * p = VectorGet ( &self -> printers, read_id );
        if ( p == NULL )
        {
            rc = make_join_printer( self, read_id, &p );
            if ( rc == 0 )
            {
                rc = VectorSet ( &self -> printers, read_id, p );
                if ( rc != 0 )
                    destroy_join_printer( p, NULL );
            }   
        }
        if ( rc == 0 && p != NULL )
        {
            va_list args;
            va_start ( args, fmt );
            rc = print_to_SBufferV( & self -> print_buffer, fmt, args );
            va_end ( args );
            
            if ( rc == 0 )
            {
                size_t num_writ, to_write;
                to_write = self -> print_buffer . S . size;
                const char * src = self -> print_buffer . S . addr;
                rc = KFileWriteAll( p -> f, p -> file_pos, src, to_write, &num_writ );
                if ( rc != 0 )
                    ErrMsg( "join_results_print().KFileWriteAll( at %lu ) -> %R", p -> file_pos, rc );
                else if ( num_writ != to_write )
                {
                    rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
                    ErrMsg( "join_results_print().KFileWriteAll( at %lu ) ( %d vs %d ) -> %R", p -> file_pos, to_write, num_writ, rc );
                }
                else
                    p -> file_pos += num_writ;
            }
        }
    }
    return rc;
}
