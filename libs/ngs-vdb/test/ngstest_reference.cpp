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
* Unit tests for NGS C interface, reference accessions
*/

#include <klib/debug.h> /* KDbgSetString */
#include "ngs_c_fixture.hpp"

#include <kapp/main.h>

#include <string.h>

#include <ktst/unit_test.hpp>

#include <kfc/xc.h>

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include "../ncbi/ngs/NGS_Pileup.h"
#include "../ncbi/ngs/NGS_ReferenceSequence.h"
#include "../ncbi/ngs/NGS_String.h"
#include "../ncbi/ngs/CSRA1_Reference.h"
#include "../ncbi/ngs/NGS_ReferenceBlobIterator.h"
#include "../ncbi/ngs/NGS_ReferenceBlob.h"

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsReferenceTestSuite);

#define ALL

const char* CSRA1_PrimaryOnly   = "SRR1063272";
const char* CSRA1_WithSecondary = "SRR833251";
const char* CSRA1_WithCircularReference = "SRR1769246";

class CSRA1_ReferenceFixture : public NGS_C_Fixture
{
public:
    CSRA1_ReferenceFixture()
    : m_align(0)
    {
    }
    ~CSRA1_ReferenceFixture()
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
#ifdef ALL
// NGS_Reference
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetCommonName, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1");
    REQUIRE_STRING ( "supercont2.1", NGS_ReferenceGetCommonName ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetCanonicalName, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( "SRR821492", "chr7" );
    const char* canoName = "NC_000007.13";
    REQUIRE_STRING ( canoName, NGS_ReferenceGetCanonicalName ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIsCircular_Yes, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( "SRR821492", "chrM" );
    REQUIRE ( NGS_ReferenceGetIsCircular ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIsCircular_No, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE ( ! NGS_ReferenceGetIsCircular ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetLength, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE_EQ ( (uint64_t)2291499l, NGS_ReferenceGetLength ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetFirstRowId, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE_EQ ( (int64_t)1, CSRA1_Reference_GetFirstRowId ( m_ref, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetLastRowId, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE_EQ ( (int64_t)459, CSRA1_Reference_GetLastRowId ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetBases_Full, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    string bases = toString ( NGS_ReferenceGetBases( m_ref, ctx, 0, -1 ), ctx, true );
    REQUIRE_EQ ( NGS_ReferenceGetLength ( m_ref, ctx ), ( uint64_t ) bases. size () );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetBases, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE_STRING ( "CCTGTCC", NGS_ReferenceGetBases( m_ref, ctx, 7000, 7 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignment_Primary, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    m_align = NGS_ReferenceGetAlignment ( m_ref, ctx, ( string ( CSRA1_PrimaryOnly ) + ".PA.1" ) . c_str () );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.1", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignments_PrimaryOnly_Primary, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    m_align = NGS_ReferenceGetAlignments ( m_ref, ctx, true, false );
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.1", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignments_PrimaryOnly_Secondary, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    m_align = NGS_ReferenceGetAlignments ( m_ref, ctx, false, true );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignments_WithSecondary_Primary, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_WithSecondary, "gi|169794206|ref|NC_010410.1|" );

    m_align = NGS_ReferenceGetAlignments ( m_ref, ctx, true, false);
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".PA.1", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignments_WithSecondary_Secondary, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_WithSecondary, "gi|169794206|ref|NC_010410.1|" );

    m_align = NGS_ReferenceGetAlignments ( m_ref, ctx, false, true );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );

    REQUIRE_STRING ( string ( CSRA1_WithSecondary ) + ".SA.169", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );

    EXIT;
}


// ReferenceGetFilteredAlignments on circular references
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignments_Circular_Wraparound, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_WithCircularReference, "NC_012920.1" );

    const bool wants_primary = true;
    const bool wants_secondary = true;
    const uint32_t no_filters = 0;
    const int32_t no_map_qual = 0;

    m_align = NGS_ReferenceGetFilteredAlignments ( m_ref, ctx, wants_primary, wants_secondary, no_filters, no_map_qual );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );

    // by default, the first returned alignment starts before the start of the circular reference
    REQUIRE_EQ ( (int64_t)16477, NGS_AlignmentGetAlignmentPosition ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignments_Circular_NoWraparound, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_WithCircularReference, "NC_012920.1" );

    const bool wants_primary = true;
    const bool wants_secondary = true;
    const uint32_t filters = NGS_AlignmentFilterBits_no_wraparound;
    const int32_t no_map_qual = 0;

    m_align = NGS_ReferenceGetFilteredAlignments ( m_ref, ctx, wants_primary, wants_secondary, filters, no_map_qual );
    REQUIRE ( ! FAILED () && m_align );
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );

    // with a no-wraparound filter, the first returned alignment starts at/after the start of the circular reference
    REQUIRE_EQ ( (int64_t)5, NGS_AlignmentGetAlignmentPosition ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetAlignmentsSlice_Circular_Wraparound, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_WithCircularReference, "NC_012920.1" );

    const bool wants_primary = true;
    const bool wants_secondary = true;
    const uint32_t no_filters = 0;
    const int32_t no_map_qual = 0;

    m_align = NGS_ReferenceGetFilteredAlignmentSlice ( m_ref, ctx, 16565, 6, wants_primary, wants_secondary, no_filters, no_map_qual ); /* ref lingth = 16569; this slice wraps around */
    REQUIRE ( ! FAILED () && m_align );

    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE_EQ ( (int64_t)16477, NGS_AlignmentGetAlignmentPosition ( m_align, ctx ) ); // SRR1769246.PA.3275490, an actual wraparound
    REQUIRE ( NGS_AlignmentIteratorNext ( m_align, ctx ) );
    REQUIRE_EQ ( (int64_t)16472, NGS_AlignmentGetAlignmentPosition ( m_align, ctx ) ); // SRR1769246.PA.3275487, overlaps with the slice but does not wrap around
    REQUIRE ( ! NGS_AlignmentIteratorNext ( m_align, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetFilteredAlignmentSlice_FullReference_Wraparound_Count, CSRA1_ReferenceFixture )
{
    ENTRY_GET_REF( CSRA1_WithCircularReference, "NC_012920.1" );

    const bool wants_primary = true;
    const bool wants_secondary = true;
    const uint32_t no_filters = 0;
    const int32_t no_map_qual = 0;

    m_align = NGS_ReferenceGetFilteredAlignmentSlice ( m_ref, ctx, 0, NGS_ReferenceGetLength ( m_ref, ctx ), wants_primary, wants_secondary, no_filters, no_map_qual );
    REQUIRE ( ! FAILED () && m_align );

    uint64_t count = 0;
    while ( NGS_AlignmentIteratorNext ( m_align, ctx ) )
    {
        ++count;
    }
    REQUIRE_EQ ( (uint64_t) 12316, count );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetFilteredAlignmentSlice_FullReference_NoWraparound_Count, CSRA1_ReferenceFixture )
{
    ENTRY_GET_REF( CSRA1_WithCircularReference, "NC_012920.1" );

    const bool wants_primary = true;
    const bool wants_secondary = true;
    const uint32_t filters = NGS_AlignmentFilterBits_no_wraparound;
    const int32_t no_map_qual = 0;

    m_align = NGS_ReferenceGetFilteredAlignmentSlice ( m_ref, ctx, 0, NGS_ReferenceGetLength ( m_ref, ctx ), wants_primary, wants_secondary, filters, no_map_qual );
    REQUIRE ( ! FAILED () && m_align );

    int64_t lastPos = 0;
    uint64_t count = 0;
    while ( NGS_AlignmentIteratorNext ( m_align, ctx ) )
    {
        ++count;
        int64_t newPos = NGS_AlignmentGetAlignmentPosition ( m_align, ctx );
        REQUIRE_LE ( lastPos, newPos );
        lastPos = newPos;
    }
    REQUIRE_EQ ( (uint64_t) 12315, count ); // does not include SRR1769246.PA.3275490, the only wraparound

    EXIT;
}

// NGS_ReferenceGetChunk
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetChunk_Empty, CSRA1_ReferenceFixture)
{   // offset beyond the end of reference
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE_STRING ( "", NGS_ReferenceGetChunk ( m_ref, ctx, 30000000, 10) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_All, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    NGS_String * chunk = NGS_ReferenceGetChunk ( m_ref, ctx, 0, (size_t)-1 );
    REQUIRE_EQ( (size_t)20000, NGS_StringSize ( chunk, ctx ) );

    string str = string ( NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) );
    REQUIRE_EQ( string("GAATTCT"), str . substr (0, 7) );
    REQUIRE_EQ( string("CATCA"), str . substr ( str.size() - 5, 5 ) );

    NGS_StringRelease ( chunk, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_Offset_1, CSRA1_ReferenceFixture)
{   // offset points into the first blob of REFERENCE.READ
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    NGS_String * chunk = NGS_ReferenceGetChunk ( m_ref, ctx, 1000, (size_t)-1 );
    REQUIRE_EQ( (size_t)19000, NGS_StringSize ( chunk, ctx ) ); // first blob's size is 20000

    string str = string ( NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) );
    REQUIRE_EQ( string("TCCATTC"), str . substr (0, 7) );
    REQUIRE_EQ( string("CATCA"), str . substr (str.size() - 5, 5) );

    NGS_StringRelease ( chunk, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_Offset_2, CSRA1_ReferenceFixture)
{   // offset points into the second blob of REFERENCE.READ
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    NGS_String * chunk = NGS_ReferenceGetChunk ( m_ref, ctx, 22000, (size_t)-1 );
    REQUIRE_EQ( (size_t)3000, NGS_StringSize ( chunk, ctx ) ); // second blob's size is 5000

    string str = string ( NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) );
    REQUIRE_EQ( string("CTCAGAT"), str . substr (0, 7)  );
    REQUIRE_EQ( string("TATTC"), str . substr (str.size() - 5, 5) );

    NGS_StringRelease ( chunk, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_OffsetLength_1, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_String * chunk = NGS_ReferenceGetChunk ( m_ref, ctx, 2000, 10 );
    REQUIRE_EQ( string ( "GGGCAAATGA" ), string ( NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) ) );
    NGS_StringRelease ( chunk, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_OffsetLength_2, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_String * chunk = NGS_ReferenceGetChunk ( m_ref, ctx, 20020, 10 );
    REQUIRE_EQ( string ( "ACATGACGGA" ), string ( NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) ) );
    NGS_StringRelease ( chunk, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_OffsetLength_ReturnShorter, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_String * chunk = NGS_ReferenceGetChunk ( m_ref, ctx, 19995, 200 ); // only 5 bases left in this blob
    REQUIRE_EQ( string ( "CATCA" ), string ( NGS_StringData ( chunk, ctx ), NGS_StringSize ( chunk, ctx ) ) );
    NGS_StringRelease ( chunk, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Reference_GetReferenceChunk_Chunks_Vs_Blobs, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( "SRR600094", "NC_000022.10" );

    NGS_String * full = NGS_ReferenceGetBases ( m_ref, ctx, 0, (uint64_t)-1 );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( full );
    REQUIRE_EQ ( (uint64_t)NGS_ReferenceGetLength ( m_ref, ctx ), (uint64_t)NGS_StringSize ( full, ctx ) );

    size_t offset=0;
    while ( offset < NGS_StringSize ( full, ctx ) )
    {
        NGS_String * chunk = NGS_ReferenceGetChunk ( m_ref, ctx, offset, 5000 );
        REQUIRE ( ! FAILED () );
        REQUIRE_NOT_NULL ( chunk );
        size_t chunkSize = NGS_StringSize ( chunk, ctx );
        REQUIRE_EQ ( string ( NGS_StringData ( full, ctx ) + offset, chunkSize ), string ( NGS_StringData ( chunk, ctx ), chunkSize ) );
        NGS_StringRelease ( chunk, ctx );
        offset += chunkSize;
    }

    NGS_StringRelease ( full, ctx );

    EXIT;
}

//

FIXTURE_TEST_CASE(CSRA1_NGS_Reference_SharedCursor, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    NGS_Reference* ref2 = NGS_ReadCollectionGetReference ( m_coll, ctx, "supercont2.2" );

    string name = toString ( NGS_ReferenceGetCommonName ( m_ref, ctx), ctx, true );
    string name2 = toString ( NGS_ReferenceGetCommonName ( ref2, ctx), ctx, true );

    REQUIRE_NE ( name, name2 );

    NGS_ReferenceRelease ( ref2, m_ctx );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetStats, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_Statistics * stats = NGS_ReferenceGetStatistics ( m_ref, ctx );
    REQUIRE ( ! FAILED () );

    // Reference stats are empty for now
    const char* path;
    REQUIRE ( ! NGS_StatisticsNextPath ( stats, ctx, "", &path ) );

    NGS_StatisticsRelease ( stats, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetPileups, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF ( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_Pileup* pileup = NGS_ReferenceGetPileups( m_ref, ctx, true, false);
    REQUIRE ( ! FAILED () && pileup );

    NGS_PileupRelease ( pileup, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetPileupSlice, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF ( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_Pileup* pileup = NGS_ReferenceGetPileupSlice( m_ref, ctx, 500, 10, true, false);
    REQUIRE ( ! FAILED () && pileup );

    NGS_PileupRelease ( pileup, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetBlobs, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF ( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_ReferenceBlobIterator* blobIt = NGS_ReferenceGetBlobs( m_ref, ctx, 0, (uint64_t)-1 );
    REQUIRE ( ! FAILED () && blobIt );

    REQUIRE ( NGS_ReferenceBlobIteratorHasMore ( blobIt, ctx ) );
    REQUIRE ( ! FAILED () );
    struct NGS_ReferenceBlob* blob = NGS_ReferenceBlobIteratorNext ( blobIt, ctx );
    REQUIRE ( ! FAILED () && blob );

    int64_t first;
    uint64_t count;
    NGS_ReferenceBlobRowRange ( blob, ctx,  & first, & count );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (int64_t)1, first );
    REQUIRE_EQ ( (uint64_t)4, count );

    NGS_ReferenceBlobRelease ( blob, ctx );
    NGS_ReferenceBlobIteratorRelease ( blobIt, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIsLocal_Yes, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE ( NGS_ReferenceGetIsLocal ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIsLocal_No, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( "SRR821492", "chrM" );
    REQUIRE ( ! NGS_ReferenceGetIsLocal ( m_ref, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIsLocalSRR619505, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF("SRR619505", "NC_000005.8");
    REQUIRE(!NGS_ReferenceGetIsLocal(m_ref, ctx));
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIsLocalSRR413283, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF("SRR413283", "FLT3_NM_004119.2");
    REQUIRE(NGS_ReferenceGetIsLocal(m_ref, ctx));
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceNotLocalSRR413283, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF("SRR496123", "NC_007112.5");
    REQUIRE(!NGS_ReferenceGetIsLocal(m_ref, ctx));
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceLocalSRR413283, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF("SRR496123", "NC_002333.2");
    REQUIRE(NGS_ReferenceGetIsLocal(m_ref, ctx));
    EXIT;
}

// Iteration over References
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIterator_GetLength_1, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_Reference* refIt = NGS_ReadCollectionGetReferences ( m_coll, m_ctx );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_EQ ( NGS_ReferenceGetLength ( refIt, ctx ), NGS_ReferenceGetLength ( m_ref, ctx ) );
    REQUIRE ( ! FAILED () );

    NGS_ReferenceRelease ( refIt, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIterator_GetLength_2, CSRA1_ReferenceFixture)
{   // bug report: after a 1-chunk reference, the next reference in an iterator report wrong length
    ENTRY_ACC( "SRR1121656" );
    m_ref = NGS_ReadCollectionGetReferences ( m_coll, m_ctx );

    bool checked = false;
    while ( NGS_ReferenceIteratorNext ( m_ref, ctx ) )
    {
        if ( string ( "GL000207.1" ) == toString ( NGS_ReferenceGetCommonName ( m_ref, ctx ), ctx, true ) )
        {
            REQUIRE_EQ ( (uint64_t)4262, NGS_ReferenceGetLength ( m_ref, ctx ) );
        }
        else if ( string ( "GL000226.1" ) == toString ( NGS_ReferenceGetCommonName ( m_ref, ctx ), ctx, true ) )
        {
            REQUIRE_EQ ( (uint64_t)15008, NGS_ReferenceGetLength ( m_ref, ctx ) );
            checked = true;
            break;
        }
    }
    REQUIRE ( checked );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIterator_GetFirstRowId, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    NGS_Reference* refIt = NGS_ReadCollectionGetReferences ( m_coll, m_ctx );

    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)460, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)785, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1101, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1318, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1681, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1966, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2246, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2526, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2764, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2976, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)3289, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)3444, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)3596, CSRA1_Reference_GetFirstRowId ( refIt, ctx ) );
    REQUIRE ( ! NGS_ReferenceIteratorNext ( refIt, ctx ) );

    NGS_ReferenceRelease ( refIt, ctx );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceIterator_GetLastRowId, CSRA1_ReferenceFixture)
{
    ENTRY_GET_REF( CSRA1_PrimaryOnly, "supercont2.1" );
    NGS_Reference* refIt = NGS_ReadCollectionGetReferences ( m_coll, m_ctx );

    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)459, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)784, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1100, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1317, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1680, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)1965, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2245, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2525, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2763, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)2975, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)3288, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)3443, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)3595, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( NGS_ReferenceIteratorNext ( refIt, ctx ) );
    REQUIRE_EQ ( (int64_t)3781, CSRA1_Reference_GetLastRowId ( refIt, ctx ) );
    REQUIRE ( ! NGS_ReferenceIteratorNext ( refIt, ctx ) );

    NGS_ReferenceRelease ( refIt, ctx );
    EXIT;
}

// NGS_ReferenceSequence

FIXTURE_TEST_CASE(SRA_Reference_Open, NGS_C_Fixture)
{
    ENTRY;
    const char* SRA_Reference = "NC_000001.10";
    NGS_ReferenceSequence * ref = NGS_ReferenceSequenceMake ( ctx, SRA_Reference );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( ref );
    NGS_ReferenceSequenceRelease ( ref, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_Reference_Open_FailsOnNonReference, NGS_C_Fixture)
{
    ENTRY;
    const char* SRA_Accession = "SRR000001";
    REQUIRE_NULL ( NGS_ReferenceSequenceMake ( ctx, SRA_Accession ) );
    REQUIRE ( ctx_xc_isa ( ctx, xcTableOpenFailed )  );
    REQUIRE_FAILED ();
    EXIT;
}
#endif

// gethostname
#if _WIN32
    #include <winsock.h>
#else
    #include <unistd.h>
#endif
static bool expectToFail() {
    char name[512] ="";
    gethostname(name, sizeof name);
    std::cerr << "GETHOSTNAME = '" << name << "': ";
    const char bad[]="tcmac0";
    if (strncmp(name,bad, sizeof bad - 1) == 0) {
      std::cerr << "bad host\n";
      return true;
    }
    std::cerr << "good host\n";
    return false;
}

FIXTURE_TEST_CASE(EBI_Reference_Open_EBI_MD5, NGS_C_Fixture)
{
/* The following request should success it order to this test to work:
   https://www.ebi.ac.uk/ena/cram/md5/ffd6aeffb54ade3d28ec7644afada2e9
   Otherwise CALL_TO_EBI_RESOLVER_FAILS is set
   and this test is expected to fail
It is known to fail on macs with old certificates file */
    const bool CALL_TO_EBI_RESOLVER_FAILS = expectToFail();

    ENTRY;
    const char* EBI_Accession = "ffd6aeffb54ade3d28ec7644afada2e9";

    NGS_ReferenceSequence * ref = NGS_ReferenceSequenceMake ( ctx, EBI_Accession );

// does not seem to fail on tcmacXXs anymore
    if ( CALL_TO_EBI_RESOLVER_FAILS ) {
//        REQUIRE ( FAILED () );
//        REQUIRE_NULL ( ref );
//        LOG(ncbi::NK::LogLevel::e_error,
//            "CANNOT TEST EBI ACCESSION BECAUSE THEIR SITE DOES NOT RESPOND!\n");
//        LOG(ncbi::NK::LogLevel::e_error, "NOW EXPECTING AN ERROR MESSAGE ...");
//        return;
    }

    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( ref );

    NGS_String* bases = NGS_ReferenceSequenceGetBases ( ref, ctx, 4, 16 );

    REQUIRE ( strcmp (NGS_StringData ( bases, ctx ), "CTTTCTGACCGAAATT") == 0 );

    REQUIRE_EQ ( NGS_ReferenceSequenceGetLength(ref, ctx), (uint64_t)784333 );

    // to suppress the warning of unused function - call toString
    toString ( bases, ctx );

    NGS_StringRelease ( bases, ctx );
    NGS_ReferenceSequenceRelease ( ref, ctx );
    EXIT;
}
#ifdef ALL
FIXTURE_TEST_CASE(EBI_Reference_Open_EBI_ACC, NGS_C_Fixture)
{
    ENTRY;
    const char* EBI_Accession = "U12345.1";
    NGS_ReferenceSequence * ref = NGS_ReferenceSequenceMake ( ctx, EBI_Accession );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( ref );

    NGS_String* bases = NGS_ReferenceSequenceGetBases ( ref, ctx, 4, 16 );
    REQUIRE ( strcmp (NGS_StringData ( bases, ctx ), "CCGCTATCAATATACT") == 0 );

    REQUIRE_EQ ( NGS_ReferenceSequenceGetLength(ref, ctx), (uint64_t)426 );

    NGS_StringRelease ( bases, ctx );
    NGS_ReferenceSequenceRelease ( ref, ctx );
    EXIT;
}
#endif
//////////////////////////////////////////// Main
int main(int argc, char* argv[])
{
    VDB::Application app(argc, argv);
    KConfigDisableUserSettings();

    if(
0)
        assert(!KDbgSetString("KNS-HTTP"));

    KConfig * kfg = NULL;
    rc_t rc = KConfigMake(&kfg, NULL);

    // turn off certificate validation to download EBI reference
    if (rc == 0)
        rc = KConfigWriteString(kfg, "/tls/allow-all-certs", "true");

    if (rc == 0)
        rc = (rc_t)NgsReferenceTestSuite(argc, app.getArgV());

    KConfigRelease(kfg);
    NGS_C_Fixture::ReleaseCache();

    app.setRc( rc );
    return app.getExitCode();
}
