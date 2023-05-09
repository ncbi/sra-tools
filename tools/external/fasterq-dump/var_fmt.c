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

#include "var_fmt.h"

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

/* ============================================================================================================= */
typedef enum vfmt_type_t { vft_literal, vft_str, vft_int } vfmt_type_t;
/* ============================================================================================================= */

/* ============================================================================================================= */
/* private: the format-elements = literal, String, uin64_t */
typedef struct vfmt_desc_t {
    vfmt_type_t type;    /* which one of the options is it? ( vft_literal not used ) */
    const String * name;    /* this one is owned by this struct! */
    uint8_t idx;            /* which idx to use ( for vft_str, vft_int ) */
    uint8_t idx2;           /* alternative idx ( for vft_str, if NULL-ptr given by client ) */
} vfmt_desc_t;

/* create a var-name-element, makes a copy of the given string and owns that copy */
static vfmt_desc_t * vfmt_create_desc( const char * src, vfmt_type_t t, uint8_t idx, uint8_t idx2 ) {
    vfmt_desc_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        rc_t rc;
        String S;
        StringInitCString( &S, src );
        rc = StringCopy( &( res -> name ), &S );
        if ( 0 == rc ) {
            res -> type = t;
            res -> idx = idx;
            res -> idx2 = idx2;
        }
        else {
            free( ( void * ) res );
            res = NULL;
        }
    }
    return res;
}

static void vfmt_destroy_desc( void * self, void * data ) {
    if ( NULL != self ) {
        vfmt_desc_t * desc = ( vfmt_desc_t * )self;
        StringWhack ( desc -> name );
        free( self );
    }
}

static int64_t vfmt_cmp_desc( const void *key, const void *n ) {
    const String * s_key = ( String * ) key;
    vfmt_desc_t * item = ( vfmt_desc_t * ) n;
    if ( s_key -> len < item -> name -> len ) {
        return -1;
    }
    else {
        uint32_t item_len = item -> name -> len;
        int32_t offset = s_key -> len - item_len;
        String s_key2;
        StringInit( &s_key2, s_key -> addr + offset, item_len, item_len );
        return StringOrder( &s_key2, item -> name );
    }
}

/* ============================================================================================================= */

typedef struct vfmt_desc_list_t {
    Vector descriptions;        /* pointers to var_desc_t - structs */
} vfmt_desc_list_t;

struct vfmt_desc_list_t * vfmt_create_desc_list( void ) {
    vfmt_desc_list_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        VectorInit ( &( res -> descriptions ), 0, 12 );
    }
    return res;
}

void vfmt_release_desc_list( vfmt_desc_list_t * self ) {
    if ( NULL != self ) {
        VectorWhack ( &( self -> descriptions ), vfmt_destroy_desc, NULL );
        free( ( void * ) self );
    }
}

static void vfmt_add_to_desc_list( vfmt_desc_list_t * self, vfmt_desc_t * desc ) {
    if ( NULL != desc ) {
        rc_t rc = VectorAppend ( &( self -> descriptions ), NULL, desc );
        if ( 0 != rc ) {
            vfmt_destroy_desc( desc, NULL );
        }
    }
}

void vfmt_add_str_to_desc_list( vfmt_desc_list_t * self, const char * name, uint32_t idx, uint32_t idx2 ) {
    if ( NULL != self && NULL != name ) {
        vfmt_add_to_desc_list( self, vfmt_create_desc( name, vft_str, idx, idx2 ) );
    }
}

void vfmt_add_int_to_desc_list( vfmt_desc_list_t * self, const char * name, uint32_t idx ) {
    if ( NULL != self && NULL != name ) {
        vfmt_add_to_desc_list( self, vfmt_create_desc( name, vft_int, idx, 0xFF ) );
    }
}

static const vfmt_desc_t * vfmt_find_desc( const vfmt_desc_list_t * self, const String * to_find ) {
    const vfmt_desc_t * res = NULL;
    if ( NULL != self && NULL != to_find ) {
        const Vector * v =  &( self -> descriptions );
        uint32_t i, l = VectorLength( v );
        for ( i = VectorStart( v ); i < l && NULL == res; ++i ) {
            const vfmt_desc_t * desc = VectorGet ( v, i );
            if ( NULL != desc ) {
                int64_t cmp = vfmt_cmp_desc( to_find, desc );
                if ( 0 == cmp ) { res = desc; }
            }
        }
    }
    return res;
}

/* ============================================================================================================= */
/* private: the format-elements = literal, String, uin64_t */
typedef struct vfmt_entry_t {
    vfmt_type_t type;       /* which one of the options is it? */
    const String * literal; /* this one is owned by this struct! */
    uint8_t idx;            /* which str/int-arg to use here */
    uint8_t idx2;           /* the alternative idx, for vft_str and client gives a NULL */
} vfmt_entry_t;
/* ============================================================================================================= */

/* create a literal format-element, makes a copy of the given string and owns that copy */
static vfmt_entry_t * vfmt_create_entry_literal( const char * src, size_t len ) {
    if ( 0 == len || NULL == src ) {
        return NULL;
    }
    else {
        vfmt_entry_t * res = calloc( 1, sizeof * res );
        if ( NULL != res ) {
            rc_t rc;
            String S;
            StringInit( &S, src, len, len );
            rc = StringCopy( &( res -> literal ), &S );
            if ( 0 == rc ) {
                res -> type = vft_literal;
            } else {
                free( ( void * ) res );
                res = NULL;
            }
        }
        return res;
    }
}

/* create a string format-element, stores the index of the string to use */
static vfmt_entry_t * vfmt_create_entry( vfmt_type_t t, uint8_t idx, uint8_t idx2 ) {
    vfmt_entry_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        res -> type = t;
        res -> idx = idx;
        res -> idx2 = idx2;
    }
    return res;
}

static vfmt_entry_t * vfmt_clone_entry( const vfmt_entry_t * src ) {
    vfmt_entry_t * res = NULL;
    if ( NULL != src ) {
        res = vfmt_create_entry( src -> type, src -> idx, src -> idx2 );
        if ( NULL != res ) {
            if ( vft_literal == res -> type ) {
                rc_t rc = StringCopy( &( res -> literal ), src -> literal );
                if ( 0 != rc ) {
                    free( ( void * ) res );
                    res = NULL;
                }
            }
        }
    }
    return res;
}

static void vfmt_copy_String_to_buffer( SBuffer_t *buffer, const String * src ) {
    if ( NULL != src && NULL != buffer ) {
        uint32_t i = 0;
        String * dst = &( buffer -> S );
        char * dst_addr = ( char * )( dst -> addr );
        while ( i < src -> len && dst -> len < buffer -> buffer_size ) {
            dst_addr [ dst -> len ] = src -> addr[ i++ ];
            dst -> len += 1;
        }
    }
}

static void vfmt_copy_int_to_buffer( SBuffer_t *buffer, uint64_t value ) {
    char temp[ 21 ];
    size_t temp_written;
    rc_t rc = string_printf ( temp, sizeof temp, &temp_written, "%lu", value );
    if ( 0 == rc ) {
        String S;
        StringInitCString( &S, temp );
        vfmt_copy_String_to_buffer( buffer, &S );
    }
}

static void vfmt_copy_int_entry_to_buffer( const vfmt_entry_t * self, SBuffer_t *buffer,
                                           const uint64_t * args, size_t args_len ) {
    if ( NULL != args && self -> idx < args_len ) {
        vfmt_copy_int_to_buffer( buffer, args[ self -> idx ] );
    }
}

/*we need both: str-args AND int-args, because of the alternative idx-usage */
static void vfmt_copy_str_entry_to_buffer( const vfmt_entry_t * self, SBuffer_t *buffer,
                                         const String ** str_args, size_t str_args_len,
                                         const uint64_t * int_args, size_t int_args_len ) {
    if ( NULL != str_args && self -> idx < str_args_len ) {
        const String * src = str_args[ self -> idx ];
        if ( NULL != src && NULL != src -> addr ) {
            /* we have a string to use */
            if ( self -> idx2 == 0xFF ) {
                /* there is no alternative -> print the string even if len == 0 */
                vfmt_copy_String_to_buffer( buffer, src );
            } else {
                if ( src -> len > 0 ) {
                    /* there is an alternative, but we have a valid string to print */
                    vfmt_copy_String_to_buffer( buffer, src );
                } else if ( NULL != int_args && self -> idx2 < int_args_len ) {
                    /* there is an alternative - and we have an empty string -  */
                    vfmt_copy_int_to_buffer( buffer, int_args[ self -> idx2 ] );
                }
            }
        } else if ( NULL != int_args && self -> idx2 < int_args_len ) {
            /* the string-source is NULL, and we have an alternative to use */
            vfmt_copy_int_to_buffer( buffer, int_args[ self -> idx2 ] );
        }
    } else if ( NULL != int_args && self -> idx2 < int_args_len ) {
        /* there is not even a array of strings, and we have an alternative to use */
        vfmt_copy_int_to_buffer( buffer, int_args[ self -> idx2 ] );
    }
}

static void vfmt_copy_entry_to_buffer( const vfmt_entry_t * self, SBuffer_t * dst,
                               const String ** str_args, size_t str_args_len,
                               const uint64_t * int_args, size_t int_args_len ) {
    if ( NULL != self && NULL != dst ) {
        switch ( self -> type ) {
            /* we enter a string literal */
            case vft_literal : vfmt_copy_String_to_buffer( dst, self -> literal );
            break;
            
            /* we enter a string argument ( supply the int-vector too, because of the alternative! ) */
            case vft_str    : vfmt_copy_str_entry_to_buffer( self, dst,
                                                            str_args, str_args_len,
                                                            int_args, int_args_len );
            break;
            
            /* we enter a int argument */
            case vft_int    : vfmt_copy_int_entry_to_buffer( self, dst, int_args, int_args_len ); break;
        }
    }
}
                               
/* releases an element, data-pointer to match VectorWhack-callback */
static void vfmt_destroy_entry( void * self, void * data ) {
    if ( NULL != self ) {
        vfmt_entry_t * vft = ( vfmt_entry_t * )self;
        if ( vft_literal == vft -> type ) { StringWhack ( vft -> literal ); }
        free( self );
    }
}

/* ============================================================================================================= */
typedef struct vfmt_t {
    Vector elements;        /* the elements are pointers to var_fmt_entry_t - structs */
    size_t fixed_len;       /* sum of all literal elements + sum of dflt-len of int-elements */
    SBuffer_t buffer;       /* internal buffer to print into */
} vfmt_t;
/* ============================================================================================================= */

static void vfmt_append_entry( vfmt_t * self, vfmt_entry_t * entry ) {
    if ( NULL == self || NULL == entry ) { return; }
    rc_t rc = VectorAppend ( &( self -> elements ), NULL, entry );
    if ( 0 != rc ) {
        vfmt_destroy_entry( entry, NULL );
    }
}

static bool vfmt_find_desc_and_add_if_found( vfmt_t * self,
                               const String * T,
                               const struct vfmt_desc_list_t * vars ) {
    // do we have so far a match against any variable-name at the end of T ?
    const vfmt_desc_t * found = vfmt_find_desc( vars, T );
    if ( NULL != found ) {
        /* yes it does! - the key is at the end of T
           T -> len cannot be shorter than item -> name -> len
           because otherwise the key would not be found by var_name_find() */
        uint32_t literal_len = ( T -> len ) - ( found -> name -> len ); 
        if ( literal_len > 0 ) {
            vfmt_append_entry( self, vfmt_create_entry_literal( T -> addr, literal_len ) );
        }
        /* we always have a non-literal,
           because otherwise the key would not have been found */
        vfmt_append_entry( self, vfmt_create_entry( found -> type, found -> idx, found -> idx2 ) );
    }
    return ( NULL != found );
}

static size_t vfmt_calc_fixed_len( Vector * v ) {
    size_t res = 0;
    uint32_t i, l = VectorLength( v );
    for ( i = VectorStart( v ); i < l; ++i ) {
        const vfmt_entry_t * entry = VectorGet ( v, i );
        switch( entry -> type ) {
            case vft_literal: res += entry -> literal -> len; break;
            case vft_int    : res += 20; break;     /* the length of max_uint64_t as string */
            case vft_str    : break;    /* we do not know yet... */
        }
    }
    return res;
}

static void vfmt_append( struct vfmt_t * self,  const String * fmt,
                         const struct vfmt_desc_list_t * vars ) {
    if ( NULL != self && NULL != fmt ) {
        uint32_t i;
        String temp = { fmt -> addr, 0, 0 };
        for ( i = 0; i < fmt -> len; ++i ) {
            if ( vfmt_find_desc_and_add_if_found( self, &temp, vars ) ) {
                /* advance T to restart searching... */
                temp . addr += temp . len;
                temp . len = 1;
                temp . size = 1;
            } else {
                temp . len++;
                temp . size++;
            }
        }
        /* handle what is left in T */
        if ( !vfmt_find_desc_and_add_if_found( self, &temp, vars ) ) {
            vfmt_append_entry( self, vfmt_create_entry_literal( temp . addr, temp . len ) );
        }
        /* calculate new fixed-len, and adjust print-buffer */
        self -> fixed_len = vfmt_calc_fixed_len( &( self -> elements ) );
        increase_SBuffer_to( &( self -> buffer ), ( self -> fixed_len * 4 ) );
    }
}

static void vfmt_append_str( struct vfmt_t * self,  const char * fmt,
                             const struct vfmt_desc_list_t * vars ) {
    if ( NULL != self && NULL != fmt ) {
        String FMT;
        StringInitCString( &FMT, fmt );
        vfmt_append( self, &FMT, vars );
    }
}

/* create an empty var-print struct, to be added into later */
static struct vfmt_t * vfmt_create_by_size( size_t buffer_size ) {
    vfmt_t * self = calloc( 1, sizeof * self );
    if ( NULL != self ) {
        VectorInit ( &( self -> elements ), 0, 12 );
        make_SBuffer( &( self -> buffer ), buffer_size );
    }
    return self;
}

/* create a var-print struct and fill it with var_fmt_t - elements */
struct vfmt_t * vfmt_create( const String * fmt, const struct vfmt_desc_list_t * vars ) {
    vfmt_t * self = vfmt_create_by_size( 2048 );
    if ( NULL != self ) {
        vfmt_append( self, fmt, vars );
    }
    return self;
}

static struct vfmt_t * vfmt_create_from_str( const char * fmt, const struct vfmt_desc_list_t * vars ) {
    vfmt_t * self = vfmt_create_by_size( 2048 );
    if ( NULL != self ) {
        vfmt_append_str( self, fmt, vars );
    }
    return self;
}

/* release a var-print struct and call destructors on its elements */
void vfmt_release( struct vfmt_t * self ) {
    if ( NULL != self ) {
        VectorWhack ( &( self -> elements ), vfmt_destroy_entry, NULL );
        release_SBuffer( &( self -> buffer ) );
        free( ( void * ) self );
    }
}

static size_t vfmt_calc_buffer_size( const struct vfmt_t * self,
                    const String ** str_args, size_t str_args_len ) {
    size_t res = 0;
    if ( NULL != self ) {
        const Vector * v = &( self -> elements );
        uint32_t i, l = VectorLength( v );
        res = self -> fixed_len;
        for ( i = VectorStart( v ); i < l; ++i ) {
            const vfmt_entry_t * entry = VectorGet( v, i );
            if ( NULL != entry ) {
                if ( vft_str == entry -> type ) {
                    if ( entry -> idx < str_args_len ) {
                        const String * S = str_args[ entry -> idx ];
                        if ( NULL != S ) {
                            if ( NULL != S -> addr ) {
                                res += S -> len;
                            }
                        }
                    }
                }
            }
        }
    }
    return res;
}

/* apply the var-fmt-struct to the given arguments, write result to buffer */
SBuffer_t * vfmt_write_to_buffer( struct vfmt_t * self,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len ) {
    SBuffer_t * res = NULL;
    if ( NULL != self )
    {
        size_t needed = vfmt_calc_buffer_size( self, str_args, str_args_len ); /* above */
        if ( needed > 0 )
        {
            rc_t rc = increase_SBuffer_to( &( self -> buffer ), needed ); /* does nothing if not neccessary */
            if ( 0 == rc )
            {
                SBuffer_t * dst = &( self -> buffer );
                self -> buffer . S . len = 0;
                self -> buffer . S . size = 0;

                const Vector * v = &( self -> elements );
                uint32_t i, l = VectorLength( v );
                for ( i = VectorStart( v ); i < l; ++i ) {
                    const vfmt_entry_t * entry = VectorGet( v, i );
                    vfmt_copy_entry_to_buffer( entry, dst,
                                               str_args, str_args_len, int_args, int_args_len );
                }
                self -> buffer . S . size = self -> buffer . S . len;
                res = &( self -> buffer );
            }
        }
    }
    return res;
}

/* apply the var-fmt-struct to the given arguments, print the result to file */
rc_t vfmt_print_to_file( struct vfmt_t * self,
                    KFile * f,
                    uint64_t * pos,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len ) {
    rc_t rc;
    if ( NULL == self || NULL == f || NULL == pos ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    } else {
        SBuffer_t * t = vfmt_write_to_buffer( self, str_args, str_args_len, int_args, int_args_len );
        if ( NULL != t ) {
            size_t num_writ;
            rc = KFileWrite( f, *pos, t -> S . addr, t -> S . len, &num_writ );
            if ( 0 == rc ) {
                *pos += num_writ;
            }
        } else {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
    }
    return rc;
}
