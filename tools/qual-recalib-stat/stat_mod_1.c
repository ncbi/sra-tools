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

#include "stat_mod_1.h"
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const char * ridx_names[ N_RIDX ] =
{
    "RAW_READ",
    "QUALITY",
    "HAS_MISMATCH",
    "SEQ_SPOT_ID",
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


typedef struct counter
{
    uint32_t mismatches;
    uint32_t count;
} counter;


typedef struct counter_vector
{
    counter *v;
    uint32_t n_counters;
} counter_vector;


typedef struct spotgrp
{
    BSTNode node;
    const String *name;
    counter_vector cnv[ N_DIMER_VALUES ][ N_GC_VALUES ][ N_HP_VALUES ][ N_QUAL_VALUES ];
} spotgrp;


static const uint8_t char_2_base_bin[26] =
{
   /* A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W  X  Y  Z*/
      0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4
};


static uint8_t dimer_2_bin( char c0, char c1 )
{
    uint8_t res = 16;

    if ( c0 >= 'A' && c0 <= 'Z' && c1 >= 'A' && c1 <= 'Z' )
    {
        uint8_t t1 = char_2_base_bin[ (uint8_t)( c0 - 'A' ) ];
        uint8_t t2 = char_2_base_bin[ (uint8_t)( c1 - 'A' ) ];
        if ( t1 < 4 && t2 < 4 )
        {
            res = t1;
            res <<= 2;
            res |= t2;
        }
    }
    return res;
}


static const char * dimer_2_ascii[] = 
{ "AA", "AC", "AG", "AT", 
  "CA", "CC", "CG", "CT",
  "GA", "GC", "GG", "GT",
  "TA", "TC", "TG", "TT", "XX" };


/******************************************************************************
    for the spot-group ( tree-node ), contains a tree of counter's
******************************************************************************/
static void CC whack_spotgroup( BSTNode *n, void *data )
{
    spotgrp * sg = ( spotgrp * )n;
    uint32_t idx, count;
    count = ( ( sizeof sg->cnv ) / sizeof( sg->cnv[0] ) );
    for ( idx = 0; idx < count; ++idx )
    {
        counter_vector * cv = (counter_vector *)&( sg->cnv[ idx ] );
        if ( cv->v != NULL )
        {
            free( cv->v );
        }
    }
    if ( sg->name != NULL )
        StringWhack ( sg->name );
    free( n );
}


static spotgrp * make_spotgrp( const char *src, const size_t len )
{
    spotgrp * sg = calloc( 1, sizeof sg[ 0 ] );
    if ( sg != NULL )
    {
        String s;
        StringInit( &s, src, len, len );
        if ( StringCopy ( &sg->name, &s ) != 0 )
        {
            free( sg );
            sg = NULL;
        }
    }
    return sg;
}


static int CC spotgroup_find( const void *item, const BSTNode *n )
{
    spotgrp * sg = ( spotgrp* ) n;
    return StringCompare ( ( String* ) item, sg->name );
}


static spotgrp * find_spotgroup( statistic *data, const char *src, const size_t len )
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


static rc_t spotgroup_enter_values( spotgrp * spotgroup,
                                    const uint8_t quality,
                                    const uint8_t dimer_code,
                                    const uint8_t gc_content,
                                    const uint8_t hp_run,
                                    const uint32_t cycle,
                                    const uint8_t rd_case,
                                    const uint64_t row_id )
{
    rc_t rc = 0;
    uint8_t q = quality;
    uint8_t d = dimer_code;
    uint8_t g = gc_content;
    uint8_t h = hp_run;
    counter_vector * cv;

    if ( q >= N_QUAL_VALUES ) q = ( N_QUAL_VALUES - 1 );
    if ( d >= N_DIMER_VALUES ) d = ( N_DIMER_VALUES - 1 );
    if ( g >= N_GC_VALUES ) g = ( N_GC_VALUES - 1 );
    if ( h >= N_HP_VALUES ) h = ( N_HP_VALUES - 1 );
    cv = &( spotgroup->cnv[ d ][ g ][ h ][ q ] );

    if ( cv->v ==  NULL )
    {
        /* the counter-block was not used before at all */
        cv->n_counters = ( ( cycle / COUNTER_BLOCK_SIZE ) + 1 ) * COUNTER_BLOCK_SIZE;
        cv->v = calloc( cv->n_counters, sizeof cv->v[0] );
        if ( cv->v == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            PLOGERR( klogInt, ( klogInt, rc, 
                     "calloc() failed at row#$(row_nr) cycle#$(cycle)",
                     "row_nr=%lu,cycle=%u", row_id, cycle ) );
        }
    }
    else
    {
        if ( cycle >= cv->n_counters )
        {
            /* the counter-block has to be extended */
            void * tmp;
            uint32_t org_len = cv->n_counters;
            cv->n_counters = ( ( cycle / COUNTER_BLOCK_SIZE ) + 1 ) * COUNTER_BLOCK_SIZE;
            /* prevent from leaking memory by capturing the new pointer in temp. var. */
            tmp = realloc( cv->v, cv->n_counters * ( sizeof cv->v[0] ) );
            if ( tmp == NULL )
            {
                rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                PLOGERR( klogInt, ( klogInt, rc, 
                         "realloc() failed at row#$(row_nr) cycle#$(cycle)",
                         "row_nr=%lu,cycle=%u", row_id, cycle ) );
            }
            else
            {
                /* the added part has to be set to zero */
                counter * to_zero_out = tmp;
                to_zero_out += org_len;
                memset( to_zero_out, 0, ( cv->n_counters - org_len ) * ( sizeof *to_zero_out ) );
                cv->v = tmp;
            }
        }
    }
    assert( cycle < cv->n_counters );

    if ( rc == 0 )
    {
        counter * cnt = &( cv->v[ cycle ] );
        switch( rd_case )
        {
            case CASE_MISMATCH : cnt->mismatches++; /* no break intented! */
            case CASE_MATCH    : cnt->count++;
                                 break;
        }
    }
    return rc;
}


static int CC spotgroup_sort( const BSTNode *item, const BSTNode *n )
{
    spotgrp * sg1 = ( spotgrp* ) item;
    spotgrp * sg2 = ( spotgrp* ) n;
    return StringCompare ( sg1->name, sg2->name );
}


/******************************************************************************
    for the statistic ( tree-node ), contains a tree of spot-groups's
******************************************************************************/
static rc_t make_vector( void ** ptr, uint32_t *len, uint32_t new_len )
{
    rc_t rc = 0;

    *len = new_len;
    *ptr = calloc( 1, new_len );
    if ( *ptr == NULL )
    {
        *len = 0;
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        LogErr( klogInt, rc, "failed to make large enough exclude-vector\n" );
    }
    return rc;
}


rc_t make_statistic( statistic *data, uint32_t gc_window, uint8_t cycle_offset,
                     KDirectory *dir, const char * exclude_db,
                     bool info, bool ignore_mismatches )
{
    rc_t rc = 0;
    memset( data, 0, sizeof *data );
    BSTreeInit( &data->spotgroups );
    data->last_used_spotgroup = NULL;
    data->sequence = NULL;
    data->ignore_mismatches = ignore_mismatches;
    memset( &data->rd_col, 0, sizeof data->rd_col );
    if ( exclude_db != NULL )
    {
        make_ref_exclude( &data->exclude, dir, exclude_db, info );
        data->ref_exclude_used = true;
    }
    data->gc_window = gc_window;
    if ( data->gc_window >= N_GC_VALUES )
    {
        data->gc_window = ( N_GC_VALUES - 1 );
    }
    data->cycle_offset = cycle_offset;
    if ( data->cycle_offset > 1 )
    {
        data->cycle_offset = 1;
    }

    if ( rc == 0 && data->ref_exclude_used )
    {
        rc = make_vector( (void**)&data->exclude_vector, &data->exclude_vector_len, 512 );
    }
    if ( rc == 0 )
    {
        rc = make_vector( (void**)&data->case_vector, &data->case_vector_len, 512 );
    }
    return rc;
}


void set_spot_pos_statistic( statistic *data, spot_pos * sequence )
{
    data->sequence = sequence;
}


void whack_statistic( statistic *data )
{
    BSTreeWhack ( &data->spotgroups, whack_spotgroup, NULL );
    if ( data->ref_exclude_used )
    {
        whack_ref_exclude( &data->exclude );
    }
    if ( data->exclude_vector != NULL )
    {
        free( data->exclude_vector );
    }
    if ( data->case_vector != NULL )
    {
        free( data->case_vector );
    }
}


static rc_t expand_and_clear_vector( void **v, uint32_t *len, uint32_t new_len, bool clear )
{
    rc_t rc = 0;
    if ( *v != NULL )
    {
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
        if ( rc == 0 && clear )
            memset( *v, 0, *len ); 
    }
    return rc;
}


static rc_t get_exlude_vector( statistic * data, uint32_t *ref_len )
{
    /* we need: REF_OFFSET, REF_LEN and REF_NAME for that */
    rc_t rc = 0;

    if ( data->ref_exclude_used )
    {
        String s_ref_name;
        const char * ref_name_base = ( const char * )data->rd_col[ RIDX_REF_SEQ_ID ].base;
        uint32_t ref_name_len = data->rd_col[ RIDX_REF_SEQ_ID ].row_len;

        int32_t ref_offset = *( ( int32_t *)data->rd_col[ RIDX_REF_POS ].base );
        *ref_len = *( ( uint32_t *)data->rd_col[ RIDX_REF_LEN ].base );

        StringInit( &s_ref_name, ref_name_base, ref_name_len, ref_name_len );

        /* make the ref-exclude-vector longer if necessary */
        rc = expand_and_clear_vector( (void**)&data->exclude_vector,
                                      &data->exclude_vector_len,
                                      *ref_len,
                                      true );
        if ( rc == 0 )
            rc = get_ref_exclude( &data->exclude,
                                  &s_ref_name,
                                  ref_offset,
                                  *ref_len,
                                  data->exclude_vector,
                                  &data->active_exclusions );
    }

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
        uint32_t base_idx;
        uint32_t roffs_idx = 0;

        /* walk backwards from the end to apply right-clipping */
        assert( n_bases > 0 );
        base_idx = n_bases - 1;
        while ( base_idx > 0 && has_mm[ base_idx ] == '1' )
        {
            data->case_vector[ base_idx-- ] = CASE_IGNORE;
            n_bases--;
        }

        base_idx = 0;
        while ( ref_idx < (int32_t)ref_len && base_idx < n_bases )
        {
            /* before we handle the reference, apply the ref-offset to the
               iterator ( in this case: ref_idx ) */
            if ( has_roffs[ base_idx ] == '1' )
            {
                int32_t roffs_value = roffs[ roffs_idx++ ];
                /* this handles the left-clipping and inserts */
                if ( roffs_value < 0 )
                {
                    while( roffs_value++ < 0 )
                    {
                        data->case_vector[ base_idx++ ] = CASE_IGNORE;
                    }
                }
                else
                {
                    ref_idx += roffs_value;
                }
            }
            assert( ref_idx >= 0 );
            /* right now ref_idx and base_idx points to corresponding positions */

            /* all the calculation is only done to put the IGNORE-flags
               into the right base-position (if necessary): */
            if ( data->exclude_vector[ ref_idx++ ] > 0 )
            {
                data->case_vector[ base_idx ] = CASE_IGNORE;
            }
            else
            {
                /* enter the mis-matches into the case-vector */
                if ( has_mm[ base_idx ] == '1' )
                {
                    data->case_vector[ base_idx ] = CASE_MISMATCH;
                }
            }
            ++base_idx;
        }
    }
    return rc;
}


static rc_t loop_through_base_calls( spotgrp *sg,
                        uint32_t gc_window,
                        uint8_t cycle_offset,
                        char * read_ptr,    /* points at begin of array */
                        uint32_t n_bases,
                        uint8_t * qual_ptr, /* points at begin of array */
                        uint8_t * case_ptr, /* points at begin of array */
                        uint32_t base_pos_offset,
                        const int64_t row_id,
                        const int32_t ofs )
{
    rc_t rc = 0;
    uint32_t base_pos;
    char prev_char = 0;
    char * gc_ptr = read_ptr;
    uint8_t gc_content = 0;
    uint8_t hp_run = 0;
    bool enter_value;

    for ( base_pos = 0; base_pos < ( n_bases - 1 ) && rc == 0; ++base_pos )
    {
        /* calculate the hp-run-count */
        if ( prev_char == *read_ptr )
        {
            hp_run++;
            assert( hp_run <= n_bases );
        }
        else
        {
            prev_char = *read_ptr;
            hp_run = 0;
        }

        /* advance the "window" */
        if ( base_pos >= ( gc_window + 1 ) )
        {
            if ( *gc_ptr == 'G' || *gc_ptr == 'C' )
            {
                assert( gc_content > 0 );
                gc_content--;
            }
            gc_ptr++;
        }

        if ( case_ptr != NULL )
        {
            enter_value = ( case_ptr[0] != CASE_IGNORE && case_ptr[ofs] != CASE_IGNORE );
        }
        else
        {
            enter_value = true;
        }

        if ( enter_value )
        {
            uint8_t case_value = CASE_MATCH;
            if ( case_ptr != NULL )
            {
                case_value = case_ptr[1];
            }
            rc = spotgroup_enter_values( sg,
                                         qual_ptr[1],
                                         dimer_2_bin( read_ptr[0], read_ptr[1] ),
                                         gc_content,
                                         hp_run,
                                         base_pos + base_pos_offset + cycle_offset,
                                         case_value,
                                         row_id );
        }

        /* handle the current base-position after the record was entered
           because we do not include the current base into the gc-content */
        if ( *read_ptr == 'G' || *read_ptr == 'C' )
            gc_content++;

        qual_ptr += ofs;
        read_ptr++;
        if ( case_ptr != NULL )
        {
            case_ptr += ofs;
        }
    }
    return rc;
}


static rc_t extract_spotgroup_statistic( statistic * data,
                                         spotgrp *sg,
                                         uint32_t n_bases,
                                         const int64_t row_id )
{
    uint32_t ref_len, seq_read_id, base_pos_offset = 0;
    char *read_ptr = ( char * )data->rd_col[ RIDX_READ ].base;
    uint8_t *qual_ptr = ( uint8_t * )data->rd_col[ RIDX_QUALITY ].base;
    const bool * reverse = data->rd_col[ RIDX_REF_ORIENTATION ].base;

    /* (1) ... make the case-vector longer if necessary */
    rc_t rc = expand_and_clear_vector( (void**)&data->case_vector,
                                      &data->case_vector_len,
                                      n_bases,
                                      true );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc, 
             "expand_and_clear_vector( case_vector ) failed at row $(row_nr)",
             "row_nr=%lu", row_id ) );
    }

    /* (2) ... get the exclusion-list for this read */
    if ( data->ref_exclude_used )
    {
        rc = get_exlude_vector( data, &ref_len );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc, 
                 "get_exlude_vector() failed at row $(row_nr)",
                 "row_nr=%lu", row_id ) );
        }

        /* (3) ... walk the exclusion-list and Mismatch-vector to build the case-vector */
        if ( rc == 0 )
        {
            rc = walk_exclude_vector( data, n_bases, ref_len );
            if ( rc != 0 )
            {
                PLOGERR( klogInt, ( klogInt, rc, 
                     "walk_exclude_vector failed at row $(row_nr)",
                     "row_nr=%lu", row_id ) );
            }
        }
    }
    else
    {
        if ( !data->ignore_mismatches )
        {
            const char * has_mm = ( const char * )data->rd_col[ RIDX_HAS_MISMATCH ].base;
            uint32_t has_mm_len = data->rd_col[ RIDX_HAS_MISMATCH ].row_len;
            uint32_t base_idx;
            for ( base_idx = 0; base_idx < n_bases && base_idx < has_mm_len; ++base_idx )
            {
                if ( has_mm[ base_idx ] == '1' )
                {
                    data->case_vector[ base_idx ] = CASE_MISMATCH;
                }
            }
        }
    }

    /* (4) ... query the base-postion of this read in the spot from the SEQUENCE-table */
    seq_read_id = *( ( uint32_t * )data->rd_col[ RIDX_SEQ_READ_ID ].base );
#ifdef LOOKUP_ALL_SEQ_READ_ID
    if ( seq_read_id > 0 )
#else
    if ( seq_read_id > 1 )
#endif
    {
        uint32_t spot_id = *( ( uint32_t * )data->rd_col[ RIDX_SEQ_SPOT_ID ].base );
        rc = query_spot_pos( data->sequence, seq_read_id, spot_id, &base_pos_offset );
    }

    /* (5) ... looping throuhg the bases ( forward/backward ) */
    if ( rc == 0 )
    {

        if ( *reverse )
        {
            uint8_t * loc_case_vector = data->case_vector + ( n_bases - 1 );
            qual_ptr += ( n_bases - 1 );
            rc = loop_through_base_calls( sg, data->gc_window, data->cycle_offset,
                    read_ptr, n_bases, qual_ptr, loc_case_vector,
                    base_pos_offset, row_id, -1 );

        }
        else
        {
            rc = loop_through_base_calls( sg, data->gc_window, data->cycle_offset,
                    read_ptr, n_bases, qual_ptr, data->case_vector, 
                    base_pos_offset, row_id, +1 );
        }
    }
    return rc;
}


static rc_t extract_statistic_from_row( statistic * data, const int64_t row_id )
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

    sg = find_spotgroup( data, spotgrp_base, spotgrp_len );
    if ( sg == NULL )
    {
        sg = make_spotgrp( spotgrp_base, spotgrp_len );
        if ( sg == NULL )
        {
            rc = RC( rcApp, rcSelf, rcConstructing, rcMemory, rcExhausted );
            PLOGERR( klogInt, ( klogInt, rc, 
                     "make_spotgrp failed at row $(row_nr)", "row_nr=%lu", row_id ) );
        }
        else
        {
            rc = BSTreeInsert ( &data->spotgroups, (BSTNode *)sg, spotgroup_sort );
            if ( rc != 0 )
            {
                PLOGERR( klogInt, ( klogInt, rc, 
                     "BSTreeInsert( new spotgroup ) at row $(row_nr)", "row_nr=%lu", row_id ) );
            }
        }
    }
    if ( rc == 0 )
    {
        uint32_t n_bases  = data->rd_col[ RIDX_READ ].row_len;
        uint32_t qual_len = data->rd_col[ RIDX_QUALITY ].row_len;
        uint32_t hmis_len = data->rd_col[ RIDX_HAS_MISMATCH ].row_len;

        if ( ( n_bases == qual_len ) &&  ( n_bases == hmis_len ) )
        {
            rc = extract_spotgroup_statistic( data, sg, n_bases, row_id );
        }
        else
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcData, rcInvalid );
            PLOGERR( klogInt, ( klogInt, rc, 
                 "number of bases, quality and has_mismatch is not the same at row $(row_nr)",
                 "row_nr=%lu", row_id ) );
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
        rc = extract_statistic_from_row( data, row_id );
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
    const char * name;
    bool run;
    stat_row row;
    uint64_t n;
} iter_ctx;


static bool CC spotgroup_iter( BSTNode *n, void *data )
{
    spotgrp *sg = ( spotgrp * ) n;
    iter_ctx *ctx = ( iter_ctx * )data;

    ctx->row.spotgroup = (char *)sg->name->addr;
    for ( ctx->row.quality = 0; ctx->row.quality < N_QUAL_VALUES && ctx->run; ++ctx->row.quality )
    {
        uint8_t dimer_nr;
        for ( dimer_nr = 0; dimer_nr < N_DIMER_VALUES && ctx->run; ++dimer_nr )
        {
            ctx->row.dimer = (char *)dimer_2_ascii[ dimer_nr ];
            for( ctx->row.gc_content = 0; ctx->row.gc_content < N_GC_VALUES; ++ctx->row.gc_content )
            {
                for ( ctx->row.hp_run = 0; ctx->row.hp_run < N_HP_VALUES && ctx->run; ++ctx->row.hp_run )
                {
                    uint32_t pos;
                    counter_vector * cv = &sg->cnv[ dimer_nr ][ ctx->row.gc_content ][ ctx->row.hp_run ][ ctx->row.quality ];
                    for ( pos = 0; pos < cv->n_counters; ++pos )
                    {
                        counter * c = &cv->v[ pos ];
                        if ( c->count > 0 )
                        {
                            ctx->row.base_pos = pos;
                            ctx->row.count = c->count;
                            ctx->row.mismatch_count = c->mismatches;

                            ctx->run = ctx->f( &ctx->row, ctx->data );
                            ctx->n++;
                         }
                    }
                }
            }
        }
    }
    return( !ctx->run );
}


uint64_t foreach_statistic( statistic * data,
    bool ( CC * f ) ( stat_row * row, void * f_data ), void *f_data )
{
    iter_ctx ctx;
    ctx.f = f;
    ctx.data = f_data;
    ctx.run = true;
    ctx.n = 0;
    BSTreeDoUntil ( &data->spotgroups, false, spotgroup_iter, &ctx );
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
