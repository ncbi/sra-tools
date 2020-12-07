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

#include "vdb-dump-context.h"
#include "vdb-dump-helper.h"

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/num-gen.h>
#include <kapp/args.h>

#include <kfg/config.h> /* KConfigSetNgcFile */

#include <os-native.h>
#include <sysalloc.h>

#include <strtol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/********************************************************************
the dump context contains all informations needed to execute the dump
********************************************************************/

static rc_t vdco_set_str( char **dst, const char *src )
{
    size_t len;
    if ( NULL == dst )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    if ( NULL != *dst )
    {
        free( *dst );
        *dst = NULL;
    }
    if ( NULL == src )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    len = string_size( src );
    if ( 0 == len )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcItem, rcEmpty );
    }
    *dst = (char*)malloc( len + 1 );
    if ( NULL == *dst )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcMemory, rcExhausted );
    }
    string_copy( *dst, len + 1, src, len );
    return 0;
}


static rc_t vdco_set_String( char **dst, const String *src )
{
    if ( NULL == dst )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    if ( NULL != *dst )
    {
        free( *dst );
        *dst = NULL;
    }
    if ( NULL == src )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    if ( 0 == src -> len )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcItem, rcEmpty );
    }
    *dst = ( char* )malloc( src -> size + 1 );
    if ( NULL == *dst )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcMemory, rcExhausted );
    }
    string_copy( *dst, src -> len + 1, src -> addr, src -> len );
    return 0;
}

static void vdco_init_values( p_dump_context ctx )
{
    ctx -> path = NULL;
    ctx -> table = NULL;
    ctx -> columns = NULL;
    ctx -> excluded_columns = NULL;
    ctx -> filter = NULL;
    ctx -> idx_range = NULL;
    ctx -> output_file = NULL;
    ctx -> output_path = NULL;
    ctx -> rows = NULL;

    ctx -> print_row_id = true;
    ctx -> print_in_hex = false;
    ctx -> lf_after_row = 1;
    ctx -> print_column_names = true;
    ctx -> print_dna_bases = false;
    ctx -> max_line_len = 0;
    ctx -> indented_line_len = 0;
    ctx -> phase = 0;
    ctx -> slice_depth = 0;

    ctx -> help_requested = false;
    ctx -> usage_requested = false;
    ctx -> schema_dump_requested = false;
    ctx -> table_enum_requested = false;
    ctx -> version_requested = false;
    ctx -> column_enum_requested = false;
    ctx -> column_enum_short = false;
    ctx -> id_range_requested = false;
    ctx -> without_sra_types = false;
    ctx -> print_num_elem = false;
    ctx -> objver_requested = false;
    ctx -> objts_requested = false;
    ctx -> objtype_requested = false;
    ctx -> idx_enum_requested = false;
    ctx -> idx_range_requested = false;
    ctx -> disable_multithreading = false;
    ctx -> table_defined = false;
    ctx -> show_spotgroups = false;
    ctx -> show_spread = false;
    ctx -> len_spread = false;
    ctx -> interactive = false; 
    ctx -> append = false;
}

rc_t vdco_init( dump_context **ctx )
{
    rc_t rc = 0;
    if ( NULL == ctx )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    if ( 0 == rc )
    {
        ( *ctx ) = ( p_dump_context )calloc( 1, sizeof **ctx );
        if ( NULL == *ctx )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        if ( rc == 0 )
        {
            VectorInit( &( ( *ctx ) -> schema_list ), 0, 5 );
            vdco_init_values( *ctx );
        }
    }
    return rc;
}

static void CC vdco_schema_list_entry_whack( void *item, void *data )
{
    free( item );
}

rc_t vdco_destroy( p_dump_context ctx )
{
    if ( NULL == ctx )
    {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );
    }
    VectorWhack( &( ctx -> schema_list ), vdco_schema_list_entry_whack, NULL );

    if ( NULL != ctx -> table )
    {
        free( ( void* )ctx -> table );
        ctx -> table = NULL;
    }

    if ( NULL != ctx -> columns )
    {
        free( ( void* )ctx -> columns );
        ctx -> columns = NULL;
    }

    if ( NULL != ctx -> excluded_columns )
    {
        free( ( void* )ctx -> excluded_columns );
        ctx -> excluded_columns = NULL;
    }

    if ( NULL != ctx -> row_range )
    {
        free( ( void* )ctx -> row_range );
        ctx -> row_range = NULL;
    }


    if ( NULL != ctx -> output_path )
    {
        free( ( void* )ctx -> output_path );
        ctx -> output_path = NULL;
    }

    if ( NULL != ctx -> output_file )
    {
        free( ( void* )ctx -> output_file );
        ctx -> output_file = NULL;
    }

    if ( NULL != ctx -> rows )
    {
        num_gen_destroy( ctx -> rows );
        ctx -> rows = NULL;
    }
    free( ctx );
    return 0;
}


static rc_t vdco_add_schema( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    char *s = string_dup_measure ( src, NULL );
    if ( s != NULL )
    {
        rc = VectorAppend( &( ctx -> schema_list ), NULL, s );
        DISP_RC( rc, "VectorAppend() failed" );
        if ( 0 != rc )
        {
            free( s );
        }
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    return rc;
}

static rc_t vdco_set_filter( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> filter ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}

/* not static because can be called directly from vdb-dump.c */
rc_t vdco_set_table( p_dump_context ctx, const char * src )
{
    rc_t rc;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> table ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}


rc_t vdco_set_table_String( p_dump_context ctx, const String * src )
{
    rc_t rc;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_String( ( char** )&( ctx -> table ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}


static rc_t vdco_set_columns( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> columns ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}

static rc_t vdco_set_excluded_columns( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> excluded_columns ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}

static rc_t vdco_set_row_range( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> row_range ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    if ( 0 == rc )
    {
        if ( NULL != ctx -> rows )
        {
            num_gen_destroy( ctx -> rows );
            ctx -> rows = NULL;
        }
        rc = num_gen_make_from_str_sorted( &ctx -> rows, src, ctx -> merge_ranges );
        DISP_RC( rc, "num_gen_make_from_str() failed" );
    }
    return rc;
}

static rc_t vdco_set_idx_range( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> idx_range ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}

static rc_t vdco_set_output_file( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> output_file ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}

static rc_t vdco_set_output_path( p_dump_context ctx, const char *src )
{
    rc_t rc = 0;
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    rc = vdco_set_str( ( char** )&( ctx -> output_path ), src );
    DISP_RC( rc, "vdco_set_str() failed" );
    return rc;
}


static bool vdco_set_format( p_dump_context ctx, const char *src )
{
    if ( ( NULL == ctx ) || ( NULL == src ) )
    {
        return false;
    }
    if ( 0 == strcmp( src, "csv" ) )
    {
        ctx -> format = df_csv;
    }
    else if ( 0 == strcmp( src, "xml" ) )
    {
        ctx -> format = df_xml;
    }
    else if ( 0 == strcmp( src, "json" ) )
    {
        ctx -> format = df_json;
    }
    else if ( 0 == strcmp( src, "piped" ) )
    {
        ctx -> format = df_piped;
    }
    else if ( 0 == strcmp( src, "sra-dump" ) )
    {
        ctx -> format = df_sra_dump;
    }
    else if ( 0 == strcmp( src, "tab" ) )
    {
        ctx -> format = df_tab;
    }
    else if ( 0 == strcmp( src, "fastq" ) )
    {
        ctx -> format = df_fastq;
    }
    else if ( 0 == strcmp( src, "fastq1" ) )
    {
        ctx -> format = df_fastq1;
    }
    else if ( 0 == strcmp( src, "fasta" ) )
    {
        ctx -> format = df_fasta;
    }
    else if ( 0 == strcmp( src, "fasta1" ) )
    {
        ctx -> format = df_fasta1;
    }
    else if ( 0 == strcmp( src, "fasta2" ) )
    {
        ctx -> format = df_fasta2;
    }
    else if ( 0 == strcmp( src, "qual" ) )
    {
        ctx -> format = df_qual;
    }
    else if ( 0 == strcmp( src, "qual1" ) )
    {
        ctx -> format = df_qual1;
    }
    else if ( 0 == strcmp( src, "sql" ) )
    {
        ctx -> format = df_sql;
    }
    else
    {
        ctx -> format = df_default;
    }
    return true;
}

static bool vdco_set_boolean_char( dump_context *ctx, const char * src )
{
    if ( NULL == ctx || NULL == src )
    {
        return false;
    }
    ctx  ->  c_boolean = 0;    
    if ( 0 == strcmp( src, "T" ) )
    {
        ctx -> c_boolean = 'T';
    }
    else if ( 0 == strcmp( src, "1" ) )
    {
        ctx -> c_boolean = '1';
    }
    return true;
}

static bool vdco_get_bool_option( const Args *args, const char *name, const bool def )
{
    bool res = def;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( 0 == rc )
    {
        res = ( count > 0 );
    }
    return res;
}

static bool vdco_get_bool_neg_option( const Args *args, const char *name, const bool def )
{
    bool res = def;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( 0 == rc )
    {
        res = ( 0 == count );
    }
    return res;
}

static uint16_t vdco_get_uint16_option( const Args *args, const char *name, const uint16_t def )
{
    uint16_t res = def;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( ( 0 == rc ) && ( count > 0 ) )
    {
        const char *s;
        rc = ArgsOptionValue( args, name, 0, ( const void ** )&s );
        DISP_RC( rc, "ArgsOptionValue() failed" );
        if ( 0 == rc )
        {
            res = atoi( s );
        }
    }
    return res;
}

static size_t vdco_get_size_t_option( const Args *args, const char *name, const size_t def )
{
    size_t res = def;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( ( 0 == rc ) && ( count > 0 ) )
    {
        const char *s;
        rc = ArgsOptionValue( args, name, 0, ( const void ** )&s );
        DISP_RC( rc, "ArgsOptionValue() failed" );
        if ( 0 == rc )
        {
            char *endp;
            res = strtou64( s, &endp, 10 );
        }
    }
    return res;
}

static const char* vdco_get_str_option( const Args *args, const char *name )
{
    const char* res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( ( 0 == rc ) && ( count > 0 ) )
    {
        rc = ArgsOptionValue( args, name, 0, ( const void** )&res );
        DISP_RC( rc, "ArgsOptionValue() failed" );
    }
    return res;
}

void vdco_set_schemas( const Args *args, p_dump_context ctx )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, OPTION_SCHEMA, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( ( 0 == rc ) && ( count > 0 ) )
    {
        uint32_t i;
        for ( i = 0; ( 0 == rc ) && ( i < count ); ++i )
        {
            const char* txt = NULL;
            rc = ArgsOptionValue( args, OPTION_SCHEMA, i, ( const void** )&txt );
            DISP_RC( rc, "ArgsOptionValue() failed" );
            if ( ( 0 == rc ) && ( NULL != txt ) )
            {
                rc = vdco_add_schema( ctx, txt );
                DISP_RC( rc, "dump_context_add_schema() failed" );
            }
        }
    }
}

size_t vdco_schema_count( p_dump_context ctx )
{
    if ( NULL == ctx )
    {
        return 0;
    }
    return VectorLength( &( ctx -> schema_list ) );
}

static void vdco_evaluate_options( const Args *args, dump_context *ctx )
{
    if ( ( NULL == args ) || ( NULL == ctx ) )
    {
        return;
    }

    ctx -> help_requested = vdco_get_bool_option( args, OPTION_HELP, false );
    ctx -> print_row_id = vdco_get_bool_option( args, OPTION_ROW_ID_ON, false );
    ctx -> lf_after_row = vdco_get_uint16_option( args, OPTION_LINE_FEED, 1 );
    ctx -> phase = vdco_get_uint16_option( args, OPTION_PHASE, 0 );
    ctx -> print_column_names = vdco_get_bool_neg_option( args, OPTION_COLNAME_OFF, true );
    ctx -> print_in_hex = vdco_get_bool_option( args, OPTION_IN_HEX, false );
    ctx -> schema_dump_requested = vdco_get_bool_option( args, OPTION_SCHEMA_DUMP, false );
    ctx -> table_enum_requested = vdco_get_bool_option( args, OPTION_TABLE_ENUM, false );
    ctx -> version_requested = vdco_get_bool_option( args, OPTION_VERSION, false );
    ctx -> column_enum_requested = vdco_get_bool_option( args, OPTION_COLUMN_ENUM, false );
    ctx -> column_enum_short = vdco_get_bool_option( args, OPTION_COLUMN_SHORT, false );
    ctx -> print_dna_bases = vdco_get_bool_option( args, OPTION_DNA_BASES, false );
    ctx -> objver_requested = vdco_get_bool_option( args, OPTION_OBJVER, false );
    ctx -> objts_requested = vdco_get_bool_option( args, OPTION_OBJTS, false );
    ctx -> objtype_requested = vdco_get_bool_option( args, OPTION_OBJTYPE, false );
    ctx -> max_line_len = vdco_get_uint16_option( args, OPTION_MAX_LINE_LEN, 0 );
    ctx -> indented_line_len = vdco_get_uint16_option( args, OPTION_LINE_INDENT, 0 );
    ctx -> id_range_requested = vdco_get_bool_option( args, OPTION_ID_RANGE, false );
    vdco_set_format( ctx, vdco_get_str_option( args, OPTION_FORMAT ) );
    ctx -> without_sra_types = vdco_get_bool_option( args, OPTION_WITHOUT_SRA, false );
    ctx -> print_num_elem = vdco_get_bool_option( args, OPTION_NUMELEM, false );
    ctx -> sum_num_elem = vdco_get_bool_option( args, OPTION_NUMELEMSUM, false );
    ctx -> show_kdb_blobs = vdco_get_bool_option( args, OPTION_SHOW_KDB_BLOBS, false );
    ctx -> show_vdb_blobs = vdco_get_bool_option( args, OPTION_SHOW_VDB_BLOBS, false );
    ctx -> enum_phys = vdco_get_bool_option( args, OPTION_ENUM_PHYS, false );
    ctx -> enum_readable = vdco_get_bool_option( args, OPTION_ENUM_READABLE, false );
    ctx -> enum_static = vdco_get_bool_option( args, OPTION_ENUM_STATIC, false );
    ctx -> idx_enum_requested = vdco_get_bool_option( args, OPTION_IDX_ENUM, false );
    ctx -> disable_multithreading = vdco_get_bool_option( args, OPTION_NO_MULTITHREAD, false );
    ctx -> print_info = vdco_get_bool_option( args, OPTION_INFO, false );
    ctx -> show_spotgroups = vdco_get_bool_option( args, OPTION_SPOTGROUPS, false );
    ctx -> merge_ranges = vdco_get_bool_option( args, OPTION_MERGE_RANGES, false );
    ctx -> show_spread = vdco_get_bool_option( args, OPTION_SPREAD, false );
    ctx -> len_spread = vdco_get_bool_option( args, OPTION_LEN_SPREAD, false );
    ctx -> slice_depth = vdco_get_uint16_option( args, OPTION_SLICE, 0 );
    ctx -> append = vdco_get_bool_option( args, OPTION_APPEND, false );
    
    ctx -> cur_cache_size = vdco_get_size_t_option( args, OPTION_CUR_CACHE, CURSOR_CACHE_SIZE );
    ctx -> output_buffer_size = vdco_get_size_t_option( args, OPTION_OUT_BUF_SIZE, DEF_OPTION_OUT_BUF_SIZE );
    
    if ( vdco_get_bool_option( args, OPTION_GZIP, false ) )
    {
        ctx -> compress_mode = orm_gzip;
    }
    else if ( vdco_get_bool_option( args, OPTION_BZIP2, false ) )
    {
        ctx -> compress_mode = orm_bzip2;
    }
    else
    {
        ctx -> compress_mode = orm_uncompressed;
    }

    vdco_set_table( ctx, vdco_get_str_option( args, OPTION_TABLE ) );
    ctx -> table_defined = ( NULL != ctx -> table );
    
    vdco_set_columns( ctx, vdco_get_str_option( args, OPTION_COLUMNS ) );
    vdco_set_excluded_columns( ctx, vdco_get_str_option( args, OPTION_EXCLUDED_COLUMNS ) );
    vdco_set_row_range( ctx, vdco_get_str_option( args, OPTION_ROWS ) );
    vdco_set_idx_range( ctx, vdco_get_str_option( args, OPTION_IDX_RANGE ) );
    vdco_set_output_file( ctx, vdco_get_str_option( args, OPTION_OUT_FILE ) );
    vdco_set_output_path( ctx, vdco_get_str_option( args, OPTION_OUT_PATH ) );

    ctx -> idx_range_requested = ( ctx -> idx_range != NULL );
    vdco_set_schemas( args, ctx );
    vdco_set_filter( ctx, vdco_get_str_option( args, OPTION_FILTER ) );
    vdco_set_boolean_char( ctx, vdco_get_str_option( args, OPTION_BOOLEAN ) );

    {
        const char * ngc = vdco_get_str_option( args, OPTION_NGC );
        if ( NULL != ngc )
        {
            KConfigSetNgcFile( ngc );
        }
    }

    if ( df_sra_dump == ctx -> format )
    {
        ctx -> without_sra_types = true;
    }
}

rc_t vdco_capture_arguments_and_options( const Args * args, dump_context *ctx )
{
    rc_t rc;

    vdco_evaluate_options( args, ctx );

    rc = ArgsHandleLogLevel( args );
    DISP_RC( rc, "ArgsHandleLogLevel() failed" );
    return rc;
}
