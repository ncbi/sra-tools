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

#include "lookup_writer.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_sbuffer_
#include "sbuffer.h"
#endif

#ifndef _h_helper_
#include "helper.h"   /* make_key() */
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_kfs_buffile_
#include <kfs/buffile.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

typedef struct lookup_writer_t {
    struct KFile * f;
    struct index_writer_t * idx;
    SBuffer_t buf;
    uint64_t pos;
} lookup_writer_t;

void release_lookup_writer( struct lookup_writer_t * writer ) {
    if ( NULL != writer ) {
        if ( NULL != writer -> f ) {
            ft_release_file( writer -> f, "release_lookup_writer()" );
        }
        release_SBuffer( &( writer -> buf ) );
        free( ( void * ) writer );
    }
}

static rc_t make_lookup_writer_obj( struct lookup_writer_t ** writer,
                             struct index_writer_t * idx,
                             struct KFile * f ) {
    rc_t rc = 0;
    lookup_writer_t * w = calloc( 1, sizeof * w );
    if ( NULL == w ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "release_lookup_writer().calloc( %d ) -> %R", ( sizeof * w ), rc );
    } else {
        w -> f = f;
        w -> idx = idx;
        rc = make_SBuffer( &( w -> buf ), 4096 );
        if ( 0 == rc ) {
            *writer = w;
        } else {
            release_lookup_writer( w );
        }
    }
    return rc;
}

rc_t make_lookup_writer( KDirectory *dir, struct index_writer_t * idx,
                         struct lookup_writer_t ** writer, size_t buf_size,
                         const char * fmt, ... ) {
    rc_t rc;
    struct KFile * f;

    va_list args;
    va_start ( args, fmt );

    rc = KDirectoryVCreateFile( dir, &f, false, 0664, kcmInit, fmt, args );
    if ( 0 != rc ) {
        ErrMsg( "make_lookup_writer().KDirectoryVCreateFile() -> %R", rc );
    } else {
        if ( buf_size > 0 ) {
            struct KFile * temp_file;
            rc = KBufFileMakeWrite( &temp_file, f, false, buf_size );
            if ( 0 != rc ) {
                ErrMsg( "make_lookup_writer().KBufFileMakeWrite() -> %R", rc );
            } else {
                ft_release_file( f, "make_lookup_writer().1" );
                f = temp_file;
            }
        }
        if ( 0 == rc ) {
            rc = make_lookup_writer_obj( writer, idx, f );
            if ( 0 != rc ) {
                ft_release_file( f, "make_lookup_writer().2" );
            }
        }
    }
    va_end ( args );
    return rc;
}

rc_t write_packed_to_lookup_writer( struct lookup_writer_t * writer,
                                    const uint64_t key,
                                    const String * bases_as_packed_4na ) {
    size_t num_writ;
    /* first write the key ( combination of seq-id and read-id ) */
    if ( key > 34000000 ) { KOutMsg( "\n!!!WP2LW key:%lu \n", key ); }
    rc_t rc = KFileWriteAll( writer -> f, writer -> pos, &key, sizeof key, &num_writ );
    if ( 0 != rc ) {
        ErrMsg( "write_packed_to_lookup_writer().KFileWriteAll( key ) -> %R", rc );
    }
    else if ( num_writ != sizeof key ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
        ErrMsg( "write_packed_to_lookup_writer().KFileWriteAll( key ) -> %R", rc );
    } else {
        uint64_t start_pos = writer -> pos; /* store the pos to be written later to the index... */

        writer -> pos += num_writ;
        /* now write the packed 4na ( length + packed data ) */
        rc = KFileWriteAll( writer -> f,
                            writer -> pos,
                            bases_as_packed_4na -> addr,
                            bases_as_packed_4na -> size,
                            &num_writ );
        if ( 0 != rc ) {
            ErrMsg( "write_packed_to_lookup_writer().KFileWriteAll( bases ) -> %R", rc );
        } else if ( num_writ != bases_as_packed_4na -> size ) {
            rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
            ErrMsg( "write_packed_to_lookup_writer().KFileWriteAll( bases ) -> %R", rc );
        } else {
            if ( NULL != writer -> idx ) {
                rc = write_key( writer -> idx, key, start_pos );
            }
            writer -> pos += num_writ;
        }
    }
    return rc;
}

static rc_t pack_4na( const String * unpacked, SBuffer_t * packed ) {
    rc_t rc = 0;
    if ( unpacked -> len < 1 ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcNull );
    } else {
        if ( unpacked -> len > 0xFFFF ) {
            rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcExcessive );
        } else {
            uint32_t i;
            uint8_t * src = ( uint8_t * )unpacked -> addr;
            uint8_t * dst = ( uint8_t * )packed -> S . addr;
            uint16_t dna_len = ( unpacked -> len & 0xFFFF );
            uint32_t len = 0;
            dst[ len++ ] = ( dna_len >> 8 );
            dst[ len++ ] = ( dna_len & 0xFF );
            for ( i = 0; i < unpacked -> len; ++i ) {
                if ( len < packed -> buffer_size ) {
                    uint8_t base = ( src[ i ] & 0x0F );
                    if ( 0 == ( i & 0x01 ) ) {
                        dst[ len ] = ( base << 4 );
                    } else {
                        dst[ len++ ] |= base;
                    }
                }
            }
            if ( unpacked -> len & 0x01 ) {
                len++;
            }
            packed -> S . size = packed -> S . len = len;
        }
    }
    return rc;
}

rc_t write_unpacked_to_lookup_writer( struct lookup_writer_t * writer,
                                      int64_t seq_spot_id,
                                      uint32_t seq_read_id,
                                      const String * bases_as_unpacked_4na ) {
    uint64_t key = hlp_make_key( seq_spot_id, seq_read_id ); /* helper.c */
    rc_t rc = pack_4na( bases_as_unpacked_4na, &writer -> buf ); /* helper.c */
    if ( 0 != rc ) {
        ErrMsg( "write_unpacked_to_lookup_writer().pack4na -> %R", rc );
    } else {
        rc = write_packed_to_lookup_writer( writer, key, &writer -> buf . S );
    }
    return rc;
}
