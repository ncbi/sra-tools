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

#include <klib/printf.h>
#include <klib/out.h>

#include <os-native.h>
#include <sysalloc.h>
#include <ctype.h>
#include <string.h>

#include "md_flag.h"

struct cigar_t
{
	char * op;
	int * count;
	size_t size, length;
};


static void init_cigar_t( struct cigar_t * c, size_t size )
{
	if ( c != NULL )
	{
		c->size = 0;
		c->length = 0;
		c->op = malloc( sizeof( c->op[ 0 ] ) * size );
		if ( c->op != NULL )
		{
			c->count = malloc( sizeof( c->count[ 0 ] ) * size );
			if ( c->count != NULL )
				c->size = size;
			else
				free( ( void * ) c->op );
		}
	}
}

static void resize_cigar_t( struct cigar_t * c, size_t new_size )
{
	if ( c != NULL )
	{
		if ( c->size == 0 )
			init_cigar_t( c, new_size );
		else if ( c->size < new_size )
		{
			char * temp_op = c->op;
			c->op = realloc( c->op, sizeof( c->op[ 0 ] ) * new_size );
			if ( c->op != NULL )
			{
				int * temp_count = c->count;
				c->count = realloc( c->count, sizeof( c->count[ 0 ] ) * new_size );
				if ( c->count != NULL )
					c->size = new_size;
				else
					c->count = temp_count;
			}
			else
				c->op = temp_op;
		}
	}
}

static void append_to_cigar_t( struct cigar_t * c, char op, int count )
{
	if ( c->length < c->size )
	{
		c->op[ c->length ] = op;
		c->count[ c->length ++ ] = count;
	}
}

void parse_cigar_t( struct cigar_t * c, const char * cigar_str )
{
	if ( c != NULL && cigar_str != NULL && cigar_str[ 0 ] != 0 )
	{
		resize_cigar_t( c, strlen( cigar_str ) );
		if ( c->size > 0 )
		{
			int count = 0;
			while ( *cigar_str != 0 && c->length < c->size )
			{
				if ( isdigit( *cigar_str ) )
				{
					count = ( count * 10 ) + ( *cigar_str - '0' );
				}
				else
				{
					if ( count == 0 ) count = 1;
					append_to_cigar_t( c, *cigar_str, count );
					count = 0;
				}
				cigar_str++;
			}
		}
	}
}

struct cigar_t * make_cigar_t( const char * cigar_str, const size_t cigar_len )
{
	struct cigar_t * res = malloc( sizeof * res );
	if ( res != NULL )
	{
		size_t size;
		if ( cigar_str != NULL && cigar_str[ 0 ] != 0 && cigar_len > 0 )
			size = cigar_len;
		else
			size = 1024;
		init_cigar_t( res, size );
		if ( res->size == size )
			parse_cigar_t( res, cigar_str );
	}
	return res;
}


void free_cigar_t( struct cigar_t * c )
{
	if ( c != NULL )
	{
		if ( c->op != NULL )
		{
			free( ( void * ) c->op );
			c->op = NULL;
		}
		if ( c->count != NULL )
		{
			free( ( void * ) c->count );
			c->count = NULL;
		}
		free( ( void * ) c );
	}
}


static rc_t kout_delete( int count, int *match_count,
						 const uint8_t * ref, const INSDC_coord_len ref_len, int *ref_idx )
{
	rc_t rc = 0;
	
	if ( *match_count > 0 )
	{
		rc = KOutMsg( "%d", *match_count );
		*match_count = 0;
	}
	
	if ( rc == 0 )
	{
		if ( ( *ref_idx + count ) < ref_len )
		{
			rc = KOutMsg( "^%.*s", count, &(ref[ *ref_idx ] ) );
			(*ref_idx) += count;
		}
		else
			rc = RC( rcExe, rcNoTarg, rcAllocating, rcItem, rcIncomplete );
	}
	return rc;
}


static rc_t kout_match( int count, int *match_count,
						const char * read, size_t read_len, int *read_idx,
						const uint8_t *ref, const INSDC_coord_len ref_len, int *ref_idx )
{
	rc_t rc = 0;
	int i;
	for ( i = 0; i < count && rc == 0; ++i )
	{
		if ( *read_idx < read_len && *ref_idx < ref_len )
		{
			if ( read[ (*read_idx)++ ] == ref[ *ref_idx ] )
			{
				(*match_count)++;
			}
			else
			{
				rc = KOutMsg( "%d%c", *match_count, ref[ *ref_idx ] );
				*match_count = 0;
			}
			(*ref_idx)++;
		}
		else
			rc = RC( rcExe, rcNoTarg, rcAllocating, rcItem, rcIncomplete );
	}
	return rc;
}


static rc_t kout_tag( const struct cigar_t * c,
					  const char * read,
					  const size_t read_len,
					  const uint8_t * ref,
					  const INSDC_coord_len ref_len )
{
	rc_t rc = 0;
	if ( c != NULL && read != NULL && read_len > 0 && ref != NULL && ref_len > 0 )
	{
		rc = KOutMsg( "\tMD:Z:" );
		if ( rc == 0 )
		{
			int read_idx = 0;
			int ref_idx = 0;
			int match_count = 0;
			int cigar_idx;
			for ( cigar_idx = 0; cigar_idx < c->length && rc == 0; ++cigar_idx )
			{
				int count = c->count[ cigar_idx ];
				switch ( c->op[ cigar_idx ] )
				{
					case 'D' : rc = kout_delete( count, &match_count, ref, ref_len, &ref_idx ); break;
					
					case 'I' : read_idx += count; break;

					case 'M' : rc = kout_match( count, &match_count, read, read_len, &read_idx, ref, ref_len, &ref_idx ); break;
				}
			}
			if ( rc == 0 && match_count > 0 )
				rc = KOutMsg( "%d", match_count );
		}
	}
	else
		rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcIncomplete );
	return rc;
}


rc_t kout_md_tag_from_cigar_string( const char * cigar_str,
									const size_t cigar_len,
									const char * read,
									const size_t read_len,
									const uint8_t * ref,
									const INSDC_coord_len ref_len )
{
	rc_t rc = 0;
	struct cigar_t * cigar = make_cigar_t( cigar_str, cigar_len );
	if ( cigar == NULL )
		rc = RC( rcExe, rcNoTarg, rcAllocating, rcItem, rcIncomplete );
	else
	{
		rc = kout_tag( cigar, read, read_len, ref, ref_len );
		free_cigar_t( cigar );
	}
	return rc;
}
