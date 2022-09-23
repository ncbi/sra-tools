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

#ifndef _h_sra_sort_col_pair_
#define _h_sra_sort_col_pair_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct VCursor;
struct TablePair;
struct RowSet;


/*--------------------------------------------------------------------------
 * ColumnReader
 *  interface to read column in random order
 */
typedef struct ColumnReader_vt ColumnReader_vt;

typedef struct ColumnReader ColumnReader;
struct ColumnReader
{
    const ColumnReader_vt *vt;
    KRefcount refcount;
    bool presorted;
    uint8_t align [ 3 ];
};

#ifndef COLREADER_IMPL
#define COLREADER_IMPL ColumnReader
#endif

struct ColumnReader_vt
{
    /* called by Release */
    void ( * whack ) ( COLREADER_IMPL *self, const ctx_t *ctx );

    /* full spec */
    const char* ( * full_spec ) ( const COLREADER_IMPL *self, const ctx_t *ctx );

    /* id-range */
    uint64_t ( * id_range ) ( const COLREADER_IMPL *self, const ctx_t *ctx, int64_t *opt_first );

    /* pre-post copy handlers */
    void ( * pre_copy ) ( COLREADER_IMPL *self, const ctx_t *ctx );
    void ( * post_copy ) ( COLREADER_IMPL *self, const ctx_t *ctx );

    /* retrieve next source row */
    const void* ( * read ) ( COLREADER_IMPL *self, const ctx_t *ctx,
        int64_t row_id, uint32_t *elem_bits, uint32_t *boff, uint32_t *row_len );
};


/* MakeColumnReader
 *  make simple column reader
 */
ColumnReader *TablePairMakeColumnReader ( struct TablePair *self, const ctx_t *ctx,
    struct VCursor const *opt_curs, const char *colspec, bool required );


/* Release
 *  releases reference
 */
void ColumnReaderRelease ( const ColumnReader *self, const ctx_t *ctx );

/* Duplicate
 */
ColumnReader *ColumnReaderDuplicate ( const ColumnReader *self, const ctx_t *ctx );


/* FullSpec
 *  returns 'src.<tbl>.<colspec>'
 */
#define ColumnReaderFullSpec( self, ctx ) \
    POLY_DISPATCH_PTR ( full_spec, self, const COLREADER_IMPL, ctx )


/* IdRange
 *  returns the number of ids available
 *  and optionally the first id
 */
#define ColumnReaderIdRange( self, ctx, opt_first ) \
    POLY_DISPATCH_INT ( id_range, self, const COLREADER_IMPL, ctx, opt_first )


/* PreCopy
 * PostCopy
 *  handlers for any operations
 */
#define ColumnReaderPreCopy( self, ctx ) \
    POLY_DISPATCH_VOID ( pre_copy, self, COLREADER_IMPL, ctx )
#define ColumnReaderPostCopy( self, ctx ) \
    POLY_DISPATCH_VOID ( post_copy, self, COLREADER_IMPL, ctx )


/* Read
 *  read next row of data
 *  returns NULL if no rows are available
 */
#define ColumnReaderRead( self, ctx, row_id, elem_bits, boff, row_len )  \
    POLY_DISPATCH_PTR ( read, self, COLREADER_IMPL, ctx, row_id, elem_bits, boff, row_len )


/* Init
 */
void ColumnReaderInit ( ColumnReader *self, const ctx_t *ctx, const ColumnReader_vt *vt );

/* Destroy
 */
#define ColumnReaderDestroy( self, ctx ) \
    ( ( void ) 0 )


/*--------------------------------------------------------------------------
 * ColumnWriter
 *  interface to write column in serial order
 */
typedef struct ColumnWriter_vt ColumnWriter_vt;

typedef struct ColumnWriter ColumnWriter;
struct ColumnWriter
{
    const ColumnWriter_vt *vt;
    KRefcount refcount;
    bool mapped;
    uint8_t align [ 3 ];
};

#ifndef COLWRITER_IMPL
#define COLWRITER_IMPL ColumnWriter
#endif

struct ColumnWriter_vt
{
    /* called by Release */
    void ( * whack ) ( COLWRITER_IMPL *self, const ctx_t *ctx );

    /* full spec */
    const char* ( * full_spec ) ( const COLWRITER_IMPL *self, const ctx_t *ctx );

    /* pre-post copy handlers */
    void ( * pre_copy ) ( COLWRITER_IMPL *self, const ctx_t *ctx );
    void ( * post_copy ) ( COLWRITER_IMPL *self, const ctx_t *ctx );

    /* write row to destination */
    void ( * write ) ( COLWRITER_IMPL *self, const ctx_t *ctx,
        uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len );
    void ( * write_static ) ( COLWRITER_IMPL *self, const ctx_t *ctx,
        uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len, uint64_t count );

    /* commit all writes */
    void ( * commit ) ( COLWRITER_IMPL *self, const ctx_t *ctx );
};


/* MakeColumnWriter
 *  make simple column writer
 */
ColumnWriter *TablePairMakeColumnWriter ( struct TablePair *self, const ctx_t *ctx,
    struct VCursor *opt_curs, const char *colspec );


/* Release
 *  releases reference
 */
void ColumnWriterRelease ( ColumnWriter *self, const ctx_t *ctx );

/* Duplicate
 */
ColumnWriter *ColumnWriterDuplicate ( const ColumnWriter *self, const ctx_t *ctx );


/* FullSpec
 *  returns 'dst.<tbl>.<colspec>'
 */
#define ColumnWriterFullSpec( self, ctx ) \
    POLY_DISPATCH_PTR ( full_spec, self, const COLWRITER_IMPL, ctx )


/* PreCopy
 * PostCopy
 *  handlers for any operations
 */
#define ColumnWriterPreCopy( self, ctx ) \
    POLY_DISPATCH_VOID ( pre_copy, self, COLWRITER_IMPL, ctx )
#define ColumnWriterPostCopy( self, ctx ) \
    POLY_DISPATCH_VOID ( post_copy, self, COLWRITER_IMPL, ctx )


/* Write
 *  write a row of data
 */
#define ColumnWriterWrite( self, ctx, elem_bits, data, boff, row_len ) \
    POLY_DISPATCH_VOID ( write, self, COLWRITER_IMPL, ctx, elem_bits, data, boff, row_len )

/* WriteStatic
 *  writes a repeated row
 */
#define ColumnWriterWriteStatic( self, ctx, elem_bits, data, boff, row_len, count ) \
    POLY_DISPATCH_VOID ( write_static, self, COLWRITER_IMPL, ctx, elem_bits, data, boff, row_len, count )


/* Commit
 *  commits all writes
 */
#define ColumnWriterCommit( self, ctx ) \
    POLY_DISPATCH_VOID ( commit, self, COLWRITER_IMPL, ctx )


/* Init
 */
void ColumnWriterInit ( ColumnWriter *self, const ctx_t *ctx,
    const ColumnWriter_vt *vt, bool mapped );

/* Destroy
 */
#define ColumnWriterDestroy( self, ctx ) \
    ( ( void ) 0 )


/*--------------------------------------------------------------------------
 * ColumnPair
 *  interface to pairing of source and destination columns
 */
typedef struct ColumnPair ColumnPair;
struct ColumnPair
{
    ColumnReader *reader;
    ColumnWriter *writer;

    const char *colspec;

    size_t full_spec_size;

    KRefcount refcount;

    bool is_static;

    bool is_mapped;

    bool presorted;

    bool large;

    char full_spec [ 1 ];
};


/* MakeColumnPair
 *  make a simple ColumnPair
 *  takes an optional source VCursor
 *  and a colspec
 */
ColumnPair *TablePairMakeColumnPair ( struct TablePair *self, const ctx_t *ctx,
    ColumnReader *reader, ColumnWriter *writer, const char *colspec, bool large );

ColumnPair *TablePairMakeStaticColumnPair ( struct TablePair *self, const ctx_t *ctx,
    ColumnReader *reader, ColumnWriter *writer, const char *colspec );


/* Release
 *  called by table at end of copy
 */
void ColumnPairRelease ( ColumnPair *self, const ctx_t *ctx );

/* Duplicate
 */
ColumnPair *ColumnPairDuplicate ( const ColumnPair *self, const ctx_t *ctx );


/* PreCopy
 * PostCopy
 */
void ColumnPairPreCopy ( const ColumnPair *self, const ctx_t *ctx );
void ColumnPairPostCopy ( const ColumnPair *self, const ctx_t *ctx );


/* Copy
 *  copy from source to destination column
 */
void ColumnPairCopy ( ColumnPair *self, const ctx_t *ctx, struct RowSet *rs );


/* CopyStatic
 *  copy static column from source to destination
 */
void ColumnPairCopyStatic ( ColumnPair *self, const ctx_t *ctx, int64_t first_id, uint64_t count );


#endif /* _h_sra_sort_col_pair_ */
