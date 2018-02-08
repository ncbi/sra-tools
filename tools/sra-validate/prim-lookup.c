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
#include "prim-lookup.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_kproc_lock_
#include <kproc/lock.h>
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

typedef struct prim_lookup
{
    KLock * lock;
    
    KVector * vec;
    /*
        key    ( 64-bit ) : row-id in PRIM-table
        value  ( 64-bit ) : read-len in upper 32 bit, ref-orientation in bit 0
    */
    
    uint64_t in_vector;
    uint64_t max_in_vector;
} prim_lookup;

void destroy_prim_lookup( prim_lookup * self )
{
    if ( self != NULL )
    {
        if ( self -> lock != NULL )
            KLockRelease ( self -> lock );
        if ( self -> vec != NULL )
            KVectorRelease ( self -> vec );

        free( ( void * ) self );
    }
}

rc_t make_prim_lookup( prim_lookup ** lookup )
{
    rc_t rc = 0;
    prim_lookup * obj = calloc( 1, sizeof * obj );
    if ( obj == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_prim_lookup().calloc( %d ) -> %R", ( sizeof * obj ), rc );
    }
    else
    {
        rc = KLockMake ( &obj -> lock );
        if ( rc != 0 )
            ErrMsg( "make_prim_lookup().KLockMake() -> %R", rc );    
        else
        {
            rc = KVectorMake ( &obj -> vec );
            if ( rc != 0 )
                ErrMsg( "make_prim_lookup().KVectorMake() -> %R", rc );    
        }
        
        if ( rc != 0 )
            destroy_prim_lookup( obj );
        else
            *lookup = obj;
    }
    return rc;

}

rc_t prim_lookup_enter( prim_lookup * self, const prim_rec * rec )
{
    rc_t rc = 0;
    if ( self == NULL || rec == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "prim_lookup_enter() -> %R", rc );
    }
    else
    {
        /* the lookup will be made based on the primary-row-id as a key */
        uint64_t key = rec -> align_row_id;
        /* the value is a combination of READ_LEN and REF_ORIENTATION */
        uint64_t value = rec -> read_len;
        value <<= 32;
        if ( rec -> ref_orient )
            value |= 1;
            
        rc = KLockAcquire ( self -> lock );
        if ( rc != 0 )
            ErrMsg( "prim_lookup_enter().KLockAcquire() -> %R", rc );
        else
        {
            rc = KVectorSetU64 ( self -> vec, key, value );
            if ( rc != 0 )
                ErrMsg( "prim_lookup_enter().KVectorSetU64( %lu => %lx ) -> %R", key, value, rc );
            else
            {
                self -> in_vector ++;
                if ( self -> in_vector > self -> max_in_vector )
                    self -> max_in_vector = self -> in_vector;
            }
            
            rc = KLockUnlock ( self -> lock );
            if ( rc != 0 )
                ErrMsg( "prim_lookup_enter().KLockUnlock() -> %R", rc );
        }
    }
    return rc;
}

rc_t prim_lookup_get( prim_lookup * self, uint64_t align_id, uint32_t * read_len, bool * ref_orient, bool * found )
{
    rc_t rc = 0;
    if ( self == NULL || read_len == NULL || ref_orient == NULL || found == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "prim_lookup_enter() -> %R", rc );
    }
    else
    {
        uint64_t key = align_id;
        uint64_t value;
        
        rc = KLockAcquire ( self -> lock );
        if ( rc != 0 )
            ErrMsg( "prim_lookup_enter().KLockAcquire() -> %R", rc );
        else
        {
            rc_t rc_v = KVectorGetU64 ( self -> vec, key, &value );
            if ( rc_v == 0 )
            {
                *ref_orient = ( ( value & 1 ) == 1 );
                value >>= 32;
                *read_len = ( value & 0xFFFFFFFF );
                *found = true;
                
                rc = KVectorUnset ( self -> vec, key );
                if ( rc != 0 )
                    ErrMsg( "prim_lookup_enter().KVectorUnset( %lu ) -> %R", key, rc );
                else
                    self -> in_vector--;
            }
            else
                *found = false;
                
            rc = KLockUnlock ( self -> lock );
            if ( rc != 0 )
                ErrMsg( "prim_lookup_enter().KLockUnlock() -> %R", rc );
        }
    }
    return rc;
}

static rc_t on_visit_vector( uint64_t key, uint64_t value, void *user_data )
{
    return KOutMsg( "prim-align-id: %,lu\n", key );
}

rc_t prim_lookup_report( const prim_lookup * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        KOutMsg( "\nlookup.in_vector     = %,lu", self -> in_vector );
        KOutMsg( "\nlookup.max_in_vector = %,lu\n", self -> max_in_vector );
        if ( self -> in_vector > 0 )
            rc = KVectorVisitU64 ( self -> vec, false, on_visit_vector, NULL );
    }
    return rc;
}