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
#ifndef _h_pl_zmw_
#define _h_pl_zmw_

#ifdef __cplusplus
extern "C" {
#endif

#include "pl-tools.h"
#include "pl-progress.h"
#include "pl-regions.h"
#include <kapp/main.h>
#include <klib/rc.h>

typedef struct zmw_tab
{
    af_data HoleNumber;
    af_data HoleStatus;
    af_data HoleXY;
    af_data NumEvent;
    af_data NumPasses;
} zmw_tab;


#define ZMW_BLOCK_SIZE 8192

typedef struct zmw_block
{
    uint32_t NumEvent[ ZMW_BLOCK_SIZE ];
    uint32_t HoleNumber[ ZMW_BLOCK_SIZE ];
    uint8_t  HoleStatus[ ZMW_BLOCK_SIZE ];
    uint16_t HoleXY[ ZMW_BLOCK_SIZE * 2 ];
    uint32_t NumPasses[ ZMW_BLOCK_SIZE ];
    uint64_t n_read;
} zmw_block;


typedef struct zmw_row
{
    uint64_t offset;
    uint64_t spot_nr;

    uint32_t NumEvent;
    uint32_t HoleNumber;
    uint16_t HoleXY[ 2 ];
    uint8_t  HoleStatus;
    uint32_t NumPasses;
} zmw_row;


typedef rc_t (*zmw_on_row)( VCursor *cursor, const uint32_t *col_idx,
                            region_type_mapping *mapping,
                            zmw_row *row, void * data );


void zmw_init( zmw_tab *tab );
void zmw_close( zmw_tab *tab );

rc_t zmw_open( const KDirectory *hdf5_dir, zmw_tab *tab,
               const bool num_passes, const char * path, bool supress_err_msg );

uint64_t zmw_total( zmw_tab *tab );

rc_t zmw_read_block( zmw_tab *tab, zmw_block * block,
                     const uint64_t total_spots,
                     const uint64_t pos,
                     const bool with_num_passes );

void zmw_block_row( zmw_block * block, zmw_row * row,
                    const uint32_t idx );


rc_t zmw_for_each( zmw_tab *tab, const KLoadProgressbar ** xml_progress, VCursor * cursor,
                   bool with_progress, const uint32_t *col_idx, region_type_mapping *mapping,
                   const bool with_num_passes, zmw_on_row on_row, void * data );

#ifdef __cplusplus
}
#endif

#endif
