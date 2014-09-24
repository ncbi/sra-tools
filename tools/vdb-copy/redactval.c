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

#include "helper.h"
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


void redact_buf_init( redact_buffer * rbuf )
{
    rbuf->buffer = NULL;
    rbuf->buffer_len = 0;
}


void redact_buf_free( redact_buffer * rbuf )
{
    if ( rbuf->buffer != NULL )
    {
        
        free( rbuf->buffer );
        rbuf->buffer = NULL;
        rbuf->buffer_len = 0;
    }
}


rc_t redact_buf_resize( redact_buffer * rbuf, const size_t new_size )
{
    rc_t rc = 0;
    if ( rbuf->buffer_len < new_size || rbuf->buffer == NULL )
    {
        /* allocate or re-allocate the buffer */
        if ( rbuf->buffer_len == 0 )
            rbuf->buffer = malloc( new_size );
        else
            rbuf->buffer = realloc( rbuf->buffer, new_size );

        /* exit */
        if ( rbuf->buffer == NULL )
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        else
            rbuf->buffer_len = new_size;
    }
    return rc;
}


/* allocate a redact-value */
static p_redact_val redact_val_init( const char* name, 
                                     const uint32_t len,
                                     const char* value )
{
    p_redact_val res = NULL;
    if ( name == NULL ) return res;
    if ( name[0] == 0 ) return res;
    res = calloc( 1, sizeof( redact_val ) );
    if ( res == NULL ) return res;
    res->name = string_dup_measure ( name, NULL );
    res->len = len;
    res->value = NULL;
    if ( value != NULL )
    {
        if ( value[0] == '\'' && value[2] == '\'' )
        {
            res->value = malloc( sizeof value[0] );
            if ( res->value )
            {
                res->len = 1;
                *( ( char * )res->value ) = value[1];
            }
        }
        else
        {
            char *endptr;
            uint64_t x = strtou64( value, &endptr, 0 );
            if ( res->len > sizeof x )
                res->len = sizeof x;
            res->value = malloc( len );
            if ( res->value )
                memcpy( res->value, &x, res->len );
        }
    }
    return res;
}


void redact_val_fill_buffer( const p_redact_val r_val,
                             redact_buffer * rbuf,
                             const size_t buffsize )
{
    size_t idx;
    char * dst = rbuf->buffer;
    for ( idx = 0; idx < buffsize; idx += r_val->len )
    {
        size_t l = r_val->len;
        if ( ( idx + l ) > buffsize ) l = ( buffsize - idx );
        memcpy( dst, r_val->value, l );
        dst += l;
    }
}


static void CC redact_val_destroy_node( void* node, void* data )
{
    p_redact_val r_val = (p_redact_val)node;
    if ( r_val != NULL )
    {
        if ( r_val->name != NULL )
            free( r_val->name );
        if ( r_val->value != NULL )
            free( r_val->value );
        free( r_val );
    }
}


/*
 * initializes a redact-val-list
*/
rc_t redact_vals_init( redact_vals** vals )
{
    if ( vals == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    (*vals) = calloc( 1, sizeof( redact_vals ) );
    if ( *vals == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    VectorInit( &((*vals)->vals), 0, 5 );
    return 0;
}


/*
 * destroys the redact-val-list
*/
rc_t redact_vals_destroy( redact_vals* vals )
{
    if ( vals == NULL )
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
    VectorWhack( &(vals->vals), redact_val_destroy_node, NULL );
    free( vals );
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
    if ( new_val == NULL )
        rc = RC( rcVDB, rcNoTarg, rcParsing, rcMemory, rcExhausted );
    else
        rc = VectorAppend( &(vals->vals), NULL, new_val );
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
    if ( vals == NULL || name == NULL || name[0] == 0 )
        return res;

    len = VectorLength( &(vals->vals) );
    for ( idx = 0;  idx < len && res == NULL; ++idx )
    {
        p_redact_val item = (p_redact_val) VectorGet ( &(vals->vals), idx );
        if ( nlt_strcmp( item->name, name ) == 0 )
            res = item;
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
    if ( vals == NULL || cast == NULL || cast[0] == 0 )
        return res;

    name = string_dup_measure ( cast, NULL );
    for ( idx = 0; name[idx] != 0; ++idx )
        if ( name[idx] == ')' ) name[idx] = 0;
    res = redact_vals_get_by_name( vals, &(name[1]) );
    free( name );
    return res;
}
