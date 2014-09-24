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

#include "vdb-dump-helper.h"

#include "vdb-dump-coldefs.h"

#include <klib/vector.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <vdb/vdb-priv.h>
#include <sra/sradb.h>
#include <sra/pacbio.h>
#include <os-native.h>
#include <sysalloc.h>

/* for platforms */
#include <insdc/sra.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* once we get used to having moved the read descriptor
   out of SRA, we should begin using those names.
   if anyone has an investment in the old names, we may
   want to provide a switch for using them... */
#if USE_OLD_SRA_NAME || 1
#define SRA_NAME( name ) \
    "SRA_" #name
#define SRA_NAMES( name1, name2 ) \
    "SRA_" #name1 "|SRA_" #name2
#else
#define SRA_NAME( name ) \
    # name
#define SRA_NAMES( name1, name2 ) \
    #name1 "|" #name2
#endif

/* implementation of the value-translation-functions */

const char SRA_PB_HS_0[] = { "SRA_PACBIO_HOLE_SEQUENCING" };
const char SRA_PB_HS_1[] = { "SRA_PACBIO_HOLE_ANTIHOLE" };
const char SRA_PB_HS_2[] = { "SRA_PACBIO_HOLE_FIDUCIAL" };
const char SRA_PB_HS_3[] = { "SRA_PACBIO_HOLE_SUSPECT" };
const char SRA_PB_HS_4[] = { "SRA_PACBIO_HOLE_ANTIMIRROR" };
const char SRA_PB_HS_5[] = { "SRA_PACBIO_HOLE_FDZMW" };
const char SRA_PB_HS_6[] = { "SRA_PACBIO_HOLE_FBZMW" };
const char SRA_PB_HS_7[] = { "SRA_PACBIO_HOLE_ANTIBEAMLET" };
const char SRA_PB_HS_8[] = { "SRA_PACBIO_HOLE_OUTSIDEFOV" };
const char SRA_PB_HS_9[] = { "unknown hole-status" };

const char *vdcd_get_hole_status_txt( const uint32_t id )
{
    switch( id )
    {
        case SRA_PACBIO_HOLE_SEQUENCING     : return( SRA_PB_HS_0 ); break;
        case SRA_PACBIO_HOLE_ANTIHOLE       : return( SRA_PB_HS_1 ); break;
        case SRA_PACBIO_HOLE_FIDUCIAL       : return( SRA_PB_HS_2 ); break;
        case SRA_PACBIO_HOLE_SUSPECT        : return( SRA_PB_HS_3 ); break;
        case SRA_PACBIO_HOLE_ANTIMIRROR     : return( SRA_PB_HS_4 ); break;
        case SRA_PACBIO_HOLE_FDZMW          : return( SRA_PB_HS_5 ); break;
        case SRA_PACBIO_HOLE_FBZMW          : return( SRA_PB_HS_6 ); break;
        case SRA_PACBIO_HOLE_ANTIBEAMLET    : return( SRA_PB_HS_7 ); break;
        case SRA_PACBIO_HOLE_OUTSIDEFOV     : return( SRA_PB_HS_8 ); break;
    }
    return( SRA_PB_HS_9 );
}

const char *vdcd_get_platform_txt( const uint32_t id )
{
#define CASE( id ) \
    case id : return # id; break

    switch( id )
    {
        CASE ( SRA_PLATFORM_UNDEFINED );
        CASE ( SRA_PLATFORM_454 );
        CASE ( SRA_PLATFORM_ILLUMINA );
        CASE ( SRA_PLATFORM_ABSOLID );
        CASE ( SRA_PLATFORM_COMPLETE_GENOMICS );
        CASE ( SRA_PLATFORM_HELICOS );
        CASE ( SRA_PLATFORM_PACBIO_SMRT );
        CASE ( SRA_PLATFORM_ION_TORRENT );
        CASE ( SRA_PLATFORM_SANGER );
    }
#undef CASE

    return "unknown platform";
}

const char SRA_RT_0[] = { SRA_NAME  ( READ_TYPE_TECHNICAL ) };
const char SRA_RT_1[] = { SRA_NAME  ( READ_TYPE_BIOLOGICAL ) };
const char SRA_RT_2[] = { SRA_NAMES ( READ_TYPE_TECHNICAL, READ_TYPE_FORWARD ) };
const char SRA_RT_3[] = { SRA_NAMES ( READ_TYPE_BIOLOGICAL, READ_TYPE_FORWARD ) };
const char SRA_RT_4[] = { SRA_NAMES ( READ_TYPE_TECHNICAL, READ_TYPE_REVERSE ) };
const char SRA_RT_5[] = { SRA_NAMES ( READ_TYPE_BIOLOGICAL, READ_TYPE_REVERSE ) };
const char SRA_RT_6[] = { "unknown read-type" };

const char *vdcd_get_read_type_txt( const uint32_t id )
{
    switch( id )
    {
        case 0 : return( SRA_RT_0 ); break;
        case 1 : return( SRA_RT_1 ); break;
        case 2 : return( SRA_RT_2 ); break;
        case 3 : return( SRA_RT_3 ); break;
        case 4 : return( SRA_RT_4 ); break;
        case 5 : return( SRA_RT_5 ); break;
    }
    return( SRA_RT_6 );
}

const char SRA_FT_0[] = { SRA_NAME ( READ_FILTER_PASS ) };
const char SRA_FT_1[] = { SRA_NAME ( READ_FILTER_REJECT ) };
const char SRA_FT_2[] = { SRA_NAME ( READ_FILTER_CRITERIA ) };
const char SRA_FT_3[] = { SRA_NAME ( READ_FILTER_REDACTED ) };
const char SRA_FT_4[] = { "unknown read-filter" };

const char *vdcd_get_read_filter_txt( const uint32_t id )
{
    switch( id )
    {
        case 0 : return( SRA_FT_0 ); break;
        case 1 : return( SRA_FT_1 ); break;
        case 2 : return( SRA_FT_2 ); break;
        case 3 : return( SRA_FT_3 ); break;
    }
    return( SRA_FT_4 );
}

/* hardcoded values taken from asm-trace/interface/sra/sradb.h */
#define SRA_KEY_PLATFORM_ID "INSDC:SRA:platform_id"
#define SRA_KEY_XREAD_TYPE "INSDC:SRA:xread_type"
#define SRA_KEY_READ_TYPE "INSDC:SRA:read_type"
#define SRA_KEY_READ_FILTER "INSDC:SRA:read_filter"
#define SRA_PACBIO_HOLE_STATUS "PacBio:hole:status"


static bool vdcd_type_cmp( const VSchema *my_schema, VTypedecl * typedecl, const char * to_check )
{
    VTypedecl type_to_check;
    rc_t rc = VSchemaResolveTypedecl ( my_schema, &type_to_check, "%s", to_check );
    if ( rc == 0 )
    {
        return VTypedeclToTypedecl ( typedecl, my_schema, &type_to_check, NULL, NULL );
    }
    return false;
}


static value_trans_fct_t vdcd_get_value_trans_fct( const VSchema *my_schema, VTypedecl * typedecl )
{
    value_trans_fct_t res = NULL;

    if ( my_schema == NULL ) return res;
    if ( typedecl == NULL ) return res;

    if ( vdcd_type_cmp( my_schema, typedecl, SRA_KEY_PLATFORM_ID ) )
    {
        res = vdcd_get_platform_txt;
    }
    else if ( vdcd_type_cmp( my_schema, typedecl, SRA_KEY_XREAD_TYPE ) )
    {
        res = vdcd_get_read_type_txt;
    }
    else if ( vdcd_type_cmp( my_schema, typedecl, SRA_KEY_READ_TYPE ) )
    {
        res = vdcd_get_read_type_txt;
    }
    else if ( vdcd_type_cmp( my_schema, typedecl, SRA_KEY_READ_FILTER ) )
    {
        res = vdcd_get_read_filter_txt;
    }
    else if ( vdcd_type_cmp( my_schema, typedecl, SRA_PACBIO_HOLE_STATUS ) )
    {
        res = vdcd_get_hole_status_txt;
    }

    return res;
}


/* implementation of the dimension-translation-functions */
static char *vdcd_get_read_desc_txt( const uint8_t * src )
{
    char *res = calloc( 1, 120 );
    SRAReadDesc desc;
    memmove( &desc, src, sizeof( desc ) );
    string_printf ( res, 119, NULL,
              "seg.start=%u, seg.len=%u, type=%u, cs_key=%u, label=%s",
              desc.seg.start, desc.seg.len, desc.type,
              desc.cs_key, desc.label );
    return res;
}

static char *vdcd_get_spot_desc_txt( const uint8_t *src )
{
    char *res = calloc( 1, 120 );
    SRASpotDesc desc;
    memmove( &desc, src, sizeof( desc ) );
    string_printf ( res, 119, NULL,
              "spot_len=%u, fixed_len=%u, signal_len=%u, clip_qual_right=%u, num_reads=%u",
              desc.spot_len, desc.fixed_len, desc.signal_len,
              desc.clip_qual_right, desc.num_reads );
    return res;
}

/* hardcoded values taken from asm-trace/interface/sra/sradb.h */
#define SRA_KEY_READ_DESC "NCBI:SRA:ReadDesc"
#define SRA_KEY_SPOT_DESC "NCBI:SRA:SpotDesc"

static dim_trans_fct_t vdcd_get_dim_trans_fct( const VSchema *my_schema, VTypedecl * typedecl )
{
    dim_trans_fct_t res = NULL;

    if ( my_schema == NULL ) return res;
    if ( typedecl == NULL ) return res;

    if ( vdcd_type_cmp( my_schema, typedecl, SRA_KEY_READ_DESC ) )
    {
        res = vdcd_get_read_desc_txt;
    }
    else if ( vdcd_type_cmp( my_schema, typedecl, SRA_KEY_SPOT_DESC ) )
    {
        res = vdcd_get_spot_desc_txt;
    }
/*
    if ( strcmp( name, SRA_KEY_READ_DESC ) == 0 )
    {
        res = vdcd_get_read_desc_txt;
    }
    else if ( strcmp( name, SRA_KEY_SPOT_DESC ) == 0 )
    {
        res = vdcd_get_spot_desc_txt;
    }
*/
    return res;
}


const char * const_s_Ascii = "Ascii";
const char * const_s_Unicode = "Unicode";
const char * const_s_Uint = "Uint";
const char * const_s_Int = "Int";
const char * const_s_Float = "Float";
const char * const_s_Bool = "Bool";
const char * const_s_Unknown = "unknown";

char *vdcd_make_domain_txt( const uint32_t domain )
{
    char* res= NULL;
    switch( domain )
    {
        case vtdAscii   : res = string_dup_measure( const_s_Ascii, NULL ); break;
        case vtdUnicode : res = string_dup_measure( const_s_Unicode, NULL ); break;
        case vtdUint    : res = string_dup_measure( const_s_Uint, NULL ); break;
        case vtdInt     : res = string_dup_measure( const_s_Int, NULL ); break;
        case vtdFloat   : res = string_dup_measure( const_s_Float, NULL ); break;
        case vtdBool    : res = string_dup_measure( const_s_Bool, NULL ); break;
        default : res = string_dup_measure( const_s_Unknown, NULL ); break;
    }
    return res;
}

/* a single column-definition */
p_col_def vdcd_init_col( const char* name, const size_t str_limit )
{
    p_col_def res = NULL;
    if ( name == NULL ) return res;
    if ( name[0] == 0 ) return res;
    res = ( p_col_def )calloc( 1, sizeof( col_def ) );
    if ( res != NULL )
    {
        res->name = string_dup_measure ( name, NULL );
        vds_make( &( res->content ), str_limit, DUMP_STR_INC );
    }
    return res;
}

void vdcd_destroy_col( p_col_def col_def )
{
    if ( col_def == NULL ) return;
    if ( col_def->name ) free( col_def->name );
    vds_free( &( col_def->content ) );
    free( col_def );
}

/* a vector of column-definitions */
bool vdcd_init( col_defs** defs, const size_t str_limit )
{
    bool res = false;
    if ( defs == NULL ) return res;
    (*defs) = calloc( 1, sizeof( col_defs ) );
    if ( *defs )
    {
        VectorInit( &((*defs)->cols), 0, 5 );
        (*defs)->max_colname_chars = 0;
        res = true;
    }
    ( *defs )->str_limit = str_limit;
    return res;
}

static void CC vdcd_destroy_node( void* node, void* data )
{
    vdcd_destroy_col( (p_col_def)node );
}

void vdcd_destroy( col_defs* defs )
{
    if ( defs == NULL ) return;
    VectorWhack( &(defs->cols), vdcd_destroy_node, NULL );
    free( defs );
}

static p_col_def vdcd_append_col( col_defs* defs, const char* name )
{
    p_col_def new_col = vdcd_init_col( name, defs->str_limit );
    if ( new_col != NULL )
    {
        if ( VectorAppend( &(defs->cols), NULL, new_col ) == 0 )
        {
            int len = string_size( name );
            if ( len > defs->max_colname_chars )
                defs->max_colname_chars = len;
        }
    }
    return new_col;
}


uint32_t vdcd_parse_string( col_defs* defs, const char* src, const VTable *my_table )
{
    uint32_t count, found = 0;
    char colname[MAX_COL_NAME_LEN+1];
    size_t i_dest = 0;
    if ( defs == NULL ) return false;
    if ( src == NULL ) return false;
    while ( *src )
    {
        if ( *src == ',' )
        {
            if ( i_dest > 0 )
            {
                colname[i_dest]=0;
                vdcd_append_col( defs, colname );
            }
            i_dest = 0;
        }
        else
        {
            if ( i_dest < MAX_COL_NAME_LEN ) colname[i_dest++]=*src;
        }
        src++;
    }
    if ( i_dest > 0 )
    {
        colname[i_dest]=0;
        vdcd_append_col( defs, colname );
    }
    count = VectorLength( &defs->cols );
    if ( count > 0 && my_table != NULL )
    {
        const VCursor *my_cursor;
        rc_t rc = VTableCreateCursorRead( my_table, &my_cursor );
        DISP_RC( rc, "VTableCreateCursorRead() failed" );
        if ( rc == 0 )
        {
            uint32_t idx;
            for ( idx = 0; idx < count; ++idx )
            {
                col_def *col = ( col_def * )VectorGet( &(defs->cols), idx );
                if ( col != NULL )
                {
                    rc = VCursorAddColumn( my_cursor, &(col->idx), "%s", col->name );
                    DISP_RC( rc, "VCursorAddColumn() failed" );
                    if ( rc == 0 )
                    {
                        rc = VCursorDatatype( my_cursor, col->idx,
                                  &(col->type_decl), &(col->type_desc) );
                        DISP_RC( rc, "VCursorDatatype() failed" );
                        if ( rc == 0 )
                            found++;
                    }
                }
            }
            rc = VCursorRelease( my_cursor );
            DISP_RC( rc, "VCursorRelease() failed" );
        }
    }
    return found;
}


uint32_t vdcd_extract_from_table( col_defs* defs, const VTable *my_table )
{
    uint32_t found = 0;
    KNamelist *names;
    rc_t rc = VTableListCol( my_table, &names );
    DISP_RC( rc, "VTableListCol() failed" );
    if ( rc == 0 )
    {
        const VCursor *my_cursor;
        rc = VTableCreateCursorRead( my_table, &my_cursor );
        DISP_RC( rc, "VTableCreateCursorRead() failed" );
        if ( rc == 0 )
        {
            uint32_t n;
            rc = KNamelistCount( names, &n );
            DISP_RC( rc, "KNamelistCount() failed" );
            if ( rc == 0 )
            {
                uint32_t i;
                for ( i = 0; i < n && rc ==0; ++i )
                {
                    const char *col_name;
                    rc = KNamelistGet( names, i, &col_name );
                    DISP_RC( rc, "KNamelistGet() failed" );
                    if ( rc == 0 )
                    {
                        p_col_def def = vdcd_append_col( defs, col_name );
                        rc = VCursorAddColumn( my_cursor, &(def->idx), "%s", def->name );
                        DISP_RC( rc, "VCursorAddColumn() failed" );
                        if ( rc == 0 )
                        {
                            rc = VCursorDatatype( my_cursor, def->idx,
                                      &(def->type_decl), &(def->type_desc) );
                            DISP_RC( rc, "VCursorDatatype() failed" );
                            if ( rc == 0 )
                            {
                                found++;
                            }
                        }
                    }
                }
            }
            rc = VCursorRelease( my_cursor );
            DISP_RC( rc, "VCursorRelease() failed" );
        }
        rc = KNamelistRelease( names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    return found;
}


bool vdcd_extract_from_phys_table( col_defs* defs, const VTable *my_table )
{
    bool col_defs_found = false;
    KNamelist *names;
    rc_t rc = VTableListPhysColumns( my_table, &names );
    DISP_RC( rc, "VTableListPhysColumns() failed" );
    if ( rc == 0 )
    {
        uint32_t n;
        uint32_t found = 0;
        rc = KNamelistCount( names, &n );
        DISP_RC( rc, "KNamelistCount() failed" );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < n && rc == 0; ++i )
            {
                const char *col_name;
                rc = KNamelistGet( names, i, &col_name );
                DISP_RC( rc, "KNamelistGet() failed" );
                if ( rc == 0 )
                {
                    vdcd_append_col( defs, col_name );
                    found++;
                }
            }
            col_defs_found = ( found > 0 );
        }
        rc = KNamelistRelease( names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    return col_defs_found;
}

typedef struct add_2_cur_context
{
    const VCursor *my_cursor;
    uint32_t count;
} add_2_cur_context;
typedef add_2_cur_context* p_add_2_cur_context;


static void CC vdcd_add_1_to_cursor( void *item, void *data )
{
    rc_t rc;
    p_col_def my_col_def = (p_col_def)item;
    p_add_2_cur_context ctx = (p_add_2_cur_context)data;

    if ( my_col_def == NULL ) return;
    if ( ctx == NULL ) return;
    if ( ctx->my_cursor == NULL ) return;
    rc = VCursorAddColumn( ctx->my_cursor, &(my_col_def->idx), "%s", my_col_def->name );
    DISP_RC( rc, "VCursorAddColumn() failed" );

    /***************************************************************************
    !!! extract type information !!!
    **************************************************************************/
    if ( rc == 0 )
    {
        rc = VCursorDatatype( ctx->my_cursor, my_col_def->idx,
                              &(my_col_def->type_decl), &(my_col_def->type_desc) );
        DISP_RC( rc, "VCursorDatatype() failed" );
        if ( rc == 0 )
        {
            ctx->count++;
            my_col_def->valid = true;
        }
    }
}


uint32_t vdcd_add_to_cursor( col_defs* defs, const VCursor *my_cursor )
{
    add_2_cur_context ctx;
    ctx.count = 0;
    ctx.my_cursor = my_cursor;
    VectorForEach( &(defs->cols), false, vdcd_add_1_to_cursor, &ctx );
    return ctx.count;
}


static void CC vdcd_reset_1_content( void *item, void *data )
{
    rc_t rc;
    p_col_def my_col_def = (p_col_def)item;
    if ( my_col_def == NULL ) return;
    rc = vds_clear( &(my_col_def->content) );
    DISP_RC( rc, "dump_str_clear() failed" );
}

void vdcd_reset_content( col_defs* defs )
{
    VectorForEach( &(defs->cols), false,
                   vdcd_reset_1_content, NULL );
}

static void CC vdcd_ins_1_trans_fkt( void *item, void *data )
{
    p_col_def my_col_def = ( p_col_def )item;
    const VSchema *my_schema = ( const VSchema * )data;

    if ( my_col_def == NULL ) return;
    if ( my_schema == NULL ) return;

    /* resolves special sra-types and retrieves the addr of
       a function that later can translate the values into plain-text
       --- is defined in this file! */
    my_col_def->value_trans_fct = vdcd_get_value_trans_fct( my_schema, &(my_col_def->type_decl) );
    my_col_def->dim_trans_fct = vdcd_get_dim_trans_fct( my_schema, &(my_col_def->type_decl) );
}

void vdcd_ins_trans_fkt( col_defs* defs, const VSchema *my_schema )
{
    if ( defs == NULL ) return;
    if ( my_schema == NULL ) return;
    VectorForEach( &(defs->cols), false,
                   vdcd_ins_1_trans_fkt, (void*)my_schema );
}


static void CC vdcd_exclude_column_cb( void *item, void *data )
{
    const char * s = (const char *)data;
    p_col_def my_col_def = (p_col_def)item;
    if ( s == NULL || my_col_def == NULL ) return;
    if ( strcmp( my_col_def->name, s ) == 0 )
        my_col_def->excluded = true;
}


void vdcd_exclude_this_column( col_defs* defs, const char* column_name )
{
    VectorForEach( &(defs->cols), false, vdcd_exclude_column_cb, (void*)column_name );
}


void vdcd_exclude_these_columns( col_defs* defs, const char* column_names )
{
    char colname[MAX_COL_NAME_LEN+1];
    size_t i_dest = 0;
    if ( defs == NULL || column_names == NULL ) return;
    while ( *column_names )
    {
        if ( *column_names == ',' )
        {
            if ( i_dest > 0 )
            {
                colname[i_dest]=0;
                vdcd_exclude_this_column( defs, colname );
            }
            i_dest = 0;
        }
        else
        {
            if ( i_dest < MAX_COL_NAME_LEN ) colname[i_dest++]=*column_names;
        }
        column_names++;
    }
    if ( i_dest > 0 )
    {
        colname[i_dest]=0;
        vdcd_exclude_this_column( defs, colname );
    }
}


bool vdcd_get_first_none_static_column_idx( col_defs* defs, const VCursor * cur, uint32_t * idx )
{
    bool res = false;
    if ( defs != NULL && cur !=NULL && idx != NULL )
    {
        uint32_t len = VectorLength( &(defs->cols) );
        if ( len > 0 )
        {
            uint32_t start = VectorStart( &(defs->cols) );
            uint32_t run_idx = start;
            while ( ( run_idx < ( start + len ) ) && !res )
            {
                col_def * cd = VectorGet( &(defs->cols), run_idx );
                if ( cd != NULL )
                {
                    int64_t  first;
                    uint64_t count;

                    rc_t rc = VCursorIdRange( cur, cd->idx, &first, &count );
                    if ( rc == 0 && count > 0 )
                    {
                        *idx = cd->idx;
                        res = true;
                    }
                }
                run_idx++;
            }
        }
    }
    return res;
}
