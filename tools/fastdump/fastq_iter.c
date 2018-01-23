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

#include <klib/data-buffer.h>

static void init_qual_to_ascii( char * q2a, size_t size )
{
    uint32_t idx;
    memset( q2a, '~', size );
    for ( idx = 0; idx < 256; idx++ )
    {
        q2a[ idx ] = idx + 33;
        if ( q2a[ idx ] == '~' )
        {
            break;
        }
    }
}

static rc_t read_bounded_quality( struct cmn_iter * cmn,
                                  uint32_t col_id,
                                  KDataBuffer * qual_buffer,
                                  char * q2a,
                                  String * quality )
{
    uint8_t * qual_values = NULL;
    uint32_t num_qual = 0;
    rc_t rc = cmn_read_uint8_array( cmn, col_id, &qual_values, &num_qual );
    if ( rc == 0 && num_qual > 0 && qual_values != NULL )
    { 
        if ( num_qual > qual_buffer -> elem_count )
            rc = KDataBufferResize ( qual_buffer, num_qual );
        if ( rc == 0 )
        {
            uint32_t idx;
            uint8_t * b = qual_buffer -> base;
            for ( idx = 0; idx < num_qual; idx++ )
                b[ idx ] = q2a[ qual_values[ idx ] ];
            quality -> addr = qual_buffer -> base;
            quality -> len  = num_qual;
            quality -> size = num_qual;
        }
    }
    if ( rc != 0 )
    {
        quality -> len = 0;
        quality -> size = 0;
        rc = 0;
    }
    return rc;
}

typedef struct fastq_csra_iter
{
    struct cmn_iter * cmn; /* cmn_iter.h */
    KDataBuffer qual_buffer;  /* klib/databuffer.h */
    fastq_iter_opt opt; /* fastq_iter.h */
    uint32_t name_id, prim_alig_id, read_id, quality_id, read_len_id, read_type_id;
    char qual_2_ascii[ 256 ];
} fastq_csra_iter;


void destroy_fastq_csra_iter( struct fastq_csra_iter * self )
{
    if ( self != NULL )
    {
        destroy_cmn_iter( self -> cmn ); /* cmn_iter.h */
        if ( self -> qual_buffer . base != NULL )
            KDataBufferWhack( &self -> qual_buffer );
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
        rc = KDataBufferMakeBytes( &self -> qual_buffer, 4096 );
        if ( rc != 0 )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_fastq_csra_iter.KDataBufferMakeBytes() -> %R", rc );
        }
        else
        {
            self -> opt = opt;
            rc = make_cmn_iter( params, "SEQUENCE", &( self -> cmn ) ); /* cmn_iter.h */
            
            if ( rc == 0 && opt . with_name )
                rc = cmn_iter_add_column( self -> cmn, "NAME", &( self -> name_id ) ); /* cmn_iter.h */

            if ( rc == 0 )
                rc = cmn_iter_add_column( self -> cmn, "PRIMARY_ALIGNMENT_ID", &( self -> prim_alig_id ) ); /* cmn_iter.h */

            if ( rc == 0 )
            {
                if ( opt . with_cmp_read )
                    rc = cmn_iter_add_column( self -> cmn, "CMP_READ", &( self -> read_id ) ); /* cmn_iter.h */
                else
                    rc = cmn_iter_add_column( self -> cmn, "READ", &( self -> read_id ) ); /* cmn_iter.h */
            }

            if ( rc == 0 )
                rc = cmn_iter_add_column( self -> cmn, "QUALITY", &( self -> quality_id ) ); /* cmn_iter.h */
                
            if ( rc == 0 && opt . with_read_len )
                rc = cmn_iter_add_column( self -> cmn, "READ_LEN", &( self -> read_len_id ) ); /* cmn_iter.h */

            if ( rc == 0 && opt . with_read_type )
                rc = cmn_iter_add_column( self -> cmn, "READ_TYPE", &( self -> read_type_id ) ); /* cmn_iter.h */

            if ( rc == 0 )
                rc = cmn_iter_range( self -> cmn, self -> prim_alig_id ); /* cmn_iter.h */
                
            if ( rc == 0 )
                init_qual_to_ascii( &( self -> qual_2_ascii[ 0 ] ), sizeof( self -> qual_2_ascii ) );

        }
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
            rc1 = cmn_read_String( self -> cmn, self -> read_id, &( rec -> read ) );
        
        if ( rc1 == 0 )
            rc1 = read_bounded_quality( self -> cmn, self -> quality_id,
                                        &( self -> qual_buffer ),
                                        &( self -> qual_2_ascii[ 0 ] ),
                                        &( rec -> quality ) );
        
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
    KDataBuffer qual_buffer;  /* klib/databuffer.h */
    uint32_t name_id, read_id, quality_id, read_len_id, read_type_id;
    char qual_2_ascii[ 256 ];
} fastq_sra_iter;


void destroy_fastq_sra_iter( struct fastq_sra_iter * self )
{
    if ( self != NULL )
    {
        destroy_cmn_iter( self -> cmn );
        if ( self -> qual_buffer . base != NULL )
            KDataBufferWhack( &self -> qual_buffer );
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
        rc = KDataBufferMakeBytes( &self -> qual_buffer, 4096 );
        if ( rc != 0 )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_fastq_csra_iter.KDataBufferMakeBytes() -> %R", rc );
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
                rc = cmn_iter_add_column( self -> cmn, "QUALITY", &( self -> quality_id ) );

            if ( rc == 0 && opt . with_read_len )
                rc = cmn_iter_add_column( self -> cmn, "READ_LEN", &( self -> read_len_id ) );
                
            if ( rc == 0 && opt . with_read_type )
                rc = cmn_iter_add_column( self -> cmn, "READ_TYPE", &( self -> read_type_id ) );

            if ( rc == 0 )
                rc = cmn_iter_range( self -> cmn, self -> read_id );

            if ( rc == 0 )
                init_qual_to_ascii( &( self -> qual_2_ascii[ 0 ] ), sizeof( self -> qual_2_ascii ) );

            if ( rc != 0 )
                destroy_fastq_sra_iter( self );
            else
                *iter = self;
        }
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
            rc1 = read_bounded_quality( self -> cmn, self -> quality_id,
                                        &( self -> qual_buffer ),
                                        &( self -> qual_2_ascii[ 0 ] ),
                                        &( rec ->quality ) );
        
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
