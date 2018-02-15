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

#ifndef _h_prim_lookup_
#include "prim-lookup.h"
#endif

#ifndef _h_csra_consumer_
#include "csra-consumer.h"
#endif

#ifndef _h_csra_producer_
#include "csra-producer.h"
#endif

static rc_t make_csra_common( const validate_ctx * vctx,
                          rc_t ( CC * thread_function ) ( const KThread *self, void *data ),
                          int64_t row_1,
                          uint64_t row_count,
                          struct prim_lookup * lookup )
{
    rc_t rc = 0;
    int64_t row = row_1;
    uint64_t rows_per_slice = ( row_count / vctx -> num_slices ) + 1;
    uint32_t slice_nr = 0;
    
    while ( rc == 0 && slice_nr < vctx -> num_slices )
    {
        validate_slice * slice = malloc( sizeof * slice );
        if ( slice == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_csra_consumers().malloc( %d ) -> %R", ( sizeof * slice ), rc );
        }
        else
        {
            slice -> vctx = vctx;
            slice -> first_row = row;
            slice -> row_count = rows_per_slice;
            slice -> slice_nr = slice_nr;
            slice -> lookup = lookup;

            rc = thread_runner_add( vctx -> threads, thread_function, slice );
            if ( rc == 0 )
            {
                row += rows_per_slice;
                slice_nr++;
            }
            else
                free( ( void * ) slice );
        }
    }
    return rc;
}

static rc_t make_csra_producers( const validate_ctx * vctx, struct prim_lookup * lookup )
{
    rc_t rc = make_csra_common( vctx,
                                csra_producer_thread,
                                1,
                                vctx -> acc_info . prim_rows,
                                lookup );
    return rc;
}

static rc_t make_csra_consumers( const validate_ctx * vctx, struct prim_lookup * lookup )
{
    rc_t rc = make_csra_common( vctx,
                                csra_consumer_thread,
                                1,
                                vctx -> acc_info . seq_rows,
                                lookup );
    return rc;
}

rc_t run_csra_validator( const validate_ctx * vctx )
{
    struct prim_lookup * lookup;
    rc_t rc = make_prim_lookup( &lookup );
    if ( rc == 0 )
    {
        uint64_t total_rows = vctx -> acc_info . seq_rows + vctx -> acc_info . prim_rows;
        start_progress( vctx -> progress, 2, total_rows );
        
        set_to_finish_validate_result( vctx -> v_res, vctx -> num_slices * 2 );
    
        rc = make_csra_producers( vctx, lookup );
        if ( rc == 0 )
        {
            rc = make_csra_consumers( vctx, lookup );
            if ( rc == 0 )
            {
                /* wait until all consumer-threads have finished */
                wait_for_validate_result( vctx -> v_res, 100 );
            }
        }
        
        prim_lookup_report( lookup );
        destroy_prim_lookup( lookup );
    }
    return rc;
}
