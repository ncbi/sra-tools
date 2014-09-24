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

#include "stat_mod_2.h"
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

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

#define USE_JUDY 1

typedef struct spotgrp
{
    BSTNode node;
    const String *name;
#ifdef USE_JUDY
    KVector *v;
#else
    counter_vector cnv[ N_MAX_QUAL_VALUES ][ N_READS ][ N_DIMER_VALUES ][ N_GC_VALUES ][ N_HP_VALUES ][ N_QUAL_VALUES ];
#endif
} spotgrp;


static const uint8_t char_2_base_bin[26] =
{
   /* A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W  X  Y  Z*/
      0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4
};


/*
    AA ...  0       CA ...  5       GA ... 10       TA ... 15       NA ... 20
    AC ...  1       CC ...  6       GC ... 11       TC ... 16       NC ... 21
    AG ...  2       CG ...  7       GG ... 12       TG ... 17       NG ... 22
    AT ...  3       CT ...  8       GT ... 13       TT ... 18       NT ... 23
    AN ...  4       CN ...  9       GN ... 14       TN ... 19       NN ... 24

    dimer-code = ( lookup( co ) * 5 ) + lookup( c1 )
    dimer-code = 0 ... 24
*/
static uint8_t dimer_2_bin( char c0, char c1 )
{
    uint8_t lookup0, lookup1;

    if ( c0 >= 'A' && c0 <= 'Z' )
    {
        lookup0 = char_2_base_bin[ (uint8_t)( c0 - 'A' ) ];
    }
    else
    {
        lookup0 = 4;
    }

    if ( c1 >= 'A' && c1 <= 'Z' )
    {
        lookup1 = char_2_base_bin[ (uint8_t)( c1 - 'A' ) ];
    }
    else
    {
        lookup1 = 4;
    }

    return ( ( lookup0 << 2 ) + lookup0 ) + lookup1;
}

static const char * dimer_2_ascii[] = 
{ "AA", "AC", "AG", "AT", "AN",
  "CA", "CC", "CG", "CT", "CN",
  "GA", "GC", "GG", "GT", "GN",
  "TA", "TC", "TG", "TT", "TN",
  "NA", "NC", "NG", "NT", "NN" };

/********************************************************************************
  6666.5555.5555.5544.4444.4444.3333.3333.3322.2222.2222.1111.1111.1100.0000.0000
  3210.9876.5432.1098.7654.3210.9876.5432.1098.7654.3210.9876.5432.1098.7654.3210
  CCCC CCCC CCCC CCCC CCCC CCCC CCCC CCCC RRRR RRDD DDDG GGGH HHHH MMMM MMQQ QQQQ

  C ... cycle ( 32 bit )
  R ... nread ( 6 bit )
  D ... dimer ( 5 bit )
  G ... gc-content ( 4 bit )
  H ... hp-run ( 5 bit )
  M ... max. qual ( 6 bit )
  Q ... quality ( 6 bit )
*********************************************************************************/
#ifdef USE_JUDY

static uint64_t encode_key( const uint32_t pos,
                            const uint8_t max_q,
                            const uint8_t nread,
                            const uint8_t dimer,
                            const uint8_t gc,
                            const uint8_t hp,
                            const uint8_t qual )
{
    uint64_t res = pos;
    res <<= 6;
    res |= ( nread & 0x3F );
    res <<= 5;
    res |= ( dimer & 0x1F );
    res <<= 4;
    res |= ( gc & 0xF );
    res <<= 5;
    res |= ( hp & 0x1F );
    res <<= 6;
    res |= ( max_q & 0x3F );
    res <<= 6;
    res |= ( qual & 0x3F );
    return res;
}


static void decode_key( const uint64_t key,
                        uint32_t *pos,
                        uint8_t *max_q,
                        uint8_t *nread,
                        uint8_t *dimer,
                        uint8_t *gc,
                        uint8_t *hp,
                        uint8_t *qual )
{
    uint64_t temp = key;
    *qual = temp & 0x3F;
    temp >>= 6;
    *max_q = temp & 0x3F;
    temp >>= 6;
    *hp = temp & 0x1F;
    temp >>= 5;
    *gc = temp & 0xF;
    temp >>= 4;
    *dimer = temp & 0x1F;
    temp >>= 5;
    *nread = temp & 0x3F;
    temp >>= 6;
    *pos = temp & 0xFFFFFFFF;
}


typedef struct two_counters
{
    uint32_t total;
    uint32_t mismatch;
} two_counters;

typedef union counter_union
{
    uint64_t value;
    two_counters counters;
} counter_union;


static bool set_counter( KVector *v,
                         const uint32_t pos,
                         const uint8_t max_q,
                         const uint8_t nread,
                         const uint8_t dimer,
                         const uint8_t gc,
                         const uint8_t hp,
                         const uint8_t qual,
                         bool mismatch )
{
    bool res = false;
    counter_union u;
    uint64_t key = encode_key( pos, max_q, nread, dimer, gc, hp, qual );
    if ( KVectorGetU64 ( v, key, &(u.value) ) == 0 )
    {
        u.counters.total++;
        if ( mismatch )
        {
            u.counters.mismatch++;
        }
    }
    else
    {
        u.counters.total = 1;
        res = true;
        if ( mismatch )
        {
            u.counters.mismatch = 1;
        }
        else
        {
            u.counters.mismatch = 0;
        }
    }
    KVectorSetU64 ( v, key, u.value );
    return res;
}

#if 0
static void get_counter( KVector *v,
                         const uint32_t pos,
                         const uint8_t max_q,
                         const uint8_t nread,
                         const uint8_t dimer,
                         const uint8_t gc,
                         const uint8_t hp,
                         const uint8_t qual,
                         uint32_t *total,
                         uint32_t *mismatch )
{
    counter_union u;
    uint64_t key = encode_key( pos, max_q, nread, dimer, gc, hp, qual );
    if ( KVectorGetU64 ( v, key, &(u.value) ) == 0 )
    {
        *total = u.counters.total;
        *mismatch = u.counters.mismatch;
    }
    else
    {
        *total = 0;
        *mismatch = 0;
    }
}
#endif

#endif

/******************************************************************************
    for the spot-group ( tree-node ), contains a tree of counter's
******************************************************************************/
static void CC whack_spotgroup( BSTNode *n, void *data )
{
    spotgrp * sg = ( spotgrp * )n;

#ifdef USE_JUDY
    KVectorRelease ( sg->v );
#else
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
#endif

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
#ifdef USE_JUDY
        else
        {
            KVectorMake ( &sg->v );
        }
#endif
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
                                    uint64_t *entries,
                                    const uint8_t quality,
                                    const uint8_t dimer_code,
                                    const uint8_t gc_content,
                                    const uint8_t hp_run,
                                    const uint8_t max_quality,
                                    const uint8_t n_read,
                                    const uint32_t cycle,
                                    const uint8_t rd_case,
                                    const uint64_t row_id )
{
    rc_t rc = 0;
    uint8_t q = quality;
    uint8_t d = dimer_code;
    uint8_t g = gc_content;
    uint8_t h = hp_run;
    uint8_t m = max_quality;
    uint8_t n = n_read;

#ifdef USE_JUDY
    bool mismatch;
#else
    counter_vector * cv;
#endif

    if ( q >= N_QUAL_VALUES ) q = ( N_QUAL_VALUES - 1 );
    if ( d >= N_DIMER_VALUES ) d = ( N_DIMER_VALUES - 1 );
    if ( g >= N_GC_VALUES ) g = ( N_GC_VALUES - 1 );
    if ( h >= N_HP_VALUES ) h = ( N_HP_VALUES - 1 );
    if ( m >= N_MAX_QUAL_VALUES ) m = ( N_MAX_QUAL_VALUES - 1 );
    if ( n >= N_READS ) n = ( N_READS - 1 );


#ifdef USE_JUDY
    mismatch = false;
    switch( rd_case )
    {
        case CASE_MISMATCH : mismatch = true; /* no break intented! */
        case CASE_MATCH    : if ( set_counter( spotgroup->v, cycle, m, n, d, g, h, q, mismatch ) )
                             {
                                (*entries)++;
                             }
                             break;
    }
#else
    cv = &( spotgroup->cnv[ m ][ n ][ d ][ g ][ h ][ q ] );

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
            case CASE_MATCH    : if ( cnt->count == 0 )
                                 {
                                    (*entries)++;
                                 }
                                 cnt->count++;
                                 break;
        }
    }
#endif
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


rc_t make_statistic( statistic *data,
                     uint32_t gc_window,
                     bool ignore_mismatches )
{
    rc_t rc = 0;
    memset( data, 0, sizeof *data );
    BSTreeInit( &data->spotgroups );
    data->last_used_spotgroup = NULL;
    data->ignore_mismatches = ignore_mismatches;
    data->gc_window = gc_window;
    if ( data->gc_window >= N_GC_VALUES )
    {
        data->gc_window = ( N_GC_VALUES - 1 );
    }
    if ( rc == 0 )
    {
        rc = make_vector( (void**)&data->case_vector, &data->case_vector_len, 512 );
    }
    return rc;
}


void whack_statistic( statistic *data )
{
    BSTreeWhack ( &data->spotgroups, whack_spotgroup, NULL );
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

#if 0
static rc_t walk_exclude_vector( statistic * data,
                                 uint32_t n_bases,
                                 row_input * row_data )
{
    rc_t rc = 0;

    if ( row_data->has_roffs_len != n_bases || 
         row_data->has_mismatch_len != n_bases )
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
            if ( row_data->has_roffs[ idx ] )
                hro_count++;
        }
        if ( hro_count != row_data->roffs_len )
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
        while ( base_idx > 0 && row_data->has_mismatch[ base_idx ] )
        {
            data->case_vector[ base_idx-- ] = CASE_IGNORE;
            n_bases--;
        }

        base_idx = 0;
        while ( ref_idx < (int32_t)row_data->exclude_len && base_idx < n_bases )
        {
            /* before we handle the reference, apply the ref-offset to the
               iterator ( in this case: ref_idx ) */
            if ( row_data->has_roffs[ base_idx ] )
            {
                int32_t roffs_value = row_data->roffs[ roffs_idx++ ];
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
            if ( row_data->exclude[ ref_idx++ ] > 0 )
            {
                data->case_vector[ base_idx ] = CASE_IGNORE;
            }
            else
            {
                /* enter the mis-matches into the case-vector */
                if ( row_data->has_mismatch[ base_idx ] )
                {
                    data->case_vector[ base_idx ] = CASE_MISMATCH;
                }
            }
            ++base_idx;
        }
    }
    return rc;
}
#endif

static rc_t loop_through_base_calls( spotgrp *sg,
                        uint64_t *entries,
                        uint32_t gc_window,
                        char * read_ptr,    /* points at begin of array */
                        uint32_t n_bases,
                        uint8_t * qual_ptr, /* points at begin of array */
                        uint8_t * case_ptr, /* points at begin of array */
                        uint32_t base_pos_offset,
                        uint8_t n_read,     /* the number of the read (0/1) */
                        const int64_t row_id,
                        const int32_t ofs )
{
    rc_t rc = 0;
    uint32_t base_pos;
    char prev_char;
    char * gc_ptr = read_ptr;
    uint8_t gc_content = 0;
    uint8_t hp_run = 0;
    uint8_t max_qual_value = 0;
    bool enter_value;
    uint8_t *saved_qual_ptr = qual_ptr;

    /* calculate the max. quality value, befor we loop through the bases a 2nd time */
    for ( base_pos = 0; base_pos < n_bases; ++base_pos )
    {
        if ( max_qual_value < *qual_ptr )
        {
            max_qual_value = *qual_ptr;
        }
        qual_ptr += ofs; /* because of going from forward or reverse */
    }
    /* restore qual_ptr */
    qual_ptr = saved_qual_ptr;

    prev_char = 'N';
    for ( base_pos = 0; base_pos < n_bases && rc == 0; ++base_pos )
    {
        /* calculate the hp-run-count */
        if ( prev_char == *read_ptr )
        {
            hp_run++;
            assert( hp_run <= n_bases );
        }
        else
        {
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
                case_value = case_ptr[ 1 ];
            }
            rc = spotgroup_enter_values( sg, entries,
                                         *qual_ptr,
                                         dimer_2_bin( prev_char, *read_ptr ),
                                         gc_content,
                                         hp_run,
                                         max_qual_value,
                                         n_read,
                                         base_pos + base_pos_offset,
                                         case_value,
                                         row_id );
        }

        /* handle the current base-position after the record was entered
           because we do not include the current base into the gc-content */
        if ( *read_ptr == 'G' || *read_ptr == 'C' )
            gc_content++;

        qual_ptr += ofs;
        prev_char = *read_ptr;
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
                                         row_input * row_data,
                                         const int64_t row_id )
{
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
    
#if 1
    if (rc == 0 && !data->ignore_mismatches) {
        unsigned si;
        unsigned ri;
        unsigned j;
        
        for (si = ri = j = 0; si < n_bases && ri < row_data->ref_len; ) {
            if (row_data->has_roffs[si]) {
                int const offset = row_data->roffs[j++];
                
                if (offset < 0) {
                    unsigned const n = -offset;
                    unsigned k;

                    /* set inserts to ignore (handles left soft clip) */
                    for (k = 0; k < n && si + k < n_bases; ++k)
                        data->case_vector[si + k] = CASE_IGNORE;
                    si += k;
                    continue;
                }
                ri += offset;
            }
            if (row_data->read[si] == 'N' || (row_data->exclude && row_data->exclude[ri]))
                data->case_vector[si] = CASE_IGNORE;
            else if (row_data->has_mismatch[si])
                data->case_vector[si] = CASE_MISMATCH;
            else
                data->case_vector[si] = CASE_MATCH;
            ++si;
            ++ri;
        }
        /* handle right soft clip */
        for ( ; si < n_bases; ++si)
            data->case_vector[si] = CASE_IGNORE;
    }
#else
    /* (2) ... get the exclusion-list for this read */
    if ( row_data->exclude != NULL )
    {
        /* (3) ... walk the exclusion-list and Mismatch-vector to build the case-vector */
        if ( rc == 0 )
        {
            rc = walk_exclude_vector( data, n_bases, row_data );
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
            uint32_t base_idx;
            
            for ( base_idx = 0;
                  base_idx < n_bases && base_idx < row_data->has_mismatch_len;
                  ++base_idx )
            {
                if ( row_data->has_mismatch[ base_idx ] )
                {
                    data->case_vector[ base_idx ] = CASE_MISMATCH;
                }
            }
            /* handle left soft clip */
            for ( base_idx = 0;
                 base_idx < n_bases && base_idx < row_data->has_mismatch_len;
                 ++base_idx )
            {
                if (!row_data->has_mismatch[base_idx]) {
                    break;
                }
                data->case_vector[base_idx] = CASE_IGNORE;
            }
            /* handle right soft clip */
            for ( base_idx = 0;
                 base_idx < n_bases && base_idx < row_data->has_mismatch_len;
                 ++base_idx )
            {
                if (!row_data->has_mismatch[row_data->has_mismatch_len - base_idx - 1]) {
                    break;
                }
                data->case_vector[row_data->has_mismatch_len - base_idx - 1] = CASE_IGNORE;
            }
        }
    }
#endif

    /* (4) ... looping throuhg the bases ( forward/backward ) */
    if ( rc == 0 )
    {
        uint8_t *qual_ptr = row_data->quality;
        if ( row_data->reversed )
        {
            uint8_t * loc_case_vector = data->case_vector + ( n_bases - 1 );
            qual_ptr += ( n_bases - 1 );
            rc = loop_through_base_calls( sg, &data->entries,
                    data->gc_window, row_data->read, n_bases, qual_ptr, loc_case_vector,
                    row_data->base_pos_offset, row_data->seq_read_id - 1, row_id, -1 );

        }
        else
        {
            rc = loop_through_base_calls( sg, &data->entries,
                    data->gc_window, row_data->read, n_bases, qual_ptr, data->case_vector, 
                    row_data->base_pos_offset, row_data->seq_read_id - 1, row_id, +1 );
        }
    }

    if ( rc == 0 )
    {
        if ( n_bases > data->max_cycle )
        {
            data->max_cycle = n_bases;
        }
    }

    return rc;
}


rc_t extract_statistic_from_row( statistic * data, 
                                 row_input * row_data,
                                 const int64_t row_id )
{
    rc_t rc = 0;
    spotgrp *sg;

    /* first try the SPOT_GROUP column (correct for newer db's) */
    char * spotgrp_base = row_data->spotgroup;
    uint32_t spotgrp_len = row_data->spotgroup_len;
    /* first try the SPOT_GROUP column (correct for newer db's) */
    if ( spotgrp_len < 1 || *spotgrp_base == 0 )
    {
        /* if empty try with SEQ_SPOT_GROUP column (correct for older db's) */
        spotgrp_base  = row_data->seq_spotgroup;
        spotgrp_len = row_data->seq_spotgroup_len;
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
        uint32_t n_bases  = row_data->read_len;

        if ( ( n_bases == row_data->quality_len ) &&
             ( n_bases == row_data->has_mismatch_len ) )
        {
            rc = extract_spotgroup_statistic( data, sg, n_bases, row_data, row_id );
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


typedef struct iter_ctx
{
    bool ( CC * f ) ( stat_row * row, void *data );
    void * data;
    const char * name;
    bool run;
    stat_row row;
    uint64_t n;
    uint32_t max_cycle;
} iter_ctx;


#ifdef USE_JUDY
static rc_t CC counter_visit( uint64_t key, uint64_t value, void * data )
{
    uint8_t q, dimer, gc, hp, mq, nr;
    uint32_t pos;
    iter_ctx *ctx = ( iter_ctx * )data;
    counter_union u;

    decode_key( key, &pos, &mq, &nr, &dimer, &gc, &hp, &q );
    u.value = value;

    ctx->row.dimer = (char *)dimer_2_ascii[ dimer ];
    ctx->row.quality = q;
    ctx->row.gc_content = gc;
    ctx->row.hp_run = hp;
    ctx->row.max_qual_value = mq;
    ctx->row.n_read = nr;
    ctx->row.base_pos = pos;
    ctx->row.count = u.counters.total;
    ctx->row.mismatch_count = u.counters.mismatch;

    ctx->run = ctx->f( &ctx->row, ctx->data );
    ctx->n++;
    return 0;
}
#endif


static bool CC spotgroup_iter( BSTNode *n, void *data )
{
    spotgrp *sg = ( spotgrp * ) n;
    iter_ctx *ctx = ( iter_ctx * )data;

#ifndef USE_JUDY
    uint8_t q, dimer, gc, hp, mq, nr;
    uint32_t pos;
#endif

    ctx->row.spotgroup = (char *)sg->name->addr;

#ifdef USE_JUDY
    KVectorVisitU64 ( sg->v, false, counter_visit, data );
#else
    for ( pos = 0; pos <= ctx->max_cycle; ++pos )
    {
        for ( nr = 0; nr < N_READS; ++nr )
        {
            for ( dimer = 0; dimer < N_DIMER_VALUES; ++dimer )
            {
                for( gc = 0; gc < N_GC_VALUES; ++gc )
                {
                    for ( hp = 0; hp < N_HP_VALUES; ++hp )
                    {
                        for ( mq = 0; mq < N_MAX_QUAL_VALUES; ++mq )
                        {
                            for ( q = 0; q < N_QUAL_VALUES; ++q )
                            {
                                counter_vector * cv = &sg->cnv[ mq ][ nr ][ dimer ][ gc ][ hp ][ q ];
                                if ( cv != NULL )
                                {
                                    if ( pos < cv->n_counters )
                                    {
                                        counter * c = &cv->v[ pos ];
                                        if ( c->count > 0 )
                                        {
                                            ctx->row.dimer = (char *)dimer_2_ascii[ dimer ];
                                            ctx->row.quality = q;
                                            ctx->row.gc_content = gc;
                                            ctx->row.hp_run = hp;
                                            ctx->row.max_qual_value = mq;
                                            ctx->row.n_read = nr;
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
                }
            }
        }
    }
#endif

    return( !ctx->run );
}


uint64_t foreach_statistic( statistic * data,
    bool ( CC * f ) ( stat_row * row, void * f_data ), void *f_data )
{
    iter_ctx ctx;
    ctx.n = 0;
    if ( f != NULL )
    {
        ctx.f = f;
        ctx.data = f_data;
        ctx.max_cycle = data->max_cycle;
        ctx.run = true;
        BSTreeDoUntil ( &data->spotgroups, false, spotgroup_iter, &ctx );
    }
    return ctx.n;
}
