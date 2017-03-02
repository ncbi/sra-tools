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

#include "ref_reader.h"

#include <string.h>
#include <klib/text.h>
#include <vdb/manager.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

/* the ref-reader is a representation of a refseq-accession */ 

typedef struct Ref_Reader
{
    const VCursor * curs;
    char * acc;
    char * buffer;
    
    size_t acc_size, buffer_data;
    uint64_t total_seq_len;
    uint32_t max_seq_len, read_idx;
    uint64_t row_id;
} Ref_Reader;


rc_t ref_reader_release( struct Ref_Reader * rr )
{
    rc_t rc = 0;
    if ( rr == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( rr->acc != NULL )
            free( ( void * ) rr->acc );
        if ( rr->buffer != NULL )
            free( ( void * ) rr->buffer );
        rc = VCursorRelease ( rr->curs );
        free( ( void * ) rr );
    }
    return rc;
}


rc_t ref_reader_make( struct Ref_Reader ** rr, const char * acc )
{
    rc_t rc = 0;

    if ( rr != NULL ) *rr = NULL;

    if ( rr == NULL || acc == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    }
    else
    {
        Ref_Reader * o = calloc( 1, sizeof *o );
        if ( o == NULL )
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
        else
        {
            o->acc_size = string_size( acc );
            o->acc = string_dup( acc, o->acc_size );
            if ( o->acc == NULL )
                rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            else
            {
                const VDBManager * mgr;
                rc = VDBManagerMakeRead( &mgr, NULL );
                if ( rc == 0 )
                {
                    const VTable * tbl;
                    rc = VDBManagerOpenTableRead( mgr, &tbl, NULL, "%s", acc );
                    if ( rc == 0 )
                    {
                        rc = VTableCreateCursorRead( tbl, &o->curs );   
                        if ( rc == 0 )
                        {
                            uint32_t total_seq_len_idx;
                            rc = VCursorAddColumn( o->curs, &total_seq_len_idx, "TOTAL_SEQ_LEN" );
                            if ( rc == 0 )
                            {
                                uint32_t max_seq_len_idx;
                                rc = VCursorAddColumn( o->curs, &max_seq_len_idx, "MAX_SEQ_LEN" );
                                if ( rc == 0 )
                                {
                                    rc = VCursorAddColumn( o->curs, &o->read_idx, "READ" );    
                                    if ( rc == 0 )
                                    {
                                        rc = VCursorOpen( o->curs );
                                        if ( rc == 0 )
                                        {
                                            uint32_t elem_bits, boff, row_len;
                                            const void * base;
                                            rc = VCursorCellDataDirect( o->curs, 1, total_seq_len_idx, &elem_bits, &base, &boff, &row_len );
                                            if ( rc == 0 )
                                            {
                                                o->total_seq_len = * ( ( uint64_t * )base );
                                                rc = VCursorCellDataDirect( o->curs, 1, max_seq_len_idx, &elem_bits, &base, &boff, &row_len );    
                                                if ( rc == 0 )
                                                {
                                                    o->max_seq_len = *( ( uint32_t * )base );
                                                    o->buffer = malloc( o->max_seq_len * 2 );
                                                    if ( o->buffer == NULL )
                                                        rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        VTableRelease( tbl );
                    }
                    VDBManagerRelease( mgr );
                }
            }
            
            if ( rc != 0 )
                ref_reader_release( o );
            else
                *rr = o;
        }
    }
    return rc;
}


rc_t ref_reader_has_name( struct Ref_Reader * rr, const char * acc, bool * has_name )
{
    rc_t rc = 0;
    
    if ( has_name != NULL ) *has_name = false;

    if ( rr == NULL || acc == NULL || has_name == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    else
    {
        size_t b_size = string_size( acc );
        int c = string_cmp( rr->acc, rr->acc_size, acc, b_size, b_size > rr->acc_size ? b_size : rr->acc_size );
        *has_name = ( c == 0 );
    }
    return rc;
}


rc_t ref_reader_get_len( struct Ref_Reader * rr, uint64_t * len )
{
    rc_t rc = 0;

    if ( len != NULL ) *len = 0;
    
    if ( rr == NULL || len == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    else
    {
        *len = rr->total_seq_len;
    }

    return rc;
}


static rc_t ref_reader_read( struct Ref_Reader * rr, int64_t row_id, char * dst, uint32_t * written )
{
    uint32_t elem_bits, boff;
    const void * base;
    rc_t rc = VCursorCellDataDirect( rr->curs, row_id, rr->read_idx, &elem_bits, &base, &boff, written );
    if ( rc == 0 )
        memmove( dst, base, *written );
    return rc;
}


rc_t ref_reader_get( struct Ref_Reader * rr, char ** buffer, uint64_t pos, size_t * available )
{
    rc_t rc = 0;
    
    if ( available != NULL ) *available = 0;
    if ( buffer != NULL ) *buffer = NULL;
    
    if ( rr == NULL || buffer == NULL || available == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    else if ( pos >= rr->total_seq_len )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcOffset, rcTooBig );
    }
    else
    {
        int64_t row_id = ( pos / rr->max_seq_len ) + 1;
        if ( row_id == rr->row_id )
        {
            /* we do not read anything here, the slice get's handed out at the end*/
        }
        else if ( row_id == rr->row_id + 1 && rr->buffer_data > rr->max_seq_len )
        {
            /* we move the 2nd half into the 1st half then read into the second half */
            uint32_t written2;
            rc_t rc2;
            
            memmove( rr->buffer, rr->buffer + rr->max_seq_len, rr->max_seq_len );
            rc2 = ref_reader_read( rr, row_id + 1, rr->buffer + rr->max_seq_len, &written2 );
            if ( rc2 == 0 )
            {
                rr->buffer_data = rr->max_seq_len + written2;
                rr->row_id += 1;
            }
            else
            {
                rr->buffer_data -= rr->max_seq_len;
            }
        }
        else
        {
            /* it is not in the internal buffer, we read 2 blocks in */
            uint32_t written1;
            rc = ref_reader_read( rr, row_id, rr->buffer, &written1 );
            if ( rc == 0 )
            {
                if ( written1 == rr->max_seq_len )
                {
                    uint32_t written2;
                    rc_t rc2 = ref_reader_read( rr, row_id + 1, rr->buffer + written1, &written2 );
                    if ( rc2 == 0 )
                        rr->buffer_data = written1 + written2;
                    else
                        rr->buffer_data = written1;
                }
                else
                    rr->buffer_data = written1;
                if ( rc == 0 )
                    rr->row_id = row_id;
            }
        }
        
        /* now we can pick a slice from the internal buffer... */
        if ( rc == 0 )
        {
            uint64_t buffer_start = ( rr->row_id - 1 ) * rr->max_seq_len;
            if ( pos >= buffer_start )
            {
                size_t offset = pos - buffer_start;
                if ( rr->buffer_data > offset )
                {
                    *buffer = rr->buffer + offset;
                    *available = rr->buffer_data - offset;
                }
            }
        }
    }
    
    return rc;
}
