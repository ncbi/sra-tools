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

#include "pl-regions.h"
#include <insdc/sra.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

enum
{
    pacbio_idx_spot_id = 0,
    pacbio_idx_type,
    pacbio_idx_start,
    pacbio_idx_end,
    pacbio_idx_score
};


/* HDF5-Groups and tables used to use the REGIONS-table */
static const char * region_groups_to_check[] = 
{ 
    "PulseData",
    NULL
};

static const char * region_tables_to_check[] = 
{ 
    "PulseData/Regions",
    NULL
};


void rgn_stat_init( regions_stat * stat )
{
    stat->inserts = 0;
    stat->expands_a = 0;
    stat->expands_i = 0;
    stat->inserts_spots = 0;
    stat->expands_spots = 0;
    stat->end_gap = 0;
    stat->overlapps = 0;
    stat->removed = 0;
}


void rgn_init( regions *rgn )
{
    init_array_file( &rgn->hdf5_regions );

    VectorInit ( &rgn->read_Regions, 0, 5 );
    VectorInit ( &rgn->sort_Regions, 0, 5 );
    VectorInit ( &rgn->stock_Regions, 0, 5 );

    rgn->data_32 = NULL;
    rgn->data_32_len = 0;
    rgn->data_8 = NULL;
    rgn->data_8_len = 0;

    rgn->offset = 0;
    rgn->spot_id = 0;
    rgn->spot_len = 0;
    rgn->hq_rgn.start = 0;
    rgn->hq_rgn.end = 0;

    rgn_stat_init( &( rgn->stat ) );

    rgn->complete_table = NULL;
    rgn->table_index = NULL;
}


static void CC region_whack( void * item, void * data )
{
    free( item );
}


static rc_t rgn_vector_move( Vector * src, Vector * dst )
{
    rc_t rc = 0;
    while ( VectorLength( src ) > 0 && rc == 0 )
    {
        region *ptr;
        rc = VectorRemove ( src, 0, (void**)&ptr );
        if ( rc == 0 )
            rc = VectorAppend ( dst, NULL, ptr );
    }
    return rc;
}


void rgn_free( regions *rgn )
{
    free_array_file( &rgn->hdf5_regions );

    VectorWhack ( &rgn->read_Regions, region_whack, NULL );
    VectorWhack ( &rgn->sort_Regions, region_whack, NULL );
    VectorWhack ( &rgn->stock_Regions, region_whack, NULL );

    if ( rgn->data_32 != NULL )
        free( rgn->data_32 );
    if ( rgn->data_8 != NULL )
        free( rgn->data_8 );

    if ( rgn->complete_table != NULL )
        free( rgn->complete_table );
    if ( rgn->table_index != NULL )
        free( rgn->table_index );
}


static
int CC rgn_sort_callback( const void *p1, const void *p2, void * data )
{
    regions *rgn = ( regions * ) data;
    int32_t idx1 = *( int32_t * ) p1;
    int32_t idx2 = *( int32_t * ) p2;
    int32_t spot_id1 = rgn->complete_table[ idx1 * RGN_COLUMN_COUNT ];
    int32_t spot_id2 = rgn->complete_table[ idx2 * RGN_COLUMN_COUNT ];
    if ( spot_id1 == spot_id2 )
        return ( idx1 - idx2 );
    return ( spot_id1 - spot_id2 );
}


static
rc_t rgn_read_complete_table( regions *rgn )
{
    rc_t rc;
    uint32_t rowcount = rgn->hdf5_regions.extents[ 0 ];
    uint32_t rowsize = sizeof( int32_t ) * RGN_COLUMN_COUNT;

    rgn->complete_table = malloc( rowcount * rowsize );
    if ( rgn->complete_table == NULL )
        rc = RC( rcExe, rcNoTarg, rcLoading, rcMemory, rcExhausted );
    else
    {
        rgn->table_index = malloc( sizeof( uint32_t ) * rowcount );
        if ( rgn->table_index == NULL )
        {
            free( rgn->complete_table );
            rgn->complete_table = NULL;
            rc = RC( rcExe, rcNoTarg, rcLoading, rcMemory, rcExhausted );
        }
        else
        {
            uint64_t n_read = 0;

            /* now let's read the whole table... */
            rc = array_file_read_dim2( &(rgn->hdf5_regions), 0, rgn->complete_table,
                                       rowcount, RGN_COLUMN_COUNT, &n_read );
            if ( rc == 0 )
            {
                uint32_t idx, first_spot_id;

                first_spot_id = rgn->complete_table[ pacbio_idx_spot_id ];
                if ( first_spot_id != 0 )
                {
                    /* in case the file we are loading is part of a multi-file submission */
                    for ( idx = 0; idx < rowcount; ++idx )
                        rgn->complete_table[ ( idx * RGN_COLUMN_COUNT ) + pacbio_idx_spot_id ] -= first_spot_id;
                }
                
                /* first let's fill the index, first with ascending row-id's */
                for ( idx = 0; idx < rowcount; ++idx )
                    rgn->table_index[ idx ] = idx;

                /* now sort the index-array by the content's spot-id's */
                ksort ( rgn->table_index, rowcount, sizeof( uint32_t ),
                        rgn_sort_callback, rgn );
                
                /* left here to print a debug-output of the sorted table-index */
                /*
                for ( idx = rowcount - 128; idx < rowcount; ++idx )
                    OUTMSG(( "idx[%i] = %i -> %i\n", 
                             idx, rgn->table_index[ idx ], 
                             rgn->complete_table[ rgn->table_index[ idx ] * RGN_COLUMN_COUNT ] ));
                */

                /* the table and the index is now ready to use... */
            }
        }
    }
    return rc;
}


rc_t rgn_open( const KDirectory *hdf5_dir, regions *rgn )
{
    rc_t rc;
    rgn_init( rgn );
    /* check if the necessary groups/tables are there */
    rc = check_src_objects( hdf5_dir, region_groups_to_check, 
                            region_tables_to_check, false );
    if ( rc == 0 )
    {
        /* open the region-table... */
        rc = open_element( hdf5_dir, &rgn->hdf5_regions,
                           region_groups_to_check[ 0 ], "Regions", 
                           REGIONS_BITSIZE, REGIONS_COLS, true, false, true );
        if ( rc == 0 )
            rc = rgn_read_complete_table( rgn );
    }
    if ( rc != 0 )
        rgn_free( rgn );
    return rc;
}


static rc_t rgn_get_or_make( Vector * stock, region ** r )
{
    rc_t rc = 0;
    /* take it out of the stock or make a new one... */
    if ( VectorLength( stock ) > 0 )
        rc = VectorRemove ( stock, 0, (void**)r );
    else
        *r = malloc( sizeof ** r );

    if ( *r == NULL )
        rc = RC( rcExe, rcNoTarg, rcLoading, rcMemory, rcExhausted );
    return rc;
}


static int CC rgn_sort_by_start( const void *item, const void *n )
{
    region * v1 = ( region * )item;
    region * v2 = ( region * )n;
    if ( v1 -> start != v2 -> start )
        return ( v1->start - v2->start );
    return ( v1->end - v2->end );
}


static rc_t rgn_store_bio_or_adapter( Vector * stock, Vector * to,
                                      const int32_t * block, int32_t sra_read_type )
{
    rc_t rc = 0;

    if ( block[ pacbio_idx_start ] < block[ pacbio_idx_end ] )
    {
        region * a_region;
        rc = rgn_get_or_make( stock, &a_region );
        if ( rc == 0 )
        {
            a_region->spot_id = block[ pacbio_idx_spot_id ];
            a_region->type    = sra_read_type;
            a_region->start   = block[ pacbio_idx_start ];
            a_region->end     = block[ pacbio_idx_end ];

            /* see every region shorter as MIN_BIOLOGICAL_LEN 
               as a technical (adapter) region */
            if ( ( a_region->end - a_region->start ) <= MIN_BIOLOGICAL_LEN )
                a_region->type = SRA_READ_TYPE_TECHNICAL;

            a_region->filter  = SRA_READ_FILTER_PASS;
            rc = VectorInsert ( to, a_region, NULL, rgn_sort_by_start );
        }
    }
    return rc;
}

static rc_t rgn_store_block( Vector * stock, Vector * to, hq_region * hq,
                             const int32_t * block, region_type_mapping *mapping,
                             bool *have_high_quality )
{
    rc_t rc = 0;
    int32_t type = block[ pacbio_idx_type ];

    if ( mapping->rgn_type_hq >=0 && type == mapping->rgn_type_hq )
    {
        /* it is an error if we have more than one high-quality-region! */
        assert ( ! * have_high_quality );

        if ( * have_high_quality )
        {
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
            LOGERR( klogErr, rc, "(* have_high_quality) in rgn_store_block()'" );
            return rc;
        }

        hq->start = block[ pacbio_idx_start ];
        hq->end   = block[ pacbio_idx_end ];
        * have_high_quality = true;
    }
    else if ( mapping->rgn_type_ga >= 0 && type == mapping->rgn_type_ga )
    {   /* so far do nothing with the "global accuracy" region */
    }
    else if ( mapping->rgn_type_adapter >= 0 && type == mapping->rgn_type_adapter )
    {   /* it is an adapter */
        rc = rgn_store_bio_or_adapter( stock, to, block, SRA_READ_TYPE_TECHNICAL );
    }
    else if ( mapping->rgn_type_insert >= 0 && type == mapping->rgn_type_insert )
    {   /* it is an insert */
        rc = rgn_store_bio_or_adapter( stock, to, block, SRA_READ_TYPE_BIOLOGICAL );
    }
    else
    {   /* the type is unknown */
        ( mapping->count_of_unknown_rgn_types )++;
        /*
        rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
        LOGERR( klogErr, rc, "( region type is unknown ) in rgn_store_block()'" );
        */
    }
    return rc;
}


/* inserts the generated region into the sort_Regions */
static rc_t rgn_generate( Vector * stock, Vector * dst,
                          const int32_t spot_id, const uint32_t start, const uint32_t len )
{
    region * a_region;
    rc_t rc = rgn_get_or_make( stock, &a_region );
    if ( rc == 0 )
    {
        a_region->spot_id = spot_id;
        a_region->type    = SRA_READ_TYPE_TECHNICAL; /*means "i dont know"*/
        a_region->start   = start;
        a_region->end     = start + len;
        a_region->filter  = SRA_READ_FILTER_CRITERIA;
        rc = VectorInsert ( dst, a_region, NULL, rgn_sort_by_start );
    }
    return rc;
}


/* *************************************************************
declares all regions inside and touching the HQ-Regions as
    SRA_READ_FILTER_PASS outside becomes SRA_READ_FILTER_CRITERIA;
************************************************************* */
#if 0
static rc_t rgn_apply_filter( Vector * stock, Vector * v, hq_region * hq,
                              const int32_t spot_id, const uint32_t spot_len )
{
    rc_t rc = 0;

    if ( hq->start == 0 && hq->end == 0 )
    {
        /* we have no HQ-Region, discard everything and create one
           READ for the whole spot, that will be TECHNICAL... */
        rgn_vector_move( v, stock );

        if ( spot_len > 0 )
            /* inserts the generated region into the sort_Regions */
            rc = rgn_generate( stock, v, spot_id, 0, spot_len );
    }
    else
    {
        uint32_t i, count = VectorLength ( v );
        for ( i = 0; i < count; ++ i )
        {
            region * a_region = VectorGet ( v, i );
            if ( a_region != NULL )
            {
                bool set_invalid = ( ( a_region->end <= hq->start ) ||
                                     ( a_region->start >= hq->end ) );
                if ( set_invalid )
                {
                    /* the region is before the hq-region
                       ---> set to SRA_READ_FILTER_CRITERIA */
                    a_region->filter = SRA_READ_FILTER_CRITERIA;
                    a_region->type = SRA_READ_TYPE_TECHNICAL;
                }
                else
                {
                    /* the region intersects with the hq-region 
                       ---> set to SRA_READ_FILTER_PASS */
                    a_region->filter = SRA_READ_FILTER_PASS;
                }
            }
        }
    }
    return rc;
}
#endif

static bool rgn_expand_last_rgn_by_1( Vector * v, int32_t *expands_a, int32_t *expands_i )
{
    region * a_region = VectorLast ( v );
    bool res = ( a_region != NULL );
    if ( res )
    {
        a_region->end += 1;
        if ( a_region->type == SRA_READ_TYPE_TECHNICAL )
            (*expands_a)++;
        else
            (*expands_i)++;
    }
    return res;
}


/* *************************************************************
if gap is 1, expand previous region by 1 (correct off-by-1)
fill in gaps > 1 ( regions are not consecutive )
correct overlapping regions
fill in a gap at the end, if the last region does not reach spotlen

    INTPUT : rgn->read_Regions
    OUTPUT : rgn->sort_Regions
************************************************************* */
static rc_t rgn_correct( Vector * stock, Vector * from, Vector * to,
                         const uint32_t spot_id, const uint32_t spot_len,
                         regions_stat * stats )
{
    rc_t rc;
    int32_t start, expands_a = 0, expands_i = 0, inserts = 0;
    uint32_t i, count = VectorLength ( from );

    for ( rc = 0, start = 0, i = 0; i < count && rc == 0; ++ i )
    {
        region * a_region;

        /* take the region out of rgn->read_Regions*/
        rc = VectorRemove ( from, 0, (void**)&a_region );
        if ( rc == 0 )
        {
            int32_t gap_len = ( a_region->start - start );

            if ( gap_len == 1 )
            {
                /* the gap-length is one, try to expand the previous region */
                if ( !rgn_expand_last_rgn_by_1( to, &expands_a, &expands_i ) )
                {
                    /* there is no previous region ! */
                    rc = rgn_generate( stock, to, spot_id, start, gap_len );
                }
            }
            else if ( gap_len > 1 )
            {
                /* generate a artificial gap in the middle or the start of the spot */
                rc = rgn_generate( stock, to, spot_id, start, gap_len );
                inserts++;
            }
            else if ( gap_len < 0 )
            {
                /* a negative gap would be an error in the sorting of the regions,
                   or an overlapp of regions */
                if ( ( a_region->start - gap_len )  > a_region->end )
                {
                    rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
                    LOGERR( klogErr, rc, "((a_region->start-gap_len)>a_region->end) in rgn_correct()'" );
                }
                else /** move the start point ***/
                {
                    a_region->start -= gap_len;
                    stats->overlapps++;
                }
            }
            
            if ( rc == 0 )
            {
                rc = VectorInsert ( to, a_region, NULL, rgn_sort_by_start );
                if ( rc != 0 )
                    LOGERR( klogErr, rc, "VectorInsert(rgn_sort_by_start) in rgn_correct()'" );
                start = a_region->end;
            }
            else
            {
                rc_t rc1 = VectorInsert( stock, a_region, NULL, NULL );
                if ( rc1 != 0 )
                    LOGERR( klogErr, rc, "VectorInsert(NULL) in rgn_correct()'" );
            }
        }
    }

    if ( rc == 0 )
    {
        /* if the last region does not reach to the end of the spot */
        if ( start < spot_len )
        {
            int32_t gap_len = ( spot_len - start );
            if ( gap_len == 1 )
            {
                /* !!! this can also happen if spot_len == 1 !!! */
                /* the gap-length is one, try to expand the previous region */
                if ( ! rgn_expand_last_rgn_by_1( to, &expands_a, &expands_i ) )
                {
                    /* there is no previous region ! */
                    rc = rgn_generate( stock, to, spot_id, start, gap_len );
                    stats->end_gap++;
                }
            }
            else if ( gap_len > 0 )
            {
                /* fill the gap to the end... */
                rc = rgn_generate( stock, to, spot_id, start, gap_len );
                stats->end_gap++;
            }
            else if ( gap_len < 0 )
            {
                /* a negative gap would be an error in the sorting of the regions,
                   or an overlapp of regions */
                rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
                LOGERR( klogErr, rc, "(gap_len<0) in rgn_correct()'" );
            }
        }
    }

    /* do some statistics */
    if ( ( expands_i + expands_a ) > 0 )
    {
        stats->expands_a += expands_a;
        stats->expands_i += expands_i;
        stats->expands_spots++;
    }
    if ( inserts > 0 )
    {
        stats->inserts += inserts;
        stats->inserts_spots++;
    }

    return rc;
}


/* *************************************************************
tries to merge overlapping adapter-regions...
( uses rgn->read_Regions as scratch-pad )

    INTPUT : rgn->sort_Regions
    OUTPUT : rgn->sort_Regions
************************************************************* */
static rc_t rgn_merge_consecutive_regions( Vector * stock, Vector * from, Vector * to )
{
    rc_t rc = 0;
    uint32_t i, count = VectorLength ( from );
    region * a_region = NULL;
    region * prev_region = NULL;

    for ( i = 0; i < count && rc == 0; ++ i )
    {
        /* take the region out of rgn->sort_Regions*/
        rc = VectorRemove ( from, 0, (void**)&a_region );
        if ( rc == 0 )
        {
            bool copy = true;
            if ( prev_region != NULL )
            {
                if ( ( a_region->start <= prev_region->end ) &&
                     ( a_region->type == prev_region->type ) &&
                     ( a_region->filter == prev_region->filter ) )
                {
                    prev_region->end = a_region->end;
                    /* put the now unused region back into the stock */
                    VectorAppend ( stock, NULL, a_region );
                    /* and keep prev-region! */
                    copy = false;
                }
            }
            if ( copy )
            {
                prev_region = a_region;
                rc = VectorInsert ( to, a_region, NULL, rgn_sort_by_start );
            }
        }
    }
    return rc;
}

/* *************************************************************
tests that the spot-len is covered with regions that
are not overlapping, have no gaps, start is ascending
the regions to check are in rgn->sort_Regions
************************************************************* */
static rc_t rgn_check( const Vector * v, const uint32_t spot_len )
{
    rc_t rc = 0;
    uint32_t i, count = VectorLength ( v );
    region * a_region = NULL;
    int32_t start = 0;

    /* special case, if the spot is empty there are not regions */
    if ( spot_len == 0 )
    {
        if ( count != 0 )
        {
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
            LOGERR( klogErr, rc, "(spot_len == 0)&&(count!=0) in region-check()'" );
        }
        return rc;
    }

    /* check that we have at least one region in the spot */
    if ( count < 1 )
    {
        rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
        LOGERR( klogErr, rc, "(count<1) in region-check()'" );
    }

    for ( i = 0; i < count && rc == 0; ++ i )
    {
        a_region = VectorGet ( v, i );
        if ( a_region == NULL )
        {
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
            LOGERR( klogErr, rc, "(a_region==NULL) in region-check()'" );
        }
        else
        {
            /* check that the region has no gap and is not overlapping */
            if ( a_region->start != start )
            {
                rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
                PLOGERR( klogErr, ( klogErr, rc, "(a_region->start($(rstart))!=start($(start))) in region-check()'",
                         "rstart=%u,start=%u", a_region->start, start ) );
            }
            else
            {
                /* check that the region is ascending */
                if ( a_region->end < start )
                {
                    rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
                    LOGERR( klogErr, rc, "(a_region->end<start) in region-check()'" );
                }
                start = a_region->end;
            }
        }
    }

    if ( rc == 0 )
    {
        /* check that the region is fully covered */
        if ( a_region->end != spot_len )
        {
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
            LOGERR( klogErr, rc, "(a_region->end!=spot_len) in region-check()'" );
        }
    }
    return rc;
}


rc_t rgn_load( regions *rgn, const uint32_t spot_id, 
               region_type_mapping *mapping, const uint32_t spot_len )
{
    rc_t rc;
    uint64_t row_count = rgn->hdf5_regions.extents[ 0 ];

    /* predefine that we have no HQ-regions read */
    rgn->hq_rgn.start = 0;
    rgn->hq_rgn.end = 0;

    /* clear out the read and the sorted vector */
    rc = rgn_vector_move( &rgn->read_Regions, &rgn->stock_Regions );
    if ( rc == 0 )
        rc = rgn_vector_move( &rgn->sort_Regions, &rgn->stock_Regions );

    if ( rc == 0 )
    {
        if ( !( rgn->spot_id == 0 || rgn->spot_id == ( spot_id - 1 ) ) )
            rc = RC( rcExe, rcNoTarg, rcLoading, rcParam, rcInvalid );
    }

    if ( rc == 0 )
    {
        int32_t * block;
        bool have_high_quality = false;
        do
        {
            int32_t idx = rgn->table_index[ rgn->offset ];
            block = &( rgn->complete_table[ idx * RGN_COLUMN_COUNT ] );
            if ( block[ pacbio_idx_spot_id ] == spot_id )
            {
                rc = rgn_store_block( &(rgn->stock_Regions), &(rgn->read_Regions),
                                      &(rgn->hq_rgn), block,
                                      mapping, & have_high_quality );
                if ( rc == 0 )
                    rgn->offset++;
            }

        } while( rc == 0 && 
                 block[ pacbio_idx_spot_id ] == spot_id &&
                 rgn->offset < row_count );
        rgn->spot_id  = spot_id;
        rgn->spot_len = spot_len;
    }

    if ( rc == 0 )
    {
#if 0  /**** does not seem to match PacBio ***/
        /* changes READ_FILTER and READ_TYPE if a region is completely
           outside of the hq-region... ( if we have one ) 
           if there is none the whole spot becomes one CRITERIA/TECHNICAL-read */
        rc = rgn_apply_filter( &(rgn->stock_Regions), &(rgn->read_Regions),
                               &(rgn->hq_rgn), rgn->spot_id, rgn->spot_len );
#endif


        /* INPUT : rgn->read_Regions / OUTPUT : rgn->sort_Regions */
        /* fills gaps, corrects off-by-1-errors and overlapping regions */
        if ( rc == 0 )
            rc = rgn_correct( &(rgn->stock_Regions), 
                              &(rgn->read_Regions), &(rgn->sort_Regions), 
                              rgn->spot_id, rgn->spot_len, &(rgn->stat) );

        /* INPUT : rgn->sort_Regions / OUTPUT : rgn->read_Regions */
        /* merges consecutive regions if READ_TYPE/READ_FILTER are the same */
        if ( rc == 0 )
            rc = rgn_merge_consecutive_regions( &(rgn->stock_Regions),
                                                &(rgn->sort_Regions),
                                                &(rgn->read_Regions) );

        /* INPUT : rgn->read_Regions */
        /* checks that the whole spot is covered, no overlapps/gaps occur,
           regions have to be sorted in ascending order */

        if ( rc == 0 )
            rc = rgn_check( &(rgn->read_Regions), rgn->spot_len );

    }
    return rc;
}


static rc_t rgn_resize_data_32( regions *rgn )
{
    rc_t rc = 0;
    size_t needed_len;

    needed_len = ( sizeof( *rgn->data_32 ) * VectorLength( &rgn->read_Regions ) );
    if ( rgn->data_32 == NULL )
    {
        rgn->data_32 = malloc( needed_len );
    }
    else if ( rgn->data_32_len < VectorLength( &rgn->read_Regions ) )
    {
        rgn->data_32 = realloc ( rgn->data_32, needed_len );
    }
    if ( rgn->data_32 == NULL )
        rc = RC( rcExe, rcNoTarg, rcLoading, rcMemory, rcExhausted );
    else
        rgn->data_32_len = VectorLength( &rgn->read_Regions );
    return rc;
}


void rgn_set_filter_value_for_all( regions *rgn, const uint32_t filter_value )
{
    uint32_t i, n = VectorLength( &rgn->read_Regions );
    for ( i = 0; i < n; ++i )
    {
        region * a_region = VectorGet ( &rgn->read_Regions, i );
        if ( a_region != NULL )
            a_region->filter = filter_value;
    }
}


static rc_t rgn_resize_data_8( regions *rgn )
{
    rc_t rc = 0;
    size_t needed_len;

    needed_len = ( sizeof( *rgn->data_8 ) * VectorLength( &rgn->read_Regions ) );
    needed_len = (needed_len + 3 ) & ~3; /** to make valgrind happy ***/
    if ( rgn->data_8 == NULL )
    {
        rgn->data_8 = malloc( needed_len );
    }
    else if ( rgn->data_8_len < VectorLength( &rgn->read_Regions ) )
    {
        rgn->data_8 = realloc ( rgn->data_8, needed_len );
    }
    if ( rgn->data_8 == NULL )
        rc = RC( rcExe, rcNoTarg, rcLoading, rcMemory, rcExhausted );
    else
        rgn->data_8_len = VectorLength( &rgn->read_Regions );
    return rc;
}


rc_t rgn_start_data( regions *rgn, uint32_t *count )
{
    rc_t rc = rgn_resize_data_32( rgn );
    if ( rc == 0 )
    {
        uint32_t i;
        uint32_t *ptr = rgn->data_32;
        *count = VectorLength( &rgn->read_Regions );
        for ( i = 0; i < (*count); ++i )
        {
            region * a_region = VectorGet ( &rgn->read_Regions, i );
            if ( a_region != NULL )
                ptr[ i ] = a_region->start;
        }
    }
    return rc;
}


rc_t rgn_len_data( regions *rgn, uint32_t *count )
{
    rc_t rc = rgn_resize_data_32( rgn );
    if ( rc == 0 )
    {
        uint32_t i;
        uint32_t *ptr = rgn->data_32;
        *count = VectorLength( &rgn->read_Regions );
        for ( i = 0; i < (*count); ++i )
        {
            region * a_region = VectorGet ( &rgn->read_Regions, i );
            if ( a_region != NULL )
                ptr[ i ] = ( a_region->end - a_region->start );
        }
    }
    return rc;
}


rc_t rgn_type_data( regions *rgn, uint32_t *count )
{
    rc_t rc = rgn_resize_data_8( rgn );
    if ( rc == 0 )
    {
        uint32_t i;
        uint8_t *ptr = rgn->data_8;

        *count = VectorLength( &rgn->read_Regions );
        for ( i = 0; i < (*count); ++i )
        {
            region * a_region = VectorGet ( &rgn->read_Regions, i );
            if ( a_region != NULL )
                ptr[ i ] = a_region->type;
        }
    }
    return rc;
}


rc_t rgn_filter_data( regions *rgn, uint32_t *count )
{
    rc_t rc = rgn_resize_data_8( rgn );
    if ( rc == 0 )
    {
        uint32_t i;
        uint8_t *ptr = rgn->data_8;

        *count = VectorLength( &rgn->read_Regions );
        for ( i = 0; i < (*count); ++i )
        {
            region * a_region = VectorGet ( &rgn->read_Regions, i );
            if ( a_region != NULL )
                ptr[ i ] = a_region->filter;
        }
    }
    return rc;
}


rc_t rgn_label_start_data( regions *rgn, uint32_t *count )
{
    rc_t rc = rgn_resize_data_32( rgn );
    if ( rc == 0 )
    {
        uint32_t i;
        uint32_t *ptr = rgn->data_32;
        uint32_t value;

        *count = VectorLength( &rgn->read_Regions );
        for ( i = 0; i < (*count); ++i )
        {
            region * a_region = VectorGet ( &rgn->read_Regions, i );
            value = label_lowquality_start; /* default value */
            if ( a_region != NULL )
                switch( a_region->type )
                {
                case SRA_READ_TYPE_BIOLOGICAL : 
                    value = label_insert_start;
                    break;
                case SRA_READ_TYPE_TECHNICAL  :
                    value = label_adapter_start;
/*
                    if ( a_region->filter == SRA_READ_FILTER_PASS )
                        value = label_adapter_start;
                    else
                        value = label_lowquality_start;
*/
                    break;
                }
            ptr[ i ] = value;
        }
    }
    return rc;
}


rc_t rgn_label_len_data( regions *rgn, uint32_t *count )
{
    rc_t rc = rgn_resize_data_32( rgn );
    if ( rc == 0 )
    {
        uint32_t i;
        uint32_t *ptr = rgn->data_32;
        uint32_t value;

        *count = VectorLength( &rgn->read_Regions );
        for ( i = 0; i < (*count); ++i )
        {
            region * a_region = VectorGet ( &rgn->read_Regions, i );
            value = label_lowquality_len; /* default value */
            if ( a_region != NULL )
                switch ( a_region->type )
                {
                case SRA_READ_TYPE_BIOLOGICAL : 
                    value = label_insert_len;
                    break;
                case SRA_READ_TYPE_TECHNICAL  : 
                    value = label_adapter_len;
/*
                    if ( a_region->filter == SRA_READ_FILTER_PASS )
                        value = label_adapter_len;
                    else
                        value = label_lowquality_len;
*/
                    break;
                }
            ptr[ i ] = value;
        }
    }
    return rc;
}


static const char rgn_string_adapter[] = "Adapter";
static const char rgn_string_insert[] = "Insert";
static const char rgn_string_hq[] = "HQRegion";
static const char rgn_string_ga[] = "GlobalAccuracy";


static int rgn_str_cmp( const char *a, const char *b )
{
    size_t asize = string_size ( a );
    size_t bsize = string_size ( b );
    return strcase_cmp ( a, asize, b, bsize, ( asize > bsize ) ? asize : bsize );
}


static rc_t rgn_set_type_code( int32_t *dst, const uint32_t type_idx )
{
    if ( *dst == -1 )
    {
        *dst = type_idx;
        return 0;
    }
    else
        return RC( rcExe, rcNoTarg, rcLoading, rcName, rcDuplicate );
}


static rc_t rgn_type_string( const char *type_string, uint32_t type_idx, 
                             region_type_mapping *mapping )
{
    if ( rgn_str_cmp( type_string, rgn_string_adapter ) == 0 )
        return rgn_set_type_code( &(mapping->rgn_type_adapter), type_idx );

    if ( rgn_str_cmp( type_string, rgn_string_insert ) == 0 )
        return rgn_set_type_code( &(mapping->rgn_type_insert), type_idx );

    if ( rgn_str_cmp( type_string, rgn_string_hq ) == 0 )
        return rgn_set_type_code( &(mapping->rgn_type_hq), type_idx );

    if ( rgn_str_cmp( type_string, rgn_string_ga ) == 0 )
        return rgn_set_type_code( &(mapping->rgn_type_ga), type_idx );

    return RC( rcExe, rcNoTarg, rcLoading, rcName, rcUnknown );
}


/* read the mapping out of the region-types out of a string... */
rc_t rgn_extract_type_mappings( const KNamelist *rgn_names,
                                region_type_mapping *mapping, bool check_completenes )
{
    rc_t rc = 0;
    uint32_t count, idx;

    mapping->rgn_type_adapter = -1;
    mapping->rgn_type_insert  = -1;
    mapping->rgn_type_hq      = -1;
    mapping->rgn_type_ga      = -1;

    rc = KNamelistCount ( rgn_names, &count );
    for ( idx = 0; idx < count && rc == 0; ++idx )
    {
        const char *name;
        rc = KNamelistGet ( rgn_names, idx, &name );
        if ( rc == 0 )
            rgn_type_string( name, idx, mapping );
    }

    if ( rc == 0 && check_completenes )
    {
        if ( mapping->rgn_type_adapter == -1 ||
             mapping->rgn_type_insert == -1 ||
             mapping->rgn_type_hq == -1 )
            rc = RC( rcExe, rcNoTarg, rcLoading, rcName, rcIncomplete );
    }

    mapping->count_of_unknown_rgn_types = 0;
    return rc;
}



rc_t rgn_show_type_mappings( region_type_mapping *mapping )
{
    rc_t rc;
    if ( mapping == NULL )
        rc = RC( rcExe, rcNoTarg, rcLoading, rcParam, rcInvalid );
    else
        rc = KOutMsg( "rgn-type-mapping->adapter   = %i\n", mapping->rgn_type_adapter );
    if ( rc == 0 )
        rc = KOutMsg( "rgn-type-mapping->insert    = %i\n", mapping->rgn_type_insert );
    if ( rc == 0 )
        rc = KOutMsg( "rgn-type-mapping->high_qual = %i\n", mapping->rgn_type_hq );
    if ( rc == 0 )
        rc = KOutMsg( "rgn-type-mapping->globe_acc = %i\n", mapping->rgn_type_ga );
    return rc;
}

