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
#include <string.h>
#include <assert.h>

/*
 * helper-function to set a string inside the context
 * ( makes a copy ) with error detection
*/
static rc_t context_set_str( char **dst, const char *src )
{
    size_t len;
    if ( dst == NULL )
        return RC( rcExe, rcNoTarg, rcWriting, rcParam, rcNull );

    if ( *dst != NULL )
    {
        free( *dst );
        *dst = NULL;
    }
    if ( src == NULL )
        return RC( rcExe, rcNoTarg, rcWriting, rcParam, rcNull );

    *dst = string_dup_measure ( src, &len );
 
    if ( len == 0 )
        return RC( rcExe, rcNoTarg, rcWriting, rcItem, rcEmpty );
    if ( *dst == NULL )
        return RC( rcExe, rcNoTarg, rcWriting, rcMemory, rcExhausted );
    return 0;
}


/*
 * helper-function to initialize the values of the context
*/
static void context_init_values( p_context ctx )
{
    ctx->src = NULL;
    ctx->dst_path = NULL;
    ctx->schema = NULL;
    ctx->chunk_size = 0;
    ctx->usage_requested = false;
    ctx->circular = false;
    ctx->quiet = false;
}


/*
 * generates a new context, initializes values
*/
rc_t context_init( context **ctx )
{
    if ( ctx == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    (*ctx) = (p_context)calloc( 1, sizeof( context ) );
    if ( *ctx == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );

    context_init_values( *ctx );
    return 0;
}


/*
 * destroys a context, frees all pointers the context owns
*/
rc_t context_destroy( p_context ctx )
{
    if ( ctx == NULL )
        return RC( rcExe, rcNoTarg, rcDestroying, rcParam, rcNull );

    if ( ctx->src )
    {
        free( (void*)ctx->src );
        ctx->src = NULL;
    }
    if ( ctx->dst_path )
    {
        free( (void*)ctx->dst_path );
        ctx->dst_path = NULL;
    }
    if ( ctx->schema )
    {
        free( (void*)ctx->schema );
        ctx->schema = NULL;
    }
    free( ctx );
    return 0;
}


/*
 * helper-function to set the source
*/
static rc_t context_set_src( p_context ctx, const char *src )
{
    if ( src == NULL ) return 0;
    return context_set_str( (char**)&(ctx->src), src );
}


/*
 * helper-function to set the destination-path
*/
static rc_t context_set_dst_path( p_context ctx, const char *src )
{
    if ( src == NULL ) return 0;
    return context_set_str( (char**)&(ctx->dst_path), src );
}


/*
 * helper-function to set the schema
*/
static rc_t context_set_schema( p_context ctx, const char *s )
{
    return context_set_str( (char**)&(ctx->schema), s );
}


static bool context_check_if_usage_necessary( p_context ctx )
{
    if ( ctx == NULL ) return false;
    if ( ctx->src == NULL || ctx->dst_path == NULL )
        ctx->usage_requested = true;
    return ctx->usage_requested;
}


static rc_t context_evaluate_arguments( const Args *my_args, p_context ctx )
{
    uint32_t count, idx;
    
    rc_t rc = ArgsParamCount( my_args, &count );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "ArgsParamCount() failed" );
    }
    if ( rc != 0 ) return rc;

    for ( idx = 0; idx < count && rc == 0; ++idx )
    {
        const char *value = NULL;
        rc = ArgsParamValue( my_args, idx, &value );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "ArgsParamValue() failed" );
        }
        else
        {
            switch( idx )
            {
            case 0 : rc = context_set_src( ctx, value );
                     if ( rc != 0 )
                     {
                        LOGERR( klogErr, rc, "context_set_src() failed" );
                     }
                     break;

            case 1 : rc = context_set_dst_path( ctx, value );
                     if ( rc != 0 )
                     {
                        LOGERR( klogErr, rc, "context_set_dst_path() failed" );
                     }
                     break;
            }
        }
    }
    return rc;
}


static bool context_get_bool_option( const Args *my_args,
                                     const char *name )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "ArgsOptionCount() failed" );
    }
    return ( ( rc == 0 && count > 0 ) );
}


static const char* context_get_str_option( const Args *my_args,
                                           const char *name )
{
    const char* res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "ArgsOptionCount() failed" );
    }
    else if ( count > 0 )
    {
        rc = ArgsOptionValue( my_args, name, 0, &res );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "ArgsOptionValue() failed" );
        }
    }
    return res;
}


static uint32_t context_get_uint_32_option( const Args *my_args,
                                            const char *name,
                                            const uint32_t def )
{
    uint32_t count, res = def;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "ArgsOptionCount() failed" );
    }
    else if ( count > 0 )
    {
        const char *s;
        rc = ArgsOptionValue( my_args, name, 0,  &s );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "ArgsOptionValue() failed" );
        }
        else
        {
            res = atoi( s );
        }
    }
    return res;
}


static rc_t context_evaluate_options( const Args *my_args, p_context ctx )
{
    rc_t rc = 0;
    const char* circular = NULL;

    assert( my_args != NULL );
    assert( ctx != NULL );

    context_set_src( ctx, context_get_str_option( my_args, OPTION_SRC ) );
    context_set_dst_path( ctx, context_get_str_option( my_args, OPTION_DST_PATH ) );
    context_set_schema( ctx, context_get_str_option( my_args, OPTION_SCHEMA ) );
    ctx->chunk_size     = context_get_uint_32_option( my_args, OPTION_CHUNK_SIZE, 0 );
    circular            = context_get_str_option( my_args, OPTION_CIRCULAR );
    ctx->quiet          = context_get_bool_option( my_args, OPTION_QUIET );
    ctx->force_target   = context_get_bool_option( my_args, OPTION_FORCE );
    ctx->input_is_file  = context_get_bool_option( my_args, OPTION_IFILE );
    if ( circular != NULL )
    {
        if ( strcasecmp( circular, "yes" ) == 0 )
        {
            ctx->circular = true;
        }
        else if ( strcasecmp( circular, "no" ) == 0 )
        {
            ctx->circular = false;
        }
        else
        {
            rc = RC( rcExe, rcNoTarg, rcParsing, rcParam, rcUnrecognized );
        }
    }
    else
    {
        rc = RC( rcExe, rcNoTarg, rcParsing, rcParam, rcEmpty );
    }
    return rc;
}


/*
 * reads all arguments and options, fills the context
 * with copies (if strings) of this data
*/
rc_t context_capture_arguments_and_options( const Args * args, p_context ctx )
{
    rc_t rc = context_evaluate_arguments( args, ctx );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "context_evaluate_arguments() failed" );
    }
    else
    {
        rc = context_evaluate_options( args, ctx );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "context_evaluate_options() failed" );
        }
        else
        {
            context_check_if_usage_necessary( ctx );
            rc = ArgsArgvValue( args, 0, &ctx->argv0 );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "ArgsArgvValue() failed" );
            }
            else
            {
                rc = ArgsHandleLogLevel( args );
                if ( rc != 0 )
                {
                    LOGERR( klogErr, rc, "ArgsHandleLogLevel() failed" );
                }
            }
        }
    }
    return rc;
}
