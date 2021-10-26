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

#ifndef _h_ngs_itf_read_collectionitf_
#define _h_ngs_itf_read_collectionitf_

#ifndef _h_ngs_itf_vtable_
#include "VTable.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_Read_v1;
struct NGS_ReadGroup_v1;
struct NGS_Reference_v1;
struct NGS_Alignment_v1;


/*--------------------------------------------------------------------------
 * NGS_ReadCollection_v1
 */
typedef struct NGS_ReadCollection_v1 NGS_ReadCollection_v1;
struct NGS_ReadCollection_v1
{
    const NGS_VTable * vt;
};

typedef struct NGS_ReadCollection_v1_vt NGS_ReadCollection_v1_vt;
struct NGS_ReadCollection_v1_vt
{
    NGS_VTable dad;

    // 1.0
    NGS_String_v1 * ( CC * get_name ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err );
    struct NGS_ReadGroup_v1 * ( CC * get_read_groups ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err );
    struct NGS_ReadGroup_v1 * ( CC * get_read_group ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, const char * spec );
    struct NGS_Reference_v1 * ( CC * get_references ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err );
    struct NGS_Reference_v1 * ( CC * get_reference ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, const char * spec );
    struct NGS_Alignment_v1 * ( CC * get_alignment ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
        const char * alignmentId );
    struct NGS_Alignment_v1 * ( CC * get_alignments ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary );
    uint64_t ( CC * get_align_count ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary );
    struct NGS_Alignment_v1 * ( CC * get_align_range ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
        uint64_t first, uint64_t count, bool wants_primary, bool wants_secondary );
    struct NGS_Read_v1 * ( CC * get_read ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, const char * readId );
    struct NGS_Read_v1 * ( CC * get_reads ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_full, bool wants_partial, bool wants_unaligned );
    uint64_t ( CC * get_read_count ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_full, bool wants_partial, bool wants_unaligned );
    struct NGS_Read_v1 * ( CC * get_read_range ) ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, 
        uint64_t first, uint64_t count, bool wants_full, bool wants_partial, bool wants_unaligned );

    // 1.1
    bool ( CC * has_read_group ) ( const NGS_ReadCollection_v1 * self, const char * spec );
    bool ( CC * has_reference ) ( const NGS_ReadCollection_v1 * self, const char * spec );
};


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_itf_read_collectionitf_ */
