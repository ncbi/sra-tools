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

#include "sqlite3ext.h"

SQLITE_EXTENSION_INIT1

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

#include <stdio.h> /* because of printf( ) for verbosity in testing... */

#include <klib/num-gen.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/text.h>
#include <klib/printf.h>

#include <kdb/manager.h> /* because path-types are defined there! */
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>

#include <kfc/xcdefs.h>
#include <kfc/except.h>
#include <kfc/ctx.h>
#include <kfc/rsrc.h>

#include <../libs/ngs/NGS_ReadCollection.h>
#include <../libs/ngs/NGS_Read.h>
#include <../libs/ngs/NGS_Alignment.h>
#include <../libs/ngs/NGS_String.h>

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
typedef struct column_description
{
    const char * typecast;
    const char * name;
} column_description;


static void CC destroy_column_description( void * item, void * data )
{
    if ( item != NULL )
    {
        column_description * desc = item;
        if ( desc->typecast != NULL ) free( ( void * )desc->typecast );
        if ( desc->name != NULL ) free( ( void * )desc->name );
        free( desc );
    }
}

static column_description * make_column_description( const char * decl )
{
    column_description * res = malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        rc_t rc = 0;
        VNamelist * l;
        memset( res, 0, sizeof( *res ) );
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
                        res->name = string_dup( s, string_size( s ) );
                }
                else if ( count == 2 )
                {
                    rc = VNameListGet( l, 0, &s );
                    if ( rc == 0 )
                    {
                        if ( s[ 0 ] == '(' )
                        {
                            const char * src = &s[ 1 ];
                            res->typecast = string_dup( src, string_size( src ) );
                        }
                        else
                            res->typecast = string_dup( s, string_size( s ) );
                        rc = VNameListGet( l, 1, &s );
                        if ( rc == 0 )
                            res->name = string_dup( s, string_size( s ) );
                    }
                }
                else
                    rc = -1;
            }
            VNamelistRelease( l );
        }
        if ( rc != 0 || res->name == NULL )
        {
            destroy_column_description( res, NULL );
            res = NULL;
        }
    }
    return res;
}


static column_description * copy_column_description( const column_description * src )
{
    column_description * res = malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        memset( res, 0, sizeof( *res ) );
        res->name = string_dup( src->name, string_size( src->name ) );
        if ( src->typecast != NULL )
            res->typecast = string_dup( src->typecast, string_size( src->typecast ) );
    }
    return res;
}


/* -------------------------------------------------------------------------------------- */
typedef struct column_instance
{
    const column_description * desc;
    uint32_t vdb_cursor_idx;
    VTypedecl vtype;
    VTypedesc vdesc;
    int64_t first;
    uint64_t count;
} column_instance;

static void CC destroy_column_instance( void * item, void * data )
{
    if ( item != NULL )
    {
        column_instance * inst = item;
        destroy_column_description( ( column_description * )inst->desc, NULL );
        sqlite3_free( inst );
    }
}

static column_instance * make_column_instance( const column_description * desc, const VCursor * curs )
{
    column_instance * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        rc_t rc = 0;
        memset( res, 0, sizeof( *res ) );
        res->desc = copy_column_description( desc );
        if ( desc->typecast != NULL )
            rc = VCursorAddColumn( curs, &res->vdb_cursor_idx, "(%s)%s", desc->typecast, desc->name );
        else
            rc = VCursorAddColumn( curs, &res->vdb_cursor_idx, "%s", desc->name );
        if ( rc != 0 )
        {
            sqlite3_free( res );
            res = NULL;
        }
    }
    return res;
}

static rc_t column_instance_post_open( column_instance * inst, const VCursor * curs )
{
    rc_t rc = VCursorDatatype( curs, inst->vdb_cursor_idx, &inst->vtype, &inst->vdesc );
    if ( rc == 0 )
        rc = VCursorIdRange( curs, inst->vdb_cursor_idx, &inst->first, &inst->count );
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
static void col_inst_bool( column_instance * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->vdb_cursor_idx, &elem_bits, &base, &boff, &row_len );
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
    size_t l = ( count * factor ) + 10; \
    char * res = sqlite3_malloc( l ); \
    if ( res != NULL ) \
    { \
        uint32_t idx; \
        rc_t rc = 0; \
        char * dst = res; \
        dst[ 0 ] = '{' ; \
        dst[ 1 ] = '"' ; \
        dst[ 2 ] = 'a' ; \
        dst[ 3 ] = '"' ; \
        dst[ 4 ] = ':' ; \
        dst[ 5 ] = '[' ; \
        dst += 6; \
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
        { \
            *dst = ']'; \
            dst += 1; \
            *dst = '}'; \
            dst += 1; \
            *dst = 0; \
        } \
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
static void col_inst_Uint( column_instance * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->vdb_cursor_idx, &elem_bits, &base, &boff, &row_len );
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
static void col_inst_Int( column_instance * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->vdb_cursor_idx, &elem_bits, &base, &boff, &row_len );
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
static void col_inst_Float( column_instance * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->vdb_cursor_idx, &elem_bits, &base, &boff, &row_len );
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
static void col_inst_Ascii( column_instance * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
{
    uint32_t elem_bits, boff, row_len;
    const void * base;
    rc_t rc = VCursorCellDataDirect( curs, row_id, inst->vdb_cursor_idx, &elem_bits, &base, &boff, &row_len );
    if ( rc == 0 && row_len > 0 )
        sqlite3_result_text( ctx, (char *)base, row_len, SQLITE_TRANSIENT );
    else
        sqlite3_result_null( ctx );
}

static void col_inst_cell( column_instance * inst, const VCursor * curs, sqlite3_context * ctx, int64_t row_id )
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

static bool init_col_desc_list( Vector * dst, const String * decl )
{
    VNamelist * l;
    VectorInit( dst, 0, 10 );
    rc_t rc = VNamelistFromString( &l, decl, ';' );
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
                    column_description * desc = make_column_description( s );
                    if ( desc != NULL )
                    {
                        rc = VectorAppend( dst, NULL, desc );
                        if ( rc != 0 )
                            destroy_column_description( desc, NULL );
                    }
                }
            }
        }
        VNamelistRelease( l );
    }
    return ( rc == 0 );
}


static void print_col_desc_list( const Vector * desc_list )
{
    if ( desc_list != NULL )
    {
        uint32_t idx, count;    
        for ( idx = 0, count = VectorLength( desc_list ); idx < count; ++idx )
        {
            column_description * desc = VectorGet( desc_list, idx );
            if ( desc != NULL )
            {
                if ( desc->typecast != NULL )
                {
                    if ( idx == 0 )
                        printf( "(%s)%s", desc->typecast, desc->name );
                    else
                        printf( ";(%s)%s", desc->typecast, desc->name );
                }
                else
                {
                    if ( idx == 0 )
                        printf( "%s", desc->name );
                    else
                        printf( ";%s", desc->name );
                }
            }
        }
    }
}

static char * make_vdb_create_table_stm( const Vector * desc_list, const char * tbl_name  )
{
    char * res = NULL;
    if ( desc_list != NULL && tbl_name != NULL )
    {
        res = sqlite3_mprintf( "CREATE TABLE %s ( ", tbl_name );
        if ( res != NULL )
        {
            uint32_t idx, count;
            for ( idx = 0, count = VectorLength( desc_list ); idx < count; ++idx )
            {
                column_description * desc = VectorGet( desc_list, idx );
                if ( desc != NULL )
                {
                    /* sqlite3-special-behavior: %z means free the ptr after inserting into result */
                    if ( idx == 0 )
                        res = sqlite3_mprintf( "%z%s", res, desc->name );
                    else
                        res = sqlite3_mprintf( "%z, %s ", res, desc->name );    
                }
            }
            /* sqlite3-special-behavior: %z means free the ptr after inserting into result */
            res = sqlite3_mprintf( "%z );", res );
        }
    }
    return res;
}

static rc_t col_desc_list_check( Vector * desc_list, const VNamelist * available, const VNamelist * exclude )
{
    rc_t rc = -1;
    if ( desc_list != NULL && available != NULL )
    {
        uint32_t idx, count = VectorLength( desc_list );
        if ( count == 0 )
        {
            /* the user DID NOT give us a list of columns to use
                --> just use all available one's ( exclude the excluded one's ) */
            rc = VNameListCount( available, &count );
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char * s = NULL;
                rc = VNameListGet( available, idx, &s );
                if ( rc == 0 && s != NULL )
                {
                    bool excluded = false;
                    if ( exclude != NULL )
                    {
                        int32_t idx;
                        if ( 0 == VNamelistContainsStr( exclude, s, &idx ) )
                            excluded = ( idx >= 0 );
                    }
                    if ( !excluded )
                    {
                        column_description * desc = make_column_description( s );
                        if ( desc != NULL )
                        {
                            rc = VectorAppend( desc_list, NULL, desc );
                            if ( rc != 0 )
                                destroy_column_description( desc, NULL );
                        }
                    }
                }
            }
        }
        else
        {
            /* the user DID give us a list of columns to use --> check if they are available */
            uint32_t not_found = 0;
            for ( idx = 0; idx < count; ++idx )
            {
                column_description * desc = VectorGet( desc_list, idx );
                if ( desc != NULL )
                {
                    if ( desc->name != NULL )
                    {
                        uint32_t found;
                        if ( VNamelistIndexOf( ( VNamelist * )available, desc->name, &found ) != 0 )
                            not_found++;
                    }
                }
            }
            if ( not_found == 0 ) rc = 0;
        }
    }
    return rc;
}


static rc_t col_desc_list_make_instances( const Vector * desc_list, Vector * dst, const VCursor * curs )
{
    rc_t rc = 0;
    uint32_t count, idx;
    for ( idx = 0, count = VectorLength( desc_list ); rc == 0 && idx < count; ++idx )
    {
        column_description * desc = VectorGet( desc_list, idx );
        if ( desc != NULL )
        {
            column_instance * inst = make_column_instance( desc, curs );
            if ( inst != NULL )
                rc = VectorAppend( dst, NULL, inst );
            else
                rc = -1;
        }
        else
            rc = -1;
    }
    return rc;
}

/* -------------------------------------------------------------------------------------- */

/* this adds columns to the cursor, opens the cursor, takes a second round to extract types and row-ranges */
static rc_t init_col_inst_list( Vector * dst, const Vector * desc_list, const VCursor * curs )
{
    rc_t rc = 0;
    VectorInit( dst, 0, VectorLength( desc_list ) );
    rc = col_desc_list_make_instances( desc_list, dst, curs );
    if ( rc == 0 )
    {
        rc = VCursorOpen( curs );
        if ( rc == 0 )
        {
            uint32_t count, idx;
            for ( idx = 0, count = VectorLength( dst ); rc == 0 && idx < count; ++idx )
            {
                column_instance * inst = VectorGet( dst, idx );
                if ( inst != NULL )
                    rc = column_instance_post_open( inst, curs );
                else
                    rc = -1;
            }
        }
    }
    return rc;
}

static void col_inst_list_get_row_range( const Vector * inst_list, int64_t * first, uint64_t * count )
{
    uint32_t v_count, v_idx;
    for ( v_idx = 0, v_count = VectorLength( inst_list ); v_idx < v_count; ++v_idx )
    {
        column_instance * inst = VectorGet( inst_list, v_idx );
        if ( inst != NULL )
        {
            if ( inst->first < *first ) *first = inst->first;
            if ( inst->count > *count ) *count = inst->count;
        }
    }
}

/* produce output for a cell */
static void col_inst_list_cell( const Vector * inst_list, const VCursor * curs,
                                sqlite3_context * ctx, int column_id, int64_t row_id )
{
    column_instance * inst = VectorGet( inst_list, column_id );
    if ( inst != NULL )
        col_inst_cell( inst, curs, ctx, row_id );
    else
        sqlite3_result_null( ctx );
}

/* -------------------------------------------------------------------------------------- */
typedef struct vdb_obj_desc
{
    const char * accession;
    const char * table_name;
    const char * row_range_str;
    struct num_gen * row_range;
    Vector column_descriptions;
    VNamelist * excluded_columns;
    size_t cache_size;
    int verbosity;
} vdb_obj_desc;

static void release_vdb_obj_desc( vdb_obj_desc * self )
{
    if ( self->accession != NULL ) sqlite3_free( ( void * )self->accession );
    if ( self->table_name != NULL ) sqlite3_free( ( void * )self->table_name );
    if ( self->row_range_str != NULL ) sqlite3_free( ( void * )self->row_range_str );
    if ( self->row_range != NULL ) num_gen_destroy( self->row_range );
    VectorWhack( &self->column_descriptions, destroy_column_description, NULL );
    if ( self->excluded_columns != NULL ) VNamelistRelease( self->excluded_columns );
}

static void vdb_obj_desc_print( vdb_obj_desc * self )
{
    printf( "---accession  = %s\n", self->accession != NULL ? self->accession : "None" );
    printf( "---cache-size = %lu\n", self->cache_size );    
    printf( "---table      = %s\n", self->table_name != NULL ? self->table_name : "None" );
    printf( "---rows       = %s\n", self->row_range_str != NULL ? self->row_range_str : "None" ); 
    printf( "---columns    = " ); print_col_desc_list( &self->column_descriptions ); printf( "\n" );
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

static bool is_equal( const String * S, const char * s1, const char * s2 )
{
    String S_template;
    StringInitCString( &S_template, s1 );
    if ( StringCaseEqual( &S_template, S ) )
        return true;
    if ( s2 != NULL )
    {
        StringInitCString( &S_template, s2 );
        return StringCaseEqual( &S_template, S );
    }
    return false;
}

static void vdb_obj_desc_parse_arg2( vdb_obj_desc * self, const char * name, const char * value )
{
	String S_name, S_value;
    bool done = false;
	
	StringInitCString( &S_name, name );
	trim_ws( &S_name );
	StringInitCString( &S_value, value );
	trim_ws( &S_value );
	
	if ( is_equal( &S_name, "acc", "A" ) )
    {
		self->accession = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
        done = true;
    }
    
	if ( !done && is_equal( &S_name, "table", "T" ) )
	{
        self->table_name = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
        done = true;
    }

    if ( !done && is_equal( &S_name, "columns", "C" ) )
        done = init_col_desc_list( &self->column_descriptions, &S_value );

    if ( !done && is_equal( &S_name, "exclude", "X" ) )
        done = ( 0 == VNamelistFromString( &self->excluded_columns, &S_value, ';' ) );

    if ( !done && is_equal( &S_name, "rows", "R" ) )
    {
        self->row_range_str = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
        done = ( 0 == num_gen_parse( self->row_range, self->row_range_str ) );
    }

    if ( !done && is_equal( &S_name, "verbose", "v" ) )
    {
        self->verbosity = StringToU64( &S_value, NULL );
        done = true;
    }

    if ( !done && is_equal( &S_name, "cache", NULL ) )
    {
        self->cache_size = StringToU64( &S_value, NULL );
        done = true;
    }
    
    if ( !done )
        printf( "unknown argument '%.*s' = '%.*s'\n", S_name.len, S_name.addr, S_value.len, S_value.addr );
}

static rc_t init_vdb_obj_desc( vdb_obj_desc * self, int argc, const char * const * argv )
{
    rc_t rc = -1;
    num_gen_make( &self->row_range );
    if ( argc > 3 )
    {
        int i;
        for ( i = 3; i < argc; i++ )
        {
            VNamelist * parts;
            rc = VNamelistFromStr( &parts, argv[ i ], '=' );
            if ( rc == 0 )
            {
                uint32_t count;
                rc = VNameListCount( parts, &count );
                if ( rc == 0 && count > 0 )
                {
                    const char * arg_name = NULL;
                    rc = VNameListGet( parts, 0, &arg_name );
                    if ( rc == 0 && arg_name != NULL )
                    {
                        if ( count > 1 )
                        {
                            const char * arg_value = NULL;
                            rc = VNameListGet( parts, 1, &arg_value );
                            if ( rc == 0 && arg_value != NULL )
                                vdb_obj_desc_parse_arg2( self, arg_name, arg_value );
                        }
                        else
                        {
                            if ( self->accession == NULL )
                                self->accession = sqlite3_mprintf( "%s", arg_name );
                        }
                    }
                }
                VNamelistRelease( parts );
                if ( self->cache_size == 0 )
                    self->cache_size = 1024 * 1024 * 32;
            }
        }
    }
    if ( self->verbosity > 0 )
        vdb_obj_desc_print( self );
    return rc;
}

               
/* -------------------------------------------------------------------------------------- */
typedef struct vdb_cursor
{
    sqlite3_vtab cursor;            /* Base class.  Must be first */
    const struct num_gen_iter * row_iter;
    vdb_obj_desc * desc;            /* cursor does not own this! */
    Vector column_instances;
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
    VectorWhack( &c->column_instances, destroy_column_instance, NULL );
    if ( c->curs != NULL ) VCursorRelease( c->curs );
    sqlite3_free( c );
    return SQLITE_OK;
}

/* create a cursor from the obj-description ( mostly it's columns ) */
static vdb_cursor * make_vdb_cursor( vdb_obj_desc * desc, const VTable * tbl )
{
    vdb_cursor * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        rc_t rc;
        memset( res, 0, sizeof( *res ) );
        res->desc = desc;

        /* we first have to make column-instances, before we can adjust the row-ranges */
		rc = VTableCreateCachedCursorRead( tbl, &res->curs, desc->cache_size );
        if ( rc == 0 )
        {
            /* this adds the columns to the cursor, opens the cursor, gets types, extracts row-range */
            rc = init_col_inst_list( &res->column_instances, &desc->column_descriptions, res->curs );
        }
        
        if ( rc == 0 )
        {
            int64_t  first = 0x7FFFFFFFFFFFFFFF;
            uint64_t count = 0;
            col_inst_list_get_row_range( &res->column_instances, &first, &count );
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
    col_inst_list_cell( &c->column_instances, c->curs, ctx, column_id, c->current_row );
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
/* this object has to be made on the heap,
   it is passed back ( typecasted ) to the sqlite-library in sqlite3_vdb_[ Create / Connect ] */
typedef struct vdb_obj
{
    sqlite3_vtab base;              /* Base class.  Must be first */
    const VDBManager * mgr;         /* the vdb-manager */
    const VDatabase *db;            /* the database to be used */
    const VTable *tbl;              /* the table to be used */
    vdb_obj_desc desc;              /* description of the object to be iterated over */
} vdb_obj;


/* destroy the whole connection database, table, manager etc. */
static int destroy_vdb_obj( vdb_obj * self )
{
    if ( self->desc.verbosity > 1 )
        printf( "---sqlite3_vdb_Disconnect()\n" );
    
    if ( self->mgr != NULL ) VDBManagerRelease( self->mgr );
    if ( self->tbl != NULL ) VTableRelease( self->tbl );
    if ( self->db != NULL ) VDatabaseRelease( self->db );
    release_vdb_obj_desc( &self->desc );
    
    sqlite3_free( self );
    return SQLITE_OK;    
}

/* check if all requested columns are available */
static rc_t vdb_obj_common_table_handler( vdb_obj * self )
{
    struct KNamelist * readable_columns;
    rc_t rc = VTableListReadableColumns( self->tbl, &readable_columns );
    if ( rc == 0 )
    {
		VNamelist * available;
		rc = VNamelist_from_KNamelist( &available, readable_columns );
		if ( rc == 0 )
		{
			rc = col_desc_list_check( &self->desc.column_descriptions, available, self->desc.excluded_columns );
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

static rc_t vdb_obj_open_db_without_table_name( vdb_obj * self )
{
	VNamelist * list;
	rc_t rc = vdb_obj_get_table_names( self->db, &list );
	if ( rc == 0 )
	{
		int32_t idx;
		rc = VNamelistContainsStr( list, "SEQUENCE", &idx );
		if ( rc == 0 && idx >= 0 )
		{
			/* we have a SEQUENCE-table */
			rc = VDatabaseOpenTableRead( self->db, &self->tbl, "SEQUENCE" );
			if ( rc == 0 )
				self->desc.table_name = sqlite3_mprintf( "SEQUENCE" );
		}
		else
		{
			/* we pick the first one */
			const char * s = NULL;
			rc = VNameListGet( list, 0, &s );
			if ( rc == 0 )
			{
				self->desc.table_name = sqlite3_mprintf( "%s", s );
				rc = VDatabaseOpenTableRead( self->db, &self->tbl, "%s", self->desc.table_name );	
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

static rc_t vdb_obj_open_db_with_mismatched_table_name( vdb_obj * self )
{
	VNamelist * list;
	rc_t rc = vdb_obj_get_table_names( self->db, &list );
	if ( rc == 0 )
	{
		String to_look_for, found;
		StringInitCString( &to_look_for, self->desc.table_name );
		if ( list_contains_value_starting_with( list, &to_look_for, &found ) )
		{
			self->desc.table_name = sqlite3_mprintf( "%.*s", found.len, found.addr );
			rc = VDatabaseOpenTableRead( self->db, &self->tbl, "%s", self->desc.table_name );
		}
		else
			rc = RC( rcApp, rcArgv, rcAccessing, rcSelf, rcInvalid );
		VNamelistRelease( list );
	}
	return rc;
}

/* the requested object is a database */
static rc_t vdb_obj_open_db( vdb_obj * self )
{
    rc_t rc = VDBManagerOpenDBRead( self->mgr, &self->db, NULL, "%s", self->desc.accession );
    if ( rc == 0 )
    {
		if ( self->desc.table_name == NULL )
			rc = vdb_obj_open_db_without_table_name( self );
		else
		{
			/* if the table-name does not fit, see if it is a shortened version of the existing ones... */
			rc = VDatabaseOpenTableRead( self->db, &self->tbl, "%s", self->desc.table_name );
			if ( rc != 0 )
				rc = vdb_obj_open_db_with_mismatched_table_name( self );
		}
		
        if ( rc == 0 )
            rc = vdb_obj_common_table_handler( self );
    }
    return rc;
}

/* the requested object is a table */
static rc_t vdb_obj_open_tbl( vdb_obj * self )
{
    rc_t rc = VDBManagerOpenTableRead( self->mgr, &self->tbl, NULL, "%s", self->desc.accession );
    if ( rc == 0 )
        rc = vdb_obj_common_table_handler( self );
    return rc;
}


/* make a database/table object, have a look if it exists, and has the columns we are asking for etc.  */
static vdb_obj * make_vdb_obj( int argc, const char * const * argv )
{
    vdb_obj * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        memset( res, 0, sizeof( *res ) );
        rc_t rc = init_vdb_obj_desc( &res->desc, argc, argv );
        if ( rc == 0 )
        {
            rc = VDBManagerMakeRead( &( res->mgr ), NULL );
            if ( rc == 0 )
            {
                /* types defined in <kdb/manager.h> !!! */
                int pt = ( VDBManagerPathType( res->mgr, "%s", res->desc.accession ) & ~ kptAlias );
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


/* take the database/table-obj and open a cursor on it */
static int vdb_obj_open( vdb_obj * self, sqlite3_vtab_cursor ** ppCursor )
{
    if ( self->desc.verbosity > 1 )
        printf( "---sqlite3_vdb_Open()\n" );
    vdb_cursor * c = make_vdb_cursor( &self->desc, self->tbl );
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
    const char * stm;
    vdb_obj * obj = make_vdb_obj( argc, argv );
    if ( obj == NULL )
        return SQLITE_ERROR;
        
    *ppVtab = ( sqlite3_vtab * )obj;

    if ( obj->desc.verbosity > 1 )
        printf( "%s", msg );
    
    stm = make_vdb_create_table_stm( &obj->desc.column_descriptions, "x" );
    if ( obj->desc.verbosity > 1 )
        printf( "stm = %s\n", stm );
    return sqlite3_declare_vtab( db, stm );    
}


/* -------------------------------------------------------------------------------------- */
/* create a new table-instance from scratch */
int sqlite3_vdb_Create( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                               sqlite3_vtab **ppVtab, char **pzErr )
{
    return sqlite3_vdb_CC( db, pAux, argc, argv, ppVtab, pzErr, "---sqlite3_vdb_Create()\n" );
}

/* open a table-instance from something existing */
int sqlite3_vdb_Connect( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                                sqlite3_vtab **ppVtab, char **pzErr )
{
    return sqlite3_vdb_CC( db, pAux, argc, argv, ppVtab, pzErr, "---sqlite3_vdb_Connect()\n" );
}

/* query what index can be used ---> maybe vdb supports that in the future */
int sqlite3_vdb_BestIndex( sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo )
{
    int res = SQLITE_ERROR;
    if ( tab != NULL )
    {
        vdb_obj * self = ( vdb_obj * )tab;
        if ( self->desc.verbosity > 2 )
            printf( "---sqlite3_vdb_BestIndex()\n" );
        if ( pIdxInfo != NULL )
            pIdxInfo->estimatedCost = 1000000;
        res = SQLITE_OK;
    }
    return res;
}

/* disconnect from a table */
int sqlite3_vdb_Disconnect( sqlite3_vtab *tab )
{
    if ( tab != NULL )
        return destroy_vdb_obj( ( vdb_obj * )tab );
    return SQLITE_ERROR;
}

/* open a cursor on a table */
int sqlite3_vdb_Open( sqlite3_vtab *tab, sqlite3_vtab_cursor **ppCursor )
{
    if ( tab != NULL )
        return vdb_obj_open( ( vdb_obj * ) tab, ppCursor );
    return SQLITE_ERROR;
}

/* close the cursor */
int sqlite3_vdb_Close( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        destroy_vdb_cursor( ( vdb_cursor * )cur );
    return SQLITE_ERROR;
}

/* this is a NOOP, because we have no index ( yet ) */
int sqlite3_vdb_Filter( sqlite3_vtab_cursor *cur, int idxNum, const char *idxStr,
                        int argc, sqlite3_value **argv )
{
    if ( cur != NULL )
    {
        vdb_cursor * self = ( vdb_cursor * )cur;
        if ( self->desc->verbosity > 2 )
            printf( "---sqlite3_vdb_Filter()\n" );
        return SQLITE_OK;
    }
    return SQLITE_ERROR;
}

/* advance to the next row */
int sqlite3_vdb_Next( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        return vdb_cursor_next( ( vdb_cursor * )cur );
    return SQLITE_ERROR;
}

/* check if we are at the end */
int sqlite3_vdb_Eof( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        return vdb_cursor_eof( ( vdb_cursor * )cur );
    return SQLITE_ERROR;
}

/* request data for a cell */
int sqlite3_vdb_Column( sqlite3_vtab_cursor *cur, sqlite3_context * ctx, int column_id )
{
    if ( cur != NULL )
        return vdb_cursor_column( ( vdb_cursor * )cur, ctx, column_id );
    return SQLITE_ERROR;
}

/* request row-id */
int sqlite3_vdb_Rowid( sqlite3_vtab_cursor *cur, sqlite_int64 * pRowid )
{
    if ( cur != NULL )
        return vdb_cursor_row_id( ( vdb_cursor * )cur, pRowid );
    return SQLITE_ERROR;
}

sqlite3_module VDB_Module =
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


/* ========================================================================================================== */

#define NGS_STYLE_READS 1
#define NGS_STYLE_FRAGMENTS 2
#define NGS_STYLE_ALIGNMENTS 3
#define NGS_STYLE_PILEUP 4

/* -------------------------------------------------------------------------------------- */
typedef struct ngs_obj_desc
{
    const char * accession;
    const char * style_string; /* READS, FRAGMENTS, ALIGNMENTS, PILEUP-EVENTS */
    int style;
    int verbosity;
} ngs_obj_desc;

static void release_ngs_obj_desc( ngs_obj_desc * self )
{
    if ( self->accession != NULL ) sqlite3_free( ( void * )self->accession );
    if ( self->style_string != NULL ) sqlite3_free( ( void * )self->style_string );
}

static void ngs_obj_desc_print( ngs_obj_desc * self )
{
    printf( "---accession  = %s\n", self->accession != NULL ? self->accession : "None" );
    printf( "---style      = %s\n", self->style_string != NULL ? self->style_string : "None" );
}

static int ngs_obj_style_2_int( const char * style )
{
    int res = 0;
    String S;
    StringInitCString( &S, style );
    
    if ( is_equal( &S, "READS", "R" ) )
        res = NGS_STYLE_READS;
    else if ( is_equal( &S, "FRAGMENTS", "F" ) )
        res = NGS_STYLE_FRAGMENTS;
    else if ( is_equal( &S, "ALIGNMENTS", "A" ) )
        res = NGS_STYLE_ALIGNMENTS;
    else if ( is_equal( &S, "PILEUP", "P" ) )
        res = NGS_STYLE_PILEUP;
    return res;
}

static void ngs_obj_desc_parse_arg2( ngs_obj_desc * self, const char * name, const char * value )
{
	String S_name, S_value;
    bool done = false;
	
	StringInitCString( &S_name, name );
	trim_ws( &S_name );
	StringInitCString( &S_value, value );
	trim_ws( &S_value );
	
	if ( is_equal( &S_name, "acc", "A" ) )
    {
		self->accession = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
        done = true;
    }
    
	if ( !done && is_equal( &S_name, "style", "S" ) )
	{
        self->style_string = sqlite3_mprintf( "%.*s", S_value.len, S_value.addr );
        self->style = ngs_obj_style_2_int( self->style_string );
        done = true;
    }

    if ( !done && is_equal( &S_name, "verbose", "v" ) )
    {
        self->verbosity = StringToU64( &S_value, NULL );
        done = true;
    }

    if ( !done )
        printf( "unknown argument '%.*s' = '%.*s'\n", S_name.len, S_name.addr, S_value.len, S_value.addr );
}

static rc_t init_ngs_obj_desc( ngs_obj_desc * self, int argc, const char * const * argv )
{
    rc_t rc = -1;
    if ( argc > 3 )
    {
        int i;
        for ( i = 3; i < argc; i++ )
        {
            VNamelist * parts;
            rc = VNamelistFromStr( &parts, argv[ i ], '=' );
            if ( rc == 0 )
            {
                uint32_t count;
                rc = VNameListCount( parts, &count );
                if ( rc == 0 && count > 0 )
                {
                    const char * arg_name = NULL;
                    rc = VNameListGet( parts, 0, &arg_name );
                    if ( rc == 0 && arg_name != NULL )
                    {
                        if ( count > 1 )
                        {
                            const char * arg_value = NULL;
                            rc = VNameListGet( parts, 1, &arg_value );
                            if ( rc == 0 && arg_value != NULL )
                                ngs_obj_desc_parse_arg2( self, arg_name, arg_value );
                        }
                        else
                        {
                            if ( self->accession == NULL )
                                self->accession = sqlite3_mprintf( "%s", arg_name );
                        }
                    }
                }
                VNamelistRelease( parts );
            }
        }
    }
    if ( self->style == 0 )
        self->style = NGS_STYLE_READS;
    if ( self->verbosity > 0 )
        ngs_obj_desc_print( self );
    return rc;
}

static char * make_ngs_create_table_stm( const ngs_obj_desc * desc, const char * tbl_name  )
{
    char * res = sqlite3_mprintf( "CREATE TABLE %s (", tbl_name );
    if ( res != NULL )
    {
        switch( desc->style )
        {
            case NGS_STYLE_READS      : res = sqlite3_mprintf( "%z SEQ, QUAL, NAME, ID, GRP, CAT, NFRAGS );", res ); break;
            case NGS_STYLE_FRAGMENTS  : res = sqlite3_mprintf( "%z SEQ, QUAL );", res ); break;
            case NGS_STYLE_ALIGNMENTS : res = sqlite3_mprintf( "%z SEQ, QUAL, ID, REFSPEC, MAPQ, RDFILTER, REFBASES, GRP, ALIG, PRIM, REFPOS, LEN, REVERSE, TLEN, CIGARS, CIGARL, HASMATE, MATEID, MATEREF, MATEREVERSE, FIRST );", res ); break;
            case NGS_STYLE_PILEUP     : res = sqlite3_mprintf( "%z SEQ );", res ); break;
            default : res = sqlite3_mprintf( "%z X );", res ); break;
        }
    }
    return res;
}

/* -------------------------------------------------------------------------------------- */
/* this object has to be made on the heap,
   it is passed back ( typecasted ) to the sqlite-library in sqlite3_vdb_[ Create / Connect ] */
typedef struct ngs_obj
{
    sqlite3_vtab base;              /* Base class.  Must be first */
    ngs_obj_desc desc;              /* the description ( ACC, Style ) */
} ngs_obj;


/* destroy the whole connection database, table, manager etc. */
static int destroy_ngs_obj( ngs_obj * self )
{
    if ( self->desc.verbosity > 1 )
        printf( "---sqlite3_ngs_Disconnect()\n" );
    
    release_ngs_obj_desc( &self->desc );
    
    sqlite3_free( self );
    return SQLITE_OK;    
}

/* gather what we want to open, check if it can be opened... */
static ngs_obj * make_ngs_obj( int argc, const char * const * argv )
{
    ngs_obj * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        memset( res, 0, sizeof( *res ) );
        rc_t rc = init_ngs_obj_desc( &res->desc, argc, argv );
        if ( rc == 0 )
        {
            /* here we have to check if we can open the accession as a read-collection... */
            if ( rc != 0 )
            {
                destroy_ngs_obj( res );
                res = NULL;
            }
        }
    }
    return res;
}

/* -------------------------------------------------------------------------------------- */
typedef struct ngs_cursor
{
    sqlite3_vtab cursor;            /* Base class.  Must be first */
    ngs_obj_desc * desc;            /* cursor does not own this! */
    NGS_ReadCollection * rd_coll;   /* the read-collection we are operating on */
    NGS_Read * m_read;              /* the read-iterator */
    NGS_Alignment * m_alig;         /* the alignment-iterator */
    int64_t current_row;    
    bool eof;
} ngs_cursor;


static int destroy_ngs_cursor( ngs_cursor * self )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    
    if ( self->desc->verbosity > 1 )
        printf( "---sqlite3_ngs_Close()\n" );

    if ( self->m_read != NULL )
        NGS_ReadRelease ( self->m_read, ctx );
    
    if ( self->m_alig != NULL )
        NGS_AlignmentRelease ( self->m_alig, ctx );

    if ( self->rd_coll != NULL )
        NGS_RefcountRelease( ( NGS_Refcount * ) self->rd_coll, ctx );
    
    sqlite3_free( self );
    return SQLITE_OK;

}

static ngs_cursor * make_ngs_cursor_READS( ngs_cursor * self )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    self->m_read = NGS_ReadCollectionGetReads( self->rd_coll, ctx, true, true, true );
    if ( FAILED() )
    {
        CLEAR();
        destroy_ngs_cursor( self );
        self = NULL;
    }
    else
    {
        self->eof = ! NGS_ReadIteratorNext( self->m_read, ctx );
        if ( FAILED() )
        {
            CLEAR();
            destroy_ngs_cursor( self );
            self = NULL;
        }
    }
    return self;
}

static ngs_cursor * make_ngs_cursor_FRAGS( ngs_cursor * self )
{
    return self;
}

static ngs_cursor * make_ngs_cursor_ALIGS( ngs_cursor * self )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    self->m_alig = NGS_ReadCollectionGetAlignments( self->rd_coll, ctx, true, false );
    if ( FAILED() )
    {
        CLEAR();
        destroy_ngs_cursor( self );
        self = NULL;
    }
    else
    {
        self->eof = ! NGS_AlignmentIteratorNext( self->m_alig, ctx );
        if ( FAILED() )
        {
            CLEAR();
            destroy_ngs_cursor( self );
            self = NULL;
        }
    }
    return self;
}

static ngs_cursor * make_ngs_cursor_PILEUP( ngs_cursor * self )
{
    return self;
}

/* create a cursor from the obj-description ( mostly it's columns ) */
static ngs_cursor * make_ngs_cursor( ngs_obj_desc * desc )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );

    ngs_cursor * res = sqlite3_malloc( sizeof( * res ) );
    if ( res != NULL )
    {
        memset( res, 0, sizeof( *res ) );
        res->desc = desc;

        res->rd_coll = NGS_ReadCollectionMake( ctx, desc->accession );
        if ( FAILED() )
        {
            CLEAR();
            destroy_ngs_cursor( res );
            res = NULL;
        }
        else switch( desc->style )
        {
            case NGS_STYLE_READS      : res = make_ngs_cursor_READS( res ); break;
            case NGS_STYLE_FRAGMENTS  : res = make_ngs_cursor_FRAGS( res ); break;
            case NGS_STYLE_ALIGNMENTS : res = make_ngs_cursor_ALIGS( res ); break;
            case NGS_STYLE_PILEUP     : res = make_ngs_cursor_PILEUP( res ); break;
        }
    }
    return res;
}


static int ngs_cursor_next( ngs_cursor * self )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    
    if ( self->desc->verbosity > 2 )
        printf( "---sqlite3_ngs_next()\n" );
    
    switch( self->desc->style )
    {
        case NGS_STYLE_READS      : self->eof = ! NGS_ReadIteratorNext( self->m_read, ctx ); break;
        case NGS_STYLE_FRAGMENTS  : self->eof = ! NGS_ReadIteratorNext( self->m_read, ctx ); break;
        case NGS_STYLE_ALIGNMENTS : self->eof = ! NGS_AlignmentIteratorNext( self->m_alig, ctx ); break;
        case NGS_STYLE_PILEUP     : break;
    }

    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    else
        self->current_row++;
    return SQLITE_OK;
}

static int ngs_cursor_eof( ngs_cursor * self )
{
    if ( self->desc->verbosity > 2 )
        printf( "---sqlite3_ngs_Eof()\n" );
    if ( self->eof )
        return SQLITE_ERROR; /* anything else than SQLITE_OK means EOF */
    return SQLITE_OK;
}

static int ngs_return_NGS_String( sqlite3_context * sql_ctx, NGS_String * ngs_str )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    
    const char * s = NGS_StringData( ngs_str, ctx );
    if ( FAILED() )
    {
        CLEAR();
        NGS_StringRelease( ngs_str, ctx );
        return SQLITE_ERROR;
    }
    else
    {
        size_t len = NGS_StringSize( ngs_str, ctx );
        if ( FAILED() )
        {
            CLEAR();
            NGS_StringRelease( ngs_str, ctx );
            return SQLITE_ERROR;
        }
        sqlite3_result_text( sql_ctx, (char *)s, len, SQLITE_TRANSIENT );
        
        NGS_StringRelease( ngs_str, ctx );
    }
    return SQLITE_OK;
}

static int ngs_cursor_read_str( ngs_cursor * self,
                               sqlite3_context * sql_ctx,
                               struct NGS_String * f( NGS_Read * read, ctx_t ctx ) )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    NGS_String * m_seq = f( self->m_read, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    return ngs_return_NGS_String( sql_ctx, m_seq );
}

static int ngs_cursor_read_str_full( ngs_cursor * self,
                               sqlite3_context * sql_ctx,
                               struct NGS_String * f( NGS_Read * read, ctx_t ctx, uint64_t offset, uint64_t length ) )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    NGS_String * m_seq = f( self->m_read, ctx, 0, ( size_t ) - 1 );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    return ngs_return_NGS_String( sql_ctx, m_seq );
}

static int ngs_cursor_column_read_CAT( ngs_cursor * self, sqlite3_context * sql_ctx )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    enum NGS_ReadCategory cat = NGS_ReadGetReadCategory( self->m_read, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    sqlite3_result_int( sql_ctx, cat );
    return SQLITE_OK;
}

static int ngs_cursor_column_read_NFRAGS( ngs_cursor * self, sqlite3_context * sql_ctx )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    uint32_t n = NGS_ReadNumFragments( self->m_read, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    sqlite3_result_int( sql_ctx, n );
    return SQLITE_OK;
}

static int ngs_cursor_column_read( ngs_cursor * self, sqlite3_context * sql_ctx, int column_id )
{
    switch( column_id )
    {
        case 0 : return ngs_cursor_read_str_full( self, sql_ctx, NGS_ReadGetReadSequence ); break; /* SEQ */
        case 1 : return ngs_cursor_read_str_full( self, sql_ctx, NGS_ReadGetReadQualities ); break; /* QUAL */
        case 2 : return ngs_cursor_read_str( self, sql_ctx, NGS_ReadGetReadName ); break; /* NAME */
        case 3 : return ngs_cursor_read_str( self, sql_ctx, NGS_ReadGetReadId ); break; /* ID */
        case 4 : return ngs_cursor_read_str( self, sql_ctx, NGS_ReadGetReadGroup ); break; /* GRP */
        case 5 : return ngs_cursor_column_read_CAT( self, sql_ctx ); break; /* CAT */
        case 6 : return ngs_cursor_column_read_NFRAGS( self, sql_ctx ); break; /* NFRAGS */
    }
    return SQLITE_ERROR;
}

static int ngs_cursor_column_frag( ngs_cursor * self, sqlite3_context * sql_ctx, int column_id )
{
    return SQLITE_ERROR;
}

static int ngs_cursor_alig_str( ngs_cursor * self,
                               sqlite3_context * sql_ctx,
                               struct NGS_String * f( NGS_Alignment * self, ctx_t ctx ) )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    NGS_String * m_seq = f( self->m_alig, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    return ngs_return_NGS_String( sql_ctx, m_seq );
}

static int ngs_cursor_alig_str_bool( ngs_cursor * self,
                               sqlite3_context * sql_ctx,
                               struct NGS_String * f( NGS_Alignment * self, ctx_t ctx, bool b ),
                               bool b )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    NGS_String * m_seq = f( self->m_alig, ctx, b );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    return ngs_return_NGS_String( sql_ctx, m_seq );
}

static int ngs_cursor_alig_bool( ngs_cursor * self,
                               sqlite3_context * sql_ctx,
                               bool f( NGS_Alignment * self, ctx_t ctx ) )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    bool b = f( self->m_alig, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    sqlite3_result_int( sql_ctx, b ? 1 : 0 );
    return SQLITE_OK;
}

static int ngs_cursor_alig_uint64( ngs_cursor * self,
                               sqlite3_context * sql_ctx,
                               uint64_t f( NGS_Alignment * self, ctx_t ctx ) )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    uint64_t value = f( self->m_alig, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    sqlite3_result_int64( sql_ctx, value );
    return SQLITE_OK;
}


static int ngs_cursor_column_alig_MAPQ( ngs_cursor * self, sqlite3_context * sql_ctx )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    int mapq = NGS_AlignmentGetMappingQuality( self->m_alig, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    sqlite3_result_int( sql_ctx, mapq );
    return SQLITE_OK;
}

static int ngs_cursor_column_alig_RDFILTER( ngs_cursor * self, sqlite3_context * sql_ctx )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    INSDC_read_filter rf = NGS_AlignmentGetReadFilter( self->m_alig, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    sqlite3_result_int( sql_ctx, rf );
    return SQLITE_OK;
}

static int ngs_cursor_column_alig_REFPOS( ngs_cursor * self, sqlite3_context * sql_ctx )
{
    HYBRID_FUNC_ENTRY( rcSRA, rcRow, rcAccessing );
    int64_t pos = NGS_AlignmentGetAlignmentPosition( self->m_alig, ctx );
    if ( FAILED() )
    {
        CLEAR();
        return SQLITE_ERROR;
    }
    sqlite3_result_int64( sql_ctx, pos );
    return SQLITE_OK;
}


static int ngs_cursor_column_alig( ngs_cursor * self, sqlite3_context * sql_ctx, int column_id )
{
    switch( column_id )
    {
        case 0  : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetClippedFragmentBases ); break; /* SEQ */
        case 1  : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetClippedFragmentQualities ); break; /* QUAL */
        case 2  : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetAlignmentId ); break; /* ID */
        case 3  : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetReferenceSpec ); break; /* REFSPEC */
        case 4  : return ngs_cursor_column_alig_MAPQ( self, sql_ctx ); break; /* MAPQ */
        case 5  : return ngs_cursor_column_alig_RDFILTER( self, sql_ctx ); break; /* RDFILTER */
        case 6  : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetReferenceBases ); break; /* REFBASES */
        case 7  : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetReadGroup ); break; /* GRP */
        case 8  : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetAlignedFragmentBases ); break; /*ALIG */
        case 9  : return ngs_cursor_alig_bool( self, sql_ctx, NGS_AlignmentIsPrimary ); break; /* PRIM */
        case 10 : return ngs_cursor_column_alig_REFPOS( self, sql_ctx ); break; /* REFPOS */
        case 11 : return ngs_cursor_alig_uint64( self, sql_ctx, NGS_AlignmentGetAlignmentLength ); break; /* LEN */
        case 12 : return ngs_cursor_alig_bool( self, sql_ctx, NGS_AlignmentGetIsReversedOrientation ); break; /* REVERSE */
        case 13 : return ngs_cursor_alig_uint64( self, sql_ctx, NGS_AlignmentGetTemplateLength ); break; /* TLEN */
        case 14 : return ngs_cursor_alig_str_bool( self, sql_ctx, NGS_AlignmentGetShortCigar, true ); break; /* CIGARS */
        case 15 : return ngs_cursor_alig_str_bool( self, sql_ctx, NGS_AlignmentGetLongCigar, true ); break; /* CIGARL */
        case 16 : return ngs_cursor_alig_bool( self, sql_ctx, NGS_AlignmentHasMate ); break; /* HASMATE */
        case 17 : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetMateAlignmentId ); /* MATEID */
        case 18 : return ngs_cursor_alig_str( self, sql_ctx, NGS_AlignmentGetMateReferenceSpec ); /* MATEREF */
        case 19 : return ngs_cursor_alig_bool( self, sql_ctx, NGS_AlignmentGetMateIsReversedOrientation ); break; /* MATEREVERSE */
        case 20 : return ngs_cursor_alig_bool( self, sql_ctx, NGS_AlignmentIsFirst ); break; /* FIRST */
    }
    return SQLITE_ERROR;
}

static int ngs_cursor_column_pileup( ngs_cursor * self, sqlite3_context * sql_ctx, int column_id )
{
    return SQLITE_ERROR;
}

static int ngs_cursor_column( ngs_cursor * self, sqlite3_context * sql_ctx, int column_id )
{
    if ( self->desc->verbosity > 2 )
        printf( "---sqlite3_ngs_Column( %d )\n", column_id );
    
    switch( self->desc->style )
    {
        case NGS_STYLE_READS      : return ngs_cursor_column_read( self, sql_ctx, column_id ); break;
        case NGS_STYLE_FRAGMENTS  : return ngs_cursor_column_frag( self, sql_ctx, column_id ); break;
        case NGS_STYLE_ALIGNMENTS : return ngs_cursor_column_alig( self, sql_ctx, column_id ); break;
        case NGS_STYLE_PILEUP     : return ngs_cursor_column_pileup( self, sql_ctx, column_id ); break;
    }
    return SQLITE_ERROR;
}

static int ngs_cursor_row_id( ngs_cursor * self, sqlite_int64 * pRowid )
{
    if ( self->desc->verbosity > 2 )
        printf( "---sqlite3_ngs_Rowid()\n" );
    if ( pRowid != NULL )
        *pRowid = self->current_row;
    return SQLITE_OK;
}

/* -------------------------------------------------------------------------------------- */
/* take ngs-obj and open a read-collection on it */
static int ngs_obj_open( ngs_obj * self, sqlite3_vtab_cursor ** ppCursor )
{
    if ( self->desc.verbosity > 1 )
        printf( "---sqlite3_ngs_Open()\n" );
    ngs_cursor * c = make_ngs_cursor( &self->desc );
    if ( c != NULL )
    {
        *ppCursor = ( sqlite3_vtab_cursor * )c;
        return SQLITE_OK;
    }
    return SQLITE_ERROR;
}

/* -------------------------------------------------------------------------------------- */
/* the common code for xxx_Create() and xxx_Connect */
static int sqlite3_ngs_CC( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                           sqlite3_vtab **ppVtab, char **pzErr, const char * msg )
{
    const char * stm;
    ngs_obj * obj = make_ngs_obj( argc, argv );
    if ( obj == NULL )
        return SQLITE_ERROR;
        
    *ppVtab = ( sqlite3_vtab * )obj;

    if ( obj->desc.verbosity > 1 )
        printf( "%s", msg );

    stm = make_ngs_create_table_stm( &obj->desc, "X" );
    if ( obj->desc.verbosity > 1 )
        printf( "stm = %s\n", stm );
    return sqlite3_declare_vtab( db, stm );
}


/* -------------------------------------------------------------------------------------- */
/* create a new table-instance from scratch */
int sqlite3_ngs_Create( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                               sqlite3_vtab **ppVtab, char **pzErr )
{
    return sqlite3_ngs_CC( db, pAux, argc, argv, ppVtab, pzErr, "---sqlite3_ngs_Create()\n" );
}

/* open a table-instance from something existing */
int sqlite3_ngs_Connect( sqlite3 *db, void *pAux, int argc, const char * const * argv,
                                sqlite3_vtab **ppVtab, char **pzErr )
{
    return sqlite3_ngs_CC( db, pAux, argc, argv, ppVtab, pzErr, "---sqlite3_ngs_Connect()\n" );
}

/* query what index can be used ---> maybe vdb supports that in the future */
int sqlite3_ngs_BestIndex( sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo )
{
    int res = SQLITE_ERROR;
    if ( tab != NULL )
    {
        ngs_obj * self = ( ngs_obj * )tab;
        if ( self->desc.verbosity > 2 )
            printf( "---sqlite3_ngs_BestIndex()\n" );
        if ( pIdxInfo != NULL )
            pIdxInfo->estimatedCost = 1000000;
        res = SQLITE_OK;
    }
    return res;
}

/* disconnect from a table */
int sqlite3_ngs_Disconnect( sqlite3_vtab *tab )
{
    if ( tab != NULL )
        return destroy_ngs_obj( ( ngs_obj * )tab );
    return SQLITE_ERROR;
}

/* open a cursor on a table */
int sqlite3_ngs_Open( sqlite3_vtab *tab, sqlite3_vtab_cursor **ppCursor )
{
    if ( tab != NULL )
        return ngs_obj_open( ( ngs_obj * ) tab, ppCursor );
    return SQLITE_ERROR;
}

/* close the cursor */
int sqlite3_ngs_Close( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        destroy_ngs_cursor( ( ngs_cursor * )cur );
    return SQLITE_ERROR;
}

/* this is a NOOP, because we have no index ( yet ) */
int sqlite3_ngs_Filter( sqlite3_vtab_cursor *cur, int idxNum, const char *idxStr,
                        int argc, sqlite3_value **argv )
{
    if ( cur != NULL )
    {
        ngs_cursor * self = ( ngs_cursor * )cur;
        if ( self->desc->verbosity > 2 )
            printf( "---sqlite3_ngs_Filter()\n" );
        return SQLITE_OK;
    }
    return SQLITE_ERROR;
}

/* advance to the next row */
int sqlite3_ngs_Next( sqlite3_vtab_cursor *cur )
{
    if ( cur != NULL )
        return ngs_cursor_next( ( ngs_cursor * )cur );
    return SQLITE_ERROR;        
}

/* check if we are at the end */
int sqlite3_ngs_Eof( sqlite3_vtab_cursor *cur )
{
     if ( cur != NULL )
        return ngs_cursor_eof( ( ngs_cursor * )cur );
    return SQLITE_ERROR;
}

/* request data for a cell */
int sqlite3_ngs_Column( sqlite3_vtab_cursor *cur, sqlite3_context * ctx, int column_id )
{
    if ( cur != NULL )
        return ngs_cursor_column( ( ngs_cursor * )cur, ctx, column_id );
    return SQLITE_ERROR;
}

/* request row-id */
int sqlite3_ngs_Rowid( sqlite3_vtab_cursor *cur, sqlite_int64 * pRowid )
{
    if ( cur != NULL )
        return ngs_cursor_row_id( ( ngs_cursor * )cur, pRowid );
    return SQLITE_ERROR;
}


sqlite3_module NGS_Module =
{
  0,                            /* iVersion */
  sqlite3_ngs_Create,           /* xCreate */
  sqlite3_ngs_Connect,          /* xConnect */
  sqlite3_ngs_BestIndex,        /* xBestIndex */
  sqlite3_ngs_Disconnect,       /* xDisconnect */
  sqlite3_ngs_Disconnect,       /* xDestroy */
  sqlite3_ngs_Open,             /* xOpen - open a cursor */
  sqlite3_ngs_Close,            /* xClose - close a cursor */
  sqlite3_ngs_Filter,           /* xFilter - configure scan constraints */
  sqlite3_ngs_Next,             /* xNext - advance a cursor */
  sqlite3_ngs_Eof,              /* xEof - check for end of scan */
  sqlite3_ngs_Column,           /* xColumn - read data */
  sqlite3_ngs_Rowid,            /* xRowid - read data */
  NULL,                         /* xUpdate */
  NULL,                         /* xBegin */
  NULL,                         /* xSync */
  NULL,                         /* xCommit */
  NULL,                         /* xRollback */
  NULL,                         /* xFindMethod */
  NULL,                         /* xRename */
};


/* ========================================================================================================== */

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
    int res = SQLITE_OK;
#ifndef SQLITE_OMIT_VIRTUALTABLE
    SQLITE_EXTENSION_INIT2( pApi );
    res = sqlite3_create_module( db, "vdb", &VDB_Module, NULL );
    if ( res == SQLITE_OK )
        res = sqlite3_create_module( db, "ngs", &NGS_Module, NULL );
#endif
    return res;
}
