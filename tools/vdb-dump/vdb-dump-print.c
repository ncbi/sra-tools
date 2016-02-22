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

#include "vdb-dump-print.h"
#include "vdb-dump-helper.h"

#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/out.h>
#include <klib/pack.h>
#include <klib/num-gen.h>

#include <kfs/directory.h>

#include <kdb/manager.h>

#include <vdb/cursor.h>
#include <vdb/table.h>
#include <vdb/database.h>

#include <stdlib.h>
#include <string.h>
#include <bitstr.h>

typedef struct vdp_context
{
    const void * base;
    const VTypedesc * type_desc;
    vdp_opts * opts;
    char * buf;
    size_t buf_size;
    size_t printed_so_far;
    bool buf_filled;

    uint32_t elem_idx;
    uint32_t elem_bits;
    uint32_t boff;
    uint32_t row_len;
    int32_t selection;
    uint64_t offset_in_bits;
    bool print_dna_bases;
} vdp_context;


static rc_t vdp_print( vdp_context * vdp_ctx, const char * fmt, ... )
{
    rc_t rc = 0;
    if ( vdp_ctx->buf_size > vdp_ctx->printed_so_far )
    {
        va_list args;
        size_t num_writ;
        size_t available = ( vdp_ctx->buf_size - vdp_ctx->printed_so_far );

        va_start ( args, fmt );
        rc = string_vprintf ( &( vdp_ctx->buf[ vdp_ctx->printed_so_far ]), available, &num_writ, fmt, args );
        vdp_ctx->printed_so_far += num_writ;
        va_end ( args );
    }

    if ( vdp_ctx->buf_size <= vdp_ctx->printed_so_far )
        vdp_ctx->buf_filled = true;
    return rc;
}


static rc_t vdp_print_string( vdp_context * vdp_ctx, const char * s )
{
    rc_t rc = 0;
    if ( vdp_ctx->buf == NULL )
    {
        rc = KOutMsg( s );
    }
    else
    {
        if ( vdp_ctx->buf_size > vdp_ctx->printed_so_far )
        {
            size_t num_writ;
            size_t available = ( vdp_ctx->buf_size - vdp_ctx->printed_so_far );

            rc = string_printf ( &( vdp_ctx->buf[ vdp_ctx->printed_so_far ]), available, &num_writ, s );
            vdp_ctx->printed_so_far += num_writ;
        }

        if ( vdp_ctx->buf_size <= vdp_ctx->printed_so_far )
            vdp_ctx->buf_filled = true;
    }
    return rc;
}

typedef rc_t( * vdp_fkt )( vdp_context * vdp_ctx );


#define BYTE_OFFSET(VALUE)  ( (VALUE) >> 3 )
#define BIT_OFFSET(VALUE)   ( (VALUE) & 0x7 )


static uint8_t BitLength2Bytes[65] =
{
         /* 0  1  2  3  4  5  6  7  8  9*/
   /* 0 */  0, 1, 1, 1, 1, 1, 1, 1, 1, 2,
   /* 1 */  2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
   /* 2 */  3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
   /* 3 */  4, 4, 4, 5, 5, 5, 5, 5, 5, 5,
   /* 4 */  5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
   /* 5 */  7, 7, 7, 7, 7, 7, 7, 8, 8, 8,
   /* 6 */  8, 8, 8, 8, 8
};

/*************************************************************************************
n_bits   [IN] ... number of bits

calculates the number of bytes that have to be copied to contain the given
number of bits
*************************************************************************************/
static uint16_t vdp_bitlength_2_bytes( const size_t n_bits )
{
    if ( n_bits > 64 )
        return 8;
    else
        return BitLength2Bytes[ n_bits ];
}


static uint64_t BitLength2Mask[33] =
{
   /* 0 */ 0x00,
   /* 1 ..  4 */  0x1,                0x3,                0x7,                0xF,
   /* 5 ..  8 */  0x1F,               0x3F,               0x7F,               0xFF,
   /* 9 .. 12 */  0x1FF,              0x3FF,              0x7FF,              0xFFF,
   /*13 .. 16 */  0x1FFF,             0x3FFF,             0x7FFF,             0xFFFF,
   /*17 .. 20 */  0x1FFFF,            0x3FFFF,            0x7FFFF,            0xFFFFF,
   /*21 .. 24 */  0x1FFFFF,           0x3FFFFF,           0x7FFFFF,           0xFFFFFF,
   /*25 .. 28 */  0x1FFFFFF,          0x3FFFFFF,          0x7FFFFFF,          0xFFFFFFF,
   /*29 .. 32 */  0x1FFFFFFF,         0x3FFFFFFF,         0x7FFFFFFF,         0xFFFFFFFF
 };


/*************************************************************************************
n_bits   [IN] ... number of bits

creates a bitmask to mask exactly the given number of bits from a longer value
*************************************************************************************/
static uint64_t vdp_bitlength_2_mask( const size_t n_bits )
{
    uint64_t res;
    if ( n_bits < 33 )
        res = BitLength2Mask[ n_bits ];
    else
    {
        if ( n_bits < 65 )
            res = BitLength2Mask[ n_bits-32 ];
        else
            res = 0xFFFFFFFF;
        res <<= 32;
        res |= 0xFFFFFFFF;
    }
    return res;
}


static void vdp_move_to_value( void* dst, vdp_context * vdp_ctx, const uint32_t n_bits )
{
    char *src_ptr = ( char* )vdp_ctx->buf + BYTE_OFFSET( vdp_ctx->offset_in_bits );
    if ( BIT_OFFSET( vdp_ctx->offset_in_bits ) == 0 )
    {
        memmove( dst, src_ptr, vdp_bitlength_2_bytes( n_bits ) );
    }
    else
    {
        bitcpy ( dst, 0, src_ptr, BIT_OFFSET( vdp_ctx->offset_in_bits ), n_bits );
    }
}


static uint64_t vdp_move_to_uint64( vdp_context * vdp_ctx )
{
    uint64_t value = 0;
    uint32_t n_bits = vdp_ctx->type_desc->intrinsic_bits;
    vdp_move_to_value( &value, vdp_ctx, n_bits );
    if ( n_bits & 7 )
    {
        size_t unpacked = 0;
        Unpack( n_bits, sizeof( value ), &value, 0, n_bits, NULL, &value, sizeof( value ), &unpacked );
    }
    value &= vdp_bitlength_2_mask( n_bits );
    vdp_ctx->offset_in_bits += n_bits;
    return value;
}


static rc_t vdp_boolean( vdp_context * vdp_ctx )
{
    rc_t rc;
    uint64_t value = vdp_move_to_uint64( vdp_ctx );
    switch( vdp_ctx->opts->c_boolean )
    {
    case '1' :  if ( value == 0 )
                    rc = vdp_print_string( vdp_ctx, "0" );
                else
                    rc = vdp_print_string( vdp_ctx, "1" );
                break;
    case 'T' :  if ( value == 0 )
                    rc = vdp_print_string( vdp_ctx, "F" );
                else
                    rc = vdp_print_string( vdp_ctx, "T" );
                break;

    default  :  if ( value == 0 )
                    rc = vdp_print_string( vdp_ctx, "false" );
                else
                    rc = vdp_print_string( vdp_ctx, "true" );
                break;
    }
    return rc;
}


static const char * uint_hex_fmt = "0x%lX";
static const char * uint_dec_fmt = "%lu";
static const char * int_dec_fmt = "%ld";

static rc_t vdp_uint( vdp_context * vdp_ctx )
{
    rc_t rc = 0;
    uint64_t value = vdp_move_to_uint64( vdp_ctx );
    if ( ( vdp_ctx->opts->without_sra_types == false )/*&&( def->value_trans_fct != NULL )*/ )
    {
/*
        const char *txt = def->value_trans_fct( (uint32_t)value );
        rc = vds_append_str( s, txt );
*/
    }
    else
    {
        const char * fmt;
        if ( vdp_ctx->opts->in_hex )
            fmt = uint_hex_fmt;
        else
            fmt = uint_dec_fmt;

        if ( vdp_ctx->buf == NULL )
            rc = KOutMsg( fmt, value );
        else
            rc = vdp_print( vdp_ctx, fmt, value );
    }
    return rc;
}


static rc_t vdp_int( vdp_context * vdp_ctx )
{
    rc_t rc = 0;
    int64_t value = (int64_t)vdp_move_to_uint64( vdp_ctx );
    if ( ( vdp_ctx->opts->without_sra_types == false )/*&&( def->value_trans_fct != NULL )*/ )
    {
/*
        const char *txt = def->value_trans_fct( (uint32_t)value );
        rc = vds_append_str( s, txt );
*/
    }
    else
    {
        const char * fmt;

        switch ( vdp_ctx->type_desc->intrinsic_bits )
        {
            case  8 : { int8_t temp = (int8_t)value;
                        value = temp; }
                      break;
            case 16 : { int16_t temp = (int16_t)value;
                        value = temp; }
                      break;
            case 32 : { int32_t temp = (int32_t)value;
                        value = temp; }
                      break;
        }

        if ( vdp_ctx->opts->in_hex )
            fmt = uint_hex_fmt;
        else
            fmt = int_dec_fmt;

        if ( vdp_ctx->buf == NULL )
            rc = KOutMsg( fmt, value );
        else
            rc = vdp_print( vdp_ctx, fmt, value );
    }
    return rc;
}


#define BITSIZE_OF_FLOAT ( sizeof(float) * 8 )
#define BITSIZE_OF_DOUBLE ( sizeof(double) * 8 )
static const char * float_fmt = "%e";
static const char * unknown_float_fmt = "unknown float-type";

static rc_t vdp_float( vdp_context * vdp_ctx )
{
    rc_t rc;
    if ( vdp_ctx->opts->in_hex )
        rc = vdp_int( vdp_ctx );
    else
    {
        uint32_t n_bits = vdp_ctx->type_desc->intrinsic_bits;
        if ( n_bits == BITSIZE_OF_FLOAT )
        {
            float value;
            vdp_move_to_value( &value, vdp_ctx, n_bits );
            if ( vdp_ctx->buf == NULL )
                rc = KOutMsg( float_fmt, value );
            else
                rc = vdp_print( vdp_ctx, float_fmt, value );
        }
        else if ( n_bits == BITSIZE_OF_DOUBLE )
        {
            double value;
            vdp_move_to_value( &value, vdp_ctx, n_bits );
            if ( vdp_ctx->buf == NULL )
                rc = KOutMsg( float_fmt, value );
            else
                rc = vdp_print( vdp_ctx, float_fmt, value );
        }
        else
        {
            rc = vdp_print_string( vdp_ctx, unknown_float_fmt );
        }
        vdp_ctx->offset_in_bits += n_bits;
    }
    return rc;
}


static const char * txt_fmt = "%.*s";

static rc_t vdp_txt_ascii( vdp_context * vdp_ctx )
{
    rc_t rc;
    char *src_ptr = (char*)vdp_ctx->buf + BYTE_OFFSET( vdp_ctx->offset_in_bits );
    if ( vdp_ctx->buf == NULL )
        rc = KOutMsg( txt_fmt, vdp_ctx->row_len, src_ptr );
    else
        rc = vdp_print( vdp_ctx, txt_fmt, vdp_ctx->row_len, src_ptr );
    return rc;
}


static rc_t vdp_hex_char( char * temp, uint32_t * idx, const uint8_t c )
{
    char s[ 8 ];
    size_t num_writ;
    rc_t rc = string_printf ( s, sizeof s, &num_writ, "%X ", c );
    if ( rc == 0 )
    {
        size_t i;
        for ( i = 0; i < num_writ; ++i )
            temp[ (*idx)++ ] = s[ i ];
    }
    return rc;
}


static rc_t vdp_hex_ascii( vdp_context * vdp_ctx )
{
    rc_t rc = 0;
    char *src_ptr = (char*)vdp_ctx->buf + BYTE_OFFSET( vdp_ctx->offset_in_bits );
    char *tmp = malloc( ( vdp_ctx->row_len + 1 ) * 4 );
    if ( tmp != NULL )
    {
        uint32_t i, dst = 0;
        for ( i = 0; i < vdp_ctx->row_len && rc == 0; ++i )
            rc = vdp_hex_char( tmp, &dst, src_ptr[ i ] );
        src_ptr[ dst ] = 0;
        if ( rc == 0 )
        {
            if ( vdp_ctx->buf == NULL )
                rc = KOutMsg( txt_fmt, dst, tmp );
            else
                rc = vdp_print( vdp_ctx, txt_fmt, dst, tmp );
        }
        free( tmp );
    }
    else
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    return rc;
}


static rc_t vdp_ascii( vdp_context * vdp_ctx )
{
    rc_t rc;

    if ( vdp_ctx->opts->in_hex )
        rc = vdp_hex_ascii( vdp_ctx );
    else
        rc = vdp_txt_ascii( vdp_ctx );

    if ( rc == 0 )
    {
        vdp_ctx->elem_idx += vdp_ctx->row_len;
        vdp_ctx->offset_in_bits += ( vdp_ctx->type_desc->intrinsic_bits * vdp_ctx->row_len );
    }
    return rc;
}


static rc_t vdp_unicode( vdp_context * vdp_ctx )
{
    return vdp_ascii( vdp_ctx );
}


/* !!!!!!!! this depends on how domains are defined in "schema.h" */
vdp_fkt vdp_dispatch[] =
{
    vdp_boolean,
    vdp_uint,
    vdp_int,
    vdp_float,
    vdp_ascii,
    vdp_unicode
};


static rc_t vdp_print_elem( vdp_context * vdp_ctx )
{
    rc_t rc = 0;

    return rc;
}


rc_t vdp_print_cell_cmn( char * buf, size_t buf_size, size_t *num_written,
                         const uint32_t elem_bits, const void * base, uint32_t boff, uint32_t row_len,
                         const VTypedesc * type_desc, vdp_opts * opts )
{
    rc_t rc = 0;
    vdp_context vdp_ctx;
    vdp_ctx.selection = ( type_desc->domain - 1 );
    if ( vdp_ctx.selection < 0 || 
         vdp_ctx.selection > ( sizeof vdp_dispatch / sizeof vdp_dispatch[ 0 ] ) )
    {
        rc = RC( rcVDB, rcNoTarg, rcVisiting, rcOffset, rcInvalid );
    }
    else
    {
        vdp_ctx.base = base;
        vdp_ctx.opts = opts;
        vdp_ctx.type_desc = type_desc;
        vdp_ctx.elem_bits = elem_bits;
        vdp_ctx.boff = boff;
        vdp_ctx.row_len = row_len;
        vdp_ctx.elem_idx = 0;

        vdp_ctx.buf = buf;
        vdp_ctx.buf_size = buf_size;
        vdp_ctx.printed_so_far = 0;
        vdp_ctx.buf_filled = false;
        vdp_ctx.offset_in_bits = 0;

        if ( ( type_desc->domain < vtdBool ) || ( type_desc->domain > vtdUnicode ) )
        {
            rc = vdp_print_string( &vdp_ctx, "unknown data-type" );
        }
        else
        {
            bool print_comma = true;

            /* hardcoded printing of dna-bases if the column-type fits */
            vdp_ctx.print_dna_bases = ( opts->print_dna_bases &
                        ( type_desc->intrinsic_dim == 2 ) &
                        ( type_desc->intrinsic_bits == 1 ) );

            if ( ( type_desc->domain == vtdBool ) && opts->c_boolean )
            {
                print_comma = false;
            }

            while( ( vdp_ctx.elem_idx < row_len ) && ( rc == 0 ) && ( !vdp_ctx.buf_filled ) )
            {
                uint32_t eidx = vdp_ctx.elem_idx;

                if ( ( eidx > 0 )&& ( vdp_ctx.print_dna_bases == false ) && print_comma )
                {
                    rc = vdp_print_string( &vdp_ctx, ", " );
                }

                /* dumps the basic data-types, implementation above
                   >>> that means it appends or prints to stdout the element-string
                   inc the vdb_ctx.element_idx by: 1...bool/int/uint/float
                                                   n...string/unicode-string */
                rc = vdp_print_elem( &vdp_ctx );

                /* insurance against endless loop */
                if ( eidx == vdp_ctx.elem_idx )
                {
                    vdp_ctx.elem_idx++;
                }
            }
        }
    }
    return rc;
}


rc_t vdp_print_cell_2_buffer( char * buffer, size_t bufsize, size_t *num_written,
                              const uint32_t elem_bits, const void * base, uint32_t boff, uint32_t row_len,
                              const VTypedesc * type_desc, vdp_opts * opts )
{
    rc_t rc = 0;

    if ( base == NULL || type_desc == NULL || buffer == NULL || opts == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcVisiting, rcParam, rcNull );
    }
    else
    {
        rc = vdp_print_cell_cmn( buffer, bufsize, num_written, elem_bits, base, boff, row_len, type_desc, opts );
    }
    return rc;
}


rc_t vdp_print_cell( const uint32_t elem_bits, const void * base, uint32_t boff, uint32_t row_len,
                     const VTypedesc * type_desc, vdp_opts * opts )
{
    rc_t rc = 0;

    if ( base == NULL || type_desc == NULL || opts == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcVisiting, rcParam, rcNull );
    }
    else
    {
        rc = vdp_print_cell_cmn( NULL, 0, NULL, elem_bits, base, boff, row_len, type_desc, opts );
    }
    return rc;
}

/* -----------------------------------------------------------------------------------------------*/


/* -----------------------------------------------------------------------------------------------*/

typedef struct vdp_src_context
{
    KDirectory *dir;
    const VDBManager *mgr;
    VSchema *schema;
    Vector sources;
    bool print_info;
} vdp_src_context;


typedef struct vdp_database
{
    const String * name;
    const VDatabase *database;
    Vector sub_databases;
    Vector sub_tables;
} vdp_database;

typedef struct vdp_table
{
    const String * name;
    const VTable *table;
    const VCursor *cursor;
    Vector columns;
} vdp_table;


typedef struct vdp_column
{
    const String * name;
    uint32_t id;
    vdp_table * tab;
} vdp_column;


typedef struct vdp_source
{
    const String * path;
    int path_type;
    vdp_table * tbl;
    vdp_database * db;
} vdp_source;


/* -----------------------------------------------------------------------------------------------*/

static void CC release_column( void *item, void * data )
{
    vdp_column * c = ( vdp_column * )item;
    if ( c != NULL )
        StringWhack ( c->name );
    free( item );
}

static rc_t vdp_add_column( vdp_table * tbl, const String * name, bool print_info )
{
    rc_t rc = 0;
    vdp_column * col = malloc( sizeof * col );
    if ( col == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        KOutMsg( "vdp_add_column( '%S' ) -> %R\n", name, rc );
    }
    else
    {
        rc = StringCopy( &col->name, name );
        if ( rc != 0 )
        {
            free( ( void * ) col );
            KOutMsg( "StringCopy( '%S' ) -> %R\n", name, rc );
        }
        else
        {
            col->tab = tbl;
            rc = VCursorAddColumn( tbl->cursor, &col->id, "%s", name->addr );
            if ( rc != 0 )
                KOutMsg( "VCursorAddColumn( '%S.%S' ) -> %R\n", tbl->name, name, rc );
            else
                rc = VectorAppend( &tbl->columns, NULL, col );

            if ( rc != 0 )
                release_column( col, NULL );
            else if ( print_info )
                KOutMsg( "column: '%S.%S' added\n", tbl->name, name );
        }
    }
    return rc;
}

/* -----------------------------------------------------------------------------------------------*/

static void CC release_table( void *item, void * data )
{
    vdp_table * tbl = ( vdp_table * ) item;
    if ( tbl != NULL )
    {
        VectorWhack( &tbl->columns, release_column, NULL );
        StringWhack ( tbl->name );
        VCursorRelease( tbl->cursor );
        VTableRelease ( tbl->table );
        free( item );
    }
}

/* we can open a table from an accession or from a database... */
static rc_t vdp_open_table( vdp_src_context * vctx,
                            vdp_source * acc, vdp_database * parent_db, const String * name )
{
    rc_t rc = 0;
    vdp_table * tbl = malloc( sizeof * tbl );
    if ( tbl == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        KOutMsg( "vdp_open_table( '%S' ) -> %R\n", name, rc );
    }
    else
    {
        rc = StringCopy( &tbl->name, name );
        if ( rc != 0 )
        {
            free( ( void * ) tbl );
            KOutMsg( "StringCopy( '%S' ) -> %R\n", name, rc );
        }
        else
        {
            VectorInit( &tbl->columns, 0, 20 );
            
            /* open the table: either from manager or from database */
            if ( acc != NULL )
                rc = VDBManagerOpenTableRead( vctx->mgr, &tbl->table, vctx->schema, "%s", name->addr );
            else
                rc = VDatabaseOpenTableRead( parent_db->database, &tbl->table, "%s", name->addr );
            
            /* enumerate columns, create cursor, add columns */
            if ( rc == 0 )
            {
                rc = VTableCreateCursorRead( tbl->table, &tbl->cursor );
                if ( rc == 0 )
                {
                    KNamelist * column_names;
                    rc = VTableListCol( tbl->table, &column_names );
                    if ( rc == 0 )
                    {
                        uint32_t count, idx;
                        rc = KNamelistCount( column_names, &count );
                        for ( idx = 0; rc == 0 && idx < count; ++idx )
                        {
                            const char * column_name;
                            rc = KNamelistGet( column_names, idx, &column_name );
                            if ( rc == 0 && column_name != NULL )
                            {
                                String temp_str;
                                StringInitCString( &temp_str, column_name );
                                rc = vdp_add_column( tbl, &temp_str, vctx->print_info );
                            }
                        }
                        KNamelistRelease( column_names );
                    }
                    rc = VCursorOpen( tbl->cursor );
                }
            }
            
            if ( rc == 0 )
            {
                /* enter the new object: either into source-struct or parent-db */
                if ( acc != NULL )
                    acc->tbl = tbl;
                else
                    rc = VectorAppend( &parent_db->sub_tables, NULL, tbl );
            }

            if ( rc != 0 )
                release_table( tbl, NULL );
            else if ( vctx->print_info )
                KOutMsg( "table: '%S' opened\n", name );
                
        }
    }
    return rc;
}

static rc_t vdp_table_adjust_ranges( vdp_table * tbl, struct num_gen * ranges )
{
    rc_t rc = 0;
    if ( tbl == NULL || ranges == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        int64_t first;
        uint64_t count;
        rc = VCursorIdRange( tbl->cursor, 0, &first, &count );
        if ( rc != 0 )
            KOutMsg( "VCursorIdRange( %S ) -> %R\n", tbl->name, rc );
        else
        {
            if ( num_gen_empty( ranges ) )
            {
                rc = num_gen_add( ranges, first, count );
                if ( rc != 0 )
                    KOutMsg( "tbl '%S' : num_gen_add( %d, %d ) -> %R\n", tbl->name, first, count, rc );
            }
            else
            {
                rc = num_gen_trim( ranges, first, count );
                if ( rc != 0 )
                    KOutMsg( "tbl '%S' : num_gen_trim( %d, %d ) -> %R\n", tbl->name, first, count, rc );
            }
        }
    }
    return rc;
}

/* -----------------------------------------------------------------------------------------------*/

static void CC release_database( void *item, void * data )
{
    vdp_database * db = ( vdp_database * ) item;
    if ( db != NULL )
    {
        StringWhack( db->name );
        VectorWhack( &db->sub_tables, release_table, NULL );
        VectorWhack( &db->sub_databases, release_database, NULL ); /* !! recursion */
        VDatabaseRelease( db->database );
        free( item );
    }
}

/* we can open a database from an accession or from a database... */
static rc_t vdp_open_database( vdp_src_context * vctx,
                               vdp_source * acc, vdp_database * parent_db, const String * name )
{
    rc_t rc = 0;
    vdp_database * db = malloc( sizeof * db );
    if ( db == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        KOutMsg( "vdp_open_database( '%S' ) -> %R\n", name, rc );
    }
    else
    {
        rc = StringCopy( &db->name, name );
        if ( rc != 0 )
        {
            free( ( void * ) db );
            KOutMsg( "string_dup_measure( '%S' ) -> %R\n", name, rc );
        }
        else
        {
            VectorInit( &db->sub_databases, 0, 5 );
            VectorInit( &db->sub_tables, 0, 5 );

            /* open the table: either from manager or from database */
            if ( acc != NULL )
                rc = VDBManagerOpenDBRead( vctx->mgr, &db->database, vctx->schema, "%s", name->addr );
            else
                rc = VDatabaseOpenDBRead( parent_db->database, &db->database, "%s", name->addr );

            /* enumerate tables, open tables */
            if ( rc == 0 )
            {
                KNamelist * table_names;
                rc_t rc1 = VDatabaseListTbl( db->database, &table_names );
                if ( rc1 == 0 )
                {
                    uint32_t count, idx;
                    rc = KNamelistCount( table_names, &count );
                    for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        const char * table_name;
                        rc = KNamelistGet( table_names, idx, &table_name );
                        if ( rc == 0 && table_name != NULL )
                        {
                            String temp_str;
                            StringInitCString( &temp_str, table_name );
                            rc = vdp_open_table( vctx, NULL, db, &temp_str );
                        }
                    }
                    KNamelistRelease( table_names );
                }
            }
            
            /* enumerate sub-db's, open sub-db's */
            if ( rc == 0 )    
            {
                KNamelist * sub_db_names;
                rc_t rc1 = VDatabaseListDB( db->database, &sub_db_names );
                if ( rc1 == 0 )
                {
                    uint32_t count, idx;
                    rc = KNamelistCount( sub_db_names, &count );
                    for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        const char * sub_db_name;
                        rc = KNamelistGet( sub_db_names, idx, &sub_db_name );
                        if ( rc == 0 && sub_db_name != NULL )
                        {
                            String temp_str;
                            StringInitCString( &temp_str, sub_db_name );
                            rc = vdp_open_database( vctx, NULL, db, &temp_str ); /* !! recursion !! */
                        }
                    }
                    KNamelistRelease( sub_db_names );
                }
            }
        
            if ( rc == 0 )
            {
                /* enter the new object: either into source-struct or parent-db */
                if ( acc != NULL )
                    acc->db = db;
                else
                    rc = VectorAppend( &parent_db->sub_databases, NULL, db );
            }
            
            if ( rc != 0 )
                release_database( db, NULL );
            else if ( vctx->print_info )
                KOutMsg( "database: '%S' opened\n", name );
        }
    }
    return rc;
}

/*
KLIB_EXTERN void* CC VectorFind ( const Vector *self, const void *key, uint32_t *idx,
    int64_t ( CC * cmp ) ( const void *key, const void *n ) );
*/

static vdp_table * vdp_db_get_table( vdp_database * db, String * path )
{
    vdp_table * res = NULL;
    if ( db != NULL )
    {
        if ( path == NULL )
        {
            res = VectorGet( &db->sub_tables, 0 );
        }
        else
        {
            
        }
    }
    return res;
}

/* -----------------------------------------------------------------------------------------------*/

static void CC release_source( void *item, void * data )
{
    vdp_source * vsrc = ( vdp_source * )item;
    if ( vsrc != NULL )
    {
        if ( vsrc->tbl != NULL ) release_table( vsrc->tbl, NULL );
        if ( vsrc->db != NULL ) release_database( vsrc->db, NULL );
        StringWhack ( vsrc->path );
        free( ( void * ) item );
    }
}

static rc_t vdp_init_source( vdp_src_context * vctx, const String * path )
{
    rc_t rc = 0;
    vdp_source * vsrc = malloc( sizeof * vsrc );
    if ( vsrc == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        KOutMsg( "vdp_init_source( '%S' ) -> %R\n", path, rc );
    }
    else
    {
        rc = StringCopy ( &vsrc->path, path );
        if ( rc != 0 )
        {
            free( ( void * ) vsrc );
            KOutMsg( "StringCopy( '%S' ) -> %R\n", path, rc );
        }
        else
        {
            vsrc->path_type = ( VDBManagerPathType ( vctx->mgr, "%s", vsrc->path->addr ) & ~ kptAlias );
            vsrc->tbl = NULL;
            vsrc->db = NULL;
            /* types defined in <kdb/manager.h> */
            switch ( vsrc->path_type )
            {
                case kptDatabase    :   rc = vdp_open_database( vctx, vsrc, NULL, vsrc->path ); break;
                case kptPrereleaseTbl:
                case kptTable       :   rc = vdp_open_table( vctx, vsrc, NULL, vsrc->path ); break;
                default : rc = RC( rcVDB, rcNoTarg, rcConstructing, rcFormat, rcUnknown ); break;
            }

            if ( rc != 0 )
                KOutMsg( "cannot open source '%S' -> %R\n", path, rc );
            else
                rc = VectorAppend( &vctx->sources, NULL, vsrc );
            
            if ( rc == 0 && vctx->print_info )
                KOutMsg( "source '%S' opened\n", path );
        }
    }
    return rc;
}

/* -----------------------------------------------------------------------------------------------*/

rc_t vdp_release_ctx( vdp_src_context * vctx )
{
    rc_t rc = 0;
    if ( vctx != NULL )
    {
        /* release all sources */
        VectorWhack( &vctx->sources, release_source, NULL );
        if ( vctx->dir != NULL )
            rc = KDirectoryRelease( vctx->dir );
        if ( rc == 0 && vctx->mgr != NULL )
            rc = VDBManagerRelease( vctx->mgr );
        if ( rc == 0 && vctx->schema != NULL )
            rc = VSchemaRelease( vctx->schema );
    }
    return rc;
}

rc_t vdp_init_ctx( vdp_src_context ** vctx, const Args * args )
{
    rc_t rc = 0;
    if ( vctx == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        KOutMsg( "vdp_init_ctx() -> %R\n", rc );
    
    }
    else
    {
        vdp_src_context * o = malloc( sizeof *o );
        if ( o == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            KOutMsg( "vdp_init_ctx() -> %R\n", rc );
        }
        else
        {
            VectorInit( &o->sources, 0, 5 );
            o->schema = NULL;
            o->print_info = false;
            rc = KDirectoryNativeDir( &o->dir );
            if ( rc != 0 )
            {
                KOutMsg( "KDirectoryNativeDir() -> %R\n", rc );
                o->dir = NULL;
            }
            else
            {
                rc = VDBManagerMakeRead ( &o->mgr, o->dir );
                if ( rc != 0 )
                {
                    KOutMsg( "VDBManagerMakeRead() -> %R\n", rc );
                    o->mgr = NULL;
                }
                else
                {
                    uint32_t count, idx;
                    vdh_parse_schema( o->mgr, &o->schema, NULL, true /*ctx->force_sra_schema*/ );
                    rc = ArgsParamCount( args, &count );
                    if ( rc != 0 )
                        KOutMsg( "ArgsParamCount() -> %R\n", rc );
                    else for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        const char *arg = NULL;
                        rc = ArgsParamValue( args, idx, (const void **)&arg );
                        if ( rc != 0 )
                            KOutMsg( "ArgsParamValue() -> %R\n", rc );
                        else if ( arg != NULL && arg[ 0 ] != 0 )
                        {
                            String temp_str;
                            StringInitCString( &temp_str, arg );
                            rc = vdp_init_source( o, &temp_str );
                        }
                    }
                }
            }
            if ( rc == 0 )
                *vctx = o;
            else
                vdp_release_ctx( o );
        }
    }
    return rc;
}


static vdp_table * vdp_get_table( vdp_src_context * vctx, uint32_t src_id, String * path )
{
    vdp_table * res = NULL;
    if ( vctx != NULL )
    {
        vdp_source * src = VectorGet( &vctx->sources, src_id );
        if ( src != NULL )
        {
            if ( src->tbl != NULL )
                res = src->tbl; /* source has only this table */
            else if ( src->db != NULL )
                res = vdp_db_get_table( src->db, NULL ); /* source is a database */
        }
    }
    return res;
}

/* -----------------------------------------------------------------------------------------------*/
static rc_t vdb_print_parse_range( struct num_gen * ranges, const String * range )
{
    rc_t rc = num_gen_parse_S( ranges, range );
    if ( rc != 0 )
        KOutMsg( "num_gen_parse_S( %S ) -> %R\n", range, rc );
    return rc;
}

static rc_t vdb_print_get_src_and_ranges( const String * S,
                struct num_gen * ranges, uint32_t * src_id )
{
    rc_t rc = 0;
    if ( S != NULL )
    {
        char * dot = string_chr( S->addr, S->len, '.' );
        if ( dot == NULL )
            rc = vdb_print_parse_range( ranges, S );
        else
        {
            String Sub;
            uint32_t dot_idx = ( dot - S->addr );
            if ( dot_idx < S->len )
            {
                String * tmp = StringSubstr( S, &Sub, dot_idx + 1, S->len - dot_idx );
                if ( tmp != NULL )
                    rc = vdb_print_parse_range( ranges, tmp );
            }
            if ( rc == 0 && dot_idx > 0 )
            {
                String * tmp = StringSubstr( S, &Sub, 0, dot_idx );
                if ( tmp != NULL )
                {
                    rc_t rc1;
                    uint64_t v = StringToU64( tmp, &rc1 );
                    if ( rc1 != 0 || v > 0xFFFF ) v = 0;
                    *src_id = ( v & 0xFFFF );
                }
            }
        }
    }
    return rc;
}


static rc_t vdb_print_show_src_and_ranges( struct num_gen * ranges, uint32_t src_id )
{
    rc_t rc;
    char buffer[ 1024 ];
    buffer[ 0 ] = 0;
    rc = num_gen_as_string( ranges, buffer, sizeof buffer, NULL, true );
    if ( rc == 0 )
        rc = KOutMsg( "src-id = %d, ranges = %s\n", src_id, buffer );
    return rc;
}


/* called from vdb-dump-interact.c, v is a vector of String-objects */
rc_t vdp_print_interactive( const Vector * v, vdp_src_context * vctx )
{
    struct num_gen * ranges;
    uint32_t src_id = 0;    /* per default use the first ( mostly only ) source */
    rc_t rc = num_gen_make_sorted( &ranges, true );
    if ( rc != 0 )
        KOutMsg( "num_gen_make_sorted() -> %R\n", rc );
    else
    {
        rc = vdb_print_get_src_and_ranges( VectorGet ( v, 1 ), ranges, &src_id );
        if ( rc == 0 )
        {
            vdp_table * tbl = vdp_get_table( vctx, src_id, NULL );
            if ( tbl == NULL )
                KOutMsg( "invalid source #%d\n", src_id );
            else
            {
                rc = vdp_table_adjust_ranges( tbl, ranges );
                
                if ( rc == 0 )
                    rc = vdb_print_show_src_and_ranges( ranges, src_id );
            }
        }
        num_gen_destroy( ranges );
    }
    return rc;
}
