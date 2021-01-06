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

#include "vdb-dump-str.h"
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>
#include <klib/namelist.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

rc_t vds_make( p_dump_str s, const size_t limit, const size_t inc )
{
    if ( NULL == s )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    s -> str_limit = limit;
    s -> buf_inc = inc;
    s -> buf_size = ( ( limit == 0 ) ? inc : limit + 1 );
    s -> buf = malloc( s -> buf_size );
    if ( NULL == s -> buf )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    s -> str_len = 0;
    s -> buf[ 0 ] = 0;
    s -> truncated = false;
    return 0;
}

rc_t vds_free( p_dump_str s )
{
    if ( NULL == s )
    {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );
    }
    free( s -> buf );
    return 0;
}

rc_t vds_clear( p_dump_str s )
{
    if ( NULL == s )
    {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );
    }
    s -> buf[ 0 ] = 0;
    s -> str_len = 0;
    s -> truncated = false;
    return 0;
}

bool vds_truncated( p_dump_str s )
{
    if ( NULL == s )
    {
        return false;
    }
    return s -> truncated;
}

char *vds_ptr( p_dump_str s )
{
    if ( NULL == s )
    {
        return NULL;
    }
    return s -> buf;
}


static rc_t vds_inc_buffer( p_dump_str s, const size_t by_len )
{
    rc_t rc = 0;
    if ( NULL == s )
    {
        rc = RC( rcVDB, rcNoTarg, rcAllocating, rcParam, rcNull );
    }
    else
    {
        size_t needed = ( s -> str_len + by_len + 1 );
        if ( needed >= s -> buf_size )
        {
            char * tmp;
            size_t new_size = s -> buf_size * 2;
            
            if ( new_size < needed ) new_size = needed + s -> buf_inc;
            tmp = realloc( s -> buf, new_size );
            if ( NULL == tmp )
            {
                rc = RC( rcVDB, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            }
            else
            {
                s -> buf = tmp;
                s -> buf_size = new_size;
            }
        }
    }
    return rc;
}


static rc_t vds_truncate( p_dump_str s, const size_t appended )
{
    if ( NULL == s || NULL == s->buf )
    {
        return RC( rcVDB, rcNoTarg, rcResizing, rcParam, rcNull );
    }
    s -> str_len += appended;
    if ( ( s -> str_limit > 0 ) && ( s -> str_len > s -> str_limit ) )
    {
        s -> buf[ s -> str_limit ] = 0;
        s -> str_len = s -> str_limit;
        s -> truncated = true;
    }
    return 0;
}


rc_t vds_append_fmt( p_dump_str s, const size_t aprox_len, const char *fmt, ... )
{
    rc_t rc = 0;
    if ( NULL == s || NULL == fmt )
    {
        rc = RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    else if ( 0 == fmt[ 0 ] )
    {
        rc = RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcEmpty );
    }
    else if ( ( s -> str_limit > 0 )&&( s -> str_len >= s -> str_limit ) )
    {
        s->truncated = true;
    }
    else
    {
        rc = vds_inc_buffer( s, aprox_len );
        if ( 0 == rc )
        {
            va_list argp;
            size_t num_writ;
            
            va_start( argp, fmt );
            rc = string_vprintf( s -> buf + s -> str_len, s -> buf_size, &num_writ, fmt, argp );
            va_end( argp );
            
            if ( 0 == rc )
            {
                rc = vds_truncate( s, num_writ ); /* adjusts str_len */
            }
        }
    }
    return rc;
}


rc_t vds_append_str( p_dump_str s, const char *s1 )
{
    rc_t rc = 0;

    if ( ( NULL == s ) || ( NULL == s1 ) )
    {
        rc = RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    else
    {
        size_t append_len = string_size( s1 );
        if ( append_len > 0 )
        {
            if ( ( s -> str_limit > 0 ) && ( s -> str_len >= s -> str_limit ) )
            {
                s -> truncated = true;
            }
            else
            {
                rc = vds_inc_buffer( s, append_len );
                if ( 0 == rc )
                {
                    size_t l = s -> str_len;
                    size_t appended = string_copy( s -> buf + l, s -> buf_size - l, s1, append_len );
                    rc = vds_truncate( s, appended ); /* adjusts str_len */
                }
            }
        }
    }
    return rc;
}


rc_t vds_append_str_no_limit_check( p_dump_str s, const char *s1 )
{
    rc_t rc;
    size_t append_len;
    if ( ( NULL == s ) || ( NULL == s1 ) )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    append_len = string_size( s1 );
    if ( 0 == append_len)
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcEmpty );
    }
    rc = vds_inc_buffer( s, append_len );
    if ( 0 == rc )
    {
        size_t l = string_size( s -> buf );
        size_t appended = string_copy( s -> buf + l, s -> buf_size - l, s1, append_len );
        s -> str_len += appended;
    }
    return rc;
}


rc_t vds_rinsert( p_dump_str s, const char *s1 )
{
    size_t len;
    if ( ( NULL == s ) || ( NULL == s1 ) )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    len = string_size( s1 );
    if ( 0 == len )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcEmpty );
    }
    if ( s -> str_len <= len )
    {
        return 0;
    }
    len = s -> str_len - len;
    string_copy( s -> buf + len, sizeof( s->buf ) - len, s1, string_size( s1 ) );
    /* s -> str_len does not change */
    return 0;
}


rc_t vds_indent( p_dump_str s, const size_t limit, const size_t indent )
{
    rc_t rc;
    size_t remaining_chars, lines, indent_block;
    char *src, *dst;

    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    if ( s -> str_len < limit )
    {
        return 0;
    }
    remaining_chars = s -> str_len - limit;
    indent_block = limit - indent;
    lines = ( remaining_chars / indent_block ) + 1;
    /* increase the buffer by ( lines * ( indent + 1 ) ) bytes */
    rc = vds_inc_buffer( s, lines * ( indent + 1 ) );
    if ( 0 == rc )
    {
        size_t line_idx = 0;
        src = s -> buf + limit;
        while ( line_idx < lines )
        {
            dst = src + ( indent + 1 );
            memmove( dst, src, remaining_chars );
            *src = '\n';
            memset( src +1 , ' ', indent );
            remaining_chars -= indent_block;
            src += ( limit + 1 );
            line_idx++;
        }
        s -> str_len += ( lines * ( indent + 1 ) );
        s -> buf[ s -> str_len ] = 0;
    }
    return rc;
}

uint16_t vds_count_char( p_dump_str s, const char c )
{
    uint16_t res = 0;
    char *temp = s -> buf;
    while(  *temp )
    {
        if ( *temp == c ) res++;
        temp++;
    }
    return res;
}

void vds_escape_quotes_loop( p_dump_str s, const char to_escape,
                             const char escape_char )
{
    char *temp = s -> buf;
    while(  *temp )
    {
        if ( *temp == to_escape )
        {
            size_t to_move = string_size( temp ) + 1;
            memmove( temp + 1, temp, to_move );
            ( *temp ) = escape_char;
            temp++;
        }
        temp++;
    }
    s -> str_len = string_size( s -> buf );
}

rc_t vds_escape( p_dump_str s, const char to_escape, const char escape_char )
{
    rc_t rc;
    uint16_t n;

    if ( NULL == s )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    n = vds_count_char( s, to_escape );
    rc = vds_inc_buffer( s, n + 1 );
    if ( 0 == rc )
    {
        vds_escape_quotes_loop( s, to_escape, escape_char );
    }
    return rc;
}

rc_t vds_enclose_string( p_dump_str s, const char c_left, const char c_right )
{
    size_t to_move;
    rc_t rc;

    if ( NULL == s )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    to_move = s -> str_len + 1;
    rc = vds_inc_buffer( s, 2 );
    if ( 0 == rc )
    {
        memmove( s -> buf + 1, s -> buf, to_move );
        s -> buf[ 0 ] = c_left;
        s -> buf[ to_move ] = c_right;
        s -> buf[ to_move + 1 ] = 0;
        s -> str_len += 2;
    }
    return rc;
}

rc_t vds_2_csv( p_dump_str s )
{
    rc_t rc = 0;
    bool has_comma, has_quotes;

    if ( NULL == s )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    has_comma = ( strchr( s -> buf, ',' ) != NULL );
    has_quotes = ( strchr( s -> buf, '"' ) != NULL );
    if ( has_quotes )
    {
        rc = vds_escape( s, '"', '"' );
    }
    if ( ( 0 == rc ) && ( has_quotes || has_comma ) )
    {
        rc = vds_enclose_string( s, '"', '"' );
    }
    return rc;
}


/* ========================================================================================= */
static rc_t vds_add_to_sections( const char * path, uint32_t start, uint32_t len, VNamelist * sections )
{
    String S;
    StringInit( &S, &( path[ start ] ), len, len );
    return VNamelistAppendString ( sections, &S );
}

rc_t vds_path_to_sections( const char * path, char delim, VNamelist ** sections )
{
    rc_t rc = 0;
    if ( NULL == path || NULL == sections )
    {
        rc = RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    else
    {
        rc = VNamelistMake ( sections, 5 );
        if ( rc == 0 )
        {
            uint32_t idx = 0, start = 0, len = 0;
            while( 0 == rc && path[ idx ] != 0 )
            {
                if ( path[ idx ] == delim )
                {
                    if ( len > 0 )
                    {
                        rc = vds_add_to_sections( path, start, len, *sections );
                    }
                    start = idx + 1;
                    len = 0;
                }
                else
                {
                    len++;
                }
                idx++;
            }
            if ( 0 == rc && len > 0 )
            {
                rc = vds_add_to_sections( path, start, len, *sections );
            }
        }
    }
    return rc;
}
