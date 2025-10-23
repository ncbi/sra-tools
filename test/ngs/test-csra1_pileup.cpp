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
* A place holder for unit tests using the NGS API
*/

#include <ktst/unit_test.hpp>
#include <kfg/config.h>

#include <../ncbi/NGS.hpp>

using namespace std;
using namespace ngs;

TEST_SUITE(CSRA1PileupTestSuite);

TEST_CASE( NoSuchRun )
{
    REQUIRE_THROW( ReadCollection obj = ncbi :: NGS :: openReadCollection ( "not a run at all" ) );
}

//////////////////////////////////////////// Main
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return CSRA1PileupTestSuite(argc, argv);
}
