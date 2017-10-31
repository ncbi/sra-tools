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
    struct cmn_iter * cmn;
    bool with_read_len, with_name;
    uint32_t name_id, prim_alig_id, cmp_read_id, quality_id, read_len_id;
} fastq_csra_iter;


void destroy_fastq_csra_iter( struct fastq_csra_iter * self )
{
    if ( self != NULL )
    {
        destroy_cmn_iter( self -> cmn );
        free( ( void * ) self );
    }
}

rc_t make_fastq_csra_iter( const cmn_params * params,
                           struct fastq_csra_iter ** iter,
                           bool with_read_len,
                           bool with_name )
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
        self -> with_read_len = with_read_len;
        self -> with_name = with_name;
        rc = make_cmn_iter( params, "SEQUENCE", &( self -> cmn ) );
        if ( rc == 0 && with_name )
            rc = cmn_iter_add_column( self -> cmn, "NAME", &( self -> name_id ) );
        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "PRIMARY_ALIGNMENT_ID", &( self -> prim_alig_id ) );
        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "CMP_READ", &( self -> cmp_read_id ) );
        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "(INSDC:quality:text:phred_33)QUALITY", &( self -> quality_id ) );
        if ( rc == 0 && with_read_len )
            rc = cmn_iter_add_column( self -> cmn, "READ_LEN", &( self -> read_len_id ) );
        if ( rc == 0 )
            rc = cmn_iter_range( self -> cmn, self -> prim_alig_id );
            
        if ( rc != 0 )
            destroy_fastq_csra_iter( self );
        else
            *iter = self;
    }
    return rc;
}

bool get_from_fastq_csra_iter( struct fastq_csra_iter * self, fastq_csra_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( self -> cmn, rc );
    if ( res )
    {
        rec -> row_id = cmn_iter_row_id( self -> cmn );
        *rc = cmn_read_uint64_array( self -> cmn, self -> prim_alig_id, rec -> prim_alig_id, 2, &( rec -> num_reads ) );
        if ( *rc == 0 && self -> with_name )
            *rc = cmn_read_String( self -> cmn, self -> name_id, &( rec -> name ) );
        if ( *rc == 0 )
            *rc = cmn_read_String( self -> cmn, self -> cmp_read_id, &( rec -> cmp_read ) );
        if ( *rc == 0 )
            *rc = cmn_read_String( self -> cmn, self -> quality_id, &( rec -> quality ) );
        if ( *rc == 0 && self -> with_read_len )
            *rc = cmn_read_uint32_array( self -> cmn, self -> read_len_id, rec -> read_len, 2, NULL );
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
    bool with_read_len, with_name;
    uint32_t name_id, read_id, quality_id, read_len_id;
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
                          const char * tbl_name,
                          struct fastq_sra_iter ** iter,
                          bool with_read_len,
                          bool with_name )
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
        self -> with_read_len = with_read_len;
        self -> with_name = with_name;
        rc = make_cmn_iter( params, tbl_name, &( self -> cmn ) );
        if ( rc == 0 && with_name )
            rc = cmn_iter_add_column( self -> cmn, "NAME", &( self -> name_id ) );
        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "READ", &( self -> read_id ) );
        if ( rc == 0 )
            rc = cmn_iter_add_column( self -> cmn, "(INSDC:quality:text:phred_33)QUALITY", &( self -> quality_id ) );
        if ( rc == 0 && with_read_len )
            rc = cmn_iter_add_column( self -> cmn, "READ_LEN", &( self -> read_len_id ) );
        if ( rc == 0 )
            rc = cmn_iter_range( self -> cmn, self -> read_id );
            
        if ( rc != 0 )
            destroy_fastq_sra_iter( self );
        else
            *iter = self;
    }
    return rc;
}

bool get_from_fastq_sra_iter( struct fastq_sra_iter * self, fastq_sra_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( self -> cmn, rc );
    if ( res )
    {
        rec -> row_id = cmn_iter_row_id( self -> cmn );
        if ( self -> with_name )
            *rc = cmn_read_String( self -> cmn, self -> name_id, &( rec -> name ) );
        else
            *rc = 0;
        if ( *rc == 0 )
            *rc = cmn_read_String( self -> cmn, self -> read_id, &( rec -> read ) );
        if ( *rc == 0 )
            *rc = cmn_read_String( self -> cmn, self -> quality_id, &( rec -> quality ) );
        if ( *rc == 0 && self -> with_read_len )
            *rc = cmn_read_uint32_array( self -> cmn, self -> read_len_id, rec -> read_len, 2, &( rec -> num_reads ) );
    }
    return res;
}

uint64_t get_row_count_of_fastq_sra_iter( struct fastq_sra_iter * self )
{
    return cmn_iter_row_count( self -> cmn );
}
