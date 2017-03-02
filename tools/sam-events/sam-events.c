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

#include <align/samextract-lib.h>
#include "ref_reader.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* because we use a lib sam-extract written i C++, we have to define this symbol !!! */
void *__gxx_personality_v0;

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

#define OPTION_REF     "reference"
#define ALIAS_REF      "r"
static const char * ref_usage[]       = { "the reference(s) as fasta-file", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CANON,     ALIAS_CANON,    NULL, canon_usage,     1,   false,       false },
    { OPTION_SOURCE,    ALIAS_SOURCE,   NULL, source_usage,    1,   false,       false },
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
    const char * ref;
    bool canonicalize;
    bool show_source;
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

static rc_t get_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_bool( args, OPTION_CANON, &ctx->canonicalize );
    if ( rc == 0 )
        rc = get_bool( args, OPTION_SOURCE, &ctx->show_source );
    if ( rc == 0 )
        rc = get_charptr( args, OPTION_REF, &ctx->ref );
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */


static rc_t print_hdr( Header * hdr )
{
    rc_t rc = KOutMsg( "%s: ", hdr->headercode );
    if ( rc == 0 )
    {
        uint32_t len = VectorLength( &hdr->tagvalues );
        uint32_t idx;
        uint32_t start = VectorStart( &hdr->tagvalues );
        for ( idx = start; rc == 0 && idx < ( start + len ); ++idx )
        {
            TagValue * tag = VectorGet( &hdr->tagvalues, idx );
            if ( tag != NULL )
                rc = KOutMsg( "%s = %s ", tag->tag, tag->value );
        }
        if ( rc == 0 )
            rc = KOutMsg( "\n" );
    }
    return rc;
}

static rc_t print_hdrs( Vector * hdrs )
{
    rc_t rc = 0;
    uint32_t len = VectorLength( hdrs );
    uint32_t idx;
    uint32_t start = VectorStart( hdrs );
    for ( idx = start; rc == 0 && idx < ( start + len ); ++idx )
    {
        Header *hdr = VectorGet( hdrs, idx );
        if ( hdr != NULL )
            rc = print_hdr( hdr );
    }
    return rc;
}

static rc_t get_headers( const tool_ctx * ctx, Extractor * extractor )
{
    rc_t rc = 0;
    Vector headers;
    rc = SAMExtractorGetHeaders( extractor, &headers );
    if ( rc == 0 )
    {
        if ( ctx->show_source )
            rc = print_hdrs( &headers );
        if ( rc == 0 )
            rc = SAMExtractorInvalidateHeaders( extractor );
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */
static rc_t print_alignment( Alignment * al, struct Refseq_Reader ** rr )
{
    rc_t rc = KOutMsg( "%s.%u\t%s\t%s\n", al->rname, al->pos, al->cigar, al->read );
    
    if ( rc == 0 )
    {
        if ( rr == NULL )
            rc = refseq_reader_make( rr, al->rname );
        else
        {
            bool has_name;
            rc = refseq_reader_has_name( *rr, al->rname, &has_name );
            if ( rc == 0 )
            {
                if ( !has_name )
                {
                    refseq_reader_release( *rr );
                    rc = refseq_reader_make( rr, al->rname );
                }
            }
        }
        
        if ( rc == 0 )
        {
            /* now we read from our ref-reader... */
            char * buffer;
            uint64_t pos = ( al->pos - 1 );
            size_t available;
            
            rc = KOutMsg( "REF\t" );
            if ( rc == 0 )
            {
                rc = refseq_reader_get( *rr, &buffer, pos, &available );
                if ( rc == 0 )
                {
                    size_t read_len = string_size( al->read );
                    if ( available > read_len )
                        available = read_len;
                    rc = KOutMsg( "%.*s\n", available, buffer );
                }
                else
                    rc = KOutMsg( "%R\n", rc );
            }
        }
    }
    return rc;
}

static rc_t get_alignments( const tool_ctx * ctx, Extractor * extractor )
{
    rc_t rc = 0;
    bool done = false;
    struct Refseq_Reader * rr = NULL;
    
    while ( rc == 0 && !done )
    {
        Vector alignments;
        rc = SAMExtractorGetAlignments( extractor, &alignments );
        if ( rc == 0 )
        {
            uint32_t len = VectorLength( &alignments );
            done = ( len == 0 );
            if ( !done )
            {
                uint32_t idx;
                uint32_t start = VectorStart( &alignments );
                for ( idx = start; idx < ( start + len ); ++idx )
                {
                    Alignment *al = VectorGet( &alignments, idx );
                    if ( al != NULL )
                    {
                        if ( ctx->show_source )
                            rc = print_alignment( al, &rr );
                    }
                }
            }
            rc = SAMExtractorInvalidateAlignments( extractor );
        }
    }
    refseq_reader_release( rr );
    return rc;
}


static rc_t produce_events( const tool_ctx * ctx, const char * src )
{
    Extractor * extractor;
    rc_t rc = SAMExtractorMake( &extractor, src, 1 );
    if ( rc != 0 )
        KOutMsg( "error (%R) creating sam-extractor from %s\n", rc, src );
    else
    {
        rc = get_headers( ctx, extractor );
        if ( rc == 0 )
            rc = get_alignments( ctx, extractor );

        rc_t rc2 = SAMExtractorRelease( extractor );
        if ( rc2 != 0 )
            KOutMsg( "error (%R) releasing sam-extractor from %s\n", rc, src );
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

            /*
            struct cFastaFile * ref = loadFastaFile( 0, ctx.ref );
            KOutMsg( "ref: %s\n", ctx.ref );
            */
            
            rc = ArgsParamCount( args, &count );
            if ( rc == 0 )
            {
                if ( count < 1 )
                    rc = Usage( args );
                else
                {
                    uint32_t idx;
                    for ( idx = 0; rc == 0 && idx < count; ++idx )
                    {
                        const char * src;
                        rc = ArgsParamValue( args, idx, ( const void ** )&src );
                        if ( rc == 0 )
                            rc = produce_events( &ctx, src );
                    }
                }
            }
            
            /*
            unloadFastaFile( ref );
            */
        }
        ArgsWhack ( args );
    }
    else
        KOutMsg( "ArgsMakeAndHandle() failed %R\n", rc );
    return rc;
}
