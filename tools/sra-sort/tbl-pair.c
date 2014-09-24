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

typedef struct TablePair StdTblPair;
#define TBLPAIR_IMPL StdTblPair

#include "tbl-pair.h"
#include "col-pair.h"
#include "db-pair.h"
#include "meta-pair.h"
#include "row-set-priv.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <kdb/meta.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/namelist.h>
#include <klib/rc.h>

#include <string.h>


FILE_ENTRY ( tbl-pair );


/*--------------------------------------------------------------------------
 * StdTblPair
 *  generic database object pair
 */

static
void StdTblPairWhack ( StdTblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    TablePairDestroy ( self, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
void StdTblPairDummyStub ( StdTblPair *self, const ctx_t *ctx )
{
}

static
ColumnPair *StdTblPairMakeColumnPair ( StdTblPair *self, const ctx_t *ctx,
    struct ColumnReader *reader, struct ColumnWriter *writer, const char *colspec, bool large )
{
    FUNC_ENTRY ( ctx );
    return TablePairMakeColumnPair ( self, ctx, reader, writer, colspec, large );
}

static
RowSetIterator *StdTblPairGetRowSetIterator ( StdTblPair *self, const ctx_t *ctx, bool mapped, bool large )
{
    FUNC_ENTRY ( ctx );
    return TablePairMakeRowSetIterator ( self, ctx, NULL, mapped, large );
}

static TablePair_vt StdTblPair_vt =
{
    StdTblPairWhack,
    StdTblPairDummyStub,
    StdTblPairDummyStub,
    StdTblPairMakeColumnPair,
    StdTblPairDummyStub,
    StdTblPairDummyStub,
    StdTblPairGetRowSetIterator
};


/*--------------------------------------------------------------------------
 * TablePair
 *  interface code
 */


/* Make
 *  makes an object based upon open source and destination VDatabase objects
 */
TablePair *TablePairMake ( const ctx_t *ctx,
    const VTable *src, VTable *dst, const char *name, const char *opt_full_spec, bool reorder )
{
    FUNC_ENTRY ( ctx );

    StdTblPair *tbl;

    TRY ( tbl = MemAlloc ( ctx, sizeof * tbl, false ) )
    {
        TRY ( TablePairInit ( tbl, ctx, & StdTblPair_vt, src, dst, name, opt_full_spec, reorder ) )
        {
            return tbl;
        }

        MemFree ( ctx, tbl, sizeof * tbl );
    }

    return NULL;
}

TablePair *DbPairMakeStdTblPair ( DbPair *self, const ctx_t *ctx,
    const VTable *src, VTable *dst, const char *name, bool reorder )
{
    FUNC_ENTRY ( ctx );
    return TablePairMake ( ctx, src, dst, name, self -> full_spec, reorder );
}


/* Release
 */
void TablePairRelease ( TablePair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "TablePair" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcTable, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release TablePair" );
        }
    }
}

/* Duplicate
 */
TablePair *TablePairDuplicate ( TablePair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "TablePair" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcTable, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate TablePair" );
            return NULL;
        }
    }

    return ( TablePair* ) self;
}


/* CommitColumnPair
 */
static
bool CC TablePairCommitColumnPair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    ColumnPair *col = item;
    TRY ( ColumnWriterCommit ( col -> writer, ctx ) )
    {
        return false;
    }
    return true;
}


/* ReleaseColumnPair
 */
static
void CC TablePairReleaseColumnPair ( void *item, void *data )
{
    const ctx_t *ctx = ( const void* ) data;
    FUNC_ENTRY ( ctx );
    ColumnPairRelease ( item, ctx );
}


/* Copy
 *  the table has to obtain a RowSetIterator
 *  which it walks vertically
 *
 *  for each RowSet, it walks its columns horizontally,
 *  resetting it between columns
 */
static
void TablePairCopyStaticColumns ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* copy static columns */
    uint32_t count = VectorLength ( & self -> static_cols );
    if ( count != 0 )
    {
#if OLD_STATIC_WRITE
        RowSetIterator *rsi;
        TRY ( rsi = TablePairMakeRowSetIterator ( self, ctx, NULL, false ) )
        {
#endif
            STATUS ( 2, "copying '%s' static columns", self -> full_spec );

            while ( ! FAILED () )
            {
                uint32_t i;

#if OLD_STATIC_WRITE
                RowSet *rs;
                ON_FAIL ( rs = RowSetIteratorNext ( rsi, ctx ) )
                    break;
                if ( rs == NULL )
                    break;
#endif
                for ( i = 0; i < count; ++ i )
                {
                    ColumnPair *col = VectorGet ( & self -> static_cols, i );
                    assert ( col != NULL );
#if OLD_STATIC_WRITE
                    ON_FAIL ( ColumnPairCopy ( col, ctx, rs ) )
                        break;
#else
                    ON_FAIL ( ColumnPairCopyStatic ( col, ctx, self -> first_id, self -> last_excl - self -> first_id ) )
                        break;
#endif
                }

#if OLD_STATIC_WRITE
                RowSetRelease ( rs, ctx );
#else
                break;
#endif
            }

#if OLD_STATIC_WRITE
            RowSetIteratorRelease ( rsi, ctx );
        }
#endif

        /* commit columns */
        if ( ! FAILED () )
        {
            STATUS ( 3, "committing static columns" );
            VectorDoUntil ( & self -> static_cols, false, TablePairCommitColumnPair, ( void* ) ctx );
        }

        /* douse columns */
        STATUS ( 3, "releasing static columns" );
        VectorWhack ( & self -> static_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    }
}

static
void TablePairCopyPresortColumns ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* copy remaining columns */
    uint32_t count = VectorLength ( & self -> presort_cols );
    if ( count != 0 )
    {
        RowSetIterator *rsi;
        TRY ( rsi = TablePairMakeSimpleRowSetIterator ( self, ctx ) )
        {
            STATUS ( 2, "copying '%s' presorted columns", self -> full_spec );

            while ( ! FAILED () )
            {
                uint32_t i;

                RowSet *rs;
                ON_FAIL ( rs = RowSetIteratorNext ( rsi, ctx ) )
                    break;
                if ( rs == NULL )
                    break;

                for ( i = 0; i < count; ++ i )
                {
                    ColumnPair *col = VectorGet ( & self -> presort_cols, i );
                    assert ( col != NULL );
                    ON_FAIL ( ColumnPairCopy ( col, ctx, rs ) )
                        break;
                }

                RowSetRelease ( rs, ctx );
            }

            RowSetIteratorRelease ( rsi, ctx );
        }

        /* commit columns */
        if ( ! FAILED () )
        {
            STATUS ( 3, "committing presorted columns" );
            VectorDoUntil ( & self -> presort_cols, false, TablePairCommitColumnPair, ( void* ) ctx );
        }

        /* douse columns */
        STATUS ( 3, "releasing presorted columns" );
        VectorWhack ( & self -> presort_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    }
}

static
void TablePairCopyMappedColumns ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* copy mapped columns */
    uint32_t count = VectorLength ( & self -> mapped_cols );
    if ( count != 0 )
    {
        RowSetIterator *rsi;
        const bool is_mapped = true;
        const bool is_large = false;
        TRY ( rsi = TablePairGetRowSetIterator ( self, ctx, is_mapped, is_large ) )
        {
            STATUS ( 2, "copying '%s' mapped columns", self -> full_spec );

            while ( ! FAILED () )
            {
                uint32_t i;

                RowSet *rs;
                ON_FAIL ( rs = RowSetIteratorNext ( rsi, ctx ) )
                    break;
                if ( rs == NULL )
                    break;

                for ( i = 0; i < count; ++ i )
                {
                    ColumnPair *col = VectorGet ( & self -> mapped_cols, i );
                    assert ( col != NULL );
                    ON_FAIL ( ColumnPairCopy ( col, ctx, rs ) )
                        break;
                }

                RowSetRelease ( rs, ctx );
            }

            RowSetIteratorRelease ( rsi, ctx );
        }

        /* commit columns */
        if ( ! FAILED () )
        {
            STATUS ( 3, "committing mapped columns" );
            VectorDoUntil ( & self -> mapped_cols, false, TablePairCommitColumnPair, ( void* ) ctx );
        }

        /* douse columns */
        STATUS ( 3, "releasing mapped columns" );
        VectorWhack ( & self -> mapped_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    }
}

static
void TablePairCopyLargeColumns ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* copy large columns */
    uint32_t count = VectorLength ( & self -> large_cols );
    if ( count != 0 )
    {
        RowSetIterator *rsi;
        const bool is_mapped = false;
        const bool is_large = true;
        TRY ( rsi = TablePairGetRowSetIterator ( self, ctx, is_mapped, is_large ) )
        {
            STATUS ( 2, "copying '%s' large columns", self -> full_spec );

            while ( ! FAILED () )
            {
                uint32_t i;

                RowSet *rs;
                ON_FAIL ( rs = RowSetIteratorNext ( rsi, ctx ) )
                    break;
                if ( rs == NULL )
                    break;

                for ( i = 0; i < count; ++ i )
                {
                    ColumnPair *col = VectorGet ( & self -> large_cols, i );
                    assert ( col != NULL );
                    ON_FAIL ( ColumnPairCopy ( col, ctx, rs ) )
                        break;
                }

                RowSetRelease ( rs, ctx );
            }

            RowSetIteratorRelease ( rsi, ctx );
        }

        /* commit columns */
        if ( ! FAILED () )
        {
            STATUS ( 3, "committing large columns" );
            VectorDoUntil ( & self -> large_cols, false, TablePairCommitColumnPair, ( void* ) ctx );
        }

        /* douse columns */
        STATUS ( 3, "releasing large columns" );
        VectorWhack ( & self -> large_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    }
}

static
void TablePairCopyLargeMappedColumns ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* copy large mapped columns */
    uint32_t count = VectorLength ( & self -> large_mapped_cols );
    if ( count != 0 )
    {
        RowSetIterator *rsi;
        const bool is_mapped = true;
        const bool is_large = true;
        TRY ( rsi = TablePairGetRowSetIterator ( self, ctx, is_mapped, is_large ) )
        {
            STATUS ( 2, "copying '%s' large mapped columns", self -> full_spec );

            while ( ! FAILED () )
            {
                uint32_t i;

                RowSet *rs;
                ON_FAIL ( rs = RowSetIteratorNext ( rsi, ctx ) )
                    break;
                if ( rs == NULL )
                    break;

                for ( i = 0; i < count; ++ i )
                {
                    ColumnPair *col = VectorGet ( & self -> large_mapped_cols, i );
                    assert ( col != NULL );
                    ON_FAIL ( ColumnPairCopy ( col, ctx, rs ) )
                        break;
                }

                RowSetRelease ( rs, ctx );
            }

            RowSetIteratorRelease ( rsi, ctx );
        }

        /* commit columns */
        if ( ! FAILED () )
        {
            STATUS ( 3, "committing large mapped columns" );
            VectorDoUntil ( & self -> large_mapped_cols, false, TablePairCommitColumnPair, ( void* ) ctx );
        }

        /* douse columns */
        STATUS ( 3, "releasing large mapped columns" );
        VectorWhack ( & self -> large_mapped_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    }
}

static
void TablePairCopyNormalColumns ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* copy remaining columns */
    uint32_t count = VectorLength ( & self -> normal_cols );
    if ( count != 0 )
    {
        RowSetIterator *rsi;
        const bool is_mapped = false;
        const bool is_large = false;
        TRY ( rsi = TablePairGetRowSetIterator ( self, ctx, is_mapped, is_large ) )
        {
            STATUS ( 2, "copying '%s' columns", self -> full_spec );

            while ( ! FAILED () )
            {
                uint32_t i;

                RowSet *rs;
                ON_FAIL ( rs = RowSetIteratorNext ( rsi, ctx ) )
                    break;
                if ( rs == NULL )
                    break;

                for ( i = 0; i < count; ++ i )
                {
                    ColumnPair *col = VectorGet ( & self -> normal_cols, i );
                    assert ( col != NULL );
                    ON_FAIL ( ColumnPairCopy ( col, ctx, rs ) )
                        break;
                }

                RowSetRelease ( rs, ctx );
            }

            RowSetIteratorRelease ( rsi, ctx );
        }

        /* commit columns */
        if ( ! FAILED () )
        {
            STATUS ( 3, "committing columns" );
            VectorDoUntil ( & self -> normal_cols, false, TablePairCommitColumnPair, ( void* ) ctx );
        }

        /* douse columns */
        STATUS ( 3, "releasing columns" );
        VectorWhack ( & self -> normal_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    }
}

void TablePairCopy ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    size_t in_use, quota;

    /* copy columns */
    

    ON_FAIL ( TablePairExplode ( self, ctx ) )
        return;

    STATUS ( 2, "copying table '%s'", self -> full_spec );

    in_use = MemInUse ( ctx, & quota );
    if ( ( quota + 1 ) != 0 )
        STATUS ( 4, "MEMORY: %,zu bytes used out of %,zu total ( %u%% )", in_use, quota, ( uint32_t ) ( in_use * 100 ) / quota );
    else
        STATUS ( 4, "MEMORY: %,zu bytes used", in_use );

    TRY ( TablePairPreCopy ( self, ctx ) )
    {
        TRY ( TablePairCopyStaticColumns ( self, ctx ) )
        {
            TRY ( TablePairCopyPresortColumns ( self, ctx ) )
            {
                TRY ( TablePairCopyMappedColumns ( self, ctx ) )
                {
                    TRY ( TablePairCopyLargeColumns ( self, ctx ) )
                    {
                        TRY ( TablePairCopyLargeMappedColumns ( self, ctx ) )
                        {
                            TablePairCopyNormalColumns ( self, ctx );
                        }
                    }
                }
            }
        }
    }

    /* cleanup */
    if ( ! FAILED () )
        TablePairPostCopy ( self, ctx );

    in_use = MemInUse ( ctx, & quota );
    if ( ( quota + 1 ) != 0 )
        STATUS ( 4, "MEMORY: %,zu bytes used out of %,zu total ( %u%% )", in_use, quota, ( uint32_t ) ( in_use * 100 ) / quota );
    else
        STATUS ( 4, "MEMORY: %,zu bytes used", in_use );

    STATUS ( 2, "finished with table '%s'", self -> full_spec );
}


/* Explode
 *  probably a bad name, but it is intended to mean
 *  register all enclosed columns
 *  and create a proper pair of KMetadata
 */
static
MetaPair *TablePairExplodeMetaPair ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    MetaPair *meta = NULL;
    const KMetadata *smeta;

    rc = VTableOpenMetadataRead ( self -> stbl, & smeta );
    if ( rc != 0 )
        ERROR ( rc, "VTableOpenMetadataRead failed on 'src.%s'", self -> full_spec );
    else
    {
        KMetadata *dmeta;
        rc = VTableOpenMetadataUpdate ( self -> dtbl, & dmeta );
        if ( rc != 0 )
            ERROR ( rc, "VTableOpenMetadataUpdate failed on 'dst.%s'", self -> full_spec );
        else
        {
            meta = MetaPairMake ( ctx, smeta, dmeta, self -> full_spec );

            KMetadataRelease ( dmeta );
        }
        
        KMetadataRelease ( smeta );
    }

    return meta;
}

static
ColumnPair *TablePairMatchColumnPair ( TablePair *self, const ctx_t *ctx, const char *name, bool required )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    KNamelist *types;
    ColumnPair *col = NULL;

    rc = VTableListWritableDatatypes ( self -> dtbl, name, & types );
    if ( rc != 0 )
        ERROR ( rc, "VTableListWritableDatatypes failed listing 'dst.%s.%s' datatypes", self -> full_spec, name );
    else
    {
        uint32_t count;
        rc = KNamelistCount ( types, & count );
        if ( rc != 0 )
            ERROR ( rc, "KNamelistCount failed listing 'dst.%s.%s' datatypes", self -> full_spec, name );
        else
        {
            const VCursor *scurs;
            rc = VTableCreateCursorRead ( self -> stbl, & scurs );
            if ( rc != 0 )
                ERROR ( rc, "VTableCreateCursorRead failed on 'src.%s'", self -> full_spec, name );
            else
            {
                rc = VCursorPermitPostOpenAdd ( scurs );
                if ( rc != 0 )
                    ERROR ( rc, "VCursorPermitPostOpenAdd failed on 'src.%s'", self -> full_spec, name );
                else
                {
                    rc = VCursorOpen ( scurs );
                    if ( rc != 0 )
                        ERROR ( rc, "VCursorOpen failed with no contents on 'src.%s'", self -> full_spec, name );
                    else
                    {
                        uint32_t i;
                        for ( i = 0; i < count; ++ i )
                        {
                            uint32_t idx;
                            char colspec [ 256 ];

                            const char *td;
                            rc = KNamelistGet ( types, i, & td );
                            if ( rc != 0 )
                            {
                                ERROR ( rc, "KNamelistGet ( %u ) failed listing 'dst.%s.%s' datatypes", i, self -> full_spec, name );
                                break;
                            }

                            rc = string_printf ( colspec, sizeof colspec, NULL, "(%s)%s", td, name );
                            if ( rc != 0 )
                            {
                                ABORT ( rc, "failed creating colspec for 'dst.%s.%s'", self -> full_spec, name );
                                break;
                            }

                            rc = VCursorAddColumn ( scurs, & idx, "%s", colspec );
                            if ( rc == 0 )
                            {
                                ColumnReader *reader;
                                TRY ( reader = TablePairMakeColumnReader ( self, ctx, scurs, colspec, true ) )
                                {
                                    ColumnWriter *writer;
                                    TRY ( writer = TablePairMakeColumnWriter ( self, ctx, NULL, colspec ) )
                                    {
                                        uint32_t j;
                                        bool large = false;
                                        bool is_static = VTableHasStaticColumn ( self -> stbl, name );
                                        if ( is_static )
                                        {
                                            if ( self -> nonstatic_col_names != NULL )
                                            {
                                                for ( j = 0; self -> nonstatic_col_names [ j ] != NULL; ++ j )
                                                {
                                                    if ( strcmp ( name, self -> nonstatic_col_names [ j ] ) == 0 )
                                                    {
                                                        is_static = false;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                        else if ( self -> large_col_names != NULL )
                                        {
                                            for ( j = 0; self -> large_col_names [ j ] != NULL; ++ j )
                                            {
                                                if ( strcmp ( name, self -> large_col_names [ j ] ) == 0 )
                                                {
                                                    large = true;
                                                    break;
                                                }
                                            }
                                        }

                                        /* at this point, if the column is static, then avoid
                                           any rigorous treatment and create an otherwise normal
                                           column pair. may want to special case it for reporting */
                                        col = is_static
                                            ? TablePairMakeStaticColumnPair ( self, ctx, reader, writer, colspec )
                                            : ( * self -> vt -> make_column_pair ) ( ( TBLPAIR_IMPL* ) self, ctx, reader, writer, colspec, large );

                                        ColumnWriterRelease ( writer, ctx );
                                    }

                                    ColumnReaderRelease ( reader, ctx );
                                }
                                break;
                            }
                        }

                        if ( col == NULL && ! FAILED () )
                            ANNOTATE ( "column '%s' cannot be written", name );
                    }
                }


                VCursorRelease ( scurs );
            }
        }

        KNamelistRelease ( types );
    }

    return col;
}

static
void TablePairDefaultExplode ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    KNamelist *pnames;

    /* look at the physical columns in source */
    rc = VTableListPhysColumns ( self -> stbl, & pnames );
    if ( rc != 0 )
        ERROR ( rc, "VTableListWritableColumns failed listing 'src.%s' physical columns", self -> full_spec );
    else
    {
        KNamelist *dnames;

        /* this gives us the columns that could potentially take input */
        rc = VTableListSeededWritableColumns ( self -> dtbl, & dnames, pnames );
        if ( rc != 0 )
            ERROR ( rc, "VTableListWritableColumns failed listing 'dst.%s' columns", self -> full_spec );
        else
        {
            uint32_t count;
            rc = KNamelistCount ( dnames, & count );
            if ( rc != 0 )
                ERROR ( rc, "KNamelistCount failed listing 'dst.%s' columns", self -> full_spec );
            else if ( count > 0 )
            {
                uint32_t i;
                for ( i = 0; ! FAILED () && i < count; ++ i )
                {
                    ColumnPair *col;

                    const char *name;
                    rc = KNamelistGet ( dnames, i, & name );
                    if ( rc != 0 )
                    {
                        ERROR ( rc, "KNamelistGet ( %u ) failed listing 'dst.%s' columns", i, self -> full_spec );
                        break;
                    }

                    /* filter out the column names that are specifically excluded */
                    if ( self -> exclude_col_names != NULL )
                    {
                        uint32_t j;
                        for ( j = 0; self -> exclude_col_names [ j ] != NULL; ++ j )
                        {
                            if ( strcmp ( name, self -> exclude_col_names [ j ] ) == 0 )
                            {
                                name = NULL;
                                break;
                            }
                        }
                        if ( name == NULL )
                            continue;
                    }

                    TRY ( col = TablePairMatchColumnPair ( self, ctx, name, true ) )
                    {
                        if ( col != NULL )
                        {
                            ON_FAIL ( TablePairAddColumnPair ( self, ctx, col ) )
                            {
                                ColumnPairRelease ( col, ctx );
                            }
                        }
                    }
                }
            }
            
            KNamelistRelease ( dnames );
        }

        KNamelistRelease ( pnames );
    }
}

void TablePairExplode ( TablePair *self, const ctx_t *ctx )
{
    if ( ! self -> exploded )
    {
        FUNC_ENTRY ( ctx );

        STATUS ( 2, "exploding table '%s'", self -> full_spec );

        /* first, ask subclass to perform explicit explode */
        TRY ( ( * self -> vt -> explode ) ( self, ctx ) )
        {
            if ( self -> meta == NULL )
            {
                TRY ( self -> meta = TablePairExplodeMetaPair ( self, ctx ) )
                {
                    MetaPairCopy ( self -> meta, ctx, self -> full_spec, self -> exclude_meta );
                    STATUS ( 3, "releasing metadata" );
                    MetaPairRelease ( self -> meta, ctx );
                    self -> meta = NULL;
                }
            }

            if ( ! FAILED () )
            {
                /* next, perform default self-explode for all columns not excluded */
                TRY ( TablePairDefaultExplode ( self, ctx ) )
                {
                    /* now create metadata pair */
                    if ( self -> meta == NULL )
                        self -> meta = TablePairExplodeMetaPair ( self, ctx );
                    if ( ! FAILED () )
                        self -> exploded = true;
                }
            }
        }
    }
}


/* AddColumnPair
 *  called from implementations
 */
void TablePairAddColumnPair ( TablePair *self, const ctx_t *ctx, ColumnPair *col )
{
    if ( col != NULL )
    {
        FUNC_ENTRY ( ctx );

        rc_t rc;
        Vector *v = & self -> normal_cols;

        const char *col_type = "";
        const char *col_size = "";

        if ( col -> large )
        {
            col_size = "large ";
            v = & self -> large_cols;
        }

        if ( col -> is_static )
        {
            col_type = "static ";
            v = & self -> static_cols;
        }
        else if ( col -> presorted )
        {
            col_type = "presorted ";
            v = & self -> presort_cols;
        }
        else if ( col -> is_mapped )
        {
            col_type = "mapped ";
            v = col -> large ? & self -> large_mapped_cols : & self -> mapped_cols;
        }

        STATUS ( 4, "adding %s%scolumn pair '%s'", col_size, col_type, col -> full_spec );
        rc = VectorAppend ( v, NULL, ( const void* ) col );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "failed to add column pair '%s'", col -> full_spec );
        else
        {
            int64_t first;
            uint64_t count;

            TRY ( count = ColumnReaderIdRange ( col -> reader, ctx, & first ) )
            {
                int64_t last_excl = first + count;
                if ( first < self -> first_id )
                    self -> first_id = first;
                /* Static columns may have incorrect range in legacy data */
                if ( last_excl > self -> last_excl && !col->is_static)
                    self -> last_excl = last_excl;
            }
        }
    }
}


/* Init
 */
void TablePairInit ( TablePair *self, const ctx_t *ctx, const TablePair_vt *vt,
    const VTable *src, VTable *dst, const char *name, const char *opt_full_spec, bool reorder )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    memset ( self, 0, sizeof * self );
    self -> vt = vt;

    VectorInit ( & self -> static_cols, 0, 32 );
    VectorInit ( & self -> presort_cols, 0, 4 );
    VectorInit ( & self -> large_cols, 0, 32 );
    VectorInit ( & self -> mapped_cols, 0, 32 );
    VectorInit ( & self -> large_mapped_cols, 0, 32 );
    VectorInit ( & self -> normal_cols, 0, 32 );

    if ( opt_full_spec == NULL )
        opt_full_spec = "";

    rc = VTableAddRef ( self -> stbl = src );
    if ( rc != 0 )
        ERROR ( rc, "failed to duplicate tbl 'src.%s%s%s'", opt_full_spec, opt_full_spec [ 0 ] ? "." : "", name );
    else
    {
        rc = VTableAddRef ( self -> dtbl = dst );
        if ( rc != 0 )
            ERROR ( rc, "failed to duplicate tbl 'dst.%s%s%s'", opt_full_spec, opt_full_spec [ 0 ] ? "." : "", name );
        else
        {
            char *full_spec;

            self -> full_spec_size = string_size ( opt_full_spec ) + string_size ( name );
            if ( opt_full_spec [ 0 ] != 0  )
                ++ self -> full_spec_size;

            TRY ( full_spec = MemAlloc ( ctx, self -> full_spec_size + 1, false ) )
            {
                if ( opt_full_spec [ 0 ] != 0 )
                    rc = string_printf ( full_spec, self -> full_spec_size + 1, NULL, "%s.%s", opt_full_spec, name );
                else
                    strcpy ( full_spec, name );
                if ( rc != 0 )
                    ABORT ( rc, "miscalculated string size" );
                else
                {
                    const Tool *tp = ctx -> caps -> tool;

                    self -> full_spec = full_spec;
                    self -> name = full_spec;
                    if ( opt_full_spec [ 0 ] != 0 )
                        self -> name += string_size ( opt_full_spec ) + 1;

                    /* start with an invalid range */
                    self -> first_id = ( int64_t ) ( ~ ( uint64_t ) 0 >> 1 );
                    self -> last_excl = - self -> first_id;

                    /* row-set id ranges */
                    self -> max_idx_ids = tp -> max_idx_ids;
                    self -> min_idx_ids = tp -> min_idx_ids;

                    /* record reordering */
                    self -> reorder = reorder;

                    KRefcountInit ( & self -> refcount, 1, "TablePair", "init", name );
                    return;
                }
            }

            VTableRelease ( self -> dtbl );
            self -> dtbl = NULL;
        }

        VTableRelease ( self -> stbl );
        self -> stbl = NULL;
    }
}

/* Destroy
 *  destroys superclass items
 */
void TablePairDestroy ( TablePair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    VectorWhack ( & self -> static_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    VectorWhack ( & self -> presort_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    VectorWhack ( & self -> large_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    VectorWhack ( & self -> mapped_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    VectorWhack ( & self -> large_mapped_cols, TablePairReleaseColumnPair, ( void* ) ctx );
    VectorWhack ( & self -> normal_cols, TablePairReleaseColumnPair, ( void* ) ctx );

    MetaPairRelease ( self -> meta, ctx );

    rc = VTableReindex ( self -> dtbl );
    if ( rc != 0 )
        WARN ( "VTableReindex failed on 'dst.%s'", self -> full_spec );
    rc = VTableRelease ( self -> dtbl );
    if ( rc != 0 )
        WARN ( "VTableRelease failed on 'dst.%s'", self -> full_spec );
    VTableRelease ( self -> stbl );

    MemFree ( ctx, ( void* ) self -> full_spec, self -> full_spec_size + 1 );

    memset ( self, 0, sizeof * self );
}
