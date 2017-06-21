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

#include <kfs/directory.h>
#include <kfs/filetools.h>

#include <insdc/insdc.h>

#include "common.h"
#include "slice.h"
#include "ref_iter.h"
#include "coverage_iter.h"

const char UsageDefaultName[] = "ref-idx";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define CSRA_CACHE_SIZE 1024 * 1024 * 32

#define OPTION_CACHE   "cache"
static const char * cache_usage[]   = { "size of cursor-cache ( dflt=32 MB ) >", NULL };

#define OPTION_SLICE   "slice"
#define ALIAS_SLICE    "s"
static const char * slice_usage[]   = { "restrict to this slice", NULL };

#define OPTION_MIN     "min"
#define ALIAS_MIN      "i"
static const char * min_usage[]     = { "min coverage to look for ( dflt = 0 )", NULL };

#define OPTION_MAX     "max"
#define ALIAS_MAX      "x"
static const char * max_usage[]     = { "max coverage to look for ( dflt = 0xFFFFFFFF )", NULL };

#define OPTION_FUNC    "function"
#define ALIAS_FUNC     "f"
static const char * func_usage[]    = { "function to perform: 0...collect min/max for whole run",
                                         "1...collect min/max for each reference",
                                         "2...report reference-rows based on min/max-constrains",
                                         NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CACHE,     NULL,          NULL, cache_usage,     1,   true,        false },
    { OPTION_SLICE,     ALIAS_SLICE,   NULL, slice_usage,     1,   true,        false },    
    { OPTION_FUNC,      ALIAS_FUNC,    NULL, func_usage,      1,   true,        false },
    { OPTION_MIN,       ALIAS_MIN,     NULL, min_usage,       1,   true,        false },
    { OPTION_MAX,       ALIAS_MAX,     NULL, max_usage,       1,   true,        false }
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
    uint32_t min_coverage, max_coverage, function;
    slice * slice;
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_size_t( args, OPTION_CACHE, &ctx->cursor_cache_size, CSRA_CACHE_SIZE );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_FUNC, &ctx->function, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MIN, &ctx->min_coverage, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MAX, &ctx->max_coverage, 0xFFFFFFFF );
    if ( rc == 0 )
        rc = get_slice( args, OPTION_SLICE, &ctx->slice );
    return rc;
}

static void release_tool_ctx( tool_ctx * ctx )
{
    if ( ctx != NULL )
    {
        if ( ctx->slice != NULL ) release_slice( ctx->slice );
    }
}


/* ----------------------------------------------------------------------------------------------- */

typedef struct limit
{
    uint32_t value;
    int64_t row;
    uint64_t pos;
    String * ref;
} limit;


static void limit_set( limit * l, uint32_t value, int64_t row, uint64_t pos, String * ref )
{
    l->value = value;
    l->row = row;
    l->pos = pos;
    l->ref = ref;
}

static rc_t f0_min_max_for_whole_run( tool_ctx * ctx, const char * src, const Vector *vec )
{
    rc_t rc = 0;
    uint32_t idx;
    limit min, max;
    limit_set( &min, 0xFFFFFFFF, 0, 0, NULL );
    limit_set( &max, 0, 0, 0, NULL );
    for ( idx = VectorStart( vec ); rc == 0 && idx < VectorLength( vec ); ++idx )
    {
        RefT * ref = VectorGet( vec, idx );
        if ( ref != NULL )
        {
            struct simple_coverage_iter * ci;
            rc = simple_coverage_iter_make( &ci, src, ctx->cursor_cache_size, ref );
            if ( rc == 0 )
            {
                SimpleCoverageT cv;
                while ( rc == 0 &&
                        simple_coverage_iter_get_capped( ci, &cv, ctx->min_coverage, ctx->max_coverage ) )
                {
                    if ( cv.prim > max.value )
                        limit_set( &max, cv.prim, cv.ref_row_id, cv.start_pos, &ref->rname );
                    if ( cv.prim < min.value )
                        limit_set( &min, cv.prim, cv.ref_row_id, cv.start_pos, &ref->rname );                    
                }
                simple_coverage_iter_release( ci );
            }
        }
    }
    if ( rc == 0 )
        rc = KOutMsg( "MAX\t%S:%u\tref-row = %ld\talignments = %,u\n",
            max.ref, max.pos, max.row, max.value );
    if ( rc == 0 )
        rc = KOutMsg( "MIN\t%S:%u\tref-row = %ld\talignments = %,u\n",
            min.ref, min.pos, min.row, min.value );
    return rc;
}

static rc_t f1_min_max_for_each_ref( tool_ctx * ctx, const char * src, const Vector *vec )
{
    rc_t rc = 0;
    uint32_t idx;
    limit min, max;    
    for ( idx = VectorStart( vec ); rc == 0 && idx < VectorLength( vec ); ++idx )
    {
        RefT * ref = VectorGet( vec, idx );
        if ( ref != NULL )
        {
            struct simple_coverage_iter * ci;
            rc = simple_coverage_iter_make( &ci, src, ctx->cursor_cache_size, ref );
            if ( rc == 0 )
            {
                SimpleCoverageT cv;
                
                limit_set( &min, 0xFFFFFFFF, 0, 0, NULL );
                limit_set( &max, 0, 0, 0, NULL );
                
                while ( rc == 0 &&
                        simple_coverage_iter_get_capped( ci, &cv, ctx->min_coverage, ctx->max_coverage ) )
                {
                    if ( cv.prim > max.value )
                        limit_set( &max, cv.prim, cv.ref_row_id, cv.start_pos, &ref->rname );
                    if ( cv.prim < min.value )
                        limit_set( &min, cv.prim, cv.ref_row_id, cv.start_pos, &ref->rname );                    
                }
                simple_coverage_iter_release( ci );
                if ( rc == 0 )
                    rc = KOutMsg( "%S\n", &ref->rname );
                if ( rc == 0 )
                    rc = KOutMsg( "\tMAX\tpos = %u\tref-row = %ld\talignments = %,u\n",
                        max.pos, max.row, max.value );
                if ( rc == 0 )
                    rc = KOutMsg( "\tMIN\tpos = %u\tref-row = %ld\talignments = %,u\n\n",
                        min.pos, min.row, min.value );
            }
        }
    }
    return rc;
}

static rc_t f2_refrows_between_min_max( tool_ctx * ctx, const char * src, const Vector *vec )
{
    rc_t rc = 0;
    uint32_t idx;
    for ( idx = VectorStart( vec ); rc == 0 && idx < VectorLength( vec ); ++idx )
    {
        RefT * ref = VectorGet( vec, idx );
        if ( ref != NULL )
        {
            bool perform = true;
            if ( ctx->slice != NULL )
            {
                perform = ( 0 == StringCompare( ctx->slice->refname, &ref->rname ) );
                if ( perform && ctx->slice->count > 0 )
                {
                    int64_t start_offset = ctx->slice->start / ref->block_size;
                    int64_t end_offset = ctx->slice->end / ref->block_size;
                    ref->start_row_id += start_offset;
                    ref->count = ( end_offset - start_offset ) + 1;
                }
            }
            if ( perform )
            {
                struct simple_coverage_iter * ci;
                rc = simple_coverage_iter_make( &ci, src, ctx->cursor_cache_size, ref );
                if ( rc == 0 )
                {
                    SimpleCoverageT cv;
                    while ( rc == 0 &&
                             simple_coverage_iter_get_capped( ci, &cv, ctx->min_coverage, ctx->max_coverage ) )
                    {
                        rc = KOutMsg( "%u\t%S:%lu.%u\t%ld\n",
                            cv.prim, &ref->rname, cv.start_pos, cv.len, cv.ref_row_id );
                    }
                    simple_coverage_iter_release( ci );
                }
            }
        }
    }
    return rc;
}


static rc_t detailed_coverage( tool_ctx * ctx, const char * src )
{
    DetailedCoverage dc;
    rc_t rc = detailed_coverage_make( src, ctx->cursor_cache_size, ctx->slice, &dc );
    if ( rc == 0 )
    {
        uint32_t idx;
        for ( idx = 0; rc == 0 && idx < dc.len; ++idx )
        {
            rc = KOutMsg( "%d\t%d\n", dc.start_pos + idx, dc.coverage[ idx ] );
        }
        detailed_coverage_release( &dc );
    }
    return rc;
}

static rc_t list_test( tool_ctx * ctx, const char * src )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    KOutMsg( "list-test\n" );
    if ( rc == 0 )
    {
        VNamelist * list;
        rc = ReadDirEntriesIntoToNamelist( &list, dir, true, true, true, src );
        if ( rc == 0 )
        {
            uint32_t idx, count;
            rc = VNameListCount( list, &count );
            for ( idx = 0; rc == 0 && idx < count; ++idx )
            {
                const char * name = NULL;
                rc = VNameListGet( list, idx, &name );
                if ( rc == 0 && name != NULL )
                    rc = KOutMsg( "--->%s\n", name );
            }
            VNamelistRelease( list );
        }
        KDirectoryRelease( dir );
    }
    return rc;
}

static rc_t common_part( tool_ctx * ctx, const char * src )
{
    rc_t rc = 0;
    if ( ctx->function < 3 )
    {
        Vector vec;
        rc = ref_iter_make_vector( &vec, src, ctx->cursor_cache_size );
        if ( rc == 0 )
        {
            switch( ctx->function )
            {
                case 0 : rc = f0_min_max_for_whole_run( ctx, src, &vec ); break;
                case 1 : rc = f1_min_max_for_each_ref( ctx, src, &vec ); break;
                case 2 : rc = f2_refrows_between_min_max( ctx, src, &vec ); break;
            }
            ref_iter_release_vector( &vec );
        }
    }
    else
    {
        switch( ctx->function )
        {
            case 3 : rc = detailed_coverage( ctx, src ); break;
            case 4 : rc = list_test( ctx, src ); break;
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
                            {
                                rc = common_part( &ctx, src ); /* <==== */
                            }
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
