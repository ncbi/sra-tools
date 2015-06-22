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
    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    s->str_limit = limit;
    s->buf_inc = inc;
    s->buf_size = ( ( limit == 0 ) ? inc : limit+1 );
    s->buf = malloc( s->buf_size );
    if ( s->buf == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    s->str_len = 0;
    s->buf[0] = 0;
    s->truncated = false;
    return 0;
}

rc_t vds_free( p_dump_str s )
{
    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );
    }
    free( s->buf );
    return 0;
}

rc_t vds_clear( p_dump_str s )
{
    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );
    }
    s->buf[0]=0;
    s->str_len = 0;
    s->truncated = false;
    return 0;
}

bool vds_truncated( p_dump_str s )
{
    if ( s == NULL ) return false;
    return s->truncated;
}

char *vds_ptr( p_dump_str s )
{
    if ( s == NULL ) return NULL;
    return s->buf;
}


static rc_t vds_inc_buffer( p_dump_str s, const size_t by_len )
{
    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcAllocating, rcParam, rcNull );
    }
    if ( ( s->str_len + by_len ) >= s->buf_size )
    {
        size_t new_len = by_len + s->str_len + 1;
        while( s->buf_size < new_len ) s->buf_size += s->buf_inc;
        s->buf = realloc( s->buf, s->buf_size );
        if ( s->buf == NULL )
        {
            return RC( rcVDB, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
        }
    }
    return 0;
}


static rc_t vds_truncate( p_dump_str s )
{
    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcResizing, rcParam, rcNull );
    }
    if ( s->buf == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcResizing, rcParam, rcNull );
    }
    s->str_len = string_size( s->buf );
    if ( ( s->str_limit > 0 )&&( s->str_len > s->str_limit ) )
    {
        s->buf[ s->str_limit ] = 0;
        s->str_len = s->str_limit;
        s->truncated = true;
    }
    return 0;
}


rc_t vds_append_fmt( p_dump_str s, const size_t aprox_len, const char *fmt, ... )
{
    va_list argp;
    rc_t rc;
    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    if ( fmt == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    if ( fmt[0] == 0 )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcEmpty );
    }
    if ( ( s->str_limit > 0 )&&( s->str_len >= s->str_limit ) )
    {
        s->truncated = true;
        return 0;
    }
    rc = vds_inc_buffer( s, aprox_len );
    if ( rc == 0 )
    {
        va_start( argp, fmt );
        string_vprintf( s->buf + s->str_len, s->buf_size-1, NULL, fmt, argp );
        va_end( argp );
        rc = vds_truncate( s ); /* adjusts str_len */
    }
    return rc;
}


rc_t vds_append_str( p_dump_str s, const char *s1 )
{
    rc_t rc = 0;

    if ( ( s == NULL )||( s1 == NULL ) )
    {
        rc = RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    else
    {
        size_t append_len = string_size( s1 );
        if ( append_len > 0 )
        {
            if ( ( s->str_limit > 0 )&&( s->str_len >= s->str_limit ) )
            {
                s->truncated = true;
            }
            else
            {
                rc = vds_inc_buffer( s, append_len );
                if ( rc == 0 )
                {
                    size_t l = string_size( s->buf );
                    string_copy( s->buf + l, s->buf_size - l, s1, append_len );
                    rc = vds_truncate( s ); /* adjusts str_len */
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
    if ( ( s == NULL )||( s1 == NULL ) )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    append_len = string_size( s1 );
    if ( append_len == 0 )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcEmpty );
    }
    rc = vds_inc_buffer( s, append_len );
    if ( rc == 0 )
    {
        size_t l = string_size( s->buf );
        string_copy( s->buf + l, s->buf_size - l, s1, append_len );
        s->str_len += append_len;
    }
    return rc;
}


rc_t vds_rinsert( p_dump_str s, const char *s1 )
{
    size_t len;
    if ( ( s == NULL )||( s1 == NULL ) )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    len = string_size( s1 );
    if ( len == 0 )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcEmpty );
    }
    if ( s->str_len <= len ) return 0;
    len = s->str_len - len;
    string_copy( s->buf + len, sizeof( s->buf ) - len, s1, string_size( s1 ) );
    /* s->str_len does not change */
    return 0;
}


rc_t vds_indent( p_dump_str s, const size_t limit, const size_t indent )
{
    size_t remaining_chars, lines, indent_block, line_idx;
    char *src, *dst;
    rc_t rc;

    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    if ( s->str_len < limit ) return 0;
    remaining_chars = s->str_len - limit;
    indent_block = limit - indent;
    lines = ( remaining_chars / indent_block ) + 1;
    /* increase the buffer by ( lines * ( indent + 1 ) ) bytes */
    rc = vds_inc_buffer( s, lines * ( indent + 1 ) );
    if ( rc == 0 )
    {
        src = s->buf + limit;
        line_idx = 0;
        while ( line_idx < lines )
        {
            dst = src + ( indent + 1 );
            memmove( dst, src, remaining_chars );
            *src = '\n';
            memset( src+1, ' ', indent );
            remaining_chars -= indent_block;
            src += ( limit+1 );
            line_idx++;
        }
        s->str_len += ( lines * ( indent + 1 ) );
        s->buf[ s->str_len ] = 0;
    }
    return rc;
}

uint16_t vds_count_char( p_dump_str s, const char c )
{
    uint16_t res = 0;
    char *temp = s->buf;
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
    char *temp = s->buf;
    while(  *temp )
    {
        if ( *temp == to_escape )
        {
        size_t to_move = string_size( temp ) + 1;
        memmove( temp+1, temp, to_move );
        (*temp) = escape_char;
        temp++;
        }
        temp++;
    }
    s->str_len = string_size( s->buf );
}

rc_t vds_escape( p_dump_str s, const char to_escape, const char escape_char )
{
    uint16_t n;
    rc_t rc;

    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    n = vds_count_char( s, to_escape );
    rc = vds_inc_buffer( s, n + 1 );
    if ( rc == 0 )
    {
        vds_escape_quotes_loop( s, to_escape, escape_char );
    }
    return rc;
}

rc_t vds_enclose_string( p_dump_str s, const char c_left, const char c_right )
{
    size_t to_move;
    rc_t rc;

    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    to_move = string_size( s->buf ) + 1;
    rc = vds_inc_buffer( s, 2 );
    if ( rc == 0 )
    {
        memmove( s->buf + 1, s->buf, to_move );
        s->buf[0]=c_left;
        s->buf[to_move]=c_right;
        s->buf[to_move+1]=0;
        s->str_len = string_size( s->buf );
    }
    return rc;
}

rc_t vds_2_csv( p_dump_str s )
{
    rc_t rc = 0;
    bool has_comma, has_quotes;

    if ( s == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    }
    has_comma = (strchr( s->buf, ',' ) != NULL );
    has_quotes = (strchr( s->buf, '"' ) != NULL );
    if ( has_quotes )
    {
        rc = vds_escape( s, '"', '"' );
    }
    if ( (rc == 0 )&&( ( has_quotes )||( has_comma ) ) )
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
    if ( path == NULL || sections == NULL )
        rc = RC( rcVDB, rcNoTarg, rcInserting, rcParam, rcNull );
    else
    {
        rc = VNamelistMake ( sections, 5 );
        if ( rc == 0 )
        {
            uint32_t idx = 0, start = 0, len = 0;
            while( rc== 0 && path[ idx ] != 0 )
            {
                if ( path[ idx ] == delim )
                {
                    if ( len > 0 )
                        rc = vds_add_to_sections( path, start, len, *sections );
                    start = idx + 1;
                    len = 0;
                }
                else
                {
                    len++;
                }
                idx++;
            }
            if ( rc == 0 && len > 0 )
                rc = vds_add_to_sections( path, start, len, *sections );
        }
    }
    return rc;
}

/* ========================================================================================= */

static void clear_recorded_errors( void )
{
	rc_t rc;
	const char * filename;
	const char * funcname;
	uint32_t line_nr;
	while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &line_nr ) )
	{
	}
}

static rc_t vds_read_line( const KFile * f, uint64_t * pos, size_t * len, char * buffer, size_t buflen )
{
	size_t num_read;
	rc_t rc = KFileRead( f, *pos, buffer, buflen, &num_read );
	if ( rc == 0 && num_read > 0 )
	{
		char * nl = string_chr ( buffer, num_read, '\n' );
		char * cr = string_chr ( buffer, num_read, '\r' );
		if ( nl != NULL )
		{
			if ( cr != NULL )
			{
				if ( nl < cr )
					*len = ( nl - buffer );
				else
					*len = ( cr - buffer );	
			}
			else
				*len = ( nl - buffer );
		}
		else if ( cr != NULL )
		{
			*len = ( cr - buffer );
		}
		else 
			*len = 0;
		*pos += ( *len + 1 );
	}
	return rc;
}

static rc_t vds_print_diff( const char * buffer1, const char * buffer2, size_t len1, size_t len2 )
{
	rc_t rc;
	String S1, S2;
	
	StringInit( &S1, buffer1, len1, len1 );
	StringInit( &S2, buffer2, len2, len2 );
	rc = KOutMsg( "\n[A] %S\n", &S1 );
	if ( rc == 0 )
		rc = KOutMsg( "[B] %S\n", &S2 );
	return rc;
}

static rc_t vds_diff_files( const KFile * f1, const KFile * f2 )
{
	rc_t rc;
	char buffer1[ 2048 ];
	char buffer2[ 2048 ];
	uint64_t pos1 = 0;
	uint64_t pos2 = 0;
	size_t len1, len2;

	do
	{
		len1 = len2 = 0;
		rc = vds_read_line( f1, &pos1, &len1, buffer1, sizeof buffer1 );
		if ( rc == 0 )
			rc = vds_read_line( f2, &pos2, &len2, buffer2, sizeof buffer2 );
		if ( rc == 0 && ( len1 > 0 || len2 > 0 ) )
			rc = vds_print_diff( buffer1, buffer2, len1, len2 );
		
	} while( rc == 0 && ( len1 > 0 || len2 > 0 ) );
	return rc;
}


rc_t vds_diff( const char * f1, const char * f2 )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
		KOutMsg( "KDirectoryNativeDir() failed\n" );
    else
    {
		const KFile * kf1;
		rc = KDirectoryOpenFileRead ( dir, &kf1, "%s", f1 );
		if ( rc != 0 )
			KOutMsg( "cannot open file '%s'\n", f1 );
		else
		{
			const KFile * kf2;
			rc = KDirectoryOpenFileRead ( dir, &kf2, "%s", f2 );
			if ( rc != 0 )
				KOutMsg( "cannot open file '%s'\n", f2 );
			else
			{
				rc = vds_diff_files( kf1, kf2 );
				KFileRelease( kf2 );
			}
			KFileRelease( kf1 );
		}
		KDirectoryRelease( dir );
	}
	clear_recorded_errors();
	
	return rc;
}