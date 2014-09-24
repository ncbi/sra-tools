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

#include "dyn_string.h"
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>

typedef struct dyn_string
{
    char * data;
    size_t allocated;
    size_t data_len;
} dyn_string;


rc_t allocated_dyn_string ( struct dyn_string **self, size_t size )
{
    rc_t rc = 0;
    struct dyn_string * res = malloc( sizeof *res );
    *self = NULL;
    if ( res == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        res->data_len = 0;
        res->data = malloc( size );
        if ( res->data != NULL )
            res->allocated = size;
        else
        {
            res->allocated = 0;
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        if ( rc != 0 )
            free( res );
        else
            *self = res;
    }
    return rc;
}


void free_dyn_string ( struct dyn_string *self )
{
    free( self->data );
    self->data = NULL;
    self->allocated = 0;
    self->data_len = 0;
    free( ( void * ) self );
}


void reset_dyn_string( struct dyn_string *self )
{
    self->data_len = 0;
}


rc_t expand_dyn_string( struct dyn_string *self, size_t new_size )
{
    rc_t rc = 0;
    if ( new_size > self->allocated )
    {
        self->data = realloc ( self->data, new_size );
        if ( self->data != NULL )
        {
            self->allocated = new_size;
        }
        else
        {
            self->allocated = 0;
            self->data_len = 0;
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
    }
    return rc;
}


rc_t add_char_2_dyn_string( struct dyn_string *self, const char c )
{
    /* does nothing if self->data_len + 2 < self->allocated */
    rc_t rc = expand_dyn_string( self, self->data_len + 2 );
    if ( rc == 0 )
    {
        self->data[ self->data_len++ ] = c;
        self->data[ self->data_len ] = 0;
    }
    return rc;
}


char * dyn_string_char( struct dyn_string *self, uint32_t idx )
{
    return( &self->data[ idx ] );
}


rc_t add_string_2_dyn_string( struct dyn_string *self, const char * s )
{
    rc_t rc;
    size_t size = string_size ( s );
    /* does nothing if self->data_len + size + 1 < self->allocated */
    rc = expand_dyn_string( self, self->data_len + size + 1 );
    if ( rc == 0 )
    {
        string_copy ( &(self->data[ self->data_len ]), self->allocated, s, size );
        self->data_len += size;
        self->data[ self->data_len ] = 0;
    }
    return rc;
}


rc_t print_2_dyn_string( struct dyn_string * self, const char *fmt, ... )
{
    rc_t rc = 0;
    bool not_enough;

    do
    {
        size_t num_writ;
        va_list args;
        va_start ( args, fmt );
        rc = string_vprintf ( &(self->data[ self->data_len ]), 
                              self->allocated - ( self->data_len + 1 ),
                              &num_writ,
                              fmt,
                              args );
        va_end ( args );

        if ( rc == 0 )
        {
            self->data_len += num_writ;
            self->data[ self->data_len ] = 0;
        }
        not_enough = ( GetRCState( rc ) == rcInsufficient );
        if ( not_enough )
        {
            rc = expand_dyn_string( self, self->allocated + ( num_writ * 2 ) );
        }
    } while ( not_enough && rc == 0 );
    return rc;
}


rc_t print_dyn_string( struct dyn_string * self )
{
    if ( self != NULL )
        return KOutMsg( "%.*s", self->data_len, self->data );
    else
        return 0;
}


size_t dyn_string_len( struct dyn_string * self )
{
    if ( self != NULL )
        return self->data_len;
    else
        return 0;
}
