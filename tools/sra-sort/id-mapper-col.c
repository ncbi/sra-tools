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

typedef struct MapRowIdColWriter MapRowIdColWriter;
#define COLWRITER_IMPL MapRowIdColWriter

#include "id-mapper-col.h"
#include "map-file.h"
#include "csra-tbl.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <klib/rc.h>

#include <string.h>

FILE_ENTRY ( id-mapper-col );


/*--------------------------------------------------------------------------
 * MapRowIdColWriter
 */

struct MapRowIdColWriter
{
    ColumnWriter dad;

    ColumnWriter *cw;

    MapFile *idx;

    int64_t *row;
    uint32_t max_row_len;

    bool assign_ids;
};

/* called by Release */
static
void MapRowIdColWriterDestroy ( MapRowIdColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnWriterRelease ( self -> cw, ctx );
    self -> cw = NULL;

    MapFileRelease ( self -> idx, ctx );
    self -> idx = NULL;

    if ( self -> row != NULL )
    {
        MemFree ( ctx, self -> row, sizeof * self -> row * self -> max_row_len );
        self -> row = NULL;
        self -> max_row_len = 0;
    }
}

static
void MapRowIdColWriterWhack ( MapRowIdColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    MapRowIdColWriterDestroy ( self, ctx );
    ColumnWriterDestroy ( & self -> dad, ctx );
    MemFree ( ctx, self, sizeof * self );
}

/* full spec */
static
const char* MapRowIdColWriterFullSpec ( const MapRowIdColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    return ColumnWriterFullSpec ( self -> cw, ctx );
}

static
void MapRowIdColWriterPreCopy ( MapRowIdColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnWriterPreCopy ( self -> cw, ctx );
}

static
void MapRowIdColWriterPostCopy ( MapRowIdColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> max_row_len > 16 )
    {
        void *row;
        TRY ( row = MemAlloc ( ctx, sizeof * self -> row * 16, false ) )
        {
            MemFree ( ctx, self -> row, sizeof * self -> row * self -> max_row_len );
            self -> row = row;
            self -> max_row_len = 16;
        }
        CATCH_ALL ()
        {
            CLEAR ();
        }
    }

    ColumnWriterPostCopy ( self -> cw, ctx );
}

/* write row to destination */
static
void MapRowIdColWriterWrite ( MapRowIdColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len )
{
    FUNC_ENTRY ( ctx );

    uint32_t i;

    /* here is where the VALUE coming in, which is a row-id,
       gets mapped to its new value before being written out */
    assert ( elem_bits == sizeof * self -> row * 8 );
    assert ( boff == 0 );

    /* resize row if needed */
    if ( self -> max_row_len < row_len )
    {
        MemFree ( ctx, self -> row, sizeof * self -> row * self -> max_row_len );

        self -> max_row_len = ( row_len + 15 ) & ~ 15U;
        ON_FAIL ( self -> row = MemAlloc ( ctx, sizeof * self -> row * self -> max_row_len, false ) )
        {
            ANNOTATE ( "failed to allocate memory for %u row-ids", self -> max_row_len );
            return;
        }
    }

    /* copy row */
    memcpy ( self -> row, data, row_len * sizeof * self -> row );

    /* map ids */
    for ( i = 0; i < row_len; ++ i )
    {
        if ( self -> row [ i ] != 0 )
        {
            int64_t new_id;

            /* map old row-id to new row-id */
            ON_FAIL ( new_id = MapFileMapSingleOldToNew ( self -> idx, ctx, self -> row [ i ], self -> assign_ids ) )
            {
                ANNOTATE ( "failed to map source row-id %ld", self -> row [ i ] );
                return;
            }
            if ( new_id == 0 )
            {
                rc_t rc = RC ( rcExe, rcColumn, rcWriting, rcId, rcNotFound );
                ERROR ( rc, "failed to map source row-id %ld", self -> row [ i ] );
                return;
            }

            self -> row [ i ] = new_id;
        }
    }

    /* now write the translated ids */
    ColumnWriterWrite ( self -> cw, ctx, sizeof * self -> row * 8, self -> row, 0, row_len );
}

static
void MapRowIdColWriterWriteStatic ( MapRowIdColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len, uint64_t count )
{
    FUNC_ENTRY ( ctx );
    rc_t rc = RC ( rcExe, rcColumn, rcWriting, rcType, rcIncorrect );
    INTERNAL_ERROR ( rc, "writing to a non-static column" );
}

/* commit all writes */
static
void MapRowIdColWriterCommit ( MapRowIdColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnWriterCommit ( self -> cw, ctx );
    MapRowIdColWriterDestroy ( self, ctx );
}

static ColumnWriter_vt MapRowIdColWriter_vt =
{
    MapRowIdColWriterWhack,
    MapRowIdColWriterFullSpec,
    MapRowIdColWriterPreCopy,
    MapRowIdColWriterPostCopy,
    MapRowIdColWriterWrite,
    MapRowIdColWriterWriteStatic,
    MapRowIdColWriterCommit
};


/* MakeMapRowIdWriter
 *  if "assign_ids" is true, the MapFile will be asked to
 *  map an old=>new id or assign (and record) a new id.
 */
ColumnWriter *TablePairMakeMapRowIdWriter ( TablePair *self,
    const ctx_t *ctx, ColumnWriter *cw, MapFile *idx, bool assign_ids )
{
    FUNC_ENTRY ( ctx );

    MapRowIdColWriter *writer;

    TRY ( writer = MemAlloc ( ctx, sizeof * writer, false ) )
    {
        TRY ( ColumnWriterInit ( & writer -> dad, ctx, & MapRowIdColWriter_vt, true ) )
        {
            writer -> max_row_len = 16;
            TRY ( writer -> row = MemAlloc ( ctx, sizeof * writer -> row * writer -> max_row_len, false ) )
            {
                TRY ( writer -> idx = MapFileDuplicate ( idx, ctx ) )
                {
                    TRY ( writer -> cw = ColumnWriterDuplicate ( cw, ctx ) )
                    {
                        writer -> assign_ids = assign_ids;
                        return & writer -> dad;
                    }

                    MapFileRelease ( writer -> idx, ctx );
                }

                ColumnWriterDestroy ( & writer -> dad, ctx );
            }

            MemFree ( ctx, writer -> row, sizeof * writer -> row * writer -> max_row_len );
        }

        MemFree ( ctx, writer, sizeof * writer );
    }

    return NULL;
}
