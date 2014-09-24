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

struct cSRATblPair;
#define TBLPAIR_IMPL struct cSRATblPair

#include "csra-tbl.h"
#include "csra-pair.h"
#include "ref-alignid-col.h"
#include "row-set.h"
#include "id-mapper-col.h"
#include "buff-writer.h"
#include "poslen-col-pair.h"
#include "meta-pair.h"
#include "map-file.h"
#include "xcheck.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"

#include <vdb/table.h>
#include <kdb/meta.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>


FILE_ENTRY ( csra-tbl );


/*--------------------------------------------------------------------------
 * RefTblPair
 *  generic database object pair
 */

static
void cSRATblPairWhack ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    RowSetIteratorRelease ( self -> rsi, ctx );
    TablePairDestroy ( & self -> dad, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
void cSRATblPairDummyStub ( cSRATblPair *self, const ctx_t *ctx )
{
}


/* REFERENCE table
 */
static
ColumnPair *cSRATblPairMakePrimAlignIdColPair ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col = NULL;
    cSRAPair *csra = self -> csra;

    ColumnReader *reader;
    const char *colspec = "(I64)PRIMARY_ALIGNMENT_IDS";

    TRY ( reader = TablePairMakeAlignIdReader ( & self -> dad, ctx,
              csra -> prim_align, csra -> pa_idx, colspec ) )
    {
        ColumnWriter *writer;
        TRY ( writer = TablePairMakeColumnWriter ( & self -> dad, ctx, NULL, colspec ) )
        {
            col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, writer, colspec, false );
                
            ColumnWriterRelease ( writer, ctx );
        }

        ColumnReaderRelease ( reader, ctx );
    }

    return col;
}

static
ColumnPair *cSRATblPairMakeSecAlignIdColPair ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col = NULL;
    cSRAPair *csra = self -> csra;

    if ( csra -> sec_align != NULL )
    {
        ColumnReader *reader;
        const char *colspec = "(I64)SECONDARY_ALIGNMENT_IDS";

        TRY ( reader = TablePairMakeAlignIdReader ( & self -> dad, ctx,
                  csra -> sec_align, csra -> sa_idx, colspec ) )
        {
            ColumnWriter *writer;
            TRY ( writer = TablePairMakeColumnWriter ( & self -> dad, ctx, NULL, colspec ) )
            {
                col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, writer, colspec, false );
                
                ColumnWriterRelease ( writer, ctx );
            }
            
            ColumnReaderRelease ( reader, ctx );
        }
    }

    return col;
}

static
void cSRATblPairExplodeRef ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col;

    TRY ( col = cSRATblPairMakePrimAlignIdColPair ( self, ctx ) )
    {
        TRY ( TablePairAddColumnPair ( & self -> dad, ctx, col ) )
        {
            TRY ( col = cSRATblPairMakeSecAlignIdColPair ( self, ctx ) )
            {
                TRY ( TablePairAddColumnPair ( & self -> dad, ctx, col ) )
                {
                }
                CATCH_ALL ()
                {
                    ColumnPairRelease ( col, ctx );
                }
            }
        }
        CATCH_ALL ()
        {
            ColumnPairRelease ( col, ctx );
        }
    }
}

static
ColumnPair *cSRATblPairMakeColumnPairRef ( cSRATblPair *self, const ctx_t *ctx,
    struct ColumnReader *reader, struct ColumnWriter *writer, const char *colspec, bool large )
{
    FUNC_ENTRY ( ctx );
    return TablePairMakeColumnPair ( & self -> dad, ctx, reader, writer, colspec, large );
}

static
RowSetIterator *cSRATblPairGetRowSetIteratorRef ( cSRATblPair *self, const ctx_t *ctx, bool mapping, bool large )
{
    FUNC_ENTRY ( ctx );
    const bool is_paired = false;
    assert ( mapping == false );
    return TablePairMakeRowSetIterator ( & self -> dad, ctx, NULL, is_paired, large );
}

static TablePair_vt cSRATblPair_Ref_vt =
{
    cSRATblPairWhack,
    cSRATblPairDummyStub,
    cSRATblPairExplodeRef,
    cSRATblPairMakeColumnPairRef,
    cSRATblPairDummyStub,
    cSRATblPairDummyStub,
    cSRATblPairGetRowSetIteratorRef
};



/* *_ALIGNMENT tables
 */

static
ColumnPair *cSRATblPairMakeSeqSpotIdColPairPrim ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col = NULL;
    cSRAPair *csra = self -> csra;

    ColumnReader *reader;
    const char *colspec = "(I64)SEQ_SPOT_ID";

    /* we expect this column to be present */
    TRY ( reader = TablePairMakeColumnReader ( & self -> dad, ctx, NULL, colspec, true ) )
    {
        ColumnWriter *writer;
        TRY ( writer = TablePairMakeColumnWriter ( & self -> dad, ctx, NULL, colspec ) )
        {
            ColumnWriter *mapped;
            const bool can_assign_ids = true;
            TRY ( mapped = TablePairMakeMapRowIdWriter ( & self -> dad, ctx, writer, csra -> seq_idx, can_assign_ids ) )
            {
                ColumnWriter *buffered;
                TRY ( buffered = cSRATblPairMakeBufferedColumnWriter ( self, ctx, mapped ) )
                {
                    col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, buffered, colspec, false );

                    ColumnWriterRelease ( buffered, ctx );
                }

                ColumnWriterRelease ( mapped, ctx );
            }
                
            ColumnWriterRelease ( writer, ctx );
        }

        ColumnReaderRelease ( reader, ctx );
    }

    return col;
}

static
ColumnPair *cSRATblPairMakeSeqSpotIdColPair ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col = NULL;
    cSRAPair *csra = self -> csra;

    ColumnReader *reader;
    const char *colspec = "(I64)SEQ_SPOT_ID";

    /* we expect this column to be present */
    TRY ( reader = TablePairMakeColumnReader ( & self -> dad, ctx, NULL, colspec, true ) )
    {
        ColumnWriter *writer;
        TRY ( writer = TablePairMakeColumnWriter ( & self -> dad, ctx, NULL, colspec ) )
        {
            ColumnWriter *buffered;
            /* create a buffered writer with NO id-assignment capabilities. */
            TRY ( buffered = cSRATblPairMakeBufferedIdRemapColumnWriter ( self, ctx,
                      writer, csra -> seq_idx, false ) )
            {
                col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, buffered, colspec, false );

                ColumnWriterRelease ( buffered, ctx );
            }
                
            ColumnWriterRelease ( writer, ctx );
        }

        ColumnReaderRelease ( reader, ctx );
    }

    return col;
}

static
ColumnPair *cSRATblPairMakePoslenColPair ( cSRATblPair *self, const ctx_t *ctx, const MapFile *idx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col = NULL;

    ColumnReader *reader;
    const char *colspec = "(U64)GLOBAL_POSLEN";

    /* we expect this column to be present */
    TRY ( reader = TablePairMakePoslenColReader ( & self -> dad, ctx, idx, colspec ) )
    {
        ColumnWriter *writer;
        TRY ( writer = TablePairMakePoslenColWriter ( & self -> dad, ctx, NULL, colspec ) )
        {
            col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, writer, colspec, false );
                
            ColumnWriterRelease ( writer, ctx );
        }

        ColumnReaderRelease ( reader, ctx );
    }

    return col;
}

static
void cSRATblPairPreExplodeAlign ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    cSRAPair *csra = self -> csra;

    switch ( self -> align_idx )
    {
    case 1:
        csra -> pa_idx = MapFileMakeForPoslen ( ctx, csra -> prim_align -> name );
        break;
    case 2:
        csra -> sa_idx = MapFileMakeForPoslen ( ctx, csra -> sec_align -> name );
        break;
    }
}

static
void cSRATblPairExplodeAlign ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col;

    /* create special case for SEQ_SPOT_ID */
    switch ( self -> align_idx )
    {
    case 1:
        col = cSRATblPairMakeSeqSpotIdColPairPrim ( self, ctx );
        break;
    case 0:
    case 2:
    case 3:
        col = cSRATblPairMakeSeqSpotIdColPair ( self, ctx );
        break;
    default:
        ANNOTATE ( "not going to dignify with an rc - bad align_idx" );
        return;
    }

    if ( ! FAILED () )
    {
        TRY ( TablePairAddColumnPair ( & self -> dad, ctx, col ) )
        {
            cSRAPair *csra = self -> csra;

            switch ( self -> align_idx )
            {
            case 1:
                col = cSRATblPairMakePoslenColPair ( self, ctx, csra -> pa_idx );
                break;
            case 2:
                col = cSRATblPairMakePoslenColPair ( self, ctx, csra -> sa_idx );
                break;
            default:
                return;
            }

            if ( ! FAILED () )
            {
                TRY ( TablePairAddColumnPair ( & self -> dad, ctx, col ) )
                {
                    return;
                }
            }
        }

        ColumnPairRelease ( col, ctx );
    }
}

static
ColumnPair *cSRATblPairMakeColumnPairAlign ( cSRATblPair *self, const ctx_t *ctx,
    struct ColumnReader *reader, struct ColumnWriter *writer, const char *colspec, bool large )
{
    FUNC_ENTRY ( ctx );

    MapFile *idx = NULL;
    ColumnPair *col = NULL;
    ColumnWriter *buffered;

    switch ( self -> align_idx )
    {
    case 0:
    case 3:
        return TablePairMakeColumnPair ( & self -> dad, ctx, reader, writer, colspec, large );
    case 1:
    case 2:
    {
        /* look for special MATE_ALIGN_ID column */
        const char *colname = strrchr ( colspec, ')' );
        if ( colname ++ == NULL )
            colname = colspec;
        if ( strcmp ( colname, "MATE_ALIGN_ID" ) == 0 )
        {
            cSRAPair *csra = self -> csra;
            idx = self -> align_idx == 1 ? csra -> pa_idx : csra -> sa_idx;
        }
        break;
    }

    default:
        ANNOTATE ( "not going to dignify with an rc - bad align_idx" );
        return NULL;
    }

    /* create a buffer on writer, possibly with mapping */
    buffered = ( idx != NULL ) ?
        /* create a buffered mapping writer but WITHOUT id assignment capabilities */
        cSRATblPairMakeBufferedIdRemapColumnWriter ( self, ctx, writer, idx, false ):
        /* create a normal buffered writer */
        cSRATblPairMakeBufferedColumnWriter ( self, ctx, writer );

    if ( ! FAILED () )
    {
        col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, buffered, colspec, large );
        ColumnWriterRelease ( buffered, ctx );
    }

    return col;
}

static
void cSRATblPairPostCopyAlign ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    cSRAPair *csra = self -> csra;

    RowSetIteratorRelease ( self -> rsi, ctx );
    self -> rsi = NULL;

    if ( self -> align_idx == 2 )
    {
        MapFileRelease ( self -> csra -> sa_idx, ctx );
        self -> csra -> sa_idx = NULL;
    }

    switch ( self -> align_idx )
    {
    case 1:
        CrossCheckRefAlignTbl ( ctx, csra -> reference -> dtbl, csra -> prim_align -> dtbl, "PRIMARY_ALIGNMENT" );
        break;
    case 2:
        CrossCheckRefAlignTbl ( ctx, csra -> reference -> dtbl, csra -> sec_align -> dtbl, "SECONDARY_ALIGNMENT" );
        break;
    }
}

static
RowSetIterator *cSRATblPairGetRowSetIteratorAlign ( cSRATblPair *self, const ctx_t *ctx, bool mapping, bool large )
{
    FUNC_ENTRY ( ctx );

    TRY ( RowSetIteratorRelease ( self -> rsi, ctx ) )
    {
        cSRAPair *csra = self -> csra;
        MapFile *idx = self -> align_idx == 1 ? csra -> pa_idx : csra -> sa_idx;
        TRY ( self -> rsi = TablePairMakeRowSetIterator ( & self -> dad, ctx, idx, mapping, large ) )
        {
            return RowSetIteratorDuplicate ( self -> rsi, ctx );
        }
    }

    return NULL;
}


static TablePair_vt cSRATblPair_Align_vt =
{
    cSRATblPairWhack,
    cSRATblPairPreExplodeAlign,
    cSRATblPairExplodeAlign,
    cSRATblPairMakeColumnPairAlign,
    cSRATblPairDummyStub,
    cSRATblPairPostCopyAlign,
    cSRATblPairGetRowSetIteratorAlign
};



/* SEQUENCE table
 */

static
ColumnPair *cSRATblPairMakeSeqPrimAlignIdColPair ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col = NULL;
    cSRAPair *csra = self -> csra;


    ColumnReader *reader;
    const char *colspec = "(I64)PRIMARY_ALIGNMENT_ID";

    /* this column may not be present if there are no alignments */
    TRY ( reader = TablePairMakeColumnReader ( & self -> dad, ctx, NULL, colspec, false ) )
    {
        if ( reader != NULL )
        {
            ColumnWriter *writer;
            TRY ( writer = TablePairMakeColumnWriter ( & self -> dad, ctx, NULL, colspec ) )
            {
                ColumnWriter *capture;
                TRY ( capture = cSRAPairMakeFirstHalfAlignedRowIdCaptureWriter ( csra, ctx, writer ) )
                {
                    ColumnWriter *buffered;

                    /* create a buffered writer WITHOUT id-assignment capabilities. */
                    TRY ( buffered = cSRATblPairMakeBufferedIdRemapColumnWriter ( self, ctx,
                              capture, csra -> pa_idx, false ) )
                    {
                        col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, buffered, colspec, true );

                        ColumnWriterRelease ( buffered, ctx );
                    }

                    ColumnWriterRelease ( capture, ctx );
                }
                
                ColumnWriterRelease ( writer, ctx );
            }
            
            ColumnReaderRelease ( reader, ctx );
        }
    }

    return col;
}

static
void cSRATblPairPreExplodeSeq ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    TRY ( TablePairExplode ( & self -> dad, ctx ) )
    {
        cSRAPair *csra = self -> csra;
        TRY ( csra -> seq_idx = MapFileMake ( ctx, self -> dad . name, true ) )
        {
            TRY ( MapFileSetIdRange ( csra -> seq_idx, ctx, self -> dad . first_id,
                      self -> dad . last_excl - self -> dad . first_id ) )
            {
                ColumnPair *col;

                /* create special case for PRIMARY_ALIGNMENT_ID */
                TRY ( col = cSRATblPairMakeSeqPrimAlignIdColPair ( self, ctx ) )
                {
                    TablePairAddColumnPair ( & self -> dad, ctx, col );
                }
            }
        }
    }
}

static
ColumnPair *cSRATblPairMakeColumnPairSeq ( cSRATblPair *self, const ctx_t *ctx,
    struct ColumnReader *reader, struct ColumnWriter *writer, const char *colspec, bool large )
{
    FUNC_ENTRY ( ctx );

    ColumnPair *col = NULL;
    ColumnWriter *buffered;

    TRY ( buffered = cSRATblPairMakeBufferedColumnWriter ( self, ctx, writer ) )
    {
        col = TablePairMakeColumnPair ( & self -> dad, ctx, reader, buffered, colspec, large );
        ColumnWriterRelease ( buffered, ctx );
    }

    return col;
}

static
void cSRATblPairPreCopySeq ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    cSRAPair *csra = self -> csra;

    STATUS ( 3, "assigning new row-ids to unaligned sequences" );
    csra -> first_unaligned_spot = MapFileAllocMissingNewIds ( self -> csra -> seq_idx, ctx );
}

static
void cSRATblPairPostCopySeq ( cSRATblPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    cSRAPair *csra = self -> csra;

    RowSetIteratorRelease ( self -> rsi, ctx );
    self -> rsi = NULL;

    MapFileRelease ( csra -> pa_idx, ctx );
    csra -> pa_idx = NULL;

    MapFileRelease ( csra -> seq_idx, ctx );
    csra -> seq_idx = NULL;

    /* record markers in metadata */
    if ( ! FAILED () && ( csra -> first_half_aligned_spot != 0 || csra -> first_unaligned_spot != 0 ) )
    {
        MetaPair *meta = self -> dad . meta;
        KMDataNode *unaligned_node;
        const char *node_path = "unaligned";
        rc_t rc = KMetadataOpenNodeUpdate ( meta -> dmeta, & unaligned_node, "%s", node_path );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "KMetadataOpenNodeUpdate failed to open '%s'", node_path );
        else
        {
            KMDataNode *node;
            if ( csra -> first_half_aligned_spot != 0 )
            {
                node_path = "first-half-aligned";
                rc = KMDataNodeOpenNodeUpdate ( unaligned_node, & node, "%s", node_path );
                if ( rc != 0 )
                    INTERNAL_ERROR ( rc, "KMDataNodeOpenNodeUpdate failed to open 'unaligned/%s'", node_path );
                else
                {
                    rc = KMDataNodeWriteB64 ( node, & csra -> first_half_aligned_spot );
                    if ( rc != 0 )
                        INTERNAL_ERROR ( rc, "KMDataNodeWriteB64 failed to write 'unaligned/%s'", node_path );

                    KMDataNodeRelease ( node );
                }
            }

            if ( ! FAILED () && csra -> first_unaligned_spot != 0 )
            {
                node_path = "first-unaligned";
                rc = KMDataNodeOpenNodeUpdate ( unaligned_node, & node, "%s", node_path );
                if ( rc != 0 )
                    INTERNAL_ERROR ( rc, "KMDataNodeOpenNodeUpdate failed to open 'unaligned/%s'", node_path );
                else
                {
                    rc = KMDataNodeWriteB64 ( node, & csra -> first_unaligned_spot );
                    if ( rc != 0 )
                        INTERNAL_ERROR ( rc, "KMDataNodeWriteB64 failed to write 'unaligned/%s'", node_path );

                    KMDataNodeRelease ( node );
                }
            }

            KMDataNodeRelease ( unaligned_node );
        }
    }
}

static
RowSetIterator *cSRATblPairGetRowSetIteratorSeq ( cSRATblPair *self, const ctx_t *ctx, bool mapping, bool large )
{
    FUNC_ENTRY ( ctx );

    TRY ( RowSetIteratorRelease ( self -> rsi, ctx ) )
    {
        cSRAPair *csra = self -> csra;
        TRY ( self -> rsi = TablePairMakeRowSetIterator ( & self -> dad, ctx, csra -> seq_idx, mapping, large ) )
        {
            return RowSetIteratorDuplicate ( self -> rsi, ctx );
        }
    }

    return NULL;
}

static TablePair_vt cSRATblPair_Seq_vt =
{
    cSRATblPairWhack,
    cSRATblPairPreExplodeSeq,
    cSRATblPairDummyStub,
    cSRATblPairMakeColumnPairSeq,
    cSRATblPairPreCopySeq,
    cSRATblPairPostCopySeq,
    cSRATblPairGetRowSetIteratorSeq
};



/* Init
 *  common initialization code
 */
static
void cSRATblPairInit ( void *self, cSRATblPair *tbl )
{
    tbl -> csra = self;
    tbl -> rsi = NULL;
    tbl -> align_idx = 0;
}


/* MakeRef
 *  special cased for REFERENCE table
 */
TablePair *cSRATblPairMakeRef ( DbPair *self, const ctx_t *ctx,
    const VTable *src, VTable *dst, const char *name, bool reorder )
{
    FUNC_ENTRY ( ctx );

    cSRATblPair *tbl;

    TRY ( tbl = MemAlloc ( ctx, sizeof * tbl, false ) )
    {
        TRY ( TablePairInit ( & tbl -> dad, ctx, & cSRATblPair_Ref_vt, src, dst, name, self -> full_spec, reorder ) )
        {
            static const char *exclude_cols [] = { "PRIMARY_ALIGNMENT_IDS", "SECONDARY_ALIGNMENT_IDS", "READ", "SPOT_GROUP", NULL };
            static const char *nonstatic_cols [] = { "NAME", NULL };
            tbl -> dad . exclude_col_names = exclude_cols;
            tbl -> dad . nonstatic_col_names = nonstatic_cols;
            cSRATblPairInit ( self, tbl );
            return & tbl -> dad;
        }

        MemFree ( ctx, tbl, sizeof * tbl );
    }

    return NULL;
}


/* MakeAlign
 *  special cased for *_ALIGNMENT tables
 */
TablePair *cSRATblPairMakeAlign ( DbPair *self, const ctx_t *ctx,
    const VTable *src, VTable *dst, const char *name, bool reorder )
{
    FUNC_ENTRY ( ctx );

    cSRATblPair *tbl;

    TRY ( tbl = MemAlloc ( ctx, sizeof * tbl, false ) )
    {
        TRY ( TablePairInit ( & tbl -> dad, ctx, & cSRATblPair_Align_vt, src, dst, name, self -> full_spec, reorder ) )
        {
            static const char *exclude_cols [] = { "GLOBAL_REF_START", "READ_LEN", "REF_ID", "REF_START", "REF_LEN", "SEQ_SPOT_ID", NULL };
            static const char *unsorted_exclude_cols [] = { "READ_LEN", "SEQ_SPOT_ID", NULL };
            tbl -> dad . exclude_col_names = reorder ? exclude_cols : unsorted_exclude_cols;
            cSRATblPairInit ( self, tbl );
            return & tbl -> dad;
        }

        MemFree ( ctx, tbl, sizeof * tbl );
    }

    return NULL;
}


/* MakeSeq
 *  special cased for SEQUENCE table
 */
TablePair *cSRATblPairMakeSeq ( DbPair *self, const ctx_t *ctx,
    const VTable *src, VTable *dst, const char *name, bool reorder )
{
    FUNC_ENTRY ( ctx );

    cSRATblPair *tbl;

    TRY ( tbl = MemAlloc ( ctx, sizeof * tbl, false ) )
    {
        TRY ( TablePairInit ( & tbl -> dad, ctx, & cSRATblPair_Seq_vt, src, dst, name, self -> full_spec, reorder ) )
        {
            static const char *exclude_cols [] = { "PRIMARY_ALIGNMENT_ID", NULL };
            static const char *nonstatic_cols [] = { "NAME_FMT", NULL };
            static const char *large_cols [] = { "CMP_READ", "QUALITY", NULL };
            static const char *exclude_meta [] = { "unaligned", NULL };
            tbl -> dad . exclude_col_names = exclude_cols;
            tbl -> dad . nonstatic_col_names = nonstatic_cols;
            tbl -> dad . large_col_names = large_cols;
            tbl -> dad . exclude_meta = exclude_meta;
            cSRATblPairInit ( self, tbl );
            return & tbl -> dad;
        }

        MemFree ( ctx, tbl, sizeof * tbl );
    }

    return NULL;
}
