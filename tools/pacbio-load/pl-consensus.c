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

#include "pl-consensus.h"
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


const char * consensus_tab_names[] = 
{ 
    /* base-space */
    "READ",
    "QUALITY",
    "NREADS",
    "READ_TYPE",
    "READ_START",
    "READ_LEN",
    "(INSDC:SRA:platform_id)PLATFORM",
    "READ_FILTER",

    /* consensus-space */
    "HOLE_NUMBER",
    "HOLE_STATUS",
    "HOLE_XY",
    "NUM_PASSES",
    "INSERTION_QV",
    "DELETION_QV",
    "DELETION_TAG",
    "SUBSTITUTION_QV",
    "SUBSTITUTION_TAG"
};


static bool check_Consensus_totalcount( BaseCalls_cmn *tab, const uint64_t expected )
{
    bool res = check_table_count( &tab->Basecall, "Basecall", expected );
    if ( res )
        res = check_table_count( &tab->DeletionQV, "DeletionQV", expected );
    if ( res )
        res = check_table_count( &tab->DeletionTag, "DeletionTag", expected );
    if ( res )
        res = check_table_count( &tab->InsertionQV, "InsertionQV", expected );
    if ( res )
        res = check_table_count( &tab->QualityValue, "QualityValue", expected );
    if ( res )
        res = check_table_count( &tab->SubstitutionQV, "SubstitutionQV", expected );
    if ( res )
        res = check_table_count( &tab->SubstitutionTag, "SubstitutionTag", expected );
    return res;
}


static rc_t consensus_load_zero_bases( VCursor *cursor, const uint32_t *col_idx )
{
    uint32_t dummy_src; 
    INSDC_SRA_read_filter filter = SRA_READ_FILTER_CRITERIA;

    rc_t rc = vdb_write_value( cursor, col_idx[ consensus_tab_READ ],
                               &dummy_src, BASECALL_BITSIZE, 0, "consensus.Basecall" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_QUALITY ],
                               &dummy_src, QUALITY_VALUE_BITSIZE, 0, "QualityValue" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_INSERTION_QV ],
                               &dummy_src, INSERTION_QV_BITSIZE, 0, "consensus.InsertionQV" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_DELETION_QV ],
                               &dummy_src, DELETION_QV_BITSIZE, 0, "consensus.DeletionQV" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_DELETION_TAG ],
                               &dummy_src, DELETION_TAG_BITSIZE, 0, "consensus.DeletionTag" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_SUBSTITUTION_QV ],
                               &dummy_src, SUBSTITUTION_QV_BITZISE, 0, "consensus.SubstitutionQV" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_SUBSTITUTION_TAG ],
                               &dummy_src, SUBSTITUTION_TAG_BITSIZE, 0, "consensus.SubstitutionTag" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_READ_FILTER ],
                               &filter, sizeof filter * 8, 1, "consensus.READ_FILTER" );
    return rc;
}


static rc_t consensus_load_spot_bases( VCursor *cursor, BaseCalls_cmn *tab,
                                       const uint32_t *col_idx, zmw_row * spot )
{
    rc_t rc = 0;
    /* we make a buffer to store NumEvent 8-bit-values
      (that is so far the biggest value we have to read per DNA-BASE) */
    char * buffer = malloc( spot->NumEvent );
    if ( buffer == NULL )
    {
        rc = RC( rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
        PLOGERR( klogErr, ( klogErr, rc, "cannot allocate $(numbytes) to read seq-data",
                            "numbytes=%u", spot->NumEvent ) );
    }
    if ( rc == 0 )
        rc = transfer_bits( cursor, col_idx[ consensus_tab_READ ],
            &tab->Basecall, buffer, spot->offset, spot->NumEvent,
            BASECALL_BITSIZE, "consensus.Basecall" );
    if ( rc == 0 )
        rc = transfer_bits( cursor, col_idx[ consensus_tab_QUALITY ],
            &tab->QualityValue, buffer, spot->offset, spot->NumEvent,
            QUALITY_VALUE_BITSIZE, "consensus.QualityValue" );
    if ( rc == 0 )
        rc = transfer_bits( cursor, col_idx[ consensus_tab_INSERTION_QV ],
            &tab->InsertionQV, buffer, spot->offset, spot->NumEvent,
            INSERTION_QV_BITSIZE, "consensus.InsertionQV" );
    if ( rc == 0 )
        rc = transfer_bits( cursor, col_idx[ consensus_tab_DELETION_QV ],
            &tab->DeletionQV, buffer, spot->offset, spot->NumEvent,
            DELETION_QV_BITSIZE, "consensus.DeletionQV" );
    if ( rc == 0 )
        rc = transfer_bits( cursor, col_idx[ consensus_tab_DELETION_TAG ],
            &tab->DeletionTag, buffer, spot->offset, spot->NumEvent,
            DELETION_TAG_BITSIZE, "consensus.DeletionTag" );
    if ( rc == 0 )
        rc = transfer_bits( cursor, col_idx[ consensus_tab_SUBSTITUTION_QV ],
            &tab->SubstitutionQV, buffer, spot->offset, spot->NumEvent,
            SUBSTITUTION_QV_BITZISE, "consensus.SubstitutionQV" );
    if ( rc == 0 )
        rc = transfer_bits( cursor, col_idx[ consensus_tab_SUBSTITUTION_TAG ],
            &tab->SubstitutionTag, buffer, spot->offset, spot->NumEvent,
            SUBSTITUTION_TAG_BITSIZE, "consensus.SubstitutionTag" );

    if ( buffer != NULL )
        free( buffer );
    return rc;
}


static rc_t consensus_load_spot( VCursor *cursor, const uint32_t *col_idx,
                                 region_type_mapping *mapping, zmw_row * spot, 
                                 void * data )
{
    BaseCalls_cmn *tab = (BaseCalls_cmn *)data;
    rc_t rc = VCursorOpenRow( cursor );
    if ( rc != 0 )
        PLOGERR( klogErr, ( klogErr, rc, "cannot open consensus-row on spot# $(spotnr)",
                            "spotnr=%u", spot->spot_nr ) );

    if ( rc == 0 )
        rc = vdb_write_uint32( cursor, col_idx[ consensus_tab_HOLE_NUMBER ],
                               spot->HoleNumber, "consensus.HOLE_NUMBER" );
    if ( rc == 0 )
        rc = vdb_write_uint8( cursor, col_idx[ consensus_tab_HOLE_STATUS ],
                              spot->HoleStatus, "consensus.HOLE_STATUS" );
    if ( rc == 0 )
        rc = vdb_write_value( cursor, col_idx[ consensus_tab_HOLE_XY ],
                              &spot->HoleXY, HOLE_XY_BITSIZE, 2, "consensus.HOLE_XY" );

    /* has to be read ... from "PulseData/ConsensusBaesCalls/Passes/NumPasses" */
    if ( rc == 0 )
        rc = vdb_write_uint32( cursor, col_idx[ consensus_tab_NUM_PASSES ],
                               spot->NumPasses, "consensus.NUM_PASSES" );

    if ( rc == 0 )
    {
        if ( spot->NumEvent > 0 )
            rc = consensus_load_spot_bases( cursor, tab, col_idx, spot );
        else
            rc = consensus_load_zero_bases( cursor, col_idx );
    }

    if ( rc == 0 )
        rc = vdb_write_uint8( cursor, col_idx[ consensus_tab_NREADS ],
                              1, "consensus.NREADS" );
    if ( rc == 0 )
        rc = vdb_write_uint32( cursor, col_idx[ consensus_tab_READ_START ],
                               0, "consensus.READ_START" );
    if ( rc == 0 )
        rc = vdb_write_uint32( cursor, col_idx[ consensus_tab_READ_LEN ],
                               spot->NumEvent, "consensus.READ_LEN" );
    if ( rc == 0 )
        rc = vdb_write_uint8( cursor, col_idx[ consensus_tab_READ_TYPE ],
                              SRA_READ_TYPE_BIOLOGICAL, "consensus.READ_TYPE" );

    if ( rc == 0 )
    {
        rc = VCursorCommitRow( cursor );
        if ( rc != 0 )
            PLOGERR( klogErr, ( klogErr, rc, "cannot commit consensus-row on spot# $(spotnr)",
                                "spotnr=%u", spot->spot_nr ) );
    }

    if ( rc == 0 )
    {
        rc = VCursorCloseRow( cursor );
        if ( rc != 0 )
            PLOGERR( klogErr, ( klogErr, rc, "cannot close consensus-row on spot# $(spotnr)",
                                "spotnr=%u", spot->spot_nr ) );

    }
    return rc;
}


static rc_t consensus_loader( ld_context *lctx, KDirectory * hdf5_src, VCursor * cursor, const char * table_name )
{
    uint32_t col_idx[ consensus_tab_count ];
    rc_t rc = add_columns( cursor, consensus_tab_count, -1, col_idx, consensus_tab_names );
    if ( rc == 0 )
    {
        rc = VCursorOpen( cursor );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot open cursor on consensus-table" );

        else
        {
            BaseCalls_cmn ConsensusTab;
            const INSDC_SRA_platform_id platform = SRA_PLATFORM_PACBIO_SMRT;

            rc = VCursorDefault ( cursor, col_idx[ consensus_tab_PLATFORM ],
                                  sizeof platform * 8, &platform, 0, 1 );
            if ( rc != 0 )
                LOGERR( klogErr, rc, "cannot set cursor-default on consensus-table for platform-column" );
            else
            {
                const INSDC_SRA_read_filter filter = SRA_READ_FILTER_PASS;
                rc = VCursorDefault ( cursor, col_idx[ consensus_tab_READ_FILTER ],
                                  sizeof filter * 8, &filter, 0, 1 );
                if ( rc != 0 )
                    LOGERR( klogErr, rc, "cannot set cursor-default on consensus-table for read-filter-column" );
            }

            if ( rc == 0 )
                rc = open_BaseCalls_cmn( hdf5_src, &ConsensusTab, true,
                                     "PulseData/ConsensusBaseCalls", lctx->cache_content, true );

            if ( rc == 0 )
            {
                uint64_t total_bases = zmw_total( &ConsensusTab.zmw );
                uint64_t total_spots = ConsensusTab.zmw.NumEvent.extents[ 0 ];

                KLogLevel tmp_lvl = KLogLevelGet();
                KLogLevelSet( klogInfo );
                PLOGMSG( klogInfo, ( klogInfo,
                         "loading consensus-table ( $(bases) bases / $(spots) spots ):",
                         "bases=%lu,spots=%lu", total_bases, total_spots ));
                KLogLevelSet( tmp_lvl );

                if ( check_Consensus_totalcount( &ConsensusTab, total_bases ) )
                    rc = zmw_for_each( &ConsensusTab.zmw, &lctx->xml_progress, cursor,
                                       lctx->with_progress, col_idx, NULL,
                                       true, consensus_load_spot, &ConsensusTab );
                else
                    rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
                close_BaseCalls_cmn( &ConsensusTab );
            }
        }
    }
    return rc;
}


/* HDF5-Groups and tables used to load the CONSENSUS-table */
static const char * consensus_groups_to_check[] = 
{ 
    "PulseData",
    "PulseData/ConsensusBaseCalls",
    "PulseData/ConsensusBaseCalls/ZMW",
    "PulseData/ConsensusBaseCalls/Passes",
    NULL
};


static const char * consensus_tables_to_check[] = 
{ 
    "PulseData/ConsensusBaseCalls/Basecall",
    "PulseData/ConsensusBaseCalls/DeletionQV",
    "PulseData/ConsensusBaseCalls/DeletionTag",
    "PulseData/ConsensusBaseCalls/InsertionQV",
    "PulseData/ConsensusBaseCalls/QualityValue",
    "PulseData/ConsensusBaseCalls/SubstitutionQV",
    "PulseData/ConsensusBaseCalls/SubstitutionTag",
    "PulseData/ConsensusBaseCalls/ZMW/HoleNumber",
    "PulseData/ConsensusBaseCalls/ZMW/HoleStatus",
    "PulseData/ConsensusBaseCalls/ZMW/HoleXY",
    "PulseData/ConsensusBaseCalls/ZMW/NumEvent",
    "PulseData/ConsensusBaseCalls/Passes/NumPasses",
    NULL
};


static const char * consensus_schema_template = "CONSENSUS";
static const char * consensus_table_to_create = "CONSENSUS";


rc_t prepare_consensus( VDatabase * database, con_ctx * sctx, ld_context *lctx )
{
    rc_t rc = prepare_table( database, &sctx->cursor,
            consensus_schema_template, consensus_table_to_create ); /* pl-tools.c ... this creates the cursor */
    if ( rc == 0 )
    {
        rc = add_columns( sctx->cursor, consensus_tab_count, -1, sctx->col_idx, consensus_tab_names );
        if ( rc == 0 )
        {
            rc = VCursorOpen( sctx->cursor );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "cannot open cursor on consensus-table" );
            }
            else
            {
                const INSDC_SRA_platform_id platform = SRA_PLATFORM_PACBIO_SMRT;

                rc = VCursorDefault ( sctx->cursor, sctx->col_idx[ consensus_tab_PLATFORM ],
                                      sizeof platform * 8, &platform, 0, 1 );
                if ( rc != 0 )
                {
                    LOGERR( klogErr, rc, "cannot set cursor-default on consensus-table for platform-column" );
                }
                else
                {
                    const INSDC_SRA_read_filter filter = SRA_READ_FILTER_PASS;
                    rc = VCursorDefault ( sctx->cursor, sctx->col_idx[ consensus_tab_READ_FILTER ],
                                      sizeof filter * 8, &filter, 0, 1 );
                    if ( rc != 0 )
                    {
                        LOGERR( klogErr, rc, "cannot set cursor-default on consensus-table for read-filter-column" );
                    }
                    else
                    {
                        sctx->lctx = lctx;
                    }
                }
            }
        }
    }
    return rc;
}


rc_t load_consensus_src( con_ctx * sctx, KDirectory * hdf5_src )
{
    BaseCalls_cmn ConsensusTab;

    rc_t rc = 0;
    if ( sctx->lctx->check_src_obj )
        rc = check_src_objects( hdf5_src, consensus_groups_to_check, 
                                consensus_tables_to_check, false );
    if ( rc == 0 )
        rc = open_BaseCalls_cmn( hdf5_src, &ConsensusTab, true,
                                 "PulseData/ConsensusBaseCalls", sctx->lctx->cache_content, true );
    if ( rc == 0 )
    {
        uint64_t total_bases = zmw_total( &ConsensusTab.zmw );
        uint64_t total_spots = ConsensusTab.zmw.NumEvent.extents[ 0 ];

        KLogLevel tmp_lvl = KLogLevelGet();
        KLogLevelSet( klogInfo );
        PLOGMSG( klogInfo, ( klogInfo,
                 "loading consensus-table ( $(bases) bases / $(spots) spots ):",
                 "bases=%lu,spots=%lu", total_bases, total_spots ));
        KLogLevelSet( tmp_lvl );

        if ( !check_Consensus_totalcount( &ConsensusTab, total_bases ) )
            rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
        else
            rc = zmw_for_each( &ConsensusTab.zmw, &sctx->lctx->xml_progress, sctx->cursor,
                               sctx->lctx->with_progress, sctx->col_idx, NULL,
                               true, consensus_load_spot, &ConsensusTab );
        close_BaseCalls_cmn( &ConsensusTab );
    }
    return rc;
}


rc_t finish_consensus( con_ctx * sctx )
{
    VCursorRelease( sctx->cursor );
    return 0;
}


rc_t load_consensus( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx )
{
    rc_t rc = 0;
    if ( lctx->check_src_obj )
        rc = check_src_objects( hdf5_src, consensus_groups_to_check, 
                                consensus_tables_to_check, false );
    if ( rc == 0 )
        rc = load_table( database, hdf5_src, lctx, consensus_schema_template, 
                         consensus_table_to_create, consensus_loader );
    return rc;
}
