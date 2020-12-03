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

typedef struct vdb_info_vers
{
    char s_vers[ 16 ];
    uint8_t major, minor, release;
} vdb_info_vers;


typedef struct vdb_info_date
{
    char date[ 32 ];
    uint64_t timestamp;

    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
} vdb_info_date;

typedef struct vdb_info_event
{
    char name[ 32 ];
    vdb_info_vers vers;
    vdb_info_date tool_date;
    vdb_info_date run_date;
} vdb_info_event;

typedef struct vdb_info_bam_hdr
{
    bool present;
    size_t hdr_bytes;
    uint32_t total_lines;
    uint32_t HD_lines;
    uint32_t SQ_lines;
    uint32_t RG_lines;
    uint32_t PG_lines;
} vdb_info_bam_hdr;

typedef struct vdb_info_data
{
    const char * acc;
    const char * s_path_type;
    const char * s_platform;

    char path[ 4096 ];
    char remote_path[ 4096 ];
    char cache[ 1024 ];
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

typedef struct split_vers_ctx
{
    char tmp[ 32 ];
    uint32_t dst;
    uint32_t sel;
} split_vers_ctx;

static void store_vers( vdb_info_vers * vers, split_vers_ctx * ctx )
{
    ctx->tmp[ ctx->dst ] = 0;
    switch ( ctx->sel )
    {
        case 0 : vers->major = atoi( ctx->tmp ); break;
        case 1 : vers->minor = atoi( ctx->tmp ); break;
        case 2 : vers->release = atoi( ctx->tmp ); break;
    }
    ctx->sel++;
    ctx->dst = 0;
}

static void split_vers( vdb_info_vers * vers )
{
    uint32_t i, l = string_measure ( vers->s_vers, NULL );
    split_vers_ctx ctx;
    memset( &ctx, 0, sizeof ctx );
    for ( i = 0; i < l; ++i )
    {
        char c = vers->s_vers[ i ];
        if ( c >= '0' && c <= '9' )
            ctx.tmp[ ctx.dst++ ] = c;
        else if ( c == '.' )
            store_vers( vers, &ctx );
    }
    if ( ctx.dst > 0 )
        store_vers( vers, &ctx );
}

/* ----------------------------------------------------------------------------- */

typedef struct split_date_ctx
{
    char tmp[ 32 ];
    uint32_t dst;
} split_date_ctx;


static uint8_t str_to_month_num( const char * s )
{
    uint8_t res = 0;
    switch( s[ 0 ] )
    {
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

static void store_date( vdb_info_date * d, split_date_ctx * ctx )
{
    uint32_t l, value;

    ctx->tmp[ ctx->dst ] = 0;
    l = string_measure ( ctx->tmp, NULL );
    value = atoi( ctx->tmp );
    if ( l == 4 )
        d->year = value;
    else if ( ( l == 8 ) && ( ctx->tmp[ 2 ] == ':' ) && ( ctx->tmp[ 5 ] == ':' ) )
    {
        ctx->tmp[ 2 ] = 0;
        d->hour = atoi( ctx->tmp );
        ctx->tmp[ 5 ] = 0;
        d->minute = atoi( &( ctx->tmp[ 3 ] ) );
    }
    else if ( l == 3 )
    {
        d->month = str_to_month_num( ctx->tmp );
    }
    else
    {
        d->day = value;
    }
    ctx->dst = 0;
}

static void split_date( vdb_info_date * d )
{
    uint32_t i, l = string_measure ( d->date, NULL );
    split_date_ctx ctx;
    memset( &ctx, 0, sizeof ctx );
    for ( i = 0; i < l; ++i )
    {
        char c = d->date[ i ];
        if ( c == ' ' )
            store_date( d, &ctx );
        else
            ctx.tmp[ ctx.dst++ ] = c;
    }
    if ( ctx.dst > 0 )
        store_date( d, &ctx );
}

/* ----------------------------------------------------------------------------- */
static bool has_col( const VTable * tab, const char * colname )
{
    bool res = false;
    struct KNamelist * columns;
    rc_t rc = VTableListReadableColumns( tab, &columns );
    if ( rc == 0 )
    {
        uint32_t count;
        rc = KNamelistCount( columns, &count );
        if ( rc == 0 && count > 0 )
        {
            uint32_t idx;
            size_t colname_size = string_size( colname );
            for ( idx = 0; idx < count && rc == 0 && !res; ++idx )
            {
                const char * a_name;
                rc = KNamelistGet ( columns, idx, &a_name );
                if ( rc == 0 )
                {
                    int cmp;
                    size_t a_name_size = string_size( a_name );
                    uint32_t max_chars = ( uint32_t )colname_size;
                    if ( a_name_size > max_chars ) max_chars = ( uint32_t )a_name_size;
                    cmp = strcase_cmp ( colname, colname_size,
                                        a_name, a_name_size,
                                        max_chars );
                    res = ( cmp == 0 );
                }
            }
        }
        KNamelistRelease( columns );
    }
    return res;
}

static const char * get_platform( const VTable * tab )
{
    const char * res = PT_NONE;
    if ( has_col( tab, "PLATFORM" ) )
    {
        const VCursor * cur;
        rc_t rc = VTableCreateCursorRead( tab, &cur );
        if ( rc == 0 )
        {
            uint32_t idx;
            rc = VCursorAddColumn( cur, &idx, "PLATFORM" );
            if ( rc == 0 )
            {
                rc = VCursorOpen( cur );
                if ( rc == 0 )
                {
                    const uint8_t * pf;
                    rc = VCursorCellDataDirect( cur, 1, idx, NULL, (const void**)&pf, NULL, NULL );
                    if ( rc == 0 )
                    {
                        res = vdcd_get_platform_txt( *pf );
                    }
                }
            }
            VCursorRelease( cur );
        }
    }
    return res;
}


static void get_string_cell( char * buffer, size_t buffer_size, const VTable * tab, int64_t row, const char * column )
{
    if ( has_col( tab, column ) )
    {
        const VCursor * cur;
        rc_t rc = VTableCreateCursorRead( tab, &cur );
        if ( rc == 0 )
        {
            uint32_t idx;
            rc = VCursorAddColumn( cur, &idx, column );
            if ( rc == 0 )
            {
                rc = VCursorOpen( cur );
                if ( rc == 0 )
                {
                    const char * src;
                    uint32_t row_len;
                    rc = VCursorCellDataDirect( cur, row, idx, NULL, (const void**)&src, NULL, &row_len );
                    if ( rc == 0 )
                    {
                        size_t num_writ;
                        string_printf( buffer, buffer_size, &num_writ, "%.*s", row_len, src );
                    }
                }
            }
            VCursorRelease( cur );
        }
    }
}


static uint64_t get_rowcount( const VTable * tbl )
{
    uint64_t res = 0;
    col_defs *col_defs;
    if ( vdcd_init( &col_defs, 1024 ) )
    {
        uint32_t invalid_columns;
        if ( vdcd_extract_from_table( col_defs, tbl, &invalid_columns ) > 0 )
        {
            const VCursor * cur;
            rc_t rc = VTableCreateCursorRead( tbl, &cur );
            if ( rc == 0 )
            {
                if ( vdcd_add_to_cursor( col_defs, cur ) )
                {
                    rc = VCursorOpen( cur );
                    if ( rc == 0 )
                    {
                        uint32_t idx;
                        if ( vdcd_get_first_none_static_column_idx( col_defs, cur, &idx ) )
                        {
                            int64_t first;
                            rc = VCursorIdRange( cur, idx, &first, &res );
                        }
                    }
                }
                VCursorRelease( cur );
            }
        }
        vdcd_destroy( col_defs );
    }
    return res;
}


/* ----------------------------------------------------------------------------- */


static void get_meta_attr( const KMDataNode * node, const char * key, char * dst, size_t dst_size )
{
    size_t size;
    rc_t rc = KMDataNodeReadAttr ( node, key, dst, dst_size, &size );
    if ( rc == 0 )
        dst[ size ] = 0;
}


static void get_meta_event( const KMetadata * meta, const char * node_path, vdb_info_event * event )
{
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, &node, node_path );
    if ( rc == 0 )
    {
        get_meta_attr( node, "name", event->name, sizeof event->name );
        if ( event->name[ 0 ] == 0 )
            get_meta_attr( node, "tool", event->name, sizeof event->name );

        get_meta_attr( node, "vers", event->vers.s_vers, sizeof event->vers.s_vers );
        split_vers( &event->vers );

        get_meta_attr( node, "run", event->run_date.date, sizeof event->run_date.date );
        split_date( &event->run_date );

        get_meta_attr( node, "date", event->tool_date.date, sizeof event->tool_date.date );
        if ( event->tool_date.date[ 0 ] == 0 )
            get_meta_attr( node, "build", event->tool_date.date, sizeof event->tool_date.date );
        split_date( &event->tool_date );

        KMDataNodeRelease ( node );
    }
}

static size_t get_node_size( const KMDataNode * node )
{
    char buffer[ 10 ];
    size_t num_read, remaining, res = 0;
    rc_t rc = KMDataNodeRead( node, 0, buffer, sizeof( buffer ), &num_read, &remaining );
    if ( rc == 0 ) res = num_read + remaining;
    return res;
}

static bool is_newline( const char c ) { return ( c == 0x0A || c == 0x0D ); }

static void inspect_line( vdb_info_bam_hdr * bam_hdr, char * line, size_t len )
{
    bam_hdr->total_lines++;
    if ( len > 3 && line[ 0 ] == '@' )
    {
        switch( line[ 1 ] )
        {
            case 'H'    : if ( line[ 2 ] == 'D' ) bam_hdr->HD_lines++; break;
            case 'S'    : if ( line[ 2 ] == 'Q' ) bam_hdr->SQ_lines++; break;
            case 'R'    : if ( line[ 2 ] == 'G' ) bam_hdr->RG_lines++; break;
            case 'P'    : if ( line[ 2 ] == 'G' ) bam_hdr->PG_lines++; break;
        }
    }
}

static void parse_buffer( vdb_info_bam_hdr * bam_hdr, char * buffer, size_t len )
{
    char * line;
    size_t idx, line_len, state = 0;
    for ( idx = 0; idx < len; ++idx )
    {
        switch( state )
        {
            case 0 :    if ( is_newline( buffer[ idx ] ) ) /* init */
                            state = 2;
                        else
                        {
                            line = &( buffer[ idx ] );
                            line_len = 1;
                            state = 1;
                        }
                        break;
                      
            case 1 :    if ( is_newline( buffer[ idx ] ) ) /* content */
                        {
                            inspect_line( bam_hdr, line, line_len );
                            state = 2;
                        }
                        else
                            line_len++;
                        break;

            case 2 :   if ( !is_newline( buffer[ idx ] ) ) /* newline */
                        {
                            line = &( buffer[ idx ] );
                            line_len = 1;
                            state = 1;
                        }
                        break;
        }
    }
}

static void get_meta_bam_hdr( vdb_info_bam_hdr * bam_hdr, const KMetadata * meta )
{
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, &node, "BAM_HEADER" );
    bam_hdr -> present = ( rc == 0 );
    if ( bam_hdr -> present )
    {
        bam_hdr->hdr_bytes = get_node_size( node );
        if ( bam_hdr->hdr_bytes > 0 )
        {
            char * buffer = malloc( bam_hdr->hdr_bytes );
            if ( buffer != NULL )
            {
                size_t num_read, remaining;
                rc = KMDataNodeRead( node, 0, buffer, bam_hdr->hdr_bytes, &num_read, &remaining );
                if ( rc == 0 )
                {
                    parse_buffer( bam_hdr, buffer, bam_hdr->hdr_bytes );
                }
                free( buffer );
            }
        }
        KMDataNodeRelease ( node );
    }
}

static void get_meta_info( vdb_info_data * data, const KMetadata * meta )
{
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, &node, "schema" );
    if ( rc == 0 )
    {
        size_t size;
        rc = KMDataNodeReadAttr ( node, "name", data->schema_name, sizeof data->schema_name, &size );
        if ( rc == 0 )
            data->schema_name[ size ] = 0;
        KMDataNodeRelease ( node );
    }

    rc = KMetadataOpenNodeRead ( meta, &node, "LOAD/timestamp" );
    if ( rc == 0 )
    {
        rc = KMDataNodeReadAsU64 ( node, &data->ts.timestamp );
        if ( rc == 0 )
        {
            KTime time_rec;
            KTimeLocal ( &time_rec, data->ts.timestamp );
            data->ts.year  = time_rec.year;
            data->ts.month = time_rec.month + 1;
            data->ts.day   = time_rec.day + 1;
            data->ts.hour  = time_rec.hour;
            data->ts.minute= time_rec.minute;
        }
        KMDataNodeRelease ( node );
    }

    get_meta_event( meta, "SOFTWARE/formatter", &data->formatter );
    get_meta_event( meta, "SOFTWARE/loader", &data->loader );
    get_meta_event( meta, "SOFTWARE/update", &data->update );
    get_meta_bam_hdr( &data->bam_hdr, meta );
}


/* ----------------------------------------------------------------------------- */


static const char * get_path_type( const VDBManager *mgr, const char * acc_or_path )
{
    const char * res = PT_NONE;
    int path_type = ( VDBManagerPathType ( mgr, "%s", acc_or_path ) & ~ kptAlias );
    switch ( path_type ) /* types defined in <kdb/manager.h> */
    {
        case kptDatabase      : res = PT_DATABASE; break;

        case kptPrereleaseTbl :
        case kptTable         : res = PT_TABLE; break;
    }
    return res;
}


static rc_t make_remote_file( const KFile ** f, const char * url )
{
    KNSManager * kns_mgr;
    rc_t rc = KNSManagerMake ( & kns_mgr );
    *f = NULL;    
    if ( rc == 0 )
    {
        rc = KNSManagerMakeHttpFile ( kns_mgr, f, NULL, 0x01010000, "%s", url );
        KNSManagerRelease ( kns_mgr );
    }
    return rc;
}


static rc_t make_local_file( const KFile ** f, const char * path )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    *f = NULL;
    if ( rc == 0 )
    {
        rc = KDirectoryOpenFileRead( dir, f, "%s", path );
        KDirectoryRelease( dir );
    }
    return rc;
}


static uint64_t get_file_size( const char * path, bool remotely )
{
    uint64_t res = 0;
    const KFile * f;
    rc_t rc = ( remotely ) ? make_remote_file( &f, path ) : make_local_file( &f, path );
    if ( rc == 0 )
    {
        KFileSize ( f, &res );
        KFileRelease( f );
    }
    return res;
}


static rc_t vdb_info_tab( vdb_info_data * data, VSchema * schema, const VDBManager *mgr )
{
    const VTable * tab;
    rc_t rc = VDBManagerOpenTableRead( mgr, &tab, schema, "%s", data->acc );
    if ( rc == 0 )
    {
        const KMetadata * meta = NULL;

        data->s_platform = get_platform( tab );
        data->seq_rows = get_rowcount( tab );
        get_string_cell( data->species, sizeof data->species, tab, 1, "DEF_LINE" );

        rc = VTableOpenMetadataRead ( tab, &meta );
        if ( rc == 0 )
        {
            get_meta_info( data, meta );
            KMetadataRelease ( meta );
        }

        VTableRelease( tab );
    }
    return rc;
}


static uint64_t get_tab_row_count( const VDatabase * db, const char * table_name )
{
    uint64_t res = 0;
    const VTable * tab;
    rc_t rc = VDatabaseOpenTableRead( db, &tab, table_name );
    if ( rc == 0 )
    {
        res = get_rowcount( tab );
        VTableRelease( tab );
    }
    return res;
}


static void get_species( char * buffer, size_t buffer_size, const VDatabase * db, const VDBManager *mgr )
{
    const VTable * tab;
    rc_t rc = VDatabaseOpenTableRead( db, &tab, "REFERENCE" );
    if ( rc == 0 )
    {
        char seq_id[ 1024 ];
        
        seq_id[ 0 ] = 0;
        get_string_cell( seq_id, sizeof seq_id, tab, 1, "SEQ_ID" );
        VTableRelease( tab );
        if ( seq_id[ 0 ] != 0 )
        {
            rc = VDBManagerOpenTableRead( mgr, &tab, NULL, "%s", seq_id );
            if ( rc == 0 )
            {
                get_string_cell( buffer, buffer_size, tab, 1, "DEF_LINE" );
                VTableRelease( tab );    
            }
        }
    }
}

static rc_t vdb_info_db( vdb_info_data * data, VSchema * schema, const VDBManager *mgr )
{
    const VDatabase * db;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", data->acc );
    if ( rc == 0 )
    {
        const VTable * tab;
        const KMetadata * meta = NULL;

        rc_t rc1 = VDatabaseOpenTableRead( db, &tab, "SEQUENCE" );
        if ( rc1 == 0 )
        {
            data->s_platform = get_platform( tab );
            data->seq_rows = get_rowcount( tab );
            VTableRelease( tab );
        }

        data->ref_rows          = get_tab_row_count( db, "REFERENCE" );
        data->prim_rows         = get_tab_row_count( db, "PRIMARY_ALIGNMENT" );
        data->sec_rows          = get_tab_row_count( db, "SECONDARY_ALIGNMENT" );
        data->ev_rows           = get_tab_row_count( db, "EVIDENCE_ALIGNMENT" );
        data->ev_int_rows       = get_tab_row_count( db, "EVIDENCE_INTERVAL" );
        data->consensus_rows    = get_tab_row_count( db, "CONSENSUS" );
        data->passes_rows       = get_tab_row_count( db, "PASSES" );
        data->metrics_rows      = get_tab_row_count( db, "ZMW_METRICS" );

        if ( data->ref_rows > 0 )
            get_species( data->species, sizeof data->species, db, mgr );
        
        rc = VDatabaseOpenMetadataRead ( db, &meta );
        if ( rc == 0 )
        {
            get_meta_info( data, meta );
            KMetadataRelease ( meta );
        }

        VDatabaseRelease( db );
    }
    return rc;

}


/* ----------------------------------------------------------------------------- */


static rc_t vdb_info_print_xml_s( const char * tag, const char * value )
{
    if ( value[ 0 ] != 0 )
        return KOutMsg( "<%s>%s</%s>\n", tag, value, tag );
    else
        return 0;
}

static rc_t vdb_info_print_xml_uint64( const char * tag, const uint64_t value )
{
    if ( value != 0 )
        return KOutMsg( "<%s>%lu</%s>\n", tag, value, tag );
    else
        return 0;
}


static rc_t vdb_info_print_xml_event( const char * tag, vdb_info_event * event )
{
    rc_t rc = 0;
    if ( event->name[ 0 ] != 0 )
    {
        rc = KOutMsg( "<%s>\n", tag );
        if ( rc == 0 )
            rc = vdb_info_print_xml_s( "NAME", event->name );
        if ( rc == 0 )
            rc = vdb_info_print_xml_s( "VERS", event->vers.s_vers );
        if ( rc == 0 )
            rc = vdb_info_print_xml_s( "TOOLDATE", event->tool_date.date );
        if ( rc == 0 )
            rc = vdb_info_print_xml_s( "RUNDATE", event->run_date.date );
        if ( rc == 0 )
            rc = KOutMsg( "</%s>\n", tag );
    }
    return rc;
}


static rc_t vdb_info_print_xml( vdb_info_data * data )
{
    rc_t rc = KOutMsg( "<info>\n" );

    if ( rc == 0 )
        rc = vdb_info_print_xml_s( "acc", data->acc );
    if ( rc == 0 )
        rc = vdb_info_print_xml_s( "path", data->path );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "size", data->file_size );
    if ( rc == 0 )
        rc = vdb_info_print_xml_s( "type", data->s_path_type );
    if ( rc == 0 )
        rc = vdb_info_print_xml_s( "platf", data->s_platform );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "SEQ", data->seq_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "REF", data->ref_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "PRIM", data->prim_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "SEC", data->sec_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "EVID", data->ev_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "EVINT", data->ev_int_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "CONS", data->consensus_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "PASS", data->passes_rows );
    if ( rc == 0 )
        rc = vdb_info_print_xml_uint64( "METR", data->metrics_rows );

    if ( rc == 0 )
        rc = vdb_info_print_xml_s( "SCHEMA", data->schema_name );

    if ( rc == 0 && data->ts.timestamp != 0 )
    {
        rc = KOutMsg( "<TIMESTAMP>0x%.016x</TIMESTAMP>\n", data->ts.timestamp );
        if ( rc == 0 )
            rc = KOutMsg( "<MONTH>%.02d</MONTH>\n", data->ts.month );
        if ( rc == 0 )
            rc = KOutMsg( "<DAY>%.02d</DAY>\n", data->ts.day );
        if ( rc == 0 )
            rc = KOutMsg( "<YEAR>%.02d</YEAR>\n", data->ts.year );
        if ( rc == 0 )
            rc = KOutMsg( "<HOUR>%.02d</HOUR>\n", data->ts.hour );
        if ( rc == 0 )
            rc = KOutMsg( "<MINUTE>%.02d</MINUTE>\n", data->ts.minute );
    }

    if ( rc == 0 && data->species[ 0 ] != 0 )
        rc = vdb_info_print_xml_s( "SPECIES", data->species );
    
    if ( rc == 0 )
        rc = vdb_info_print_xml_event( "FORMATTER", &data->formatter );
    if ( rc == 0 )
        rc = vdb_info_print_xml_event( "LOADER", &data->loader );
    if ( rc == 0 )
        rc = vdb_info_print_xml_event( "UPDATE", &data->update );

    if ( rc == 0 )
        rc = KOutMsg( "</info>\n" );
    return rc;
}


/* ----------------------------------------------------------------------------- */


static rc_t vdb_info_print_json_s( const char * tag, const char * value )
{
    if ( value[ 0 ] != 0 )
        return KOutMsg( "\"%s\":\"%s\",\n", tag, value );
    else
        return 0;
}

static rc_t vdb_info_print_json_uint64( const char * tag, const uint64_t value )
{
    if ( value != 0 )
        return KOutMsg( "\"%s\":%lu,\n", tag, value );
    else
        return 0;
}

static rc_t vdb_info_print_json_event( const char * tag, vdb_info_event * event )
{
    rc_t rc = 0;
    if ( event->name[ 0 ] != 0 )
    {
        rc = KOutMsg( "\"%s\":{\n", tag );
        if ( rc == 0 )
            rc = vdb_info_print_json_s( "NAME", event->name );
        if ( rc == 0 )
            rc = vdb_info_print_json_s( "VERS", event->vers.s_vers );
        if ( rc == 0 )
            rc = vdb_info_print_json_s( "TOOLDATE", event->tool_date.date );
        if ( rc == 0 )
            rc = vdb_info_print_json_s( "RUNDATE", event->run_date.date );
        if ( rc == 0 )
            rc = KOutMsg( "},\n", tag );
    }
    return rc;
}


static rc_t vdb_info_print_json( vdb_info_data * data )
{
    rc_t rc = KOutMsg( "{\n" );

    if ( rc == 0 )
        rc = vdb_info_print_json_s( "acc", data->acc );
    if ( rc == 0 )
        rc = vdb_info_print_json_s( "path", data->path );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "size", data->file_size );
    if ( rc == 0 )
        rc = vdb_info_print_json_s( "type", data->s_path_type );
    if ( rc == 0 )
        rc = vdb_info_print_json_s( "platf", data->s_platform );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "SEQ", data->seq_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "REF", data->ref_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "PRIM", data->prim_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "SEC", data->sec_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "EVID", data->ev_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "EVINT", data->ev_int_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "CONS", data->consensus_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "PASS", data->passes_rows );
    if ( rc == 0 )
        rc = vdb_info_print_json_uint64( "METR", data->metrics_rows );

    if ( rc == 0 )
        rc = vdb_info_print_json_s( "SCHEMA", data->schema_name );

    if ( rc == 0 && data->ts.timestamp != 0 )
    {
        rc = vdb_info_print_json_uint64( "TIMESTAMP", data->ts.timestamp );
        if ( rc == 0 )
            rc = KOutMsg( "\"MONTH\":%d,\n", data->ts.month );
        if ( rc == 0 )
            rc = KOutMsg( "\"DAY\":%d,\n", data->ts.day );
        if ( rc == 0 )
            rc = KOutMsg( "\"YEAR\":%d,\n", data->ts.year );
        if ( rc == 0 )
            rc = KOutMsg( "\"HOUR\":%d,\n", data->ts.hour );
        if ( rc == 0 )
            rc = KOutMsg( "\"MINUTE\":%d,\n", data->ts.minute );
    }

    if ( rc == 0 && data->species[ 0 ] != 0 )
        rc = vdb_info_print_json_s( "SPECIES", data->species );
    
    if ( rc == 0 )
        rc = vdb_info_print_json_event( "FORMATTER", &data->formatter );
    if ( rc == 0 )
        rc = vdb_info_print_json_event( "LOADER", &data->loader );
    if ( rc == 0 )
        rc = vdb_info_print_json_event( "UPDATE", &data->update );

    if ( rc == 0 )
        rc = KOutMsg( "};\n" );
    return rc;

}


/* ----------------------------------------------------------------------------- */

static const char dflt_event_name[] = "-";

static rc_t vdb_info_print_sep_event( vdb_info_event * event, const char sep, bool last )
{
    rc_t rc;
    const char * ev_name = event->name;
    if ( ev_name == NULL || ev_name[ 0 ] == 0 )
        ev_name = dflt_event_name;
    
    if ( last )
    {
        rc = KOutMsg( "'%s'%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d",
                      ev_name, sep,
                      event->vers.major, sep, event->vers.minor, sep, event->vers.release, sep,
                      event->tool_date.month, sep, event->tool_date.day, sep, event->tool_date.year, sep,
                      event->run_date.month, sep, event->run_date.day, sep, event->run_date.year );
    }
    else
    {
        rc = KOutMsg( "'%s'%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c",
                      ev_name, sep,
                      event->vers.major, sep, event->vers.minor, sep, event->vers.release, sep,
                      event->tool_date.month, sep, event->tool_date.day, sep, event->tool_date.year, sep,
                      event->run_date.month, sep, event->run_date.day, sep, event->run_date.year, sep );
    }
    return rc;
}


static rc_t vdb_info_print_sep( vdb_info_data * data, const char sep )
{
    rc_t rc = KOutMsg( "'%s'%c%lu%c%c%c'%s'%c",
                       data->acc, sep, data->file_size, sep,
                       data->s_path_type[0], sep, &(data->s_platform[13]), sep );
    if ( rc == 0 )
        rc = KOutMsg( "%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c%lu%c",
                      data->seq_rows, sep, data->ref_rows, sep,
                      data->prim_rows, sep, data->sec_rows, sep, data->ev_rows, sep,
                      data->ev_int_rows, sep, data->consensus_rows, sep,
                      data->passes_rows, sep, data->metrics_rows, sep );
    if ( rc == 0 )
        rc = KOutMsg( "'%s'%c%d%c%d%c%d%c%d%c%d%c",
                      data->schema_name, sep,
                      data->ts.month, sep, data->ts.day, sep, data->ts.year, sep,
                      data->ts.hour, sep, data->ts.minute, sep );

    if ( rc == 0 )
    {
        if ( data->species[ 0 ] != 0 )
            rc = KOutMsg( "'%s'%c", data->species, sep );
        else
            rc = KOutMsg( "-%c", sep );
    }
        
    if ( rc == 0 )
        rc = vdb_info_print_sep_event( &data->formatter, sep, false );
    if ( rc == 0 )
        rc = vdb_info_print_sep_event( &data->loader, sep, false );
    if ( rc == 0 )
        rc = vdb_info_print_sep_event( &data->update, sep, true );
        
    if ( rc == 0 )
        rc = KOutMsg( "\n" );

    return rc;
}


/* ----------------------------------------------------------------------------- */


static rc_t vdb_info_print_dflt_date( vdb_info_date * d, const char *prefix0, const char *prefix1 )
{
    rc_t rc = 0;

    if ( d->date[ 0 ] != 0 )
        rc = KOutMsg( "%s%s: %s (%d/%d/%d %d:%d)\n", prefix0, prefix1,
                      d->date, d->month, d->day, d->year, d->hour, d->minute );

    return rc;
}

static rc_t vdb_info_print_dflt_event( vdb_info_event * event, const char *prefix )
{
    rc_t rc = 0;
    if ( event->name[ 0 ] != 0 )
        rc = KOutMsg( "%s    : %s\n", prefix, event->name );
    if ( rc == 0 && event->vers.s_vers[ 0 ] != 0 )
        rc = KOutMsg( "%sVER : %s\n", prefix, event->vers.s_vers );
    if ( rc == 0 )
        rc = vdb_info_print_dflt_date( &event->tool_date, prefix, "DATE" );
    if ( rc == 0 )
        rc = vdb_info_print_dflt_date( &event->run_date,  prefix, "RUN " );
    return rc;
}


static rc_t vdb_info_print_dflt( vdb_info_data * data )
{
    rc_t rc= KOutMsg( "acc    : %s\n", data->acc );

    if ( rc == 0 && data->path[ 0 ] != 0 )
        rc = KOutMsg( "path   : %s\n", data->path );

    if ( rc == 0 && data->remote_path[ 0 ] != 0 )
        rc = KOutMsg( "remote : %s\n", data->remote_path );

    if ( rc == 0 && data->file_size != 0 )
        rc = KOutMsg( "size   : %,lu\n", data->file_size );

    if ( rc == 0 && data->cache[ 0 ] != 0 )
    {
        rc = KOutMsg( "cache  : %s\n", data->cache );
        if ( rc == 0 )
            rc = KOutMsg( "percent: %f\n", data->cache_percent );
        if ( rc == 0 )
            rc = KOutMsg( "bytes  : %,lu\n", data->bytes_in_cache );
    }
    
    if ( rc == 0 && data->s_path_type[ 0 ] != 0 )    
        rc = KOutMsg( "type   : %s\n", data->s_path_type );

    if ( rc == 0 && data->s_platform[ 0 ] != 0 )
        rc = KOutMsg( "platf  : %s\n", data->s_platform );

    if ( rc == 0 && data->seq_rows != 0 )
        rc = KOutMsg( "SEQ    : %,lu\n", data->seq_rows );

    if ( rc == 0 && data->ref_rows != 0 )
        rc = KOutMsg( "REF    : %,lu\n", data->ref_rows );

    if ( rc == 0 && data->prim_rows != 0 )
        rc = KOutMsg( "PRIM   : %,lu\n", data->prim_rows );

    if ( rc == 0 && data->sec_rows != 0 )
        rc = KOutMsg( "SEC    : %,lu\n", data->sec_rows );

    if ( rc == 0 && data->ev_rows != 0 )
        rc = KOutMsg( "EVID   : %,lu\n", data->ev_rows );

    if ( rc == 0 && data->ev_int_rows != 0 )
        rc = KOutMsg( "EVINT  : %,lu\n", data->ev_int_rows );

    if ( rc == 0 && data->consensus_rows != 0 )
        rc = KOutMsg( "CONS   : %,lu\n", data->consensus_rows );

    if ( rc == 0 && data->passes_rows != 0 )
        rc = KOutMsg( "PASS   : %,lu\n", data->passes_rows );

    if ( rc == 0 && data->metrics_rows != 0 )
        rc = KOutMsg( "METR   : %,lu\n", data->metrics_rows );

    if ( rc == 0 && data->schema_name[ 0 ] != 0 )
        rc = KOutMsg( "SCHEMA : %s\n", data->schema_name );

    if ( rc == 0 && data->ts.timestamp != 0 )
        rc = KOutMsg( "TIME   : 0x%.016x (%.02d/%.02d/%d %.02d:%.02d)\n",
                      data->ts.timestamp, data->ts.month, data->ts.day, data->ts.year,
                      data->ts.hour, data->ts.minute );

    if ( rc == 0 && data->species[ 0 ] != 0 )
        rc = KOutMsg( "SPECIES: %s\n", data->species );
        
    if ( rc == 0 )
        vdb_info_print_dflt_event( &data->formatter, "FMT" );
    if ( rc == 0 )
        vdb_info_print_dflt_event( &data->loader, "LDR" );
    if ( rc == 0 )
        vdb_info_print_dflt_event( &data->update, "UPD" );

    if ( rc == 0 && data->bam_hdr.present )
    {
        rc = KOutMsg( "BAMHDR : %d bytes / %d lines\n", data->bam_hdr.hdr_bytes, data->bam_hdr.total_lines );
        if ( rc == 0 && data->bam_hdr.HD_lines > 0 )
            rc = KOutMsg( "BAMHDR : %d HD-lines\n", data->bam_hdr.HD_lines );
        if ( rc == 0 && data->bam_hdr.SQ_lines > 0 )
            rc = KOutMsg( "BAMHDR : %d SQ-lines\n", data->bam_hdr.SQ_lines );
        if ( rc == 0 && data->bam_hdr.RG_lines > 0 )
            rc = KOutMsg( "BAMHDR : %d RG-lines\n", data->bam_hdr.RG_lines );
        if ( rc == 0 && data->bam_hdr.PG_lines > 0 )
            rc = KOutMsg( "BAMHDR : %d PG-lines\n", data->bam_hdr.PG_lines );
    }
    
    return rc;
}


/* ----------------------------------------------------------------------------- */

static rc_t vdb_info_print_sql_event( const char * prefix, bool last )
{
    rc_t rc = KOutMsg( "%s_NAME VARCHAR, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_VER_MAJOR INTEGER, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_VER_MINOR INTEGER, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_VER_RELEASE INTEGER, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_TOOL_MONTH INTEGER, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_TOOL_DAY INTEGER, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_TOOL_YEAR INTEGER, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_RUN_MONTH INTEGER, ", prefix );
    if ( rc == 0 )
        rc = KOutMsg( "%s_RUN_DAY INTEGER, ", prefix );

    if ( rc == 0 )
    {
        if ( last )
            rc = KOutMsg( "%s_RUN_YEAR INTEGER ", prefix );
        else
            rc = KOutMsg( "%s_RUN_YEAR INTEGER, ", prefix );
    }
    return rc;
}

static rc_t vdb_info_print_sql_header( const char * table_name )
{
    rc_t rc = KOutMsg( "CREATE TABLE %s ( ", table_name );
    if ( rc == 0 )
        rc = KOutMsg( "ACC VARCHAR(12) PRIMARY KEY, " );
    if ( rc == 0 )
        rc = KOutMsg( "FILESIZE INTEGER, " );

    if ( rc == 0 )
        rc = KOutMsg( "TAB_OR_DB VARCHAR(1), " );
    if ( rc == 0 )
        rc = KOutMsg( "PLATFORM VARCHAR(16), " );
    if ( rc == 0 )
        rc = KOutMsg( "SEQ_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "REF_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "PRIM_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "SEC_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "EV_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "EV_INT_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "CONS_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "PASS_ROWS INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "METR_ROWS INTEGER, " );

    if ( rc == 0 )
        rc = KOutMsg( "SCHEMA VARCHAR, " );

    if ( rc == 0 )
        rc = KOutMsg( "TS_MONTH INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "TS_DAY INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "TS_YEAR INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "TS_HOUR INTEGER, " );
    if ( rc == 0 )
        rc = KOutMsg( "TS_MINUTE INTEGER, " );

    if ( rc == 0 )
        vdb_info_print_sql_event( "FMT", false );
    if ( rc == 0 )
        vdb_info_print_sql_event( "LD", false );
    if ( rc == 0 )
        vdb_info_print_sql_event( "UPD", true );

    if ( rc == 0 )
        rc = KOutMsg( ");\n" );

    return rc;
}


static rc_t vdb_info_print_ev_sql( vdb_info_event * event, bool last )
{
    rc_t rc;
    if ( last )
    {
        rc = KOutMsg( "\'%s\', %d, %d, %d, %d, %d, %d, %d, %d, %d ",
                    event->name, event->vers.major, event->vers.minor, event->vers.release,
                    event->tool_date.month, event->tool_date.day, event->tool_date.year,
                    event->run_date.month, event->run_date.day, event->run_date.year );
    }
    else
    {
        rc = KOutMsg( "\'%s\', %d, %d, %d, %d, %d, %d, %d, %d, %d, ",
                    event->name, event->vers.major, event->vers.minor, event->vers.release,
                    event->tool_date.month, event->tool_date.day, event->tool_date.year,
                    event->run_date.month, event->run_date.day, event->run_date.year );
    }
    return rc;
}


static rc_t vdb_info_print_sql( const char * table_name, vdb_info_data * data )
{
    rc_t rc = KOutMsg( "INSERT INTO %s VALUES ( ", table_name );

    if ( rc == 0 )
        rc= KOutMsg( "\'%s\', ", data->acc );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->file_size );
    if ( rc == 0 )
        rc = KOutMsg( "\'%c\', ", data->s_path_type[0] );
    if ( rc == 0 )
        rc = KOutMsg( "\'%s\', ", &( data->s_platform[13] ) );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->seq_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->ref_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->prim_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->sec_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->ev_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->ev_int_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->consensus_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->passes_rows );
    if ( rc == 0 )
        rc = KOutMsg( "%lu, ", data->metrics_rows );

    if ( rc == 0 )
        rc = KOutMsg( "\'%s\', ", data->schema_name );

    if ( rc == 0 )
        rc = KOutMsg( "%d, %d, %d, %d, %d, ",
                      data->ts.month, data->ts.day, data->ts.year, data->ts.hour, data->ts.minute );

    if ( rc == 0 )
        rc = vdb_info_print_ev_sql( &data->formatter, false );
    if ( rc == 0 )
        rc = vdb_info_print_ev_sql( &data->loader, false );
    if ( rc == 0 )
        rc = vdb_info_print_ev_sql( &data->update, true );

    if ( rc == 0 )
        rc = KOutMsg( ");\n" );

    return rc;
}


/* ----------------------------------------------------------------------------- */


static uint8_t digits_of( uint64_t value )
{
    uint8_t res = 0;
          if ( value > 99999999999 ) res = 12;
    else if ( value > 9999999999 ) res = 11;
    else if ( value > 999999999 ) res = 10;
    else if ( value > 99999999 ) res = 9;
    else if ( value > 9999999 ) res = 8;
    else if ( value > 999999 ) res = 7;
    else if ( value > 99999 ) res = 6;
    else if ( value > 9999 ) res = 5;
    else if ( value > 999 ) res = 4;
    else if ( value > 99 ) res = 3;
    else if ( value > 9 ) res = 2;
    else res = 1;
    return res;
}

static rc_t vdb_info_1( VSchema * schema, dump_format_t format, const VDBManager *mgr,
                        const char * acc_or_path, const char * table_name )
{
    rc_t rc = 0;
    vdb_info_data data;

    memset( &data, 0, sizeof data );
    data.s_platform = PT_NONE;
    data.acc = acc_or_path;

    /* #1 get path-type */
    data.s_path_type = get_path_type( mgr, acc_or_path );

    if ( data.s_path_type[ 0 ] == 'D' || data.s_path_type[ 0 ] == 'T' )
    {
        rc_t rc1;

        /* #2 fork by table or database */
        switch ( data.s_path_type[ 0 ] )
        {
            case 'D' : vdb_info_db( &data, schema, mgr ); break;
            case 'T' : vdb_info_tab( &data, schema, mgr ); break;
        }

        /* try to resolve the path locally */
        rc1 = resolve_accession( acc_or_path, data.path, sizeof data.path, false ); /* vdb-dump-helper.c */
        if ( rc1 == 0 )
        {
            data.file_size = get_file_size( data.path, false );
            resolve_remote_accession( acc_or_path, data.remote_path, sizeof data.remote_path ); /* vdb-dump-helper.c */
        }
        else
        {
            /* try to resolve the path remotely */
            rc1 = resolve_accession( acc_or_path, data.path, sizeof data.path, true ); /* vdb-dump-helper.c */
            if ( rc1 == 0 )
            {
                data.file_size = get_file_size( data.path, true );
                /* try to find out the cache-file */
                rc1 = resolve_cache( acc_or_path, data.cache, sizeof data.cache ); /* vdb-dump-helper.c */
                if ( rc1 == 0 )
                {
                    /* try to find out cache completeness */
                    check_cache_comleteness( data.cache, &data.cache_percent, &data.bytes_in_cache );
                }
            }
        }
        
        switch ( format )
        {
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
               const char * acc_or_path, struct num_gen * rows )
{
    rc_t rc = 0;
    VSchema * schema = NULL;

    vdh_parse_schema( mgr, &schema, schema_list, false );

    if ( format == df_sql )
        rc = vdb_info_print_sql_header( acc_or_path );

    if ( rows != NULL && !num_gen_empty( rows ) )
    {
        const struct num_gen_iter * iter;
        rc = num_gen_iterator_make( rows, &iter );
        if ( rc == 0 )
        {
            int64_t max_row;
            rc = num_gen_iterator_max( iter, &max_row );
            if ( rc == 0 )
            {
                int64_t id;
                uint8_t digits = digits_of( max_row );

                while ( rc == 0 && num_gen_iterator_next( iter, &id, &rc ) )
                {
                    char acc[ 64 ];
                    size_t num_writ;
                    rc_t rc1 = -1;
                    switch ( digits )
                    {
                        case 1 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%ld", acc_or_path, id ); break;
                        case 2 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.02ld", acc_or_path, id ); break;
                        case 3 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.03ld", acc_or_path, id ); break;
                        case 4 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.04ld", acc_or_path, id ); break;
                        case 5 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.05ld", acc_or_path, id ); break;
                        case 6 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.06ld", acc_or_path, id ); break;
                        case 7 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.07ld", acc_or_path, id ); break;
                        case 8 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.08ld", acc_or_path, id ); break;
                        case 9 : rc1 = string_printf ( acc, sizeof acc, &num_writ, "%s%.09ld", acc_or_path, id ); break;
                    }

                    if ( rc1 == 0 )
                        rc = vdb_info_1( schema, format, mgr, acc, acc_or_path );
                }
            }
            num_gen_iterator_destroy( iter );
        }
    }
    else
        rc = vdb_info_1( schema, format, mgr, acc_or_path, acc_or_path );

    if ( schema != NULL )
        VSchemaRelease( schema );

    return rc;
}
