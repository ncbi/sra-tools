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

#include <vdb/schema.h>
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
#define DISP_RC(rc,err) if( rc != 0 ) LOGERR( klogInt, rc, err );

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

static rc_t vdb_dump_txt_ascii( p_dump_str s, const p_dump_src src,
                                const p_col_def def )
{
    char *src_ptr = ( char* )src -> buf + BYTE_OFFSET( src -> offset_in_bits );
    return vds_append_fmt( s, src -> number_of_elements,
                           "%.*s", src -> number_of_elements, src_ptr );

}

static rc_t vdb_dump_hex_char( char * temp, uint32_t * idx, const uint8_t c )
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

static rc_t vdb_dump_hex_ascii( p_dump_str s, const p_dump_src src,
                                const p_col_def def )
{
    rc_t rc = 0;
    char *src_ptr = ( char* )src -> buf + BYTE_OFFSET( src -> offset_in_bits );
    char *tmp = malloc( src -> number_of_elements * 4 );
    if ( NULL != tmp )
    {
        uint32_t i, dst = 0;
        for ( i = 0; i < src->number_of_elements && 0 == rc; ++i )
        {
            rc = vdb_dump_hex_char( tmp, &dst, src_ptr[ i ] );
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
static rc_t vdt_dump_ascii( p_dump_str s, const p_dump_src src,
                            const p_col_def def )
{
    rc_t rc;
    if ( src -> in_hex )
    {
        rc = vdb_dump_hex_ascii( s, src, def );
        DISP_RC( rc, "vdb_dump_hex_ascii() failed" );
    }
    else
    {
        rc = vdb_dump_txt_ascii( s, src, def );
        DISP_RC( rc, "vdb_dump_txt_ascii() failed" );
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
static rc_t vdt_dump_unicode( p_dump_str s, const p_dump_src src,
                              const p_col_def def )
{
    rc_t rc;
    if ( src -> in_hex )
    {
        rc = vdb_dump_hex_ascii( s, src, def );
        DISP_RC( rc, "vdb_dump_hex_ascii() failed" );
    }
    else
    {
        rc = vdb_dump_txt_ascii( s, src, def );
        DISP_RC( rc, "vdb_dump_txt_ascii() failed" );
    }
    if ( 0 == rc )
    {
        src -> element_idx += src -> number_of_elements;
        src -> offset_in_bits += ( def -> type_desc . intrinsic_bits * src -> number_of_elements );
    }
    return rc;
}

void vdt_move_to_value( void* dst, const p_dump_src src, const uint32_t n_bits )
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
    DISP_RC( rc, "dump_str_append_str() failed" )
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
    if ( ( ! src -> without_sra_types ) && ( NULL != def -> value_trans_fct ) )
    {
        const char *txt = def -> value_trans_fct( ( uint32_t )value );
        rc = vds_append_str( s, txt );
        DISP_RC( rc, "dump_str_append_str() failed" )
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
        DISP_RC( rc, "dump_str_append_fmt() failed" )
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
    if ( ( ! src -> without_sra_types ) && ( NULL != def -> value_trans_fct ) )
    {
        const char *txt = def -> value_trans_fct( ( uint32_t )value );
        rc = vds_append_str( s, txt );
        DISP_RC( rc, "dump_str_append_str() failed" )
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
        DISP_RC( rc, "dump_str_append_fmt() failed" )
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
            DISP_RC( rc, "dump_str_append_fmt() failed" )
        }
        else if ( BITSIZE_OF_DOUBLE == def -> type_desc . intrinsic_bits )
        {
            double value;
            vdt_move_to_value( &value, src, def -> type_desc . intrinsic_bits );
            rc = vds_append_fmt( s, MAX_CHARS_FOR_DOUBLE, "%e", value );
            DISP_RC( rc, "dump_str_append_fmt() failed" )
        }
        else
        {
            rc = vds_append_str( s, "unknown float-type" );
            DISP_RC( rc, "dump_str_append_str() failed" )
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
    DISP_RC( rc, "dump_str_append_fmt() failed" )
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
    vdt_dump_ascii,
    vdt_dump_unicode
};

rc_t vdt_dump_dim_trans( const p_dump_src src, const p_col_def def,
                         const int dimension )
{
    rc_t rc = 0;
    char *s;
    uint8_t *sbuf = ( uint8_t * )src -> buf;
    sbuf += ( src -> offset_in_bits >> 3 );
    s = def -> dim_trans_fct( sbuf );
    src -> offset_in_bits += ( def -> type_desc . intrinsic_bits * dimension );
    rc = vds_append_str( &( def -> content ), s );
    DISP_RC( rc, "dump_str_append_str() failed" )
    /* we have to free, because dim_trans_fct()
       makes the string dynamically */
    free( s );
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
            DISP_RC( rc, "dump_str_append_str() failed" )
        }
        if ( rc == 0 )
        {
            rc = vdt_DomainDispatch[ selection ]( &( def -> content ), src, def );
            DISP_RC( rc, "DomainDispatch[]() failed" )
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
rc_t vdt_dump_element( const p_dump_src src, const p_col_def def, bool bracket )
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
        DISP_RC( rc, "DomainDispatch[]() failed" )
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
            bool trans = ( ( ! src -> without_sra_types ) && ( NULL != def -> dim_trans_fct ) );
            bool paren = ( ( src -> number_of_elements > 1 ) || ( !trans ) );

            if ( paren )
            {
                rc = vds_append_str( &( def -> content), bracket ? "[" : "{" );
                DISP_RC( rc, "dump_str_append_str() failed" )
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
                DISP_RC( rc, "dump_str_append_str() failed" )
            }
        }
    }
    src -> element_idx++;
    return rc;
}

void vdm_clear_recorded_errors( void )
{
    rc_t rc;
    const char * filename;
    const char * funcname;
    uint32_t line_nr;
    while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &line_nr ) )
    {
    }
}

static rc_t walk_sections( const VDatabase * base_db, const VDatabase ** sub_db,
                    const VNamelist * sections, uint32_t count )
{
    rc_t rc = 0;
    const VDatabase * parent_db = base_db;
    if ( 0 == count )
    {
        rc = VDatabaseAddRef ( parent_db );
        DISP_RC( rc, "VDatabaseAddRef() failed" );
    }
    else
    {
        uint32_t idx;
        for ( idx = 0; 0 == rc && idx < count; ++idx )
        {
            const char * dbname;
            rc = VNameListGet ( sections, idx, &dbname );
            DISP_RC( rc, "VNameListGet() failed" );
            if ( 0 == rc )
            {
                const VDatabase * temp;
                rc = VDatabaseOpenDBRead ( parent_db, &temp, "%s", dbname );
                DISP_RC( rc, "VDatabaseOpenDBRead() failed" );
                if ( 0 == rc && idx > 0 )
                {
                    rc = VDatabaseRelease ( parent_db );
                    DISP_RC( rc, "VDatabaseRelease() failed" );
                }
                if ( 0 == rc )
                {
                    parent_db = temp;
                }
            }
        }
    }
    
    if ( 0 == rc )
    {
        *sub_db = parent_db;
    }
    return rc;
}

rc_t check_table_empty( const VTable * tab )
{
    bool empty;
    rc_t rc = VTableIsEmpty( tab, &empty );
    DISP_RC( rc, "VTableIsEmpty() failed" );
    if ( 0 == rc && empty )
    {
        vdm_clear_recorded_errors();
        KOutMsg( "the requested table is empty!\n" );
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTable, rcEmpty );
    }
    return rc;
}

rc_t open_table_by_path( const VDatabase * db, const char * inner_db_path, const VTable ** tab )
{
    VNamelist * sections;
    rc_t rc = vds_path_to_sections( inner_db_path, '.', &sections );
    DISP_RC( rc, "vds_path_to_sections() failed" );
    if ( 0 == rc )
    {
        uint32_t count;
        rc = VNameListCount ( sections, &count );
        DISP_RC( rc, "VNameListCount() failed" );
        if ( 0 == rc && count > 0 )
        {
            const VDatabase * sub_db;
            rc = walk_sections( db, &sub_db, sections, count - 1 );
            if ( 0 == rc )
            {
                const char * tabname;
                rc = VNameListGet ( sections, count - 1, &tabname );
                DISP_RC( rc, "VNameListGet() failed" );
                if ( 0 == rc )
                {
                    rc = VDatabaseOpenTableRead( sub_db, tab, "%s", tabname );
                    DISP_RC( rc, "VDatabaseOpenTableRead() failed" );
                    if ( 0 == rc )
                    {
                        rc = check_table_empty( *tab );
                        if ( 0 != rc )
                        {
                            rc_t rc2 = VTableRelease( *tab );
                            DISP_RC( rc2, "VTableRelease() failed" );
                            tab = NULL;
                        }
                    }
                }
                {
                    rc_t rc2 = VDatabaseRelease ( sub_db );
                    DISP_RC( rc2, "VDatabaseRelease() failed" );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VNamelistRelease ( sections );
            DISP_RC( rc2, "VNamelistRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}
