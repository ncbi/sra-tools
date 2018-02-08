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

/* the seq-rec with values from the lookup */
typedef struct csra_rec
{
    seq_rec rec;
    uint32_t read_len[ 2 ];
    bool ref_orient[ 2 ];
    bool found[ 2 ];
} csra_rec;

static uint32_t validate_alig_count_vs_alig_id( const seq_rec * rec, uint32_t idx, struct logger * log )
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


static uint32_t validate_csra_rec( validate_slice * slice, const csra_rec * csra )
{
    uint32_t res = 0;
    const seq_rec * rec = &csra -> rec;
    if ( rec -> num_alig_id != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%ld : rowlen( PRIMARY_ALIGNMENT_ID ) != 2", rec -> row_id );
        res++;
    }
    if ( csra -> rec . num_alig_count != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%ld : rowlen( ALIGNMENT_COUNT ) != 2", rec -> row_id );
        res++;
    }
    if ( rec -> num_alig_id != rec -> num_alig_count )
    {
        log_write( slice -> vctx -> log, "SEQ.#%ld : rowlen( ALIGNMENT_COUNT )[%u] != rowlen( PRIMARY_ALIGNMENT_ID )[ %u]",
                rec -> row_id, rec -> num_alig_count, rec -> num_alig_id );
        res++;
    }

    res += validate_alig_count_vs_alig_id( rec, 0, slice -> vctx -> log );
    res += validate_alig_count_vs_alig_id( rec, 1, slice -> vctx -> log );
    
    return res;
}

static rc_t fill_in_lookup_values( csra_rec * csra, struct prim_lookup * lookup, bool * ready )
{
    rc_t rc = 0;
    * ready = true;
    uint64_t prim_alig_id = csra -> rec . prim_alig_id[ 0 ];
    if ( prim_alig_id > 0 )
    {
        rc = prim_lookup_get( lookup, prim_alig_id,
                &csra -> read_len[ 0 ], &csra -> ref_orient[ 0 ], &csra -> found[ 0 ] );
        if ( rc == 0 )
        {
            if ( !csra -> found[ 0 ] )
                *ready = false;
        }
    }
    if ( rc == 0 )
    {
        prim_alig_id = csra -> rec . prim_alig_id[ 1 ];
        if ( prim_alig_id > 0 )
        {
            rc = prim_lookup_get( lookup, prim_alig_id,
                    &csra -> read_len[ 1 ], &csra -> ref_orient[ 1 ], &csra -> found[ 1 ] );
            if ( rc == 0 )
            {
                if ( !csra -> found[ 1 ] )
                    *ready = false;
            }
        }
    }
    return rc;
}

static csra_rec * copy_csra_rec( const csra_rec * src )
{
    /* problem: with have to make a deep-copy!!! */
}

static void CC on_backlog_free( void * item, void * data )
{
    free( item );
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
        csra_rec csra;
        Vector backlog;
        
        VectorInit ( &backlog, 0, 512 );
        
        while ( get_from_seq_iter( iter, &csra . rec, &rc ) && rc == 0 )
        {
            bool ready;
            rc = fill_in_lookup_values( &csra, slice -> lookup, &ready );
            if ( rc == 0 )
            {
                if ( ready )
                {
                    /* all joins ( if any ) could be made, do the validation */
                    uint32_t errors = validate_csra_rec( slice, &csra );
                    rc = update_seq_validate_result( slice -> vctx -> v_res, errors );
                    if ( rc == 0 )
                        update_progress( slice -> vctx -> progress, 1 );
                }
                else
                {
                    /* some/all joins are not read( yet ) put this one on the back-log */
                    copy_csra_rec( const csra_rec * src )
                }
            }
        }
        
        VectorWhack ( &backlog, on_backlog_free, NULL );

        destroy_seq_iter( iter );
    }
    finish_validate_result( slice -> vctx -> v_res );
    free( data );
    return rc;
}
