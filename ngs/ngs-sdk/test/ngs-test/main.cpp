#include <stdexcept>

#include <test/test_engine/test_engine.hpp>
#include <test/test_engine/ReadCollectionItf.hpp>

////////////////////////////////////

// our little unit testing framework

static unsigned int tests_run = 0;
static unsigned int tests_failed = 0;
static unsigned int tests_passed = 0;

#define TEST_BEGIN(f) void f() { try { ++tests_run;

#define TEST_END ++tests_passed; } \
    catch (std::exception& x)   { std::cout << "exception in " << __FILE__ << ":" << __LINE__ << ": " << x.what()   << std::endl; ++tests_failed; } \
    catch (...)                 { std::cout << "exception in " << __FILE__ << ":" << __LINE__ << ": unknown"        << std::endl; ++tests_failed; } }

#define Assert(v) \
    { if ( ! ( v ) ) throw std::logic_error ( "Assertion failed: " #v ); }

////////////////////////////////////
#define TEST_BEGIN_READCOLLECTION( v ) \
    TEST_BEGIN ( v ) \
    ngs::ReadCollection rc = ngs_test_engine::NGS::openReadCollection ( "test" );

/////////// ReadCollection
TEST_BEGIN ( ReadCollection_CreateDestroy )
    {
        ngs::ReadCollection rc = ngs_test_engine::NGS::openReadCollection ( "test" );
        Assert ( ngs_test_engine::ReadCollectionItf::instanceCount == 1 );
    }
    Assert ( ngs_test_engine::ReadCollectionItf::instanceCount == 0 );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getName )
    ngs::String name = rc.getName ();
    Assert ( "test" == name );
TEST_END

TEST_BEGIN ( ReadCollection_Duplicate )
    {
        ngs::ReadCollection rc1 = ngs_test_engine::NGS::openReadCollection ( "test" );
        Assert ( ngs_test_engine::ReadCollectionItf::instanceCount == 1 );

        ngs::ReadCollection rc2 = rc1;
        Assert ( ngs_test_engine::ReadCollectionItf::instanceCount == 1 );

        ngs::String name = rc2.getName ();
        Assert ( "test" == name );
    }
    Assert ( ngs_test_engine::ReadCollectionItf::instanceCount == 0 );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getReadGroups )
    ngs::ReadGroupIterator rgs = rc.getReadGroups ();
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getReadGroup )
    ngs::ReadGroup rgs = rc.getReadGroup ( "readgroup" );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getReferences )
    ngs::ReferenceIterator refs = rc.getReferences ();
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getReference )
    ngs::Reference ref = rc.getReference ( "reference" );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getAlignment )
    ngs::Alignment al = rc.getAlignment ("alignment" );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getAlignments )
    ngs::AlignmentIterator als = rc.getAlignments ( ngs::Alignment::all );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getAlignmentCount )
    uint64_t count = rc.getAlignmentCount ( ngs::Alignment::all );
    Assert ( 13 == count );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getAlignmentRange )
    ngs::AlignmentIterator als = rc.getAlignmentRange ( 1, 20, ngs::Alignment::all );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getRead )
    ngs::Read read = rc.getRead ( "read" );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getReads )
    ngs::ReadIterator read = rc.getReads ( ngs::Read::all );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getReadCount )
    uint64_t count = rc.getReadCount ( ngs::Read::all );
    Assert ( 26 == count );
TEST_END

TEST_BEGIN_READCOLLECTION ( ReadCollection_getReadRange )
    ngs::ReadIterator read = rc.getReadRange ( 1, 25, ngs::Read::all );
TEST_END

void TestReadCollection()
{
    ReadCollection_CreateDestroy ();
    ReadCollection_getName ();
    ReadCollection_getReadGroups ();
    ReadCollection_getReadGroup ();
    ReadCollection_getReferences ();
    ReadCollection_getReference ();
    ReadCollection_getAlignment ();
    ReadCollection_getAlignments ();
    ReadCollection_getAlignmentCount ();
    ReadCollection_getAlignmentRange ();
    ReadCollection_getRead ();
    ReadCollection_getReads ();
    ReadCollection_getReadCount();
    ReadCollection_getReadRange();
}

/////////// ReadGroup
TEST_BEGIN_READCOLLECTION ( ReadGroup_Iteration )
    ngs::ReadGroupIterator readGroupIt = rc.getReadGroups ();
    // 2 read groups
    Assert ( readGroupIt.nextReadGroup() );
    Assert ( readGroupIt.nextReadGroup() );
    Assert ( ! readGroupIt.nextReadGroup() );
TEST_END

#define TEST_BEGIN_READGROUP( v ) \
    TEST_BEGIN_READCOLLECTION ( v ) \
        ngs::ReadGroupIterator readGroupIt = rc.getReadGroups (); \
        Assert ( readGroupIt.nextReadGroup() ); \
        ngs::ReadGroup readGroup = readGroupIt;

TEST_BEGIN_READGROUP ( ReadGroup_getName )
    ngs::String name = readGroup.getName();
    Assert ( "readgroup" == name );
TEST_END

TEST_BEGIN_READGROUP ( ReadGroup_getStatistics )
    ngs::Statistics stat = readGroup.getStatistics();
TEST_END

void TestReadGroup()
{
    ReadGroup_Iteration ();
    ReadGroup_getName ();
    ReadGroup_getStatistics();
}

/////////// Reference
TEST_BEGIN_READCOLLECTION ( Reference_Iteration )
    ngs::ReferenceIterator refs = rc.getReferences();
    // 3 references
    Assert ( refs.nextReference() );
    Assert ( refs.nextReference() );
    Assert ( refs.nextReference() );
    Assert ( ! refs.nextReference() );
TEST_END

#define TEST_BEGIN_REFERENCE( v ) \
    TEST_BEGIN_READCOLLECTION ( v ) \
        ngs::ReferenceIterator refs = rc.getReferences(); \
        Assert ( refs.nextReference() );

TEST_BEGIN_REFERENCE ( Reference_getCommonName )
    ngs::String name = refs.getCommonName();
    Assert ( "common name" == name );
TEST_END

TEST_BEGIN_REFERENCE ( Reference_getCanonicalName )
    ngs::String name = refs.getCanonicalName();
    Assert ( "canonical name" == name );
TEST_END

TEST_BEGIN_REFERENCE ( Reference_getIsCircular )
    Assert ( ! refs.getIsCircular() );
TEST_END

TEST_BEGIN_REFERENCE ( Reference_getIsLocal )
    Assert ( refs.getIsLocal() );
TEST_END

TEST_BEGIN_REFERENCE ( Reference_getLength )
    uint64_t length = refs.getLength ();
    Assert ( 101 == length );
TEST_END

TEST_BEGIN_REFERENCE ( Reference_getReferenceBases )
    ngs::String bases = refs.getReferenceBases ( 1, 100 );
    Assert ( "CAGT" == bases );
TEST_END

TEST_BEGIN_REFERENCE ( Reference_getReferenceChunk )
    ngs::String chunk = refs.getReferenceChunk ( 2, 20 ).toString ();
    Assert ( "AG" == chunk );
TEST_END

TEST_BEGIN_REFERENCE ( Reference_getAlignmentCount )
    uint64_t count = refs.getAlignmentCount ( ngs::Alignment::all );
    Assert ( 19 == count );
TEST_END

TEST_BEGIN_REFERENCE( Reference_getAlignment )
    ngs::Alignment al = refs.getAlignment ("alignment" );
TEST_END

TEST_BEGIN_REFERENCE( Reference_getAlignments )
    ngs::AlignmentIterator als = refs.getAlignments ( ngs::Alignment::all );
TEST_END

TEST_BEGIN_REFERENCE( Reference_getAlignmentSlice )
    ngs::AlignmentIterator als = refs.getAlignmentSlice ( 3, 2, ngs::Alignment::all );
TEST_END

TEST_BEGIN_REFERENCE( Reference_getFilteredAlignmentSlice )
    ngs::AlignmentIterator als = refs.getFilteredAlignmentSlice ( 3, 2, ngs::Alignment::all, ngs::Alignment::maxMapQuality, 0 );
TEST_END

TEST_BEGIN_REFERENCE( Reference_getPileups )
    ngs::PileupIterator pups = refs.getPileups ( ngs::Alignment::all );
TEST_END

TEST_BEGIN_REFERENCE( Reference_getPileupSlice )
    ngs::PileupIterator pups = refs.getPileupSlice ( 2, 5, ngs::Alignment::all );
TEST_END

void TestReference()
{
    Reference_Iteration ();

    Reference_getCommonName ();
    Reference_getCanonicalName ();
    Reference_getIsCircular ();
    Reference_getIsLocal ();
    Reference_getLength ();
    Reference_getReferenceBases ();
    Reference_getReferenceChunk ();
    Reference_getAlignmentCount ();
    Reference_getAlignment ();
    Reference_getAlignments ();
    Reference_getAlignmentSlice ();
    Reference_getFilteredAlignmentSlice ();
    Reference_getPileups();
    Reference_getPileupSlice();
}

/////////// Read

TEST_BEGIN_READCOLLECTION ( Read_Iteration)
    ngs::ReadIterator it = rc.getReads( ngs::Read::all );
    // 6 reads
    Assert ( it.nextRead() );
    Assert ( it.nextRead() );
    Assert ( it.nextRead() );
    Assert ( it.nextRead() );
    Assert ( it.nextRead() );
    Assert ( it.nextRead() );
    Assert ( ! it.nextRead() );
TEST_END

#define TEST_BEGIN_READ( v ) \
    TEST_BEGIN_READCOLLECTION ( v ) \
        ngs::Read read = rc.getRead("read");

TEST_BEGIN_READ ( Read_getFragmentId )
    ngs::String id = read.getFragmentId().toString();
    Assert ( "readFragId" == id );
TEST_END

TEST_BEGIN_READ( Read_getFragmentBases )
    ngs::String bases = read.getFragmentBases().toString();
    Assert ( "AGCT" == bases );
TEST_END

TEST_BEGIN_READ( Read_getFragmentBasesOffset )
    ngs::String bases = read.getFragmentBases( 1 ).toString();
    Assert ( "GCT" == bases );
TEST_END

TEST_BEGIN_READ( Read_getFragmentBasesOffsetLength )
    ngs::String bases = read.getFragmentBases( 1, 2 ).toString();
    Assert ( "GC" == bases );
TEST_END

TEST_BEGIN_READ( Read_getFragmentQualities )
    ngs::String quals = read.getFragmentQualities().toString();
    Assert ( "bbd^" == quals );
TEST_END

TEST_BEGIN_READ( Read_getFragmentQualitiesOffset )
    ngs::String quals = read.getFragmentQualities( 1 ).toString();
    Assert ( "bd^" == quals );
TEST_END

TEST_BEGIN_READ( Read_getFragmentQualitiesOffsetLength )
    ngs::String quals = read.getFragmentQualities( 1, 2 ).toString();
    Assert ( "bd" == quals );
TEST_END

TEST_BEGIN_READ( Read_IterationFragments )
    // 2 fragments
    Assert ( read.nextFragment() );
    Assert ( read.nextFragment() );
    Assert ( ! read.nextFragment() );
TEST_END

TEST_BEGIN_READ( Read_getReadId )
    ngs::String id = read.getReadId().toString();
    Assert ( "readId" == id );
TEST_END

TEST_BEGIN_READ( Read_getNumFragments )
    uint32_t count = read.getNumFragments();
    Assert ( 2 == count );
TEST_END

TEST_BEGIN_READ( Read_getReadCategory )
    ngs::Read::ReadCategory cat = read.getReadCategory();
    Assert ( ngs::Read::partiallyAligned == cat );
TEST_END

TEST_BEGIN_READ( Read_getReadGroup )
    ngs::String name = read.getReadGroup();
    Assert ( "readGroup" == name );
TEST_END

TEST_BEGIN_READ( Read_getReadName )
    ngs::String name = read.getReadName().toString();
    Assert ( "readName" == name );
TEST_END

TEST_BEGIN_READ( Read_getReadBases )
    ngs::String bases = read.getReadBases().toString();
    Assert ( "TGCA" == bases );
TEST_END

TEST_BEGIN_READ( Read_getReadBasesOffset )
    ngs::String bases = read.getReadBases( 1 ).toString();
    Assert ( "GCA" == bases );
TEST_END

TEST_BEGIN_READ( Read_getReadBasesOffsetLength )
    ngs::String bases = read.getReadBases( 1, 2 ).toString();
    Assert ( "GC" == bases );
TEST_END

TEST_BEGIN_READ( Read_getReadQualities )
    ngs::String quals = read.getReadQualities().toString();
    Assert ( "qrst" == quals );
TEST_END

TEST_BEGIN_READ( Read_getReadQualitiesOffset )
    ngs::String quals = read.getReadQualities( 1 ).toString();
    Assert ( "rst" == quals );
TEST_END

TEST_BEGIN_READ( Read_getReadQualitiesOffsetLength )
    ngs::String quals = read.getReadQualities( 1, 2 ).toString();
    Assert ( "rs" == quals );
TEST_END

void TestRead ()
{
    Read_Iteration ();

    Read_getFragmentId ();
    Read_getFragmentBases ();
    Read_getFragmentBasesOffset ();
    Read_getFragmentBasesOffsetLength ();
    Read_getFragmentQualities();
    Read_getFragmentQualitiesOffset ();
    Read_getFragmentQualitiesOffsetLength ();

    Read_IterationFragments ();

    Read_getReadId ();
    Read_getNumFragments ();
    Read_getReadCategory ();
    Read_getReadGroup ();
    Read_getReadName ();
    Read_getReadBases ();
    Read_getReadBasesOffset ();
    Read_getReadBasesOffsetLength ();
    Read_getReadQualities();
    Read_getReadQualitiesOffset ();
    Read_getReadQualitiesOffsetLength ();
}

/////////// Alignment

TEST_BEGIN_READCOLLECTION ( Alignment_Iteration )
    ngs::AlignmentIterator it = rc.getAlignments ( ngs::Alignment::all );
    // 4 alignments
    Assert ( it.nextAlignment() );
    Assert ( it.nextAlignment() );
    Assert ( it.nextAlignment() );
    Assert ( it.nextAlignment() );
    Assert ( ! it.nextAlignment() );
TEST_END


#define TEST_BEGIN_ALIGNMENT( v ) \
    TEST_BEGIN_READCOLLECTION ( v ) \
        ngs::Alignment align = rc.getAlignment("align");

TEST_BEGIN_ALIGNMENT( Alignment_getFragmentId )
    ngs::String id = align.getFragmentId().toString();
    Assert ( "alignFragId" == id );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getFragmentBases )
    ngs::String bases = align.getFragmentBases().toString();
    Assert ( "AGCT" == bases );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getFragmentBasesOffset )
    ngs::String bases = align.getFragmentBases( 1 ).toString();
    Assert ( "GCT" == bases );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getFragmentBasesOffsetLength )
    ngs::String bases = align.getFragmentBases( 1, 2 ).toString();
    Assert ( "GC" == bases );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getFragmentQualities )
    ngs::String quals = align.getFragmentQualities().toString();
    Assert ( "bbd^" == quals );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getFragmentQualitiesOffset )
    ngs::String quals = align.getFragmentQualities( 1 ).toString();
    Assert ( "bd^" == quals );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getFragmentQualitiesOffsetLength )
    ngs::String quals = align.getFragmentQualities( 1, 2 ).toString();
    Assert ( "bd" == quals );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getAlignmentId )
    ngs::String id = align.getAlignmentId().toString();
    Assert ( "align" == id );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getReferenceSpec )
    ngs::String spec = align.getReferenceSpec();
    Assert ( "referenceSpec" == spec );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getMappingQuality )
    int qual = align.getMappingQuality();
    Assert ( 90 == qual );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getReferenceBases )
    ngs::String bases = align.getReferenceBases().toString();
    Assert ( "CTAG" == bases );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getReadGroup )
    ngs::String name = align.getReadGroup();
    Assert ( "alignReadGroup" == name );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getReadId )
    ngs::String id = align.getReadId().toString();
    Assert ( "alignReadId" == id );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getClippedFragmentBases )
    ngs::String bases = align.getClippedFragmentBases().toString();
    Assert ( "TA" == bases );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getClippedFragmentQualities )
    ngs::String quals = align.getClippedFragmentQualities().toString();
    Assert ( "bd" == quals );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getAlignedFragmentBases )
    ngs::String bases = align.getAlignedFragmentBases().toString();
    Assert ( "AC" == bases );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getAlignmentCategory )
    ngs::Alignment::AlignmentCategory cat = align.getAlignmentCategory();
    Assert ( ngs::Alignment::secondaryAlignment == cat );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getAlignmentPosition )
    int64_t pos = align.getAlignmentPosition();
    Assert ( 123 == pos );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getReferencePositionProjectionRange )
    uint64_t range = align.getReferencePositionProjectionRange( 44 );
    Assert ( range == (((uint64_t)123 << 32) | 15) );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getAlignmentLength )
    uint64_t len = align.getAlignmentLength();
    Assert ( 321 == len );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getIsReversedOrientation )
    Assert ( align.getIsReversedOrientation() );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getSoftClip )
    int clip = align.getSoftClip( ngs::Alignment::clipRight );
    Assert ( 2 == clip );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getTemplateLength )
    uint64_t len = align.getTemplateLength();
    Assert ( 17 == len );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getShortCigar )
    ngs::String cigar = align.getShortCigar( true ).toString();
    Assert ( "MD" == cigar );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getLongCigar )
    ngs::String cigar = align.getLongCigar( false ).toString();
    Assert ( "MDNA" == cigar );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_hasMate )
    Assert ( align.hasMate() );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getMateAlignmentId )
    ngs::String id = align.getMateAlignmentId().toString();
    Assert ( "mateId" == id );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getMateAlignment )
    ngs::Alignment mate = align.getMateAlignment();
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getMateReferenceSpec )
    ngs::String spec = align.getMateReferenceSpec();
    Assert ( "mateReferenceSpec" == spec );
TEST_END

TEST_BEGIN_ALIGNMENT( Alignment_getMateIsReversedOrientation )
    Assert ( align.getMateIsReversedOrientation() );
TEST_END


void TestAlignment ()
{
    Alignment_Iteration ();

    Alignment_getFragmentId ();
    Alignment_getFragmentBases ();
    Alignment_getFragmentBasesOffset ();
    Alignment_getFragmentBasesOffsetLength ();
    Alignment_getFragmentQualities();
    Alignment_getFragmentQualitiesOffset ();
    Alignment_getFragmentQualitiesOffsetLength ();

    Alignment_getAlignmentId ();
    Alignment_getReferenceSpec ();
    Alignment_getMappingQuality ();
    Alignment_getReferenceBases ();
    Alignment_getReadGroup ();
    Alignment_getReadId ();
    Alignment_getClippedFragmentBases ();
    Alignment_getClippedFragmentQualities ();
    Alignment_getAlignedFragmentBases ();
    Alignment_getAlignmentCategory ();
    Alignment_getAlignmentPosition ();
    Alignment_getAlignmentLength ();
    Alignment_getIsReversedOrientation ();
    Alignment_getSoftClip ();
    Alignment_getTemplateLength ();
    Alignment_getShortCigar ();
    Alignment_getLongCigar ();
    Alignment_hasMate ();
    Alignment_getMateAlignmentId ();
    Alignment_getMateAlignment ();
    Alignment_getMateReferenceSpec ();
    Alignment_getMateIsReversedOrientation ();
}

/////////// Pileup

TEST_BEGIN_READCOLLECTION ( Pileup_Iteration )
    ngs::PileupIterator it = rc.getReference ( "refspec" ) .getPileups ( ngs::Alignment::all );
    // 3 pileups
    Assert ( it.nextPileup () );
    Assert ( it.nextPileup () );
    Assert ( it.nextPileup () );
    Assert ( ! it.nextPileup () );
TEST_END

#define TEST_BEGIN_PILEUP( v ) \
    TEST_BEGIN_READCOLLECTION ( v ) \
        ngs::PileupIterator it = rc.getReference ( "refspec" ) .getPileups ( ngs::Alignment::all ); \
        Assert ( it.nextPileup () ); \
        ngs::Pileup pileup = it;

TEST_BEGIN_PILEUP ( Pileup_getReferenceSpec )
    Assert ( "pileupRefSpec" == pileup.getReferenceSpec() );
TEST_END

TEST_BEGIN_PILEUP ( Pileup_getReferencePosition )
    Assert ( 12345 == pileup.getReferencePosition() );
TEST_END

TEST_BEGIN_PILEUP ( Pileup_getReferenceBase )
    char base = pileup.getReferenceBase ();
TEST_END

TEST_BEGIN_PILEUP ( Pileup_getPileupDepth )
    Assert ( 21 == pileup.getPileupDepth() );
TEST_END

void TestPileup ()
{
    Pileup_Iteration ();

    Pileup_getReferenceSpec ();
    Pileup_getReferencePosition ();
    Pileup_getReferenceBase ();
    Pileup_getPileupDepth ();
}

/////////// PileupEvent

TEST_BEGIN_READCOLLECTION ( PileupEvent_Iteration )
    ngs::PileupIterator pileupIt = rc.getReference ( "refspec" ) .getPileups ( ngs::Alignment::all );
    Assert ( pileupIt.nextPileup () );
    ngs::PileupEventIterator eventIt = pileupIt;
    // 7 events
    Assert ( eventIt.nextPileupEvent () );
    Assert ( eventIt.nextPileupEvent () );
    Assert ( eventIt.nextPileupEvent () );
    Assert ( eventIt.nextPileupEvent () );
    Assert ( eventIt.nextPileupEvent () );
    Assert ( eventIt.nextPileupEvent () );
    Assert ( eventIt.nextPileupEvent () );
    Assert ( ! eventIt.nextPileupEvent () );
TEST_END

#define TEST_BEGIN_PILEUPEVENT( v ) \
    TEST_BEGIN_READCOLLECTION ( v ) \
        ngs::PileupIterator pileupIt = rc.getReference ( "refspec" ) .getPileups ( ngs::Alignment::all ); \
        Assert ( pileupIt.nextPileup () ); \
        ngs::PileupEventIterator eventIt = pileupIt; \
        Assert ( eventIt.nextPileupEvent () ); \
        ngs::PileupEvent evt = eventIt;

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getMappingQuality )
    Assert ( 98 == evt.getMappingQuality() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getAlignmentId )
    Assert ( "pileupEventAlignId" == evt.getAlignmentId().toString() );
TEST_END

#if 0
TEST_BEGIN_PILEUPEVENT ( PileupEvent_getAlignment )
    ngs::Alignment al = evt.getAlignment();
TEST_END
#endif

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getAlignmentPosition )
    Assert ( 5678 == evt.getAlignmentPosition() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getFirstAlignmentPosition )
    Assert ( 90123 == evt.getFirstAlignmentPosition() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getLastAlignmentPosition )
    Assert ( 45678 == evt.getLastAlignmentPosition() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getEventType )
    Assert ( ngs::PileupEvent::mismatch == evt.getEventType() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getAlignmentBase )
    Assert ( 'A' == evt.getAlignmentBase() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getAlignmentQuality)
    Assert ( 'q' == evt.getAlignmentQuality() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getInsertionBases)
    Assert ( "AC" == evt.getInsertionBases().toString() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getInsertionQualities)
    Assert ( "#$" == evt.getInsertionQualities().toString() );
TEST_END

/*
TEST_BEGIN_PILEUPEVENT ( PileupEvent_getDeletionCount)
    Assert ( 23 == evt.getDeletionCount() );
TEST_END
*/

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getEventRepeatCount)
    Assert ( 45 == evt.getEventRepeatCount() );
TEST_END

TEST_BEGIN_PILEUPEVENT ( PileupEvent_getEventIndelType)
    Assert ( ngs::PileupEvent::intron_minus == evt.getEventIndelType() );
TEST_END

void TestPileupEvent ()
{
    PileupEvent_Iteration();

    PileupEvent_getMappingQuality ();
    PileupEvent_getAlignmentId ();
    //PileupEvent_getAlignment ();
    PileupEvent_getAlignmentPosition ();
    PileupEvent_getFirstAlignmentPosition ();
    PileupEvent_getLastAlignmentPosition ();
    PileupEvent_getEventType ();
    PileupEvent_getAlignmentBase ();
    PileupEvent_getAlignmentQuality ();
    PileupEvent_getInsertionBases ();
    PileupEvent_getInsertionQualities ();
    //PileupEvent_getDeletionCount ();
    PileupEvent_getEventRepeatCount ();
    PileupEvent_getEventIndelType ();

}

/////////// Statistics
#define TEST_BEGIN_STATISTICS( v ) \
    TEST_BEGIN_READCOLLECTION ( v ) \
        ngs::ReadGroupIterator readGroupIt = rc.getReadGroups (); \
        Assert ( readGroupIt.nextReadGroup() ); \
        ngs::ReadGroup readGroup = readGroupIt; \
        ngs::Statistics stat = readGroup.getStatistics();

TEST_BEGIN_STATISTICS ( Statistics_getValueType)
    Assert ( ngs::Statistics::uint64 == stat.getValueType("path") );
TEST_END

TEST_BEGIN_STATISTICS ( Statistics_getAsString)
    Assert ( "string" == stat.getAsString("path") );
TEST_END

TEST_BEGIN_STATISTICS ( Statistics_getAsI64)
    Assert ( -14 == stat.getAsI64("path") );
TEST_END

TEST_BEGIN_STATISTICS ( Statistics_getAsU64)
    Assert ( 144 == stat.getAsU64("path") );
TEST_END

TEST_BEGIN_STATISTICS ( Statistics_getAsDouble)
    Assert ( 3.14 == stat.getAsDouble("path") );
TEST_END

TEST_BEGIN_STATISTICS ( Statistics_nextPath)
    Assert ( "nextpath" == stat.nextPath("path") );
TEST_END

void TestStatistics ()
{
    Statistics_getValueType ();
    Statistics_getAsString ();
    Statistics_getAsI64 ();
    Statistics_getAsU64 ();
    Statistics_getAsDouble ();
    Statistics_nextPath ();
}

/////////// main

int main ()
{
    ReadCollection_Duplicate(); // call this one first to test correct population of static vtables!

    TestReadCollection ();
    TestReadGroup ();
    TestReference ();
    TestRead ();
    TestAlignment ();
    TestPileup ();
    TestPileupEvent ();
    TestStatistics ();


    // check for object leaks
    Assert ( ngs_test_engine::ReadCollectionItf::instanceCount == 0 );
    Assert ( ngs_test_engine::ReadGroupItf::instanceCount == 0 );
    Assert ( ngs_test_engine::ReferenceItf::instanceCount == 0 );
    Assert ( ngs_test_engine::ReadItf::instanceCount == 0 );
    Assert ( ngs_test_engine::AlignmentItf::instanceCount == 0 );
    Assert ( ngs_test_engine::PileupItf::instanceCount == 0 );

    // report results
    std :: cout << "Test cases run: " << tests_run << std::endl;
    std :: cout << "Test cases passed: " << tests_passed << std::endl;
    if ( tests_failed > 0 )
    {
        std :: cout << "Test cases failed: " << tests_failed << std::endl;
        return -1;
    }
    return 0;
}
