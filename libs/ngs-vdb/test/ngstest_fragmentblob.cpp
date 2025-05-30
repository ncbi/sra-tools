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
* Unit tests for low-level NGS functions
*/

// suppress macro max from windows.h
#define NOMINMAX

#include "ngs_c_fixture.hpp"

#include <../ncbi/ngs/NGS_Cursor.h>
#include <../ncbi/ngs/SRA_Read.h>
#include <../ncbi/ngs/NGS_FragmentBlob.h>
#include <../ncbi/ngs/NGS_FragmentBlobIterator.h>

#include <kapp/main.h>

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <vdb/table.h>
#include <vdb/database.h>

#include <stdexcept>
#include <cstring>
#include <limits>
#include <cmath>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsFragmentBlobTestSuite);

const char* SRA_Accession = "SRR000001";

//////////////////////////////////////////// NGS_FragmentBlob

class FragmentBlobFixture : public NGS_C_Fixture
{
public:
    FragmentBlobFixture ()
    :   m_tbl ( 0 ),
        m_curs ( 0 ),
        m_blob ( 0 )
    {
    }

    void MakeBlob ( const char* acc, int64_t rowId )
    {
        m_tbl = openTable( acc );
        m_curs = NGS_CursorMake ( m_ctx, m_tbl, sequence_col_specs, seq_NUM_COLS );
        if ( m_curs == 0 )
        {
            throw logic_error ("FragmentBlobFixture::MakeBlob NGS_CursorMake failed");
        }
        NGS_String* run = NGS_StringMake ( m_ctx, acc, string_size ( acc ) );
        m_blob = NGS_FragmentBlobMake ( m_ctx, run, m_curs, rowId );
        NGS_StringRelease ( run, m_ctx );
        if ( m_blob == 0 )
        {
            throw logic_error ("FragmentBlobFixture::MakeBlob NGS_FragmentBlobMake failed");
        }
    }
    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if ( m_blob != 0 )
            {
                NGS_FragmentBlobRelease ( m_blob, m_ctx );
            }
            if ( m_curs != 0 )
            {
                NGS_CursorRelease ( m_curs, m_ctx );
            }
            if ( m_tbl != 0 )
            {
                VTableRelease ( m_tbl );
            }
        }
        NGS_C_Fixture :: Release ();
    }

    const VTable* m_tbl;
    const NGS_Cursor* m_curs;
    struct NGS_FragmentBlob* m_blob;
};

TEST_CASE ( NGS_FragmentBlobMake_BadCursor)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRow, rcAccessing );

    NGS_String* run = NGS_StringMake ( ctx, "", 0 );
    REQUIRE ( ! FAILED () );

    struct NGS_FragmentBlob * blob = NGS_FragmentBlobMake ( ctx, run, NULL, 1 );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( blob );

    NGS_StringRelease ( run, ctx );
    REQUIRE ( ! FAILED () );
}

FIXTURE_TEST_CASE ( NGS_FragmentBlobMake_NullRunName, FragmentBlobFixture )
{
    ENTRY;

    m_tbl = openTable( SRA_Accession );
    m_curs = NGS_CursorMake ( m_ctx, m_tbl, sequence_col_specs, seq_NUM_COLS );
    REQUIRE_NOT_NULL ( m_curs );

    struct NGS_FragmentBlob * blob = NGS_FragmentBlobMake ( ctx, NULL, m_curs, 1 );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( blob );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlobMake_BadRowId, FragmentBlobFixture )
{
    ENTRY;

    m_tbl = openTable( SRA_Accession );
    m_curs = NGS_CursorMake ( m_ctx, m_tbl, sequence_col_specs, seq_NUM_COLS );
    REQUIRE_NOT_NULL ( m_curs );
    NGS_String* run = NGS_StringMake ( m_ctx, SRA_Accession, string_size ( SRA_Accession ) );

    m_blob = NGS_FragmentBlobMake ( m_ctx, run, m_curs, -1 ); // bad row Id
    REQUIRE_FAILED ();

    NGS_StringRelease ( run, m_ctx );
    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_RowRange, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    int64_t first = 0;
    uint64_t count = 0;
    NGS_FragmentBlobRowRange ( m_blob, m_ctx, & first, & count );
    REQUIRE_EQ ( (int64_t)1, first );
    REQUIRE_EQ ( (uint64_t)4, count );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_DuplicateRelease, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    // Duplicate
    NGS_FragmentBlob* anotherBlob = NGS_FragmentBlobDuplicate ( m_blob, m_ctx );
    REQUIRE_NOT_NULL ( anotherBlob );
    // Release
    NGS_FragmentBlobRelease ( anotherBlob, m_ctx );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_Data_BadArg, FragmentBlobFixture )
{
    ENTRY;

    REQUIRE_NULL ( NGS_FragmentBlobData ( NULL, m_ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE ( NGS_FragmentBlob_Data, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    const void* data = NGS_FragmentBlobData ( m_blob, m_ctx );
    REQUIRE_NOT_NULL ( data );
    REQUIRE_EQ ( string ( "TCAGAT" ), string ( (const char*)data, 6 ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_Size_BadArg, FragmentBlobFixture )
{
    ENTRY;

    NGS_FragmentBlobSize ( NULL, m_ctx );
    REQUIRE_FAILED ();

    EXIT;
}
FIXTURE_TEST_CASE ( NGS_FragmentBlob_Size, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    REQUIRE_EQ ( (uint64_t)1080, NGS_FragmentBlobSize ( m_blob, m_ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_Run, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    const NGS_String * run = NGS_FragmentBlobRun ( m_blob, m_ctx );
    REQUIRE_EQ ( string ( SRA_Accession ), string ( NGS_StringData ( run, m_ctx ) , NGS_StringSize ( run, m_ctx ) ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_InfoByOffset_BadSelf, FragmentBlobFixture )
{
    ENTRY;

    int64_t rowId;
    uint64_t fragStart;
    uint64_t baseCount;
    int32_t bioNumber;
    NGS_FragmentBlobInfoByOffset ( NULL, ctx, 0, & rowId, & fragStart, & baseCount, & bioNumber );
    REQUIRE_FAILED ();

    EXIT;
}
// TODO: NULL for optional parameters

FIXTURE_TEST_CASE ( NGS_FragmentBlob_InfoByOffset_Biological, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    int64_t rowId;
    uint64_t fragStart;
    uint64_t baseCount;
    int32_t bioNumber;
    // offset 300 is in row 2 which starts at 284 and consists of 4 fragments:
    // technical, start 284, len 4
    // biological #0, start 288, len 115 <== expect to see this for offset 300
    // technical, start 403, len 44
    // biological #1, start 447, len 99
    NGS_FragmentBlobInfoByOffset ( m_blob, ctx, 300, & rowId, & fragStart, & baseCount, & bioNumber );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (int64_t)2, rowId );
    REQUIRE_EQ ( (uint64_t)288, fragStart );
    REQUIRE_EQ ( (uint64_t)115, baseCount );
    REQUIRE_EQ ( (int32_t)0, bioNumber );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_InfoByOffset_Technical, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    int64_t rowId;
    uint64_t fragStart;
    uint64_t baseCount;
    int32_t bioNumber;
    // offset 300 is in row 2 which starts at 284 and consists of 4 fragments:
    // technical, start 284, len 4
    // biological #0, start 288, len 115
    // technical, start 403, len 44  <== expect to see this for offset 410
    // biological #1, start 447, len 99
    NGS_FragmentBlobInfoByOffset ( m_blob, ctx, 410, & rowId, & fragStart, & baseCount, & bioNumber );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (int64_t)2, rowId );
    REQUIRE_EQ ( (uint64_t)403, fragStart );
    REQUIRE_EQ ( (uint64_t)44, baseCount );
    REQUIRE_EQ ( (int32_t)-1, bioNumber );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_InfoByOffset_WithRepeat, FragmentBlobFixture )
{   // VDB-3026, VDB-2809
    ENTRY;

    // In SRR341578.SEQUENCE, rows 197317 and 197318 have identical values in the READ column
    // we test correct handling of the repeat count in the row map by providing an offset
    // that points into the second of the two rows
    const int64_t RepeatedRowId = 197318;
    const uint64_t OffsetIntoRepeatedRowId = 8889;
    const char* acc = "SRR341578";

    const VDatabase *db = openDB( acc );
    REQUIRE_RC ( VDatabaseOpenTableRead ( db, & m_tbl, "SEQUENCE" ) );
    REQUIRE_RC ( VDatabaseRelease ( db ) );

    m_curs = NGS_CursorMake ( m_ctx, m_tbl, sequence_col_specs, seq_NUM_COLS );
    REQUIRE_NOT_NULL ( m_curs );

    // The blob-making code in vdb is very-very smart and decides on the size of the blob
    // to create based on the access patterns.
    // If we specify our rowId directly, we will get a one-row blob. We want
    // more than one row so that repeat count is > 1. So, we walk from 1 up to our RepeatedRowId
    // to make sure we got a bigger blob:
    {
        NGS_String* run = NGS_StringMake ( m_ctx, acc, string_size ( acc ) );
        int64_t rowId = 1;
        while ( ! FAILED () )
        {
            m_blob = NGS_FragmentBlobMake ( m_ctx, run, m_curs, rowId );
            int64_t first = 0;
            uint64_t count = 0;
            NGS_FragmentBlobRowRange ( m_blob, m_ctx, & first, & count );
            if ( first + (int64_t)count > RepeatedRowId )
            {
                break; // m_blob is what we need
            }
            NGS_FragmentBlobRelease ( m_blob, m_ctx );
            rowId += count;
        }
        NGS_StringRelease ( run, m_ctx );
    }

    {   // verify access to the second of 2 repeated cells
        int64_t rowId;
        uint64_t fragStart;
        uint64_t baseCount;
        int32_t bioNumber;
        NGS_FragmentBlobInfoByOffset ( m_blob, ctx, OffsetIntoRepeatedRowId, & rowId, & fragStart, & baseCount, & bioNumber );
        REQUIRE ( ! FAILED () );
        REQUIRE_EQ ( RepeatedRowId, rowId );
        REQUIRE_EQ ( (uint64_t)8888, fragStart );
        REQUIRE_EQ ( (uint64_t)101, baseCount );
        REQUIRE_EQ ( (int32_t)0, bioNumber );
    }

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlob_MakeFragmentId, FragmentBlobFixture )
{
    ENTRY;
    MakeBlob ( SRA_Accession, 1 );

    NGS_String* id = NGS_FragmentBlobMakeFragmentId ( m_blob, ctx, 2, 1 );
    REQUIRE_EQ ( string ( SRA_Accession ) + ".FR1.2", string ( NGS_StringData ( id, ctx ), NGS_StringSize ( id, ctx ) ) );
    NGS_StringRelease ( id, ctx );

    EXIT;
}

//////////////////////////////////////////// NGS_FragmentBlobIterator

class BlobIteratorFixture : public NGS_C_Fixture
{
public:
    BlobIteratorFixture ()
    :   m_tbl ( 0 ),
        m_blobIt ( 0 )
    {
    }

    void MakeIterator( const char* acc )
    {
        m_tbl = openTable( acc );
        NGS_String* run = NGS_StringMake ( m_ctx, acc, string_size ( acc ) );
        m_blobIt = NGS_FragmentBlobIteratorMake ( m_ctx, run, m_tbl );
        NGS_StringRelease ( run, m_ctx );
    }
    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if ( m_blobIt != 0 )
            {
                NGS_FragmentBlobIteratorRelease ( m_blobIt, m_ctx );
            }
            if ( m_tbl != 0 )
            {
                VTableRelease ( m_tbl );
            }
        }
        NGS_C_Fixture :: Release ();
    }

    const VTable* m_tbl;
    struct NGS_FragmentBlobIterator* m_blobIt;
};

FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_BadMake, BlobIteratorFixture )
{
    ENTRY;

    NGS_String* run = NGS_StringMake ( m_ctx, "", 0 );
    REQUIRE ( ! FAILED () );

    struct NGS_FragmentBlobIterator* blobIt = NGS_FragmentBlobIteratorMake ( ctx, run, NULL );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( blobIt );
    NGS_StringRelease ( run, ctx );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_CreateRelease, BlobIteratorFixture )
{
    ENTRY;
    m_tbl = openTable( SRA_Accession );

    NGS_String* run = NGS_StringMake ( m_ctx, SRA_Accession, string_size ( SRA_Accession ) );
    REQUIRE ( ! FAILED () );
    struct NGS_FragmentBlobIterator* blobIt = NGS_FragmentBlobIteratorMake ( m_ctx, run, m_tbl );
    REQUIRE ( ! FAILED () );
    NGS_StringRelease ( run, m_ctx );
    REQUIRE_NOT_NULL ( blobIt );
    REQUIRE ( ! FAILED () );
    // Release
    NGS_FragmentBlobIteratorRelease ( blobIt, m_ctx );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_DuplicateRelease, BlobIteratorFixture )
{
    ENTRY;
    MakeIterator ( SRA_Accession );

    // Duplicate
    struct NGS_FragmentBlobIterator* anotherBlobIt = NGS_FragmentBlobIteratorDuplicate ( m_blobIt, m_ctx );
    REQUIRE_NOT_NULL ( anotherBlobIt );
    // Release
    NGS_FragmentBlobIteratorRelease ( anotherBlobIt, m_ctx );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_Next, BlobIteratorFixture )
{
    ENTRY;
    MakeIterator ( SRA_Accession );

    struct NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( m_blobIt, m_ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( blob );
    NGS_FragmentBlobRelease ( blob, ctx );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_HasMore, BlobIteratorFixture )
{
    ENTRY;
    MakeIterator ( SRA_Accession );

    REQUIRE ( NGS_FragmentBlobIteratorHasMore ( m_blobIt, m_ctx ) );
    REQUIRE ( ! FAILED () );

    EXIT;
}

#if VDB_3075_has_been_fixed
FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_FullScan, BlobIteratorFixture )
{
    ENTRY;
    MakeIterator ( SRA_Accession );

    uint32_t count = 0;
    while ( NGS_FragmentBlobIteratorHasMore ( m_blobIt, m_ctx ) )
    {
        struct NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( m_blobIt, m_ctx );
        REQUIRE_NOT_NULL ( blob );
        NGS_FragmentBlobRelease ( blob, ctx );
        ++count;
    }
    REQUIRE_EQ ( (uint32_t)243, count);
    REQUIRE_NULL ( NGS_FragmentBlobIteratorNext ( m_blobIt, m_ctx ) );

    EXIT;
}
#endif

FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_SparseTable, BlobIteratorFixture )
{
    ENTRY;
    MakeIterator ( "./data/SparseFragmentBlobs" );
    // only row 1 and 10 are present

    REQUIRE ( NGS_FragmentBlobIteratorHasMore ( m_blobIt, m_ctx ) );
    {
        struct NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( m_blobIt, m_ctx );
        REQUIRE_NOT_NULL ( blob );
        int64_t first = 0;
        uint64_t count = 0;
        NGS_FragmentBlobRowRange ( blob, m_ctx, & first, & count );
        REQUIRE_EQ ( (int64_t)1, first );
        REQUIRE_EQ ( (uint64_t)1, count );
        NGS_FragmentBlobRelease ( blob, ctx );
    }

    REQUIRE ( NGS_FragmentBlobIteratorHasMore ( m_blobIt, m_ctx ) );
    {
        struct NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( m_blobIt, m_ctx );
        REQUIRE_NOT_NULL ( blob );
        int64_t first = 0;
        uint64_t count = 0;
        NGS_FragmentBlobRowRange ( blob, m_ctx, & first, & count );
        REQUIRE_EQ ( (int64_t)10, first );
        REQUIRE_EQ ( (uint64_t)1, count );
        NGS_FragmentBlobRelease ( blob, ctx );
    }

    REQUIRE ( ! NGS_FragmentBlobIteratorHasMore ( m_blobIt, m_ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE ( NGS_FragmentBlobIterator_IteratorRetreats, BlobIteratorFixture )
{   // VDB-2809: NGS_FragmentBlobIterator returns overlapping blobs on CSRA1 accessions
    ENTRY;
    const char* acc = "SRR833251";
    const VDatabase *db = openDB( acc );
    REQUIRE_RC ( VDatabaseOpenTableRead ( db, & m_tbl, "SEQUENCE" ) );
    REQUIRE_RC ( VDatabaseRelease ( db ) );

    NGS_String* run = NGS_StringMake ( m_ctx, acc, string_size ( acc ) );
    m_blobIt = NGS_FragmentBlobIteratorMake ( m_ctx, run, m_tbl );
    NGS_StringRelease ( run, m_ctx );

    int64_t rowId = 1;
    while (true)
    {
        struct NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( m_blobIt, m_ctx );
        if ( blob == 0 )
        {
            break;
        }
        int64_t first = 0;
        uint64_t count = 0;
        NGS_FragmentBlobRowRange ( blob, m_ctx, & first, & count );
        REQUIRE_EQ ( rowId, first );
        NGS_FragmentBlobRelease ( blob, ctx );
        rowId = first + count;
    }

    EXIT;
}

//////////////////////////////////////////// Main

int main(int argc, char* argv[]) 
{
    KConfigDisableUserSettings();
    int ret=NgsFragmentBlobTestSuite(argc, argv);
    NGS_C_Fixture::ReleaseCache();
    return ret;
}
