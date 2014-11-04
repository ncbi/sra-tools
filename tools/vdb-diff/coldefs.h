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

#ifndef _h_vdb_coldefs_
#define _h_vdb_coldefs_

#include <klib/defs.h>
#include <klib/rc.h>
#include <klib/vector.h>
#include <klib/namelist.h>

#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
col-def is the definition of a single column: index/type
********************************************************************/
typedef struct col_def
{
    uint32_t idx;       	/* index of this column in curosr */
} col_def;


/********************************************************************
col-pair is the definition of pair of columns with same name
********************************************************************/
typedef struct col_pair
{
    char * name;
	col_def	pair[ 2 ];		/* a pair of sub-col-defs */
} col_pair;


/********************************************************************
the col-defs are a vector of single column-pairs
********************************************************************/
typedef struct col_defs
{
    Vector cols;
} col_defs;


/*
 * initializes a column-definitions-list
*/
rc_t col_defs_init( col_defs ** defs );


/*
 * destroys the column-definitions-list
*/
rc_t col_defs_destroy( col_defs * defs );


/*
 * setup the list with pairs made from the given list
*/
rc_t col_defs_fill( col_defs * defs, const KNamelist * list );


/*
 * how many columns do we have in here?
*/
rc_t col_defs_count( const col_defs * defs, uint32_t * count );


/*
 * add the pairs in defs to the given cursor
*/
rc_t col_defs_add_to_cursor( col_defs * defs, const VCursor * cur, int idx );


#ifdef __cplusplus
}
#endif

#endif
