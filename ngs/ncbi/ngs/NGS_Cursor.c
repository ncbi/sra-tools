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

#include "NGS_Cursor.h"

#include "NGS_String.h"
#include <ngs/itf/Refcount.h>

#include <sysalloc.h>

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <vdb/table.h>
#include <vdb/database.h>

#include <klib/rc.h>

/*--------------------------------------------------------------------------
 * NGS_Cursor
 *  Shareable cursor with lazy adding of columns
 *  reference counted
 */

#define ILLEGAL_COL_IDX 0xFFFFFFFF

struct NGS_Cursor
{
    NGS_Refcount dad;

    VCursor* curs;

    uint32_t num_cols;
    char ** col_specs;      /* [num_cols] */
    uint32_t* col_idx;      /* [num_cols] */
    NGS_String ** col_data; /* [num_cols] */

    /* row range */
    int64_t first;
    uint64_t count;
};

void NGS_CursorWhack ( NGS_Cursor * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcConstructing );

    uint32_t i;

    VCursorRelease ( self -> curs );

    for ( i = 0; i < self -> num_cols; ++i )
    {
        if ( self -> col_specs != NULL )
        {
            free ( self -> col_specs [ i ] );
        }
        if ( self -> col_data != NULL )
        {
            NGS_StringRelease ( self -> col_data [ i ], ctx );
        }
    }

    free ( self -> col_specs );
    free ( self -> col_data );

    free ( self -> col_idx );
}

static NGS_Refcount_vt NGS_Cursor_vt =
{
    NGS_CursorWhack
};

/* Make
 */
const NGS_Cursor * NGS_CursorMake ( ctx_t ctx, const struct VTable* table, const char * col_specs[], uint32_t num_cols )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    NGS_Cursor* ref = calloc( 1, sizeof (*ref) );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating NGS_Cursor" );
    else
    {
        TRY ( NGS_RefcountInit ( ctx, (NGS_Refcount*) & ref -> dad, & ITF_Refcount_vt . dad, & NGS_Cursor_vt, "NGS_Cursor", "" ) )
        {
            rc_t rc = VTableCreateCursorRead ( table, (const VCursor**) & ref -> curs );
            if ( rc != 0 )
            {
                INTERNAL_ERROR ( xcCursorCreateFailed, "VTableCreateCursorRead rc = %R", rc );
                NGS_CursorWhack ( ref, ctx );
                free ( ref );
                return NULL;
            }

            ref -> num_cols = num_cols;

            /* make a copy of col specs */
            ref -> col_specs = malloc ( num_cols * sizeof ( col_specs [ 0 ] ) );
            if ( ref -> col_specs == NULL )
            {
                SYSTEM_ERROR ( xcNoMemory, "allocating NGS_Cursor . col_specs" );
                NGS_CursorWhack ( ref, ctx );
                free ( ref );
                return NULL;
            }

            {
                uint32_t i;
                for ( i = 0; i < num_cols; ++i )
                {
                    ref -> col_specs [ i ] = string_dup ( col_specs [ i ], string_size ( col_specs [ i ] ) );
                    if ( ref -> col_specs [ i ] == NULL )
                    {
                        SYSTEM_ERROR ( xcNoMemory, "populating NGS_Cursor . col_specs" );
                        NGS_CursorWhack ( ref, ctx );
                        free ( ref );
                        return NULL;
                    }
                }
            }

            ref -> col_idx = calloc ( num_cols,  sizeof ( ref -> col_idx [ 0 ] ) );
            if ( ref -> col_idx == NULL )
            {
                SYSTEM_ERROR ( xcNoMemory, "allocating NGS_Cursor . col_idx" );
                NGS_CursorWhack ( ref, ctx );
                free ( ref );
                return NULL;
            }

            ref -> col_data = calloc ( num_cols,  sizeof ( ref -> col_data[ 0 ] ) );
            if ( ref -> col_idx == NULL )
            {
                SYSTEM_ERROR ( xcNoMemory, "allocating NGS_Cursor . col_data" );
                NGS_CursorWhack ( ref, ctx );
                free ( ref );
                return NULL;
            }

            {   /* add first column; leave other for lazy add */
                const char * col_spec = col_specs [ 0 ];
                rc_t rc = VCursorAddColumn ( ref -> curs, & ref -> col_idx [ 0 ], "%s", col_spec );
                if ( rc != 0 )
                {
                    ref -> col_idx [ 0 ] = ILLEGAL_COL_IDX;
                    INTERNAL_ERROR ( xcColumnNotFound, "VCursorAddColumn %s rc = %R", col_spec, rc );
                    NGS_CursorWhack ( ref, ctx );
                    free ( ref );
                    return NULL;
                }

                rc = VCursorPermitPostOpenAdd ( ref -> curs );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( xcCursorOpenFailed, "PostOpenAdd failed rc = %R", rc );
                    NGS_CursorWhack ( ref, ctx );
                    free ( ref );
                    return NULL;
                }

                /* open cursor */
                rc = VCursorOpen ( ref -> curs );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( xcCursorOpenFailed, "VCursorOpen failed rc = %R", rc );
                    NGS_CursorWhack ( ref, ctx );
                    free ( ref );
                    return NULL;
                }

                rc = VCursorIdRange ( ref -> curs, 0, & ref -> first, & ref -> count );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( xcCursorOpenFailed, "VCursorIdRange failed rc = %R", rc );
                    NGS_CursorWhack ( ref, ctx );
                    free ( ref );
                    return NULL;
                }

                return ref;
            }
        }
    }

    return NULL;
}

const NGS_Cursor* NGS_CursorMakeDb ( ctx_t ctx,
                                     const VDatabase* db,
                                     const NGS_String* run_name,
                                     const char* tableName,
                                     const char * col_specs[],
                                     uint32_t num_cols )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcConstructing );

    const VTable * table;
    rc_t rc = VDatabaseOpenTableRead ( db, & table, "%s", tableName );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcTableOpenFailed, "%.*s.%s rc = %R", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ), tableName, rc );
        return NULL;
    }

    {
        const NGS_Cursor* ret = NGS_CursorMake ( ctx, table, col_specs, num_cols );
        VTableRelease ( table );
        return ret;
    }
}

/* Release
 *  release reference
 */
void NGS_CursorRelease ( const NGS_Cursor * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcDestroying );

    if ( self != NULL )
    {
        NGS_RefcountRelease ( & self -> dad, ctx );
    }
}

/* Duplicate
 *  duplicate reference
 */
const NGS_Cursor * NGS_CursorDuplicate ( const NGS_Cursor * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcConstructing );

    if ( self != NULL )
    {
        NGS_RefcountDuplicate( & self -> dad, ctx );
    }
    return ( NGS_Cursor* ) self;
}

static
void
AddColumn ( const NGS_Cursor *self, ctx_t ctx, uint32_t colIdx )
{
    /* lazy add */
    if ( self -> col_idx [ colIdx ] == 0 )
    {
        const char * col_spec = self -> col_specs [ colIdx ];
        rc_t rc = VCursorAddColumn ( self -> curs, & self -> col_idx [ colIdx ], "%s", col_spec );
        if ( rc != 0 && GetRCState ( rc ) != rcExists )
        {
            self -> col_idx [ colIdx ] = ILLEGAL_COL_IDX;
            INTERNAL_ERROR ( xcColumnNotFound, "VCursorAddColumn failed: '%s' rc = %R", col_spec, rc );
        }
    }
    else if ( self -> col_idx [ colIdx ] == ILLEGAL_COL_IDX )
    {
        const char * col_spec = self -> col_specs [ colIdx ];
        INTERNAL_ERROR ( xcColumnNotFound, "VCursorAddColumn previously failed: '%s'", col_spec );
    }

}

void NGS_CursorCellDataDirect ( const NGS_Cursor *self,
                                ctx_t ctx,
                                int64_t rowId,
                                uint32_t colIdx,
                                uint32_t *elem_bits,
                                const void **base,
                                uint32_t *boff,
                                uint32_t *row_len )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    rc_t rc;

    assert ( self != NULL );

    TRY ( AddColumn ( self, ctx, colIdx ) )
    {
        rc = VCursorCellDataDirect ( self -> curs, rowId, self -> col_idx [ colIdx ], elem_bits, base, boff, row_len );
        if ( rc != 0 )
        {
            INTERNAL_ERROR ( xcColumnNotFound, "VCursorCellDataDirect failed: '%s' [%ld] rc = %R", self -> col_specs [ colIdx ], rowId, rc );
        }
    }
}

/* GetRowCount
 */
uint64_t NGS_CursorGetRowCount ( const NGS_Cursor * self, ctx_t ctx )
{
    assert ( self != NULL );

    return self -> count;
}

/* GetRowRange
 */
void NGS_CursorGetRowRange ( const NGS_Cursor * self, ctx_t ctx, int64_t* first, uint64_t* count )
{
    assert ( self != NULL );
    assert ( first != NULL );
    assert ( count != NULL );

    *first = self -> first;
    *count = self -> count;
}

NGS_String * NGS_CursorGetString ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    assert ( self -> col_data );
    assert ( self -> curs );

    /* invalidate any outstanding string */
    NGS_StringInvalidate ( self -> col_data [ colIdx ], ctx );

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self, ctx, rowId, colIdx, & elem_bits, & base, & boff, & row_len ) )
        {
            NGS_String * new_data;

            assert ( elem_bits == 8 );
            assert ( boff == 0 );

            /* create new string */
            TRY ( new_data = NGS_StringMake ( ctx, base, row_len ) )
            {
                NGS_StringRelease ( self -> col_data [ colIdx ], ctx );
                self -> col_data [ colIdx ] = new_data;
                return NGS_StringDuplicate ( self -> col_data [ colIdx ], ctx );
            }
        }
    }
    return NULL;
}

/* GetInt64
*/
int64_t NGS_CursorGetInt64 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    assert ( self -> col_data );
    assert ( self -> col_idx );

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self, ctx, rowId, colIdx, & elem_bits, & base, & boff, & row_len ) )
        {
            if ( base == 0 || row_len == 0 )
                INTERNAL_ERROR ( xcColumnReadFailed, "cell value is missing" );
            else
            {
                assert ( elem_bits == 64 || elem_bits == 32 );
                assert ( boff == 0 );

                if ( elem_bits == 64 )
                    return *(int64_t*)base;
                else
                    return *(int32_t*)base;
            }
        }
    }

    return 0;
}

/* GetUInt64
*/
uint64_t NGS_CursorGetUInt64 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    assert ( self -> col_data );
    assert ( self -> col_idx );

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self, ctx, rowId, colIdx, & elem_bits, & base, & boff, & row_len ) )
        {
            if ( base == 0 || row_len == 0 )
                INTERNAL_ERROR ( xcColumnReadFailed, "cell value is missing" );
            else
            {
                assert ( elem_bits == 64 || elem_bits == 32 );
                assert ( boff == 0 );

                if ( elem_bits == 64 )
                    return *(uint64_t*)base;
                else
                    return *(uint32_t*)base;
            }
        }
    }

    return 0;
}


/* GetInt32
*/
int32_t NGS_CursorGetInt32 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    assert ( self -> col_data );
    assert ( self -> col_idx );

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self, ctx, rowId, colIdx, & elem_bits, & base, & boff, & row_len ) )
        {
            if ( base == 0 || row_len == 0 )
                INTERNAL_ERROR ( xcColumnReadFailed, "cell value is missing" );
            else
            {
                assert ( elem_bits == 32 );
                assert ( boff == 0 );

                return *(int32_t*)base;
            }
        }
    }

    return 0;
}

/* GetUInt32
*/
uint32_t NGS_CursorGetUInt32 ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    assert ( self -> col_data );
    assert ( self -> col_idx );

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self, ctx, rowId, colIdx, & elem_bits, & base, & boff, & row_len ) )
        {
            if ( base == 0 || row_len == 0 )
                INTERNAL_ERROR ( xcColumnReadFailed, "cell value is missing" );
            else
            {
                assert ( elem_bits == 32 );
                assert ( boff == 0 );

                return *(uint32_t*)base;
            }
        }
    }

    return 0;
}

/* GetBool
*/
bool NGS_CursorGetBool ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    assert ( self -> col_data );
    assert ( self -> col_idx );

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self, ctx, rowId, colIdx, & elem_bits, & base, & boff, & row_len ) )
        {
            if ( base == 0 || row_len == 0 )
                INTERNAL_ERROR ( xcColumnReadFailed, "cell value is missing" );
            else
            {
                assert ( elem_bits == 8 );
                assert ( boff == 0 );

                return *(bool*)base;
            }
        }
    }

    return false;
}

/* GetChar
*/
char NGS_CursorGetChar ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t colIdx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    assert ( self -> col_data );
    assert ( self -> col_idx );

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self, ctx, rowId, colIdx, & elem_bits, & base, & boff, & row_len ) )
        {
            if ( base == 0 || row_len == 0 )
                INTERNAL_ERROR ( xcColumnReadFailed, "cell value is missing" );
            else
            {
                assert ( elem_bits == 8 );
                assert ( boff == 0 );

                return *(char*)base;
            }
        }
    }

    return '?';
}


/* GetTable
 */
const VTable* NGS_CursorGetTable ( const NGS_Cursor * self, ctx_t ctx )
{
    assert ( self );
	{
		const VTable* tbl;
		rc_t rc = VCursorOpenParentRead( self -> curs, &tbl );
		if ( rc == 0 )
		{
			return tbl;
		}
		INTERNAL_ERROR ( xcCursorAccessFailed, "VCursorOpenParentRead rc = %R", rc );
		return NULL;
	}
}

/* GetVCursor
 */
const VCursor* NGS_CursorGetVCursor ( const NGS_Cursor * self )
{
    assert ( self );
    return self -> curs;
}

uint32_t NGS_CursorGetColumnIndex ( const NGS_Cursor * self, ctx_t ctx, uint32_t column_id )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    ON_FAIL ( AddColumn ( self, ctx, column_id ) )
    {
        return 0;
    }
    return self -> col_idx [ column_id ];
}

/* GetVBlob
 */
const struct VBlob* NGS_CursorGetVBlob ( const NGS_Cursor * self, ctx_t ctx, int64_t rowId, uint32_t column_id )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    rc_t rc = VCursorSetRowId ( self -> curs, rowId );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "VCursorSetRowId() rc = %R", rc );
    }
    else
    {
        rc = VCursorOpenRow ( self -> curs );
        if ( rc != 0 )
        {
            INTERNAL_ERROR ( xcUnexpected, "VCursorOpenRow() rc = %R", rc );
        }
        else
        {
            const struct VBlob* ret;
            rc = VCursorGetBlob ( self -> curs, & ret, NGS_CursorGetColumnIndex ( self, ctx, column_id ) );
            if ( rc != 0 || FAILED () )
            {
                VCursorCloseRow ( self -> curs );
                INTERNAL_ERROR ( xcUnexpected, "VCursorGetBlob(READ) rc = %R", rc );
            }
            else
            {
                VCursorCloseRow ( self -> curs );
                return ret;
            }
        }
    }
    return NULL;
}
