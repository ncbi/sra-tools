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

#include <stdio.h>
#include <ctype.h>

/* use the allele-dictionary to reduce the alleles per position */
#include "common.h"
#include "slice.h"
#include "slice_2_rowrange.h"

const char UsageDefaultName[] = "t3";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define OPTION_SLICE   "slice"
#define ALIAS_SLICE    "s"
static const char * slice_usage[]     = { "process only this slice of the reference <name[:from-to]>", NULL };

#define OPTION_REF     "reference"
#define ALIAS_REF      "r"
static const char * report_usage[]    = { "report the REFERENCE-table", NULL };

#define OPTION_DETAIL  "detail"
#define ALIAS_DETAIL   "d"
static const char * detail_usage[]    = { "detailed report of the REFERENCE-table", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_SLICE,     ALIAS_SLICE,    NULL, slice_usage,     1,   true,        false },
    { OPTION_REF,       ALIAS_REF,      NULL, report_usage,    1,   false,       false },
    { OPTION_DETAIL,    ALIAS_DETAIL,   NULL, detail_usage,    1,   false,       false }
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

typedef struct tool_ctx
{
    struct slice * slice;
    bool report, detail;
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_slice( args, OPTION_SLICE, &ctx->slice );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_REF, &ctx->report );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_DETAIL, &ctx->detail );
    return rc;
}

static void cleanup_tool_ctx( tool_ctx * ctx )
{
    if ( ctx->slice != NULL )
        release_slice( ctx->slice );
}


/* ----------------------------------------------------------------------------------------------- */

static rc_t print_ref( RefInfo * info, bool detailed )
{
    rc_t rc = 0;
    if ( info != NULL )
    {
        rc = KOutMsg( "%S\t%S\t%,ld.%,lu\t%,ld.%,lu\t%s\t%,lu\t%s\n",
                        info->name,
                        info->seq_id,
                        info->first_row_in_reftable, info->row_count_in_reftable,
                        info->first_alignment_id, info->alignment_id_count,
                        info->circular ? "true" : "false",
                        info->len,
                        info->sorted ? "true" : "false" );
        if ( detailed )
        {
            uint32_t count = VectorLength( &info->blocks );
            uint32_t start = VectorStart( &info->blocks );
            uint32_t ofs;
        
            for ( ofs = 0; rc == 0 && ofs < count; ++ofs )
            {
                BlockInfo * blk = VectorGet( &info->blocks, ofs + start );
                if ( blk != NULL )
                {
                    rc = KOutMsg( "\t%ld.%u\t%s\n",
                                   blk->first_alignment_id,
                                   blk->count,
                                   blk->sorted ? "true" : "false"
                                   );
                }
            }
        }
    }
    return rc;
}

static rc_t print_ref_info( Vector * v, bool detailed )
{
    rc_t rc = 0;
    bool sorted = true;
    uint32_t count = VectorLength( v );
    uint32_t start = VectorStart( v );
    uint32_t ofs;
    for ( ofs = 0; rc == 0 && ofs < count; ++ofs )
    {
        RefInfo * info = VectorGet( v, ofs + start );
        rc = print_ref( info, detailed );
        if ( rc == 0 && info != NULL )
            if ( !info->sorted ) sorted = false;
    }
    if ( rc == 0 )
        rc = KOutMsg( "sorted: %s\n", sorted ? "true" : "false" );
    return rc;
}

static rc_t run( Args * args, uint32_t count, tool_ctx * ctx )
{
    rc_t rc = 0;
    uint32_t idx;
    const char * acc;
    for ( idx = 0; rc == 0 && idx < count; ++idx )
    {
        rc = ArgsParamValue( args, idx, ( const void ** )&acc );
        if ( rc != 0 )
            log_err( "ArgsParamValue( %d ) failed %R", idx, rc );
        else
        {
            if ( ctx->report )
            {
                Vector v;
                rc = extract_reference_info( acc, &v );
                if ( rc == 0 )
                {
                    print_ref_info( &v, ctx->detail );
                    clear_reference_info( &v );
                }
            }
            else if ( ctx->slice != NULL )
            {
                row_range found;
                
                print_slice( ctx->slice );
                rc = slice_2_row_range( ctx->slice, acc, &found );
                if ( rc == 0 )
                {
                    rc = KOutMsg( "slice found : %ld.%lu\n", found.first_row, found.row_count );
                }
                else
                {
                    rc = KOutMsg( "slice not found ( %R )\n", rc );
                }
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
                rc = run( args, count, &ctx );
            cleanup_tool_ctx( &ctx );
        }
        ArgsWhack ( args );
    }
    return rc;
}
