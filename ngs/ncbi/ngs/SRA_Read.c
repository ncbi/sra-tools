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

#include "SRA_Read.h"

#include "NGS_String.h"
#include "NGS_Cursor.h"
#include "NGS_Id.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/refcount.h>
#include <klib/rc.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>
#include <insdc/insdc.h>

#include <stddef.h>
#include <assert.h>

#include <sysalloc.h>

#ifndef min
#   define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

/*--------------------------------------------------------------------------
 * SRA_Read
 */

const char * sequence_col_specs [] =
{
    "(INSDC:dna:text)READ",
    "READ_TYPE",
    "(INSDC:quality:phred)QUALITY",
    "(INSDC:coord:len)READ_LEN",
    "NAME",
    "SPOT_GROUP",
    "PRIMARY_ALIGNMENT_ID",
    "(U64)SPOT_COUNT",
    "(INSDC:dna:text)CMP_READ",
};

static NGS_Read_vt NGS_Read_vt_inst =
{
    {
        {
            /* NGS_Refcount */
            SRA_ReadWhack
        },

        /* NGS_Fragment */
        SRA_FragmentGetId,
        SRA_FragmentGetSequence,
        SRA_FragmentGetQualities,
        SRA_FragmentIsPaired,
        SRA_FragmentIsAligned,
        SRA_FragmentNext
    },

    /* NGS_Read */
    SRA_ReadGetId,
    SRA_ReadGetName,
    SRA_ReadGetReadGroup,
    SRA_ReadGetCategory,
    SRA_ReadGetSequence,
    SRA_ReadGetQualities,
    SRA_ReadNumFragments,
    SRA_ReadFragIsAligned,
    SRA_ReadIteratorNext,
    SRA_ReadIteratorGetCount,
};

/* Init
 */
static
void SRA_ReadInit ( ctx_t ctx, SRA_Read * self, const char *clsname, const char *instname, const NGS_String * run_name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    if ( self == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_ReadInit ( ctx, & self -> dad, & NGS_Read_vt_inst, clsname, instname ) )
        {
            TRY ( self -> run_name = NGS_StringDuplicate ( run_name, ctx ) )
            {
                self -> wants_full      = true;
                self -> wants_partial   = true;
                self -> wants_unaligned = true;
            }
        }
    }
}

static
void SRA_ReadIteratorInit ( ctx_t ctx,
                            SRA_Read * self,
                            const char *clsname,
                            const char *instname,
                            const NGS_String * run_name,
                            bool wants_full,
                            bool wants_partial,
                            bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    if ( self == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_ReadIteratorInit ( ctx, & self -> dad, & NGS_Read_vt_inst, clsname, instname ) )
        {
            TRY ( self -> run_name = NGS_StringDuplicate ( run_name, ctx ) )
            {
                self -> wants_full      = wants_full;
                self -> wants_partial   = wants_partial;
                self -> wants_unaligned = wants_unaligned;
            }
        }
    }
}

/* Whack
 */
void SRA_ReadWhack ( SRA_Read * self, ctx_t ctx )
{
    NGS_CursorRelease ( self -> curs, ctx );

    NGS_StringRelease ( self -> group_name, ctx );
    NGS_StringRelease ( self -> run_name, ctx );
}

/* Release
 *  release reference
 */
void SRA_ReadRelease ( const SRA_Read * self, ctx_t ctx )
{
    if ( self != NULL )
    {
        NGS_ReadRelease ( & self -> dad, ctx );
    }
}

/* Duplicate
 *  duplicate reference
 */
SRA_Read * SRA_ReadDuplicate ( const SRA_Read * self, ctx_t ctx )
{
    return NGS_RefcountDuplicate ( NGS_ReadToRefcount ( & self -> dad ), ctx );
}

void SRA_ReadIteratorInitFragment ( SRA_Read * self, ctx_t ctx )
{
    const void * base;
    uint32_t elem_bits, boff, row_len;

    /* read from READ_TYPE must succeed */
    TRY ( NGS_CursorCellDataDirect ( self -> curs, ctx, self -> cur_row, seq_READ_TYPE, & elem_bits, & base, & boff, & row_len ) )
    {
        assert ( elem_bits == 8 );
        assert ( boff == 0 );
        self -> READ_TYPE = base;

        TRY ( NGS_CursorCellDataDirect ( self -> curs, ctx, self -> cur_row, seq_READ_LEN, & elem_bits, & base, & boff, & row_len ) )
        {
            uint32_t i;

            assert ( elem_bits == 32 );
            assert ( boff == 0 );
            self -> READ_LEN = base;
            self -> frag_max = row_len;

            /* naked hackery to quickly scan types */
            assert ( READ_TYPE_TECHNICAL == 0 );
            assert ( READ_TYPE_BIOLOGICAL == 1 );

            /* NB - should also be taking READ_FILTER into account */
            for ( i = 0; i < row_len; ++ i )
            {
                if ( self -> READ_LEN [ i ] != 0 )
                {
                    self -> bio_frags += self -> READ_TYPE [ i ] & READ_TYPE_BIOLOGICAL;
                }
            }
        }
    }
}

NGS_Read * SRA_ReadMake ( ctx_t ctx, const NGS_Cursor * curs, int64_t readId, const struct NGS_String * run_name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    SRA_Read * ref;

    assert ( curs != NULL );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating SRA_Read(%lu) on '%.*s'", readId, NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s(%lu)", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ), readId );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( SRA_ReadInit ( ctx, ref, "SRA_Read", instname, run_name ) )
        {
            uint64_t row_count = NGS_CursorGetRowCount ( curs, ctx );

            /* validate the requested rowId and seek to it */
            if ( readId <= 0 || (uint64_t)readId > row_count )
            {
                INTERNAL_ERROR ( xcCursorAccessFailed, "rowId ( %li ) out of range for %.*s", readId, NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
            }
            else
            {
                ref -> curs = NGS_CursorDuplicate ( curs, ctx );
                ref -> cur_row = readId;
                TRY ( SRA_ReadIteratorInitFragment ( ref, ctx ) )
                {
                    ref -> row_max = readId + 1;
                    ref -> row_count = 1;
                    ref -> seen_first = true;
                    return & ref -> dad;
                }
            }

            SRA_ReadRelease ( ref, ctx );
            return NULL;
        }
        free ( ref );
    }

    return NULL;
}

/* GetReadId
 */
NGS_String * SRA_ReadGetId ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return NULL;
    }

    /* if current row is valid, read data */
    if ( self -> cur_row < self -> row_max )
        return NGS_IdMake ( ctx, self -> run_name, NGSObject_Read, self -> cur_row );

    USER_ERROR ( xcCursorExhausted, "No more rows available" );
    return NULL;
}

/* GetReadName
 */
NGS_String * SRA_ReadGetName ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return NULL;
    }
    else
    {
        NGS_String * ret;
        ON_FAIL ( ret = NGS_CursorGetString( self -> curs, ctx, self -> cur_row, seq_NAME ) )
        {
            if ( GetRCObject ( ctx -> rc ) == (int)rcColumn && GetRCState ( ctx -> rc ) == rcNotFound )
            {   /* no NAME column; synthesize a read name based on run_name and row_id */
                CLEAR ();
                ret = NGS_IdMake ( ctx, self -> run_name, NGSObject_Read, self -> cur_row );
            }
        }

        return ret;
    }
}

/* GetReadGroup
 */
NGS_String * SRA_ReadGetReadGroup ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return NULL;
    }

    return NGS_CursorGetString ( self -> curs, ctx, self -> cur_row, seq_GROUP );
}

enum NGS_ReadCategory SRA_ReadGetCategory ( const SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return NGS_ReadCategory_unaligned;
    }

    if ( self -> cur_row < self -> row_max )
    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        ON_FAIL ( NGS_CursorCellDataDirect ( self -> curs, ctx, self -> cur_row, seq_PRIMARY_ALIGNMENT_ID, & elem_bits, & base, & boff, & row_len ) )
        {
            CLEAR();
            return NGS_ReadCategory_unaligned;
        }

        {
            uint32_t i;
            bool seen_aligned = false;
            bool seen_unaligned = false;
            const int64_t * orig = base;
            assert(elem_bits == 64);
            for ( i = 0; i < row_len; ++ i )
            {
                if (orig[i] == 0)
                {
                    if (!seen_unaligned)
                        seen_unaligned = true;
                }
                else
                {
                    if (!seen_aligned)
                        seen_aligned = true;
                }
            }
            if (seen_aligned)
            {
                return seen_unaligned ? NGS_ReadCategory_partiallyAligned : NGS_ReadCategory_fullyAligned;
            }
            else
            {   /* no aligned fragments */
                return NGS_ReadCategory_unaligned;
            }
        }
    }
    USER_ERROR ( xcCursorExhausted, "No more rows available" );
    return NGS_ReadCategory_unaligned;
}

/* GetReadSequence
 */
NGS_String * SRA_ReadGetSequence ( SRA_Read * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );
    NGS_String * seq;

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return NULL;
    }

    TRY ( seq = NGS_CursorGetString ( self -> curs, ctx, self -> cur_row, seq_READ ) )
    {
        NGS_String * sub;
        TRY ( sub = NGS_StringSubstrOffsetSize ( seq, ctx, offset, size ) )
        {
            NGS_StringRelease ( seq, ctx );
            seq = sub;
        }
    }

    return seq;
}

/* GetReadQualities
 * GetReadSubQualities
 */
static
NGS_String * GetReadQualities ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    /* if current row is valid, read data */
    if ( self -> cur_row < self -> row_max )
    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self -> curs, ctx, self -> cur_row, seq_QUALITY, & elem_bits, & base, & boff, & row_len ) )
        {
            NGS_String * new_data = NULL;

            assert ( elem_bits == 8 );
            assert ( boff == 0 );

            {   /* convert to ascii-33 */
                char * copy = malloc ( row_len + 1 );
                if ( copy == NULL )
                    SYSTEM_ERROR ( xcNoMemory, "allocating %u bytes for QUALITY row %ld", row_len + 1, self -> cur_row );
                else
                {
                    uint32_t i;
                    const uint8_t * orig = base;
                    for ( i = 0; i < row_len; ++ i )
                        copy [ i ] = ( char ) ( orig [ i ] + 33 );
                    copy [ i ] = 0;

                    new_data = NGS_StringMakeOwned ( ctx, copy, row_len );
                    if ( FAILED () )
                        free ( copy );
                }
            }

            if ( ! FAILED () )
            {
                return new_data;
            }
        }
    }

    return NULL;
}

NGS_String * SRA_ReadGetQualities ( SRA_Read * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );
    NGS_String * qual;

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return NULL;
    }

    TRY ( qual = GetReadQualities ( self, ctx ) )
    {
        NGS_String * sub;
        TRY ( sub = NGS_StringSubstrOffsetSize ( qual, ctx, offset, size ) )
        {
            NGS_StringRelease ( qual, ctx );
            qual = sub;
        }
    }

    return qual;
}

/* NumFragments
 */
uint32_t SRA_ReadNumFragments ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return 0;
    }

    if ( self -> cur_row >= self -> row_max )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }

    return self -> bio_frags;
}

/* FragIsAligned
 */
bool SRA_ReadFragIsAligned ( SRA_Read * self, ctx_t ctx, uint32_t frag_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return 0;
    }

    if ( self -> cur_row >= self -> row_max )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }

    if ( frag_idx >= self -> bio_frags )
    {
        USER_ERROR ( xcIntegerOutOfBounds, "bad fragment index" );
        return false;
    }

    return false;
}

/*--------------------------------------------------------------------------
 * NGS_ReadIterator
 */

/* Make
 */
NGS_Read * SRA_ReadIteratorMake ( ctx_t ctx,
                                        const NGS_Cursor * curs,
                                        const NGS_String * run_name,
                                        bool wants_full,
                                        bool wants_partial,
                                        bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    SRA_Read * ref;

    assert ( curs != NULL );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating NGS_ReadIterator on '%.*s'", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( SRA_ReadIteratorInit ( ctx, ref, "NGS_ReadIterator", instname, run_name, wants_full, wants_partial, wants_unaligned ) )
        {
            ref -> curs = NGS_CursorDuplicate ( curs, ctx );
            TRY ( NGS_CursorGetRowRange ( ref -> curs, ctx, & ref -> cur_row, & ref -> row_count ) )
            {
                ref -> row_max = ref -> cur_row + ref -> row_count;
                return & ref -> dad;
            }
            SRA_ReadRelease ( ref, ctx );
            return NULL;
        }

        free ( ref );
    }

    return NULL;
}

/* MakeRange
 */
NGS_Read * SRA_ReadIteratorMakeRange ( ctx_t ctx,
                                       const NGS_Cursor * curs,
                                       const NGS_String * run_name,
                                       uint64_t first,
                                       uint64_t count,
                                       bool wants_full,
                                       bool wants_partial,
                                       bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    SRA_Read * ref;

    assert ( curs != NULL );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating NGS_ReadIterator on '%.*s'", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( SRA_ReadIteratorInit ( ctx, ref, "NGS_ReadIterator", instname, run_name, wants_full, wants_partial, wants_unaligned ) )
        {
            ref -> curs = NGS_CursorDuplicate ( curs, ctx );
            TRY ( NGS_CursorGetRowRange ( ref -> curs, ctx, & ref -> cur_row, & ref -> row_count ) )
            {
                ref -> row_max = min ( first + count, ref -> cur_row + ref -> row_count );
                ref -> cur_row = first;
                return & ref -> dad;
            }
            SRA_ReadRelease ( ref, ctx );
            return NULL;
        }

        free ( ref );
    }

    return NULL;
}

/* MakeReadGroup
 */
NGS_Read * SRA_ReadIteratorMakeReadGroup ( ctx_t ctx,
                                           const NGS_Cursor * curs,
                                           const NGS_String * run_name,
                                           const NGS_String * group_name,
                                           uint64_t first,
                                           uint64_t count,
                                           bool wants_full,
                                           bool wants_partial,
                                           bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    TRY ( SRA_Read * ref = (SRA_Read*) SRA_ReadIteratorMakeRange ( ctx,
                                                                   curs,
                                                                   run_name,
                                                                   first,
                                                                   count,
                                                                   wants_full,
                                                                   wants_partial,
                                                                   wants_unaligned ) )
    {
        TRY ( ref -> group_name = NGS_StringDuplicate ( group_name, ctx ) )
        {
            return & ref -> dad;
        }
        SRA_ReadWhack ( ref, ctx );
    }
    return NULL;
}

/* Next
 */
bool SRA_ReadIteratorNext ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( self != NULL );

    /* reset fragment */
    self -> seen_first_frag = false;
    self -> seen_last_frag = false;

    self -> cur_frag = 0;
    self -> bio_frags = 0;
    self -> frag_idx = 0;
    self -> frag_max = 0;
    self -> frag_start = 0;
    self -> frag_len = 0;

    self -> READ_TYPE = NULL;
    self -> READ_LEN = NULL;

    if ( self -> seen_first )
    {   /* move to next row */
        ++ self -> cur_row;
    }
    else
    {
        self -> seen_first = true;
    }

    while ( self -> cur_row < self -> row_max )
    {
        /* work the category filter */
        if ( ! self -> wants_full ||
             ! self -> wants_partial ||
             ! self -> wants_unaligned )
        {
            ON_FAIL ( enum NGS_ReadCategory cat = SRA_ReadGetCategory ( self, ctx ) )
                return false;

            if ( ( cat == NGS_ReadCategory_fullyAligned && ! self -> wants_full )
                 ||
                 ( cat == NGS_ReadCategory_partiallyAligned && ! self -> wants_partial )
                 ||
                 ( cat == NGS_ReadCategory_unaligned && ! self -> wants_unaligned ) )
            {
                ++ self -> cur_row;
                continue;
            }
        }

        /* work the read group filter if required */
        if ( self -> group_name != NULL )
        {
            uint32_t size;

            ON_FAIL ( NGS_String* group = NGS_CursorGetString ( self -> curs, ctx, self -> cur_row, seq_GROUP ) )
                return false;

            size = ( uint32_t ) NGS_StringSize ( group, ctx );
            if ( string_cmp ( NGS_StringData ( self -> group_name, ctx ),
                              NGS_StringSize ( self -> group_name, ctx ),
                              NGS_StringData ( group, ctx ),
                              size,
                              size ) != 0 )
            {
                NGS_StringRelease ( group, ctx );
                ++ self -> cur_row;
                continue;
            }
            NGS_StringRelease ( group, ctx );
        }

        TRY ( SRA_ReadIteratorInitFragment ( self, ctx ) )
        {
            return true;
        }

        break;
    }

    return false;
}

/* GetCount
 *  TEMPORARY
 */
uint64_t SRA_ReadIteratorGetCount ( const SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( self != NULL );
    return self -> row_count;
}

NGS_String * SRA_FragmentGetId ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( self != NULL );
    if ( ! self -> seen_first_frag )
    {
        USER_ERROR ( xcIteratorUninitialized, "Fragment accessed before a call to FragmentIteratorNext()" );
        return NULL;
    }
    if ( self -> seen_last_frag )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }

    return NGS_IdMakeFragment ( ctx, self -> run_name, false, self -> cur_row, self -> cur_frag );
}

static
NGS_String * GetFragmentString ( const SRA_Read * self, ctx_t ctx, NGS_String * str )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );

    assert ( self != NULL );
    if ( ! self -> seen_first_frag )
    {
        USER_ERROR ( xcIteratorUninitialized, "Fragment accessed before a call to FragmentIteratorNext()" );
        return NULL;
    }
    if ( self -> seen_last_frag )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }

    if ( self -> cur_row < self -> row_max )
    {
        TRY ( NGS_String * frag = NGS_StringSubstrOffsetSize ( str, ctx, self -> frag_start, self -> frag_len ) )
        {
            return frag;
        }
    }

    return NULL;
}

struct NGS_String * SRA_FragmentGetSequence ( SRA_Read * self, ctx_t ctx, uint64_t offset, uint64_t length )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );
    NGS_String * ret = NULL;

    assert ( self != NULL );
    if ( ! self -> seen_first_frag )
    {
        USER_ERROR ( xcIteratorUninitialized, "Fragment accessed before a call to FragmentIteratorNext()" );
        return NULL;
    }
    if ( self -> seen_last_frag )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }

    {
        TRY ( NGS_String * read = NGS_CursorGetString ( self -> curs, ctx, self -> cur_row, seq_READ ) )
        {
            TRY ( NGS_String * seq = GetFragmentString ( self, ctx, read ) )
            {
                ret = NGS_StringSubstrOffsetSize ( seq, ctx, offset, length );
                NGS_StringRelease ( seq, ctx );
            }
            NGS_StringRelease ( read, ctx );
        }
    }
    return ret;
}

struct NGS_String * SRA_FragmentGetQualities ( SRA_Read * self, ctx_t ctx, uint64_t offset, uint64_t length )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );
    NGS_String * ret = NULL;

    assert ( self != NULL );
    if ( ! self -> seen_first_frag )
    {
        USER_ERROR ( xcIteratorUninitialized, "Fragment accessed before a call to FragmentIteratorNext()" );
        return NULL;
    }
    if ( self -> seen_last_frag )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }

    {
        TRY ( NGS_String * readQual = GetReadQualities ( self, ctx ) )
        {
            TRY ( NGS_String * fragQual = GetFragmentString ( self, ctx, readQual ) )
            {
                ret = NGS_StringSubstrOffsetSize ( fragQual, ctx, offset, length );
                NGS_StringRelease ( fragQual, ctx );
            }
            NGS_StringRelease ( readQual, ctx );
        }
    }
    return ret;
}

bool SRA_FragmentIsPaired ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    assert ( self != NULL );
    if ( ! self -> seen_first_frag )
    {
        USER_ERROR ( xcIteratorUninitialized, "Fragment accessed before a call to FragmentIteratorNext()" );
        return false;
    }
    if ( self -> seen_last_frag )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }

    return self -> bio_frags > 1;
}

bool SRA_FragmentIsAligned ( SRA_Read * self, ctx_t ctx )
{
    assert ( self != NULL );
    return false;
}

bool SRA_FragmentNext ( SRA_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to ReadIteratorNext()" );
        return false;
    }

    if ( self -> seen_first_frag )
    {   /* move to next fragment */
        ++ self -> cur_frag;
        ++ self -> frag_idx;
    }

    /* advance to next non-empty biological fragment */
    for ( self -> seen_first_frag = true; self -> frag_idx < self -> frag_max; ++ self -> frag_idx )
    {
        if ( self -> READ_LEN [ self -> frag_idx ] != 0 )
        {
            /* get coordinates */
            self -> frag_start += self -> frag_len;
            self -> frag_len = self -> READ_LEN [ self -> frag_idx ];

            /* test for biological fragment */
            assert ( READ_TYPE_TECHNICAL == 0 );
            assert ( READ_TYPE_BIOLOGICAL == 1 );
            if ( ( self -> READ_TYPE [ self -> frag_idx ] & READ_TYPE_BIOLOGICAL ) != 0 )
                return true;
        }
    }

    self -> seen_last_frag = true;
    return false;
}

