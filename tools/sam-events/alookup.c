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

#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/printf.h>

#include "allele_lookup.h"

#include <stdio.h>
#include <ctype.h>

const char UsageDefaultName[] = "alookup";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define OPTION_CACHE   "cache"
#define ALIAS_CACHE   "c"
static const char * cache_usage[]     = { "the lookup-cache to use", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CACHE,     ALIAS_CACHE,    NULL, cache_usage,     1,   true,        false }
};

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    uint32_t i;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg( "Options:\n" );

    HelpOptionsStandard ();
    for ( i = 0; i < sizeof ( ToolOptions ) / sizeof ( ToolOptions[ 0 ] ); ++i )
        HelpOptionLine ( ToolOptions[ i ].aliases, ToolOptions[ i ].name, NULL, ToolOptions[ i ].help );

    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t log_err( const char * t_fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;
    
    va_list args;
    va_start( args, t_fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, t_fmt, args );
    va_end( args );
    if ( rc == 0 )
        rc = LogMsg( klogErr, buffer );
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

typedef struct tool_ctx
{
    const char * cache_file;
} tool_ctx;


static rc_t get_charptr( const Args * args, const char *option, const char ** value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    if ( rc == 0 && count > 0 )
    {
        rc = ArgsOptionValue( args, option, 0, ( const void ** )value );
        if ( rc != 0 )
            *value = NULL;
    }
    else
        *value = NULL;
    return rc;
}


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_charptr( args, OPTION_CACHE, &ctx->cache_file );
    if ( rc == 0 )
    {
        if ( ctx->cache_file == NULL )
        {
            rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
            log_err( "cache-file is missing!" );
        }
    }
    return rc;
}

static void release_tool_ctx( tool_ctx * ctx )
{
    if ( ctx != NULL )
    {

    }
}


/* ----------------------------------------------------------------------------------------------- */

static rc_t perform_dump( tool_ctx * ctx )
{
    struct Allele_Lookup * al;
    rc_t rc = allele_lookup_make( &al, ctx->cache_file );
    if ( rc == 0 )
    {
        struct Lookup_Cursor * curs;    
        rc = lookup_cursor_make( al, &curs );
        if ( rc == 0 )
        {
            bool running = true;
            while ( running )
            {
                String key;
                uint64_t values[ 2 ];

                running = lookup_cursor_next( curs, &key, values );
                if ( running )
                {
                    KOutMsg( "%S\t%lu\t%lX\n", &key, values[ 0 ], values[ 1 ] );
                    {
                        rc_t rc1 = Quitting();
                        if ( rc1 != 0 ) running = false;
                    }
                }
            }
            lookup_cursor_release( curs );
        }
        rc = allele_lookup_release( al );
    }
    return rc;
}


static rc_t perform_lookup( Args * args, uint32_t count, const char * cache_file )
{
    struct Allele_Lookup * al;
    rc_t rc = allele_lookup_make( &al, cache_file );
    if ( rc == 0 )
    {
        uint32_t idx;
        const char * allele;
        String S;
        uint64_t values[ 2 ];
       
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            rc = ArgsParamValue( args, idx, ( const void ** )&allele );
            if ( rc != 0 )
                log_err( "ArgsParamValue( %d ) failed %R", idx, rc );
            else
            {
                StringInitCString( &S, allele );
                if ( allele_lookup_perform( al, &S, values ) )
                    rc = KOutMsg( "%S\t%lu\t%lX\n", &S, values[ 0 ], values[ 1 ] );
                else
                    rc = KOutMsg( "%S\n", &S );
            }
        }
        allele_lookup_release( al );
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                ToolOptions, sizeof ( ToolOptions ) / sizeof ( OptDef ) );
    if ( rc != 0 )
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    else
    {
        tool_ctx ctx;
        rc = fill_out_tool_ctx( args, &ctx );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = ArgsParamCount( args, &count );
            if ( rc != 0 )
                log_err( "ArgsParamCount() failed %R", rc );
            else
            {
                if ( count < 1 )
                    rc = perform_dump( &ctx );
                else
                    rc = perform_lookup( args, count, ctx.cache_file );
            }
            release_tool_ctx( &ctx );
        }
        ArgsWhack ( args );
    }
    return rc;
}
