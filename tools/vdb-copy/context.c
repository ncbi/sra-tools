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

#include "context.h"
#include <sysalloc.h>
#include <stdlib.h>


/*
 * helper-function to set a string inside the context
 * ( makes a copy ) with error detection
*/
static rc_t context_set_str( char **dst, const char *src )
{
    size_t len;
    if ( dst == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );

    if ( *dst != NULL )
    {
        free( *dst );
        *dst = NULL;
    }
    if ( src == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );

    *dst = string_dup_measure ( src, &len );
 
    if ( len == 0 )
        return RC( rcVDB, rcNoTarg, rcWriting, rcItem, rcEmpty );
    if ( *dst == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcMemory, rcExhausted );
    return 0;
}


/*
 * helper-function to initialize the values of the context
*/
static void context_init_values( p_context ctx )
{
    ctx->src_path = NULL;
    ctx->dst_path = NULL;
    ctx->kfg_path = NULL;
    ctx->table = NULL;
    ctx->columns = NULL;
    ctx->excluded_columns = NULL;
    ctx->platform_id = 0;
    ctx->src_schema_list = NULL;

    ctx->usage_requested = false;
    ctx->ignore_reject = false;
    ctx->ignore_redact = false;
    ctx->dont_check_accession = false;
    ctx->reindex = false;
    ctx->show_redact = false;
    ctx->show_meta = false;
    ctx->md5_mode = MD5_MODE_AUTO;
    ctx->force_kcmInit = false;
    ctx->force_unlock = false;

    ctx->dont_remove_target = false;
    config_values_init( &(ctx->config) );
    redact_vals_init( &(ctx->rvals) );
    ctx->dst_schema_tabname = NULL;
    ctx->legacy_schema_file = NULL;
    ctx->legacy_dont_copy = NULL;
}


/*
 * generates a new context, initializes values
*/
rc_t context_init( context **ctx )
{
    rc_t rc;

    if ( ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    (*ctx) = (p_context)calloc( 1, sizeof( context ) );
    if ( *ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );

    context_init_values( *ctx );
    rc = num_gen_make( &((*ctx)->row_generator) );
    DISP_RC( rc, "num_gen_make() failed" );
    return rc;
}


/*
 * destroys a context, frees all pointers the context owns
*/
rc_t context_destroy( p_context ctx )
{
    if ( ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );

    if ( ctx->src_path != NULL )
    {
        free( (void*)ctx->src_path );
        ctx->src_path = NULL;
    }
    if ( ctx->dst_path != NULL )
    {
        free( (void*)ctx->dst_path );
        ctx->dst_path = NULL;
    }
    if ( ctx->kfg_path != NULL )
    {
        free( (void*)ctx->kfg_path );
        ctx->kfg_path = NULL;
    }
    if ( ctx->src_schema_list != NULL )
    {
        KNamelistRelease( ctx->src_schema_list );
        ctx->src_schema_list = NULL;
    }
    if ( ctx->table != NULL )
    {
        free( (void*)ctx->table );
        ctx->table = NULL;
    }
    if ( ctx->columns != NULL )
    {
        free( (void*)ctx->columns );
        ctx->columns = NULL;
    }
    if ( ctx->excluded_columns != NULL )
    {
        free( (void*)ctx->excluded_columns );
        ctx->excluded_columns = NULL;
    }

    num_gen_destroy( ctx->row_generator );
    config_values_destroy( &( ctx->config ) );
    redact_vals_destroy( ctx->rvals );

    if ( ctx->dst_schema_tabname != NULL )
    {
        free( (void*)ctx->dst_schema_tabname );
        ctx->dst_schema_tabname = NULL;
    }
    if ( ctx->legacy_schema_file != NULL )
    {
        free( (void*)ctx->legacy_schema_file );
        ctx->legacy_schema_file = NULL;
    }
    if ( ctx->legacy_dont_copy != NULL )
    {
        free( (void*)ctx->legacy_dont_copy );
        ctx->legacy_dont_copy = NULL;
    }

    free( ctx );
    return 0;
}


/*
 * clear's the number-generator and sets the given intervall
*/
rc_t context_set_range( p_context ctx, 
                        const int64_t first, const uint64_t count )
{
    rc_t rc = num_gen_clear( ctx->row_generator );
    if ( rc == 0 )
        rc = num_gen_add( ctx->row_generator, first, count );
    return rc;
}


/*
 * performs the range check to trim the internal number
 * generator to the given range
*/
rc_t context_range_check( p_context ctx, 
                          const int64_t first, const uint64_t count )
{
    return num_gen_range_check( ctx->row_generator, first, count );
}


/*
 * helper-function to set the source-path
*/
static rc_t context_set_src_path( p_context ctx, const char *src )
{
    return context_set_str( (char**)&(ctx->src_path), src );
}


/*
 * helper-function to set the destination-path
*/
static rc_t context_set_dst_path( p_context ctx, const char *src )
{
    return context_set_str( (char**)&(ctx->dst_path), src );
}


/*
 * helper-function to set the path for additional kfg-files
*/
static rc_t context_set_kfg_path( p_context ctx, const char *src )
{
    rc_t rc = context_set_str( (char**)&(ctx->kfg_path), src );
    if ( ctx->kfg_path == NULL )
        context_set_kfg_path( ctx, "." );
    return rc;
}


/*
 * helper-function to set the name of the source-table
*/
static rc_t context_set_table( p_context ctx, const char *src )
{
    rc_t rc;
    if ( ctx == NULL || src == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    rc = context_set_str( (char**)&(ctx->table), src );
    DISP_RC( rc, "context_set_table:context_set_str() failed" );
    return rc;
}

#if ALLOW_COLUMN_SPEC
static rc_t context_set_columns( p_context ctx, const char *src )
{
    rc_t rc;
    if ( ctx == NULL || src == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    rc = context_set_str( (char**)&(ctx->columns), src );
    DISP_RC( rc, "context_set_columns:dump_context_set_str() failed" );
    return rc;
}
#endif

static rc_t context_set_excluded_columns( p_context ctx, const char *src )
{
    rc_t rc;
    if ( ctx == NULL || src == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    rc = context_set_str( (char**)&(ctx->excluded_columns), src );
    DISP_RC( rc, "context_set_excluded_columns:dump_context_set_str() failed" );
    return rc;
}


static rc_t context_set_md5_mode( p_context ctx, const char *src )
{
    if ( ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    ctx->md5_mode = MD5_MODE_AUTO;
    if ( src == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    switch( src[ 0 ] )
    {
    case 'O' :
    case '1' :
    case 'o' : ctx->md5_mode = MD5_MODE_ON; break;

    case '0' :
    case 'x' :
    case 'X' : ctx->md5_mode = MD5_MODE_OFF; break;
    }
    return 0;
}


static rc_t context_set_blob_checksum( p_context ctx, const char *src )
{
    if ( ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    ctx->blob_checksum = BLOB_CHECKSUM_AUTO;
    if ( src == NULL )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    switch( src[ 0 ] )
    {
    case 'O' :
    case '1' :
    case 'o' : ctx->blob_checksum = BLOB_CHECKSUM_CRC32; break;

    case '5' :
    case 'M' :
    case 'm' : ctx->blob_checksum = BLOB_CHECKSUM_MD5; break;

    case '0' :
    case 'x' :
    case 'X' : ctx->blob_checksum = BLOB_CHECKSUM_OFF; break;
    }
    return 0;
}


static rc_t context_set_row_range( p_context ctx, const char *src )
{
    if ( ( ctx == NULL )||( src == NULL ) )
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    return num_gen_parse( ctx->row_generator, src );
}


static bool context_check_if_usage_necessary( p_context ctx )
{
    if ( ctx == NULL ) return false;
    if ( ctx->src_path == NULL || ctx->dst_path == NULL )
        ctx->usage_requested = true;
    return ctx->usage_requested;
}


static rc_t context_evaluate_arguments( const Args *my_args, p_context ctx )
{
    uint32_t count, idx;
    
    rc_t rc = ArgsParamCount( my_args, &count );
    DISP_RC( rc, "ArgsParamCount() failed" );
    if ( rc != 0 ) return rc;

    for ( idx = 0; idx < count && rc == 0; ++idx )
    {
        const char *value = NULL;
        rc = ArgsParamValue( my_args, idx, &value );
        DISP_RC( rc, "ArgsParamValue() failed" );
        if ( rc == 0 )
        {
            switch( idx )
            {
            case 0 : rc = context_set_src_path( ctx, value );
                     DISP_RC( rc, "context_set_src_path() failed" );
                     break;

            case 1 : rc = context_set_dst_path( ctx, value );
                     DISP_RC( rc, "context_set_dst_path() failed" );
                     break;

            }
        }
    }
    return rc;
}


static bool context_get_bool_option( const Args *my_args,
                                     const char *name,
                                     const bool def )
{
    bool res = def;
    uint32_t count = 0;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( rc == 0 && count > 0 )
        res = true;
    return res;
}


static const char* context_get_str_option( const Args *my_args,
                                           const char *name )
{
    const char* res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( ( rc == 0 )&&( count > 0 ) )
    {
        rc = ArgsOptionValue( my_args, name, 0, &res );
    }
    return res;
}


/*
 * returns the number of schema's given on the commandline
*/
uint32_t context_schema_count( p_context ctx )
{
    uint32_t res = 0;
    if ( ctx != NULL )
        if ( ctx->src_schema_list != 0 )
        {
            uint32_t count;
            if ( KNamelistCount( ctx->src_schema_list, &count ) == 0 )
                res = count;
        }
    return res;
}


static void context_evaluate_options( const Args *my_args, p_context ctx )
{
    if ( my_args == NULL ) return;
    if ( ctx == NULL ) return;

    ctx->dont_check_accession = context_get_bool_option( my_args, OPTION_WITHOUT_ACCESSION, false );
    ctx->ignore_reject = context_get_bool_option( my_args, OPTION_IGNORE_REJECT, true );
    ctx->ignore_redact = context_get_bool_option( my_args, OPTION_IGNORE_REDACT, false );
    ctx->show_matching = context_get_bool_option( my_args, OPTION_SHOW_MATCHING, false );
    ctx->show_progress = context_get_bool_option( my_args, OPTION_SHOW_PROGRESS, false );
    ctx->ignore_incomp = context_get_bool_option( my_args, OPTION_IGNORE_INCOMP, false );
    ctx->reindex       = context_get_bool_option( my_args, OPTION_REINDEX, true );
    ctx->show_redact   = context_get_bool_option( my_args, OPTION_SHOW_REDACT, false );
    ctx->show_meta     = context_get_bool_option( my_args, OPTION_SHOW_META, false );
    ctx->force_kcmInit = context_get_bool_option( my_args, OPTION_FORCE, false );
    ctx->force_unlock  = context_get_bool_option( my_args, OPTION_UNLOCK, false );

    context_set_md5_mode( ctx, context_get_str_option( my_args, OPTION_MD5_MODE ) );
    context_set_blob_checksum( ctx, context_get_str_option( my_args, OPTION_BLOB_CHECKSUM ) );

#if ALLOW_EXTERNAL_CONFIG
    context_set_kfg_path( ctx, context_get_str_option( my_args, OPTION_KFG_PATH ) );
#endif

    context_set_table( ctx, context_get_str_option( my_args, OPTION_TABLE ) );
#if ALLOW_COLUMN_SPEC
    context_set_columns( ctx, context_get_str_option( my_args, OPTION_COLUMNS ) );
#endif
    context_set_excluded_columns( ctx, context_get_str_option( my_args, OPTION_EXCLUDED_COLUMNS ) );
    context_set_row_range( ctx, context_get_str_option( my_args, OPTION_ROWS ) );
    nlt_make_namelist_from_string( &(ctx->src_schema_list), 
                                   context_get_str_option( my_args, OPTION_SCHEMA ) );
}


/*
 * reads all arguments and options, fills the context
 * with copies (if strings) of this data
*/
rc_t context_capture_arguments_and_options( const Args * args, p_context ctx )
{
    rc_t rc;

    rc = context_evaluate_arguments( args, ctx );
    DISP_RC( rc, "evaluate_arguments() failed" );
    if ( rc == 0 )
    {
        context_evaluate_options( args, ctx );
        context_check_if_usage_necessary( ctx );

        rc = ArgsHandleLogLevel( args );
        DISP_RC( rc, "ArgsHandleLogLevel() failed" );
    }
    return rc;
}
