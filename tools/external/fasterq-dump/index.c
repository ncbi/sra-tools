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
#include "kfs/directory.h"
#include <stdint.h>

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_kfs_buffile_
#include <kfs/buffile.h>
#endif

typedef struct index_writer_t {
    struct KFile * f;
    uint64_t frequency, pos, last_key;
} index_writer_t;

void release_index_writer( struct index_writer_t * writer ) {
    if ( NULL != writer ) {
        if ( NULL != writer -> f ) {
            ft_release_file( writer -> f, "release_index_writer()" );
        }
        free( ( void * ) writer );
    }
}

static rc_t write_value( index_writer_t * writer, uint64_t value ) {
    rc_t rc = KFileWriteExactly( writer -> f, writer -> pos, &value, sizeof value );
    if ( 0 != rc ) {
        ErrMsg( "write_value().KFileWriteExactly( key ) -> %R", rc );
    } else {
        writer -> pos += sizeof value;
    }
    return rc;
}

static rc_t write_key_and_offset( index_writer_t * writer, uint64_t key, uint64_t offset ) {
    rc_t rc = write_value( writer, key );
    if ( 0 == rc ) {
        rc = write_value( writer, offset );
    }
    return rc;
}

rc_t write_key( struct index_writer_t * writer, uint64_t key, uint64_t offset ) {
    rc_t rc = 0;
    if ( NULL == writer ) {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "write_key() -> %R", rc );
    } else {
        uint64_t last_plus_freq = writer -> last_key + writer -> frequency;
        if ( key > last_plus_freq ) {
            rc = write_key_and_offset( writer, key, offset );
            if ( 0 == rc ) {
                writer -> last_key = key;
            }
        }
    }
    return rc;
}

static rc_t make_index_writer_obj( struct index_writer_t ** writer,
                                   uint64_t frequency,
                                   struct KFile * f ) {
    rc_t rc = 0;
    index_writer_t * w = calloc( 1, sizeof * w );
    if ( NULL == w ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_index_writer_obj().calloc( %d ) -> %R", ( sizeof * w ), rc );
    } else {
        w -> f = f;
        w -> frequency = frequency;
        rc = write_value( w, frequency );
        if ( 0 == rc ) {
            rc = write_key_and_offset( w, 1, 0 );
        }

        if ( 0 == rc ) {
            *writer = w;
        } else {
            release_index_writer( w );
        }
    }
    return rc;
}

rc_t make_index_writer( KDirectory * dir, struct index_writer_t ** writer,
                        size_t buf_size, uint64_t frequency, const char * fmt, ... ) {
    rc_t rc;
    struct KFile * f;

    va_list args;
    va_start ( args, fmt );

    rc = KDirectoryVCreateFile( dir, &f, false, 0664, kcmInit, fmt, args );
    va_end ( args );

    if ( 0 != rc ) {
        ErrMsg( "make_index_writer().KDirectoryVCreateFile() -> %R", rc );
    } else {
        if ( buf_size > 0 ) {
            struct KFile * temp_file;
            rc = KBufFileMakeWrite( &temp_file, f, false, buf_size );
            if ( 0 != rc ) {
                ErrMsg( "make_index_writer().KBufFileMakeWrite() -> %R", rc );
            } else {
                ft_release_file( f, "make_index_writer().1" );
                f = temp_file;
            }
        }

        if ( 0 == rc ) {
            rc = make_index_writer_obj( writer, frequency, f );
            if ( 0 != rc ) {
                ft_release_file( f, "make_index_writer().2" );
            }
        }
    }
    return rc;
}

/* ----------------------------------------------------------------------- */

typedef struct index_reader_t {
    const struct KFile * f;
    uint64_t frequency, file_size, max_key;
} index_reader_t;

void release_index_reader( index_reader_t * reader ) {
    if ( NULL != reader ) {
        if ( NULL != reader -> f ) {
            ft_release_file( reader -> f, "make_index_reader()" );
        }
        free( ( void * ) reader );
    }
}

static rc_t read_value( const index_reader_t * reader, uint64_t pos, uint64_t * value ) {
    rc_t rc = KFileReadExactly( reader -> f, pos, ( void * )value, sizeof *value );
    if ( 0 != rc ) {
        ErrMsg( "read_value().KFileReadExactly( at %ld ) -> %R", pos, rc );
    }
    return rc;
}

typedef struct index_entry_t {
    uint64_t key;
    uint64_t offset;
} index_entry_t;

static rc_t read_entry( const index_reader_t * reader, uint64_t entry_idx,
                        index_entry_t * entry, uint32_t count ) {
    uint64_t pos = sizeof reader -> frequency;
    size_t to_read = ( sizeof *entry ) * count;
    pos += entry_idx * ( sizeof * entry );
    rc_t rc = KFileReadExactly( reader -> f, pos, ( void * )entry, to_read );
    if ( 0 != rc ) {
        ErrMsg( "read_entry().KFileReadExactly( at %ld, entry_idx = %ld ) -> %R",
                pos, entry_idx, rc );
    }
    return rc;
}

static rc_t make_index_reader_obj( index_reader_t ** reader,
                                   const struct KFile * f ) {
    rc_t rc = 0;
    index_reader_t * r = calloc( 1, sizeof * r );
    if ( NULL == r ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "index.c make_index_reader_obj().calloc( %d ) -> %R", ( sizeof * r ), rc );
    } else {
        r -> f = f;
        rc = read_value( r, 0, &r -> frequency );
        if ( 0 == rc ) {
            rc = KFileSize( f, &r -> file_size );
            if ( 0 != rc ) {
                ErrMsg( "index.c make_index_reader_obj().KFileSize() -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            get_max_key( r, &r -> max_key );
            *reader = r;
        } else {
            release_index_reader( r );
        }
    }
    return rc;
}

rc_t make_index_reader( const KDirectory * dir, index_reader_t ** reader,
                        size_t buf_size, const char * fmt, ... ) {
    rc_t rc;
    const struct KFile * f;

    va_list args;
    va_start ( args, fmt );

    rc = KDirectoryVOpenFileRead( dir, &f, fmt, args );
    va_end ( args );

    if ( 0 != rc ) {
        ErrMsg( "index.c make_index_reader() KDirectoryVOpenFileRead() -> %R", rc );
    } else {
        if ( buf_size > 0 ) {
            const struct KFile * temp_file;
            rc = KBufFileMakeRead( &temp_file, f, buf_size );
            if ( 0 != rc ) {
                ErrMsg( "index.c make_index_reader() KBufFileMakeRead() -> %R", rc );
            } else {
                rc = ft_release_file( f, "make_index_reader()" );
                f = temp_file;
            }
        }
        if ( 0 == rc ) {
            rc = make_index_reader_obj( reader, f );
        }
    }
    return rc;
}

/* =================================================================================================== */

static uint64_t index_entry_count( const index_reader_t * self ) {
    uint64_t res = self -> file_size - ( sizeof self -> frequency );
    res /= sizeof( index_entry_t );
    return res;
}

static rc_t nearest_offset_linear_search( const index_reader_t * self,
                         uint64_t key_to_find,
                         uint64_t * key_found,
                         uint64_t * offset,
                         uint64_t entry_count ) {
    index_entry_t entries[ 20 ];
    rc_t rc = read_entry( self, 0, entries, entry_count );
    if ( 0 == rc ) {
        uint64_t entry_idx = 0;
        bool found = false;
        while( !found && entry_idx < entry_count ) {
            bool is_lower  = key_to_find < entries[ entry_idx ] . key;
            bool is_higher = key_to_find > entries[ entry_idx ] . key;
            found = !( is_lower || is_higher );
            if ( found ) {
                *key_found = entries[ entry_idx ] . key;
                *offset = entries[ entry_idx ] . offset;
            }
            entry_idx++;
        }
        if ( !found ) {
            rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
            ErrMsg( "index.c nearest_offset_linear_search( key not found ) -> %R", rc );
        }
    }
    return rc;
}

static rc_t nearest_offset_binary_search( const index_reader_t * self,
                         uint64_t key_to_find,
                         uint64_t * key_found,
                         uint64_t * offset,
                         uint64_t entry_count ) {
    uint64_t lower_bound = 0;
    uint64_t upper_bound = entry_count - 1;
    index_entry_t entries[ 2 ];
    rc_t rc = 0;
    bool found = false;
    while ( 0 == rc && !found ) {
        uint64_t entry_idx = lower_bound + ( ( upper_bound - lower_bound ) / 2 );
        rc = read_entry( self, entry_idx, entries, 2 );
        if ( 0 == rc ) {
            bool is_lower  = key_to_find < entries[ 0 ] . key;
            bool is_higher = key_to_find > entries[ 1 ] . key;
            found = !( is_lower || is_higher );
            if ( found ) {
                /* make it easier for the caller which only loops forward to
                   find the exact key in the lookup-table */
                if ( key_to_find == entries[ 1 ] . key ) {
                    *key_found = entries[ 1 ] . key;
                    *offset = entries[ 1 ] . offset;
                } else {
                    *key_found = entries[ 0 ] . key;
                    *offset = entries[ 0 ] . offset;
                }
            } else {
                if ( is_lower ) {
                    upper_bound = entry_idx - 1;
                } else {
                    lower_bound = entry_idx + 1;
                }
            }
        }
    }
    return rc;
}

/* the caller is in lookup_reader.c indexed_seek() - it only searches forward from the
   key_found/offset position!!! */
rc_t get_nearest_offset( const index_reader_t * self,
                         uint64_t key_to_find,
                         uint64_t * key_found,
                         uint64_t * offset ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == key_found || NULL == offset ) {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "index.c get_nearest_offset() -> %R", rc );
    } else {
        uint64_t entry_count = index_entry_count( self );
        if ( 0 == entry_count ) {
            rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcTooShort );
            ErrMsg( "index.c get_nearest_offset( entry_count == 0 ) -> %R", rc );
        } else if ( key_to_find > self -> max_key ) {
            rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcIncorrect );
            ErrMsg( "index.c get_nearest_offset( key_to_find > max_key ) -> %R", rc );
        }
        else if ( entry_count <= 20 ) {
            rc = nearest_offset_linear_search( self, key_to_find, key_found, offset, entry_count );
        } else {
            rc = nearest_offset_binary_search( self, key_to_find, key_found, offset, entry_count );
        }
    }
    return rc;
}

/* =================================================================================================== */

rc_t get_max_key( const index_reader_t * self, uint64_t * max_key ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == max_key ) {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "index.c get_max_key() -> %R", rc );
    } else if ( self -> max_key > 0 ) {
        *max_key = self -> max_key;
    } else {
        uint64_t data[ 6 ];
        size_t to_read = sizeof data;
        uint64_t pos = 0;

        if ( self -> file_size >= to_read ) {
            pos = self -> file_size - to_read;
        } else {
            to_read = self -> file_size;
        }

        rc = KFileReadExactly( self -> f, pos, ( void * )&data[ 0 ], to_read );
        if ( 0 != rc ) {
            ErrMsg( "index.c get_max_key().KFileReadExactly( at %lu ) failed %R", pos, rc );
        } else {
            if ( self -> file_size >= to_read ) {
                *max_key = data[ 4 ];
            }
            else if ( self -> file_size == ( sizeof data[ 0 ] * 3 ) ) {
                *max_key = data[ 2 ];
            } else {
                rc = RC( rcVDB, rcNoTarg, rcReading, rcFormat, rcInvalid );
                ErrMsg( "index.c get_max_key() - index file has invalid size of %lu", self -> file_size );
            }
        }
    }
    return rc;
}
