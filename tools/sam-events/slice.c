/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnologmsgy Information
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
#include "slice.h"
#include "common.h"
#include <klib/out.h>

void release_slice( slice * slice )
{
    if ( slice != NULL )
    {
        if ( slice->refname != NULL )
            StringWhack( slice->refname );
        free( ( void * ) slice );
    }
}


slice * make_slice( uint64_t start, uint64_t count, const String * refname )
{
    slice * res = calloc( 1, sizeof *res );
    if ( res != NULL )
    {
        rc_t rc = StringCopy( &res->refname, refname );
        if ( rc == 0 )
        {
            res->start = start;
            res->count = count;
            res->end = start + count;
        }
        else
        {
            release_slice( res );
            res = NULL;
        }
    }
    return res;
}


void print_slice( slice * slice )
{
    if ( slice != NULL )
        KOutMsg( "slice: %S [ %ld.%ld ]\n", slice->refname, slice->start, slice->count );
}



/* S has this format: 'refname[:start-end]' or refname[:start.count] */
slice * make_slice_from_str( const String * S )
{
    slice * res = NULL;
    if ( S != NULL )
    {
        String refname;
        uint32_t i = 0;
        
        StringInit( &refname, S->addr, 0, 0 );
        while ( i < S->len && S->addr[ i ] != ':' ) { i++; }
        refname.len = refname.size = i;
        if ( i == S->len )
            res = make_slice( 0, 0, &refname );
        else
        {
            rc_t rc;
            String S_Start;
            uint32_t j = 0;
            uint64_t start;
            
            i++;
            StringInit( &S_Start, &( S->addr[ i ] ), 0, 0 );
            while ( i < S->len && S->addr[ i ] != '-' && S->addr[ i ] != '.' ) { i++; j++; }
            if ( j > 0 )
            {
                S_Start.len = S_Start.size = j;
                start = StringToU64( &S_Start, &rc );
                if ( rc == 0 )
                {
                    if ( i == S->len )
                        res = make_slice( start, 0, &refname );
                    else
                    {
                        String S_End_or_Count;
                        char dot_or_dash = S->addr[ i ];
                        j = 0;
                        i++;
                        StringInit( &S_End_or_Count, &( S->addr[ i ] ), 0, 0 );
                        while ( i < S->len ) { i++; j++; }
                        S_End_or_Count.len = S_End_or_Count.size = j;
                        if ( j > 0 )
                        {
                            uint64_t end_or_count = StringToU64( &S_End_or_Count, &rc );
                            if ( rc == 0 )
                            {
                                if ( dot_or_dash == '.' )
                                    res = make_slice( start, end_or_count, &refname );
                                else
                                    res = make_slice( start, end_or_count - start, &refname );
                            }
                        }
                        else
                            res = make_slice( start, 0, &refname );
                    }
                }
            }
            else
                res = make_slice( 0, 0, &refname );
        }
    }
    return res;
}


bool filter_by_slice( const slice * slice, const String * refname, uint64_t start, uint64_t end )
{
    bool res = ( StringCompare( slice->refname, refname ) == 0 );
    if ( res )
        res = ( ( end >= slice->start ) && ( start <= slice->end ) );
    return res;
}


bool filter_by_slices( const Vector * slices, const String * refname, uint64_t pos, uint32_t len )
{
    bool res = false;
	uint32_t idx;
	uint64_t end = pos + len;
	uint32_t v_start = VectorStart( slices );
	uint32_t v_end = v_start + VectorLength( slices );
	for ( idx = v_start; !res && idx < v_end; ++idx )
	{
		const slice * slice = VectorGet( slices, idx );
		if ( slice != NULL )
			res = filter_by_slice( slice, refname, pos, end );
	}
    return res;
}


rc_t get_slice( const Args * args, const char *option, slice ** slice )
{
	const char * value;
	rc_t rc = get_charptr( args, option, &value );
	if ( rc == 0 && value != NULL )
	{
		String S;
		StringInitCString( &S, value );
		*slice = make_slice_from_str( &S );
	}
	return rc;
}


rc_t get_slices( const Args * args, const char *option, Vector * slices )
{
    uint32_t count, i;
    rc_t rc = ArgsOptionCount( args, option, &count );
    for ( i = 0; i < count && rc == 0; ++i )
    {
        const char * value;
        rc = ArgsOptionValue( args, option, i, ( const void ** )&value );
        if ( rc == 0 && value != NULL )
        {
            String S;
            StringInitCString( &S, value );
            {
                slice * s = make_slice_from_str( &S );
                rc = VectorAppend( slices, NULL, s );
                if ( rc != 0 )
                    release_slice( s );
            }
        }
    }
    return rc;
}
