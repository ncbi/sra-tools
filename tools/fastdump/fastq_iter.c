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

#include <os-native.h>
#include <sysalloc.h>

typedef struct fastq_iter
{
    struct cmn_iter * cmn;
    uint32_t prim_alig_id, cmp_read_id, quality_id;
} fastq_iter;


void destroy_fastq_iter( struct fastq_iter * iter )
{
    if ( iter != NULL )
    {
        destroy_cmn_iter( iter->cmn );
        free( ( void * ) iter );
    }
}

rc_t make_fastq_iter( cmn_params * params, struct fastq_iter ** iter )
{
    rc_t rc = 0;
    fastq_iter * i = calloc( 1, sizeof * i );
    if ( i == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_fastq_iter.calloc( %d ) -> %R", ( sizeof * i ), rc );
    }
    else
    {
        rc = make_cmn_iter( params, "SEQUENCE", &i->cmn );    
        if ( rc == 0 )
            rc = cmn_iter_add_column( i->cmn, "PRIMARY_ALIGNMENT_ID", &i->prim_alig_id );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i->cmn, "CMP_READ", &i->cmp_read_id );
        if ( rc == 0 )
            rc = cmn_iter_add_column( i->cmn, "QUALITY", &i->quality_id );
        if ( rc == 0 )
            rc = cmn_iter_range( i->cmn, i->prim_alig_id );
            
        if ( rc != 0 )
            destroy_fastq_iter( i );
        else
            *iter = i;
    }
    return rc;
}

bool get_from_fastq_iter( struct fastq_iter * iter, fastq_rec * rec, rc_t * rc )
{
    bool res = cmn_iter_next( iter->cmn, rc );
    if ( res )
    {
        rec->row_id = cmn_iter_row_id( iter->cmn );
        *rc = cmn_read_uint64_array( iter->cmn, iter->prim_alig_id, rec->prim_alig_id, 2 );
        if ( *rc == 0 )
            *rc = cmn_read_String( iter->cmn, iter->cmp_read_id, &rec->cmp_read );
        if ( *rc == 0 )
            *rc = cmn_read_String( iter->cmn, iter->quality_id, &rec->quality );
    }
    return res;

}

uint64_t get_row_count_of_fastq_iter( struct fastq_iter * iter )
{
    return cmn_iter_row_count( iter->cmn );
}
