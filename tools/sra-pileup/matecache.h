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
#ifndef _h_matecache_
#define _h_matecache_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/vector.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/log.h>

#include <insdc/sra.h>

typedef struct matecache_stat
{
    uint64_t count;
    uint64_t lookups;
    uint64_t finds;
    uint64_t inserts;
} matecache_stat;


typedef struct matecache_per_file
{
    KVector *same_ref_64;   /* ref-pos and ref-idx */
    KVector *same_ref_16;   /* flags */

    KVector *unaligned_64_a;  /* ref-pos and ref-idx */
    KVector *unaligned_64_b;  /* seq_spot_id */

    matecache_stat stat_same_ref;
    matecache_stat stat_unaligned;
    uint64_t maxcount_same_ref;
} matecache_per_file;


typedef struct matecache
{
    matecache_per_file *per_file;
    uint32_t count;
    uint32_t flashes;
} matecache;


/* general cache functions */

rc_t make_matecache( matecache **self, uint32_t count );

void release_matecache( matecache * const self );

rc_t matecache_clear_same_ref( matecache * const self );

rc_t matecache_report( const matecache * const self );


/* cache functions for aligned mates on the same reference */

rc_t matecache_insert_same_ref( matecache * const self,
        uint32_t db_idx, int64_t key, INSDC_coord_zero ref_pos, uint32_t flags, INSDC_coord_len tlen );

rc_t matecache_lookup_same_ref( const matecache * const self, uint32_t db_idx, int64_t key,
                       INSDC_coord_zero *ref_pos, uint32_t *flags, INSDC_coord_len *tlen );

rc_t matecache_remove_same_ref( matecache * const self, uint32_t db_idx, int64_t key );


/* cache functions for half aligned mates */

/*
    db_idx  ... index, what input-file did the alignment come from
    key     ... row-id of aligned half
    pos     ... position of the aligned half
    flags   ... sam-flags of the aligned half
    ref_idx ... idx of reference the aligned half aligns to
*/

rc_t matecache_insert_unaligned( matecache * const self,
        uint32_t db_idx, int64_t key, INSDC_coord_zero ref_pos, uint32_t ref_idx, int64_t seq_id );

rc_t matecache_lookup_unaligned( const matecache * const self, uint32_t db_idx, int64_t key,
                                 INSDC_coord_zero * const ref_pos, uint32_t * const ref_idx, int64_t * const seq_id );

rc_t foreach_unaligned_entry( const matecache * const self,
                              uint32_t db_idx,
                              rc_t ( CC * f ) ( int64_t seq_id, int64_t al_id, void * user_data ),
                              void * user_data );


#endif
