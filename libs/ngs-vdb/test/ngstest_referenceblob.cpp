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
* Unit tests for low-level NGS functions handling blob-bases access to REFERENCE table
*/

// suppress macro max from windows.h
#define NOMINMAX

#include "ngs_c_fixture.hpp"

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <kapp/main.h>

#include <vdb/database.h>

#include <../ncbi/ngs/NGS_Cursor.h>
#include <../ncbi/ngs/NGS_ReferenceBlob.h>
#include <../ncbi/ngs/NGS_ReferenceBlobIterator.h>

#include <../ncbi/ngs/CSRA1_Reference.h>
#include <../ncbi/ngs/VByteBlob.h>

#include <stdexcept>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsReferenceBlobTestSuite);

const char* CSRA1_Accession = "SRR1063272";
const char* CSRA1_Accession_WithRepeats = "SRR600094";
const char* CSRA1_Accession_WithCircular = "SRR1769246";

//////////////////////////////////////////// NGS_ReferenceBlob

class ReferenceBlobFixture : public NGS_C_Fixture
{
public:
    ReferenceBlobFixture ()
    :   m_curs ( 0 ),
        m_blob ( 0 )
    {
    }

    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if ( m_blob != 0 )
            {
                NGS_ReferenceBlobRelease ( m_blob, m_ctx );
            }
            if ( m_curs != 0 )
            {
                NGS_CursorRelease ( m_curs, m_ctx );
            }
        }
        NGS_C_Fixture :: Release ();
    }

    void GetCursor(const char* p_acc )
    {
        const VDatabase *db = openDB( p_acc );
        NGS_String* run_name = NGS_StringMake ( m_ctx, "", 0);

        if ( m_curs != 0 )
        {
            NGS_CursorRelease ( m_curs, m_ctx );
        }
        m_curs = NGS_CursorMakeDb ( m_ctx, db, run_name, "REFERENCE", reference_col_specs, reference_NUM_COLS );

        NGS_StringRelease ( run_name, m_ctx );
        THROW_ON_RC ( VDatabaseRelease ( db ) );
    }
    void GetBlob ( const char* p_acc, int64_t p_rowId, int64_t p_refFirst = 1, int64_t p_refLast = 0 )
    {
        GetCursor ( p_acc );
        if ( m_blob != 0 )
        {
            NGS_ReferenceBlobRelease ( m_blob, m_ctx );
        }
        m_blob = NGS_ReferenceBlobMake ( m_ctx, m_curs, p_rowId, p_refFirst, p_refLast );
    }

    void CheckRange( ctx_t ctx, int64_t p_first, uint64_t p_count )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );
        int64_t first;
        uint64_t count;
        ON_FAIL ( NGS_ReferenceBlobRowRange ( m_blob, ctx,  & first, & count ) )
        {
            throw std :: logic_error ( "NGS_ReferenceBlobRowRange() failed" );
        }
        if ( p_first != first || p_count != count )
        {
            ostringstream str;
            str << "CheckRange(first=" << p_first << ", count=" << p_count << ") : first=" << first << ", count=" << count;
            throw std :: logic_error ( str.str() );
        }
    }

    const NGS_Cursor* m_curs;
    struct NGS_ReferenceBlob* m_blob;
};

// Make

TEST_CASE ( NGS_ReferenceBlob_Make_BadCursor)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRow, rcAccessing );

    struct NGS_ReferenceBlob * blob = NGS_ReferenceBlobMake ( ctx, NULL, 1, 1, 0 );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( blob );
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Make_BadRowId, ReferenceBlobFixture )
{
    ENTRY;
    GetCursor ( CSRA1_Accession );

    struct NGS_ReferenceBlob * blob = NGS_ReferenceBlobMake ( ctx, m_curs, 1, 2, 0 ); // rowId < refStart
    REQUIRE_FAILED ();
    REQUIRE_NULL ( blob );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Make_BadStartId, ReferenceBlobFixture )
{
    ENTRY;
    GetCursor ( CSRA1_Accession );

    struct NGS_ReferenceBlob * blob = NGS_ReferenceBlobMake ( ctx, m_curs, 1, -1, 1 ); // bad refStart
    REQUIRE_FAILED ();
    REQUIRE_NULL ( blob );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Make, ReferenceBlobFixture )
{
    ENTRY;
    GetCursor ( CSRA1_Accession );

    m_blob = NGS_ReferenceBlobMake ( ctx, m_curs, 1, 1, 0 );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( m_blob );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_DuplicateRelease, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    // Duplicate
    NGS_ReferenceBlob* anotherBlob = NGS_ReferenceBlobDuplicate ( m_blob, ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( anotherBlob );
    // Release
    NGS_ReferenceBlobRelease ( anotherBlob, ctx );

    EXIT;
}

// Range

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Range_NullArg_1, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    uint64_t rowCount;
    NGS_ReferenceBlobRowRange ( m_blob, ctx, NULL, &rowCount );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint64_t)4, rowCount);

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Range_NullArg_2, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    int64_t firstRow;
    NGS_ReferenceBlobRowRange ( m_blob, ctx, &firstRow, NULL );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (int64_t)1, firstRow);

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Range_NullArgs, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    NGS_ReferenceBlobRowRange ( m_blob, ctx, NULL, NULL );
    REQUIRE ( ! FAILED () );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Range, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );
    CheckRange ( ctx, 1, 4 );
    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_RangeLimited, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1, 1, 3 );
    CheckRange ( ctx, 1, 3 );
    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Range_FirstRowInsideBlob, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 3 );
    CheckRange ( ctx, 3, 1 );
    EXIT;
}

// Data

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Data_BadArg, ReferenceBlobFixture )
{
    ENTRY;

    const void* data = NGS_ReferenceBlobData ( NULL, ctx );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( data );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Data, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    const void* data = NGS_ReferenceBlobData ( m_blob, ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( data );
    REQUIRE_EQ ( string ( "GAATTCTAAA" ), string ( (const char*)data, 10 ) );

    EXIT;
}

// Size

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Size_BadArg, ReferenceBlobFixture )
{
    ENTRY;

    NGS_ReferenceBlobSize ( NULL, ctx );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE ( NGS_ReferenceBlob_Size, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    REQUIRE_EQ ( (uint64_t)20000, NGS_ReferenceBlobSize ( m_blob, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_UnpackedSize_NoRepeats, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    REQUIRE_EQ ( (uint64_t)20000, NGS_ReferenceBlobUnpackedSize ( m_blob, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_UnpackedSize_WithRepeats, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 96 ); /* this blob consists of 9 repeated all-N rows */

    REQUIRE_EQ ( (uint64_t)45000, NGS_ReferenceBlobUnpackedSize ( m_blob, ctx ) );

    EXIT;
}

// ResolveOffset

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_BadSelf, ReferenceBlobFixture )
{
    ENTRY;

    uint64_t inRef;
    NGS_ReferenceBlobResolveOffset ( NULL, ctx, 0, & inRef, NULL, NULL );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_OffsetOutOfRange, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    uint64_t inRef;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 20000, & inRef, NULL, NULL );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_NullOutputParam, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 0, NULL, NULL, NULL );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_NullOptionalParams, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    uint64_t inRef;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 0, & inRef, NULL, NULL );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_FirstChunk_NoRepeat, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 ); /* ref positions 1-20000 in this blob */

    uint64_t inRef;
    uint32_t repeatCount;
    uint64_t increment;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 6123, & inRef, & repeatCount, & increment );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint64_t)6123, inRef );
    REQUIRE_EQ ( (uint32_t)1, repeatCount  );
    REQUIRE_EQ ( (uint64_t)0, increment  );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_NotTheFirstReference, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 461, 460 ); /* 2nd row for the reference supercont2.1 */

    uint64_t inRef;
    uint32_t repeatCount;
    uint64_t increment;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 99, & inRef, & repeatCount, & increment );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint64_t)5099, inRef );   // relative to the reference the blob is in
    REQUIRE_EQ ( (uint32_t)1, repeatCount  );
    REQUIRE_EQ ( (uint64_t)0, increment  );

    EXIT;
}


FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_NoRepeat, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 5 ); /* ref positions 20001-25000 in this blob */

    uint64_t inRef;
    uint32_t repeatCount;
    uint64_t increment;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 123, & inRef, & repeatCount, & increment );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint64_t)20123, inRef );
    REQUIRE_EQ ( (uint32_t)1, repeatCount  );
    REQUIRE_EQ ( (uint64_t)0, increment  );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_WithRepeat, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 1 );

    uint64_t inRef;
    uint32_t repeatCount;
    uint64_t increment;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 0, & inRef, & repeatCount, & increment );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint64_t)0, inRef );
    REQUIRE_EQ ( (uint32_t)2, repeatCount  );
    REQUIRE_EQ ( (uint64_t)5000, increment  );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlob_ResolveOffset_PastRepeat, ReferenceBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 1 );

    uint64_t inRef;
    uint32_t repeatCount;
    uint64_t increment;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 5100, & inRef, & repeatCount, & increment );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint64_t)10100, inRef );
    REQUIRE_EQ ( (uint32_t)1, repeatCount  );
    REQUIRE_EQ ( (uint64_t)0, increment  );

    EXIT;
}

//////////////////////////////////////////// NGS_ReferenceBlobIterator

class ReferenceBlobIteratorFixture : public ReferenceBlobFixture
{
public:
    ReferenceBlobIteratorFixture ()
    :   m_blobIt ( 0 )
    {
    }

    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if ( m_blobIt != 0 )
            {
                NGS_ReferenceBlobIteratorRelease ( m_blobIt, m_ctx );
            }
        }
        ReferenceBlobFixture :: Release ();
    }

    void GetIterator ( const char* p_acc, int64_t p_rowId, int64_t p_lastRowId )
    {
        GetCursor ( p_acc );
        if ( m_blobIt != 0 )
        {
            NGS_ReferenceBlobIteratorRelease ( m_blobIt, m_ctx );
        }
        m_blobIt = NGS_ReferenceBlobIteratorMake ( m_ctx, m_curs, 1, p_rowId, p_lastRowId );
    }

    bool NextBlob( ctx_t ctx )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );
        if ( m_blob != 0)
        {
            NGS_ReferenceBlobRelease ( m_blob, m_ctx );
            m_blob = 0;
        }
        TRY  ( bool ret =  NGS_ReferenceBlobIteratorHasMore ( m_blobIt, ctx ) )
        {
            if ( ret )
            {
                ON_FAIL ( m_blob = NGS_ReferenceBlobIteratorNext ( m_blobIt, ctx ) )
                {
                    throw std :: logic_error ( "NGS_ReferenceBlobIteratorNext() failed" );
                }
            }
           return ret;
        }
        throw std :: logic_error ( "NGS_ReferenceBlobIteratorHasMore() failed" );
    }

    struct NGS_ReferenceBlobIterator* m_blobIt;
};

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIteratorMake_NullCursor, ReferenceBlobIteratorFixture )
{
    ENTRY;

    struct NGS_ReferenceBlobIterator* blobIt = NGS_ReferenceBlobIteratorMake ( ctx, NULL, 1, 1, 2 );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( blobIt );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_CreateRelease, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetCursor ( CSRA1_Accession );

    struct NGS_ReferenceBlobIterator* blobIt = NGS_ReferenceBlobIteratorMake ( ctx, m_curs, 1, 1, 2 );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( blobIt );
    NGS_ReferenceBlobIteratorRelease ( blobIt, ctx );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_DuplicateRelease, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 2 );

    // Duplicate
    struct NGS_ReferenceBlobIterator* anotherBlobIt = NGS_ReferenceBlobIteratorDuplicate ( m_blobIt, ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( anotherBlobIt );
    // Release
    NGS_ReferenceBlobIteratorRelease ( anotherBlobIt, ctx );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_Next_BadSelf, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 2 );
    m_blob = NGS_ReferenceBlobIteratorNext ( NULL, ctx );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( m_blob );
    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_Next_Empty, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 0 );

    m_blob = NGS_ReferenceBlobIteratorNext ( m_blobIt, ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NULL ( m_blob );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_Next, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 10 );
    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 1, 4 );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_Limited_Next, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 2 );
    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 1, 2 );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_Next_Multiple, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 100 );
    REQUIRE ( NextBlob ( ctx ) );
    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 5, 4 );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_Next_End, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 1 );
    REQUIRE ( NextBlob ( ctx ) );
    REQUIRE ( ! NextBlob ( ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_HasMore_Empty, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 0 );

    REQUIRE ( ! NGS_ReferenceBlobIteratorHasMore ( m_blobIt, ctx ) );
    REQUIRE ( ! FAILED () );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_HasMore, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 1 );

    REQUIRE ( NGS_ReferenceBlobIteratorHasMore ( m_blobIt, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_FullScan, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 1, 459 );
    size_t count = 0;
    while ( NGS_ReferenceBlobIteratorHasMore ( m_blobIt, ctx ) )
    {
        REQUIRE ( NextBlob ( ctx ) );
        ++count;
    }
    REQUIRE_EQ( (size_t)12, count);

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_ReferenceBlobIterator_MidTable, ReferenceBlobIteratorFixture )
{
    ENTRY;
    GetIterator ( CSRA1_Accession, 100, 106 ); // start in the middle of the reference table

    // move to the second blob in the set
    REQUIRE ( NextBlob ( ctx ) );
    REQUIRE ( NextBlob ( ctx ) );

    CheckRange ( ctx, 101, 4 );

    // verify that the blob was given the right reference start row
    uint64_t inRef;
    uint32_t repeatCount;
    uint64_t increment;
    NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 99, & inRef, & repeatCount, & increment );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint64_t)500099, inRef ); // relative to the ref start, which is row 1 in this case
    REQUIRE_EQ ( (uint32_t)1, repeatCount  );
    REQUIRE_EQ ( (uint64_t)0, increment  );

    EXIT;
}

// NGS_ReferenceGetBlobs

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetBlobs_All, ReferenceBlobIteratorFixture)
{
    ENTRY_GET_REF ( CSRA1_Accession_WithCircular, "NC_012920.1" );

    m_blobIt = NGS_ReferenceGetBlobs( m_ref, ctx, 0, (uint64_t)-1 );
    REQUIRE ( ! FAILED () && m_blobIt );

    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 619147, 1 );
    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 619148, 1 );
    REQUIRE ( NextBlob ( ctx ) );
    // the actual blob contains 4 rows but only the first 2 are from this reference
    CheckRange ( ctx, 619149, 2 );
    // size includes only the 2 reported rows
    REQUIRE_EQ ( (uint64_t)6569, NGS_ReferenceBlobSize ( m_blob, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetBlobs_Slice_SingleBlob, ReferenceBlobIteratorFixture)
{
    ENTRY_GET_REF ( CSRA1_Accession, "supercont2.1" );

    m_blobIt = NGS_ReferenceGetBlobs( m_ref, ctx, 25000, 5000 );
    REQUIRE ( ! FAILED () && m_blobIt );

    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 6, 1 );
    // inside a slice-bound iterator, blobs resolve offsets (inReference) to be in bases from the start of the reference, not the slice
    uint64_t inReference = 0;
    TRY (NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 10, &inReference, NULL, NULL ) )
    {
        REQUIRE_EQ ( (uint64_t)25010, inReference );
    }

    REQUIRE ( ! NextBlob ( ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReferenceGetBlobs_Slice_MultipleBlobs, ReferenceBlobIteratorFixture)
{
    ENTRY_GET_REF ( CSRA1_Accession, "supercont2.1" );

    m_blobIt = NGS_ReferenceGetBlobs( m_ref, ctx, 6000, 5000 );
    REQUIRE ( ! FAILED () && m_blobIt );

    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 2, 1 );
    // inside a slice-bound iterator, blobs resolve offsets (inReference) to be in bases from the start of the reference, not the slice
    uint64_t inReference = 0;
    TRY (NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 100, &inReference, NULL, NULL ) )
    {
        REQUIRE_EQ ( (uint64_t)5100, inReference );
    }

    REQUIRE ( NextBlob ( ctx ) );
    CheckRange ( ctx, 3, 1 );
    TRY (NGS_ReferenceBlobResolveOffset ( m_blob, ctx, 200, &inReference, NULL, NULL ) )
    {
        REQUIRE_EQ ( (uint64_t)10200, inReference );
    }
    REQUIRE ( ! NextBlob ( ctx ) );

    EXIT;
}

//////////////////////////////////////////// Main

int main(int argc, char* argv[])
{
    KConfigDisableUserSettings();
    int m_coll=NgsReferenceBlobTestSuite(argc, argv);
    NGS_C_Fixture::ReleaseCache();
    return m_coll;
}
