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

#define OPTION_ALIG    "alignment"
#define ALIAS_ALIG     "a"
static const char * alig_usage[]      = { "show alignments>", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_SLICE,     ALIAS_SLICE,    NULL, slice_usage,     1,   true,        false },
    { OPTION_ALIG,      ALIAS_ALIG,     NULL, alig_usage,      1,   false,       false }    
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
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_slice( args, OPTION_SLICE, &ctx->slice );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_ALIG, &ctx->print_alignment );
    return rc;
}

static void cleanup_tool_ctx( tool_ctx * ctx )
{
    if ( ctx->slice != NULL )
        release_slice( ctx->slice );
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


static rc_t switch_reference( ref_context * self, const String * rname )
{
    rc_t rc = 0;
    
    /* prime the current-ref-name with the ref-name of the alignment */
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
    return rc;
}


static rc_t check_rname( ref_context * self, const String * rname )
{
    rc_t rc = 0;
    int cmp = 1;
    
    if ( self->ref_index >= 0 )
        cmp = StringCompare( self->rname, rname );
        
    if ( cmp != 0 )
    {
        /* we are entering a new reference! */
        rc = switch_reference( self, rname );
    }
    
    return rc;
}


#define NUM_EVENTS 1024

static rc_t enum_events( const ref_context * ref_context, const AlignmentT * a )
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
                                   ref_context->fasta,
                                   ref_context->ref_index );
    if ( num_events < 0 )
        log_err( "expandCIGRAR failed for cigar '%S'", &a->cigar );
    else
    {
        int idx;
        for ( idx = 0; rc == 0 && idx < num_events; ++idx )
        {
            String bases;
            struct Event2 * ev = &events[ idx ];
            StringInit( &bases, &a->read.addr[ ev->seqPos ], ev->seqLen, ev->seqLen );
            rc = KOutMsg( "%S:%lu:%u:%S\n", ref_context->rname, ev->refPos, ev->refLen, &bases );
        }
    }
    return rc;
}


#define CACHE_SIZE 32 * 1024 * 1024

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
            ref_context ref_context;
            
            memset( &ref_context, 0, sizeof ref_context );
            ref_context.fasta = loadcSRA( acc, CACHE_SIZE );
            if ( ref_context.fasta != NULL )
            {
                struct alig_iter * ai;
                rc = alig_iter_csra_make( &ai, acc, CACHE_SIZE, ctx->slice );
                if ( rc == 0 )
                {
                    AlignmentT a;
                    while ( rc == 0 && alig_iter_get( ai, &a ) )
                    {
                        if ( a.filter == 0 )
                        {
                            if ( ctx->print_alignment )
                                rc = print_alignment( &a );

                            if ( rc == 0 )
                            {
                                rc = check_rname( &ref_context, &a.rname );
                                if ( rc == 0 )
                                    rc = enum_events( &ref_context, &a );
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
