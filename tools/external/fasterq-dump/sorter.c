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

#include "sorter.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_raw_read_iter_
#include "raw_read_iter.h"
#endif

#ifndef _h_merge_sorter_
#include "merge_sorter.h"
#endif

#ifndef _h_progress_thread_
#include "progress_thread.h"
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

/*
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */
#include <atomic.h>

typedef struct lookup_producer_t {
    struct raw_read_iter_t * iter; /* raw_read_iter.h */
    KVector * store;
    struct bg_progress_t * progress; /* progress_thread.h */
    struct background_vector_merger_t * merger; /* merge_sorter.h */
    SBuffer_t buf; /* helper.h */
    uint64_t bytes_in_store;
    atomic64_t * processed_row_count;
    uint32_t chunk_id, sub_file_id;
    size_t buf_size, mem_limit;
    bool single;
} lookup_producer_t;


static void release_producer( lookup_producer_t * self ) {
    if ( NULL != self ) {
        release_SBuffer( &( self -> buf ) ); /* helper.c */
        if ( NULL != self -> iter ) {
            destroy_raw_read_iter( self -> iter ); /* raw_read_iter.c */
        }
        if ( NULL != self -> store ) {
            KVectorRelease( self -> store );
        }
        free( ( void * ) self );
    }
}

static rc_t push_store_to_merger( lookup_producer_t * self, bool last ) {
    rc_t rc = 0;
    if ( self -> bytes_in_store > 0 ) {
        rc = push_to_background_vector_merger( self -> merger, self -> store ); /* this might block! merge_sorter.c */
        if ( 0 == rc ) {
            self -> store = NULL;
            self -> bytes_in_store = 0;
            if ( !last ) {
                rc = KVectorMake( &self -> store );
                if ( 0 != rc ) {
                    ErrMsg( "sorter.c push_store_to_merger().KVectorMake() -> %R", rc );
                }
            }
        }
    }
    return rc;
}

static const char xASCII_to_4na[ 256 ] = {
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x10 0x11 0x12 0x13 0x14 0x15 0x16 0x17 0x18 0x19 0x1A 0x1B 0x1C 0x1D 0x1E 0x1F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x2A 0x2B 0x2C 0x2D 0x2E 0x2F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x30 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38 0x39 0x3A 0x3B 0x3C 0x3D 0x3E 0x3F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x40 0x41 0x42 0x43 0x44 0x45 0x46 0x47 0x48 0x49 0x4A 0x4B 0x4C 0x4D 0x4E 0x4F */
    /* @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O */
       0,   1,   0,   2,   0,   0,   0,   4,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x50 0x51 0x52 0x53 0x54 0x55 0x56 0x57 0x58 0x59 0x5A 0x5B 0x5C 0x5D 0x5E 0x5F */
    /* P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _ */
       0,   0,   0,   0,   8,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x60 0x61 0x62 0x63 0x64 0x65 0x66 0x67 0x68 0x69 0x6A 0x6B 0x6C 0x6D 0x6E 0x6F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x70 0x71 0x72 0x73 0x74 0x75 0x76 0x77 0x78 0x79 0x7A 0x7B 0x7C 0x7D 0x7E 0x7F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x80 0x81 0x82 0x83 0x84 0x85 0x86 0x87 0x88 0x89 0x8A 0x8B 0x8C 0x8D 0x8E 0x8F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x90 0x91 0x92 0x93 0x94 0x95 0x96 0x97 0x98 0x99 0x9A 0x9B 0x9C 0x9D 0x9E 0x9F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xA0 0xA1 0xA2 0xA3 0xA4 0xA5 0xA6 0xA7 0xA8 0xA9 0xAA 0xAB 0xAC 0xAD 0xAE 0xAF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xB0 0xB1 0xB2 0xB3 0xB4 0xB5 0xB6 0xB7 0xB8 0xB9 0xBA 0xBB 0xBC 0xBD 0xBE 0xBF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xC0 0xC1 0xC2 0xC3 0xC4 0xC5 0xC6 0xC7 0xC8 0xC9 0xCA 0xCB 0xCC 0xCD 0xCE 0xCF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xD0 0xD1 0xD2 0xD3 0xD4 0xD5 0xD6 0xD7 0xD8 0xD9 0xDA 0xDB 0xDC 0xDD 0xDE 0xDF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xE0 0xE1 0xE2 0xE3 0xE4 0xE5 0xE6 0xE7 0xE8 0xE9 0xEA 0xEB 0xEC 0xED 0xEE 0xEF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xF0 0xF1 0xF2 0xF3 0xF4 0xF5 0xF6 0xF7 0xF8 0xF9 0xFA 0xFB 0xFC 0xFD 0xFE 0xFF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};


static rc_t pack_read_2_4na( const String * bases, SBuffer_t * packed_bases ) {
    rc_t rc = 0;
    if ( bases -> len < 1 ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcNull );
    } else {
        if ( bases -> len > 0xFFFF ) {
            /* make sure that we have no more than max u16 bases, because we only use 2 bytes
               for that in the lookup-file! */
            rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcExcessive );
        } else {
            /* we have to down-convert from 32-bits to 16-bits */
            const uint16_t num_bases = ( bases -> len & 0xFFFF );

             /* 2 bases per byte + 2 bytes for num_bases + 2 bytes extra */
            const uint16_t buffer_bytes_needed = ( num_bases / 2 ) + 4;

             /* enlarge the buffer if needed */
            if ( packed_bases -> buffer_size < buffer_bytes_needed ) {
                rc = increase_SBuffer_to( packed_bases, buffer_bytes_needed );
                if ( 0 != rc ) {
                    ErrMsg( "sorter.c pack_read_2_4na() cannot increase buffer from %u to %u",
                            packed_bases -> buffer_size, buffer_bytes_needed );
                }
            }

            if ( 0 == rc ) {
                uint32_t src_idx = 0;
                uint32_t dst_idx = 0;
                const uint8_t * src = ( uint8_t * )bases -> addr;
                uint8_t * dst = ( uint8_t * )packed_bases -> S . addr;

                /* write num_bases to the target */
                dst[ dst_idx++ ] = ( num_bases >> 8 );
                dst[ dst_idx++ ] = ( num_bases & 0xFF );

                /* for each base: encode to 4na and write ot buffer */
                for ( src_idx = 0; src_idx < num_bases; ++src_idx ) {
                    if ( dst_idx < packed_bases -> buffer_size ) {
                        uint8_t base = ( xASCII_to_4na[ src[ src_idx ] ] & 0x0F );
                        if ( 0 == ( src_idx & 0x01 ) ) {
                            dst[ dst_idx ] = ( base << 4 );
                        } else {
                            dst[ dst_idx++ ] |= base;
                        }
                    }
                }
                /* if we have not finished a whole byte - increase the lenght! */
                if ( bases -> len & 0x01 ) { dst_idx++; }

                /* set the length into the String... */
                packed_bases -> S . size = packed_bases -> S . len = dst_idx;
            }
        }
    }
    return rc;
}

static rc_t write_to_store( lookup_producer_t * self,
                            uint64_t key,
                            const String * read ) {
    /* we write it to the store...*/
    rc_t rc = pack_read_2_4na( read, &( self -> buf ) ); /* helper.c */
    if ( 0 != rc ) {
        ErrMsg( "sorter.c write_to_store().pack_read_2_4na() failed %R", rc );
    } else {
        const String * to_store;
        rc = StringCopy( &to_store, &( self -> buf . S ) );
        if ( 0 != rc ) {
            ErrMsg( "sorter.c write_to_store().StringCopy() -> %R", rc );
        } else {
            rc = KVectorSetPtr( self -> store, key, ( const void * )to_store );
            if ( 0 != rc ) {
                ErrMsg( "sorter.c write_to_store().KVectorSetPtr() -> %R", rc );
            } else {
                size_t item_size = ( sizeof key ) + ( sizeof *to_store ) + to_store -> size;
                self -> bytes_in_store += item_size;
            }
        }

        if ( 0 == rc &&
             self -> mem_limit > 0 &&
             self -> bytes_in_store >= self -> mem_limit ) {
                rc = push_store_to_merger( self, false ); /* this might block ! */
        }
    }
    return rc;
}

static rc_t CC producer_thread_func( const KThread *self, void *data ) {
    rc_t rc1, rc = 0;
    lookup_producer_t * producer = data;
    raw_read_rec_t rec;
    uint64_t row_count = 0;

    while ( 0 == rc && get_from_raw_read_iter( producer -> iter, &rec, &rc1 ) ) { /* raw_read_iter.c */
        rc_t rc2 = hlp_get_quitting(); /* helper.c */
        if ( 0 == rc2 ) {
            if ( 0 == rc1 ) {
                if ( rec . read . len < 1 ) {
                    rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcNull );
                    ErrMsg( "sorter.c producer_thread_func: rec.read.len = %d", rec . read . len );
                } else {
                    uint64_t key = hlp_make_key( rec . seq_spot_id, rec . seq_read_id ); /* helper.c */
                    /* the keys are allowed to be out of order here */
                    rc = write_to_store( producer, key, &rec . read ); /* above! */
                    if ( 0 == rc ) {
                        bg_progress_inc( producer -> progress ); /* progress_thread.c (ignores NULL) */
                        row_count++;
                    }
                }
            } else {
                ErrMsg( "sorter.c get_from_raw_read_iter( %lu ) -> %R", rec . seq_spot_id, rc1 );
                rc = rc1;
            }
        } else {
            rc = rc2;
        }
    }
    if ( 0 == rc ) {
        /* now we have to push out / write out what is left in the last store */
        rc = push_store_to_merger( producer, true ); /* this might block ! */
    } else {
        hlp_set_quitting(); /* helper.c */
    }

    if ( 0 == rc && 0 != producer -> processed_row_count ) {
        atomic64_read_and_add( producer -> processed_row_count, row_count );
    }
    release_producer( producer ); /* above */

    return rc;
}

/* -------------------------------------------------------------------------------------------- */

rc_t execute_lookup_production( const lookup_production_args_t * args ) {
    rc_t rc = 0;

    if ( args -> show_progress ) {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "lookup :" );
        KOutHandlerSetStdOut();
    }

    tell_total_rowcount_to_vector_merger( args -> merger, args -> align_row_count ); /* merge_sorter.h */

    if ( rc == 0 ) {
        Vector threads;
        uint32_t chunk_id = 1;
        int64_t row = 1;
        struct bg_progress_t * progress = NULL; /* progress_thread.h */
        atomic64_t processed_row_count;
        uint64_t rows_per_thread = ( args -> align_row_count / args -> num_threads ) + 1;

        atomic64_set( &processed_row_count, 0 );
        VectorInit( &threads, 0, args -> num_threads );
        if ( args -> show_progress ) {
            rc = bg_progress_make( &progress, args -> align_row_count, 0, 0 ); /* progress_thread.c */
        }

        while ( 0 == rc && ( row < ( int64_t )args -> align_row_count ) ) {
            lookup_producer_t * producer = calloc( 1, sizeof *producer );
            if ( NULL != producer ) {

                /* initialize the producer */
                rc = KVectorMake( &producer -> store );
                if ( 0 != rc ) {
                    ErrMsg( "sorter.c init_multi_producer().KVectorMake() -> %R", rc );
                } else {
                    rc = make_SBuffer( &( producer -> buf ), 4096 ); /* helper.c */
                    if ( 0 == rc ) {
                        cmn_iter_params_t cip;   /* cm_iter.h */

                        producer -> iter            = NULL;
                        producer -> progress        = progress;
                        producer -> merger          = args -> merger;
                        producer -> bytes_in_store  = 0;
                        producer -> chunk_id        = chunk_id;
                        producer -> sub_file_id     = 0;
                        producer -> buf_size        = args -> buf_size;
                        producer -> mem_limit       = args -> mem_limit;
                        producer -> single          = false;
                        producer -> processed_row_count = &processed_row_count;

                        cip . dir                = args -> dir;
                        cip . vdb_mgr            = args -> vdb_mgr;
                        cip . accession_short    = args -> accession_short;
                        cip . accession_path     = args -> accession_path;
                        cip . first_row          = row;
                        cip . row_count          = rows_per_thread;
                        cip . cursor_cache       = args -> cursor_cache;

                        rc = make_raw_read_iter( &cip, &( producer -> iter ) );
                    }
                }

                if ( 0 == rc ) {
                    KThread * thread;
                    rc = hlp_make_thread( &thread, producer_thread_func, producer, THREAD_BIG_STACK_SIZE );
                    if ( 0 != rc ) {
                        ErrMsg( "sorter.c run_producer_pool().helper_make_thread( sort-thread #%d ) -> %R", chunk_id - 1, rc );
                    } else {
                        rc = VectorAppend( &threads, NULL, thread );
                        if ( 0 != rc ) {
                            ErrMsg( "sorter.c run_producer_pool().VectorAppend( sort-thread #%d ) -> %R", chunk_id - 1, rc );
                        } else {
                            row  += rows_per_thread;
                            chunk_id++;
                        }
                    }
                } else {
                    release_producer( producer ); /* above */
                }
            } else {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            }
        }

        /* collect all the sorter-threads */
        rc = hlp_join_and_release_threads( &threads );
        if ( 0 != rc ) {
            ErrMsg( "sorter.c run_producer_pool().join_and_release_threads -> %R", rc );
        }

        /* all sorter-threads are done now, tell the progress-thread to terminate! */
        bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/

        if ( 0 == rc ) {
            uint64_t value = atomic64_read( &processed_row_count );
            if ( value != args -> align_row_count ) {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSize, rcInvalid );
                ErrMsg( "sorter.c run_producer_pool() : processed lookup rows: %lu of %lu", value, args -> align_row_count );
            }
        }
    }

    /* signal to the receiver-end of the job-queue that nothing will be put into the
       queue any more... */
    if ( rc == 0 && args -> merger != NULL ) {
        rc = seal_background_vector_merger( args -> merger ); /* merge_sorter.c */
    }

    if ( rc != 0 ) {
        ErrMsg( "sorter.c execute_lookup_production() -> %R", rc );
    }
    return rc;
}
