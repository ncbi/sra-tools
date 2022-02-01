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

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

typedef struct dyn_string {
    char * data;
    size_t allocated;
    size_t data_len;
} dyn_string;

rc_t ds_allocate( struct dyn_string **self, size_t size ) {
    rc_t rc = 0;
    struct dyn_string * res = malloc( sizeof *res );
    *self = NULL;
    if ( res == NULL ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    } else {
        res -> data_len = 0;
        res -> data = malloc( size );
        if ( res -> data != NULL ) {
            res -> allocated = size;
        } else {
            res -> allocated = 0;
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        if ( rc != 0 ) {
            free( res );
        }
        else {
            *self = res;
        }
    }
    return rc;
}

void ds_free( struct dyn_string *self ) {
    if ( NULL != self ) {
        if ( NULL != self -> data ) {
            free( self -> data );
        }
        self -> data = NULL;
        self -> allocated = 0;
        self -> data_len = 0;
        free( ( void * ) self );
    }
}

void ds_reset( struct dyn_string *self ) {
    if ( NULL != self ) {
        self -> data_len = 0;
    }
}

rc_t ds_expand( struct dyn_string *self, size_t new_size ) {
    rc_t rc = 0;
    if ( NULL != self ) {
        if ( new_size > self -> allocated ) {
            self -> data = realloc ( self -> data, new_size );
            if ( self -> data != NULL ) {
                self -> allocated = new_size;
            } else {
                self -> allocated = 0;
                self -> data_len = 0;
                rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            }
        }
    } else {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    return rc;
}

rc_t ds_add_char( struct dyn_string *self, const char c ) {
    /* does nothing if self->data_len + 2 < self->allocated */
    rc_t rc = ds_expand( self, self -> data_len + 2 );
    if ( rc == 0 ) {
        self -> data[ self -> data_len++ ] = c;
        self -> data[ self -> data_len ] = 0; /* terminate */
    }
    return rc;
}

rc_t ds_repeat_char( struct dyn_string *self, const char c, uint32_t n ) {
    /* does nothing if self->data_len + 2 < self->allocated */
    rc_t rc = ds_expand( self, self -> data_len + n + 1 );
    if ( rc == 0 ) {
        uint32_t i;
        for ( i = 0; i < n; ++i ) {
            self -> data[ self -> data_len++ ] = c;
        }
        self -> data[ self -> data_len ] = 0; /* terminate */
    }
    return rc;
}

char * ds_get_char( struct dyn_string *self, uint32_t idx ) {
    char * res = NULL;
    if ( NULL != self ) {
        res = &( self -> data[ idx ] );
    }
    return res;
}

rc_t ds_add_str( struct dyn_string *self, const char * s ) {
    rc_t rc;
    if ( NULL != self ) {
        size_t size = string_size ( s );
        /* does nothing if self->data_len + size + 1 < self->allocated */
        rc = ds_expand( self, self -> data_len + size + 1 );
        if ( rc == 0 ) {
            string_copy( &( self -> data[ self -> data_len ] ), self -> allocated, s, size );
            self -> data_len += size;
            self -> data[ self -> data_len ] = 0;
        }
    } else {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    return rc;
}

rc_t ds_add_ds( struct dyn_string *self, struct dyn_string *other ) {
    rc_t rc;
    if ( NULL != self ) {
        if ( NULL != other ) {
            size_t size = other -> data_len;
            if ( size > 0 )	{
                /* does nothing if self->data_len + size + 1 < self->allocated */
                rc = ds_expand( self, self -> data_len + size + 1 );
                if ( rc == 0 ) {
                    string_copy( &( self -> data[ self -> data_len ] ), self -> allocated, other -> data, size );
                    self -> data_len += size;
                    self -> data[ self -> data_len ] = 0;
                }
            } else {
                rc = 0;
            }
        } else {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
        }
    } else {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    return rc;
}

rc_t ds_add_fmt( struct dyn_string * self, const char *fmt, ... ) {
    rc_t rc;
    if ( NULL != self ) {
        if ( NULL != fmt ) {
            bool not_enough;
            do {
                size_t num_writ;
                va_list args;
                va_start ( args, fmt );
                rc = string_vprintf ( &( self -> data[ self -> data_len ] ), 
                                    self -> allocated - ( self -> data_len + 1 ),
                                    &num_writ,
                                    fmt,
                                    args );
                va_end ( args );

                if ( rc == 0 ) {
                    self -> data_len += num_writ;
                    self -> data[ self -> data_len ] = 0;
                }
                not_enough = ( GetRCState( rc ) == rcInsufficient );
                if ( not_enough ) {
                    rc = ds_expand( self, self -> allocated + ( num_writ * 2 ) );
                }
            } while ( not_enough && rc == 0 );
        } else {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
        }
    } else {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    return rc;
}

rc_t ds_print( struct dyn_string * self ) {
    if ( self != NULL ) {
        return KOutMsg( "%.*s", self -> data_len, self -> data );
    } else {
        return 0;
    }
}

size_t ds_len( struct dyn_string * self ) {
    if ( self != NULL ) {
        return self -> data_len;
    } else {
        return 0;
    }
}

rc_t ds_print_char_n( struct dyn_string *self, const char c, uint32_t n ) {
    rc_t rc = ds_expand( self, n );
    if ( 0 == rc ) {
        if ( self -> data_len < n ) {
            rc = ds_repeat_char( self, c, n - self -> data_len );               
        }
        rc = KOutMsg( "%.*s", n, self -> data );
    }
    return rc;
}
