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

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

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
#include "allele_dict.h"


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

#define OPTION_CANON   "canonicalize"
#define ALIAS_CANON    "c"
static const char * canon_usage[]     = { "canonicalize events", NULL };

#define OPTION_SOURCE  "show-source"
#define ALIAS_SOURCE   "s"
static const char * source_usage[]    = { "show source from sam-extractor", NULL };

#define OPTION_VALID   "validate"
#define ALIAS_VALID    "a"
static const char * valid_usage[]     = { "validate cigar-string", NULL };

#define OPTION_REDUCE  "reduce"
static const char * reduce_usage[]     = { "reduce ( count ) the events", NULL };

#define OPTION_LIMIT   "limit"
#define ALIAS_LIMIT    "l"
static const char * limit_usage[]     = { "limit the output to this number of alignments", NULL };

#define OPTION_MINCNT  "min-count"
#define ALIAS_MINCNT   "m"
static const char * mincnt_usage[]    = { "minimum count per event", NULL };

#define OPTION_PURGE   "purge"
#define ALIAS_PURGE    "p"
static const char * purge_usage[]     = { "after how many ref-pos in dict perform pureg", NULL };

#define OPTION_FAST    "fast"
static const char * fast_usage[]      = { "bypass SAM validation", NULL };

#define OPTION_CSRA    "csra"
static const char * csra_usage[]      = { "take input as csra not as sam-file", NULL };

#define OPTION_REF     "reference"
#define ALIAS_REF      "r"
static const char * ref_usage[]       = { "the reference(s) as fasta-file", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CANON,     ALIAS_CANON,    NULL, canon_usage,     1,   false,       false },
    { OPTION_SOURCE,    ALIAS_SOURCE,   NULL, source_usage,    1,   false,       false },
    { OPTION_VALID,     ALIAS_VALID,    NULL, valid_usage,     1,   false,       false },
    { OPTION_REDUCE,    NULL,          NULL, reduce_usage,    1,   false,       false },
    { OPTION_FAST,      NULL,          NULL, fast_usage,      1,   false,       false },
    { OPTION_CSRA,      NULL,          NULL, csra_usage,      1,   false,       false },
    { OPTION_LIMIT,     ALIAS_LIMIT,    NULL, limit_usage,     1,   true,        false },
    { OPTION_MINCNT,    ALIAS_MINCNT,   NULL, mincnt_usage,    1,   true,        false },
    { OPTION_PURGE,     ALIAS_PURGE,    NULL, purge_usage,     1,   true,        false },
    { OPTION_REF,       ALIAS_REF,      NULL, ref_usage,       1,   true,        false }
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
    uint32_t limit, min_count, purge;
    bool canonicalize, show_source, validate_cigar, reduce, fast, csra;
    const char * ref;
    struct cFastaFile * fasta;
} tool_ctx;


static rc_t get_bool( const Args * args, const char *option, bool *value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    *value = ( rc == 0 && count > 0 );
    return rc;
}


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


static rc_t get_uint32( const Args * args, const char *option, uint32_t * value, uint32_t dflt )
{
    const char * svalue;
    rc_t rc = get_charptr( args, option, &svalue );
    if ( rc == 0 && svalue != NULL )
        *value = atoi( svalue );
    else
        *value = dflt;
    return 0;
}

static rc_t get_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_bool( args, OPTION_CANON, &ctx->canonicalize );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_SOURCE, &ctx->show_source );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_VALID, &ctx->validate_cigar );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_REDUCE, &ctx->reduce );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_FAST, &ctx->fast );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_CSRA, &ctx->csra );
    if ( rc == 0 )
        rc = get_charptr( args, OPTION_REF, &ctx->ref );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_LIMIT, &ctx->limit, 0 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_PURGE, &ctx->purge, 4096 );
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_MINCNT, &ctx->min_count, 1 );
    ctx->fasta = NULL;
    if ( ctx->purge == 0 ) ctx->purge = 4096;
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


#define REF_NAME_LEN 1024

typedef struct current_ref
{
    const String * rname;
    const char * ref_bases;
    unsigned ref_bases_count;
    struct Allele_Dict * ad;
    int idx;
    uint32_t min_count;    
} current_ref;



/* ----------------------------------------------------------------------------------------------- */
static rc_t CC print_event( uint32_t count, const String * rname, size_t position,
                            uint32_t deletes, uint32_t inserts, const char * bases,
                            void * user_data )
{
    current_ref * ref = user_data;
    /*COUNT - REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
    if ( count >= ref->min_count )
        return KOutMsg( "%d\t%S\t%d\t%d\t%d\t%s\n", count, rname, position, deletes, inserts, bases );
    else
        return 0;
}


/* ----------------------------------------------------------------------------------------------- */

static rc_t process_mismatch( const tool_ctx * ctx,
                             AlignmentT * al,
                             struct Event * ev,
                             const current_ref * current )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                      /* to be created */
                                     current->ref_bases,            /* the reference-bases */
                                     current->ref_bases_count,      /* length of the reference */
                                     ( al->pos - 1 ) + ev->refPos, /* position of event */
                                     ev->length,                    /* length of deletion */
                                     &( al->read.addr[ ev->seqPos ] ),  /* inserted bases */
                                     ev->length,                    /* number of inserted bases */
                                     refvarAlgRA );
    if ( rc != 0 )
        log_err( "RefVariationIUPACMake() failed rc=%R", rc );
    else
    {
        rc_t rc2;
        INSDC_dna_text const * allele;
        size_t allele_len, allele_start;
        
        rc = RefVariationGetAllele( ref_var, &allele, &allele_len, &allele_start );
        if ( rc != 0 )
            log_err( "RefVariationGetAllele() failed rc=%R", rc );
        else
        {
            if ( ctx->reduce )
                rc = allele_dict_put( current->ad, allele_start, allele_len, allele_len, allele );
            else
                /*REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
                rc = KOutMsg( "%S\t%d\t%d\t%d\t%s\n", &al->rname, allele_start, allele_len, allele_len, allele );
        }   
        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


static rc_t process_insert( const tool_ctx * ctx,
                             AlignmentT * al,
                             struct Event * ev,
                             const current_ref * current )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                      /* to be created */
                                     current->ref_bases,            /* the reference-bases */
                                     current->ref_bases_count,      /* length of the reference */
                                     ( al->pos - 1 ) + ev->refPos, /* position of event */
                                     0,                             /* length of deletion */
                                     &( al->read.addr[ ev->seqPos ] ),  /* inserted bases */
                                     ev->length,                    /* number of inserted bases */
                                     refvarAlgRA );
    if ( rc != 0 )
        log_err( "RefVariationIUPACMake() failed rc=%R", rc );
    else
    {
        rc_t rc2;
        INSDC_dna_text const * allele;
        size_t allele_len, allele_start;
        
        rc = RefVariationGetAllele( ref_var, &allele, &allele_len, &allele_start );
        if ( rc != 0 )
            log_err( "RefVariationGetAllele() failed rc=%R", rc );
        else
        {
            if ( ctx->reduce )
                rc = allele_dict_put( current->ad, allele_start, 0, allele_len, allele );
            else
                /*REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
                rc = KOutMsg( "%S\t%d\t%d\t%d\t%s\n", &al->rname, allele_start, 0, allele_len, allele );
        }   
        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


static rc_t process_delete( const tool_ctx * ctx,
                             AlignmentT * al,
                             struct Event * ev,
                             const current_ref * current )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                      /* to be created */
                                     current->ref_bases,            /* the reference-bases */
                                     current->ref_bases_count,      /* length of the reference */
                                     ( al->pos - 1 ) + ev->refPos, /* position of event */
                                     ev->length,                    /* length of deletion */
                                     NULL,                          /* inserted bases */
                                     0,                             /* number of inserted bases */
                                     refvarAlgRA );
    if ( rc != 0 )
        log_err( "RefVariationIUPACMake() failed rc=%R", rc );
    else
    {
        rc_t rc2;
        INSDC_dna_text const * allele;
        size_t allele_len, allele_start;
        
        rc = RefVariationGetAllele( ref_var, &allele, &allele_len, &allele_start );
        if ( rc != 0 )
            log_err( "RefVariationGetAllele() failed rc=%R", rc );
        else
        {
            if ( ctx->reduce )
                rc = allele_dict_put( current->ad, allele_start, ev->length, 0, NULL );
            else
                /*REFNAME - EVENT-POS - DELETES - INSERTS - BASES */        
                rc = KOutMsg( "%S\t%d\t%d\t%d\t\n", &al->rname, allele_start, ev->length );
        }   
        
        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


const char * const ev_txt[] = { "none", "match", "mismatch", "insert", "delete" };

static rc_t process_events( const tool_ctx * ctx,
                            AlignmentT * al,
                            unsigned refLength,
                            struct Event * const events,
                            int num_events,
                            const current_ref * current )
{
    rc_t rc = 0;
    
    /*
        printing the reference-slice for this alignment...
        rc = KOutMsg( "FASTA: %.*s\n", refLength, &( current->ref_bases[ al->pos - 1 ] ) );
    */

    int idx;    
    for ( idx = 0; rc == 0 && idx < num_events; ++idx )
    {
        struct Event * ev = &events[ idx ];
        if ( ev->type != match )
        {
            if ( ctx->show_source )
                rc = KOutMsg( "\ttype = %s\tlength = %d\trefPos = %d\tseqPos = %d\n",
                              ev_txt[ ev->type ], ev->length, ev->refPos, ev->seqPos );

            if ( rc == 0 )
            {
                switch( ev->type )
                {
                    case mismatch   : rc = process_mismatch( ctx, al, ev, current ); break;
                    case insertion  : rc = process_insert( ctx, al, ev, current ); break;
                    case deletion   : rc = process_delete( ctx, al, ev, current ); break;
                }
            }
        }
    }
    return rc;
}


#define NUM_EVENTS 1024

static rc_t process_alignment( const tool_ctx * ctx,
                               AlignmentT * al,
                               current_ref * current )
{
    rc_t rc = 0;
    int valid = 0;
    unsigned refLength;
    
    if ( ctx->show_source )
        rc = KOutMsg( "\n\t[%S].%u\t%S\t%S\n", al->rname, al->pos, al->cigar, al->read );
    
    /* validate the cigar: */
    if ( rc == 0 )
    {
        unsigned seqLength;
        /* in expandCIGAR.h ( expandCIGAR.cpp ) */
        valid = validateCIGAR( al->cigar.len, al->cigar.addr, &refLength, &seqLength );
        if ( ctx->validate_cigar )
        {
            if ( valid == 0 )
            {
                if ( al->read.len != seqLength )
                {
                    log_err( "cigar '%S' invalid ( %d != %d )", &al->cigar, al->read.len, seqLength );
                    rc = RC( rcApp, rcNoTarg, rcDecoding, rcParam, rcInvalid );
                }
            }
            else
            {
                log_err( "cigar '%S' invalid", &al->cigar );
                rc = RC( rcApp, rcNoTarg, rcDecoding, rcParam, rcInvalid );
            }
        }
    }

    if ( rc == 0 && valid == 0 )
    {
        int remaining = 1;
        int offset = 0;
        struct Event events[ NUM_EVENTS ];
        
        while ( rc == 0 && remaining > 0 )
        {
            /* in expandCIGAR.h ( expandCIGAR.cpp ) */
            int num_events = expandCIGAR( events,
                                          NUM_EVENTS,
                                          offset,
                                          &remaining,
                                          al->cigar.len,
                                          al->cigar.addr,
                                          al->read.addr,
                                          al->pos - 1,
                                          ctx->fasta,
                                          current->idx );
            if ( num_events < 0 )
                log_err( "expandCIGRAR failed for cigar '%S'", &al->cigar );
            else
            {
                rc = process_events( ctx, al, refLength, events, num_events, current );
                offset += num_events;
            }
        }
    }
    return rc;
}


static rc_t enter_reference( struct cFastaFile * fasta, 
                             current_ref * current,
                             const String * rname )
{
    rc_t rc = 0;
    
    /* prime the current-ref-name with the ref-name of the alignment */
    if ( current->rname != NULL )
        StringWhack ( current->rname );
    rc = StringCopy( &current->rname, rname );
    
    if ( fasta != NULL )
        current->idx = FastaFile_getNamedSequence( fasta, rname->size, rname->addr );
    else
        current->idx = -1;
        
    if ( current->idx < 0 )
    {
        rc = RC( rcExe, rcNoTarg, rcVisiting, rcId, rcInvalid );
        log_err( "'%S' not found in fasta-file", rname );
    }
    else
        current->ref_bases_count = FastaFile_getSequenceData( fasta, current->idx, &current->ref_bases );
    return rc;
}


static rc_t check_rname( const tool_ctx * ctx, const String * rname, current_ref * current )
{
    rc_t rc = 0;
    int cmp = 1;
    
    if ( current->idx >= 0 )
        cmp = StringCompare( current->rname, rname );
    
    if ( cmp != 0 )
    {
        /* we are entering a new reference */

        if ( ctx->reduce && current->ad != NULL )
        {
            uint64_t max_pos;
            rc = allele_get_min_max( current->ad, NULL, &max_pos );
            if ( rc == 0 )
                rc = allele_dict_visit( current->ad, max_pos + 1, print_event, current );
            if ( rc == 0 )
                rc = allele_dict_release( current->ad );
        }
        
        if ( rc == 0 )
            rc = enter_reference( ctx->fasta, current, rname );
        if ( rc == 0 && ctx->reduce )
            rc = allele_dict_make( &current->ad, rname );
    }
    
    return rc;
}


static rc_t finish_alignement_dict( current_ref * current )
{
    rc_t rc = 0;
    if ( current->ad != NULL )
    {
        uint64_t max_pos;
        rc = allele_get_min_max( current->ad, NULL, &max_pos );
        if ( rc == 0 )
            rc = allele_dict_visit( current->ad, max_pos + 1, print_event, current );
        if ( rc == 0 )
            rc = allele_dict_release( current->ad );
    }
    return rc;
}


static rc_t process_alignments_from_extractor( const tool_ctx * ctx, Extractor * extractor )
{
    rc_t rc = 0;
    bool done = false;
    uint32_t counter = 0;
    current_ref current = { .idx = -1, .ad = NULL, .rname = NULL, .min_count = ctx->min_count };

    while ( rc == 0 && !done )
    {
        Vector alignments;
        rc = SAMExtractorGetAlignments( extractor, &alignments );
        if ( rc == 0 )
        {
            uint32_t idx;
            uint32_t start = VectorStart( &alignments );
            uint32_t len = VectorLength( &alignments );
            done = ( len == 0 );
            for ( idx = start; !done && idx < ( start + len ); ++idx )
            {
                Alignment * ex_al = VectorGet( &alignments, idx );
                if ( ex_al != NULL )
                {
                    AlignmentT al;
                    
                    StringInitCString( &al.rname, ex_al->rname );
                    StringInitCString( &al.cigar, ex_al->cigar );
                    StringInitCString( &al.read, ex_al->read );
                    al.pos = ex_al->pos;

                    rc = check_rname( ctx, &al.rname, &current );
                    
                    /* this is the meat!!! */
                    if ( rc == 0 )
                        rc = process_alignment( ctx, &al, &current );

                    /* use the limit for testing */
                    if ( ctx->limit > 0 )
                        done = ( ++counter >= ctx->limit );
                }
            }

            /* if we are reducing, purge the allele-dict if the spread exeeds max. alignment-length */
            if ( rc == 0 && ctx->reduce && current.ad != NULL )
            {
                uint64_t min_pos, max_pos;
                rc = allele_get_min_max( current.ad, &min_pos, &max_pos );
                {
                    if ( ( max_pos - min_pos ) > ( ctx->purge * 2 ) )
                    {
                        rc = allele_dict_visit( current.ad, min_pos + ctx->purge, print_event, &current );
                        if ( rc == 0 )
                            rc = allele_dict_purge( current.ad, min_pos + ctx->purge );
                    }
                }
            }
            
            /* now we are telling the extractor that we are done the alignments.... */
            rc = SAMExtractorInvalidateAlignments( extractor );
        }
    }
    
    /* print the final dictionary content and release the dictionary */
    if ( rc == 0 )
        rc = finish_alignement_dict( &current );
    
    return rc;
}


static rc_t produce_events_from_file_checked( const tool_ctx * ctx, const char * file_name )
{
    Extractor * extractor;
    rc_t rc = SAMExtractorMake( &extractor, file_name, 1 );
    if ( rc != 0 )
        log_err( "error (%R) creating sam-extractor from %s", rc, file_name );
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
            log_err( "error (%R) releasing sam-extractor from %s", rc, file_name );
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */
typedef struct line_handler_ctx
{
    const tool_ctx * ctx;
    current_ref current;
    uint64_t counter;
    rc_t rc;
} line_handler_ctx;

static rc_t CC on_file_line( const String * line, void * data )
{
    rc_t rc = 0;
    if ( line->addr[ 0 ] != '@' )
    {
        line_handler_ctx * lhctx = data;
        VNamelist * l;
        rc_t rc2 = VNamelistFromString( &l, line, '\t' );
        if ( rc2 == 0 )
        {
            uint32_t count;
            rc2 = VNameListCount( l, &count );
            if ( rc2 == 0 && count > 10 )
            {
                AlignmentT al;
                const char * s;
                rc2 = VNameListGet( l, 2, &s );
                if ( rc2 == 0 )
                {
                    StringInitCString( &( al.rname ), s );
                    rc2 = VNameListGet( l, 3, &s );
                    if ( rc2 == 0 )
                    {
                        al.pos = atoi( s );
                        rc2 = VNameListGet( l, 5, &s );
                        if ( rc2 == 0 )
                        {
                            StringInitCString( &( al.cigar ), s );
                            rc2 = VNameListGet( l, 9, &s );
                            if ( rc2 == 0 )
                            {
                                StringInitCString( &( al.read ), s );
                                
                                /* KOutMsg( "---%S\t%d\t%S\t%S\n", &al.rname, al.pos, &al.cigar, &al.read ); */
                                rc2 = check_rname( lhctx->ctx, &al.rname, &( lhctx->current ) );
                                
                                /* this is the meat!!! */
                                if ( rc2 == 0 )
                                    rc2 = process_alignment( lhctx->ctx, &al, &( lhctx->current ) );
                                
                                /* use the limit for testing */
                                if ( lhctx->ctx->limit > 0 )
                                {
                                    lhctx->counter += 1;
                                    if ( lhctx->counter >= lhctx->ctx->limit )
                                        rc = -1;
                                }
                            }
                        }
                    }
                }
            }
            VNamelistRelease ( l );
        }
        lhctx->rc = rc2;
    }
    return rc;
}


static rc_t produce_events_from_KFile( const tool_ctx * ctx, const KFile * f )
{
    line_handler_ctx lhctx = { .ctx = ctx, .counter = 0, .rc = 0,
            .current.idx = -1, .current.ad = NULL, .current.min_count = ctx->min_count };

    ProcessFileLineByLine( f, on_file_line, &lhctx );
    
    /* print the final dictionary content and release the dictionary */
    if ( lhctx.rc == 0 )
        lhctx.rc = finish_alignement_dict( &lhctx.current );

    return lhctx.rc;
}


static rc_t produce_events_from_stdin( const tool_ctx * ctx )
{
    const KFile * f;
    rc_t rc = KFileMakeStdIn( &f );
    if ( rc != 0 )
        log_err( "error (%R) opening stdin as file", rc );
    else
    {
        rc = produce_events_from_KFile( ctx, f );
        KFileRelease( f );
    }
    return rc;
}


static rc_t produce_events_from_file_unchecked( const tool_ctx * ctx, const KDirectory * dir, const char * filename )
{
    const KFile * f;
    rc_t rc = KDirectoryOpenFileRead( dir, &f, "%s", filename );
    if ( rc != 0 )
        log_err( "error (%R) opening '%s'", rc, filename );
    else
    {
        rc = produce_events_from_KFile( ctx, f );
        KFileRelease( f );
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */
static rc_t main_sam_input( const Args * args, tool_ctx * ctx )
{
    rc_t rc = 0;

    ctx->fasta = loadFastaFile( 0, ctx->ref );
    if ( ctx->fasta == NULL )
    {
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
                rc = produce_events_from_stdin( ctx );
            }
            else
            {
                uint32_t idx;
                const char * filename;
                
                if ( ctx->fast )
                {
                    KDirectory * dir = NULL;
                    rc = KDirectoryNativeDir( &dir );
                    if ( rc == 0 )
                    {
                        for ( idx = 0; rc == 0 && idx < count; ++idx )
                        {
                            rc = ArgsParamValue( args, idx, ( const void ** )&filename );
                            if ( rc == 0 )
                                rc = produce_events_from_file_unchecked( ctx, dir, filename );
                        }
                        KDirectoryRelease( dir );
                    }
                }
                else
                {
                    for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        rc = ArgsParamValue( args, idx, ( const void ** )&filename );
                        if ( rc == 0 )
                            rc = produce_events_from_file_checked( ctx, filename );
                    }
                }
            }
        }
        unloadFastaFile( ctx->fasta );
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */
typedef struct tbl_src
{
    const VCursor *curs;
    const char * acc;
    int64_t first_row;
    uint64_t row_count;
    uint32_t cigar_idx, rname_idx, rpos_idx, read_idx;

} tbl_src;


static rc_t produce_events_from_tbl_src( const tool_ctx * ctx, const tbl_src * tsrc )
{
    rc_t rc = 0;
    bool done = false;
    int64_t row_id = tsrc->first_row;
    uint64_t rows_processed = 0;
    current_ref current = { .idx = -1, .ad = NULL, .rname = NULL, .min_count = ctx->min_count };
    
    while ( rc == 0 && !done )
    {
        AlignmentT al;
        uint32_t elem_bits, boff, row_len;

        /* get the CIGAR */
        rc = VCursorCellDataDirect( tsrc->curs, row_id, tsrc->cigar_idx, &elem_bits, ( const void ** )&al.cigar.addr, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.CIGAR[ %ld ] %R", tsrc->acc, row_id, rc );
        else
            al.cigar.len = al.cigar.size = row_len;
        
        if ( rc == 0 )
        {
            /* get the REFERENCE-NAME */
            rc = VCursorCellDataDirect( tsrc->curs, row_id, tsrc->rname_idx, &elem_bits, ( const void ** )&al.rname.addr, &boff, &row_len );
            if ( rc != 0 )
                log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_SEQ_ID[ %ld ] %R", tsrc->acc, row_id, rc );
            else
                al.rname.len = al.rname.size = row_len;
        }

        if ( rc == 0 )
        {
            /* get the READ */
            rc = VCursorCellDataDirect( tsrc->curs, row_id, tsrc->read_idx, &elem_bits, ( const void ** )&al.read.addr, &boff, &row_len );
            if ( rc != 0 )
                log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.READ[ %ld ] %R", tsrc->acc, row_id, rc );
            else
                al.read.len = al.read.size = row_len;
        }
        
        if ( rc == 0 )
        {
            /* get the REFERENCE-POSITION */
            uint32_t * pp;
            rc = VCursorCellDataDirect( tsrc->curs, row_id, tsrc->rpos_idx, &elem_bits, ( const void ** )&pp, &boff, &row_len );
            if ( rc != 0 )
                log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_POS[ %ld ] %R", tsrc->acc, row_id, rc );
            else
                al.pos = pp[ 0 ] + 1; /* to produce the same as the SAM-spec, a 1-based postion! */
        }
        
        /* check if the REFERENCE-NAME has changed, flush the allele-dict if so, make a new one */
        if ( rc == 0 )
            rc = check_rname( ctx, &al.rname, &current );

        /* the common alignment-processing: get the cigar-events, canonicalize the events, put them into the allele-dictionary */
        if ( rc == 0 )
            rc = process_alignment( ctx, &al, &current );

        /* if we are reducing, purge the allele-dict if the spread exeeds the purge-value * 2 */
        if ( rc == 0 && ctx->reduce && current.ad != NULL )
        {
            uint64_t min_pos, max_pos;
            rc = allele_get_min_max( current.ad, &min_pos, &max_pos );
            {
                if ( ( max_pos - min_pos ) > ( ctx->purge * 2 ) )
                {
                    rc = allele_dict_visit( current.ad, min_pos + ctx->purge, print_event, &current );
                    if ( rc == 0 )
                        rc = allele_dict_purge( current.ad, min_pos + ctx->purge );
                }
            }
        }
        
        /* handle the loop-termination and find the next row to handle... */
        if ( rc == 0 )
        {
            rows_processed++;
    
            if ( rows_processed >= tsrc->row_count )
                done = true;
            else if ( ctx->limit > 0 && ( rows_processed >= ctx->limit ) )
                done = true;
            else
            {
                /* find the next row ( to skip over empty rows... ) */
                int64_t nxt;
                rc = VCursorFindNextRowIdDirect( tsrc->curs, tsrc->read_idx, row_id + 1, &nxt );
                if ( rc != 0 )
                    log_err( "cannot find next row-id of '%s'.PRIMARY_ALIGNMENT.REF_POS[ %ld ] %R", tsrc->acc, row_id, rc );
                else
                    row_id = nxt;
            }
        }
    } /* while !done */
    
    /* print what is left in the allele-dictionary */
    if ( rc == 0 )
        rc = finish_alignement_dict( &current );

    return rc;
}

static rc_t add_col_to_cursor( const VCursor *curs, uint32_t * idx, const char * colname, const char * acc )
{
    rc_t rc = VCursorAddColumn( curs, idx, colname );
    if ( rc != 0 )
        log_err( "cannot add '%s' to cursor for '%s'.PRIMARY_ALIGNMENT %R", colname, acc, rc );
    return rc;
}

static rc_t produce_events_from_accession( tool_ctx * ctx, const VDBManager * mgr, const char * acc )
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
        const VDatabase *db;
        rc_t rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", acc );
        if ( rc != 0 )
            log_err( "cannot open '%s' as vdb-database %R", acc, rc );
        else
        {
            const VTable *tbl;
            rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
            if ( rc != 0 )
                log_err( "cannot open '%s'.PRIMARY_ALIGNMENT as vdb-table %R", acc, rc );
            else
            {
                tbl_src tsrc;
                tsrc.acc = acc;
                rc = VTableCreateCursorRead( tbl, &tsrc.curs );
                if ( rc != 0 )
                    log_err( "cannot create cursor for '%s'.PRIMARY_ALIGNMENT %R", acc, rc );
                else
                {
                    rc = add_col_to_cursor( tsrc.curs, &tsrc.cigar_idx, "CIGAR_SHORT", acc );
                    if ( rc == 0 )
                        rc = add_col_to_cursor( tsrc.curs, &tsrc.rname_idx, "REF_SEQ_ID", acc );
                    if ( rc == 0 )
                        rc = add_col_to_cursor( tsrc.curs, &tsrc.rpos_idx, "REF_POS", acc );
                    if ( rc == 0 )
                        rc = add_col_to_cursor( tsrc.curs, &tsrc.read_idx, "READ", acc );
                    if ( rc == 0 )
                    {
                        rc = VCursorOpen( tsrc.curs );
                        if ( rc != 0 )
                            log_err( "cannot open cursor for '%s'.PRIMARY_ALIGNMENT %R", acc, rc );
                        else
                        {
                            rc = VCursorIdRange( tsrc.curs, tsrc.read_idx, &tsrc.first_row, &tsrc.row_count );
                            if ( rc != 0 )
                                log_err( "cannot query row-range for '%s'.PRIMARY_ALIGNMENT %R", acc, rc );
                            else
                                rc = produce_events_from_tbl_src( ctx, &tsrc );
                        }
                    }
                    VCursorRelease( tsrc.curs );
                }
                VTableRelease( tbl );
            }
            VDatabaseRelease( db );
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
            log_err( "not accessins given" );
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
                const VDBManager * mgr;

                rc = VDBManagerMakeRead( &mgr, NULL );
                if ( rc != 0 )
                    log_err( "cannot create vdb-manager %R", rc );
                else
                {
                    uint32_t idx;
                    const char * acc;
                    for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        rc = ArgsParamValue( args, idx, ( const void ** )&acc );
                        if ( rc == 0 )
                            rc = produce_events_from_accession( ctx, mgr, acc );
                    }
                    VDBManagerRelease ( mgr );
                }
            }
            
            if ( ctx->ref != NULL && ctx->fasta != NULL )
                unloadFastaFile( ctx->fasta );
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
        rc = get_tool_ctx( args, &ctx );
        if ( rc == 0 )
        {
            if ( ctx.csra )
                rc = main_csra_input( args, &ctx );
            else
                rc = main_sam_input( args, &ctx );
        }
        ArgsWhack ( args );
    }
    else
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    return rc;
}
