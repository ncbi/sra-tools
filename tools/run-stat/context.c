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

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/status.h>
#include <klib/text.h>
#include <kapp/args.h>
#include <os-native.h>
#include <sysalloc.h>

#include "helper.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


static rc_t ctx_set_str( char **dst, const char *src )
{
    size_t len;
    if ( dst == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    if ( *dst != NULL )
    {
        free( *dst );
        *dst = NULL;
    }
    if ( src == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    }
    len = strlen( src );
    if ( len == 0 )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcItem, rcEmpty );
    }
    *dst = (char*)malloc( len + 1 );
    if ( *dst == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcWriting, rcMemory, rcExhausted );
    }
    strcpy( *dst, src );
    return 0;
}


static void ctx_init_values( p_stat_ctx ctx )
{
    ctx->path = NULL;
    ctx->table = NULL;
    ctx->schema_list = NULL;
    ctx->module_list = NULL;
    ctx->output_path = NULL;
    ctx->name_prefix = NULL;

    ctx->usage_requested = false;
    ctx->dont_check_accession = false;
    ctx->show_progress = false;
}

rc_t ctx_init( stat_ctx **ctx )
{
    rc_t rc = 0;
    if ( ctx == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    if ( rc == 0 )
    {
        (*ctx) = (p_stat_ctx)calloc( 1, sizeof( stat_ctx ) );
        if ( *ctx == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        if ( rc == 0 )
        {
            ctx_init_values( *ctx );
            rc = ng_make( &((*ctx)->row_generator) );
            DISP_RC( rc, "num_gen_make() failed" );
        }
    }
    return rc;
}


rc_t ctx_destroy( p_stat_ctx ctx )
{
    rc_t rc = 0;
    if ( ctx == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcDestroying, rcParam, rcNull );
    }
    if ( rc == 0 )
    {
        if ( ctx->path != NULL )
        {
            free( (void*)ctx->path );
            ctx->path = NULL;
        }

        if ( ctx->table != NULL )
        {
            free( (void*)ctx->table );
            ctx->table = NULL;
        }

        if ( ctx->name_prefix != NULL )
        {
            free( (void*)ctx->name_prefix );
            ctx->name_prefix = NULL;
        }

        if ( ctx->output_path != NULL )
        {
            free( (void*)ctx->output_path );
            ctx->output_path = NULL;
        }

        if ( ctx->schema_list != NULL )
        {
            KNamelistRelease( ctx->schema_list );
            ctx->schema_list = NULL;
        }

        if ( ctx->module_list != NULL )
        {
            KNamelistRelease( ctx->module_list );
            ctx->module_list = NULL;
        }

        ng_destroy( ctx->row_generator );
        free( ctx );
    }
    return rc;
}


static rc_t ctx_set_path( p_stat_ctx ctx, const char *src )
{
    rc_t rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    if ( ctx != NULL && src != NULL )
    {
        rc = ctx_set_str( (char**)&(ctx->path), src );
        DISP_RC( rc, "dump_context_set_str() failed" );
    }
    return rc;
}


/* not static because can be called directly from run-stat.c */
rc_t ctx_set_table( p_stat_ctx ctx, const char *src )
{
    rc_t rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    if ( ctx != NULL && src != NULL )
    {
        rc = ctx_set_str( (char**)&(ctx->table), src );
        DISP_RC( rc, "stat_context_set_str() failed" );
    }
    return rc;
}


static rc_t ctx_set_row_range( p_stat_ctx ctx, const char *src )
{
    rc_t rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    if ( ctx != NULL && src != NULL )
    {
        ng_parse( ctx->row_generator, src );
        rc = 0;
    }
    return rc;
}


static rc_t ctx_set_name_prefix( p_stat_ctx ctx, const char *src )
{
    rc_t rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    if ( ctx != NULL )
    {
        if ( src != NULL )
        {
            rc = ctx_set_str( (char**)&(ctx->name_prefix), src );
            DISP_RC( rc, "dump_context_set_str() failed" );
        }
        else
        {
            ctx->name_prefix = string_dup_measure( DEFAULT_REPORT_PREFIX, NULL );
            rc = 0;
        }
    }
    return rc;
}


static rc_t ctx_set_output_path( p_stat_ctx ctx, const char *src )
{
    rc_t rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    if ( ctx != NULL && src != NULL )
    {
        rc = ctx_set_str( (char**)&(ctx->output_path), src );
        DISP_RC( rc, "dump_context_set_str() failed" );
    }
    return rc;
}


static rc_t ctx_set_report_type( p_stat_ctx ctx, const char *src )
{
    rc_t rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
    if ( ctx != NULL )
    {
        ctx->report_type = RT_TXT; /* default */
        if ( src != NULL )
        {
            if ( strcmp( src, "csv" ) == 0 )
                ctx->report_type = RT_CSV;
            else if ( strcmp( src, "xml" ) == 0 )
                ctx->report_type = RT_XML;
            else if ( strcmp( src, "json" ) == 0 )
                ctx->report_type = RT_JSO;
        }
        rc = 0;
    }
    return rc;
}


static bool ctx_check_if_usage_necessary( p_stat_ctx ctx )
{
    if ( ctx == NULL ) return false;
    if ( ctx->path == NULL ) ctx->usage_requested = true;
    return ctx->usage_requested;
}


static rc_t ctx_evaluate_arguments( const Args *my_args,
                                    p_stat_ctx ctx )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( my_args, &count );
    DISP_RC( rc, "ArgsParamCount() failed" );
    if ( rc == 0 )
    {
        uint32_t idx;
        for ( idx=0; idx < count; ++idx )
        {
            const char *value = NULL;
            rc = ArgsParamValue( my_args, idx, &value );
            DISP_RC( rc, "ArgsParamValue() failed" );
            if ( rc == 0 )
            {
                switch( idx )
                {
                    case 0 : rc = ctx_set_path( ctx, value );
                        DISP_RC( rc, "ctx_set_path() failed" );
                        break;
                }
            }
        }
    }
    return rc;
}


static bool ctx_get_bool_option( const Args *my_args,
                                 const char *name,
                                 const bool def )
{
    bool res = def;
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( rc == 0 )
        res = ( count > 0 );
    return res;
}


#if 0
static bool ctx_get_bool_neg_option( const Args *my_args,
                                     const char *name,
                                     const bool def )
{
    bool res = def;
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( rc == 0 )
        res = ( count == 0 );
    return res;
}


static uint16_t ctx_get_uint16_option( const Args *my_args,
                                       const char *name,
                                       const uint16_t def )
{
    uint16_t res = def;
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( ( rc == 0 )&&( count > 0 ) )
    {
        const char *s;
        rc = ArgsOptionValue( my_args, name, 0,  &s );
        DISP_RC( rc, "ArgsOptionValue() failed" );
        if ( rc == 0 ) res = atoi( s );
    }
    return res;
}
#endif

static const char* ctx_get_str_option( const Args *my_args,
                                       const char *name )
{
    const char* res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    DISP_RC( rc, "ArgsOptionCount() failed" );
    if ( ( rc == 0 )&&( count > 0 ) )
    {
        rc = ArgsOptionValue( my_args, name, 0, &res );
        DISP_RC( rc, "ArgsOptionValue() failed" );
    }
    return res;
}


uint32_t context_schema_count( p_stat_ctx ctx )
{
    uint32_t res = 0;
    if ( ctx != NULL )
        if ( ctx->schema_list != 0 )
        {
            uint32_t count;
            if ( KNamelistCount( ctx->schema_list, &count ) == 0 )
                res = count;
        }
    return res;
}


static void ctx_evaluate_modules( const Args *my_args, p_stat_ctx ctx )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, OPTION_MODULE, &count );
    DISP_RC( rc, "ArgsOptionCount( OPTION_MODULE ) failed" );
    if ( ( rc == 0 )&( count > 0 ) )
    {
        VNamelist *v_names;
        rc_t rc = VNamelistMake ( &v_names, 5 );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < count; ++i )
            {
                const char *s;
                rc = ArgsOptionValue( my_args, OPTION_MODULE, i,  &s );
                DISP_RC( rc, "ArgsOptionValue(OPTION_MODULE) failed" );
                if ( rc == 0 )
                    VNamelistAppend ( v_names, s );
            }
            VNamelistToConstNamelist ( v_names, &ctx->module_list );
            VNamelistRelease( v_names );
        }
    }
}   


static void ctx_evaluate_options( const Args *my_args, p_stat_ctx ctx )
{
    if ( my_args == NULL ) return;
    if ( ctx == NULL ) return;

    ctx->dont_check_accession = ctx_get_bool_option( my_args,
                    OPTION_WITHOUT_ACCESSION, false );
    ctx->show_progress = ctx_get_bool_option( my_args,
                    OPTION_PROGRESS, false );

    ctx_set_table( ctx, ctx_get_str_option( my_args, OPTION_TABLE ) );
    ctx_set_row_range( ctx, ctx_get_str_option( my_args, OPTION_ROWS ) );

    helper_make_namelist_from_string( &(ctx->schema_list), 
                    ctx_get_str_option( my_args, OPTION_SCHEMA ), ',' );

    ctx_evaluate_modules( my_args, ctx );

    ctx->produce_grafic = ctx_get_bool_option( my_args, OPTION_GRAFIC, false );
    ctx_set_report_type( ctx, ctx_get_str_option( my_args, OPTION_REPORT ) );
    ctx_set_output_path( ctx, ctx_get_str_option( my_args, OPTION_OUTPUT ) );
    ctx_set_name_prefix( ctx, ctx_get_str_option( my_args, OPTION_PREFIX ) );
}


rc_t ctx_capture_arguments_and_options( const Args * args, p_stat_ctx ctx )
{
    rc_t rc;

    rc = ctx_evaluate_arguments( args, ctx );
    DISP_RC( rc, "ctx_evaluate_arguments() failed" );
    if ( rc == 0 )
    {
        ctx_evaluate_options( args, ctx );
        ctx_check_if_usage_necessary( ctx );

        rc = ArgsHandleLogLevel( args );
        DISP_RC( rc, "ArgsHandleLogLevel() failed" );
    }
    return rc;
}
