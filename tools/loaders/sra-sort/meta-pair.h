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

#ifndef _h_sra_sort_meta_pair_
#define _h_sra_sort_meta_pair_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct KMetadata;


/*--------------------------------------------------------------------------
 * MetaPair
 *  interface to pairing of source and destination tables
 */
typedef struct MetaPair_vt MetaPair_vt;

typedef struct MetaPair MetaPair;
struct MetaPair
{
    /* polymorphic */
    const MetaPair_vt *vt;

    /* the pair of KMetadata */
    struct KMetadata const *smeta;
    struct KMetadata *dmeta;

    KRefcount refcount;
};

#ifndef METAPAIR_IMPL
#define METAPAIR_IMPL MetaPair
#endif

struct MetaPair_vt
{
    void ( * whack ) ( METAPAIR_IMPL *self, const ctx_t *ctx );
};


/* Make
 *  make a standard metadata pair from existing KMetadata objects
 */
MetaPair *MetaPairMake ( const ctx_t *ctx,
    struct KMetadata const *src, struct KMetadata *dst,
    const char *full_path );


/* Release
 *  called by table at end of copy
 */
void MetaPairRelease ( MetaPair *self, const ctx_t *ctx );

/* Duplicate
 */
MetaPair *MetaPairDuplicate ( MetaPair *self, const ctx_t *ctx );


/* Init
 */
void MetaPairInit ( MetaPair *self, const ctx_t *ctx, const MetaPair_vt *vt,
    struct KMetadata const *src, struct KMetadata *dst, const char *full_spec );

/* Destroy
 */
void MetaPairDestroy ( MetaPair *self, const ctx_t *ctx );


/* Copy
 *  copy from source to destination
 */
void MetaPairCopy ( MetaPair *self, const ctx_t *ctx,
    const char *owner_spec, const char **exclude );


#endif /* _h_sra_sort_meta_pair_ */
