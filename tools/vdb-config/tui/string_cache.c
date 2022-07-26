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

#include "string_cache.h"
#include <klib/text.h>

#include <sysalloc.h>

typedef struct cache_row
{
    char ** cells;
    int * cell_idx;
    int cell_count;
    int nxt_free_ptr;
} cache_row;


static cache_row * make_cache_row( int count )
{
    cache_row * res = malloc( sizeof *res );
    if ( res != NULL )
    {
        res->cells = calloc( sizeof( char * ), count );
        if ( res->cells != NULL )
        {
            res->cell_idx = malloc( sizeof( int ) * count );
            if ( res->cell_idx != NULL )
            {
                int i;
                for ( i = 0; i < count; ++i ) res->cell_idx[ i ] = -1;
                res->cell_count = count;
                res->nxt_free_ptr = 0;
            }
            else
            {
                free( res->cells );
                free( res );
                res = NULL;
            }
        }
        else
        {
            free( res );
            res = NULL;
        }
    }
    return res;
}


static void release_cache_row( cache_row * cr )
{
    if ( cr != NULL )
    {
        if ( cr->cells != NULL )
        {
            int i;
            for ( i = 0; i < cr->cell_count; ++i )
            {
                char * s = cr->cells[ i ];
                if ( s != NULL ) free( s );
            }
            free( cr->cells );
        }
        if ( cr->cell_idx != NULL ) free( cr->cell_idx );
        free( cr );
    }
}


static const char * get_cell_from_cache_row( cache_row * cr, int idx )
{
    const char * res = NULL;
    if ( cr != NULL )
    {
        if ( idx < cr->cell_count )
        {
            int ptr_idx = cr->cell_idx[ idx ];
            if ( ptr_idx >= 0 && ptr_idx < cr->cell_count )
                res = cr->cells[ ptr_idx ];
        }
    }
    return res;
}


static int put_cell_to_cache_row( cache_row * cr, int idx, const char * s )
{
    int res = 0;
    if ( cr != NULL )
    {
        if ( idx < cr->cell_count )
        {
            int ptr_idx = cr->cell_idx[ idx ];
            if ( ptr_idx >= 0 )
            {
                /* cell is already used... */
                char * s_ = cr->cells[ ptr_idx ];
                if ( s_ != NULL ) free( s_ );
                cr->cells[ ptr_idx ] = ( s != NULL ) ? string_dup_measure ( s, NULL ) : NULL;
                res++;
            }
            else if ( cr->nxt_free_ptr < cr->cell_count )
            {
                /* cell is not already used, get the next free space */
                
                ptr_idx = cr->nxt_free_ptr++;
                cr->cells[ ptr_idx ] = ( s != NULL ) ? string_dup_measure ( s, NULL ) : NULL;
                cr->cell_idx[ idx ] = ptr_idx;
                res++;
            }
        }
    }
    return res;
}


static void clear_cache_row( cache_row * cr )
{
    if ( cr != NULL )
    {
        int i;
        for ( i = 0; i < cr->cell_count; ++i )
        {
            if ( cr->cells[ i ] != NULL ) free( cr->cells[ i ] );
            cr->cells[ i ] = NULL;
            cr->cell_idx[ i ] = -1;
        }
        cr->nxt_free_ptr = 0;
    }
}


/* ----------------------------------------------------------------------------------------------------- */


typedef struct string_cache
{
    cache_row ** rows;
    long long int row_offset;
    int row_count;
} string_cache;


static void release_string_cache_rows( cache_row ** rows, int count )
{
    int i;
    for ( i = 0; i < count; ++i )
    {
        if ( rows[ i ] != NULL )
            release_cache_row( rows[ i ] ); 
    }
    free( rows );
}


string_cache * make_string_cache( int row_count, int col_count )
{
    string_cache * res = malloc( sizeof *res );
    if ( res != NULL )
    {
        res->rows = calloc( sizeof( *res->rows ), row_count );
        if ( res->rows != NULL )
        {
            int i, ok;
            for ( i = 0, ok = 1; i < row_count && ok; ++i )
            {
                res->rows[ i ] = make_cache_row( col_count );
                if ( res->rows[ i ] == NULL )
                    ok = 0;
            }
            if ( ok )
            {
                res->row_count = row_count;
                res->row_offset = 0;
            }
            else
            {
                release_string_cache_rows( res->rows, row_count );
                free( res );
                res = NULL;
            }
        }
        else
        {
            free( res );
            res = NULL;
        }
    }
    return res;
}


void release_string_cache( string_cache * sc )
{
    if ( sc != NULL )
    {
        if ( sc->rows != NULL )
            release_string_cache_rows( sc->rows, sc->row_count );
        free( sc );
    }
}


int get_string_cache_rowcount( const string_cache * sc )
{
    if ( sc != NULL )
        return sc->row_count;
    else
        return 0;
}


long long int get_string_cache_start( const string_cache * sc )
{
    if ( sc != NULL )
        return sc->row_offset;
    else
        return 0;
}


const char * get_cell_from_string_cache( const string_cache * sc, long long int row, int col )
{
    const char * res = NULL;
    if ( sc != NULL )
    {
        long long int temp_idx = ( row - sc->row_offset );
        if ( temp_idx >= 0 && temp_idx < sc->row_count )
            res = get_cell_from_cache_row( sc->rows[ temp_idx ], col );
    }
    return res;
}


static void invalidate_string_cache_from_to( string_cache * sc, int from, int count )
{
    if ( sc != NULL )
        if ( sc->rows != NULL )
        {
            int i;
            for ( i = 0; i < count; ++i )
                clear_cache_row( sc->rows[ i + from ] );
        }
}


static void reverse_cache_rows( cache_row ** rows, int start, int count )
{
    int end = start + count - 1;
    while ( start < end )
    {
        cache_row * temp = rows[ start ];
        rows[ start ] = rows[ end ];
        rows[ end ] = temp;
        start++;
        end--;
    }
}


static void rotate_cache_rows_up( cache_row ** rows, int pivot, int count )
{
    reverse_cache_rows( rows, 0, count );
    reverse_cache_rows( rows, 0, pivot );
    reverse_cache_rows( rows, pivot, count - pivot );
}


static void rotate_cache_rows_down( cache_row ** rows, int pivot, int count )
{
    reverse_cache_rows( rows, 0, pivot );
    reverse_cache_rows( rows, pivot, count - pivot );
    reverse_cache_rows( rows, 0, count );
}


void invalidate_string_cache( string_cache * sc )
{
    invalidate_string_cache_from_to( sc, 0, sc->row_count );
}


int put_cell_to_string_cache( string_cache * sc, long long int row, int col, const char * s )
{
    int res = 0;
    if ( sc != NULL )
    {
        if ( row < sc->row_offset && row >= 0 )
        {
            /* ====================================================================================== */
            /* access is before the current window: decide if we can shift or need a full invalidation */

            long long int distance = ( sc->row_offset - row );
            /* printf( "-- before window ( distance = %lld ) --\n", distance ); */
            if ( distance >= sc->row_count )
            {
                /* ===> */
                /* jumping out of the cache content: full invalidation */
                /* printf( "-- full invalidation of cache --\n" ); */

                invalidate_string_cache_from_to( sc, 0, sc->row_count );
                sc->row_offset = row;
                res = put_cell_to_cache_row( sc->rows[ 0 ], col, s );
            }
            else
            {
                /* ===> */
                /* some content can still be used: rotate and partial invalidation */
                /* printf( "-- rotate by %lld rows up and partial invalidation of cache --\n", distance ); */

                rotate_cache_rows_up( sc->rows, (int)distance, sc->row_count );
                invalidate_string_cache_from_to( sc, 0, (int)distance );
                sc->row_offset = row;
                res = put_cell_to_cache_row( sc->rows[ 0 ], col, s );
            }
        }
        else if ( row >= ( sc->row_offset + sc->row_count ) )
        {
            /* ====================================================================================== */
            /* acess is after the current window: decide if we can shift or need a full invalidation */
            long long int distance = ( row - ( sc->row_offset + sc->row_count ) ) + 1;
            /* printf( "-- after window ( distance = %lld ) --\n", distance ); */
            if ( distance >= sc->row_count )
            {
                /* ===> */
                /* jumping out of the cache content: full invalidation */
                /* printf( "-- full invalidation of cache --\n" ); */

                invalidate_string_cache_from_to( sc, 0, sc->row_count );
                sc->row_offset = row;
                res = put_cell_to_cache_row( sc->rows[ 0 ], col, s );
            }
            else
            {
                /* ===> */
                /* some content can still be used: rotate and partial invalidation */
                /* printf( "-- rotate by %lld rows down and partial invalidation of cache --\n", distance ); */
                rotate_cache_rows_down( sc->rows, (int)distance, sc->row_count );
                invalidate_string_cache_from_to( sc, ( sc->row_count - (int)distance ), (int)distance );
                sc->row_offset += distance;
                res = put_cell_to_cache_row( sc->rows[ sc->row_count - 1 ], col, s );
            }
        }
        else if ( row >= 0 )
        {
            /* ====================================================================================== */
            /* access is in the current window: no need to shift or invalidate the rows */
            long long int rel_row = ( row - sc->row_offset );
            /* printf( "-- in window ( rel. row = %lld ) --\n", rel_row ); */
            res = put_cell_to_cache_row( sc->rows[ rel_row ], col, s );
        }
    }
    return res;
}
