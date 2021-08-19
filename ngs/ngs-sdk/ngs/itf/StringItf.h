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

#ifndef _h_ngs_itf_stringitf_
#define _h_ngs_itf_stringitf_

#ifndef _h_ngs_itf_vtable_
#include <ngs/itf/VTable.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * NGS_String_v1
 */
struct NGS_String_v1
{
    const NGS_VTable * vt;
};

typedef struct NGS_String_v1_vt NGS_String_v1_vt;
struct NGS_String_v1_vt
{
    NGS_VTable dad;

    const char* ( CC * data ) ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err );
    size_t ( CC * size ) ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err );
    NGS_String_v1* ( CC * substr ) ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err,
        size_t offset, size_t size );
};


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_itf_stringitf_ */
