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

#include "vdb-dump-filter.h"
#include <klib/text.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <os-native.h>
#include <sysalloc.h>

bool vdfi_make_filter( filter **flt, const char *expression )
{
    bool res = false;
    if ( flt == NULL ) return res;
    (*flt) = malloc( sizeof( flt ) );
    if ( *flt )
    {
        (*flt)->expression = string_dup_measure( expression, NULL );
        (*flt)->filter_type = filter_unknown;
        res = true;
    }
    return res;
}

void vdfi_destroy_filter( p_filter flt )
{
    if ( flt == NULL ) return;
    free( flt->expression );
    free( flt );
}

bool vdfi_make( filters **flts )
{
    bool res = false;
    if ( flts == NULL ) return res;
    (*flts) = malloc( sizeof( filters ) );
    if ( *flts )
    {
        VectorInit( &( (*flts)->filters ), 0, 5 );
        (*flts)->match = false;
        (*flts)->count = 0;
        res = true;
    }
    return res;
}

static void CC vdfi_destroy_filter_1( void* node, void* data )
{
    vdfi_destroy_filter( (p_filter)node );
}

void vdfi_destroy( p_filters flts )
{
    if ( flts == NULL ) return;
    VectorWhack( &(flts->filters), vdfi_destroy_filter_1, NULL );
    free( flts );
}

bool vdfi_new_filter( p_filters flts, const char *expression )
{
    bool res = false;
    filter *new_filter;
    if ( flts == NULL ) return res;
    if ( expression == NULL ) return res;
    if ( expression[0] == 0 ) return res;
    if ( vdfi_make_filter( &new_filter, expression ) )
    {
        res = ( VectorAppend( &(flts->filters), NULL, new_filter ) == 0 );
        if ( res ) flts->count++;
    }
    return res;
}

void vdfi_reset( p_filters flts )
{
    if ( flts == NULL ) return;
    flts->match = false;
}

bool vdfi_match( p_filters flts )
{
    if ( flts == NULL ) return false;
    return flts->match;
}

uint16_t vdfi_count( p_filters flts )
{
    if ( flts == NULL ) return 0;
    return flts->count;
}
