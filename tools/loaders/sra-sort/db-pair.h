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

#ifndef _h_sra_sort_db_pair_
#define _h_sra_sort_db_pair_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct KDirectory;
struct VTable;
struct VDatabase;
struct TablePair;
struct MetaPair;
struct DirPair;


/*--------------------------------------------------------------------------
 * DbPair
 *  interface to pairing of source and destination tables
 */
typedef struct DbPair_vt DbPair_vt;

typedef struct DbPair DbPair;
struct DbPair
{
    /* polymorphic */
    const DbPair_vt *vt;

    /* the pair of VDatabases */
    struct VDatabase const *sdb;
    struct VDatabase *ddb;

    /* the set of enclosed DbPairs */
    Vector dbs;

    /* the set of TablePairs */
    Vector tbls;

    /* the set of DirPairs */
    Vector dirs;

    /* the pair of KMetadata */
    struct MetaPair *meta;

    /* exclusions */
    const char **exclude_dbs;
    const char **exclude_tbls;
    const char **exclude_meta;

    /* simple db name */
    const char *name;

    /* full db spec */
    size_t full_spec_size;
    const char *full_spec;

    /* reference counting */
    KRefcount refcount;
    uint32_t align;
};

#ifndef DBPAIR_IMPL
#define DBPAIR_IMPL DbPair
#endif

struct DbPair_vt
{
    void ( * whack ) ( DBPAIR_IMPL *self, const ctx_t *ctx );
    void ( * explode ) ( DBPAIR_IMPL *self, const ctx_t *ctx );
    const char* ( * get_tbl_mbr ) ( const DBPAIR_IMPL *self, const ctx_t *ctx,
        struct VTable const *src, const char *name );
};


/* Make
 *  makes an object based upon open source and destination VDatabase objects
 */
DbPair *DbPairMake ( const ctx_t *ctx,
    struct VDatabase const *src, struct VDatabase *dst, const char *name );


/* Release
 */
void DbPairRelease ( DbPair *self, const ctx_t *ctx );

/* Duplicate
 */
DbPair *DbPairDuplicate ( DbPair *self, const ctx_t *ctx );


/* Copy
 *  copy from source to destination
 */
void DbPairCopy ( DbPair *self, const ctx_t *ctx );


/* Run
 */
void DbPairRun ( DbPair *self, const ctx_t *ctx );


/* MakeDbPair
 */
DbPair *DbPairMakeDbPair ( DbPair *self, const ctx_t *ctx, const char *member, const char *name, bool required,
    DbPair* ( * make ) ( DbPair *self, const ctx_t *ctx,
        struct VDatabase const *src, struct VDatabase *dst, const char *name )
     );

DbPair *DbPairMakeStdDbPair ( struct DbPair *self, const ctx_t *ctx,
    struct VDatabase const *src, struct VDatabase *dst, const char *name );


/* MakeTablePair
 */
struct TablePair *DbPairMakeTablePair ( DbPair *self, const ctx_t *ctx,
    const char *member, const char *name, bool required, bool reorder,
    struct TablePair* ( * make ) ( DbPair *self, const ctx_t *ctx,
        struct VTable const *src, struct VTable *dst, const char *name, bool reorder )
     );


/* GetTblMember
 *  v1 schema does not preserve member name with an object
 *  some schemas confuse types with the same name
 */
#define DbPairGetTblMember( self, ctx, src, name ) \
    POLY_DISPATCH_PTR ( get_tbl_mbr, self, const DBPAIR_IMPL, ctx, src, name )


/* AddDbPair
 *  called from implementations
 */
void DbPairAddDbPair ( DbPair *self, const ctx_t *ctx, DbPair *db );


/* AddTablePair
 *  called from implementations
 */
void DbPairAddTablePair ( DbPair *self, const ctx_t *ctx, struct TablePair *tbl );


/* MakeDirPair
 */
struct DirPair *DbPairMakeDirPair ( DbPair *self, const ctx_t *ctx, const char *name, bool required,
    struct DirPair* ( * make ) ( DbPair *self, const ctx_t *ctx,
        struct KDirectory const *src, struct KDirectory *dst, const char *name )
     );


/* AddDirPair
 *  called from implementations
 */
void DbPairAddDirPair ( DbPair *self, const ctx_t *ctx, struct DirPair *dir );


/* Init
 *  initialize superclass with vt
 *  also db pair and name
 *  plus optional full-spec
 */
void DbPairInit ( DbPair *self, const ctx_t *ctx, const DbPair_vt *vt,
    struct VDatabase const *src, struct VDatabase *dst,
    const char *name, const char *opt_full_spec );

/* Destroy
 *  destroys superclass items
 */
void DbPairDestroy ( DbPair *self, const ctx_t *ctx );


#endif /* _h_sra_sort_db_pair_ */
