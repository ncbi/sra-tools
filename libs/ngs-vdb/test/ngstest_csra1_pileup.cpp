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
* Unit tests for NGS C interface, CSRA1 archives, Pileup-related functions
*/

#include "ngs_c_fixture.hpp"

#include "../ncbi/ngs/NGS_Pileup.h"
#include "../ncbi/ngs/NGS_PileupEvent.h"

#include <kapp/main.h>

#include <kdb/manager.h>

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <vdb/manager.h>
#include <vdb/vdb-priv.h>

#include <klib/printf.h>

#include <limits.h>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsCsra1PileupTestSuite);

const char* CSRA1_PrimaryOnly   = "SRR1063272";
const char* CSRA1_WithSecondary = "SRR833251";
const char* CSRA1_WithCircularReference = "SRR821492";

#define ENTRY_GET_PILEUP(acc,ref) \
    ENTRY_GET_REF(acc,ref) \
    m_pileup = NGS_ReferenceGetPileups( m_ref, ctx, true, false); \
    REQUIRE ( ! FAILED () && m_pileup );

#define ENTRY_GET_PILEUP_NEXT(acc,ref) \
    ENTRY_GET_PILEUP(acc,ref) \
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );

#define ENTRY_GET_PILEUP_SLICE(acc,ref,offset,size) \
    ENTRY_GET_REF(acc,ref) \
    m_pileup = NGS_ReferenceGetPileupSlice( m_ref, ctx, offset, size, true, false); \
    REQUIRE ( ! FAILED () && m_pileup );

#define ENTRY_GET_PILEUP_SLICE_NEXT(acc,ref,offset,size) \
    ENTRY_GET_PILEUP_SLICE(acc,ref,offset,size); \
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );

class CSRA1_Fixture : public NGS_C_Fixture
{
public:
    CSRA1_Fixture()
    : m_pileup (0)
    {
    }

    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if ( m_pileup != 0 )
            {
                NGS_PileupRelease ( m_pileup, m_ctx );
            }
        }
        NGS_C_Fixture :: Release ();
    }

    void Advance ( uint32_t count )
    {
        while ( count > 0 )
        {
            if  ( ! NGS_PileupIteratorNext ( m_pileup, m_ctx ) )
                throw std :: logic_error ( "CSRA1_Fixture::Advance : NGS_PileupIteratorNext() failed" );
            --count;
        }
    }

    void PrintAll ()
    {
        HYBRID_FUNC_ENTRY ( rcSRA, rcRow, rcAccessing );
        while (NGS_PileupIteratorNext ( m_pileup, ctx ))
        {
            int64_t pos = NGS_PileupGetReferencePosition ( m_pileup, ctx );
            if  ( FAILED () )
                throw std :: logic_error ( "CSRA1_Fixture::PrintAll : NGS_PileupGetReferencePosition() failed" );
            unsigned int depth = NGS_PileupGetPileupDepth ( m_pileup, ctx );
            if  ( FAILED () )
                throw std :: logic_error ( "CSRA1_Fixture::PrintAll : NGS_PileupGetPileupDepth() failed" );
            string ref = toString ( NGS_PileupGetReferenceSpec ( m_pileup, ctx ), ctx, true );
            if  ( FAILED () )
                throw std :: logic_error ( "CSRA1_Fixture::PrintAll : NGS_PileupGetReferenceSpec() failed" );
            if ( depth != 0 )
                cout <<  ref << "\t" << pos << "\t" << depth << endl;
        }
    }


    NGS_Pileup*     m_pileup;
};

//// PileupIterator, full non-circular reference

// no access before a call to NGS_PileupIteratorNext():

FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessBeforeNext_PileupGetReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );

    REQUIRE_NULL ( NGS_PileupGetReferenceSpec ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessBeforeNext_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_PileupGetReferencePosition ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessBeforeNext_PileupGetReferenceBase, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );

    REQUIRE_EQ ( (char)0, NGS_PileupGetReferenceBase ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessBeforeNext_PileupGetPileupDepth, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );

    NGS_PileupGetPileupDepth ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}

// access after a call to NGS_PileupIteratorNext():

FIXTURE_TEST_CASE(CSRA1_PileupIterator_AccessAfterNext_PileupGetReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_NEXT ( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE_STRING ( "supercont2.1", NGS_PileupGetReferenceSpec ( m_pileup, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIterator_AccessAfterNext_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_NEXT ( CSRA1_PrimaryOnly, "supercont2.1" );

    REQUIRE_EQ ( (int64_t)0, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE ( ! FAILED () );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIterator_AccessAfterNext_PileupGetReferenceBase, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_NEXT ( CSRA1_PrimaryOnly, "supercont2.1" );

    char base = NGS_PileupGetReferenceBase ( m_pileup, ctx );
    REQUIRE ( ! FAILED () && base != 0 );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIterator_AccessAfterNext_PileupGetPileupDepth, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_NEXT ( CSRA1_PrimaryOnly, "supercont2.1" );
    REQUIRE_EQ ( (unsigned int)0, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    EXIT;
}

// no access after the end of iteration
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessAfterEnd_PileupGetReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    REQUIRE_NULL ( NGS_PileupGetReferenceSpec ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessAfterEnd_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    NGS_PileupGetReferencePosition ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessAfterEnd_PileupGetReferenceBase, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    REQUIRE_EQ ( (char)0, NGS_PileupGetReferenceBase ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessAfterEnd_PileupGetPileupDepth, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    NGS_PileupGetPileupDepth ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_NoAccessAfterEnd_PileupIteratorNext, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    REQUIRE ( ! NGS_PileupIteratorNext ( m_pileup, ctx ) );

    EXIT;
}

// regular operation

FIXTURE_TEST_CASE(CSRA1_PileupIterator_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_NEXT ( CSRA1_PrimaryOnly, "supercont2.1" );

    REQUIRE_EQ ( (int64_t)0, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (int64_t)1, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (int64_t)2, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIterator_PileupGetPileupDepth_1, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );

    Advance(85);

    REQUIRE_EQ ( (int64_t)84, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)0, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );

    Advance(1);

    REQUIRE_EQ ( (int64_t)85, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)1, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_PileupGetPileupDepth_2, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );

    Advance(186);
    REQUIRE_EQ ( (int64_t)185, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)2, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIterator_PileupGetPileupDepth_3, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP ( CSRA1_PrimaryOnly, "supercont2.1" );

    Advance(5491);

    REQUIRE_EQ ( (int64_t)5490, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)2, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );

    Advance(1);

    REQUIRE_EQ ( (int64_t)5491, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)3, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );

    EXIT;
}

#if 0
FIXTURE_TEST_CASE(CSRA1_PileupIterator_PrintOut, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP( CSRA1_PrimaryOnly, "supercont2.1" );
    PrintAll ();
    EXIT;
}
#endif

//// PileupIterator, non-circular reference slice

// no access before a call to NGS_PileupIteratorNext():

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessBeforeNext_PileupGetReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );

    REQUIRE_NULL ( NGS_PileupGetReferenceSpec ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessBeforeNext_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );

    NGS_PileupGetReferencePosition ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessBeforeNext_PileupGetReferenceBase, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );

    REQUIRE_EQ ( (char)0, NGS_PileupGetReferenceBase ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessBeforeNext_PileupGetPileupDepth, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );

    NGS_PileupGetPileupDepth ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}

// access after a call to NGS_PileupIteratorNext():

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_AccessAfterNext_PileupGetReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE_NEXT( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );
    REQUIRE_STRING ( "supercont2.1", NGS_PileupGetReferenceSpec ( m_pileup, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_AccessAfterNext_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE_NEXT( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );

    REQUIRE_EQ ( (int64_t)5431, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE ( ! FAILED () );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_AccessAfterNext_PileupGetReferenceBase, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE_NEXT( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );

    char base = NGS_PileupGetReferenceBase ( m_pileup, ctx );
    REQUIRE ( ! FAILED () && base != 0 );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_AccessAfterNext_PileupGetPileupDepth, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE_NEXT( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );
    REQUIRE_EQ ( (unsigned int)1, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    EXIT;
}

// no access after the end of iteration:

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessAfterEnd_PileupGetReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    REQUIRE_NULL ( NGS_PileupGetReferenceSpec ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessAfterEnd_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    NGS_PileupGetReferencePosition ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessAfterEnd_PileupGetReferenceBase, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    REQUIRE_EQ ( (char) 0, NGS_PileupGetReferenceBase ( m_pileup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_NoAccessAfterEnd_PileupGetPileupDepth, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 4 );
    while ( NGS_PileupIteratorNext ( m_pileup, ctx ) ) {}

    NGS_PileupGetPileupDepth ( m_pileup, ctx );
    REQUIRE_FAILED ();

    EXIT;
}

// regular operation

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_PileupGetReferencePosition, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5431, 2 );

    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (int64_t)5431, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (int64_t)5432, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE ( ! NGS_PileupIteratorNext ( m_pileup, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_PileupGetPileupDepth_NoFiltering, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 404, 4 );

    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)1, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)1, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)2, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)2, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( ! NGS_PileupIteratorNext ( m_pileup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_PileupIteratorSlice_PileupGetPileupDepth_WithFiltering, CSRA1_Fixture)
{   // alignments marked as rejected are not included into pileup depth counts
    ENTRY_GET_PILEUP_SLICE( CSRA1_PrimaryOnly, "supercont2.1", 5505, 4 );

// positions 5505-5508 match a rejected alignment; with filtering turned off the reported depths would be 3, 3, 4, 4.

    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)2, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)2, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)3, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)3, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( ! NGS_PileupIteratorNext ( m_pileup, ctx ) );

    EXIT;
}
//TODO: alignment filtering-related schema variations
// no RD_FILTER physically exists in either PRIMARY_ALIGNMENT or SEQUENCE (no filtering)
//      (use VTableListPhysColumns) (NB. READ_FILTER may be present but virtual!)
// RD_FILTER physically exists in PRIMARY_ALIGNMENT
// RD_FILTER_CACHE appears to exist virtually in PRIMARY_ALIGNMENT:
// RD_FILTER physically exists in SEQUENCE and RD_FILTER_CACHE does not exist in PRIMARY_ALIGNMENT

////TODO: PileupIterator, full circular reference
////TODO: PileupIterator, circular reference slice

// discrepancies with sra-pileup

#if SHOW_UNIMPLEMENTED
FIXTURE_TEST_CASE(CSRA1_Pileup_ExtraPileupReported, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( "SRR833251", "gi|169794206|ref|NC_010410.1|", 19416, 2 );
    // sra-pileup stops at position 19417 (0-based), ngs-pileup at position 19418

    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (int64_t)19416, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)1, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (int64_t)19417, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)0, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );
    REQUIRE ( ! NGS_PileupIteratorNext ( m_pileup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_Pileup_ExtraEventReported, CSRA1_Fixture)
{
    ENTRY_GET_PILEUP_SLICE( "SRR1063272", "supercont2.1", 12979, 1 );
    // at position 12980 (0-based) sra-pileup reports depth of 15, ngs-pileup 16

    REQUIRE ( NGS_PileupIteratorNext ( m_pileup, ctx ) );
    REQUIRE_EQ ( (int64_t)12979, NGS_PileupGetReferencePosition ( m_pileup, ctx ) );
    REQUIRE_EQ ( (unsigned int)15, NGS_PileupGetPileupDepth ( m_pileup, ctx ) );

    EXIT;
}
#endif

//// PileupEvent

//TODO: NGS_PileupEventGetReferenceSpec
//TODO: NGS_PileupEventGetReferencePosition
//TODO: NGS_PileupEventGetMappingQuality
//TODO: NGS_PileupEventGetAlignmentId
//TODO: NGS_PileupEventGetAlignment
//TODO: NGS_PileupEventGetAlignmentPosition
//TODO: NGS_PileupEventGetFirstAlignmentPosition
//TODO: NGS_PileupEventGetLastAlignmentPosition
//TODO: NGS_PileupEventGetEventType
//TODO: NGS_PileupEventGetAlignmentBase
//TODO: NGS_PileupEventGetAlignmentQuality
//TODO: NGS_PileupEventGetInsertionBases
//TODO: NGS_PileupEventGetInsertionQualities
//TODO: NGS_PileupEventGetDeletionCount

//////////////////////////////////////////// Main
int main(int argc, char* argv[])
{
    KConfigDisableUserSettings();
    int ret = NgsCsra1PileupTestSuite(argc, argv);
    NGS_C_Fixture::ReleaseCache();
    return ret;
}
