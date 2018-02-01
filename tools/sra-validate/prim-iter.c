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

#include "prim-iter.h"
#include "cmn.h"
#include "cmn-iter.h"

typedef struct prim_iter
{
    struct cmn_iter * cmn;
    uint32_t seq_spot_id, seq_read_id, ref_orient_id, read_len_id;
} prim_iter;


void destroy_prim_iter( prim_iter * self )
{
    if ( self != NULL )
    {
        destroy_cmn_iter( self -> cmn );
        free( ( void * ) self );
    }
}

rc_t make_prim_iter( cmn_params * params, prim_iter ** iter )
{
    
    rc_t rc = 0;
    prim_iter * i = calloc( 1, sizeof * i );
    if ( i == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "prim-iter.c make_prim_iter().calloc( %d ) -> %R", ( sizeof * i ), rc );
    }
    else
    {
        rc = make_cmn_iter( params, "PRIMARY_ALIGNMENT", &i -> cmn );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "SEQ_SPOT_ID", &i -> seq_spot_id );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "SEQ_READ_ID", &i -> seq_read_id );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "REF_ORIENTATION", &i -> ref_orient_id );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "READ_LEN", &i -> read_len_id );

        if ( rc == 0 )
            rc = cmn_iter_range( i -> cmn, i -> seq_spot_id );

        if ( rc != 0 )
            destroy_prim_iter( i );
        else
            *iter = i;
    }
    return rc;
}

bool get_from_prim_iter( prim_iter * self, prim_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( self -> cmn, rc );
    if ( res )
    {
        rc_t rc1 = cmn_read_uint64( self -> cmn, self -> seq_spot_id, &rec -> seq_spot_id );
        if ( rc1 == 0 )
        {
            uint32_t value;
            rc1 = cmn_read_uint32( self -> cmn, self -> seq_read_id, &value );
            if ( rc1 == 0 )
                rec -> ref_orient = ( uint8_t )( value & 0xFF );
        }
        if ( rc1 == 0 )
            rc1 = cmn_read_uint8( self -> cmn, self -> ref_orient_id, &rec -> ref_orient );
        if ( rc1 == 0 )
            rc1 = cmn_read_uint32( self -> cmn, self -> read_len_id, &rec -> read_len );
        if ( rc != 0 )
            *rc = rc1;
    }
    return res;
}

uint64_t get_prim_row_count( prim_iter * self )
{
    return cmn_iter_row_count( self -> cmn );
}
