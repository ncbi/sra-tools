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

typedef struct SRA_DB_ReadCollection SRA_DB_ReadCollection;
#define NGS_READCOLLECTION SRA_DB_ReadCollection

#include "NGS_ReadCollection.h"
#include "NGS_Reference.h"
#include "NGS_Alignment.h"
#include "NGS_Read.h"
#include "NGS_FragmentBlobIterator.h"

#include "NGS_Cursor.h"
#include "NGS_String.h"
#include "NGS_Id.h"

#include "SRA_Read.h"
#include "SRA_ReadGroup.h"
#include "SRA_ReadGroupInfo.h"
#include "SRA_Statistics.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <strtol.h> /* strtoi64 */

#include <stddef.h>
#include <assert.h>

#include <sysalloc.h>


/*--------------------------------------------------------------------------
 * SRA_DB_ReadCollection
 */

struct SRA_DB_ReadCollection
{
    NGS_ReadCollection dad;
    const NGS_String * run_name;

    const VDatabase * db;

    const NGS_Cursor* curs; /* used for individual reads */
    const struct SRA_ReadGroupInfo* group_info;
};

static
void SRA_DB_ReadCollectionWhack ( SRA_DB_ReadCollection * self, ctx_t ctx )
{
    NGS_CursorRelease ( self -> curs, ctx );
    SRA_ReadGroupInfoRelease ( self -> group_info, ctx );
    VDatabaseRelease ( self -> db );
    NGS_StringRelease ( self -> run_name, ctx );
}

static
NGS_String * SRA_DB_ReadCollectionGetName ( SRA_DB_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    return NGS_StringDuplicate ( self -> run_name, ctx );
}

static
void GetReadGroupInfo( SRA_DB_ReadCollection * self, ctx_t ctx )
{
    if ( self -> group_info == NULL )
    {
        const VTable * table;
        rc_t rc = VDatabaseOpenTableRead ( self -> db, & table, "SEQUENCE" );
        if ( rc != 0 )
        {
            INTERNAL_ERROR ( xcUnexpected, "VDatabaseOpenTableRead(SEQUENCE) rc = %R", rc );
        }
        else
        {
            self -> group_info = SRA_ReadGroupInfoMake ( ctx, table );
            VTableRelease ( table );
        }
    }
}

static
NGS_ReadGroup * SRA_DB_ReadCollectionGetReadGroups ( SRA_DB_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    TRY ( GetReadGroupInfo( self, ctx ) )
    {
        TRY ( const NGS_Cursor * curs = NGS_CursorMakeDb ( ctx, self -> db, self -> run_name, "SEQUENCE", sequence_col_specs, seq_NUM_COLS ) )
        {
            NGS_ReadGroup * ret = SRA_ReadGroupIteratorMake ( ctx, curs, self -> group_info, self -> run_name );
            NGS_CursorRelease ( curs, ctx );
            return ret;
        }
    }
    return NULL;
}

static
bool SRA_DB_ReadCollectionHasReadGroup ( SRA_DB_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    bool ret = false;

    if ( self -> curs == NULL )
    {
        ON_FAIL ( self -> curs = NGS_CursorMakeDb ( ctx, self -> db, self -> run_name, "SEQUENCE", sequence_col_specs, seq_NUM_COLS ) )
            return ret;
    }
    TRY ( GetReadGroupInfo( self, ctx ) )
    {
        TRY ( SRA_ReadGroupInfoFind ( self -> group_info, ctx, spec, string_size ( spec ) ) )
        {
            ret = true;
        }
        CATCH_ALL()
        {
            CLEAR();
        }
    }
    return ret;
}

static
NGS_ReadGroup * SRA_DB_ReadCollectionGetReadGroup ( SRA_DB_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    if ( self -> curs == NULL )
    {
        ON_FAIL ( self -> curs = NGS_CursorMakeDb ( ctx, self -> db, self -> run_name, "SEQUENCE", sequence_col_specs, seq_NUM_COLS ) )
            return NULL;
    }
    TRY ( GetReadGroupInfo( self, ctx ) )
    {
        NGS_ReadGroup * ret = SRA_ReadGroupMake ( ctx, self -> curs, self -> group_info, self -> run_name, spec, string_size ( spec ));
        return ret;
    }
    return NULL;
}

static
NGS_Reference * SRA_DB_ReadCollectionGetReferences ( SRA_DB_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* create empty reference iterator */
    return NGS_ReferenceMakeNull ( ctx, & self -> dad );
}

static
bool SRA_DB_ReadCollectionHasReference ( SRA_DB_ReadCollection * self, ctx_t ctx, const char * spec )
{
    return false;
}

static
NGS_Reference * SRA_DB_ReadCollectionGetReference ( SRA_DB_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* always fail */
    INTERNAL_ERROR ( xcRowNotFound, "Reference not found ( NAME = %s )", spec );
    return NULL;
}

static
NGS_Alignment * SRA_DB_ReadCollectionGetAlignments ( SRA_DB_ReadCollection * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* create empty alignment iterator */
    return NGS_AlignmentMakeNull ( ctx, NGS_StringData(self -> run_name, ctx), NGS_StringSize(self -> run_name, ctx) );
}

static
NGS_Alignment * SRA_DB_ReadCollectionGetAlignment ( SRA_DB_ReadCollection * self, ctx_t ctx, const char * alignmentId )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* always fail */
    INTERNAL_ERROR ( xcRowNotFound, "Aligment not found ( ID = %ld )", alignmentId );
    return NULL;
}

static
uint64_t SRA_DB_ReadCollectionGetAlignmentCount ( SRA_DB_ReadCollection * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary )
{
    return 0;
}

static
NGS_Alignment * SRA_DB_ReadCollectionGetAlignmentRange ( SRA_DB_ReadCollection * self, ctx_t ctx, uint64_t first, uint64_t count,
    bool wants_primary, bool wants_secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    /* create empty alignment iterator */
    return NGS_AlignmentMakeNull ( ctx, NGS_StringData(self -> run_name, ctx), NGS_StringSize(self -> run_name, ctx) );
}

NGS_Read * SRA_DB_ReadCollectionGetReads ( SRA_DB_ReadCollection * self, ctx_t ctx,
    bool wants_full, bool wants_partial, bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    if ( ! wants_unaligned )
    {
        return NGS_ReadMakeNull ( ctx, self -> run_name );
    }
    else
    {
        TRY ( const NGS_Cursor * curs = NGS_CursorMakeDb ( ctx, self -> db, self -> run_name, "SEQUENCE", sequence_col_specs, seq_NUM_COLS ) )
        {
            NGS_Read * ret =  SRA_ReadIteratorMake ( ctx, curs, self -> run_name, /*wants_full, wants_partial*/ true, true, wants_unaligned );
            NGS_CursorRelease ( curs, ctx );
            return ret;
        }
    }

    return NULL;
}

NGS_Read * SRA_DB_ReadCollectionGetRead ( SRA_DB_ReadCollection * self, ctx_t ctx, const char * readIdStr )
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
                ON_FAIL ( self -> curs = NGS_CursorMakeDb ( ctx, self -> db, self -> run_name, "SEQUENCE", sequence_col_specs, seq_NUM_COLS ) )
                    return NULL;
            }
            return SRA_ReadMake ( ctx, self -> curs, id . rowId, self -> run_name );
        }
    }
    return NULL;
}

static
uint64_t SRA_DB_ReadCollectionGetReadCount ( SRA_DB_ReadCollection * self, ctx_t ctx, bool wants_full, bool wants_partial, bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );

    if ( ! wants_unaligned )
        return 0;

    if ( self -> curs == NULL )
    {
        ON_FAIL ( self -> curs = NGS_CursorMakeDb ( ctx, self -> db, self -> run_name, "SEQUENCE", sequence_col_specs, seq_NUM_COLS ) )
            return 0;
    }

    return NGS_CursorGetRowCount ( self -> curs, ctx );
}

NGS_Read * SRA_DB_ReadCollectionGetReadRange ( SRA_DB_ReadCollection * self, ctx_t ctx, uint64_t first, uint64_t count,
        bool wants_full, bool wants_partial, bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcTable, rcAccessing );

    TRY ( const NGS_Cursor * curs = NGS_CursorMakeDb ( ctx, self -> db, self -> run_name, "SEQUENCE", sequence_col_specs, seq_NUM_COLS ) )
    {
        NGS_Read * ret = SRA_ReadIteratorMakeRange ( ctx, curs, self -> run_name, first, count, /*wants_full, wants_partial*/ true, true, wants_unaligned );
        NGS_CursorRelease ( curs, ctx );
        return ret;
    }
    return NULL;
}


static struct NGS_Statistics* SRADB_ReadCollectionGetStatistics ( SRA_DB_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    const VTable * table;
    rc_t rc = VDatabaseOpenTableRead ( self -> db, & table, "SEQUENCE" );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "VDatabaseOpenTableRead(SEQUENCE) rc = %R", rc );
        return NULL;
    }
    else
    {
        TRY ( NGS_Statistics * ret = SRA_StatisticsMake ( ctx ) )
        {
            TRY ( SRA_StatisticsLoadTableStats ( ret, ctx, table, "SEQUENCE" ) )
            {
                SRA_StatisticsLoadBamHeader ( ret, ctx, self -> db );
                VTableRelease ( table );
                return ret;
            }
            NGS_StatisticsRelease ( ret, ctx );
        }
        VTableRelease ( table );
    }
    return NULL;
}

static struct NGS_FragmentBlobIterator* SRADB_ReadCollectionGetFragmentBlobs ( SRA_DB_ReadCollection * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    const VTable * table;
    rc_t rc = VDatabaseOpenTableRead ( self -> db, & table, "SEQUENCE" );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "VDatabaseOpenTableRead(SEQUENCE) rc = %R", rc );
    }
    else
    {
        TRY ( NGS_FragmentBlobIterator* ret = NGS_FragmentBlobIteratorMake ( ctx, self -> run_name, table ) )
        {
            VTableRelease ( table );
            return ret;
        }
        VTableRelease ( table );
    }

    return NULL;
}

static NGS_ReadCollection_vt SRA_DB_ReadCollection_vt =
{
    /* NGS_Refcount */
    { SRA_DB_ReadCollectionWhack },

    /* NGS_Read_Collection */
    SRA_DB_ReadCollectionGetName,
    SRA_DB_ReadCollectionGetReadGroups,
    SRA_DB_ReadCollectionHasReadGroup,
    SRA_DB_ReadCollectionGetReadGroup,
    SRA_DB_ReadCollectionGetReferences,
    SRA_DB_ReadCollectionHasReference,
    SRA_DB_ReadCollectionGetReference,
    SRA_DB_ReadCollectionGetAlignments,
    SRA_DB_ReadCollectionGetAlignment,
    SRA_DB_ReadCollectionGetAlignmentCount,
    SRA_DB_ReadCollectionGetAlignmentRange,
    SRA_DB_ReadCollectionGetReads,
    SRA_DB_ReadCollectionGetRead,
    SRA_DB_ReadCollectionGetReadCount,
    SRA_DB_ReadCollectionGetReadRange,
    SRADB_ReadCollectionGetStatistics,
    SRADB_ReadCollectionGetFragmentBlobs
};

NGS_ReadCollection * NGS_ReadCollectionMakeVDatabase ( ctx_t ctx, const VDatabase *db, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcConstructing );

    size_t spec_size;
    SRA_DB_ReadCollection * ref;

    assert ( db != NULL );

    assert ( spec != NULL );
    spec_size = string_size ( spec );
    assert ( spec_size != 0 );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating SRA_DB_ReadCollection ( '%s' )", spec );
    else
    {
        TRY ( NGS_ReadCollectionInit ( ctx, & ref -> dad, & SRA_DB_ReadCollection_vt, "SRA_DB_ReadCollection", spec ) )
        {
            const char * name, * dot, * end;

            ref -> db = db;

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

            /* initialize "run_name" */
            TRY ( ref -> run_name = NGS_StringMakeCopy ( ctx, name, end - name ) )
            {
                return & ref -> dad;
            }
        }

        free ( ref );
    }

    VDatabaseRelease ( db );

    return NULL;
}
