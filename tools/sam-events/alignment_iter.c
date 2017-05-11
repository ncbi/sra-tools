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

/* use the SAM extract functions from the sam-extract library build from libs/align */
#include <align/samextract-lib.h>

#include <klib/text.h>
#include <klib/log.h>
#include <klib/printf.h>
#include <klib/out.h>

#include <kfs/directory.h>
#include <kfs/file.h>
 
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

/* ----------------------------------------------------------------------------------------------- */

#define ALIG_ITER_N_COLS 5

#define ALIG_ITER_MODE_CSRA 1
#define ALIG_ITER_MODE_SAM 2

typedef struct alig_iter
{
    /* the mode-switch */
    uint32_t mode;
    
    /* common for both modes: the name of the source, and the slice */
    const char * source;
    const slice * slice;

    /* for the CSRA-MODE */
    const VCursor *curs;
    uint32_t idx[ ALIG_ITER_N_COLS ];
    row_range to_process;
    uint64_t rows_processed;
    int64_t current_row;
    
    /* for the SAM-MODE */
    KDirectory * dir;
    const KFile * file;
    Extractor * sam_extractor;
    Vector sam_alignments;
    bool sam_alignments_loaded;
    uint32_t sam_alignments_idx;
    uint32_t sam_alignments_len;
} alig_iter;


/* releae an alignment-iterator */
rc_t alig_iter_release( struct alig_iter * ai )
{
    rc_t rc = 0;
    if ( ai == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( ai->mode == ALIG_ITER_MODE_CSRA )
        {
            if ( ai->curs != NULL )
            {
                rc = VCursorRelease( ai->curs );
                if ( rc != 0 )
                    log_err( "error (%R) releasing VCursor for %s", rc, ai->source );
            }
        }
        else if ( ai->mode == ALIG_ITER_MODE_SAM )
        {
            if ( ai->sam_alignments_loaded )
            {
                rc = SAMExtractorInvalidateAlignments( ai->sam_extractor );
                if ( rc != 0 )
                    log_err( "SAMExtractorInvalidateAlignments( '%s' ) failed (%R)", ai->source, rc );
            }
            if ( rc == 0 )
            {
                rc = SAMExtractorRelease( ai->sam_extractor );
                if ( rc != 0 )
                    log_err( "SAMExtractorRelease( '%s' ) failed (%R)", ai->source, rc );
            }
            if ( ai->file != NULL ) KFileRelease( ai->file );
            if ( ai->dir != NULL ) KDirectoryRelease( ai->dir );
        }
        free( ( void * ) ai );
    }
    return rc;
}


static rc_t alig_iter_csra_initialize( struct alig_iter * ai, size_t cache_capacity )
{
    const VDBManager * mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    if ( rc != 0 )
        log_err( "aligmnet_iter.alig_iter_initialize.VDBManagerMakeRead() %R", rc );
    else
    {
        const VDatabase * db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", ai->source );
        if ( rc != 0 )
            log_err( "aligmnet_iter.alig_iter_initialize.VDBManagerOpenDBRead( '%s' ) %R", ai->source, rc );
        else
        {
            row_range requested_range;
            if ( ai->slice != NULL )
                rc = slice_2_row_range_db( ai->slice, ai->source, db, &requested_range );
                
            if ( rc == 0 )
            {
                const VTable * tbl;
                rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
                if ( rc != 0 )
                    log_err( "aligmnet_iter.alig_iter_initialize.VDatabaseOpenTableRead( '%s'.PRIMARY_ALIGNMENT ) %R", ai->source, rc );
                else
                {
                    if ( cache_capacity > 0 )
                        rc = VTableCreateCachedCursorRead( tbl, &ai->curs, cache_capacity );
                    else
                        rc = VTableCreateCursorRead( tbl, &ai->curs );
                    if ( rc != 0 )
                        log_err( "aligmnet_iter.alig_iter_initialize.TableCreateCursorRead( '%s'.PRIMARY_ALIGNMENT ) %R", ai->source, rc );
                    else
                    {
                        rc = add_cols_to_cursor( ai->curs, ai->idx, "PRIMARY_ALIGNMENT", ai->source, ALIG_ITER_N_COLS,
                                                 "CIGAR_SHORT", "REF_SEQ_ID", "READ", "REF_POS", "SAM_FLAGS" );
                        if ( rc == 0 && ai->slice == NULL )
                        {
                            rc = VCursorIdRange( ai->curs, ai->idx[ 3 ], &requested_range.first_row, &requested_range.row_count );
                            if ( rc != 0 )
                                log_err( "aligmnet_iter.alig_iter_initialize.VCursorIdRange( '%s'.PRIMARY_ALIGNMENT ) %R", ai->source, rc );
                        }
                        if ( rc == 0 )
                        {
                            ai->to_process.first_row = requested_range.first_row;
                            ai->to_process.row_count = requested_range.row_count;
                            ai->current_row = requested_range.first_row;
                            ai->mode = ALIG_ITER_MODE_CSRA;
                        }
                    }
                    VTableRelease( tbl );
                }
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
    return rc;
}


static rc_t alig_iter_sam_initialize( struct alig_iter * ai, const char * name )
{
    rc_t rc = 0;
    if ( name == NULL )
    {
        rc = KFileMakeStdIn( &ai->file );
        if ( rc != 0 )
            log_err( "error (%R) opening stdin as file", rc );
    }
    else
    {
        rc = KDirectoryNativeDir( &ai->dir );
        if ( rc != 0 )
            log_err( "KDirectoryNativeDir() failed %R", rc );
        else
        {
            rc = KDirectoryOpenFileRead( ai->dir, &ai->file, "%s", name );
            if ( rc != 0 )
                log_err( "error (%R) opening '%s'", rc, name );
        }
    }
    if ( rc == 0 )
    {
        rc = SAMExtractorMake( &ai->sam_extractor, ai->file, 1 );
        if ( rc != 0 )
            log_err( "error (%R) creating sam-extractor from %s", rc, ai->source );
        else
        {
            /* we have to invalidate ( ask the the extractor to internally destroy ) the headers
               even if we did not ask for them!!! */
            rc = SAMExtractorInvalidateHeaders( ai->sam_extractor );
            if ( rc != 0 )
                log_err( "SAMExtractorInvalidateHeaders( '%s' ) failed %R", ai->source, rc );
            else
            {
                ai->sam_alignments_loaded = false;
                ai->sam_alignments_idx = 0;
                ai->sam_alignments_len = 0;
            }
        }
    }
    return rc;
}


static rc_t alig_iter_common_make( struct alig_iter ** ai, uint32_t mode, const char * source, const slice * slice )
{
    rc_t rc = 0;
    if ( ai == NULL || source == NULL )
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
            o->mode = mode;
            o->source = source;
            o->slice = slice;
        }
        
        if ( rc == 0 )
            *ai = o;
        else
            alig_iter_release( o );
    }
    return rc;

}

/* construct an alignmet-iterator from an accession */
rc_t alig_iter_csra_make( struct alig_iter ** ai, const char * acc, size_t cache_capacity, const slice * slice )
{
    rc_t rc = alig_iter_common_make( ai, ALIG_ITER_MODE_CSRA, acc, slice );
    if ( rc == 0 )
    {
        rc = alig_iter_csra_initialize( *ai, cache_capacity );
        if ( rc != 0 )
        {
            alig_iter_release( *ai );
            *ai = NULL;
        }
    }
    return rc;
}


/* construct an alignmet-iterator from a file ( and it's name for error messages ) */
rc_t alig_iter_sam_make( struct alig_iter ** ai, const char * name, const slice * slice )
{
    rc_t rc;
    if ( name == NULL )
        rc = alig_iter_common_make( ai, ALIG_ITER_MODE_SAM, "STDIN", slice );
    else
        rc = alig_iter_common_make( ai, ALIG_ITER_MODE_SAM, name, slice );
    if ( rc == 0 )
    {
        rc = alig_iter_sam_initialize( *ai, name );
        if ( rc != 0 )
        {
            alig_iter_release( *ai );
            *ai = NULL;
        }
    }
    return rc;
}


static rc_t fill_alignment( struct alig_iter * ai, int64_t row_id, AlignmentT * al )
{
    uint32_t elem_bits, boff, row_len;
    
    /* get the CIGAR */
    rc_t rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 0 ], &elem_bits, ( const void ** )&al->cigar.addr, &boff, &row_len );
    if ( rc != 0 )
        log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.CIGAR[ %ld ] %R", ai->source, row_id, rc );
    else
        al->cigar.len = al->cigar.size = row_len;
    
    if ( rc == 0 )
    {
        /* get the REFERENCE-NAME */
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 1 ], &elem_bits, ( const void ** )&al->rname.addr, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_SEQ_ID[ %ld ] %R", ai->source, row_id, rc );
        else
            al->rname.len = al->rname.size = row_len;
    }

    if ( rc == 0 )
    {
        /* get the READ */
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 2 ], &elem_bits, ( const void ** )&al->read.addr, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.READ[ %ld ] %R", ai->source, row_id, rc );
        else
            al->read.len = al->read.size = row_len;
    }
    
    if ( rc == 0 )
    {
        /* get the REFERENCE-POSITION */
        uint32_t * pp;
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 3 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_POS[ %ld ] %R", ai->source, row_id, rc );
        else
            al->pos = pp[ 0 ] + 1; /* to produce the same as the SAM-spec, a 1-based postion! */
    }
    
    if ( rc == 0 )
    {
        /*get the strand and first ( by looking at SAM_FLAGS ) */
        uint32_t * pp;
        rc = VCursorCellDataDirect( ai->curs, row_id, ai->idx[ 4 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.SAM_FLAGS[ %ld ] %R", ai->source, row_id, rc );
        else
            inspect_sam_flags( al, pp[ 0 ] );
    }
    return rc;
}


static bool alig_iter_load_sam_vector( struct alig_iter * ai )
{
    bool res = false;
    rc_t rc = SAMExtractorGetAlignments( ai->sam_extractor, &ai->sam_alignments );
    if ( rc != 0 )
        log_err( "SAMExtractorGetAlignments( '%s' ) failed %R", ai->source, rc );
    else
    {
        ai->sam_alignments_loaded = true;
        ai->sam_alignments_idx = 0;
        ai->sam_alignments_len = VectorLength( &ai->sam_alignments );
        res = ( ai->sam_alignments_len > 0 );
    }
    return res;
}


/* get the next alignemnt from the iter */
bool alig_iter_get( struct alig_iter * ai, AlignmentT * alignment )
{
    bool res = false;
    if ( ai != NULL && alignment != NULL )
    {
        if ( ai->mode == ALIG_ITER_MODE_CSRA )
        {
            res = ( ai->rows_processed < ai->to_process.row_count );
            if ( res )
            {
                rc_t rc = fill_alignment( ai, ai->current_row, alignment );
                if ( rc == 0 )
                {
                    ai->current_row++;
                    ai->rows_processed++;
                }
                else
                    res = false;
            }
        }
        else if ( ai->mode == ALIG_ITER_MODE_SAM )
        {
            res = ai->sam_alignments_loaded;
            if ( !res )
                res = alig_iter_load_sam_vector( ai );
            if ( res && ( ai->sam_alignments_idx >= ai->sam_alignments_len ) )
            {
                rc_t rc = SAMExtractorInvalidateAlignments( ai->sam_extractor );
                if ( rc != 0 )
                {
                    log_err( "SAMExtractorInvalidateAlignments( '%s' ) failed %R", ai->source, rc );
                    res = false;
                }
                else
                    res = alig_iter_load_sam_vector( ai );
            }
            if ( res )
            {
                Alignment * ex_al = VectorGet( &ai->sam_alignments, ai->sam_alignments_idx );
                res = ( ex_al != NULL );
                if ( res )
                {
                    StringInitCString( &alignment->rname, ex_al->rname );
                    StringInitCString( &alignment->cigar, ex_al->cigar );
                    StringInitCString( &alignment->read, ex_al->read );
                    alignment->pos = ex_al->pos;
                    inspect_sam_flags( alignment, ex_al->flags ); /* common.c */
                    ai->sam_alignments_idx++;
                }
            }
        }
    }
    return res;
}
