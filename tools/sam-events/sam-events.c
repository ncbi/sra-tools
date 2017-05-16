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

/* use the allele-dictionary to reduce the alleles per position */
#include "common.h"

/* use the alignment-iterator to read from a csra-file or sam-file */
#include "alignment_iter.h"

/* use the slice module */
#include "slice.h"

/* use the alignment-consumer */
#include "alig_consumer.h"
#include "alig_consumer2.h"

/* because we use a lib sam-extract written i C++, we have to define this symbol !!! */
/* void *__gxx_personality_v0; */

const char UsageDefaultName[] = "sam-events";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define CSRA_CACHE_SIZE 1024 * 1024 * 32

#define OPTION_MINCNT  "min-count"
#define ALIAS_MINCNT   "m"
static const char * mincnt_usage[]    = { "minimum count per event", NULL };

#define OPTION_MINFWD  "min-fwd"
static const char * minfwd_usage[]    = { "minimum count on forward strand", NULL };

#define OPTION_MINREV  "min-rev"
static const char * minrev_usage[]    = { "minimum count on reverse strand", NULL };

#define OPTION_MINTP   "min-t+"
static const char * mintp_usage[]     = { "minimum count on t+ template", NULL };

#define OPTION_MINTN  "min-t-"
static const char * mintn_usage[]     = { "minimum count on t- template", NULL };

#define OPTION_PURGE   "purge"
#define ALIAS_PURGE    "p"
static const char * purge_usage[]     = { "after how many ref-pos in dict perform pureg", NULL };

#define OPTION_LOG     "log"
#define ALIAS_LOG      "o"
static const char * log_usage[]       = { "log the alignments into text-file", NULL };

#define OPTION_CSRA    "csra"
static const char * csra_usage[]      = { "take input as csra not as sam-file", NULL };

#define OPTION_REF     "reference"
#define ALIAS_REF      "r"
static const char * ref_usage[]       = { "the reference(s) as fasta-file", NULL };

#define OPTION_SLICE   "slice"
#define ALIAS_SLICE    "s"
static const char * slice_usage[]     = { "process only this slice of the reference <name[:from-to]>", NULL };

#define OPTION_LOOKUP  "lookup"
#define ALIAS_LOOKUP   "l"
static const char * lookup_usage[]    = { "perfrom lookup for each allele into this lmdb-file >", NULL };

#define OPTION_STRAT   "strat"
static const char * strat_usage[]     = { "storage strategy ( dflt=0 ) >", NULL };

#define OPTION_EVSTRAT "evstrat"
static const char * evstrat_usage[]   = { "event strategy ( dflt=0 ) >", NULL };

#define OPTION_CACHE   "cache"
static const char * cache_usage[]     = { "size of cursor-cache ( dflt=32 MB ) >", NULL };


OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_REF,       ALIAS_REF,      NULL, ref_usage,       1,   true,        false },
    { OPTION_SLICE,     ALIAS_SLICE,    NULL, slice_usage,     1,   true,        false },
    { OPTION_LOOKUP,    ALIAS_LOOKUP,   NULL, lookup_usage,    1,   true,        false },    
    { OPTION_CSRA,      NULL,          NULL, csra_usage,      1,   false,       false },
    
    { OPTION_MINCNT,    ALIAS_MINCNT,   NULL, mincnt_usage,    1,   true,        false },
    { OPTION_MINFWD,    NULL,          NULL, minfwd_usage,    1,   true,        false },
    { OPTION_MINREV,    NULL,          NULL, minrev_usage,    1,   true,        false },
    { OPTION_MINTP,     NULL,          NULL, mintp_usage,     1,   true,        false },
    { OPTION_MINTN,     NULL,          NULL, mintn_usage,     1,   true,        false },

    { OPTION_PURGE,     ALIAS_PURGE,    NULL, purge_usage,     1,   true,        false },
    { OPTION_STRAT,     NULL,          NULL, strat_usage,     1,   true,        false },
    { OPTION_EVSTRAT,   NULL,          NULL, evstrat_usage,   1,   true,        false },
    { OPTION_CACHE,     NULL,          NULL, cache_usage,     1,   true,        false },    
    { OPTION_LOG,       ALIAS_LOG,      NULL, log_usage,       1,   true,        false }
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

    KOutMsg( "  Output-format: C1 C2 C3 C4 REFNAME POS NUMDEL NUMINS BASES\n" );
    KOutMsg( "    C1 ........ occurances on forward strand\n" );
    KOutMsg( "    C2 ........ occurances on reverse strand\n" );
    KOutMsg( "    C3 ........ occurances on forward template\n" );
    KOutMsg( "    C4 ........ occurances on reverse template\n" );
    KOutMsg( "    REFNAME ... canonical name of the reference/chromosome\n" );
    KOutMsg( "    POS ....... 1-based position on reference/chromosome\n" );
    KOutMsg( "    NUMDEL .... number of deletions on reference/chromosome\n" );
    KOutMsg( "    NUMINS .... number of insertions at position\n" );
    KOutMsg( "    BASES  .... inserted bases\n\n" );

    KOutMsg( "Options:\n" );

    HelpOptionsStandard ();
    for ( i = 0; i < sizeof ( ToolOptions ) / sizeof ( ToolOptions[ 0 ] ); ++i )
        HelpOptionLine ( ToolOptions[ i ].aliases, ToolOptions[ i ].name, NULL, ToolOptions[ i ].help );

    return rc;
}


typedef struct tool_ctx
{
    alig_consumer_data ac_data;
    
    bool csra, unsorted;
    const char * ref;
    const char * logfilename;
    struct Writer * log;
    uint64_t pos_lookups;
    size_t cursor_cache_size;
    uint32_t ev_strategy;
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc;
    
    ctx->ac_data.fasta = NULL;
    ctx->log = NULL;
    ctx->logfilename = NULL;
    ctx->ac_data.purge = 4096;
    ctx->unsorted = false;
    ctx->ac_data.slice = NULL;
    ctx->ac_data.lookup = NULL;
    ctx->pos_lookups = 0;
    
    rc = get_bool( args, OPTION_CSRA, &ctx->csra );

    if ( rc == 0 )
        rc = get_charptr( args, OPTION_REF, &ctx->ref );

    if ( rc == 0 )
        rc = get_slice( args, OPTION_SLICE, ( slice ** )&ctx->ac_data.slice );
		
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_PURGE, &ctx->ac_data.purge, 4096 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_STRAT, &ctx->ac_data.dict_strategy, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_EVSTRAT, &ctx->ev_strategy, 0 );
    if ( rc == 0 )
        rc = get_size_t( args, OPTION_CACHE, &ctx->cursor_cache_size, CSRA_CACHE_SIZE );
    if ( rc == 0 )
        rc = get_charptr( args, OPTION_LOG, &ctx->logfilename );
    
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINCNT, &ctx->ac_data.limits.total, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINFWD, &ctx->ac_data.limits.fwd, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINREV, &ctx->ac_data.limits.rev, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINTP, &ctx->ac_data.limits.t_pos, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINTN, &ctx->ac_data.limits.t_neg, 0 );

    if ( rc == 0 )
    {
        const char * lookup_file = NULL;
        rc = get_charptr( args, OPTION_LOOKUP, &lookup_file );
        if ( rc == 0 && lookup_file != NULL )
            rc = allele_lookup_make( &ctx->ac_data.lookup, lookup_file );
    }
    
    return rc;
}

static void release_tool_ctx( tool_ctx * ctx )
{
    if ( ctx != NULL )
    {
        if ( ctx->log != NULL ) writer_release( ctx->log );
        if ( ctx->ac_data.fasta != NULL ) unloadFastaFile( ctx->ac_data.fasta );
        if ( ctx->ac_data.slice != NULL ) release_slice( ( slice * )ctx->ac_data.slice );
        if ( ctx->ac_data.lookup != NULL ) allele_lookup_release( ctx->ac_data.lookup );
    }
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t consume_alignments_strategy_0( tool_ctx * ctx, struct alig_iter * ai )
{
    struct alig_consumer * consumer;
    rc_t rc = alig_consumer_make( &consumer, &ctx->ac_data );
    if ( rc == 0 )
    {
        bool running = true;
        while ( running )
        {
            AlignmentT alignment;
            
            running = alig_iter_get( ai, &alignment );
            if ( running )
            {
                /* consume the alignment */
                if ( rc == 0 )
                    rc = alig_consumer_consume_alignment( consumer, &alignment );

                /* check if we are quitting... */
                if ( rc == 0 ) { running = ( Quitting() == 0 ); }
            }
        }
        ctx->unsorted = alig_consumer_get_unsorted( consumer );
        alig_consumer_release( consumer );
    }
    return rc;
}


static rc_t consume_alignments_strategy_1( tool_ctx * ctx, struct alig_iter * ai )
{
    struct alig_consumer2 * consumer;
    rc_t rc = alig_consumer2_make( &consumer, &ctx->ac_data );
    if ( rc == 0 )
    {
        bool running = true;
        while ( running )
        {
            AlignmentT alignment;
            
            running = alig_iter_get( ai, &alignment );
            if ( running )
            {
                /* consume the alignment */
                if ( rc == 0 )
                    rc = alig_consumer2_consume_alignment( consumer, &alignment );

                /* check if we are quitting... */
                if ( rc == 0 ) { running = ( Quitting() == 0 ); }
            }
        }
        ctx->unsorted = alig_consumer2_get_unsorted( consumer );
        alig_consumer2_release( consumer );
    }
    return rc;
}


/* source can be NULL for stdin.... */
static rc_t produce_events_for_source( tool_ctx * ctx, const char * source )
{
    rc_t rc = 0;
    bool unload_ref = false;
    
    if ( ctx->ref == NULL && ctx->csra && source != NULL )
    {
        /* no fasta-file given, get the fasta out of the accession! */
        /* header in expandCIGAR.h code in fasta-file.[hpp/cpp]*/
        ctx->ac_data.fasta = loadcSRA( source, ctx->cursor_cache_size );
        unload_ref = true;
    }

    if ( ctx->ac_data.fasta != NULL )
    {
        /* this is the source of the alignments, if fills out AlignmentT-records */
        struct alig_iter * ai;
        
        /* this source can be made from a csra-accession or a the path of a SAM-file */
        if ( ctx->csra )
            rc = alig_iter_csra_make( &ai, source, ctx->cursor_cache_size, ctx->ac_data.slice );
        else
            rc = alig_iter_sam_make( &ai, source, ctx->ac_data.slice );
        
        if ( rc == 0 )
        {
            if ( ctx->ev_strategy == 0 )
                rc = consume_alignments_strategy_0( ctx, ai );
            else if ( ctx->ev_strategy == 1 )
                rc = consume_alignments_strategy_1( ctx, ai );
            else
                rc = KOutMsg( "unknown event strategy of %d\n", ctx->ev_strategy );
            
            alig_iter_release( ai );
        }
    }
    
    if ( unload_ref )
    {
        /* we have to 'unload' the fasta-file if it was produced from a CSRA-accession */
        unloadFastaFile( ctx->ac_data.fasta );
        ctx->ac_data.fasta = NULL;
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
            if ( ctx.logfilename != NULL )
                rc = writer_make( &ctx.log, ctx.logfilename );
            if ( rc == 0 )
            {
                uint32_t count, idx;
                rc = ArgsParamCount( args, &count );
                if ( rc == 0 )
                {
                    if ( ctx.ref != NULL )
                    {
                        /* the user gave as a FASTA-file as reference! */
                        ctx.ac_data.fasta = loadFastaFile( 0, ctx.ref );
                        if ( ctx.ac_data.fasta == NULL )
                        {
                            log_err( "cannot open reference '%s'", ctx.ref );
                            rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
                        }
                    }

                    if ( count < 1 )
                    {
                        if ( ctx.csra )
                        {
                            /* no accession(s) given at commandline, we need one for csra! */
                            rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                            log_err( "no accession(s) given" );
                        }
                        else
                            /* if not source given, we open STDIN for SAM-input */
                            rc = produce_events_for_source( &ctx, NULL );   /* <==== */
                    }
                    else
                    {
                        const char * src;
                        for ( idx = 0; rc == 0 && idx < count; ++idx )
                        {
                            rc = ArgsParamValue( args, idx, ( const void ** )&src );
                            if ( rc == 0 )
                                rc = produce_events_for_source( &ctx, src ); /* <==== */
                        }
                    }
                }
            }
            if ( ctx.unsorted )
                log_err( "the source was not sorted, alleles are not correctly counted" );
            release_tool_ctx( &ctx );
        }
        clear_recorded_errors();
        ArgsWhack ( args );
    }
    else
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    return rc;
}
