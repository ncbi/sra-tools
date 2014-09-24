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

#include "pl-passes.h"
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


const char * passes_tab_names[] = 
{ 
    "ADAPTER_HIT_BEFORE",
    "ADAPTER_HIT_AFTER",
    "PASS_DIRECTION",
    "PASS_NUM_BASES",
    "PASS_START_BASE"
};


typedef struct Passes_src
{
    af_data AdapterHitBefore;
    af_data AdapterHitAfter;
    af_data PassDirection;
    af_data PassNumBases;
    af_data PassStartBase;
} Passes_src;


static void init_Passes_src( Passes_src *tab )
{
    init_array_file( &tab->AdapterHitBefore );
    init_array_file( &tab->AdapterHitAfter );
    init_array_file( &tab->PassDirection );
    init_array_file( &tab->PassNumBases );
    init_array_file( &tab->PassStartBase );
}


static void close_Passes_src( Passes_src *tab )
{
    free_array_file( &tab->AdapterHitBefore );
    free_array_file( &tab->AdapterHitAfter );
    free_array_file( &tab->PassDirection );
    free_array_file( &tab->PassNumBases );
    free_array_file( &tab->PassStartBase );
}

static rc_t open_Passes_src( const KDirectory *hdf5_dir, Passes_src *tab,
                             const char * path, bool cache_content )
{
    rc_t rc;

    init_Passes_src( tab );

    rc = open_element( hdf5_dir, &tab->AdapterHitBefore, path, "AdapterHitBefore",
                  ADAPTER_HIT_BEFORE_BITSIZE, ADAPTER_HIT_BEFORE_COLS, 
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->AdapterHitAfter, path, "AdapterHitAfter",
                  ADAPTER_HIT_AFTER_BITSIZE, ADAPTER_HIT_AFTER_COLS,
                  true, cache_content, false );
    if ( rc == 0 )    
        rc = open_element( hdf5_dir, &tab->PassDirection, path, "PassDirection",
                  PASS_DIRECTION_BITSIZE, PASS_DIRECTION_COLS,
                  true, cache_content, false );
    if ( rc == 0 )    
        rc = open_element( hdf5_dir, &tab->PassNumBases, path, "PassNumBases",
                  PASS_NUM_BASES_BITSIZE, PASS_NUM_BASES_COLS,
                  true, cache_content, false );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->PassStartBase, path, "PassStartBase",
                  PASS_START_BASE_BITSIZE, PASS_START_BASE_COLS,
                  true, cache_content, false );
    if ( rc != 0 )
        close_Passes_src( tab );
    return rc;
}


static bool check_Passes_ext( af_data *af, bool *needed, uint64_t *expected,
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


static bool check_Passes_extents( Passes_src *tab )
{
    bool res = true;
    bool needed = true;
    uint64_t expected = 0;

    if ( tab->AdapterHitBefore.rc == 0 && res )
        res = check_Passes_ext( &tab->AdapterHitBefore, &needed, &expected, "AdapterHitBefore" );

    if ( tab->AdapterHitAfter.rc == 0 && res )
        res = check_Passes_ext( &tab->AdapterHitAfter, &needed, &expected, "AdapterHitAfter" );

    if ( tab->PassDirection.rc == 0 && res )
        res = check_Passes_ext( &tab->PassDirection, &needed, &expected, "PassDirection" );

    if ( tab->PassNumBases.rc == 0 && res )
        res = check_Passes_ext( &tab->PassNumBases, &needed, &expected, "PassNumBases" );

    if ( tab->PassStartBase.rc == 0 && res )
        res = check_Passes_ext( &tab->PassStartBase, &needed, &expected, "PassStartBase" );

    return res;
}


#define PASS_BLOCK_SIZE 1024

typedef struct pass_block
{
    uint8_t  AdapterHitBefore[ PASS_BLOCK_SIZE ];
    uint8_t  AdapterHitAfter[ PASS_BLOCK_SIZE ];
    uint8_t  PassDirection[ PASS_BLOCK_SIZE ];
    uint32_t PassNumBases[ PASS_BLOCK_SIZE ];
    uint32_t PassStartBase[ PASS_BLOCK_SIZE ];
    uint64_t n_read;
} pass_block;


static rc_t pass_block_read_from_src( Passes_src *tab,
                                      const uint64_t total_passes,
                                      const uint64_t pos,
                                      pass_block * block )
{
    rc_t rc = 0;
    uint64_t to_read = PASS_BLOCK_SIZE;
    uint64_t read; 

    block->n_read = 0;
    if ( ( pos + to_read ) >= total_passes )
        to_read = ( total_passes - pos );

    if ( tab->AdapterHitBefore.rc == 0 )
        rc = array_file_read_dim1( &tab->AdapterHitBefore, pos, 
                                   &block->AdapterHitBefore[0],
                                   to_read, &read );
    else
        memset( &block->AdapterHitBefore[0], 0, to_read * sizeof( uint8_t ) );


    if ( rc == 0 && tab->AdapterHitAfter.rc == 0 )
        rc = array_file_read_dim1( &tab->AdapterHitAfter, pos, 
                                   &block->AdapterHitAfter[0],
                                   to_read, &read );
    else
        memset( &block->AdapterHitAfter[0], 0, to_read * sizeof( uint8_t ) );

    if ( rc == 0 && tab->PassDirection.rc == 0 )
        rc = array_file_read_dim1( &tab->PassDirection, pos, 
                                   &block->PassDirection[0],
                                   to_read, &read );
    else
        memset( &block->PassDirection[0], 0, to_read * sizeof( uint8_t ) );

    if ( rc == 0 && tab->PassNumBases.rc == 0 )
        rc = array_file_read_dim1( &tab->PassNumBases, pos, 
                                   &block->PassNumBases[0],
                                   to_read, &read );
    else
        memset( &block->PassNumBases[0], 0, to_read * sizeof( uint32_t ) );

    if ( rc == 0 && tab->PassStartBase.rc == 0 )
        rc = array_file_read_dim1( &tab->PassStartBase, pos, 
                                   &block->PassStartBase[0],
                                   to_read, &read );
    else
        memset( &block->PassStartBase[0], 0, to_read * sizeof( uint32_t ) );

    if ( rc == 0 )
        block->n_read = read;

    return rc;
}


static rc_t passes_load_pass( VCursor *cursor, pass_block *block,
                              const uint32_t idx, uint32_t *col_idx )
{
    rc_t rc = VCursorOpenRow( cursor );
    if ( rc != 0 )
        LOGERR( klogErr, rc, "cannot open passes-row" );

    if ( rc == 0 )
        rc = vdb_write_uint8( cursor, 
                        col_idx[ passes_tab_ADAPTER_HIT_BEFORE ],
                        block->AdapterHitBefore[ idx ],
                        "passes.AdapterHitBefore" );
    if ( rc == 0 )
        rc = vdb_write_uint8( cursor, 
                        col_idx[ passes_tab_ADAPTER_HIT_AFTER ],
                        block->AdapterHitAfter[ idx ],
                        "passes.AdapterHitAfter" );
    if ( rc == 0 )
        rc = vdb_write_uint8( cursor, 
                        col_idx[ passes_tab_PASS_DIRECTION ],
                        block->PassDirection[ idx ],
                        "passes.PassDirection" );
    if ( rc == 0 )
        rc = vdb_write_uint32( cursor, 
                        col_idx[ passes_tab_PASS_NUM_BASES ],
                        block->PassNumBases[ idx ],
                        "passes.PassNumBases" );
    if ( rc == 0 )
        rc = vdb_write_uint32( cursor, 
                        col_idx[ passes_tab_PASS_START_BASE ],
                        block->PassStartBase[ idx ],
                        "passes.PassStartBase" );

    if ( rc == 0 )
    {
        rc = VCursorCommitRow( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot commit passes-row" );
    }
    if ( rc == 0 )
    {
        rc = VCursorCloseRow( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot close passes-row" );
    }

    return rc;
}


static rc_t passes_load_loop( ld_context *lctx, VCursor * cursor, Passes_src *tab,
                              uint32_t *col_idx )
{
    rc_t rc = 0;
    KLogLevel tmp_lvl;
    pass_block block;
    pl_progress *progress;
    uint64_t pos = 0;
    uint64_t total_passes = tab->AdapterHitBefore.extents[0];

    pl_progress_make( &progress, total_passes );
    rc = progress_chunk( &lctx->xml_progress, total_passes );

    tmp_lvl = KLogLevelGet();
    KLogLevelSet( klogInfo );
    PLOGMSG( klogInfo, ( klogInfo,
                         "loading passes-table ( $(rows) rows ) :",
                         "rows=%lu", total_passes ));
    KLogLevelSet( tmp_lvl );

    while( pos < total_passes && rc == 0 )
    {
        rc = pass_block_read_from_src( tab, total_passes, pos, &block );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < block.n_read && rc == 0; ++i )
            {
                rc = Quitting();
                if ( rc == 0 )
                {
                    rc = passes_load_pass( cursor, &block, i, col_idx );
                    if ( rc == 0 )
                    {
                        rc = progress_step( lctx->xml_progress );
                        if ( lctx->with_progress )
                            pl_progress_increment( progress, 1 );
                    }
                }
                else
                    LOGERR( klogErr, rc, "...loading passes interrupted" );
            }
            pos += block.n_read;
        }
    }

    pl_progress_destroy( progress );

    if ( rc == 0 )
    {
        rc = VCursorCommit( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot commit cursor on PASSES-tab" );
    }
    return rc;
}


static rc_t passes_loader( ld_context * lctx, KDirectory * hdf5_src, VCursor * cursor, const char * table_name )
{
    uint32_t col_idx[ passes_tab_count ];
    rc_t rc = add_columns( cursor, passes_tab_count, -1, col_idx, passes_tab_names );
    if ( rc == 0 )
    {
        rc = VCursorOpen( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot open cursor on PASSES-tab" );
        else
        {
            Passes_src Passes;
            rc = open_Passes_src( hdf5_src, &Passes,
                                  "PulseData/ConsensusBaseCalls/Passes",
                                  lctx->cache_content );
            if ( rc == 0 )
            {
                if ( check_Passes_extents( &Passes ) )
                    rc = passes_load_loop( lctx, cursor, &Passes, col_idx );
                else
                    rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
                close_Passes_src( &Passes );
            }
        }
    }
    return rc;
}

/* HDF5-Groups and tables used to load the PASSES-table */
static const char * passes_groups_to_check[] = 
{ 
    "PulseData/ConsensusBaseCalls/Passes",
    NULL
};

static const char * passes_tables_to_check[] = 
{ 
    "PulseData/ConsensusBaseCalls/Passes/AdapterHitAfter",
    "PulseData/ConsensusBaseCalls/Passes/AdapterHitBefore",
    "PulseData/ConsensusBaseCalls/Passes/PassDirection",
    "PulseData/ConsensusBaseCalls/Passes/PassNumBases",
    "PulseData/ConsensusBaseCalls/Passes/PassStartBase",
    NULL
};

static const char * passes_schema_template = "PASSES";
static const char * passes_table_to_create = "PASSES";


rc_t prepare_passes( VDatabase * database, pas_ctx * sctx, ld_context *lctx )
{
    rc_t rc = prepare_table( database, &sctx->cursor,
            passes_schema_template, passes_table_to_create ); /* pl-tools.c ... this creates the cursor */
    if ( rc == 0 )
    {
        rc = add_columns( sctx->cursor, passes_tab_count, -1, sctx->col_idx, passes_tab_names );
        if ( rc == 0 )
        {
            rc = VCursorOpen( sctx->cursor );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "cannot open cursor on PASSES-tab" );
            }
            else
            {
                sctx->lctx = lctx;
            }
        }
    }
    return rc;
}


rc_t load_passes_src( pas_ctx * sctx, KDirectory * hdf5_src )
{
    Passes_src Passes;
    rc_t rc = 0;

    if ( sctx->lctx->check_src_obj )
        rc = check_src_objects( hdf5_src, passes_groups_to_check, 
                                passes_tables_to_check, false );
    if ( rc == 0 )
        rc = open_Passes_src( hdf5_src, &Passes, "PulseData/ConsensusBaseCalls/Passes",
                               sctx->lctx->cache_content );
    if ( rc == 0 )
    {
        if ( check_Passes_extents( &Passes ) )
            rc = passes_load_loop( sctx->lctx, sctx->cursor, &Passes, sctx->col_idx );
        else
            rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
        close_Passes_src( &Passes );
    }
    return rc;
}


rc_t finish_passes( pas_ctx * sctx )
{
    VCursorRelease( sctx->cursor );
    return 0;
}


rc_t load_passes( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx )
{
    rc_t rc = 0;
    if ( lctx->check_src_obj )
        rc = check_src_objects( hdf5_src, passes_groups_to_check, 
                                passes_tables_to_check, false );
    if ( rc == 0 )
        rc = load_table( database, hdf5_src, lctx, passes_schema_template, 
                         passes_table_to_create, passes_loader );
    return rc;
}
