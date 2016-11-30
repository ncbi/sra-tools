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

#include "var-expand-module.h"
#include "Globals.h" // defines global variable G which has things like command line options
#include "bam.h"
#include <klib/printf.h>
#include <klib/text.h>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <align/writer-reference.h>

static rc_t log_this( var_expand_data * data, const char * fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( rc == 0 )
    {
        size_t num_writ2;
        rc = KFileWrite( data->log, data->log_pos, buffer, num_writ, &num_writ2 );
        if ( rc == 0 )
            data->log_pos += num_writ2;
    }   
    va_end( list );
    return rc;
} 

static rc_t realloc_seq_buffer( var_expand_data * data )
{
    rc_t rc = 0;
    if ( data->seq_buffer_len < data->seq_len )
    {
        if ( data->seq_buffer != NULL )
            free( ( void * )data->seq_buffer );
        data->seq_buffer_len = data->seq_len * 2;
        data->seq_buffer = malloc( data->seq_buffer_len );
        if ( data->seq_buffer == NULL )
        {
            rc = RC ( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            data->seq_buffer_len = 0;
        }
    }
    return rc;
}

static rc_t realloc_ref_buffer( var_expand_data * data )
{
    rc_t rc = 0;
    if ( data->ref_buffer_len < data->ref_len )
    {
        if ( data->ref_buffer != NULL )
            free( ( void * )data->ref_buffer );
        data->ref_buffer_len = data->ref_len * 2;
        data->ref_buffer = malloc( data->ref_buffer_len );
        if ( data->ref_buffer == NULL )
        {
            rc = RC ( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            data->ref_buffer_len = 0;
        }
    }
    return rc;
}


rc_t var_expand_init( var_expand_data ** data )
{
    rc_t rc = 0;
    if ( data == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    else
    {
        var_expand_data * tmp = calloc( 1, sizeof( *tmp ) );
        if ( tmp == NULL )
        {
            rc = RC ( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            rc = KDirectoryNativeDir_v1( &tmp->dir );
            if ( rc == 0 )
            {
                rc = KDirectoryCreateFile ( tmp->dir, &tmp->log, false, 0664, kcmInit, "var-expand.log" );
                if ( rc == 0 )
                {
                    log_this( tmp, "started\n" );
                    *data = tmp;
                }
                else
                    tmp->log = NULL;
            }
            else
                tmp->dir = NULL;

            if ( rc != 0 )
            {
                var_expand_finish( tmp );
            }
        }
    }
    return rc;
}


variation * make_variation( const char * bases, INSDC_coord_len len, INSDC_coord_zero offset )
{
    variation * res;
    size_t vs = len + sizeof( *res );
    res = malloc( vs );
    if ( res != NULL )
    {
        res->offset = offset;
        res->len = len;
        res->count = 1;
        memcpy( &( res->bases[ 0 ] ), bases, ( size_t )len );
    }
    return res;
}


static void var_on_event( var_expand_data * data, BAMCigarType cigar_op, uint32_t cigar_op_len,
                          INSDC_coord_zero * offset_into_ref, INSDC_coord_zero *offset_into_read )
{
    const uint8_t * p;
    const INSDC_coord_zero abs_offset_into_ref = *offset_into_ref + data->seq_pos_on_ref;
    uint32_t variation_len = cigar_op_len;
    
    if ( cigar_op == ct_Delete )
        p = &( data->ref_buffer[ *offset_into_ref ] );
    else if ( cigar_op == ct_Match )
    {
        uint32_t matched_bases = string_match( (char *)&( data->ref_buffer[ *offset_into_ref ] ), cigar_op_len,
                                               (char *)&( data->seq_buffer[ *offset_into_read ] ), cigar_op_len,
                                               cigar_op_len, NULL );
        if ( matched_bases < cigar_op_len )
        {
            p = &( data->seq_buffer[ *offset_into_read ] );    
        }
        else
            p = NULL;
    }
    else
        p = &( data->seq_buffer[ *offset_into_read ] );

    log_this( data, "'%c'.%d at %ld = '%.*s'\n",
        cigar_op, cigar_op_len, abs_offset_into_ref, variation_len, p );
    
    if ( p != NULL )
    {
        variation * var = make_variation( ( char * )p, variation_len, abs_offset_into_ref );
        if ( var != NULL )
            DLListPushTail( &data->variations, ( DLNode * )var );
    }
    
    if ( cigar_op == ct_Insert )
    {
        *offset_into_read += cigar_op_len;
    }
    else if ( cigar_op == ct_Delete )
    {
        *offset_into_ref += cigar_op_len;
    }
    else
    {
        *offset_into_ref += cigar_op_len;
        *offset_into_read += cigar_op_len;
    }

}


rc_t var_expand_handle( var_expand_data * data, BAM_Alignment const *alignment, struct ReferenceSeq const *refSequence )
{
    rc_t rc = 0;
    if ( data == NULL || alignment == NULL || refSequence == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    else
    {
        /* increment how many alignments we have seen - for testing purpose */
        data->alignments_seen++;
        log_this( data, "------------------------------#%d:\n", data->alignments_seen );
        
        /* get the length of the read out of the alignment */
        rc = BAM_AlignmentGetReadLength( alignment, &data->seq_len );
        if ( rc == 0 )
        {
            /* ( eventually ) expand the 2 buffers in our var_expand_data buffer to fit the read and reference */
            rc = realloc_seq_buffer( data );
            if ( rc == 0 )
            {
                /* get the read out of the alignment and put it in our single buffer */
                rc = BAM_AlignmentGetSequence( alignment, ( char * )data->seq_buffer );
                if ( rc == 0 )
                {
                    /* get the position on the reference out of the alignment */
                    rc = BAM_AlignmentGetPosition2( alignment, &data->seq_pos_on_ref, &data->seq_len_on_ref );
                    if ( rc == 0 )
                    {
                        INSDC_coord_zero ref_offset = data->seq_pos_on_ref;
                        INSDC_coord_len actual_ref_len = 0;

                        if ( data->seq_pos_on_ref < data->last_seq_pos_on_ref )
                        {
                            log_this( data, "!!!alignments out of order!!! %ld->%ld\n",
                                data->last_seq_pos_on_ref, data->seq_pos_on_ref );
                        }
                        data->last_seq_pos_on_ref = data->seq_pos_on_ref;
                        
                        data->ref_len = data->seq_len_on_ref;
                        log_this( data, "\tseq: '%.*s' at [%ld.%d], len=%d\n",
                            data->seq_len, data->seq_buffer, data->seq_pos_on_ref, data->seq_len_on_ref, data->seq_len );

                        rc = realloc_ref_buffer( data );
                        if ( rc == 0 )
                        {
                            rc = ReferenceSeq_Read( refSequence, ref_offset, data->ref_len, data->ref_buffer, &actual_ref_len );
                            if ( rc == 0 )
                            {
                                log_this( data, "\tref: '%.*s' at [%ld.%d]\n",
                                    data->ref_len, data->ref_buffer, data->seq_pos_on_ref, actual_ref_len );
                            
                                {
                                    uint32_t cigar_op_count, idx;
                                    INSDC_coord_zero offset_into_ref = 0;
                                    INSDC_coord_zero offset_into_read = 0;
                                    
                                    /* get how many cigar-operations we have for this alignment */
                                    rc = BAM_AlignmentGetCigarCount( alignment, &cigar_op_count );
                                    for ( idx = 0; rc == 0 && idx < cigar_op_count; ++ idx )
                                    {
                                        BAMCigarType cigar_op;
                                        uint32_t cigar_op_len;
                                        rc = BAM_AlignmentGetCigar( alignment, idx, &cigar_op, &cigar_op_len );
                                        if ( rc == 0 )
                                        {
                                            var_on_event( data, cigar_op, cigar_op_len, &offset_into_ref, &offset_into_read );
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            log_this( data, "\n" );
        } 
    }
    return rc;
}


static void cleanup_variations( var_expand_data * data )
{
    DLNode * node = DLListPopHead( &data->variations );
    while ( node != NULL )
    {
        variation * var = ( variation * )node;
        log_this( data, "var at %ld = '%.*s'\n", var->offset, var->len, var->bases );
        free( ( void * ) node );
        node = DLListPopHead( &data->variations );    
    }
}

rc_t var_expand_finish( var_expand_data * data )
{
    rc_t rc = 0;
    if ( data == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    else
    {
        log_this( data, "%ld alignments seen\n", data->alignments_seen );
        
        cleanup_variations( data );
        
        if ( data->log != NULL )
            KFileRelease( data->log );
        if ( data->dir != NULL )
            KDirectoryRelease( data->dir );
        if ( data->seq_buffer != NULL )
            free( ( void * )data->seq_buffer );
        if ( data->ref_buffer != NULL )
            free( ( void * )data->ref_buffer );
        free( ( void * ) data );
    }
    return rc;
}
