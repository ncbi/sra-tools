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

#ifndef _h_string_cache_
#define _h_string_cache_

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct string_cache;

struct string_cache * make_string_cache( int row_count, int col_count );
void release_string_cache( struct string_cache * sc );

int get_string_cache_rowcount( const struct string_cache * sc );
long long int get_string_cache_start( const struct string_cache * sc );

const char * get_cell_from_string_cache( const struct string_cache * sc, long long int row, int col );
void invalidate_string_cache( struct string_cache * sc );
int put_cell_to_string_cache( struct string_cache * sc, long long int row, int col, const char * s );

#ifdef __cplusplus
}
#endif

#endif /* _h_string_cache_ */
