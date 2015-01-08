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


#include "sam-dump-opts.h"
#include "perf_log.h"

#include <klib/time.h>
#include <align/quality-quantizer.h>
#include <sysalloc.h>

#define CURSOR_CACHE_SIZE 256*1024*1024

/* =========================================================================================== */


int cmp_pchar( const char * a, const char * b )
{
    int res = 0;
    if ( ( a != NULL )&&( b != NULL ) )
    {
        size_t len_a = string_size( a );
        size_t len_b = string_size( b );
        res = string_cmp ( a, len_a, b, len_b, ( len_a < len_b ) ? len_b : len_a );
    }
    return res;
}


/* =========================================================================================== */


static range * make_range( const uint64_t start, const uint64_t end )
{
    range *res = calloc( sizeof *res, 1 );
    if ( res != NULL )
    {
        res->start = start;
        res->end = end;
    }
    return res;
}


static int cmp_range( const range * a, const range * b )
{

    int res = ( a->start - b->start );
    if ( res == 0 )
        res = ( a->end - b->end );
    return res;
}


static bool range_overlapp( const range * a, const range * b )
{
    return ( !( ( b->end < a->start ) || ( b->start > a->end ) ) );
}


/* =========================================================================================== */


static reference_region * make_reference_region( const char *name )
{
    reference_region *res = calloc( sizeof *res, 1 );
    if ( res != NULL )
    {
        res->name = string_dup_measure ( name, NULL );
        if ( res->name == NULL )
        {
            free( res );
            res = NULL;
        }
        else
            VectorInit ( &res->ranges, 0, 5 );
    }
    return res;
}


static int CC cmp_range_wrapper( const void *item, const void *n )
{   return cmp_range( item, n ); }


static rc_t add_ref_region_range( reference_region * self, const uint64_t start, const uint64_t end )
{
    rc_t rc = 0;
    range *r = make_range( start, end );
    if ( r == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        rc = VectorInsert ( &self->ranges, r, NULL, cmp_range_wrapper );
        if ( rc != 0 )
            free( r );
    }
    return rc;
}


#define RR_NAME  1
#define RR_START 2
#define RR_END   3


static void put_c( char *s, size_t size, size_t *dst, char c )
{
    if ( *dst < ( size - 1 ) )
        s[ *dst ] = c;
    (*dst)++;
}

static void finish_txt( char *s, size_t size, size_t *dst )
{
    if ( *dst > size )
        s[ size - 1 ] = 0;
    else
        s[ *dst ] = 0;
    *dst = 0;
}

static uint64_t finish_num( char *s, size_t size, size_t *dst )
{
    uint64_t res = 0;
    char *endp;
    finish_txt( s, size, dst );
    res = strtou64( s, &endp, 10 );
    return res;
}


/* s = refname:1000-2000 */
static void parse_definition( const char *s, char * name, size_t len,
                              uint64_t *start, uint64_t *end )
{
    size_t n = string_size( s );

    *start = 0;
    *end   = 0;
    name[ 0 ] = 0;
    if ( n > 0 )
    {
        size_t i, st, dst = 0;
        char tmp[ 32 ];
        st = RR_NAME;
        for ( i = 0; i < n; ++i )
        {
            char c = s[ i ];
            switch( st )
            {
                case RR_NAME  : if ( c == ':' )
                                {
                                    finish_txt( name, len, &dst );
                                    st = RR_START;
                                }
                                else
                                {
                                    put_c( name, len, &dst, c );
                                }
                                break;

                case RR_START : if ( c == '-' )
                                {
                                    *start = finish_num( tmp, sizeof tmp, &dst );
                                    st = RR_END;
                                }
                                else if ( ( c >= '0' )&&( c <= '9' ) )
                                {
                                    put_c( tmp, sizeof tmp, &dst, c );
                                }
                                break;

                case RR_END   : if ( ( c >= '0' )&&( c <= '9' ) )
                                {
                                    put_c( tmp, sizeof tmp, &dst, c );
                                }
                                break;
            }
        }
        switch( st )
        {
            case RR_NAME  : finish_txt( name, len, &dst );
                            break;

            case RR_START : *start = finish_num( tmp, sizeof tmp, &dst );
                            break;

            case RR_END   : *end = finish_num( tmp, sizeof tmp, &dst );
                            break;
        }
    }
}


static void CC release_range_wrapper( void * item, void * data )
{
    free( item );
}


static void free_reference_region( reference_region * self )
{
    free( (void*)self->name );
    VectorWhack ( &self->ranges, release_range_wrapper, NULL );
    free( self );
}


static void check_ref_region_ranges( reference_region * self )
{
    uint32_t n = VectorLength( &self->ranges );
    uint32_t i = 0;
    range *a = NULL;
    while ( i < n )
    {
        range *b = VectorGet ( &self->ranges, i );
        bool remove = false;
        if ( a != NULL )
        {
            remove = range_overlapp( a, b );
            if ( remove )
            {
                range *r;
                a->end = b->end;
                VectorRemove ( &self->ranges, i, (void**)&r );
                free( r );
                n--;
            }
        }
        if ( !remove )
        {
            a = b;
            ++i;
        }
    }
}


/* =========================================================================================== */


static int CC reference_vs_pchar_wrapper( const void *item, const BSTNode *n )
{
    const reference_region * r = ( const reference_region * )n;
    return cmp_pchar( (const char *)item, r->name );
}

static reference_region * find_reference_region( BSTree * regions, const char * name )
{
    return ( reference_region * ) BSTreeFind ( regions, name, reference_vs_pchar_wrapper );
}


typedef struct frrl
{
    const char * name;
    size_t len;
} frrl;


static int cmp_pchar_vs_len( const char * a, const char * b, size_t len_b )
{
    int res = 0;
    if ( ( a != NULL )&&( b != NULL ) )
    {
        size_t len_a = string_size( a );
        res = string_cmp ( a, len_a, b, len_b, ( len_a < len_b ) ? len_b : len_a );
    }
    return res;
}

static int CC reference_vs_frr_wrapper( const void *item, const BSTNode *n )
{
    const reference_region * r = ( const reference_region * )n;
    const frrl * ctx = item;
    return cmp_pchar_vs_len( r->name, ctx->name, ctx->len );
}

static reference_region * find_reference_region_len( BSTree * regions, const char * name, size_t len )
{
    frrl ctx;
    ctx.name = name;
    ctx.len  = len;
    return ( reference_region * ) BSTreeFind ( regions, &ctx, reference_vs_frr_wrapper );
}


static int CC ref_vs_ref_wrapper( const BSTNode *item, const BSTNode *n )
{
   const reference_region * a = ( const reference_region * )item;
   const reference_region * b = ( const reference_region * )n;
   return cmp_pchar( a->name, b->name );
}

static rc_t add_refrange( BSTree * regions, const char * name, const uint64_t start, const uint64_t end )
{
    rc_t rc;

    reference_region * r = find_reference_region( regions, name );
    if ( r == NULL )
    {
        r = make_reference_region( name );
        if ( r == NULL )
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        else
            rc = add_ref_region_range( r, start, end );
        if ( rc == 0 )
            rc = BSTreeInsert ( regions, (BSTNode *)r, ref_vs_ref_wrapper );
        if ( rc != 0 )
            free_reference_region( r );
    }
    else
    {
        rc = add_ref_region_range( r, start, end );
    }
    return rc;
}


static rc_t parse_and_add_region( BSTree * regions, const char * s )
{
    rc_t rc = 0;
    uint64_t start, end;
    char name[ 64 ];
    parse_definition( s, name, sizeof name, &start, &end );
    if ( name[ 0 ] == 0 )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        uint64_t start_0based = ( start > 0 ) ? start - 1 : 0;
        uint64_t end_0based = ( end > 0 ) ? end - 1 : 0;
        if ( end_0based != 0 && end_0based < start_0based )
        {
            uint64_t temp = end_0based;
            end_0based = start_0based;
            start_0based = temp;
        }
        rc = add_refrange( regions, name, start_0based, end_0based );
    }
    return rc;
}


static void CC check_refrange_wrapper( BSTNode *n, void *data )
{
    check_ref_region_ranges( ( reference_region * ) n );
}


static void check_ref_regions( BSTree * regions )
{
    BSTreeForEach ( regions, false, check_refrange_wrapper, NULL );
}


static void CC release_ref_region_wrapper( BSTNode *n, void * data )
{
    free_reference_region( ( reference_region * ) n );
}


static void free_ref_regions( BSTree * regions )
{    
    BSTreeWhack ( regions, release_ref_region_wrapper, NULL );
}


static void CC count_ref_region_wrapper( BSTNode *n, void *data )
{   
    reference_region * r = ( reference_region * ) n;
    uint32_t * count = ( uint32_t * ) data;
    *count += VectorLength( &(r->ranges) );
}


static uint32_t count_ref_regions( BSTree * regions )
{
    uint32_t res = 0;
    BSTreeForEach ( regions, false, count_ref_region_wrapper, &res );
    return res;
}


/* =========================================================================================== */


static void CC foreach_reference_wrapper( BSTNode *n, void *data )
{   
    reference_region * r = ( reference_region * ) n;
    foreach_reference_func * func = ( foreach_reference_func * )data;

    if ( func->rc == 0 )
        func->rc = func->on_reference( r->name, &(r->ranges), func->data );
}


rc_t foreach_reference( BSTree * regions,
    rc_t ( CC * on_reference ) ( const char * name, Vector *ranges, void *data ), 
    void *data )
{
    foreach_reference_func func;

    func.on_reference = on_reference;
    func.data = data;
    func.rc = 0;
    BSTreeForEach ( regions, false, foreach_reference_wrapper, &func );
    return func.rc;
}


/* =========================================================================================== */

/* s = 1000-2000 */
static void parse_matepair_definition( const char *s, uint64_t *start, uint64_t *end )
{
    size_t n = string_size( s );
    if ( n > 0 )
    {
        size_t i, st, dst = 0;
        char tmp[ 32 ];
        st = RR_START;
        for ( i = 0; i < n; ++i )
        {
            char c = s[ i ];
            switch( st )
            {
                case RR_START : if ( c == '-' )
                                {
                                    *start = finish_num( tmp, sizeof tmp, &dst );
                                    st = RR_END;
                                }
                                else if ( ( c >= '0' )&&( c <= '9' ) )
                                {
                                    put_c( tmp, sizeof tmp, &dst, c );
                                }
                                break;

                case RR_END   : if ( ( c >= '0' )&&( c <= '9' ) )
                                {
                                    put_c( tmp, sizeof tmp, &dst, c );
                                }
                                break;
            }
        }
        switch( st )
        {
            case RR_START : *start = finish_num( tmp, sizeof tmp, &dst );
                            break;

            case RR_END   : *end = finish_num( tmp, sizeof tmp, &dst );
                            break;
        }
    }
}


static rc_t parse_and_add_matepair_dist( Vector * dist_vector, const char * s )
{
    rc_t rc = 0;
    uint64_t start = 0, end = 0;
    range *r;

    if ( cmp_pchar( s, "unknown" ) != 0 )
        parse_matepair_definition( s, &start, &end );

    r = make_range( start, end );
    if ( r == NULL )
    {
        rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
        (void)LOGERR( klogErr, rc, "error storing matepair-distance" );
    }
    else
    {
        rc = VectorInsert ( dist_vector, r, NULL, cmp_range_wrapper );
        if ( rc != 0 )
            free( r );
    }
    return rc;
}


bool filter_by_matepair_dist( const samdump_opts * opts, int32_t tlen )
{
    bool res = false;

    uint32_t count = VectorLength( &opts->mp_dist );
    if ( count > 0 )
    {
        uint32_t idx, tlen_v;

        res = false;
        if ( tlen < 0 )
            tlen_v = -(tlen);
        else
            tlen_v = tlen;

        for ( idx = 0; idx < count && !res; ++idx )
        {
            range * r = VectorGet( &opts->mp_dist, idx );
            if ( r != NULL )
                res = ( ( tlen_v >= r->start )&&( tlen_v <= r->end ) );
        }
    }
    return res;
}

/* =========================================================================================== */


static rc_t get_bool_option( Args * args, const char * name, bool *value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "error reading comandline option '$(t)'", "t=%s", name ) );
    }
    else
    {
        *value = ( count > 0 );
    }
    return rc;
}


static rc_t gather_region_options( Args * args, samdump_opts * opts )
{
    uint32_t count;

    rc_t rc = ArgsOptionCount( args, OPT_REGION, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "error counting comandline option '$(t)'", "t=%s", OPT_REGION ) );
    }
    else if ( count > 0 )
    {
        uint32_t i;

        BSTreeInit( &opts->regions );
        for ( i = 0; i < count && rc == 0; ++i )
        {
            const char * s;
            rc = ArgsOptionValue( args, OPT_REGION, i, &s );
            if ( rc != 0 )
            {
                (void)PLOGERR( klogErr, ( klogErr, rc, "error retrieving comandline option '$(t)'", "t=%s", OPT_REGION ) );
            }
            else
                rc = parse_and_add_region( &opts->regions, s );
        }
        if ( rc == 0 )
        {
            check_ref_regions( &opts->regions );
            opts->region_count = count_ref_regions( &opts->regions );
        }
    }
    return rc;
}


static rc_t gather_matepair_distances( Args * args, samdump_opts * opts )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, OPT_MATE_DIST, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "error counting comandline option '$(t)'", "t=%s", OPT_REGION ) );
    }
    else if ( count > 0 )
    {
        uint32_t i;

        VectorInit( &opts->mp_dist, 0, 10 );
        for ( i = 0; i < count && rc == 0; ++i )
        {
            const char * s;
            rc = ArgsOptionValue( args, OPT_MATE_DIST, i, &s );
            if ( rc != 0 )
            {
                (void)PLOGERR( klogErr, ( klogErr, rc, "error retrieving comandline option '$(t)'", "t=%s", OPT_MATE_DIST ) );
            }
            else
                rc = parse_and_add_matepair_dist( &opts->mp_dist, s );
        }
        opts->use_matepair_filter = ( VectorLength( &opts->mp_dist ) > 0 );
    }
    return rc;
}


static rc_t gather_2_bool( Args * args, const char *name1, const char *name2, bool *b1, bool *b2 )
{
    rc_t rc = get_bool_option( args, name1, b1 );
    if ( rc != 0 ) return rc;

    rc = get_bool_option( args, name2, b2 );
    if ( rc != 0 ) return rc;

    if ( *b1 && *b2 )
    {
        rc = RC( rcExe, rcNoTarg, rcValidating, rcParam, rcInvalid );
        (void)PLOGERR( klogErr, ( klogErr, rc, "the parameters '--$(p1)' and '--$(p2)' are mutually exclusive", 
                      "p1=%s,p2=%s", name1, name2 ) );
    }
    return rc;
}


static rc_t gather_flag_options( Args * args, samdump_opts * opts )
{
    bool dump_prim_only;

    /* do we have to dump unaligned reads too? */
    rc_t rc = get_bool_option( args, OPT_UNALIGNED, &opts->dump_unaligned_reads );
    if ( rc != 0 ) return rc;

    /* assume we have to dump primary alignments, maybe switched off later by other option */
    opts->dump_primary_alignments = true;

    /* we are dumping secondary alignments only if the user does
       not explicitly ask for "primary only" */
    rc = get_bool_option( args, OPT_PRIM_ONLY, &dump_prim_only );
    if ( rc != 0 ) return rc;
    opts->dump_secondary_alignments = !dump_prim_only;

    /* check if we are asked to be in cga-tool compatible mode */
    rc = get_bool_option( args, OPT_CG_SAM, &opts->dump_cg_sam );
    if ( rc != 0 ) return rc;

    /* check if we have to dump "EVIDENCE_INTERVAL" against the reference */
    rc = get_bool_option( args, OPT_CG_EVIDENCE, &opts->dump_cg_evidence );
    if ( rc != 0 ) return rc;

    /* check if we have to dump "EVIDENCE_ALIGNMENT" against "EVIDENCE_INTERVAL" */
    rc = get_bool_option( args, OPT_CG_EV_DNB, &opts->dump_cg_ev_dnb );
    if ( rc != 0 ) return rc;

    {
        bool dump_cg_mappings;

        /* look if cg-mappings is requested */
        rc = get_bool_option( args, OPT_CG_MAPP, &dump_cg_mappings );
        if ( rc != 0 ) return rc;

        /* do some mode logic ... */
        if ( !dump_cg_mappings &&
             ( opts->dump_cga_tools_mode || opts->dump_cg_evidence || opts->dump_cg_ev_dnb || opts->dump_cg_sam ) )
        {
            opts->dump_primary_alignments = false;
            opts->dump_secondary_alignments = false;
            opts->dump_unaligned_reads = false;
        }
        else
        {
            opts->dump_primary_alignments = true;
            opts->dump_secondary_alignments = !dump_prim_only;
            /* opts->dump_unaligned_reads = ( opts->dump_unaligned_reads && opts->region_count == 0 ); */
        }
    }

    /* do we have to print the long-version of the cigar-string? */
    rc = get_bool_option( args, OPT_CIGAR_LONG, &opts->use_long_cigar );
    if ( rc != 0 ) return rc;

    /* print the sequence-id instead of the sequence-name */
    rc = get_bool_option( args, OPT_USE_SEQID, &opts->use_seqid_as_refname );
    if ( rc != 0 ) return rc;

    /* print the READ where matched bases are replaced with '=' */
    rc = get_bool_option( args, OPT_HIDE_IDENT, &opts->print_matches_as_equal_sign );
    if ( rc != 0 ) return rc;

    {
        bool recalc_header, dont_print_header;

        /* do we have to recalculate the header instead of printing the meta-data */
        rc = get_bool_option( args, OPT_RECAL_HDR, &recalc_header );
        if ( rc != 0 ) return rc;
        /* should we suppress the header completely */
        rc = get_bool_option( args, OPT_NO_HDR, &dont_print_header );
        if ( rc != 0 ) return rc;
        if ( dont_print_header )
            opts->header_mode = hm_none;
        else if ( recalc_header )
            opts->header_mode = hm_recalc;
        else
            opts->header_mode = hm_dump;
    }

    {
        bool cigar_cg, cigar_cg_merge;

        /* do we have to transform cigar into cg-style ( has B/N ) */
        /* do we have to transform cg-data(length of read/patterns in cigar) into valid SAM (cigar/READ/QUALITY) */
        rc = gather_2_bool( args, OPT_CIGAR_CG, OPT_CIGAR_CG_M, &cigar_cg, &cigar_cg_merge );
        if ( rc != 0 ) return rc;
        if ( cigar_cg )
            opts->cigar_treatment = ct_cg_style;
        if ( cigar_cg_merge )
            opts->cigar_treatment = ct_cg_merge;
    }

    {
        bool gzip, bzip2;

        /* do we have to compress the output with gzip ? */
        /* do we have to compress the output with bzip2 ? */
        rc = gather_2_bool( args, OPT_GZIP, OPT_BZIP2, &gzip, &bzip2 );
        if ( rc != 0 ) return rc;
        if ( gzip )
            opts->output_compression = oc_gzip;
        if ( bzip2 )
            opts->output_compression = oc_bzip2;
    }


    {
        bool fasta, fastq;

        /* output in FASTA - mode  ? */
        /* output in FASTQ - mode  ? */
        rc = gather_2_bool( args, OPT_FASTA, OPT_FASTQ, &fasta, &fastq );
        if ( rc != 0 ) return rc;
        if ( fasta )
            opts->output_format = of_fasta;
        if ( fastq )
            opts->output_format = of_fastq;
    }


    /* do we have to reverse unaligned reads if the flag in the row says so */
    rc = get_bool_option( args, OPT_REVERSE, &opts->reverse_unaligned_reads );
    if ( rc != 0 ) return rc;

    /* do we have to add the spotgroup to the qname-column */
    rc = get_bool_option( args, OPT_SPOTGRP, &opts->print_spot_group_in_name );
    if ( rc != 0 ) return rc;

    /* do we have to report options instead of executing */
    rc = get_bool_option( args, OPT_REPORT, &opts->report_options );
    if ( rc != 0 ) return rc;

    /* do we have to print the alignment id in column XI */
    rc = get_bool_option( args, OPT_XI_DEBUG, &opts->print_alignment_id_in_column_xi );
    if ( rc != 0 ) return rc;

    /* do we have to print the alignment id in column XI */
    rc = get_bool_option( args, OPT_CACHEREPORT, &opts->report_cache );
    if ( rc != 0 ) return rc;

    /* do we have to dump unaligned reads only */
    rc = get_bool_option( args, OPT_UNALIGNED_ONLY, &opts->dump_unaligned_only );
    if ( rc != 0 ) return rc;

    /* do we have to print the spot-group in a special way */
    rc = get_bool_option( args, OPT_CG_NAMES, &opts->print_cg_names );
    if ( rc != 0 ) return rc;

    /* do we use a mate-cache */
    rc = get_bool_option( args, OPT_NO_MATE_CACHE, &opts->use_mate_cache );
    if ( rc != 0 ) return rc;
    opts->use_mate_cache = !opts->use_mate_cache;

    /* do we use a mate-cache */
    rc = get_bool_option( args, OPT_LEGACY, &opts->force_legacy );
    if ( rc != 0 ) return rc;

    rc = get_bool_option( args, OPT_NEW, &opts->force_new );
    if ( rc != 0 ) return rc;

    if ( opts->force_new && opts->force_legacy )
        opts->force_new = false;

    /* do we have to merge cigar ( and read/qual ) for cg-operations in cigar */
    rc = get_bool_option( args, OPT_CIGAR_CG_M, &opts->merge_cg_cigar );
    if ( rc != 0 ) return rc;

    /* do we enable rna-splicing */
    rc = get_bool_option( args, OPT_RNA_SPLICE, &opts->rna_splicing );
    if ( rc != 0 ) return rc;
    
    /* do we disable multi-threading */    
    rc = get_bool_option( args, OPT_NO_MT, &opts->no_mt );
    
    /* forcing to use the legacy code in case of Evidence-Dnb was requested */
    if ( rc == 0 )
    {
        if ( opts->dump_cg_ev_dnb )
        {
            opts->force_legacy = true;
            opts->force_new = false;
        }
    }
    return rc;
}


static rc_t get_str_option( Args * args, const char * name, const char ** s )
{
    uint32_t count;

    rc_t rc = ArgsOptionCount( args, name, &count );
    *s = NULL;
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "error counting comandline option '$(t)'", "t=%s", name ) );
    }
    else if ( count > 0 )
    {
        rc = ArgsOptionValue( args, name, 0, s );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "error retrieving comandline option '$(t)'", "t=%s", name ) );
        }
    }
    return rc;
}


static rc_t get_uint32_option( Args * args, const char * name, uint32_t def, uint32_t * value, bool dflt_if_zero )
{
    const char * s;
    rc_t rc = get_str_option( args, name, &s );
    *value = def;
    if ( rc == 0 && s != NULL )
    {
        char *endp;
        *value = strtou32( s, &endp, 0 );
        if ( *endp != '\0' )
        {
            rc = RC( rcExe, rcArgv, rcProcessing, rcParam, rcInvalid );
            (void)PLOGERR( klogErr, ( klogErr, rc, "error converting string to integer option '$(t)'", "t=%s", name ) );
        }
        else if ( dflt_if_zero && *value == 0 )
            *value = def;
    }
    return rc;
}


static rc_t get_int32_options( Args * args, const char * name, int32_t * value, bool * used )
{
    const char * s;
    rc_t rc = get_str_option( args, name, &s );
    if ( rc == 0 && s != NULL )
    {
        char *endp;
        *value = strtoi32( s, &endp, 0 );
        if ( *endp != '\0' )
        {
            rc = RC( rcExe, rcArgv, rcProcessing, rcParam, rcInvalid );
            (void)PLOGERR( klogErr, ( klogErr, rc, "error converting string to integer option '$(t)'", "t=%s", name ) );
        }
        else
        {
            if ( used != NULL )
                *used = true;
        }
    }
    return rc;
}

/*
static rc_t get_int64_option( Args * args, const char * name, uint64_t def, uint64_t * value )
{
    const char * s;
    rc_t rc = get_str_option( args, name, &s );
    *value = def;
    if ( rc == 0 && s != NULL )
    {
        char *endp;
        *value = strtou32( s, &endp, 0 );
        if ( *endp != '\0' )
        {
            rc = RC( rcExe, rcArgv, rcProcessing, rcParam, rcInvalid );
            (void)PLOGERR( klogErr, ( klogErr, rc, "error converting string to integer option '$(t)'", "t=%s", name ) );
        }
        else if ( *value == 0 )
            *value = def;
    }
    return rc;
}
*/

static rc_t gather_int_options( Args * args, samdump_opts * opts )
{
    rc_t rc = get_uint32_option( args, OPT_MATE_GAP, 10000, &opts->mape_gap_cache_limit, true );
    if ( rc == 0 )
        rc = get_uint32_option( args, OPT_OUTBUFSIZE, 1024 * 32, &opts->output_buffer_size, false );

    if ( rc == 0 )
    {
        uint32_t cs;
        rc = get_uint32_option( args, OPT_CURSOR_CACHE, CURSOR_CACHE_SIZE, &cs, false );
        if ( rc == 0 )
            opts->cursor_cache_size = ( size_t )cs;
    }

    if ( rc == 0 )
    {
        uint32_t mode;
        rc = get_uint32_option( args, OPT_DUMP_MODE, 0, &mode, true );
        if ( rc == 0 )
        {
            switch( mode )
            {
                case 0  : opts->dump_mode = dm_one_ref_at_a_time; break;
                case 1  : opts->dump_mode = dm_prepare_all_refs; break;
                default : opts->dump_mode = dm_one_ref_at_a_time; break;
            }
        }
    }

    if ( rc == 0 )
        rc = get_int32_options( args, OPT_MIN_MAPQ, &opts->min_mapq, &opts->use_min_mapq );

    if ( rc == 0 )
        rc = get_uint32_option( args, OPT_RNA_SPLICEL, 0, &opts->rna_splice_level, true );

    return rc;
}


static rc_t gather_string_options( Args * args, samdump_opts * opts )
{
    const char * s;
    uint32_t count;

    rc_t rc = get_str_option( args, OPT_PREFIX, &s );
    if ( rc == 0 && s != NULL )
    {
        opts->qname_prefix = string_dup_measure( s, NULL );
        if ( opts->qname_prefix == NULL )
        {
            rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
            (void)LOGERR( klogErr, rc, "error storing QNAME-PREFIX" );
        }
    }

    if ( rc == 0 )
    {
        rc = get_str_option( args, OPT_Q_QUANT, &s );
        if ( rc == 0 && s != NULL )
        {
            opts->qual_quant = string_dup_measure( s, NULL );
            if ( opts->qual_quant == NULL )
            {
                rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
                (void)LOGERR( klogErr, rc, "error storing QUAL-QUANT" );
            }
            else
            {
                bool bres = QualityQuantizerInitMatrix( opts->qual_quant_matrix, opts->qual_quant );
                if ( !bres )
                {
                    rc = RC( rcExe, rcNoTarg, rcValidating, rcParam, rcInvalid );
                    (void)LOGERR( klogErr, rc, "error initializing quality-quantizer-matrix" );
                }
            }
        }
    }

    if ( rc == 0 )
    {
        rc = get_str_option( args, OPT_OUTPUTFILE, &s );
        if ( rc == 0 && s != NULL )
        {
            opts->outputfile = string_dup_measure( s, NULL );
            if ( opts->outputfile == NULL )
            {
                rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
                (void)LOGERR( klogErr, rc, "error storing OUTPUTFILE" );
            }
        }
    }

    if ( rc == 0 )
    {
        rc = ArgsOptionCount( args, OPT_HDR_COMMENT, &count );
        if ( rc == 0 && count > 0 )
        {
            uint32_t i;
            rc = VNamelistMake( &opts->hdr_comments, 10 );
            for ( i = 0; i < count && rc == 0; ++i )
            {
                const char * src;
                rc = ArgsOptionValue( args, OPT_HDR_COMMENT, i, &src );
                if ( rc != 0 )
                {
                    (void)PLOGERR( klogErr, ( klogErr, rc, "error retrieving comandline option '$(t)' #$(n)", 
                                              "t=%s,n=%u", OPT_HDR_COMMENT, i ) );
                }
                else
                {
                    rc = VNamelistAppend( opts->hdr_comments, src );
                    if ( rc != 0 )
                    {
                        (void)PLOGERR( klogErr, ( klogErr, rc, "error appending hdr-comment '$(t)'", 
                                                  "t=%s", src ) );
                    }
                }
            }
            if ( rc != 0 )
            {
                VNamelistRelease( opts->hdr_comments );
                opts->hdr_comments = NULL;
            }
        }
    }

    if ( rc == 0 )
    {
        rc = ArgsParamCount( args, &count );
        if ( rc == 0 && count > 0 )
        {
            uint32_t i;
            rc = VNamelistMake( &opts->input_files, 10 );
            for ( i = 0; i < count && rc == 0; ++i )
            {
                const char * src;
                rc = ArgsParamValue( args, i, &src );
                if ( rc != 0 )
                {
                    (void)PLOGERR( klogErr, ( klogErr, rc, "error retrieving comandline param #$(n)", 
                                              "n=%u", i ) );
                }
                else
                {
                    rc = VNamelistAppend( opts->input_files, src );
                    if ( rc != 0 )
                    {
                        (void)PLOGERR( klogErr, ( klogErr, rc, "error appending input_file '$(t)'", 
                                                  "t=%s", src ) );
                    }
                }
            }
            if ( rc != 0 )
            {
                VNamelistRelease( opts->input_files );
                opts->input_files = NULL;
            }
        }
        opts->input_file_count = count;
    }

    rc = get_str_option( args, OPT_CIGAR_TEST, &s );
    if ( rc == 0 && s != NULL )
    {
        opts->cigar_test = string_dup_measure( s, NULL );
        if ( opts->cigar_test == NULL )
        {
            rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
            (void)LOGERR( klogErr, rc, "error storing CIGAR-TEST into sam-dump-options" );
        }
    }

    rc = get_str_option( args, OPT_HDR_FILE, &s );
    if ( rc == 0 && s != NULL )
    {
        opts->header_file = string_dup_measure( s, NULL );
        if ( opts->header_file == NULL )
        {
            rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
            (void)LOGERR( klogErr, rc, "error storing HDR-FILE into sam-dump-options" );
        }
        else
            opts->header_mode = hm_file;
    }

    opts->perf_log = NULL;

#if _DEBUGGING
    rc = get_str_option( args, OPT_TIMING, &s );
    if ( rc == 0 && s != NULL )
    {
        opts->timing_file = string_dup_measure( s, NULL );
        if ( opts->timing_file == NULL )
        {
            rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
            (void)LOGERR( klogErr, rc, "error storing timing-FILE into sam-dump-options" );
        }
        else
            opts->perf_log = make_perf_log( opts->timing_file, "sam-dump" );
    }
#endif

    rc = get_str_option( args, OPT_RNA_SPLICE_LOG, &s );
    if ( rc == 0 && s != NULL )
    {
        opts->rna_splice_log_file = string_dup_measure( s, NULL );
        if ( opts->rna_splice_log_file == NULL )
        {
            rc = RC( rcExe, rcNoTarg, rcValidating, rcMemory, rcExhausted );
            (void)LOGERR( klogErr, rc, "error storing rna-splice-log-FILE into sam-dump-options" );
        }
        else
            opts->rna_splice_log = make_rna_splice_log( opts->rna_splice_log_file, "sam-dump" );
    }

    return rc;
}


/*********************************************************************************************

                                            fa ... fully aligned reads
                                            ha ... half aligned reads
                                            hu ... half unaligned reads
                                            fu ... fully unaligned reads

    regions > 0     |   dump-unaligned  |
    ----------------------------------------------------------------------
        no          |       no          |   fa  ha  hu
        no          |       yes         |   fa  ha  hu  fu
        yes         |       no          |   fa  ha
        yes         |       yes         |   fa  ha  hu

*********************************************************************************************/
static void gather_unaligned_options( samdump_opts * opts )
{
    if ( opts->region_count == 0 )
    {
        if ( opts->dump_unaligned_only )
        {
            opts->print_half_unaligned_reads  = false;
            opts->print_fully_unaligned_reads = true;
        }
        else
        {
            opts->print_half_unaligned_reads  = true;
            opts->print_fully_unaligned_reads = opts->dump_unaligned_reads;
        }
    }
    else
    {
        opts->print_fully_unaligned_reads = false;
        opts->print_half_unaligned_reads  = opts->dump_unaligned_reads;
    }
}

/* =========================================================================================== */


static rc_t CC report_reference_cb( const char * name, Vector * ranges, void *data )
{
    rc_t rc = KOutMsg( "region: <%s>\n",  name );
    if ( rc == 0 )
    {
        uint32_t count = VectorLength( ranges );
        if ( count > 0 )
        {
            uint32_t i;
            for ( i = VectorStart( ranges ); i < count && rc == 0; ++i )
            {
                range *r = VectorGet( ranges, i );
                if ( r->end == 0 )
                {
                    if ( r->start == 0 )
                        rc = KOutMsg( "\t[ start ... end ]\n" );
                    else
                        rc = KOutMsg( "\t[ %u ... ]\n", r->start );
                }
                else
                    rc = KOutMsg( "\t[ %u ... %u ]\n", r->start, r->end );
            }
        }
    }
    return rc;
}


void report_options( const samdump_opts * opts )
{
    rc_t rc;
    uint32_t len;

    KOutMsg( "dump unaligned reads  : %s\n",  opts->dump_unaligned_reads ? "YES" : "NO" );
    KOutMsg( "dump unaligned only   : %s\n",  opts->dump_unaligned_only ? "YES" : "NO" );
    KOutMsg( "dump prim. alignments : %s\n",  opts->dump_primary_alignments ? "YES" : "NO" );
    KOutMsg( "dump sec. alignments  : %s\n",  opts->dump_secondary_alignments ? "YES" : "NO" );
    KOutMsg( "dump cga-tools-mode   : %s\n",  opts->dump_cga_tools_mode ? "YES" : "NO" );
    KOutMsg( "dump cg-evidence      : %s\n",  opts->dump_cg_evidence ? "YES" : "NO" );
    KOutMsg( "dump cg-ev-dnb        : %s\n",  opts->dump_cg_ev_dnb ? "YES" : "NO" );
    KOutMsg( "dump cg-sam           : %s\n",  opts->dump_cg_sam ? "YES" : "NO" );
    KOutMsg( "merge cg-cigar        : %s\n",  opts->merge_cg_cigar ? "YES" : "NO" );

    KOutMsg( "use long cigar        : %s\n",  opts->use_long_cigar ? "YES" : "NO" );
    KOutMsg( "print seqid           : %s\n",  opts->use_seqid_as_refname ? "YES" : "NO" );
    KOutMsg( "print READ with '='   : %s\n",  opts->print_matches_as_equal_sign ? "YES" : "NO" );
    KOutMsg( "reverse unaligned rd  : %s\n",  opts->reverse_unaligned_reads ? "YES" : "NO" );
    KOutMsg( "add spotgrp to qname  : %s\n",  opts->print_spot_group_in_name ? "YES" : "NO" );
    KOutMsg( "print id in col. XI   : %s\n",  opts->print_alignment_id_in_column_xi ? "YES" : "NO" );
    KOutMsg( "report matecache      : %s\n",  opts->report_cache ? "YES" : "NO" );
    KOutMsg( "print cg-names        : %s\n",  opts->print_cg_names ? "YES" : "NO" );

    switch( opts->header_mode )
    {
        case hm_none   : KOutMsg( "header-mode           : dont print\n" ); break;
        case hm_recalc : KOutMsg( "header-mode           : recalculate\n" ); break;
        case hm_dump   : KOutMsg( "header-mode           : print meta-data\n" ); break;
        case hm_file   : KOutMsg( "header-mode           : take from '%s'\n", opts->header_file ); break;
        default        : KOutMsg( "header-mode           : unknown\n" ); break;
    }

    switch( opts->cigar_treatment )
    {
        case ct_unchanged : KOutMsg( "cigar-treatment       : unchanged\n" ); break;
        case ct_cg_style  : KOutMsg( "cigar-treatment       : transform cg to 'cg-style\n" ); break;
        case ct_cg_merge  : KOutMsg( "cigar-treatment       : transform cg to valid SAM\n" ); break;
        default           : KOutMsg( "cigar-treatment       : unknow'\n" ); break;
    }

    switch( opts->output_compression )
    {
        case oc_none  : KOutMsg( "output-compression    : none\n" ); break;
        case oc_gzip  : KOutMsg( "output-compression    : gzip\n" ); break;
        case oc_bzip2 : KOutMsg( "output-compression    : bzip2\n" ); break;
        default       : KOutMsg( "output-compression    : unknown\n" ); break;
    }

    switch( opts->output_format )
    {
        case of_sam   : KOutMsg( "output-format         : SAM\n" ); break;
        case of_fasta : KOutMsg( "output-format         : FASTA\n" ); break;
        case of_fastq : KOutMsg( "output-format         : FASTQ\n" ); break;
        default       : KOutMsg( "output-format         : unknown\n" ); break;
    }

    switch( opts->dump_mode )
    {
        case dm_one_ref_at_a_time : KOutMsg( "dump-mode             : one ref at a time\n" ); break;
        case dm_prepare_all_refs  : KOutMsg( "dump-mode             : prepare all refs\n" ); break;
        default                   : KOutMsg( "dump-mode             : unknown\n" ); break;
    }

    KOutMsg( "number of regions     : %u\n",  opts->region_count );
    if ( opts->region_count > 0 )
        foreach_reference( (BSTree *)&opts->regions, report_reference_cb, NULL );

    if ( opts->qname_prefix == NULL )
        KOutMsg( "qname-prefix          : NONE\n" );
    else
        KOutMsg( "qname-prefix          : '%s'\n",  opts->qname_prefix );

    if ( opts->qual_quant == NULL )
        KOutMsg( "quality-quantization  : NONE\n" );
    else
        KOutMsg( "quality-quantization  : '%s'\n",  opts->qual_quant );

    if ( opts->hdr_comments != NULL )
    {
        rc = VNameListCount( opts->hdr_comments, &len );
        if ( rc == 0 && len > 0 )
        {
            uint32_t i;

            KOutMsg( "header-comments-count : %u\n",  len );
            for ( i = 0; i < len; ++i )
            {
                const char * s;
                rc = VNameListGet( opts->hdr_comments, i, &s );
                if ( rc == 0 && s != NULL )
                    KOutMsg( "header-comment[ %u ]  : '%s'\n",  i, s );
            }
        }
    }

    if ( opts->input_files != NULL )
    {
        rc = VNameListCount( opts->input_files, &len );
        if ( rc == 0 && len > 0 )
        {
            uint32_t i;

            KOutMsg( "input-file-count      : %u\n",  len );
            for ( i = 0; i < len; ++i )
            {
                const char * s;
                rc = VNameListGet( opts->input_files, i, &s );
                if ( rc == 0 && s != NULL )
                    KOutMsg( "input-file[ %u ]      : '%s'\n",  i, s );
            }
        }
    }

    len = VectorLength( &opts->mp_dist );
    if ( len > 0 )
    {
        uint32_t i;

        KOutMsg( "matepair-dist-filters : %u\n",  len );
        for ( i = 0; i < len; ++i )
        {
            range *r = VectorGet( &opts->mp_dist, i );
            if ( r != NULL )
                KOutMsg( "matepair-dist[ %u ]   : '%u ... %u'\n",  i, r->start, r->end );
        }
    }

    KOutMsg( "mate-gap-cache-limit  : %u\n",  opts->mape_gap_cache_limit );
    KOutMsg( "outputfile            : %s\n",  opts->outputfile );
    KOutMsg( "outputbuffer-size     : %u\n",  opts->output_buffer_size );
    KOutMsg( "cursor-cache-size     : %u\n",  opts->cursor_cache_size );

    KOutMsg( "use mate-cache        : %s\n",  opts->use_mate_cache ? "YES" : "NO" );
    KOutMsg( "force legacy code     : %s\n",  opts->force_legacy ? "YES" : "NO" );
    KOutMsg( "use min-mapq          : %s\n",  opts->use_min_mapq ? "YES" : "NO" );
    KOutMsg( "min-mapq              : %i\n",  opts->min_mapq );
    KOutMsg( "rna-splicing          : %s\n",  opts->rna_splicing ? "YES" : "NO" );
    KOutMsg( "rna-splice-level      : %u\n",  opts->rna_splice_level );
    KOutMsg( "rna-splice-log        : %s\n",  opts->rna_splice_log_file );

    KOutMsg( "multithreading        : %s\n",  opts->no_mt ? "NO" : "YES" );    

#if _DEBUGGING
    if ( opts->timing_file != NULL )
        KOutMsg( "timing-file           : '%s'\n",  opts->timing_file );
    else
        KOutMsg( "timing-file           : NONE\n" );
#endif

}


/* =========================================================================================== */


void release_options( samdump_opts * opts )
{
    free_ref_regions( &opts->regions );
    if ( opts->qname_prefix != NULL )
        free( (void*)opts->qname_prefix );
    if ( opts->qual_quant != NULL )
        free( (void*)opts->qual_quant );
    if( opts->outputfile != NULL )
        free( (void*)opts->outputfile );
    if( opts->header_file != NULL )
        free( (void*)opts->header_file );
    if( opts->timing_file != NULL )
        free( (void*)opts->timing_file );
    if( opts->rna_splice_log_file != NULL )
        free( (void*)opts->rna_splice_log_file );

#if _DEBUGGING
    if ( opts->perf_log != NULL )
        free_perf_log( opts->perf_log );
#endif

    if ( opts->rna_splice_log != NULL )
        free_rna_splice_log( opts->rna_splice_log );

    VNamelistRelease( opts->hdr_comments );
    VNamelistRelease( opts->input_files );
    VectorWhack ( &opts->mp_dist, release_range_wrapper, NULL );
}

/* =========================================================================================== */


rc_t gather_options( Args * args, samdump_opts * opts )
{
    rc_t rc = gather_region_options( args, opts );
    if ( rc == 0 )
        rc = gather_flag_options( args, opts );
    if ( rc == 0 )
        rc = gather_string_options( args, opts );
    if ( rc == 0 )
        rc = gather_int_options( args, opts );
    if ( rc == 0 )
        rc = gather_matepair_distances( args, opts );
    if ( rc == 0 )
        gather_unaligned_options( opts );
    return rc;
}


bool is_this_alignment_requested( const samdump_opts * opts, const char *refname, uint32_t refname_len,
                                  uint64_t start, uint64_t end )
{
    bool res = false;
    if ( opts != NULL )
    {
        reference_region *rr = find_reference_region_len( ( BSTree * )&opts->regions, refname, refname_len );
        if ( rr != NULL )
        {
            Vector *v = &(rr->ranges);
            uint32_t i, count = VectorLength( v );
            for ( i = 0; i < count && !res; ++i )
            {
                range * r = VectorGet ( v, i );
                if ( r != NULL )
                {
                    res = ( ( end >= r->start )&&( start <= r->end ) );
                }
            }
        }
    }
    return res;
}


rc_t dump_name( const samdump_opts * opts, int64_t seq_spot_id,
                const char * spot_group, uint32_t spot_group_len )
{
    rc_t rc;

    if ( opts->print_cg_names )
    {
        if ( spot_group != NULL && spot_group_len != 0 )
            rc = KOutMsg( "%.*s-1:%lu", spot_group_len, spot_group, seq_spot_id );
        else
            rc = KOutMsg( "%lu", seq_spot_id );
    }
    else
    {
        if ( opts->qname_prefix != NULL )
        {
            /* we do have to print a prefix */
            if ( opts->print_spot_group_in_name && spot_group != NULL && spot_group_len > 0 )
                rc = KOutMsg( "%s.%lu.%.*s", opts->qname_prefix, seq_spot_id, spot_group_len, spot_group );
            else
            /* we do NOT have to append the spot-group */
                rc = KOutMsg( "%s.%lu", opts->qname_prefix, seq_spot_id );
        }
        else
        {
            /* we do NOT have to print a prefix */
            if ( opts->print_spot_group_in_name && spot_group != NULL && spot_group_len > 0 )
                rc = KOutMsg( "%lu.%.*s", seq_spot_id, spot_group_len, spot_group );
            else
            /* we do NOT have to append the spot-group */
                rc = KOutMsg( "%lu", seq_spot_id );
        }
    }
    return rc;
}


rc_t dump_name_legacy( const samdump_opts * opts, const char * name, size_t name_len,
                       const char * spot_group, uint32_t spot_group_len )
{
    rc_t rc;

    if ( opts->qname_prefix != NULL )
    {
        /* we do have to print a prefix */
        if ( opts->print_spot_group_in_name && spot_group != NULL && spot_group_len > 0 )
            rc = KOutMsg( "%s.%.*s%.*s", opts->qname_prefix, name_len, name, spot_group_len, spot_group );
        else
        /* we do NOT have to append the spot-group */
            rc = KOutMsg( "%s.%.*s", opts->qname_prefix, name_len, name );
    }
    else
    {
        /* we do NOT have to print a prefix */
        if ( opts->print_spot_group_in_name && spot_group != NULL && spot_group_len > 0 )
            rc = KOutMsg( "%.*s.%.*s", name_len, name, spot_group_len, spot_group );
        else
        /* we do NOT have to append the spot-group */
            rc = KOutMsg( "%.*s", name_len, name );
    }
    return rc;
}

#define USE_KWRT_HANDLER 1

rc_t dump_quality( const samdump_opts * opts, char const *quality, uint32_t qual_len, bool reverse )
{
    uint32_t i;
    rc_t rc = 0;
    bool quantize = ( opts->qual_quant != NULL );

    size_t size = 0;
    char buffer [ 4096 ];
#if USE_KWRT_HANDLER
    size_t num_writ;
    KWrtHandler * kout_msg_handler = KOutHandlerGet ();
    assert ( kout_msg_handler != NULL );
#endif
    if ( reverse )
    {
        if ( quantize )
        {
            for ( i = qual_len; i > 0; )
            {
                uint32_t qual = quality[ -- i ];
                buffer [ size ] = ( opts->qual_quant_matrix[ qual ] + 33 );
                if ( ++ size == sizeof buffer )
                {
#if USE_KWRT_HANDLER
                    rc = ( * kout_msg_handler -> writer ) ( kout_msg_handler -> data, buffer, size, & num_writ );
#else
                    rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
#endif
                    if ( rc != 0 )
                        break;
                    size = 0;
                }
            }
        }
        else
        {
            for ( i = qual_len; i > 0; )
            {
                buffer [ size ] = quality[ -- i ] + 33;
                if ( ++ size == sizeof buffer )
                {
#if USE_KWRT_HANDLER
                    rc = ( * kout_msg_handler -> writer ) ( kout_msg_handler -> data, buffer, size, & num_writ );
#else
                    rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
#endif
                    if ( rc != 0 )
                        break;
                    size = 0;
                }
            }
        }
    }
    else
    {
        if ( quantize )
        {
            for ( i = 0; i < qual_len; ++i )
            {
                uint32_t qual = quality[ i ];
                buffer [ size ] = opts->qual_quant_matrix[ qual ] + 33;
                if ( ++ size == sizeof buffer )
                {
#if USE_KWRT_HANDLER
                    rc = ( * kout_msg_handler -> writer ) ( kout_msg_handler -> data, buffer, size, & num_writ );
#else
                    rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
#endif
                    if ( rc != 0 )
                        break;
                    size = 0;
                }
            }

        }
        else
        {
            for ( i = 0; i < qual_len && rc == 0; ++i )
            {
                buffer [ size ] = quality[ i ] + 33;
                if ( ++ size == sizeof buffer )
                {
#if USE_KWRT_HANDLER
                    rc = ( * kout_msg_handler -> writer ) ( kout_msg_handler -> data, buffer, size, & num_writ );
#else
                    rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
#endif
                    if ( rc != 0 )
                        break;
                    size = 0;
                }
            }
        }
    }

    if ( rc == 0 && size != 0 )
    {
#if USE_KWRT_HANDLER
        rc = ( * kout_msg_handler -> writer ) ( kout_msg_handler -> data, buffer, size, & num_writ );
#else
        rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
#endif
    }
    return rc;
}


rc_t dump_quality_33( const samdump_opts * opts, char const *quality, uint32_t qual_len, bool reverse )
{
    uint32_t i;
    rc_t rc = 0;
    bool quantize = ( opts->qual_quant != NULL );

    size_t size = 0;
    char buffer [ 4096 ];

    if ( reverse )
    {
        if ( quantize )
        {
            for ( i = 0; i < qual_len && rc == 0; ++i )
            {
                uint32_t qual = quality[ qual_len - i - 1 ] - 33;
                buffer [ size ] = ( opts->qual_quant_matrix[ qual ] + 33 );
                if ( ++ size == sizeof buffer )
                {
                    rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
                    if ( rc != 0 )
                        break;
                    size = 0;
                }
            }
        }
        else
        {
            for ( i = 0; i < qual_len && rc == 0; ++i )
            {
                buffer [ size ] = quality[ qual_len - i - 1 ];
                if ( ++ size == sizeof buffer )
                {
                    rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
                    if ( rc != 0 )
                        break;
                    size = 0;
                }
            }
        }
    }
    else
    {
        if ( quantize )
        {
            for ( i = 0; i < qual_len && rc == 0; ++i )
            {
                uint32_t qual = quality[ i ] - 33;
                buffer [ size ] = opts->qual_quant_matrix[ qual ] + 33;
                if ( ++ size == sizeof buffer )
                {
                    rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );
                    if ( rc != 0 )
                        break;
                    size = 0;
                }
            }

        }
        else
        {
            rc = KOutMsg( "%.*s", qual_len, quality );
        }
    }

    if ( rc == 0 && size != 0 )
        rc = KOutMsg( "%.*s", ( uint32_t ) size, buffer );

    return rc;
}
