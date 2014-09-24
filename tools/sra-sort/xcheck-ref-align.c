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

#include "caps.h"
#include "ctx.h"
#include "mem.h"
#include "except.h"
#include "status.h"

#include <kapp/main.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <kproc/thread.h>
#include <klib/rc.h>

#include <string.h>

#if ! _DEBUGGING
#define USE_BGTHREAD 1
#endif

FILE_ENTRY ( xcheck-ref-align );


/*--------------------------------------------------------------------------
 * TestReferenceCell
 *  properly sorted tables will allow both columns to be walked.
 *  each row of the REFERENCE table column MUST contain zero or more
 *  sequential integers, and the first integer MUST continue the sequence
 *  established by previous rows. therefore, the row will be fully specified
 *  by a start and stop id pair.
 */
static
int64_t TestReferenceCell ( const ctx_t *ctx,
    const VCursor *ref_curs, uint32_t align_ids_idx,
    const char *align_name, int64_t ref_row_id, int64_t excl_ref_last_idx )
{
    FUNC_ENTRY ( ctx );

    const int64_t *cell;
    uint32_t elem_bits, boff, row_len;

    rc_t rc = VCursorCellDataDirect ( ref_curs, ref_row_id, align_ids_idx,
        & elem_bits, ( const void** ) & cell, & boff, & row_len );
    if ( rc != 0 )
        ERROR ( rc, "VCursorCellDataDirect - failed to read row %ld from REFERENCE cursor", ref_row_id );
    else if ( elem_bits != sizeof * cell * 8 )
    {
        rc = RC ( rcExe, rcIndex, rcValidating, rcSize, rcIncorrect );
        ERROR ( rc, "VCursorCellDataDirect - elem_bits of %u reading row %ld from REFERENCE cursor", elem_bits, ref_row_id );
    }
    else if ( boff != 0 )
    {
        rc = RC ( rcExe, rcIndex, rcValidating, rcOffset, rcIncorrect );
        ERROR ( rc, "VCursorCellDataDirect - bit offset of %u reading row %ld from REFERENCE cursor", boff, ref_row_id );
    }
    else
    {
        uint32_t i;
        for ( i = 0; i < row_len; ++ i )
        {
            if ( cell [ i ] != excl_ref_last_idx + i )
            {
                rc = RC ( rcExe, rcIndex, rcValidating, rcId, rcIncorrect );
                ERROR ( rc, "REFERENCE.%s_IDS.%ld: expected id %ld but found %ld",
                        align_name, ref_row_id, excl_ref_last_idx + i, cell [ i ] );
                break;
            }
        }

        excl_ref_last_idx += row_len;
    }

    return excl_ref_last_idx;
}

static
void TestAlignCell ( const ctx_t *ctx,
    const VCursor *align_curs, uint32_t ref_id_idx,
    const char *align_name, int64_t align_row_id, int64_t ref_row_id )
{
    FUNC_ENTRY ( ctx );

    const int64_t *cell;
    uint32_t elem_bits, boff, row_len;


    rc_t rc = VCursorCellDataDirect ( align_curs, align_row_id, ref_id_idx,
        & elem_bits, ( const void** ) & cell, & boff, & row_len );
    if ( rc != 0 )
        ERROR ( rc, "VCursorCellDataDirect - failed to read row %ld from %s cursor", align_row_id, align_name );
    else if ( elem_bits != sizeof * cell * 8 )
    {
        rc = RC ( rcExe, rcIndex, rcValidating, rcSize, rcIncorrect );
        ERROR ( rc, "VCursorCellDataDirect - elem_bits of %u reading row %ld from %s cursor", elem_bits, align_row_id, align_name );
    }
    else if ( boff != 0 )
    {
        rc = RC ( rcExe, rcIndex, rcValidating, rcOffset, rcIncorrect );
        ERROR ( rc, "VCursorCellDataDirect - bit offset of %u reading row %ld from %s cursor", boff, align_row_id, align_name );
    }
    else if ( row_len != 1 )
    {
        rc = RC ( rcExe, rcIndex, rcValidating, rcRange, rcIncorrect );
        ERROR ( rc, "VCursorCellDataDirect - row_len of %u reading row %ld from %s cursor", row_len, align_row_id, align_name );
    }
    else if ( * cell != ref_row_id )
    {
        rc = RC ( rcExe, rcIndex, rcValidating, rcId, rcIncorrect );
        ERROR ( rc, "%s.REF_ID.%ld: expected id %ld but found %ld",
                align_name, align_row_id, ref_row_id, cell [ 0 ] );
    }
}

/*--------------------------------------------------------------------------
 * CrossCheckRefAlignCols
 *  performs the cross-check
 */
static
void CrossCheckRefAlignCols ( const ctx_t *ctx,
    const VCursor *ref_curs, uint32_t align_ids_idx,
    const VCursor *align_curs, uint32_t ref_id_idx, const char *align_name )
{
    FUNC_ENTRY ( ctx );

    int64_t ref_row_id, excl_ref_last_id;
    rc_t rc = VCursorIdRange ( ref_curs, 0, & ref_row_id, ( uint64_t* ) & excl_ref_last_id );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VCursorIdRange - failed to establish row range on REFERENCE cursor" );
    else
    {
        int64_t align_row_id, excl_align_last_id;

        excl_ref_last_id += ref_row_id;

        rc = VCursorIdRange ( align_curs, 0, & align_row_id, ( uint64_t* ) & excl_align_last_id );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VCursorIdRange - failed to establish row range on %s cursor", align_name );
        else
        {
            int64_t excl_last_align_idx;

            excl_align_last_id += align_row_id;

            for ( excl_last_align_idx = 1; ref_row_id < excl_ref_last_id; ++ ref_row_id )
            {
                int64_t first_align_idx = excl_last_align_idx;

                /* rule for bailing out */
                rc = Quitting ();
                if ( rc != 0 || FAILED () )
                    break;

                /* the REFERENCE id cell should be filled purely with sequential ids */
                TRY ( excl_last_align_idx = TestReferenceCell ( ctx, ref_curs, align_ids_idx,
                          align_name, ref_row_id, excl_last_align_idx ) )
                {
                    /* the ids must be within the range of the alignment table */
                    if ( excl_last_align_idx > excl_align_last_id )
                    {
                        rc = RC ( rcExe, rcIndex, rcValidating, rcId, rcExcessive );
                        ERROR ( rc, "REFERENCE.%s_IDS.%ld: references non-existant rows ( %ld .. %ld : max %ld )",
                                align_name, ref_row_id, first_align_idx, excl_last_align_idx, excl_align_last_id );
                        break;
                    }

                    /* this is more of a permanent assert */
                    if ( first_align_idx != align_row_id )
                    {
                        rc = RC ( rcExe, rcIndex, rcValidating, rcId, rcIncorrect );
                        ERROR ( rc, "REFERENCE.%s_IDS.%ld: expected id %ld but found %ld",
                                align_name, ref_row_id, first_align_idx, align_row_id );
                        break;
                    }

                    /* each of the rows in alignment table must point back
                       to the same row in the REFERENCE table */
                    for ( ; align_row_id < excl_last_align_idx; ++ align_row_id )
                    {
                        ON_FAIL ( TestAlignCell ( ctx, align_curs, ref_id_idx,
                                      align_name, align_row_id, ref_row_id ) )
                            break;
                    }
                }
            }

            /* at this point, we must have seen every record */
            if ( ! FAILED () )
            {
                if ( ref_row_id != excl_ref_last_id )
                {
                    rc = RC ( rcExe, rcIndex, rcValidating, rcRange, rcIncomplete );
                    ERROR ( rc, "REFERENCE.%s_IDS: scan stopped on row %ld of %ld",
                            align_name, ref_row_id, excl_ref_last_id );
                }
                if ( align_row_id != excl_align_last_id )
                {
                    rc = RC ( rcExe, rcIndex, rcValidating, rcRange, rcIncomplete );
                    ERROR ( rc, "%s.REF_ID: scan stopped on row %ld of %ld",
                            align_name, align_row_id, excl_align_last_id );
                }
            }
        }
    }
}



/*--------------------------------------------------------------------------
 * CrossCheckRefAlignCurs
 *  adds columns and opens cursors
 */
static
void CrossCheckRefAlignCurs ( const ctx_t *ctx,
    const VCursor *ref_curs, const VCursor *align_curs, const char *align_name )
{
    FUNC_ENTRY ( ctx );

    uint32_t align_ids_idx;
    rc_t rc = VCursorAddColumn ( ref_curs, & align_ids_idx, "%s_IDS", align_name );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VCursorAddColumn - failed to add column '%s_IDS' to REFERENCE cursor", align_name );
    else
    {
        uint32_t ref_id_idx;
        rc = VCursorAddColumn ( align_curs, & ref_id_idx, "REF_ID" );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VCursorAddColumn - failed to add column 'REF_ID' to %s cursor", align_name );
        else
        {
            rc = VCursorOpen ( ref_curs );
            if ( rc != 0 )
                INTERNAL_ERROR ( rc, "VCursorOpen - failed to open cursor on REFERENCE table" );
            else
            {
                rc = VCursorOpen ( align_curs );
                if ( rc != 0 )
                    INTERNAL_ERROR ( rc, "VCursorOpen - failed to open cursor on %s table", align_name );
                else
                {
                    rc = VCursorSetRowId ( ref_curs, 1 );
                    if ( rc != 0 )
                        INTERNAL_ERROR ( rc, "VCursorSetRowId - failed to set row-id on REFERENCE cursor" );
                    else
                    {
                        rc = VCursorSetRowId ( align_curs, 1 );
                        if ( rc != 0 )
                            INTERNAL_ERROR ( rc, "VCursorSetRowId - failed to set row-id on %s cursor", align_name );
                        else
                        {
                            CrossCheckRefAlignCols ( ctx, ref_curs, align_ids_idx, align_curs, ref_id_idx, align_name );
                        }
                    }
                }
            }
        }
    }
}


/*--------------------------------------------------------------------------
 * CrossCheckRefAlignTbl
 *  checks REFERENCE.<name>_IDS for properly sorted form
 *  runs a cross-check of REFERENCE.<name>_IDS against <name>.REF_ID
 */
static
void CrossCheckRefAlignTblInt ( const ctx_t *ctx,
    const VTable *ref_tbl, const VTable *align_tbl, const char *align_name )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const VCursor *ref_curs;

    rc = VTableCreateCursorRead ( ref_tbl, & ref_curs );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "VTableCreateCursorRead - failed to open cursor on REFERENCE table" );
    else
    {
        const VCursor *align_curs;
        rc = VTableCreateCursorRead ( align_tbl, & align_curs );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "VTableCreateCursorRead - failed to open cursor on %s table", align_name );
        else
        {
            CrossCheckRefAlignCurs ( ctx, ref_curs, align_curs, align_name );

            VCursorRelease ( align_curs );
        }

        VCursorRelease ( ref_curs );
    }
}

#if USE_BGTHREAD

typedef struct CrossCheckRefAlignTblData CrossCheckRefAlignTblData;
struct CrossCheckRefAlignTblData
{
    Caps caps;
    const VTable *ref_tbl, *align_tbl;
    char align_name [ 1 ];
};

static
rc_t CC CrossCheckRefAlignTblRun ( const KThread *self, void *data )
{
    CrossCheckRefAlignTblData *pb = data;

    DECLARE_CTX_INFO ();
    ctx_t thread_ctx = { & pb -> caps, NULL, & ctx_info };
    const ctx_t *ctx = & thread_ctx;

    STATUS ( 2, "running consistency-check on background thread" );

    CrossCheckRefAlignTblInt ( ctx, pb -> ref_tbl, pb -> align_tbl, pb -> align_name );

    STATUS ( 2, "finished consistency-check on background thread" );

    VTableRelease ( pb -> align_tbl );
    VTableRelease ( pb -> ref_tbl );
    CapsWhack ( & pb -> caps, ctx );

    return ctx -> rc;
}

#endif

void CrossCheckRefAlignTbl ( const ctx_t *ctx,
    const VTable *ref_tbl, const VTable *align_tbl, const char *align_name )
{
    FUNC_ENTRY ( ctx );

#if USE_BGTHREAD
    size_t name_len;
    CrossCheckRefAlignTblData *pb;
#endif

    STATUS ( 2, "consistency-check on join indices between REFERENCE and %s tables", align_name );

#if USE_BGTHREAD
    name_len = strlen ( align_name );
    TRY ( pb = MemAlloc ( ctx, sizeof * pb + name_len, false ) )
    {
        TRY ( CapsInit ( & pb -> caps, ctx ) )
        {
            rc_t rc = VTableAddRef ( pb -> ref_tbl = ref_tbl );
            if ( rc != 0 )
                ERROR ( rc, "VTableAddRef failed on REFERENCE table" );
            else
            {
                rc = VTableAddRef ( pb -> align_tbl = align_tbl );
                if ( rc != 0 )
                    ERROR ( rc, "VTableAddRef failed on %s table", align_name );
                else
                {
                    KThread *t;

                    strcpy ( pb -> align_name, align_name );

                    rc = KThreadMake ( & t, CrossCheckRefAlignTblRun, pb );
                    if ( rc == 0 )
                    {
                        return;
                    }

                    VTableRelease ( pb -> align_tbl );
                }

                VTableRelease ( pb -> ref_tbl );
            }

            CapsWhack ( & pb -> caps, ctx );
        }

        MemFree ( ctx, pb, sizeof * pb + name_len );
    }
#else
    CrossCheckRefAlignTblInt ( ctx, ref_tbl, align_tbl, align_name );
#endif
}
