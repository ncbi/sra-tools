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

/**
* Unit tests for NGS C interface, ReadCollection-related functions, CSRA1 archives
*/

#include "ngs_c_fixture.hpp"

#include <../ncbi/ngs/CSRA1_ReadCollection.h>

#include <../ncbi/ngs/NGS_Cursor.h>

#include <kapp/main.h>

#include <kdb/manager.h>

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <vdb/manager.h>
#include <vdb/vdb-priv.h>

#include <klib/printf.h>

#include <limits.h>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsCsra1ReadCollectionTestSuite);

const char* CSRA1_PrimaryOnly   = "SRR1063272";
const char* CSRA1_WithSecondary = "SRR833251";

class CSRA1_Fixture : public NGS_C_Fixture
{
public:
    CSRA1_Fixture()
    : m_align(0)
    {
    }
    ~CSRA1_Fixture()
    {
    }

    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if (m_align != 0)
            {
                NGS_AlignmentRelease ( m_align, m_ctx );
            }
        }
        NGS_C_Fixture :: Release ();
    }


    NGS_Alignment*      m_align;
};

// CSRA1_ReadCollection

FIXTURE_TEST_CASE(CSRA1_ReadCollection_Open, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_NOT_NULL(m_coll);
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetName, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_STRING ( CSRA1_PrimaryOnly, NGS_ReadCollectionGetName ( m_coll, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetRead, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    m_read = NGS_ReadCollectionGetRead ( m_coll, ctx, ReadId ( 1 )  . c_str () );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}

const uint64_t GetReadCount_Total       = 2280633;
const uint64_t GetReadCount_Unaligned   =  246555;
const uint64_t GetReadCount_Aligned     = GetReadCount_Total - GetReadCount_Unaligned; // 2034078
const uint64_t GetReadCount_FullyAligned = 1953623;
const uint64_t GetReadCount_PartiallyAligned = GetReadCount_Aligned - GetReadCount_FullyAligned; // 80455

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadCount_All, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_EQ( GetReadCount_Total, NGS_ReadCollectionGetReadCount ( m_coll, ctx, true, true, true ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadCount_Unaligned, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_EQ( GetReadCount_Unaligned, NGS_ReadCollectionGetReadCount ( m_coll, ctx, false, false, true ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadCount_Aligned, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_EQ( GetReadCount_Aligned, NGS_ReadCollectionGetReadCount ( m_coll, ctx, true, true, false ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadCount_FullyAligned, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_EQ( GetReadCount_FullyAligned, NGS_ReadCollectionGetReadCount ( m_coll, ctx, true, false, false ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadCount_PartiallyAligned, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_EQ( GetReadCount_PartiallyAligned, NGS_ReadCollectionGetReadCount ( m_coll, ctx, false, true, false ) );
    EXIT;
}


FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads_All, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads_Aligned, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, false, false);
    REQUIRE ( ! FAILED () && m_read );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( NGS_ReadCategory_fullyAligned, NGS_ReadGetReadCategory ( m_read, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads_Partial, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, false, true, false);
    REQUIRE ( ! FAILED () && m_read );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( NGS_ReadCategory_partiallyAligned, NGS_ReadGetReadCategory ( m_read, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads_Unaligned, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, false, false, true);
    REQUIRE ( ! FAILED () && m_read );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( NGS_ReadCategory_unaligned, NGS_ReadGetReadCategory ( m_read, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads_None, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, false, false, false);
    REQUIRE ( ! FAILED () && m_read );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetReadRange, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 2, 3, true, true, true );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    m_align = NGS_ReadCollectionGetAlignment ( m_coll, ctx, ( string ( CSRA1_PrimaryOnly ) + ".PA.1" ) . c_str () );
    REQUIRE_NOT_NULL ( m_align );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment_IdLow, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    REQUIRE_NULL ( NGS_ReadCollectionGetAlignment ( m_coll, ctx, ( string ( CSRA1_PrimaryOnly ) + ".PA.0" ) . c_str () ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment_Secondary, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_WithSecondary );
    m_align = NGS_ReadCollectionGetAlignment ( m_coll, ctx, ( string ( CSRA1_WithSecondary ) + ".SA.169" ) . c_str () );
    REQUIRE_NOT_NULL ( m_align );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.169", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetReference, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    m_ref = NGS_ReadCollectionGetReference ( m_coll, ctx, "supercont2.1" );
    REQUIRE_NOT_NULL ( m_ref );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetReferences, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    m_ref = NGS_ReadCollectionGetReferences ( m_coll, ctx );
    REQUIRE_NOT_NULL ( m_ref );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetReadGroups, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetReadGroup_NotFound, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    REQUIRE_NULL ( NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "wontfindme" ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetReadGroup_WithGroups_DefaultNotFound, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    REQUIRE_NULL ( NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetReadGroup_WithGroups_Found, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "C1ELY.6");
    REQUIRE_NOT_NULL ( m_readGroup );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionHasReadGroup_NotFound, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    REQUIRE ( ! NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "wontfindme" ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionHasReadGroup_WithGroups_DefaultNotFound, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    REQUIRE ( ! NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "" ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionHasReadGroup_WithGroups_Found, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    REQUIRE ( NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "C1ELY.6") );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetAlignments_All, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_WithSecondary);

    m_align = NGS_ReadCollectionGetAlignments ( m_coll, ctx, true, true );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE ( NGS_AlignmentIsPrimary ( m_align, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetAlignments_FilteredPrimary, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_WithSecondary);

    m_align = NGS_ReadCollectionGetAlignments ( m_coll, ctx, true, false );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE ( NGS_AlignmentIsPrimary ( m_align, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetAlignments_FilteredSecondary, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_WithSecondary);

    m_align = NGS_ReadCollectionGetAlignments ( m_coll, ctx, false, true );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_AlignmentIsPrimary ( m_align, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollectionGetAlignments_FilteredNone, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_WithSecondary);

    m_align = NGS_ReadCollectionGetAlignments ( m_coll, ctx, false, false );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Primary_None, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );
    REQUIRE_EQ( (uint64_t)0, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, false, false) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Primary_All, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );
    REQUIRE_EQ( (uint64_t)3987701, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, true, true) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Primary_CategoryPrimary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );
    REQUIRE_EQ( (uint64_t)3987701, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, true, false) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Primary_CategorySecondary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );
    REQUIRE_EQ( (uint64_t)0, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, false, true) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Secondary_All, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );
    REQUIRE_EQ( (uint64_t)178, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, true, true) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Secondary_CategoryPrimary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );
    REQUIRE_EQ( (uint64_t)168, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, true, false) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Secondary_CategorySecondary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );
    REQUIRE_EQ( (uint64_t)10, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, false, true) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_Primary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 3, 2, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.3", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.4", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_PrimarySingle, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 4, 1, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.4", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_PrimaryOutOfRange_CutLow, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 0, 13, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.1", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_PrimaryOutOfRangeHigh, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 3987702, 1, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_PrimaryOutOfRange_CutHigh, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_PrimaryOnly );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 3987701, 2, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.3987701", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_Secondary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 170, 2, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.170", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.171", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}


FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_SecondarySingle, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 174, 1, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.174", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_SecondaryOutOfRangeHigh, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 179, 1, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_SecondaryOutOfRange_CutHigh, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 178, 4, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.178", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_All, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 167, 4, true, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".PA.167", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".PA.168", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.169", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.170", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRangeFiltered_Primary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 167, 4, true, false );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".PA.167", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".PA.168", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRangeFiltered_Secondary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 167, 4, false, true );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.169", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.170", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRangeFiltered_None, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );

    m_align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 167, 4, false, false);
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(NGS_ReadCollection_MakeAlignmentCursor_Reuse, CSRA1_Fixture)
{   // if an exclusive cursor is requested, the currently open cursor is returned if it is not in use
    ENTRY_ACC ( CSRA1_WithSecondary );

    // create and release a non-exclusive cursor
    const NGS_Cursor* c1 = CSRA1_ReadCollectionMakeAlignmentCursor ( ( CSRA1_ReadCollection * ) m_coll, ctx, true, false );
    REQUIRE ( ! FAILED () && c1 );
    NGS_CursorRelease ( c1, ctx );
    REQUIRE ( ! FAILED () );

    // request an exclusive cursor, verify that it is the same one as former c1
    const NGS_Cursor* c2 = CSRA1_ReadCollectionMakeAlignmentCursor ( ( CSRA1_ReadCollection * ) m_coll, ctx, true, true );
    REQUIRE ( ! FAILED () && c2 );
    REQUIRE_EQ ( c1, c2 );

    // request another non-exclusive cursor, make sure it is a new one
    c1 = CSRA1_ReadCollectionMakeAlignmentCursor ( ( CSRA1_ReadCollection * ) m_coll, ctx, true, false );
    REQUIRE ( ! FAILED () && c1 );
    REQUIRE_NE ( c1, c2 );

    NGS_CursorRelease ( c2, ctx );
    REQUIRE ( ! FAILED () );
    NGS_CursorRelease ( c1, ctx );

    EXIT;
}
FIXTURE_TEST_CASE(NGS_ReadCollection_MakeAlignmentCursor_New, CSRA1_Fixture)
{   // if an exclusive cursor is requested, a new cursor is created if the currently open one is in use
    ENTRY_ACC ( CSRA1_WithSecondary );

    // create a non-exclusive cursor
    const NGS_Cursor* c1 = CSRA1_ReadCollectionMakeAlignmentCursor ( ( CSRA1_ReadCollection * ) m_coll, ctx, true, false );
    REQUIRE ( ! FAILED () && c1 );

    // request an exclusive cursor, verify that it is not c1
    const NGS_Cursor* c2 = CSRA1_ReadCollectionMakeAlignmentCursor ( ( CSRA1_ReadCollection * ) m_coll, ctx, true, true );
    REQUIRE ( ! FAILED () && c2 );
    REQUIRE_NE ( c1, c2 );
    NGS_CursorRelease ( c2, ctx );
    REQUIRE ( ! FAILED () );

    // request a non-exclusive cursor, verify that it is the same as c1
    c2 = CSRA1_ReadCollectionMakeAlignmentCursor ( ( CSRA1_ReadCollection * ) m_coll, ctx, true, false );
    REQUIRE ( ! FAILED () && c2 );
    REQUIRE_EQ ( c1, c2 );

    NGS_CursorRelease ( c2, ctx );
    REQUIRE ( ! FAILED () );
    NGS_CursorRelease ( c1, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadCollectionGetStats, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_WithSecondary);

    NGS_Statistics * stats = NGS_ReadCollectionGetStatistics ( m_coll, ctx );
    REQUIRE ( ! FAILED () );

    REQUIRE_EQ ( (uint64_t)615696,  NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)615696,  NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)598728,  NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/CMP_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)3048,    NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)3048,    NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)1,       NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_MIN" ) );

    REQUIRE_EQ ( (uint64_t)3936291, NGS_StatisticsGetAsU64 ( stats, ctx, "REFERENCE/BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)3936291, NGS_StatisticsGetAsU64 ( stats, ctx, "REFERENCE/BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)3936291, NGS_StatisticsGetAsU64 ( stats, ctx, "REFERENCE/CMP_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)788,     NGS_StatisticsGetAsU64 ( stats, ctx, "REFERENCE/SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)788,     NGS_StatisticsGetAsU64 ( stats, ctx, "REFERENCE/SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)1,       NGS_StatisticsGetAsU64 ( stats, ctx, "REFERENCE/SPOT_MIN" ) );

    REQUIRE_EQ ( (uint64_t)16968,   NGS_StatisticsGetAsU64 ( stats, ctx, "PRIMARY_ALIGNMENT/BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)16968,   NGS_StatisticsGetAsU64 ( stats, ctx, "PRIMARY_ALIGNMENT/BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)168,     NGS_StatisticsGetAsU64 ( stats, ctx, "PRIMARY_ALIGNMENT/SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)168,     NGS_StatisticsGetAsU64 ( stats, ctx, "PRIMARY_ALIGNMENT/SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)1,       NGS_StatisticsGetAsU64 ( stats, ctx, "PRIMARY_ALIGNMENT/SPOT_MIN" ) );

    REQUIRE_EQ ( (uint64_t)1010,    NGS_StatisticsGetAsU64 ( stats, ctx, "SECONDARY_ALIGNMENT/BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)1010,    NGS_StatisticsGetAsU64 ( stats, ctx, "SECONDARY_ALIGNMENT/BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)10,      NGS_StatisticsGetAsU64 ( stats, ctx, "SECONDARY_ALIGNMENT/SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)10,      NGS_StatisticsGetAsU64 ( stats, ctx, "SECONDARY_ALIGNMENT/SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)1,       NGS_StatisticsGetAsU64 ( stats, ctx, "SECONDARY_ALIGNMENT/SPOT_MIN" ) );

    NGS_StatisticsRelease ( stats, ctx );

    EXIT;
}

//////////////////////////////////////////// Main
int main(int argc, char* argv[])
{
    KConfigDisableUserSettings();
    int ret=NgsCsra1ReadCollectionTestSuite(argc, argv);
    NGS_C_Fixture::ReleaseCache();
    return ret;
}
