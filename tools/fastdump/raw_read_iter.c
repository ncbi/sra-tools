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

#include "raw_read_iter.h"
#include "cmn_iter.h"
#include "helper.h"

#include <os-native.h>
#include <sysalloc.h>

typedef struct raw_read_iter
{
    struct cmn_iter * cmn;
    uint32_t seq_spot_id, seq_read_id, raw_read_id;
} raw_read_iter;


void destroy_raw_read_iter( struct raw_read_iter * iter )
{
    if ( iter != NULL )
    {
        destroy_cmn_iter( iter->cmn );
        free( ( void * ) iter );
    }
}


rc_t make_raw_read_iter( cmn_params * params, struct raw_read_iter ** iter )
{
    
    rc_t rc = 0;
    raw_read_iter * i = calloc( 1, sizeof * i );
    if ( i == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_raw_read_iter.calloc( %d ) -> %R", ( sizeof * i ), rc );
    }
    else
    {
        rc = make_cmn_iter( params, "PRIMARY_ALIGNMENT", &i->cmn );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i->cmn, "SEQ_SPOT_ID", &i->seq_spot_id );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i->cmn, "SEQ_READ_ID", &i->seq_read_id );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i->cmn, "(INSDC:4na:bin)RAW_READ", &i->raw_read_id );
        if ( rc == 0 )
            rc = cmn_iter_range( i->cmn, i->seq_spot_id );

        if ( rc != 0 )
            destroy_raw_read_iter( i );
        else
            *iter = i;
    }
    return rc;
}


bool get_from_raw_read_iter( struct raw_read_iter * iter, raw_read_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( iter->cmn, rc );
    if ( res )
    {
        *rc = cmn_read_uint64( iter->cmn, iter->seq_spot_id, &rec->seq_spot_id );
        if ( *rc == 0 )
            *rc = cmn_read_uint32( iter->cmn, iter->seq_read_id, &rec->seq_read_id );
        if ( *rc == 0 )
            *rc = cmn_read_String( iter->cmn, iter->raw_read_id, &rec->raw_read );
    }
    return res;
}


uint64_t get_row_count_of_raw_read( struct raw_read_iter * iter )
{
    return cmn_iter_row_count( iter->cmn );
}
