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

#include "ngs_c_fixture.hpp"

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <kapp/main.h>

#include <vdb/database.h>
#include <vdb/blob.h>

#include <../ncbi/ngs/NGS_Cursor.h>

#include <../ncbi/ngs/CSRA1_Reference.h>
#include <../ncbi/ngs/VByteBlob.h>

#include <stdexcept>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsByteBlobTestSuite);

const char* CSRA1_Accession = "SRR1063272";
const char* CSRA1_Accession_WithRepeats = "SRR600094";

const uint32_t ChunkSize = 5000;

//////////////////////////////////////////// VByteBlob

class ByteBlobFixture : public NGS_C_Fixture
{
public:
    ByteBlobFixture ()
    :   m_curs ( 0 ),
        m_blob ( 0 ),
        m_data ( 0 ),
        m_size ( 0 ),
        m_rowCount ( 0 )
    {
    }

    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if ( m_blob != 0 )
            {
                VBlobRelease ( m_blob );
            }
            if ( m_curs != 0 )
            {
                NGS_CursorRelease ( m_curs, m_ctx );
            }
        }
        NGS_C_Fixture :: Release ();
    }

    void GetBlob ( const char* p_acc, int64_t p_firstRowId )
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

        if ( m_blob != 0 )
        {
            VBlobRelease ( m_blob );
        }
        m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, p_firstRowId, reference_READ );
    }

    void CheckRange( ctx_t ctx, int64_t p_first, uint64_t p_count )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcReading );
        int64_t first;
        uint64_t count;
        if ( VBlobIdRange ( m_blob, & first, & count ) != 0 )
        {
            throw std :: logic_error ( "VBlobRowRange() failed" );
        }
        if ( p_first != first || p_count != count )
        {
            ostringstream str;
            str << "CheckRange(first=" << p_first << ", count=" << p_count << ") : first=" << first << ", count=" << count;
            throw std :: logic_error ( str.str() );
        }
    }

    const NGS_Cursor* m_curs;
    const struct VBlob* m_blob;

    const void* m_data;
    uint64_t m_size;
    uint64_t m_rowCount;
};

// void VByteBlob_ContiguousChunk ( const struct VBlob* blob,  ctx_t ctx, int64_t rowId, uint64_t maxRows, const void** data, uint64_t* size, bool stopAtRepeat );
FIXTURE_TEST_CASE ( VByteBlob_BadRowId, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    VByteBlob_ContiguousChunk ( m_blob, ctx, 0, 0, false, &m_data, &m_size, 0 );
    REQUIRE_FAILED();

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_GoodRowId, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    VByteBlob_ContiguousChunk ( m_blob, ctx, 1, 0, false, &m_data, &m_size, &m_rowCount );
    REQUIRE ( ! FAILED() );
    CheckRange ( ctx, 1, 4 );
    REQUIRE_EQ ( ( uint64_t ) 4, m_rowCount );

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_LargeRowId, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    VByteBlob_ContiguousChunk ( m_blob, ctx, 4000, 0, false, &m_data, &m_size, 0 ); // accession row-count = 3,781
    REQUIRE_FAILED();

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_StopAtRepeat_NoRepeat, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    VByteBlob_ContiguousChunk ( m_blob, ctx, 1, 0, true, &m_data, &m_size, & m_rowCount );
    REQUIRE ( ! FAILED() );
    CheckRange ( ctx, 1, 4 );
    REQUIRE_EQ ( ( uint64_t ) 4, m_rowCount );
    REQUIRE_EQ ( ( uint64_t ) m_rowCount * ChunkSize, m_size );

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_StopAtRepeat_YesRepeat, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 1 ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 5, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 9, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 13, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 17, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 33, reference_READ ); // this blob has 9 repeated rows, 37-45; 16 rows total
    CheckRange ( ctx, 33, 16 ); // full blob

    VByteBlob_ContiguousChunk ( m_blob, ctx, 33, 0, true, &m_data, &m_size, & m_rowCount );
    REQUIRE ( ! FAILED() );
    REQUIRE_EQ ( ( uint64_t ) 5, m_rowCount ); // only 5 first rows included into the contiguous chunk (up to the first repetition)
    REQUIRE_EQ ( ( uint64_t ) m_rowCount * ChunkSize, m_size );

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_NoStopAtRepeat_YesRepeat, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 1 ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 5, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 9, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 13, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 17, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 33, reference_READ ); // this blob has 9 repeated rows, 37-45; 16 rows total, 8 packed

    VByteBlob_ContiguousChunk ( m_blob, ctx, 33, 0, false, &m_data, &m_size, & m_rowCount ); // ignore repeats
    REQUIRE ( ! FAILED() );
    REQUIRE_EQ ( ( uint64_t ) 16, m_rowCount ); // all rows included: 8 = 7 unpacked + 9 packed into 1
    REQUIRE_EQ ( ( uint64_t ) 8 * ChunkSize, m_size );  // 16 rows packed into 8 chunks

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_MaxRows_MoreThanPresent, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    VByteBlob_ContiguousChunk ( m_blob, ctx, 1, 10, false, &m_data, &m_size, & m_rowCount );
    REQUIRE ( ! FAILED() );
    CheckRange ( ctx, 1, 4 );
    REQUIRE_EQ ( ( uint64_t ) 4, m_rowCount );
    REQUIRE_EQ ( ( uint64_t ) m_rowCount * ChunkSize, m_size );

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_MaxRows_LessThanPresent, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession, 1 );

    VByteBlob_ContiguousChunk ( m_blob, ctx, 1, 2, false, &m_data, &m_size, & m_rowCount );
    REQUIRE ( ! FAILED() );
    REQUIRE_EQ ( ( uint64_t ) 2, m_rowCount );
    REQUIRE_EQ ( ( uint64_t ) m_rowCount * ChunkSize, m_size );

    EXIT;
}


FIXTURE_TEST_CASE ( VByteBlob_MaxRows_LessThanPresent_WithRepeatsAfter, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 1 ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 5, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 9, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 13, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 17, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 33, reference_READ ); // this blob has 9 repeated rows, 37-45; 16 rows total, 8 packed

    VByteBlob_ContiguousChunk ( m_blob, ctx, 33, 4, true, &m_data, &m_size, & m_rowCount );
    REQUIRE ( ! FAILED() );
    REQUIRE_EQ ( ( uint64_t ) 4, m_rowCount ); // 4 rows (repeats come after)
    REQUIRE_EQ ( ( uint64_t ) m_rowCount * ChunkSize, m_size );

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_MaxRows_LessThanPresent_WithRepeatsBefore, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 1 ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 5, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 9, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 13, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 17, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 33, reference_READ ); // this blob has 9 repeated rows, 37-45; 16 rows total, 8 packed

    VByteBlob_ContiguousChunk ( m_blob, ctx, 33, 14, true, &m_data, &m_size, & m_rowCount );
    REQUIRE ( ! FAILED() );
    REQUIRE_EQ ( ( uint64_t ) 5, m_rowCount ); // only 5 first rows included into the contiguous chunk (up to the first repetition)
    REQUIRE_EQ ( ( uint64_t ) m_rowCount * ChunkSize, m_size );

    EXIT;
}

FIXTURE_TEST_CASE ( VByteBlob_MaxRows_LessThanPresent_WithRepeatsOverlapping, ByteBlobFixture )
{
    ENTRY;
    GetBlob ( CSRA1_Accession_WithRepeats, 1 ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 5, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 9, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 13, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 17, reference_READ ); VBlobRelease ( m_blob );
    m_blob = NGS_CursorGetVBlob ( m_curs, m_ctx, 33, reference_READ ); // this blob has 9 repeated rows, 37-45; 16 rows total, 8 packed

    VByteBlob_ContiguousChunk ( m_blob, ctx, 33, 7, true, &m_data, &m_size, & m_rowCount ); // 4 rows and 3 of the repeated
    REQUIRE ( ! FAILED() );
    REQUIRE_EQ ( ( uint64_t ) 5, m_rowCount ); // only 5 first rows included into the contiguous chunk (up to the first repetition)
    REQUIRE_EQ ( ( uint64_t ) m_rowCount * ChunkSize, m_size );

    EXIT;
}

//////////////////////////////////////////// Main

int main( int argc, char* argv[] )
{
    KConfigDisableUserSettings();
    int ret=NgsByteBlobTestSuite(argc, argv);
    ByteBlobFixture::ReleaseCache();
    return ret;
}
