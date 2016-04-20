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

#include <klib/printf.h>
#include <klib/out.h>

#include <os-native.h>
#include <sysalloc.h>
#include <ctype.h>
#include <string.h>

#include "cigar.h"

struct cigar_t
{
    char * op;
    int * count;
    size_t size, length;
};


static void init_cigar_t( struct cigar_t * c, size_t size )
{
    if ( c != NULL )
    {
        c->size = 0;
        c->length = 0;
        c->op = malloc( sizeof( c->op[ 0 ] ) * size );
        if ( c->op != NULL )
        {
            c->count = malloc( sizeof( c->count[ 0 ] ) * size );
            if ( c->count != NULL )
                c->size = size;
            else
                free( ( void * ) c->op );
        }
    }
}


static void resize_cigar_t( struct cigar_t * c, size_t new_size )
{
    if ( c != NULL )
    {
        if ( c->size == 0 )
            init_cigar_t( c, new_size );
        else if ( c->size < new_size )
        {
            char * temp_op = c->op;
            c->op = realloc( c->op, sizeof( c->op[ 0 ] ) * new_size );
            if ( c->op != NULL )
            {
                int * temp_count = c->count;
                c->count = realloc( c->count, sizeof( c->count[ 0 ] ) * new_size );
                if ( c->count != NULL )
                    c->size = new_size;
                else
                    c->count = temp_count;
            }
            else
                c->op = temp_op;
        }
    }
}

static void append_to_cigar_t( struct cigar_t * c, char op, int count )
{
    if ( c->length < c->size )
    {
        c->op[ c->length ] = op;
        c->count[ c->length ++ ] = count;
    }
}

void parse_cigar_t( struct cigar_t * c, const char * cigar_str )
{
    if ( c != NULL && cigar_str != NULL && cigar_str[ 0 ] != 0 )
    {
        resize_cigar_t( c, strlen( cigar_str ) );
        if ( c->size > 0 )
        {
            int count = 0;
            while ( *cigar_str != 0 && c->length < c->size )
            {
                if ( isdigit( *cigar_str ) )
                {
                    count = ( count * 10 ) + ( *cigar_str - '0' );
                }
                else
                {
                    if ( count == 0 ) count = 1;
                    append_to_cigar_t( c, *cigar_str, count );
                    count = 0;
                }
                cigar_str++;
            }
        }
    }
}


struct cigar_t * make_cigar_t( const char * cigar_str )
{
    struct cigar_t * res = malloc( sizeof * res );
    if ( res != NULL )
    {
        size_t size;
        if ( cigar_str != NULL && cigar_str[ 0 ] != 0 )
            size = strlen( cigar_str );
        else
            size = 1024;
        init_cigar_t( res, size );
        if ( res->size == size )
            parse_cigar_t( res, cigar_str );
    }
    return res;
}


void free_cigar_t( struct cigar_t * c )
{
    if ( c != NULL )
    {
        if ( c->op != NULL )
        {
            free( ( void * ) c->op );
            c->op = NULL;
        }
        if ( c->count != NULL )
        {
            free( ( void * ) c->count );
            c->count = NULL;
        }
        free( ( void * ) c );
    }
}


int cigar_t_reflen( const struct cigar_t * c )
{
    int res = 0;
    if ( c != NULL )
    {
        int i;
        for ( i = 0; i < c->length; ++i )
        {
            switch( c->op[ i ] )
            {
                case 'A'    : res += c->count[ i ]; break;
                case 'C'    : res += c->count[ i ]; break;
                case 'G'    : res += c->count[ i ]; break;
                case 'T'    : res += c->count[ i ]; break;

                case 'D'    : res += c->count[ i ]; break;
                case 'M'    : res += c->count[ i ]; break;            
            }
        }
    }
    return res;
}


int cigar_t_readlen( const struct cigar_t * c )
{
    int res = 0;
    if ( c != NULL )
    {
        int i;
        for ( i = 0; i < c->length; ++i )
        {
            if ( c->op[ i ] != 'D' )
                res += c->count[ i ];
        }
    }
    return res;
}


int cigar_t_inslen( const struct cigar_t * c )
{
    int res = 0;
    if ( c != NULL )
    {
        int i;
        for ( i = 0; i < c->length; ++i )
        {
            if ( c->op[ i ] == 'I' )
                res += c->count[ i ];
        }
    }
    return res;
}


size_t cigar_t_string( char * buffer, size_t buf_len, const struct cigar_t * c )
{
    size_t res = 0;
    if ( buffer != NULL && buf_len > 0 && c != NULL && c->length > 0 )
    {
        int i;
        for ( i = 0; i < c->length && res < buf_len; ++i )
        {
            size_t num_writ;
            string_printf( &buffer[ res ], buf_len - res, &num_writ,
                        "%d%c", c->count[ i ], c->op[ i ] );
            res += num_writ;
        }
        if ( res < buf_len )
            buffer[ res ] = 0;
    }
    return res;
}


void debug_cigar_t( const struct cigar_t * c )
{
    if ( c != NULL )
    {
        int i;
        for ( i = 0; i < c->length; ++i )
            KOutMsg( "c[%d]: %d x %c\n", i, c->count[ i ], c->op[ i ] );
    }
}

static int can_merge( char op1, char op2 )
{
    char mop1 = op1;
    char mop2 = op2;
    if ( mop1 == 'A' || mop1 == 'C' || mop1 == 'G' || mop1 == 'T' )
        mop1 = 'M';
    if ( mop2 == 'A' || mop2 == 'C' || mop2 == 'G' || mop2 == 'T' )
        mop2 = 'M';
    return ( mop1 == mop2 );
}


struct cigar_t * merge_cigar_t( const struct cigar_t * c )
{
    struct cigar_t * res = NULL;
    if ( c != NULL && c -> length > 0 )
    {
        res = malloc( sizeof * res );
        if ( res != NULL )
        {
            init_cigar_t( res, c->size );
            if ( res->size == c->size )
            {
                int i, last;
                append_to_cigar_t( res, c->op[ 0 ], c->count[ 0 ] );
                for ( i = 1; i < c->length; ++i )
                {
                    last = res->length - 1;
                    if ( can_merge( c->op[ i ], res->op[ last ] ) )
                    {
                        res->count[ last ] += c->count[ i ];
                        res->op[ last ] = 'M';
                    }
                    else
                        append_to_cigar_t( res, c->op[ i ], c->count[ i ] );    
                }
            }
        }
    }
    return res;
}


static void append_base( char * buffer, size_t buf_len, size_t * buf_idx, int count, char c )
{
    int i;
    for ( i = 0; i < count && *buf_idx < buf_len; ++i )
        buffer[ (*buf_idx)++ ] = c;
}

static void append_bases( char * buffer, size_t buf_len, size_t * buf_idx, int count,
                          const char * src, int src_len, int *src_idx )
{
    int i;
    for ( i = 0; i < count && *buf_idx < buf_len && *src_idx < src_len; ++i )
        buffer[ (*buf_idx)++ ] = src[ (*src_idx)++ ];
}


size_t cigar_t_2_read( char * buffer, size_t buf_len,
                       const struct cigar_t * c, const char * ref_bases, const char * ins_bases )
{
    size_t res = 0;
    if ( buffer != NULL && buf_len > 0 && c != NULL )
    {
        int readlen = cigar_t_readlen( c );
        if ( readlen > 0 )
        {
            int needed_ref_bases = cigar_t_reflen( c );
            int available_ref_bases = ref_bases != NULL ? strlen( ref_bases ) : 0;
            if ( available_ref_bases >= needed_ref_bases )
            {
                int needed_ins_bases = cigar_t_inslen( c );
                int available_ins_bases = ins_bases != NULL ? strlen( ins_bases ) : 0;    
                if ( available_ins_bases >= needed_ins_bases )
                {
                    int ref_idx = 0;
                    int ins_idx = 0;
                    int cigar_idx;
                    for ( cigar_idx = 0; cigar_idx < c->length; ++cigar_idx )
                    {
                        int count = c->count[ cigar_idx ];
                        switch ( c->op[ cigar_idx ] )
                        {
                            case 'A' : append_base( buffer, buf_len, &res, count, 'A' );
                                        ref_idx += count;
                                        break;

                            case 'C' : append_base( buffer, buf_len, &res, count, 'C' );
                                        ref_idx += count;
                                        break;

                            case 'G' : append_base( buffer, buf_len, &res, count, 'G' );
                                        ref_idx += count;                                                
                                        break;

                            case 'T' : append_base( buffer, buf_len, &res, count, 'T' );
                                        ref_idx += count;                                                
                                        break;

                            case 'D' : ref_idx += count;
                                        break;
                            
                            case 'I' : append_bases( buffer, buf_len, &res, count,
                                                      ins_bases, available_ins_bases, &ins_idx );
                                        break;

                            case 'M' : append_bases( buffer, buf_len, &res, count,
                                                      ref_bases, available_ref_bases, &ref_idx );
                                        break;
                        }
                    }
                    if ( res < buf_len )
                        buffer[ res ] = 0;
                }
            }
        }
    }
    return res;
}


static void print_matchcount( char * buffer, size_t buf_len, size_t *buf_idx, int *match_count )
{
    size_t num_writ;
    string_printf( &buffer[ *buf_idx ], buf_len - *buf_idx, &num_writ,    "%d", *match_count );
    *match_count = 0;
    *buf_idx += num_writ;
}

static void md_delete( char * buffer, size_t buf_len, size_t *buf_idx, int count, int *match_count,
                        const char * reference, int *ref_idx )
{
    if ( *match_count > 0 )
        print_matchcount( buffer, buf_len, buf_idx, match_count );
        
    if ( *buf_idx + count + 1 < buf_len )
    {
        int i;
        buffer[ (*buf_idx)++ ] = '^';
        for ( i = 0; i < count; ++i )
            buffer[ (*buf_idx)++ ] = reference[ (*ref_idx)++ ];
    }
}

static void md_match( char * buffer, size_t buf_len, size_t *buf_idx, int count, int *match_count,
                      const char * read, int *read_idx, const char *reference, int *ref_idx )
{
    int i;
    for ( i = 0; i < count; ++i )
    {
        if ( read[ (*read_idx)++ ] == reference[ *ref_idx ] )
        {
            (*match_count)++;
        }
        else
        {
            print_matchcount( buffer, buf_len, buf_idx, match_count );
            if ( *buf_idx < buf_len )
                buffer[ (*buf_idx)++ ] = reference[ *ref_idx ];
        }
        (*ref_idx)++;
    }
}

size_t md_tag( char * buffer, size_t buf_len,
               const struct cigar_t * c, const char * read, const char * reference )
{
    size_t res = 0;
    if ( buffer != NULL && buf_len > 0 && c != NULL )
    {
        int read_idx = 0;
        int ref_idx = 0;
        int match_count = 0;
        int cigar_idx;
        for ( cigar_idx = 0; cigar_idx < c->length; ++cigar_idx )
        {
            int count = c->count[ cigar_idx ];
            switch ( c->op[ cigar_idx ] )
            {
                case 'D' :     md_delete( buffer, buf_len, &res, count, &match_count,    
                                       reference, &ref_idx );
                            break;
                
                case 'I' : read_idx += count; break;

                case 'M' : md_match( buffer, buf_len, &res, count, &match_count,
                                      read, &read_idx, reference, &ref_idx );
                            break;
            }
        }
        if ( match_count > 0 )
            print_matchcount( buffer, buf_len, &res, &match_count );
            
        if ( res < buf_len )
            buffer[ res ] = 0;
    }
    return res;
}
