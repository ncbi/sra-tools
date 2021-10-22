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

#ifndef _h_ngs_itf_alignmentitf_
#define _h_ngs_itf_alignmentitf_

#ifndef _h_ngs_itf_vtable_
#include <ngs/itf/VTable.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * NGS_Alignment_v1
 */
typedef struct NGS_Alignment_v1 NGS_Alignment_v1;
struct NGS_Alignment_v1
{
    const NGS_VTable * vt;
};

typedef struct NGS_Alignment_v1_vt NGS_Alignment_v1_vt;
struct NGS_Alignment_v1_vt
{
    NGS_VTable dad;

    /* v1.0 interface */
    NGS_String_v1 * ( CC * get_id ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_ref_spec ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    int32_t ( CC * get_map_qual ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_ref_bases ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_read_group ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_read_id ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_clipped_frag_bases ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_clipped_frag_quals ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_aligned_frag_bases ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    bool ( CC * is_primary ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    int64_t ( CC * get_align_pos ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    uint64_t ( CC * get_align_length ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    bool ( CC * get_is_reversed ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    int32_t ( CC * get_soft_clip ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, uint32_t edge );
    uint64_t ( CC * get_template_len ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_short_cigar ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, bool clipped );
    NGS_String_v1 * ( CC * get_long_cigar ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, bool clipped );
    bool ( CC * has_mate ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_mate_id ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_Alignment_v1 * ( CC * get_mate_alignment ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_mate_ref_spec ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    bool ( CC * get_mate_is_reversed ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
    bool ( CC * next ) ( NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );

    /* v1.1 */
    char ( CC * get_rna_orientation ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );

    /* v1.2 */
    uint64_t ( CC * get_ref_pos_projection_range ) ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, int64_t ref_pos );
};


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_itf_alignmentitf_ */
