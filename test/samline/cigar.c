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

#include <os-native.h>
#include <sysalloc.h>
#include <ctype.h>
#include <string.h>

#include "cigar.h"

static cigar_opt * make_cigar_opt( uint32_t count, char op )
{
	cigar_opt * res = malloc( sizeof * res );
	if ( res != NULL )
	{
		res -> op = op;
		res -> count = ( count == 0 ) ? 1 : count;
		res -> next = NULL;
	}
	return res;
}


cigar_opt * parse_cigar( const char * cigar )
{
	cigar_opt * res = NULL;
	if ( cigar != NULL && cigar[ 0 ] != 0 )
	{
		cigar_opt * tail = NULL;	
		uint32_t count = 0;
		while ( *cigar != 0 )
		{
			if ( isdigit( *cigar ) )
			{
				count = ( count * 10 ) + ( *cigar - '0' );
			}
			else
			{
				if ( tail == NULL )
				{
					res = tail = make_cigar_opt( count, *cigar );
				}
				else
				{
					tail -> next = make_cigar_opt( count, *cigar );
					tail = tail -> next;
				}
				count = 0;
			}
			cigar++;
		}
	}
	return res;
}

void free_cigar( cigar_opt * cigar )
{
	while( cigar != NULL )
	{
		cigar_opt * temp = cigar->next;
		free( cigar );
		cigar = temp;
	}
}

uint32_t calc_reflen( const cigar_opt * cigar )
{
	uint32_t res = 0;
	while( cigar != NULL )
	{
		switch( cigar->op )
		{
			case 'A'	: res += cigar->count; break;
			case 'C'	: res += cigar->count; break;
			case 'G'	: res += cigar->count; break;
			case 'T'	: res += cigar->count; break;

			case 'D'	: res += cigar->count; break;
			case 'M'	: res += cigar->count; break;			
		}
		cigar = cigar->next;
	}
	return res;
}

uint32_t calc_readlen( const cigar_opt * cigar )
{
	uint32_t res = 0;
	while( cigar != NULL )
	{
		if ( cigar->op != 'D' )
			res += cigar->count;
		cigar = cigar->next;
	}
	return res;
}

uint32_t calc_inslen( const cigar_opt * cigar )
{
	uint32_t res = 0;
	while( cigar != NULL )
	{
		if ( cigar->op == 'I' )
			res += cigar->count;
		cigar = cigar->next;
	}
	return res;
}


static uint32_t op_count( const cigar_opt * cigar )
{
	uint32_t res = 0;
	while( cigar != NULL )
	{
		res++;
		cigar = cigar->next;
	}
	return res;
}

char * to_string( const cigar_opt * cigar )
{
	uint32_t l = op_count( cigar ) * 5;
	char * res = malloc( l );
	if ( res != NULL )
	{
		uint32_t dst_idx = 0;
		while( cigar != NULL && dst_idx < l )
		{
			size_t num_writ;
			string_printf( &res[ dst_idx ], l - dst_idx, &num_writ,
						"%d%c", cigar->count, cigar->op );
			dst_idx += num_writ;
			cigar = cigar->next;
		}
		res[ dst_idx ] = 0;
	}
	return res;
}


static int can_merge( char op1, char op2 )
{
	char mop1 = op1;
	char mop2 = op2;
	if ( mop1 == 'A' || mop1 == 'C' || mop1 == 'G' || mop1 == 'T' )
		mop1 = 'M';
	if ( mop2 == 'A' || mop2 == 'C' || mop2 == 'G' || mop2 == 'T' )
		mop2 = 'M';
	return ( mop1 == mop2 );
}

cigar_opt * merge_match_and_mismatch( const cigar_opt * cigar )
{
	cigar_opt *	head = NULL;
	cigar_opt * tail = NULL;
	
	while( cigar != NULL )	
	{
		if ( head == NULL )
		{
			head = tail = make_cigar_opt( cigar->count, cigar->op );
		}
		else
		{
			if ( can_merge( cigar->op, tail->op ) )
			{
				tail->count += cigar->count;
				tail->op = 'M';
			}
			else
			{
				tail -> next = make_cigar_opt( cigar->count, cigar->op );
				tail = tail -> next;
			}
		}
		cigar = cigar->next;
	}
	return head;
}

char * produce_read( const cigar_opt * cigar, const char * ref_bases, const char * ins_bases )
{
	char * res = NULL;
	if ( cigar != NULL )
	{
		uint32_t readlen = calc_readlen( cigar );
		if ( readlen > 0 )
		{
			uint32_t needed_ref_bases = calc_reflen( cigar );
			uint32_t available_ref_bases = ref_bases != NULL ? strlen( ref_bases ) : 0;
			if ( available_ref_bases >= needed_ref_bases )
			{
				uint32_t needed_ins_bases = calc_inslen( cigar );
				uint32_t available_ins_bases = ins_bases != NULL ? strlen( ins_bases ) : 0;	
				if ( available_ins_bases >= needed_ins_bases )
				{
					res = malloc( readlen + 1 );
					if ( res != NULL )
					{
						uint32_t dst_idx = 0;
						uint32_t ref_idx = 0;
						uint32_t ins_idx = 0;
						uint32_t i;
						while( cigar != NULL && dst_idx < readlen )
						{
							switch ( cigar->op )
							{
								case 'A' : for ( i = 0; i < cigar->count; ++i )
												res[ dst_idx++ ] = 'A';
											ref_idx += cigar->count;
											break;

								case 'C' : for ( i = 0; i < cigar->count; ++i )
												res[ dst_idx++ ] = 'C';
											ref_idx += cigar->count;
											break;

								case 'G' : for ( i = 0; i < cigar->count; ++i )
												res[ dst_idx++ ] = 'G';
											ref_idx += cigar->count;												
											break;

								case 'T' : for ( i = 0; i < cigar->count; ++i )
												res[ dst_idx++ ] = 'T';
											ref_idx += cigar->count;												
											break;

								case 'D' : ref_idx += cigar->count;
											break;
								
								case 'I' : for ( i = 0; i < cigar->count; ++i )
												res[ dst_idx++ ] = ins_bases[ ins_idx++ ];
											break;

								case 'M' : for ( i = 0; i < cigar->count; ++i )
												res[ dst_idx++ ] = ref_bases[ ref_idx++ ];
											break;
							}
							cigar = cigar->next;
						}
					}
				}
			}
		}
	}
	return res;
}

