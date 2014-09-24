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

typedef struct SimpleColumnReader SimpleColumnReader;
#define COLREADER_IMPL SimpleColumnReader

typedef struct SimpleColumnWriter SimpleColumnWriter;
#define COLWRITER_IMPL SimpleColumnWriter

#include "col-pair.h"
#include "tbl-pair.h"
#include "row-set.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"

#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <kapp/main.h>
#include <kfs/defs.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>

FILE_ENTRY ( col-pair );


/*--------------------------------------------------------------------------
 * SimpleColumnReader
 *  implementation of ColumnReader based upon VCursor direct read
 */
struct SimpleColumnReader
{
    ColumnReader dad;

    const VCursor *curs;
    uint32_t idx;

    uint32_t full_spec_size;
    char full_spec [ 1 ];
};


/* Whack
 */
static
void SimpleColumnReaderWhack ( SimpleColumnReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    size_t bytes = sizeof * self + self -> full_spec_size;

    rc = VCursorRelease ( self -> curs );
    if ( rc != 0 )
        WARN ( "VCursorRelease failed on '%s'", self -> full_spec );

    self -> curs = NULL;
    MemFree ( ctx, self, bytes );
}

/* FullSpec
 */
static
const char *SimpleColumnReaderFullSpec ( const SimpleColumnReader *self, const ctx_t *ctx )
{
    return self -> full_spec;
}


/* IdRange
 *  returns the number of ids available
 *  and optionally the first id
 */
static
uint64_t SimpleColumnReaderIdRange ( const SimpleColumnReader *self,
    const ctx_t *ctx, int64_t *opt_first )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    uint64_t count;

    rc = VCursorIdRange ( self -> curs, self -> idx, opt_first, & count );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VCursorIdRange failed for column '%s'", self -> full_spec );

    return count;
}


/* DummyStub
 *  useful for pre/post-copy
 */
static
void SimpleColumnReaderDummyStub ( SimpleColumnReader *self, const ctx_t *ctx )
{
}


/* Read
 *  read next row of data
 *  returns NULL if no rows are available
 */
static
const void *SimpleColumnReaderRead ( SimpleColumnReader *self, const ctx_t *ctx,
    int64_t row_id, uint32_t *elem_bits, uint32_t *boff, uint32_t *row_len )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const void *base;

    rc = VCursorCellDataDirect ( self -> curs, row_id, self -> idx, elem_bits, & base, boff, row_len );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VCursorCellDataDirect ( %ld ) failed for column '%s'", row_id, self -> full_spec );

    return base;
}

static ColumnReader_vt SimpleColumnReader_vt =
{
    SimpleColumnReaderWhack,
    SimpleColumnReaderFullSpec,
    SimpleColumnReaderIdRange,
    SimpleColumnReaderDummyStub,
    SimpleColumnReaderDummyStub,
    SimpleColumnReaderRead
};


/*--------------------------------------------------------------------------
 * ColumnReader
 *  interface to read column in serial order
 */


/* MakeColumnReader
 *  make simple column reader
 */
ColumnReader *TablePairMakeColumnReader ( TablePair *self, const ctx_t *ctx,
    const VCursor *opt_curs, const char *colspec, bool required )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const VCursor *curs;

    if ( opt_curs != NULL )
        rc = VCursorAddRef ( curs = opt_curs );
    else
        rc = VTableCreateCursorRead ( self -> stbl, & curs );
    if ( rc != 0 )
        ERROR ( rc, "failed to create cursor on column 'src.%s.%s'", self -> full_spec, colspec );
    else
    {
        uint32_t idx;
        rc = VCursorAddColumn ( curs, & idx, "%s", colspec );
        if ( rc != 0 && GetRCState ( rc ) != rcExists )
        {
            if ( required )
                ERROR ( rc, "failed to add column 'src.%s.%s' to cursor", self -> full_spec, colspec );
        }
        else
        {
            rc = VCursorOpen ( curs );
            if ( rc != 0 )
            {
                if ( required )
                    ERROR ( rc, "failed to open cursor on column 'src.%s.%s'", self -> full_spec, colspec );
            }
            else
            {
                SimpleColumnReader *col;
                size_t full_spec_size = self -> full_spec_size + string_size ( colspec ) + sizeof "src.." - 1;

                TRY ( col = MemAlloc ( ctx, sizeof * col + full_spec_size, false ) )
                {
                    ColumnReaderInit ( & col -> dad, ctx, & SimpleColumnReader_vt );
                    col -> curs = curs;
                    col -> idx = idx;
                    col -> full_spec_size = ( uint32_t ) full_spec_size;

                    rc = string_printf ( col -> full_spec, full_spec_size + 1, NULL,
                        "src.%s.%s", self -> full_spec, colspec );
                    if ( rc == 0 )
                        return & col -> dad;

                    ABORT ( rc, "miscalculated string size" );
                }
                CATCH_ALL ()
                {
                    ANNOTATE ( "failed to allocate %zu bytes for SimpleColumnReader", sizeof * col + full_spec_size );
                }
            }
        }

        VCursorRelease ( curs );
    }

    return NULL;
}


/* Release
 *  releases reference
 */
void ColumnReaderRelease ( const ColumnReader *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "ColumnReader" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release ColumnReader" );
        }
    }
}

/* Duplicate
 */
ColumnReader *ColumnReaderDuplicate ( const ColumnReader *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "ColumnReader" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate ColumnReader" );
            return NULL;
        }
    }

    return ( ColumnReader* ) self;
}


/* Init
 */
void ColumnReaderInit ( ColumnReader *self, const ctx_t *ctx, const ColumnReader_vt *vt )
{
    if ( self == NULL )
    {
        rc_t rc = RC ( rcExe, rcCursor, rcConstructing, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad ColumnReader" );
    }
    else
    {
        self -> vt = vt;
        KRefcountInit ( & self -> refcount, 1, "ColumnReader", "init", "" );
        self -> presorted = false;
        memset ( self -> align, 0, sizeof self -> align );
    }
}


/*--------------------------------------------------------------------------
 * SimpleColumnWriter
 *  implementation of ColumnWriter based upon VCursor
 */
struct SimpleColumnWriter
{
    ColumnWriter dad;

    VCursor *curs;
    uint32_t idx;

    uint32_t full_spec_size;
    char full_spec [ 1 ];
};

static
void SimpleColumnWriterWhack ( SimpleColumnWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    rc = VCursorRelease ( self -> curs );
    if ( rc != 0 )
        ERROR ( rc, "VCursorRelease failed on column '%s'", self -> full_spec );

    MemFree ( ctx, self, sizeof * self + self -> full_spec_size );
}


static
const char *SimpleColumnWriterFullSpec ( const SimpleColumnWriter *self, const ctx_t *ctx )
{
    return self -> full_spec;
}

static
void SimpleColumnWriterDummyStub ( SimpleColumnWriter *self, const ctx_t *ctx )
{
}

static
void SimpleColumnWriterWrite ( SimpleColumnWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    rc = VCursorOpenRow ( self -> curs );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VCursorOpenRow failed on column '%s'", self -> full_spec );
    else
    {
        rc = VCursorWrite ( self -> curs, self -> idx, elem_bits, data, boff, row_len );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "VCursorWrite failed for column '%s'", self -> full_spec );
        else
        {
            rc = VCursorCommitRow ( self -> curs );
            if ( rc != 0 )
                SYSTEM_ERROR ( rc, "VCursorCommit failed for column '%s'", self -> full_spec );
        }

        rc = VCursorCloseRow ( self -> curs );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VCursorCloseRow failed on column '%s'", self -> full_spec );
    }
}

static
void SimpleColumnWriterWriteStatic ( SimpleColumnWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len, uint64_t count )
{
    FUNC_ENTRY ( ctx );

    for ( ; ! FAILED () && count > 0; -- count )
    {
        rc_t rc = VCursorOpenRow ( self -> curs );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VCursorOpenRow failed on column '%s'", self -> full_spec );
        else
        {
            rc = VCursorWrite ( self -> curs, self -> idx, elem_bits, data, boff, row_len );
            if ( rc != 0 )
                SYSTEM_ERROR ( rc, "VCursorWrite failed for column '%s'", self -> full_spec );
            else
            {
                rc = VCursorCommitRow ( self -> curs );
                if ( rc != 0 )
                    SYSTEM_ERROR ( rc, "VCursorCommitRow failed for column '%s'", self -> full_spec );
                else if ( count > 1 )
                {
                    /* setting it small since threshold is detected in CommitRow, but execused on CloseRow */
                    uint64_t cnt = ( count < 0x20000000U ) ? count : 0x20000000U; 
                    rc = VCursorRepeatRow ( self -> curs, cnt - 1 );
                    if ( rc != 0 )
                        SYSTEM_ERROR ( rc, "VCursorRepeatRow failed for column '%s'", self -> full_spec );
                    else
                        count -= cnt - 1;
                }
            }

            rc = VCursorCloseRow ( self -> curs );
            if ( rc != 0 )
                INTERNAL_ERROR ( rc, "VCursorCloseRow failed on column '%s'", self -> full_spec );
        }
    }
}

static
void SimpleColumnWriterCommit ( SimpleColumnWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    rc = VCursorCommit ( self -> curs );
    if ( rc != 0 )
        ERROR ( rc, "VCursorCommit failed on column '%s'", self -> full_spec );
    else
    {
        rc = VCursorRelease ( self -> curs );
        if ( rc != 0 )
            ERROR ( rc, "VCursorRelease failed on column '%s'", self -> full_spec );
        else
        {
            self -> curs = NULL;
        }
    }
}


static ColumnWriter_vt SimpleColumnWriter_vt =
{
    SimpleColumnWriterWhack,
    SimpleColumnWriterFullSpec,
    SimpleColumnWriterDummyStub,
    SimpleColumnWriterDummyStub,
    SimpleColumnWriterWrite,
    SimpleColumnWriterWriteStatic,
    SimpleColumnWriterCommit
};



/*--------------------------------------------------------------------------
 * ColumnWriter
 *  interface to write column in serial order
 */


/* MakeColumnWriter
 *  make simple column writer
 */
ColumnWriter *TablePairMakeColumnWriter ( TablePair *self, const ctx_t *ctx,
   VCursor *opt_curs, const char *colspec )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    VCursor *curs;

    if ( opt_curs != NULL )
        rc = VCursorAddRef ( curs = opt_curs );
    else
        rc = VTableCreateCursorWrite ( self -> dtbl, & curs, kcmInsert );
    if ( rc != 0 )
        ERROR ( rc, "failed to create cursor on column 'dst.%s.%s'", self -> full_spec, colspec );
    else
    {
        uint32_t idx;
        rc = VCursorAddColumn ( curs, & idx, "%s", colspec );
        if ( rc != 0 )
            ERROR ( rc, "failed to add column 'dst.%s.%s' to cursor", self -> full_spec, colspec );
        else
        {
	    VCursorSuspendTriggers ( curs );
            rc = VCursorOpen ( curs );
            if ( rc != 0 )
                ERROR ( rc, "failed to open cursor on column 'dst.%s.%s'", self -> full_spec, colspec );
            {
                SimpleColumnWriter *col;
                size_t full_spec_size = self -> full_spec_size + string_size ( colspec ) + sizeof "dst.." - 1;

                TRY ( col = MemAlloc ( ctx, sizeof * col + full_spec_size, false ) )
                {
                    ColumnWriterInit ( & col -> dad, ctx, & SimpleColumnWriter_vt, false );
                    col -> curs = curs;
                    col -> idx = idx;

                    col -> full_spec_size = ( uint32_t ) full_spec_size;
                    rc = string_printf ( col -> full_spec, full_spec_size + 1, NULL,
                        "dst.%s.%s", self -> full_spec, colspec );
                    if ( rc == 0 )
                        return & col -> dad;

                    ABORT ( rc, "miscalculated string size" );
                }
                CATCH_ALL ()
                {
                    ANNOTATE ( "failed to allocate %zu bytes for SimpleColumnWriter", sizeof * col + full_spec_size );
                }
            }
        }

        VCursorRelease ( curs );
    }

    return NULL;
}


/* Release
 *  releases reference
 */
void ColumnWriterRelease ( ColumnWriter *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "ColumnWriter" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release ColumnWriter" );
        }
    }
}

/* Duplicate
 */
ColumnWriter *ColumnWriterDuplicate ( const ColumnWriter *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "ColumnWriter" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcCursor, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate ColumnWriter" );
            return NULL;
        }
    }

    return ( ColumnWriter* ) self;
}


/* Init
 */
void ColumnWriterInit ( ColumnWriter *self, const ctx_t *ctx, const ColumnWriter_vt *vt, bool mapped )
{
    if ( self == NULL )
    {
        rc_t rc = RC ( rcExe, rcCursor, rcConstructing, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad ColumnWriter" );
    }
    else
    {
        self -> vt = vt;
        KRefcountInit ( & self -> refcount, 1, "ColumnWriter", "init", "" );
        self -> mapped = mapped;
        memset ( self -> align, 0, sizeof self -> align );
    }
}



/*--------------------------------------------------------------------------
 * ColumnPair
 *  interface to pairing of source and destination columns
 */

/* Whack
 */
static
void ColumnPairWhack ( ColumnPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnReaderRelease ( self -> reader, ctx );
    ColumnWriterRelease ( self -> writer, ctx );
    MemFree ( ctx, self, sizeof * self + self -> full_spec_size );
}

/* MakeColumnPair
 *  make a simple ColumnPair
 *  takes an optional source VCursor
 *  and a colspec
 */
ColumnPair *TablePairMakeColumnPair ( TablePair *self, const ctx_t *ctx,
    ColumnReader *reader, ColumnWriter *writer, const char *colspec, bool large )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col;
    size_t full_spec_size = self -> full_spec_size + string_size ( colspec ) + 1;

    TRY ( col = MemAlloc ( ctx, sizeof * col + full_spec_size, false ) )
    {
        TRY ( col -> reader = ColumnReaderDuplicate ( reader, ctx ) )
        {
            TRY ( col -> writer = ColumnWriterDuplicate ( writer, ctx ) )
            {
                rc_t rc;

                col -> full_spec_size = full_spec_size;
                KRefcountInit ( & col -> refcount, 1, "ColumnPair", "make", colspec );
                col -> is_static = false;
                col -> is_mapped = writer -> mapped;
                col -> presorted = reader -> presorted;
                col -> large = large;

                rc = string_printf ( col -> full_spec, full_spec_size + 1, NULL,
                    "%s.%s", self -> full_spec, colspec );
                if ( rc == 0 )
                {
                    col -> colspec = & col -> full_spec [ self -> full_spec_size + 1 ];
                    return col;
                }

                ABORT ( rc, "miscalculated string size" );
            }

            ColumnReaderRelease ( col -> reader, ctx );
        }

        MemFree ( ctx, col, sizeof * col + full_spec_size );
    }

    return NULL;
}

ColumnPair *TablePairMakeStaticColumnPair ( TablePair *self, const ctx_t *ctx,
    ColumnReader *reader, ColumnWriter *writer, const char *colspec )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col;

    TRY ( col = TablePairMakeColumnPair ( self, ctx, reader, writer, colspec, false ) )
    {
        if ( col != NULL )
            col -> is_static = true;
    }

    return col;
}


/* Release
 *  called by table at end of copy
 */
void ColumnPairRelease ( ColumnPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "ColumnPair" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ColumnPairWhack ( self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcColumn, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release ColumnPair" );
        }
    }
}

/* Duplicate
 */
ColumnPair *ColumnPairDuplicate ( const ColumnPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "ColumnPair" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcColumn, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate ColumnPair" );
            return NULL;
        }
    }

    return ( ColumnPair* ) self;
}


/* PreCopy
 * PostCopy
 */
void ColumnPairPreCopy ( const ColumnPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnReaderPreCopy ( self -> reader, ctx );
    ColumnWriterPreCopy ( self -> writer, ctx );
}

void ColumnPairPostCopy ( const ColumnPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnReaderPostCopy ( self -> reader, ctx );
    ColumnWriterPostCopy ( self -> writer, ctx );
}


/* Copy
 *  copy from source to destination column
 */
void ColumnPairCopy ( ColumnPair *self, const ctx_t *ctx, RowSet *rs )
{
    FUNC_ENTRY ( ctx );

    STATUS ( 3, "copying column '%s'", self -> full_spec );

    TRY ( RowSetReset ( rs, ctx, self -> is_static ) )
    {
        TRY ( ColumnPairPreCopy ( self, ctx ) )
        {
            while ( ! FAILED () )
            {
                rc_t rc;
                size_t i, count;
                int64_t row_ids [ 8 * 1024 ];

                ON_FAIL ( count = RowSetNext ( rs, ctx, row_ids, sizeof row_ids / sizeof row_ids [ 0 ] ) )
                    break;
                if ( count == 0 )
                    break;

                rc = Quitting ();
                if ( rc != 0 )
                {
                    INFO_ERROR ( rc, "quitting" );
                    break;
                }

                for ( i = 0; ! FAILED () && i < count; ++ i )
                {
                    const void *base;
                    uint32_t elem_bits, boff, row_len;
                    
                    TRY ( base = ColumnReaderRead ( self -> reader, ctx, row_ids [ i ], & elem_bits, & boff, & row_len ) )
                    {
                        ColumnWriterWrite ( self -> writer, ctx, elem_bits, base, boff, row_len );
                    }
                }
            }

            ColumnPairPostCopy ( self, ctx );
        }
    }
}


/* CopyStatic
 *  copy static column from source to destination
 */
void ColumnPairCopyStatic ( ColumnPair *self, const ctx_t *ctx, int64_t first_id, uint64_t count )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const void *base;
    uint32_t elem_bits, boff, row_len;

    if ( ! self -> is_static )
    {
        rc = RC ( rcExe, rcColumn, rcCopying, rcType, rcIncorrect );
        INTERNAL_ERROR ( rc, "'%s' IS NOT A STATIC COLUMN", self -> full_spec );
        return;
    }

    STATUS ( 3, "copying static column '%s'", self -> full_spec );
                    
    TRY ( base = ColumnReaderRead ( self -> reader, ctx, first_id, & elem_bits, & boff, & row_len ) )
    {
        ColumnWriterWriteStatic ( self -> writer, ctx, elem_bits, base, boff, row_len, count );
    }
}
