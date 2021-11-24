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

#ifndef _h_ngs_itf_referenceitf_
#define _h_ngs_itf_referenceitf_

#ifndef _h_ngs_itf_vtable_
#include "VTable.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_Pileup_v1;
struct NGS_Alignment_v1;

enum NGS_ReferenceAlignFlags
{
    NGS_ReferenceAlignFlags_wants_primary       = 0x01,
    NGS_ReferenceAlignFlags_wants_secondary     = 0x02,
    NGS_ReferenceAlignFlags_pass_bad            = 0x04,
    NGS_ReferenceAlignFlags_pass_dups           = 0x08,
    NGS_ReferenceAlignFlags_min_map_qual        = 0x10,
    NGS_ReferenceAlignFlags_max_map_qual        = 0x20,
    NGS_ReferenceAlignFlags_no_wraparound       = 0x40,
    NGS_ReferenceAlignFlags_start_within_window = 0x80
};


/*--------------------------------------------------------------------------
 * NGS_Reference_v1
 */
typedef struct NGS_Reference_v1 NGS_Reference_v1;
struct NGS_Reference_v1
{
    const NGS_VTable * vt;
};

typedef struct NGS_Reference_v1_vt NGS_Reference_v1_vt;
struct NGS_Reference_v1_vt
{
    NGS_VTable dad;

    /* v1.0 interface */
    NGS_String_v1 * ( CC * get_cmn_name ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_canon_name ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
    bool ( CC * is_circular ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
    uint64_t ( CC * get_length ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_ref_bases ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length );
    NGS_String_v1 * ( CC * get_ref_chunk ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length );
    struct NGS_Alignment_v1 * ( CC * get_alignment ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, const char * alignmentId );
    struct NGS_Alignment_v1 * ( CC * get_alignments ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary );
    struct NGS_Alignment_v1 * ( CC * get_align_slice ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
        int64_t start, uint64_t length, bool wants_primary, bool wants_secondary );
    struct NGS_Pileup_v1 * ( CC * get_pileups ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary );
    struct NGS_Pileup_v1 * ( CC * get_pileup_slice ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
        int64_t start, uint64_t length, bool wants_primary, bool wants_secondary );
    bool ( CC * next ) ( NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );

    /* v1.1 interface */
    struct NGS_Pileup_v1 * ( CC * get_filtered_pileups ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, uint32_t flags, int32_t map_qual );
    struct NGS_Pileup_v1 * ( CC * get_filtered_pileup_slice ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, int64_t start, uint64_t length, uint32_t flags, int32_t map_qual );

    /* 1.2 interface */
    uint64_t ( CC * get_align_count ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary );

    /* 1.3 interface */
    struct NGS_Alignment_v1 * ( CC * get_filtered_alignments ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, uint32_t flags, int32_t map_qual );
    struct NGS_Alignment_v1 * ( CC * get_filtered_align_slice ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err, int64_t start, uint64_t length, uint32_t flags, int32_t map_qual );

    /* 1.4 interface */
    bool ( CC * is_local ) ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
};


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_itf_referenceitf_ */
