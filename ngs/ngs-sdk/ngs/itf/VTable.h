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

#ifndef _h_ngs_itf_vtable_
#define _h_ngs_itf_vtable_

#ifndef _h_ngs_itf_defs_
#include <ngs/itf/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
typedef struct NGS_VTable NGS_VTable;


/*--------------------------------------------------------------------------
 * NGS_HierCache
 */
typedef struct NGS_HierCache NGS_HierCache;
struct NGS_HierCache
{
    const NGS_HierCache * volatile next;

    size_t length;
    struct
    {
        const NGS_VTable * parent;
        const void * itf_tok;
    } hier [ 1 ];
};


/*--------------------------------------------------------------------------
 * NGS_VTable
 */

/* some members are mutable */
#ifdef __cplusplus
#define NGS_MUTABLE mutable
#else
#define NGS_MUTABLE
#endif

struct NGS_VTable
{
    /* name of the implementation class */
    const char * class_name;

    /* name of the interface */
    const char * itf_name;

    /* interface minor version number */
    size_t minor_version;

    /* allow for single-inheritance
       interface hierarchy */
    const NGS_VTable * parent;

    /* cached linear inheritance array
       the implementation must set this to NULL */
    NGS_MUTABLE const NGS_HierCache *  volatile cache;
};

#undef NGS_MUTABLE

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_itf_vtable_ */
