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
#include "coverage_iter.h"
#include "common.h"

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

#define COVERAGE_ITER_N_COLS 3

typedef struct simple_coverage_iter
{
    /* common for both modes: the name of the source */
    const char * source;

    /* for the CSRA-MODE */
    const VDatabase * db;    
    const VCursor *curs;
    uint32_t idx[ COVERAGE_ITER_N_COLS ];
    row_range to_process;
    uint64_t rows_processed;
    int64_t current_row;
    uint64_t position;
    uint32_t last_len;
} simple_coverage_iter;


/* release an reference-iterator */
rc_t simple_coverage_iter_release( struct simple_coverage_iter * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( self->curs != NULL )
        {
            rc = VCursorRelease( self->curs );
            if ( rc != 0 )
                log_err( "error (%R) releasing VCursor for %s", rc, self->source );
        }
        if ( self->db != NULL )
        {
            rc = VDatabaseRelease( self->db );
            if ( rc != 0 )
                log_err( "error (%R) releasing VDatabase for %s", rc, self->source );
           
        }
        free( ( void * ) self );
    }
    return rc;
}

static rc_t simple_coverage_iter_initialize( struct simple_coverage_iter * self,
                                             size_t cache_capacity,
                                             const RefT * ref )
{
    const VDBManager * mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    if ( rc != 0 )
        log_err( "coverage_iter_initialize.initialize.VDBManagerMakeRead() %R", rc );
    else
    {
        rc = VDBManagerOpenDBRead( mgr, &self->db, NULL, "%s", self->source );
        if ( rc != 0 )
            log_err( "coverage_iter_initialize.initialize.VDBManagerOpenDBRead( '%s' ) %R", self->source, rc );
        else
        {
            const VTable * tbl;
            rc = VDatabaseOpenTableRead( self->db, &tbl, "%s", "REFERENCE" );
            if ( rc != 0 )
                log_err( "coverage_iter_initialize.initialize.VDatabaseOpenTableRead( '%s'.REFERENCE ) %R", self->source, rc );
            else
            {
                if ( cache_capacity > 0 )
                    rc = VTableCreateCachedCursorRead( tbl, &self->curs, cache_capacity );
                else
                    rc = VTableCreateCursorRead( tbl, &self->curs );
                if ( rc != 0 )
                    log_err( "coverage_iter_initialize.initialize.TableCreateCursorRead( '%s'.REFERENCE ) %R", self->source, rc );
                else
                {
                    rc = add_cols_to_cursor( self->curs, self->idx, "REFERENCE", self->source,
                                             COVERAGE_ITER_N_COLS,
                                             "PRIMARY_ALIGNMENT_IDS",
                                             "SECONDARY_ALIGNMENT_IDS",
                                             "SEQ_LEN" );
                    if ( rc == 0 )
                    {
                        self->to_process.first_row = ref->start_row_id;
                        self->to_process.row_count = ref->count;
                        self->current_row = ref->start_row_id;
                    }
                }
                VTableRelease( tbl );
            }
        }
        VDBManagerRelease( mgr );
    }
    return rc;
}

/* construct an reference-iterator from an accession */
rc_t simple_coverage_iter_make( struct simple_coverage_iter ** self,
                                const char * src,
                                size_t cache_capacity,
                                const RefT * ref )
{
    rc_t rc = 0;
    if ( self == NULL || src == NULL || ref == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "coverage_iter.make() given a NULL-ptr" );
    }
    else
    {
        simple_coverage_iter * o = calloc( 1, sizeof *o );
        *self = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "coverage_iter_initialize.make() memory exhausted" );
        }
        else
        {
            o->source = src;
            rc = simple_coverage_iter_initialize( o, cache_capacity, ref );
        }
        
        if ( rc == 0 )
            *self = o;
        else
            simple_coverage_iter_release( o );
    }
    return rc;
}


static rc_t fill_simple_coverage_row( struct simple_coverage_iter * self,
                                      int64_t row_id,
                                      SimpleCoverageT * ct )
{
    uint32_t elem_bits, boff, row_len;
    const VCursor * curs = self->curs;
    uint32_t * idx = self->idx;
    int64_t * pp64;
    uint64_t * pp32;
    
    /* get count of PRIM-ID's */
    rc_t rc = VCursorCellDataDirect( curs, row_id, idx[ 0 ], &elem_bits, ( const void ** )&pp64, &boff, &row_len );
    if ( rc != 0 )
        log_err( "cannot read '%s'.REFERENCE.SEQ_ID[ %ld ] %R", self->source, row_id, rc );
    else
    {
        ct->prim = row_len;
        ct->prim_ids = pp64;
    }
    
    if ( rc == 0 )
    {
        /* get count of SEC-ID's */
        rc = VCursorCellDataDirect( curs, row_id, idx[ 1 ], &elem_bits, ( const void ** )&pp64, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.REFERENCE.SEQ_LEN[ %ld ] %R", self->source, row_id, rc );
        else
        {
            ct->sec = row_len;
            ct->sec_ids = pp64;
        }
    }
    if ( rc == 0 )
    {
        /* get SEQ_LEN */
        rc = VCursorCellDataDirect( curs, row_id, idx[ 2 ], &elem_bits, ( const void ** )&pp32, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.REFERENCE.SEQ_LEN[ %ld ] %R", self->source, row_id, rc );
        else
            ct->len = pp32[ 0 ];
    }

    return rc;
}


/* get the next reference from the iter */
bool simple_coverage_iter_get( struct simple_coverage_iter * self,
                               SimpleCoverageT * ct )
{
    bool res = ( self != NULL && ct != NULL  );
    if ( res )
    {
        res = ( self->rows_processed < self->to_process.row_count );
        if ( res )
        {
            self->position += self->last_len;
            res = ( fill_simple_coverage_row( self, self->current_row, ct ) == 0 );
            if ( res )
            {
                self->current_row++;
                self->rows_processed++;
                self->last_len = ct->len;
            }
        }
    }
    return res;
}


bool simple_coverage_iter_get_capped( struct simple_coverage_iter * self,
                                      SimpleCoverageT * ct,
                                      uint32_t min,
                                      uint32_t max )
{
    bool res = false;
    bool running = true;
    while ( running )
    {
        res = simple_coverage_iter_get( self, ct );
        if ( res )
        {
            running = !( ct->prim >= min && ct->prim <= max );
            if ( !running )
            {
                ct->ref_row_id = self->current_row - 1; 
                ct->start_pos = self->position;
            }
        }
        else
            running = false;
    }
    return res;
}


/* ========================================================================================= */

#define ALIG_ITER_N_COLS 2

typedef struct simple_alig_iter
{
    /* common for both modes: the name of the source */
    const char * source;

    /* for the CSRA-MODE */
    const VCursor *curs;
    uint32_t idx[ ALIG_ITER_N_COLS ];
} simple_alig_iter;


static rc_t simple_alig_iter_release( simple_alig_iter * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( self->curs != NULL )
        {
            rc = VCursorRelease( self->curs );
            if ( rc != 0 )
                log_err( "error (%R) releasing VCursor for %s", rc, self->source );
        }
        free( ( void * ) self );
    }
    return rc;
}


static rc_t simple_alig_iter_initialize( simple_alig_iter * self,
                                         const VDatabase * db,
                                         size_t cache_capacity )
{
    const VTable * tbl;
    rc_t rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
    if ( rc != 0 )
        log_err( "simple_alig_iter_initialize.VDatabaseOpenTableRead( '%s'.PRIMARY_ALIGNMENT ) %R", self->source, rc );
    else
    {
        if ( cache_capacity > 0 )
            rc = VTableCreateCachedCursorRead( tbl, &self->curs, cache_capacity );
        else
            rc = VTableCreateCursorRead( tbl, &self->curs );
        if ( rc != 0 )
            log_err( "simple_alig_iter_initialize.TableCreateCursorRead( '%s'.REFERENCE ) %R", self->source, rc );
        else
        {
            rc = add_cols_to_cursor( self->curs, self->idx, "PRIMARY_ALIGNMENT", self->source,
                                      ALIG_ITER_N_COLS,
                                     "REF_POS",
                                     "REF_LEN" );
        }
        VTableRelease( tbl );
    }
    return rc;
}

static rc_t simple_alig_iter_make( simple_alig_iter ** self,
                                    const VDatabase * db,
                                    const char * src,
                                    size_t cache_capacity )
{
    rc_t rc = 0;
    if ( self == NULL || src == NULL || db == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "simple_alig_iter_make() given a NULL-ptr" );
    }
    else
    {
        simple_alig_iter * o = calloc( 1, sizeof *o );
        *self = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "simple_alig_iter_make() memory exhausted" );
        }
        else
        {
            o->source = src;
            rc = simple_alig_iter_initialize( o, db, cache_capacity );
        }
        
        if ( rc == 0 )
            *self = o;
        else
            simple_alig_iter_release( o );
    }
    return rc;
}


static rc_t simple_alig_iter_read( const simple_alig_iter * self,
                                   int64_t row_id,
                                   uint32_t * start,
                                   uint32_t * len )
{
    uint32_t elem_bits, boff, row_len;
    const VCursor * curs = self->curs;
    const uint32_t * idx = self->idx;
    uint64_t * pp32;
    
    /* get REF_POS */
    rc_t rc = VCursorCellDataDirect( curs, row_id, idx[ 0 ], &elem_bits, ( const void ** )&pp32, &boff, &row_len );
    if ( rc != 0 )
        log_err( "cannot read '%s'.REFERENCE.SEQ_ID[ %ld ] %R", self->source, row_id, rc );
    else
        *start = pp32[ 0 ];
    
    if ( rc == 0 )
    {
        /* row_len of HAS_MISMATCH */
        rc = VCursorCellDataDirect( curs, row_id, idx[ 1 ], &elem_bits, ( const void ** )&pp32, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.REFERENCE.SEQ_LEN[ %ld ] %R", self->source, row_id, rc );
        else
            *len = pp32[ 0 ];
    }
    return rc;

}
                            
                            
rc_t detailed_coverage_make( const char * src,
                             size_t cache_capacity,
                             const slice * slice,
                             DetailedCoverage * dcoverage )
{
    rc_t rc = 0;
    if ( src == NULL || slice == NULL || dcoverage == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "detailed_coverage_make() given a NULL-ptr" );
    }
    else
    {
        dcoverage->coverage = calloc( slice->count, sizeof *( dcoverage->coverage ) );
        if ( dcoverage->coverage == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "detailed_coverage_make() memory exhausted" );
        }
        else
        {
            RefT * ref;
            rc = ref_iter_find( &ref, src, cache_capacity, slice->refname );
            if ( rc == 0 )
            {
                simple_coverage_iter * c_iter;
                
                dcoverage->start_pos = slice->start;
                dcoverage->len = slice->count;
                
                /* adjust *ref to restrict it to the given slice... */
                if ( slice->count > 0 )
                {
                    int64_t start_offset = slice->start / ref->block_size;
                    int64_t end_offset = slice->end / ref->block_size;
                    if ( start_offset > 0 ) start_offset--;
                    ref->start_row_id += start_offset;
                    ref->count = ( end_offset - start_offset ) + 1;
                }
                
                rc = simple_coverage_iter_make( &c_iter, src, cache_capacity, ref );
                if ( rc == 0 )
                {
                    simple_alig_iter * a_iter;
                    
                    rc = simple_alig_iter_make( &a_iter, c_iter->db, src, cache_capacity );
                    if ( rc == 0 )
                    {
                        SimpleCoverageT ct;
                        while( rc == 0 &&
                                simple_coverage_iter_get( c_iter, &ct ) )
                        {
                            uint32_t idx;
                            for ( idx = 0; rc == 0 && idx < ct.prim; ++idx )
                            {
                                uint32_t ref_pos, ref_len;
                                rc = simple_alig_iter_read( a_iter, ct.prim_ids[ idx ], &ref_pos, &ref_len );
                                if ( rc == 0 )
                                {
                                    if ( ( ( ref_pos + ref_len - 1 ) >= slice->start ) &&
                                          ref_pos < slice->end )
                                    {
                                        int32_t rel_start = ( ref_pos - ( uint32_t )slice->start );
                                        int32_t i;
                                        
                                        if ( rel_start < 0 )
                                        {
                                            ref_len += rel_start;
                                            rel_start = 0;
                                        }
                                        for ( i = rel_start; i < ref_len && i < slice->count; ++i )
                                        {
                                            dcoverage->coverage[ i ]++;
                                        }
                                    }
                                }
                            }
                        }
                        simple_alig_iter_release( a_iter );    
                    }
                    simple_coverage_iter_release( c_iter );
                }
                RefT_release( ref );                
            }
        }
    }
    return rc;
}


rc_t detailed_coverage_release( DetailedCoverage * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( self->coverage != NULL )
        {
            free( ( void * ) self->coverage );
            self->coverage = NULL;
        }
    }
    return rc;

}