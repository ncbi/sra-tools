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

#include "coldefs.h"
#include <klib/text.h>
#include <klib/out.h>

#include <vdb/table.h>

#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/*
 * helper-function for: col_defs_destroy
 * - free's everything a node owns
 * - free's the node
*/
static void CC col_defs_destroy_pair( void * node, void * data )
{
    col_pair * pair = node;
    if ( pair != NULL )
    {
	
        if ( pair -> name != NULL )
		{
			free( pair -> name );
			pair -> name = NULL;
		}
	
        free( pair );
    }
}


/*
 * initializes a column-definitions-list
*/
rc_t col_defs_init( col_defs ** defs )
{
	rc_t rc = 0;
	
    if ( defs == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
	else
	{
		col_defs * tmp = calloc( 1, sizeof( * tmp ) );
		if ( tmp == NULL )
			rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
		else
		{
			VectorInit( &( tmp ->cols ), 0, 5 );
			*defs = tmp;
		}
	}
    return rc;
}


/*
 * destroys the column-definitions-list
*/
rc_t col_defs_destroy( col_defs * defs )
{
	rc_t rc = 0;
    if ( defs == NULL )
        rc = RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
	else
	{
		VectorWhack( &( defs -> cols ), col_defs_destroy_pair, NULL );
		free( defs );
	}
    return rc;
}


/*
 * helper-function for: col_defs_parse_string / col_defs_extract_from_table
 * - creates a column-definition by the column-name
 * - adds the definition to the column-definition-vector
*/
static rc_t col_defs_append_col_pair( col_defs * defs, const char * name )
{
    rc_t rc = 0;
	
	if ( name == NULL )
		rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
	else if ( name[ 0 ] == 0 )
		rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcEmpty );
	else
	{
		col_pair * new_pair = calloc( 1, sizeof( * new_pair ) );
		if ( new_pair == NULL )
			rc = RC( rcVDB, rcNoTarg, rcParsing, rcMemory, rcExhausted );
		else
		{
			new_pair -> name = string_dup_measure ( name, NULL );
			rc = VectorAppend( &( defs -> cols ), NULL, new_pair );
			if ( rc != 0 )
				col_defs_destroy_pair( new_pair, NULL );
		}
	}
    return rc;
}


/*
*/
rc_t col_defs_fill( col_defs * defs, const KNamelist * list )
{
    rc_t rc = 0;

    if ( defs == NULL )
        rc = RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    else if ( list == NULL )
        rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
	else
	{
		uint32_t count;
		rc = KNamelistCount( list, &count );
		if ( rc != 0 )
			KOutMsg( "col_defs_extract_from_table:KNamelistCount() failed %R\n", rc );
		else
		{
			uint32_t idx;
			for ( idx = 0; idx < count && rc == 0; ++idx )
			{
				const char * name;
				rc = KNamelistGet( list, idx, &name );
				if ( rc != 0 )
					KOutMsg( "col_defs_extract_from_table:KNamelistGet() failed %R\n", rc );
				else
					rc = col_defs_append_col_pair( defs, name );
			}
		}
	}
    return rc;
}


/*
 * how many columns do we have in here?
*/
rc_t col_defs_count( const col_defs * defs, uint32_t * count )
{
	rc_t rc = 0;
    if ( defs == NULL )
        rc = RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    else if ( count == NULL )
        rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
	else
	{
		*count = VectorLength( &( defs -> cols ) );
	}
	return rc;
}


/*
 * add the pairs in defs to the given cursor
*/
rc_t col_defs_add_to_cursor( col_defs * defs, const VCursor * cur, int idx )
{
    rc_t rc = 0;

    if ( defs == NULL )
        rc = RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    else if ( cur == NULL )
        rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
    else if ( idx < 0 || idx > 1 )
        rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
	else
	{
		uint32_t i;
		uint32_t count = VectorLength( &( defs -> cols ) );
		for ( i = 0; i < count && rc == 0; ++i )
		{
			col_pair * pair = VectorGet( &( defs -> cols ), i );
			if ( pair != NULL )
				rc = VCursorAddColumn( cur, &( pair -> pair[ idx ].idx ), "%s", pair -> name );
		}
	}
	return rc;
}
