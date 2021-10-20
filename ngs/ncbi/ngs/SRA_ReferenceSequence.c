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

#include "SRA_ReferenceSequence.h"

typedef struct SRA_ReferenceSequence SRA_ReferenceSequence;
#define NGS_REFERENCESEQUENCE SRA_ReferenceSequence
#include "NGS_ReferenceSequence.h"

#include "NGS_String.h"
#include "NGS_Cursor.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/refcount.h>

#include <vdb/manager.h>
#include <vdb/table.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>

#include <stddef.h>
#include <assert.h>

#include <strtol.h>
#include <string.h>

#include <sysalloc.h>

/*--------------------------------------------------------------------------
 * SRA_ReferenceSequence
 */

static void                SRA_ReferenceSequenceWhack ( SRA_ReferenceSequence * self, ctx_t ctx );
static NGS_String *        SRA_ReferenceSequenceGetCanonicalName ( SRA_ReferenceSequence * self, ctx_t ctx );
static bool                SRA_ReferenceSequenceGetIsCircular ( SRA_ReferenceSequence const* self, ctx_t ctx );
static uint64_t            SRA_ReferenceSequenceGetLength ( SRA_ReferenceSequence * self, ctx_t ctx );
static struct NGS_String * SRA_ReferenceSequenceGetBases ( SRA_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size );
static struct NGS_String * SRA_ReferenceSequenceGetChunk ( SRA_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size );

static NGS_ReferenceSequence_vt SRA_ReferenceSequence_vt_inst =
{
    /* NGS_Refcount */
    { SRA_ReferenceSequenceWhack },

    /* NGS_ReferenceSequence */
    SRA_ReferenceSequenceGetCanonicalName,
    SRA_ReferenceSequenceGetIsCircular,
    SRA_ReferenceSequenceGetLength,
    SRA_ReferenceSequenceGetBases,
    SRA_ReferenceSequenceGetChunk,
};


struct SRA_ReferenceSequence
{
    NGS_ReferenceSequence dad;

    const VTable * tbl;
    const struct NGS_Cursor * curs;

    uint32_t chunk_size;

    int64_t first_row;
    int64_t last_row;  /* inclusive */
    uint64_t cur_length; /* size of current reference in bases (0 = not yet counted) */
};

static char const* g_ReferenceTableColumnNames [] =
{
    "(bool)CIRCULAR",
    /*"(utf8)NAME",*/
    "(ascii)SEQ_ID",
    "(INSDC:coord:len)SEQ_LEN",
    /*"(INSDC:coord:one)SEQ_START",*/
    "(U32)MAX_SEQ_LEN",
    "(ascii)READ",
    /*"(I64)PRIMARY_ALIGNMENT_IDS",
    "(I64)SECONDARY_ALIGNMENT_IDS",
    "(INSDC:coord:len)OVERLAP_REF_LEN",
    "(INSDC:coord:zero)OVERLAP_REF_POS"*/
};

enum g_ReferenceTableColumns
{
    reference_CIRCULAR,
    /*reference_NAME,*/
    reference_SEQ_ID,
    reference_SEQ_LEN,
    /*reference_SEQ_START,*/
    reference_MAX_SEQ_LEN,
    reference_READ,
    /*reference_PRIMARY_ALIGNMENT_IDS,
    reference_SECONDARY_ALIGNMENT_IDS,
    reference_OVERLAP_REF_LEN,
    reference_OVERLAP_REF_POS,*/

    reference_NUM_COLS
};


static
void SRA_ReferenceSequenceWhack ( SRA_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcClosing );

    NGS_CursorRelease ( self -> curs, ctx );
    VTableRelease ( self -> tbl );

    self -> curs = NULL;
    self -> tbl = NULL;
}

/* Init
 */
static
void SRA_ReferenceSequenceInit ( ctx_t ctx,
                           SRA_ReferenceSequence * ref,
                           const char *clsname,
                           const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcOpening );

    if ( ref == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_ReferenceSequenceInit ( ctx, & ref -> dad, & SRA_ReferenceSequence_vt_inst, clsname, instname ) )
        {
            /* TODO: maybe initialize more*/
        }
    }
}

NGS_ReferenceSequence * NGS_ReferenceSequenceMakeSRA ( ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcOpening );

    SRA_ReferenceSequence * ref;

    assert ( spec != NULL );
    assert ( spec [0] != '\0' );

    ref = calloc ( 1, sizeof *ref );
    if ( ref == NULL )
    {
        SYSTEM_ERROR ( xcNoMemory, "allocating SRA_ReferenceSequence ( '%s' )", spec );
    }
    else
    {
        TRY ( SRA_ReferenceSequenceInit ( ctx, ref, "NGS_ReferenceSequence", spec ) )
        {
            rc_t rc;

            const VDBManager * mgr = ctx -> rsrc -> vdb;
            assert ( mgr != NULL );

            rc = VDBManagerOpenTableRead ( mgr, & ref -> tbl, NULL, spec );
            if ( rc != 0 )
            {
                INTERNAL_ERROR ( xcUnexpected, "failed to open table '%s': rc = %R", spec, rc );
            }
            else
            {   /* VDB-2641: examine the schema name to make sure this is an SRA table */
                char ts_buff[1024];
                rc = VTableTypespec ( ref -> tbl, ts_buff, sizeof ( ts_buff ) );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( xcUnexpected, "VTableTypespec failed: rc = %R", rc );
                }
                else
                {
                    const char REF_PREFIX[] = "NCBI:refseq:";
                    size_t pref_size = sizeof ( REF_PREFIX ) - 1;
                    if ( string_match ( REF_PREFIX, pref_size, ts_buff, string_size ( ts_buff ), (uint32_t)pref_size, NULL ) != pref_size )
                    {
                        USER_ERROR ( xcTableOpenFailed, "Cannot open accession '%s' as a reference table.", spec );
                    }
                    else
                    {
                        ref -> curs = NGS_CursorMake ( ctx, ref -> tbl, g_ReferenceTableColumnNames, reference_NUM_COLS );
                        if ( ref -> curs != NULL )
                        {
                            uint64_t row_count = 0;
                            TRY ( NGS_CursorGetRowRange ( ref->curs, ctx, & ref -> first_row, & row_count ) )
                            {
                                ref -> last_row = ref -> first_row + (int64_t) row_count - 1; /* TODO: it might be incorrect in general case */
                                TRY ( ref -> chunk_size = NGS_CursorGetUInt32 ( ref -> curs, ctx, ref -> first_row, reference_MAX_SEQ_LEN ) )
                                {
                                    return (NGS_ReferenceSequence*) ref;
                                }
                            }
                        }
                    }
                }
            }
            SRA_ReferenceSequenceWhack ( ref , ctx );
        }
        free ( ref );
    }
    return NULL;
}


int64_t SRA_ReferenceSequence_GetFirstRowId ( const struct NGS_ReferenceSequence * self, ctx_t ctx )
{
    assert ( ( void * ) self -> dad . vt == ( void * ) & SRA_ReferenceSequence_vt_inst );
    return ( ( SRA_ReferenceSequence const * ) self ) -> first_row;
}

int64_t SRA_ReferenceSequence_GetLastRowId ( const struct NGS_ReferenceSequence * self, ctx_t ctx )
{
    assert ( ( void * ) self -> dad . vt == ( void * ) & SRA_ReferenceSequence_vt_inst );
    return ( ( SRA_ReferenceSequence const * ) self ) -> last_row;
}

NGS_String * SRA_ReferenceSequenceGetCanonicalName ( SRA_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );

    return NGS_CursorGetString ( self -> curs, ctx, self -> first_row, reference_SEQ_ID);
}

bool SRA_ReferenceSequenceGetIsCircular ( const SRA_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );

    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }

    /* if current row is valid, read data */
    if ( self -> first_row <= self -> last_row )
    {
        return NGS_CursorGetBool ( self -> curs, ctx, self -> first_row, reference_CIRCULAR );
    }

    return false;
}

uint64_t SRA_ReferenceSequenceGetLength ( SRA_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return 0;
    }

    if ( self -> cur_length == 0 ) /* not yet calculated */
    {
        self -> cur_length =  self -> chunk_size * ( self -> last_row - self -> first_row ) +
                              NGS_CursorGetUInt32 ( self -> curs,
                                                    ctx,
                                                    self -> last_row,
                                                    reference_SEQ_LEN );
    }

    return self -> cur_length;
}

struct NGS_String * SRA_ReferenceSequenceGetBases ( SRA_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }

    {
        uint64_t totalBases = SRA_ReferenceSequenceGetLength ( self, ctx );
        if ( offset >= totalBases )
        {
            return NGS_StringMake ( ctx, "", 0 );
        }
        else
        {
            uint64_t basesToReturn = totalBases - offset;
            char* data;

            if (size != (size_t)-1 && basesToReturn > size)
                basesToReturn = size;

            data = (char*) malloc ( basesToReturn );
            if ( data == NULL )
            {
                SYSTEM_ERROR ( xcNoMemory, "allocating %lu bases", basesToReturn );
                return NGS_StringMake ( ctx, "", 0 );
            }
            else
            {
                size_t cur_offset = 0;
                while ( cur_offset < basesToReturn )
                {
                    /* we will potentially ask for more than available in the current chunk;
                    SRA_ReferenceSequenceGetChunkSize will return only as much as is available in the chunk */
                    NGS_String* chunk = SRA_ReferenceSequenceGetChunk ( self, ctx, offset + cur_offset, basesToReturn - cur_offset );
                    cur_offset += string_copy(data + cur_offset, basesToReturn - cur_offset,
                        NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) );
                    NGS_StringRelease ( chunk, ctx );
                }
                return NGS_StringMakeOwned ( ctx, data, basesToReturn );
            }
        }
    }
}

struct NGS_String * SRA_ReferenceSequenceGetChunk ( SRA_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }

    if ( offset >= SRA_ReferenceSequenceGetLength ( self, ctx ) )
    {
        return NGS_StringMake ( ctx, "", 0 );
    }
    else
    {
        const NGS_String* read = NGS_CursorGetString ( self -> curs, ctx, self -> first_row + offset / self -> chunk_size, reference_READ);
        NGS_String* ret;
        if ( size == (size_t)-1 )
            ret = NGS_StringSubstrOffset ( read, ctx, offset % self -> chunk_size );
        else
            ret = NGS_StringSubstrOffsetSize ( read, ctx, offset % self -> chunk_size, size );
        NGS_StringRelease ( read, ctx );
        return ret;
    }
}
