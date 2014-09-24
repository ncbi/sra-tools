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

typedef struct PoslenColReader PoslenColReader;
#define COLREADER_IMPL PoslenColReader

typedef struct PoslenColWriter PoslenColWriter;
#define COLWRITER_IMPL PoslenColWriter

#include "col-pair.h"
#include "tbl-pair.h"
#include "map-file.h"
#include "glob-poslen.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <kapp/main.h>
#include <kfs/defs.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>

FILE_ENTRY ( poslen-col-pair );


/*--------------------------------------------------------------------------
 * PoslenColReader
 *  implementation of ColumnReader based upon MapFile temp column read
 */
struct PoslenColReader
{
    ColumnReader dad;

    /* cached first id in column */
    int64_t first_id;

    /* buffer for reading poslen */
    int64_t start_id, excl_stop_id;
    uint64_t *buff;
    size_t max_ids;

    /* source of poslen */
    const MapFile *tmp_col;

    size_t full_spec_size;
    char full_spec [ 1 ];
};


/* Whack
 */
static
void PoslenColReaderWhack ( PoslenColReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* self-bytes */
    size_t bytes = sizeof * self + self -> full_spec_size;

    /* free buffer */
    if ( self -> buff != NULL )
    {
        ON_FAIL ( MemFree ( ctx, self -> buff, self -> max_ids * sizeof * self -> buff ) )
        {
            CLEAR ();
        }
        self -> buff = NULL;
    }

    /* release MapFile */
    ON_FAIL ( MapFileRelease ( self -> tmp_col, ctx ) )
    {
        WARN ( "MapFileRelease failed on '%s'", self -> full_spec );
        CLEAR ();
    }
    self -> tmp_col = NULL;

    /* free self */
    MemFree ( ctx, self, bytes );
}

/* FullSpec
 */
static
const char *PoslenColReaderFullSpec ( const PoslenColReader *self, const ctx_t *ctx )
{
    return self -> full_spec;
}


/* IdRange
 *  returns the number of ids available
 *  and optionally the first id
 */
static
uint64_t PoslenColReaderIdRange ( const PoslenColReader *self,
    const ctx_t *ctx, int64_t *opt_first )
{
    FUNC_ENTRY ( ctx );

    if ( opt_first != NULL )
    {
        ON_FAIL ( * opt_first = self -> first_id )
            return 0;
    }

    return MapFileCount ( self -> tmp_col, ctx );
}


/* PreCopy
 *  create buffer
 */
static
void PoslenColReaderPreCopy ( PoslenColReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    const Tool *tp = ctx -> caps -> tool;

    self -> max_ids = tp -> max_poslen_ids;
    if ( tp -> max_poslen_ids < 4096 )
        self -> max_ids = 4096;

    TRY ( self -> buff = MemAlloc ( ctx, self -> max_ids * sizeof * self -> buff, false ) )
    {
        self -> start_id = self -> excl_stop_id = self -> first_id;
    }
    CATCH_ALL ()
    {
        ANNOTATE ( "failed to allocate buffer for PoslenColReader" );
    }
}


/* PostCopy
 *  douse buffer
 */
static
void PoslenColReaderPostCopy ( PoslenColReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> buff != NULL )
    {
        TRY ( MemFree ( ctx, self -> buff, self -> max_ids * sizeof * self -> buff ) )
        {
            self -> buff = NULL;
        }
    }
}


/* Read
 *  read next row of data
 *  returns NULL if no rows are available
 */
static
const void *PoslenColReaderRead ( PoslenColReader *self, const ctx_t *ctx,
    int64_t row_id, uint32_t *elem_bits, uint32_t *boff, uint32_t *row_len )
{
    FUNC_ENTRY ( ctx );

    uint32_t dummy;

    if ( elem_bits == NULL )
        elem_bits = & dummy;
    if ( boff == NULL )
        boff = & dummy;
    if ( row_len == NULL )
        row_len = & dummy;

    * elem_bits = 64;
    * boff = 0;
    * row_len = 0;

    assert ( self -> buff != NULL );

    if ( row_id < self -> start_id || row_id >= self -> excl_stop_id )
    {
        size_t num_read;

        if ( row_id < self -> first_id )
            return NULL;

        /* generate page-aligned starting id */
        self -> start_id = ( row_id - self -> first_id );
        self -> start_id -= self -> start_id % self -> max_ids;
        self -> start_id += self -> first_id;
        assert ( self -> start_id + self -> max_ids > row_id );

        /* read into buffer */
        ON_FAIL ( num_read = MapFileReadPoslen ( self -> tmp_col, ctx, self -> start_id, self -> buff, self -> max_ids ) )
        {
            self -> start_id = self -> excl_stop_id = self -> first_id;
            return NULL;
        }
        self -> excl_stop_id = self -> start_id + num_read;

        if ( row_id >= self -> excl_stop_id )
            return NULL;
    }

    * row_len = 1;
    return & self -> buff [ row_id - self -> start_id ];
}

static ColumnReader_vt PoslenColReader_vt =
{
    PoslenColReaderWhack,
    PoslenColReaderFullSpec,
    PoslenColReaderIdRange,
    PoslenColReaderPreCopy,
    PoslenColReaderPostCopy,
    PoslenColReaderRead
};


/*--------------------------------------------------------------------------
 * TablePair
 */


/* MakePoslenColReader
 *  make temporary column reader
 */
ColumnReader *TablePairMakePoslenColReader ( TablePair *self, const ctx_t *ctx,
    const MapFile *tmp_col, const char *colspec )
{
    FUNC_ENTRY ( ctx );

    PoslenColReader *col;
    size_t full_spec_size = self -> full_spec_size + string_size ( colspec ) + sizeof "src.." - 1;

    TRY ( col = MemAlloc ( ctx, sizeof * col + full_spec_size, false ) )
    {
        TRY ( ColumnReaderInit ( & col -> dad, ctx, & PoslenColReader_vt ) )
        {
            TRY ( col -> first_id = MapFileFirst ( tmp_col, ctx ) )
            {
                TRY ( col -> tmp_col = MapFileDuplicate ( tmp_col, ctx ) )
                {
                    rc_t rc;

                    col -> dad . presorted = true;
                    col -> start_id = col -> excl_stop_id = col -> first_id;
                    col -> buff = NULL;
                    col -> max_ids = ctx -> caps -> tool -> max_poslen_ids;
                    col -> full_spec_size = ( uint32_t ) full_spec_size;

                    rc = string_printf ( col -> full_spec, full_spec_size + 1, NULL,
                        "src.%s.%s", self -> full_spec, colspec );
                    if ( rc == 0 )
                        return & col -> dad;

                    ABORT ( rc, "miscalculated string size" );
                }
            }
        }

        MemFree ( ctx, col, sizeof * col + full_spec_size );
    }
    CATCH_ALL ()
    {
        ANNOTATE ( "failed to allocate %zu bytes for PoslenColReader", sizeof * col + full_spec_size );
    }

    return NULL;

}


/*--------------------------------------------------------------------------
 * PoslenColWriter
 *  implementation of ColumnWriter based upon VCursor
 */
struct PoslenColWriter
{
    ColumnWriter dad;

    VCursor *curs;
    uint32_t global_ref_start;
    uint32_t ref_len;

    uint32_t full_spec_size;
    char full_spec [ 1 ];
};

static
void PoslenColWriterWhack ( PoslenColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    rc = VCursorRelease ( self -> curs );
    if ( rc != 0 )
        ERROR ( rc, "VCursorRelease failed on column '%s'", self -> full_spec );

    MemFree ( ctx, self, sizeof * self + self -> full_spec_size );
}


static
const char *PoslenColWriterFullSpec ( const PoslenColWriter *self, const ctx_t *ctx )
{
    return self -> full_spec;
}

static
void PoslenColWriterDummyStub ( PoslenColWriter *self, const ctx_t *ctx )
{
}

static
void PoslenColWriterWrite ( PoslenColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    uint64_t poslen;

    uint32_t ref_len;
    uint64_t global_ref_start;

    assert ( elem_bits == 64 );
    assert ( boff == 0 );
    assert ( row_len == 1 );

    memcpy ( & poslen, data, sizeof poslen );
    global_ref_start = decode_pos_len ( poslen );
    ref_len = poslen_to_len ( poslen );

    rc = VCursorOpenRow ( self -> curs );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VCursorOpenRow failed on column '%s'", self -> full_spec );
    else
    {
        rc = VCursorWrite ( self -> curs, self -> global_ref_start, sizeof global_ref_start * 8, & global_ref_start, 0, 1 );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "VCursorWrite failed for GLOBAL_REF_START of column '%s'", self -> full_spec );
        else
        {
            rc = VCursorWrite ( self -> curs, self -> ref_len, sizeof ref_len * 8, & ref_len, 0, 1 );
            if ( rc != 0 )
                SYSTEM_ERROR ( rc, "VCursorWrite failed for REF_LEN of column '%s'", self -> full_spec );
            else
            {
                rc = VCursorCommitRow ( self -> curs );
                if ( rc != 0 )
                    SYSTEM_ERROR ( rc, "VCursorCommit failed for column '%s'", self -> full_spec );
            }
        }

        rc = VCursorCloseRow ( self -> curs );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VCursorCloseRow failed on column '%s'", self -> full_spec );
    }
}

static
void PoslenColWriterWriteStatic ( PoslenColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len, uint64_t count )
{
    FUNC_ENTRY ( ctx );

    uint64_t poslen;

    uint32_t ref_len;
    uint64_t global_ref_start;

    assert ( elem_bits == 64 );
    assert ( boff == 0 );
    assert ( row_len == 1 );

    memcpy ( & poslen, data, sizeof poslen );
    global_ref_start = decode_pos_len ( poslen );
    ref_len = poslen_to_len ( poslen );

    for ( ; ! FAILED () && count > 0; -- count )
    {
        rc_t rc = VCursorOpenRow ( self -> curs );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VCursorOpenRow failed on column '%s'", self -> full_spec );
        else
        {
            rc = VCursorWrite ( self -> curs, self -> global_ref_start, sizeof global_ref_start * 8, & global_ref_start, 0, 1 );
            if ( rc != 0 )
                SYSTEM_ERROR ( rc, "VCursorWrite failed for GLOBAL_REF_START of column '%s'", self -> full_spec );
            else
            {
                rc = VCursorWrite ( self -> curs, self -> ref_len, sizeof ref_len * 8, & ref_len, 0, 1 );
                if ( rc != 0 )
                    SYSTEM_ERROR ( rc, "VCursorWrite failed for REF_LEN of column '%s'", self -> full_spec );
                else
                {
                    rc = VCursorCommitRow ( self -> curs );
                    if ( rc != 0 )
                        SYSTEM_ERROR ( rc, "VCursorCommit failed for column '%s'", self -> full_spec );
                    else if ( count > 1 )
                    {
                        uint64_t cnt = (count < 0x20000000U ) ? count : 0x20000000U;
                        rc = VCursorRepeatRow ( self -> curs, cnt - 1 );
                        if ( rc != 0 )
                            SYSTEM_ERROR ( rc, "VCursorRepeatRow failed for column '%s'", self -> full_spec );
                        count -= cnt - 1;
                    }
                }
            }

            rc = VCursorCloseRow ( self -> curs );
            if ( rc != 0 )
                INTERNAL_ERROR ( rc, "VCursorCloseRow failed on column '%s'", self -> full_spec );
        }
    }
}

static
void PoslenColWriterCommit ( PoslenColWriter *self, const ctx_t *ctx )
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


static ColumnWriter_vt PoslenColWriter_vt =
{
    PoslenColWriterWhack,
    PoslenColWriterFullSpec,
    PoslenColWriterDummyStub,
    PoslenColWriterDummyStub,
    PoslenColWriterWrite,
    PoslenColWriterWriteStatic,
    PoslenColWriterCommit
};



/*--------------------------------------------------------------------------
 * TablePair
 */


/* MakePoslenColWriter
 */
ColumnWriter *TablePairMakePoslenColWriter ( TablePair *self, const ctx_t *ctx,
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
        uint32_t global_ref_start;
        const char *subcolspec = "(U64)GLOBAL_REF_START";
        rc = VCursorAddColumn ( curs, & global_ref_start, subcolspec );
        if ( rc != 0 )
            ERROR ( rc, "failed to add column 'dst.%s.%s' to cursor", self -> full_spec, subcolspec );
        else
        {
            uint32_t ref_len;
            subcolspec = "(INSDC:coord:len)REF_LEN";
            rc = VCursorAddColumn ( curs, & ref_len, subcolspec );
            if ( rc != 0 )
                ERROR ( rc, "failed to add column 'dst.%s.%s' to cursor", self -> full_spec, subcolspec );
            else
            {
                VCursorSuspendTriggers ( curs );
                rc = VCursorOpen ( curs );
                if ( rc != 0 )
                    ERROR ( rc, "failed to open cursor on columns for 'dst.%s.%s'", self -> full_spec, colspec );
                else
                {
                    PoslenColWriter *col;
                    size_t full_spec_size = self -> full_spec_size + string_size ( colspec ) + sizeof "dst.." - 1;

                    TRY ( col = MemAlloc ( ctx, sizeof * col + full_spec_size, false ) )
                    {
                        ColumnWriterInit ( & col -> dad, ctx, & PoslenColWriter_vt, false );
                        col -> curs = curs;
                        col -> global_ref_start = global_ref_start;
                        col -> ref_len = ref_len;

                        col -> full_spec_size = ( uint32_t ) full_spec_size;
                        rc = string_printf ( col -> full_spec, full_spec_size + 1, NULL,
                            "dst.%s.%s", self -> full_spec, colspec );
                        if ( rc == 0 )
                            return & col -> dad;

                        ABORT ( rc, "miscalculated string size" );
                    }
                    CATCH_ALL ()
                    {
                        ANNOTATE ( "failed to allocate %zu bytes for PoslenColWriter", sizeof * col + full_spec_size );
                    }
                }
            }
        }

        VCursorRelease ( curs );
    }

    return NULL;
}
