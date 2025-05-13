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
* Unit tests for NGS C interface, CSRA1 archives
*/

#include "ngs_c_fixture.hpp"

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

TEST_SUITE(NgsCsra1TestSuite);

const char* CSRA1_PrimaryOnly   = "SRR1063272";
const char* CSRA1_WithSecondary = "SRR833251";
const char* CSRA1_WithGroups = "SRR822962";
const char* CSRA1_WithCircularReference = "SRR1769246";
const char* CSRA1_Older = "SRR353866";

#define ENTRY_GET_ALIGN(acc, alignNo) \
    ENTRY_ACC(acc); \
    GetAlignment(alignNo);

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


    void GetAlignment ( uint64_t id, bool primary = true )
    {
        stringstream str;
        str << toString ( NGS_ReadCollectionGetName ( m_coll, m_ctx ), m_ctx, true ) << ( primary ? ".PA." : ".SA.") << id;
        m_align = NGS_ReadCollectionGetAlignment ( m_coll, m_ctx, str . str () . c_str () );
        if ( m_ctx -> rc != 0 || m_align == 0 )
            throw std :: logic_error ( "GetAlignment() failed" );
    }

    NGS_Alignment*      m_align;
};

// NGS_Read

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadName, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "1", NGS_ReadGetReadName ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadId, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( ReadId ( 1 ),  NGS_ReadGetReadId ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadGroup, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "C1ELY.6",  NGS_ReadGetReadGroup ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadCategory_Aligned, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );

    enum NGS_ReadCategory cat = NGS_ReadGetReadCategory ( m_read, ctx );
    REQUIRE_EQ ( NGS_ReadCategory_fullyAligned, cat );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadCategory_Partial, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 3 );

    enum NGS_ReadCategory cat = NGS_ReadGetReadCategory ( m_read, ctx );
    REQUIRE_EQ ( NGS_ReadCategory_partiallyAligned, cat );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadSequence_Full, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );

    string expected ("ACTCGACATTCTGCCTTCGACCTATCTTTCTCCTCTCCCAGTCATCGCCCAGTAGAATTACCAGGCAATGAACCAGGGCCTTCCATCCCAACGGCACAGC"
                     "AAAGGTATGATACCTGAGATGTTGCGGGATGGTGGGTTTGTGAGGAGATGGCCACGCAGGCAAGGTCTTTTGGAATGGTTCACTGTTGGAGTGAACCCAT"
                     "AT");
    REQUIRE_STRING( expected,  NGS_ReadGetReadSequence ( m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadSequence_PartialNoLength, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "AT",  NGS_ReadGetReadSequence ( m_read, ctx, 200, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadSequence_PartialLength, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "CATA", NGS_ReadGetReadSequence ( m_read, ctx, 197, 4 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadQualities_Full, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );

    string expected (
        "@@CDDBDFFBFHFIEEFGIGGHIEHIGIGGFGEGAFDHIIIIIGGGDFHII;=BF@FEHGIEEH?AHHFHFFFFDC5'5=?CC?ADCD@AC??9BDDCDB"
        "<@@@DDADDFFHGHIIDHFFHDEFEHIIGHIIDGGGFHIJIGAGHAH=;DGEGEEEDDDB<ABBD;ACDDDCBCCCDD@CCDDDCDCDBDD@ACC>A@?>"
        "C3");
    REQUIRE_STRING ( expected, NGS_ReadGetReadQualities ( m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadQualities_PartialNoLength, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "C3", NGS_ReadGetReadQualities ( m_read, ctx, 200, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGetReadQualities_PartialLength, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "@?>C", NGS_ReadGetReadQualities ( m_read, ctx, 197, 4 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadNumFragments, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );

    uint32_t num = NGS_ReadNumFragments ( m_read, ctx );
    REQUIRE_EQ ( (uint32_t)2, num );

    EXIT;
}

// NGS_Fragment (through an NGS_Read object)
FIXTURE_TEST_CASE(CSRA1_NGS_NoFragmentBeforeNext, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    m_read = NGS_ReadCollectionGetRead ( m_coll, ctx, ReadId ( 1 ) . c_str () );

    // no access to a Fragment before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentGetId, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentGetSequence_Full, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );

    string expected = "ACTCGACATTCTGCCTTCGACCTATCTTTCTCCTCTCCCAGTCATCGCCCAGTAGAATTACCAGGCAATGAACCAGGGCC"
                      "TTCCATCCCAACGGCACAGCA";
    REQUIRE_STRING( expected,  NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 0, (size_t)-1 ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentGetSequence_PartialNoLength, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "TTCCATCCCAACGGCACAGCA",  NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 80, (size_t)-1 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentGetSequence_PartialLength, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "TTCC", NGS_FragmentGetSequence ( (NGS_Fragment*)m_read, ctx, 80, 4 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentGetQualities_Full, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );
    string expected = "@@CDDBDFFBFHFIEEFGIGGHIEHIGIGGFGEGAFDHIIIIIGGGDFHII;=BF@FEHGIEEH?AHHFHFFFFDC5'5=?CC?ADCD@AC??9BDDCDB<";
    REQUIRE_STRING ( expected, NGS_FragmentGetQualities ( (NGS_Fragment*)m_read, ctx, 0, (size_t)-1 ) );
    EXIT;
}

// Iteration over Fragments
FIXTURE_TEST_CASE(CSRA1_NGS_FragmentIteratorNext, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 ); // calls NGS_FragmentIteratorNext

    REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".FR1.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentIteratorNext_SingleFragment, CSRA1_Fixture)
{
    ENTRY_GET_READ( "ERR055323", 1 );
    REQUIRE_STRING ( "ERR055323.FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentIteratorNext_NullRead, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, false ); // will return an empty iterator
    REQUIRE_NULL ( NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_FragmentIteratorNext_BeyondEnd, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_PrimaryOnly, 1 );

    REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );

    EXIT;
}

// Iteration over Reads
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads_Next, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, true, true, true );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    // on the first read
    REQUIRE_STRING ( ReadId ( 1 ), NGS_ReadGetReadId ( m_read, ctx ) );

    // iterate over fragments
    {
        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) ); // position on the first fragment
        REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".FR0.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".FR1.1", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    }

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! FAILED () );

    // on the second read
    REQUIRE_STRING ( ReadId ( 2 ), NGS_ReadGetReadId ( m_read, ctx ) );

    {
        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) ); // position on the first fragment
        REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".FR0.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
        REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".FR1.2", NGS_FragmentGetId ( (NGS_Fragment*)m_read, ctx ) );

        REQUIRE ( ! NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, ctx ) );
    }

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_GetReadsIteratorNext_BeyondEnd, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReads ( m_coll, ctx, false, false, false);
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

// Iteration over a range of Reads
FIXTURE_TEST_CASE(CSRA1_ReadRange_NoReadBeforeNext, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 3, 2, true, true, true );
    REQUIRE ( ! FAILED () && m_read );

    // no access to a Read before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_ReadGetReadId ( m_read, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadRange, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 3, 2, true, true, true );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE_STRING ( ReadId ( 3 ), NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    REQUIRE_STRING ( ReadId ( 4 ), NGS_ReadGetReadId ( m_read, ctx ) );

    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadRange_Empty, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );
    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 6000000, 2, true, true, true );
    REQUIRE ( ! NGS_ReadIteratorNext ( m_read, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadRange_Filtered, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_read = NGS_ReadCollectionGetReadRange ( m_coll, ctx, 1, 2000000, false, false, true );
    REQUIRE ( ! FAILED () && m_read );

    REQUIRE ( NGS_ReadIteratorNext ( m_read, ctx ) );
    enum NGS_ReadCategory cat = NGS_ReadGetReadCategory ( m_read, ctx );
    REQUIRE_EQ ( NGS_ReadCategory_unaligned, cat );

    EXIT;
}

// NGS_Fragment (through an NGS_Alignment object)
FIXTURE_TEST_CASE(CSRA1_NoAlignmentFragmentBeforeNext, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_PrimaryOnly );

    m_align = NGS_ReadCollectionGetAlignments ( m_coll, m_ctx, true, true );
    // no access to a Fragment before a call to NGS_FragmentIteratorNext
    REQUIRE_NULL ( NGS_FragmentGetId ( (NGS_Fragment*)m_align, ctx ) );
    REQUIRE_FAILED ();

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentFragmentGetId, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 5 );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".FA1.5", NGS_FragmentGetId ( (NGS_Fragment*)m_align, ctx ) );
                                                                // NB. alignment #5 refers to read #165753
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentFragmentGetSequence, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "TCGAC", NGS_FragmentGetSequence ( (NGS_Fragment*)m_align, ctx, 2, 5 ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentFragmentGetQualities, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "CDDBD", NGS_FragmentGetQualities ( (NGS_Fragment*)m_align, ctx, 2, 5 ) );
    EXIT;
}

// NGS_Alignment
//TODO: secondary alignments

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetAlignmentId, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".PA.1", NGS_AlignmentGetAlignmentId ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( string( "supercont2.1" ), NGS_AlignmentGetReferenceSpec ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMappingQuality, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_EQ( 60, NGS_AlignmentGetMappingQuality( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetReferenceBases, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "ACTCGACATTCTGTCTTCGACCTATCTTTCTCCTCTCCCAGTCATCGCCCAGTAGAATTACCAGGCAATGAACCACGGCCTTTCATCCCAACGGCACAGCA",
                     NGS_AlignmentGetReferenceBases( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetReadGroup, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "C1ELY.6", NGS_AlignmentGetReadGroup( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetReadId, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 5 );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".R.165753", NGS_AlignmentGetReadId ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentIsPrimary_Yes, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 5 );
    REQUIRE( NGS_AlignmentIsPrimary ( m_align, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentIsPrimary_No, CSRA1_Fixture)
{
    ENTRY_ACC( CSRA1_WithSecondary );
    GetAlignment(175, false);
    REQUIRE( ! NGS_AlignmentIsPrimary ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentIsFirst_Yes, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE( NGS_AlignmentIsFirst ( m_align, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentIsFirst_No, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 2 );
    REQUIRE( ! NGS_AlignmentIsFirst ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetAlignmentPosition, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_EQ ( (int64_t)85, NGS_AlignmentGetAlignmentPosition ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetAlignmentLength, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_EQ ( (uint64_t)101, NGS_AlignmentGetAlignmentLength ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetIsReversedOrientation_False, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE ( ! NGS_AlignmentGetIsReversedOrientation ( m_align, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetIsReversedOrientation_True, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 2 );
    REQUIRE ( NGS_AlignmentGetIsReversedOrientation ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetSoftClip_None, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_EQ ( 0, NGS_AlignmentGetSoftClip ( m_align, ctx, true ) );
    REQUIRE_EQ ( 0, NGS_AlignmentGetSoftClip ( m_align, ctx, false ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetSoftClip_Left, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 4 );
    REQUIRE_EQ ( 5, NGS_AlignmentGetSoftClip ( m_align, ctx, true ) );
    REQUIRE_EQ ( 0, NGS_AlignmentGetSoftClip ( m_align, ctx, false ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetSoftClip_Right, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 10 );
    REQUIRE_EQ ( 0, NGS_AlignmentGetSoftClip ( m_align, ctx, true ) );
    REQUIRE_EQ ( 13, NGS_AlignmentGetSoftClip ( m_align, ctx, false ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetTemplateLength, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_EQ ( (uint64_t)201, NGS_AlignmentGetTemplateLength ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateAlignment, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );

    NGS_Alignment* mate = NGS_AlignmentGetMateAlignment ( m_align, ctx );
    REQUIRE ( ! FAILED () && mate );

    // mate is alignment #2
    REQUIRE_STRING ( string( CSRA1_PrimaryOnly ) + ".FA1.2", NGS_FragmentGetId( (NGS_Fragment*)mate, ctx ) );

    NGS_AlignmentRelease ( mate, ctx );

    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetClippedFragmentQualities, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 4 );
    REQUIRE_STRING ( "#AA>55;5(;63;;3@;A9??;6..73CDCIDA>DCB>@B=;@B?;;ADAB<DD?1*>@C9:EC?2++A3+F4EEB<E>EEIEDC2?C:;AB+==1",
                     NGS_AlignmentGetClippedFragmentQualities ( m_align, ctx ));
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetAlignedFragmentBases, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 2 );
    REQUIRE_STRING ( "ATATGGGTTCACTCCAACAGTGAACCATTCCAAAAGACCTTGCCTGCGTGGCCATCTCCTCACAAACCCACCATCCCGCAACATCTCAGGTATCATACCTT",
                     NGS_AlignmentGetAlignedFragmentBases ( m_align, ctx ));
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetShortCigar_Unclipped, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 4 );
    REQUIRE_STRING ( "5S96M", NGS_AlignmentGetShortCigar ( m_align, ctx, false ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetShortCigar_Clipped, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 4 );
    REQUIRE_STRING ( "96M", NGS_AlignmentGetShortCigar ( m_align, ctx, true ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetLongCigar_Unclipped, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 4 );
    REQUIRE_STRING ( "5S1X8=1X39=1X46=", NGS_AlignmentGetLongCigar ( m_align, ctx, false ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetLongCigar_Clipped, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 4 );
    REQUIRE_STRING ( "1X8=1X39=1X46=", NGS_AlignmentGetLongCigar ( m_align, ctx, true ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentHasMate_Primary_NoMate, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 99 );
    REQUIRE ( ! NGS_AlignmentHasMate ( m_align, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentHasMate_Primary_YesMate, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE ( NGS_AlignmentHasMate ( m_align, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentHasMate_Secondary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );
    GetAlignment ( 169, false );
    REQUIRE ( ! NGS_AlignmentHasMate ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateAlignmentId_Primary, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( string ( CSRA1_PrimaryOnly ) + ".PA.2", NGS_AlignmentGetMateAlignmentId ( m_align, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateAlignmentId_Missing, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 99 );
    NGS_AlignmentGetMateAlignmentId ( m_align, ctx );
    REQUIRE_FAILED ();
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateAlignmentId_Secondary, CSRA1_Fixture)
{
    ENTRY_ACC ( CSRA1_WithSecondary );
    GetAlignment ( 172, false );
    NGS_AlignmentGetMateAlignmentId ( m_align, ctx );
    REQUIRE_FAILED ();
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateReferenceSpec, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE_STRING ( "supercont2.1", NGS_AlignmentGetMateReferenceSpec ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateReferenceSpec_OlderSchema, CSRA1_Fixture)
{   // VDB-3065: for older accessions mate reference spec is in a different column
    ENTRY_GET_ALIGN( CSRA1_Older, 1 );
    REQUIRE_STRING ( "AAAB01006027.1", NGS_AlignmentGetMateReferenceSpec ( m_align, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateIsReversedOrientation_True, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 1 );
    REQUIRE ( NGS_AlignmentGetMateIsReversedOrientation ( m_align, ctx ) );
    EXIT;
}
FIXTURE_TEST_CASE(CSRA1_NGS_AlignmentGetMateIsReversedOrientation_False, CSRA1_Fixture)
{
    ENTRY_GET_ALIGN( CSRA1_PrimaryOnly, 2 );
    REQUIRE ( ! NGS_AlignmentGetMateIsReversedOrientation ( m_align, ctx ) );
    EXIT;
}

// ReadGroups
FIXTURE_TEST_CASE(CSRA1_NGS_ReadGroupGetName, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    const char * name = "C1ELY.6";
    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, name );
    REQUIRE ( ! FAILED () && m_readGroup );
    REQUIRE_STRING ( name, NGS_ReadGroupGetName ( m_readGroup, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_HasReadGroup, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    const char * name = "C1ELY.6";
    REQUIRE ( NGS_ReadCollectionHasReadGroup ( m_coll, ctx, name ) && ! FAILED() );
    REQUIRE ( ! NGS_ReadCollectionHasReadGroup ( m_coll, ctx, "non-existent read group" ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGroupGetStats, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_WithGroups);
    m_readGroup = NGS_ReadCollectionGetReadGroup ( m_coll, ctx, "GS57510-FS3-L03" );

    NGS_Statistics * stats = NGS_ReadGroupGetStatistics ( m_readGroup, ctx );
    REQUIRE ( ! FAILED () );

    REQUIRE_EQ ( (uint64_t)34164461870, NGS_StatisticsGetAsU64 ( stats, ctx, "BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)34164461870, NGS_StatisticsGetAsU64 ( stats, ctx, "BIO_BASE_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)488063741,   NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_COUNT" ) );
    REQUIRE_EQ ( (uint64_t)5368875807,  NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_MAX" ) );
    REQUIRE_EQ ( (uint64_t)4880812067,  NGS_StatisticsGetAsU64 ( stats, ctx, "SPOT_MIN" ) );

    NGS_StatisticsRelease ( stats, ctx );

    EXIT;
}

// parsing BAM_HEADER: (tab-separated), for CSRA1_PrimaryOnly:
//   "@RG  ID:C1ELY.6      PL:illumina     PU:C1ELYACXX121221.6.ATTCTAGG   LB:Pond-203140  PI:0"
//   "DT:2012-12-21T00:00:00-0500     SM:Cryptococcus neoformans var. grubii MW-RSA913        CN:BI"
//
// SAM/BAM Read group line (@RG), tags:
// ID (required):   Read group identifier. Each @RG line must have a unique ID. The value of ID is used in the RG
//                  tags of alignment records. Must be unique among all read groups in header section. Read group
//                  IDs may be modified when merging SAM files in order to handle collisions.
// CN:  Name of sequencing center producing the read.
// DS:  Description.
// DT:  Date the run was produced (ISO8601 date or date/time).
// FO:  Flow order. The array of nucleotide bases that correspond to the nucleotides used for each row of each read. Multi-base
//      rows are encoded in IUPAC format, and non-nucleotide rows by various other characters. Format: /\*|[ACMGRSVTWYHKDBN]+/
// KS:  The array of nucleotide bases that correspond to the key sequence of each read.
// LB:  Library.
// PG:  Programs used for processing the read group.
// PI:  Predicted median insert size.
// PL:  Platform/technology used to produce the reads. Valid values: CAPILLARY, LS454, ILLUMINA, SOLID, HELICOS, IONTORRENT and PACBIO.
// PU:  Platform unit (e.g. rowcell-barcode.lane for Illumina or slide for SOLiD). Unique identifier.
// SM:  Sample. Use pool name where a pool is being sequenced


// Iteration over ReadGroups
FIXTURE_TEST_CASE(CSRA1_NGS_ReadGroupIterator_NoReadGroupBeforeNext, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);
    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( ! FAILED () && m_readGroup );
    NGS_ReadGroupGetName ( m_readGroup, ctx );
    REQUIRE_FAILED( );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGroupIterator_AfterNext, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_WithGroups);
    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );
    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "GS46253-FS3-L03", NGS_ReadGroupGetName ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );
    REQUIRE ( ! FAILED () );
    REQUIRE_STRING ( "GS54387-FS3-L01", NGS_ReadGroupGetName ( m_readGroup, ctx ) );
    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadGroupNext_BeyondEnd, CSRA1_Fixture)
{
    ENTRY_GET_READ( CSRA1_WithGroups, 1 );
    m_readGroup = NGS_ReadCollectionGetReadGroups ( m_coll, ctx );

    size_t count = 0;
    while ( NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) )
    {
        REQUIRE ( ! FAILED () );
        ++count;
    }

    REQUIRE_EQ ( (size_t)13, count );

    NGS_ReadGroupGetName ( m_readGroup, ctx );
    REQUIRE_FAILED( );

    REQUIRE ( ! NGS_ReadGroupIteratorNext ( m_readGroup, ctx ) );

    EXIT;
}

FIXTURE_TEST_CASE(CSRA1_NGS_ReadCollectionHasReference, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);

    REQUIRE ( m_coll != NULL );
    REQUIRE ( NGS_ReadCollectionHasReference ( m_coll, ctx, "supercont2.2" ) );
    REQUIRE ( ! NGS_ReadCollectionHasReference ( m_coll, ctx, "non-existent acc" ) );

    EXIT;
}

// Fragment Blobs

FIXTURE_TEST_CASE(CSRA1_GetFragmentBlobs, CSRA1_Fixture)
{
    ENTRY_ACC(CSRA1_PrimaryOnly);

    NGS_FragmentBlobIterator* blobIt = NGS_ReadCollectionGetFragmentBlobs ( m_coll, ctx );
    REQUIRE ( ! FAILED () );
    REQUIRE_NOT_NULL ( blobIt );

    NGS_FragmentBlob* blob = NGS_FragmentBlobIteratorNext ( blobIt, ctx );

    REQUIRE_EQ ( (uint64_t)808, NGS_FragmentBlobSize ( blob, ctx ) );

    NGS_FragmentBlobRelease ( blob, ctx );
    NGS_FragmentBlobIteratorRelease ( blobIt, ctx );

    EXIT;
}

//////////////////////////////////////////// Main
int main(int argc, char* argv[]) 
{
    KConfigDisableUserSettings();
    int ret = NgsCsra1TestSuite(argc, argv);
    NGS_C_Fixture::ReleaseCache();
    return ret;
}
