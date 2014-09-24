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
#include "context.h"

#include <kfs/file.h>
#include <klib/printf.h>
#include <os-native.h>

#include <sysalloc.h>
#include <bitstr.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


int helper_str_cmp( const char *a, const char *b )
{
    size_t asize = string_size ( a );
    size_t bsize = string_size ( b );
    return strcase_cmp ( a, asize, b, bsize, ( asize > bsize ) ? asize : bsize );
}


rc_t char_buffer_init( p_char_buffer buffer, const size_t size )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( buffer != NULL )
    {
        buffer->len  = 0;
        rc = 0;
        if ( size > 0 )
        {
            buffer->ptr  = malloc( size );
            if ( buffer->ptr != NULL )
                buffer->size = size;
            else
                rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            buffer->ptr  = NULL;
            buffer->size = 0;
        }
        buffer->trans_ptr = buffer->ptr;
    }
    return rc;
}


void char_buffer_destroy( p_char_buffer buffer )
{
    if ( buffer != NULL )
    {
        free( buffer->ptr );
        buffer->ptr = NULL;
        buffer->len  = 0;
        buffer->size = 0;
    }
}


rc_t char_buffer_realloc( p_char_buffer buffer, const size_t new_size )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( buffer != NULL )
    {
        if ( buffer->ptr == NULL )
            rc = char_buffer_init( buffer, new_size );
        else
        {
            rc = 0;
            if ( new_size > buffer->size )
            {
                buffer->ptr = realloc( buffer->ptr, new_size );
                if ( buffer->ptr != NULL )
                {
                    buffer->size = new_size;
                    buffer->trans_ptr = buffer->ptr;
                }
                else
                    rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            }
        }
    }
    return rc;
}


rc_t char_buffer_append_cstring( p_char_buffer buffer, const char * s )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( buffer != NULL )
    {
        size_t s_len = string_size( s );
        if ( s_len > 0 )
        {
            if ( buffer->ptr == NULL )
            {
                rc = char_buffer_init( buffer, s_len + 1 );
                if ( rc == 0 )
                {
                    string_copy ( buffer->ptr, buffer->size, s, s_len );
                }
            }
            else
            {
                size_t needed = buffer->len + s_len + 1;
                if ( needed < buffer->size )
                {
                    string_copy ( buffer->ptr + buffer->len, 
                                  buffer->size - buffer->len, s, s_len );
                    buffer->len += s_len;
                    rc = 0;
                }
                else
                {
                    rc = char_buffer_realloc( buffer, needed );
                    if ( rc == 0 )
                    {
                        string_copy ( buffer->ptr + buffer->len, 
                                      buffer->size - buffer->len, s, s_len );
                        buffer->len += s_len;
                    }
                }
            }
        }
    }
    return rc;
}


rc_t char_buffer_printfv( p_char_buffer buffer, const size_t estimated_len,
                          const char * fmt, va_list args )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( buffer != NULL )
    {
        bool done = false;
        size_t len = estimated_len + 1;

        while ( !done )
        {
            if ( buffer->ptr == NULL )
                rc = char_buffer_init( buffer, len );
            else
            {
                size_t needed = buffer->len + len;
                rc = 0;
                if ( needed > buffer->size )
                    rc = char_buffer_realloc( buffer, needed );
            }
            done = ( rc != 0 );
            if ( !done )
            {
                size_t written;
                rc = string_vprintf ( buffer->ptr + buffer->len,
                                      buffer->size - buffer->len,
                                      &written,
                                      fmt,
                                      args );
                done = ( rc == 0 );
                if ( done )
                    buffer->len += written;
                else
                    len += len;
            }
        }
    }
    return rc;
}


rc_t char_buffer_printf( p_char_buffer buffer, const size_t estimated_len,
                         const char * fmt, ... )
{
    rc_t rc;
    va_list args;
    va_start ( args, fmt );
    rc = char_buffer_printfv( buffer, estimated_len, fmt, args );
    va_end ( args );
    return rc;
}


rc_t char_buffer_saveto( p_char_buffer buffer, const char * filename )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( buffer != NULL && buffer->len > 0 && filename != NULL )
    {
        KDirectory * dir;
        rc = KDirectoryNativeDir ( &dir );
        if ( rc == 0 )
        {
            KFile * f;
            rc = KDirectoryCreateFile ( dir, &f, true, 0664, kcmInit, "%s", filename );
            if ( rc == 0 )
            {
                rc = KFileWrite ( f, 0, buffer->ptr, buffer->len, NULL );
                KFileRelease( f );
            }
            KDirectoryRelease( dir );
        }
    }
    return rc;
}


rc_t int_buffer_init( p_int_buffer buffer, const size_t size )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( buffer != NULL )
    {
        buffer->len  = 0;
        rc = 0;
        if ( size > 0 )
        {
            buffer->ptr  = malloc( size * sizeof( buffer->ptr[ 0 ] ) );
            if ( buffer->ptr != NULL )
                buffer->size = size;
            else
                rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            buffer->ptr  = NULL;
            buffer->size = 0;
        }
    }
    return rc;

}


void int_buffer_destroy( p_int_buffer buffer )
{
    if ( buffer != NULL )
    {
        free( buffer->ptr );
        buffer->ptr = NULL;
        buffer->len  = 0;
        buffer->size = 0;
    }
}


rc_t int_buffer_realloc( p_int_buffer buffer, const size_t new_size )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( buffer != NULL )
    {
        if ( buffer->ptr == NULL )
            rc = int_buffer_init( buffer, new_size );
        else
        {
            rc = 0;
            if ( new_size > buffer->size )
            {
                buffer->ptr = realloc( buffer->ptr, new_size * sizeof( buffer->ptr[ 0 ] ) );
                if ( buffer->ptr != NULL )
                    buffer->size = new_size;
                else
                    rc= RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            }
        }
    }
    return rc;
}


/*
 * calls the given manager to create a new SRA-schema
 * takes the list of user-supplied schema's (which can be empty)
 * and let the created schema parse all of them
*/
rc_t helper_parse_schema( const VDBManager *my_manager,
                          VSchema **new_schema,
                          const KNamelist *schema_list )
{
    rc_t rc;
    if ( my_manager == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( new_schema == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );

    rc = VDBManagerMakeSRASchema( my_manager, new_schema );
    DISP_RC( rc, "VDBManagerMakeSRASchema() failed" );

    if ( ( rc == 0 )&&( schema_list != NULL ) )
    {
        uint32_t count;
        if ( KNamelistCount( schema_list, &count ) == 0 )
        {
            uint32_t idx;
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char *s;
                if ( KNamelistGet( schema_list, idx, &s ) == 0 )
                {
                    rc = VSchemaParseFile( *new_schema, "%s", s );
                    DISP_RC( rc, "VSchemaParseFile() failed" );
                }
            }
        }
    }
    return rc;
}


rc_t helper_make_namelist_from_string( const KNamelist **list, 
                                       const char * src,
                                       const char split_char )
{
    VNamelist *v_names;
    rc_t rc = VNamelistMake ( &v_names, 5 );
    if ( rc == 0 )
    {
        if ( ( src != NULL )&&( src[ 0 ] != 0 ) )
        {
            char * s = string_dup_measure ( src, NULL );
            if ( s )
            {
                uint32_t str_begin = 0;
                uint32_t str_end = 0;
                char c;
                do
                {
                    c = s[ str_end ];
                    if ( c == split_char || c == 0 )
                    {
                        if ( str_begin < str_end )
                        {
                            char c_temp = c;
                            s[ str_end ] = 0;
                            rc = VNamelistAppend ( v_names, &(s[str_begin]) );
                            s[ str_end ] = c_temp;
                        }
                        str_begin = str_end + 1;
                    }
                    str_end++;
                } while ( c != 0 && rc == 0 );
                free( s );
            }
        }
        rc = VNamelistToConstNamelist ( v_names, list );
        VNamelistRelease( v_names );
    }
    return rc;
}


/*************************************************************************************
helper-function to check if a given table is in the list of tables
if found put that name into the dump-context
*************************************************************************************/
bool helper_take_this_table_from_db( const VDatabase * db,
                                     p_stat_ctx ctx,
                                     const char * table_to_find )
{
    bool we_found_a_table = false;
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( db, &tbl_names );
    DISP_RC( rc, "VDatabaseListTbl() failed" );
    if ( rc == 0 )
    {
        uint32_t n;
        rc = KNamelistCount( tbl_names, &n );
        DISP_RC( rc, "KNamelistCount() failed" );
        if ( ( rc == 0 )&&( n > 0 ) )
        {
            uint32_t i;
            for ( i = 0; i < n && rc == 0 && !we_found_a_table; ++i )
            {
                const char *tbl_name;
                rc = KNamelistGet( tbl_names, i, &tbl_name );
                DISP_RC( rc, "KNamelistGet() failed" );
                if ( rc == 0 )
                {
                    if ( helper_str_cmp( tbl_name, table_to_find ) == 0 )
                    {
                        ctx_set_table( ctx, tbl_name );
                        we_found_a_table = true;
                    }
                }
            }
        }
        rc = KNamelistRelease( tbl_names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    return we_found_a_table;
}


bool helper_take_1st_table_from_db( const VDatabase *db,
                                    p_stat_ctx ctx )
{
    bool we_found_a_table = false;
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( db, &tbl_names );
    DISP_RC( rc, "VDatabaseListTbl() failed" );
    if ( rc == 0 )
    {
        uint32_t n;
        rc = KNamelistCount( tbl_names, &n );
        DISP_RC( rc, "KNamelistCount() failed" );
        if ( ( rc == 0 )&&( n > 0 ) )
        {
            const char *tbl_name;
            rc = KNamelistGet( tbl_names, 0, &tbl_name );
            DISP_RC( rc, "KNamelistGet() failed" );
            if ( rc == 0 )
            {
                ctx_set_table( ctx, tbl_name );
                we_found_a_table = true;
            }
        }
        rc = KNamelistRelease( tbl_names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    return we_found_a_table;
}


char * helper_concat( const char * s1, const char * s2 )
{
    size_t l1 = string_size ( s1 );
    size_t l2 = string_size ( s2 );
    size_t l = l1 + l2 + 1;
    char * res = malloc( l );
    if ( res != NULL )
    {
        size_t l3 = string_copy ( res, l, s1, l1 );
        string_copy ( res + l3, l - l3, s2, l2 );
    }
    return res;
}


double percent( const uint64_t value, const uint64_t sum )
{
    double res = 0.0;
    if ( sum > 0 && value > 0 )
    {
        res = value;
        res *= 100;
        res /= sum;
    }
    return res;
}


/*********************************************************************************
    "bases(bio)" ---> module = "bases", param = "bio"
*********************************************************************************/
rc_t helper_split_by_parenthesis( const char * src, char ** module, char ** param )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( src != NULL && module != NULL && param!= NULL )
    {
        size_t src_size = string_size( src );
        *module = NULL;
        *param  = NULL;
        if ( src_size < 1 )
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcEmpty );
        else
        {
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            *module = malloc( src_size + 1 );
            if ( *module != NULL )
            {
                *param = malloc( src_size + 1 );
                if ( *param == NULL )
                {
                    free( *module );
                    *module = NULL;
                }
                else
                {
                    size_t src_idx, dst_idx;
                    bool b_module = true;
                    dst_idx = 0;
                    for ( src_idx = 0; src_idx < src_size; ++src_idx )
                    {
                        char c = src[ src_idx ];
                        if ( b_module )
                        {
                            if ( c == '(' )
                            {
                                (*module)[ dst_idx ] = 0;
                                dst_idx = 0;
                                b_module = false;
                            }
                            else
                                (*module)[ dst_idx++ ] = c;
                        }
                        else
                        {
                            if ( c != ')' )
                                (*param)[ dst_idx++ ] = c;
                        }
                    }
                    if ( b_module )
                    {
                        (*module)[ dst_idx ] = 0;
                        free( *param );
                        *param = NULL;
                    }
                    else
                        (*param)[ dst_idx ] = 0;

                    rc = 0;
                }
            }
        }
    }
    return rc;
}


static rc_t read_value( void * dst, const VCursor * cur, 
                        const uint32_t cur_idx, const uint32_t dst_bits )
{
    uint32_t elem_bits, boff, elem_count;
    const void * base;
    rc_t rc = VCursorCellData ( cur, cur_idx, &elem_bits,
                                &base, &boff, &elem_count );
    if ( rc == 0 )
    {
        if ( elem_bits > dst_bits )
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        else
        {
            if ( boff == 0 )
                memmove( dst, base, elem_bits >> 3 );
            else
                bitcpy ( dst, 0, base, boff, elem_bits );
        }
    }
    return rc;
}


rc_t helper_read_uint64( const VCursor * cur, const uint32_t cur_idx,
                         uint64_t *value )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( cur != NULL && value != NULL )
    {
        *value = 0;
        rc = read_value( value, cur, cur_idx, 64 );
    }
    return rc;
}


rc_t helper_read_uint32( const VCursor * cur, const uint32_t cur_idx,
                         uint32_t *value )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( cur != NULL && value != NULL )
    {
        *value = 0;
        rc = read_value( value, cur, cur_idx, 32 );
    }
    return rc;
}


static rc_t read_int32_values( p_int_buffer buf, const void * base,
                               uint32_t boff, uint32_t elem_count )
{
    rc_t rc = 0;
    if ( boff == 0 )
    {
        buf->trans_ptr = base;
        buf->len = elem_count;
    }
    else
    {
        if ( buf->size < elem_count )
            rc = int_buffer_realloc( buf, elem_count );
        if ( rc == 0 )
        {
            bitcpy ( buf->ptr, 0, base, boff, elem_count << 2 );
            buf->trans_ptr = buf->ptr;
            buf->len = elem_count;
        }
    }
    return rc;
}


static rc_t read_int8_values( p_int_buffer buf, const void * base,
                              uint32_t boff, uint32_t elem_count )
{
    rc_t rc = 0;

    if ( buf->size < elem_count )
        rc = int_buffer_realloc( buf, elem_count );
    if ( rc == 0 )
    {
        if ( boff == 0 )
        {
            const uint8_t * src = base;
            uint32_t idx;
            for ( idx = 0; idx < elem_count; idx++ )
                buf->ptr[ idx ] = src[ idx ];
        }
        else
        {
            uint8_t * src = malloc( elem_count );
            if ( src != NULL )
            {
                uint32_t idx;
                bitcpy ( src, 0, base, boff, elem_count );
                for ( idx = 0; idx < elem_count; idx++ )
                    buf->ptr[ idx ] = src[ idx ];
                free( src );
            }
            else
                rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
    }
    if ( rc == 0 )
    {
        buf->trans_ptr = buf->ptr;
        buf->len = elem_count;
    }
    return rc;
}


rc_t helper_read_int32_values( const VCursor * cur, const uint32_t cur_idx,
                               p_int_buffer buf )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( cur != NULL && buf != NULL )
    {
        uint32_t elem_bits, boff, elem_count;
        const void * base;
        rc = VCursorCellData ( cur, cur_idx, &elem_bits,
                               &base, &boff, &elem_count );
        if ( rc == 0 )
        {
            switch( elem_bits )
            {
            case 32 : rc = read_int32_values( buf, base, boff, elem_count ); break;
            case  8 : rc = read_int8_values( buf, base, boff, elem_count ); break;
            }
        }
        else
            buf->len = 0;
    }
    return rc;
}


rc_t helper_read_char_values( const VCursor * cur, const uint32_t cur_idx,
                              p_char_buffer buf )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( cur != NULL && buf != NULL )
    {
        uint32_t elem_bits, boff, elem_count;
        const void * base;
        rc = VCursorCellData ( cur, cur_idx, &elem_bits,
                               &base, &boff, &elem_count );
        if ( rc ==  0 )
        {
            if ( boff == 0 )
            {
                buf->trans_ptr = base;
                buf->len = elem_count;
            }
            else
            {
                if ( buf->size < elem_count )
                    rc = char_buffer_realloc( buf, elem_count );
                if ( rc == 0 )
                {
                    bitcpy ( buf->ptr, 0, base, boff, elem_count * elem_bits );
                    buf->trans_ptr = buf->ptr;
                    buf->len = elem_count;
                }
            }
        }
        else
            buf->len = 0;
    }
    return rc;
}
