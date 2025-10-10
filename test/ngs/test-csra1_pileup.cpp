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
* Unit tests for the SRA schema library
*/

#include <ktst/unit_test.hpp>
#include <kfg/config.h>

#include <../ncbi/NGS.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/ReferenceIterator.hpp>
#include <ngs/Reference.hpp>
#include <ngs/PileupIterator.hpp>
#include <ngs/Pileup.hpp>
#include <ngs/PileupEventIterator.hpp>

using namespace std;
using namespace ngs;

TEST_SUITE(CSRA1PileupTestSuite);

TEST_CASE( IteratePileups ) // VDB-6127
{
    ReadCollection obj = ncbi :: NGS :: openReadCollection ( "SRR619505" );
    auto it = obj.getReferences();
    REQUIRE( it.nextReference() );
    REQUIRE_EQ( string("NC_000005.8"), it.getCanonicalName() ); // chr5
    REQUIRE( ! it.getIsCircular() );
    auto pit = it.getPileups( Alignment::primaryAlignment );
    while( pit.nextPileup() )
    {
        while ( pit.nextPileupEvent() ) // used to assert after a few iterations
        {
        }
    }
}

//////////////////////////////////////////// Main
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return CSRA1PileupTestSuite(argc, argv);
}
