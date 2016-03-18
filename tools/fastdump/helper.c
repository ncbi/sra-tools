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

#include "helper.h"
#include <klib/log.h>
#include <klib/printf.h>
#include <kfs/defs.h>

rc_t ErrMsg( const char * fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( rc == 0 )
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    va_end( list );
    return rc;
} 

rc_t CC ArgsOptionCount( const struct Args * self, const char * option_name, uint32_t * count );
rc_t CC ArgsOptionValue( const struct Args * self, const char * option_name, uint32_t iteration, const void ** value );

const char * get_str_option( const struct Args *args, const char *name, const char * dflt )
{
    const char* res = dflt;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc == 0 && count > 0 )
    {
        rc = ArgsOptionValue( args, name, 0, (const void**)&res );
        if ( rc != 0 ) res = dflt;
    }
    return res;
}

bool get_bool_option( const struct Args *args, const char *name )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    return ( rc == 0 && count > 0 );
}


uint64_t get_uint64_t_option( const struct Args * args, const char *name, uint64_t dflt )
{
    uint64_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( s != NULL )
    {
        size_t l = string_size( s );
        if ( l > 0 )
        {
            char * endptr;
            res = strtol( s, &endptr, 0 );
        }
    }
    return res;

}

size_t get_size_t_option( const struct Args * args, const char *name, size_t dflt )
{
    size_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( s != NULL )
    {
        size_t l = string_size( s );
        if ( l > 0 )
        {
            size_t multipl = 1;
            switch( s[ l - 1 ] )
            {
                case 'k' :
                case 'K' : multipl = 1024; break;
                case 'm' :
                case 'M' : multipl = 1024 * 1024; break;
                case 'g' :
                case 'G' : multipl = 1024 * 1024 * 1024; break;
            }
            
            if ( multipl > 1 )
            {
                char * src = string_dup( s, l - 1 );
                if ( src != NULL )
                {
                    char * endptr;
                    res = strtol( src, &endptr, 0 ) * multipl;
                    free( src );
                }
            }
            else
            {
                char * endptr;
                res = strtol( s, &endptr, 0 );
            }
        }
    }
    return res;
}


rc_t make_SBuffer( SBuffer * buffer, size_t len )
{
    rc_t rc = 0;
    String * S = &buffer->S;
    S->addr = malloc( len );
    if ( S->addr == NULL )
    {
        S->size = S->len = 0;
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "malloc( %d ) -> %R", ( len ), rc );
    }
    else
    {
        S->size = 0;
        S->len = 0;
        buffer->buffer_size = len;
    }
    return rc;
}


void release_SBuffer( SBuffer * buffer )
{
    if ( buffer != NULL )
    {
        String * S = &buffer->S;
        if ( S->addr != NULL )
            free( ( void * ) S->addr );
    }
}


rc_t print_to_SBufferV( SBuffer * buffer, const char * fmt, va_list args )
{
    char * dst = ( char * )buffer->S.addr;
    size_t num_writ = 0;
    
    rc_t rc = string_vprintf( dst, buffer->buffer_size, &num_writ, fmt, args );
    if ( rc != 0 )
        ErrMsg( "string_vprintf() -> %R", rc );
    buffer->S.len = buffer->S.size = num_writ;
    
    return rc;
}


rc_t print_to_SBuffer( SBuffer * buffer, const char * fmt, ... )
{
    rc_t rc;
    va_list args;

    va_start( args, fmt );
    rc = print_to_SBufferV( buffer, fmt, args );
    va_end( args );

    return rc;
}


rc_t add_column( const VCursor * cursor, const char * name, uint32_t * id )
{
    rc_t rc = VCursorAddColumn( cursor, id, name );
    if ( rc != 0 )
        ErrMsg( "VCursorAddColumn( '%s' ) -> %R", name, rc );
    return rc;
}


rc_t make_row_iter( struct num_gen * ranges, int64_t first, uint64_t count, 
                    const struct num_gen_iter ** iter )
{
    rc_t rc;
    if ( num_gen_empty( ranges ) )
    {
        rc = num_gen_add( ranges, first, count );
        if ( rc != 0 )
            ErrMsg( "num_gen_add( %li, %ld ) -> %R", first, count, rc );
    }
    else
    {
        rc = num_gen_trim( ranges, first, count );
        if ( rc != 0 )
            ErrMsg( "num_gen_trim( %li, %ld ) -> %R", first, count, rc );
    }
    rc = num_gen_iterator_make( ranges, iter );
    if ( rc != 0 )
        ErrMsg( "num_gen_iterator_make() -> %R", rc );
    return rc;
}


rc_t split_string( String * in, String * p0, String * p1, uint32_t ch )
{
    rc_t rc = 0;
    char * ch_ptr = string_chr( in->addr, in->size, ch );
    if ( ch_ptr == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
    else
    {
        p0->addr = in->addr;
        p0->len  = p0->size = ( ch_ptr - p0->addr );
        p1->addr = ch_ptr + 1;
        p1->len  = p1->size = in->len - ( p0->len + 1 );
    }
    return rc;
}


format_t get_format_t( const char * format )
{
    format_t res = ft_special;
    if ( format != NULL && format[ 0 ] != 0 )
    {
        String Format, FastqFormat;
        StringInitCString( &Format, format );
        StringInitCString( &FastqFormat, "fastq" );
        if ( 0 == StringCaseCompare ( &Format, &FastqFormat ) )
            res = ft_fastq;
        else
        {
            String LookupFormat;
            StringInitCString( &LookupFormat, "lookup" );
            if ( 0 == StringCaseCompare ( &Format, &LookupFormat ) )
                res = ft_lookup;
            else
            {
                String TestFormat;
                StringInitCString( &TestFormat, "test" );
                if ( 0 == StringCaseCompare ( &Format, &TestFormat ) )
                    res = ft_test;
            }
        }
    }
    return res;
}


uint64_t make_key( int64_t seq_spot_id, uint32_t seq_read_id )
{
    uint64_t key = seq_spot_id;
    key <<= 1;
    key |= ( seq_read_id == 2 ) ? 1 : 0;
    return key;
}


void pack_4na( const String * unpacked, SBuffer * packed )
{
    uint32_t i;
    char * src = ( char * )unpacked->addr;
    char * dst = ( char * )packed->S.addr;
    uint16_t dna_len = ( unpacked->len & 0xFFFF );
    uint32_t len = 0;
    dst[ len++ ] = ( dna_len >> 8 );
    dst[ len++ ] = ( dna_len & 0xFF );
    for ( i = 0; i < unpacked->len; ++i )
    {
        if ( len < packed->buffer_size )
        {
            char base = ( src[ i ] & 0x0F );
            if ( 0 == ( i & 0x01 ) )
                dst[ len ] = ( base << 4 );
            else
                dst[ len++ ] |= base;
        }
    }
    if ( unpacked->len & 0x01 )
        len++;
    packed->S.size = packed->S.len = len;
}


static char x4na_to_ASCII[ 16 ] =
{
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       'N', 'A', 'C', 'N', 'G', 'N', 'N', 'N', 'T', 'N', 'N', 'N', 'N', 'N', 'N', 'N'
};


void unpack_4na( const String * packed, SBuffer * unpacked )
{
    uint32_t i;
    char * src = ( char * )packed->addr;
    char * dst = ( char * )unpacked->S.addr;
    uint32_t dst_idx = 0;
    uint16_t dna_len = src[ 0 ];
    dna_len <<= 8;
    dna_len |= src[ 1 ];
    for ( i = 2; i < packed->len; ++i )
    {
        uint8_t packed_byte = src[ i ];
        if ( dst_idx < unpacked->buffer_size )
            dst[ dst_idx++ ] = x4na_to_ASCII[ ( packed_byte >> 4 ) & 0x0F ];
        if ( dst_idx < unpacked->buffer_size )
            dst[ dst_idx++ ] = x4na_to_ASCII[ packed_byte & 0x0F ];
    }
    unpacked->S.len = unpacked->S.size = dna_len;
    dst[ dna_len + 2 ] = 0;
}


uint64_t calc_percent( uint64_t max, uint64_t value, uint16_t digits )
{
    uint64_t res = value;
    switch ( digits )
    {
        case 1 : res *= 1000; break;
        case 2 : res *= 10000; break;
        default : res *= 100; break;
    }
    if ( max > 0 ) res /= max;
    return res;
}


bool file_exists( const KDirectory * dir, const char * fmt, ... )
{
    uint32_t pt;
    va_list list;
    
    va_start( list, fmt );
    pt = KDirectoryVPathType( dir, fmt, list );
    va_end( list );

    return ( pt == kptFile ) ;
}