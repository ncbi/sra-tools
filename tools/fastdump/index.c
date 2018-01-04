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
    rc_t rc = 0;
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

static rc_t make_index_writer_obj( struct index_writer ** writer,
                                   uint64_t frequency,
                                   struct KFile * f )
{
    rc_t rc = 0;
    index_writer * w = calloc( 1, sizeof * w );
    if ( w == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * w ), rc );
    }
    else
    {
        w -> f = f;
        w -> frequency = frequency;
        rc = write_value( w, frequency );
        if ( rc == 0 )
            rc = write_key_and_offset( w, 1, 0 );

        if ( rc == 0 )
            *writer = w;
        else
            release_index_writer( w );
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
        if ( buf_size > 0 )
        {
            struct KFile * temp_file;
            rc = KBufFileMakeWrite( &temp_file, f, false, buf_size );
            if ( rc != 0 )
                ErrMsg( "KBufFileMakeWrite() -> %R", rc );
            else
            {
                KFileRelease( f );
                f = temp_file;
            }
        }

        if ( rc == 0 )
        {
            rc = make_index_writer_obj( writer, frequency, f );
            if ( rc != 0 )
                KFileRelease( f );
        }
    }
    va_end ( args );
   
    return rc;
}


/* ----------------------------------------------------------------------- */

typedef struct index_reader
{
    const struct KFile * f;
    uint64_t frequency, file_size, max_key;
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
    rc_t rc = KFileReadAll( reader->f, pos, ( void * )value, sizeof *value, &num_read );
    if ( rc != 0 )
        ErrMsg( "read_value.KFileReadAll( at %ld ) -> %R", pos, rc );
    else if ( num_read != sizeof *value )
        rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
    return rc;
}

static rc_t make_index_reader_obj( struct index_reader ** reader,
                                   const struct KFile * f )
{
    rc_t rc = 0;
    index_reader * r = calloc( 1, sizeof * r );
    if ( r == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * r ), rc );
    }
    else
    {
        r -> f = f;
        rc = read_value( r, 0, &r -> frequency );
        if ( rc == 0 )
            rc = KFileSize( f, &r -> file_size );

        if ( rc == 0 )
        {
            get_max_key( r, &r -> max_key );
            *reader = r;
        }
        else
            release_index_reader( r );
    }
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
        if ( buf_size > 0 )
        {
            const struct KFile * temp_file;
            rc = KBufFileMakeRead( &temp_file, f, buf_size );
            if ( rc != 0 )
                ErrMsg( "KBufFileMakeRead() -> %R", rc );
            else
            {
                KFileRelease( f );
                f = temp_file;
            }
        }
        if ( rc == 0 )
            rc = make_index_reader_obj( reader, f );
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
    rc_t rc = KFileReadAll( reader->f, pos, ( void *)data, to_read, &num_read );
    if ( rc != 0 )
        ErrMsg( "read_3.KFileReadAll( at %ld ) -> %R", pos, rc );
    else if ( num_read != to_read )
        rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
    return rc;
}


rc_t get_nearest_offset( const struct index_reader * reader, uint64_t key_to_find,
                   uint64_t * key_found, uint64_t * offset )
{
    rc_t rc = 0;
    if ( reader == NULL || key_found == NULL || offset == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "get_nearest_offset() -> %R", rc );
    }
    else
    {
        uint64_t data[ 6 ];
        /*
            data[ 0 ] ... key0      data[ 1 ] ... offset0
            data[ 2 ] ... key1      data[ 3 ] ... offset1
            data[ 4 ] ... key2      data[ 5 ] ... offset2
        */
        uint64_t pos = key_to_pos_guess( reader, key_to_find );
        bool found = false;
        while ( rc == 0 && !found && pos < reader->file_size )
        {        
            rc = read_3( reader, pos, data, sizeof data );
            if ( rc == 0 )
            {
                if ( key_to_find >= data[ 0 ] && key_to_find < data[ 2 ] )
                {
                    /* key_to_find is between key0 and key1 */
                    found = true;
                    *key_found = data[ 0 ];
                    *offset = data[ 1 ];
                }
                else if ( key_to_find >= data[ 2 ] && key_to_find < data[ 4 ] )
                {
                    /* key_to_find is between key1 and key2 */
                    found = true;
                    *key_found = data[ 2 ];
                    *offset = data[ 3 ];
                }
                if ( !found )
                {
                    if ( key_to_find < data[ 0 ] )
                    {
                        /* key_to_find is smaller than our guess */
                        if ( pos > sizeof reader->frequency )
                            pos -= ( 2 * ( sizeof reader->frequency ) );
                        else
                        {
                            found = true;
                            *key_found = data[ 0 ];
                            *offset = data[ 1 ];
                        }
                    }
                    else if ( key_to_find > data[ 4 ] )
                    {
                        /* key_to_find is bigger than our guess */
                        pos += ( 2 * ( sizeof reader -> frequency ) );
                    }
                }
            }
        }
        if ( !found )
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
    }
    return rc;
}


rc_t get_max_key( const struct index_reader * reader, uint64_t * max_key )
{
    rc_t rc = 0;
    if ( reader == NULL || max_key == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "get_nearest_offset() -> %R", rc );
    }
    else if ( reader->max_key > 0 )
    {
        *max_key = reader->max_key;
    }
    else
    {
        uint64_t data[ 6 ];
        uint64_t pos = reader -> file_size - ( sizeof data );
        rc = read_3( reader, pos, data, sizeof data );
        if ( rc == 0 )
             *max_key = data[ 4 ];
    }
    return rc;
}