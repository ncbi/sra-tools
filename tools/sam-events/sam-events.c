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

/*
#define OPTION_PURGE   "purge"
#define ALIAS_PURGE    "p"
static const char * purge_usage[]     = { "after how many ref-pos in dict perform pureg", NULL };

#define OPTION_LOG     "log"
#define ALIAS_LOG      "o"
static const char * log_usage[]       = { "log the alignments into text-file", NULL };
*/
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
    { OPTION_REF,       ALIAS_REF,      NULL, ref_usage,       1,   true,        false },
    { OPTION_FAST,      NULL,          NULL, fast_usage,      1,   false,       false },
    { OPTION_CSRA,      NULL,          NULL, csra_usage,      1,   false,       false },
    
    { OPTION_MINCNT,    ALIAS_MINCNT,   NULL, mincnt_usage,    1,   true,        false },
    { OPTION_MINFWD,    NULL,          NULL, minfwd_usage,    1,   true,        false },
    { OPTION_MINREV,    NULL,          NULL, minrev_usage,    1,   true,        false },
    { OPTION_MINTP,     NULL,          NULL, mintp_usage,     1,   true,        false },
    { OPTION_MINTN,     NULL,          NULL, mintn_usage,     1,   true,        false },

    /*
    { OPTION_PURGE,     ALIAS_PURGE,    NULL, purge_usage,     1,   true,        false },
    { OPTION_LOG,       ALIAS_LOG,      NULL, log_usage,       1,   true,        false }
    */
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
    bool fast, csra, unsorted;
    counters limits;
    const char * ref;
    struct cFastaFile * fasta;
    const char * logfilename;
    struct Writer * log;
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
    rc_t rc;
    
    ctx->fasta = NULL;
    ctx->log = NULL;
    ctx->logfilename = NULL;
    ctx->purge = 4096;
    ctx->unsorted = false;
    
    rc = get_bool( args, OPTION_FAST, &ctx->fast );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_CSRA, &ctx->csra );
    if ( rc == 0 )
        rc = get_charptr( args, OPTION_REF, &ctx->ref );

    /*
    if ( rc == 0 )
        rc = get_uint32( args, OPTION_PURGE, &ctx->purge, 4096 );
    if ( rc == 0 )
        rc = get_charptr( args, OPTION_LOG, &ctx->logfilename );
    */
    
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
static rc_t CC print_event( const counters * count, const String * rname, size_t position,
                            uint32_t deletes, uint32_t inserts, const char * bases,
                            void * user_data );

#define REF_NAME_LEN 1024

typedef struct current_ref
{
    const String * rname;
    const char * ref_bases;
    unsigned ref_bases_count;
    struct Allele_Dict * ad;
    uint64_t position;
    int idx;
    uint32_t min_count;
    const counters * limits;
    VNamelist * seen_refs;
    bool unsorted;
} current_ref;


static rc_t init_current_ref( current_ref * current, uint32_t min_count, const counters * limits )
{
    rc_t rc = 0;
    current->rname = NULL;
    current->ref_bases = NULL;
    current->ref_bases_count = 0;
    current->ad = NULL;
    current->position = 0;
    current->idx = -1;
    current->min_count = min_count;
    current->limits = limits;
    current->unsorted = false;
    rc = VNamelistMake( &current->seen_refs, 10 );
    return rc;
}

static rc_t finish_current_ref( current_ref * current )
{
    rc_t rc = allele_dict_visit_all_and_release( current->ad, print_event, current );
    if ( rc == 0 )
        rc = VNamelistRelease( current->seen_refs );
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */
static rc_t CC print_event( const counters * count, const String * rname, size_t position,
                            uint32_t deletes, uint32_t inserts, const char * bases,
                            void * user_data )
{
    const current_ref * current = user_data;
    const counters * limits = current->limits;
    bool print;
    
    if ( current->min_count > 0 )
        print = ( count->fwd + count->rev >= current->min_count );
    else
        print = true;
    if ( print && limits->fwd > 0 )
        print = ( count->fwd >= limits->fwd );
    if ( print && limits->rev > 0 )
        print = ( count->rev >= limits->rev );
    if ( print && limits->t_pos > 0 )
        print = ( count->t_pos >= limits->t_pos );
    if ( print && limits->t_neg > 0 )
        print = ( count->t_neg >= limits->t_neg );

    /*COUNT-FWD - COUNT-REV - COUNT-t+ - COUNT-t- - REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
    
    if ( print )
        return KOutMsg( "%d\t%d\t%d\t%d\t%S\t%d\t%d\t%d\t%s\n",
                         count->fwd, count->rev, count->t_pos, count->t_neg,
                         rname, position, deletes, inserts, bases );
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
            rc = allele_dict_put( current->ad, allele_start, allele_len, allele_len, allele, al->fwd, al->first );

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
            rc = allele_dict_put( current->ad, allele_start, 0, allele_len, allele, al->fwd, al->first );

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
            rc = allele_dict_put( current->ad, allele_start, ev->length, 0, NULL, al->fwd, al->first );
        
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
    int idx;    
    for ( idx = 0; rc == 0 && idx < num_events; ++idx )
    {
        struct Event * ev = &events[ idx ];
        switch( ev->type )
        {
            case mismatch   : rc = process_mismatch( ctx, al, ev, current ); break;
            case insertion  : rc = process_insert( ctx, al, ev, current ); break;
            case deletion   : rc = process_delete( ctx, al, ev, current ); break;
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
    
    /* validate the cigar: */
    if ( rc == 0 )
    {
        unsigned seqLength;
        /* in expandCIGAR.h ( expandCIGAR.cpp ), we have to call it to get the refLength ! */
        valid = validateCIGAR( al->cigar.len, al->cigar.addr, &refLength, &seqLength );
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

static void inspect_sam_flags( AlignmentT * al, uint32_t sam_flags )
{
    al->fwd = ( ( sam_flags & 0x10 ) == 0 );
    if ( ( sam_flags & 0x01 ) == 0x01 )
        al->first = ( ( sam_flags & 0x40 ) == 0x40 );
    else
        al->first = true;
}
                    
static rc_t switch_reference( struct cFastaFile * fasta, 
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


static rc_t check_rname( struct cFastaFile * fasta, const String * rname, uint64_t position, current_ref * current )
{
    rc_t rc = 0;
    int cmp = 1;
    
    if ( current->idx >= 0 )
        cmp = StringCompare( current->rname, rname );
        
    if ( cmp != 0 )
    {
        /* we are entering a new reference! */
        int32_t idx;
        
        rc = VNamelistContainsString( current->seen_refs, rname, &idx );
        if ( rc == 0 )
        {
            if ( idx < 0 )
                rc = VNamelistAppendString( current->seen_refs, rname );
            else
            {
                current->unsorted = true;
                log_err( "unsorted: %S", rname );
            }
        }
        
        /* print all entries found in the allele-dict, and then release the whole allele-dict */
        if ( rc == 0 && current->ad != NULL )
            rc = allele_dict_visit_all_and_release( current->ad, print_event, current );
       
        /* switch to the new reference!
           - store the new refname in the current-struct
           - get the index of the new reference into the fasta-file
        */
        if ( rc == 0 )
            rc = switch_reference( fasta, current, rname );
            
        /* make a new allele-dict */
        if ( rc == 0 )
            rc = allele_dict_make( &current->ad, rname );
    }
    else
    {
        if ( position < current->position )
            current->unsorted = true;
    }
    current->position = position;
    
    return rc;
}


static rc_t process_alignments_from_extractor( tool_ctx * ctx, Extractor * extractor )
{
    rc_t rc = 0;
    bool done = false;
    current_ref current;

    init_current_ref( &current, ctx->min_count, &( ctx->limits ) );

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
                    inspect_sam_flags( &al, ex_al->flags );
                    
                    rc = check_rname( ctx->fasta, &al.rname, al.pos, &current );
                    
                    /* this is the meat!!! */
                    if ( rc == 0 )
                        rc = process_alignment( ctx, &al, &current );
                }
            }

            /* if we are reducing, purge the allele-dict if the spread exeeds max. alignment-length */
            if ( rc == 0 )
                rc = allele_dict_visit_and_purge( current.ad, ctx->purge, print_event, &current );
            
            /* now we are telling the extractor that we are done the alignments.... */
            rc = SAMExtractorInvalidateAlignments( extractor );
        }
    }
    
    /* print the final dictionary content and release the dictionary */
    if ( rc == 0 )
        rc = finish_current_ref( &current );
    ctx->unsorted = current.unsorted;
    
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
typedef struct line_handler_ctx
{
    const tool_ctx * ctx;
    current_ref current;
    uint64_t counter;
    rc_t rc;
} line_handler_ctx;

#define SAM_COL_FLAGS 1
#define SAM_COL_RNAME 2
#define SAM_COL_RPOS 3
#define SAM_COL_CIGAR 5
#define SAM_COL_SEQ 9

static rc_t extract_String( const VNamelist * l, uint32_t idx, String * dst )
{
    const char * s;
    rc_t rc = VNameListGet( l, idx, &s );
    if ( rc == 0 )
        StringInitCString( dst, s );
    return rc;
}

static rc_t extract_Num( const VNamelist * l, uint32_t idx, uint32_t * dst )
{
    const char * s;
    rc_t rc = VNameListGet( l, idx, &s );
    if ( rc == 0 )
        *dst = atoi( s );
    return rc;
}

static rc_t extract_Num64( const VNamelist * l, uint32_t idx, uint64_t * dst )
{
    const char * s;
    rc_t rc = VNameListGet( l, idx, &s );
    if ( rc == 0 )
        *dst = atoi( s );
    return rc;
}

static rc_t CC on_file_line( const String * line, void * data )
{
    if ( line->addr[ 0 ] != '@' )
    {
        line_handler_ctx * lhctx = data;
        VNamelist * l;
        rc_t rc = VNamelistFromString( &l, line, '\t' );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = VNameListCount( l, &count );
            if ( rc == 0 && count > 10 )
            {
                AlignmentT al;
                uint32_t flags;
                rc = extract_Num( l, SAM_COL_FLAGS, &flags );
                if ( rc == 0 )
                    inspect_sam_flags( &al, flags );
                if ( rc == 0 && ( ( flags & 0x4 ) == 0 ) )
                {
                    /* handle only mapped ( aligned ) sam-lines */
                    if ( rc == 0 )
                        rc = extract_String( l, SAM_COL_RNAME, &( al.rname ) );
                    if ( rc == 0 )
                        rc = extract_Num64( l, SAM_COL_RPOS, &( al.pos ) );
                    if ( rc == 0 )
                        rc = extract_String( l, SAM_COL_CIGAR, &( al.cigar ) );
                    if ( rc == 0 )
                        rc = extract_String( l, SAM_COL_SEQ, &( al.read ) );                

                    if ( rc == 0 )
                    {
                        /* KOutMsg( "---%S\t%d\t%S\t%S\n", &al.rname, al.pos, &al.cigar, &al.read ); */
                        rc = check_rname( lhctx->ctx->fasta, &al.rname, al.pos, &( lhctx->current ) );
                        
                        /* this is the meat!!! */
                        if ( rc == 0 )
                            rc = process_alignment( lhctx->ctx, &al, &( lhctx->current ) );
                    }
                }
            }
            VNamelistRelease ( l );
        }
        lhctx->rc = rc;
    }
    return 0;
}


static rc_t produce_events_from_KFile( tool_ctx * ctx, const KFile * f )
{
    line_handler_ctx lhctx = { .ctx = ctx, .counter = 0, .rc = 0 };
    
    init_current_ref( &lhctx.current, ctx->min_count, &( ctx->limits ) );

    ProcessFileLineByLine( f, on_file_line, &lhctx );
    
    /* print the final dictionary content and release the dictionary */
    if ( lhctx.rc == 0 )
        lhctx.rc = finish_current_ref( &( lhctx.current ) );
    ctx->unsorted = lhctx.current.unsorted;
    
    return lhctx.rc;
}


static rc_t produce_events_from_stdin_unchecked( tool_ctx * ctx )
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


static rc_t produce_events_from_file_unchecked( tool_ctx * ctx, const KDirectory * dir, const char * filename )
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
                if ( ctx->fast )
                    rc = produce_events_from_stdin_unchecked( ctx );
                else
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
                    
                    if ( ctx->fast )
                    {
                        for ( idx = 0; rc == 0 && idx < count; ++idx )
                        {
                            rc = ArgsParamValue( args, idx, ( const void ** )&filename );
                            if ( rc == 0 )
                                rc = produce_events_from_file_unchecked( ctx, dir, filename );
                        }
                    }
                    else
                    {
                        for ( idx = 0; rc == 0 && idx < count; ++idx )
                        {
                            rc = ArgsParamValue( args, idx, ( const void ** )&filename );
                            if ( rc == 0 )
                                rc = produce_events_from_file_checked( ctx, dir, filename );
                        }
                    }
                    KDirectoryRelease( dir );
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
    uint32_t cigar_idx, rname_idx, rpos_idx, read_idx, sam_flags_idx;

} tbl_src;


static rc_t produce_events_from_tbl_src( tool_ctx * ctx, const tbl_src * tsrc )
{
    rc_t rc = 0;
    bool done = false;
    int64_t row_id = tsrc->first_row;
    uint64_t rows_processed = 0;
    current_ref current;
                            
    init_current_ref( &current, ctx->min_count, &(ctx->limits) );
    
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
        
        if ( rc == 0 )
        {
            /*get the strand and first ( by looking at SAM_FLAGS ) */
            uint32_t * pp;
            rc = VCursorCellDataDirect( tsrc->curs, row_id, tsrc->sam_flags_idx, &elem_bits, ( const void ** )&pp, &boff, &row_len );
            if ( rc != 0 )
                log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.SAM_FLAGS[ %ld ] %R", tsrc->acc, row_id, rc );
            else
            {
                uint32_t flags = pp[ 0 ];
                /* replicate sam-dump's special treatment of sam-flags if no unaligned reads are dumped
                   and the mate is unaligned */
                /* if ( ( flags & 0x1 ) && ( flags & 0x8 ) ) flags &= ~0xC9; */
                inspect_sam_flags( &al, flags );
            }
        }

        /* check if the REFERENCE-NAME has changed, flush the allele-dict if so, make a new one */
        if ( rc == 0 )
            rc = check_rname( ctx->fasta, &al.rname, al.pos, &current );

        /* the common alignment-processing: get the cigar-events, canonicalize the events, put them into the allele-dictionary */
        if ( rc == 0 )
            rc = process_alignment( ctx, &al, &current );

        /* if we are reducing, purge the allele-dict if the spread exeeds the purge-value * 2 */
        if ( rc == 0 )
            rc = allele_dict_visit_and_purge( current.ad, ctx->purge, print_event, &current );
        
        /* handle the loop-termination and find the next row to handle... */
        if ( rc == 0 )
        {
            rows_processed++;
    
            if ( rows_processed >= tsrc->row_count )
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
        rc = finish_current_ref( &current );
    ctx->unsorted = current.unsorted;
    
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
                        rc = add_col_to_cursor( tsrc.curs, &tsrc.sam_flags_idx, "SAM_FLAGS", acc );
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
            if ( ctx.logfilename != NULL )
                rc = writer_make( &ctx.log, ctx.logfilename );
            if ( rc == 0 )
            {
                if ( ctx.csra )
                    rc = main_csra_input( args, &ctx );
                else
                    rc = main_sam_input( args, &ctx );
                
                if ( ctx.log != NULL )
                    writer_release( ctx.log );
            }
            if ( ctx.unsorted )
                log_err( "the source was not sorted, alleles are not correctly counted" );
        }
        ArgsWhack ( args );
    }
    else
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    return rc;
}
