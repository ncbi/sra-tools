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

#include <klib/out.h>
#include <kapp/args.h>

#include "pileup_options.h"
#include "dyn_string.h"
#include "ref_walker.h"
#include "4na_ascii.h"

/*
static rc_t CC pileup_test_enter_ref( ref_walker_data * rwd )
{
    return KOutMsg( "\nentering >%s<\n", rwd->ref_name );
}

static rc_t CC pileup_test_exit_ref( ref_walker_data * rwd )
{
    return KOutMsg( "exit >%s<\n", rwd->ref_name );
}

static rc_t CC pileup_test_enter_ref_window( ref_walker_data * rwd )
{
    return KOutMsg( "   enter window >%s< [ %,lu ... %,lu ]\n", rwd->ref_name, rwd->ref_start, rwd->ref_end );
}

static rc_t CC pileup_test_exit_ref_window( ref_walker_data * rwd )
{
    return KOutMsg( "   exit window >%s< [ %,lu ... %,lu ]\n", rwd->ref_name, rwd->ref_start, rwd->ref_end );
}

static rc_t CC pileup_test_enter_ref_pos( ref_walker_data * rwd )
{
    return KOutMsg( "   enter pos [ %,lu ], d=%u\n", rwd->pos, rwd->depth );
}

static rc_t CC pileup_test_exit_ref_pos( ref_walker_data * rwd )
{
    return KOutMsg( "   exit pos [ %,lu ], d=%u\n", rwd->pos, rwd->depth );
}

static rc_t CC pileup_test_enter_spot_group( ref_walker_data * rwd )
{
    return KOutMsg( "       enter spot-group [ %,lu ], %.*s\n", rwd->pos, rwd->spot_group_len, rwd->spot_group );
}

static rc_t CC pileup_test_exit_spot_group( ref_walker_data * rwd )
{
    return KOutMsg( "       exit spot-group [ %,lu ], %.*s\n", rwd->pos, rwd->spot_group_len, rwd->spot_group );
}

static rc_t CC pileup_test_alignment( ref_walker_data * rwd )
{
    rc_t rc = KOutMsg( "          alignment\t" );
    if ( rc == 0 )
    {
        if ( !rwd->valid )
        {
            KOutMsg( "invalid" );
        }
        else
        {
            KOutMsg( "%s%s", rwd->reverse ? "<" : ">", rwd->match ? "." : "!" );
        }
    }
    KOutMsg( "\n" );
    return rc;
}

static rc_t pileup_test( Args * args, pileup_options *options )
{
    struct ref_walker * walker;
    rc_t rc = ref_walker_create( &walker );
    if ( rc == 0 )
    {
        uint32_t idx, count;
        rc = ArgsParamCount( args, &count );
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char * src = NULL;
            rc = ArgsParamValue( args, idx, &src );
            if ( rc == 0 && src != NULL )
                rc = ref_walker_add_source( walker, src );
        }

        if ( rc == 0 )
        {
            rc = ArgsOptionCount( args, OPTION_REF, &count );
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char * s = NULL;
                rc = ArgsOptionValue( args, OPTION_REF, idx, &s );
                if ( rc == 0 && s != NULL )
                    rc = ref_walker_parse_and_add_range( walker, s );
            }
        }

        if ( rc == 0 )
        {
            ref_walker_callbacks callbacks = 
                {   pileup_test_enter_ref,
                    pileup_test_exit_ref,
                    pileup_test_enter_ref_window,
                    pileup_test_exit_ref_window,
                    pileup_test_enter_ref_pos,
                    pileup_test_exit_ref_pos,
                    pileup_test_enter_spot_group,
                    pileup_test_exit_spot_group,
                    pileup_test_alignment };
            rc = ref_walker_set_callbacks( walker, &callbacks );
        }

        if ( rc == 0 )
            rc = ref_walker_walk( walker, NULL );

        ref_walker_destroy( walker );
    }
    return rc;
}
*/


/* =========================================================================================== */


typedef struct pileup_v2_ctx
{
    struct dyn_string * bases;
    struct dyn_string * qual;
    bool print_qual;
    bool div_by_spotgrp;
    bool debug;
} pileup_v2_ctx;


static rc_t CC pileup_v2_enter_ref_pos( ref_walker_data * rwd )
{
    pileup_v2_ctx * ctx = rwd->data;
    /* make shure that bases/qual have the necessary length ( depth * 2 ) */
    uint32_t l = ( rwd->depth * 2 );
    rc_t rc = expand_dyn_string( ctx->bases, l );
    if ( rc == 0 )
        reset_dyn_string( ctx->bases );
    if ( rc == 0 && ctx->print_qual )
    {
        rc = expand_dyn_string( ctx->qual, l );
        if ( rc == 0 )
            reset_dyn_string( ctx->qual );
    }
    return rc;
}


static rc_t CC pileup_v2_exit_ref_pos( ref_walker_data * rwd )
{
    pileup_v2_ctx * ctx = rwd->data;
    rc_t rc = KOutMsg( "%s\t%u\t%c\t%u\t", rwd->ref_name, rwd->pos + 1, rwd->ascii_ref_base, rwd->depth );
    if ( rc == 0 )
        rc = print_dyn_string( ctx->bases );
    if ( rc == 0 && ctx->print_qual )
    {
        rc = KOutMsg( "\t" );
        if ( rc == 0 )
            rc = print_dyn_string( ctx->qual );
    }
    if ( rc == 0 )
        rc = KOutMsg( "\n" );
    return rc;
}


static rc_t CC pileup_v2_enter_spot_group( ref_walker_data * rwd )
{
    rc_t rc = 0;
    pileup_v2_ctx * ctx = rwd->data;
    if ( ctx->div_by_spotgrp )
    {
        if ( dyn_string_len( ctx->bases ) > 0 )
            rc = add_char_2_dyn_string( ctx->bases, '\t' );
        if ( rc == 0 && ctx->print_qual && dyn_string_len( ctx->qual ) > 0 )
            rc = add_char_2_dyn_string( ctx->qual, '\t' );
    }
    return rc;
}


static rc_t CC pileup_v2_alignment( ref_walker_data * rwd )
{
    rc_t rc = 0;
    pileup_v2_ctx * ctx = rwd->data;

    if ( !rwd->valid )
    {
        rc = add_char_2_dyn_string( ctx->bases, '?' );
        if ( rc == 0 && ctx->print_qual )
            rc = add_char_2_dyn_string( ctx->qual, '?' );
    }
    else
    {
        if ( rwd->first )
        {
            char s[ 3 ];
            int32_t c = rwd->mapq + 33;
            if ( c > '~' ) { c = '~'; }
            if ( c < 33 ) { c = 33; }
            s[ 0 ] = '^';
            s[ 1 ] = c;
            s[ 2 ] = 0;
            rc = add_string_2_dyn_string( ctx->bases, s );
        }


        if ( rc == 0 )
        {
            if ( rwd->skip )
            {
                if ( rwd->reverse )
                    rc = add_char_2_dyn_string( ctx->bases, '<' );
                else
                    rc = add_char_2_dyn_string( ctx->bases, '>' );
            }
            else
            {
                if ( rwd->match )
                    rc = add_char_2_dyn_string( ctx->bases, ( rwd->reverse ? ',' : '.' ) );
                else
                    rc = add_char_2_dyn_string( ctx->bases, rwd->ascii_alignment_base );
            }
        }

        if ( rc == 0 && rwd->ins )
        {
            uint32_t i, n = rwd->ins_bases_count;
            
            rc = print_2_dyn_string( ctx->bases, "+%u", rwd->ins_bases_count );
            for ( i = 0; i < n && rc == 0; ++i )
                rc = add_char_2_dyn_string( ctx->bases, _4na_to_ascii( rwd->ins_bases[ i ], rwd->reverse ) );
        }

        if ( rc == 0 && rwd->del && rwd->del_bases_count > 0 && rwd->del_bases != NULL )
        {
            uint32_t i, n = rwd->del_bases_count;
            rc = print_2_dyn_string( ctx->bases, "-%u", n );
            for ( i = 0; i < n && rc == 0; ++i )
                rc = add_char_2_dyn_string( ctx->bases, _4na_to_ascii( rwd->del_bases[ i ], rwd->reverse ) );
        }

        if ( rc == 0 && rwd->last )
            rc = add_char_2_dyn_string( ctx->bases, '$' );

        if ( rc == 0 && ctx->print_qual )
        {
            rc = add_char_2_dyn_string( ctx->qual, rwd->quality );
        }
    }

    return rc;
}


rc_t pileup_v2( Args * args, pileup_options *options )
{
    struct ref_walker * walker;

    /* create walker */
    rc_t rc = ref_walker_create( &walker );
    if ( rc == 0 )
    {
        uint32_t idx, count;

        /* add sources to walker */
        rc = ArgsParamCount( args, &count );
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char * src = NULL;
            rc = ArgsParamValue( args, idx, &src );
            if ( rc == 0 && src != NULL )
                rc = ref_walker_add_source( walker, src );
        }

        /* add ranges to walker */
        if ( rc == 0 )
        {
            rc = ArgsOptionCount( args, OPTION_REF, &count );
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char * s = NULL;
                rc = ArgsOptionValue( args, OPTION_REF, idx, &s );
                if ( rc == 0 && s != NULL )
                    rc = ref_walker_parse_and_add_range( walker, s );
            }
        }

        /* set callbacks for walker */
        if ( rc == 0 )
        {
            ref_walker_callbacks callbacks = 
                {   NULL,
                    NULL,
                    NULL,
                    NULL,
                    pileup_v2_enter_ref_pos,
                    pileup_v2_exit_ref_pos,
                    pileup_v2_enter_spot_group,
                    NULL,
                    pileup_v2_alignment };
            rc = ref_walker_set_callbacks( walker, &callbacks );
        }

        /* translate the commandline options into walker 'INTERESTS' */
        if ( rc == 0 )
        {
            uint32_t interest = RW_INTEREST_INDEL | RW_INTEREST_BASE;

            if ( options->process_dups ) interest |= RW_INTEREST_DUPS;
            if ( !options->omit_qualities ) interest |= RW_INTEREST_QUAL;
            if ( !options->no_skip ) interest |= RW_INTEREST_SKIP;
            if ( options->show_id ) interest |= RW_INTEREST_DEBUG;
            if ( options->use_seq_name ) interest |= RW_INTEREST_SEQNAME;
            if ( options->cmn.tab_select & primary_ats ) interest |= RW_INTEREST_PRIM;
            if ( options->cmn.tab_select & secondary_ats ) interest |= RW_INTEREST_SEC;
            if ( options->cmn.tab_select & evidence_ats ) interest |= RW_INTEREST_EV;

            rc = ref_walker_set_interest( walker, interest );
            if ( rc == 0 )
                rc = ref_walker_set_min_mapq( walker, options->minmapq );
        }

        /* let the walker call the callbacks while iterating over the sources/ranges */
        if ( rc == 0 )
        {
            pileup_v2_ctx ctx;
            memset( &ctx, 0, sizeof ctx );
            rc = allocated_dyn_string ( &ctx.bases, 1000 );
            if ( rc == 0 )
            {
                rc = allocated_dyn_string ( &ctx.qual, 1000 );
                if ( rc == 0 )
                {
                    ctx.print_qual = !options->omit_qualities;
                    ctx.div_by_spotgrp = options->div_by_spotgrp;
                    ctx.debug = options->show_id;

                    /***********************************/
                    rc = ref_walker_walk( walker, &ctx );
                    /***********************************/

                    free_dyn_string ( ctx.qual );
                }
                free_dyn_string ( ctx.bases );
            }
        }

        /* destroy the walker */
        ref_walker_destroy( walker );
    }
    return rc;
}
