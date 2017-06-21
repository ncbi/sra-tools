#include "sqlite3ext.h"

SQLITE_EXTENSION_INIT1

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#include <klib/num-gen.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/printf.h>

#include <kdb/manager.h> /* because path-types are defined there! */
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>

/*
** A macro to hint to the compiler that a function should not be
** inlined.
*/
#if defined(__GNUC__)
#  define CSV_NOINLINE  __attribute__((noinline))
#elif defined(_MSC_VER) && _MSC_VER>=1310
#  define CSV_NOINLINE  __declspec(noinline)
#else
#  define CSV_NOINLINE
#endif

/* -------------------------------------------------------------------------------------- */
static rc_t VNamelist_from_KNamelist( VNamelist ** dst, const KNamelist * src )
{
	rc_t rc = VNamelistMake( dst, 5 );
	if ( rc == 0 )
	{
		uint32_t idx, count;
		rc = KNamelistCount( src, &count );
		for ( idx = 0; rc == 0 && idx < count; ++idx )
		{
			const char * s = NULL;
			rc = KNamelistGet( src, idx, &s );
			if ( rc == 0 && s != NULL )
				rc = VNamelistAppend( *dst, s );
		}
	}
	return rc;
}

/* -------------------------------------------------------------------------------------- */
typedef struct col_desc
{
    const char * typecast;
    const char * name;
} col_desc;

static void CC destroy_col_desc( void * item, void * data )
{
    if ( item != NULL )
    {
        col_desc * desc = item;
        if ( desc->typecast != NULL ) sqlite3_free( ( void * )desc->typecast );
        if ( desc->name != NULL ) sqlite3_free( ( void * )desc->name );
        sqlite3_free( desc );
    }
}

static col_desc * make_col_desc( const char * decl )
{
    col_desc * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        rc_t rc = 0;
        memset( res, 0, sizeof( *res ) );
        {
            VNamelist * l;
            rc = VNamelistFromStr( &l, decl, ')' );
            if ( rc == 0 )
            {
                uint32_t count;
                rc = VNameListCount( l, &count );
                if ( rc == 0 )
                {
                    const char * s;
                    if ( count == 1 )
                    {
                        rc = VNameListGet( l, 0, &s );
                        if ( rc == 0 )
                            res->name = sqlite3_mprintf( s );
                    }
                    else if ( count == 2 )
                    {
                        rc = VNameListGet( l, 0, &s );
                        if ( rc == 0 )
                        {
                            if ( s[ 0 ] == '(' )
                                res->typecast = sqlite3_mprintf( &s[ 1 ] );
                            else
                                res->typecast = sqlite3_mprintf( s );
                            rc = VNameListGet( l, 1, &s );
                            if ( rc == 0 )
                                res->name = sqlite3_mprintf( s );
                        }
                    }
                    else
                    {
                        rc = -1;
                    }
                }
                VNamelistRelease( l );
            }
        }
        if ( rc != 0 || res->name == NULL )
        {
            destroy_col_desc( res, NULL );
            res = NULL;
        }
    }
    return res;
}

static void CC print_col_desc( void * item, void * data )
{
    if ( item != NULL && data != NULL )
    {
        col_desc * obj = item;
        char * sep = data;
        if ( obj->typecast != NULL )
        {
            if ( *sep != 0 )
                printf( "%c(%s)%s", *sep, obj->typecast, obj->name );
            else
                printf( "(%s)%s", obj->typecast, obj->name );
        }
        else
        {
            if ( *sep != 0 )
                printf( "%c%s", *sep, obj->name );
            else
                printf( "%s", obj->name );
        }
        if ( *sep == 0 )
            *sep = ';';
    }
}


typedef struct append_col_desc_name_ctx
{
    char * res;
    char sep;
} append_col_desc_name_ctx;

static void CC append_col_desc_name( void * item, void * data )
{
    if ( item != NULL && data != NULL )
    {
        col_desc * obj = item;
        append_col_desc_name_ctx * ctx = data;
        
        if ( obj->name != NULL )
        {
            char * s;
            /* sqlite3-special-behavior: %z means free the ptr after inserting into result */
            if ( ctx->sep != 0 )
                s = sqlite3_mprintf( "%z%c %s ", ctx->res, ctx->sep, obj->name );
            else
                s = sqlite3_mprintf( "%z%s", ctx->res, obj->name );
            ctx->res = s;
        }
        if ( ctx->sep == 0 )
            ctx->sep = ',';
    }
}

typedef struct col_desc_check_ctx
{
    const VNamelist * available;
    int not_found;
} col_desc_check_ctx;

static void CC check_col_desc_available( void * item, void * data )
{
    if ( item != NULL && data != NULL )
    {
        col_desc * obj = item;
        col_desc_check_ctx * ctx = data;

        if ( obj->name != NULL )
        {
            uint32_t found;
            rc_t rc = VNamelistIndexOf( ( VNamelist * )ctx->available, obj->name, &found );
            if ( rc != 0 )
                ctx->not_found++;
        }
        else
            ctx->not_found++;
    }
}

/* -------------------------------------------------------------------------------------- */
typedef struct col_inst
{
    const col_desc * desc;
    uint32_t idx;
    VTypedecl vtype;
    VTypedesc vdesc;
    int64_t first;
    uint64_t count;
} col_inst;

static void CC destroy_col_inst( void * item, void * data )
{
    if ( item != NULL )
    {
        col_desc * desc = item;
        sqlite3_free( desc );
    }
}

static col_inst * make_col_inst( const col_desc * desc, const VCursor * curs )
{
    col_inst * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        rc_t rc = 0;
        memset( res, 0, sizeof( *res ) );
        res->desc = desc;
        if ( desc->typecast != NULL )
            rc = VCursorAddColumn( curs, &res->idx, "(%s)%s", desc->typecast, desc->name );
        else
            rc = VCursorAddColumn( curs, &res->idx, "%s", desc->name );
        if ( rc != 0 )
        {
            sqlite3_free( res );
            res = NULL;
        }
    }
    return res;
}

static rc_t col_inst_post_open( col_inst * inst, const VCursor * curs )
{
    rc_t rc = VCursorDatatype( curs, inst->idx, &inst->vtype, &inst->vdesc );
    if ( rc == 0 )
        rc = VCursorIdRange( curs, inst->idx, &inst->first, &inst->count );
    return rc;
}

static char * print_bool_vector( const uint8_t * base, uint32_t count )
{
    size_t l = count * 4;
    char * res = sqlite3_malloc( l );
    if ( res != NULL )
    {
        uint32_t idx;
        rc_t rc = 0;
        char * dst = res;
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            size_t num_writ;
            uint8_t v = base[ idx ] > 0 ? 1 : 0;
            if ( idx == 0 )
                rc = string_printf( dst, l, &num_writ, "%d", v );
            else
                rc = string_printf( dst, l, &num_writ, ", %d", v );
            l -= num_writ;
            dst += num_writ;
        }
        if ( rc == 0 )
            *dst = 0;
        else
        {
            sqlite3_free( res );
            res = NULL;
        }
    }
    return res;
}


/* we are printing booleans ( booleans are always 8 bit )*/
static void col_inst_bool( col_inst * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->idx, &elem_bits, &base, &boff, &row_len );
    if ( rc == 0 && row_len > 0 )
    {
        if ( row_len == 1 )
        {
            switch( elem_bits )
            {
                case  8  : sqlite3_result_int( ctx, *( (  uint8_t * )base ) ); break;
                case 16  : sqlite3_result_int( ctx, *( ( uint16_t * )base ) ); break;
                case 32  : sqlite3_result_int( ctx, *( ( uint32_t * )base ) ); break;
                case 64  : sqlite3_result_int64( ctx, *( ( uint64_t * )base ) ); break;
                default  : sqlite3_result_int( ctx, *( (  uint8_t * )base ) ); break;
            }
        }
        else
        {
            /* make a transient text from it ( booleans are always 8 bit ) */
            char * txt = print_bool_vector( base, row_len );
            if ( txt != NULL )
            {
                sqlite3_result_text( ctx, txt, -1, SQLITE_TRANSIENT );
                sqlite3_free( txt );
            }
            else
                sqlite3_result_null( ctx );
        }
    }
    else
        sqlite3_result_null( ctx );
}


#define PRINT_VECTOR( T, factor, fmt1, fmt2 ) \
static char * print_##T##_vec( const T * base, uint32_t count ) \
{ \
    size_t l = count * factor; \
    char * res = sqlite3_malloc( l ); \
    if ( res != NULL ) \
    { \
        uint32_t idx; \
        rc_t rc = 0; \
        char * dst = res; \
        for ( idx = 0; rc == 0 && idx < count; ++idx ) \
        { \
            size_t num_writ; \
            if ( idx == 0 ) \
                rc = string_printf( dst, l, &num_writ, fmt1, base[ idx ] ); \
            else \
                rc = string_printf( dst, l, &num_writ, fmt2, base[ idx ] ); \
            l -= num_writ; \
            dst += num_writ; \
        } \
        if ( rc == 0 ) \
            *dst = 0; \
        else \
        { \
            sqlite3_free( res ); \
            res = NULL; \
        } \
    } \
    return res; \
} \


PRINT_VECTOR( uint8_t, 5, "%u", ", %u" )
/* static char * print_uint8_t_vec( const uint8_t * base, uint32_t count ) */

PRINT_VECTOR( uint16_t, 7, "%u", ", %u" )
/* static char * print_uint16_t_ve( const uint16_t * base, uint32_t count ) */

PRINT_VECTOR( uint32_t, 12, "%u", ", %u" )
/* static char * print_uint32_t_ve( const uint32_t * base, uint32_t count ) */

PRINT_VECTOR( uint64_t, 22, "%lu", ", %lu" )
/* static char * print_uint64_t_ve( const uint64_t * base, uint32_t count ) */


/* we are printing unsigned integers */
static void col_inst_Uint( col_inst * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->idx, &elem_bits, &base, &boff, &row_len );
    if ( rc == 0 && row_len > 0 )
    {
        if ( row_len == 1 )
        {
            switch( elem_bits )
            {
                case  8  : sqlite3_result_int( ctx, *( (  uint8_t * )base ) ); break;
                case 16  : sqlite3_result_int( ctx, *( ( uint16_t * )base ) ); break;
                case 32  : sqlite3_result_int( ctx, *( ( uint32_t * )base ) ); break;
                case 64  : sqlite3_result_int64( ctx, *( ( uint64_t * )base ) ); break;
                default  : sqlite3_result_int( ctx, *( (  uint8_t * )base ) ); break;
            }
        }
        else
        {
            char * txt = NULL;
            switch( elem_bits )
            {
                case 8  : txt = print_uint8_t_vec( base, row_len ); break;
                case 16 : txt = print_uint16_t_vec( base, row_len ); break;
                case 32 : txt = print_uint32_t_vec( base, row_len ); break;
                case 64 : txt = print_uint64_t_vec( base, row_len ); break;
                
            }
            if ( txt != NULL )
            {
                sqlite3_result_text( ctx, txt, -1, SQLITE_TRANSIENT );
                sqlite3_free( txt );
            }
            else
                sqlite3_result_null( ctx );
        }
    }
    else
        sqlite3_result_null( ctx );
}

PRINT_VECTOR( int8_t, 6, "%d", ", %d" )
/* static char * print_int8_t_vec( const int8_t * base, uint32_t count ) */

PRINT_VECTOR( int16_t, 8, "%d", ", %d" )
/* static char * print_int16_t_vec( const int16_t * base, uint32_t count ) */

PRINT_VECTOR( int32_t, 13, "%d", ", %d" )
/* static char * print_int32_t_vec( const int32_t * base, uint32_t count ) */

PRINT_VECTOR( int64_t, 23, "%ld", ", %ld" )
/* static char * print_int64_t_vec( const int64_t * base, uint32_t count ) */


/* we are printing signed integers */
static void col_inst_Int( col_inst * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->idx, &elem_bits, &base, &boff, &row_len );
    if ( rc == 0 && row_len > 0 )
    {
        if ( row_len == 1 )
        {
            switch( elem_bits )
            {
                case  8  : sqlite3_result_int( ctx, *( (  int8_t * )base ) ); break;
                case 16  : sqlite3_result_int( ctx, *( ( int16_t * )base ) ); break;
                case 32  : sqlite3_result_int( ctx, *( ( int32_t * )base ) ); break;
                case 64  : sqlite3_result_int64( ctx, *( ( int64_t * )base ) ); break;
                default  : sqlite3_result_int( ctx, *( (  int8_t * )base ) ); break;
            }
        }
        else
        {
            char * txt = NULL;
            switch( elem_bits )
            {
                case 8  : txt = print_int8_t_vec( base, row_len ); break;
                case 16 : txt = print_int16_t_vec( base, row_len ); break;
                case 32 : txt = print_int32_t_vec( base, row_len ); break;
                case 64 : txt = print_int64_t_vec( base, row_len ); break;
            }
            if ( txt != NULL )
            {
                sqlite3_result_text( ctx, txt, -1, SQLITE_TRANSIENT );
                sqlite3_free( txt );
            }
            else
                sqlite3_result_null( ctx );
        }
    }
    else
        sqlite3_result_null( ctx );
}

#define MAX_CHARS_FOR_DOUBLE 26
#define BITSIZE_OF_FLOAT ( sizeof(float) * 8 )
#define BITSIZE_OF_DOUBLE ( sizeof(double) * 8 )

PRINT_VECTOR( float, MAX_CHARS_FOR_DOUBLE, "%f", ", %f" )
/* static char * print_float_vec( const float * base, uint32_t count ) */

PRINT_VECTOR( double, MAX_CHARS_FOR_DOUBLE, "%f", ", %f" )
/* static char * print_double_vec( const double * base, uint32_t count ) */

/* we are printing signed floats */
static void col_inst_Float( col_inst * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->idx, &elem_bits, &base, &boff, &row_len );
    if ( rc == 0 && row_len > 0 )
    {
        if ( row_len == 1 )
        {
            if ( elem_bits == BITSIZE_OF_FLOAT )
            {
                const double v = *( ( const float * )base );
                sqlite3_result_double( ctx, v );
            }
            else if ( elem_bits == BITSIZE_OF_DOUBLE )
                sqlite3_result_double( ctx, *( ( const double * )base ) );
            else
                sqlite3_result_null( ctx );
        }
        else
        {
            char * txt = NULL;
            if ( elem_bits == BITSIZE_OF_FLOAT )
                txt = print_float_vec( base, row_len );
            else if ( elem_bits == BITSIZE_OF_DOUBLE ) 
                txt = print_double_vec( base, row_len );
            if ( txt != NULL )
            {
                sqlite3_result_text( ctx, txt, -1, SQLITE_TRANSIENT );
                sqlite3_free( txt );
            }
            else
                sqlite3_result_null( ctx );
        }
    }
    else
        sqlite3_result_null( ctx );
}

#undef PRINT_VECTOR

/* we are printing text */
static void col_inst_Ascii( col_inst * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->idx, &elem_bits, &base, &boff, &row_len );
    if ( rc == 0 && row_len > 0 )
        sqlite3_result_text( ctx, (char *)base, row_len, SQLITE_TRANSIENT );
    else
        sqlite3_result_null( ctx );
}

static void col_inst_cell( col_inst * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    switch( inst->vdesc.domain )
    {
        case vtdBool    : col_inst_bool( inst, curs, ctx, row_id ); break;
        case vtdUint    : col_inst_Uint( inst, curs, ctx, row_id ); break;
        case vtdInt     : col_inst_Int( inst, curs, ctx, row_id ); break;
        case vtdFloat   : col_inst_Float( inst, curs, ctx, row_id ); break;
        case vtdAscii   :
        case vtdUnicode : col_inst_Ascii( inst, curs, ctx, row_id ); break;
        default : sqlite3_result_null( ctx );
    }
}
    
/* -------------------------------------------------------------------------------------- */
typedef struct col_desc_list
{
    Vector v;
} col_desc_list;


static void destroy_col_desc_list( col_desc_list * l )
{
    if ( l != NULL )
    {
        VectorWhack( &( l->v ), destroy_col_desc, NULL );
        sqlite3_free( l );
    }
}

static col_desc_list * make_col_desc_list( const char * decl )
{
    col_desc_list * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        memset( res, 0, sizeof( *res ) );
        {
            VNamelist * l;
            rc_t rc = VNamelistFromStr( &l, decl, ';' );
            if ( rc == 0 )
            {
                uint32_t count, idx;
                rc = VNameListCount( l, &count );
                if ( rc == 0 )
                {
                    const char * s;
                    for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        rc = VNameListGet( l, idx, &s );
                        if ( rc == 0 )
                        {
                            col_desc * cd = make_col_desc( s );
                            if ( cd != NULL )
                            {
                                rc = VectorAppend( &res->v, NULL, cd );
                                if ( rc != 0 )
                                    destroy_col_desc( cd, NULL );
                            }
                        }
                    }
                }
                VNamelistRelease( l );
            }
            if ( rc != 0 )
            {
                destroy_col_desc_list( res );
                res = NULL;
            }
        }
    }
    return res;
}

static void print_col_desc_list( col_desc_list * obj )
{
    if ( obj != NULL )
    {
        char sep = 0;
        VectorForEach( &( obj->v ), false, print_col_desc, &sep );
    }
}

static char * make_create_table_stm( col_desc_list * obj, const char * tbl_name  )
{
    char * res = NULL;
    if ( obj != NULL && tbl_name != NULL )
    {
        append_col_desc_name_ctx ctx;
        ctx.res = sqlite3_mprintf( "CREATE TABLE %s ( ", tbl_name );
        ctx.sep = 0;
        VectorForEach( &( obj->v ), false, append_col_desc_name, &ctx );
        res = sqlite3_mprintf( "%z);", ctx.res );
    }
    return res;
}

static rc_t col_desc_list_check( const col_desc_list * obj, const VNamelist * available  )
{
    rc_t rc = -1;
    if ( obj != NULL && available != NULL )
    {
        col_desc_check_ctx ctx;
        ctx.available = available;
        ctx.not_found = 0;
        VectorForEach( &( obj->v ), false, check_col_desc_available, &ctx );
        if ( ctx.not_found == 0 ) rc = 0;
    }
    return rc;
}

static rc_t col_desc_list_make_instances( const col_desc_list * obj, Vector * v, const VCursor * curs )
{
    rc_t rc = 0;
    uint32_t count, idx;
    for ( idx = 0, count = VectorLength( &( obj->v ) ); rc == 0 && idx < count; ++idx )
    {
        col_desc * desc = VectorGet( &( obj->v ), idx );
        if ( desc != NULL )
        {
            col_inst * inst = make_col_inst( desc, curs );
            if ( inst != NULL )
                rc = VectorAppend( v, NULL, inst );
            else
                rc = -1;
        }
        else
            rc = -1;
    }
    return rc;
}

/* -------------------------------------------------------------------------------------- */
typedef struct col_inst_list
{
    Vector v;
} col_inst_list;


static void destroy_col_inst_list( col_inst_list * l )
{
    if ( l != NULL )
    {
        VectorWhack( &( l->v ), destroy_col_inst, NULL );
        sqlite3_free( l );
    }
}

/* this adds columns to the cursor, opens the cursor, takes a second round to extract types and row-ranges */
static col_inst_list * make_col_inst_list( const col_desc_list * desc_list, const VCursor * curs )
{
    col_inst_list * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        memset( res, 0, sizeof( *res ) );
        {
            rc_t rc = col_desc_list_make_instances( desc_list, &res->v, curs );
            if ( rc == 0 )
            {
                rc = VCursorOpen( curs );
                if ( rc == 0 )
                {
                    uint32_t count, idx;
                    for ( idx = 0, count = VectorLength( &( res->v ) ); rc == 0 && idx < count; ++idx )
                    {
                        col_inst * inst = VectorGet( &( res->v ), idx );
                        if ( inst != NULL )
                            rc = col_inst_post_open( inst, curs );
                        else
                            rc = -1;
                    }
                }
            }
            if ( rc != 0 )
            {
                destroy_col_inst_list( res );
                res = NULL;
            }
        }
    }
    return res;
}

static void col_inst_list_get_row_range( col_inst_list * l, int64_t * first, uint64_t * count )
{
    uint32_t v_count, v_idx;
    for ( v_idx = 0, v_count = VectorLength( &( l->v ) ); v_idx < v_count; ++v_idx )
    {
        col_inst * inst = VectorGet( &( l->v ), v_idx );
        if ( inst != NULL )
        {
            if ( inst->first < *first ) *first = inst->first;
            if ( inst->count > *count ) *count = inst->count;
        }
    }
}

/* produce output for a cell */
static void col_inst_list_cell( col_inst_list * l, const VCursor * curs,
    sqlite3_context * ctx, int column_id, int64_t row_id )
{
    col_inst * inst = VectorGet( &( l->v ), column_id );
    if ( inst != NULL )
        col_inst_cell( inst, curs, ctx, row_id );
    else
        sqlite3_result_null( ctx );
}

/* -------------------------------------------------------------------------------------- */

typedef struct obj_desc
{
    const char * accession;
    const char * table_name;
    const char * row_range_str;
    struct num_gen * row_range;
    col_desc_list * cols;
    int verbosity;
} obj_desc;


static void destroy_obj_desc( obj_desc * desc )
{
    if ( desc != NULL )
    {
        if ( desc->accession != NULL ) sqlite3_free( ( void * )desc->accession );
        if ( desc->table_name != NULL ) sqlite3_free( ( void * )desc->table_name );
        if ( desc->row_range_str != NULL ) sqlite3_free( ( void * )desc->row_range_str );
        if ( desc->row_range != NULL ) num_gen_destroy( desc->row_range );
        destroy_col_desc_list( desc->cols );
        sqlite3_free( desc );
    }
}


static void obj_desc_print( obj_desc * desc )
{
    printf( "---accession = %s\n", desc->accession != NULL ? desc->accession : "None" );
    printf( "---table     = %s\n", desc->table_name != NULL ? desc->table_name : "None" );
    printf( "---rows      = %s\n", desc->row_range_str != NULL ? desc->row_range_str : "None" ); 
    printf( "---columns   = " ); print_col_desc_list( desc->cols ); printf( "\n" );
}


void trim_ws( String * S )
{
    while( S->addr[ 0 ] == ' ' || S->addr[ 0 ] == '\t' )
	{
		S->addr++;
		S->len--;
		S->size--;
	}
	
    while( S->len > 0 && ( S->addr[ S->len - 1 ] == ' ' || S->addr[ S->len - 1 ] == '\t' ) )
	{
		S->len--;
		S->size--;
	}
}


static void obj_desc_parse_arg2( obj_desc * desc, const char * name, const char * value )
{
	String S_name, S_value, S_template;
	const char * ACC_ARG = "acc";
	const char * TBL_ARG = "table";
	const char * COL_ARG = "columns";
	const char * ROW_ARG = "rows";
	const char * VERB_ARG = "verbose";
	
	StringInitCString( &S_name, name );
	trim_ws( &S_name );
	StringInitCString( &S_value, value );
	trim_ws( &S_value );
	
	StringInitCString( &S_template, ACC_ARG );
	if ( StringCaseEqual( &S_template, &S_name ) )
		desc->accession = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
	else
	{
		StringInitCString( &S_template, TBL_ARG );
		if ( StringCaseEqual( &S_template, &S_name ) )
			desc->table_name = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
		else
		{
			StringInitCString( &S_template, COL_ARG );
			if ( StringCaseEqual( &S_template, &S_name ) )
			{
				const char * s = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
				desc->cols = make_col_desc_list( s );
				sqlite3_free( ( void * )s );
			}
			else
			{
				StringInitCString( &S_template, ROW_ARG );
				if ( StringCaseEqual( &S_template, &S_name ) )
				{
					desc->row_range_str = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
					num_gen_parse( desc->row_range, desc->row_range_str );
				}
				else
				{
					StringInitCString( &S_template, VERB_ARG );
					if ( StringCaseEqual( &S_template, &S_name ) )
						desc->verbosity = StringToU64( &S_value, NULL );
					else
					{
						printf( "unknown argument '%.*s' = '%.*s'\n", S_name.len, S_name.addr, S_value.len, S_value.addr );
					}
				}
			}
		}
	}
}

static obj_desc * make_obj_desc( int argc, const char * const * argv )
{
    obj_desc * desc = sqlite3_malloc( sizeof( * desc ) );
    if ( desc != NULL )
    {
        memset( desc, 0, sizeof( *desc ) );
        num_gen_make( &desc->row_range );
        if ( argc > 3 )
        {
            int i;
            for ( i = 3; i < argc; i++ )
			{
				VNamelist * parts;
				rc_t rc = VNamelistFromStr( &parts, argv[ i ], '=' );
				if ( rc == 0 )
				{
					uint32_t count;
					rc = VNameListCount( parts, &count );
					if ( rc == 0 && count == 2 )
					{
						const char * arg_name = NULL;
						rc = VNameListGet( parts, 0, &arg_name );
						if ( rc == 0 && arg_name != NULL )
						{
							const char * arg_value = NULL;
							rc = VNameListGet( parts, 1, &arg_value );
							if ( rc == 0 && arg_value != NULL )
								obj_desc_parse_arg2( desc, arg_name, arg_value );
						}
					}
					VNamelistRelease( parts );
				}
			}
        }
        if ( desc->verbosity > 0 )
            obj_desc_print( desc );
    }
    return desc;

}

               
/* -------------------------------------------------------------------------------------- */
typedef struct vdb_cursor
{
    sqlite3_vtab cursor;            /* Base class.  Must be first */
    const struct num_gen_iter * row_iter;
    obj_desc * desc;                /* cursor does not own this! */
    col_inst_list * col_inst;       /* we do own this one! */
    const VCursor * curs;
    int64_t current_row;    
    bool eof;
} vdb_cursor;


/* destroy the cursor, ---> release the VDB_Cursor */
static int destroy_vdb_cursor( vdb_cursor * c )
{
    if ( c->desc->verbosity > 1 )
        printf( "---sqlite3_vdb_Close()\n" );
    if ( c->row_iter != NULL ) num_gen_iterator_destroy( c->row_iter );
    if ( c->col_inst != NULL ) destroy_col_inst_list( c->col_inst );
    if ( c->curs != NULL ) VCursorRelease( c->curs );
    sqlite3_free( c );
    return SQLITE_OK;
}

/* create a cursor from the obj-description ( mostly it's columns ) */
static vdb_cursor * make_vdb_cursor( obj_desc * desc, const VTable * tbl )
{
    vdb_cursor * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        rc_t rc;
        memset( res, 0, sizeof( *res ) );
        res->desc = desc;
        
        /* we first have to make column-instances, before we can adjust the row-ranges */
        /*rc = VTableCreateCursorRead( tbl, &res->curs ); */
		rc = VTableCreateCachedCursorRead( tbl, &res->curs, 1024 * 1024 * 32 );
        if ( rc == 0 )
        {
            /* this adds the columns to the cursor, opens the cursor, gets types, extracts row-range */
            res->col_inst = make_col_inst_list( desc->cols, res->curs );
            if ( res->col_inst == NULL )
                rc = -1;
        }
        if ( rc == 0 )
        {
            int64_t  first = 0x7FFFFFFFFFFFFFFF;
            uint64_t count = 0;
            col_inst_list_get_row_range( res->col_inst, &first, &count );
			if ( first == 0x7FFFFFFFFFFFFFFF )
				first = 0;
            if ( num_gen_empty( desc->row_range ) )
                rc = num_gen_add( desc->row_range, first, count );
            else
                rc = num_gen_trim( desc->row_range, first, count );
        }
        if ( rc == 0 )
        {
            rc = num_gen_iterator_make( desc->row_range, &res->row_iter );
            if ( rc == 0 )
                res->eof = !num_gen_iterator_next( res->row_iter, &res->current_row, NULL );
            else
                res->eof = true;
        } 
		
        if ( res->eof )
            rc = -1;

        if ( rc != 0 )
        {
            destroy_vdb_cursor( res );
            res = NULL;
        }
    }
    return res;
}

/* filter by a given index ---> vdb may support that in the future */
static int vdb_cursor_filter( vdb_cursor * c, int idxNum, const char *idxStr,
                              int argc, sqlite3_value **argv )
{
    if ( c->desc->verbosity > 2 )
        printf( "---sqlite3_vdb_Filter()\n" );
    return SQLITE_OK;
}

/* advance to the next row ---> num_gen_iterator_next() */
static int vdb_cursor_next( vdb_cursor * c )
{
    if ( c->desc->verbosity > 2 )
        printf( "---sqlite3_vdb_Next()\n" );
    c->eof = !num_gen_iterator_next( c->row_iter, &c->current_row, NULL );
    return SQLITE_OK;
}

/* are we done? ---> inspect c->eof */
static int vdb_cursor_eof( vdb_cursor * c )
{
    if ( c->desc->verbosity > 2 )
        printf( "---sqlite3_vdb_Eof()\n" );
    if ( c->eof )
        return SQLITE_ERROR; /* anything else than SQLITE_OK means EOF */
    return SQLITE_OK;
}

/* print a column ---> get a cell from vdb and translate it into text */
static int vdb_cursor_column( vdb_cursor * c, sqlite3_context * ctx, int column_id )
{
    if ( c->desc->verbosity > 2 )
        printf( "---sqlite3_vdb_Column( %d )\n", column_id );
    col_inst_list_cell( c->col_inst, c->curs, ctx, column_id, c->current_row );
    return SQLITE_OK;
}

/* return the current row-id ---> return c->current_row */
static int vdb_cursor_row_id( vdb_cursor * c, sqlite_int64 * pRowid )
{
    if ( c->desc->verbosity > 2 )
        printf( "---sqlite3_vdb_Rowid()\n" );
    if ( pRowid != NULL )
        *pRowid = c->current_row;
    return SQLITE_OK;
}


/* -------------------------------------------------------------------------------------- */
typedef struct vdb_obj
{
    sqlite3_vtab base;              /* Base class.  Must be first */
    const VDBManager * mgr;         /* the vdb-manager */
    const VDatabase *db;            /* the database to be used */
    const VTable *tbl;              /* the table to be used */
    obj_desc * desc;                /* description of the object to be iterated over */
} vdb_obj;


/* destroy the whole connection database, table, manager etc. */
static int destroy_vdb_obj( vdb_obj * obj )
{
    if ( obj->desc->verbosity > 1 )
        printf( "---sqlite3_vdb_Disconnect()\n" );
    
    if ( obj->mgr != NULL ) VDBManagerRelease( obj->mgr );
    if ( obj->tbl != NULL ) VTableRelease( obj->tbl );
    if ( obj->db != NULL ) VDatabaseRelease( obj->db );
    if ( obj->desc != NULL ) destroy_obj_desc( obj->desc );
    
    sqlite3_free( obj );
    return SQLITE_OK;    
}

/* check if all requested columns are available */
static rc_t vdb_obj_common_table_handler( vdb_obj * obj )
{
    struct KNamelist * readable_columns;
    rc_t rc = VTableListReadableColumns( obj->tbl, &readable_columns );
    if ( rc == 0 )
    {
		VNamelist * available;
		rc = VNamelist_from_KNamelist( &available, readable_columns );
		if ( rc == 0 )
		{
			rc = col_desc_list_check( obj->desc->cols, available );
			VNamelistRelease( available );
		}
		KNamelistRelease( readable_columns );
	}
    return rc;
}


static rc_t vdb_obj_get_table_names( const VDatabase * db, VNamelist ** list )
{
	rc_t rc = VNamelistMake( list, 5 );
	if ( rc == 0 )
	{
		KNamelist * names;
		rc_t rc1 = VDatabaseListTbl( db, &names );
		if ( rc1 == 0 )
		{
			uint32_t idx, count;
			rc1 = KNamelistCount( names, &count );
			for ( idx = 0; rc1 == 0 && idx < count; ++idx )
			{
				const char * s = NULL;
				rc1 = KNamelistGet( names, idx, &s );
				if ( rc1 == 0 && s != NULL )
					rc1 = VNamelistAppend( *list, s );
			}
			KNamelistRelease( names );
		}
	}
	return rc;
}

static rc_t vdb_obj_open_db_without_table_name( vdb_obj * obj )
{
	VNamelist * list;
	rc_t rc = vdb_obj_get_table_names( obj->db, &list );
	if ( rc == 0 )
	{
		int32_t idx;
		rc = VNamelistContainsStr( list, "SEQUENCE", &idx );
		if ( rc == 0 && idx >= 0 )
		{
			/* we have a SEQUENCE-table */
			rc = VDatabaseOpenTableRead( obj->db, &obj->tbl, "SEQUENCE" );
			if ( rc == 0 )
				obj->desc->table_name = sqlite3_mprintf( "SEQUENCE" );
		}
		else
		{
			/* we pick the first one */
			const char * s = NULL;
			rc = VNameListGet( list, 0, &s );
			if ( rc == 0 )
			{
				obj->desc->table_name = sqlite3_mprintf( "%s", s );
				rc = VDatabaseOpenTableRead( obj->db, &obj->tbl, "%s", obj->desc->table_name );	
			}
		}
		VNamelistRelease( list );
	}
	return rc;
}


static bool list_contains_value_starting_with( const VNamelist * list, const String * value, String * found )
{
    bool res = false;
    uint32_t i, count;
    rc_t rc = VNameListCount( list, &count );
	for ( i = 0; i < count && rc == 0 && !res; ++i )
	{
		const char *s = NULL;
		rc = VNameListGet( list, i, &s );
		if ( rc == 0 && s != NULL )
		{
			String item;
			StringInitCString( &item, s );
			if ( value->len <= item.len )
			{
				item.len = value->len;
				item.size = value->size;
				res = ( StringCompare ( &item, value ) == 0 );
				if ( res )
					StringInitCString( found, s );
			}
		}
	}
    return res;
}

static rc_t vdb_obj_open_db_with_mismatched_table_name( vdb_obj * obj )
{
	VNamelist * list;
	rc_t rc = vdb_obj_get_table_names( obj->db, &list );
	if ( rc == 0 )
	{
		String to_look_for, found;
		StringInitCString( &to_look_for, obj->desc->table_name );
		if ( list_contains_value_starting_with( list, &to_look_for, &found ) )
		{
			obj->desc->table_name = sqlite3_mprintf( "%.*s", found.len, found.addr );
			rc = VDatabaseOpenTableRead( obj->db, &obj->tbl, "%s", obj->desc->table_name );
		}
		else
			rc = RC( rcApp, rcArgv, rcAccessing, rcSelf, rcInvalid );
		VNamelistRelease( list );
	}
	return rc;
}

/* the requested object is a database */
static rc_t vdb_obj_open_db( vdb_obj * obj )
{
    rc_t rc = VDBManagerOpenDBRead( obj->mgr, &obj->db, NULL, "%s", obj->desc->accession );
    if ( rc == 0 )
    {
		if ( obj->desc->table_name == NULL )
			rc = vdb_obj_open_db_without_table_name( obj );
		else
		{
			/* if the table-name does not fit, see if it is a shortened version of the existing ones... */
			rc = VDatabaseOpenTableRead( obj->db, &obj->tbl, "%s", obj->desc->table_name );
			if ( rc != 0 )
				rc = vdb_obj_open_db_with_mismatched_table_name( obj );
		}
		
        if ( rc == 0 )
            rc = vdb_obj_common_table_handler( obj );
    }
    return rc;
}

/* the requested object is a table */
static rc_t vdb_obj_open_tbl( vdb_obj * obj )
{
    rc_t rc = VDBManagerOpenTableRead( obj->mgr, &obj->tbl, NULL, "%s", obj->desc->accession );
    if ( rc == 0 )
        rc = vdb_obj_common_table_handler( obj );
    return rc;
}


/* make a database/table object, have a look if it exists, and has the columns we are asking for etc.  */
static vdb_obj * make_vdb_obj( int argc, const char * const * argv )
{
    vdb_obj * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        memset( res, 0, sizeof( *res ) );
        res->desc = make_obj_desc( argc, argv );
        if ( res->desc == NULL )
        {
            destroy_vdb_obj( res );
            res = NULL;
        }
        else
        {
            rc_t rc = VDBManagerMakeRead( &( res->mgr ), NULL );
            if ( rc == 0 )
            {
                /* types defined in <kdb/manager.h> !!! */
                int pt = ( VDBManagerPathType( res->mgr, "%s", res->desc->accession ) & ~ kptAlias );
                switch ( pt )
                {
                    case kptDatabase      : rc = vdb_obj_open_db( res ); break;
                    case kptTable         : 
                    case kptPrereleaseTbl : rc = vdb_obj_open_tbl( res ); break;
                    default : rc = -1; break;
                }
            }
            if ( rc != 0 )
            {
                destroy_vdb_obj( res );
                res = NULL;
            }
        }
    }
    return res;
}

/* query what index can be used ---> maybe vdb supports that in the future */
static int vdb_obj_BestIndex( vdb_obj * obj, sqlite3_index_info * pIdxInfo )
{
    if ( obj->desc->verbosity > 2 )
        printf( "---sqlite3_vdb_BestIndex()\n" );
    if ( pIdxInfo != NULL )
        pIdxInfo->estimatedCost = 1000000;
    return SQLITE_OK;
}

/* take the database/table-obj and open a cursor on it */
static int vdb_obj_open( vdb_obj * obj, sqlite3_vtab_cursor ** ppCursor )
{
    if ( obj->desc->verbosity > 1 )
        printf( "---sqlite3_vdb_Open()\n" );
    vdb_cursor * c = make_vdb_cursor( obj->desc, obj->tbl );
    if ( c != NULL )
    {
        *ppCursor = ( sqlite3_vtab_cursor * )c;
        return SQLITE_OK;
    }
    return SQLITE_ERROR;
}


/* -------------------------------------------------------------------------------------- */
/* the common code for xxx_Create() and xxx_Connect */
static int sqlite3_vdb_CC( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                           sqlite3_vtab **ppVtab, char **pzErr, const char * msg )
{
    vdb_obj * obj = make_vdb_obj( argc, argv );
    if ( obj == NULL )
        return SQLITE_ERROR;
        
    *ppVtab = ( sqlite3_vtab * )obj;

    if ( obj->desc->verbosity > 1 )
        printf( msg );
    
    {
        const char * stm = make_create_table_stm( obj->desc->cols, "x" );
        if ( obj->desc->verbosity > 1 )
            printf( "stm = %s\n", stm );
        return sqlite3_declare_vtab( db, stm );    
    }
}


/* -------------------------------------------------------------------------------------- */
static int sqlite3_vdb_Create( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                               sqlite3_vtab **ppVtab, char **pzErr )
{
    return sqlite3_vdb_CC( db, pAux, argc, argv, ppVtab, pzErr, "---sqlite3_vdb_Create()\n" );
}

static int sqlite3_vdb_Connect( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                                sqlite3_vtab **ppVtab, char **pzErr )
{
    return sqlite3_vdb_CC( db, pAux, argc, argv, ppVtab, pzErr, "---sqlite3_vdb_Connect()\n" );
}

static int sqlite3_vdb_BestIndex( sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo )
{
    if ( tab != NULL )
        return vdb_obj_BestIndex( ( vdb_obj * )tab, pIdxInfo );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Disconnect( sqlite3_vtab *tab )
{
    if ( tab != NULL )
        return destroy_vdb_obj( ( vdb_obj * )tab );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Open( sqlite3_vtab *tab, sqlite3_vtab_cursor **ppCursor )
{
    if ( tab != NULL )
        return vdb_obj_open( ( vdb_obj * ) tab, ppCursor );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Close( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        destroy_vdb_cursor( ( vdb_cursor * )cur );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Filter( sqlite3_vtab_cursor *cur, int idxNum, const char *idxStr,
                               int argc, sqlite3_value **argv )
{
    if ( cur != NULL )
        return vdb_cursor_filter( ( vdb_cursor * )cur, idxNum, idxStr, argc, argv );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Next( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        return vdb_cursor_next( ( vdb_cursor * )cur );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Eof( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        return vdb_cursor_eof( ( vdb_cursor * )cur );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Column( sqlite3_vtab_cursor *cur, sqlite3_context * ctx, int column_id )
{
    if ( cur != NULL )
        return vdb_cursor_column( ( vdb_cursor * )cur, ctx, column_id );
    return SQLITE_ERROR;
}

static int sqlite3_vdb_Rowid( sqlite3_vtab_cursor *cur, sqlite_int64 * pRowid )
{
    if ( cur != NULL )
        return vdb_cursor_row_id( ( vdb_cursor * )cur, pRowid );
    return SQLITE_ERROR;
}

static sqlite3_module VDB_Module =
{
  0,                            /* iVersion */
  sqlite3_vdb_Create,           /* xCreate */
  sqlite3_vdb_Connect,          /* xConnect */
  sqlite3_vdb_BestIndex,        /* xBestIndex */
  sqlite3_vdb_Disconnect,       /* xDisconnect */
  sqlite3_vdb_Disconnect,       /* xDestroy */
  sqlite3_vdb_Open,             /* xOpen - open a cursor */
  sqlite3_vdb_Close,            /* xClose - close a cursor */
  sqlite3_vdb_Filter,           /* xFilter - configure scan constraints */
  sqlite3_vdb_Next,             /* xNext - advance a cursor */
  sqlite3_vdb_Eof,              /* xEof - check for end of scan */
  sqlite3_vdb_Column,           /* xColumn - read data */
  sqlite3_vdb_Rowid,            /* xRowid - read data */
  NULL,                         /* xUpdate */
  NULL,                         /* xBegin */
  NULL,                         /* xSync */
  NULL,                         /* xCommit */
  NULL,                         /* xRollback */
  NULL,                         /* xFindMethod */
  NULL,                         /* xRename */
};


#ifdef _WIN32
__declspec( dllexport )
#endif


/* 
** This routine is called when the extension is loaded.  The new
** CSV virtual table module is registered with the calling database
** connection.
*/
int sqlite3_sqlitevdb_init( sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi )
{
#ifndef SQLITE_OMIT_VIRTUALTABLE
    SQLITE_EXTENSION_INIT2( pApi );
    return sqlite3_create_module( db, "vdb", &VDB_Module, NULL );
#else
    return SQLITE_OK;
#endif
}
