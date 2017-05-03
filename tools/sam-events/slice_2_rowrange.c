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

#include "slice_2_rowrange.h"

#include <klib/text.h>
#include <klib/log.h>
#include <klib/printf.h>
#include <klib/out.h>
 
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

typedef struct search_ctx
{
    uint32_t a_idx[ 4 ];
    uint32_t state;
    uint32_t MAX_SEQ_LEN;
    int64_t last;
    const VCursor * curs;
    const char * acc;
    const struct slice * slice;
    struct row_range * alig_range;
} search_ctx;


/*
    0 ... search for sctx->slice->refname
    1 ... sctx->slice->refname found, searching for sctx->slice->start
    2 ... sctx->slice->refname found, searching last row of refname
    3 ... sctx->slice->refname found, sctx->slice->start found, searching for end of slice
    4 ... end of slice found...
*/


static rc_t get_String_from_cursor( const VCursor * curs, uint32_t col_idx, int64_t row_id, String * S )
{
    uint32_t elem_bits, boff;
    rc_t rc = VCursorCellDataDirect( curs, row_id, col_idx, &elem_bits, ( const void ** )&S->addr, &boff, &S->len );
    if ( rc != 0 )
        log_err( "get_String_from_cursor().VCursorCellDataDirect( REFERENCE[ %ld / %d ] ) %R", row_id, col_idx, rc );
    else
        S->size = S->len;
    return rc;
}


static rc_t get_u32_from_cursor( const VCursor * curs, uint32_t col_idx, int64_t row_id, uint32_t * value )
{
    uint32_t elem_bits, boff, rowlen;
    uint32_t * ptr;
    rc_t rc = VCursorCellDataDirect( curs, row_id, col_idx, &elem_bits, ( const void ** )&ptr, &boff, &rowlen );
    if ( rc != 0 )
        log_err( "get_u32_from_cursor().VCursorCellDataDirect( REFERENCE[ %ld / %d ] ) %R", row_id, col_idx, rc );
    else
        *value = ptr[ 0 ];
    return rc;
}


static rc_t get_u8_from_cursor( const VCursor * curs, uint32_t col_idx, int64_t row_id, uint8_t * value )
{
    uint32_t elem_bits, boff, rowlen;
    uint8_t * ptr;
    rc_t rc = VCursorCellDataDirect( curs, row_id, col_idx, &elem_bits, ( const void ** )&ptr, &boff, &rowlen );
    if ( rc != 0 )
        log_err( "get_u32_from_cursor().VCursorCellDataDirect( REFERENCE[ %ld / %d ] ) %R", row_id, col_idx, rc );
    else
        *value = ptr[ 0 ];
    return rc;
}


static rc_t get_first_last_i64_from_cursor( const VCursor * curs, uint32_t col_idx, int64_t row_id, int64_t * first, int64_t * last )
{
    uint32_t elem_bits, boff, rowlen;
    int64_t * ptr;
    rc_t rc = VCursorCellDataDirect( curs, row_id, col_idx, &elem_bits, ( const void ** )&ptr, &boff, &rowlen );
    if ( rc != 0 )
        log_err( "get_first_i64_from_cursor().VCursorCellDataDirect( REFERENCE[ %ld / %d ] ) %R", row_id, col_idx, rc );
    else
    {
        if ( rowlen > 0 )
        {
            if ( first != NULL ) *first = ptr[ 0 ];
            if ( last != NULL ) *last = ptr[ rowlen - 1 ];
        }
    }
    return rc;
}


static rc_t get_i64_vector_cursor( const VCursor * curs, uint32_t col_idx, int64_t row_id, int64_t ** ptr, uint32_t * count )
{
    uint32_t elem_bits, boff;
    rc_t rc = VCursorCellDataDirect( curs, row_id, col_idx, &elem_bits, ( const void ** )ptr, &boff, count );
    if ( rc != 0 )
        log_err( "get_first_i64_from_cursor().VCursorCellDataDirect( REFERENCE[ %ld / %d ] ) %R", row_id, col_idx, rc );
    return rc;
}


static rc_t has_refname( search_ctx * sctx, int64_t row_id, bool * res )
{
    String S;
    rc_t rc = get_String_from_cursor( sctx->curs, sctx->a_idx[ 0 ], row_id, &S );
    *res = false;
    if ( rc == 0 )
    {
        if ( StringCompare( &S, sctx->slice->refname ) == 0 )
            *res = true;
        else
        {
            rc = get_String_from_cursor( sctx->curs, sctx->a_idx[ 1 ], row_id, &S );
            if ( StringCompare( &S, sctx->slice->refname ) == 0 )
                *res = true;
        }
    }
    return rc;
}

/* we are looking for the first row in the reference-table that has the searched for name */
static rc_t slice_2_row_range_loop_state0( search_ctx * sctx, int64_t row_id )
{
    bool found;
    rc_t rc = has_refname( sctx, row_id, &found );
    if ( rc == 0 && found )
    {
        /* we have found the first row in the reference-table that has the looked-for name ! */
        if ( sctx->slice->count > 0 )
        {
            /* the slice does ask for a specific range on that reference ... */
            
            int64_t row_id_start = row_id;
            int64_t row_id_end = row_id;
            
            /* if the start-offset is beyond the first 5K block */
            if ( sctx->slice->start >= sctx->MAX_SEQ_LEN )
                row_id_start += ( ( sctx->slice->start / sctx->MAX_SEQ_LEN ) - 1 );
            
            if ( sctx->slice->end >= sctx->MAX_SEQ_LEN )
                row_id_end += ( sctx->slice->end / sctx->MAX_SEQ_LEN );
            
            if ( row_id_start == row_id_end )
            {
                /* start and end of the request are in the same 5K block */
                rc = get_first_last_i64_from_cursor( sctx->curs, sctx->a_idx[ 2 ], row_id_start, &sctx->alig_range->first_row, &sctx->last );
            }
            else
            {
                /* start and end of the request are in different 5K blocks */
                rc = get_first_last_i64_from_cursor( sctx->curs, sctx->a_idx[ 2 ], row_id_start, &sctx->alig_range->first_row, NULL );
                if ( rc == 0 )
                {
                    /* let us see if the refname is still there at ref_end */
                    found = false;
                    while( rc == 0 && ( !found ) && ( row_id_end > row_id_start ) )
                    {
                        rc = has_refname( sctx, row_id_end, &found );
                        if ( rc == 0 )
                        {
                            if ( found )
                                rc = get_first_last_i64_from_cursor( sctx->curs, sctx->a_idx[ 2 ], row_id_end, NULL, &sctx->last );
                            else
                                row_id_end--;
                        }
                    }
                }
            }
            if ( rc == 0 )
                sctx->alig_range->row_count = ( ( sctx->last - sctx->alig_range->first_row ) + 1 );
            
            sctx->state = 2; /* we are done */
        }
        else
        {
            /* the slice does ask for the whole reference ... */
            
            /* we record the alignment-row-id it starts with and keep on searching for the end... */
            rc = get_first_last_i64_from_cursor( sctx->curs, sctx->a_idx[ 2 ], row_id, &sctx->alig_range->first_row, &sctx->last );
            sctx->state += 1;
        }
    }
    return rc;
}


/* we are in the right chromosome/reference, let as search for the end of it */
static rc_t slice_2_row_range_loop_state1( search_ctx * sctx, int64_t row_id )
{
    bool found;
    rc_t rc = has_refname( sctx, row_id, &found );
    if ( rc == 0 )
    {
        if ( found )
        {
            /* we are still in the same reference, record the last alignment-row-id */
            if ( sctx->alig_range->first_row == 0 )
                rc = get_first_last_i64_from_cursor( sctx->curs, sctx->a_idx[ 2 ], row_id, &sctx->alig_range->first_row, &sctx->last );
            else
                rc = get_first_last_i64_from_cursor( sctx->curs, sctx->a_idx[ 2 ], row_id, NULL, &sctx->last );
        }
        else
        {
            /* it is not the same reference any more, calculate the length and done */
            sctx->alig_range->row_count = ( ( sctx->last - sctx->alig_range->first_row ) + 1 );
            sctx->state += 1;
        }
    }
    return rc;
}


static rc_t slice_2_row_range_tbl( const struct slice * slice, const char * acc, const VTable * tbl, struct row_range * range )
{
    size_t capacity = 1024 * 1024 * 32;
    search_ctx sctx;
    rc_t rc = VTableCreateCachedCursorRead( tbl, &sctx.curs, capacity );
    if ( rc != 0 )
        log_err( "slice_2_row_range_tbl().VTableCreateCachedCursorRead( '%s'.REFERENCE, %lu ) %R", acc, capacity, rc );
    else
    {
        sctx.acc = acc;
        rc = add_cols_to_cursor( sctx.curs, sctx.a_idx, "REFERENCE", acc, 4,
                "SEQ_ID", "NAME", "PRIMARY_ALIGNMENT_IDS", "MAX_SEQ_LEN" );
        if ( rc == 0 )
        {
            row_range ref_rows;        
            rc = VCursorIdRange( sctx.curs, sctx.a_idx[ 2 ], &ref_rows.first_row, &ref_rows.row_count );
            if ( rc != 0 )
                log_err( "slice_2_row_range_tbl().VCursorIdRange( '%s'.REFERENCE ) %R", acc, rc );
            else
            {
                int64_t row_id = ref_rows.first_row;
                rc = get_u32_from_cursor( sctx.curs, sctx.a_idx[ 3 ], row_id, &sctx.MAX_SEQ_LEN );
                if ( rc == 0 )
                {
                    bool running = true;
                    uint64_t rows_processed = 0;

                    sctx.slice = slice;
                    sctx.alig_range = range;
                    sctx.state = 0;
                    
                    while ( rc == 0 && running )
                    {
                        if ( sctx.state == 0 )
                            rc = slice_2_row_range_loop_state0( &sctx, row_id );
                        else if ( sctx.state == 1 )
                            rc = slice_2_row_range_loop_state1( &sctx, row_id );
                       
                        rows_processed++;
                        if ( rows_processed >= ref_rows.row_count )
                            running = false;
                        else if ( sctx.state < 2 )
                            row_id++;
                        else
                            running = false;
                    }
                    
                    if ( rc == 0 )
                    {
                        if ( sctx.state == 0 )
                        {
                            /* we are still in state 0, that means reference not found ! */
                            rc = SILENT_RC( rcApp, rcNoTarg, rcSearching, rcRange, rcNotFound );
                        }
                        else if ( sctx.state == 1 )
                        {
                            /* we are still in state 1, that means end of reference not found! ( because last one ) */
                            sctx.alig_range->row_count = ( ( sctx.last - sctx.alig_range->first_row ) + 1 );
                        }
                    }
                }
            }
        }
        VCursorRelease( sctx.curs );
    }
    return rc;
}


rc_t slice_2_row_range_db( const struct slice * slice, const char * acc, const VDatabase * db, struct row_range * range )
{
    rc_t rc = 0;
    if ( slice == NULL || acc == NULL || range == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "slice_2_row_range() given a NULL-ptr" );
    }
    else
    {
        const VTable * tbl;

        range->first_row = 0;
        range->row_count = 0;
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", "REFERENCE" );
        if ( rc != 0 )
            log_err( "slice_2_row_range_db().VDatabaseOpenTableRead( '%s'.REFERENCE ) %R", acc, rc );
        else
        {
            rc = slice_2_row_range_tbl( slice, acc, tbl, range );
            VTableRelease( tbl );
        }
    }
    return rc;
}


rc_t slice_2_row_range( const struct slice * slice, const char * acc, struct row_range * range )
{
    rc_t rc = 0;
    if ( slice == NULL || acc == NULL || range == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "slice_2_row_range() given a NULL-ptr" );
    }
    else
    {
        const VDBManager * mgr;    

        range->first_row = 0;
        range->row_count = 0;
        rc = VDBManagerMakeRead( &mgr, NULL );
        if ( rc != 0 )
            log_err( "slice_2_row_range().VDBManagerMakeRead() %R", rc );
        else
        {
            const VDatabase * db;
            rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", acc );
            if ( rc != 0 )
                log_err( "slice_2_row_range().VDBManagerOpenDBRead( '%s' ) %R", acc, rc );
            else
            {
                rc = slice_2_row_range_db( slice, acc, db, range );
                VDatabaseRelease( db );
            }
            VDBManagerRelease( mgr );
        }
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

/*
typedef struct BlockInfo
{
    int64_t first_alignment_id;
    uint32_t alignment_id_count;
    bool sorted;
} BlockInfo;

typedef struct RefInfo
{
    const String * name;
    const String * seq_id;
    int64_t first_row_in_reftable;
    uint64_t row_count_in_reftable;
    int64_t first_alignment_id;
    uint64_t alignment_id_count;
    uint64_t len;
    Vector blocks; // vector of pointers to BlockInfo structs
    uint32_t blocksize;    
    bool circular;    
    bool sorted;
} RefInfo;
*/

static void release_BlockInfo( void * item, void * data )
{
    if ( item != NULL )
    {
        free( ( void * ) item );
    }
}


static bool is_sorted( const int64_t * block, uint32_t count )
{
    int64_t value = block[ 0 ];
    uint32_t idx = 1;
    while( idx < count )
    {
        int64_t curr = block[ idx++ ];
        if ( curr < value ) return false;
        value = curr;
    }
    return true;
}

static void initialize_BlockInfo( BlockInfo *self, const int64_t * block, uint32_t count )
{
    if ( count > 0 )
    {
        self->first_alignment_id = block[ 0 ];
        self->count = count;
        self->sorted = is_sorted( block, count );
    }
}

static rc_t make_BlockInfo( BlockInfo ** self, int64_t * block, uint32_t count )
{
    rc_t rc = 0;
    if ( self != NULL ) *self = NULL;
    if ( self == NULL || block == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "make_BlockInfo() given a NULL-ptr" );
    }
    else
    {
        BlockInfo * o = calloc( 1, sizeof *o );
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "make_BlockInfo() memory exhausted" );
        }
        else
            initialize_BlockInfo( o, block, count );

        if ( rc == 0 )
            *self = o;
        else
            release_BlockInfo( o, NULL );
    }
    return rc;
}


static void release_RefInfo( void * item, void * data )
{
    if ( item != NULL )
    {
        RefInfo * self = item;
        if ( self->name != NULL ) StringWhack ( self->name );
        if ( self->seq_id != NULL ) StringWhack( self->seq_id );
        VectorWhack( &self->blocks, release_BlockInfo, NULL );
        free( ( void * ) self );
    }
}


static rc_t initialize_RefInfo( RefInfo *self, const String * name, const String * seq_id,
                                uint32_t blocksize, bool circular, int64_t first_row_in_reftable )
{
    rc_t rc = StringCopy( &self->name, name );
    if ( rc != 0 )
        log_err( "initialize_RefInfo().StringCopy( name ) = %R", rc );
    else
    {
        rc = StringCopy( &self->seq_id, seq_id );
        if ( rc != 0 )
            log_err( "initialize_RefInfo().StringCopy( seq_id ) = %R", rc );
        else
        {
            self->first_row_in_reftable = first_row_in_reftable;
            self->blocksize = blocksize;
            self->circular = circular;
            self->sorted = true;
            VectorInit( &self->blocks, 0, 10 );
        }
    }
    return rc;
}


static rc_t make_RefInfo( RefInfo **self, const String * name, const String * seq_id,
                          uint32_t blocksize, bool circular, int64_t first_row_in_reftable )
{
    rc_t rc = 0;
    if ( self != NULL ) *self = NULL;
    if ( self == NULL || name == NULL || seq_id == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "make_RefInfo() given a NULL-ptr" );
    }
    else
    {
        RefInfo * o = calloc( 1, sizeof *o );
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "make_RefInfo() memory exhausted" );
        }
        else
            rc = initialize_RefInfo( o, name, seq_id, blocksize, circular, first_row_in_reftable );
        
        if ( rc == 0 )
            *self = o;
        else
            release_RefInfo( o, NULL );
    }
    return rc;
}


static bool RefInfoMatch( const RefInfo * self, const String * name, const String * seq_id )
{
    bool res = ( StringCompare( self->name, name ) == 0 );
    if ( res ) res = ( StringCompare( self->seq_id, seq_id ) == 0 );
    return res;
}


void clear_reference_info( Vector * info ) { VectorWhack( info, release_RefInfo, NULL ); }


static rc_t extract_reference_info_tbl( const char * acc, const VTable * tbl, Vector * info_vector )
{
    size_t capacity = 1024 * 1024 * 32;
    const VCursor * curs;
    rc_t rc = VTableCreateCachedCursorRead( tbl, &curs, capacity );
    if ( rc != 0 )
        log_err( "extract_reference_info_tbl().VTableCreateCachedCursorRead( '%s'.REFERENCE, %lu ) %R", acc, capacity, rc );
    else
    {
        uint32_t a_idx[ 6 ];
        rc = add_cols_to_cursor( curs, a_idx, "REFERENCE", acc, 6,
            "SEQ_ID", "NAME", "PRIMARY_ALIGNMENT_IDS", "MAX_SEQ_LEN", "CIRCULAR", "READ_LEN" );
        if ( rc == 0 )
        {
            row_range ref_rows;        
            rc = VCursorIdRange( curs, a_idx[ 2 ], &ref_rows.first_row, &ref_rows.row_count );
            if ( rc != 0 )
                log_err( "extract_reference_info_tbl().VCursorIdRange( '%s'.REFERENCE ) %R", acc, rc );
            else
            {
                int64_t row_id = ref_rows.first_row;
                uint32_t max_seq_len;
                rc = get_u32_from_cursor( curs, a_idx[ 3 ], row_id, &max_seq_len );
                if ( rc == 0 )
                {
                    bool running = true;
                    uint64_t rows_processed = 0;
                    RefInfo * info = NULL;
                    
                    while ( rc == 0 && running )
                    {
                        String SEQ_ID, NAME;
                        rc = get_String_from_cursor( curs, a_idx[ 0 ], row_id, &SEQ_ID );
                        if ( rc == 0 )
                        {
                            rc = get_String_from_cursor( curs, a_idx[ 1 ], row_id, &NAME );
                            if ( rc == 0 )
                            {
                                uint8_t circular;
                                if ( info == NULL )
                                {
                                    rc = get_u8_from_cursor( curs, a_idx[ 4 ], row_id, &circular );
                                    if ( rc == 0 )
                                        rc = make_RefInfo( &info, &NAME, &SEQ_ID, max_seq_len, circular != 0, row_id );
                                }
                                else if ( !RefInfoMatch( info, &NAME, &SEQ_ID ) )
                                {
                                    rc = VectorAppend( info_vector, NULL, info );
                                    if ( rc == 0 )
                                    {
                                        rc = get_u8_from_cursor( curs, a_idx[ 4 ], row_id, &circular );
                                        if ( rc == 0 )
                                            rc = make_RefInfo( &info, &NAME, &SEQ_ID, max_seq_len, circular != 0, row_id );
                                    }
                                }
                                
                                if ( rc == 0 )
                                {
                                    uint32_t read_len;
                                    info->row_count_in_reftable++;
                                    rc = get_u32_from_cursor( curs, a_idx[ 5 ], row_id, &read_len );
                                    if ( rc == 0 )
                                    {
                                        int64_t *ids;
                                        uint32_t count;
                                        
                                        info->len += read_len;
                                        rc = get_i64_vector_cursor( curs, a_idx[ 2 ], row_id, &ids, &count );
                                        if ( rc == 0 && count > 0 )
                                        {
                                            BlockInfo * blk;
                                            
                                            if ( info->first_alignment_id == 0 )
                                                info->first_alignment_id = ids[ 0 ];
                                            info->alignment_id_count = ( ids[ count - 1 ] - info->first_alignment_id ) + 1;
                                            
                                            rc = make_BlockInfo( &blk, ids, count );
                                            if ( rc == 0 )
                                            {
                                                if ( !blk->sorted ) info->sorted = false;
                                                rc = VectorAppend( &info->blocks, NULL, blk );
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        rows_processed++;
                        if ( rows_processed >= ref_rows.row_count )
                            running = false;
                        else
                            row_id++;
                    }
                    if ( info != NULL )
                        rc = VectorAppend( info_vector, NULL, info );
                }
            }
        }
        VCursorRelease( curs );
    }
    return rc;
}

rc_t extract_reference_info( const char * acc, Vector * info )
{
    rc_t rc = 0;
    if ( acc == NULL || info == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "extract_reference_info() given a NULL-ptr" );
    }
    else
    {
        const VDBManager * mgr;

        VectorInit( info, 0, 10 );
        KOutMsg( "reporting\n" );
        rc = VDBManagerMakeRead( &mgr, NULL );
        if ( rc != 0 )
            log_err( "extract_reference_info().VDBManagerMakeRead() %R", rc );
        else
        {
            const VDatabase * db;
            rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", acc );
            if ( rc != 0 )
                log_err( "extract_reference_info().VDBManagerOpenDBRead( '%s' ) %R", acc, rc );
            else
            {
                const VTable * tbl;
                rc = VDatabaseOpenTableRead( db, &tbl, "%s", "REFERENCE" );
                if ( rc != 0 )
                    log_err( "slice_2_row_range_db().VDatabaseOpenTableRead( '%s'.REFERENCE ) %R", acc, rc );
                else
                {
                    rc = extract_reference_info_tbl( acc, tbl, info );
                    VTableRelease( tbl );
                }
                VDatabaseRelease( db );
            }
            VDBManagerRelease( mgr );
        }
    }
    return rc;
}
