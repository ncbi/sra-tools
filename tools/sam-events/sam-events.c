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

#include <os-native.h>
#include <sysalloc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char UsageDefaultName[] = "sam-events";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define OPTION_CANON   "canonicalize"
#define ALIAS_CANON    "c"
static const char * canon_usage[]     = { "canonicalize events", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CANON,     ALIAS_CANON,    NULL, canon_usage,     1,   false,       false }
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

    KOutMsg ( "Options:\n" );

    HelpOptionsStandard ();
    for ( i = 0; i < sizeof ( ToolOptions ) / sizeof ( ToolOptions[ 0 ] ); ++i )
        HelpOptionLine ( ToolOptions[ i ].aliases, ToolOptions[ i ].name, NULL, ToolOptions[ i ].help );

    return rc;
}


typedef struct tool_ctx
{
    bool canonicalize;
} tool_ctx;


static rc_t get_bool( Args * args, const char *option, bool *value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    *value = ( rc == 0 && count > 0 );
    return rc;
}

static rc_t get_tool_ctx( Args * args, tool_ctx * ctx )
{
    rc_t rc = get_bool( args, OPTION_CANON, &ctx->canonicalize );
    return rc;
}

static rc_t produce_events( const tool_ctx * ctx, const char * src )
{
    rc_t rc = KOutMsg( "produce_events( '%s' )\n", src );
    
    return rc;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                ToolOptions, sizeof ( ToolOptions ) / sizeof ( OptDef ) );
    if ( rc == 0 )
    {
        tool_ctx ctx;
        rc = get_tool_ctx( args, &ctx );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = ArgsParamCount( args, &count );
            if ( rc == 0 )
            {
                if ( count < 1 )
                    rc = Usage( args );
                else
                {
                    uint32_t idx;
                    for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        const char * src;
                        rc = ArgsParamValue( args, idx, ( const void ** )&src );
                        if ( rc == 0 )
                        {
                            rc = produce_events( &ctx, src );
                        }
                    }
                }
            }
        }
        ArgsWhack ( args );
    }
    else
        KOutMsg( "ArgsMakeAndHandle() failed %R\n", rc );
    return rc;
}
