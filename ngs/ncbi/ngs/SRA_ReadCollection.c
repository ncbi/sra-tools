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

typedef struct SRA_ReadCollection SRA_ReadCollection;
#define NGS_READCOLLECTION SRA_ReadCollection

#include "NGS_ReadCollection.h"
#include "NGS_Reference.h"
#include "NGS_Alignment.h"
#include "NGS_FragmentBlobIterator.h"

#include "NGS_String.h"
#include "NGS_Cursor.h"
#include "NGS_Id.h"

#include "SRA_Read.h"
#include "SRA_ReadGroup.h"
#include "SRA_ReadGroupInfo.h"
#include "SRA_Statistics.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <vdb/table.h>
#include <klib/text.h>

#include <strtol.h> /* strtoi64 */

#include <stddef.h>
#include <assert.h>

#include <sysalloc.h>


/*--------------------------------------------------------------------------
 * SRA_ReadCollection
 */

struct SRA_ReadCollection
{
    NGS_ReadCollection dad;
    const VTable * tbl;
    const NGS_String * run_name;

    const NGS_Cursor* curs; /* used for individual reads */
    const struct SRA_ReadGroupInfo* group_info;
};

static
void SRA_ReadCollectionWhack ( SRA_ReadCollection * self, ctx_t ctx )
{
    NGS_CursorRelease ( self -> curs, ctx );
    NGS_StringRelease ( self -> run_name, ctx );
    SRA_ReadGroupInfoRelease ( self -> group_info, ctx );
    VTableRelease ( self -> tbl );
}

static
NGS_String * SRA_ReadCollectionGetName ( SRA_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );
    return NGS_StringDuplicate ( self -> run_name, ctx );
}

static
NGS_ReadGroup * SRA_ReadCollectionGetReadGroups ( SRA_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    if ( self -> group_info == NULL )
    {
        ON_FAIL ( self -> group_info = SRA_ReadGroupInfoMake ( ctx, self -> tbl ) )
            return NULL;
    }

    {
        TRY ( const NGS_Cursor * curs = NGS_CursorMake ( ctx, self -> tbl, sequence_col_specs, seq_NUM_COLS ) )
        {
            NGS_ReadGroup * ret = SRA_ReadGroupIteratorMake ( ctx, curs, self -> group_info, self -> run_name );
            NGS_CursorRelease ( curs, ctx );
            return ret;
        }
    }
    return NULL;
}

static
bool SRA_ReadCollectionHasReadGroup ( SRA_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    bool ret = false;

    if ( self -> curs == NULL )
    {
        ON_FAIL ( self -> curs = NGS_CursorMake ( ctx, self -> tbl, sequence_col_specs, seq_NUM_COLS ) )
            return ret;
    }
    if ( self -> group_info == NULL )
    {
        ON_FAIL ( self -> group_info = SRA_ReadGroupInfoMake ( ctx, self -> tbl ) )
            return ret;
    }
    TRY ( SRA_ReadGroupInfoFind ( self -> group_info, ctx, spec, string_size ( spec ) ) )
    {
        ret = true;
    }
    CATCH_ALL()
    {
        CLEAR();
    }
    return ret;
}

static
NGS_ReadGroup * SRA_ReadCollectionGetReadGroup ( SRA_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    if ( self -> curs == NULL )
    {
        ON_FAIL ( self -> curs = NGS_CursorMake ( ctx, self -> tbl, sequence_col_specs, seq_NUM_COLS ) )
            return 0;
    }
    if ( self -> group_info == NULL )
    {
        ON_FAIL ( self -> group_info = SRA_ReadGroupInfoMake ( ctx, self -> tbl ) )
            return NULL;
    }
    {
        TRY ( NGS_ReadGroup * ret =  SRA_ReadGroupMake ( ctx, self -> curs, self -> group_info, self -> run_name, spec, string_size ( spec ) ) )
        {
            return ret;
        }
    }
    return NULL;
}

static
NGS_Reference * SRA_ReadCollectionGetReferences ( SRA_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* create empty reference iterator */
    return NGS_ReferenceMakeNull ( ctx, & self -> dad );
}

static
bool SRA_ReadCollectionHasReference ( SRA_ReadCollection * self, ctx_t ctx, const char * spec )
{
    return false;
}

static
NGS_Reference * SRA_ReadCollectionGetReference ( SRA_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* always fail */
    INTERNAL_ERROR ( xcRowNotFound, "Reference not found ( NAME = %s )", spec );
    return NULL;
}

static
NGS_Alignment * SRA_ReadCollectionGetAlignments ( SRA_ReadCollection * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* create empty alignment iterator */
    return NGS_AlignmentMakeNull ( ctx, NGS_StringData(self -> run_name, ctx), NGS_StringSize(self -> run_name, ctx) );
}

static
NGS_Alignment * SRA_ReadCollectionGetAlignment ( SRA_ReadCollection * self, ctx_t ctx, const char * alignmentId )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* always fail */
    INTERNAL_ERROR ( xcRowNotFound, "Aligment not found ( ID = %ld )", alignmentId );
    return NULL;
}

static
uint64_t SRA_ReadCollectionGetAlignmentCount ( SRA_ReadCollection * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    return 0;
}

static
NGS_Alignment * SRA_ReadCollectionGetAlignmentRange ( SRA_ReadCollection * self, ctx_t ctx, uint64_t first, uint64_t count,
    bool wants_primary, bool wants_secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* create empty alignment iterator */
    return NGS_AlignmentMakeNull ( ctx, NGS_StringData(self -> run_name, ctx), NGS_StringSize(self -> run_name, ctx) );
}

struct NGS_Read * SRA_ReadCollectionGetReads ( SRA_ReadCollection * self, ctx_t ctx,
    bool wants_full, bool wants_partial, bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    if ( ! wants_unaligned )
    {
        return NGS_ReadMakeNull ( ctx, self -> run_name );
    }
    else
    {   /* iterators get their own cursors */
        TRY ( const NGS_Cursor * curs = NGS_CursorMake ( ctx, self -> tbl, sequence_col_specs, seq_NUM_COLS ) )
        {
            NGS_Read * ret = SRA_ReadIteratorMake ( ctx, curs, self -> run_name, /*wants_full, wants_partial*/ true, true, wants_unaligned );
            NGS_CursorRelease ( curs, ctx );
            return ret;
        }
    }

    return NULL;
}

NGS_Read * SRA_ReadCollectionGetRead ( SRA_ReadCollection * self, ctx_t ctx, const char * readIdStr )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    TRY ( struct NGS_Id id = NGS_IdParse ( readIdStr, string_size ( readIdStr ), ctx ) )
    {
        if ( string_cmp ( NGS_StringData ( self -> run_name, ctx ),
            NGS_StringSize ( self -> run_name, ctx ),
            id . run . addr,
            id . run . size,
            id . run . len ) != 0 )
        {
            INTERNAL_ERROR ( xcArcIncorrect,
                " expected '%.*s', actual '%.*s'",
                NGS_StringSize ( self -> run_name, ctx ),
                NGS_StringData ( self -> run_name, ctx ),
                id . run . size,
                id . run . addr );
        }
        else
        {
            /* individual reads share one iterator attached to ReadCollection */
            if ( self -> curs == NULL )
            {
                ON_FAIL ( self -> curs = NGS_CursorMake ( ctx, self -> tbl, sequence_col_specs, seq_NUM_COLS ) )
                    return NULL;
            }
            return SRA_ReadMake ( ctx, self -> curs, id . rowId, self -> run_name );
        }
    }
    return NULL;
}

static
uint64_t SRA_ReadCollectionGetReadCount ( SRA_ReadCollection * self, ctx_t ctx, bool wants_full, bool wants_partial, bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    if ( ! wants_unaligned )
        return 0;

    if ( self -> curs == NULL )
    {
        ON_FAIL ( self -> curs = NGS_CursorMake ( ctx, self -> tbl, sequence_col_specs, seq_NUM_COLS ) )
            return 0;
    }

    return NGS_CursorGetRowCount ( self -> curs, ctx );
}

NGS_Read * SRA_ReadCollectionGetReadRange ( SRA_ReadCollection * self, ctx_t ctx, uint64_t first, uint64_t count,
        bool wants_full, bool wants_partial, bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* iterators get their own cursors */
    TRY ( const NGS_Cursor * curs = NGS_CursorMake ( ctx, self -> tbl, sequence_col_specs, seq_NUM_COLS ) )
    {
        NGS_Read * ret = SRA_ReadIteratorMakeRange ( ctx, curs, self -> run_name, first, count, /*wants_full, wants_partial*/ true, true, wants_unaligned );
        NGS_CursorRelease ( curs, ctx );
        return ret;
    }

    return NULL;
}

static struct NGS_Statistics* SRA_ReadCollectionGetStatistics ( SRA_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( NGS_Statistics * ret = SRA_StatisticsMake ( ctx ) )
    {
        TRY ( SRA_StatisticsLoadTableStats ( ret, ctx, self -> tbl, "SEQUENCE" ) )
        {
            return ret;
        }
        NGS_StatisticsRelease ( ret, ctx );
    }

    return NULL;
}

static struct NGS_FragmentBlobIterator* SRA_ReadCollectionGetFragmentBlobs ( SRA_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( NGS_FragmentBlobIterator* ret = NGS_FragmentBlobIteratorMake ( ctx, self -> run_name, self -> tbl ) )
    {
        return ret;
    }

    return NULL;
}

static NGS_ReadCollection_vt SRA_ReadCollection_vt =
{
    /* NGS_Refcount */
    { SRA_ReadCollectionWhack },

    /* NGS_Read_Collection */
    SRA_ReadCollectionGetName,
    SRA_ReadCollectionGetReadGroups,
    SRA_ReadCollectionHasReadGroup,
    SRA_ReadCollectionGetReadGroup,
    SRA_ReadCollectionGetReferences,
    SRA_ReadCollectionHasReference,
    SRA_ReadCollectionGetReference,
    SRA_ReadCollectionGetAlignments,
    SRA_ReadCollectionGetAlignment,
    SRA_ReadCollectionGetAlignmentCount,
    SRA_ReadCollectionGetAlignmentRange,
    SRA_ReadCollectionGetReads,
    SRA_ReadCollectionGetRead,
    SRA_ReadCollectionGetReadCount,
    SRA_ReadCollectionGetReadRange,
    SRA_ReadCollectionGetStatistics,
    SRA_ReadCollectionGetFragmentBlobs
};

NGS_ReadCollection * NGS_ReadCollectionMakeVTable ( ctx_t ctx, const VTable *tbl, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcConstructing );

    size_t spec_size;
    SRA_ReadCollection * ref;

    assert ( tbl != NULL );

    assert ( spec != NULL );
    spec_size = string_size ( spec );
    assert ( spec_size != 0 );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating SRA_ReadCollection ( '%s' )", spec );
    else
    {
        TRY ( NGS_ReadCollectionInit ( ctx, & ref -> dad, & SRA_ReadCollection_vt, "SRA_ReadCollection", spec ) )
        {
            const char * name, * dot, * end;

            ref -> tbl = tbl;

            end = & spec [ spec_size ];

            /* TBD - this is a hack */
            name = string_rchr ( spec, spec_size, '/' );
            if ( name ++ == NULL )
                name = spec;

            dot = string_rchr ( name, end - name, '.' );
            if ( dot != NULL )
            {
                if ( strcase_cmp ( dot, end - dot, ".ncbi_enc", sizeof ".ncbi_enc" - 1, -1 ) == 0 )
                {
                    end = dot;
                    dot = string_rchr ( name, end - name, '.' );
                }
                if ( dot != NULL && strcase_cmp ( dot, end - dot, ".sra", sizeof ".sra" - 1, -1 ) == 0 )
                    end = dot;
            }

            /* initialize "name" */
            TRY ( ref -> run_name = NGS_StringMakeCopy ( ctx, name, end - name ) )
            {
                return & ref -> dad;
            }
        }

        free ( ref );
    }

    VTableRelease ( tbl );

    return NULL;
}
