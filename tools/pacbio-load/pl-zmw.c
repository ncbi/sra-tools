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

#include "pl-zmw.h"
#include <sysalloc.h>

void zmw_init( zmw_tab *tab )
{
    init_array_file( &tab->HoleNumber );
    init_array_file( &tab->HoleStatus );
    init_array_file( &tab->HoleXY );
    init_array_file( &tab->NumEvent );
    init_array_file( &tab->NumPasses );
}


void zmw_close( zmw_tab *tab )
{
    free_array_file( &tab->HoleNumber );
    free_array_file( &tab->HoleStatus );
    free_array_file( &tab->HoleXY );
    free_array_file( &tab->NumEvent );
    free_array_file( &tab->NumPasses );
}


rc_t zmw_open( const KDirectory *hdf5_dir, zmw_tab *tab,
               const bool num_passes, const char * path, bool supress_err_msg )
{
    rc_t rc;

    zmw_init( tab );
    rc = open_element( hdf5_dir, &tab->HoleNumber, path, "ZMW/HoleNumber", 
                       HOLE_NUMBER_BITSIZE, HOLE_NUMBER_COLS, true, false, supress_err_msg );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->HoleStatus, path, "ZMW/HoleStatus", 
                           HOLE_STATUS_BITSIZE, HOLE_STATUS_COLS, true, false, supress_err_msg );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->HoleXY, path, "ZMW/HoleXY",
                           HOLE_XY_BITSIZE, HOLE_XY_COLS, true, false, supress_err_msg );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->NumEvent, path, "ZMW/NumEvent",
                           NUMEVENT_BITSIZE, NUMEVENT_COLS, true, false, supress_err_msg );
    if ( rc == 0 && num_passes )
        rc = open_element( hdf5_dir, &tab->NumPasses, path, "Passes/NumPasses", 
                           NUMPASSES_BITSIZE, NUMPASSES_COLS, true, false, supress_err_msg );

    if ( rc != 0 )
        zmw_close( tab ); /* releases only initialized elements */
    return rc;
}


uint64_t zmw_total( zmw_tab *tab )
{
    rc_t rc = 0;
    uint64_t res = 0, pos = 0;
    uint64_t num_entries = tab->NumEvent.extents[0];

    while( pos < num_entries && rc == 0 )
    {
        uint64_t n_read, to_read = ZMW_BLOCK_SIZE;
        uint32_t d[ ZMW_BLOCK_SIZE ];

        if ( ( pos + to_read ) >= num_entries )
            to_read = ( num_entries - pos );
        rc = array_file_read_dim1( &tab->NumEvent, pos, d, to_read, &n_read );
        if ( rc == 0 )
        {
            uint32_t i;
            pos+=n_read;
            for ( i = 0; i < n_read; ++i )
                res += d[ i ];
        }
    }
    return res;
}


rc_t zmw_read_block( zmw_tab *tab, zmw_block * block,
                     const uint64_t total_spots,
                     const uint64_t pos,
                     const bool with_num_passes )
{
    rc_t rc;
    uint64_t to_read = ZMW_BLOCK_SIZE;
    uint64_t read_NumEvent, read_HoleNumber, read_HoleStatus, 
             read_HoleXY, read_NumPasses;

    block->n_read = 0;
    if ( ( pos + to_read ) >= total_spots )
        to_read = ( total_spots - pos );
    rc = array_file_read_dim1( &tab->NumEvent, pos, &block->NumEvent[0],
                                to_read, &read_NumEvent );
    if ( rc == 0 )
        rc = array_file_read_dim1( &tab->HoleNumber, pos, &block->HoleNumber[0],
                                    to_read, &read_HoleNumber );
    if ( rc == 0 )
        rc = array_file_read_dim1( &tab->HoleStatus, pos, &block->HoleStatus[0],
                                    to_read, &read_HoleStatus );
    if ( rc == 0 )
        rc = array_file_read_dim2( &tab->HoleXY, pos, &block->HoleXY[0],
                                   to_read, 2, &read_HoleXY );
    if ( rc == 0 && with_num_passes )
        rc = array_file_read_dim1( &tab->NumPasses, pos, &block->NumPasses[0],
                                    to_read, &read_NumPasses );
    if ( rc == 0 )
    {
        if ( ( read_NumEvent != read_HoleNumber ) ||
             ( read_NumEvent != read_HoleStatus ) ||
             ( read_NumEvent != read_HoleXY ) )
        {
            rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
            LOGERR( klogErr, rc, "Diff in NumEvents/HoleNumber/HoleStatus/HoleXY" );
        }
        else
        {
            if ( with_num_passes && read_NumEvent != read_NumPasses )
            {
                rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
                LOGERR( klogErr, rc, "Diff in NumEvents/NumPasses" );
            }
            else
                block->n_read = read_NumEvent;
        }
    }
    return rc;
}


void zmw_block_row( zmw_block * block, zmw_row * row, 
                    const uint32_t idx )
{
    row->NumEvent    = block->NumEvent[ idx ];
    row->HoleNumber  = block->HoleNumber[ idx ];
    row->HoleStatus  = block->HoleStatus[ idx ];
    row->HoleXY[ 0 ] = block->HoleXY[ idx * 2 ];
    row->HoleXY[ 1 ] = block->HoleXY[ idx * 2 + 1 ];
    row->NumPasses   = block->NumPasses[ idx ];
}



rc_t zmw_for_each( zmw_tab *tab, const KLoadProgressbar ** xml_progress, VCursor * cursor,
                   bool with_progress, const uint32_t *col_idx, region_type_mapping *mapping,
                   const bool with_num_passes, zmw_on_row on_row, void * data )
{
    zmw_block block;
    zmw_row row;
    pl_progress *progress;
    uint64_t pos = 0;
    uint64_t total_rows = tab->NumEvent.extents[0];

    rc_t rc = progress_chunk( xml_progress, total_rows );
    if ( with_progress )
        pl_progress_make( &progress, total_rows );
    row.spot_nr = 0;
    row.offset = 0;
    while( pos < total_rows && rc == 0 )
    {
        rc = zmw_read_block( tab, &block, total_rows, pos, with_num_passes );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < block.n_read && rc == 0; ++i )
            {
                rc = Quitting();
                if ( rc == 0 )
                {
                    zmw_block_row( &block, &row, i );
                    rc = on_row( cursor, col_idx, mapping, &row, data );
                    if ( rc == 0 )
                    {
                        rc = progress_step( *xml_progress );
                        if ( with_progress )
                            pl_progress_increment( progress, 1 );
                    }
                    row.offset += block.NumEvent[ i ];
                    row.spot_nr ++;
                }
                else
                    LOGERR( klogErr, rc, "...loading ZMW-table interrupted" );
            }
            pos += block.n_read;
        }
    }

    if ( with_progress )
        pl_progress_destroy( progress );

    if ( rc == 0 )
    {
        rc = VCursorCommit( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot commit vdb-cursor on ZMW-table" );
    }
    return rc;
}
