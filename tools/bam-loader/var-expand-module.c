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

static rc_t realloc_buffers( var_expand_data * data, uint32_t new_len )
{
    rc_t rc = 0;

    if ( data->seq_buffer_len < new_len )
    {
        if ( data->seq_buffer != NULL )
            free( ( void * )data->seq_buffer );
        data->seq_buffer = malloc( new_len * 2 );
        if ( data->seq_buffer == NULL )
            rc = RC ( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        else
            data->seq_buffer_len = new_len * 2;
    }
    
    if ( data->ref_buffer_len < new_len )
    {
        if ( data->ref_buffer != NULL )
            free( ( void * )data->ref_buffer );
        data->ref_buffer = malloc( new_len * 2 );
        if ( data->ref_buffer == NULL )
            rc = RC ( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        else
            data->ref_buffer_len = new_len * 2;
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
        var_expand_data * tmp = malloc( sizeof( *tmp ) );
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
                    tmp->log_pos = 0;
                    tmp->alignments_seen = 0;
                    tmp->seq_buffer = NULL;
                    tmp->seq_buffer_len = 0;
                    tmp->ref_buffer = NULL;
                    tmp->ref_buffer_len = 0;

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


rc_t var_expand_handle( var_expand_data * data, BAM_Alignment const *alignment, struct ReferenceSeq const *refSequence )
{
    rc_t rc = 0;
    if ( data == NULL || alignment == NULL || refSequence == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    else
    {
        uint32_t seq_len;
        
        /* increment how many alignments we have seen - for testing purpose */
        data->alignments_seen++;
        
        /* get the length of the read out of the alignment */
        rc = BAM_AlignmentGetReadLength( alignment, &seq_len );
        if ( rc == 0 )
        {
            /* ( eventually ) expand the 2 buffers in our var_expand_data buffer to fit the read and reference */
            rc = realloc_buffers( data, seq_len );
            if ( rc == 0 )
            {
                /* get the read out of the alignment and put it in our single buffer */
                rc = BAM_AlignmentGetSequence( alignment, data->seq_buffer );
                if ( rc == 0 )
                {
                    int64_t pos_on_ref;
                    uint32_t len_on_ref;
                    /* get the position on the reference out of the alignment */
                    rc = BAM_AlignmentGetPosition2( alignment, &pos_on_ref, &len_on_ref );
                    if ( rc == 0 )
                    {
                        uint32_t cigar_op_count;
                        /* get how many cigar-operations we have for this alignment */
                        rc = BAM_AlignmentGetCigarCount( alignment, &cigar_op_count );
                        if ( rc == 0 )
                        {

                            INSDC_coord_zero ref_offset = pos_on_ref;
                            INSDC_coord_len ref_len = len_on_ref;
                            INSDC_coord_len actual_ref_len = 0;
                            
                            rc = ReferenceSeq_Read( refSequence, ref_offset, ref_len, data->ref_buffer, &actual_ref_len );
                        
                            if ( rc == 0 )
                            {
                                log_this( data, "#%d:\n", data->alignments_seen );
                                log_this( data, "\t'%.*s' at [%ld.%d], len=%d, cig-ops=%d\n",
                                    seq_len, data->seq_buffer, pos_on_ref, len_on_ref, seq_len, cigar_op_count );
                                log_this( data, "\t'%.*s' at [%ld.%d]\n\n",
                                    ref_len, data->ref_buffer, pos_on_ref, actual_ref_len );
                                
                            }
/*
                            rc_t BAM_AlignmentGetCigar ( const BAM_Alignment *self,
    uint32_t n, BAMCigarType *type, uint32_t *length );
*/
                        }
                    }
                }
            }
        } 
    }
    return rc;
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
