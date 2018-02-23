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
#include <klib/rc.h>

#include <insdc/insdc.h>

/* use the allele-dictionary to reduce the alleles per position */
#include "common.h"
#include "alignment_iter.h"

const char UsageDefaultName[] = "sam-events";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define CSRA_CACHE_SIZE 1024 * 1024 * 32

#define OPTION_CACHE   "cache"
static const char * cache_usage[]     = { "size of cursor-cache ( dflt=32 MB ) >", NULL };


OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CACHE,     NULL,          NULL, cache_usage,     1,   true,        false }
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


typedef struct tool_ctx
{
    size_t cursor_cache_size;
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_size_t( args, OPTION_CACHE, &ctx->cursor_cache_size, CSRA_CACHE_SIZE );
    return rc;
}

static void release_tool_ctx( tool_ctx * ctx )
{
    if ( ctx != NULL )
    {

    }
}


typedef struct inspect_ctx
{
    VNamelist * seen_refs;
    const String * current_ref;
    uint64_t current_pos;
    uint64_t alignments;
    uint32_t references;
    bool sorted;
} inspect_ctx;


static rc_t initialize_inspect_ctx( inspect_ctx * ictx )
{
    rc_t rc = VNamelistMake( &ictx->seen_refs, 10 );
    if ( rc != 0 )
        log_err( "VNamelistMake() failed" );
    else
    {
        ictx->current_ref = NULL;
        ictx->current_pos = 0;
        ictx->alignments = 0;
        ictx->references = 0;
        ictx->sorted = true;
    }
    return rc;
}

static rc_t deinitialize_inspect_ctx( inspect_ctx * ictx )
{
    rc_t rc = VNamelistRelease( ictx->seen_refs );
    if ( ictx->current_ref != NULL ) StringWhack ( ictx->current_ref );
    return rc;
}


static rc_t handle_alignment( inspect_ctx * ictx, AlignmentT * al )
{
    rc_t rc = 0;
    int cmp = ( ictx->current_ref != NULL ) ? StringCompare( ictx->current_ref, &al->rname ) : 1;
    if ( cmp != 0 )
    {
        int32_t idx;
        rc = VNamelistContainsString( ictx->seen_refs, &al->rname, &idx );
        if ( rc != 0 )
            log_err( "VNamelistContainsString() failed" );
        else
        {
            if ( idx < 0 )
            {
                rc = VNamelistAppendString( ictx->seen_refs, &al->rname );
                if ( rc != 0 )
                    log_err( "VNamelistAppendString() failed" );
                else
                {
                    if ( ictx->current_ref != NULL ) StringWhack ( ictx->current_ref );
                    ictx->current_pos = 0;
                    ictx->references += 1;
                    ictx->alignments += 1;
                    rc = StringCopy( &ictx->current_ref, &al->rname );
                    if ( rc != 0 )
                        log_err( "StringCopy() failed" );
                }
            }
            else
            {
                ictx->sorted = false;
                rc = KOutMsg( "repeated reference : %S ( at row_id #%lu )", &al->rname, al->row_id );
            }
        }
    }
    else
    {
        if ( al->pos < ictx->current_pos )
        {
            ictx->sorted = false;
            rc = KOutMsg( "unsorted position : %S:%lu ( at row_id #%lu )\n", &al->rname, al->pos, al->row_id );
        }
        else
        {
            ictx->current_pos = al->pos;
            ictx->alignments += 1;
        }
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t inspect_source( tool_ctx * ctx, const char * source )
{
    rc_t rc = KOutMsg( "inspecting: %s\n", source );
    if ( rc == 0 )
    {
        struct alig_iter * src_iter;
        rc = alig_iter_make( &src_iter, source, ctx->cursor_cache_size );
        if ( rc == 0 )
        {
            inspect_ctx ictx;
            rc = initialize_inspect_ctx( &ictx );
            if ( rc == 0 )
            {
                AlignmentT alignment;

                while ( rc == 0 && ictx.sorted && alig_iter_get( src_iter, &alignment ) )
                {
                    rc = handle_alignment( &ictx, &alignment );
                }
                alig_iter_release( src_iter );

                if ( rc == 0 )
                {
                    if ( ictx.sorted )
                        rc = KOutMsg( "%s is sorted! ( %,lu alignments, %,lu references )\n",
                                    source,
                                    ictx.alignments,
                                    ictx.references );
                    else
                        rc = KOutMsg( "%s is unsorted! Test stopped after %,lu alignments, %,lu references\n",
                                    source,
                                    ictx.alignments,
                                    ictx.references );                    
                }
                
                if ( rc == 0 && !ictx.sorted )
                    rc = RC( rcApp, rcNoTarg, rcAllocating, rcConstraint, rcViolated );
                deinitialize_inspect_ctx( &ictx );
            }
        }
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */


rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                ToolOptions, sizeof ( ToolOptions ) / sizeof ( OptDef ) );
    if ( rc == 0 )
    {
        tool_ctx ctx;
        rc = fill_out_tool_ctx( args, &ctx );
        if ( rc == 0 )
        {
            if ( rc == 0 )
            {
                uint32_t count, idx;
                rc = ArgsParamCount( args, &count );
                if ( rc == 0 )
                {
                    if ( count < 1 )
                    {
                        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                        log_err( "no accession given!" );
                    }
                    else
                    {
                        const char * src;
                        for ( idx = 0; rc == 0 && idx < count; ++idx )
                        {
                            rc = ArgsParamValue( args, idx, ( const void ** )&src );
                            if ( rc == 0 )
                                rc = inspect_source( &ctx, src ); /* <==== */
                        }
                    }
                }
            }
            release_tool_ctx( &ctx );
        }
        clear_recorded_errors();
        ArgsWhack ( args );
    }
    else
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    return rc;
}
