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

#include <sysalloc.h>
#include <stdlib.h>

#include "key_value.h"
#include "mod_cmn.h"
#include "mod_reads.h"
#include "mod_reads_defs.h"
#include "mod_reads_helper.h"
#include "helper.h"
#include "chart.h"


/* this is called for every single row in a table */
static rc_t CC mod_reads_row ( p_module self,
                               const VCursor * cur )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( self != NULL && cur != NULL )
    {
        if ( self->mod_ctx != NULL )
        {
            p_reads_data data = ( p_reads_data )self->mod_ctx;
            /* in here we read the data (bases/quality and read-start/len)... */
            rc = rd_filter_read( &data->filter, cur );
            if ( rc == 0 )
            {
                uint32_t base_pos;
                uint32_t read_len = filter_get_read_len( &data->filter );
                uint16_t kmer = 0;

                /* count the number of READS/SEQUENCES */
                data->sum_reads++;

                /* count the bases total and per sequence-position */
                for ( base_pos = 0; base_pos < read_len; ++base_pos )
                {
                    char base = filter_get_base( &data->filter, base_pos );
                    uint8_t qual = filter_get_quality( &data->filter, base_pos );
                    uint8_t compressed_base_pos = compress_base_pos( base_pos );

                    /* clip the quality to fit between MIN(2) and MAX(40) */
                    if ( qual < MIN_QUALITY ) qual = MIN_QUALITY;
                    if ( qual > MAX_QUALITY ) qual = MAX_QUALITY;

                    /* count the base-postion-related base-data */
                    count_base( &data->bp_data[ compressed_base_pos ], base, qual );

                    /* count the total base-data */
                    count_base( &data->total, base, qual );

                    /* handle the kmer's */
                    kmer = count_kmer( data, kmer, compressed_base_pos, base );

                    /* detect min. and max. quality */
                    if ( qual < data->min_quality )
                        data->min_quality = qual;
                    if ( qual > data->max_quality )
                        data->max_quality = qual;
                }

                /* detect min. and max. read-len */
                if ( read_len < data->min_read_len )
                    data->min_read_len = read_len;
                if ( read_len > data->max_read_len )
                    data->max_read_len = read_len;

                /* count how often a particular read-len occurs */
                data->bp_data[ compress_base_pos( read_len ) ].read_count++;
            }
        }
    }
    return rc;
}


/* this is called once, after all rows are handled */
static rc_t CC mod_reads_post_rows ( p_module self )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( self != NULL && self->mod_ctx != NULL )
    {
        p_reads_data data = self->mod_ctx; 
        p_base_pos bp;
        uint32_t base_pos;

        /* first we calculate for every base-position:
           for quality: mean/median/quarat/centile
           for bases  : percentage */
        for ( base_pos = 0; base_pos < MAX_COMPRESSED_BASE_POS; ++base_pos )
        {
            bp = &data->bp_data[ base_pos ];
            calculate_quality_mean_median_quart_centile( bp );
            calculate_bases_percentage( bp );
        }

        /* then we calculate for the sum:
           for quality: mean/median/quarat/centile
           for bases  : percentage and probability 
           for kmers  : observed vs. expected occurance */
        bp = &data->total;
        calculate_quality_mean_median_quart_centile( bp );
        calculate_bases_percentage( bp );
        calculate_base_probability( data );
        calculate_kmer_observed_vs_expected( data );

        data->mean_read_len = bp->count;
        data->mean_read_len /= data->sum_reads;

        rc = 0;
    }
    return rc;
}


static rc_t print_totals( p_reads_data data, p_report report )
{
    uint8_t i;
    p_base_pos bp = &( data->total );

    rc_t rc = report_set_columns( report, 21, 
        "bases", "reads", "mean_readlen", "min_readlen", "max_readlen", 
        "GC", "GC_percent", "A", "A_percent", "C", "C_percent", 
        "G", "G_percent", "T", "T_percent", "N", "N_percent",
        "mean_quality", "median_quality", "min_quality", "max_quality" );

    if ( rc == 0 )
        rc = report_new_data( report );

    if ( rc == 0 )
        rc = report_add_data( report, 20, "%lu", bp->count );
    if ( rc == 0 )
        rc = report_add_data( report, 20, "%lu", data->sum_reads );
    if ( rc == 0 )
        rc = report_add_data( report, 20, "%.2f", data->mean_read_len );
    if ( rc == 0 )
        rc = report_add_data( report, 20, "%lu", data->min_read_len );
    if ( rc == 0 )
        rc = report_add_data( report, 20, "%lu", data->max_read_len );

    if ( rc == 0 )
        rc = report_add_data( report, 40, "%lu", 
                    bp->base_sum[ IDX_G ] + bp->base_sum[ IDX_C ] );
    if ( rc == 0 )
        rc = report_add_data( report, 40, "%.2f", 
                    bp->base_percent[ IDX_G ] + bp->base_percent[ IDX_C ] );

    for ( i = IDX_A; i <= IDX_N && rc == 0 ; ++i )
    {
        rc = report_add_data( report, 20, "%lu", bp->base_sum[ i ] );
        if ( rc == 0 )
            rc = report_add_data( report, 20, "%.2f", bp->base_percent[ i ] );
    }

    if ( rc == 0 )
        rc = report_add_data( report, 20, "%.2f", bp->mean );
    if ( rc == 0 )
        rc = report_add_data( report, 20, "%.2f", bp->median );
    if ( rc == 0 )
        rc = report_add_data( report, 20, "%u", data->min_quality );
    if ( rc == 0 )
        rc = report_add_data( report, 20, "%u", data->max_quality );

    return rc;
}


static rc_t print_pos_bases( p_reads_data data, p_report report )
{
    rc_t rc = report_set_columns( report, 8, "from", "to", "A", "C", "G", "T", "N", "reads" );
    if ( rc == 0 )
    {
        uint8_t i, max_i = compress_base_pos( data->max_read_len );
        for ( i = 0; i <= max_i && rc == 0 ; ++i )
        {
            uint8_t j;
            p_base_pos bp = &data->bp_data[ i ];
            uint32_t to = bp->to;
            if ( to > MAX_BASE_POS ) to = MAX_BASE_POS;

            if ( rc == 0 )
                rc = report_new_data( report );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", bp->from );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", to );

            for ( j = IDX_A; j <= IDX_N && rc == 0; ++j )
                rc = report_add_data( report, 20, "%.1f", bp->base_percent[ j ] );

            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", bp->read_count );
        }
    }
    return rc;
}


static void prepare_hruler( uint32_t * pp, uint32_t n, uint32_t spread )
{
    uint8_t idx;
    for ( idx = 0; idx < n; ++idx )
        pp[ idx ] = compress_start_lookup( idx * spread );
}


static void prepare_pos_bases_line( p_reads_data data, uint32_t * pp, 
                                    uint32_t select )
{
    uint8_t i;
    for ( i = 0; i <= MAX_COMPRESSED_BASE_POS; ++i )
    {
        p_base_pos bp = &data->bp_data[ i ];
        double value;
        if ( select <= IDX_N )
            value = bp->base_percent[ select ];
        else
            value = bp->base_percent[ IDX_C ] + bp->base_percent[ IDX_G ];
        if ( value > 60 )
            value = 0;
        pp[ i ] = ( value * 10 );
    }
}


static rc_t graph_pos_bases( p_reads_data data, const char * filename )
{
    chart c;
    rc_t rc = chart_init( &c, 100, 100, 600, 500 );
    if ( rc == 0 )
    {
        double factor = 0.7;
        uint32_t pp[ MAX_COMPRESSED_BASE_POS + 1 ];
        uint8_t max_i = compress_base_pos( data->max_read_len );
        uint32_t h_spread = 600 / max_i;

        chart_captions( &c, "bases per position", "base-pos", "%" );
        chart_vruler( &c, 0, 60, factor * 10, 10, "%" );
        prepare_hruler( pp, max_i / 2, 2 );
        chart_hruler( &c, pp, max_i / 2, 0, h_spread * 2 );

        prepare_pos_bases_line( data, pp, IDX_A );
        chart_line( &c, pp, max_i, factor, h_spread, "red", "A" );

        prepare_pos_bases_line( data, pp, IDX_C );
        chart_line( &c, pp, max_i, factor, h_spread, "green", "C" );

        prepare_pos_bases_line( data, pp, IDX_G );
        chart_line( &c, pp, max_i, factor, h_spread, "blue", "G" );

        prepare_pos_bases_line( data, pp, IDX_T );
        chart_line( &c, pp, max_i, factor, h_spread, "yellow", "T" );

        prepare_pos_bases_line( data, pp, IDX_N );
        chart_line( &c, pp, max_i, factor, h_spread, "orange", "N" );

        prepare_pos_bases_line( data, pp, 5 );
        chart_line( &c, pp, max_i, factor, h_spread, "pink", "GC" );

        rc = chart_write( &c, filename );
        chart_destroy( &c );
    }
    return rc;
}


static rc_t graph_pos_quality( p_reads_data data, const char * filename )
{
    chart c;
    rc_t rc = chart_init( &c, 100, 100, 600, 500 );
    if ( rc == 0 )
    {
        char * box_style = create_fill_style( &c, "black", "yellow", 1 );
        if ( box_style != NULL )
        {
            char * median_style = create_line_style( &c, "red", 4 );
            if ( median_style != NULL )
            {
                uint32_t i;
                uint32_t pp[ MAX_COMPRESSED_BASE_POS + 1 ];
                uint8_t max_i = compress_base_pos( data->max_read_len );
                uint32_t h_spread = ( 600 - 10 ) / max_i;

                chart_captions( &c, "quality per position", "base-pos", "phred" );
                chart_vruler( &c, 0, MAX_QUALITY, 10.0, 5, NULL );
                prepare_hruler( pp, ( max_i / 2 ) + 1, 2 );
                chart_hruler( &c, pp, ( max_i / 2 ) + 1, 
                              5 + ( h_spread / 2 ), h_spread * 2 );
                for ( i = 0; i <= max_i; ++i )
                {
                    p_base_pos bp = &data->bp_data[ i ];
                    chart_box_whisker( &c,
                               5 + ( i * h_spread ) + 2, /* x */
                               h_spread - 2,         /* dx */
                               bp->lower_centile,    /* p_low */
                               bp->upper_centile,    /* p_high */
                               bp->lower_quart,      /* q_low */
                               bp->upper_quart,      /* q_high */
                               bp->median,           /* median */
                               10.0,                 /* factor */
                               &( box_style[1] ),
                               &( median_style[1] ) );
                }
                rc = chart_write( &c, filename );
                free( median_style );
            }
            free( box_style );
        }
        chart_destroy( &c );
    }
    return rc;
}


static rc_t print_pos_quality( p_reads_data data, p_report report )
{
    rc_t rc = report_set_columns( report, 10, "from", "to", "mean", "median",
                "mode", "q-up", "q-low", "c-up", "c-low", "reads" );
    if ( rc == 0 )
    {
        uint8_t i, max_i = compress_base_pos( data->max_read_len );
        for ( i = 0; i <= max_i && rc == 0 ; ++i )
        {
            p_base_pos bp = &data->bp_data[ i ];
            uint32_t to = bp->to;
            if ( to > MAX_BASE_POS ) to = MAX_BASE_POS;

            if ( rc == 0 )
                rc = report_new_data( report );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", bp->from );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", to );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.2f", bp->mean );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.2f", bp->median );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.2f", bp->mode );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.2f", bp->upper_quart );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.2f", bp->lower_quart );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.2f", bp->upper_centile );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.2f", bp->lower_centile );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", bp->count );
        }
    }
    return rc;
}


static uint64_t max_count_of_read_length( p_reads_data data )
{
    uint32_t res = 0;
    uint8_t i;
    for ( i = 0; i <= MAX_COMPRESSED_BASE_POS; ++i )
    {
        p_base_pos bp = &data->bp_data[ i ];
        if ( bp->read_count > res )
            res = bp->read_count;
    }
    return res;
}


static rc_t graph_read_length( p_reads_data data, const char * filename )
{
    chart c;
    rc_t rc = chart_init( &c, 100, 100, 600, 500 );
    if ( rc == 0 )
    {
        double factor;
        uint8_t i;
        uint32_t pp[ MAX_COMPRESSED_BASE_POS + 1 ];
        uint32_t h_spread = 12;
        uint64_t max_value = max_count_of_read_length( data );

        chart_captions( &c, "distribution of read-length", "read-length", "count" );
        factor = max_value;
        factor /= 450;

        /*               max_y, step_y, max_value */
        chart_vruler1( &c, 450, 50, max_value, NULL );
        prepare_hruler( pp, ( MAX_COMPRESSED_BASE_POS + 1 ) / 2, 2 );
        chart_hruler( &c, pp, ( MAX_COMPRESSED_BASE_POS + 1 ) / 2,
                      0, h_spread * 2 );

        /* prepare the one and only line to draw */
        for ( i = 0; i <= MAX_COMPRESSED_BASE_POS; ++i )
        {
            p_base_pos bp = &data->bp_data[ i ];
            uint64_t value = bp->read_count;
            value *= 450;
            value /= max_value;
            pp[ i ] = (uint32_t) value;
        }
        chart_line( &c, pp, MAX_COMPRESSED_BASE_POS + 1, 1.0, 
                    h_spread, "blue", "read-length" );

        rc = chart_write( &c, filename );
        chart_destroy( &c );
    }
    return rc;
}


static rc_t print_read_length( p_reads_data data, p_report report )
{
    uint8_t i;
    rc_t rc = report_set_columns( report, 3, "from", "to", "reads" );
    if ( rc == 0 )
    {
        for ( i = 0; i <= MAX_COMPRESSED_BASE_POS && rc == 0 ; ++i )
        {
            p_base_pos bp = &data->bp_data[ i ];
            uint32_t to = bp->to;
            if ( to > MAX_BASE_POS ) to = MAX_BASE_POS;
            if ( rc == 0 )
                rc = report_new_data( report );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", bp->from );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", to );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", bp->read_count );
        }
    }
    return rc;
}


static uint64_t max_count_of_quality( p_reads_data data )
{
    uint32_t res = 0;
    uint8_t i;
    for ( i = 0; i <= 40; ++i )
    {
        if ( data->total.qual[ i ] > res )
            res = data->total.qual[ i ];
    }
    return res;
}


static rc_t graph_qual_distr( p_reads_data data, const char * filename )
{
    chart c;
    rc_t rc = chart_init( &c, 100, 100, 600, 500 );
    if ( rc == 0 )
    {
        double factor;
        uint8_t i;
        uint32_t pp[ 42 ];
        uint32_t h_spread = 12;
        uint32_t max_value = max_count_of_quality( data );

        chart_captions( &c, "distribution of quality", "quality", "count" );
        factor = max_value;
        factor /= 450;

        /*               max_y, step_y, max_value */
        chart_vruler1( &c, 450, 50, max_value, NULL );
        for ( i = 0; i <= 21; ++i )
            pp[ i ] = i * 2;
        chart_hruler( &c, pp, 21, 0, h_spread * 2 );

        /* prepare the one and only line to draw */
        for ( i = 0; i <= 41; ++i )
        {
            uint64_t value = data->total.qual[ i ];
            value *= 450;
            value /= max_value;
            pp[ i ] = (uint32_t) value;
        }
        chart_line( &c, pp, 41, 1.0, h_spread, "blue", "count of quality" );

        rc = chart_write( &c, filename );
        chart_destroy( &c );
    }
    return rc;
}


static rc_t print_qual_distr( p_reads_data data, p_report report )
{
    uint8_t i;
    rc_t rc = report_set_columns( report, 2, "quality_value", "count" );
    if ( rc == 0 )
    {
        for ( i = 0; i < 41 && rc == 0 ; ++i )
        {
            rc = report_new_data( report );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", i );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%u", data->total.qual[ i ] );
        }
    }
    return rc;
}


static rc_t print_kmers( p_reads_data data, p_report report )
{
    uint16_t i;
    rc_t rc = report_set_columns( report, 7, 
        "sequence", "count", "probability", "expected", 
        "observed-vs-expected",  "max-obs-vs-exp", "at" );
    if ( rc == 0 )
    {
        for ( i = 0; i < NKMER5 && rc == 0 ; ++i )
        {
            uint16_t idx = data->kmer5_idx[ i ];
            p_kmer5_count k = &( data->kmer5[ idx ] );

            rc = report_new_data( report );
            if ( rc == 0 )
            {
                char s[ 6 ];
                kmer_int_2_ascii( idx, s );
                rc = report_add_data( report, 10, "%s", s );
            }
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%lu", k->total_count );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%f", k->probability );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%.0f", k->expected );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%f", k->observed_vs_expected );
            if ( rc == 0 )
                rc = report_add_data( report, 20, "%f", k->max_bp_obs_vs_exp );
            if ( rc == 0 )
            {
                uint32_t from, to;
                from = compress_start_lookup( k->max_bp_obs_vs_exp_at );
                to   = compress_end_lookup( k->max_bp_obs_vs_exp_at );
                if ( from == to )
                    rc = report_add_data( report, 20, "%d", from );
                else
                    rc = report_add_data( report, 20, "%d .. %d", from, to );
            }
        }
    }
    return rc;
}


static rc_t CC mod_reads_free ( p_module self )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( self != NULL )
    {
        if ( self->mod_ctx != NULL )
        {
            p_reads_data data = self->mod_ctx; 
            rd_filter_destroy( &data->filter );
            free( self->mod_ctx );
        }
        rc = 0;
    }
    return rc;
}


static rc_t analyze_param( p_reads_data data, const char * param )
{
    const KNamelist *plist;
    rc_t rc = helper_make_namelist_from_string( &plist, param, '/' );
    if ( rc ==  0 )
    {
        uint32_t count;
        rc = KNamelistCount( plist, &count );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < count && rc == 0; ++i )
            {
                const char * s;
                rc = KNamelistGet( plist, i, &s );
                if ( rc == 0 )
                {
                    if ( helper_str_cmp( s, "bio" ) == 0 )
                        data->bio = true;
                    else if ( helper_str_cmp( s, "trim" ) == 0 )
                        data->trim = true;
                    else if ( helper_str_cmp( s, "cut" ) == 0 )
                        data->cut = true;
                }
            }
        }
        KNamelistRelease( plist );
    }
    return rc;
}


static rc_t CC mod_reads_count( p_module self, uint32_t * count )
{
    *count = 6;
    return 0;
}


static rc_t CC mod_reads_name( p_module self, uint32_t n, char ** name )
{
    switch ( n )
    {
        case 0 : *name = string_dup_measure ( "global", NULL ); break;
        case 1 : *name = string_dup_measure ( "bases", NULL ); break;
        case 2 : *name = string_dup_measure ( "quality", NULL ); break;
        case 3 : *name = string_dup_measure ( "readlength", NULL ); break;
        case 4 : *name = string_dup_measure ( "qual_dist", NULL ); break;
        case 5 : *name = string_dup_measure ( "k_mer", NULL ); break;
        default : *name = string_dup_measure ( "unknown", NULL ); break;
    }
    return 0;
}


static rc_t CC mod_reads_report( p_module self, 
                                 p_report report,
                                 const uint32_t sub_select )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcInvalid );
    p_reads_data data = (p_reads_data)self->mod_ctx;
    if ( data != NULL && report != NULL )
    {
        switch( sub_select )
        {
            case 0 : rc = print_totals( data, report ); break;
            case 1 : rc = print_pos_bases( data, report ); break;
            case 2 : rc = print_pos_quality( data, report ); break;
            case 3 : rc = print_read_length( data, report ); break;
            case 4 : rc = print_qual_distr( data, report ); break;
            case 5 : rc = print_kmers( data, report ); break;
        }
    }
    return rc;
}


static rc_t CC mod_reads_graph( p_module self, uint32_t n, const char * filename )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcUnsupported );
    p_reads_data data = (p_reads_data)self->mod_ctx;
    switch( n )
    {
        case 1 : rc = graph_pos_bases( data, filename ); break;
        case 2 : rc = graph_pos_quality( data, filename ); break;
        case 3 : rc = graph_read_length( data, filename ); break;
        case 4 : rc = graph_qual_distr( data, filename ); break;
    }
    return rc;
}


static rc_t CC mod_reads_pre_open( p_module self, const VCursor * cur )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && cur != NULL )
    {
        p_reads_data data = self->mod_ctx;
        rc = rd_filter_pre_open( &data->filter, cur );
    }
    return rc;
}


rc_t mod_reads_init( p_module self, const char * param )
{
    rc_t rc = 0;

    /* put in it's name */
    self->name = string_dup_measure ( "bases", NULL );

    /* connect the callback-functions */
    self->f_pre_open = mod_reads_pre_open;
    self->f_row    = mod_reads_row;
    self->f_post_rows = mod_reads_post_rows;
    self->f_count  = mod_reads_count;
    self->f_name   = mod_reads_name;
    self->f_report = mod_reads_report;
    self->f_graph  = mod_reads_graph;
    self->f_free   = mod_reads_free;

    /* initialize the module context */
    self->mod_ctx  = calloc( 1, sizeof( struct reads_data ) );
    if ( self->mod_ctx == NULL )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    else
    {
        p_reads_data data = self->mod_ctx;
        /* this is done in 2 steps to work on 32-bit-compilers too
           ( no 64-bit literals in 32-bit-compilers ) */
        data->min_read_len = 0xFFFFFFFF;
        data->min_read_len <<= 32;
        data->min_read_len |= 0xFFFFFFFF;
        data->min_quality = 0xFF;
        setup_bp_array( data->bp_data, MAX_COMPRESSED_BASE_POS );

        rc = rd_filter_init( &data->filter );
        if ( rc == 0 && param != NULL )
            rc = analyze_param( data, param );
        if ( rc == 0 )
            rc = rd_filter_set_flags( &data->filter, data->bio, data->trim, data->cut );
        if ( rc == 0 )
        {
            if ( param != NULL )
                OUTMSG(( "module 'bases' initialized (%s)\n", param ));
            else
                OUTMSG(( "module 'bases' initialized\n" ));
        }
    }
    return rc;
}
