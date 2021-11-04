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

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

/* ============================================================================================================= */
typedef enum var_fmt_type_t { vft_literal, vft_str, vft_int } var_fmt_type_t;
/* ============================================================================================================= */

/* ============================================================================================================= */
/* private: the format-elements = literal, String, uin64_t */
typedef struct var_desc_t {
    var_fmt_type_t type;    /* which one of the options is it? ( vft_literal not used ) */
    const String * name;    /* this one is owned by this struct! */
    uint8_t idx;            /* which idx to use ( for vft_str, vft_int ) */
    uint8_t idx2;           /* alternative idx ( for vft_str, if NULL-ptr given by client ) */
} var_desc_t;
/* ============================================================================================================= */

/* create a var-name-element, makes a copy of the given string and owns that copy */
static var_desc_t * var_desc_create( const char * src, var_fmt_type_t t, uint8_t idx, uint8_t idx2 ) {
    var_desc_t * res = calloc( 1, sizeof * res );
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

static void destroy_var_desc( void * self, void * data ) {
    if ( NULL != self ) {
        var_desc_t * desc = ( var_desc_t * )self;
        StringWhack ( desc -> name );
        free( self );
    }
}

static int64_t var_desc_cmp( const void *key, const void *n ) {
    const String * s_key = ( String * ) key;
    var_desc_t * item = ( var_desc_t * ) n;
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

typedef struct var_desc_list_t {
    Vector descriptions;        /* pointers to var_desc_t - structs */
} var_desc_list_t;

struct var_desc_list_t * create_var_desc_list( void ) {
    var_desc_list_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        VectorInit ( &( res -> descriptions ), 0, 12 );
    }
    return res;
}

void release_var_desc_list( var_desc_list_t * self ) {
    if ( NULL != self ) {
        VectorWhack ( &( self -> descriptions ), destroy_var_desc, NULL );
        free( ( void * ) self );
    }
}

static void var_desc_list_add( var_desc_list_t * self, var_desc_t * desc ) {
    if ( NULL != desc ) {
        rc_t rc = VectorAppend ( &( self -> descriptions ), NULL, desc );
        if ( 0 != rc ) {
            destroy_var_desc( desc, NULL );
        }
    }
}

void var_desc_list_add_str( var_desc_list_t * self, const char * name, uint32_t idx, uint32_t idx2 ) {
    if ( NULL != self && NULL != name ) {
        var_desc_list_add( self, var_desc_create( name, vft_str, idx, idx2 ) );
    }
}

void var_desc_list_add_int( var_desc_list_t * self, const char * name, uint32_t idx ) {
    if ( NULL != self && NULL != name ) {
        var_desc_list_add( self, var_desc_create( name, vft_int, idx, 0xFF ) );
    }
}

static const var_desc_t * var_desc_list_find( const var_desc_list_t * self, const String * to_find ) {
    const var_desc_t * res = NULL;
    if ( NULL != self && NULL != to_find ) {
        const Vector * v =  &( self -> descriptions );
        uint32_t i, l = VectorLength( v );
        for ( i = VectorStart( v ); i < l && NULL == res; ++i ) {
            const var_desc_t * desc = VectorGet ( v, i );
            if ( NULL != desc ) {
                int64_t cmp = var_desc_cmp( to_find, desc );
                if ( 0 == cmp ) { res = desc; }
            }
        }
    }
    return res;
}

static void var_desc_test_find( var_desc_list_t * self, char * to_find ) {
    String S1;
    StringInitCString( &S1, to_find );
    const var_desc_t * desc = var_desc_list_find( self, &S1 );

    KOutMsg( "found( '%s' ) = %p\n", to_find, desc );
    if ( NULL != desc ) {
        KOutMsg( "\tname = %S, idx = %u, type = %u\n", desc -> name, desc -> idx, desc -> type );
    }
}

void var_desc_list_test( void ) {
    var_desc_list_t * lst = NULL;
    
    KOutMsg( "var-desc-list-test\n" );
    lst = create_var_desc_list();
    if ( NULL != lst ) {
        var_desc_list_add_str( lst, "$ac", 0, 0xFF );
        var_desc_list_add_str( lst, "$sg", 1, 0xFF );
        var_desc_list_add_int( lst, "$si", 0 );
        var_desc_list_add_int( lst, "$sl", 1 );

        var_desc_test_find( lst, "test" );
        var_desc_test_find( lst, "$ac" );
        var_desc_test_find( lst, "$sg" );
        var_desc_test_find( lst, "$si" );
        var_desc_test_find( lst, "$sl" );
        var_desc_test_find( lst, "xyz$ac" );
        var_desc_test_find( lst, "xyz$ac2" );

        release_var_desc_list( lst );
    }
}

/* ============================================================================================================= */
/* private: the format-elements = literal, String, uin64_t */
typedef struct var_fmt_entry_t {
    var_fmt_type_t type;    /* which one of the options is it? */
    const String * literal; /* this one is owned by this struct! */
    uint8_t idx;            /* which str/int-arg to use here */
    uint8_t idx2;           /* the alternative idx, for vft_str and client gives a NULL */
} var_fmt_entry_t;
/* ============================================================================================================= */


/* create a literal format-element, makes a copy of the given string and owns that copy */
static var_fmt_entry_t * var_fmt_entry_create_literal( const char * src, size_t len ) {
    if ( 0 == len || NULL == src ) {
        return NULL;
    }
    else {
        var_fmt_entry_t * res = calloc( 1, sizeof * res );
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
static var_fmt_entry_t * var_fmt_entry_create( var_fmt_type_t t, uint8_t idx, uint8_t idx2 ) {
    var_fmt_entry_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        res -> type = t;
        res -> idx = idx;
        res -> idx2 = idx2;
    }
    return res;
}

static var_fmt_entry_t * clone_var_fmt_entry( const var_fmt_entry_t * src ) {
    var_fmt_entry_t * res = NULL;
    if ( NULL != src ) {
        res = var_fmt_entry_create( src -> type, src -> idx, src -> idx2 );
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

static void var_fmt_String_to_buffer( SBuffer_t *buffer, const String * src ) {
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

static void var_fmt_entry_int_value_to_buffer( SBuffer_t *buffer, uint64_t value ) {
    char temp[ 21 ];
    size_t temp_written;
    rc_t rc = string_printf ( temp, sizeof temp, &temp_written, "%lu", value );
    if ( 0 == rc ) {
        String S;
        StringInitCString( &S, temp );
        var_fmt_String_to_buffer( buffer, &S );
    }
}

static void var_fmt_entry_int_to_buffer( const var_fmt_entry_t * self, SBuffer_t *buffer,
                                         const uint64_t * args, size_t args_len ) {
    if ( NULL != args && self -> idx < args_len ) {
        var_fmt_entry_int_value_to_buffer( buffer, args[ self -> idx ] );
    }
}

/*we need both: str-args AND int-args, because of the alternative idx-usage */
static void var_fmt_entry_str_to_buffer( const var_fmt_entry_t * self, SBuffer_t *buffer,
                                         const String ** str_args, size_t str_args_len,
                                         const uint64_t * int_args, size_t int_args_len ) {
    if ( NULL != str_args && self -> idx < str_args_len ) {
        const String * src = str_args[ self -> idx ];
        if ( NULL != src ) {
            /* we have a string to use */
            if ( self -> idx2 == 0xFF ) {
                /* there is no alternative -> print the string even if len == 0 */
                var_fmt_String_to_buffer( buffer, src );
            } else {
                if ( src -> len > 0 ) {
                    /* there is an alternative, but we have a valid string to print */
                    var_fmt_String_to_buffer( buffer, src );
                } else if ( NULL != int_args && self -> idx2 < int_args_len ) {
                    /* there is an alternative - and we have an empty string -  */
                    var_fmt_entry_int_value_to_buffer( buffer, int_args[ self -> idx2 ] );
                }
            }
        } else if ( NULL != int_args && self -> idx2 < int_args_len ) {
            /* the string-source is NULL, and we have an alternative to use */
            var_fmt_entry_int_value_to_buffer( buffer, int_args[ self -> idx2 ] );
        }
    } else if ( NULL != int_args && self -> idx2 < int_args_len ) {
        /* there is not even a array of strings, and we have an alternative to use */
        var_fmt_entry_int_value_to_buffer( buffer, int_args[ self -> idx2 ] );
    }
}
    
/* releases an element, data-pointer to match VectorWhack-callback */
static void destroy_var_fmt_entry( void * self, void * data ) {
    if ( NULL != self ) {
        var_fmt_entry_t * vft = ( var_fmt_entry_t * )self;
        if ( vft_literal == vft -> type ) { StringWhack ( vft -> literal ); }
        free( self );
    }
}

/* ============================================================================================================= */
typedef struct var_fmt_t {
    Vector elements;        /* the elements are pointers to var_fmt_entry_t - structs */
    size_t fixed_len;       /* sum of all literal elements + sum of dflt-len of int-elements */
    SBuffer_t buffer;       /* internal buffer to print into */
} var_fmt_t;
/* ============================================================================================================= */

static void var_fmt_add_entry( var_fmt_t * self, var_fmt_entry_t * entry ) {
    if ( NULL == self || NULL == entry ) { return; }
    rc_t rc = VectorAppend ( &( self -> elements ), NULL, entry );
    if ( 0 != rc ) {
        destroy_var_fmt_entry( entry, NULL );
    }
}

static bool var_fmt_find_and_add( var_fmt_t * self,
                                  const String * T,
                                  const struct var_desc_list_t * vars ) {
    // do we have so far a match against any variable-name at the end of T ?
    const var_desc_t * found = var_desc_list_find( vars, T );
    if ( NULL != found ) {
        /* yes it does! - the key is at the end of T
           T -> len cannot be shorter than item -> name -> len
           because otherwise the key would not be found by var_name_find() */
        uint32_t literal_len = ( T -> len ) - ( found -> name -> len ); 
        if ( literal_len > 0 ) {
            var_fmt_add_entry( self, var_fmt_entry_create_literal( T -> addr, literal_len ) );
        }
        /* we always have a non-literal,
           because otherwise the key would not have been found */
        var_fmt_add_entry( self, var_fmt_entry_create( found -> type, found -> idx, found -> idx2 ) );
    }
    return ( NULL != found );
}

static size_t var_fmt_calc_fixed_len( Vector * v ) {
    size_t res = 0;
    uint32_t i, l = VectorLength( v );
    for ( i = VectorStart( v ); i < l; ++i ) {
        const var_fmt_entry_t * entry = VectorGet ( v, i );
        switch( entry -> type ) {
            case vft_literal: res += entry -> literal -> len; break;
            case vft_int    : res += 20; break;     /* the length of max_uint64_t as string */
            case vft_str    : break;    /* we do not know yet... */
        }
    }
    return res;
}

/* create an empty var-print struct, to be added into later */
struct var_fmt_t * create_empty_var_fmt( size_t buffer_size ) {
    var_fmt_t * self = calloc( 1, sizeof * self );
    if ( NULL != self ) {
        VectorInit ( &( self -> elements ), 0, 12 );
        make_SBuffer( &( self -> buffer ), buffer_size );
    }
    return self;
}

void var_fmt_append( struct var_fmt_t * self,  const String * fmt, const struct var_desc_list_t * vars ) {
    if ( NULL != self && NULL != fmt ) {
        uint32_t i;
        String temp = { fmt -> addr, 0, 0 };
        for ( i = 0; i < fmt -> len; ++i ) {
            if ( var_fmt_find_and_add( self, &temp, vars ) ) {
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
        if ( !var_fmt_find_and_add( self, &temp, vars ) ) {
            var_fmt_add_entry( self, var_fmt_entry_create_literal( temp . addr, temp . len ) );
        }
        /* calculate new fixed-len, and adjust print-buffer */
        self -> fixed_len = var_fmt_calc_fixed_len( &( self -> elements ) );
        increase_SBuffer_to( &( self -> buffer ), ( self -> fixed_len * 4 ) );
    }
}

void var_fmt_append_str( struct var_fmt_t * self,  const char * fmt, const struct var_desc_list_t * vars ) {
    if ( NULL != self && NULL != fmt ) {
        String FMT;
        StringInitCString( &FMT, fmt );
        var_fmt_append( self, &FMT, vars );
    }
}

/* create a var-print struct and fill it with var_fmt_t - elements */
struct var_fmt_t * create_var_fmt( const String * fmt, const struct var_desc_list_t * vars ) {
    var_fmt_t * self = create_empty_var_fmt( 2048 );
    if ( NULL != self ) {
        var_fmt_append( self, fmt, vars );
    }
    return self;    
}

struct var_fmt_t * create_var_fmt_str( const char * fmt, const struct var_desc_list_t * vars ) {
    var_fmt_t * self = create_empty_var_fmt( 2048 );
    if ( NULL != self ) {
        var_fmt_append_str( self, fmt, vars );
    }
    return self;    
}

struct var_fmt_t * var_fmt_clone( const struct var_fmt_t * src ) {
    var_fmt_t * self = NULL;
    if ( NULL != src ) {
        self = create_empty_var_fmt( src -> buffer . buffer_size );
        if ( NULL != self ) {
            const Vector * v = &( src -> elements );
            uint32_t i, l = VectorLength( v );
            for ( i = VectorStart( v ); i < l; ++i ) {
                var_fmt_add_entry( self, clone_var_fmt_entry(  VectorGet( v, i ) ) );
            }
            self -> fixed_len = var_fmt_calc_fixed_len( &( self -> elements ) );
            increase_SBuffer_to( &( self -> buffer ), ( self -> fixed_len * 4 ) );
        }
    }
    return self;
}

/* release a var-print struct and call destructors on its elements */
void release_var_fmt( struct var_fmt_t * self ) {
    if ( NULL != self ) {
        VectorWhack ( &( self -> elements ), destroy_var_fmt_entry, NULL );
        release_SBuffer( &( self -> buffer ) );
        free( ( void * ) self );
    }
}

size_t var_fmt_buffer_size( const struct var_fmt_t * self,
                    const String ** str_args, size_t str_args_len ) {
    size_t res = 0;
    if ( NULL != self ) {
        const Vector * v = &( self -> elements );
        uint32_t i, l = VectorLength( v );
        res = self -> fixed_len;
        for ( i = VectorStart( v ); i < l; ++i ) {
            const var_fmt_entry_t * entry = VectorGet( v, i );
            if ( NULL != entry ) {
                if ( vft_str == entry -> type ) {
                    if ( entry -> idx < str_args_len ) {
                        const String * S = str_args[ entry -> idx ];
                        if ( NULL != S ) {
                            res += S -> len;
                        }
                    }
                }
            }
        }
    }
    return res;
}

void var_fmt_debug( const struct var_fmt_t * self ) {
    if ( NULL != self ) {
        const Vector * v = &( self -> elements );
        uint32_t i, l = VectorLength( v );
        KOutMsg( "\nvar-fmt:" );
        for ( i = VectorStart( v ); i < l; ++i ) {
            const var_fmt_entry_t * entry = VectorGet( v, i );
            if ( NULL != entry ) {
                switch ( entry -> type ) {
                    case vft_literal : KOutMsg( "\nliteral: '%S'", entry -> literal  ); break;
                    case vft_str     : KOutMsg( "\nstr: #%u", entry -> idx  ); break;
                    case vft_int     : KOutMsg( "\nint: #%u", entry -> idx  ); break;
                    default          : KOutMsg( "\nunknown entry-type" ); break;
                }
            }
        }
        KOutMsg( "\ndone\n" );
    }
}

/* apply the var-fmt-struct to the given arguments, write result to buffer */
SBuffer_t * var_fmt_to_buffer( struct var_fmt_t * self,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len ) {
    SBuffer_t * res = NULL;
    if ( NULL != self )
    {
        size_t needed = var_fmt_buffer_size( self, str_args, str_args_len );
        if ( needed > 0 )
        {
            rc_t rc = increase_SBuffer_to( &( self -> buffer ), needed );
            if ( 0 == rc )
            {
                const Vector * v = &( self -> elements );
                uint32_t i, l = VectorLength( v );
                SBuffer_t * dst = &( self -> buffer );
                self -> buffer . S . len = 0;
                self -> buffer . S . size = 0;

                for ( i = VectorStart( v ); i < l; ++i ) {
                    const var_fmt_entry_t * entry = VectorGet( v, i );
                    if ( NULL != entry ) {
                        switch ( entry -> type ) {
                            /* we enter a string literal */
                            case vft_literal : var_fmt_String_to_buffer( dst,
                                                                         entry -> literal );
                                               break;

                            /* we enter a string argument ( supply the int-vector too, because of the alternative! ) */
                            case vft_str    : var_fmt_entry_str_to_buffer( entry, dst,
                                                                           str_args, str_args_len,
                                                                           int_args, int_args_len );
                                              break;

                            /* we enter a int argument */
                            case vft_int    : var_fmt_entry_int_to_buffer( entry, dst, int_args, int_args_len ); break;
                        }
                    }
                }
                self -> buffer . S . size = self -> buffer . S . len;
                res = &( self -> buffer );
            }
        }
    }
    return res;
}

/* apply the var-fmt-struct to the given arguments, print the result to stdout */
rc_t var_fmt_to_stdout( struct var_fmt_t * self,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len ) {
    rc_t rc;
    SBuffer_t * t = var_fmt_to_buffer( self, str_args, str_args_len, int_args, int_args_len );
    if ( NULL != t ) {
        rc = KOutMsg( "%S", &( t -> S ) );
    } else {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    return rc;
}

/* apply the var-fmt-struct to the given arguments, print the result to file */
rc_t var_fmt_to_file( struct var_fmt_t * self,
                    KFile * f,
                    uint64_t * pos,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len ) {
    rc_t rc;
    if ( NULL == self || NULL == f || NULL == pos ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    } else {
        SBuffer_t * t = var_fmt_to_buffer( self, str_args, str_args_len, int_args, int_args_len );
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

void var_fmt_test( void ) {
    var_fmt_t * fmt = NULL;
    var_desc_list_t * desc_lst = NULL;
    
    KOutMsg( "var-fmt-test\n" );

    desc_lst = create_var_desc_list();
    if ( NULL != desc_lst ) {
        var_desc_list_add_str( desc_lst, "$ac", 0, 0xFF );
        var_desc_list_add_str( desc_lst, "$sg", 1, 0xFF );
        var_desc_list_add_int( desc_lst, "$si", 0 );
        var_desc_list_add_int( desc_lst, "$sl", 1 );

        fmt = create_var_fmt_str( ">$ac.$si/$sl this $ac/$sg is a test $si-$sl format", desc_lst );
        release_var_desc_list( desc_lst );
    }

    if ( NULL != fmt ) {
        const char * s_acc = "SRR1234567";
        const char * s_grp = "SG_1";
        String S_acc;
        String S_grp;
        const String * strings[ 2 ];
        uint64_t ints[ 2 ];

        var_fmt_debug( fmt );

        StringInitCString( &S_acc, s_acc );
        StringInitCString( &S_grp, s_grp );
        strings[ 0 ] = &S_acc;
        strings[ 1 ] = &S_grp;
        ints[ 0 ] = 1001;
        ints[ 1 ] = 77;

        {
            size_t needed = var_fmt_buffer_size( fmt, strings, 2 );
            KOutMsg( "recomended buffer-size = %u\n", needed );
        }

        {
            var_fmt_t * cloned = var_fmt_clone( fmt );
            var_fmt_append_str( cloned, " and this!\n", NULL );
            var_fmt_to_stdout( cloned, strings, 2, ints, 2 );
        }
    }
    release_var_fmt( fmt );
}
