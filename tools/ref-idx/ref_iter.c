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
#include "ref_iter.h"
#include "common.h"

#include <klib/log.h>
#include <klib/out.h>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

/* ----------------------------------------------------------------------------------------------- */

#define REF_ITER_N_COLS 3

typedef struct RefRow
{
    String rname;
    uint32_t seq_len;
} RefRow;

typedef struct ref_iter
{
    /* common for both modes: the name of the source */
    const char * source;

    /* for the CSRA-MODE */
    const VCursor *curs;
    uint32_t idx[ REF_ITER_N_COLS ];
    row_range to_process;
    uint64_t rows_processed;
    int64_t current_row;
    uint32_t block_size;
} ref_iter;


/* release an reference-iterator */
rc_t ref_iter_release( struct ref_iter * self )
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


static rc_t ref_iter_initialize( struct ref_iter * self, size_t cache_capacity )
{
    const VDBManager * mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    if ( rc != 0 )
        log_err( "ref_iter.initialize.VDBManagerMakeRead() %R", rc );
    else
    {
        const VDatabase * db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", self->source );
        if ( rc != 0 )
            log_err( "ref_iter.initialize.VDBManagerOpenDBRead( '%s' ) %R", self->source, rc );
        else
        {
            const VTable * tbl;
            rc = VDatabaseOpenTableRead( db, &tbl, "%s", "REFERENCE" );
            if ( rc != 0 )
                log_err( "ref_iter.initialize.VDatabaseOpenTableRead( '%s'.REFERENCE ) %R", self->source, rc );
            else
            {
                if ( cache_capacity > 0 )
                    rc = VTableCreateCachedCursorRead( tbl, &self->curs, cache_capacity );
                else
                    rc = VTableCreateCursorRead( tbl, &self->curs );
                if ( rc != 0 )
                    log_err( "ref_iter.initialize.TableCreateCursorRead( '%s'.REFERENCE ) %R", self->source, rc );
                else
                {
                    row_range id_range;
                    rc = add_cols_to_cursor( self->curs, self->idx, "REFERENCE", self->source, REF_ITER_N_COLS,
                                             "SEQ_ID", "SEQ_LEN", "MAX_SEQ_LEN" );
                    if ( rc == 0 )
                    {
                        rc = VCursorIdRange( self->curs, self->idx[ 0 ], &id_range.first_row, &id_range.row_count );
                        if ( rc != 0 )
                            log_err( "ref_iter.initialize.VCursorIdRange( '%s'.REFERENCE ) %R", self->source, rc );
                    }
                    if ( rc == 0 )
                    {
                        self->to_process.first_row = id_range.first_row;
                        self->to_process.row_count = id_range.row_count;
                        self->current_row = id_range.first_row;
                    }
                }
                VTableRelease( tbl );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
    return rc;
}

/* construct an reference-iterator from an accession */
rc_t ref_iter_make( struct ref_iter ** self, const char * acc, size_t cache_capacity )
{
    rc_t rc = 0;
    if ( self == NULL || acc == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "ref_iter.make() given a NULL-ptr" );
    }
    else
    {
        ref_iter * o = calloc( 1, sizeof *o );
        *self = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "ref_iter.make() memory exhausted" );
        }
        else
        {
            o->source = acc;
            rc = ref_iter_initialize( o, cache_capacity );
        }
        
        if ( rc == 0 )
            *self = o;
        else
            ref_iter_release( o );
    }
    return rc;
}


static rc_t fill_ref_row( struct ref_iter * self, int64_t row_id, RefRow * rr )
{
    uint32_t elem_bits, boff, row_len;
    const VCursor * curs = self->curs;
    uint32_t * idx = self->idx;
    
    /* get the REFERENCE-NAME */
    rc_t rc = VCursorCellDataDirect( curs, row_id, idx[ 0 ], &elem_bits, ( const void ** )&rr->rname.addr, &boff, &row_len );
    if ( rc != 0 )
        log_err( "cannot read '%s'.REFERENCE.SEQ_ID[ %ld ] %R", self->source, row_id, rc );
    else
        rr->rname.len = rr->rname.size = row_len;

    if ( rc == 0 )
    {
        /* get SEQ_LEN */
        uint32_t * pp;
        rc = VCursorCellDataDirect( curs, row_id, idx[ 1 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.REFERENCE.SEQ_LEN[ %ld ] %R", self->source, row_id, rc );
        else
            rr->seq_len = pp[ 0 ];
    }

    if ( rc == 0 && self->block_size == 0 )
    {
        /* get MAX_SEQ_LEN */
        uint32_t * pp;
        rc = VCursorCellDataDirect( curs, row_id, idx[ 2 ], &elem_bits, ( const void ** )&pp, &boff, &row_len );
        if ( rc != 0 )
            log_err( "cannot read '%s'.REFERENCE.MAX_SEQ_LEN[ %ld ] %R", self->source, row_id, rc );
        else
            self->block_size = pp[ 0 ];
    }
    return rc;
}


/* get the next reference from the iter */
bool ref_iter_get( struct ref_iter * self, RefT * ref )
{
    bool res = ( self != NULL && ref != NULL );
    if ( res )
    {
        bool searching = true;

        ref->start_row_id = self->current_row;
        ref->count = 0;
        ref->reflen = 0;

        while( searching )
        {
            if ( self->rows_processed >= self->to_process.row_count )
            {
                searching = false;
                res = ( ref->count > 0 );
            }
            else
            {
                RefRow rr;
                if ( fill_ref_row( self, self->current_row, &rr ) != 0 )
                {
                    searching = res = false;
                }
                else
                {
                    int cmp = 0;
                    if ( ref->count == 0 )
                    {
                        ref->rname.addr = rr.rname.addr;
                        ref->rname.len = ref->rname.size = rr.rname.len;
                    }
                    else
                        cmp = StringCompare( &ref->rname, &rr.rname );

                    searching = ( cmp == 0 );
                    if ( searching )
                    {
                        ref->count += 1;
                        ref->reflen += rr.seq_len;
                        self->current_row++;
                        self->rows_processed++;
                    }
                }
            }
        }
        if ( res )
            ref->block_size = self->block_size;
    }
    return res;
}


void RefT_release( RefT * ref )
{
    if ( ref != NULL )
    {
        if ( ref->rname.addr != NULL )
            free( ( void * )ref->rname.addr );
        free( ( void * )ref );
    }
}

static void CC vector_entry_release( void * item, void * data )
{
    RefT_release( item );
}


void ref_iter_release_vector( Vector * vec )
{
    VectorWhack( vec, vector_entry_release, NULL );
}

rc_t RefT_copy( const RefT * src, RefT **dst )
{
    rc_t rc = 0;
    RefT * o = malloc( sizeof * o );
    if ( o == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
        log_err( "RefT_copy() memory exhausted" );
    }
    else
    {
        o->rname.addr = string_dup( src->rname.addr, src->rname.size );
        if ( o->rname.addr == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "RefT_copy() memory exhausted" );
            free( ( void * ) o );
        }
        else
        {
            o->rname.size = src->rname.size;
            o->rname.len = src->rname.len;
            o->start_row_id = src->start_row_id;
            o->count = src->count;
            o->reflen = src->reflen;
            o->block_size = src->block_size;
            *dst = o;
        }
    }
    return rc;
}

rc_t ref_iter_make_vector( Vector * vec, const char * acc, size_t cache_capacity )
{
    rc_t rc = 0;
    if ( vec == NULL || acc == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "ref_iter_make_vector() given a NULL-ptr" );
    }
    else
    {
        struct ref_iter * iter;
        rc = ref_iter_make( &iter, acc, cache_capacity );
        if ( rc == 0 )
        {
            RefT ref;
            VectorInit( vec, 0, 10 );
            while ( rc == 0 && ref_iter_get( iter, &ref ) )
            {
                /* have to make a copy, because vector will outlive the cursor in iter */
                RefT * ref_copy;
                rc = RefT_copy( &ref, &ref_copy );
                if ( rc == 0 )
                {
                    rc = VectorAppend( vec, NULL, ref_copy );
                    if ( rc != 0 )
                        RefT_release( ref_copy );
                }
            }
            rc = ref_iter_release( iter );
            if ( rc != 0 )
                ref_iter_release_vector( vec );
        }
    }
    return rc;
}


rc_t ref_iter_find( RefT ** ref, const char * acc, size_t cache_capacity, const String * rname )
{
    rc_t rc = 0;
    if ( ref == NULL || acc == NULL || rname == NULL  )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "ref_iter_find() given a NULL-ptr" );
    }
    else
    {
        struct ref_iter * iter;
        *ref = NULL;
        rc = ref_iter_make( &iter, acc, cache_capacity );
        if ( rc == 0 )
        {
            RefT rt;
            bool found = false;
            while ( rc == 0 && !found && ref_iter_get( iter, &rt ) )
            {
                found = ( 0 == StringCompare( rname, &rt.rname ) );
                if ( found )
                    rc = RefT_copy( &rt, ref );
            }
            if ( !found )
                rc = RC( rcApp, rcNoTarg, rcAllocating, rcItem, rcNotFound );
            ref_iter_release( iter );
        }
    }
    return rc;
}
