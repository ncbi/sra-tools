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
#ifndef _h_pl_regions_
#define _h_pl_regions_

#ifdef __cplusplus
extern "C" {
#endif

#include "pl-tools.h"
#include <klib/vector.h>
#include <klib/sort.h>
#include <klib/rc.h>
#include <insdc/sra.h>


#define RGN_COLUMN_COUNT 5
#define MIN_BIOLOGICAL_LEN 10

typedef struct region_type_mapping
{
    int32_t rgn_type_adapter;   /* technical */
    int32_t rgn_type_insert;    /* biological */
    int32_t rgn_type_hq;        /* HighQualityRegion */
    int32_t rgn_type_ga;        /* GlobalAccuracy ??? */

    uint64_t count_of_unknown_rgn_types;
} region_type_mapping;


typedef struct region
{
    int32_t spot_id;
    int32_t type;
    int32_t start;
    int32_t end;
    int32_t filter;
} region;


typedef struct regions_stat
{
    uint32_t inserts;
    uint32_t inserts_spots;
    uint32_t expands_a;
    uint32_t expands_i;
    uint32_t expands_spots;
    uint32_t end_gap;
    uint32_t overlapps;
    uint32_t removed;
} regions_stat;


typedef struct hq_region
{
    uint32_t start;
    uint32_t end;
} hq_region;


typedef struct regions
{
    af_data hdf5_regions;
    Vector read_Regions;
    Vector sort_Regions;
    Vector stock_Regions;
    hq_region hq_rgn;
    uint64_t offset;
    uint32_t spot_id;
    uint32_t spot_len;
    uint32_t * data_32;
    uint8_t * data_8;
    size_t data_32_len;
    size_t data_8_len;

    regions_stat stat;

    int32_t * complete_table;
    int32_t * table_index;
} regions;


void rgn_init( regions *rgn );
void rgn_free( regions *rgn );

static const char def_label[] = "AdapterInsertLowQuality";
static const size_t def_label_len = 23;

static const uint32_t label_adapter_start    = 0;
static const uint32_t label_adapter_len      = 7;
static const uint32_t label_insert_start     = 7;
static const uint32_t label_insert_len       = 6;
static const uint32_t label_lowquality_start = 13;
static const uint32_t label_lowquality_len   = 10;

rc_t rgn_open( const KDirectory *hdf5_dir, regions *rgn );

rc_t rgn_load( regions *rgn, const uint32_t spot_id, 
               region_type_mapping *mapping, const uint32_t spot_len );

void rgn_set_filter_value_for_all( regions *rgn, const uint32_t filter_value );

rc_t rgn_start_data( regions *rgn, uint32_t *count );
rc_t rgn_len_data( regions *rgn, uint32_t *count );
rc_t rgn_type_data( regions *rgn, uint32_t *count );
rc_t rgn_filter_data( regions *rgn, uint32_t *count );
rc_t rgn_label_start_data( regions *rgn, uint32_t *count );
rc_t rgn_label_len_data( regions *rgn, uint32_t *count );

rc_t rgn_extract_type_mappings( const KNamelist *rgn_names, region_type_mapping *mapping, bool check_completenes );
rc_t rgn_show_type_mappings( region_type_mapping *mapping );

#ifdef __cplusplus
}
#endif

#endif
