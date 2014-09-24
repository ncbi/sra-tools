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

#ifndef _h_vdb_dump_filter_
#define _h_vdb_dump_filter_

#include <klib/vector.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum filter_t
{
    filter_unknown,
    filter_bool,
    filter_integer,
    filter_float,
    filter_text
} filter_t;

/********************************************************************
the dump_filter contains a filter expression for a column
********************************************************************/
typedef struct filter
{
    filter_t filter_type;
    char *expression;
} filter;
typedef filter* p_filter;

void filter_dosomething( p_filter flt );

bool filter_make( filter **flt, const char *expression );
void filter_destroy( p_filter flt );

/********************************************************************
the dump_filters is a vector of column_filters...
********************************************************************/
typedef struct filters
{
    Vector filters;
    uint16_t count;
    bool match;
} filters;
typedef filters* p_filters;

bool vdfi_make( filters **flts );
void vdfi_destroy( p_filters flts );
bool vdfi_new_filter( p_filters flts, const char *expression );
void vdfi_reset( p_filters flts );
bool vdfi_match( p_filters flts );
uint16_t vdfi_count( p_filters flts );

#ifdef __cplusplus
}
#endif

#endif
