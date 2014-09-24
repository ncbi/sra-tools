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

#include "stat_mod.h"
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const char * ridx_names[ N_RIDX ] =
{
    "READ",
    "QUALITY",
    "HAS_MISMATCH",
    "SPOT_GROUP",
    "SEQ_SPOT_GROUP",
    "REF_ORIENTATION",
    "READ_LEN",
    "SEQ_READ_ID",
    "HAS_REF_OFFSET",
    "REF_OFFSET",
    "REF_POS",
    "REF_SEQ_ID",
    "REF_LEN"
};


static const char * widx_names[ N_WIDX ] =
{
    "SPOT_GROUP",
    "KMER",
    "ORIG_QUAL",
    "TOTAL_COUNT",
    "MISMATCH_COUNT",
    "CYCLE",
    "HPRUN",
    "GC_CONTENT"
};

typedef struct qual
{
    uint32_t mismatches;
    uint32_t count;
} qual;


typedef struct pos_vector
{
    uint32_t len;
    struct qual *v;
} pos_vector;


typedef struct spotgrp
{
    BSTNode node;
    const String *name;
    pos_vector dimers[N_DIMER_VALUES][N_QUAL_VALUES][N_GC_VALUES][N_HP_VALUES];
} spotgrp;


static const uint8_t char_2_base_bin[26] =
{
   /* A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W  X  Y  Z*/
      0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0
};


static const char * dimer_2_ascii[] = 
{ "AA", "AC", "AG", "AT", 
  "CA", "CC", "CG", "CT",
  "GA", "GC", "GG", "GT",
  "TA", "TC", "TG", "TT", "XX" };


static void CC whack_spotgroup( BSTNode *n, void *data )
{
    spotgrp * sg = ( spotgrp * )n;
    uint8_t dimer_idx;
    for ( dimer_idx = 0; dimer_idx < N_DIMER_VALUES; ++dimer_idx )
    {
        uint8_t qual_idx;
        for ( qual_idx = 0; qual_idx < N_QUAL_VALUES; ++qual_idx )
        {
            uint8_t gc_idx;
            for ( gc_idx = 0; gc_idx < N_GC_VALUES; ++gc_idx )
            {
                uint8_t hp_idx;
                for ( hp_idx = 0; hp_idx < N_HP_VALUES; ++hp_idx )
                {
                    pos_vector *pv = &sg->dimers[dimer_idx][qual_idx][gc_idx][hp_idx];
                    if ( pv->v != NULL )
                        free( pv->v );
                }
            }
        }
    }
    if ( sg->name != NULL )
        StringWhack ( sg->name );
    free( n );
}


rc_t make_statistic( statistic *data, uint32_t gc_window,
                     KDirectory *dir, const char * exclude_db )
{
    rc_t rc = 0;
    BSTreeInit( &data->spotgroups );
    data->last_used_spotgroup = NULL;
    memset( &data->rd_col, 0, sizeof data->rd_col );
    make_ref_exclude( &data->exclude, dir, exclude_db );
    data->gc_window = gc_window;

    data->exclude_vector_len = 512;
    data->exclude_vector = calloc( 1, data->exclude_vector_len );
    if ( data->exclude_vector == NULL )
    {
        data->exclude_vector_len = 0;
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        LogErr( klogInt, rc, "failed to make large enough exclude-vector\n" );
    }

    if ( rc == 0 )
    {
        data->case_vector_len = 512;
        data->case_vector = calloc( 1, data->case_vector_len );
        if ( data->case_vector == NULL )
        {
            data->case_vector_len = 0;
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            LogErr( klogInt, rc, "failed to make large enough case-vector\n" );
        }
    }

    return rc;
}


void whack_statistic( statistic *data )
{
    BSTreeWhack ( &data->spotgroups, whack_spotgroup, NULL );
    whack_ref_exclude( &data->exclude );
    if ( data->exclude_vector != NULL )
    {
        free( data->exclude_vector );
        data->exclude_vector = NULL;
        data->exclude_vector_len = 0;
    }
    if ( data->case_vector != NULL )
    {
        free( data->case_vector );
        data->case_vector = NULL;
        data->case_vector_len = 0;
    }
}


static spotgrp * make_spotgrp( const char *src, const size_t len )
{
    spotgrp * res = calloc( 1, sizeof * res );
    if ( res != NULL )
    {
        String s;
        StringInit( &s, src, len, len );
        if ( StringCopy ( &res->name, &s ) != 0 )
        {
            free( res );
            res = NULL;
        }
    }
    return res;
}


static int CC spotgroup_find( const void *item, const BSTNode *n )
{
    spotgrp * sg = ( spotgrp* ) n;
    return StringCompare ( ( String* ) item, sg->name );
}


static spotgrp * find_spotgrp( statistic *data, const char *src, const size_t len )
{
    String s;
    BSTNode *node;

    StringInit( &s, src, len, len );
    if ( data->last_used_spotgroup != NULL )
    {
        spotgrp * sg = ( spotgrp* )data->last_used_spotgroup;
        if ( StringCompare ( &s, sg->name ) == 0 )
            return sg;
    }

    node = BSTreeFind ( &data->spotgroups, &s, spotgroup_find );
    if ( node == NULL )
        return NULL;
    else
    {
        data->last_used_spotgroup = node;
        return ( spotgrp *) node;
    }
}


static uint8_t dimer_2_bin( const char * dimer )
{
    uint8_t res = 0;
    char c0 = dimer[ 0 ];
    char c1 = dimer[ 1 ];
    if ( c0 == 'N' || c1 == 'N' )
        res = 16;
    else
    {
        if ( c0 >= 'A' && c0 <= 'Z' )
            res |= char_2_base_bin[ (uint8_t)( c0 - 'A' ) ];
        res <<= 2;
        if ( c1 >= 'A' && c1 <= 'Z' )
            res |= char_2_base_bin[ (uint8_t)( c1 - 'A' ) ];
    }
    return res;
}


static rc_t spotgroup_enter_values( spotgrp * sg, 
                                    stat_row * row, 
                                    uint8_t dimer_idx,
                                    uint8_t rd_case,
                                    uint32_t base_pos_offset )
{
    rc_t rc = 0;
    pos_vector *pv;
    uint32_t base_pos = row->base_pos + base_pos_offset;

    if ( row->quality >= N_QUAL_VALUES )
        row->quality = N_QUAL_VALUES - 1;
    if ( row->gc_content >= N_GC_VALUES )
        row->gc_content = N_GC_VALUES - 1;
    if ( row->hp_run >= N_HP_VALUES )
        row->hp_run = N_HP_VALUES - 1;

    pv = &sg->dimers[ dimer_idx ][ row->quality ][ row->gc_content ][ row->hp_run ];
    if ( pv->len == 0 )
    {
        /* vector was not used before */
        pv->len = ( ( base_pos + 1 ) / POS_VECTOR_INC ) + 1;
        pv->len *= POS_VECTOR_INC;
        pv->v = calloc( pv->len, sizeof pv->v[0] );
        if ( pv->v == NULL )
        {
            rc = RC( rcApp, rcSelf, rcConstructing, rcMemory, rcExhausted );
        }
    }
    else if ( pv->len < ( base_pos + 1 ) )
    {
        void * tmp;
        /* vector has to be increased */
        uint32_t org_len = pv->len;
        pv->len = ( ( base_pos + 1 ) / POS_VECTOR_INC ) + 1;
        pv->len *= POS_VECTOR_INC;
        /* prevent from leaking memory by capturing the new pointer in temp. var. */
        tmp = realloc( pv->v, pv->len * ( sizeof pv->v[0] ) );
        if ( tmp == NULL )
        {
            rc = RC( rcApp, rcSelf, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            pv->v = tmp;
            /* the added part has to be set to zero */
            qual *to_zero = pv->v;
            to_zero += org_len;
            memset( to_zero, 0, ( pv->len - org_len ) * ( sizeof pv->v[0] ) );
        }
    }

    if ( rc == 0 )
    {
        qual *q = &pv->v[ base_pos ];
        switch( rd_case )
        {
            case CASE_MISMATCH : q->mismatches++; /* no break intented! */
            case CASE_MATCH    : q->count++;
                                 break;
        }
    }
    else
    {
        pv->len = 0;
    }
    return rc;
}


static int CC spotgroup_sort( const BSTNode *item, const BSTNode *n )
{
    spotgrp * sg1 = ( spotgrp* ) item;
    spotgrp * sg2 = ( spotgrp* ) n;
    return StringCompare ( sg1->name, sg2->name );
}


static rc_t expand_and_clear_vector( uint8_t **v, uint32_t *len, uint32_t new_len )
{
    rc_t rc = 0;
    if ( *len < new_len )
    {
        *len += new_len;
        *v = realloc( *v, *len );
        if ( *v == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            LogErr( klogInt, rc, "failed to expand (uint8_t)vector\n" );
        }
    }
    if ( rc == 0 )
        memset( *v, 0, *len ); 
    return rc;
}


static rc_t get_exlude_vector( statistic * data, uint32_t *ref_len )
{
    /* we need: REF_OFFSET, REF_LEN and REF_NAME for that */
    rc_t rc;
    String s_ref_name;

    const char * ref_name_base = ( const char * )data->rd_col[ RIDX_REF_SEQ_ID ].base;
    uint32_t ref_name_len = data->rd_col[ RIDX_REF_SEQ_ID ].row_len;

    int32_t ref_offset = *( ( int32_t *)data->rd_col[ RIDX_REF_POS ].base );
    *ref_len = *( ( uint32_t *)data->rd_col[ RIDX_REF_LEN ].base );

    StringInit( &s_ref_name, ref_name_base, ref_name_len, ref_name_len );

    /* make the ref-exclude-vector longer if necessary */
    rc = expand_and_clear_vector( &data->exclude_vector,
                                  &data->exclude_vector_len,
                                  *ref_len );
    if ( rc == 0 )
        rc = get_ref_exclude( &data->exclude,
                              &s_ref_name,
                              ref_offset,
                              *ref_len,
                              data->exclude_vector );

    return rc;
}


static rc_t walk_exclude_vector( statistic * data,
                                 uint32_t n_bases,
                                 uint32_t ref_len )
{
    rc_t rc = 0;
    /* we need: HAS_REF_OFFSET, REF_OFFSET and HAS_MISMATCH for that */

    const char * has_roffs = ( const char * )data->rd_col[ RIDX_HAS_REF_OFFSET ].base;
    uint32_t has_roffs_len = data->rd_col[ RIDX_HAS_REF_OFFSET ].row_len;

    const int32_t * roffs = ( const int32_t * )data->rd_col[ RIDX_REF_OFFSET ].base;
    uint32_t roffs_len = data->rd_col[ RIDX_REF_OFFSET ].row_len;

    const char * has_mm = ( const char * )data->rd_col[ RIDX_HAS_MISMATCH ].base;
    uint32_t has_mm_len = data->rd_col[ RIDX_HAS_MISMATCH ].row_len;

    if ( has_roffs_len != n_bases || has_mm_len != n_bases )
    {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        LogErr( klogInt, rc, "number of bases does not match length of HAS_REF_OFFSET or HAS_MISMATCH\n" );
    }
    else
    {
        /* we count how many REF_OFFSETS have to be there... */
        uint32_t hro_count = 0;
        uint32_t idx;
        for ( idx = 0; idx < n_bases; ++idx )
        {
            if ( has_roffs[ idx ] == '1' )
                hro_count++;
        }
        if ( hro_count != roffs_len )
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            LogErr( klogInt, rc, "number of HAS_REF_OFFSET=1 does not match length of REF_OFFSET\n" );
        }
    }

    /* */
    if ( rc == 0 )
    {
        int32_t ref_idx = 0;
        uint32_t base_idx = 0;
        uint32_t roffs_idx = 0;
        while ( ref_idx < (int32_t)ref_len )
        {
            /* before we handle the reference, apply the ref-offset to the
               iterator ( in this case: ref_idx ) */
            if ( has_roffs[ base_idx ] == '1' )
            {
                /* this handles the left-clipping (... ref_idx < 0 )*/
                for ( ref_idx += roffs[ roffs_idx++ ]; ref_idx < 0; ++ref_idx )
                {
                    data->case_vector[ base_idx++ ] = CASE_IGNORE;
                }
            }
            assert( ref_idx >= 0 );
            /* all the calculation is only done to put the IGNORE-flags
               into the right base-position (if necessary): */
            if ( data->exclude_vector[ ref_idx++ ] > 0 )
                data->case_vector[ base_idx ] = CASE_IGNORE;
            base_idx++;
        }
        /* walk backwards from the end to apply right-clipping */
        assert( n_bases > 0 );
        base_idx = n_bases - 1;
        while ( base_idx > 0 && has_mm[ base_idx ] == '1' )
        {
            data->case_vector[ base_idx-- ] = CASE_IGNORE;
        }
    }

    /* enter the mis-matches into the case-vector,
       but only where the base-position is not ignored... */
    if ( rc == 0 )
    {
        uint32_t base_idx;
        for ( base_idx = 0; base_idx < n_bases; ++base_idx )
        {
            if ( has_mm[ base_idx ] == '1' &&
                 data->case_vector[ base_idx ] != CASE_IGNORE )
            {
                data->case_vector[ base_idx ] = CASE_MISMATCH;
            }
        }
    }
    return rc;
}


static const char base_complement[26] =
{
   /* ABCDEFGHIJKLMNOPQRSTUVWXYZ*/
     "TBGDEFCHIJKLMNOPQRSAUVWXYZ"
};


static char complement( const char base )
{
    if ( base >= 'A' && base <= 'Z' )
        return base_complement[ base - 'A' ];
    else
        return 'N';
}


static void reverse_read_and_case( statistic * data,
                                   char * rd_ptr,
                                   uint8_t *qual_ptr,
                                   uint32_t n_bases )
{
    uint32_t p1;
    uint32_t p2 = n_bases - 1;
    uint32_t n = ( n_bases / 2 );
    for ( p1 = 0; p1 < n; ++p1, --p2 )
    {
        char rd_temp = rd_ptr[ p1 ];
        uint8_t c_temp = data->case_vector[ p1 ];
        uint8_t q_temp = qual_ptr[ p1 ];

        rd_ptr[ p1 ] = complement( rd_ptr[ p2 ] );
        data->case_vector[ p1 ] = data->case_vector[ p2 ];
        qual_ptr[ p1 ] = qual_ptr[ p2 ];

        rd_ptr[ p2 ] = complement( rd_temp );
        data->case_vector[ p2 ] = c_temp;
        qual_ptr[ p2 ] = q_temp;
    }
    /* don't forget to complement the base in the middle (if n_bases is odd) */
    if ( n_bases & 1 )
        rd_ptr[ n ] = complement( rd_ptr[ n ] );
}


static rc_t loop_through_base_calls( spotgrp *sg,
                       uint32_t gc_window,
                       char * read_ptr,
                       uint32_t n_bases,
                       uint8_t * qual_ptr,
                       uint8_t * case_ptr,
                       uint32_t base_pos_offset )
{
    rc_t rc = 0;
    char prev_char = 0;
    stat_row row;
    char *gc_ptr = read_ptr;
    memset( &row, 0, sizeof row );

    for ( row.base_pos = 0; row.base_pos < ( n_bases - 1 ) && rc == 0; ++row.base_pos )
    {
        /* calculate the hp-run-count */
        if ( prev_char == *read_ptr )
        {
            row.hp_run++;
            assert( row.hp_run <= n_bases );
        }
        else
        {
            prev_char = *read_ptr;
            row.hp_run = 0;
        }

        /* advance the "window" */
        if ( row.base_pos >= ( gc_window + 1 ) )
        {
            if ( *gc_ptr == 'G' || *gc_ptr == 'C' )
            {
                assert( row.gc_content > 0 );
                row.gc_content--;
            }
            gc_ptr++;
        }

        if ( case_ptr[0] != CASE_IGNORE && case_ptr[1] != CASE_IGNORE )
        {
            row.quality = *qual_ptr;
            rc = spotgroup_enter_values( sg,
                                    &row, 
                                    dimer_2_bin( read_ptr ),
                                    *case_ptr,
                                    base_pos_offset );
        }

        /* handle the current base-position after the record was entered
           because we do not include the current base into the gc-content */
        if ( *read_ptr == 'G' || *read_ptr == 'C' )
            row.gc_content++;

        qual_ptr++;
        read_ptr++;
        case_ptr++;
    }
    return rc;
}


static rc_t extract_spotgroup_statistic( statistic * data,
                                         spotgrp *sg,
                                         uint32_t n_bases )
{
    uint32_t ref_len, base_pos_offset = 0;
    char *read_ptr = ( char * )data->rd_col[ RIDX_READ ].base;
    uint8_t *qual_ptr = ( uint8_t * )data->rd_col[ RIDX_QUALITY ].base;

    /* (1) ... get the exclusion-list for this read */
    rc_t rc = get_exlude_vector( data, &ref_len );

    /* (2) ... make the case-vector longer if necessary */
    if ( rc == 0 )
    {
        rc = expand_and_clear_vector( &data->case_vector,
                                      &data->case_vector_len,
                                      n_bases );
    }

    /* (3) ... walk the exclusion-list and Mismatch-vector to build the case-vector */
    if ( rc == 0 )
    {
        rc = walk_exclude_vector( data, n_bases, ref_len );
    }

    /* (4) ... if we have to, we reverse case-vector and revers/complement the read-vector */
    if ( rc == 0 )
    {
        const bool * reverse = data->rd_col[ RIDX_REF_ORIENTATION ].base;
        if ( *reverse )
        {
            reverse_read_and_case( data, read_ptr, qual_ptr, n_bases );
        }
    }

    /* (5) ... check if we are in SEQ_READ_ID == 2 */
    if ( *( ( uint32_t * )data->rd_col[ RIDX_SEQ_READ_ID ].base ) == 2 )
    {
        base_pos_offset = n_bases;
    }
    /* TBD: make it more general for more than 2 reads per spot of different sized reads */

    /* (6) ... */
    if ( rc == 0 )
    {
        rc = loop_through_base_calls( sg, data->gc_window, read_ptr, n_bases, 
                   qual_ptr, data->case_vector, base_pos_offset );
    }
    return rc;
}


static rc_t extract_statistic_from_row( statistic * data )
{
    rc_t rc = 0;
    spotgrp *sg;

    /* first try the SPOT_GROUP column (correct for newer db's) */
    const char * spotgrp_base  = data->rd_col[ RIDX_SPOT_GROUP ].base;
    uint32_t spotgrp_len = data->rd_col[ RIDX_SPOT_GROUP ].row_len;
    if ( spotgrp_len < 1 || *spotgrp_base == 0 )
    {
        /* if empty try with SEQ_SPOT_GROUP column (correct for older db's) */
        spotgrp_base  = data->rd_col[ RIDX_SEQ_SPOT_GROUP ].base;
        spotgrp_len = data->rd_col[ RIDX_SEQ_SPOT_GROUP ].row_len;
    }

    sg = find_spotgrp( data, spotgrp_base, spotgrp_len );
    if ( sg == NULL )
    {
        sg = make_spotgrp( spotgrp_base, spotgrp_len );
        if ( sg == NULL )
            rc = RC( rcApp, rcSelf, rcConstructing, rcMemory, rcExhausted );
        else
            rc = BSTreeInsert ( &data->spotgroups, (BSTNode *)sg, spotgroup_sort );
    }
    if ( rc == 0 )
    {
        uint32_t n_bases  = data->rd_col[ RIDX_READ ].row_len;
        uint32_t qual_len = data->rd_col[ RIDX_QUALITY ].row_len;
        uint32_t hmis_len = data->rd_col[ RIDX_HAS_MISMATCH ].row_len;

        if ( ( n_bases == qual_len ) &&  ( n_bases == hmis_len ) )
        {
            rc = extract_spotgroup_statistic( data, sg, n_bases );
        }
        else
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcData, rcInvalid );
            LogErr( klogInt, rc, "number of bases, quality and has_mismatch is not the same\n" );
        }
    }
    return rc;
}


rc_t read_and_extract_statistic_from_row( statistic * data, const VCursor *my_cursor, 
                         const int64_t row_id )
{
    rc_t rc = read_cells( my_cursor, row_id, data->rd_col, ridx_names, N_RIDX );
    if ( rc == 0 )
    {
        rc = extract_statistic_from_row( data );
    }
    return rc;
}


rc_t query_statistic_rowrange( statistic * data, const VCursor *my_cursor, 
                               int64_t *first, uint64_t * count )
{
    rc_t rc = VCursorIdRange ( my_cursor, data->rd_col[ RIDX_READ ].idx, first, count );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VCursorIdRange() failed\n" );
    return rc;
}


rc_t open_statistic_cursor( statistic * data, const VCursor *my_cursor )
{
    rc_t rc = add_columns( my_cursor, data->rd_col, ridx_names, N_RIDX );
    if ( rc == 0 )
    {
        rc = VCursorOpen ( my_cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorOpen failed\n" );
    }
    return rc;
}


typedef struct iter_ctx
{
    bool ( CC * f ) ( stat_row * row, void *data );
    void * data;
    uint64_t n;
} iter_ctx;


static void CC spotgroup_iter( BSTNode *n, void *data )
{
    spotgrp *sg = ( spotgrp * ) n;
    iter_ctx *ctx = ( iter_ctx * )data;
    bool run = true;
    const char * name = sg->name->addr;
    uint8_t dimer_idx;
    for ( dimer_idx = 0; dimer_idx < N_DIMER_VALUES && run; ++dimer_idx )
    {
        uint8_t qual_idx;
        for ( qual_idx = 0; qual_idx < N_QUAL_VALUES && run ; ++qual_idx )
        {
            uint8_t gc_idx;
            for ( gc_idx = 0; gc_idx < N_GC_VALUES && run ; ++gc_idx )
            {
                uint8_t hp_idx;
                for ( hp_idx = 0; hp_idx < N_HP_VALUES && run ; ++hp_idx )
                {
                    pos_vector *pv = &sg->dimers[dimer_idx][qual_idx][gc_idx][hp_idx];
                    if ( pv->v != NULL )
                    {
                        uint32_t pos_idx;
                        for ( pos_idx = 0; pos_idx < pv->len && run; ++pos_idx )
                        {
                            qual *q = &pv->v[pos_idx];
                            if ( q->count > 0 )
                            {
                                stat_row row;
                                /* the parameters */
                                row.spotgroup  = (char *)name;
                                row.dimer      = (char *)dimer_2_ascii[dimer_idx];
                                row.quality    = qual_idx;
                                row.hp_run     = hp_idx;
                                row.gc_content = gc_idx;
                                row.base_pos   = pos_idx;

                                /* the counters */
                                row.count      = q->count;
                                row.mismatch_count = q->mismatches;

                                run = ctx->f( &row, ctx->data );
                                ctx->n++;
                            }
                        }
                    }
                }
            }
        }
    }
}


uint64_t foreach_statistic( statistic * data,
    bool ( CC * f ) ( stat_row * row, void * f_data ), void *f_data )
{
    iter_ctx ctx;
    ctx.f = f;
    ctx.data = f_data;
    ctx.n = 0;
    BSTreeForEach ( &data->spotgroups, false, spotgroup_iter, &ctx );
    return ctx.n;
}


/************** WRITER **********************************************/
static rc_t open_writer_cursor( statistic_writer *writer )
{
    rc_t rc = 0;
    uint32_t idx;

    for ( idx = 0; idx < N_WIDX && rc == 0; ++idx )
    {
        rc = add_column( writer->cursor, &writer->wr_col[ idx ], widx_names[ idx ] );
    }
    if ( rc == 0 )
    {
        rc = VCursorOpen ( writer->cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorOpen failed\n" );
    }
    return rc;
}


rc_t make_statistic_writer( statistic_writer *writer, VCursor * cursor )
{
    writer->cursor = cursor;
    memset( &writer->wr_col, 0, sizeof writer->wr_col );
    return open_writer_cursor( writer );
}


typedef struct writer_ctx
{
    statistic_writer *writer;
    rc_t rc;
} writer_ctx;


static bool CC write_cb( stat_row * row, void * data )
{
    writer_ctx * ctx = ( writer_ctx * ) data;
    col * cols = ( col * )&ctx->writer->wr_col;
    VCursor * cursor = ctx->writer->cursor;
  
    rc_t rc = VCursorOpenRow( cursor );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VCursorOpen() failed\n" );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_SPOT_GROUP ].idx, 8,
                              row->spotgroup, string_size( row->spotgroup ),
                              widx_names[ WIDX_SPOT_GROUP ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_KMER ].idx, 8,
                              row->dimer, string_size( row->dimer ),
                              widx_names[ WIDX_KMER ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_ORIG_QUAL ].idx, 8,
                              &row->quality, 1, widx_names[ WIDX_ORIG_QUAL ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_CYCLE ].idx, 32,
                              &row->base_pos, 1, widx_names[ WIDX_CYCLE ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_TOTAL_COUNT ].idx, 32,
                              &row->count, 1, widx_names[ WIDX_TOTAL_COUNT ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_MISMATCH_COUNT ].idx, 32,
                              &row->mismatch_count, 1,
                              widx_names[ WIDX_MISMATCH_COUNT ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_HPRUN ].idx, 32,
                              &row->hp_run, 1, widx_names[ WIDX_HPRUN ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_GC_CONTENT ].idx, 32,
                              &row->gc_content, 1, widx_names[ WIDX_GC_CONTENT ] );
    if ( rc == 0 )
    {
        rc = VCursorCommitRow( cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorCommitRow() failed\n" );
    }
    if ( rc == 0 )
    {
        rc = VCursorCloseRow( cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorCloseRow() failed\n" );
    }

    ctx->rc = rc;
    return ( rc == 0 );
}


rc_t write_statistic( statistic_writer *writer, statistic *data,
                      uint64_t * written )
{
    writer_ctx ctx;
    uint64_t count;

    ctx.writer = writer;
    ctx.rc = 0;
    count = foreach_statistic( data, write_cb, &ctx );
    if ( written != NULL ) *written = count;

    return ctx.rc;
}


rc_t whack_statistic_writer( statistic_writer *writer )
{
    rc_t rc = VCursorCommit( writer->cursor );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VCursorCommit() failed\n" );
    return rc;
}
