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

#ifndef _h_sra_sort_tbl_pair_
#define _h_sra_sort_tbl_pair_

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
struct VTable;
struct DbPair;
struct ColumnReader;
struct ColumnWriter;
struct ColumnPair;
struct MetaPair;
struct RowSetIterator;


/*--------------------------------------------------------------------------
 * TablePair
 *  interface to pairing of source and destination tables
 */
typedef struct TablePair_vt TablePair_vt;

typedef struct TablePair TablePair;
struct TablePair
{
    /* table id range */
    int64_t first_id, last_excl;

    /* polymorphic */
    const TablePair_vt *vt;

    /* the pair of VTables */
    struct VTable const *stbl;
    struct VTable *dtbl;

    /* the pair of KMetadata */
    struct MetaPair *meta;

    /* the ColumnPairs */
    Vector static_cols;
    Vector presort_cols;
    Vector large_cols;
    Vector mapped_cols;
    Vector large_mapped_cols;
    Vector normal_cols;

    /* filters */
    const char **exclude_meta;
    const char **exclude_col_names;
    const char **mapped_col_names;
    const char **nonstatic_col_names;
    const char **large_col_names;

    /* row-set id ranges */
    size_t max_idx_ids;
    size_t min_idx_ids;

    /* simple table name */
    const char *name;

    /* full table spec */
    size_t full_spec_size;
    const char *full_spec;

    /* reference counting */
    KRefcount refcount;

    /* true if table will be reordered */
    bool reorder;

    /* true if already exploded */
    bool exploded;

    uint8_t align [ 2 ];
};

#ifndef TBLPAIR_IMPL
#define TBLPAIR_IMPL TablePair
#endif

struct TablePair_vt
{
    void ( * whack ) ( TBLPAIR_IMPL *self, const ctx_t *ctx );
    void ( * pre_explode ) ( TBLPAIR_IMPL *self, const ctx_t *ctx );
    void ( * explode ) ( TBLPAIR_IMPL *self, const ctx_t *ctx );
    struct ColumnPair* ( *make_column_pair ) ( TBLPAIR_IMPL *self, const ctx_t *ctx,
        struct ColumnReader *reader, struct ColumnWriter *writer, const char *colspec, bool large );
    void ( * pre_copy ) ( TBLPAIR_IMPL *self, const ctx_t *ctx );
    void ( * post_copy ) ( TBLPAIR_IMPL *self, const ctx_t *ctx );
    struct RowSetIterator* ( *get_rowset_iter ) ( TBLPAIR_IMPL *self,
        const ctx_t *ctx, bool mapped, bool large );
};


/* Make
 *  make a standard table pair from existing VTable objects
 */
TablePair *TablePairMake ( const ctx_t *ctx,
    struct VTable const *src, struct VTable *dst,
    const char *name, const char *opt_full_spec, bool reorder );

TablePair *DbPairMakeStdTblPair ( struct DbPair *self, const ctx_t *ctx,
    struct VTable const *src, struct VTable *dst, const char *name, bool reorder );


/* Release
 *  called by table at end of copy
 */
void TablePairRelease ( TablePair *self, const ctx_t *ctx );

/* Duplicate
 */
TablePair *TablePairDuplicate ( TablePair *self, const ctx_t *ctx );


/* Copy
 *  copy from source to destination table
 */
void TablePairCopy ( TablePair *self, const ctx_t *ctx );


/* PreExplode
 *  perform explosion before overall coopy
 */
#define TablePairPreExplode( self, ctx ) \
    POLY_DISPATCH_VOID ( pre_explode, self, TBLPAIR_IMPL, ctx )


/* Explode
 *  probably a bad name, but it is intended to mean
 *  register all enclosed columns
 *  and create a proper pair of KMetadata
 */
void TablePairExplode ( TablePair *self, const ctx_t *ctx );


/* AddColumnPair
 *  called from implementations
 */
void TablePairAddColumnPair ( TablePair *self, const ctx_t *ctx, struct ColumnPair *col );


/* PreCopy
 * PostCopy
 *  give table a chance to prepare and cleanup
 */
#define TablePairPreCopy( self, ctx ) \
    POLY_DISPATCH_VOID ( pre_copy, self, TBLPAIR_IMPL, ctx )
#define TablePairPostCopy( self, ctx ) \
    POLY_DISPATCH_VOID ( post_copy, self, TBLPAIR_IMPL, ctx )


/* GetRowSetIterator
 *  returns an iterator for copying
 */
#define TablePairGetRowSetIterator( self, ctx, mapped, large ) \
    POLY_DISPATCH_PTR ( get_rowset_iter, self, TBLPAIR_IMPL, ctx, mapped, large )


/* Init
 */
void TablePairInit ( TablePair *self, const ctx_t *ctx, const TablePair_vt *vt,
    struct VTable const *src, struct VTable *dst,
    const char *name, const char *opt_full_spec, bool reorder );

/* Destroy
 */
void TablePairDestroy ( TablePair *self, const ctx_t *ctx );


#endif /* _h_sra_sort_tbl_pair_ */
