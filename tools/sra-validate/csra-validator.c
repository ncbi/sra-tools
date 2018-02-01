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

#include "csra-validator.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_prim_iter_
#include "prim-iter.h"
#endif

#ifndef _h_seq_iter_
#include "seq-iter.h"
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

static uint32_t validate_csra_rec( seq_rec * rec, struct logger * log )
{
    uint32_t res = 0;
    if ( rec -> num_alig_id != 2 )
    {
        log_write( log, "SEQ.#%ld : rowlen( PRIMARY_ALIGNMENT_ID ) != 2", rec -> row_id );
        res++;
    }
    if ( rec -> num_alig_count != 2 )
    {
        log_write( log, "SEQ.#%ld : rowlen( ALIGNMENT_COUNT ) != 2", rec -> row_id );
        res++;
    }
    if ( rec -> num_alig_id > 0 && rec -> num_alig_count > 0 )
        res += validate_alig_count_vs_alig_id( rec, 0, log );
    if ( rec -> num_alig_id > 1 && rec -> num_alig_count > 1 )
        res += validate_alig_count_vs_alig_id( rec, 1, log );

    return res;
}

typedef struct csra_seq_slice
{
    const KDirectory * dir;
    const acc_info_t * acc_info;
    struct logger * log;
    struct validate_result * v_res;
    struct progress * progress;
    size_t cursor_cache;
    int64_t first_row;
    uint64_t row_count;
    uint32_t slice_nr;
} csra_seq_slice;

static rc_t CC validate_slice( const KThread *self, void *data )
{
    csra_seq_slice * slice = data;
    cmn_params p = { slice -> dir,
                     slice -> acc_info -> accession,
                     slice -> first_row,
                     slice -> row_count,
                     slice -> cursor_cache }; /* cmn-iter.h */
    struct seq_iter * i;
    rc_t rc = make_seq_iter( &p, &i );
    if ( rc == 0 )
    {
        seq_rec rec;
        while ( get_from_seq_iter( i, &rec, &rc ) && rc == 0 )
        {
            rc = update_validate_result( slice -> v_res, validate_csra_rec( &rec, slice -> log ) );
            if ( rc == 0 )
                update_progress( slice -> progress, 1 );
        }
        destroy_seq_iter( i );
    }
    free( data );
    return rc;
    
}

rc_t run_csra_validator_slice( const KDirectory * dir,
                               const acc_info_t * acc_info,
                               struct logger * log,
                               struct validate_result * v_res,
                               struct thread_runner * threads,
                               struct progress * progress,
                               int64_t first_row,
                               uint64_t row_count,
                               uint32_t slice_nr,
                               size_t cursor_cache )
{
    rc_t rc = 0;
    csra_seq_slice * slice = malloc( sizeof * slice );
    if ( slice == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "run_csra_validator_slice().malloc( %d ) -> %R", ( sizeof * slice ), rc );
    }
    else
    {
        slice -> dir = dir;
        slice -> acc_info = acc_info;
        slice -> log = log;
        slice -> v_res = v_res;
        slice -> progress = progress;
        slice -> cursor_cache = cursor_cache;
        slice -> first_row = first_row;
        slice -> row_count = row_count;
        slice -> slice_nr = slice_nr;
        
        rc = thread_runner_add( threads, validate_slice, slice );
        if ( rc != 0 )
            free( ( void * ) slice );
    }
    return rc;
}

rc_t run_csra_validator( const KDirectory * dir,
                         const acc_info_t * acc_info,
                         struct logger * log,
                         struct validate_result * v_res,
                         struct thread_runner * threads,
                         struct progress * progress,
                         uint32_t num_slices,
                         size_t cursor_cache )
{
    rc_t rc = 0;
    int64_t row = 1;
    uint64_t rows_per_slice = ( acc_info -> seq_rows / num_slices ) + 1;
    uint32_t slice_nr = 0;
    
    start_progress( progress, 2, acc_info -> seq_rows );
    while ( rc == 0 && slice_nr < num_slices )
    {
        rc = run_csra_validator_slice( dir,
                                       acc_info,
                                       log,
                                       v_res,
                                       threads,
                                       progress,
                                       row,
                                       rows_per_slice,
                                       slice_nr++,
                                       cursor_cache );
        if ( rc == 0 )
            row += rows_per_slice;
    }
    return rc;
}
