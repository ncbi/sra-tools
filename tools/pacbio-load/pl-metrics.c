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

#include "pl-metrics.h"
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char * metrics_tab_names[] = 
{ 
    "BASE_FRACTION",
    "BASE_IPD",
    "BASE_RATE",
    "BASE_WIDTH",
    "CHAN_BASE_QV",
    "CHAN_DEL_QV",
    "CHAN_INS_QV",
    "CHAN_SUB_QV",
    "LOCAL_BASE_RATE",
    "DARK_BASE_RATE",
    "HQ_RGN_START_TIME",
    "HQ_RGN_END_TIME",
    "HQ_RGN_SNR",
    "PRODUCTIVITY",
    "READ_SCORE",
    "READ_BASE_QV",
    "READ_DEL_QV",
    "READ_INS_QV",
    "READ_SUB_QV"
};


typedef struct Metrics_src
{
    af_data BaseFraction;
    af_data BaseIpd;
    af_data BaseRate;
    af_data BaseWidth;
    af_data CmBasQV;
    af_data CmDelQV;
    af_data CmInsQV;
    af_data CmSubQV;
    af_data LocalBaseRate;
    af_data DarkBaseRate;
    af_data HQRegionStartTime;
    af_data HQRegionEndTime;
    af_data HQRegionSNR;
    af_data Productivity;
    af_data ReadScore;
    af_data RmBasQV;
    af_data RmDelQV;
    af_data RmInsQV;
    af_data RmSubQV;
} Metrics_src;


static void init_Metrics_src( Metrics_src *tab )
{
    init_array_file( &tab->BaseFraction );
    init_array_file( &tab->BaseIpd );
    init_array_file( &tab->BaseRate );
    init_array_file( &tab->BaseWidth );
    init_array_file( &tab->CmBasQV );
    init_array_file( &tab->CmDelQV );
    init_array_file( &tab->CmInsQV );
    init_array_file( &tab->CmSubQV );
    init_array_file( &tab->LocalBaseRate );
    init_array_file( &tab->DarkBaseRate );
    init_array_file( &tab->HQRegionStartTime );
    init_array_file( &tab->HQRegionEndTime );
    init_array_file( &tab->HQRegionSNR );
    init_array_file( &tab->Productivity );
    init_array_file( &tab->ReadScore );
    init_array_file( &tab->RmBasQV );
    init_array_file( &tab->RmDelQV );
    init_array_file( &tab->RmInsQV );
    init_array_file( &tab->RmSubQV );
}


static void close_Metrics_src( Metrics_src *tab )
{
    free_array_file( &tab->BaseFraction );
    free_array_file( &tab->BaseIpd );
    free_array_file( &tab->BaseRate );
    free_array_file( &tab->BaseWidth );
    free_array_file( &tab->CmBasQV );
    free_array_file( &tab->CmDelQV );
    free_array_file( &tab->CmInsQV );
    free_array_file( &tab->CmSubQV );
    free_array_file( &tab->LocalBaseRate );
    free_array_file( &tab->DarkBaseRate );
    free_array_file( &tab->HQRegionStartTime );
    free_array_file( &tab->HQRegionEndTime );
    free_array_file( &tab->HQRegionSNR );
    free_array_file( &tab->Productivity );
    free_array_file( &tab->ReadScore );
    free_array_file( &tab->RmBasQV );
    free_array_file( &tab->RmDelQV );
    free_array_file( &tab->RmInsQV );
    free_array_file( &tab->RmSubQV );
}


static rc_t open_Metrics_src( const KDirectory *hdf5_dir, Metrics_src *tab,
                              const char * path, bool cache_content )
{
    rc_t rc, rc_none;
    init_Metrics_src( tab );

    rc = open_element( hdf5_dir, &tab->BaseFraction, path, "BaseFraction", 
                  BASE_FRACTION_BITSIZE, BASE_FRACTION_COLS,
                  true, cache_content, false );

    /* it is ok if BaseIpd is missing !!! */
    if ( rc == 0 )
    {
        rc_none = open_element( hdf5_dir, &tab->BaseIpd, path, "BaseIpd", 
                    BASE_IPD_BITSIZE, BASE_IPD_COLS,
                    true, cache_content, true );
        if ( rc_none != 0 )
        {
            print_log_info( "metrics -> BaseIpd is missing" );
        }
    }

    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->BaseRate, path, "BaseRate", 
                  BASE_RATE_BITSIZE, BASE_RATE_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->BaseWidth, path, "BaseWidth", 
                  BASE_WIDTH_BITSIZE, BASE_WIDTH_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->CmBasQV, path, "CmBasQv", 
                  CM_BAS_QV_BITSIZE, CM_BAS_QV_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->CmDelQV, path, "CmDelQv",
                  CM_DEL_QV_BITSIZE, CM_DEL_QV_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->CmInsQV, path, "CmInsQv",
                  CM_INS_QV_BITSIZE, CM_INS_QV_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->CmSubQV, path, "CmSubQv",
                  CM_SUB_QV_BITSIZE, CM_SUB_QV_COLS,
                  true, cache_content, false );

    if ( rc == 0 )
    {
        /* it is ok if LocalBaseRate is missing !!! */
        rc_none = open_element( hdf5_dir, &tab->LocalBaseRate, path, "LocalBaseRate",
                  LOCAL_BASE_RATE_BITSIZE, LOCAL_BASE_RATE_COLS,
                  true, cache_content, true );
        if ( rc_none != 0 )
        {
            print_log_info( "metrics -> LocalBaseRate is missing" );
        }

        /* it is ok if DarkBaseRate is missing !!! */
        rc_none = open_element( hdf5_dir, &tab->DarkBaseRate, path, "DarkBaseRate",
                      DARK_BASE_RATE_BITSIZE, DARK_BASE_RATE_COLS,
                      true, cache_content, true );
        if ( rc_none != 0 )
        {
            print_log_info( "metrics -> DarkBaseRate is missing" );
        }

        /* it is ok if HQRegionStartTime is missing !!! */
        rc_none = open_element( hdf5_dir, &tab->HQRegionStartTime, path, "HQRegionStartTime",
                      HQ_REGION_START_TIME_BITSIZE, HQ_REGION_START_TIME_COLS,
                      true, cache_content, true );
        if ( rc_none != 0 )
        {
            print_log_info( "metrics -> HQRegionStartTime is missing" );
        }

        /* it is ok if HQRegionEndTime is missing !!! */
        rc_none = open_element( hdf5_dir, &tab->HQRegionEndTime, path, "HQRegionEndTime",
                      HQ_REGION_END_TIME_BITSIZE, HQ_REGION_END_TIME_COLS,
                      true, cache_content, true );
        if ( rc_none != 0 )
        {
            print_log_info( "metrics -> HQRegionEndTime is missing" );
        }

        /* it is ok of HQRegionSNR is missing !!! ( discovered 12/16/2011 )*/
        rc_none = open_element( hdf5_dir, &tab->HQRegionSNR, path, "HQRegionSNR",
                    HQ_REGION_SNR_BITSIZE, HQ_REGION_SNR_COLS,
                    true, cache_content, true );
        if ( rc_none != 0 )
        {
            print_log_info( "metrics -> HQRegionSNR is missing" );
        }
    }

    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->Productivity, path, "Productivity",
                  PRODUCTIVITY_BITSIZE, PRODUCTIVITY_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->ReadScore, path, "ReadScore",
                  READ_SCORE_BITSIZE, READ_SCORE_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->RmBasQV, path, "RmBasQv", 
                  RM_BAS_QV_BITSIZE, RM_BAS_QV_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->RmDelQV, path, "RmDelQv",
                  RM_DEL_QV_BITSIZE, RM_DEL_QV_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->RmInsQV, path, "RmInsQv",
                  RM_INS_QV_BITSIZE, RM_INS_QV_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->RmSubQV, path, "RmSubQv",
                  RM_SUB_QV_BITSIZE, RM_SUB_QV_COLS,
                  true, cache_content, false );
    if ( rc != 0 )
        close_Metrics_src( tab );
    return rc;
}


static bool check_Metrics_ext( af_data *af, bool *needed, uint64_t *expected,
                               const char * s )
{
    bool res = true;

    if ( *needed )
    {
        *expected = af->extents[ 0 ];
        *needed = false;
    }
    else
        res = check_table_count( af, s, *expected );
    return res;
}

static bool check_Metrics_extents( Metrics_src *tab )
{
    bool res = true;
    bool needed = true;
    uint64_t expected = 0;

    if ( tab->BaseFraction.rc == 0 && res )
        res = check_Metrics_ext( &tab->BaseFraction, &needed, &expected, "BaseFraction" );

    if ( tab->BaseIpd.rc == 0 && res )
        res = check_Metrics_ext( &tab->BaseIpd, &needed, &expected, "BaseIpd" );

    if ( tab->BaseRate.rc == 0 && res )
        res = check_Metrics_ext( &tab->BaseRate, &needed, &expected, "BaseRate" );

    if ( tab->BaseWidth.rc == 0 && res )
        res = check_Metrics_ext( &tab->BaseWidth, &needed, &expected, "BaseWidth" );

    if ( tab->CmBasQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->CmBasQV, &needed, &expected, "CmBasQV" );

    if ( tab->CmDelQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->CmDelQV, &needed, &expected, "CmDelQV" );

    if ( tab->CmInsQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->CmInsQV, &needed, &expected, "CmInsQV" );

    if ( tab->CmSubQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->CmSubQV, &needed, &expected, "CmSubQV" );

    if ( tab->LocalBaseRate.rc == 0 && res )
        res = check_Metrics_ext( &tab->LocalBaseRate, &needed, &expected, "LocalBaseRate" );

    if ( tab->DarkBaseRate.rc == 0 && res )
        res = check_Metrics_ext( &tab->DarkBaseRate, &needed, &expected, "DarkBaseRate" );

    if ( tab->HQRegionStartTime.rc == 0 && res )
        res = check_Metrics_ext( &tab->HQRegionStartTime, &needed, &expected, "HQRegionStartTime" );

    if ( tab->HQRegionEndTime.rc == 0 && res )
        res = check_Metrics_ext( &tab->HQRegionEndTime, &needed, &expected, "HQRegionEndTime" );

    if ( tab->HQRegionSNR.rc == 0 && res )
        res = check_Metrics_ext( &tab->HQRegionSNR, &needed, &expected, "HQRegionSNR" );

    if ( tab->Productivity.rc == 0 && res )
        res = check_Metrics_ext( &tab->Productivity, &needed, &expected, "Productivity" );

    if ( tab->ReadScore.rc == 0 && res )
        res = check_Metrics_ext( &tab->ReadScore, &needed, &expected, "ReadScore" );

    if ( tab->RmBasQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->RmBasQV, &needed, &expected, "RmBasQV" );

    if ( tab->RmDelQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->RmDelQV, &needed, &expected, "RmDelQV" );

    if ( tab->RmInsQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->RmInsQV, &needed, &expected, "RmInsQV" );

    if ( tab->RmSubQV.rc == 0 && res )
        res = check_Metrics_ext( &tab->RmSubQV, &needed, &expected, "RmSubQV" );

    return res;
}


#define METRICS_BLOCK_SIZE 1024

typedef struct metrics_block
{
    float   BaseFraction[ METRICS_BLOCK_SIZE ][4];
    float   BaseIpd[ METRICS_BLOCK_SIZE ];
    float   BaseRate[ METRICS_BLOCK_SIZE ];
    float   BaseWidth[ METRICS_BLOCK_SIZE ];
    float   CmBasQV[ METRICS_BLOCK_SIZE ][4];
    float   CmDelQV[ METRICS_BLOCK_SIZE ][4];
    float   CmInsQV[ METRICS_BLOCK_SIZE ][4];
    float   CmSubQV[ METRICS_BLOCK_SIZE ][4];
    float   LocalBaseRate[ METRICS_BLOCK_SIZE ];
    float   DarkBaseRate[ METRICS_BLOCK_SIZE ];
    float   HQRegionStartTime[ METRICS_BLOCK_SIZE ];
    float   HQRegionEndTime[ METRICS_BLOCK_SIZE ];
    float   HQRegionSNR[ METRICS_BLOCK_SIZE ][4];
    uint8_t Productivity[ METRICS_BLOCK_SIZE ];
    float   ReadScore[ METRICS_BLOCK_SIZE ];
    float   RmBasQV[ METRICS_BLOCK_SIZE ];
    float   RmDelQV[ METRICS_BLOCK_SIZE ];
    float   RmInsQV[ METRICS_BLOCK_SIZE ];
    float   RmSubQV[ METRICS_BLOCK_SIZE ];
    uint64_t n_read;
} metrics_block;



static rc_t metrics_block_read_from_src( Metrics_src *tab,
                                      const uint64_t total_rows,
                                      const uint64_t pos,
                                      metrics_block * block )
{
    rc_t rc = 0 ;
    uint64_t to_read = METRICS_BLOCK_SIZE;
    uint64_t read;

    block->n_read = 0;
    if ( ( pos + to_read ) >= total_rows )
        to_read = ( total_rows - pos );

    if ( tab->BaseFraction.rc == 0 )
        rc = array_file_read_dim2( &tab->BaseFraction, pos, 
                                   &block->BaseFraction[0][0],
                                   to_read, 4, &read );
    else
        memset( &block->BaseFraction[0][0], 0, to_read * 4 * sizeof( float ) );

    if ( rc == 0 && tab->BaseIpd.rc == 0 )
        rc = array_file_read_dim1( &tab->BaseIpd, pos, 
                                   &block->BaseIpd[0],
                                   to_read, &read );
    else
        memset( &block->BaseIpd[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->BaseRate.rc == 0 )
        rc = array_file_read_dim1( &tab->BaseRate, pos, 
                                   &block->BaseRate[0],
                                   to_read, &read );
    else
        memset( &block->BaseRate[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->BaseWidth.rc == 0 )
        rc = array_file_read_dim1( &tab->BaseWidth, pos, 
                                   &block->BaseWidth[0],
                                   to_read, &read );
    else
        memset( &block->BaseWidth[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->CmBasQV.rc == 0 )
        rc = array_file_read_dim2( &tab->CmBasQV, pos, 
                                   &block->CmBasQV[0][0],
                                   to_read, 4, &read );
    else
        memset( &block->CmBasQV[0][0], 0, to_read * 4 * sizeof( float ) );

    if ( rc == 0 && tab->CmDelQV.rc == 0 )
        rc = array_file_read_dim2( &tab->CmDelQV, pos, 
                                   &block->CmDelQV[0][0],
                                   to_read, 4, &read );
    else
        memset( &block->CmDelQV[0][0], 0, to_read * 4 * sizeof( float ) );

    if ( rc == 0 && tab->CmInsQV.rc == 0 )
        rc = array_file_read_dim2( &tab->CmInsQV, pos, 
                                   &block->CmInsQV[0][0],
                                   to_read, 4, &read );
    else
        memset( &block->CmInsQV[0][0], 0, to_read * 4 * sizeof( float ) );

    if ( rc == 0 && tab->CmSubQV.rc == 0 )
        rc = array_file_read_dim2( &tab->CmSubQV, pos, 
                                   &block->CmSubQV[0][0],
                                   to_read, 4, &read );
    else
        memset( &block->CmSubQV[0][0], 0, to_read * 4 * sizeof( float ) );

    if ( rc == 0 && tab->LocalBaseRate.rc == 0 )
        rc = array_file_read_dim1( &tab->LocalBaseRate, pos, 
                                   &block->LocalBaseRate[0],
                                   to_read, &read );
    else
        memset( &block->LocalBaseRate[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->DarkBaseRate.rc == 0 )
        rc = array_file_read_dim1( &tab->DarkBaseRate, pos, 
                                   &block->DarkBaseRate[0],
                                   to_read, &read );
    else
        memset( &block->DarkBaseRate[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->HQRegionStartTime.rc == 0 )
        rc = array_file_read_dim1( &tab->HQRegionStartTime, pos, 
                                   &block->HQRegionStartTime[0],
                                   to_read, &read );
    else
        memset( &block->HQRegionStartTime[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->HQRegionEndTime.rc == 0 )
        rc = array_file_read_dim1( &tab->HQRegionEndTime, pos, 
                                   &block->HQRegionEndTime[0],
                                   to_read, &read );
    else
        memset( &block->HQRegionEndTime[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->HQRegionSNR.rc == 0 )
        rc = array_file_read_dim2( &tab->HQRegionSNR, pos, 
                                   &block->HQRegionSNR[0][0],
                                   to_read, 4, &read );
    else
        memset( &block->HQRegionSNR[0], 0, to_read * 4 * sizeof( float ) );

    if ( rc == 0 && tab->Productivity.rc == 0 )
        rc = array_file_read_dim1( &tab->Productivity, pos, 
                                   &block->Productivity[0],
                                   to_read, &read );
    else
        memset( &block->Productivity[0], 0, to_read * sizeof( uint8_t ) );

    if ( rc == 0 && tab->ReadScore.rc == 0 )
        rc = array_file_read_dim1( &tab->ReadScore, pos, 
                                   &block->ReadScore[0],
                                   to_read, &read );
    else
        memset( &block->ReadScore[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->RmBasQV.rc == 0 )
        rc = array_file_read_dim1( &tab->RmBasQV, pos, 
                                   &block->RmBasQV[0],
                                   to_read, &read );
    else
        memset( &block->RmBasQV[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->RmDelQV.rc == 0 )
        rc = array_file_read_dim1( &tab->RmDelQV, pos, 
                                   &block->RmDelQV[0],
                                   to_read, &read );
    else
        memset( &block->RmDelQV[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->RmInsQV.rc == 0 )
        rc = array_file_read_dim1( &tab->RmInsQV, pos, 
                                   &block->RmInsQV[0],
                                   to_read, &read );
    else
        memset( &block->RmInsQV[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 && tab->RmSubQV.rc == 0 )
        rc = array_file_read_dim1( &tab->RmSubQV, pos, 
                                   &block->RmSubQV[0],
                                   to_read, &read );
    else
        memset( &block->RmSubQV[0], 0, to_read * sizeof( float ) );

    if ( rc == 0 )
        block->n_read = read;
    return rc;
}


static rc_t metrics_load( VCursor *cursor, metrics_block *block,
                          const uint32_t idx, uint32_t *col_idx )
{
    rc_t rc = VCursorOpenRow( cursor );
    if ( rc != 0 )
        LOGERR( klogErr, rc, "cannot open metrics-row" );

    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ metrics_tab_BASE_FRACTION ],
                              &block->BaseFraction[idx][0], 32, 4, 
                              "metrics.BaseFraction" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_BASE_IPD ],
                                block->BaseIpd[idx], "metrics.BaseIpd" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_BASE_RATE ],
                                block->BaseRate[idx], "metrics.BaseRate" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_BASE_WIDTH ],
                                block->BaseWidth[idx], "metrics.BaseWidth" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ metrics_tab_CHAN_BASE_QV ],
                              &block->CmBasQV[idx][0], CM_BAS_QV_BITSIZE, 4, 
                              "metrics.CmBasQv" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ metrics_tab_CHAN_DEL_QV ],
                              &block->CmDelQV[idx][0], CM_DEL_QV_BITSIZE, 4, 
                              "metrics.CmDelQv" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ metrics_tab_CHAN_INS_QV ],
                              &block->CmInsQV[idx][0], CM_INS_QV_BITSIZE, 4, 
                              "metrics.CmInsQv" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ metrics_tab_CHAN_SUB_QV ],
                              &block->CmSubQV[idx][0], CM_SUB_QV_BITSIZE, 4, 
                              "metrics.CmSubQv" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_LOCAL_BASE_RATE ],
                                block->LocalBaseRate[idx],
                                "metrics.LocalBaseRate" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_DARK_BASE_RATE ],
                                block->DarkBaseRate[idx],
                                "metrics.DarkBaseRate" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_HQ_RGN_START_TIME ],
                                block->HQRegionStartTime[idx], 
                                "metrics.HQRegionStartTime" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_HQ_RGN_END_TIME ],
                                block->HQRegionEndTime[idx], 
                                "metrics.HQRegionEndTime" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ metrics_tab_HQ_RGN_SNR ],
                              &block->HQRegionSNR[idx][0], HQ_REGION_SNR_BITSIZE, 4, 
                              "metrics.HQRegionSNR" );
    if ( rc == 0 )
        rc = vdb_write_uint8( cursor, col_idx[ metrics_tab_PRODUCTIVITY ],
                              block->Productivity[idx], 
                              "metrics.Productivity" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_READ_SCORE ],
                                block->ReadScore[idx], 
                                "metrics.ReadScore" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_READ_BASE_QV ],
                                block->RmBasQV[idx], 
                                "metrics.RmBasQV" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_READ_DEL_QV ],
                                block->RmDelQV[idx], 
                                "metrics.RmDelQV" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_READ_INS_QV ],
                                block->RmInsQV[idx], 
                                "metrics.RmInsQV" );
    if ( rc == 0 )
        rc = vdb_write_float32( cursor, col_idx[ metrics_tab_READ_SUB_QV ],
                                block->RmSubQV[idx], 
                                "metrics.RmSubQV" );

    if ( rc == 0 )
    {
        rc = VCursorCommitRow( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot commit metrics-row" );
    }
    if ( rc == 0 )
    {
        rc = VCursorCloseRow( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot close metrics-row" );
    }

    return rc;
}


static rc_t metrics_load_loop( ld_context *lctx, VCursor * cursor, Metrics_src *tab,
                               uint32_t *col_idx )
{
    rc_t rc = 0;
    KLogLevel tmp_lvl;
    metrics_block block;
    pl_progress *progress;
    uint64_t pos = 0;
    uint64_t total_rows = tab->BaseFraction.extents[0];

    pl_progress_make( &progress, total_rows );
    rc = progress_chunk( &lctx->xml_progress, total_rows );

    tmp_lvl = KLogLevelGet();
    KLogLevelSet( klogInfo );
    PLOGMSG( klogInfo, ( klogInfo,
                         "loading metrics-table ( $(rows) rows ) :",
                         "rows=%lu", total_rows   ));
    KLogLevelSet( tmp_lvl );

    while( pos < total_rows && rc == 0 )
    {
        rc = metrics_block_read_from_src( tab, total_rows, pos, &block );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < block.n_read && rc == 0; ++i )
            {
                rc = Quitting();
                if ( rc == 0 )
                {
                    /* to be replaced with progressbar action... */
                    rc = metrics_load( cursor, &block, i, col_idx );
                    if ( rc == 0 )
                    {
                        rc = progress_step( lctx->xml_progress );
                        if ( lctx->with_progress )
                            pl_progress_increment( progress, 1 );
                    }
                }
                else
                    LOGERR( klogErr, rc, "...loading metrics interrupted" );
            }
            pos += block.n_read;
        }
    }

    pl_progress_destroy( progress );

    if ( rc == 0 )
    {
        rc = VCursorCommit( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot commit cursor on metrics-tab" );
    }
    return rc;
}


static rc_t metrics_loader( ld_context *lctx, KDirectory * hdf5_src, VCursor * cursor, const char * table_name )
{
    uint32_t col_idx[ metrics_tab_count ];
    rc_t rc = add_columns( cursor, metrics_tab_count, -1, col_idx, metrics_tab_names );
    if ( rc == 0 )
    {
        rc = VCursorOpen( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot open cursor on metrics-tab" );
        else
        {
            Metrics_src Metrics;
            rc = open_Metrics_src( hdf5_src, &Metrics,
                                   "PulseData/BaseCalls/ZMWMetrics",
                                   lctx->cache_content );
            if ( rc == 0 )
            {
                if ( check_Metrics_extents( &Metrics ) )
                    rc = metrics_load_loop( lctx, cursor, &Metrics, col_idx );
                else
                    rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
                close_Metrics_src( &Metrics );
            }
        }
    }
    return rc;
}


/* HDF5-Groups and tables used to load the METRICS-table */
static const char * metrics_groups_to_check[] = 
{ 
    "PulseData/BaseCalls/ZMWMetrics",
    NULL
};


static const char * metrics_schema_template = "ZMW_METRICS";
static const char * metrics_table_to_create = "ZMW_METRICS";

rc_t prepare_metrics( VDatabase * database, met_ctx * sctx, ld_context *lctx )
{
    rc_t rc = prepare_table( database, &sctx->cursor,
            metrics_schema_template, metrics_table_to_create ); /* pl-tools.c ... this creates the cursor */
    if ( rc == 0 )
        rc = add_columns( sctx->cursor, metrics_tab_count, -1, sctx->col_idx, metrics_tab_names );
    if ( rc == 0 )
    {
        rc = VCursorOpen( sctx->cursor );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "cannot open cursor on metrics-tab" );
        }
        else
        {
            sctx->lctx = lctx;
        }
    }
    return rc;
}


rc_t load_metrics_src( met_ctx * sctx, KDirectory * hdf5_src )
{
    Metrics_src Metrics;
    rc_t rc = 0;

    if ( sctx->lctx->check_src_obj )
        /* in case of metrics any combination of columns can be missing... */
        rc = check_src_objects( hdf5_src, metrics_groups_to_check, NULL, false );

    if ( rc == 0 )
        rc = open_Metrics_src( hdf5_src, &Metrics, "PulseData/BaseCalls/ZMWMetrics",
                               sctx->lctx->cache_content );
    if ( rc == 0 )
    {
        if ( check_Metrics_extents( &Metrics ) )
            rc = metrics_load_loop( sctx->lctx, sctx->cursor, &Metrics, sctx->col_idx );
        else
            rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
        close_Metrics_src( &Metrics );
    }
    return rc;
}


rc_t finish_metrics( met_ctx * sctx )
{
    VCursorRelease( sctx->cursor );
    return 0;
}


rc_t load_metrics( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx )
{
    rc_t rc = 0;
    if ( lctx->check_src_obj )
        /* in case of metrics any combination of columns can be missing... */
        rc = check_src_objects( hdf5_src, metrics_groups_to_check, 
                                NULL, false );

    if ( rc == 0 )
        rc = load_table( database, hdf5_src, lctx, metrics_schema_template, 
                         metrics_table_to_create, metrics_loader );
    return rc;
}
