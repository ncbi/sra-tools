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

#include "csra-producer.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_prim_iter_
#include "prim-iter.h"
#endif

/* from kapp/main.h */
rc_t CC Quitting ( void );

rc_t CC csra_producer_thread( const KThread *self, void *data )
{
    validate_slice * slice = data;
    cmn_params p = { slice -> vctx -> dir,
                     slice -> vctx -> acc_info . accession,
                     slice -> first_row,
                     slice -> row_count,
                     slice -> vctx -> cursor_cache }; /* cmn-iter.h */
    struct prim_iter * iter;
    rc_t rc = make_prim_iter( &p, &iter );
    if ( rc == 0 )
    {
        prim_rec rec;
        bool running = true;
        while ( running && rc == 0 )
        {
            running = ( Quitting() == 0 );
            if ( running )
                running = get_from_prim_iter( iter, &rec, &rc );
            if ( running )
            {
                rc = prim_lookup_enter( slice -> lookup, &rec );
                if ( rc == 0 )
                    rc = update_prim_validate_result( slice -> vctx -> v_res, 0 );
                if ( rc == 0 )
                    update_progress( slice -> vctx -> progress, 1 );
            }
        }
        destroy_prim_iter( iter );
    }
    finish_validate_result( slice -> vctx -> v_res );
    free( data );
    return rc;
}
