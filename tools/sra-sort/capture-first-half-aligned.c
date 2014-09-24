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

typedef struct CaptureFirstHalfAlignedColWriter CaptureFirstHalfAlignedColWriter;
#define COLWRITER_IMPL CaptureFirstHalfAlignedColWriter

#include "csra-pair.h"
#include "col-pair.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <klib/rc.h>

FILE_ENTRY ( capture-first-half-aligned );


/*--------------------------------------------------------------------------
 * CaptureFirstHalfAlignedColWriter
 */
struct CaptureFirstHalfAlignedColWriter
{
    ColumnWriter dad;

    int64_t row_id;
    cSRAPair *csra;
    ColumnWriter *writer;
};

static
void CaptureFirstHalfAlignedColWriterWhack ( CaptureFirstHalfAlignedColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnWriterRelease ( self -> writer, ctx );
    self -> csra = NULL;
    self -> writer = NULL;
    MemFree ( ctx, self, sizeof * self );
}

static
const char *CaptureFirstHalfAlignedColWriterFullSpec ( const CaptureFirstHalfAlignedColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    return ColumnWriterFullSpec ( self -> writer, ctx );
}

static
void CaptureFirstHalfAlignedColWriterPreCopy ( CaptureFirstHalfAlignedColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnWriterPreCopy ( self -> writer, ctx );
}

static
void CaptureFirstHalfAlignedColWriterPostCopy ( CaptureFirstHalfAlignedColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnWriterPostCopy ( self -> writer, ctx );
}

static
void CaptureFirstHalfAlignedColWriterWrite ( CaptureFirstHalfAlignedColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len )
{
    FUNC_ENTRY ( ctx );

    cSRAPair *csra = self -> csra;
    if ( csra -> first_half_aligned_spot == 0 )
    {
        uint32_t i;
        const int64_t *row = data;

        for ( i = 0; i < row_len; ++ i )
        {
            if ( row [ i ] == 0 )
            {
                csra -> first_half_aligned_spot = self -> row_id;
                break;
            }
        }
    }

    TRY ( ColumnWriterWrite ( self -> writer, ctx, elem_bits, data, boff, row_len ) )
    {
        ++ self -> row_id;
    }
}

static
void CaptureFirstHalfAlignedColWriterWriteStatic ( CaptureFirstHalfAlignedColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len, uint64_t count )
{
    FUNC_ENTRY ( ctx );

    cSRAPair *csra = self -> csra;
    if ( csra -> first_half_aligned_spot == 0 )
    {
        uint32_t i;
        const int64_t *row = data;

        for ( i = 0; i < row_len; ++ i )
        {
            if ( row [ i ] == 0 )
            {
                csra -> first_half_aligned_spot = self -> row_id;
                break;
            }
        }
    }

    TRY ( ColumnWriterWriteStatic ( self -> writer, ctx, elem_bits, data, boff, row_len, count ) )
    {
        self -> row_id += count;
    }
}

static
void CaptureFirstHalfAlignedColWriterCommit ( CaptureFirstHalfAlignedColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnWriterCommit ( self -> writer, ctx );
}

static ColumnWriter_vt CaptureFirstHalfAlignedColWriter_vt =
{
    CaptureFirstHalfAlignedColWriterWhack,
    CaptureFirstHalfAlignedColWriterFullSpec,
    CaptureFirstHalfAlignedColWriterPreCopy,
    CaptureFirstHalfAlignedColWriterPostCopy,
    CaptureFirstHalfAlignedColWriterWrite,
    CaptureFirstHalfAlignedColWriterWriteStatic,
    CaptureFirstHalfAlignedColWriterCommit
};

/*--------------------------------------------------------------------------
 * cSRAPair
 */

/* MakeFirstHalfAlignedRowIdCaptureWriter
 *  a simple monitor on SEQUENCE.PRIMARY_ALIGNMENT_ID looking for
 *  half-aligned spots and capturing the first occurrence.
 */
ColumnWriter *cSRAPairMakeFirstHalfAlignedRowIdCaptureWriter ( cSRAPair *self,
    const ctx_t *ctx, ColumnWriter *writer )
{
    FUNC_ENTRY ( ctx );

    CaptureFirstHalfAlignedColWriter *mon;

    TRY ( mon = MemAlloc ( ctx, sizeof * mon, false ) )
    {
        TRY ( ColumnWriterInit ( & mon -> dad, ctx, & CaptureFirstHalfAlignedColWriter_vt, false ) )
        {
            mon -> row_id = 1;
            mon -> csra = self;
            TRY ( mon -> writer = ColumnWriterDuplicate ( writer, ctx ) )
            {
                return & mon -> dad;
            }
        }

        MemFree ( ctx, mon, sizeof * mon );
    }

    return NULL;
}
