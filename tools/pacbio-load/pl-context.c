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

#include "pl-context.h"
#include <sysalloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static char * ctx_set_str( const char *src, const char *dflt )
{
    char * res = NULL;
    if ( src != NULL && src[0] != 0 )
        res = string_dup_measure ( src, NULL );
    else if ( dflt != NULL && dflt[0] != 0 )
        res = string_dup_measure ( dflt, NULL );
    return res;
}


static rc_t ctx_get_params( const Args * args, context *ctx )
{
    uint32_t idx, count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
        LOGERR( klogErr, rc, "ArgsParamCount failed" );
    else
    {
        if ( count < 1 )
        {
            rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
            LOGERR( klogErr, rc, "hdf5-source-file missing" );
            Usage ( args );
        }
        else for ( idx = 0; idx < count; ++idx )
        {
            const char *parameter = NULL;
            rc = ArgsParamValue( args, idx, &parameter );
            if ( rc != 0 )
                LOGERR( klogErr, rc, "error reading commandline-parameter" );
            else
                rc = VNamelistAppend ( ctx->src_paths, parameter );
        }
    }
    return rc;
}


static bool ctx_get_bool( const Args *args, const char *name, const bool def )
{
    uint32_t count = 0;
    if ( ArgsOptionCount( args, name, &count ) == 0 )
        return ( count > 0 );
    else
        return def;
}


static const char* ctx_get_str( const Args *args, const char *name, const char *def )
{
    const char * res = def;
    uint32_t count = 0;
    if ( ArgsOptionCount( args, name, &count ) == 0 && count > 0 )
    {
        if ( ArgsOptionValue( args, name, 0, &res ) != 0 )
            res = def;
    }
    return res;
}


void ctx_free( context *ctx )
{
    if ( ctx->dst_path != NULL )
        free( ctx->dst_path );
    if ( ctx->schema_name != NULL )
        free( ctx->schema_name );
    if ( ctx->tabs != NULL )
        free( ctx->tabs );
    VNamelistRelease ( ctx->src_paths );
}


rc_t ctx_init( const Args * args, context *ctx )
{
    rc_t rc;

    ctx->dst_path = NULL;
    ctx->schema_name = NULL;
    ctx->tabs = NULL;
    ctx->force = false;
    ctx->with_progress = false;

    rc = VNamelistMake ( &ctx->src_paths, 5 );
    if ( rc == 0 )
    {
        rc = ctx_get_params( args, ctx );
        if ( rc == 0 )
        {
            ctx->force = ctx_get_bool( args, OPTION_FORCE, false );
            ctx->with_progress = ctx_get_bool( args, OPTION_WITH_PROGRESS, false );
            ctx->schema_name = ctx_set_str( ctx_get_str( args, OPTION_SCHEMA, DFLT_SCHEMA ), DFLT_SCHEMA );
            ctx->dst_path = ctx_set_str( ctx_get_str( args, OPTION_OUTPUT, NULL ), NULL );
            ctx->tabs = ctx_set_str( ctx_get_str( args, OPTION_TABS, NULL ), NULL );
        }
        if ( rc == 0 )
        {
            if ( ctx->dst_path == NULL )
            {
                rc = RC( rcExe, rcArgv, rcReading, rcParam, rcInvalid );
                LOGMSG( klogErr, "vdb-output-directory missing!" );
            }
        }
    }
    if ( rc != 0 )
        ctx_free( ctx );
    return rc;
}


rc_t ctx_show( context * ctx )
{
    rc_t rc;
    uint32_t idx, count;

    KLogLevel tmp_lvl = KLogLevelGet();
    KLogLevelSet( klogInfo );

    LOGMSG( klogInfo, "pacbio-load:" );

    rc = VNameListCount ( ctx->src_paths, &count );
    if ( rc == 0 && count > 0 )
    {
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char * name = NULL;
            rc = VNameListGet ( ctx->src_paths, idx, &name );
            if ( rc == 0 && name != NULL )
                PLOGMSG( klogInfo, ( klogInfo, "   from    : '$(SRC)'", "SRC=%s", name ));
        }
    }

    PLOGMSG( klogInfo, ( klogInfo, "   into    : '$(SRC)'", "SRC=%s", ctx->dst_path ));
    PLOGMSG( klogInfo, ( klogInfo, "   schema  : '$(SRC)'", "SRC=%s", ctx->schema_name ));
    if ( ctx->force )
        LOGMSG( klogInfo, "   force   : 'yes'" );
    else
        LOGMSG( klogInfo, "   force   : 'no'" );
    if ( ctx->tabs != NULL )
        PLOGMSG( klogInfo, ( klogInfo, "   tabs    : '$(SRC)'", "SRC=%s", ctx->tabs ));

    KLogLevelSet( tmp_lvl );
    return rc;
}


static bool ctx_ld_module( context * ctx, const char c )
{
    if ( ctx->tabs == NULL )
        return true;
    else
        return ( strchr( ctx->tabs, c ) != NULL );
}


bool ctx_ld_sequence( context * ctx )
{
    return ctx_ld_module( ctx, 'S' );
}


bool ctx_ld_consensus( context * ctx )
{
    return ctx_ld_module( ctx, 'C' );
}


bool ctx_ld_passes( context * ctx )
{
    return ctx_ld_module( ctx, 'P' );
}


bool ctx_ld_metrics( context * ctx )
{
    return ctx_ld_module( ctx, 'M' );
}
