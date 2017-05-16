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
    const String * file_desc;
    SAMExtractor * sam_extractor;
    Vector sam_alignments;
    bool sam_alignments_loaded;
    uint32_t sam_alignments_idx;
    uint32_t sam_alignments_len;
} alig_iter;


/* releae an alignment-iterator */
rc_t alig_iter_release( struct alig_iter * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( self->mode == ALIG_ITER_MODE_CSRA )
        {
            if ( self->curs != NULL )
            {
                rc = VCursorRelease( self->curs );
                if ( rc != 0 )
                    log_err( "error (%R) releasing VCursor for %s", rc, self->source );
            }
        }
        else if ( self->mode == ALIG_ITER_MODE_SAM )
        {
            if ( self->sam_alignments_loaded )
            {
                rc = SAMExtractorInvalidateAlignments( self->sam_extractor );
                if ( rc != 0 )
                    log_err( "SAMExtractorInvalidateAlignments( '%s' ) failed (%R)", self->source, rc );
            }
            if ( rc == 0 )
            {
                rc = SAMExtractorRelease( self->sam_extractor );
                if ( rc != 0 )
                    log_err( "SAMExtractorRelease( '%s' ) failed (%R)", self->source, rc );
            }
            if ( self->file != NULL ) KFileRelease( self->file );
            if ( self->dir != NULL ) KDirectoryRelease( self->dir );
            if ( self->file_desc != NULL ) StringWhack( self->file_desc );
        }
        free( ( void * ) self );
    }
    return rc;
}


static rc_t alig_iter_csra_initialize( struct alig_iter * self, size_t cache_capacity )
{
    const VDBManager * mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    if ( rc != 0 )
        log_err( "aligmnet_iter.alig_iter_initialize.VDBManagerMakeRead() %R", rc );
    else
    {
        const VDatabase * db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", self->source );
        if ( rc != 0 )
            log_err( "aligmnet_iter.alig_iter_initialize.VDBManagerOpenDBRead( '%s' ) %R", self->source, rc );
        else
        {
            row_range requested_range;
            if ( self->slice != NULL )
                rc = slice_2_row_range_db( self->slice, self->source, db, &requested_range );
                
            if ( rc == 0 )
            {
                const VTable * tbl;
                rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
                if ( rc != 0 )
                    log_err( "aligmnet_iter.alig_iter_initialize.VDatabaseOpenTableRead( '%s'.PRIMARY_ALIGNMENT ) %R", self->source, rc );
                else
                {
                    if ( cache_capacity > 0 )
                        rc = VTableCreateCachedCursorRead( tbl, &self->curs, cache_capacity );
                    else
                        rc = VTableCreateCursorRead( tbl, &self->curs );
                    if ( rc != 0 )
                        log_err( "aligmnet_iter.alig_iter_initialize.TableCreateCursorRead( '%s'.PRIMARY_ALIGNMENT ) %R", self->source, rc );
                    else
                    {
                        rc = add_cols_to_cursor( self->curs, self->idx, "PRIMARY_ALIGNMENT", self->source, ALIG_ITER_N_COLS,
                                                 "CIGAR_SHORT", "REF_SEQ_ID", "READ", "REF_POS", "SAM_FLAGS" );
                        if ( rc == 0 && self->slice == NULL )
                        {
                            rc = VCursorIdRange( self->curs, self->idx[ 3 ], &requested_range.first_row, &requested_range.row_count );
                            if ( rc != 0 )
                                log_err( "aligmnet_iter.alig_iter_initialize.VCursorIdRange( '%s'.PRIMARY_ALIGNMENT ) %R", self->source, rc );
                        }
                        if ( rc == 0 )
                        {
                            self->to_process.first_row = requested_range.first_row;
                            self->to_process.row_count = requested_range.row_count;
                            self->current_row = requested_range.first_row;
                            self->mode = ALIG_ITER_MODE_CSRA;
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


static rc_t alig_iter_sam_initialize( struct alig_iter * self, const char * name )
{
    rc_t rc = 0;
    if ( name == NULL )
    {
        rc = KFileMakeStdIn( &self->file );
        if ( rc != 0 )
            log_err( "error (%R) opening stdin as file", rc );
        else
        {
            String S;
            StringInitCString( &S, "STDIN" );
            StringCopy( &self->file_desc, &S );
        }
    }
    else
    {
        rc = KDirectoryNativeDir( &self->dir );
        if ( rc != 0 )
            log_err( "KDirectoryNativeDir() failed %R", rc );
        else
        {
            rc = KDirectoryOpenFileRead( self->dir, &self->file, "%s", name );
            if ( rc != 0 )
                log_err( "error (%R) opening '%s'", rc, name );
            else
            {
                String S;
                StringInitCString( &S, name );
                StringCopy( &self->file_desc, &S );
            }
        }
    }
    if ( rc == 0 )
    {
    
        rc = SAMExtractorMake( &self->sam_extractor,
                               self->file,
                               ( String * )self->file_desc,
                               1 );
        if ( rc != 0 )
            log_err( "error (%R) creating sam-extractor from %S", rc, self->file_desc );
        else
        {
            /* we have to invalidate ( ask the the extractor to internally destroy ) the headers
               even if we did not ask for them!!! */
            rc = SAMExtractorInvalidateHeaders( self->sam_extractor );
            if ( rc != 0 )
                log_err( "SAMExtractorInvalidateHeaders( '%s' ) failed %R", self->source, rc );
            else
            {
                self->sam_alignments_loaded = false;
                self->sam_alignments_idx = 0;
                self->sam_alignments_len = 0;
            }
        }
    }
    return rc;
}


static rc_t alig_iter_common_make( struct alig_iter ** self, uint32_t mode, const char * source, const slice * slice )
{
    rc_t rc = 0;
    if ( self == NULL || source == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "aligmnet_iter.alig_iter_make() given a NULL-ptr" );
    }
    else
    {
        alig_iter * o = calloc( 1, sizeof *o );
        *self = NULL;
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
            *self = o;
        else
            alig_iter_release( o );
    }
    return rc;
}

/* construct an alignmet-iterator from an accession */
rc_t alig_iter_csra_make( struct alig_iter ** self, const char * acc, size_t cache_capacity, const slice * slice )
{
    rc_t rc = alig_iter_common_make( self, ALIG_ITER_MODE_CSRA, acc, slice );
    if ( rc == 0 )
    {
        rc = alig_iter_csra_initialize( *self, cache_capacity );
        if ( rc != 0 )
        {
            alig_iter_release( *self );
            *self = NULL;
        }
    }
    return rc;
}


/* construct an alignmet-iterator from a file ( and it's name for error messages ) */
rc_t alig_iter_sam_make( struct alig_iter ** self, const char * name, const slice * slice )
{
    rc_t rc = alig_iter_common_make( self, ALIG_ITER_MODE_SAM, name == NULL ? "STDIN" : name, slice );
    if ( rc == 0 )
    {
        rc = alig_iter_sam_initialize( *self, name );
        if ( rc != 0 )
        {
            alig_iter_release( *self );
            *self = NULL;
        }
    }
    return rc;
}


static rc_t fill_alignment( struct alig_iter * self, int64_t row_id, AlignmentT * al )
{
    uint32_t elem_bits, boff, row_len;
    const VCursor * curs = self->curs;
    uint32_t * idx = self->idx;
    
    /* get the CIGAR */
    rc_t rc = VCursorCellDataDirect( curs, row_id, idx[ 0 ], &elem_bits, ( const void ** )&al->cigar.addr, &boff, &row_len );
    if ( rc != 0 )
        log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.CIGAR[ %ld ] %R", self->source, row_id, rc );
    else
        al->cigar.len = al->cigar.size = row_len;
    
    if ( rc == 0 )
    {
        /* get the REFERENCE-NAME */
        rc = VCursorCellDataDirect( curs, row_id, idx[ 1 ], &elem_bits, ( const void ** )&al->rname.addr, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_SEQ_ID[ %ld ] %R", self->source, row_id, rc );
        else
            al->rname.len = al->rname.size = row_len;
    }

    if ( rc == 0 )
    {
        /* get the READ */
        rc = VCursorCellDataDirect( curs, row_id, idx[ 2 ], &elem_bits, ( const void ** )&al->read.addr, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.READ[ %ld ] %R", self->source, row_id, rc );
        else
            al->read.len = al->read.size = row_len;
    }
    
    if ( rc == 0 )
    {
        /* get the REFERENCE-POSITION */
        uint32_t * pp;
        rc = VCursorCellDataDirect( curs, row_id, idx[ 3 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.REF_POS[ %ld ] %R", self->source, row_id, rc );
        else
            al->pos = pp[ 0 ] + 1; /* to produce the same as the SAM-spec, a 1-based postion! */
    }
    
    if ( rc == 0 )
    {
        /*get the strand and first ( by looking at SAM_FLAGS ) */
        uint32_t * pp;
        rc = VCursorCellDataDirect( curs, row_id, idx[ 4 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.PRIMARY_ALIGNMENT.SAM_FLAGS[ %ld ] %R", self->source, row_id, rc );
        else
            inspect_sam_flags( al, pp[ 0 ] );
    }
    return rc;
}


static bool alig_iter_load_sam_vector( struct alig_iter * self )
{
    bool res = false;
    rc_t rc = SAMExtractorGetAlignments( self->sam_extractor, &self->sam_alignments );
    if ( rc != 0 )
        log_err( "SAMExtractorGetAlignments( '%s' ) failed %R", self->source, rc );
    else
    {
        self->sam_alignments_loaded = true;
        self->sam_alignments_idx = 0;
        self->sam_alignments_len = VectorLength( &self->sam_alignments );
        res = ( self->sam_alignments_len > 0 );
    }
    return res;
}


/* get the next alignemnt from the iter */
bool alig_iter_get( struct alig_iter * self, AlignmentT * alignment )
{
    bool res = false;
    if ( self != NULL && alignment != NULL )
    {
        if ( self->mode == ALIG_ITER_MODE_CSRA )
        {
            res = ( self->rows_processed < self->to_process.row_count );
            if ( res )
            {
                rc_t rc = fill_alignment( self, self->current_row, alignment );
                if ( rc == 0 )
                {
                    self->current_row++;
                    self->rows_processed++;
                }
                else
                    res = false;
            }
        }
        else if ( self->mode == ALIG_ITER_MODE_SAM )
        {
            res = self->sam_alignments_loaded;
            if ( !res )
                res = alig_iter_load_sam_vector( self );
            if ( res && ( self->sam_alignments_idx >= self->sam_alignments_len ) )
            {
                rc_t rc = SAMExtractorInvalidateAlignments( self->sam_extractor );
                if ( rc != 0 )
                {
                    log_err( "SAMExtractorInvalidateAlignments( '%s' ) failed %R", self->source, rc );
                    res = false;
                }
                else
                    res = alig_iter_load_sam_vector( self );
            }
            if ( res )
            {
                Alignment * ex_al = VectorGet( &self->sam_alignments, self->sam_alignments_idx );
                res = ( ex_al != NULL );
                if ( res )
                {
                    StringInitCString( &alignment->rname, ex_al->rname );
                    StringInitCString( &alignment->cigar, ex_al->cigar );
                    StringInitCString( &alignment->read, ex_al->read );
                    alignment->pos = ex_al->pos;
                    inspect_sam_flags( alignment, ex_al->flags ); /* common.c */
                    self->sam_alignments_idx++;
                }
            }
        }
    }
    return res;
}
