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

#include "vdb-dump-tools.h"
#include "vdb-dump-str.h"
#include "vdb-dump-helper.h"

#include <klib/printf.h>
#include <klib/rc.h>
#include <klib/pack.h>
#include <klib/out.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <bitstr.h>

#include <klib/rc.h>
#include <klib/log.h>

#define BYTE_OFFSET(VALUE)  ( (VALUE) >> 3 )
#define BIT_OFFSET(VALUE)   ( (VALUE) & 0x7 )

uint8_t BitLength2Bytes[65] =
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
static uint16_t vdt_bitlength_2_bytes( const size_t n_bits )
{
    return ( n_bits > 64 ) ? 8 : BitLength2Bytes[ n_bits ];
}

uint64_t BitLength2Mask[33] =
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
static uint64_t vdt_bitlength_2_mask( const size_t n_bits )
{
    uint64_t res;
    if ( n_bits < 33 )
    {
        res = BitLength2Mask[ n_bits ];
    }
    else
    {
        res = ( n_bits < 65 ) ? BitLength2Mask[ n_bits - 32 ] : 0xFFFFFFFF;
        res <<= 32;
        res |= 0xFFFFFFFF;
    }
    return res;
}


typedef struct bit_iter
{
    const uint8_t *buf;
    uint64_t bit_offset;
} bit_iter;
typedef bit_iter* p_bit_iter;

#define MACRO_GET( DATA_TYPE ) \
    DATA_TYPE res = 0; \
    const uint8_t * p = iter -> buf + BYTE_OFFSET( iter -> bit_offset ); \
    bitcpy ( &res, 0, p, BIT_OFFSET( iter -> bit_offset ), num_bits ); \
    iter -> bit_offset += num_bits; \
    return res;

static uint8_t vdt_get_u8( p_bit_iter iter, uint8_t num_bits )
{
    MACRO_GET( uint8_t )
}

static uint64_t vdt_get_u64( p_bit_iter iter, uint8_t num_bits )
{
    MACRO_GET( uint64_t )
}

static int64_t vdt_get_i64( p_bit_iter iter, uint8_t num_bits )
{
    MACRO_GET( int64_t )
}

static float vdt_get_f32( p_bit_iter iter, uint8_t num_bits )
{
    MACRO_GET( float )
}

static double vdt_get_f64( p_bit_iter iter, uint8_t num_bits )
{
    MACRO_GET( double )
}

#undef MACRO_GET

static uint8_t * vdb_get_bits( p_bit_iter iter, size_t num_bytes )
{
    uint8_t * res = malloc( num_bytes );
    if ( NULL != res )
    {
        const uint8_t * p = iter -> buf + BYTE_OFFSET( iter -> bit_offset );
        bitcpy ( res, 0, p, BIT_OFFSET( iter -> bit_offset ), num_bytes << 3 );
        iter -> bit_offset += ( num_bytes << 3 );
    }
    return res;
}
    
static void vdt_move_to_value( void* dst, const p_dump_src src, const uint32_t n_bits )
{
    char *src_ptr = ( char* )src -> buf + BYTE_OFFSET( src -> offset_in_bits );
    if ( 0 == BIT_OFFSET( src -> offset_in_bits ) )
    {
        memmove( dst, src_ptr, vdt_bitlength_2_bytes( n_bits ) );
    }
    else
    {
        bitcpy ( dst, 0, src_ptr, BIT_OFFSET( src -> offset_in_bits ), n_bits );
    }
}

static uint64_t vdt_move_to_uint64( const p_dump_src src, const uint32_t n_bits )
{
    uint64_t value = 0;
    vdt_move_to_value( &value, src, n_bits );
    if ( 0 != ( n_bits & 7 ) )
    {
        size_t unpacked = 0;
        Unpack( n_bits, sizeof( value ), &value, 0, n_bits,
                NULL, &value, sizeof( value ), &unpacked );
    }
    value &= vdt_bitlength_2_mask( n_bits );
    src -> offset_in_bits += n_bits;
    return value;
}

/*************************************************************************************
    byte-source-iter

    returns: uint8,int8,uint16,int16,uint32,int32,uint64,int64,fload,double
*************************************************************************************/

static rc_t vdt_ascii_v1( p_dump_str s, const p_dump_src src, const p_col_def def )
{
    char *src_ptr = ( char* )src -> buf + BYTE_OFFSET( src -> offset_in_bits );
    return vds_append_fmt( s, src -> number_of_elements,
                           "%.*s", src -> number_of_elements, src_ptr );

}

static rc_t vdt_hex_char_v1( char * temp, uint32_t * idx, const uint8_t c )
{
    char s[ 8 ];
    size_t num_writ;
    rc_t rc = string_printf ( s, sizeof s, &num_writ, "%X ", c );
    if ( 0 == rc )
    {
        size_t i;
        for ( i = 0; i < num_writ; ++i )
        {
            temp[ ( *idx )++ ] = s[ i ];
        }
    }
    return rc;
}

static rc_t vdt_hex_ascii_v1( p_dump_str s, const p_dump_src src, const p_col_def def )
{
    rc_t rc = 0;
    char *src_ptr = ( char* )src -> buf + BYTE_OFFSET( src -> offset_in_bits );
    char *tmp = malloc( src -> number_of_elements * 4 );
    if ( NULL != tmp )
    {
        uint32_t i, dst = 0;
        for ( i = 0; i < src->number_of_elements && 0 == rc; ++i )
        {
            rc = vdt_hex_char_v1( tmp, &dst, src_ptr[ i ] );
        }
        if ( 0 == rc )
        {
            rc = vds_append_fmt( s, dst, "%.*s", dst, tmp );
        }
        free( tmp );
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    return rc;
}

/*************************************************************************************
src         [IN] ... buffer containing the data
dpo         [IN] ... pointer to buffer-offset (bit-offset-part will be ignored)

dumps an ascii-string
*************************************************************************************/
static rc_t vdt_hex_or_ascii_v1( p_dump_str s, const p_dump_src src, const p_col_def def )
{
    rc_t rc;
    if ( src -> in_hex )
    {
        rc = vdt_hex_ascii_v1( s, src, def );
        DISP_RC( rc, "vdt_hex_ascii_v1() failed" );
    }
    else
    {
        rc = vdt_ascii_v1( s, src, def );
        DISP_RC( rc, "vdt_ascii_v1() failed" );
    }
    if ( 0 == rc )
    {
        src -> element_idx += src -> number_of_elements;
        src -> offset_in_bits += ( def -> type_desc . intrinsic_bits * src -> number_of_elements );
    }
    return rc;
}

/*************************************************************************************
src         [IN] ... buffer containing the data
dpo         [IN] ... pointer to buffer-offset (bit-offset-part will be ignored)

dumps an ascii-string
*************************************************************************************/
static rc_t vdt_unicode_v1( p_dump_str s, const p_dump_src src, const p_col_def def )
{
    rc_t rc;
    if ( src -> in_hex )
    {
        rc = vdt_hex_ascii_v1( s, src, def );
        DISP_RC( rc, "vdt_hex_ascii_v1() failed" );
    }
    else
    {
        rc = vdt_ascii_v1( s, src, def );
        DISP_RC( rc, "vdt_ascii_v1() failed" );
    }
    if ( 0 == rc )
    {
        src -> element_idx += src -> number_of_elements;
        src -> offset_in_bits += ( def -> type_desc . intrinsic_bits * src -> number_of_elements );
    }
    return rc;
}


/*************************************************************************************
src         [IN] ... buffer containing the data
my_col_def  [IN] ... the definition of the column to be dumped

dumps boolean values (up to 64 bits)
*************************************************************************************/
static rc_t vdt_dump_boolean_element( p_dump_str s, const p_dump_src src,
                                      const p_col_def def )
{
    rc_t rc;
    uint64_t value = vdt_move_to_uint64( src, def -> type_desc . intrinsic_bits );
    switch( src -> c_boolean )
    {
        case '1' :  if ( 0 == value )
                    {
                        rc = vds_append_str( s, "0" );
                    }
                    else
                    {
                        rc = vds_append_str( s, "1" );
                    }
                    break;
                    
        case 'T' :  if ( 0 == value )
                    {
                        rc = vds_append_str( s, "F" );
                    }
                    else
                    {
                        rc = vds_append_str( s, "T" );
                    }
                    break;

        default  :  if ( 0 == value )
                    {
                        rc = vds_append_str( s, "false" );
                    }
                    else
                    {
                        rc = vds_append_str( s, "true" );
                    }
                    break;
    }
    DISP_RC( rc, "dump_str_append_str() failed" );
    return rc;
}

#define MAX_CHARS_FOR_HEX_UINT64 20
#define MAX_CHARS_FOR_DEC_UINT64 22

/*************************************************************************************
src         [IN] ... buffer containing the data
my_col_def  [IN] ... the definition of the column to be dumped

unsigned int's up to a length of 64 bit are supported
unused bits are masked out
*************************************************************************************/
static rc_t vdt_dump_uint_element( p_dump_str s, const p_dump_src src,
                                   const p_col_def def )
{
    rc_t rc = 0;
    uint64_t value = vdt_move_to_uint64( src, def -> type_desc . intrinsic_bits );
    if ( ( ! src -> without_sra_types ) && ( NULL != def -> value_trans_fn ) )
    {
        const char *txt = def -> value_trans_fn( ( uint32_t )value );
        rc = vds_append_str( s, txt );
        DISP_RC( rc, "dump_str_append_str() failed" );
    }
    else
    {
        if ( src -> in_hex )
        {
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, "0x%lX", value );
        }
        else
        {
            rc = vds_append_fmt( s, MAX_CHARS_FOR_DEC_UINT64, "%lu", value );
        }
        DISP_RC( rc, "dump_str_append_fmt() failed" );
    }
    return rc;
}

/*************************************************************************************
src         [IN] ... buffer containing the data
my_col_def  [IN] ... the definition of the column to be dumped

signed int's up to a length of 64 bit are supported
unused bits are masked out
*************************************************************************************/
static rc_t vdt_dump_int_element( p_dump_str s, const p_dump_src src,
                                  const p_col_def def )
{
    rc_t rc = 0;
    int64_t value = ( int64_t )vdt_move_to_uint64( src, def -> type_desc . intrinsic_bits );
    if ( ( ! src -> without_sra_types ) && ( NULL != def -> value_trans_fn ) )
    {
        const char *txt = def -> value_trans_fn( ( uint32_t )value );
        rc = vds_append_str( s, txt );
        DISP_RC( rc, "dump_str_append_str() failed" );
    }
    else
    {
        switch ( def -> type_desc . intrinsic_bits )
        {
            case  8 : {
                        int8_t temp = ( int8_t )value;
                        value = temp;
                      }
                      break;

            case 16 : {
                        int16_t temp = ( int16_t )value;
                        value = temp;
                      }
                      break;

            case 32 : {
                        int32_t temp = ( int32_t )value;
                        value = temp;
                      }
                      break;
        }

        if ( src -> in_hex )
        {
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, "0x%lX", value );
        }
        else
        {
            rc = vds_append_fmt( s, MAX_CHARS_FOR_DEC_UINT64, "%ld", value );
        }
        DISP_RC( rc, "dump_str_append_fmt() failed" );
    }
    return rc;
}

#define MAX_CHARS_FOR_DOUBLE 26
#define BITSIZE_OF_FLOAT ( sizeof(float) * 8 )
#define BITSIZE_OF_DOUBLE ( sizeof(double) * 8 )

/*************************************************************************************
src         [IN] ... buffer containing the data
my_col_def  [IN] ... the definition of the column to be dumped

only 2 float-types: "float"(32 bit) and "double"(64 bit) are supported
*************************************************************************************/
static rc_t vdt_dump_float_element( p_dump_str s, const p_dump_src src,
                                    const p_col_def def )
{
    rc_t rc;
    if ( src -> in_hex )
    {
        rc = vdt_dump_int_element( s, src, def );
    }
    else
    {
        if ( BITSIZE_OF_FLOAT == def -> type_desc . intrinsic_bits )
        {
            float value;
            vdt_move_to_value( &value, src, def -> type_desc . intrinsic_bits );
            rc = vds_append_fmt( s, MAX_CHARS_FOR_DOUBLE, "%e", value );
            DISP_RC( rc, "dump_str_append_fmt() failed" );
        }
        else if ( BITSIZE_OF_DOUBLE == def -> type_desc . intrinsic_bits )
        {
            double value;
            vdt_move_to_value( &value, src, def -> type_desc . intrinsic_bits );
            rc = vds_append_fmt( s, MAX_CHARS_FOR_DOUBLE, "%e", value );
            DISP_RC( rc, "dump_str_append_fmt() failed" );
        }
        else
        {
            rc = vds_append_str( s, "unknown float-type" );
            DISP_RC( rc, "dump_str_append_str() failed" );
        }
        src -> offset_in_bits += def -> type_desc.intrinsic_bits;
    }
    return rc;
}

char dna_chars[4] = { 'A', 'C', 'G', 'T' };

/* special function to translate dim=2,bits=1 into a DNA-base */
static rc_t vdt_dump_base_element( p_dump_str s,
                                   const p_dump_src src,
                                   const p_col_def def )
{
    rc_t rc;
    /* do not replace this with value2=value1, because move_to_uint64
       increments the src-bit-counter !*/
    uint64_t value1 = vdt_move_to_uint64( src, def -> type_desc . intrinsic_bits );
    uint64_t value2 = vdt_move_to_uint64( src, def -> type_desc . intrinsic_bits );
    value1 <<= 1;
    value1 |= value2;
    rc = vds_append_fmt( s, 1, "%c", dna_chars[ value1 & 0x03 ] );
    DISP_RC( rc, "dump_str_append_fmt() failed" );
    return rc;
}

typedef rc_t(*vdt_dump_fkt_t)( p_dump_str s,
                               const p_dump_src src,
                               const p_col_def def );

/* !!!!!!!! this depends on how domains are defined in "schema.h" */
vdt_dump_fkt_t vdt_DomainDispatch[] =
{
    vdt_dump_boolean_element,
    vdt_dump_uint_element,
    vdt_dump_int_element,
    vdt_dump_float_element,
    vdt_hex_or_ascii_v1,
    vdt_unicode_v1
};

rc_t vdt_dump_dim_trans( const p_dump_src src, const p_col_def def,
                         const int dimension )
{
    rc_t rc = 0;
    uint8_t *sbuf = ( uint8_t * )src -> buf;
    char trans_txt[ 512 ];
    size_t written;
    
    sbuf += ( src -> offset_in_bits >> 3 );
    rc = def -> dim_trans_fn( trans_txt, sizeof trans_txt, &written, sbuf, src -> output_format );
    src -> offset_in_bits += ( def -> type_desc . intrinsic_bits * dimension );
    if ( 0 == rc )
    {
        rc = vds_append_str( &( def -> content ), trans_txt );
        DISP_RC( rc, "dump_str_append_str() failed" );
    }
    return rc;
}

rc_t vdt_dump_dim( const p_dump_src src, const p_col_def def,
                   const int dimension, const int selection )
{
    rc_t rc = 0;
    int i = 0;
    bool print_comma = true;

    if ( 0 == selection ) /* cell-type == boolean */
    {
        /* if long form "false" or "true" separate elements by comma */
        print_comma = ( 0 == src -> c_boolean );
    }

    while ( ( i < dimension )&&( 0 == rc ) )
    {
        /* selection 0 ... boolean */
        if ( print_comma && ( i > 0 ) )
        {
            rc = vds_append_str( &( def -> content ), ", " );
            DISP_RC( rc, "dump_str_append_str() failed" );
        }
        if ( rc == 0 )
        {
            rc = vdt_DomainDispatch[ selection ]( &( def -> content ), src, def );
            DISP_RC( rc, "DomainDispatch[]() failed" );
        }
        i++;
    }
    return rc;
}

/*************************************************************************************
src         [IN] ... buffer containing the data
my_col_def  [IN] ... the definition of the column to be dumped

dumps one data-element (or a vector of it)
*************************************************************************************/
static rc_t vdt_dump_cell_element( const p_dump_src src, const p_col_def def, bool bracket )
{
    int dimension, selection;
    rc_t rc = 0;

    if ( ( NULL == src ) || ( NULL == def ) )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    dimension   = def -> type_desc . intrinsic_dim;
    selection   = def -> type_desc . domain - 1;

   
    if ( 1 == dimension )
    {
        /* we have only 1 dimension ---> just print this value */
        rc = vdt_DomainDispatch[ selection ]( &( def -> content ), src, def );
        DISP_RC( rc, "DomainDispatch[]() failed" );
    }
    else
    {
        /* we have more than 1 dimension ---> repeat printing value's */
        if ( src -> print_dna_bases )
        {
            rc = vdt_dump_base_element( &( def -> content ), src, def );
        }
        else
        {
            bool trans = ( ( ! src -> without_sra_types ) && ( NULL != def -> dim_trans_fn ) );
            bool paren = ( ( src -> number_of_elements > 1 ) || ( !trans ) );

            if ( paren )
            {
                rc = vds_append_str( &( def -> content), bracket ? "[" : "{" );
                DISP_RC( rc, "dump_str_append_str() failed" );
            }

            if ( 0 == rc )
            {
                if ( trans )
                {
                    rc = vdt_dump_dim_trans( src, def, dimension );
                }
                else
                {
                    rc = vdt_dump_dim( src, def, dimension, selection );
                }
            }

            if ( paren && ( 0 == rc ) )
            {
                rc = vds_append_str( &( def -> content ), bracket ? "]" : "}" );
                DISP_RC( rc, "dump_str_append_str() failed" );
            }
        }
    }
    src -> element_idx++;
    return rc;
}


static rc_t vdt_dump_cell_dflt( const p_dump_src src, const p_col_def def )
{
    rc_t rc = 0;
    /* loop through the elements(dimension's) of a cell */
    while( ( src -> element_idx < src -> number_of_elements )&&( rc == 0 ) )
    {
        uint32_t eidx = src -> element_idx;
        if ( ( eidx > 0 )&& ( ! src -> print_dna_bases ) && src -> print_comma )
        {
            if ( src -> translate_sra_values )
            {
                vds_append_str( &( def -> content ), "," );
            }
            else
            {
                vds_append_str( &( def -> content ), ", " );
            }
        }

        rc = vdt_dump_cell_element( src, def, !( src -> translate_sra_values ) );

        /* insurance against endless loop */
        if ( eidx == src -> element_idx )
        {
            src -> element_idx++;
        }
    }
    return rc;
}


static rc_t vdt_dump_cell_json( const p_dump_src src, const p_col_def def )
{
    rc_t rc = 0;
    /* loop through the elements(dimension's) of a cell */
    while( ( src -> element_idx < src -> number_of_elements )&&( rc == 0 ) )
    {
        uint32_t eidx = src -> element_idx;
        if ( ( eidx > 0 )&& ( ! src -> print_dna_bases ) && src -> print_comma )
        {
            if ( src -> translate_sra_values )
            {
                vds_append_str( &( def -> content ), "," );
            }
            else
            {
                vds_append_str( &( def -> content ), ", " );
            }
        }

        rc = vdt_dump_cell_element( src, def, !( src -> translate_sra_values ) );

        /* insurance against endless loop */
        if ( eidx == src -> element_idx )
        {
            src -> element_idx++;
        }
    }
    return rc;
}


static rc_t vdt_print_cell_debug( const p_dump_src src, const p_col_def def )
{
    return vds_append_fmt( &( def -> content ), 128,
        "<dom=%u, dim=%u, num=%u, bits=%u, ofs=%u> ",
        def -> type_desc . domain,
        def -> type_desc . intrinsic_dim,
        src -> number_of_elements,
        def -> type_desc . intrinsic_bits,
        src -> offset_in_bits );
}

/*************************************************************************************
src         [IN] ... buffer containing the data
my_col_def  [IN] ... the definition of the column to be dumped

dumps all data-elements of a cell
*************************************************************************************/
rc_t vdt_format_cell_v1( const p_dump_src src, const p_col_def def, bool cell_debug )
{
    rc_t rc = 0;

    if ( cell_debug )
    {
        rc = vdt_print_cell_debug( src, def );
    }

    if ( 0 == rc )
    {
        /* initialize the element-idx ( for dimension > 1 ) */
        src -> element_idx = 0;

        switch ( src -> output_format )
        {
            case df_json : rc = vdt_dump_cell_json( src, def ); break;
            default      : rc = vdt_dump_cell_dflt( src, def ); break;
        }
    }
    return rc;
}

/* ================================================================================= */

const char * bool_true_1      = "1";
const char * bool_false_1     = "0";
const char * bool_true_T      = "T";
const char * bool_false_T     = "F";
const char * bool_true_dflt   = "true";
const char * bool_false_dflt  = "false";

static void vdb_get_bool_strings( char c_boolean, const char ** s_true, const char ** s_false )
{
    *s_true  = bool_true_dflt;
    *s_false = bool_false_dflt;    
    switch( c_boolean )
    {
        case '1' :  *s_true = bool_true_1; *s_false = bool_false_1; break;
        case 'T' :  *s_true = bool_true_T; *s_false = bool_false_T; break;
    }
}

/* --------------------------------------------------------------------------------- */

#define MACRO_IN_HEX \
    uint64_t value; \
    if ( 1 == n ) \
    { \
        value = vdt_get_u64( bi, def -> type_desc . intrinsic_bits ); \
        rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, "0x%lX", value ); \
    } \
    else \
    { \
        uint32_t i; \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            value = vdt_get_u64( bi, def -> type_desc . intrinsic_bits ); \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, "0x%lX, ", value ); \
        } \
        if ( 0 == rc ) \
        { \
            value = vdt_get_u64( bi, def -> type_desc . intrinsic_bits ); \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, "0x%lX", value ); \
        } \
    }

static rc_t vdt_format_slice_nbb_bool( const p_dump_src src, const p_col_def def, p_bit_iter bi, uint32_t n )
{
    rc_t rc = 0;
    uint32_t i;
    p_dump_str s = &( def -> content );
    if ( src -> in_hex )
    {
        MACRO_IN_HEX
    }
    else
    {
        const char * bt_true;
        const char * bt_false;    

        vdb_get_bool_strings( src -> c_boolean, &bt_true, &bt_false );
        for ( i = 0; 0 == rc && i < n; ++i )
        {
            uint64_t value = vdt_get_u64( bi, def -> type_desc . intrinsic_bits );
            rc = vds_append_str( s, 0 == value ? bt_false : bt_true );
        }
    }
    return rc;
}

#define MACRO_TRANSLATE( DATA_TYPE, GETTER_FUNC ) \
    if ( 1 == n ) \
    { \
        DATA_TYPE value = GETTER_FUNC( bi, def -> type_desc . intrinsic_bits ); \
        const char *txt = def -> value_trans_fn( ( uint32_t )value ); \
        if ( NULL != txt ) \
        { \
            if ( df_json == src -> output_format ) \
            { \
                size_t txt_len = string_size( txt ); \
                rc = vds_append_fmt( s, txt_len + 3, "\"%s\"", txt ); \
            } \
            else \
            { \
                rc = vds_append_str( s, txt ); \
            }\
        } \
    } \
    else \
    { \
        if ( df_json == src -> output_format ) \
        { \
            rc = vds_append_str( s, "[" ); \
        } \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            DATA_TYPE value = GETTER_FUNC( bi, def -> type_desc . intrinsic_bits ); \
            const char *txt = def -> value_trans_fn( ( uint32_t )value ); \
            if ( NULL != txt ) \
            { \
                size_t txt_len = string_size( txt ); \
                if ( df_json == src -> output_format ) \
                { \
                    rc = vds_append_fmt( s, txt_len + 6, "\"%s\", ", txt ); \
                } \
                else \
                { \
                    rc = vds_append_fmt( s, txt_len + 4, "%s, ", txt ); \
                }\
            }\
        } \
        if ( 0 == rc ) \
        { \
            DATA_TYPE value = GETTER_FUNC( bi, def -> type_desc . intrinsic_bits ); \
            const char *txt = def -> value_trans_fn( ( uint32_t )value ); \
            if ( NULL != txt ) \
            { \
                if ( df_json == src -> output_format ) \
                { \
                    size_t txt_len = string_size( txt ); \
                    rc = vds_append_fmt( s, txt_len + 3, "\"%s\"", txt ); \
                } \
                else \
                { \
                    rc = vds_append_str( s, txt ); \
                }\
            } \
        } \
        if ( 0 == rc && df_json == src -> output_format ) \
        { \
            rc = vds_append_str( s, "]" ); \
        } \
    }

#define MACRO_PRINT( DATA_TYPE, GETTER_FUNC, FMT1, FMT2 ) \
    if ( 1 == n ) \
    { \
        DATA_TYPE value = GETTER_FUNC( bi, def -> type_desc . intrinsic_bits ); \
        rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT1, value ); \
    } \
    else \
    { \
        if ( df_json == src -> output_format ) \
        { \
            rc = vds_append_str( s, "[" ); \
        } \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            DATA_TYPE value = GETTER_FUNC( bi, def -> type_desc . intrinsic_bits ); \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT2, value ); \
        } \
        if ( 0 == rc ) \
        { \
            DATA_TYPE value = GETTER_FUNC( bi, def -> type_desc . intrinsic_bits ); \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT1, value ); \
        } \
        if ( 0 == rc && df_json == src -> output_format ) \
        { \
            rc = vds_append_str( s, "]" ); \
        } \
    }

static rc_t vdt_format_slice_nbb_unsig( const p_dump_src src, const p_col_def def, p_bit_iter bi, uint32_t n )
{
    rc_t rc = 0;
    uint32_t i;
    p_dump_str s = &( def -> content );
    if ( src -> in_hex )
    {
        MACRO_IN_HEX
    }
    else if ( src -> value_trans )
    {
        MACRO_TRANSLATE( uint64_t, vdt_get_u64 )
    }
    else
    {
        MACRO_PRINT( uint64_t, vdt_get_u64, "%lu", "%lu, " )
    }
    return rc;
}

static rc_t vdt_format_slice_nbb_sig( const p_dump_src src, const p_col_def def, p_bit_iter bi, uint32_t n )
{
    rc_t rc = 0;
    uint32_t i;
    p_dump_str s = &( def -> content );
    if ( src -> in_hex )
    {
        MACRO_IN_HEX
    }
    else if ( src -> value_trans )
    {
        MACRO_TRANSLATE( uint64_t, vdt_get_u64 )
    }
    else
    {
        MACRO_PRINT( int64_t, vdt_get_i64, "%lu", "%lu, " )
    }
    return rc;
}

/* here the bit-size should be only 32-bit or 64-bit, but we are not on a byte-boundary */
static rc_t vdt_format_slice_nbb_float( const p_dump_src src, const p_col_def def, p_bit_iter bi, uint32_t n )
{
    rc_t rc = 0;
    uint32_t i;
    p_dump_str s = &( def -> content );
    if ( src -> in_hex )
    {
        MACRO_IN_HEX
    }
    else
    {
        if ( 32 == def -> type_desc . intrinsic_bits )
        {
            MACRO_PRINT( float, vdt_get_f32, "%e", "%e, " )
        }
        else if ( 64 == def -> type_desc . intrinsic_bits )
        {
            MACRO_PRINT( double, vdt_get_f64, "%e", "%e, " )            
        }
        else
        {
            /* this should not happen... */
        }
    }
    return rc;
}

/* here the bit-size should be only 8-bit, but we are not on a byte-boundary */
static rc_t vdt_format_slice_nbb_ascii( const p_dump_src src, const p_col_def def, p_bit_iter bi, uint32_t n )
{
    rc_t rc = 0;
    uint32_t i;
    p_dump_str s = &( def -> content );

    if ( src -> in_hex )
    {
        MACRO_IN_HEX
    }
    else
    {
        if ( def -> type_desc . intrinsic_bits < 9 )
        {
            if ( df_json == src -> output_format )
            {
                rc = vds_append_str( s, "\"" );
            }
            for ( i = 0; 0 == rc && i < n; ++i )
            {
                uint8_t value = vdt_get_u8( bi, def -> type_desc . intrinsic_bits );
                rc = vds_append_fmt( s, 4, "%c", value );
            }
            if ( 0 == rc && df_json == src -> output_format )
            {
                rc = vds_append_str( s, "\"" );
            }
        }
        else
        {
            /* this should not happen... */
        }
    }
    return rc;
}

#undef MACRO_IN_HEX
#undef MACRO_TRANSLATE
#undef MACRO_PRINT

static rc_t vdt_format_slice_nbb( const p_dump_src src, const p_col_def def, p_bit_iter bi, uint32_t n )
{
    switch( def -> type_desc . domain )
    {
        /* boolean */
        case 1 : return vdt_format_slice_nbb_bool( src, def, bi, n ); break;
        
        /* unsigned integers */
        case 2 : return vdt_format_slice_nbb_unsig( src, def, bi, n ); break;
        
        /* signed integers */
        case 3 : return vdt_format_slice_nbb_sig( src, def, bi, n ); break;
        
        /* floats */
        case 4 : return vdt_format_slice_nbb_float( src, def, bi, n ); break;

        /* text */
        case 5 :
        case 6 : return vdt_format_slice_nbb_ascii( src, def, bi, n ); break;

        default : /* this should not be reached - we checked before !*/ break;
    }
    return 0;
}

static rc_t vdt_format_cell_nbb_dim1_v2( const p_dump_src src, const p_col_def def )
{
    uint32_t n = src -> number_of_elements;
    bit_iter bi = { src -> buf, src -> offset_in_bits };
    return vdt_format_slice_nbb( src, def, &bi, n );
}

static rc_t vdt_format_nbb_dim_trans_json( const p_dump_src src, const p_col_def def )
{
    bit_iter bi = { src -> buf, src -> offset_in_bits };
    size_t num_bytes = def -> dim_trans_size;
    uint32_t n = src -> number_of_elements;
    uint32_t group;
    p_dump_str ds = &( def -> content );

    rc_t rc = vds_append_str( ds, "[" );
    for ( group = 0; 0 == rc && group < n; ++group )
    {
        char trans_txt[ 512 ];
        size_t written;
        uint8_t * slice = vdb_get_bits( &bi, num_bytes );
        if ( NULL != slice )
        {
            rc = def -> dim_trans_fn( trans_txt, sizeof trans_txt, &written,
                                        slice, src -> output_format );
            if ( 0 == rc )
            {
                bool not_last = ( group < n - 1 );
                rc = vds_append_fmt( ds, written + 3, not_last ? "%s," : "%s", trans_txt );
            }
            free( slice );
        }
    }
    if ( 0 == rc )
    {
        rc = vds_append_str( ds, "]" );
    }
    return rc;
}

static rc_t vdt_format_nbb_dim_trans_dflt( const p_dump_src src, const p_col_def def )
{
    rc_t rc = 0;
    bit_iter bi = { src -> buf, src -> offset_in_bits };
    size_t num_bytes = def -> dim_trans_size;
    uint32_t n = src -> number_of_elements;
    uint32_t group;

    for ( group = 0; 0 == rc && group < n; ++group )
    {
        char trans_txt[ 512 ];
        size_t written;
        uint8_t * slice = vdb_get_bits( &bi, num_bytes );
        if ( NULL != slice )
        {
            rc = def -> dim_trans_fn( trans_txt, sizeof trans_txt, &written,
                                        slice, src -> output_format );
            if ( 0 == rc )
            {
                rc = vds_append_fmt( &( def -> content ), written + 3, "[%s]", trans_txt );
            }
            free( slice );
        }
    }
    return rc;
}

static rc_t vdt_format_cell_nbb_dim2_v2( const p_dump_src src, const p_col_def def )
{
    rc_t rc = 0;
    bool group_trans = ( ! src -> without_sra_types ) && ( NULL != def -> dim_trans_fn );
    
    if ( group_trans )
    {
        switch ( src -> output_format )
        {
            case df_json : return vdt_format_nbb_dim_trans_json( src, def ); break;
            case df_xml  : ; /* fall through for now */
            default      : return vdt_format_nbb_dim_trans_dflt( src, def ); break;
        }
    }
    else
    {
        bit_iter bi = { src -> buf, src -> offset_in_bits };
        uint32_t dim = def -> type_desc . intrinsic_dim;
        uint32_t n = src -> number_of_elements;
        uint32_t group;

        if ( df_json == src -> output_format )
        {
            rc = vds_append_str( &( def -> content ), "[" );
        }

        for ( group = 0; 0 == rc && group < n; ++group )
        {
            rc = vdt_format_slice_nbb( src, def, &bi, dim );
            if ( 0 == rc && group < n - 1 )
            {
                rc = vds_append_str( &( def -> content ), "," );
            }
        }

        if ( 0 == rc && df_json == src -> output_format )
        {
            rc = vds_append_str( &( def -> content ), "]" );
        }
    }
    return rc;
}

/* --------------------------------------------------------------------------------- */

#define MACRO_IN_HEX_JSON( FMT1, FMT2 ) \
    if ( 1 == n ) \
    { \
        rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT1, data[ 0 ] ); \
    } \
    else \
    { \
        uint32_t i; \
        if ( brackets ) \
        { \
           rc = vds_append_str( s, "[" ); \
        } \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT2, data[ i ] ); \
        } \
        if ( 0 == rc ) \
        { \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT1, data[ n - 1 ] ); \
        } \
        if ( 0 == rc && brackets ) \
        { \
           rc = vds_append_str( s, "]" ); \
        } \
    }

#define MACRO_IN_HEX_DFLT( FMT1, FMT2 ) \
    if ( 1 == n ) \
    { \
        rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT1, data[ 0 ] ); \
    } \
    else \
    { \
        uint32_t i; \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT2, data[ i ] ); \
        } \
        if ( 0 == rc ) \
        { \
            rc = vds_append_fmt( s, MAX_CHARS_FOR_HEX_UINT64, FMT1, data[ n - 1 ] ); \
        } \
    }

#define MACRO_IN_HEX_SHORT \
    if ( df_json == src -> output_format ) \
    { \
        MACRO_IN_HEX_JSON( "\"0x%X\"", "\"0x%X\", " ) \
    } \
    else \
    { \
        MACRO_IN_HEX_DFLT( "0x%X", "0x%X, " ) \
    }

#define MACRO_IN_HEX_LONG \
    if ( df_json == src -> output_format ) \
    { \
        MACRO_IN_HEX_JSON( "\"0x%lX\"", "\"0x%lX\", " ) \
    } \
    else \
    { \
        MACRO_IN_HEX_DFLT( "0x%lX", "0x%lX, " ) \
    }

static rc_t vdt_format_slice_bb_bool( const p_dump_src src, const p_col_def def,
                                      const uint8_t * data, uint32_t n, bool brackets )
{
    rc_t rc = 0;
    uint32_t i;
    p_dump_str s = &( def -> content );

    if ( src -> in_hex )
    {
        MACRO_IN_HEX_SHORT
    }
    else
    {
        const char * bt_true;
        const char * bt_false;    
        vdb_get_bool_strings( src -> c_boolean, &bt_true, &bt_false );
        for ( i = 0; 0 == rc && i < n; ++i )
        {
            rc = vds_append_str( s, 0 == data[ i ] ? bt_false : bt_true );
        }
    }
    return rc;
}

#define MACRO_TRANSLATE_JSON \
    if ( 1 == n ) \
    { \
        const char *txt = def -> value_trans_fn( ( uint32_t )data[ 0 ] ); \
        size_t txt_len = string_size( txt ) ;\
        rc = vds_append_fmt( s, txt_len + 3, "\"%s\"", txt ); \
    } \
    else \
    { \
        uint32_t i; \
        rc = vds_append_str( s, "[" ); \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            const char *txt = def -> value_trans_fn( ( uint32_t )data[ i ] ); \
            size_t txt_len = string_size( txt ) ;\
            rc = vds_append_fmt( s, txt_len + 5, "\"%s\", ", txt ); \
        } \
        if ( 0 == rc ) \
        { \
            const char *txt = def -> value_trans_fn( ( uint32_t )data[ n - 1 ] ); \
            size_t txt_len = string_size( txt ) ;\
            rc = vds_append_fmt( s, txt_len + 4, "\"%s\"]", txt ); \
        } \
    }


#define MACRO_TRANSLATE_DFLT \
    if ( 1 == n ) \
    { \
        const char *txt = def -> value_trans_fn( ( uint32_t )data[ 0 ] ); \
        rc = vds_append_str( s, txt ); \
    } \
    else \
    { \
        uint32_t i; \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            const char *txt = def -> value_trans_fn( ( uint32_t )data[ i ] ); \
            size_t txt_len = string_size( txt ); \
            rc = vds_append_fmt( s, txt_len + 3, "%s, ", txt ); \
        } \
        if ( 0 == rc ) \
        { \
            const char *txt = def -> value_trans_fn( ( uint32_t )data[ n - 1 ] ); \
            rc = vds_append_str( s, txt ); \
        } \
    }


#define MACRO_TRANSLATE \
    if ( df_json == src -> output_format ) \
    { \
        MACRO_TRANSLATE_JSON \
    } \
    else \
    { \
        MACRO_TRANSLATE_DFLT \
    }

#define MACRO_PRINT_JSON( RESERVE, FMT1, FMT2 ) \
    if ( 1 == n ) \
    { \
        rc = vds_append_fmt( s, RESERVE, FMT1, data[ 0 ] ); \
    } \
    else \
    { \
        uint32_t i; \
        if ( brackets ) \
        { \
            rc = vds_append_str( &( def -> content ), "[" ); \
        } \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            rc = vds_append_fmt( s, RESERVE, FMT2, data[ i ] ); \
        } \
        if ( 0 == rc ) \
        { \
            rc = vds_append_fmt( s, RESERVE, FMT1, data[ n - 1 ] ); \
        } \
        if ( 0 == rc && brackets ) \
        { \
            rc = vds_append_str( &( def -> content ), "]" ); \
        } \
    }

#define MACRO_PRINT_DFLT( RESERVE, FMT1, FMT2 ) \
    if ( 1 == n ) \
    { \
        rc = vds_append_fmt( s, RESERVE, FMT1, data[ 0 ] ); \
    } \
    else \
    { \
        uint32_t i; \
        for ( i = 0; 0 == rc && i < n - 1; ++i ) \
        { \
            rc = vds_append_fmt( s, RESERVE, FMT2, data[ i ] ); \
        } \
        if ( 0 == rc ) \
        { \
            rc = vds_append_fmt( s, RESERVE, FMT1, data[ n - 1 ] ); \
        } \
    }

#define MACRO_PRINT( RESERVE, FMT1, FMT2 ) \
    if ( df_json == src -> output_format ) \
    { \
        MACRO_PRINT_JSON( RESERVE, FMT1, FMT2 ) \
    } \
    else \
    { \
        MACRO_PRINT_DFLT( RESERVE, FMT1, FMT2 ) \
    }

#define MACRO_PRINT_SHORT( RESERVE, FMT1, FMT2 ) \
    rc_t rc = 0; \
    p_dump_str s = &( def -> content ); \
    if ( src -> in_hex ) \
    { \
        MACRO_IN_HEX_SHORT \
    } \
    else if ( src -> value_trans ) \
    { \
        MACRO_TRANSLATE \
    } \
    else \
    { \
        MACRO_PRINT( RESERVE, FMT1, FMT2 ) \
    } \
    return rc;

#define MACRO_PRINT_LONG( RESERVE, FMT1, FMT2 ) \
    rc_t rc = 0; \
    p_dump_str s = &( def -> content ); \
    if ( src -> in_hex ) \
    { \
        MACRO_IN_HEX_LONG \
    } \
    else if ( src -> value_trans ) \
    { \
        MACRO_TRANSLATE \
    } \
    else \
    { \
        MACRO_PRINT( RESERVE, FMT1, FMT2 ) \
    } \
    return rc;

    
static rc_t vdt_format_slice_bb_u8( const p_dump_src src, const p_col_def def,
                                    const uint8_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_SHORT( MAX_CHARS_FOR_DEC_UINT64, "%u", "%u, " )
}

static rc_t vdt_format_slice_bb_i8( const p_dump_src src, const p_col_def def,
                                    const int8_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_SHORT( MAX_CHARS_FOR_DEC_UINT64, "%d", "%d, " )
}

static rc_t vdt_format_slice_bb_u16( const p_dump_src src, const p_col_def def,
                                     const uint16_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_SHORT( MAX_CHARS_FOR_DEC_UINT64, "%u", "%u, " )
}

static rc_t vdt_format_slice_bb_i16( const p_dump_src src, const p_col_def def,
                                     const int16_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_SHORT( MAX_CHARS_FOR_DEC_UINT64, "%d", "%d, " )
}

static rc_t vdt_format_slice_bb_u32( const p_dump_src src, const p_col_def def,
                                     const uint32_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_SHORT( MAX_CHARS_FOR_DEC_UINT64, "%u", "%u, " )
}

static rc_t vdt_format_slice_bb_i32( const p_dump_src src, const p_col_def def,
                                     const int32_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_SHORT( MAX_CHARS_FOR_DEC_UINT64, "%d", "%d, " )
}

static rc_t vdt_format_slice_bb_u64( const p_dump_src src, const p_col_def def,
                                     const uint64_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_LONG( MAX_CHARS_FOR_DEC_UINT64, "%lu", "%lu, " )
}

static rc_t vdt_format_slice_bb_i64( const p_dump_src src, const p_col_def def,
                                     const int64_t * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_LONG( MAX_CHARS_FOR_DEC_UINT64, "%ld", "%ld, " )
}

static rc_t vdt_format_slice_bb_f32( const p_dump_src src, const p_col_def def,
                                     const float * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_SHORT( MAX_CHARS_FOR_DOUBLE, "%e", "%e, " )
}

static rc_t vdt_format_slice_bb_f64( const p_dump_src src, const p_col_def def,
                                     const double * data, uint32_t n, bool brackets )
{
    MACRO_PRINT_LONG( MAX_CHARS_FOR_DOUBLE, "%e", "%e, " )
}

static rc_t vdt_format_slice_bb_ascii( const p_dump_src src, const p_col_def def,
                                       const uint8_t * data, uint32_t n, bool brackets )
{
    rc_t rc = 0;
    p_dump_str s = &( def -> content );
    if ( src -> in_hex )
    {
        MACRO_IN_HEX_SHORT
    }
    else
    {
        if ( df_json == src -> output_format )
        {
            rc = vds_append_fmt( s, n, "\"%.*s\"", n, data );
        }
        else
        {
            rc = vds_append_fmt( s, n, "%.*s", n, data );
        }
    }
    return rc;
}

#undef MACRO_IN_HEX
#undef MACRO_IN_HEX_SHORT
#undef MACRO_IN_HEX_LONG
#undef MACRO_TRANSLATE
#undef MACRO_PRINT
#undef MACRO_PRINT_SHORT
#undef MACRO_PRINT_LONG

/* on a byte-boundary, 1 dimensional array, default format */
static rc_t vdt_format_cell_bb_dim1_v2( const p_dump_src src, const p_col_def def )
{
    uint32_t n = src -> number_of_elements;
    switch( def -> type_desc . domain )
    {
        /* boolean */
        case 1 : return vdt_format_slice_bb_bool( src, def, src -> buf, n, true ); break;
        
        /* unsigned integers */
        case 2 : switch( def -> type_desc . intrinsic_bits )
                 {
                    case  8 : return vdt_format_slice_bb_u8( src, def, src -> buf, n, true ); break;
                    case 16 : return vdt_format_slice_bb_u16( src, def, src -> buf, n, true ); break;
                    case 32 : return vdt_format_slice_bb_u32( src, def, src -> buf, n, true ); break;
                    case 64 : return vdt_format_slice_bb_u64( src, def, src -> buf, n, true ); break;
                 }
                 break;
        
        /* signed integers */
        case 3 : switch( def -> type_desc . intrinsic_bits )
                 {
                    case  8 : return vdt_format_slice_bb_i8( src, def, src -> buf, n, true ); break;
                    case 16 : return vdt_format_slice_bb_i16( src, def, src -> buf, n, true ); break;
                    case 32 : return vdt_format_slice_bb_i32( src, def, src -> buf, n, true ); break;
                    case 64 : return vdt_format_slice_bb_i64( src, def, src -> buf, n, true ); break;
                 }
                 break;
        
        /* floats */
        case 4 : switch( def -> type_desc . intrinsic_bits )
                 {
                    case 32 : return vdt_format_slice_bb_f32( src, def, src -> buf, n, true ); break;
                    case 64 : return vdt_format_slice_bb_f64( src, def, src -> buf, n, true ); break;
                 }
                 break;

        /* text */
        case 5 :
        case 6 : return vdt_format_slice_bb_ascii( src, def, src -> buf, n, true ); break;

        default : /* this should not be reached - we checked before !*/ break;
    }
    return 0;
}

#define MACRO_DIM2( DATA_TYPE, SLICE_FUNC ) \
    rc_t rc = 0; \
    uint32_t dim = def -> type_desc . intrinsic_dim; \
    uint32_t n = src -> number_of_elements; \
    uint32_t group; \
    const DATA_TYPE * data = src -> buf; \
    if ( df_json == src -> output_format ) \
    { \
        rc = vds_append_str( &( def -> content ), "[" ); \
    } \
    for ( group = 0; 0 == rc && group < n; ++group ) \
    { \
        const DATA_TYPE * slice = &( data[ group * dim ] ); \
        rc = vds_append_str( &( def -> content ), "[" ); \
        if ( 0 == rc ) \
        { \
            rc = SLICE_FUNC( src, def, slice, dim, false ); \
        } \
        if ( 0 == rc ) \
        { \
            rc = vds_append_str( &( def -> content ), group < n - 1 ? "], " : "]" ); \
        } \
    } \
    if ( 0 == rc && df_json == src -> output_format ) \
    { \
        rc = vds_append_str( &( def -> content ), "]" ); \
    } \
    return rc;

/* on a byte-boundary, 2 dimensional array of bool ( aka 1 byte ) */
static rc_t vdt_format_cell_bb_dim2_bool( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( uint8_t, vdt_format_slice_bb_bool )
}

/* on a byte-boundary, 2 dimensional array of uint8 */
static rc_t vdt_format_cell_bb_dim2_u8( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( uint8_t, vdt_format_slice_bb_u8 )
}

/* on a byte-boundary, 2 dimensional array of uint16 */
static rc_t vdt_format_cell_bb_dim2_u16( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( uint16_t, vdt_format_slice_bb_u16 )
}

/* on a byte-boundary, 2 dimensional array of uint32 */
static rc_t vdt_format_cell_bb_dim2_u32( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( uint32_t, vdt_format_slice_bb_u32 )
}

/* on a byte-boundary, 2 dimensional array of uint64 */
static rc_t vdt_format_cell_bb_dim2_u64( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( uint64_t, vdt_format_slice_bb_u64 )
}

/* on a byte-boundary, 2 dimensional array of int8 */
static rc_t vdt_format_cell_bb_dim2_i8( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( int8_t, vdt_format_slice_bb_i8 )
}

/* on a byte-boundary, 2 dimensional array of uint16 */
static rc_t vdt_format_cell_bb_dim2_i16( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( int16_t, vdt_format_slice_bb_i16 )
}

/* on a byte-boundary, 2 dimensional array of int32 */
static rc_t vdt_format_cell_bb_dim2_i32( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( int32_t, vdt_format_slice_bb_i32 )
}

/* on a byte-boundary, 2 dimensional array of int64 */
static rc_t vdt_format_cell_bb_dim2_i64( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( int64_t, vdt_format_slice_bb_i64 )
}

/* on a byte-boundary, 2 dimensional array of floats */
static rc_t vdt_format_cell_bb_dim2_f32( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( float, vdt_format_slice_bb_f32 )
}

/* on a byte-boundary, 2 dimensional array of doubles */
static rc_t vdt_format_cell_bb_dim2_f64( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( double, vdt_format_slice_bb_f64 )
}

/* on a byte-boundary, 2 dimensional array of ascii-text ( 8 bit ) */
static rc_t vdt_format_cell_bb_dim2_ascii( const p_dump_src src, const p_col_def def )
{
    MACRO_DIM2( uint8_t, vdt_format_slice_bb_ascii )
}

#undef MACRO_DIM2

static rc_t vdt_format_bb_dim_trans_json( const p_dump_src src, const p_col_def def )
{
    uint32_t n = src -> number_of_elements;
    uint32_t group;
    const uint8_t * data = src -> buf;  /* this type-casts the ptr ! */
    p_dump_str ds = &( def -> content );

    rc_t rc = vds_append_str( ds, "[" );
    for ( group = 0; 0 == rc && group < n; ++group )
    {
        char trans_txt[ 512 ];
        size_t written;
        const uint8_t * slice = &( data[ group * def -> type_desc . intrinsic_dim ] );
        rc = def -> dim_trans_fn( trans_txt, sizeof trans_txt, &written,
                                    slice, src -> output_format );
        if ( 0 == rc )
        {
            bool not_last = ( group < n - 1 );
            rc = vds_append_fmt( ds, written + 3, not_last ? "%s," : "%s", trans_txt );
        }
    }
    if ( 0 == rc )
    {
        rc = vds_append_str( ds, "]" );
    }
    return rc;
}

static rc_t vdt_format_bb_dim_trans_dflt( const p_dump_src src, const p_col_def def )
{
    rc_t rc = 0;
    uint32_t n = src -> number_of_elements;
    uint32_t group;
    const uint8_t * data = src -> buf;  /* this type-casts the ptr ! */
    
    for ( group = 0; 0 == rc && group < n; ++group )
    {
        char trans_txt[ 512 ];
        size_t written;
        const uint8_t * slice = &( data[ group * def -> type_desc . intrinsic_dim ] );
        rc = def -> dim_trans_fn( trans_txt, sizeof trans_txt, &written,
                                    slice, src -> output_format );
        if ( 0 == rc )
        {
            rc = vds_append_fmt( &( def -> content ), written + 3, "[%s]", trans_txt );
        }
    }
    return rc;
}

/* on a byte-boundary, 2 dimensional array, default format */
static rc_t vdt_format_cell_bb_dim2_v2( const p_dump_src src, const p_col_def def )
{
    rc_t rc = 0;
    bool group_trans = ( ! src -> without_sra_types ) && ( NULL != def -> dim_trans_fn );
    
    if ( group_trans )
    {
        switch ( src -> output_format )
        {
            case df_json : return vdt_format_bb_dim_trans_json( src, def ); break;
            case df_xml  : ; /* fall through for now */
            default      : return vdt_format_bb_dim_trans_dflt( src, def ); break;
        }
    }
    else
    {
        switch( def -> type_desc . domain )
        {
            /* boolean */        
            case 1 : return vdt_format_cell_bb_dim2_bool( src, def ); break;

            /* unsigned integers */
            case 2 : switch( def -> type_desc . intrinsic_bits )
                    {
                        case  8 : return vdt_format_cell_bb_dim2_u8( src, def ); break;
                        case 16 : return vdt_format_cell_bb_dim2_u16( src, def ); break;
                        case 32 : return vdt_format_cell_bb_dim2_u32( src, def ); break;
                        case 64 : return vdt_format_cell_bb_dim2_u64( src, def ); break;
                    }
                    break;

            /* signed integers */
            case 3 : switch( def -> type_desc . intrinsic_bits )
                    {
                        case  8 : return vdt_format_cell_bb_dim2_i8( src, def ); break;
                        case 16 : return vdt_format_cell_bb_dim2_i16( src, def ); break;
                        case 32 : return vdt_format_cell_bb_dim2_i32( src, def ); break;
                        case 64 : return vdt_format_cell_bb_dim2_i64( src, def ); break;
                    }
                    break;
            
            /* floats */
            case 4 : switch( def -> type_desc . intrinsic_bits )
                    {
                        case 32 : return vdt_format_cell_bb_dim2_f32( src, def ); break;
                        case 64 : return vdt_format_cell_bb_dim2_f64( src, def ); break;
                    }
                    break;
            
            /* text */
            case 5 :
            case 6 : return vdt_format_cell_bb_dim2_ascii( src, def ); break; /* can that happen? */

            default : /* this should not be reached - we checked before !*/ break;
        }
    }
    return rc;
}

/*************************************************************************************
src         [IN] ... buffer containing the data
my_col_def  [IN] ... the definition of the column to be dumped

new and improved print of a cell, takes advantage of the fact that most ( if not 
all ) cells have offset == 0 and can be printed as a typecast to an array
*************************************************************************************/
rc_t vdt_format_cell_v2( const p_dump_src src, const p_col_def def, bool cell_debug )
{
    rc_t rc = 0;

    if ( cell_debug )
    {
        /* for debug purpose only: if '--cell-debug' via cmd-line, prepend each cell-value with
         * <dom=d, dim=i, num=n, bits=b, ofs=o>
         */
        rc = vdt_print_cell_debug( src, def );
    }

    if ( 0 == rc )
    {
        uint32_t dom  = def -> type_desc . domain;
        uint32_t dim  = def -> type_desc . intrinsic_dim;
        p_dump_str ds = &( def -> content );

        if ( dom < 1 || dom > 6 )
        {
            /* insurance against unknown domains */
            switch ( src -> output_format )
            {
                case df_json : rc = vds_append_fmt( ds, 32, "\"unknown domain: #%u\"", dom ); break;
                case df_xml  : rc = vds_append_fmt( ds, 32, "unknown domain: #%u", dom ); break;
                default      : rc = vds_append_fmt( ds, 32, "unknown domain: #%u", dom ); break;
            }
        }
        else if ( dim < 1 )
        {
            /* insurance against invalid dimension */
            switch ( src -> output_format )
            {
                case df_json : rc = vds_append_fmt( ds, 32, "\"invalid dimension: #%u\"", dim ); break;
                case df_xml  : rc = vds_append_fmt( ds, 32, "invalid dimension: #%u", dim ); break;
                default      : rc = vds_append_fmt( ds, 32, "invalid dimension: #%u", dim ); break;
            }
        }
        else if ( src -> number_of_elements > 0 )
        {
            /* we can take a simpler and faster approach if the data is on a byte-boundary! 
            * it always seems to be...
            */
            
            uint32_t bits = def -> type_desc . intrinsic_bits;
            uint32_t ofs  = src -> offset_in_bits;
            bool on_byte_boundary = ( 0 == ofs && ( 8 == bits || 16 == bits || 32 == bits || 64 == bits ) );
            
            /* precompute this setting to prevent it from beeing computed later in the detailed functions */
            src -> value_trans = ( ! src -> without_sra_types ) && ( NULL != def -> value_trans_fn );
            src -> group_trans = ( ! src -> without_sra_types ) && ( NULL != def -> dim_trans_fn );

            if ( on_byte_boundary )
            {
                /* on a byte-boundary, bit-size is 8 or 16 or 32 or 64 : the common case */
                if ( 1 == dim )
                {
                    /* the cell is a 1-dimensional vector of elements ... */
                    rc = vdt_format_cell_bb_dim1_v2( src, def );
                }
                else
                {
                    /* the cell is a 2-dimensional vector of elements ... */
                    rc = vdt_format_cell_bb_dim2_v2( src, def );
                }
            }    
            else
            {
                /* NOT on a byte-boundary, or bit-size is NOT 8 or 16 or 32 or 64 : the rare case */
                if ( 1 == dim )
                {
                    /* the cell is a 1-dimensional vector of elements ... */
                    rc = vdt_format_cell_nbb_dim1_v2( src, def );
                }
                else
                {
                    /* the cell is a 2-dimensional vector of elements ... */
                    rc = vdt_format_cell_nbb_dim2_v2( src, def );
                }
            }
        }
        else
        {
            /* we need format-specific handling in case of json/xml for an empty cell */
            switch ( src -> output_format )
            {
                case df_json : rc = vds_append_str( ds, "\"\"" ); break;
                case df_xml  : break;
                default      : break; /* nothing by default */
            }
        }
    }
    return rc;
}
