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

#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/filetools.h>

/* use the SAM extract functions from the sam-extract library build from libs/align */
#include <align/samextract-lib.h>

/* use the ref-variation functions in libs/search */
#include <search/ref-variation.h>

/* use the cigar-parser and reference-reader
    ( cigar2events.[cpp/hpp], expandCIGAR.[cpp/h], fasta-file.[cpp/hpp] ) */
#include "expandCIGAR.h"

/* use the allele-dictionary to reduce the alleles per position */
#include "common.h"

/* use the alignment-iterator to read from a csra-file */
#include "alignment_iter.h"

/* use the allele-dictionary to reduce the alleles per position */
#include "allele_dict.h"

/* use the allele-lookup module */
#include "allele_lookup.h"

/* use the slice module */
#include "slice.h"

/* use the alignment-consumer */
#include "alig_consumer.h"

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
    uint32_t min_count, purge;
    bool csra, unsorted;
    counters limits;
    const char * ref;
    struct cFastaFile * fasta;
    const char * logfilename;
    struct Writer * log;
	slice * slice;
    struct Allele_Lookup * lookup;
    uint64_t pos_lookups;
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc;
    
    ctx->fasta = NULL;
    ctx->log = NULL;
    ctx->logfilename = NULL;
    ctx->purge = 4096;
    ctx->unsorted = false;
    ctx->slice = NULL;
    ctx->lookup = NULL;
    ctx->pos_lookups = 0;
    
    rc = get_bool( args, OPTION_CSRA, &ctx->csra );

    if ( rc == 0 )
        rc = get_charptr( args, OPTION_REF, &ctx->ref );

    if ( rc == 0 )
        rc = get_slice( args, OPTION_SLICE, &ctx->slice );
		
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_PURGE, &ctx->purge, 4096 );
    if ( rc == 0 )
        rc = get_charptr( args, OPTION_LOG, &ctx->logfilename );
    
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINCNT, &ctx->min_count, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINFWD, &ctx->limits.fwd, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINREV, &ctx->limits.rev, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINTP, &ctx->limits.t_pos, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINTN, &ctx->limits.t_neg, 0 );

    if ( rc == 0 )
    {
        const char * lookup_file = NULL;
        rc = get_charptr( args, OPTION_LOOKUP, &lookup_file );
        if ( rc == 0 && lookup_file != NULL )
            rc = allele_lookup_make( &ctx->lookup, lookup_file );
    }
    return rc;
}

static void release_tool_ctx( tool_ctx * ctx )
{
    if ( ctx != NULL )
    {
        
        if ( ctx->log != NULL ) writer_release( ctx->log );
        if ( ctx->fasta != NULL ) unloadFastaFile( ctx->fasta );
        if ( ctx->slice != NULL ) release_slice( ctx->slice );
        if ( ctx->lookup != NULL ) allele_lookup_release( ctx->lookup );
    }
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t process_alignments_from_extractor( tool_ctx * ctx, Extractor * extractor )
{
    struct alig_consumer * consumer;
    rc_t rc = alig_consumer_make( &consumer, ctx->min_count, &ctx->limits, ctx->lookup,
                                  ctx->slice, ctx->fasta, ctx->purge );
    if ( rc == 0 )
    {
        bool running = true;

        while( rc == 0 && running )
        {
            Vector alignments;
            rc = SAMExtractorGetAlignments( extractor, &alignments );
            if ( rc == 0 )
            {
                uint32_t idx;
                uint32_t start = VectorStart( &alignments );
                uint32_t len = VectorLength( &alignments );
                if ( len == 0 ) running = false;
                for ( idx = start; running && idx < ( start + len ); ++idx )
                {
                    Alignment * ex_al = VectorGet( &alignments, idx );
                    if ( ex_al != NULL )
                    {
                        AlignmentT al;
                        
                        StringInitCString( &al.rname, ex_al->rname );
                        StringInitCString( &al.cigar, ex_al->cigar );
                        StringInitCString( &al.read, ex_al->read );
                        al.pos = ex_al->pos;
                        inspect_sam_flags( &al, ex_al->flags );
            
                        rc = alig_consumer_consume_alignment( consumer, &al );
                    }
                }

                /* if we are reducing, purge the allele-dict if the spread exeeds max. alignment-length */
                if ( rc == 0 )
                    rc = alig_consumer_visit_and_purge( consumer, ctx->purge );
                
                /* now we are telling the extractor that we are done the alignments.... */
                rc = SAMExtractorInvalidateAlignments( extractor );
            }
            
            if ( rc == 0 && running ) running = ( Quitting() == 0 );
        }
        
        ctx->unsorted = alig_consumer_get_unsorted( consumer );
        alig_consumer_release( consumer );
    }
   
    return rc;
}

static rc_t produce_events_from_extractor( tool_ctx * ctx, const KFile * f, const char * filename )
{
    Extractor * extractor;
    rc_t rc = SAMExtractorMake( &extractor, f, 1 );
    if ( rc != 0 )
        log_err( "error (%R) creating sam-extractor from %s", rc, filename );
    else
    {
        rc_t rc2;
        /* we have to invalidate ( ask the the extractor to internally destroy ) the headers
           even if we did not ask for them!!! */
        rc = SAMExtractorInvalidateHeaders( extractor );
            
        if ( rc == 0 )
            rc = process_alignments_from_extractor( ctx, extractor );

        rc2 = SAMExtractorRelease( extractor );
        if ( rc2 != 0 )
            log_err( "error (%R) releasing sam-extractor from %s", rc, filename );
    }
    return rc;
}

static rc_t produce_events_from_stdin_checked( tool_ctx * ctx )
{
    const KFile * f;
    rc_t rc = KFileMakeStdIn( &f );
    if ( rc != 0 )
        log_err( "error (%R) opening stdin as file", rc );
    else
    {
        rc = produce_events_from_extractor( ctx, f, "stdin" );
        KFileRelease( f );
    }
    return rc;
}

static rc_t produce_events_from_file_checked( tool_ctx * ctx, KDirectory * dir, const char * filename )
{
    const KFile * f;
    rc_t rc = KDirectoryOpenFileRead( dir, &f, "%s", filename );
    if ( rc != 0 )
        log_err( "error (%R) opening '%s'", rc, filename );
    else
    {
        rc = produce_events_from_extractor( ctx, f, filename );
        KFileRelease( f );
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t main_sam_input( const Args * args, tool_ctx * ctx )
{
    rc_t rc = 0;

	if ( ctx->ref == NULL )
	{
        /* we always need a reference-file for SAM-input, if we have none: error */
        log_err( "not reference-file given!" );
        rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
		ctx->fasta = NULL;
	
	}
	else
	{
		ctx->fasta = loadFastaFile( 0, ctx->ref );
		if ( ctx->fasta == NULL )
		{
			/* we always need a reference-file for SAM-input, if we have none: error */
			log_err( "cannot open reference '%s'", ctx->ref );
			rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
		}
		else
		{
			uint32_t count;
			rc = ArgsParamCount( args, &count );
			if ( rc == 0 )
			{
				if ( count < 1 )
				{
					/* no filename(s) given at commandline ... assuming stdin as source */
                    rc = produce_events_from_stdin_checked( ctx );
				}
				else
				{
					KDirectory * dir = NULL;
					rc = KDirectoryNativeDir( &dir );
					if ( rc == 0 )
					{
				
						uint32_t idx;
						const char * filename;
                        for ( idx = 0; rc == 0 && idx < count; ++idx )
                        {
                            rc = ArgsParamValue( args, idx, ( const void ** )&filename );
                            if ( rc == 0 )
                                rc = produce_events_from_file_checked( ctx, dir, filename );
                        }
						KDirectoryRelease( dir );
					}
				}
			}
			unloadFastaFile( ctx->fasta );
            ctx->fasta = NULL;
		}
	}
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t produce_events_from_accession( tool_ctx * ctx, const char * acc )
{
    rc_t rc = 0;
    
    if ( ctx->ref == NULL )
    {
        /* no fasta-file given, get the fasta out of the accession! */
        /* header in expandCIGAR.h code in fasta-file.[hpp/cpp]*/
        ctx->fasta = loadcSRA( acc );
    }

    if ( ctx->fasta != NULL )
    {
        struct alig_iter * ai;
        rc = alig_iter_make( &ai, acc );
        if ( rc == 0 ) 
        {
            struct alig_consumer * consumer;
            rc = alig_consumer_make( &consumer, ctx->min_count, &ctx->limits, ctx->lookup, ctx->slice,
                                     ctx->fasta, ctx->purge );
            if ( rc == 0 )
            {
                bool running = true;
                while ( running )
                {
                    AlignmentT alignment;
                    uint64_t processed;
                    
                    running = alig_iter_get( ai, &alignment, &processed );
                    if ( running )
                    {
                        /* consume the alignment */
                        if ( rc == 0 )
                            rc = alig_consumer_consume_alignment( consumer, &alignment );

                        /* if we are reducing, purge the allele-dict if the spread exeeds the purge-value * 2 */
                        if ( rc == 0 && ( ( processed % ctx->purge ) == 0 ) )
                            rc = alig_consumer_visit_and_purge( consumer, ctx->purge );

                        /* check if we are quitting... */
                        if ( rc == 0 ) { running = ( Quitting() == 0 ); }
                    }
                }
                ctx->unsorted = alig_consumer_get_unsorted( consumer );
                alig_consumer_release( consumer );
            }
            alig_iter_release( ai );
        }
        unloadFastaFile( ctx->fasta );
        ctx->fasta = NULL;
    }
    return rc;
}

static rc_t main_csra_input( const Args * args, tool_ctx * ctx )
{
    rc_t rc = 0;
    uint32_t count;
    rc = ArgsParamCount( args, &count );
    if ( rc == 0 )
    {
        if ( count < 1 )
        {
            /* no accession(s) given at commandline ... */
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            log_err( "no accession(s) given" );
        }
        else
        {
            if ( ctx->ref != NULL )
            {
                /* we are processing cSRA-accessions, but the user gave as a FASTA-file as reference! */
                ctx->fasta = loadFastaFile( 0, ctx->ref );
                if ( ctx->fasta == NULL )
                {
                    log_err( "cannot open reference '%s'", ctx->ref );
                    rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
                }
            }
            
            if ( rc == 0 )
            {
                uint32_t idx;
                const char * acc;
                for ( idx = 0; rc == 0 && idx < count; ++idx )
                {
                    rc = ArgsParamValue( args, idx, ( const void ** )&acc );
                    if ( rc == 0 )
                        rc = produce_events_from_accession( ctx, acc );
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
                if ( ctx.csra )
                    rc = main_csra_input( args, &ctx );
                else
                    rc = main_sam_input( args, &ctx );
            }
            if ( ctx.unsorted )
                log_err( "the source was not sorted, alleles are not correctly counted" );
            release_tool_ctx( &ctx );
        }
        ArgsWhack ( args );
    }
    else
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    return rc;
}
