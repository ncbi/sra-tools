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

#include <kfs/file.h>
#include <kfs/buffile.h>

#include <string.h>

typedef struct lookup_reader
{
    const struct KFile * f;
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


rc_t make_lookup_reader( KDirectory *dir, struct lookup_reader ** reader,
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
            lookup_reader * r = calloc( 1, sizeof * r );
            if ( r == NULL )
            {
                KFileRelease( temp_file );
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "calloc( %d ) -> %R", ( sizeof * r ), rc );
            }
            else
            {
                rc = make_SBuffer( &r->buf, 4096 );
                if ( rc != 0 )
                    KFileRelease( temp_file );
                else
                {
                    r->f = temp_file;
                    *reader = r;
                }
            }
        }
    }
    va_end ( args );
    return rc;
}


rc_t get_packed_and_key_from_lookup_reader( struct lookup_reader * reader,
                        uint64_t * key, SBuffer * packed_bases )
{
    size_t num_read;
    char buffer1[ 10 ];
    rc_t rc = KFileRead( reader->f, reader->pos, buffer1, sizeof buffer1, &num_read );
    if ( rc != 0 )
        ErrMsg( "KFileRead( at %ld, to_read %u ) -> %R", reader->pos, sizeof buffer1, rc );
    else if ( num_read != sizeof buffer1 )
    {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
        /* ErrMsg( "KFileRead( %ld ) %d vs %d -> %R", reader->pos, num_read, sizeof buffer1, rc ); */
    }
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
