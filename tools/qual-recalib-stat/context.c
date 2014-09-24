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
 * generates a new context, initializes values
*/
rc_t context_init( context **ctx )
{
    rc_t rc;

    if ( ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    (*ctx) = (p_context)calloc( 1, sizeof **ctx );
    if ( *ctx == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );

    /* because of calloc the context is zero'd out
       default-values can be set here: */

    rc = num_gen_make( &((*ctx)->row_generator) );
    if ( rc != 0 )
        OUTMSG(( "num_gen_make() failed %r\n", rc ));
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
    if ( ctx->output_file_path != NULL )
    {
        free( (void*)ctx->output_file_path );
        ctx->output_file_path = NULL;
    }
    if ( ctx->output_mode != NULL )
    {
        free( (void*)ctx->output_mode );
        ctx->output_mode = NULL;
    }
    if ( ctx->src_schema_list != NULL )
    {
        KNamelistRelease( ctx->src_schema_list );
        ctx->src_schema_list = NULL;
    }
    num_gen_destroy( ctx->row_generator );
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
 * helper-function to set the output-path
*/
static rc_t context_set_out_file_path( p_context ctx, const char *src )
{
    return context_set_str( (char**)&(ctx->output_file_path), src );
}


/*
 * helper-function to set the output-mode
*/
static rc_t context_set_out_mode( p_context ctx, const char *src )
{
    return context_set_str( (char**)&(ctx->output_mode), src );
}


/*
 * helper-function to set path to exclude-db
*/
static rc_t context_set_exclude_path( p_context ctx, const char *src )
{
    return context_set_str( (char**)&(ctx->exclude_file_path), src );
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
    if ( ctx->src_path == NULL )
        ctx->usage_requested = true;
    return ctx->usage_requested;
}


static rc_t context_evaluate_arguments( const Args *my_args, p_context ctx )
{
    uint32_t count, idx;
    
    rc_t rc = ArgsParamCount( my_args, &count );
    if ( rc != 0 )
    {
        OUTMSG(( "ArgsParamCount() failed %R\n", rc ));
        return rc;
    }

    for ( idx = 0; idx < count && rc == 0; ++idx )
    {
        const char *value = NULL;
        rc = ArgsParamValue( my_args, idx, &value );
        if ( rc != 0 )
        {
            OUTMSG(( "ArgsParamValue() failed %R\n", rc ));
        }
        else
        {
            switch( idx )
            {
            case 0 : rc = context_set_src_path( ctx, value );
                     if ( rc != 0 )
                        OUTMSG(( "context_set_src_path() failed %R\n", rc ));
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


static uint32_t context_get_int_option( const Args *my_args,
                                        const char *name,
                                        const uint32_t def )
{
    uint32_t count, res = def;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( ( rc == 0 )&&( count > 0 ) )
    {
        const char *s;
        rc = ArgsOptionValue( my_args, name, 0,  &s );
        if ( rc == 0 ) res = atoi( s );
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

    ctx->show_progress = context_get_bool_option( my_args, OPTION_SHOW_PROGRESS, false );
    ctx->info = context_get_bool_option( my_args, OPTION_INFO, false );
    ctx->ignore_mismatch = context_get_bool_option( my_args, OPTION_IGNORE_MISMATCH, false );
    context_set_row_range( ctx, context_get_str_option( my_args, OPTION_ROWS ) );
    nlt_make_namelist_from_string( &(ctx->src_schema_list), 
                                   context_get_str_option( my_args, OPTION_SCHEMA ) );
    context_set_out_file_path( ctx, context_get_str_option( my_args, OPTION_OUTFILE ) );
    context_set_out_mode( ctx, context_get_str_option( my_args, OPTION_OUTMODE ) );
    if ( ctx->output_mode == NULL )
        context_set_out_mode( ctx, "file" );
    ctx->gc_window = context_get_int_option( my_args, OPTION_GCWINDOW, 7 );
    context_set_exclude_path( ctx, context_get_str_option( my_args, OPTION_EXCLUDE ) );
}


/*
 * reads all arguments and options, fills the context
 * with copies (if strings) of this data
*/
rc_t context_capture_arguments_and_options( const Args * args, p_context ctx )
{
    rc_t rc;

    rc = context_evaluate_arguments( args, ctx );
    if ( rc != 0 )
    {
        OUTMSG(( "context_evaluate_arguments() failed %R\n", rc ));
    }
    else
    {
        context_evaluate_options( args, ctx );
        context_check_if_usage_necessary( ctx );

        rc = ArgsHandleLogLevel( args );
        if ( rc != 0 )
            OUTMSG(( "ArgsHandleLogLevel() failed %R\n", rc ));
    }
    return rc;
}
