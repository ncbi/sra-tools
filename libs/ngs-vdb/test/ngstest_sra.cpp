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
* Unit tests for NGS C interface, SRA archives
*/

#include <klib/debug.h> // KDbgSetString

#include "ngs_c_fixture.hpp"
#include <ktst/unit_test.hpp>

#include <kapp/main.h>

#include "../ncbi/ngs/NGS_ReadCollection.h"
#include "../ncbi/ngs/NGS_FragmentBlobIterator.h"
#include "../ncbi/ngs/NGS_FragmentBlob.h"

#include <kdb/manager.h>

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <vdb/manager.h>
#include <vdb/vdb-priv.h>

#include <klib/printf.h>

#include <limits.h>

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(NgsSraTestSuite);

const char* SRA_Accession = "SRR000001";
const char* SRA_Accession_WithReadGroups = "SRR006061";
uint64_t SRA_Accession_ReadCount = 470985;

class SRA_Fixture : public NGS_C_Fixture { };

// SRA_ReadCollection

FIXTURE_TEST_CASE(SRA_ReadCollection_Open, SRA_Fixture)
{
    ENTRY;
    m_coll = open ( ctx, SRA_Accession );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( m_coll );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_Open_FailsOnReference, SRA_Fixture)
{
    ENTRY;
    const char* SRA_Reference = "NC_000001.10";
    REQUIRE_NULL ( NGS_ReadCollectionMake ( ctx, SRA_Reference ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReadCount, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE_EQ ( SRA_Accession_ReadCount, NGS_ReadCollectionGetReadCount ( m_coll, ctx, true, true, true ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetRead, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    m_read = NGS_ReadCollectionGetRead ( m_coll, ctx, ReadId ( 1 ) . c_str () );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads_All, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads_Filtered_Aligned, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false);
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads_Filtered_Unaligned, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, false, false, true);
    REQUIRE ( ! FAILED () && m_read );
    uint64_t count = 0;
    while ( NGS_ReadIteratorNext ( m_read, ctx ) )
    {
        REQUIRE ( NGS_ReadGetReadCategory ( m_read, ctx ) == NGS_ReadCategory_unaligned && ! FAILED () );
        ++count;
    }
    REQUIRE_EQ ( SRA_Accession_ReadCount, count );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReadRange, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 2, 3, true, true, true );
    REQUIRE_NOT_NULL ( m_read );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetName, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE_STRING ( SRA_Accession, NGS_ReadCollectionGetName ( m_coll, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReferences, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    NGS_Reference * ref = NGS_ReadCollectionGetReferences ( m_coll, ctx );
    REQUIRE ( ! FAILED () && ref );

    REQUIRE_NULL ( NGS_ReferenceGetCommonName ( ref, ctx ) );
    REQUIRE_FAILED();

    REQUIRE ( ! NGS_ReferenceIteratorNext ( ref, ctx ) );

    NGS_ReferenceRelease ( ref, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReference, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE_NULL ( NGS_ReadCollectionGetReference ( m_coll, ctx, "wont find me anyways" ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetAlignments, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    NGS_Alignment * align = NGS_ReadCollectionGetAlignments ( m_coll, ctx, true, true );
    REQUIRE ( ! FAILED () && align );

    REQUIRE_NULL ( NGS_AlignmentGetReferenceSpec ( align, ctx ) );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_AlignmentIteratorNext ( align, ctx ) );

    NGS_AlignmentRelease ( align, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetAlignment, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE_NULL ( NGS_ReadCollectionGetAlignment ( m_coll, ctx, ReadId ( 1 ) . c_str () ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetAlignmentCount, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE_EQ ( (uint64_t)0, NGS_ReadCollectionGetAlignmentCount ( m_coll, ctx, true, true ) );
    EXIT;
}
FIXTURE_TEST_CASE(SRA_ReadCollectionGetAlignmentRange, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    NGS_Alignment * align = NGS_ReadCollectionGetAlignmentRange ( m_coll, ctx, 1, 2, true, true );
    REQUIRE ( ! FAILED () && align );

    REQUIRE_NULL ( NGS_AlignmentGetReferenceSpec ( align, ctx ) );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_AlignmentIteratorNext ( align, ctx ) );

    NGS_AlignmentRelease ( align, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReadGroups_NoGroups, SRA_Fixture)
{   // in some SRA archives, a single read group including the whole run
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE ( ! NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReadGroups_WithGroups, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );

    // etc

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReadGroup_NotFound, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE_NULL ( NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "wontfindme" ) );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReadGroup_NoGroups_DefaultFound_ByEmptyString, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" );
    REQUIRE_NOT_NULL ( m_readGroup );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReadGroup_WithGroups_DefaultFound, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);
    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" );
    REQUIRE_NOT_NULL ( m_readGroup );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionGetReadGroup_WithGroups_Found, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);
    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "S77_V2");
    REQUIRE_NOT_NULL ( m_readGroup );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionHasReadGroup_NotFound, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE ( ! NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "wontfindme" ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionHasReadGroup_NoGroups_DefaultFound_ByEmptyString, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    REQUIRE ( NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "" ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionHasReadGroup_WithGroups_DefaultFound, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);
    REQUIRE ( NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "" ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollectionHasReadGroup_WithGroups_Found, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);
    REQUIRE ( NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "S77_V2") );
    EXIT;
}

// NGS_Read

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadName, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    REQUIRE_STRING ( "EM7LVYS02FOYNU", NGS_ReadGetReadName ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadId, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    REQUIRE_STRING ( ReadId ( 1 ), NGS_ReadGetReadId ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadGroup_Default, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    REQUIRE_STRING ( "", NGS_ReadGetReadGroup ( m_read, ctx ));
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadGroup, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession_WithReadGroups, 1);
    REQUIRE_STRING ( "S103_V2", NGS_ReadGetReadGroup ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadCategory, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);

    enum NGS_ReadCategory cat = NGS_ReadGetReadCategory ( m_read, ctx );
    REQUIRE_EQ ( NGS_ReadCategory_unaligned, cat );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadSequence_Full, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);

    string expected (
        "TCAGATTCTCCTAGCCTACATCCGTACGAGTTAGCGTGGGATTACGAGGTGCACACCATTTCATTCCGTACGGGTAAATT"
        "TTTGTATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCTGACCTCAAGTGATTTG"
        "CCTGCCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTATAA"
        "ATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG");
    REQUIRE_STRING ( expected, NGS_ReadGetReadSequence ( m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadSequence_PartialNoLength, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    string expected ("TGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTATAAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG" );
    REQUIRE_STRING ( expected,  NGS_ReadGetReadSequence ( m_read, ctx, 200, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadSequence_PartialLength, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    string expected ("TGAGCGAGTT");
    REQUIRE_STRING ( expected, NGS_ReadGetReadSequence ( m_read, ctx, 200, 10 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadQualities_Full, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);

    string expected (
        "=<8<85)9=9/3-8?68<7=8<3657747==49==+;FB2;A;5:'*>69<:74)9.;C?+;<B<B;(<';FA/;C>*GC"
        "8/%9<=GC8.#=2:5:16D==<EA2EA.;5=44<;2C=5;@73&<<2;5;6+9<?776+:24'26:7,<9A;=:;0C>*6"
        "?7<<C=D=<52?:9CA2CA23<2<;3CA12:A<9414<7<<6;99<2/=9#<;9B@27.;=6>:77>:1<A>+CA138?<"
        ")C@2166:A<B?->:%<<9<;33<;6?9;<;4=:%<$CA1+1%1");
    REQUIRE_STRING ( expected, NGS_ReadGetReadQualities ( m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadQualities_PartialNoLength, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    string expected ( "<6;99<2/=9#<;9B@27.;=6>:77>:1<A>+CA138?<)C@2166:A<B?->:%<<9<;33<;6?9;<;4=:%<$CA1+1%1" );
    REQUIRE_STRING ( expected, NGS_ReadGetReadQualities ( m_read, ctx, 200, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadGetReadQualities_PartialLength, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    REQUIRE_STRING ( "<6;99<2/=9", NGS_ReadGetReadQualities ( m_read, ctx, 200, 10 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadNumFragments, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    uint32_t num = NGS_ReadNumFragments ( m_read, ctx );
    REQUIRE_EQ ( (uint32_t)1, num ); // VDB-3132: only count non-empty fragments
    EXIT;
}

// NGS_Fragment (through an NGS_Read object)
FIXTURE_TEST_CASE(SRA_NGS_NoFragmentBeforeNext, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetRead ( m_coll, ctx, ReadId ( 1 ) . c_str () );

    // no access to a Fragment before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_FragmentGetQualities ( (NGS_Fragment*)m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentGetId, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    REQUIRE_STRING ( string( SRA_Accession ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentGetSequence_Full, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    string expected =
        "ATTCTCCTAGCCTACATCCGTACGAGTTAGCGTGGGATTACGAGGTGCACACCATTTCATTCCGTACGGGTAAATTTTTG"
        "TATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCTGACCTCAAGTGATTTGCCTG"
        "CCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTAT"
        "AAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG";
    REQUIRE_STRING ( expected, NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 0, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentGetSequence_PartialNoLength, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    REQUIRE_STRING (
        "TATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCTGACCTCAAGTGATTTGCCTG"
        "CCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTAT"
        "AAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG",
        NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 80, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentGetSequence_PartialLength, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    REQUIRE_STRING ( "TATT",  NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 80, 4 ) );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentGetQualities_Full, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);
    string expected =
        "85)9=9/3-8?68<7=8<3657747==49==+;FB2;A;5:'*>69<:74)9.;C?+;<B<B;(<';FA/;C>*GC8/%9"
        "<=GC8.#=2:5:16D==<EA2EA.;5=44<;2C=5;@73&<<2;5;6+9<?776+:24'26:7,<9A;=:;0C>*6?7<<"
        "C=D=<52?:9CA2CA23<2<;3CA12:A<9414<7<<6;99<2/=9#<;9B@27.;=6>:77>:1<A>+CA138?<)C@2"
        "166:A<B?->:%<<9<;33<;6?9;<;4=:%<$CA1+1%1";
    REQUIRE_STRING ( expected,  NGS_FragmentGetQualities ( (NGS_Fragment*)m_read, ctx, 0, (size_t)-1 ) );
    EXIT;
}

// Iteration over Fragments
FIXTURE_TEST_CASE(SRA_NGS_FragmentIteratorNext, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 2); // calls NGS_FragmentIteratorNext

    REQUIRE_STRING ( string ( SRA_Accession ) + ".FR0.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

    REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( string ( SRA_Accession ) + ".FR1.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentIteratorNext_SkipEmpty, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1); // calls NGS_FragmentIteratorNext

    REQUIRE_STRING ( string ( SRA_Accession ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    // VDB-3132: skip empty FR1.1
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentIterator_NullRead, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false ); // will return an empty iterator
    REQUIRE ( ! FAILED () );

    NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_FragmentGetQualities ( (NGS_Fragment*)m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_FragmentIteratorNext_BeyondEnd, SRA_Fixture)
{
    ENTRY_GET_READ(SRA_Accession, 1);  // calls NGS_FragmentIteratorNext
    // row 1 contains only 1 non-empty bio fragment
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    // no access through the iterator after the end
    NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_FragmentGetQualities ( (NGS_Fragment*)m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );

    EXIT;
}

// Iteration over Reads
FIXTURE_TEST_CASE(SRA_ReadIterator_NoReadBeforeNext, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );
    // no access to a Read before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadName ( m_read, ctx ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadGroup ( m_read, ctx ) );
    REQUIRE_FAILED ();
    NGS_ReadGetReadCategory ( m_read, ctx );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadSequence ( m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadQualities ( m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();
    NGS_ReadNumFragments ( m_read, ctx );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads_Next, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    // on the first m_read
    REQUIRE_STRING ( ReadId ( 1 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    {   // iterate over fragments
        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) ); // position on the first fragment
        REQUIRE ( ! FAILED () );

        REQUIRE_STRING ( string ( SRA_Accession ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
        // the second bio read is empty and not reported
        REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE ( ! FAILED () );
    }

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    // on the second m_read
    REQUIRE_STRING ( ReadId ( 2 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    {   // iterate over fragments
        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) ); // position on the first fragment
        REQUIRE ( ! FAILED () );

        REQUIRE_STRING ( string ( SRA_Accession ) + ".FR0.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE ( ! FAILED () );

        REQUIRE_STRING ( string ( SRA_Accession ) + ".FR1.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE ( ! FAILED () );
    }

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads_None, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_GetReadsIteratorNext_BeyondEnd, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false ); // empty
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    // no access through the iterator after the end
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadName ( m_read, ctx ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadGroup ( m_read, ctx ) );
    REQUIRE_FAILED ();
    NGS_ReadGetReadCategory ( m_read, ctx );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadSequence ( m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();
    REQUIRE_NULL ( NGS_ReadGetReadQualities ( m_read, ctx, 0, 1 ) );
    REQUIRE_FAILED ();
    NGS_ReadNumFragments ( m_read, ctx );
    REQUIRE_FAILED ();

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_NGS_ReadIteratorGetCount, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);
    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );

    REQUIRE_EQ( SRA_Accession_ReadCount, NGS_ReadIteratorGetCount ( m_read, ctx ) );
    // read count does not change after Next()
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE_EQ( SRA_Accession_ReadCount, NGS_ReadIteratorGetCount ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReads_NumFragments, SRA_Fixture)
{    // bug report: NumFreagment increases with each call to NGS_ReadIteratorNext
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) ); // skip row 1 which has only 1 non-empty bio fragment
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );

    REQUIRE_STRING ( ReadId ( 2 ),  NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint32_t)2, NGS_ReadNumFragments ( m_read, ctx ) );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_EQ ( (uint32_t)2, NGS_ReadNumFragments ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    EXIT;
}

// Iteration over a range of Reads
FIXTURE_TEST_CASE(SRA_ReadRange_NoReadBeforeNext, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 3, 2, true, true, true );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( m_read );
    // no access to a Read before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReadRange, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 3, 2, true, true, true );
    REQUIRE ( ! FAILED () );
    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 3 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 4 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadCollection_GetReadRange_Empty, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 6000000, 2, true, true, true );
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}
FIXTURE_TEST_CASE(SRA_ReadCollection_GetReadRange_Filtered, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 1, 200000, true, true, false);
    REQUIRE ( ! FAILED () );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

// ReadGroup

FIXTURE_TEST_CASE(SRA_ReadGroupGetName_Default, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "", NGS_ReadGroupGetName ( m_readGroup, ctx ) );

    EXIT;
}

#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetReads_NoGroups, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" );

    m_read = NGS_ReadGroupGetReads ( m_readGroup, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 1 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 2 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    EXIT;
}
#endif

#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetReads_WithGroups, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "S77_V2" );
    REQUIRE ( ! FAILED () );

    m_read = NGS_ReadGroupGetReads ( m_readGroup, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 29 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_STRING ( ReadId ( 30 ),  NGS_ReadGetReadId ( m_read, ctx ) );
    // etc

    EXIT;
}
#endif

#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetReads_Filtered, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" );

    m_read = NGS_ReadGroupGetReads ( m_readGroup, ctx, true, true, false ); // no unaligned
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) ); // no reads

    EXIT;
}
#endif

#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetRead, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" );

    m_read = NGS_ReadGroupGetRead ( m_readGroup, ctx, ReadId ( 1 ) . c_str () );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE_STRING ( ReadId ( 1 ),  NGS_ReadGetReadId ( m_read, ctx ) );

    EXIT;
}
#endif

#if READ_GROUP_SUPPORTS_READS
FIXTURE_TEST_CASE(SRA_ReadGroupGetRead_WrongReadGroup, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "S77_V2" ); // reads 29 through 125

    REQUIRE_NULL ( NGS_ReadGroupGetRead ( m_readGroup, ctx, ReadId ( 126 ) . c_str () ) );
    REQUIRE_FAILED ();

    EXIT;
}
#endif

FIXTURE_TEST_CASE(SRA_ReadGroupGetStatistics_default, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "" );
    REQUIRE ( ! FAILED () );
    NGS_Statistics * stats = NGS_ReadGroupGetStatistics ( m_readGroup, ctx );
    REQUIRE ( ! FAILED () && stats );

	NGS_StatisticsRelease ( stats, ctx );
    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadGroupGetStatistics_named, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "S77_V2" ); // reads 29 through 125
    REQUIRE ( ! FAILED () && m_readGroup );
    NGS_Statistics * stats = NGS_ReadGroupGetStatistics ( m_readGroup, ctx );
    REQUIRE ( ! FAILED () && stats );

	NGS_StatisticsRelease ( stats, ctx );
    EXIT;
}


// Iteration over ReadGroups

FIXTURE_TEST_CASE(SRA_ReadGroupIterator_NoGroupBeforeNext, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE_NULL ( NGS_ReadGroupGetName ( m_readGroup, ctx ) );
    REQUIRE_FAILED ();

#if READ_GROUP_SUPPORTS_READS
    REQUIRE_NULL ( NGS_ReadGroupGetRead ( m_readGroup, ctx, ReadId ( 1 ) . c_str () ) );
    REQUIRE_FAILED ();

    REQUIRE_NULL ( NGS_ReadGroupGetReads ( m_readGroup, ctx, true, true, true ) );
    REQUIRE_FAILED ();
#endif

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadGroupIterator_NoGroups_AfterNext, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "", NGS_ReadGroupGetName ( m_readGroup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadGroupIterator_WithGroups_AfterNext, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );

    // iteration is in the alphanumeric order, as stored in the metadata
    REQUIRE_STRING ( "S100_V2", NGS_ReadGroupGetName ( m_readGroup, ctx ) );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "S103_V2", NGS_ReadGroupGetName ( m_readGroup, ctx ) );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "S104_V2", NGS_ReadGroupGetName ( m_readGroup, ctx ) );

    // etc

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadGroupIterator_BeyondEnd, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );

    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE ( ! NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );

    REQUIRE_NULL ( NGS_ReadGroupGetName ( m_readGroup, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

// Statistics

FIXTURE_TEST_CASE(SRA_ReadCollectionGetStatistics, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    NGS_Statistics * stats = NGS_ReadCollectionGetStatistics ( m_coll, ctx );
    REQUIRE ( ! FAILED () );

    REQUIRE_EQ ( (uint64_t)132958771,   NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)114588308,   NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)496499,      NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)496499,      NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)1,           NGS_StatisticsGetAsU64 ( stats, ctx, "SEQUENCE/SPOT_MIN" ) );

    NGS_StatisticsRelease ( stats, ctx );

    EXIT;
}

FIXTURE_TEST_CASE(SRA_ReadGroupGetStatistics, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession_WithReadGroups);

    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "S77_V2" );
    REQUIRE ( ! FAILED () );

    NGS_Statistics * stats = NGS_ReadGroupGetStatistics ( m_readGroup, ctx );
    REQUIRE ( ! FAILED () );

    REQUIRE_EQ ( (uint64_t)25920,   NGS_StatisticsGetAsU64 ( stats, ctx, "BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)22331,   NGS_StatisticsGetAsU64 ( stats, ctx, "BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)97,      NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)125,     NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)29,      NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_MIN" ) );

    NGS_StatisticsRelease ( stats, ctx );

    EXIT;
}

// Fragment Blobs

FIXTURE_TEST_CASE(SRA_GetFragmentBlobs, SRA_Fixture)
{
    ENTRY_ACC(SRA_Accession);

    NGS_FragmentBlobIterator* blobIt = NGS_ReadCollectionGetFragmentBlobs ( m_coll, ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( blobIt );

    NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( blobIt, ctx );

    REQUIRE_EQ ( (uint64_t)1080, NGS_FragmentBlobSize ( blob, ctx ) );

    NGS_FragmentBlobRelease ( blob, ctx );
    NGS_FragmentBlobIteratorRelease ( blobIt, ctx );

    EXIT;
}

//////////////////////////////////////////// Main
int main(int argc, char* argv[])
{
    //assert(!KDbgSetString("KFG"));
    //assert(!KDbgSetString("VFS"));
    KConfigDisableUserSettings();

    putenv((char*)"NCBI_VDB_QUALITY=R");

    int ret=NgsSraTestSuite(argc, argv);
    NGS_C_Fixture::ReleaseCache();
    return ret ;
}
