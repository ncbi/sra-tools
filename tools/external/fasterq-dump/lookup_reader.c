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

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_file_printer_
#include "file_printer.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_kfs_buffile_
#include <kfs/buffile.h>
#endif

typedef struct lookup_reader_t {
    const struct KFile * f;
    const struct index_reader_t * index;
    SBuffer_t buf;
    uint64_t pos, f_size, max_key;
} lookup_reader_t;

void release_lookup_reader( struct lookup_reader_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> f ) {
            ft_release_file( self -> f, "release_lookup_reader()" );
        }
        release_SBuffer( &( self -> buf ) ); /* helper.c */
        free( ( void * ) self );
    }
}

static rc_t make_lookup_reader_obj( struct lookup_reader_t ** reader,
                                    const struct index_reader_t * index,
                                    const struct KFile * f ) {
    rc_t rc = 0;
    lookup_reader_t * r = calloc( 1, sizeof * r );
    if ( NULL == r ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_lookup_reader_obj().calloc( %d ) -> %R", ( sizeof * r ), rc );
    } else {
        r -> f = f;
        r -> index = index;
        rc = KFileSize( f, & r -> f_size );
        if ( 0 == rc ) {
            rc = make_SBuffer( &( r -> buf ), 4096 ); /* helper.c */
        } else {
            ErrMsg( "make_lookup_reader_obj().KFileSize() -> %R", rc );
        }

        if ( 0 == rc && NULL != index ) {
            rc = get_max_key( index, & r -> max_key ); /* index.c */
        }

        if ( 0 == rc ) {
            *reader = r;
        } else {
            release_lookup_reader( r );
        }
    }
    return rc;
}

rc_t make_lookup_reader( const KDirectory *dir, const struct index_reader_t * index,
                         struct lookup_reader_t ** reader, size_t buf_size, const char * fmt, ... ) {
    rc_t rc;
    const struct KFile * f = NULL;

    va_list args;
    va_start ( args, fmt );
    rc = KDirectoryVOpenFileRead( dir, &f, fmt, args );
    va_end ( args );

    if ( 0 != rc ) {
        ErrMsg( "make_lookup_reader().KDirectoryVOpenFileRead( '?' ) -> %R",  rc );
    } else {
        if ( buf_size > 0 ) {
            const struct KFile * temp_file = NULL;
            rc = KBufFileMakeRead( &temp_file, f, buf_size );
            if ( 0 != rc ) {
                ErrMsg( "make_lookup_reader().KBufFileMakeRead() -> %R", rc );
            } else {
                rc = ft_release_file( f, "make_lookup_reader()" );
                if ( 0 == rc ) { f = temp_file; }
            }
        }
        if ( 0 == rc ) {
            rc = make_lookup_reader_obj( reader, index, f );
        }
    }
    return rc;
}

static rc_t read_key_and_len( struct lookup_reader_t * self, uint64_t pos, uint64_t *key, size_t *len ) {
    size_t num_read;
    /*const size_t buffer_size = sizeof( *key ) + sizeof( dna_len_t );*/
    uint8_t buffer[ sizeof( *key ) + sizeof( dna_len_t ) ];
    rc_t rc = KFileReadAll( self -> f, pos, buffer, sizeof buffer, &num_read );
    if ( rc != 0 ) {
        ErrMsg( "read_key_and_len().KFileReadAll( at %ld, to_read %u ) -> %R", pos, sizeof buffer, rc );
    } else if ( num_read != sizeof buffer ) {
        if ( 0 == num_read ) {
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
        } else {
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
        }
    } else {
        dna_len_t dna_len;
        memmove( key, buffer, sizeof *key ); /* get the 8 bytes ( uint64_t ) out of the buffer */
        memmove( &dna_len, buffer + sizeof *key, sizeof dna_len );   /* get 2/4 of of the buffer */
        {
            // the dna_len encodes the number of bases ! NOT the number of bytes
            // adjust for odd vs. even number of bases to calculate the number of bytes to read
            size_t dna_bytes = ( dna_len & 1 ) ? ( dna_len + 1 ) >> 1 : dna_len >> 1;
            *len = ( ( sizeof *key ) + ( sizeof dna_len ) + dna_bytes );
        }
    }
    return rc;
}

static bool keys_equal( uint64_t key1, uint64_t key2 ) {
    bool res = ( key1 == key2 );
    if ( !res ) {
        res = ( ( 0 == ( key1 & 0x01 ) ) && key2 == ( key1 + 1 ) );
    }
    return res;
}

static rc_t loop_until_key_found( struct lookup_reader_t * self, uint64_t key_to_find,
        uint64_t *key_found , uint64_t *offset ) {
    rc_t rc = 0;
    bool done = false;
    uint64_t curr = *offset;
    while ( !done && 0 == rc ) {
        size_t found_len = 0;
        rc = read_key_and_len( self, curr, key_found, &found_len );
        if ( keys_equal( key_to_find, *key_found ) ) {
            done = true;
            *offset = curr;
        } else if ( key_to_find > *key_found ) {
            curr += found_len;
        } else {
            done = true;
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
        }
    }
    return rc;
}

static rc_t full_table_seek( struct lookup_reader_t * self, uint64_t key_to_find, uint64_t * key_found ) {
    /* we have no index! search the whole thing... */
    uint64_t offset = 0;
    rc_t rc = loop_until_key_found( self, key_to_find, key_found, &offset );
    if ( 0 == rc ) {
        if ( keys_equal( key_to_find, *key_found ) ) {
            self -> pos = offset;
        } else {
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
            ErrMsg( "lookup_reader.c full_table_seek( key: %ld ) -> %R", key_to_find, rc );
        }
    }
    return rc;
}

static rc_t indexed_seek( struct lookup_reader_t * self, uint64_t key_to_find, uint64_t * key_found, bool exactly ) {
    /* we have a index! find set pos to the found offset */
    rc_t rc;
    uint64_t offset = 0;
    if ( self -> max_key > 0 && ( key_to_find > self -> max_key ) ) {
        rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcTooBig );
        /* ErrMsg( "lookup_reader.c indexed_seek( key_to_find=%lu, max_key=%lu ) -> %R", key_to_find, self -> max_key, rc ); */
    } else {
        rc = get_nearest_offset( self -> index, key_to_find, key_found, &offset ); /* in index.c */
        if ( 0 == rc ) {
            if ( keys_equal( key_to_find, *key_found ) ) {
                self -> pos = offset;
            } else {
                if ( exactly ) {
                    rc = loop_until_key_found( self, key_to_find, key_found, &offset );
                    if ( 0 == rc ) {
                        if ( keys_equal( key_to_find, *key_found ) ) {
                            self -> pos = offset;
                        } else {
                            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
                        }
                    } else {
                        rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
                    }
                } else {
                    self -> pos = offset;
                    rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcId, rcNotFound );
                }
            }
        }
    }
    return rc;
}

rc_t seek_lookup_reader( struct lookup_reader_t * self, uint64_t key_to_find, uint64_t * key_found, bool exactly ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == key_found ) {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "lookup_reader.c seek_lookup_reader() -> %R", rc );
    } else {
        if ( NULL != self -> index ) {
            rc = indexed_seek( self, key_to_find, key_found, exactly );
            if ( 0 != rc ) {
                rc = full_table_seek( self, key_to_find, key_found );
            }
        } else {
            rc = full_table_seek( self, key_to_find, key_found );
        }
    }
    return rc;
}

rc_t lookup_reader_get( struct lookup_reader_t * self, uint64_t * key, SBuffer_t * packed_bases ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == key || NULL == packed_bases ) {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "lookup_reader_get() #invalid input# -> %R",  rc );
    } else {
        if ( self -> pos >= ( self -> f_size - 1 ) ) {
            rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
        } else {
            size_t num_read;
            const size_t buffer1_size = sizeof( *key ) + sizeof( dna_len_t );
            uint8_t buffer1[ sizeof( *key ) + sizeof( dna_len_t ) ];

            rc = KFileReadAll( self -> f, self -> pos, buffer1, buffer1_size, &num_read );
            if ( 0 != rc ) {
                /* we are not able to read buffer1_size bytes from the file */
                ErrMsg( "lookup_reader_get().KFileReadAll( at %ld, to_read %u ) -> %R", self -> pos, buffer1_size, rc );
            } else {
                if ( num_read != sizeof buffer1 ) {
                    rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
                    ErrMsg( "lookup_reader_get().KFileReadAll( at %ld, to_read %lu vs %lu )", self -> pos, sizeof buffer1, num_read );
                } else {
                    dna_len_t dna_bases;

                    /* we get the key out of buffer1 */
                    memcpy( key, buffer1, sizeof *key );
                    /* we get the dna-len out of buffer1 */
                    memcpy( &dna_bases, &( buffer1[ sizeof *key ] ), sizeof dna_bases );

                    /* adjust the number of bytes to read to the half of the dna_len */
                    if ( 0 == dna_bases ) {
                        rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
                        ErrMsg( "lookup_reader_get() to_read == 0 at %lu, key = %lu", self -> pos, *key );
                        packed_bases -> S . size = 0;
                        packed_bases -> S . len = 0;
                        self -> pos += ( sizeof ( *key ) + sizeof( dna_bases ) );
                    } else {
                        size_t dna_bytes = ( dna_bases / 2 );
                        if ( 1 == ( dna_bases & 1 ) ) { dna_bytes++; }
                        /* maybe we have to increase the size of the SBuffer, after seeing the real dna-length */
                        if ( packed_bases -> buffer_size < ( dna_bytes + sizeof dna_bases ) ) {
                            rc = increase_SBuffer_to( packed_bases, dna_bytes + dna_bases );
                        }
                        if ( 0 == rc ) {
                            uint8_t * dst = ( uint8_t * )( packed_bases -> S . addr );

                            /* we write the dna-base-count into the first 4 bytes of the destination */
                            memcpy( dst, &( buffer1[ sizeof *key ] ), sizeof dna_bases );
                            dst += sizeof dna_bases;

                            rc = KFileReadAll( self -> f,
                                                self -> pos + ( sizeof * key ) + sizeof( dna_bases ),
                                                dst,
                                                dna_bytes,
                                                &num_read );
                            if ( 0 != rc ) {
                                ErrMsg( "lookup_reader_get().KFileReadAll( at %ld, to_read %u ) -> %R",
                                        self -> pos + sizeof( *key ) + sizeof( dna_bases ) , dna_bytes, rc );
                            } else if ( num_read != dna_bytes ) {
                                rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
                                ErrMsg( "lookup_reader_get().KFileReadAll( %ld ) %d vs %d -> %R",
                                        self -> pos + sizeof( *key ) + sizeof( dna_bases ), num_read, dna_bases, rc );
                            } else {
                                packed_bases -> S . size = num_read + sizeof( dna_bases );
                                packed_bases -> S . len = ( uint32_t )packed_bases -> S . size;
                                self -> pos += ( sizeof( *key ) + sizeof( dna_bases ) + dna_bytes );
                            }
                        }
                    }
                }
            }
        }
    }
    return rc;
}

static const char x4na_to_ASCII_fwd[ 16 ] = {
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       'N', 'A', 'C', 'N', 'G', 'N', 'N', 'N', 'T', 'N', 'N', 'N', 'N', 'N', 'N', 'N'
};

static const char x4na_to_ASCII_rev[ 16 ] = {
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       'N', 'T', 'G', 'N', 'C', 'N', 'N', 'N', 'A', 'N', 'N', 'N', 'N', 'N', 'N', 'N'
};

static rc_t unpack_4na( const String * packed, SBuffer_t * unpacked, bool reverse ) {
    rc_t rc = 0;
    uint8_t * src = ( uint8_t * )packed -> addr;
    dna_len_t dna_len;

    /* the first 2 bytes are the 16-bit dna-length */
    memmove( &dna_len, src, sizeof( dna_len_t ) );

    if ( dna_len > unpacked -> buffer_size ) {
        rc = increase_SBuffer_to( unpacked, dna_len + 4 );
        if ( 0 != rc ) {
            ErrMsg( "lookup_reader_get().unpack_4na() -> %R failed to increase buffer", rc );
        }
    }
    if ( 0 == rc ) {
        if ( 0 == dna_len ) {
            unpacked -> S . size = 0;
            unpacked -> S . len = 0;
        } else {
            uint8_t * dst = ( uint8_t * )unpacked -> S . addr;
            int32_t dst_idx;
            uint32_t src_idx;

            /* use the complement-lookup-table in case of reverse */
            const char * lookup = reverse ? x4na_to_ASCII_rev : x4na_to_ASCII_fwd;

            dst_idx = reverse ? dna_len - 1 : 0;

            for ( src_idx = sizeof( dna_len_t ); dst_idx >= 0 && src_idx < packed -> len; ++src_idx ) {
                /* get the packed byte out of the packed input */
                uint8_t packed_byte = src[ src_idx ];

                /* write the first unpacked byte */
                if ( dst_idx < unpacked -> buffer_size ) {
                    dst[ dst_idx ] = lookup[ ( packed_byte >> 4 ) & 0x0F ];
                    dst_idx += reverse ? -1 : 1;
                }

                /* write the second unpacked byte */
                if ( dst_idx < unpacked -> buffer_size ) {
                    dst[ dst_idx ] = lookup[ packed_byte & 0x0F ];
                    dst_idx += reverse ? -1 : 1;
                }
            }

            /* set the dna-length in the output-string */
            unpacked -> S . size = dna_len;
            unpacked -> S . len = ( uint32_t )unpacked -> S . size;

            /* terminated the output-string, just in case */
            dst[ dna_len ] = 0;
        }
    }
    return rc;
}

rc_t lookup_bases( struct lookup_reader_t * self, int64_t row_id, uint32_t read_id, SBuffer_t * B, bool reverse ) {
    int64_t found_row_id;
    uint32_t found_read_id;
    uint64_t key;
    rc_t rc = 0;
    if ( NULL == self || NULL == B ) {
        rc = RC( rcRuntime, rcData, rcAccessing, rcMemory, rcNull );
    } else if ( NULL == B -> S . addr ) {
        rc = RC( rcRuntime, rcData, rcAccessing, rcMemory, rcNull );
    }

    if ( 0 == rc ) {
        rc = lookup_reader_get( self, &key, &self -> buf );
        if ( 0 == rc ) {
            found_row_id = key >> 1;
            found_read_id = key & 1 ? 2 : 1;

            if ( found_row_id == row_id && found_read_id == read_id ) {
                rc = unpack_4na( &self -> buf . S, B, reverse ); /* helper.c */
            } else {
                /* in case the reader is not pointed to the right position, we try to seek again */
                rc_t rc1;
                uint64_t key_found;
                uint64_t key_to_find = row_id;

                key_to_find <<= 1;
                if ( 1 == read_id ) {
                    key_to_find &= 0xFFFFFFFFFFFFFFFE;
                } else {
                    key_to_find |= 1;
                }

                rc1 = seek_lookup_reader( self, key_to_find, &key_found, true );
                if ( 0 == rc1 ) {
                    rc = lookup_reader_get( self, &key, &self -> buf );
                    if ( 0 == rc ) {
                        found_row_id = key >> 1;
                        found_read_id = key & 1 ? 2 : 1;

                        if ( found_row_id == row_id && found_read_id == read_id ) {
                            rc = unpack_4na( &self -> buf . S, B, reverse ); /* helper.c */
                        } else {
                            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
                            ErrMsg( "lookup_bases #2( %lu.%u ) ---> found %lu.%u (at pos=%lu)",
                                        row_id, read_id, found_row_id, found_read_id, self -> pos );
                        }
                    }
                } else {
                    rc = rc1;
                    ErrMsg( "lookup_bases( %lu.%u ) ---> seek failed ---> %R", row_id, read_id, rc );
                }
            }
        } else {
            ErrMsg( "lookup_bases( %lu.%u ) failed ---> %R", row_id, read_id, rc );
        }
    } else {
        ErrMsg( "lookup_bases( %lu.%u ) failed ---> %R", row_id, read_id, rc );
    }
    return rc;
}

rc_t lookup_check( struct lookup_reader_t * self ) {
    rc_t rc = 0;
    int64_t last_key = 0;

    while ( 0 == rc && self -> pos < self -> f_size ) {
        uint64_t key;
        size_t len;
        rc = read_key_and_len( self, self -> pos, &key, &len );
        if ( 0 == rc ) {
            if ( last_key < key ) {
                last_key = key;
            } else {
                rc = SILENT_RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
                ErrMsg( "lookup_reader.c lookup_check() jump from %lu to %lu at %lu", last_key, key, self -> pos );
            }
            self -> pos += len;
        }
    }
    return rc;
}

rc_t lookup_check_file( const KDirectory *dir, size_t buf_size, const char * filename ) {
    lookup_reader_t * reader;
    rc_t rc = make_lookup_reader( dir, NULL, &reader, buf_size, "%s", filename );
    if ( 0 == rc ) {
        rc = lookup_check( reader );
        release_lookup_reader( reader );
    }
    return rc;
}

rc_t lookup_count( struct lookup_reader_t * self, uint32_t * count ) {
    rc_t rc = 0;
    int32_t n = 0;

    while ( 0 == rc && self -> pos < self -> f_size ) {
        uint64_t key;
        size_t len;
        rc = read_key_and_len( self, self -> pos, &key, &len );
        if ( 0 == rc ) {
            n++;
            self -> pos += len;
        }
    }
    *count = ( 0 == rc ) ? n : 0;
    return rc;
}

rc_t lookup_count_file( const KDirectory *dir, size_t buf_size, const char * filename, uint32_t * count ) {
    lookup_reader_t * reader;
    rc_t rc = make_lookup_reader( dir, NULL, &reader, buf_size, "%s", filename );
    if ( 0 == rc ) {
        rc = lookup_count( reader, count );
        release_lookup_reader( reader );
    }
    return rc;
}

rc_t write_out_lookup( const KDirectory *dir, size_t buf_size, const char * lookup_file, const char * output_file ) {
    lookup_reader_t * reader;
    rc_t rc = make_lookup_reader( dir, NULL, &reader, buf_size, "%s", lookup_file );
    if ( 0 == rc ) {
        struct file_printer_t * printer;
        rc = make_file_printer_from_filename( dir, &printer, buf_size, 1024, "%s", output_file );
        if ( 0 == rc ) {
            while ( 0 == rc && reader -> pos < reader -> f_size ) {
                uint64_t key;
                size_t len;
                rc = read_key_and_len( reader, reader -> pos, &key, &len );
                if ( 0 == rc ) {
                    rc = file_print( printer, "%lu\n", key );
                    reader -> pos += len;
                }
            }
            destroy_file_printer( printer );
        }
        release_lookup_reader( reader );
    }
    return rc;
}
