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

#include "fastq_iter.h"
#include "helper.h"

typedef struct fastq_csra_iter
{
    struct cmn_iter * cmn; /* cmn_iter.h */
    fastq_iter_opt opt; /* fastq_iter.h */
    uint32_t name_id, prim_alig_id, cmp_read_id, quality_id, read_len_id, read_type_id;
} fastq_csra_iter;


void destroy_fastq_csra_iter( struct fastq_csra_iter * self )
{
    if ( self != NULL )
    {
        destroy_cmn_iter( self -> cmn ); /* cmn_iter.h */
        free( ( void * ) self );
    }
}

rc_t make_fastq_csra_iter( const cmn_params * params,
                           fastq_iter_opt opt,
                           struct fastq_csra_iter ** iter )
{
    rc_t rc = 0;
    fastq_csra_iter * self = calloc( 1, sizeof * self );
    if ( self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_fastq_csra_iter.calloc( %d ) -> %R", ( sizeof * self ), rc );
    }
    else
    {
        self -> opt = opt;
        rc = make_cmn_iter( params, "SEQUENCE", &( self -> cmn ) ); /* cmn_iter.h */
        
        if ( rc == 0 && opt . with_name )
            rc = cmn_iter_add_column( self -> cmn, "NAME", &( self -> name_id ) ); /* cmn_iter.h */

        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "PRIMARY_ALIGNMENT_ID", &( self -> prim_alig_id ) ); /* cmn_iter.h */

        if ( rc == 0 && opt . with_cmp_read )
            rc = cmn_iter_add_column( self -> cmn, "CMP_READ", &( self -> cmp_read_id ) ); /* cmn_iter.h */

        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "(INSDC:quality:text:phred_33)QUALITY", &( self -> quality_id ) ); /* cmn_iter.h */

        if ( rc == 0 && opt . with_read_len )
            rc = cmn_iter_add_column( self -> cmn, "READ_LEN", &( self -> read_len_id ) ); /* cmn_iter.h */

        if ( rc == 0 && opt . with_read_type )
            rc = cmn_iter_add_column( self -> cmn, "READ_TYPE", &( self -> read_type_id ) ); /* cmn_iter.h */

        if ( rc == 0 )
            rc = cmn_iter_range( self -> cmn, self -> prim_alig_id ); /* cmn_iter.h */
            
        if ( rc != 0 )
            destroy_fastq_csra_iter( self ); /* above */
        else
            *iter = self;
    }
    return rc;
}

bool get_from_fastq_csra_iter( struct fastq_csra_iter * self, fastq_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( self -> cmn, rc );
    if ( res )
    {
        rec -> row_id = cmn_iter_row_id( self -> cmn );

        rc_t rc1 = cmn_read_uint64_array( self -> cmn, self -> prim_alig_id, rec -> prim_alig_id, 2, &( rec -> num_alig_id ) );

        if ( rc1 == 0 && self -> opt . with_name )
            rc1 = cmn_read_String( self -> cmn, self -> name_id, &( rec -> name ) );

        if ( rc1 == 0 )
        {
            if ( self -> opt . with_cmp_read )
                rc1 = cmn_read_String( self -> cmn, self -> cmp_read_id, &( rec -> read ) );
            else
            {
                rec -> read . len = 0;
                rec -> read . size = 0;
            }
        }
        
        if ( rc1 == 0 )
            rc1 = cmn_read_String( self -> cmn, self -> quality_id, &( rec -> quality ) );
            
        if ( rc1 == 0 )
        {
            if ( self -> opt . with_read_len )
                rc1 = cmn_read_uint32_array( self -> cmn, self -> read_len_id, &rec -> read_len, &( rec -> num_read_len ) );
            else
                rec -> num_read_len = 1;
        }
        
        if ( rc1 == 0 )
        {
            if ( self -> opt . with_read_type )
                rc1 = cmn_read_uint8_array( self -> cmn, self -> read_type_id, &rec -> read_type, &( rec -> num_read_type ) );
            else
                rec -> num_read_type = 0;
        }
        
        if ( rc != NULL )
            *rc = rc1;
    }   
    return res;
}

uint64_t get_row_count_of_fastq_csra_iter( struct fastq_csra_iter * self )
{
    return cmn_iter_row_count( self -> cmn );
}

/* ------------------------------------------------------------------------------------------------------------- */

typedef struct fastq_sra_iter
{
    struct cmn_iter * cmn;
    fastq_iter_opt opt;
    uint32_t name_id, read_id, quality_id, read_len_id, read_type_id;
} fastq_sra_iter;


void destroy_fastq_sra_iter( struct fastq_sra_iter * self )
{
    if ( self != NULL )
    {
        destroy_cmn_iter( self -> cmn );
        free( ( void * ) self );
    }
}

rc_t make_fastq_sra_iter( const cmn_params * params,
                          fastq_iter_opt opt,
                          const char * tbl_name,
                          struct fastq_sra_iter ** iter )
{
    rc_t rc = 0;
    fastq_sra_iter * self = calloc( 1, sizeof * self );
    if ( self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_fastq_tbl_iter.calloc( %d ) -> %R", ( sizeof * self ), rc );
    }
    else
    {
        self -> opt = opt;
        rc = make_cmn_iter( params, tbl_name, &( self -> cmn ) );

        if ( rc == 0 && opt . with_name )
            rc = cmn_iter_add_column( self -> cmn, "NAME", &( self -> name_id ) );

        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "READ", &( self -> read_id ) );

        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "(INSDC:quality:text:phred_33)QUALITY", &( self -> quality_id ) );

        if ( rc == 0 && opt . with_read_len )
            rc = cmn_iter_add_column( self -> cmn, "READ_LEN", &( self -> read_len_id ) );
            
        if ( rc == 0 && opt . with_read_type )
            rc = cmn_iter_add_column( self -> cmn, "READ_TYPE", &( self -> read_type_id ) );

        if ( rc == 0 )
            rc = cmn_iter_range( self -> cmn, self -> read_id );
            
        if ( rc != 0 )
            destroy_fastq_sra_iter( self );
        else
            *iter = self;
    }
    return rc;
}

bool get_from_fastq_sra_iter( struct fastq_sra_iter * self, fastq_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( self -> cmn, rc );
    if ( res )
    {
        rc_t rc1 = 0;
        
        rec -> row_id = cmn_iter_row_id( self -> cmn );
        
        if ( self -> opt . with_name )
            rc1 = cmn_read_String( self -> cmn, self -> name_id, &( rec -> name ) );
            
        if ( rc1 == 0 )
            rc1 = cmn_read_String( self -> cmn, self -> read_id, &( rec -> read ) );
            
        if ( rc1 == 0 )
            rc1 = cmn_read_String( self -> cmn, self -> quality_id, &( rec -> quality ) );
        
        if ( rc1 == 0 )
        {
            if ( self -> opt . with_read_len )
                rc1 = cmn_read_uint32_array( self -> cmn, self -> read_len_id, &rec -> read_len, &( rec -> num_read_len ) );
            else
                rec -> num_read_len = 1;
        }
        
        if ( rc1 == 0 )
        {
            if ( self -> opt . with_read_type )
                rc1 = cmn_read_uint8_array( self -> cmn, self -> read_type_id, &rec -> read_type, &( rec -> num_read_type ) );
            else
                rec -> num_read_type = 0;
        }
        
        if ( rc != NULL )
            *rc = rc1;
    }
    return res;
}

uint64_t get_row_count_of_fastq_sra_iter( struct fastq_sra_iter * self )
{
    return cmn_iter_row_count( self -> cmn );
}
