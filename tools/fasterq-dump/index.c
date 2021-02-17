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
    if ( NULL != writer )
    {
        if ( NULL != writer -> f )
        {
            rc_t rc = KFileRelease( writer -> f );
            if ( 0 != rc )
            {
                ErrMsg( "index.c release_index_writer().KFileRelease() -> %R", rc );
            }
        }
        free( ( void * ) writer );
    }
}


static rc_t write_value( index_writer * writer, uint64_t value )
{
    rc_t rc = KFileWriteExactly( writer -> f, writer -> pos, &value, sizeof value );
    if ( 0 != rc )
    {
        ErrMsg( "index.c write_value().KFileWriteExactly( key ) -> %R", rc );
    }
    else
    {
        writer -> pos += sizeof value;
    }
    return rc;
}


static rc_t write_key_and_offset( index_writer * writer, uint64_t key, uint64_t offset )
{
    rc_t rc = write_value( writer, key );
    if ( 0 == rc )
    {
        rc = write_value( writer, offset );
    }
    return rc;
}


rc_t write_key( struct index_writer * writer, uint64_t key, uint64_t offset )
{
    rc_t rc = 0;
    if ( NULL == writer )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "index.c write_key() -> %R", rc );
    }
    else
    {
        if ( key > ( writer -> last_key + writer -> frequency ) )
        {
            rc = write_key_and_offset( writer, key, offset );
            if ( 0 == rc )
            {
                writer -> last_key = key;
            }
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
    if ( NULL == w )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "index.c make_index_writer_obj().calloc( %d ) -> %R", ( sizeof * w ), rc );
    }
    else
    {
        w -> f = f;
        w -> frequency = frequency;
        rc = write_value( w, frequency );
        if ( 0 == rc )
        {
            rc = write_key_and_offset( w, 1, 0 );
        }

        if ( 0 == rc )
        {
            *writer = w;
        }
        else
        {
            release_index_writer( w );
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
    va_end ( args );

    if ( 0 != rc )
    {
        ErrMsg( "index.c make_index_writer().KDirectoryVCreateFile() -> %R", rc );
    }
    else
    {
        if ( buf_size > 0 )
        {
            struct KFile * temp_file;
            rc = KBufFileMakeWrite( &temp_file, f, false, buf_size );
            if ( 0 != rc )
            {
                ErrMsg( "index.c make_index_writer().KBufFileMakeWrite() -> %R", rc );
            }
            else
            {
                {
                    rc_t rc2 = KFileRelease( f );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "index.c make_index_writer().KFileRelease().1 -> %R", rc2 );
                    }
                }
                f = temp_file;
            }
        }

        if ( 0 == rc )
        {
            rc = make_index_writer_obj( writer, frequency, f );
            if ( 0 != rc )
            {
                rc_t rc2 = KFileRelease( f );
                if ( 0 != rc2 )
                {
                    ErrMsg( "index.c make_index_writer().KFileRelease().2 -> %R", rc2 );
                }
            }
        }
    }
    return rc;
}


/* ----------------------------------------------------------------------- */

typedef struct index_reader
{
    const struct KFile * f;
    uint64_t frequency, file_size, max_key;
} index_reader;


void release_index_reader( index_reader * reader )
{
    if ( NULL != reader )
    {
        if ( NULL != reader -> f )
        {
            rc_t rc = KFileRelease( reader -> f );
            if ( 0 != rc )
            {
                ErrMsg( "index.c make_index_reader().KFileRelease() -> %R", rc );
            }
        }
        free( ( void * ) reader );
    }
}

static rc_t read_value( index_reader * reader, uint64_t pos, uint64_t * value )
{
    rc_t rc = KFileReadExactly( reader -> f, pos, ( void * )value, sizeof *value );
    if ( 0 != rc )
    {
        ErrMsg( "index.c read_value().KFileReadExactly( at %ld ) -> %R", pos, rc );
    }
    return rc;
}

static rc_t make_index_reader_obj( index_reader ** reader,
                                   const struct KFile * f )
{
    rc_t rc = 0;
    index_reader * r = calloc( 1, sizeof * r );
    if ( NULL == r )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "index.c make_index_reader_obj().calloc( %d ) -> %R", ( sizeof * r ), rc );
    }
    else
    {
        r -> f = f;
        rc = read_value( r, 0, &r -> frequency );
        if ( 0 == rc )
        {
            rc = KFileSize( f, &r -> file_size );
            if ( 0 != rc )
            {
                ErrMsg( "index.c make_index_reader_obj().KFileSize() -> %R", rc );
            }
        }

        if ( 0 == rc )
        {
            get_max_key( r, &r -> max_key );
            *reader = r;
        }
        else
        {
            release_index_reader( r );
        }
    }
    return rc;
}

rc_t make_index_reader( const KDirectory * dir, index_reader ** reader,
                        size_t buf_size, const char * fmt, ... )
{
    rc_t rc;
    const struct KFile * f;

    va_list args;
    va_start ( args, fmt );

    rc = KDirectoryVOpenFileRead( dir, &f, fmt, args );
    va_end ( args );

    if ( 0 != rc )
    {
        ErrMsg( "index.c make_index_reader() KDirectoryVOpenFileRead() -> %R", rc );
    }
    else
    {
        if ( buf_size > 0 )
        {
            const struct KFile * temp_file;
            rc = KBufFileMakeRead( &temp_file, f, buf_size );
            if ( 0 != rc )
            {
                ErrMsg( "index.c make_index_reader() KBufFileMakeRead() -> %R", rc );
            }
            else
            {
                rc = KFileRelease( f );
                if ( 0 != rc )
                {
                    ErrMsg( "index.c make_index_reader() KFileRelease() -> %R", rc );
                }
                f = temp_file;
            }
        }
        if ( 0 == rc )
        {
            rc = make_index_reader_obj( reader, f );
        }
    }
    return rc;
}


static uint64_t key_to_pos_guess( const index_reader * self, uint64_t key )
{
    uint64_t chunk_id = ( key / self -> frequency );
    return ( ( sizeof self -> frequency ) + ( chunk_id * ( 2 * ( sizeof self -> frequency ) ) ) );
}

rc_t get_nearest_offset( const index_reader * self,
                         uint64_t key_to_find,
                         uint64_t * key_found,
                         uint64_t * offset )
{
    rc_t rc = 0;
    if ( NULL == self || NULL == key_found || NULL == offset )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "index.c get_nearest_offset() -> %R", rc );
    }
    else if ( self -> file_size <= 24 )
    {
        rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
    }
    else
    {
        uint64_t data[ 6 ];
        /*
            data[ 0 ] ... key0      data[ 1 ] ... offset0
            data[ 2 ] ... key1      data[ 3 ] ... offset1
            data[ 4 ] ... key2      data[ 5 ] ... offset2
        */
        uint64_t pos = key_to_pos_guess( self, key_to_find );
        size_t to_read = sizeof data;
        bool found = false;
        uint64_t rounds = 0;
        while ( 0 == rc && !found && pos < self -> file_size )
        {
            rounds++;
            if ( rounds > 100 )
            {
                rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcExhausted );
                ErrMsg( "index.c get_nearest_offset() -> too many rounds pos = %lu of %lu", pos, self -> file_size );
            }
            else
            {
                if ( ( ( pos + to_read ) - 1 ) > self -> file_size )
                {
                    to_read = ( self -> file_size - pos );
                }
                rc = KFileReadExactly( self -> f, pos, ( void * )&( data[ 0 ] ), to_read );
                if ( 0 != rc )
                {
                    ErrMsg( "index.c get_nearest_offset().KFileReadExactly( %lu of %lu, to_read = %lu ) failed %R",
                            pos, self -> file_size, to_read, rc );
                }
                else
                {
                    if ( to_read >= ( ( sizeof data[ 0 ] ) * 4 ) &&
                         key_to_find >= data[ 0 ] &&
                         key_to_find < data[ 2 ] )
                    {
                        /* key_to_find is between key0 and key1 */
                        found = true;
                        *key_found = data[ 0 ];
                        *offset = data[ 1 ];
                    }
                    else if ( to_read >= ( ( sizeof data[ 0 ] ) * 6 ) &&
                              key_to_find >= data[ 2 ] &&
                              key_to_find < data[ 4 ] )
                    {
                        /* key_to_find is between key1 and key2 */
                        found = true;
                        *key_found = data[ 2 ];
                        *offset = data[ 3 ];
                    }

                    if ( !found )
                    {
                        if ( to_read >= ( ( sizeof data[ 0 ] ) * 2 ) &&
                             key_to_find < data[ 0 ] )
                        {
                            /* key_to_find is smaller than our guess */
                            if ( pos > sizeof self -> frequency )
                            {
                                pos -= ( 2 * ( sizeof self -> frequency ) );
                            }
                            else
                            {
                                found = true;
                                *key_found = data[ 0 ];
                                *offset = data[ 1 ];
                            }
                        }
                        else if ( to_read >= ( ( sizeof data[ 0 ] ) * 6 ) &&
                                  key_to_find > data[ 4 ] )
                        {
                            /* key_to_find is bigger than our guess */
                            pos += ( 2 * ( sizeof self -> frequency ) );
                        }
                    }
                }
            }
        }
        if ( !found )
        {
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
        }
    }
    return rc;
}

rc_t get_max_key( const index_reader * self, uint64_t * max_key )
{
    rc_t rc = 0;
    if ( NULL == self || NULL == max_key )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "index.c get_max_key() -> %R", rc );
    }
    else if ( self -> max_key > 0 )
    {
        *max_key = self -> max_key;
    }
    else
    {
        uint64_t data[ 6 ];
        size_t to_read = sizeof data;
        uint64_t pos = 0;

        if ( self -> file_size >= to_read )
        {
            pos = self -> file_size - to_read;
        }
        else
        {
            to_read = self -> file_size;
        }

        rc = KFileReadExactly( self -> f, pos, ( void * )&data[ 0 ], to_read );
        if ( 0 != rc )
        {
            ErrMsg( "index.c get_max_key().KFileReadExactly( at %lu ) failed %R", pos, rc );
        }
        else
        {
            if ( self -> file_size >= to_read )
            {
                *max_key = data[ 4 ];
            }
            else if ( self -> file_size == ( sizeof data[ 0 ] * 3 ) )
            {
                *max_key = data[ 2 ];
            }
            else
            {
                rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
                ErrMsg( "index.c get_max_key() - index file has invalid size of %lu", self -> file_size );    
            }
        }
    }
    return rc;
}
