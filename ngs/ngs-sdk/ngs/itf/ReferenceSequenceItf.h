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

#ifndef _h_ngs_itf_reference_sequence_itf_
#define _h_ngs_itf_reference_sequence_itf_

#ifndef _h_ngs_itf_vtable_
#include "VTable.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */


/*--------------------------------------------------------------------------
 * NGS_ReferenceSequence_v1
 */
typedef struct NGS_ReferenceSequence_v1 NGS_ReferenceSequence_v1;
struct NGS_ReferenceSequence_v1
{
    const NGS_VTable * vt;
};

typedef struct NGS_ReferenceSequence_v1_vt NGS_ReferenceSequence_v1_vt;
struct NGS_ReferenceSequence_v1_vt
{
    NGS_VTable dad;

    /* v1.0 interface */
    NGS_String_v1 * ( CC * get_canon_name ) ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err );
    bool ( CC * is_circular ) ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err );
    uint64_t ( CC * get_length ) ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1 * ( CC * get_ref_bases ) ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length );
    NGS_String_v1 * ( CC * get_ref_chunk ) ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length );
};


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_itf_reference_sequence_itf_ */
