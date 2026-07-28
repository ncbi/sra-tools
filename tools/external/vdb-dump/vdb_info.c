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

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/time.h>
#include <klib/printf.h>
#include <klib/num-gen.h>
#include <klib/text.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <kns/manager.h>
#include <kns/http.h>

#include <kdb/manager.h>
#include <kdb/meta.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>
#include <vdb/vdb-priv.h>

#include "vdb-dump-context.h"
#include "vdb-dump-helper.h"
#include "vdb-dump-coldefs.h"

#include <stdlib.h>
#include <string.h>

static const char * PT_DATABASE = "Database";
static const char * PT_TABLE    = "Table";
static const char * PT_NONE     = "None";

typedef struct vdb_info_vers {
    char s_vers[ 16 ];
    uint8_t major, minor, release;
} vdb_info_vers;

typedef struct vdb_info_date {
    char date[ 32 ];
    uint64_t timestamp;

    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
} vdb_info_date;

typedef struct vdb_info_event {
    char name[ 32 ];
    vdb_info_vers vers;
    vdb_info_date tool_date;
    vdb_info_date run_date;
} vdb_info_event;

typedef struct vdb_info_bam_hdr {
    bool present;
    size_t hdr_bytes;
    uint32_t total_lines;
    uint32_t HD_lines;
    uint32_t SQ_lines;
    uint32_t RG_lines;
    uint32_t PG_lines;
} vdb_info_bam_hdr;

typedef struct vdb_info_data {
    const char * acc;
    const char * s_path_type;
    const char * s_platform;

    char path[ 4096 ];
    const VPath * localP;
    char remote_path[4096];
    const VPath * remoteP;
    char cache[ 1024 ];
    const VPath * cacheP;
    char schema_name[ 1024 ];
    char species[ 1024 ];
    
    vdb_info_event formatter;
    vdb_info_event loader;
    vdb_info_event update;

    vdb_info_date ts;

    vdb_info_bam_hdr bam_hdr;
    
    float cache_percent;
    uint64_t bytes_in_cache;

    uint64_t seq_rows;
    uint64_t ref_rows;
    uint64_t prim_rows;
    uint64_t sec_rows;
    uint64_t ev_rows;
    uint64_t ev_int_rows;
    uint64_t consensus_rows;
    uint64_t passes_rows;
    uint64_t metrics_rows;

    uint64_t file_size;
} vdb_info_data;


/* ----------------------------------------------------------------------------- */

typedef struct split_vers_ctx {
    char tmp[ 32 ];
    uint32_t dst;
    uint32_t sel;
} split_vers_ctx;

static void store_vers( vdb_info_vers * vers, split_vers_ctx * ctx ) {
    ctx -> tmp[ ctx -> dst ] = 0;
    switch ( ctx -> sel ) {
        case 0 : vers -> major   = atoi( ctx -> tmp ); break;
        case 1 : vers -> minor   = atoi( ctx -> tmp ); break;
        case 2 : vers -> release = atoi( ctx -> tmp ); break;
    }
    ( ctx -> sel )++;
    ctx -> dst = 0;
}

static void split_vers( vdb_info_vers * vers ) {
    uint32_t i, l = string_measure ( vers -> s_vers, NULL );
    split_vers_ctx ctx;
    memset( &ctx, 0, sizeof ctx );
    for ( i = 0; i < l; ++i ) {
        char c = vers -> s_vers[ i ];
        if ( c >= '0' && c <= '9' ) {
            ctx . tmp[ ( ctx . dst )++ ] = c;
        } else if ( '.' == c ) {
            store_vers( vers, &ctx );
        }
    }
    if ( ctx . dst > 0 ) {
        store_vers( vers, &ctx );
    }
}

/* ----------------------------------------------------------------------------- */

typedef struct split_date_ctx {
    char tmp[ 32 ];
    uint32_t dst;
} split_date_ctx;

static uint8_t str_to_month_num( const char * s ) {
    uint8_t res = 0;
    switch( s[ 0 ] ) {
        case 'A' : ;
        case 'a' : switch ( s[ 1 ] )
                    {
                        case 'P' : ;
                        case 'p' : switch ( s[ 2 ] )
                                    {
                                        case 'R' :  ;
                                        case 'r' :  res = 4; break;
                                    }
                                    break;
                        case 'U' : ;
                        case 'u' : switch ( s[ 2 ] )
                                    {
                                        case 'G' :  ;
                                        case 'g' :  res = 8; break;
                                    }
                                    break;
                    }
                    break;

        case 'F' : ;
        case 'f' : switch ( s[ 1 ] )
                    {
                        case 'E' : ;
                        case 'e' : switch ( s[ 2 ] )
                                    {
                                        case 'B' :  ;
                                        case 'b' :  res = 2; break;
                                    }
                                    break;
                    }
                    break;

        case 'J' : ;
        case 'j' : switch ( s[ 1 ] )
                    {
                        case 'A' : ;
                        case 'a' : switch ( s[ 2 ] )
                                    {
                                        case 'N' :  ;
                                        case 'n' :  res = 1; break;
                                    }
                                    break;
                        case 'U' : ;
                        case 'u' : switch ( s[ 2 ] )
                                    {
                                        case 'N' :  ;
                                        case 'n' :  res = 6; break;
                                        case 'L' :  ;
                                        case 'l' :  res = 7; break;
                                    }
                                    break;
                    }
                    break;

        case 'M' : ;
        case 'm' : switch ( s[ 1 ] )
                    {
                        case 'A' : ;
                        case 'a' : switch ( s[ 2 ] )
                                    {
                                        case 'R' :  ;
                                        case 'r' :  res = 3; break;
                                        case 'Y' :  ;
                                        case 'y' :  res = 5; break;
                                    }
                                    break;
                    }
                    break;

        case 'S' : ;
        case 's' : switch ( s[ 1 ] )
                    {
                        case 'E' : ;
                        case 'e' : switch ( s[ 2 ] )
                                    {
                                        case 'P' :  ;
                                        case 'p' :  res = 9; break;
                                    }
                                    break;
                    }
                    break;

        case 'O' : ;
        case 'o' : switch ( s[ 1 ] )
                    {
                        case 'C' : ;
                        case 'c' : switch ( s[ 2 ] )
                                    {
                                        case 'T' :  ;
                                        case 't' :  res = 10; break;
                                    }
                                    break;
                    }
                    break;

        case 'N' : ;
        case 'n' : switch ( s[ 1 ] )
                    {
                        case 'O' : ;
                        case 'o' : switch ( s[ 2 ] )
                                    {
                                        case 'V' :  ;
                                        case 'v' :  res = 11; break;
                                    }
                                    break;
                    }
                    break;

        case 'D' : ;
        case 'd' : switch ( s[ 1 ] )
                    {
                        case 'E' : ;
                        case 'e' : switch ( s[ 2 ] )
                                    {
                                        case 'C' :  ;
                                        case 'c' :  res = 12; break;
                                    }
                                    break;
                    }
                    break;
    }
    return res;
}

static void store_date( vdb_info_date * d, split_date_ctx * ctx ) {
    uint32_t l, value;

    ctx -> tmp[ ctx -> dst ] = 0;
    l = string_measure ( ctx -> tmp, NULL );
    value = atoi( ctx -> tmp );
    if ( 4 == l ) {
        d -> year = value;
    } else if ( ( 8 == l ) && ( ':' == ctx -> tmp[ 2 ] ) && ( ':' == ctx -> tmp[ 5 ] ) ) {
        ctx -> tmp[ 2 ] = 0;
        d -> hour = atoi( ctx -> tmp );
        ctx -> tmp[ 5 ] = 0;
        d -> minute = atoi( &( ctx -> tmp[ 3 ] ) );
    } else if ( 3 == l ) {
        d -> month = str_to_month_num( ctx -> tmp );
    } else {
        d -> day = value;
    }
    ctx -> dst = 0;
}

static void split_date( vdb_info_date * d ) {
    uint32_t i, l = string_measure ( d -> date, NULL );
    split_date_ctx ctx;
    memset( &ctx, 0, sizeof ctx );
    for ( i = 0; i < l; ++i ) {
        char c = d -> date[ i ];
        if ( ' ' == c ) {
            store_date( d, &ctx );
        } else {
            ctx.tmp[ ( ctx . dst )++ ] = c;
        }
    }
    if ( ctx . dst > 0 ) {
        store_date( d, &ctx );
    }
}

/* ----------------------------------------------------------------------------- */
static bool has_col( const VTable * tab, const char * colname ) {
    bool res = false;
    struct KNamelist * columns;
    rc_t rc = VTableListReadableColumns( tab, &columns );
    if ( 0 == rc ) {
        uint32_t count;
        rc = KNamelistCount( columns, &count );
        if ( 0 == rc && count > 0 ) {
            uint32_t idx;
            size_t colname_size = string_size( colname );
            for ( idx = 0; idx < count && 0 == rc && !res; ++idx ) {
                const char * a_name;
                rc = KNamelistGet ( columns, idx, &a_name );
                if ( 0 == rc ) {
                    int cmp;
                    size_t a_name_size = string_size( a_name );
                    uint32_t max_chars = ( uint32_t )colname_size;
                    if ( a_name_size > max_chars ) max_chars = ( uint32_t )a_name_size;
                    cmp = strcase_cmp ( colname, colname_size,
                                        a_name, a_name_size,
                                        max_chars );
                    res = ( 0 == cmp );
                }
            }
        }
        vdh_knamelist_release( rc, columns );
    }
    return res;
}

static const char * get_platform( const VTable * tab ) {
    const char * res = PT_NONE;
    if ( has_col( tab, "PLATFORM" ) ) {
        const VCursor * cur;
        rc_t rc = VTableCreateCursorRead( tab, &cur );
        if ( 0 == rc ) {
            uint32_t idx;
            rc = VCursorAddColumn( cur, &idx, "PLATFORM" );
            if ( 0 == rc ) {
                rc = VCursorOpen( cur );
                if ( 0 == rc ) {
                    const uint8_t * pf;
                    uint32_t row_len;
                    rc = VCursorCellDataDirect( cur, 1, idx, NULL, (const void**)&pf, NULL, &row_len );
                    if ( 0 == rc && row_len > 0 ) {
                        res = vdcd_get_platform_txt( *pf );
                    }
                }
            }
            vdh_vcursor_release( rc, cur );
        }
    }
    return res;
}

static void get_string_cell( char * buffer, size_t buffer_size, const VTable * tab, 
                             int64_t row, const char * column ) {
    if ( has_col( tab, column ) ) {
        const VCursor * cur;
        rc_t rc = VTableCreateCursorRead( tab, &cur );
        if ( 0 == rc ) {
            uint32_t idx;
            rc = VCursorAddColumn( cur, &idx, column );
            if ( 0 == rc ) {
                rc = VCursorOpen( cur );
                if ( 0 == rc ) {
                    const char * src;
                    uint32_t row_len;
                    rc = VCursorCellDataDirect( cur, row, idx, NULL, (const void**)&src, NULL, &row_len );
                    if ( 0 == rc ) {
                        size_t num_writ;
                        string_printf( buffer, buffer_size, &num_writ, "%.*s", row_len, src );
                    }
                }
            }
            vdh_vcursor_release( rc, cur );
        }
    }
}

static uint32_t get_length_cell( const VTable * tab, int64_t row, const char * column, rc_t *prc )
{
    uint32_t result = 0;
    if ( has_col( tab, column ) ) {
        const VCursor * cur;
        rc_t rc = VTableCreateCursorRead( tab, &cur );
        if ( 0 == rc ) {
            uint32_t idx;
            rc = VCursorAddColumn( cur, &idx, column );
            if ( 0 == rc ) {
                rc = VCursorOpen( cur );
                if ( 0 == rc ) {
                    void const *dummy = 0;
                    rc = VCursorCellDataDirect( cur, row, idx, NULL, &dummy, NULL, &result );
                    (void)dummy;
                }
            }
            vdh_vcursor_release( rc, cur );
        }
        assert(prc);
        *prc = rc;
    }
    return result;
}

static uint64_t get_rowcount( const VTable * tbl ) {
    uint64_t res = 0;
    col_defs *col_defs;
    if ( vdcd_init( &col_defs, 1024 ) ) {
        uint32_t invalid_columns;
        if ( vdcd_extract_from_table( col_defs, tbl, &invalid_columns ) > 0 ) {
            const VCursor * cur;
            rc_t rc = VTableCreateCursorRead( tbl, &cur );
            if ( 0 == rc ) {
                if ( vdcd_add_to_cursor( col_defs, cur ) ) {
                    rc = VCursorOpen( cur );
                    if ( 0 == rc ) {
                        uint32_t idx;
                        if ( vdcd_get_first_none_static_column_idx( col_defs, cur, &idx ) ) {
                            int64_t first;
                            VCursorIdRange( cur, idx, &first, &res );
                        }
                    }
                }
                vdh_vcursor_release( rc, cur );
            }
        }
        vdcd_destroy( col_defs );
    }
    return res;
}

/* ----------------------------------------------------------------------------- */

static void get_meta_attr( const KMDataNode * node, const char * key, char * dst, size_t dst_size ) {
    size_t size;
    rc_t rc = KMDataNodeReadAttr ( node, key, dst, dst_size, &size );
    if ( 0 == rc ) {
        dst[ size ] = 0;
    }
}

static void get_meta_event( const KMetadata * meta, const char * node_path, vdb_info_event * event ) {
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, &node, node_path );
    if ( 0 == rc ) {
        get_meta_attr( node, "name", event -> name, sizeof event -> name );
        if ( 0 == event -> name[ 0 ] ) {
            get_meta_attr( node, "tool", event -> name, sizeof event -> name );
        }

        get_meta_attr( node, "vers", event -> vers . s_vers, sizeof event -> vers . s_vers );
        split_vers( &( event -> vers ) );

        get_meta_attr( node, "run", event -> run_date . date, sizeof event -> run_date . date );
        split_date( &( event -> run_date ) );

        get_meta_attr( node, "date", event -> tool_date . date, sizeof event -> tool_date . date );
        if ( 0 == event -> tool_date . date[ 0 ] ) {
            get_meta_attr( node, "build", event -> tool_date . date, sizeof event -> tool_date . date );
        }
        split_date( &( event -> tool_date ) );
        vdh_datanode_release( 0, node );
    }
}

static size_t get_node_size( const KMDataNode * node ) {
    char buffer[ 10 ];
    size_t num_read, remaining, res = 0;
    rc_t rc = KMDataNodeRead( node, 0, buffer, sizeof( buffer ), &num_read, &remaining );
    if ( 0 == rc ) {
        res = num_read + remaining;
    }
    return res;
}

static bool is_newline( const char c ) {
    return ( ( 0x0A == c ) || ( 0x0D == c ) );
}

static void inspect_line( vdb_info_bam_hdr * bam_hdr, char * line, size_t len ) {
    ( bam_hdr -> total_lines )++;
    if ( ( len > 3 ) && ( '@' == line[ 0 ] ) ) {
        switch( line[ 1 ] ) {
            case 'H'    : if ( 'D' == line[ 2 ] ) ( bam_hdr -> HD_lines )++; break;
            case 'S'    : if ( 'Q' == line[ 2 ] ) ( bam_hdr -> SQ_lines )++; break;
            case 'R'    : if ( 'G' == line[ 2 ] ) ( bam_hdr -> RG_lines )++; break;
            case 'P'    : if ( 'G' == line[ 2 ] ) ( bam_hdr -> PG_lines )++; break;
        }
    }
}

static void parse_buffer( vdb_info_bam_hdr * bam_hdr, char * buffer, size_t len ) {
    char * line;
    size_t idx, line_len, state = 0;
    for ( idx = 0; idx < len; ++idx ) {
        switch( state ) {
            case 0 :    if ( is_newline( buffer[ idx ] ) ) { /* init */
                            state = 2;
                        } else {
                            line = &( buffer[ idx ] );
                            line_len = 1;
                            state = 1;
                        }
                        break;
                      
            case 1 :    if ( is_newline( buffer[ idx ] ) ) { /* content */
                            inspect_line( bam_hdr, line, line_len );
                            state = 2;
                        } else {
                            line_len++;
                        }
                        break;

            case 2 :   if ( !is_newline( buffer[ idx ] ) ) { /* newline */
                            line = &( buffer[ idx ] );
                            line_len = 1;
                            state = 1;
                        }
                        break;
        }
    }
}

static void get_meta_bam_hdr( vdb_info_bam_hdr * bam_hdr, const KMetadata * meta ) {
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, &node, "BAM_HEADER" );
    bam_hdr -> present = ( 0 == rc );
    if ( bam_hdr -> present ) {
        bam_hdr -> hdr_bytes = get_node_size( node );
        if ( bam_hdr -> hdr_bytes > 0 ) {
            char * buffer = malloc( bam_hdr -> hdr_bytes );
            if ( NULL != buffer ) {
                size_t num_read, remaining;
                rc = KMDataNodeRead( node, 0, buffer, bam_hdr -> hdr_bytes, &num_read, &remaining );
                if ( 0 == rc ) {
                    parse_buffer( bam_hdr, buffer, bam_hdr->hdr_bytes );
                }
                free( buffer );
            }
        }
        vdh_datanode_release( rc, node );
    }
}

static void get_meta_info( vdb_info_data * data, const KMetadata * meta ) {
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, &node, "schema" );
    if ( 0 == rc ) {
        size_t size;
        rc = KMDataNodeReadAttr ( node, "name", data -> schema_name, sizeof data -> schema_name, &size );
        if ( 0 == rc ) {
            data -> schema_name[ size ] = 0;
        }
        rc = vdh_datanode_release( rc, node );
    }

    rc = KMetadataOpenNodeRead ( meta, &node, "LOAD/timestamp" );
    if ( 0 == rc ) {
        rc = KMDataNodeReadAsU64 ( node, &( data -> ts . timestamp ) );
        if ( 0 == rc ) {
            KTime time_rec;
            KTimeLocal ( &time_rec, data -> ts . timestamp );
            data -> ts . year  = time_rec . year;
            data -> ts . month = time_rec . month + 1;
            data -> ts . day   = time_rec . day + 1;
            data -> ts . hour  = time_rec . hour;
            data -> ts . minute= time_rec . minute;
        }
        rc = vdh_datanode_release( rc, node );
    }

    get_meta_event( meta, "SOFTWARE/formatter", &( data -> formatter ) );
    get_meta_event( meta, "SOFTWARE/loader", &( data -> loader ) );
    get_meta_event( meta, "SOFTWARE/update", &( data -> update ) );
    get_meta_bam_hdr( &( data -> bam_hdr ), meta );
}

/* ----------------------------------------------------------------------------- */

static const char * get_path_type( const VDBManager *mgr, const char * acc_or_path ) {
    const char * res = PT_NONE;
    int path_type = ( VDBManagerPathType ( mgr, "%s", acc_or_path ) & ~ kptAlias );
    switch ( path_type ) { /* types defined in <kdb/manager.h> */
        case kptDatabase      : res = PT_DATABASE; break;

        case kptPrereleaseTbl :
        case kptTable         : res = PT_TABLE; break;

        default : res = PT_NONE; break;
    }
    return res;
}

static rc_t make_remote_file( const KFile ** f, const char * url ) {
    KNSManager * kns_mgr;
    rc_t rc = KNSManagerMake ( & kns_mgr );
    *f = NULL;    
    if ( 0 == rc ) {
        rc = KNSManagerMakeHttpFile ( kns_mgr, f, NULL, 0x01010000, "%s", url );
        KNSManagerRelease ( kns_mgr );
    }
    return rc;
}

static rc_t make_local_file( const KFile ** f, const char * path ) {
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    *f = NULL;
    if ( 0 == rc ) {
        rc = KDirectoryOpenFileRead( dir, f, "%s", path );
        rc = vdh_kdirectory_release( rc, dir );
    }
    return rc;
}

static uint64_t get_file_size( const char * path, bool remotely ) {
    uint64_t res = 0;
    const KFile * f;
    rc_t rc = ( remotely ) ? make_remote_file( &f, path ) : make_local_file( &f, path );
    if ( 0 == rc ) {
        KFileSize ( f, &res );
        rc = vdh_kfile_release( rc, f );
    }
    return res;
}

static rc_t vdb_info_tab( vdb_info_data * data, VSchema * schema, const VDBManager *mgr ) {
    const VTable * tab;
    rc_t rc = VDBManagerOpenTableRead( mgr, &tab, schema, "%s", data->acc );
    if ( 0 == rc ) {
        const KMetadata * meta = NULL;
        data -> s_platform = get_platform( tab );
        data -> seq_rows = get_rowcount( tab );
        get_string_cell( data -> species, sizeof data -> species, tab, 1, "DEF_LINE" );
        rc = VTableOpenMetadataRead ( tab, &meta );
        if ( 0 == rc ) {
            get_meta_info( data, meta );
            rc = vdh_kmeta_release( rc, meta );
        }
        rc = vdh_vtable_release( rc, tab );
    }
    return rc;
}

static uint64_t get_tab_row_count( const VDatabase * db, const char * table_name ) {
    uint64_t res = 0;
    const VTable * tab;
    rc_t rc = VDatabaseOpenTableRead( db, &tab, table_name );
    if ( 0 == rc ) {
        res = get_rowcount( tab );
        rc = vdh_vtable_release( rc, tab );
    }
    return res;
}

static bool is_local_reference(const VTable * ref_tbl, int64_t row) {
    /* local references have data in CMP_READ */
    rc_t rc = 0;
    uint32_t len = get_length_cell(ref_tbl, row, "CMP_READ", &rc);
    return rc == 0 && len != 0;
}

static void get_species( char * buffer, size_t buffer_size, const VDatabase * db, const VDBManager *mgr ) {
    const VTable * ref_tbl;
    rc_t rc = VDatabaseOpenTableRead( db, &ref_tbl, "REFERENCE" );
    if ( 0 == rc && is_local_reference(ref_tbl, 1) == false ) {
        char seq_id[ 1024 ];
        seq_id[ 0 ] = 0;
        get_string_cell( seq_id, sizeof seq_id, ref_tbl, 1, "SEQ_ID" );
        rc = vdh_vtable_release( rc, ref_tbl );
        if ( 0 != seq_id[ 0 ] ) {
            const VTable * seq_tbl;
            rc = VDBManagerOpenTableRead( mgr, &seq_tbl, NULL, "%s", seq_id );
            if ( 0 == rc ) {
                get_string_cell( buffer, buffer_size, seq_tbl, 1, "DEF_LINE" );
                rc = vdh_vtable_release( rc, seq_tbl );
            }
        }
    }
}

static rc_t vdb_info_db( vdb_info_data * data, VSchema * schema, const VDBManager *mgr ) {
    const VDatabase * db;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", data -> acc );
    if ( 0 == rc ) {
        const VTable * seq_tbl;
        const KMetadata * meta = NULL;

        rc_t rc1 = VDatabaseOpenTableRead( db, &seq_tbl, "SEQUENCE" );
        if ( 0 == rc1 ) {
            data -> s_platform = get_platform( seq_tbl );
            data -> seq_rows = get_rowcount( seq_tbl );
            rc = vdh_vtable_release( rc, seq_tbl );
        }

        data -> ref_rows          = get_tab_row_count( db, "REFERENCE" );
        data -> prim_rows         = get_tab_row_count( db, "PRIMARY_ALIGNMENT" );
        data -> sec_rows          = get_tab_row_count( db, "SECONDARY_ALIGNMENT" );
        data -> ev_rows           = get_tab_row_count( db, "EVIDENCE_ALIGNMENT" );
        data -> ev_int_rows       = get_tab_row_count( db, "EVIDENCE_INTERVAL" );
        data -> consensus_rows    = get_tab_row_count( db, "CONSENSUS" );
        data -> passes_rows       = get_tab_row_count( db, "PASSES" );
        data -> metrics_rows      = get_tab_row_count( db, "ZMW_METRICS" );

        if ( data -> ref_rows > 0 ) {
            get_species( data -> species, sizeof data -> species, db, mgr );
        }

        rc = VDatabaseOpenMetadataRead ( db, &meta );
        if ( 0 == rc ) {
            get_meta_info( data, meta );
            rc = vdh_kmeta_release( rc, meta );
        }
        rc = vdh_vdatabase_release( rc, db );
    }
    return rc;
}

/* ----------------------------------------------------------------------------- */

static rc_t vdb_info_print_xml_s( const char * tag, const char * value ) {
    if ( 0 != value[ 0 ] ) { return KOutMsg( "<%s>%s</%s>\n", tag, value, tag ); }
    return 0;
}

static rc_t vdb_info_print_xml_uint64( const char * tag, const uint64_t value ) {
    if ( 0 != value ) { return KOutMsg( "<%s>%lu</%s>\n", tag, value, tag ); }
    return 0;
}

static rc_t vdb_info_print_xml_event( const char * tag, vdb_info_event * event ) {
    rc_t rc = 0;
    if ( 0 != event -> name[ 0 ] ) {
        rc = KOutMsg( "<%s>\n", tag );
        if ( 0 == rc ) { rc = vdb_info_print_xml_s( "NAME", event -> name ); }
        if ( 0 == rc ) { rc = vdb_info_print_xml_s( "VERS", event -> vers . s_vers ); }
        if ( 0 == rc ) { rc = vdb_info_print_xml_s( "TOOLDATE", event -> tool_date . date ); }
        if ( 0 == rc ) { rc = vdb_info_print_xml_s( "RUNDATE", event -> run_date . date ); }
        if ( 0 == rc ) { rc = KOutMsg( "</%s>\n", tag ); }
    }
    return rc;
}

static rc_t vdb_info_print_xml( vdb_info_data * data ) {
    rc_t rc = KOutMsg( "<info>\n" );
    if ( 0 == rc ) { rc = vdb_info_print_xml_s( "acc", data -> acc ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_s( "path", data -> path ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "size", data -> file_size ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_s( "type", data -> s_path_type ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_s( "platf", data -> s_platform ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "SEQ", data -> seq_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "REF", data -> ref_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "PRIM", data -> prim_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "SEC", data -> sec_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "EVID", data -> ev_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "EVINT", data -> ev_int_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "CONS", data -> consensus_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "PASS", data -> passes_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_uint64( "METR", data -> metrics_rows ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_s( "SCHEMA", data -> schema_name ); }
    if ( ( 0 == rc ) && ( 0 != data -> ts . timestamp ) ) {
        rc = KOutMsg( "<TIMESTAMP>0x%.016x</TIMESTAMP>\n", data -> ts . timestamp );
        if ( 0 == rc ) { rc = KOutMsg( "<MONTH>%.02d</MONTH>\n", data -> ts . month ); }
        if ( 0 == rc ) { rc = KOutMsg( "<DAY>%.02d</DAY>\n", data -> ts . day ); }
        if ( 0 == rc ) { rc = KOutMsg( "<YEAR>%.02d</YEAR>\n", data -> ts . year ); }
        if ( 0 == rc ) { rc = KOutMsg( "<HOUR>%.02d</HOUR>\n", data -> ts . hour ); }
        if ( 0 == rc ) { rc = KOutMsg( "<MINUTE>%.02d</MINUTE>\n", data -> ts . minute ); }
    }
    if ( ( 0 == rc ) && ( 0 != data -> species[ 0 ] ) ) {
        rc = vdb_info_print_xml_s( "SPECIES", data -> species );
    }
    if ( 0 == rc ) { rc = vdb_info_print_xml_event( "FORMATTER", &( data -> formatter ) ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_event( "LOADER", &( data -> loader ) ); }
    if ( 0 == rc ) { rc = vdb_info_print_xml_event( "UPDATE", &( data -> update ) ); }
    if ( 0 == rc ) { rc = KOutMsg( "</info>\n" ); }
    return rc;
}

/* ----------------------------------------------------------------------------- */

static rc_t vdb_info_print_json_s( const char * tag, const char * value, bool * first ) {
    rc_t rc = 0;
    if ( 0 != value[ 0 ] ) {
        rc = KOutMsg( *first ? "\n\"%s\":\"%s\"" : ",\n\"%s\":\"%s\"", tag, value );
        *first = false;
    }
    return rc;
}

static rc_t vdb_info_print_json_uint64( const char * tag, const uint64_t value, bool *first ) {
    rc_t rc = 0;
    if ( 0 != value ) {
        rc = KOutMsg( *first ? "\n\"%s\":%lu" : ",\n\"%s\":%lu", tag, value );
        *first = false;
    }
    return rc;
}

static rc_t vdb_info_print_json_uint32( const char * tag, const uint32_t value, bool *first ) {
    rc_t rc = KOutMsg( *first ? "\n\"%s\":%u" : ",\n\"%s\":%u", tag, value );
    *first = false;
    return rc;
}

static rc_t vdb_info_print_json_event( const char * tag, vdb_info_event * event, bool *first ) {
    rc_t rc = 0;
    if ( 0 != event->name[ 0 ] ) {
        bool sub_first = true;
        rc = KOutMsg( *first ? "\n\"%s\":{" : ",\n\"%s\":{" , tag );
        *first = false;
        if ( 0 == rc ) { rc = vdb_info_print_json_s( "NAME", event -> name, &sub_first ); }
        if ( 0 == rc ) { rc = vdb_info_print_json_s( "VERS", event -> vers . s_vers, &sub_first ); }
        if ( 0 == rc ) { rc = vdb_info_print_json_s( "TOOLDATE", event -> tool_date . date, &sub_first ); }
        if ( 0 == rc ) { rc = vdb_info_print_json_s( "RUNDATE", event -> run_date . date, &sub_first ); }
        if ( 0 == rc ) { rc = KOutMsg( "\n}" ); }
    }
    return rc;
}

static rc_t vdb_info_print_json( vdb_info_data * data ) {
    rc_t rc = KOutMsg( "{" );
    bool first = true;
    if ( 0 == rc ) { rc = vdb_info_print_json_s( "acc", data -> acc, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_s( "path", data -> path, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "size", data -> file_size, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_s( "type", data -> s_path_type, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_s( "platf", data -> s_platform, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "SEQ", data -> seq_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "REF", data -> ref_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "PRIM", data -> prim_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "SEC", data -> sec_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "EVID", data -> ev_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "EVINT", data -> ev_int_rows, &first );}
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "CONS", data -> consensus_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "PASS", data -> passes_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint64( "METR", data -> metrics_rows, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_s( "SCHEMA", data -> schema_name, &first ); }
    if ( 0 == rc && 0 != data -> ts . timestamp ) { rc = vdb_info_print_json_uint64( "TIMESTAMP", data -> ts . timestamp, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint32( "MONTH",  data -> ts . month, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint32( "DAY",    data -> ts . day, &first); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint32( "YEAR",   data -> ts . year, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint32( "HOUR",   data -> ts . hour, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_uint32( "MINUTE", data -> ts . minute, &first ); }
    if ( 0 == rc && 0 != data->species[ 0 ] ) { rc = vdb_info_print_json_s( "SPECIES", data -> species, &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_event( "FORMATTER", &( data -> formatter ), &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_event( "LOADER", &( data -> loader ), &first ); }
    if ( 0 == rc ) { rc = vdb_info_print_json_event( "UPDATE", &( data -> update ), &first ); }
    if ( 0 == rc ) { rc = KOutMsg( "}\n" ); }
    return rc;
}

/* ----------------------------------------------------------------------------- */

static const char dflt_event_name[] = "-";

static rc_t vdb_info_print_sep_event( vdb_info_event * event, const char sep, bool last ) {
    rc_t rc;
    const char * ev_name = event -> name;
    if ( ( NULL == ev_name ) || ( 0 == ev_name[ 0 ] ) ) {
        ev_name = dflt_event_name;
    }

    if ( last ) {
        rc = KOutMsg( "'%s'%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d",
                      ev_name, sep,
                      event -> vers . major, sep, event -> vers . minor, sep, event -> vers . release, sep,
                      event -> tool_date . month, sep, event -> tool_date . day, sep, event -> tool_date . year, sep,
                      event -> run_date . month, sep, event -> run_date . day, sep, event -> run_date . year );
    } else {
        rc = KOutMsg( "'%s'%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c",
                      ev_name, sep,
                      event -> vers . major, sep, event -> vers . minor, sep, event -> vers . release, sep,
                      event -> tool_date . month, sep, event -> tool_date . day, sep, event -> tool_date . year, sep,
                      event -> run_date . month, sep, event -> run_date . day, sep, event -> run_date . year, sep );
    }
    return rc;
}


static rc_t vdb_info_print_sep( vdb_info_data * data, const char sep ) {
    rc_t rc = KOutMsg( "'%s'%c%lu%c%c%c'%s'%c",
                       data -> acc, sep, data -> file_size, sep,
                       data -> s_path_type[ 0 ], sep, &( data -> s_platform[ 13 ] ), sep );
    if ( 0 == rc ) {
        rc = KOutMsg( "%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c",
                      data -> seq_rows, sep, data -> ref_rows, sep,
                      data -> prim_rows, sep, data -> sec_rows, sep, data -> ev_rows, sep,
                      data -> ev_int_rows, sep, data -> consensus_rows, sep,
                      data -> passes_rows, sep, data -> metrics_rows, sep );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "'%s'%c%d%c%d%c%d%c%d%c%d%c",
                      data -> schema_name, sep,
                      data -> ts . month, sep, data -> ts . day, sep, data -> ts . year, sep,
                      data -> ts . hour, sep, data -> ts . minute, sep );
    }
    if ( 0 == rc ) {
        if ( 0 != data -> species[ 0 ] ) {
            rc = KOutMsg( "'%s'%c", data -> species, sep );
        } else {
            rc = KOutMsg( "-%c", sep );
        }
    }
    if ( 0 == rc ) {
        rc = vdb_info_print_sep_event( &( data -> formatter ), sep, false );
    }
    if ( 0 == rc ) {
        rc = vdb_info_print_sep_event( &( data -> loader ), sep, false );
    }
    if ( 0 == rc ) {
        rc = vdb_info_print_sep_event( &( data -> update ), sep, true );
    }   
    if ( 0 == rc ) {
        rc = KOutMsg( "\n" );
    }
    return rc;
}

/* ----------------------------------------------------------------------------- */

static rc_t vdb_info_print_dflt_date( vdb_info_date * d, const char *prefix0, const char *prefix1 ) {
    if ( 0 != d -> date[ 0 ] ) {
        return KOutMsg( "%s%s: %s (%d/%d/%d %d:%d)\n", prefix0, prefix1,
                        d -> date, d -> month, d -> day, d -> year, d -> hour, d -> minute );
    }
    return 0;
}

static rc_t vdb_info_print_dflt_event( vdb_info_event * event, const char *prefix ) {
    rc_t rc = 0;
    if ( 0 != event -> name[ 0 ] ) {
        rc = KOutMsg( "%s    : %s\n", prefix, event -> name );
    }
    if ( ( 0 == rc ) && ( 0 != event -> vers . s_vers[ 0 ] ) ) {
        rc = KOutMsg( "%sVER : %s\n", prefix, event -> vers . s_vers );
    }
    if ( 0 == rc ) {
        rc = vdb_info_print_dflt_date( &( event -> tool_date ), prefix, "DATE" );
    }
    if ( 0 == rc ) {
        rc = vdb_info_print_dflt_date( &( event -> run_date ),  prefix, "RUN " );
    }
    return rc;
}

static rc_t vdb_info_print_dflt( vdb_info_data * data ) {
    rc_t rc= KOutMsg( "acc    : %s\n", data -> acc );
    if ( ( 0 == rc ) && ( 0 != data -> path[ 0 ] ) ) {
        rc = KOutMsg( "path   : %s\n", data -> path );
    }
    if ( ( 0 == rc ) && ( 0 != data -> remote_path[ 0 ] ) ) {
        rc = KOutMsg( "remote : %s\n", data -> remote_path );
    }
    if ( ( 0 == rc ) && ( 0 != data -> file_size ) ) {
        rc = KOutMsg( "size   : %,lu\n", data -> file_size );
    }
    if ( ( 0 == rc ) && ( 0 != data -> cache[ 0 ] ) ) {
        rc = KOutMsg( "cache  : %s\n", data -> cache );
        if ( 0 == rc ) {
            rc = KOutMsg( "percent: %f\n", data -> cache_percent );
        }
        if ( 0 == rc ) {
            rc = KOutMsg( "bytes  : %,lu\n", data -> bytes_in_cache );
        }
    }
    if ( ( 0 == rc ) && ( data -> s_path_type[ 0 ] ) ) {
        rc = KOutMsg( "type   : %s\n", data -> s_path_type );
    }
    if ( ( 0 == rc ) && ( 0 != data -> s_platform[ 0 ] ) ) {
        rc = KOutMsg( "platf  : %s\n", data -> s_platform );
    }
    if ( ( 0 == rc ) && ( 0 != data -> seq_rows ) ) {
        rc = KOutMsg( "SEQ    : %,lu\n", data -> seq_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> ref_rows ) ) {
        rc = KOutMsg( "REF    : %,lu\n", data -> ref_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> prim_rows ) ) {
        rc = KOutMsg( "PRIM   : %,lu\n", data -> prim_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> sec_rows ) ) {
        rc = KOutMsg( "SEC    : %,lu\n", data -> sec_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> ev_rows ) ) {
        rc = KOutMsg( "EVID   : %,lu\n", data -> ev_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> ev_int_rows ) ) {
        rc = KOutMsg( "EVINT  : %,lu\n", data -> ev_int_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> consensus_rows ) ) {
        rc = KOutMsg( "CONS   : %,lu\n", data -> consensus_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> passes_rows ) ) {
        rc = KOutMsg( "PASS   : %,lu\n", data -> passes_rows );
    }
    if ( ( 0 == rc ) && ( 0 != data -> metrics_rows ) ) {
        rc = KOutMsg( "METR   : %,lu\n", data -> metrics_rows );
    }
    if ( ( 0 == rc) && ( 0 != data -> schema_name[ 0 ] ) ) {
        rc = KOutMsg( "SCHEMA : %s\n", data -> schema_name );
    }
    if ( ( 0 == rc ) && ( 0 != data -> ts . timestamp ) ) {
        rc = KOutMsg( "TIME   : 0x%.016x (%.02d/%.02d/%d %.02d:%.02d)\n",
                      data -> ts . timestamp, data -> ts . month, data -> ts . day,
                      data -> ts . year, data -> ts . hour, data -> ts . minute );
    }
    if ( ( 0 == rc ) && ( 0 != data -> species[ 0 ] ) ) {
        rc = KOutMsg( "SPECIES: %s\n", data -> species );
    }   
    if ( 0 == rc ) {
        rc = vdb_info_print_dflt_event( &( data -> formatter ), "FMT" );
    }
    if ( 0 == rc ) {
        rc = vdb_info_print_dflt_event( &( data -> loader ), "LDR" );
    }
    if ( 0 == rc ) {
        rc = vdb_info_print_dflt_event( &( data -> update ), "UPD" );
    }
    if ( ( 0 == rc ) && ( data -> bam_hdr . present ) ) {
        rc = KOutMsg( "BAMHDR : %d bytes / %d lines\n",
            data -> bam_hdr . hdr_bytes, data -> bam_hdr . total_lines );
        if ( ( 0 == rc ) && ( data -> bam_hdr . HD_lines > 0 ) ) {
            rc = KOutMsg( "BAMHDR : %d HD-lines\n", data -> bam_hdr . HD_lines );
        }
        if ( ( 0 == rc ) && ( data -> bam_hdr . SQ_lines > 0 ) ) {
            rc = KOutMsg( "BAMHDR : %d SQ-lines\n", data -> bam_hdr . SQ_lines );
        }
        if ( ( 0 == rc ) && ( data -> bam_hdr . RG_lines > 0 ) ) {
            rc = KOutMsg( "BAMHDR : %d RG-lines\n", data -> bam_hdr . RG_lines );
        }
        if ( ( 0 == rc ) && ( data -> bam_hdr . PG_lines > 0 ) ) {
            rc = KOutMsg( "BAMHDR : %d PG-lines\n", data -> bam_hdr . PG_lines );
        }
    }
    return rc;
}

/* ----------------------------------------------------------------------------- */

static rc_t vdb_info_print_sql_event( const char * prefix, bool last ) {
    rc_t rc = KOutMsg( "%s_NAME VARCHAR, ", prefix );
    if ( 0 == rc ) { rc = KOutMsg( "%s_VER_MAJOR INTEGER, ", prefix ); }
    if ( 0 == rc ) { rc = KOutMsg( "%s_VER_MINOR INTEGER, ", prefix ); }
    if ( 0 == rc ) { rc = KOutMsg( "%s_VER_RELEASE INTEGER, ", prefix ); }
    if ( 0 == rc ) { rc = KOutMsg( "%s_TOOL_MONTH INTEGER, ", prefix ); }
    if ( 0 == rc ) { rc = KOutMsg( "%s_TOOL_DAY INTEGER, ", prefix ); }
    if ( 0 == rc ) { rc = KOutMsg( "%s_TOOL_YEAR INTEGER, ", prefix ); }
    if ( 0 == rc ) { rc = KOutMsg( "%s_RUN_MONTH INTEGER, ", prefix ); }
    if ( 0 == rc ) { rc = KOutMsg( "%s_RUN_DAY INTEGER, ", prefix ); }
    if ( 0 == rc ) {
        if ( last ) {
            rc = KOutMsg( "%s_RUN_YEAR INTEGER ", prefix );
        } else {
            rc = KOutMsg( "%s_RUN_YEAR INTEGER, ", prefix );
        }
    }
    return rc;
}

static rc_t vdb_info_print_sql_header( const char * table_name ) {
    rc_t rc = KOutMsg( "CREATE TABLE %s ( ", table_name );
    if ( 0 == rc ) { rc = KOutMsg( "ACC VARCHAR(12) PRIMARY KEY, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "FILESIZE INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "TAB_OR_DB VARCHAR(1), " ); }
    if ( 0 == rc ) { rc = KOutMsg( "PLATFORM VARCHAR(16), " ); }
    if ( 0 == rc ) { rc = KOutMsg( "SEQ_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "REF_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "PRIM_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "SEC_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "EV_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "EV_INT_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "CONS_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "PASS_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "METR_ROWS INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "SCHEMA VARCHAR, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "TS_MONTH INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "TS_DAY INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "TS_YEAR INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "TS_HOUR INTEGER, " ); }
    if ( 0 == rc ) { rc = KOutMsg( "TS_MINUTE INTEGER, " ); }
    if ( 0 == rc ) { rc = vdb_info_print_sql_event( "FMT", false ); }
    if ( 0 == rc ) { rc = vdb_info_print_sql_event( "LD", false ); }
    if ( 0 == rc ) { rc = vdb_info_print_sql_event( "UPD", true ); }
    if ( 0 == rc ) { rc = KOutMsg( ");\n" ); }
    return rc;
}

static rc_t vdb_info_print_ev_sql( vdb_info_event * event, bool last ) {
    rc_t rc;
    if ( last ) {
        rc = KOutMsg( "\'%s\', %d, %d, %d, %d, %d, %d, %d, %d, %d ",
                    event -> name, event -> vers . major, event -> vers . minor, event -> vers . release,
                    event -> tool_date . month, event -> tool_date . day, event -> tool_date . year,
                    event -> run_date . month, event -> run_date . day, event -> run_date . year );
    } else {
        rc = KOutMsg( "\'%s\', %d, %d, %d, %d, %d, %d, %d, %d, %d, ",
                    event -> name, event -> vers . major, event -> vers . minor, event -> vers . release,
                    event -> tool_date . month, event -> tool_date . day, event -> tool_date . year,
                    event -> run_date . month, event -> run_date . day, event -> run_date . year );
    }
    return rc;
}

static rc_t vdb_info_print_sql( const char * table_name, vdb_info_data * data ) {
    rc_t rc = KOutMsg( "INSERT INTO %s VALUES ( ", table_name );
    if ( 0 == rc ) { rc= KOutMsg( "\'%s\', ", data -> acc ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> file_size ); }
    if ( 0 == rc ) { rc = KOutMsg( "\'%c\', ", data -> s_path_type[ 0 ] ); }
    if ( 0 == rc ) { rc = KOutMsg( "\'%s\', ", &( data -> s_platform[ 13 ] ) ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> seq_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> ref_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> prim_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> sec_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> ev_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> ev_int_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> consensus_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> passes_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "%lu, ", data -> metrics_rows ); }
    if ( 0 == rc ) { rc = KOutMsg( "\'%s\', ", data -> schema_name ); }
    if ( 0 == rc ) {
        rc = KOutMsg( "%d, %d, %d, %d, %d, ",
                      data -> ts . month, data -> ts . day, data -> ts . year,
                      data -> ts . hour, data -> ts . minute );
    }
    if ( 0 == rc ) { rc = vdb_info_print_ev_sql( &( data -> formatter ), false ); }
    if ( 0 == rc ) { rc = vdb_info_print_ev_sql( &( data -> loader ), false ); }
    if ( 0 == rc ) { rc = vdb_info_print_ev_sql( &( data -> update ), true ); }
    if ( 0 == rc ) { rc = KOutMsg( ");\n" ); }
    return rc;
}

/* ----------------------------------------------------------------------------- */

static uint8_t digits_of( uint64_t value ) {
    uint8_t res = 0;
         if ( value > 99999999999 ) { res = 12; }
    else if ( value > 9999999999 ) { res = 11; }
    else if ( value > 999999999 ) { res = 10; }
    else if ( value > 99999999 ) { res = 9; }
    else if ( value > 9999999 ) { res = 8; }
    else if ( value > 999999 ) { res = 7; }
    else if ( value > 99999 ) { res = 6; }
    else if ( value > 9999 ) { res = 5; }
    else if ( value > 999 ) { res = 4; }
    else if ( value > 99 ) { res = 3; }
    else if ( value > 9 ) { res = 2; }
    else { res = 1; }
    return res;
}

static rc_t vdb_info_1( VSchema * schema, dump_format_t format, const VDBManager *mgr,
                        const char * acc_or_path, const char * table_name ) {
    rc_t rc = 0;
    vdb_info_data data;

    memset( (void *)&data, 0, sizeof data );
    data.s_platform = PT_NONE;
    data.acc = acc_or_path;

    /* #1 get path-type */
    data.s_path_type = get_path_type( mgr, acc_or_path );

    if ( 'D' == data . s_path_type[ 0 ] || 'T' == data . s_path_type[ 0 ] ) {
        rc_t rc1;

        /* #2 fork by table or database */
        switch ( data . s_path_type[ 0 ] ) {
            case 'D' : vdb_info_db( &data, schema, mgr ); break;
            case 'T' : vdb_info_tab( &data, schema, mgr ); break;
            default : break;
        }

        /* try to resolve the path locally */
        rc1 = vdh_resolve(acc_or_path,
            &data.localP, &data.remoteP, &data.cacheP);
        if (0 == rc1)
            rc1 = vdh_set_local_or_remote_to_str(data.localP, data.remoteP,
                data.path, sizeof data.path);
        if ( 0 == rc1 ) {
            data . file_size = get_file_size( data . path,
                data.localP == NULL );
            rc1 = vdh_set_VPath_to_str(data.remoteP,
                data.remote_path, sizeof data.remote_path);
            if (0 == rc1) {
                rc1 = vdh_set_VPath_to_str(data.cacheP,
                    data.cache, sizeof data.cache);
                if (0 == rc1 && data.cacheP != NULL)
                    vdh_check_cache_comleteness(data.cache,
                        &data.cache_percent, &(data.bytes_in_cache));
            }
        } 
        switch ( format ) {
            case df_xml  : rc = vdb_info_print_xml( &data ); break;
            case df_json : rc = vdb_info_print_json( &data ); break;
            case df_csv  : rc = vdb_info_print_sep( &data, ',' ); break;
            case df_tab  : rc = vdb_info_print_sep( &data, '\t' ); break;
            case df_sql  : rc = vdb_info_print_sql( table_name, &data ); break;
            default     : rc = vdb_info_print_dflt( &data ); break;
        }
    }
    return rc;
}

rc_t vdb_info( Vector * schema_list, dump_format_t format, const VDBManager *mgr,
               const char * acc_or_path, struct num_gen * rows ) {
    VSchema * schema = NULL;
    rc_t rc = vdh_parse_schema( mgr, &schema, schema_list );
    if ( 0 == rc ) {
        if ( df_sql == format ) {
            rc = vdb_info_print_sql_header( acc_or_path );
        }
        if ( NULL != rows && !num_gen_empty( rows ) ) {
            const struct num_gen_iter * iter;
            rc = num_gen_iterator_make( rows, &iter );
            if ( 0 == rc ) {
                int64_t max_row;
                rc = num_gen_iterator_max( iter, &max_row );
                if ( 0 == rc ) {
                    int64_t id;
                    uint8_t digits = digits_of( max_row );

                    while ( 0 == rc && num_gen_iterator_next( iter, &id, &rc ) ) {
                        char acc[ 64 ];
                        size_t num_writ;
                        rc_t rc1 = -1;
                        switch ( digits ) {
                            case 1 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%ld", acc_or_path, id ); break;
                            case 2 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.02ld", acc_or_path, id ); break;
                            case 3 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.03ld", acc_or_path, id ); break;
                            case 4 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.04ld", acc_or_path, id ); break;
                            case 5 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.05ld", acc_or_path, id ); break;
                            case 6 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.06ld", acc_or_path, id ); break;
                            case 7 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.07ld", acc_or_path, id ); break;
                            case 8 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.08ld", acc_or_path, id ); break;
                            case 9 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.09ld", acc_or_path, id ); break;
                            default : break;
                        }
                        if ( 0 == rc1 ) {
                            rc = vdb_info_1( schema, format, mgr, acc, acc_or_path );
                        }
                    }
                }
                num_gen_iterator_destroy( iter );
            }
        } else {
            rc = vdb_info_1( schema, format, mgr, acc_or_path, acc_or_path );
        }
        rc = vdh_vschema_release( rc, schema );
    }
    return rc;
}
