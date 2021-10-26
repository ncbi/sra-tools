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

#include "CSRA1_Read.h"

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

static bool CSRA1_ReadIteratorNext ( CSRA1_Read * self, ctx_t ctx );

/*--------------------------------------------------------------------------
 * CSRA1_Read
 */
struct CSRA1_Read
{
    SRA_Read dad;
};

static bool                CSRA1_FragmentIsAligned ( CSRA1_Read * self, ctx_t ctx );
static bool                CSRA1_ReadFragIsAligned ( CSRA1_Read * self, ctx_t ctx, uint32_t frag_idx );

static NGS_Read_vt CSRA1_Read_vt_inst =
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
        CSRA1_FragmentIsAligned,
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
    CSRA1_ReadFragIsAligned,
    CSRA1_ReadIteratorNext,
    SRA_ReadIteratorGetCount,
};

/* Init
 */
static
void CSRA1_ReadInit ( ctx_t ctx, SRA_Read * self, const char *clsname, const char *instname, const NGS_String * run_name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    if ( self == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_ReadInit ( ctx, & self -> dad, & CSRA1_Read_vt_inst, clsname, instname ) )
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

/* Release
 *  release reference
 */
void CSRA1_ReadRelease ( const CSRA1_Read * self, ctx_t ctx )
{
    if ( self != NULL )
    {
        NGS_ReadRelease ( & self -> dad . dad, ctx );
    }
}

/*--------------------------------------------------------------------------
 * NGS_ReadIterator
 */

static
void CSRA1_ReadIteratorInit ( ctx_t ctx,
                             CSRA1_Read * cself,
                             const char *clsname,
                             const char *instname,
                             const NGS_String * run_name,
                             bool wants_full,
                             bool wants_partial,
                             bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    if ( cself == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        SRA_Read * self = & cself -> dad;
        TRY ( NGS_ReadIteratorInit ( ctx, & self -> dad, & CSRA1_Read_vt_inst, clsname, instname ) )
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

/* Make
 */
NGS_Read * CSRA1_ReadIteratorMake ( ctx_t ctx,
                                 const NGS_Cursor * curs,
                                 const NGS_String * run_name,
                                 bool wants_full,
                                 bool wants_partial,
                                 bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    CSRA1_Read * cref;
    SRA_Read * ref;

    assert ( curs != NULL );

    cref = calloc ( 1, sizeof * cref );
    if ( cref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating CSRA1_ReadIterator on '%.*s'", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( CSRA1_ReadIteratorInit ( ctx, cref, "CSRA1_ReadIterator", instname, run_name, wants_full, wants_partial, wants_unaligned ) )
        {
            ref = & cref -> dad;

            ref -> curs = NGS_CursorDuplicate ( curs, ctx );
            TRY ( NGS_CursorGetRowRange ( ref -> curs, ctx, & ref -> cur_row, & ref -> row_count ) )
            {
                ref -> row_max = ref -> cur_row + ref -> row_count;
                return & ref -> dad;
            }
            CSRA1_ReadRelease ( cref, ctx );
            return NULL;
        }

        free ( cref );
    }

    return NULL;
}

/* MakeRange
 */
NGS_Read * CSRA1_ReadIteratorMakeRange ( ctx_t ctx,
                                      const NGS_Cursor * curs,
                                      const NGS_String * run_name,
                                      uint64_t first,
                                      uint64_t count,
                                      bool wants_full,
                                      bool wants_partial,
                                      bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    CSRA1_Read * cref;
    SRA_Read * ref;

    assert ( curs != NULL );

    cref = calloc ( 1, sizeof * ref );
    if ( cref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating CSRA1_ReadIterator on '%.*s'", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( CSRA1_ReadIteratorInit ( ctx, cref, "CSRA1_ReadIterator", instname, run_name, wants_full, wants_partial, wants_unaligned ) )
        {
            ref = & cref -> dad;

            ref -> curs = NGS_CursorDuplicate ( curs, ctx );
            TRY ( NGS_CursorGetRowRange ( ref -> curs, ctx, & ref -> cur_row, & ref -> row_count ) )
            {
                ref -> row_max = min ( first + count, ref -> cur_row + ref -> row_count );
                ref -> cur_row = first;
                return & ref -> dad;
            }
            CSRA1_ReadRelease ( cref, ctx );
            return NULL;
        }

        free ( cref );
    }

    return NULL;
}

/* MakeReadGroup
 */
NGS_Read * CSRA1_ReadIteratorMakeReadGroup ( ctx_t ctx,
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

    TRY ( CSRA1_Read * cref = (CSRA1_Read*) CSRA1_ReadIteratorMakeRange ( ctx,
                                                                  curs,
                                                                  run_name,
                                                                  first,
                                                                  count,
                                                                  wants_full,
                                                                  wants_partial,
                                                                  wants_unaligned ) )
    {
        SRA_Read * ref = & cref -> dad;
        TRY ( ref -> group_name = NGS_StringDuplicate ( group_name, ctx ) )
        {
            return & ref -> dad;
        }

        CSRA1_ReadRelease ( cref, ctx );
    }
    return NULL;
}

NGS_Read * CSRA1_ReadMake ( ctx_t ctx, const NGS_Cursor * curs, int64_t readId, const struct NGS_String * run_name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    CSRA1_Read * cref;
    SRA_Read * ref;

    assert ( curs != NULL );

    cref = calloc ( 1, sizeof * cref );
    if ( cref == NULL )
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
        ref = & cref -> dad;

        TRY ( CSRA1_ReadInit ( ctx, ref, "CSRA1_Read", instname, run_name ) )
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
                TRY ( SRA_ReadIteratorInitFragment ( cref , ctx ) )
                {
                    ref -> row_max = readId + 1;
                    ref -> row_count = 1;
                    ref -> seen_first = true;
                    return & ref -> dad;
                }
            }

            CSRA1_ReadRelease ( cref, ctx );
            return NULL;
        }
        free ( cref );
    }

    return NULL;
}

bool CSRA1_FragmentIsAligned ( CSRA1_Read * cself, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    const SRA_Read * self;

    assert ( cself != NULL );

    self = & cself -> dad;

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to nextRead()" );
        return false;
    }

    if ( self -> cur_row >= self -> row_max )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }

    if ( ! self -> seen_first_frag )
    {
        USER_ERROR ( xcIteratorUninitialized, "Fragment accessed before a call to nextFragment()" );
        return false;
    }

    if ( self -> frag_idx >= self -> frag_max )
    {
        USER_ERROR ( xcCursorExhausted, "No more fragments available" );
        return false;
    }

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        ON_FAIL ( NGS_CursorCellDataDirect ( self -> curs, ctx, self -> cur_row, seq_PRIMARY_ALIGNMENT_ID, & elem_bits, & base, & boff, & row_len ) )
        {
            CLEAR();
            return false;
        }

        {
            const int64_t * orig = base;
            assert(elem_bits == 64);
            assert(boff == 0);

            return orig[self -> frag_idx] != 0;
        }
    }

}

bool CSRA1_ReadFragIsAligned ( CSRA1_Read * cself, ctx_t ctx, uint32_t frag_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    const SRA_Read * self;

    assert ( cself != NULL );

    self = & cself -> dad;

    if ( ! self -> seen_first )
    {
        USER_ERROR ( xcIteratorUninitialized, "Read accessed before a call to nextRead()" );
        return false;
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

    {
        const void * base;
        uint32_t elem_bits, boff, row_len;
        TRY ( NGS_CursorCellDataDirect ( self -> curs, ctx, self -> cur_row, seq_PRIMARY_ALIGNMENT_ID, & elem_bits, & base, & boff, & row_len ) )
        {
            uint32_t idx, bidx;
            const int64_t * orig = base;
            assert ( base != NULL );
            assert ( elem_bits == 64 );
            assert ( boff == 0 );
            assert ( row_len == self -> frag_max );

            /* technically, we do not expect technical reads (fragments) within CSRA1,
               but it is correct to check for this possibility
               same applies to 0-length biological fragments (VDB-3132)
            */
            if ( self -> bio_frags == self -> frag_max )
                return orig [ frag_idx ] != 0;

            for ( idx = bidx = 0; idx < row_len; ++ idx )
            {
                if ( ( self -> READ_TYPE [ idx ] & READ_TYPE_BIOLOGICAL ) != 0 && self -> READ_LEN [ idx ] != 0 )
                {
                    if ( bidx == frag_idx )
                        return orig [ idx ] != 0;

                    ++ bidx;
                }
            }
        }
    }

    CLEAR();
    return false;
}

bool
CSRA1_ReadIteratorNext ( CSRA1_Read * cself, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    SRA_Read * self;

    assert ( cself != NULL );

    self = & cself -> dad;

    if ( self -> wants_full )
    {
        return SRA_ReadIteratorNext ( cself, ctx );
    }

    /* to iterate over partially aligned and unaligned reads, use column CMP_READ */

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
        enum NGS_ReadCategory cat;
        NGS_String * read;
        bool aligned;
        ON_FAIL ( read = NGS_CursorGetString ( self -> curs, ctx, self -> cur_row, seq_CMP_READ ) )
            return false;

        aligned = NGS_StringSize ( read, ctx ) == 0;
        NGS_StringRelease( read, ctx );
        if ( aligned )
        {   // aligned read - skip
            ++ self -> cur_row;
            continue;
        }

        /* work the category filter, we know wants_full is false */
        ON_FAIL ( cat = SRA_ReadGetCategory ( cself, ctx ) )
            return false;

        if ( ( cat == NGS_ReadCategory_partiallyAligned && ! self -> wants_partial )
                ||
             ( cat == NGS_ReadCategory_unaligned && ! self -> wants_unaligned ) )
        {
            ++ self -> cur_row;
            continue;
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

        TRY ( SRA_ReadIteratorInitFragment ( cself, ctx ) )
        {
            return true;
        }

        break;
    }

    return false;
}
