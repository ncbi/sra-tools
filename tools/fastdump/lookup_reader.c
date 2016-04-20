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

#include "lookup_reader.h"
#include "helper.h"

#include <klib/printf.h>
#include <kfs/file.h>
#include <kfs/buffile.h>

#include <string.h>

typedef struct lookup_reader
{
    const struct KFile * f;
    const struct index_reader * index;
    SBuffer buf;
    uint64_t pos;
} lookup_reader;


void release_lookup_reader( struct lookup_reader * reader )
{
    if ( reader != NULL )
    {
        if ( reader->f != NULL ) KFileRelease( reader->f );
        release_SBuffer( &reader->buf );
        free( ( void * ) reader );
    }
}


rc_t make_lookup_reader( const KDirectory *dir, const struct index_reader * index,
                         struct lookup_reader ** reader, size_t buf_size, const char * fmt, ... )
{
    rc_t rc;
    const struct KFile * f = NULL;
    
    va_list args;
    va_start ( args, fmt );
    
    rc = KDirectoryVOpenFileRead( dir, &f, fmt, args );
    if ( rc != 0 )
    {
        char tmp[ 4096 ];
        size_t num_writ;
        rc_t rc1 = string_vprintf( tmp, sizeof tmp, &num_writ, fmt, args );
        if ( rc1 != 0 )
            ErrMsg( "make_lookup_reader.KDirectoryVOpenFileRead( '?' ) -> %R", rc );
        else
            ErrMsg( "make_lookup_reader.KDirectoryVOpenFileRead( '%s' ) -> %R", tmp, rc );
    }
    else
    {
        const struct KFile * temp_file = NULL;
        rc = KBufFileMakeRead( &temp_file, f, buf_size );
        KFileRelease( f );
        if ( rc != 0 )
        {
            ErrMsg( "make_lookup_reader.KBufFileMakeRead() -> %R", rc );
        }
        else
        {
            lookup_reader * r = calloc( 1, sizeof * r );
            if ( r == NULL )
            {
                KFileRelease( temp_file );
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "make_lookup_reader.calloc( %d ) -> %R", ( sizeof * r ), rc );
            }
            else
            {
                r->f = temp_file;
                r->index = index;
                rc = make_SBuffer( &r->buf, 4096 );
                if ( rc == 0 )
                    *reader = r;
                else
                    release_lookup_reader( r );
            }
        }
    }
    va_end ( args );
    return rc;
}


static rc_t read_key_and_len( struct lookup_reader * reader, uint64_t pos, uint64_t *key, size_t *len )
{
    size_t num_read;
    char buffer[ 10 ];
    rc_t rc = KFileRead( reader->f, pos, buffer, sizeof buffer, &num_read );
    if ( rc != 0 )
        ErrMsg( "read_key_and_len.KFileRead( at %ld, to_read %u ) -> %R", pos, sizeof buffer, rc );
    else if ( num_read != sizeof buffer )
        rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
    else
    {
        uint16_t dna_len;
        size_t packed_len;
        memmove( key, buffer, sizeof *key );
        dna_len = buffer[ 8 ];
        dna_len <<= 8;
        dna_len |= buffer[ 9 ];
        packed_len = ( dna_len & 1 ) ? ( dna_len + 1 ) >> 1 : dna_len >> 1;
        *len = ( ( sizeof *key ) + ( sizeof dna_len ) + packed_len );
    }
    return rc;
}


static bool keys_equal( uint64_t key1, uint64_t key2 )
{
    bool res = ( key1 == key2 );
    if ( !res )
        res = ( ( ( key1 & 0x01 ) == 0 ) && key2 == ( key1 + 1 ) );
    return res;
}

static rc_t loop_until_key_found( struct lookup_reader * reader, uint64_t key_to_find,
        uint64_t *key_found , uint64_t *offset )
{
    rc_t rc = 0;
    bool done = false;
    uint64_t curr = *offset;
    while ( !done && rc == 0 )
    {
        size_t found_len;
        rc = read_key_and_len( reader, curr, key_found, &found_len );
        if ( keys_equal( key_to_find, *key_found ) )
        {
            done = true;
            *offset = curr;
        }
        else if ( key_to_find > *key_found )
            curr += found_len;
        else
        {
            done = true;
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
        }
    }
    return rc;
}


rc_t seek_lookup_reader( struct lookup_reader * reader, uint64_t key_to_find, uint64_t * key_found, bool exactly )
{
    rc_t rc = 0;
    uint64_t offset = 0;
    if ( reader == NULL || key_found == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "seek_lookup_reader() -> %R", rc );
    }
    else if ( reader->index != NULL )
    {
        /* we have a index! find set pos to the found offset */
        uint64_t max_key;
        rc = get_max_key( reader->index, &max_key );
        if ( rc == 0 )
        {
            if ( key_to_find > max_key )
                rc = RC( rcVDB, rcNoTarg, rcReading, rcId, rcTooBig );
            else
            {
                rc = get_nearest_offset( reader->index, key_to_find, key_found, &offset ); /* in index.c */
                if ( rc == 0 )
                {
                    if ( keys_equal( key_to_find, *key_found ) )
                        reader->pos = offset;
                    else
                    {
                        if ( exactly )
                        {
                            rc = loop_until_key_found( reader, key_to_find, key_found, &offset );
                            if ( rc == 0 )
                            {
                                if ( keys_equal( key_to_find, *key_found ) )
                                    reader->pos = offset;
                                else
                                {
                                    rc = RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
                                    ErrMsg( "seek_lookup_reader( key: %ld ) -> %R", key_to_find, rc );
                                }
                            }
                        }
                        else
                            reader->pos = offset;
                    }
                }
            }
        }
    }
    else
    {
        rc = loop_until_key_found( reader, key_to_find, key_found, &offset );
        if ( rc == 0 )
        {
            if ( keys_equal( key_to_find, *key_found ) )
                reader->pos = offset;
            else
            {
                rc = RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
                ErrMsg( "seek_lookup_reader( key: %ld ) -> %R", key_to_find, rc );
            }
        }
    }
    return rc;
}


rc_t get_packed_and_key_from_lookup_reader( struct lookup_reader * reader,
                        uint64_t * key, SBuffer * packed_bases )
{
    rc_t rc;
    if ( reader == NULL || key == NULL || packed_bases == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "get_packed_and_key_from_lookup_reader() -> %R", rc );
    }
    else
    {
        size_t num_read;
        char buffer1[ 10 ];
        rc = KFileRead( reader->f, reader->pos, buffer1, sizeof buffer1, &num_read );
        if ( rc != 0 )
            ErrMsg( "KFileRead( at %ld, to_read %u ) -> %R", reader->pos, sizeof buffer1, rc );
        else if ( num_read != sizeof buffer1 )
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
        else
        {
            uint16_t dna_len;
            size_t to_read;
            char * dst = ( char * )packed_bases->S.addr;
            
            memmove( key, buffer1, sizeof *key );

            dna_len = buffer1[ 8 ];
            dna_len <<= 8;
            dna_len |= buffer1[ 9 ];
            dst[ 0 ] = buffer1[ 8 ];
            dst[ 1 ] = buffer1[ 9 ];
            dst += 2;
            to_read = ( dna_len & 1 ) ? ( dna_len + 1 ) >> 1 : dna_len >> 1;
            if ( to_read > ( packed_bases->buffer_size - 2 ) )
                to_read = ( packed_bases->buffer_size - 2 );
            if ( rc == 0 )
            {
                rc = KFileRead( reader->f, reader->pos + 10, dst, to_read, &num_read );
                if ( rc != 0 )
                    ErrMsg( "KFileRead( at %ld, to_read %u ) -> %R", reader->pos + 10, to_read, rc );
                else if ( num_read != to_read )
                {
                    rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
                    ErrMsg( "KFileRead( %ld ) %d vs %d -> %R", reader->pos + 10, num_read, to_read, rc );
                }
                else
                {
                    packed_bases->S.len = packed_bases->S.size = num_read + 2;
                    reader->pos += ( num_read + 10 );
                }
            }
        }
    }
    return rc;
}

rc_t get_packed_from_lookup_reader( struct lookup_reader * reader,
                        int64_t * seq_spot_id, uint32_t * seq_read_id, SBuffer * packed_bases )
{
    uint64_t key;
    rc_t rc = get_packed_and_key_from_lookup_reader( reader, &key, packed_bases );
    if ( rc == 0 )
    {
        *seq_spot_id = key >> 1;
        *seq_read_id = key & 1 ? 2 : 1;
    }
    return rc;
}


rc_t get_bases_from_lookup_reader( struct lookup_reader * reader,
                        int64_t * seq_spot_id, uint32_t * seq_read_id, SBuffer * bases )
{
    rc_t rc = get_packed_from_lookup_reader( reader, seq_spot_id, seq_read_id, &reader->buf );
    if ( rc == 0 )
        unpack_4na( &reader->buf.S, bases );
    return rc;
}


rc_t lookup_bases( struct lookup_reader * lookup, int64_t row_id, uint32_t read_id, SBuffer * B )
{
    int64_t found_seq_spot_id;
    uint32_t found_seq_read_id;
    rc_t rc = get_bases_from_lookup_reader( lookup, &found_seq_spot_id, &found_seq_read_id, B );
    if ( rc == 0 )
    {
        if ( found_seq_spot_id != row_id || found_seq_read_id != read_id )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
            ErrMsg( "id-mismatch for seq_id = %lu vs. %lu / read_id = %u vs %lu",
                    found_seq_spot_id, row_id, found_seq_read_id, read_id );
        }
    }
    return rc;
}
