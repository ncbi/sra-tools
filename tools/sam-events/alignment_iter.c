/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnologmsgy Information
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
#include "alignment_iter.h"
#include "slice_2_rowrange.h"
#include "common.h"

#include <klib/text.h>
#include <klib/log.h>
#include <klib/printf.h>
#include <klib/out.h>
 
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

/* ----------------------------------------------------------------------------------------------- */

#define ALIG_ITER_N_COLS 5

typedef struct alig_iter
{
    const char * acc;
    const VDBManager * mgr;
    const VDatabase * db;
    const VTable * tbl;
    const VCursor *curs;
    uint32_t idx[ ALIG_ITER_N_COLS ];
    /* row_range total; */
    row_range to_process;
    uint64_t rows_processed;
    int64_t current_row;
    bool use_find_next_row;
} alig_iter;


/* releae an alignment-iterator */
rc_t alig_iter_release( struct alig_iter * ai )
{
    rc_t rc = 0;

    if ( ai == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( ai->curs != NULL ) VCursorRelease( ai->curs );
        if ( ai->tbl != NULL ) VTableRelease( ai->tbl );
        if ( ai->db != NULL ) VDatabaseRelease( ai->db );
        if ( ai->mgr != NULL ) VDBManagerRelease( ai->mgr );
        free( ( void * ) ai );
    }
    return rc;
}


static rc_t alig_iter_initialize( struct alig_iter * ai, size_t cache_capacity, const slice * slice )
{
    rc_t rc = VDBManagerMakeRead( &ai->mgr, NULL );
    if ( rc != 0 )
        log_err( "aligmnet_iter.alig_iter_initialize.VDBManagerMakeRead() %R", rc );
    else
    {
        rc = VDBManagerOpenDBRead( ai->mgr, &ai->db, NULL, "%s", ai->acc );
        if ( rc != 0 )
            log_err( "aligmnet_iter.alig_iter_initialize.VDBManagerOpenDBRead( '%s' ) %R", ai->acc, rc );
        else
        {
            row_range requested_range;
            if ( slice != NULL )
                rc = slice_2_row_range_db( slice, ai->acc, ai->db, &requested_range );
                
            if ( rc == 0 )
            {
                rc = VDatabaseOpenTableRead( ai->db, &ai->tbl, "%s", "PRIMARY_ALIGNMENT" );
                if ( rc != 0 )
                    log_err( "aligmnet_iter.alig_iter_initialize.VDatabaseOpenTableRead( '%s'.PRIMARY_ALIGNMENT ) %R", ai->acc, rc );
                else
                {
                    if ( cache_capacity > 0 )
                        rc = VTableCreateCachedCursorRead( ai->tbl, &ai->curs, cache_capacity );
                    else
                        rc = VTableCreateCursorRead( ai->tbl, &ai->curs );
                    if ( rc != 0 )
                        log_err( "aligmnet_iter.alig_iter_initialize.TableCreateCursorRead( '%s'.PRIMARY_ALIGNMENT ) %R", ai->acc, rc );
                    else
                    {
                        rc = add_cols_to_cursor( ai->curs, ai->idx, "PRIMARY_ALIGNMENT", ai->acc, ALIG_ITER_N_COLS,
                                                 "CIGAR_SHORT", "REF_SEQ_ID", "READ", "REF_POS", "SAM_FLAGS" );
                        if ( rc == 0 && slice == NULL )
                        {
                            rc = VCursorIdRange( ai->curs, ai->idx[ 3 ], &requested_range.first_row, &requested_range.row_count );
                            if ( rc != 0 )
                                log_err( "aligmnet_iter.alig_iter_initialize.VCursorIdRange( '%s'.PRIMARY_ALIGNMENT ) %R", ai->acc, rc );
                        }
                        if ( rc == 0 )
                        {
                            ai->to_process.first_row = requested_range.first_row;
                            ai->to_process.row_count = requested_range.row_count;
                            ai->current_row = requested_range.first_row;
                            ai->use_find_next_row = false;
                        }
                    }
                }
            }
        }
    }
    return rc;
}


/* construct an alignmet-iterator from an accession */
rc_t alig_iter_make( struct alig_iter ** ai, const char * acc, size_t cache_capacity, const slice * slice )
{
    rc_t rc = 0;
    if ( ai == NULL || acc == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "aligmnet_iter.alig_iter_make() given a NULL-ptr" );
    }
    else
    {
        alig_iter * o = calloc( 1, sizeof *o );
        *ai = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "aligmnet_iter.alig_iter_make() memory exhausted" );
        }
        else
        {
            o->acc = acc;
            rc = alig_iter_initialize( o, cache_capacity, slice );
        }
        
        if ( rc == 0 )
            *ai = o;
        else
            alig_iter_release( o );
    }
    return rc;
}


static rc_t fill_alignment( struct alig_iter * ai, int64_t row_id, AlignmentT * al )
{
    uint32_t elem_bits, boff, row_len;
    
    /* get the CIGAR */
    rc_t rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 0 ], &elem_bits, ( const void ** )&al->cigar.addr, &boff, &row_len );
    if ( rc != 0 )
        log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.CIGAR[ %ld ] %R", ai->acc, row_id, rc );
    else
        al->cigar.len = al->cigar.size = row_len;
    
    if ( rc == 0 )
    {
        /* get the REFERENCE-NAME */
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 1 ], &elem_bits, ( const void ** )&al->rname.addr, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_SEQ_ID[ %ld ] %R", ai->acc, row_id, rc );
        else
            al->rname.len = al->rname.size = row_len;
    }

    if ( rc == 0 )
    {
        /* get the READ */
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 2 ], &elem_bits, ( const void ** )&al->read.addr, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.READ[ %ld ] %R", ai->acc, row_id, rc );
        else
            al->read.len = al->read.size = row_len;
    }
    
    if ( rc == 0 )
    {
        /* get the REFERENCE-POSITION */
        uint32_t * pp;
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 3 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_POS[ %ld ] %R", ai->acc, row_id, rc );
        else
            al->pos = pp[ 0 ] + 1; /* to produce the same as the SAM-spec, a 1-based postion! */
    }
    
    if ( rc == 0 )
    {
        /*get the strand and first ( by looking at SAM_FLAGS ) */
        uint32_t * pp;
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 4 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.SAM_FLAGS[ %ld ] %R", ai->acc, row_id, rc );
        else
            inspect_sam_flags( al, pp[ 0 ] );
    }
    return rc;
}


/* get the next alignemnt from the iter */
bool alig_iter_get( struct alig_iter * ai, AlignmentT * alignment, uint64_t * processed )
{
    bool res = false;
    if ( ai != NULL && alignment != NULL )
    {
        res = ( ai->rows_processed < ai->to_process.row_count );
        if ( res )
        {
            rc_t rc = fill_alignment( ai, ai->current_row, alignment );
            if ( rc == 0 )
            {
                if ( ai->use_find_next_row )
                {
                    /* find the next row ( to skip over empty rows... ) */
                    int64_t nxt;
                    rc_t rc = VCursorFindNextRowIdDirect( ai->curs, ai->idx[ 3 ], ai->current_row + 1, &nxt );
                    if ( rc != 0 )
                        log_err( "cannot find next row-id of '%s'.PRIMARY_ALIGNMENT.REF_POS[ %ld ] %R", ai->acc, ai->current_row, rc );
                    else
                        ai->current_row = nxt;
                }
                else
                    ai->current_row++;
                    
                ai->rows_processed++;
            }
            else
                res = false;
        }
        if ( processed != NULL ) *processed = ai->rows_processed;
    }
    return res;
}
