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

#include "matecache.h"
#include <sysalloc.h>
#include <stdlib.h>

void release_matecache( matecache * const self )
{
    if ( self != NULL )
    {
        if ( self->per_file != NULL )
        {
            uint32_t idx;
            for ( idx = 0; idx < self->count; ++idx )
            {
                if ( self->per_file[ idx ].same_ref_64 != NULL )
                    KVectorRelease( self->per_file[ idx ].same_ref_64 );
                if ( self->per_file[ idx ].same_ref_16 != NULL )
                    KVectorRelease( self->per_file[ idx ].same_ref_16 );

                if ( self->per_file[ idx ].unaligned_64_a != NULL )
                    KVectorRelease( self->per_file[ idx ].unaligned_64_a );
                if ( self->per_file[ idx ].unaligned_64_b != NULL )
                    KVectorRelease( self->per_file[ idx ].unaligned_64_b );
            }
            free( self->per_file );
        }
        free( self );
    }
}


rc_t make_matecache( matecache **self, uint32_t count )
{
    rc_t rc = 0;

    matecache * mc = calloc( sizeof * mc, 1 );
    *self = NULL;
    if ( mc == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        (void)LOGERR( klogErr, rc, "cannot create matecache structure" );
    }
    else
    {
        mc->count = count;
        mc->per_file = calloc( sizeof *(mc->per_file), count );
        if ( mc->per_file == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            (void)LOGERR( klogErr, rc, "cannot create matecache internal structure" );
        }
        else
        {
            uint32_t idx;
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                rc = KVectorMake( &( mc->per_file[ idx ].same_ref_64 ) );
                if ( rc != 0 )
                    (void)LOGERR( klogErr, rc, "cannot create KVector (same-ref) U64" );
                else
                {
                    rc = KVectorMake( &( mc->per_file[ idx ].same_ref_16 ) );
                    if ( rc != 0 )
                        (void)LOGERR( klogErr, rc, "cannot create KVector (same-ref) U16" );
                    else
                    {
                        rc = KVectorMake( &( mc->per_file[ idx ].unaligned_64_a ) );
                        if ( rc != 0 )
                            (void)LOGERR( klogErr, rc, "cannot create KVector (unaligned a) U64" );
                        else
                        {
                            rc = KVectorMake( &( mc->per_file[ idx ].unaligned_64_b ) );
                            if ( rc != 0 )
                                (void)LOGERR( klogErr, rc, "cannot create KVector (unaligned b) U64" );
                        }
                    }
                }
            }
            if ( rc == 0 )
                *self = mc;
        }
        if ( rc != 0 )
            release_matecache( mc );
    }
    return rc;
}

#if 0
static int32_t calc_tlen( uint32_t self_pos, uint32_t mate_pos,
                   uint32_t self_len, uint32_t mate_len, uint32_t read_num )
{
    int32_t res = 0;
    unsigned const self_left  = self_pos;
    unsigned const mate_left  = mate_pos;
    unsigned const self_right = self_left + self_len;
    unsigned const mate_right = mate_left + mate_len;
    unsigned const  leftmost  = (self_left  < mate_left ) ? self_left  : mate_left;
    unsigned const rightmost  = (self_right > mate_right) ? self_right : mate_right;
    unsigned const tlen = rightmost - leftmost;
            
    /* The standard says, "The leftmost segment has a plus sign and the rightmost has a minus sign." */
    if (   ( self_left <= mate_left && self_right >= mate_right )      /* mate fully contained within self or */
        || ( mate_left <= self_left && mate_right >= self_right ) )    /* self fully contained within mate; */
    {
        if ( self_left < mate_left || ( read_num == 1 && self_left == mate_left ) )
            res = tlen;
        else
            res = -tlen;
    }
    else if ( ( self_right == mate_right && mate_left == leftmost ) /* both are rightmost, but mate is leftmost */
             ||  self_right == rightmost )
    {
        res = -tlen;
    }
    else
        res = tlen;

    return res;
}
#endif

static rc_t matecache_check( const matecache * const self, uint32_t db_idx, matecache_per_file ** mcpf )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
        (void)LOGERR( klogErr, rc, "cannot insert into same-ref-cache" );
    }
    else if ( db_idx < self->count )
    {
        *mcpf = &self->per_file[ db_idx ];
        if ( mcpf == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcInvalid );
            (void)LOGERR( klogErr, rc, "cannot insert into same-ref-cache" );
        }
    }
    return rc;
}


rc_t matecache_insert_same_ref( matecache * const self,
        uint32_t db_idx, int64_t key, INSDC_coord_zero ref_pos, uint32_t flags, INSDC_coord_len tlen )
{
    matecache_per_file * mcpf = NULL;
    rc_t rc = matecache_check( self, db_idx, &mcpf );
    if ( rc == 0 )
    {
        uint64_t ref_pos_and_tlen = ref_pos;
        ref_pos_and_tlen <<= 32;
        ref_pos_and_tlen |= tlen;
        rc = KVectorSetU64( mcpf->same_ref_64, key, ref_pos_and_tlen );
        if ( rc != 0 )
            (void)LOGERR( klogErr, rc, "cannot insert into KVector (same-ref) U64" );
        else
        {
            rc = KVectorSetU16( mcpf->same_ref_16, key, flags );
            if ( rc != 0 )
                (void)LOGERR( klogErr, rc, "cannot insert into KVector (same-ref) U16" );
        }
        if ( rc == 0 )
        {
            mcpf->stat_same_ref.count++;
            if ( mcpf->stat_same_ref.count > mcpf->maxcount_same_ref )
                mcpf->maxcount_same_ref = mcpf->stat_same_ref.count;
            mcpf->stat_same_ref.inserts++;
        }
    }
    return rc;
}


rc_t matecache_lookup_same_ref( const matecache * const self, uint32_t db_idx, int64_t key,
                       INSDC_coord_zero *ref_pos, uint32_t *flags, INSDC_coord_len *tlen )
{
    matecache_per_file * mcpf = NULL;
    rc_t rc = matecache_check( self, db_idx, &mcpf );
    if ( rc == 0 )
    {
        uint64_t value64;
        mcpf->stat_same_ref.lookups++;
        rc = KVectorGetU64( mcpf->same_ref_64, key, &value64 );
        if ( rc != 0 )
        {
            if ( GetRCState( rc ) != rcNotFound )
                (void)LOGERR( klogErr, rc, "cannot retrieve value (same-ref) U64" );
        }
        else
        {
            uint16_t value16;
            rc = KVectorGetU16( mcpf->same_ref_16, key, &value16 );
            if ( rc != 0 )
            {
                if ( GetRCState( rc ) != rcNotFound )
                    (void)LOGERR( klogErr, rc, "cannot retrieve value (same-ref) U16" );
            }
            else
            {
                *ref_pos = ( value64 >> 32 );
                *tlen = ( value64 & 0xFFFFFFFF );
                *flags = value16;
                mcpf->stat_same_ref.finds++;
            }
        }
    }
    return rc;
}


rc_t matecache_remove_same_ref( matecache * const self, uint32_t db_idx, int64_t key )
{
    matecache_per_file * mcpf = NULL;
    rc_t rc = matecache_check( self, db_idx, &mcpf );
    if ( rc == 0 )
    {
        rc = KVectorUnset( mcpf->same_ref_64, key );
        if ( rc != 0 )
            (void)LOGERR( klogErr, rc, "cannot remove from same-ref-cache (64bit)" );
        else
        {
            rc = KVectorUnset( mcpf->same_ref_16, key );
            if ( rc != 0 )
                (void)LOGERR( klogErr, rc, "cannot remove from same-ref-cache (16bit)" );
        }
        if ( rc == 0 && mcpf->stat_same_ref.count > 0 )
            mcpf->stat_same_ref.count--;
    }
    return rc;
}


static rc_t matecache_clear_same_ref_per_file( matecache_per_file * const mcpf )
{
    rc_t rc = KVectorRelease( mcpf->same_ref_64 );
    if ( rc != 0 )
        (void)LOGERR( klogErr, rc, "cannot release same-ref-cache (64bit)" );
    else
    {
        rc = KVectorRelease( mcpf->same_ref_16 );
        if ( rc != 0 )
            (void)LOGERR( klogErr, rc, "cannot release same-ref-cache (16bit)" );
        else
        {
            rc = KVectorMake( &mcpf->same_ref_64 );
            if ( rc != 0 )
                (void)LOGERR( klogErr, rc, "cannot make same-ref-cache (64bit)" );
            else
            {
                rc = KVectorMake( &mcpf->same_ref_16 );
                if ( rc != 0 )
                    (void)LOGERR( klogErr, rc, "cannot make same-ref-cache (16bit)" );
            }
        }
    }
    return rc;
}


rc_t matecache_clear_same_ref( matecache * const self )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
        (void)LOGERR( klogErr, rc, "cannot clear same-ref-cache" );
    }
    else
    {
        uint32_t idx;
        for ( idx = 0; idx < self->count && rc == 0; ++idx )
        {
            rc = matecache_clear_same_ref_per_file( &self->per_file[ idx ] );
        }
        self->flashes++;
   }
    return rc;
}


rc_t matecache_report( const matecache * const self )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
        (void)LOGERR( klogErr, rc, "cannot report same-ref-cache" );
    }
    else
    {
        uint32_t idx;
        for ( idx = 0; idx < self->count && rc == 0; ++idx )
        {
            rc = KOutMsg( "on same reference:\n" );
            if ( rc == 0 )
                rc = KOutMsg( "matecache[ %u ].maxcount = %,lu\n", idx, self->per_file[ idx ].maxcount_same_ref );
            if ( rc == 0 )
                rc = KOutMsg( "matecache[ %u ].inserts = %,lu\n", idx, self->per_file[ idx ].stat_same_ref.inserts );
            if ( rc == 0 )
                rc = KOutMsg( "matecache[ %u ].lookups = %,lu\n", idx, self->per_file[ idx ].stat_same_ref.lookups );
            if ( rc == 0 )
                rc = KOutMsg( "matecache[ %u ].finds = %,lu\n", idx, self->per_file[ idx ].stat_same_ref.finds );
            if ( rc == 0 )
                rc = KOutMsg( "unaligned:\n" );
            if ( rc == 0 )
                rc = KOutMsg( "matecache[ %u ].count = %,lu\n", idx, self->per_file[ idx ].stat_unaligned.count );
            if ( rc == 0 )
                rc = KOutMsg( "matecache[ %u ].lookups = %,lu\n", idx, self->per_file[ idx ].stat_unaligned.lookups );
            if ( rc == 0 )
                rc = KOutMsg( "matecache[ %u ].finds = %,lu\n", idx, self->per_file[ idx ].stat_unaligned.finds );
        }
        if ( rc == 0 )
            rc = KOutMsg( "matecache.flashes = %,lu\n", idx, self->flashes );
    }
    return rc;
}


rc_t matecache_insert_unaligned( matecache * const self,
        uint32_t db_idx, int64_t key, INSDC_coord_zero ref_pos, uint32_t ref_idx, int64_t seq_id )
{
    matecache_per_file * mcpf = NULL;
    rc_t rc = matecache_check( self, db_idx, &mcpf );
    if ( rc == 0 )
    {
        uint64_t ref_pos_and_ref_idx = ref_pos;
        ref_pos_and_ref_idx <<= 32;
        ref_pos_and_ref_idx |= ref_idx;
        rc = KVectorSetU64( mcpf->unaligned_64_a, key, ref_pos_and_ref_idx );
        if ( rc != 0 )
            (void)LOGERR( klogErr, rc, "cannot insert into KVector (unaligned a) U64" );
        else
        {
            rc = KVectorSetU64( mcpf->unaligned_64_b, key, seq_id );
            if ( rc != 0 )
                (void)LOGERR( klogErr, rc, "cannot insert into KVector (unaligned b) U64" );
        }
        if ( rc == 0 )
        {
            mcpf->stat_unaligned.count++;
            mcpf->stat_unaligned.inserts++;
        }
    }
    return rc;
}


rc_t matecache_lookup_unaligned( const matecache * const self, uint32_t db_idx, int64_t key,
                                 INSDC_coord_zero * const ref_pos, uint32_t * const ref_idx, int64_t * const seq_id )
{
    matecache_per_file * mcpf = NULL;
    rc_t rc = matecache_check( self, db_idx, &mcpf );
    if ( rc == 0 )
    {
        uint64_t value64_a;
        mcpf->stat_unaligned.lookups++;
        rc = KVectorGetU64( mcpf->unaligned_64_a, key, &value64_a );
        if ( rc != 0 )
        {
            if ( GetRCState( rc ) != rcNotFound )
                (void)LOGERR( klogErr, rc, "cannot retrieve value (unaligned a) U64" );
        }
        else
        {
            uint64_t value64_b;
            rc = KVectorGetU64( mcpf->unaligned_64_b, key, &value64_b );
            if ( rc != 0 )
            {
                if ( GetRCState( rc ) != rcNotFound )
                    (void)LOGERR( klogErr, rc, "cannot retrieve value (unaligned b) U64" );
            }
            else
            {
                *seq_id = ( int64_t )value64_b;
                *ref_pos = ( value64_a >> 32 );
                *ref_idx = ( value64_a & 0xFFFFFFFF );
                mcpf->stat_unaligned.finds++;
            }
        }
    }
    return rc;
}


typedef struct visit_ctx
{
    rc_t ( CC * f ) ( int64_t seq_id, int64_t al_id, void * user_data );
    void * user_data;
} visit_ctx;


static rc_t CC on_seq_id( uint64_t key, int64_t value, void *user_data )
{
    visit_ctx * vctx = user_data;
    return vctx->f( value, ( int64_t )key, vctx->user_data );
}


rc_t foreach_unaligned_entry( const matecache * const self,
                              uint32_t db_idx,
                              rc_t ( CC * f ) ( int64_t seq_id, int64_t al_id, void * user_data ),
                              void * user_data )
{
    matecache_per_file * mcpf = NULL;
    rc_t rc = matecache_check( self, db_idx, &mcpf );
    if ( rc == 0 )
    {
        visit_ctx vctx;
        vctx.f = f;
        vctx.user_data = user_data;
        rc = KVectorVisitI64 ( mcpf->unaligned_64_b, false, on_seq_id, &vctx );
    }
    return rc;
}
