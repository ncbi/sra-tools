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

#ifndef _h_ngs_refcount_
#define _h_ngs_refcount_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards & externs
 */
struct NGS_VTable;
struct NGS_Refcount_v1_vt;
extern struct NGS_Refcount_v1_vt ITF_Refcount_vt;


/*--------------------------------------------------------------------------
 * NGS_Refcount
 */
typedef struct NGS_Refcount NGS_Refcount;

/* Release
 *  resilient to NULL self
 */
void NGS_RefcountRelease ( const NGS_Refcount * self, ctx_t ctx );

/* Duplicate
 *  resilient to NULL self
 */
void * NGS_RefcountDuplicate ( const NGS_Refcount * self, ctx_t ctx );


/*--------------------------------------------------------------------------
 * implementation details
 */
typedef struct NGS_Refcount_vt NGS_Refcount_vt;
struct NGS_Refcount
{
    /* interface vtable from NGS SDK */
    struct NGS_VTable const * ivt;

    /* internal vtable for NGS polymorphism */
    const NGS_Refcount_vt * vt;

    /* the counter */
    KRefcount refcount;
    uint32_t filler;
};

#ifndef NGS_REFCOUNT
#define NGS_REFCOUNT NGS_Refcount
#endif

struct NGS_Refcount_vt
{
    void ( * whack ) ( NGS_REFCOUNT * self, ctx_t ctx );
};

/* Init
 */
void NGS_RefcountInit ( ctx_t ctx, NGS_Refcount * ref,
    struct NGS_VTable const * ivt, const NGS_Refcount_vt * vt,
    const char *clsname, const char *instname );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_refcount_ */
