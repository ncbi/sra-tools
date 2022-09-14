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

#ifndef _h_sra_sort_dir_pair_
#define _h_sra_sort_dir_pair_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct DbPair;
struct KDirectory;


/*--------------------------------------------------------------------------
 * DirPair
 *  interface to pairing of source and destination directories
 */
typedef struct DirPair_vt DirPair_vt;

typedef struct DirPair DirPair;
struct DirPair
{
    /* polymorphic */
    const DirPair_vt *vt;

    /* the pair of KDirectories */
    struct KDirectory const *sdir;
    struct KDirectory *ddir;

    /* simple directory name */
    const char *name;

    /* full directory spec */
    size_t full_spec_size;
    const char *full_spec;

    /* reference counting */
    KRefcount refcount;

    /* owner spec size */
    uint32_t owner_spec_size;
};

#ifndef DIRPAIR_IMPL
#define DIRPAIR_IMPL DirPair
#endif

struct DirPair_vt
{
    void ( * whack ) ( DIRPAIR_IMPL *self, const ctx_t *ctx );
};


/* Make
 *  make a standard directory pair from existing KDirectory objects
 */
DirPair *DirPairMake ( const ctx_t *ctx,
    struct KDirectory const *src, struct KDirectory *dst,
    const char *name, const char *full_spec );

DirPair *DbPairMakeStdDirPair ( struct DbPair *self, const ctx_t *ctx,
    struct KDirectory const *src, struct KDirectory *dst, const char *name );


/* Release
 *  called by db at end of copy
 */
void DirPairRelease ( DirPair *self, const ctx_t *ctx );

/* Duplicate
 */
DirPair *DirPairDuplicate ( DirPair *self, const ctx_t *ctx );


/* Copy
 *  copy from source to destination directory
 */
void DirPairCopy ( DirPair *self, const ctx_t *ctx );


/* Init
 */
void DirPairInit ( DirPair *self, const ctx_t *ctx, const DirPair_vt *vt,
    struct KDirectory const *src, struct KDirectory *dst,
    const char *name, const char *opt_full_spec );

/* Destroy
 */
void DirPairDestroy ( DirPair *self, const ctx_t *ctx );


#endif /* _h_sra_sort_dir_pair_ */
