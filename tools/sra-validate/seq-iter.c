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

#include "seq-iter.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

typedef struct seq_iter
{
    struct cmn_iter * cmn; /* cmn-iter.h */
    /* uint32_t name_id, cmp_read_id; */
    uint32_t prim_alig_id, alig_count_id, read_len_id, read_type_id, qual_id;
} seq_iter;


void destroy_seq_iter( struct seq_iter * self )
{
    if ( self != NULL )
    {
        destroy_cmn_iter( self -> cmn ); /* cmn_iter.h */
        free( ( void * ) self );
    }
}

rc_t make_seq_iter( const cmn_params * params, seq_iter ** iter )
{
    rc_t rc = 0;
    seq_iter * i = calloc( 1, sizeof * i );
    if ( i == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_seq_iter().calloc( %d ) -> %R", ( sizeof * i ), rc );
    }
    else
    {
        rc = make_cmn_iter( params, "SEQUENCE", &( i -> cmn ) ); /* cmn-iter.h */

        /*
        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "NAME", &( i -> name_id ) );

        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "CMP_READ", &( i -> cmp_read_id ) );
        */
        
        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "PRIMARY_ALIGNMENT_ID", &( i -> prim_alig_id ) ); /* cmn-iter.h */

        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "ALIGNMENT_COUNT", &( i -> alig_count_id ) ); /* cmn-iter.h */

        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "READ_LEN", &( i -> read_len_id ) ); /* cmn-iter.h */

        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "READ_TYPE", &( i -> read_type_id ) ); /* cmn-iter.h */

        if ( rc == 0 )
            rc = cmn_iter_add_column( i -> cmn, "QUALITY", &( i -> qual_id ) ); /* cmn-iter.h */
            
        if ( rc == 0 )
            rc = cmn_iter_range( i -> cmn, i -> prim_alig_id ); /* cmn-iter.h */
                
        if ( rc != 0 )
            destroy_seq_iter( i ); /* above */
        else
            *iter = i;
    }
    return rc;
}

bool get_from_seq_iter( seq_iter * self, seq_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( self -> cmn, rc );
    if ( res )
    {
        rc_t rc1;
        rec -> row_id = cmn_iter_get_row_id( self -> cmn );

        rc1 = cmn_read_uint64_array( self -> cmn, self -> prim_alig_id, &( rec -> prim_alig_id ), &( rec -> num_alig_id ) );
        
        if ( rc1 == 0 )
            rc1 = cmn_read_uint8_array( self -> cmn, self -> alig_count_id, &( rec -> alig_count ), &( rec -> num_alig_count ) );

        /*
        if ( rc1 == 0 )
            rc1 = cmn_read_String( self -> cmn, self -> name_id, &( rec -> name ) );
        
        if ( rc1 == 0 )
            rc1 = cmn_read_String( self -> cmn, self -> cmp_read_id, &( rec -> cmp_read ) );
        */

        if ( rc1 == 0 )
            rc1 = cmn_read_uint32_array( self -> cmn, self -> read_len_id, &rec -> read_len, &( rec -> num_read_len ) );
        
        if ( rc1 == 0 )
            rc1 = cmn_read_uint8_array( self -> cmn, self -> read_type_id, &rec -> read_type, &( rec -> num_read_type ) );
        
        if ( rc1 == 0 )
            rc1 = cmn_read_uint8_array( self -> cmn, self -> qual_id, &rec -> qual, &( rec -> num_qual ) );
        
        if ( rc != NULL )
            *rc = rc1;
    }   
    return res;
}

uint64_t get_row_count_of_seq_iter( seq_iter * self )
{
    return cmn_iter_row_count( self -> cmn );
}
