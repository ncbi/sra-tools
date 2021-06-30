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

#include "redactval.h"

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_namelist_tools_
#include "namelist_tools.h"
#endif


void redact_buf_init( redact_buffer * rbuf )
{
    rbuf -> buffer = NULL;
    rbuf -> buffer_len = 0;
}


void redact_buf_free( redact_buffer * rbuf )
{
    if ( NULL != rbuf -> buffer )
    {
        
        free( ( void* )rbuf -> buffer );
        rbuf -> buffer = NULL;
        rbuf -> buffer_len = 0;
    }
}


rc_t redact_buf_resize( redact_buffer * rbuf, const size_t new_size )
{
    rc_t rc = 0;
    if ( ( rbuf -> buffer_len < new_size ) || ( NULL == rbuf -> buffer ) )
    {
        /* allocate or re-allocate the buffer */
        if ( 0 == rbuf -> buffer_len )
        {
            rbuf -> buffer = malloc( new_size );
        }
        else
        {
            rbuf -> buffer = realloc( rbuf -> buffer, new_size );
        }
        /* exit */
        if ( NULL == rbuf -> buffer )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            rbuf -> buffer_len = new_size;
        }
    }
    return rc;
}


/* allocate a redact-value */
static p_redact_val redact_val_init( const char* name, 
                                     const uint32_t len,
                                     const char* value )
{
    p_redact_val res = NULL;
    if ( NULL == name ) return res;
    if ( 0 == name[ 0 ] ) return res;
    res = calloc( 1, sizeof( redact_val ) );
    if ( NULL == res ) return res;
    res -> name = string_dup_measure ( name, NULL );
    res -> len = len;
    res -> value = NULL;
    if ( NULL != value )
    {
        if ( ( '\'' == value[ 0 ] ) && ( '\'' == value[ 2 ] ) )
        {
            res -> value = malloc( sizeof value[ 0 ] );
            if ( NULL != res -> value )
            {
                res -> len = 1;
                *( ( char * )( res -> value ) ) = value[ 1 ];
            }
        }
        else
        {
            char *endptr;
            uint64_t x = strtou64( value, &endptr, 0 );
            if ( res -> len > sizeof x )
            {
                res -> len = sizeof x;
            }
            res -> value = malloc( len );
            if ( NULL != res -> value )
            {
                memmove( res -> value, &x, res -> len );
            }
        }
    }
    return res;
}


void redact_val_fill_buffer( const p_redact_val r_val,
                             redact_buffer * rbuf,
                             const size_t buffsize )
{
    size_t idx;
    char * dst = rbuf -> buffer;
    for ( idx = 0; idx < buffsize; idx += r_val -> len )
    {
        size_t l = r_val -> len;
        if ( ( idx + l ) > buffsize )
        {
            l = ( buffsize - idx );
        }
        memmove( dst, r_val -> value, l );
        dst += l;
    }
}


static void CC redact_val_destroy_node( void* node, void* data )
{
    p_redact_val r_val = (p_redact_val)node;
    if ( NULL != r_val )
    {
        if ( NULL != r_val -> name )
        {
            free( ( void* )( r_val -> name ) );
        }
        if ( NULL != r_val -> value )
        {
            free( ( void* )( r_val -> value ) );
        }
        free( ( void* )r_val );
    }
}


/*
 * initializes a redact-val-list
*/
rc_t redact_vals_init( redact_vals** vals )
{
    if ( NULL == vals )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    (*vals) = calloc( 1, sizeof( redact_vals ) );
    if ( NULL == *vals )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    VectorInit( &( (*vals) -> vals ), 0, 5 );
    return 0;
}


/*
 * destroys the redact-val-list
*/
rc_t redact_vals_destroy( redact_vals* vals )
{
    if ( NULL == vals )
    {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
    }
    VectorWhack( &( vals -> vals ), redact_val_destroy_node, NULL );
    free( ( void* )vals );
    return 0;
}


/*
 * adds a entry into the redact-val-list
*/
rc_t redact_vals_add( redact_vals* vals, const char* name, 
                      const uint32_t len, const char* value )
{
    rc_t rc;
    p_redact_val new_val = redact_val_init( name, len, value );
    if ( NULL == new_val )
    {
        rc = RC( rcVDB, rcNoTarg, rcParsing, rcMemory, rcExhausted );
    }
    else
    {
        rc = VectorAppend( &( vals -> vals ), NULL, new_val );
    }
    return rc;
}


/*
 * returns a pointer to a redact-value by type-name
*/
p_redact_val redact_vals_get_by_name( const redact_vals* vals,
                                      const char * name )
{
    p_redact_val res = NULL;
    uint32_t idx, len;
    if ( ( NULL == vals ) || ( NULL == name ) || ( 0 == name[ 0 ] ) )
    {
        return res;
    }

    len = VectorLength( &( vals -> vals ) );
    for ( idx = 0;  idx < len && NULL == res; ++idx )
    {
        p_redact_val item = ( p_redact_val ) VectorGet ( &( vals -> vals ), idx );
        if ( 0 == nlt_strcmp( item -> name, name ) )
        {
            res = item;
        }
    }
    return res;
}

/*
 * returns a pointer to a redact-value by type-cast
*/
p_redact_val redact_vals_get_by_cast( const redact_vals* vals,
                                      const char * cast )
{
    uint32_t idx;
    p_redact_val res = NULL;
    char * name;
    if ( ( NULL == vals ) || ( NULL == cast ) || ( 0 == cast[ 0 ] ) )
    {
        return res;
    }
    name = string_dup_measure ( cast, NULL );
    for ( idx = 0; 0 != name[ idx ]; ++idx )
    {
        if ( ')' == name[ idx ] )
        {
            name[ idx ] = 0;
        }
    }
    res = redact_vals_get_by_name( vals, &( name[ 1 ] ) );
    free( ( void* )name );
    return res;
}
