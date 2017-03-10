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
#define ALIAS_REDUCE   "u"
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
#define ALIAS_FAST     "f"
static const char * fast_usage[]      = { "bypass SAM validation", NULL };

#define OPTION_REF     "reference"
#define ALIAS_REF      "r"
static const char * ref_usage[]       = { "the reference(s) as fasta-file", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CANON,     ALIAS_CANON,    NULL, canon_usage,     1,   false,       false },
    { OPTION_SOURCE,    ALIAS_SOURCE,   NULL, source_usage,    1,   false,       false },
    { OPTION_VALID,     ALIAS_VALID,    NULL, valid_usage,     1,   false,       false },
    { OPTION_REDUCE,    ALIAS_REDUCE,   NULL, reduce_usage,    1,   false,       false },
    { OPTION_FAST,      ALIAS_FAST,     NULL, fast_usage,      1,   false,       false },    
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
    bool canonicalize;
    bool show_source;
    bool validate_cigar;
    bool reduce;
    bool fast;
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
    rc_t rc = ArgsOptionValue( args, option, 0, ( const void ** )value );
    if ( rc != 0 )
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
    int idx;
    char name[ REF_NAME_LEN ];
    size_t name_size;
    uint32_t min_count;
    const char * ref_bases;
    unsigned ref_bases_count;
    struct Allele_Dict * ad;
} current_ref;



/* ----------------------------------------------------------------------------------------------- */
static rc_t CC print_event( size_t position,
                            uint32_t deletes, uint32_t inserts, uint32_t count,
                            const char * bases, void * user_data )
{
    current_ref * ref = user_data;
    /*COUNT - REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
    if ( count >= ref->min_count )
        return KOutMsg( "%d\t%s\t%d\t%d\t%d\t%s\n", count, ref->name, position, deletes, inserts, bases );
    else
        return 0;
}


/* ----------------------------------------------------------------------------------------------- */

static rc_t process_mismatch( const tool_ctx * ctx,
                             Alignment * al,
                             struct Event * ev,
                             const current_ref * current )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                      /* to be created */
                                     current->ref_bases,            /* the reference-bases */
                                     current->ref_bases_count,      /* length of the reference */
                                     ( al->pos - 1 ) + ev->refPos, /* position of event */
                                     ev->length,                    /* length of deletion */
                                     &( al->read[ ev->seqPos ] ),  /* inserted bases */
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
            if ( current->ad == NULL )
                /*REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
                rc = KOutMsg( "%s\t%d\t%d\t%d\t%s\n", al->rname, allele_start, allele_len, allele_len, allele );
            else
                rc = allele_dict_put( current->ad, allele_start, allele_len, allele_len, allele );
        }   
        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


static rc_t process_insert( const tool_ctx * ctx,
                             Alignment * al,
                             struct Event * ev,
                             const current_ref * current )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                      /* to be created */
                                     current->ref_bases,            /* the reference-bases */
                                     current->ref_bases_count,      /* length of the reference */
                                     ( al->pos - 1 ) + ev->refPos, /* position of event */
                                     0,                             /* length of deletion */
                                     &( al->read[ ev->seqPos ] ),  /* inserted bases */
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
            if ( current->ad == NULL )
                /*REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
                rc = KOutMsg( "%s\t%d\t%d\t%d\t%s\n", al->rname, allele_start, 0, allele_len, allele );
            else
                rc = allele_dict_put( current->ad, allele_start, 0, allele_len, allele );
        }   
        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


static rc_t process_delete( const tool_ctx * ctx,
                             Alignment * al,
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
            if ( current->ad == NULL )
                /*REFNAME - EVENT-POS - DELETES - INSERTS - BASES */        
                rc = KOutMsg( "%s\t%d\t%d\t%d\t\n", al->rname, allele_start, ev->length );
            else
                rc = allele_dict_put( current->ad, allele_start, ev->length, 0, NULL );
        }   
        
        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


const char * const ev_txt[] = { "none", "match", "mismatch", "insert", "delete" };

static rc_t process_events( const tool_ctx * ctx,
                            Alignment * al,
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
                               Alignment * al,
                               current_ref * current )
{
    rc_t rc = 0;
    int valid = 0;
    unsigned refLength;
    
    if ( ctx->show_source )
        rc = KOutMsg( "\n\t[%s].%u\t%s\t%s\n", al->rname, al->pos, al->cigar, al->read );
    
    /* validate the cigar: */
    if ( rc == 0 )
    {
        unsigned seqLength;
        valid = validateCIGAR( 0, al->cigar, &refLength, &seqLength );
        if ( ctx->validate_cigar )
        {
            if ( valid == 0 )
            {
                size_t seq_len = string_size( al->read );
                if ( seq_len != seqLength )
                {
                    log_err( "cigar '%s' invalid ( %d != %d )", al->cigar, seq_len, seqLength );
                    rc = RC( rcApp, rcNoTarg, rcDecoding, rcParam, rcInvalid );
                }
            }
            else
            {
                log_err( "cigar '%s' invalid", al->cigar );
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
            int num_events = expandCIGAR( events,
                                          NUM_EVENTS,
                                          offset,
                                          &remaining,
                                          al->cigar,
                                          al->read,
                                          al->pos - 1,
                                          ctx->fasta,
                                          current->idx );
            if ( num_events < 0 )
                log_err( "expandCIGRAR failed for cigar '%s'", al->cigar );
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
                             const char * new_name,
                             size_t new_name_size )
{
    rc_t rc = 0;
    
    /* prime the current-ref-name with the ref-name of the alignment */
    string_copy( current->name, sizeof current->name, new_name, new_name_size );
    current->name_size = new_name_size;
    current->idx = FastaFile_getNamedSequence( fasta, 0, new_name );
    if ( current->idx < 0 )
    {
        rc = RC( rcExe, rcNoTarg, rcVisiting, rcId, rcInvalid );
        log_err( "'%s' not found in fasta-file", new_name );
    }
    else
        current->ref_bases_count = FastaFile_getSequenceData( fasta, current->idx, &current->ref_bases );
    return rc;
}


static rc_t new_allele_dict( current_ref * current )
{
    rc_t rc = 0;
    /* create a new allele-dictionary with each reference we enter */
    if ( current->ad != NULL )
    {
        rc = allele_dict_visit( current->ad, 0, print_event, current );
        if ( rc == 0 )
            rc = allele_dict_release( current->ad );
    }
    if ( rc == 0 )
        rc = allele_dict_make( &current->ad );
    return rc;
}


static rc_t check_rname( const tool_ctx * ctx, const char * rname, current_ref * current )
{
    rc_t rc = 0;
    
    /* check if we have entered a new reference... */
    size_t rname_size = string_size( rname );
    if ( current->idx < 0 )
    {
        rc = enter_reference( ctx->fasta, current, rname, rname_size );
        if ( rc == 0 && ctx->reduce )
            rc = new_allele_dict( current );
    }
    else
    {
        /* check if we enter a new reference */
        int cmp = string_cmp( current->name, current->name_size,
                              rname, rname_size,
                              current->name_size > rname_size ? current->name_size : rname_size );
        if ( cmp != 0 )
        {
            /* we are entering a new reference */
            rc = enter_reference( ctx->fasta, current, rname, rname_size );
            if ( rc == 0 && ctx->reduce )
                rc = new_allele_dict( current );
        }
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


static rc_t process_alignments( const tool_ctx * ctx, Extractor * extractor )
{
    rc_t rc = 0;
    bool done = false;
    uint32_t counter = 0;
    current_ref current;
    
    current.idx = -1;
    current.ad = NULL;
    current.min_count = ctx->min_count;

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
                Alignment *al = VectorGet( &alignments, idx );
                if ( al != NULL )
                {
                    rc = check_rname( ctx, al->rname, &current );
                    
                    /* this is the meat!!! */
                    if ( rc == 0 )
                        rc = process_alignment( ctx, al, &current );
                    
                    /* use the limit for testing */
                    if ( ctx->limit > 0 )
                        done = ( ++counter >= ctx->limit );
                }
            }

            /* if we are reducing, purge the allele-dict if the spread exeeds max. alignment-length */
            if ( rc == 0 && ctx->reduce )
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
        /* we have to invalidate ( ask the the extractor to internally destroy ) the headers
           even if we did not ask for them!!! */
        rc = SAMExtractorInvalidateHeaders( extractor );
            
        if ( rc == 0 )
            rc = process_alignments( ctx, extractor );

        rc_t rc2 = SAMExtractorRelease( extractor );
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

static rc_t CC on_stdin_line( const String * line, void * data )
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
                Alignment al;
                
                rc2 = VNameListGet( l, 2, &( al.rname ) );
                if ( rc2 == 0 )
                {
                    const char * spos;
                    rc2 = VNameListGet( l, 3, &spos );
                    if ( rc2 == 0 )
                    {
                        al.pos = atoi( spos );
                        rc2 = VNameListGet( l, 5, &( al.cigar ) );
                        if ( rc2 == 0 )
                        {
                            rc2 = VNameListGet( l, 9, &( al.read ) );
                            if ( rc == 0 )
                            {
                                rc2 = check_rname( lhctx->ctx, al.rname, &( lhctx->current ) );
                                
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

    ProcessFileLineByLine( f, on_stdin_line, &lhctx );
    
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

            ctx.fasta = loadFastaFile( 0, ctx.ref );        
            if ( ctx.fasta == NULL )
            {
                log_err( "cannot open reference '%s'", ctx.ref );
                RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
            }
            else
            {
                rc = ArgsParamCount( args, &count );
                if ( rc == 0 )
                {
                    if ( count < 1 )
                    {
                        /* no filename(s) given at commandline ... assuming stdin as source */
                        rc = produce_events_from_stdin( &ctx );
                    }
                    else
                    {
                        uint32_t idx;
                        const char * filename;
                        
                        if ( ctx.fast )
                        {
                            KDirectory * dir = NULL;
                            rc = KDirectoryNativeDir( &dir );
                            if ( rc == 0 )
                            {
                                for ( idx = 0; rc == 0 && idx < count; ++idx )
                                {
                                    rc = ArgsParamValue( args, idx, ( const void ** )&filename );
                                    if ( rc == 0 )
                                        rc = produce_events_from_file_unchecked( &ctx, dir, filename );
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
                                    rc = produce_events_from_file_checked( &ctx, filename );
                            }
                        }
                    }
                }
                unloadFastaFile( ctx.fasta );
            }
        }
        ArgsWhack ( args );
    }
    else
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    return rc;
}
