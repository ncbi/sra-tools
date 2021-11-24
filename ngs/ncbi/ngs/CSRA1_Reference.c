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

#include "CSRA1_Reference.h"

typedef struct CSRA1_Reference CSRA1_Reference;
#define NGS_REFERENCE CSRA1_Reference
#include "NGS_Reference.h"

#include "NGS_ReadCollection.h"
#include "NGS_Alignment.h"
#include "NGS_ReferenceBlobIterator.h"

#include "NGS_String.h"
#include "NGS_Cursor.h"

#include "SRA_Statistics.h"

#include "CSRA1_ReferenceWindow.h"
#include "CSRA1_Pileup.h"

#include "VByteBlob.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/refcount.h>

#include <vdb/table.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>
#include <vdb/blob.h>
#include "../vdb/blob-priv.h"

#include <stddef.h>
#include <assert.h>

#include <strtol.h>
#include <string.h>

#include <sysalloc.h>

/*--------------------------------------------------------------------------
 * CSRA1_Reference
 */
static void                     CSRA1_ReferenceWhack ( CSRA1_Reference * self, ctx_t ctx );
static NGS_String *             CSRA1_ReferenceGetCommonName ( CSRA1_Reference * self, ctx_t ctx );
static NGS_String *             CSRA1_ReferenceGetCanonicalName ( CSRA1_Reference * self, ctx_t ctx );
static bool                     CSRA1_ReferenceGetIsCircular ( const CSRA1_Reference * self, ctx_t ctx );
static uint64_t                 CSRA1_ReferenceGetLength ( CSRA1_Reference * self, ctx_t ctx );
static struct NGS_String *      CSRA1_ReferenceGetBases ( CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size );
static struct NGS_String *      CSRA1_ReferenceGetChunk ( CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size );
static struct NGS_Alignment*    CSRA1_ReferenceGetAlignment ( CSRA1_Reference * self, ctx_t ctx, const char * alignmentId );
static struct NGS_Alignment*    CSRA1_ReferenceGetAlignments ( CSRA1_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
static uint64_t                 CSRA1_ReferenceGetAlignmentCount ( const CSRA1_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary );
static struct NGS_Alignment*    CSRA1_ReferenceGetAlignmentSlice ( CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size,
    bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
static struct NGS_Pileup*       CSRA1_ReferenceGetPileups ( CSRA1_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
static struct NGS_Pileup*       CSRA1_ReferenceGetPileupSlice ( CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual );
struct NGS_Statistics*          CSRA1_ReferenceGetStatistics ( const CSRA1_Reference * self, ctx_t ctx );
static bool                     CSRA1_ReferenceGetIsLocal ( const CSRA1_Reference * self, ctx_t ctx );
static struct NGS_ReferenceBlobIterator*  CSRA1_ReferenceGetBlobs ( const CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size );
static bool                     CSRA1_ReferenceIteratorNext ( CSRA1_Reference * self, ctx_t ctx );

static NGS_Reference_vt CSRA1_Reference_vt_inst =
{
    /* NGS_Refcount */
    { CSRA1_ReferenceWhack },

    /* NGS_Reference */
    CSRA1_ReferenceGetCommonName,
    CSRA1_ReferenceGetCanonicalName,
    CSRA1_ReferenceGetIsCircular,
    CSRA1_ReferenceGetLength,
    CSRA1_ReferenceGetBases,
    CSRA1_ReferenceGetChunk,
    CSRA1_ReferenceGetAlignment,
    CSRA1_ReferenceGetAlignments,
    CSRA1_ReferenceGetAlignmentCount,
    CSRA1_ReferenceGetAlignmentSlice,
    CSRA1_ReferenceGetPileups,
    CSRA1_ReferenceGetPileupSlice,
    CSRA1_ReferenceGetStatistics,
    CSRA1_ReferenceGetIsLocal,
    CSRA1_ReferenceGetBlobs,

    /* NGS_ReferenceIterator */
    CSRA1_ReferenceIteratorNext,
};

struct CSRA1_Reference
{
    NGS_Reference dad;

    uint32_t chunk_size;

    int64_t first_row;
    int64_t last_row;  /* inclusive */
    const struct VDatabase * db; /* pointer to the opened db, cannot be NULL */
    const struct NGS_Cursor * curs; /* can be NULL if created for an empty iterator */
    uint64_t align_id_offset;
    uint64_t cur_length; /* size of current reference in bases (0 = not yet counted) */

    int64_t iteration_row_last; /* 0 = not iterating */

    bool seen_first;
};

int64_t CSRA1_Reference_GetFirstRowId ( const struct NGS_Reference * self, ctx_t ctx )
{
    assert ( ( void * ) self -> dad . vt == ( void * ) & CSRA1_Reference_vt_inst );
    return ( ( CSRA1_Reference const * ) self ) -> first_row;
}

int64_t CSRA1_Reference_GetLastRowId ( const struct NGS_Reference * self, ctx_t ctx )
{
    assert ( ( void * ) self -> dad . vt == ( void * ) & CSRA1_Reference_vt_inst );
    return ( ( CSRA1_Reference const * ) self ) -> last_row;
}

/* Init
 */
static
void CSRA1_ReferenceInit ( ctx_t ctx,
                           CSRA1_Reference * ref,
                           NGS_ReadCollection * coll,
                           const char *clsname,
                           const char *instname,
                           uint64_t align_id_offset )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    if ( ref == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_ReferenceInit ( ctx, & ref -> dad, & CSRA1_Reference_vt_inst, clsname, instname, coll ) )
        {
            ref -> align_id_offset = align_id_offset;
        }
    }
}

/* Whack
 */
static
void CSRA1_ReferenceWhack ( CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcDestroying );

    NGS_CursorRelease ( self -> curs, ctx );

    VDatabaseRelease ( self -> db );
    self -> db = NULL;

    NGS_ReferenceWhack ( & self -> dad, ctx );
}

NGS_String * CSRA1_ReferenceGetCommonName ( CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    return NGS_CursorGetString ( self -> curs, ctx, self -> first_row, reference_NAME );
}

NGS_String * CSRA1_ReferenceGetCanonicalName ( CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    return NGS_CursorGetString ( self -> curs, ctx, self -> first_row, reference_SEQ_ID);
}

bool CSRA1_ReferenceGetIsCircular ( const CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );

    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return false;
    }

    /* if current row is valid, read data */
    if ( self -> first_row <= self -> last_row )
    {
        return NGS_CursorGetBool ( self -> curs, ctx, self -> first_row, reference_CIRCULAR );
    }

    return false;
}

static
uint64_t CountRows ( NGS_Cursor const * curs, ctx_t ctx, uint32_t colIdx, const void* value, uint32_t value_size, int64_t firstRow, uint64_t end_row)
{   /* count consecutive rows having the same value in column # colIdx as in firstRow, starting from and including firstRow */
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    const void* last_value = value;
    uint64_t cur_row = (uint64_t)firstRow + 1;
    while (cur_row < end_row)
    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        ON_FAIL ( NGS_CursorCellDataDirect ( curs, ctx, cur_row, colIdx, & elem_bits, & base, & boff, & row_len ) )
            return 0;

        if (base != last_value)
        {
            if ( value_size != row_len || memcmp ( base, last_value, row_len ) != 0 )
                break;

            last_value = base;
        }

        ++ cur_row;
    }
    return cur_row - firstRow;
}

uint64_t CSRA1_ReferenceGetLength ( CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return 0;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
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

struct NGS_String * CSRA1_ReferenceGetBases ( CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    {
        uint64_t totalBases = CSRA1_ReferenceGetLength ( self, ctx );
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
                    /* we will potentially ask for more than available in the current blob;
                        CSRA1_ReferenceGetChunk will return only as much as is available in the blob */
                    NGS_String* chunk;
                    ON_FAIL ( chunk = CSRA1_ReferenceGetChunk ( self, ctx, offset + cur_offset, basesToReturn - cur_offset ) )
                    {
                        free ( data );
                        return NULL;
                    }
                    cur_offset += string_copy ( data + cur_offset, basesToReturn - cur_offset,
                                                NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) );
                    NGS_StringRelease ( chunk, ctx );
                }
                return NGS_StringMakeOwned ( ctx, data, basesToReturn );
            }
        }
    }
}

struct NGS_String * CSRA1_ReferenceGetChunk ( CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{   /* return maximum available contiguous bases starting from the offset */
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    NGS_String* ret = NULL;
    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
    }
    else if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
    }
    else if ( offset >= CSRA1_ReferenceGetLength ( self, ctx ) )
    {
        ret = NGS_StringMake ( ctx, "", 0 );
    }
    else
    {
        uint64_t totalBases = CSRA1_ReferenceGetLength ( self, ctx );
        if ( offset >= totalBases )
        {
            return NGS_StringMake ( ctx, "", 0 );
        }
        else
        {
            int64_t rowId = self -> first_row + offset / self -> chunk_size;
            TRY ( const VBlob* blob = NGS_CursorGetVBlob ( self -> curs, ctx, rowId, reference_READ ) )
            {
                rc_t rc;
                const void* data;
                uint64_t cont_size;
                TRY ( VByteBlob_ContiguousChunk ( blob, ctx, rowId, 0, true, & data, & cont_size, 0 ) ) /* stop at a repeated row */
                {
                    uint64_t offsetInBlob =  offset % self -> chunk_size;
                    if ( size == (uint64_t)-1 || offsetInBlob + size > cont_size )
                    {
                        size = cont_size - offsetInBlob;
                    }
                    if ( offset + size > totalBases )
                    {   /* when requested more bases than there are in the reference, be careful not to return a part of the next reference sitting in the same blob */
                        size = totalBases - offset;
                    }
                    ret = NGS_StringMakeCopy ( ctx, (const char*)data + offsetInBlob, size ); /* have to make a copy since otherwise would have to hold on to the entire blob, with no idea when to release it */
                }

                rc = VBlobRelease ( (VBlob*) blob );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( xcUnexpected, "VBlobRelease() rc = %R", rc );
                }
            }
        }
    }
    return ret;
}

struct NGS_Alignment* CSRA1_ReferenceGetAlignment ( CSRA1_Reference * self, ctx_t ctx, const char * alignmentIdStr )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    {
        TRY ( NGS_Alignment* ref = NGS_ReadCollectionGetAlignment ( self -> dad . coll, ctx, alignmentIdStr ) )
        {
            TRY ( NGS_String * spec = NGS_AlignmentGetReferenceSpec( ref, ctx ) )
            {
                TRY ( NGS_String * commonName = CSRA1_ReferenceGetCommonName ( self, ctx ) )
                {
                    if ( string_cmp( NGS_StringData ( spec, ctx ),
                                     NGS_StringSize ( spec, ctx ),
                                     NGS_StringData ( commonName, ctx ),
                                     NGS_StringSize ( commonName, ctx ),
                                     (uint32_t)NGS_StringSize ( spec, ctx ) ) == 0 )
                    {
                        NGS_StringRelease ( spec, ctx );
                        NGS_StringRelease ( commonName, ctx );
                        return ref;
                    }

                    USER_ERROR ( xcWrongReference,
                                "Requested alignment is on a wrong reference: reference '%.*s', alignment has '%.*s'",
                                NGS_StringSize ( commonName, ctx ), NGS_StringData ( commonName, ctx ),
                                NGS_StringSize ( spec, ctx ), NGS_StringData ( spec, ctx ) );

                    NGS_StringRelease ( commonName, ctx );
                }
                NGS_StringRelease ( spec, ctx );
            }
            NGS_AlignmentRelease ( ref, ctx );
        }
    }
    return NULL;
}

struct NGS_Alignment* CSRA1_ReferenceGetAlignments ( CSRA1_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    {
        TRY ( bool circular = CSRA1_ReferenceGetIsCircular ( self, ctx ) )
        {
            TRY ( uint64_t ref_len = CSRA1_ReferenceGetLength ( self, ctx ) )
            {
                /* wants_with_window does not matter, this is not a slice */
                filters &= ~ NGS_AlignmentFilterBits_start_within_window;

                return CSRA1_ReferenceWindowMake ( ctx,
                                                   self -> dad . coll,
                                                   self -> curs,
                                                   circular,
                                                   ref_len,
                                                   self -> chunk_size,
                                                   self -> first_row,
                                                   self -> first_row,
                                                   self -> last_row + 1,
                                                   0,
                                                   0,
                                                   wants_primary,
                                                   wants_secondary,
                                                   filters,
                                                   map_qual,
                                                   self -> align_id_offset );
            }
        }
    }

    return NULL;
}


uint64_t CSRA1_ReferenceGetAlignmentCount ( const CSRA1_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return 0;
    }

    {
        uint64_t res = 0;
        uint64_t cur_row = self -> first_row;

        while ( cur_row <= self -> last_row )
        {
            const void * base;
            uint32_t elem_bits, boff, row_len;

            if ( wants_primary )
            {
                ON_FAIL ( NGS_CursorCellDataDirect ( self -> curs, ctx, cur_row, reference_PRIMARY_ALIGNMENT_IDS,
                                                     & elem_bits, & base, & boff, & row_len ) )
                    return res;

                res += row_len;
            }

            if ( wants_secondary )
            {
                ON_FAIL ( NGS_CursorCellDataDirect ( self -> curs, ctx, cur_row, reference_SECONDARY_ALIGNMENT_IDS,
                                                     & elem_bits, & base, & boff, & row_len ) )
                    return res;

                res += row_len;
            }

            cur_row ++;
        }
        return res;
    }
}

/*
    Calculate starting reference chunk to cover alignments overlapping with the slice;
    separately for primary and secondary alignments
*/
static
void LoadOverlaps ( CSRA1_Reference * self,
                    ctx_t ctx,
                    uint32_t chunk_size,
                    uint64_t offset,
                    int64_t * primary_begin,
                    int64_t * secondary_begin)
{
    int64_t first_row = self -> first_row + offset / chunk_size;
    uint32_t primary_len;
    uint32_t secondary_len;
    int32_t primary_pos;
    int32_t secondary_pos;
    uint32_t offset_in_chunk = offset % chunk_size;

    {   /*OVERLAP_REF_LEN*/
        const void* base;
        uint32_t elem_bits, boff, row_len;
        ON_FAIL ( NGS_CursorCellDataDirect ( self -> curs, ctx, first_row, reference_OVERLAP_REF_LEN, & elem_bits, & base, & boff, & row_len ) )
        {   /* no overlap columns, apply 10-chunk lookback */
            CLEAR ();
            if ( first_row > 11 )
            {
                *primary_begin =
                *secondary_begin = first_row - 10;
            }
            else
            {
                *primary_begin =
                *secondary_begin = 1;
            }
            return;
        }

        assert ( elem_bits == 32 );
        assert ( boff == 0 );
        assert ( row_len == 3 );

        primary_len     = ( (const uint32_t*)base ) [0];
        secondary_len   = ( (const uint32_t*)base ) [1];
    }

    if (primary_len == 0 && secondary_len == 0)
    {
        *primary_begin = *secondary_begin = first_row;
    }
    else
    {   /*OVERLAP_REF_POS*/
        const void* base;
        uint32_t elem_bits, boff, row_len;
        ON_FAIL( NGS_CursorCellDataDirect ( self -> curs, ctx, first_row, reference_OVERLAP_REF_POS, & elem_bits, & base, & boff, & row_len ) )
            return;

        assert ( elem_bits == 32 );
        assert ( boff == 0 );
        assert ( row_len == 3 );

        primary_pos     = ( (const int32_t*)base ) [0];
        secondary_pos   = ( (const int32_t*)base ) [1];

        if ( primary_len == 0 || primary_len < offset_in_chunk )
        {
            * primary_begin = first_row;
        }
        else
        {
            * primary_begin = self -> first_row + primary_pos / chunk_size;
        }

        if ( secondary_len == 0 || secondary_len < offset_in_chunk )
        {
            * secondary_begin = first_row;
        }
        else
        {
            * secondary_begin = self -> first_row + secondary_pos / chunk_size;
        }
    }
}

struct NGS_Alignment* CSRA1_ReferenceGetAlignmentSlice ( CSRA1_Reference * self,
                                                         ctx_t ctx,
                                                         uint64_t offset,
                                                         uint64_t size,
                                                         bool wants_primary,
                                                         bool wants_secondary,
                                                         uint32_t filters,
                                                         int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    if ( size == 0 )
    {
        NGS_Alignment* ret = NGS_AlignmentMakeNull ( ctx, "", 0 );
        return ret;
    }

    {
        TRY ( bool circular = CSRA1_ReferenceGetIsCircular ( self, ctx ) )
        {
            TRY ( uint64_t ref_len = CSRA1_ReferenceGetLength ( self, ctx ) )
            {
                if ( circular )
                {   /* for a circular reference, always look at the whole of it
                       (to account for alignments that start near the end and overlap with the first chunk) */
                    return CSRA1_ReferenceWindowMake ( ctx,
                                                       self -> dad . coll,
                                                       self -> curs,
                                                       true, /* circular */
                                                       ref_len,
                                                       self -> chunk_size,
                                                       self->first_row, /*primary_begin*/
                                                       self->first_row, /*secondary_begin*/
                                                       self -> last_row + 1,
                                                       offset,
                                                       size,
                                                       wants_primary,
                                                       wants_secondary,
                                                       filters,
                                                       map_qual,
                                                       self -> align_id_offset );
                }
                else
                {   /* for non-circular references, restrict the set of chunks to go through */
                    int64_t primary_begin   = self->first_row;
                    int64_t secondary_begin = self->first_row;

                    /* calculate the row range taking "overlaps" into account */
                    TRY ( LoadOverlaps ( self, ctx, self -> chunk_size, offset, & primary_begin, & secondary_begin ) )
                    {
                        /* calculate the last chunk (same for all types of alignments) */
                        int64_t end = self -> first_row + ( offset + size - 1 ) / self -> chunk_size + 1;
                        if ( end > self -> last_row )
                            end = self -> last_row + 1;

                        return CSRA1_ReferenceWindowMake ( ctx,
                                                           self -> dad . coll,
                                                           self -> curs,
                                                           false,
                                                           ref_len,
                                                           self -> chunk_size,
                                                           primary_begin,
                                                           secondary_begin,
                                                           end,
                                                           offset,
                                                           size,
                                                           wants_primary,
                                                           wants_secondary,
                                                           filters,
                                                           map_qual,
                                                           self -> align_id_offset );
                    }
                }
            }
        }
        return NULL;
    }
}

struct NGS_Pileup* CSRA1_ReferenceGetPileups ( CSRA1_Reference * self, ctx_t ctx, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    return CSRA1_PileupIteratorMake ( ctx,
                                      & self -> dad,
                                      self -> db,
                                      self -> curs,
                                      CSRA1_Reference_GetFirstRowId ( (NGS_Reference const*)self, ctx ),
                                      CSRA1_Reference_GetLastRowId ( (NGS_Reference const*)self, ctx ),
                                      wants_primary,
                                      wants_secondary,
                                      filters,
                                      map_qual );
}

static struct NGS_Pileup* CSRA1_ReferenceGetPileupSlice ( CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size, bool wants_primary, bool wants_secondary, uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }

    return CSRA1_PileupIteratorMakeSlice ( ctx,
                                           & self -> dad,
                                           self -> db,
                                           self -> curs,
                                           CSRA1_Reference_GetFirstRowId ( (NGS_Reference const*)self, ctx ),
                                           CSRA1_Reference_GetLastRowId ( (NGS_Reference const*)self, ctx ),
                                           offset,
                                           size,
                                           wants_primary,
                                           wants_secondary,
                                           filters,
                                           map_qual );
}

struct NGS_Statistics* CSRA1_ReferenceGetStatistics ( const CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );
    /* for now, return an empty stats object */
    return SRA_StatisticsMake ( ctx );
}

bool
CSRA1_ReferenceGetIsLocal ( const CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );
    assert ( self );

    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return false;
    }

    /* if current row is valid, read data */
    if ( self -> first_row <= self -> last_row )
    {
        const void *base;
        uint32_t row_len;
        TRY ( NGS_CursorCellDataDirect ( self -> curs,
                                         ctx,
                                         self -> first_row,
                                         reference_CMP_READ,
                                         NULL,
                                         & base,
                                         NULL,
                                         & row_len ) )
        {
            return row_len != 0;
        }
    }
    return false;
}

struct NGS_ReferenceBlobIterator* CSRA1_ReferenceGetBlobs ( const CSRA1_Reference * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self );
    if ( self -> curs == NULL )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Reference accessed before a call to ReferenceIteratorNext()" );
        return NULL;
    }
    else
    {
        uint64_t startRow = self -> first_row + offset / self -> chunk_size;
        uint64_t lastRow = size == (uint64_t)-1 ?
                                    self -> last_row :
                                    self -> first_row + ( offset + size - 1 ) / self -> chunk_size;
        return NGS_ReferenceBlobIteratorMake ( ctx, self -> curs, self -> first_row, startRow, lastRow );
    }
}

bool CSRA1_ReferenceFind ( NGS_Cursor const * curs, ctx_t ctx, const char * spec, int64_t* firstRow, uint64_t* rowCount )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    int64_t firstRow_local;
    uint64_t rowCount_local;

    if ( firstRow == NULL )
        firstRow = & firstRow_local;

    if ( rowCount == NULL )
        rowCount = & rowCount_local;

    assert ( curs != NULL );
    assert ( spec != NULL );

    {   /* use index on reference name if available */
        TRY ( const VTable* table = NGS_CursorGetTable ( curs, ctx ) )
        {
            const KIndex *index;
            rc_t rc = VTableOpenIndexRead( table, & index, "i_name" );
            VTableRelease( table );
            if ( rc == 0 )
            {
                rc = KIndexFindText ( index, spec, firstRow, rowCount, NULL, NULL );
                KIndexRelease ( index );
                if ( rc == 0 )
                    return true;
            }
        }
    }
    /* index not available - do a table scan */
    if ( ! FAILED () )
    {
        int64_t cur_row;
        int64_t end_row;
        uint64_t total_row_count;

        size_t spec_size = string_size ( spec );

        TRY ( NGS_CursorGetRowRange ( curs, ctx, & cur_row, & total_row_count ) )
        {
            const void * prev_NAME_base = NULL;
            const void * prev_SEQ_ID_base = NULL;
            end_row = cur_row + total_row_count;
            while ( cur_row < end_row )
            {
                const void * base;
                uint32_t elem_bits, boff, row_len;

                /* try NAME */
                ON_FAIL ( NGS_CursorCellDataDirect ( curs, ctx, cur_row, reference_NAME, & elem_bits, & base, & boff, & row_len ) )
                    return false;

                /* if the value has not changed, the base ptr will not be updated */
                if ( prev_NAME_base != base )
                {
                    if ( ( size_t ) row_len == spec_size )
                    {
                        assert ( elem_bits == 8 );
                        assert ( boff == 0 );

                        if ( memcmp ( spec, base, row_len ) == 0 )
                        {
                            *firstRow = cur_row;
                            *rowCount = CountRows( curs, ctx, reference_NAME, base, row_len, * firstRow, end_row );
                            return true;
                        }
                    }

                    prev_NAME_base = base;
                }

                /* try SEQ_ID */
                ON_FAIL ( NGS_CursorCellDataDirect ( curs, ctx, cur_row, reference_SEQ_ID, & elem_bits, & base, & boff, & row_len ) )
                    return false;

                /* if the value has not changed, the base ptr will not be updated */
                if ( prev_SEQ_ID_base != base )
                {
                    if ( ( size_t ) row_len == spec_size )
                    {
                        assert ( elem_bits == 8 );
                        assert ( boff == 0 );

                        if ( memcmp ( spec, base, row_len ) == 0 )
                        {
                            *firstRow = cur_row;
                            *rowCount = CountRows( curs, ctx, reference_SEQ_ID, base, row_len, * firstRow, end_row );
                            return true;
                        }
                    }

                    prev_SEQ_ID_base = base;
                }

                ++cur_row;
            }
        }
    }

    return false;
}

NGS_Reference * CSRA1_ReferenceMake ( ctx_t ctx,
                                      struct NGS_ReadCollection * coll,
                                      const struct VDatabase * db,
                                      const struct NGS_Cursor * curs,
                                      const char * spec,
                                      uint64_t align_id_offset )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( coll != NULL );
    assert ( curs != NULL );

    {
        TRY ( NGS_String * collName = NGS_ReadCollectionGetName ( coll, ctx ) )
        {
            CSRA1_Reference * ref = calloc ( 1, sizeof * ref );
            if ( ref == NULL )
            {
                SYSTEM_ERROR ( xcNoMemory, "allocating CSRA1_Reference(%s) on '%.*s'"
                               , spec
                               , ( uint32_t ) NGS_StringSize ( collName, ctx ), NGS_StringData ( collName, ctx )
                    );
            }
            else
            {
#if _DEBUGGING
                char instname [ 256 ];
                string_printf ( instname, sizeof instname, NULL, "%.*s(%s)"
                               , ( uint32_t ) NGS_StringSize ( collName, ctx ), NGS_StringData ( collName, ctx )
                                , spec );
                instname [ sizeof instname - 1 ] = 0;
#else
                const char *instname = "";
#endif
                TRY ( CSRA1_ReferenceInit ( ctx, ref, coll, "CSRA1_Reference", instname, align_id_offset ) )
                {
                    uint64_t rowCount;

                    ref -> curs = NGS_CursorDuplicate ( curs, ctx );

                    ref -> db = db;
                    VDatabaseAddRef ( ref -> db );


                    /* find requested name */
                    if ( CSRA1_ReferenceFind ( ref -> curs, ctx, spec, & ref -> first_row, & rowCount ) )
                    {
                        TRY ( ref -> chunk_size = NGS_CursorGetUInt32 ( ref -> curs, ctx, ref -> first_row, reference_MAX_SEQ_LEN ) )
                        {
                            ref -> iteration_row_last = 0;
                            ref -> last_row = ref -> first_row + rowCount - 1;
                            ref -> seen_first = true;
                            NGS_StringRelease ( collName, ctx );
                            return ( NGS_Reference * ) ref;
                        }
                    }

                    INTERNAL_ERROR ( xcRowNotFound, "Reference not found ( NAME = %s )", spec );
                    CSRA1_ReferenceWhack ( ref, ctx );
                }

                free ( ref );
            }
            NGS_StringRelease ( collName, ctx );
        }
    }

    return NULL;
}

/*--------------------------------------------------------------------------
 * NGS_ReferenceIterator
 */

/* Make
 */
NGS_Reference * CSRA1_ReferenceIteratorMake ( ctx_t ctx,
                                                    struct NGS_ReadCollection * coll,
                                                    const struct VDatabase * db,
                                                    const struct NGS_Cursor * curs,
                                                    uint64_t align_id_offset )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( coll != NULL );
    assert ( db != NULL );
    assert ( curs != NULL );

    {
        TRY ( NGS_String * collName = NGS_ReadCollectionGetName ( coll, ctx ) )
        {
            CSRA1_Reference * ref = calloc ( 1, sizeof * ref );
            if ( ref == NULL )
            {
                SYSTEM_ERROR ( xcNoMemory, "allocating CSRA1_ReferenceIterator on '%.*s'"
                               , ( uint32_t ) NGS_StringSize ( collName, ctx ), NGS_StringData ( collName, ctx )
                    );
            }
            else
            {
#if _DEBUGGING
                char instname [ 256 ];
                string_printf ( instname, sizeof instname, NULL, "%.*s"
                               , ( uint32_t ) NGS_StringSize ( collName, ctx ), NGS_StringData ( collName, ctx )
                    );
                instname [ sizeof instname - 1 ] = 0;
#else
                const char *instname = "";
#endif
                TRY ( CSRA1_ReferenceInit ( ctx, ref, coll, "CSRA1_Reference", instname, align_id_offset ) )
                {
                    uint64_t row_count;

                    ref -> curs = NGS_CursorDuplicate ( curs, ctx );

                    ref -> db = db;
                    VDatabaseAddRef ( ref -> db );

                    TRY ( NGS_CursorGetRowRange ( ref -> curs, ctx, & ref -> first_row, & row_count ) )
                    {
                        TRY ( ref -> chunk_size = NGS_CursorGetUInt32 ( ref -> curs, ctx, ref -> first_row, reference_MAX_SEQ_LEN ) )
                        {
                            ref -> iteration_row_last = ref -> first_row + row_count - 1;
                            ref -> last_row     = 0; /* will be set by CSRA1_ReferenceIteratorNext*/
                            ref -> seen_first   = false;
                            NGS_StringRelease ( collName, ctx );
                            return & ref -> dad;
                        }
                    }
                    CSRA1_ReferenceWhack ( ref, ctx );
                }

                free ( ref );
            }
            NGS_StringRelease ( collName, ctx );
        }
    }

    return NULL;
}

/* Next
 */
bool CSRA1_ReferenceIteratorNext ( CSRA1_Reference * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );

    if ( self -> curs == NULL  || self -> first_row > self -> iteration_row_last)
        return false; /* iteration over or not initialized */

    self -> cur_length = 0;

    if ( self -> seen_first )
    {   /* skip to the next reference */
        self -> first_row = self -> last_row + 1;
        if ( self -> first_row > self -> iteration_row_last)
        {   /* end of iteration */
            self -> last_row = self -> first_row;
            return false;
        }
    }
    else
    {   /* first reference */
        self -> seen_first = true;
    }

    {   /* update self -> last_row */
        const void * refNameBase = NULL;
        uint32_t nameLength;

        {   /* get the new reference's name */
            uint32_t elem_bits, boff;
            ON_FAIL ( NGS_CursorCellDataDirect ( self -> curs, ctx, self -> first_row, reference_NAME,
                                                 & elem_bits, & refNameBase, & boff, & nameLength ) )
                return false;
            assert ( elem_bits == 8 );
            assert ( boff == 0 );
        }

        {   /* use index on reference name if available */
            uint64_t rowCount;
            rc_t rc = 1; /* != 0 in case the following TRY fails */
            TRY ( const VTable* table = NGS_CursorGetTable ( self -> curs, ctx ) )
            {
                const KIndex *index;
                rc = VTableOpenIndexRead( table, & index, "i_name" );
                VTableRelease( table );
                if ( rc == 0 )
                {
                    char* key = string_dup ( ( const char * ) refNameBase, nameLength );
                    int64_t firstRow;
                    rc = KIndexFindText ( index, key, & firstRow, & rowCount, NULL, NULL );
                    assert ( firstRow == self -> first_row );
                    KIndexRelease ( index );
                    free ( key );
                }
            }

            CLEAR();

            if ( rc != 0 )
            {   /* index is not available, do a table scan */
                rowCount = CountRows ( self -> curs, ctx, reference_NAME, refNameBase, nameLength, self -> first_row, self -> iteration_row_last );
            }

            self -> last_row = self -> first_row + rowCount - 1;
        }
    }

    return true;
}
