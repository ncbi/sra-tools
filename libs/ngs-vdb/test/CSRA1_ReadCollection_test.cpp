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
* Unit tests for NGS interface, CSRA1 based implementation, ReadCollection class
*/

/**
* This file is to be #included into a test suite
*/

#include <klib/text.h>

//TODO: getReads (categories)
//TODO: getReadCount (categories)
//TODO: getReadGroups
//TODO: error cases

// Open

FIXTURE_TEST_CASE(CSRA1_ReadCollection_Open, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_PrimaryOnly );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_Open_Failed, CSRA1_Fixture)
{
    REQUIRE_THROW ( ngs :: ReadCollection run = open ( "idonotopen" ) );
}

// Read collection functions

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetName, CSRA1_Fixture)
{
    ngs::String name1, name2;
    {
        ngs::ReadCollection run = open ("data/SRR611340");
        name1 = run.getName();
        char const* pNoSlash = string_rchr( name1.c_str(), name1.length(), '/' );
        REQUIRE ( pNoSlash == NULL );
    }
    {
        ngs::ReadCollection run = open ("SRR611340");
        name2 = run.getName();
    }
    REQUIRE_EQ ( name1, name2 );
}

// READS

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = open ( CSRA1_PrimaryOnly ) . getReads ( ngs :: Read :: all );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReads_Filtered, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = open ( CSRA1_PrimaryOnly ) . getReads ( ngs :: Read :: unaligned );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReadRange, CSRA1_Fixture)
{
    ngs :: ReadIterator readIt = open ( CSRA1_PrimaryOnly ) . getReadRange ( 100, 200 );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetRead, CSRA1_Fixture)
{
    ngs :: Read read = getRead ( string ( CSRA1_PrimaryOnly ) + ".R.1" );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetRead_IdTooHigh, CSRA1_Fixture)
{
    REQUIRE_THROW ( ngs :: Read read =
                        open ( CSRA1_PrimaryOnly ) . getRead ( string ( CSRA1_PrimaryOnly ) + ".R.2280634" ) );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetRead_IdTooLow, CSRA1_Fixture)
{
    REQUIRE_THROW ( ngs :: Read read =
                        open ( CSRA1_PrimaryOnly ) . getRead ( string ( CSRA1_PrimaryOnly ) + ".R.0" ) );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_ReadCount, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_PrimaryOnly );
    REQUIRE_EQ( (uint64_t)2280633, run . getReadCount() );
}

// REFERENCE

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReference, CSRA1_Fixture)
{
    ngs :: Reference ref = getReference ( "supercont2.1" );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_HasReference, CSRA1_Fixture)
{
    REQUIRE ( hasReference ( "supercont2.1" ) );
    REQUIRE ( ! hasReference ( "non-existent acc" ) );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReference_Failed, CSRA1_Fixture)
{
    REQUIRE_THROW ( ngs :: Reference ref = getReference ( "supercnut2.1" ) );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetReferences, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_PrimaryOnly );
    ngs :: ReferenceIterator refIt = run . getReferences ();
}

// ALIGNMENT

// GetAlignment

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment_Primary, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_PrimaryOnly );
    ngs :: Alignment al = run . getAlignment ( string ( CSRA1_PrimaryOnly ) + ".PA.1" );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment_Primary_OutOfRange_Low, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_PrimaryOnly );
    REQUIRE_THROW ( ngs :: Alignment al = run . getAlignment ( string ( CSRA1_PrimaryOnly ) + ".PA.0" ) );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment_Primary_OutOfRange_High, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_PrimaryOnly );
    REQUIRE_THROW ( run . getAlignment ( string ( CSRA1_PrimaryOnly ) + ".PA.3987703" ) );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment_Secondary, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    ngs :: Alignment al = run . getAlignment ( string ( CSRA1_WithSecondary ) + ".SA.169" );
    REQUIRE_EQ( ngs :: Alignment :: secondaryAlignment, al . getAlignmentCategory() );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignment_Secondary_OutOfRange, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    REQUIRE_THROW ( ngs :: Alignment al = run . getAlignment ( string ( CSRA1_WithSecondary ) + ".SA.179" ) );
}

// GetAlignments

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignments_Primary, CSRA1_Fixture)
{
    ngs :: AlignmentIterator alIt = getAlignments ( ngs :: Alignment :: primaryAlignment );
    REQUIRE( alIt . nextAlignment() );
    REQUIRE_EQ( ngs :: Alignment :: primaryAlignment, alIt . getAlignmentCategory() );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignments_Secondary, CSRA1_Fixture)
{
    ngs :: AlignmentIterator alIt = getAlignments ( ngs :: Alignment :: secondaryAlignment );
    REQUIRE( alIt . nextAlignment() );
    REQUIRE_EQ( ngs :: Alignment :: secondaryAlignment, alIt . getAlignmentCategory() );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignments_All, CSRA1_Fixture)
{
    ngs :: AlignmentIterator alIt = getAlignments ( ngs :: Alignment :: all );
    REQUIRE( alIt . nextAlignment() );
    REQUIRE_EQ( ngs :: Alignment :: primaryAlignment, alIt . getAlignmentCategory() );
}

// GetAlignmentCount
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Secondary, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    REQUIRE_EQ( (uint64_t)178, run . getAlignmentCount() );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Secondary_CategoryAll, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    REQUIRE_EQ( (uint64_t)178, run . getAlignmentCount( ngs :: Alignment :: all ) );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Secondary_CategoryPrimary, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    REQUIRE_EQ( (uint64_t)168, run . getAlignmentCount( ngs :: Alignment :: primaryAlignment ) );
}
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentCount_Secondary_CategorySecondary, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    REQUIRE_EQ( (uint64_t)10, run . getAlignmentCount( ngs :: Alignment :: secondaryAlignment ) );
}

// GetAlignmentRange
FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_All, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    ngs :: AlignmentIterator alIt = run . getAlignmentRange ( 168, 2 ); // both in primary and secondary tables
    REQUIRE ( alIt . nextAlignment() );
    REQUIRE_EQ( string ( CSRA1_WithSecondary ) + ".PA.168", alIt . getAlignmentId () . toString () );
    REQUIRE_EQ( ngs :: Alignment :: primaryAlignment, alIt . getAlignmentCategory() );
    REQUIRE ( alIt . nextAlignment() );
    REQUIRE_EQ( string ( CSRA1_WithSecondary ) + ".SA.169", alIt . getAlignmentId () . toString () );
    REQUIRE_EQ( ngs :: Alignment :: secondaryAlignment, alIt . getAlignmentCategory() );
    REQUIRE ( ! alIt . nextAlignment() );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRange_FilteredAll, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    ngs :: AlignmentIterator alIt = run . getAlignmentRange ( 168, 2, ngs :: Alignment :: all ); // both in primary and secondary tables
    REQUIRE ( alIt . nextAlignment() );
    REQUIRE_EQ( string ( CSRA1_WithSecondary ) + ".PA.168", alIt . getAlignmentId () . toString () );
    REQUIRE_EQ( ngs :: Alignment :: primaryAlignment, alIt . getAlignmentCategory() );
    REQUIRE ( alIt . nextAlignment() );
    REQUIRE_EQ( string ( CSRA1_WithSecondary ) + ".SA.169", alIt . getAlignmentId () . toString () );
    REQUIRE_EQ( ngs :: Alignment :: secondaryAlignment, alIt . getAlignmentCategory() );
    REQUIRE ( ! alIt . nextAlignment() );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRangeFiltered_Primary, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    ngs :: AlignmentIterator alIt = run . getAlignmentRange ( 168, 2, ngs :: Alignment :: primaryAlignment );
    REQUIRE ( alIt . nextAlignment() );
    REQUIRE_EQ( string ( CSRA1_WithSecondary ) + ".PA.168", alIt . getAlignmentId () . toString () );
    REQUIRE_EQ( ngs :: Alignment :: primaryAlignment, alIt . getAlignmentCategory() );
    REQUIRE ( ! alIt . nextAlignment() );
}

FIXTURE_TEST_CASE(CSRA1_ReadCollection_GetAlignmentRangeFiltered_Secondary, CSRA1_Fixture)
{
    ngs :: ReadCollection run = open ( CSRA1_WithSecondary );
    ngs :: AlignmentIterator alIt = run . getAlignmentRange ( 168, 2, ngs :: Alignment :: secondaryAlignment );
    REQUIRE ( alIt . nextAlignment() );
    REQUIRE_EQ( string ( CSRA1_WithSecondary ) + ".SA.169", alIt . getAlignmentId () . toString () );
    REQUIRE_EQ( ngs :: Alignment :: secondaryAlignment, alIt . getAlignmentCategory() );
    REQUIRE ( ! alIt . nextAlignment() );
}

