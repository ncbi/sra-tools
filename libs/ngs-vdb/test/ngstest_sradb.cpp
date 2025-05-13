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
* Unit tests for NGS C interface, SRADB archives
*/

#include "ngs_c_fixture.hpp"

#include <os-native.h>

#include <limits.h>

#include <kapp/main.h>

#include <klib/printf.h>

#include <kdb/manager.h>

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <vdb/manager.h>
#include <vdb/vdb-priv.h>

#include "../ncbi/ngs/NGS_FragmentBlobIterator.h"
#include "../ncbi/ngs/NGS_FragmentBlob.h"

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsSradbTestSuite);

const char* SRADB_Accession = "SRR600096";
uint64_t SRADB_Accession_ReadCount = 16;

class SRADB_Fixture : public NGS_C_Fixture {};

// SRADB_ReadCollection
FIXTURE_TEST_CASE(SRADB_ReadCollection_Open, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE_NOT_NULL ( m_coll);
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReadCount, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE_EQ ( SRADB_Accession_ReadCount, NGS_ReadCollectionGetReadCount ( m_coll, ctx, true, true, true ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetRead, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    m_read = NGS_ReadCollectionGetRead ( m_coll, ctx, ReadId ( 1 ) . c_str () );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads_Filtered_Aligned, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false);
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads_Filtered_Unaligned, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, false, false, true);
    REQUIRE ( ! FAILED () && m_read );

    uint64_t count = 0;
    while ( NGS_ReadIteratorNext ( m_read, ctx ) )
    {
        REQUIRE ( NGS_ReadGetReadCategory ( m_read, ctx ) == NGS_ReadCategory_unaligned && ! FAILED () );
        ++count;
    }
    REQUIRE_EQ ( SRADB_Accession_ReadCount, count );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetReadRange, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 2, 3, true, true, true );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}
FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReadRange_Filtered, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 1, 200000, true, true, false);
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetName, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE_STRING ( SRADB_Accession, NGS_ReadCollectionGetName ( m_coll, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetReferences, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    NGS_Reference* ref = NGS_ReadCollectionGetReferences ( m_coll, ctx );
    REQUIRE ( ! FAILED () && ref);

    REQUIRE_NULL ( NGS_ReferenceGetCommonName ( ref, ctx ) );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_ReferenceIteratorNext ( ref, ctx ) );

    NGS_ReferenceRelease ( ref, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetReference, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE_NULL ( NGS_ReadCollectionGetReference ( m_coll, ctx, "wont find me anyways" ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetAlignments, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    NGS_Alignment* align = NGS_ReadCollectionGetAlignments ( m_coll, ctx, true, true );
    REQUIRE ( ! FAILED () && align );

    REQUIRE_NULL ( NGS_AlignmentGetReferenceSpec ( align, ctx ) );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_AlignmentIteratorNext ( align, ctx ) );

    NGS_AlignmentRelease ( align, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetAlignment, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE_NULL ( NGS_ReadCollectionGetAlignment ( m_coll, ctx, "1" ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetAlignmentCount, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE_EQ ( (uint64_t)0, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, true, true ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetAlignmentRange, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    NGS_Alignment* align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 1, 2, true, true );
    REQUIRE ( ! FAILED () && align);

    REQUIRE_NULL ( NGS_AlignmentGetReferenceSpec ( align, ctx ) );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_AlignmentIteratorNext ( align, ctx ) );

    NGS_AlignmentRelease ( align, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetReadGroups, SRADB_Fixture)
{   // in SRADB archives, a single read group including the whole run
    ENTRY_ACC(SRADB_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE ( ! NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetReadGroup_NotFound, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE_NULL ( NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "wontfindme" ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionGetReadGroup_Found, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "A1DLC.1" );
    REQUIRE_NOT_NULL ( m_readGroup );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionHasReadGroup_NotFound, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE ( ! NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "wontfindme" ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollectionHasReadGroup_Found, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    REQUIRE ( NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "A1DLC.1" ) );
    EXIT;
}

// NGS_Read

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadName, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    REQUIRE_STRING ( string ( "1" ),  NGS_ReadGetReadName ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadId, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    REQUIRE_STRING ( ReadId ( 1 ), NGS_ReadGetReadId ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadGroup, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    REQUIRE_STRING ( string ( "A1DLC.1" ),  NGS_ReadGetReadGroup ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadCategory, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);

    enum NGS_ReadCategory cat = NGS_ReadGetReadCategory ( m_read, ctx );
    REQUIRE_EQ ( NGS_ReadCategory_unaligned, cat );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadSequence_Full, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);

    string expected (
        "TACGGAGGGGGCTAGCGTTGCTCGGAATTACTGGGCGTAAAGGGCGCGTAGGCGGACAGTTAAGTCGGGGGTGAAAGCCCCGGGCTCAACCTCGGAATTG"
        "CCTTCGATACTGGCTGGCTTGAGTACGGTAGAGGGGGGTGGAACTCCTAGTGTAGAGGTGAAATTCGTAGAGATTCCTGTTTGCTCCCCACGCTTTCGCG"
        "CCTCAGCGTCAGTAACGGTCCAGTGTGTCGCCTTCGCCACTGGTACTCTTCCTGCTATCTACGCATCTCATTCTACACACGTCGCGCGCCACACCTCTCT"
        "AACACACGTGACACAGCCACTCTCTGCCGTTACTTCGCTGCTCTGCCGCC"
    );
    REQUIRE_STRING( expected,  NGS_ReadGetReadSequence ( m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadSequence_PartialNoLength, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    string expected ("AACACACGTGACACAGCCACTCTCTGCCGTTACTTCGCTGCTCTGCCGCC");
    REQUIRE_STRING( expected,  NGS_ReadGetReadSequence ( m_read, ctx, 300, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadSequence_PartialLength, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    REQUIRE_STRING ( "AACACA",  NGS_ReadGetReadSequence ( m_read, ctx, 300, 6 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadQualities_Full, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);

    string expected(
        "====,<==@7<@<@@@CEE=CCEECCEEEEFFFFEECCCEEF=EEDDCCDCEEDDDDDEEEEDDEE*=))9;;=EE(;?E(4;<<;<<EEEE;--'9<;?"
        "EEE=;E=EE<E=EE<9(9EE;(6<?E#################################################?????BBBBDDDDDBBDEEFFFEE7"
        "CHH58E=EECCEG///75,5CF-5A5-5C@FEEDFDEE:E--55----5,@@@,,5,5@=?7?#####################################"
        "##################################################"
    );
    REQUIRE_STRING ( expected,  NGS_ReadGetReadQualities ( m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadQualities_PartialNoLength, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    string expected("##################################################");
    REQUIRE_STRING ( expected,  NGS_ReadGetReadQualities ( m_read, ctx, 300, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadGetReadQualities_PartialLength, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    REQUIRE_STRING ( "CHH58E=EEC",  NGS_ReadGetReadQualities ( m_read, ctx, 200, 10 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_ReadNumFragments, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);

    uint32_t num = NGS_ReadNumFragments ( m_read, ctx );
    REQUIRE_EQ ( (uint32_t)2, num );

    EXIT;
}

// NGS_Fragment (through an NGS_Read object)
FIXTURE_TEST_CASE(SRADB_NGS_NoFragmentBeforeNext, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetRead ( m_coll, ctx, ReadId ( 1 ) . c_str () );
    REQUIRE ( ! FAILED () );
    // no access to a Fragment before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_FragmentGetId, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    REQUIRE_STRING ( string( SRADB_Accession ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_FragmentGetSequence_Full, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);

    REQUIRE ( ! FAILED () );
    string expected(
        "TACGGAGGGGGCTAGCGTTGCTCGGAATTACTGGGCGTAAAGGGCGCGTAGGCGGACAGTTAAGTCGGGGGTGAAAGCCCCGGGCTCAACCTCGGAATTG"
        "CCTTCGATACTGGCTGGCTTGAGTACGGTAGAGGGGGGTGGAACTCCTAGTGTAGAGGTGAAATTCGTAGAGATT"
    );
    REQUIRE_STRING ( expected,  NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_FragmentGetSequence_PartialNoLength, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    string expected = "CCTTCGATACTGGCTGGCTTGAGTACGGTAGAGGGGGGTGGAACTCCTAGTGTAGAGGTGAAATTCGTAGAGATT";
    REQUIRE_STRING ( expected,  NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 100, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_FragmentGetSequence_PartialLength, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);
    REQUIRE_STRING ( "CCTT",  NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 100, 4 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_FragmentGetQualities_Full, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);

    string expected(
        "====,<==@7<@<@@@CEE=CCEECCEEEEFFFFEECCCEEF=EEDDCCDCEEDDDDDEEEEDDEE*=))9;;=EE(;?E(4;<<;<<EEEE;--'9<;?"
        "EEE=;E=EE<E=EE<9(9EE;(6<?E#################################################"
    );
    REQUIRE_STRING ( expected,  NGS_FragmentGetQualities ( (NGS_Fragment*)m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

// Iteration over Fragments
FIXTURE_TEST_CASE(SRADB_NGS_FragmentIteratorNext, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1); // calls NGS_FragmentIteratorNext

    REQUIRE_STRING ( string( SRADB_Accession ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

    REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( string( SRADB_Accession ) + ".FR1.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_FragmentIteratorNext_NullRead, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false ); // will return an empty iterator
    REQUIRE ( ! FAILED () );

    NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_FragmentIteratorNext_BeyondEnd, SRADB_Fixture)
{
    ENTRY_GET_READ(SRADB_Accession, 1);

    REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );

    EXIT;
}

// Iteration over Reads
FIXTURE_TEST_CASE(SRADB_ReadIterator_NoReadBeforeNext, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );
    // no access to a Read before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads_Next, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    // on the first m_read
    REQUIRE_STRING ( ReadId ( 1 ), NGS_ReadGetReadId ( m_read, ctx ) );

    {   // iterate over fragments
        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) ); // position on the first fragment
        REQUIRE ( ! FAILED () );

        REQUIRE_STRING ( string( SRADB_Accession ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE ( ! FAILED () );

        REQUIRE_STRING ( string( SRADB_Accession ) + ".FR1.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE ( ! FAILED () );
    }

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    // on the second m_read
    REQUIRE_STRING ( ReadId ( 2 ), NGS_ReadGetReadId ( m_read, ctx ) );

    {   // iterate over fragments
        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) ); // position on the first fragment
        REQUIRE ( ! FAILED () );

        REQUIRE_STRING ( string( SRADB_Accession ) + ".FR0.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE ( ! FAILED () );

        REQUIRE_STRING ( string( SRADB_Accession ) + ".FR1.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE ( ! FAILED () );
    }

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads_All, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    // on the first m_read
    string id = toString ( NGS_ReadGetReadId ( m_read, ctx ), ctx, true );
    while ( NGS_ReadIteratorNext ( m_read, ctx ) )
    {
        REQUIRE ( ! FAILED () );
        id = toString ( NGS_ReadGetReadId ( m_read, ctx ), ctx, true );
        REQUIRE ( ! FAILED () );
    }
    REQUIRE_EQ ( ReadId ( SRADB_Accession_ReadCount ), id );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReads_None, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false );
    REQUIRE ( ! FAILED () && m_read );

    NGS_ReadGetReadId ( m_read, ctx );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_NGS_GetReadsIteratorNext_BeyondEnd, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false ); // empty
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

// Iteration over a range of Reads
FIXTURE_TEST_CASE(SRADB_ReadRange_NoReadBeforeNext, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 3, 2, true, true, true );
    REQUIRE ( ! FAILED () && m_read );
    // no access to a Read before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReadRange, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 3, 2, true, true, true );
    REQUIRE ( ! FAILED () );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 3 ), NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 4 ), NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadCollection_GetReadRange_Empty, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 6000000, 2, true, true, true );
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

// ReadGroup

FIXTURE_TEST_CASE(SRA_ReadGroupGetName, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "A1DLC.1" );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "A1DLC.1", NGS_ReadGroupGetName ( m_readGroup, ctx ) );

    EXIT;
}


#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetReads, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "A1DLC.1" );

    m_read = NGS_ReadGroupGetReads ( m_readGroup, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 1 ), NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 2 ), NGS_ReadGetReadId ( m_read, ctx ) );

    EXIT;
}
#endif

#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetReads_Filtered, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "A1DLC.1" );

    m_read = NGS_ReadGroupGetReads ( m_readGroup, ctx, true, true, false ); // no unaligned
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) ); // no reads

    EXIT;
}
#endif

#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetRead, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "A1DLC.1" );

    m_read = NGS_ReadGroupGetRead ( m_readGroup, ctx, ReadId ( 3 ) . c_str () );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE_STRING ( ReadId ( 3 ), NGS_ReadGetReadId ( m_read, ctx ) );

    EXIT;
}
#endif

// Iteration over ReadGroups

FIXTURE_TEST_CASE(SRADB_ReadGroupIterator_NoGroupBeforeNext, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );

    REQUIRE_NULL ( NGS_ReadGroupGetName ( m_readGroup, ctx ) );
    REQUIRE_FAILED ();

#if READ_GROUP_SUPPORTS_READS
    REQUIRE_NULL ( NGS_ReadGroupGetRead ( m_readGroup, ctx, ReadId ( 3 ) . c_str () ) );
    REQUIRE_FAILED ();

    REQUIRE_NULL ( NGS_ReadGroupGetReads ( m_readGroup, ctx, true, true, true ) );
    REQUIRE_FAILED ();
#endif

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadGroupIterator_AfterNext, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "A1DLC.1", NGS_ReadGroupGetName ( m_readGroup, ctx ) );

    EXIT;
}


FIXTURE_TEST_CASE(SRADB_ReadGroupIterator_BeyondEnd, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);
    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );

    REQUIRE ( ! NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_NULL ( NGS_ReadGroupGetName ( m_readGroup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

// Statistics
FIXTURE_TEST_CASE(SRADB_ReadCollectionGetStatistics, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    NGS_Statistics * stats = NGS_ReadCollectionGetStatistics ( m_coll, ctx );
    REQUIRE ( ! FAILED () );

    REQUIRE_EQ ( (uint64_t)5600,    NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)5600,    NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)16,      NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)16,      NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)1,       NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_MIN" ) );

    const char* bam =
        "@HD\tVN:1.0\tSO:queryname\n"
        "@RG\tID:A1DLC.1\tPL:illumina\tPU:A1DLC120809.1.AATGAGCCCACG\tLB:Solexa-112136\tPI:393\tDT:2012-08-09T00:00:00-0400\tSM:12341_SN_05_1\tCN:BI\n"
        "@PG\tID:bwa.negative.screen\tPN:bwa\tVN:0.5.9-tpx\tCL:bwa aln Screening.negative.Homo_sapiens_GRCh37.1_p5.fasta"
        " -q 5 -l 32 -k 2 -t $NSLOTS -o 1 -f illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.Screening.negative.Homo_sapiens_GRCh37.1_p5.2.sai"
        " illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.2.fastq.gz; bwa aln Screening.negative.Homo_sapiens_GRCh37.1_p5.fasta -q 5 -l 32 -k 2"
        " -t $NSLOTS -o 1 -f illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.Screening.negative.Homo_sapiens_GRCh37.1_p5.1.sai"
        " illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.1.fastq.gz; bwa sampe -t $NSLOTS -T -P -a 589 -f"
        " illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.Screening.negative.Homo_sapiens_GRCh37.1_p5.aligned_bwa.sam"
        " Screening.negative.Homo_sapiens_GRCh37.1_p5.fasta illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.Screening.negative.Homo_sapiens_GRCh37.1_p5.1.sai"
        " illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.Screening.negative.Homo_sapiens_GRCh37.1_p5.2.sai"
        " illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.1.fastq.gz illuminaReadGroupBamScreening.G21523.A1DLC120809.1.Illumina_P7-Bonijeci.BI6212661.negative.2.fastq.gz\n";

    REQUIRE_STRING ( string ( bam ), NGS_StatisticsGetAsString ( stats, ctx, "BAM_HEADER" ) );

    NGS_StatisticsRelease ( stats, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRADB_ReadGroupGetStatistics, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "A1DLC.1" );
    REQUIRE ( ! FAILED () );

    NGS_Statistics * stats = NGS_ReadGroupGetStatistics ( m_readGroup, ctx );
    REQUIRE ( ! FAILED () );

    REQUIRE_EQ ( (uint64_t)5600,    NGS_StatisticsGetAsU64 ( stats, ctx, "BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)5600,    NGS_StatisticsGetAsU64 ( stats, ctx, "BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)16,      NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)16,      NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)1,       NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_MIN" ) );

    NGS_StatisticsRelease ( stats, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(PACBIO_ThrowsOnGetReadId, SRADB_Fixture)
{   // VDB-2668
    ENTRY_ACC("SRR287782");

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE_NOT_NULL ( m_read );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE_STRING ( "SRR287782.R.1", NGS_ReadGetReadName ( m_read, ctx ) ); // VDB-2668: throws "no NAME-column found"

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE_STRING ( "SRR287782.R.2", NGS_ReadGetReadName ( m_read, ctx ) ); // VDB-2668, review: does not update on Next

    EXIT;
}

// Fragment Blobs

FIXTURE_TEST_CASE(SRADB_GetFragmentBlobs, SRADB_Fixture)
{
    ENTRY_ACC(SRADB_Accession);

    NGS_FragmentBlobIterator* blobIt = NGS_ReadCollectionGetFragmentBlobs ( m_coll, ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( blobIt );

    NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( blobIt, ctx );

    REQUIRE_EQ ( (uint64_t)5600, NGS_FragmentBlobSize ( blob, ctx ) );

    NGS_FragmentBlobRelease ( blob, ctx );
    NGS_FragmentBlobIteratorRelease ( blobIt, ctx );

    EXIT;
}

//////////////////////////////////////////// Main
int main(int argc, char* argv[])
{
    KConfigDisableUserSettings();
    setenv("NCBI_VDB_QUALITY", "R", 1);
    int ret=NgsSradbTestSuite(argc, argv);
    NGS_C_Fixture::ReleaseCache();
    return ret;
}
