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

#include "csra-consumer.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_seq_iter_
#include "seq-iter.h"
#endif

#ifndef _h_klib_time_
#include <klib/time.h>
#endif

static uint32_t validate_alig_count_vs_alig_id( seq_rec * rec, uint32_t idx, struct logger * log )
{
    uint32_t res = 0;
    uint8_t count = rec -> alig_count[ idx ];
    if ( count == 0 )
    {
        if ( rec -> prim_alig_id[ idx ] != 0 )
        {
            log_write( log, "SEQ.#%ld : ALIGNMENT_COUNT[ %u ] == 0, PRIMARY_ALIGNMENT_ID[ %u ] = %ld",
                       rec -> row_id, idx, idx, rec -> prim_alig_id[ idx ] );
            res++;
        }
    }
    else if ( count == 1 )
    {
        if ( rec -> prim_alig_id[ idx ] == 0 )
        {
            log_write( log, "SEQ.#%ld : ALIGNMENT_COUNT[ %u ] == 1, PRIMARY_ALIGNMENT_ID[ %u ] == 0",
                       rec -> row_id, idx, idx );
            res++;
        }
    }
    else
    {
        log_write( log, "SEQ.#%ld : ALIGNMENT_COUNT[ %u ] = %u", rec -> row_id, idx, count );
        res++;
    }
    return res;
}

static uint32_t validate_join( validate_slice * slice, seq_rec * rec, uint32_t idx )
{
    uint32_t res = 0;
    bool looking = true;
    rc_t rc = 0;
    while ( rc == 0 && looking )
    {
        uint32_t read_len;
        bool ref_orient, found;

        rc = prim_lookup_get( slice -> lookup, rec -> prim_alig_id[ idx ], &read_len, &ref_orient, &found );
        if ( rc == 0 )
        {
            if ( found )
            {
                looking = false;
            }
            else
            {
                rc = KSleep( 20 );
            }
        }
        else
            looking = false;
    }
    return res;
}

static uint32_t validate_csra_rec( validate_slice * slice, seq_rec * rec )
{
    uint32_t res = 0;
    if ( rec -> num_alig_id != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%ld : rowlen( PRIMARY_ALIGNMENT_ID ) != 2", rec -> row_id );
        res++;
    }
    if ( rec -> num_alig_count != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%ld : rowlen( ALIGNMENT_COUNT ) != 2", rec -> row_id );
        res++;
    }
    
    if ( rec -> num_alig_id > 0 && rec -> num_alig_count > 0 )
    {
        if ( rec -> prim_alig_id[ 0 ] > 0 )
            res += validate_join( slice, rec, 0 );
        res += validate_alig_count_vs_alig_id( rec, 0, slice -> vctx -> log );
    }
    
    if ( rec -> num_alig_id > 1 && rec -> num_alig_count > 1 )
    {
        if ( rec -> prim_alig_id[ 1 ] > 0 )
            res += validate_join( slice, rec, 1 );
        res += validate_alig_count_vs_alig_id( rec, 1, slice -> vctx -> log );
    }
    
    return res;
}

rc_t CC csra_consumer_thread( const KThread *self, void *data )
{
    validate_slice * slice = data;
    cmn_params p = { slice -> vctx -> dir,
                     slice -> vctx -> acc_info -> accession,
                     slice -> first_row,
                     slice -> row_count,
                     slice -> vctx -> cursor_cache }; /* cmn-iter.h */
    struct seq_iter * iter;
    rc_t rc = make_seq_iter( &p, &iter );
    if ( rc == 0 )
    {
        seq_rec rec;
        while ( get_from_seq_iter( iter, &rec, &rc ) && rc == 0 )
        {
            uint32_t errors = validate_csra_rec( slice, &rec );
            rc = update_seq_validate_result( slice -> vctx -> v_res, errors );
            if ( rc == 0 )
                update_progress( slice -> vctx -> progress, 1 );
        }
        destroy_seq_iter( iter );
    }
    finish_validate_result( slice -> vctx -> v_res );
    free( data );
    return rc;
}
