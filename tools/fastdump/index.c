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
#include "index.h"
#include "helper.h"

#include <kfs/file.h>
#include <kfs/buffile.h>

typedef struct index_writer
{
    struct KFile * f;
    uint64_t frequency, pos, last_key;
} index_writer;


void release_index_writer( struct index_writer * writer )
{
    if ( writer != NULL )
    {
        if ( writer->f != NULL ) KFileRelease( writer->f );
        free( ( void * ) writer );
    }
}


static rc_t write_value( index_writer * writer, uint64_t value )
{
    size_t num_writ;
    rc_t rc = KFileWriteAll( writer->f, writer->pos, &value, sizeof value, &num_writ );
    if ( rc != 0 )
        ErrMsg( "write_value.KFileWriteAll( key ) -> %R", rc );
    else if ( num_writ != sizeof value )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
        ErrMsg( "write_value.KFileWriteAll( key ) -> %R", rc );
    }
    else
        writer->pos += num_writ;
    return rc;
}


static rc_t write_key_and_offset( index_writer * writer, uint64_t key, uint64_t offset )
{
    rc_t rc = write_value( writer, key );
    if ( rc == 0 )
        rc = write_value( writer, offset );
    return rc;
}


rc_t write_key( struct index_writer * writer, uint64_t key, uint64_t offset )
{
    rc_t rc;
    if ( writer == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "write_index_valuer() -> %R", rc );
    }
    else
    {
        if ( key > ( writer->last_key + writer->frequency ) )
        {
            rc = write_key_and_offset( writer, key, offset );
            if ( rc == 0 )
                writer->last_key = key;
        }
    }
    return rc;
}


rc_t make_index_writer( KDirectory * dir, struct index_writer ** writer,
                        size_t buf_size, uint64_t frequency, const char * fmt, ... )
{
    rc_t rc;
    struct KFile * f;
    
    va_list args;
    va_start ( args, fmt );

    rc = KDirectoryVCreateFile( dir, &f, false, 0664, kcmInit, fmt, args );
    if ( rc != 0 )
        ErrMsg( "KDirectoryVCreateFile() -> %R", rc );
    else
    {
        struct KFile * temp_file;
        rc = KBufFileMakeWrite( &temp_file, f, false, buf_size );
        KFileRelease( f );
        if ( rc != 0 )
            ErrMsg( "KBufFileMakeWrite() -> %R", rc );
        else
        {
            index_writer * w = calloc( 1, sizeof * w );
            if ( w == NULL )
            {
                KFileRelease( temp_file );
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "calloc( %d ) -> %R", ( sizeof * w ), rc );
            }
            else
            {
                w->f = temp_file;
                w->frequency = frequency;
                rc = write_value( w, frequency );
                if ( rc == 0 )
                    rc = write_key_and_offset( w, 1, 0 );
                    
                if ( rc == 0 )
                    *writer = w;
                else
                    release_index_writer( w );
            }
        }
    }
    va_end ( args );
   
    return rc;
}


/* ----------------------------------------------------------------------- */

typedef struct index_reader
{
    const struct KFile * f;
    uint64_t frequency, file_size;
} index_reader;


void release_index_reader( struct index_reader * reader )
{
    if ( reader != NULL )
    {
        if ( reader->f != NULL ) KFileRelease( reader->f );
        free( ( void * ) reader );
    }
}


static rc_t read_value( struct index_reader * reader, uint64_t pos, uint64_t * value )
{
    size_t num_read;
    rc_t rc = KFileRead( reader->f, pos, ( void *)value, sizeof *value, &num_read );
    if ( rc != 0 )
        ErrMsg( "read_value.KFileRead( at %ld ) -> %R", pos, rc );
    else if ( num_read != sizeof *value )
        rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
    return rc;
}


rc_t make_index_reader( KDirectory * dir, struct index_reader ** reader,
                        size_t buf_size, const char * fmt, ... )
{
    rc_t rc;
    const struct KFile * f;
    
    va_list args;
    va_start ( args, fmt );
    
    rc = KDirectoryVOpenFileRead( dir, &f, fmt, args );
    if ( rc != 0 )
        ErrMsg( "KDirectoryVOpenFileRead() -> %R", rc );
    else
    {
        const struct KFile * temp_file;
        rc = KBufFileMakeRead( &temp_file, f, buf_size );
        KFileRelease( f );
        if ( rc != 0 )
        {
            ErrMsg( "KBufFileMakeRead() -> %R", rc );
        }
        else
        {
            index_reader * r = calloc( 1, sizeof * r );
            if ( r == NULL )
            {
                KFileRelease( temp_file );
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "calloc( %d ) -> %R", ( sizeof * r ), rc );
            }
            else
            {
                r->f = temp_file;
                rc = read_value( r, 0, &r->frequency );
                if ( rc == 0 )
                    rc = KFileSize( temp_file, &r->file_size );

                if ( rc == 0 )
                    *reader = r;
                else
                    release_index_reader( r );
            }
        }
    }
    va_end ( args );
    return rc;
}


static uint64_t key_to_pos_guess( const struct index_reader * reader, uint64_t key )
{
    uint64_t chunk_id = ( key / reader->frequency );
    return ( ( sizeof reader->frequency ) + ( chunk_id * ( 2 * ( sizeof reader->frequency ) ) ) );
}


static rc_t read_3( const struct index_reader * reader, uint64_t pos, uint64_t * data, size_t to_read )
{
    size_t num_read;
    rc_t rc = KFileRead( reader->f, pos, ( void *)data, to_read, &num_read );
    if ( rc != 0 )
        ErrMsg( "read_3.KFileRead( at %ld ) -> %R", pos, rc );
    else if ( num_read != to_read )
        rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
    return rc;
}


rc_t get_nearest_offset( const struct index_reader * reader, uint64_t key,
                   uint64_t * nearest, uint64_t * offset )
{
    rc_t rc = 0;
    if ( reader == NULL || nearest == NULL || offset == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "get_nearest_offset() -> %R", rc );
    }
    else
    {
        uint64_t data[ 6 ];
        uint64_t pos = key_to_pos_guess( reader, key );
        bool found = false;
        while ( rc == 0 && !found && pos < reader->file_size )
        {        
            rc = read_3( reader, pos, data, sizeof data );
            if ( rc == 0 )
            {
                if ( key >= data[ 0 ] && key < data[ 2 ] )
                {
                    found = true;
                    *nearest = data[ 0 ]; *offset = data[ 1 ];
                }
                else if ( key >= data[ 2 ] && key < data[ 4 ] )
                {
                    found = true;
                    *nearest = data[ 2 ]; *offset = data[ 3 ];
                }
                if ( !found )
                {
                    if ( key < data[ 0 ] )
                    {
                        if ( pos > sizeof reader->frequency )
                            pos -= ( 2 * ( sizeof reader->frequency ) );
                        else
                        {
                            found = true;
                            *nearest = data[ 0 ]; *offset = data[ 1 ];
                        }
                    }
                    else if ( key > data[ 4 ] )
                    {
                        pos += ( 2 * ( sizeof reader->frequency ) );
                    }
                }
            }
        }
        if ( !found )
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
    }
    return rc;
}
