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

#include "ref_inventory.h"

#ifndef _h_klib_container_
#include <klib/container.h>
#endif

#ifndef _h_cmn_iter_
#include "cmn_iter.h"
#endif

#ifndef _h_simple_fasta_iter_
#include "simple_fasta_iter.h"
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_data_buffer
#include <klib/data-buffer.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#ifndef _h_file_printer_
#include "file_printer.h"
#endif

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

/* ------------------------------------------------------------------------------------------------------------- */

/* describes one reference */
typedef struct ref_inventory_entry_t {
    SLNode next;
    const String * name;
    const String * seq_id;
    uint32_t first_row;
    uint32_t row_count;
    uint64_t base_count;
    uint64_t cmp_base_count;    
    bool circular;
    bool internal;
} ref_inventory_entry_t;

static void destroy_ref_inventory_entry( ref_inventory_entry_t * self ) {
    if ( NULL != self ) {
        StringWhack( self -> name );
        StringWhack( self -> seq_id );
        free( ( void * ) self );
    }
}

static ref_inventory_entry_t * make_ref_inventory_entry( const String * name, const String * seq_id,
                                     int64_t row, uint64_t base_count,
                                     uint64_t cmp_base_count, bool circular ) {
    rc_t rc = 0;
    ref_inventory_entry_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        /* we have to make copies of name/seq_id - because they are pointers into cursor-memory 
         which can be re-used any time */
        rc = StringCopy( &( res -> name ), name );
        if ( 0 == rc ) {
            rc = StringCopy( &( res -> seq_id ), seq_id );
        }
        if ( 0 == rc ) {
            res -> first_row = row;
            res -> row_count = 1;
            res -> base_count = base_count;
            res -> cmp_base_count = cmp_base_count;            
            res -> internal = false;
            res -> circular = circular;
        }
        if ( 0 != rc ) {
            destroy_ref_inventory_entry( res );
            res = NULL;
        }
    }
    return res;
}

static void ref_inventory_entry_add( ref_inventory_entry_t * self, uint32_t base_count, uint32_t cmp_bases ) {
    self -> row_count += 1;
    self -> base_count += base_count;
    self -> cmp_base_count += cmp_bases;
}

const String * ref_inventory_entry_name( const ref_inventory_entry_t * self ) {
    if ( NULL != self ) { return self -> name; }
    return NULL;
}

const String * ref_inventory_entry_seq_id( const ref_inventory_entry_t * self ) {
    if ( NULL != self ) { return self -> seq_id; }
    return NULL;
}

int64_t ref_inventory_entry_first_row( const ref_inventory_entry_t * self ) {
    if ( NULL != self ) { return self -> first_row; }
    return -1;
}

uint64_t ref_inventory_entry_row_count( const ref_inventory_entry_t * self ) {
    if ( NULL != self ) { return self -> row_count; }
    return 0;
}

bool ref_entry_inventory_is_internal( const ref_inventory_entry_t * self ) {
    if ( NULL != self ) { return self -> internal; }
    return false;
}

bool ref_inventory_entry_is_circular( const ref_inventory_entry_t * self ) {
    if ( NULL != self ) { return self -> circular; }
    return false;
}

void ref_inventory_entry_report( const ref_inventory_entry_t * self ) {
    if ( NULL != self ) {
        KOutMsg( "\nName     : %S",   self -> name );
        KOutMsg( "\nSeq-Id   : %S",   self -> seq_id );
        KOutMsg( "\n1st row  : %d",   self -> first_row );
        KOutMsg( "\nrows     : %d",   self -> row_count );
        KOutMsg( "\nbases    : %lu",  self -> base_count );
        KOutMsg( "\ncmpbases : %lu",  self -> cmp_base_count );        
        KOutMsg( "\ncircular : %s",   self -> circular ? "Y" : "N" );
        KOutMsg( "\ninternal : %s\n", self -> internal ? "Y" : "N" );
    }
}

/* ------------------------------------------------------------------------------------------------------------- */

typedef struct ref_inventory_reader_t {
    struct cmn_iter_t * cmn;
    uint32_t cur_idx_name;
    uint32_t cur_idx_seq_id;
    uint32_t cur_idx_spot_id;
    uint32_t cur_idx_spot_len;
    uint32_t cur_idx_cmp_read;    
    uint32_t cur_idx_circular;
} ref_inventory_reader_t;

typedef struct ref_inventory_rec_t {
    String name;
    String seq_id;
    String cmp_read;
    uint32_t spot_id;
    uint32_t spot_len;
    bool circular;
} ref_inventory_rec_t;

static void destroy_ref_inventory_reader( ref_inventory_reader_t * self ) {
    if ( NULL != self ) {
        destroy_cmn_iter( self -> cmn );
        free( ( void * ) self );
    }
}

static ref_inventory_reader_t * make_ref_inventory_reader( const cmn_iter_params_t * params ) {
    ref_inventory_reader_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        rc_t rc = make_cmn_iter( params, "REFERENCE", &( res -> cmn ) );
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( res -> cmn, "NAME", &( res -> cur_idx_name ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( res -> cmn, "SEQ_ID", &( res -> cur_idx_seq_id ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( res -> cmn, "CMP_READ", &( res -> cur_idx_cmp_read ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( res -> cmn, "SPOT_ID", &( res -> cur_idx_spot_id ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( res -> cmn, "SPOT_LEN", &( res -> cur_idx_spot_len ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( res -> cmn, "CIRCULAR", &( res -> cur_idx_circular ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_range( res -> cmn, res -> cur_idx_name );
        }
        if ( 0 != rc ) {
            destroy_ref_inventory_reader( res );
            res = NULL;
        }
    }
    return res;
}

static bool ref_inventory_reader_read( ref_inventory_reader_t * self, ref_inventory_rec_t * rec ) {
    rc_t rc2;
    bool res = cmn_iter_next( self -> cmn, &rc2 );
    if ( res ) {
        rc_t rc = cmn_read_String( self -> cmn, self -> cur_idx_name, &( rec -> name ) );
        if ( 0 == rc ) {
            rc = cmn_read_String( self -> cmn, self -> cur_idx_seq_id, &( rec -> seq_id ) );            
        }
        if ( 0 == rc ) {
            rc = cmn_read_String( self -> cmn, self -> cur_idx_cmp_read, &( rec -> cmp_read ) );
        }
        if ( 0 == rc ) {
            rc = cmn_read_uint32( self -> cmn, self -> cur_idx_spot_id, &( rec -> spot_id ) );
        }
        if ( 0 == rc ) {
            rc = cmn_read_uint32( self -> cmn, self -> cur_idx_spot_len, &( rec -> spot_len ) );
        }
        if ( 0 == rc ) {
            rc = cmn_read_bool( self -> cmn, self -> cur_idx_circular, &( rec -> circular ) );
        }
        res = ( 0 == rc );
    }
    return res;
}
/* ------------------------------------------------------------------------------------------------------------- */

typedef struct ref_inventory_t {
    cmn_iter_params_t iter_params;
    SLList list;
    uint32_t count;
} ref_inventory_t;

static void destroy_inventory_entry( SLNode *n, void *data ) {
    ref_inventory_entry_t * e = ( ref_inventory_entry_t * )( n );
    destroy_ref_inventory_entry( e );
}

void destroy_ref_inventory( ref_inventory_t * self ) {
    if ( NULL != self ) {
        SLListWhack ( &( self -> list ), destroy_inventory_entry, NULL );
        free( ( void * ) self );
    }
}

uint32_t ref_inventory_count( const ref_inventory_t * self ) {
    return NULL != self ? self -> count : 0;
}

typedef struct ref_inventory_find_ctx_t {
    const String * to_find;
    ref_inventory_entry_t * found;
} ref_inventory_find_ctx_t;


static bool check_ref_inventory_entry_for_name( SLNode * n, void * data ) {
    ref_inventory_entry_t * entry = ( ref_inventory_entry_t * )n;
    ref_inventory_find_ctx_t * ctx = ( ref_inventory_find_ctx_t * )data;
    bool res = ( 0 == StringCompare( entry -> name, ctx -> to_find ) );
    if ( res ) { ctx -> found = entry; }
    return res;
}

const ref_inventory_entry_t * get_ref_inventory_entry_by_name( ref_inventory_t * inventory,
                                                               const String * name ) {
    ref_inventory_entry_t * res = NULL;
    if ( NULL != inventory && NULL != name ) {
        ref_inventory_find_ctx_t ctx = { .to_find = name, .found = NULL };
        bool found = SLListDoUntil( &( inventory -> list ), check_ref_inventory_entry_for_name, &ctx );
        if ( found ) { res = ctx . found; }
    }
    return res;
}

static bool check_ref_inventory_entry_for_seq_id( SLNode * n, void * data ) {
    ref_inventory_entry_t * entry = ( ref_inventory_entry_t * )n;
    ref_inventory_find_ctx_t * ctx = ( ref_inventory_find_ctx_t * )data;
    bool res = ( 0 == StringCompare( entry -> seq_id, ctx -> to_find ) );
    if ( res ) { ctx -> found = entry; }
    return res;
}

const ref_inventory_entry_t * get_ref_inventory_entry_by_seq_id( ref_inventory_t * inventory,
                                                                 const String * seq_id ) {
    ref_inventory_entry_t * res = NULL;
    if ( NULL != inventory && NULL != seq_id ) {
        ref_inventory_find_ctx_t ctx = { .to_find = seq_id, .found = NULL };
        bool found = SLListDoUntil( &( inventory -> list ), check_ref_inventory_entry_for_seq_id, &ctx );
        if ( found ) { res = ctx . found; }
    }
    return res;
}

static void insert_ref_inventory_entry( ref_inventory_t * self, ref_inventory_entry_t * entry ) {
    entry -> internal = ( entry -> cmp_base_count > 0 );
    SLListPushTail( &( self -> list ), ( SLNode * )entry );
    self -> count += 1;
}

static void fill_ref_inventory( ref_inventory_t * self ) {
    ref_inventory_reader_t * reader = make_ref_inventory_reader( &( self -> iter_params ) );
    if ( NULL != reader ) {
        ref_inventory_rec_t rec;
        ref_inventory_entry_t * e = NULL;
        while ( ref_inventory_reader_read( reader, &rec ) ) {
            if ( NULL == e ) {
                e = make_ref_inventory_entry( &( rec . name ), &( rec . seq_id ), rec . spot_id,
                                    rec . spot_len, rec .cmp_read . len, rec . circular );
            } else {
                bool same_name = ( 0 == StringCompare( e -> name, &( rec . name ) ) );
                if ( same_name ) {
                    /* belongs to the same reference */
                    ref_inventory_entry_add( e, rec . spot_len, rec . cmp_read . len );
                } else {
                    /* is a new reference */
                    insert_ref_inventory_entry( self, e );
                    e = make_ref_inventory_entry( &( rec . name ), &( rec . seq_id ), rec . spot_id,
                                        rec . spot_len, rec . cmp_read . len, rec . circular );                    
                }
            }
        }
        /* insert the last one... */
        insert_ref_inventory_entry( self, e );
        destroy_ref_inventory_reader( reader );
    }
}

ref_inventory_t * make_ref_inventory( const tool_ctx_t * tool_ctx ) {
    ref_inventory_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        bool populated = tctx_populate_cmn_iter_params( tool_ctx, &( res -> iter_params ) );
        if ( !populated ) {
            destroy_ref_inventory( res );
            res = NULL;
        } else {
            SLListInit( &( res -> list ) );
            fill_ref_inventory( res );
        }
    }
    return res;
}

void ref_inventory_report( const ref_inventory_t * self ) {
    struct ref_inventory_iter_t * iter = make_ref_inventory_iter( self, NULL );
    if ( NULL != iter ) {
        bool running = true;
        while ( running ) {
            const ref_inventory_entry_t * e = ref_inventory_iter_next( iter );
            running = ( NULL != e );
            if ( running ) {
               ref_inventory_entry_report( e );
            }
        }
        destroy_ref_inventory_iter( iter );
    }
}

uint64_t ref_inventory_base_count( const ref_inventory_t * self ) {
    uint64_t res = 0;
    struct ref_inventory_iter_t * iter = make_ref_inventory_iter( self, NULL );
    if ( NULL != iter ) {
        bool running = true;
        while ( running ) {
            const ref_inventory_entry_t * e = ref_inventory_iter_next( iter );
            running = ( NULL != e );
            if ( running ) {
                res += e -> base_count;
            }
        }
        destroy_ref_inventory_iter( iter );
    }
    return res;
}

/* ------------------------------------------------------------------------------------------------------------- */
typedef struct ref_inventory_filter_t {
    ref_inventory_location_t location;
    VNamelist * names;
} ref_inventory_filter_t;

void destroy_ref_inventory_filter( ref_inventory_filter_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> names ) {
            VNamelistRelease( self -> names );
        }
        free( ( void * ) self );
    }
}

ref_inventory_filter_t * make_ref_inventory_filter( ref_inventory_location_t location,
                                                    const VNamelist * names ) {
    ref_inventory_filter_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        res -> location = location;
        res -> names = NULL;
        if ( NULL != names ) {
            uint32_t count = 0;
            rc_t rc = VNameListCount( names, &count );
            if ( 0 == rc && count > 0 ) {
                rc = CopyVNamelist( &( res -> names ), names );
                if ( 0 != rc ) {
                    res -> names = NULL;
                    destroy_ref_inventory_filter( res );
                    res = NULL;
                }
            }
        }
    }
    return res;
}

static void print_ref_inventory_filter( const ref_inventory_filter_t * self ) {
    if ( NULL != self ) {
        switch( self -> location ) {
            case ri_all    : KOutMsg( "filter:all\n" ); break;
            case ri_intern : KOutMsg( "filter:intern\n" ); break;
            case ri_extern : KOutMsg( "filter:extern\n" ); break;
        }
        if ( NULL != self -> names ) {
            uint32_t count, idx;
            rc_t rc = VNameListCount( self -> names, &count );
            if ( 0 == rc ) {
                KOutMsg( "filter:%u reference-names\n", count );
                for ( idx = 0; 0 == rc && idx < count; ++idx ) {
                    const char * s = NULL;
                    rc = VNameListGet( self -> names, idx, &s );
                    if ( 0 == rc && NULL != s ) {
                        KOutMsg( "filter:[%u] = %s\n", idx, s );                        
                    }
                }
            }
        } else {
            KOutMsg( "filter:no reference-names\n" );
        }
    }
}

static bool check_location( bool internal, ref_inventory_location_t location_filter ) {
    bool res = true;
    switch ( location_filter ) {
        case ri_all     : res = true; break;
        case ri_intern  : res = internal; break;
        case ri_extern  : res = !internal; break;
    }
    return res;
}

static bool check_ref_inventory_filter( const ref_inventory_filter_t * self,
                                        const ref_inventory_entry_t * entry ) {
    bool res = true;
    if ( NULL != self ) {
        res = check_location( entry -> internal, self -> location );
        if ( res && NULL != self -> names ) {
            int32_t idx = -1;
            rc_t rc = VNamelistContainsString( self -> names, entry -> name, &idx );
            res = ( 0 == rc && idx > -1 );
            if ( !res ) {
                rc = VNamelistContainsString( self -> names, entry -> seq_id, &idx );
                res = ( 0 == rc && idx > -1 );
            }
        }
    }
    return res;
}

/* ------------------------------------------------------------------------------------------------------------- */

typedef struct ref_inventory_iter_t {
    const SLNode * current;
    const ref_inventory_filter_t * filter;
} ref_inventory_iter_t;

void destroy_ref_inventory_iter( ref_inventory_iter_t * self ) {
    if ( NULL != self ) {
        free( ( void * ) self );
    }
}

ref_inventory_iter_t * make_ref_inventory_iter( const ref_inventory_t * src, 
                                                const ref_inventory_filter_t * filter ) {
    ref_inventory_iter_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        res -> current =  SLListHead( &( src -> list ) );
        res -> filter = filter;
    }
    return res;
}

static const ref_inventory_entry_t * ref_inventory_iter_get_next( ref_inventory_iter_t * self ) {
    const ref_inventory_entry_t * res = NULL;
    if ( NULL != self -> current ) {
        res = ( const ref_inventory_entry_t * )self -> current;
        self -> current = SLNodeNext( self -> current );
    }
    return res;
}

const ref_inventory_entry_t * ref_inventory_iter_next( ref_inventory_iter_t * self ) {
    const ref_inventory_entry_t * res = NULL;
    if ( NULL != self ) {
        while ( NULL == res ) {
            res = ref_inventory_iter_get_next( self );
            if ( NULL == res ) { break; }
            if ( !check_ref_inventory_filter( self -> filter, res ) ) { res = NULL; }
        }
    }
    return res;
}

void ref_inventory_iter_report( ref_inventory_iter_t * self ) {
    if ( NULL != self ) {
        const ref_inventory_entry_t * e = ref_inventory_iter_next( self );
        while ( NULL != e ) {
            ref_inventory_entry_report( e );
            e = ref_inventory_iter_next( self );
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------- */

typedef struct ref_bases_t {
    ref_inventory_t * inventory;
    ref_inventory_iter_t * inventory_iter;
    const ref_inventory_filter_t * filter;
    const ref_inventory_entry_t * entry;
    struct cmn_iter_t * cmn;
    cmn_iter_params_t iter_params;
    uint32_t cur_idx_bases;
} ref_bases_t;

void destroy_ref_bases( ref_bases_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> inventory_iter ) { 
            destroy_ref_inventory_iter( self -> inventory_iter );
        }
        if ( NULL != self -> inventory ) {
            destroy_ref_inventory( self -> inventory );
        }
        if ( NULL != self -> cmn ) {
            destroy_cmn_iter( self -> cmn );
        }
        free( ( void * ) self );
    }
}

ref_bases_t * make_ref_bases( const tool_ctx_t * tool_ctx, 
                              const ref_inventory_filter_t * filter ) {
    ref_bases_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        bool success = false;
        res -> entry = NULL;
        res -> inventory = make_ref_inventory( tool_ctx );
        res -> filter = filter;
        if ( NULL != res -> inventory ) {
            res -> inventory_iter = make_ref_inventory_iter( res -> inventory, filter );
            if ( NULL == res -> inventory_iter ) {
                destroy_ref_bases( res );
                res = NULL;
            } else {
                bool populated = tctx_populate_cmn_iter_params( tool_ctx, &( res -> iter_params ) ); 
                if ( populated ) {
                    rc_t rc = make_cmn_iter( &( res -> iter_params ), "REFERENCE", &( res -> cmn ) );
                    if ( 0 == rc ) {
                        rc = cmn_iter_add_column( res -> cmn, "READ", &( res -> cur_idx_bases ) );
                    }
                    if ( 0 == rc ) {
                        rc = cmn_iter_range( res -> cmn, res -> cur_idx_bases );
                    }
                    success = ( 0 == rc );
                }
            }
        }
        if ( !success ) {
            destroy_ref_bases( res );
            res = NULL;
        }
    }
    return res;
}

void ref_bases_report( const ref_bases_t * self ) {
    if ( NULL != self ) {
        ref_inventory_iter_t * iter = make_ref_inventory_iter( self -> inventory, 
                                                               self -> filter );
        ref_inventory_iter_report( iter );
        destroy_ref_inventory_iter( iter );
    }
}
    
const ref_inventory_entry_t * ref_bases_next_ref( ref_bases_t * self ) {
    if ( NULL != self ) {
        self -> entry = ref_inventory_iter_next( self -> inventory_iter );
        if ( NULL != self -> entry ) {
            rc_t rc = cmn_iter_set_range( self -> cmn, 
                                          self -> entry -> first_row,
                                          self -> entry -> row_count );
            if ( 0 != rc ) {
                self -> entry = NULL;
            }
        }
    }
    return self -> entry;
}

bool ref_bases_next_chunk( ref_bases_t * self, String * dst ) {
    bool res = false;
    if ( NULL != self && NULL != dst ) {
        rc_t rc;
        res = cmn_iter_next( self -> cmn, &rc );
        if ( res ) {
            rc = cmn_read_String( self -> cmn, self -> cur_idx_bases, dst );
        }
        res = ( res && ( 0 == rc ) );
    }
    return res;
}

/* ------------------------------------------------------------------------------------------------------------- */

typedef struct ref_printer_t {
    const String * data;
    KFile * stdout_file;
    struct file_printer_t * file_printer;
    uint32_t limit;
} ref_printer_t;

void destroy_ref_printer( ref_printer_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> data ) {
            StringWhack ( self -> data );
        }
        if ( NULL != self -> file_printer ) {
            destroy_file_printer( self -> file_printer ); /* file_printer.c */
        }
        if ( NULL != self -> stdout_file ) {
            KFileRelease( self -> stdout_file );
        }
        free( ( void * )self );
    }
}

ref_printer_t * make_ref_printer_by_ref_name( const tool_ctx_t * tool_ctx,
                                              const String * ref_name ) {
    ref_printer_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        rc_t rc = 0;
        res -> data = NULL;     /* we have no data -- yet -- */
        res -> limit = 70;      /* hardcoded for now - maybe will be in tool_ctx- */
        res -> stdout_file = NULL;

        rc = make_file_printer_from_filename( tool_ctx -> dir, 
                                              &( res -> file_printer ),
                                              tool_ctx -> buf_size,
                                              1024,
                                              "%s.%*s.fasta", /* !!! %S does not work!!! */
                                              tool_ctx -> accession_short,
                                              ref_name -> len,
                                              ref_name -> addr );  /* file_printer.c */
        if ( 0 != rc ) {
            res -> file_printer = NULL;
            destroy_ref_printer( res );
            res = NULL;
        }
    }
    return res;
}

ref_printer_t * make_ref_printer( const tool_ctx_t * tool_ctx ) {
    ref_printer_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        rc_t rc = 0;
        res -> data = NULL;     /* we have no data -- yet -- */
        res -> limit = 70;      /* hardcoded for now - maybe will be in tool_ctx- */
        res -> stdout_file = NULL;
        
        if ( tool_ctx -> use_stdout ) {
            rc = KFileMakeStdOut( &( res -> stdout_file ) );
            if ( 0 == rc ) {
                rc = make_file_printer_from_file( res -> stdout_file,
                                                &( res -> file_printer ),
                                                tool_ctx -> buf_size ); /* file_printer.c */
            }
        } else {
            rc = make_file_printer_from_filename( tool_ctx -> dir, 
                                                &( res -> file_printer ),
                                                tool_ctx -> buf_size,
                                                1024,
                                                "%s.ref.fasta",
                                                tool_ctx -> accession_short ); /* file_printer.c */
        }
        if ( 0 != rc ) {
            res -> file_printer = NULL;
            destroy_ref_printer( res ); /* file_printer.c */
            res = NULL;
        }
    }
    return res;
}

bool ref_printer_add( ref_printer_t * self, const String * S ) {
    bool res = ( NULL != self && NULL != S );
    if ( res && ( S -> len > 0 ) ) {
        rc_t rc;
        if ( NULL == self -> data ) {
            rc = StringCopy( &( self -> data ), S );
            res = ( 0 == rc );
        } else {
            const String * tmp;
            rc = StringConcat( &tmp, self -> data, S );
            res = ( 0 == rc );
            if ( res ) {
                StringWhack( self -> data );
                self -> data = tmp;
            }
        }
    }
    return res;
}

static bool ref_printer_has_data( const ref_printer_t * self ) {
    bool res = ( NULL != self );
    res = res && ( NULL != self -> data );
    return res && ( self -> data -> len > 0 );
}

static void ref_printer_write( ref_printer_t * self, const String * to_write, const String * tmp ) {
    file_print( self -> file_printer, "%S\n", to_write );
    StringWhack( self -> data );
    self -> data = tmp;
}
                               
bool ref_printer_flush( ref_printer_t * self, bool all ) {
    bool res = ( NULL != self );
    if ( res ) {
        while ( ref_printer_has_data( self ) && ( self -> data -> len >= self -> limit ) ) {
            String to_flush;
            String * to_write = StringSubstr( self -> data, &to_flush, 0, self -> limit );
            if ( NULL == to_write ) {
                res = false;
                break;
            } else {
                const String * tmp = NULL;
                uint32_t left = self -> data -> len - self -> limit;
                if ( left > 0 ) {
                    String rem;
                    String * rem_ptr = StringSubstr( self -> data, &rem, self -> limit, left );
                    if ( NULL == rem_ptr ) {
                        res = false;
                        break;
                    } else {
                        rc_t rc = StringCopy( &tmp, rem_ptr );
                        if ( 0 != rc ) {
                            res = false;
                            break;
                        }
                    }
                }
                ref_printer_write( self, to_write, tmp );
            }
        }
        if ( ref_printer_has_data( self ) && all ) {
            ref_printer_write( self, self -> data, NULL );
        }
    }
    return res;
}

/* ------------------------------------------------------------------------------------------------------------- */

bool test_ref_inventory( const tool_ctx_t * tool_ctx ) {
    struct ref_inventory_t * inventory = make_ref_inventory( tool_ctx ); /* above */
    bool res = ( NULL != inventory );
    if ( res ) {
        uint32_t num_refs = ref_inventory_count( inventory );
        KOutMsg( "inventory filled with %u references\n", num_refs );  
        res = ( num_refs > 0 );
        ref_inventory_report( inventory );

        destroy_ref_inventory( inventory ); /* ref_inventory.c */
    }
    return res;
}

bool test_ref_inventory_bases( const tool_ctx_t * tool_ctx ) {
    ref_bases_t * bases = make_ref_bases( tool_ctx, NULL );
    bool res = ( NULL != bases );
    if ( res ) {
        uint32_t cnt = 0;
        const ref_inventory_entry_t * entry = ref_bases_next_ref( bases );
        while ( NULL != entry ) {
            String s_bases;
            uint64_t base_cnt = 0;
            uint32_t chunks = 0;
            bool has_next = ref_bases_next_chunk( bases, &s_bases );
            while ( has_next ) {
                base_cnt += s_bases . len;
                chunks++;
                cnt++;
                has_next = ref_bases_next_chunk( bases, &s_bases );                        
            }
            KOutMsg( "[%S] has %lu bases in %u chunks\n",
                        ref_inventory_entry_name( entry ), base_cnt, chunks );
            /* here it goes */
            entry = ref_bases_next_ref( bases );
        }
        destroy_ref_bases( bases );
        res = ( cnt > 0 );
    }
    return res;
}

static void ref_print_data_buffer( ref_printer_t * printer, 
                                   const KDataBuffer * buffer ) {
    String S;
    StringInit( &S, buffer -> base, buffer -> elem_count - 1, buffer -> elem_count -1 );
    ref_printer_flush( printer, true );
    if ( ref_printer_add( printer, &S ) ) {
        ref_printer_flush( printer, true );
    }
}

static rc_t ref_print_defline_acc( ref_printer_t * printer, const char * acc ) {
    KDataBuffer buffer;
    rc_t rc = KDataBufferMake( &buffer, 8, 0 );
    if ( 0 == rc ) {
        rc = KDataBufferPrintf( &buffer, ">%s", acc );
        if ( 0 == rc ) {
            ref_print_data_buffer( printer, &buffer );
        }
        KDataBufferWhack( &buffer );
    }
    return rc;
}
    
static rc_t ref_print_defline_for_entry( ref_printer_t * printer, 
                        const ref_inventory_entry_t * entry,
                        bool use_name ) {
    KDataBuffer buffer;
    rc_t rc = KDataBufferMake( &buffer, 8, 0 );
    if ( 0 == rc ) {
        const String * name = use_name ? entry -> name : entry -> seq_id;
        rc = KDataBufferPrintf( &buffer, ">%S", name );
        if ( 0 == rc ) {
            ref_print_data_buffer( printer, &buffer );
        }
        KDataBufferWhack( &buffer );
    }
    return rc;
}

static bool ref_inventory_print_chunk( ref_printer_t * printer, const String * S ) {
    bool res = ref_printer_add( printer, S );
    if ( res ) { res = ref_printer_flush( printer, false ); }
    return res;
}

ref_inventory_location_t location_from_tool_ctx( const tool_ctx_t * tool_ctx ) {
    ref_inventory_location_t res = ri_all;
    if ( tool_ctx -> only_external_refs ) {
        if ( tool_ctx -> only_internal_refs ) {
            res = ri_all;
        } else {
            res = ri_extern;
        }
        
    } else {
        if ( tool_ctx -> only_internal_refs ) {
            res = ri_intern;
        } else {
            res = ri_all;
        }
    }
    return res;
}

static rc_t ref_inventory_print_single_file( const tool_ctx_t * tool_ctx,
                                             ref_bases_t * bases ) {
    rc_t rc = 0;
    ref_printer_t * printer = make_ref_printer( tool_ctx );
    if ( NULL == printer ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "ref_inventory_print_single_file() . make_ref_printer_by_ref_name() -> %R", rc );
    } else {
        const ref_inventory_entry_t * entry = ref_bases_next_ref( bases );
        while ( NULL != entry ) {
            String s_bases;
            uint64_t base_cnt = 0;
            uint32_t chunks = 0;
            
            /* ===> */ ref_print_defline_for_entry( printer, entry, tool_ctx -> use_name );
            bool has_next = ref_bases_next_chunk( bases, &s_bases );
            while ( has_next ) {
                base_cnt += s_bases . len;
                chunks++;
                /* ===> */ ref_inventory_print_chunk( printer, &s_bases );
                has_next = ref_bases_next_chunk( bases, &s_bases );                        
            }
            entry = ref_bases_next_ref( bases );
        }
        ref_printer_flush( printer, true );
        destroy_ref_printer( printer );
    }
    return rc;
}

static rc_t ref_inventory_print_split_file( const tool_ctx_t * tool_ctx,
                                             ref_bases_t * bases ) {
    rc_t rc = 0;
    const ref_inventory_entry_t * entry = ref_bases_next_ref( bases );
    while ( 0 == rc && NULL != entry ) {
        ref_printer_t * printer = make_ref_printer_by_ref_name( tool_ctx, 
            tool_ctx -> use_name ? entry -> name : entry -> seq_id );
        if ( NULL == printer ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "ref_inventory_print_split_file() . make_ref_printer_by_ref_name() -> %R", rc );
        } else {
            String s_bases;
            uint64_t base_cnt = 0;
            uint32_t chunks = 0;
            
            /* ===> */ ref_print_defline_for_entry( printer, entry, tool_ctx -> use_name );
            bool has_next = ref_bases_next_chunk( bases, &s_bases );
            while ( has_next ) {
                base_cnt += s_bases . len;
                chunks++;
                /* ===> */ ref_inventory_print_chunk( printer, &s_bases );
                has_next = ref_bases_next_chunk( bases, &s_bases );  
            }
            ref_printer_flush( printer, true );
            destroy_ref_printer( printer );
        }
        entry = ref_bases_next_ref( bases );
    }
    return rc;
}

rc_t ref_inventory_print( const tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    ref_inventory_location_t location = location_from_tool_ctx( tool_ctx );
    ref_inventory_filter_t * filter = make_ref_inventory_filter( location, tool_ctx -> ref_name_filter );
    if ( NULL == filter ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "ref_inventory_print() . make_ref_inventory_filter() -> %R", rc );
    } else {
        ref_bases_t * bases = make_ref_bases( tool_ctx, filter );
        if ( NULL == bases ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "ref_inventory_print() . make_ref_bases() -> %R", rc );
        }
        else {
            if ( tool_ctx -> split_file ) {
                rc = ref_inventory_print_split_file( tool_ctx, bases );
            } else {
                rc = ref_inventory_print_single_file( tool_ctx, bases );
            }
            destroy_ref_bases( bases );
        }
        destroy_ref_inventory_filter( filter );
    }
    return rc;
}

rc_t ref_inventory_print_report( const tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    ref_inventory_location_t location = location_from_tool_ctx( tool_ctx );
    ref_inventory_filter_t * filter = make_ref_inventory_filter( location, tool_ctx -> ref_name_filter );
    if ( NULL == filter ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "ref_inventory_print_report() . make_ref_inventory_filter() -> %R", rc );
    } else {
        ref_bases_t * bases = make_ref_bases( tool_ctx, filter );
        if ( NULL == bases ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "ref_inventory_print_report() . make_ref_bases() -> %R", rc );
        }
        else {
            ref_bases_report( bases );
            destroy_ref_bases( bases );
        }
        destroy_ref_inventory_filter( filter );
    }
    return rc;
}

rc_t ref_inventory_print_concatenated( const tool_ctx_t * tool_ctx, const char * tbl_name ) {
    rc_t rc = 0;
    ref_printer_t * printer = make_ref_printer( tool_ctx );    
    if ( NULL == printer ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "ref_inventory_print_concatenated() . make_ref_printer() -> %R", rc );
    } else {
        rc = ref_print_defline_acc( printer, tool_ctx -> accession_short );
        if ( 0 == rc ) {
            cmn_iter_params_t iter_params;
            if ( populate_cmn_iter_params( &iter_params,
                                           tool_ctx -> dir,
                                           tool_ctx -> vdb_mgr,
                                           tool_ctx -> accession_short,
                                           tool_ctx -> accession_path,
                                           tool_ctx -> cursor_cache,
                                           0, 0 ) ) {
                struct simple_fasta_iter_t * iter;
                rc = sfai_create( &iter_params, tbl_name, &iter );
                if ( 0 == rc ) {
                    String read;
                    KOutMsg( "row-count=%ld\n", sfai_get_row_count( iter ) );
                    while( sfai_get( iter, &read, &rc ) ) {
                        ref_printer_add( printer, &read );
                        ref_printer_flush( printer, false );
                    }
                    ref_printer_flush( printer, true );
                    sfai_destroy( iter );
                }
            }
        }
        destroy_ref_printer( printer );
    }
    return rc;
}
