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

#ifndef _h_ref_walker_
#define _h_ref_walker_

#include <klib/container.h>
#include <insdc/sra.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ref_walker;


/* create the ref-walker ( not ref-counted ) */
rc_t ref_walker_create( struct ref_walker ** self );

#define RW_INTEREST_INDEL   0x0001
#define RW_INTEREST_DEBUG   0x0002
#define RW_INTEREST_BASE    0x0004
#define RW_INTEREST_QUAL    0x0008
#define RW_INTEREST_SKIP    0x0010
#define RW_INTEREST_DUPS    0x0020
#define RW_INTEREST_TLEN    0x0040
#define RW_INTEREST_SEQNAME 0x0080
#define RW_INTEREST_PRIM    0x0100
#define RW_INTEREST_SEC     0x0200
#define RW_INTEREST_EV      0x0400

typedef struct ref_walker_data
{
    /* for the reference - level */
    const char * ref_name;
    uint64_t ref_start;
    uint64_t ref_end;

    /* for the position - level */
    INSDC_coord_zero pos;
    uint32_t depth;
    INSDC_4na_bin bin_ref_base;
    char ascii_ref_base;

    /* for the spot-group - level */
    const char * spot_group;
    size_t spot_group_len;

    /* for the alignment - level */
    int32_t state, mapq;
    INSDC_4na_bin bin_alignment_base;
    char ascii_alignment_base;
    char quality;
    INSDC_coord_zero seq_pos;
    bool ins, del, reverse, first, last, skip, match, valid;

    /* indels for alignment */
    const INSDC_4na_bin * ins_bases;
    uint32_t ins_bases_count;

    INSDC_coord_zero del_ref_pos;
    const INSDC_4na_bin * del_bases;
    uint32_t del_bases_count;

    /* for debugging purpose */
    uint64_t alignment_id;
    uint32_t alignment_start_pos;
    uint32_t alignment_len;

    void * data;
} ref_walker_data;


typedef rc_t ( CC * ref_walker_callback )( ref_walker_data * rwd );


typedef struct ref_walker_callbacks
{
    ref_walker_callback on_enter_ref;
    ref_walker_callback on_exit_ref;

    ref_walker_callback on_enter_ref_window;
    ref_walker_callback on_exit_ref_window;

    ref_walker_callback on_enter_ref_pos;
    ref_walker_callback on_exit_ref_pos;

    ref_walker_callback on_enter_spot_group;
    ref_walker_callback on_exit_spot_group;

    ref_walker_callback on_alignment;
} ref_walker_callbacks;


/* set boolean / numeric parameters */
rc_t ref_walker_set_min_mapq( struct ref_walker * self, int32_t min_mapq );
rc_t ref_walker_set_spot_group( struct ref_walker * self, const char * spot_group );
rc_t ref_walker_set_merge_diff( struct ref_walker * self, uint64_t merge_diff );
rc_t ref_walker_set_interest( struct ref_walker * self, uint32_t interest );
rc_t ref_walker_get_interest( struct ref_walker * self, uint32_t * interest );

/* set callbacks */
rc_t ref_walker_set_callbacks( struct ref_walker * self, ref_walker_callbacks * callbacks );

/* add_sources and ranges */
rc_t ref_walker_add_source( struct ref_walker * self, const char * src );

rc_t ref_walker_parse_and_add_range( struct ref_walker * self, const char * range );

rc_t ref_walker_add_range( struct ref_walker * self, const char * name, const uint64_t start, const uint64_t end );


/* walk the sources/ranges by calling the supplied call-backs, passing data to the callbacks */
rc_t ref_walker_walk( struct ref_walker * self, void * data );


/* destroy the ref-walker */
rc_t ref_walker_destroy( struct ref_walker * self );
    
#ifdef __cplusplus
}
#endif

#endif /*  _h_ref_walker_ */
