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
#include <klib/num-gen.h>

#include <insdc/insdc.h>
#include <search/ref-variation.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* use the allele-dictionary to reduce the alleles per position */
#include "common.h"
#include "slice.h"

/* use the alignment-iterator to read from a csra-file or sam-file */
#include "alignment_iter.h"

#include "expandCIGAR.h"

const char UsageDefaultName[] = "t4";

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

#define OPTION_ALIG    "print-alignment"
#define ALIAS_ALIG     "a"
static const char * alig_usage[]      = { "print alignments", NULL };

#define OPTION_EVENT   "print-events"
#define ALIAS_EVENT    "e"
static const char * event_usage[]     = { "print events", NULL };

#define OPTION_ALLELE  "print-alleles"
#define ALIAS_ALLELE   "l"
static const char * allele_usage[]    = { "print alleles", NULL };

#define OPTION_ROWS    "rows"
#define ALIAS_ROWS     "R"
static const char * rows_usage[]      = { "row-range", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_SLICE,     ALIAS_SLICE,    NULL, slice_usage,     1,   true,        false },
    { OPTION_ALIG,      ALIAS_ALIG,     NULL, alig_usage,      1,   false,       false },
    { OPTION_EVENT,     ALIAS_EVENT,    NULL, event_usage,     1,   false,       false },
    { OPTION_ALLELE,    ALIAS_ALLELE,   NULL, allele_usage,    1,   false,       false },
    { OPTION_ROWS,      ALIAS_ROWS,     NULL, rows_usage,      1,   true,        false }
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
    bool print_alignment;
    bool print_events;
    bool print_alleles;
    struct num_gen * rows;
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_slice( args, OPTION_SLICE, &ctx->slice );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_ALIG, &ctx->print_alignment );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_EVENT, &ctx->print_events );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_ALLELE, &ctx->print_alleles );
    if ( rc == 0 )
    {
        const char * s_rows = NULL;
        rc = get_charptr( args, OPTION_ROWS, &s_rows );
        if ( rc == 0 && s_rows != NULL )
            rc = num_gen_make_from_str_sorted( &ctx->rows, s_rows, false );
        else
            ctx->rows = NULL;
    }
    return rc;
}

static void cleanup_tool_ctx( tool_ctx * ctx )
{
    if ( ctx->slice != NULL )
        release_slice( ctx->slice );
    if ( ctx->rows != NULL )
        num_gen_destroy( ctx->rows );
}


/* ----------------------------------------------------------------------------------------------- */

typedef struct ref_context
{
    int ref_index;
    const String * rname;
    const char * ref_bases;
    unsigned ref_bases_count;
    struct cFastaFile * fasta;
} ref_context;


static rc_t check_rname( ref_context * self, const String * rname )
{
    rc_t rc = 0;
    int cmp = 1;
    
    if ( self->ref_index >= 0 )
        cmp = StringCompare( self->rname, rname );
        
    if ( cmp != 0 )
    {
        /* we are entering a new reference! */
        if ( self->rname != NULL )
            StringWhack ( self->rname );
        rc = StringCopy( &self->rname, rname );
        if ( self->fasta != NULL )
            self->ref_index = FastaFile_getNamedSequence( self->fasta, rname->size, rname->addr );
        else
            self->ref_index = -1;
            
        if ( self->ref_index < 0 )
        {
            rc = RC( rcExe, rcNoTarg, rcVisiting, rcId, rcInvalid );
            log_err( "'%S' not found in fasta-file", rname );
        }
        else
            self->ref_bases_count = FastaFile_getSequenceData( self->fasta, self->ref_index, &self->ref_bases );
    }
    return rc;
}


#define NUM_EVENTS 1024

static rc_t enum_events( const tool_ctx * ctx, const ref_context * ref_ctx, const AlignmentT * a )
{
    rc_t rc = 0;
    struct Event2 events[ NUM_EVENTS ];
 
    /* in expandCIGAR.h ( expandCIGAR.cpp ) */
    int num_events = expandCIGAR3( events,
                                   NUM_EVENTS,
                                   a->cigar.len,
                                   a->cigar.addr,
                                   a->read.addr,
                                   a->pos - 1,
                                   ref_ctx->fasta,
                                   ref_ctx->ref_index );
    if ( num_events < 0 )
        log_err( "expandCIGRAR failed for cigar '%S'", &a->cigar );
    else
    {
        int idx;
        for ( idx = 0; rc == 0 && idx < num_events; ++idx )
        {
            struct Event2 * ev = &events[ idx ];
            
            if ( ctx->print_events )
            {
                String bases;
                StringInit( &bases, &a->read.addr[ ev->seqPos ], ev->seqLen, ev->seqLen );
                rc = KOutMsg( "%S:%lu:%u:%S\n", ref_ctx->rname, ev->refPos, ev->refLen, &bases );
            }
            
            if ( ctx->print_alleles )
            {
                RefVariation * ref_var;       
                rc = RefVariationIUPACMake( &ref_var,                   /* to be created */
                            ref_ctx->ref_bases,                         /* the reference-bases */
                            ref_ctx->ref_bases_count,                   /* length of the reference */
                            ev->refPos,                                 /* position of event */
                            ev->refLen,                                 /* length of deletion */
                            &( a->read.addr[ ev->seqPos ] ),           /* inserted bases */
                            ev->seqLen,                                 /* number of inserted bases */
                            refvarAlgRA );
                if ( rc != 0 )
                    log_err( "RefVariationIUPACMake() failed rc=%R", rc );
                else
                {
                    INSDC_dna_text const * allele;
                    size_t allele_len, allele_start;
                    
                    rc = RefVariationGetAllele( ref_var, &allele, &allele_len, &allele_start );
                    if ( rc != 0 )
                        log_err( "RefVariationGetAllele() failed rc=%R", rc );
                    else
                    {
                        size_t allele_len_on_ref;
                        rc = RefVariationGetAlleleLenOnRef( ref_var, &allele_len_on_ref );
                        if ( rc != 0 )
                            log_err( "RefVariationGetAlleleLenOnRef() failed rc=%R", rc );
                        else
                        {
                            uint64_t pos = allele_start;
                            uint32_t deletes = allele_len_on_ref;
                            uint32_t inserts = allele_len;
                            bool filtered = true;
                            
                            if ( ctx->slice != NULL )
                                filtered = filter_by_slice( ctx->slice, ref_ctx->rname, pos, inserts );

                            if ( filtered )
                            {
                                String bases;
                                StringInit( &bases, allele, inserts, inserts );
                                rc = KOutMsg( "%S:%lu:%u:%S\n", ref_ctx->rname, pos, deletes, &bases );
                            }
                        }
                    }
                    
                    {
                        rc_t rc2 = RefVariationRelease( ref_var );
                        if ( rc2 != 0 )
                            log_err( "RefVariationRelease() failed rc=%R", rc2 );
                    }
                }
            }
            
        }
    }
    return rc;
}


#define CACHE_SIZE 32 * 1024 * 1024

static rc_t run( const Args * args, uint32_t count, const tool_ctx * ctx )
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
            ref_context ref_context;
            
            memset( &ref_context, 0, sizeof ref_context );
            ref_context.fasta = loadcSRA( acc, CACHE_SIZE );
            if ( ref_context.fasta != NULL )
            {
                struct alig_iter * ai;
                rc = alig_iter_csra_make( &ai, acc, CACHE_SIZE, ctx->slice, ctx->rows );
                if ( rc == 0 )
                {
                    AlignmentT a;
                    while ( rc == 0 && alig_iter_get( ai, &a ) )
                    {
                        if ( a.filter == READ_FILTER_PASS )
                        {
                            if ( ctx->print_alignment )
                                rc = print_alignment( &a );

                            if ( rc == 0 )
                            {
                                rc = check_rname( &ref_context, &a.rname );
                                if ( rc == 0 )
                                    rc = enum_events( ctx, &ref_context, &a );
                            }
                        }
                    }
                    alig_iter_release( ai );
                }
                unloadFastaFile( ref_context.fasta );
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
            bool print_something = ctx.print_alignment || ctx.print_events || ctx.print_alleles;
            if ( print_something )
            {
                uint32_t count;
                rc = ArgsParamCount( args, &count );
                if ( rc != 0 )
                    log_err( "ArgsParamCount() failed %R", rc );
                else
                    rc = run( args, count, &ctx );
            }
            else
            {
                rc = KOutMsg( "nothing to print!\n" );
            }
            cleanup_tool_ctx( &ctx );
        }
        ArgsWhack ( args );
    }
    return rc;
}
