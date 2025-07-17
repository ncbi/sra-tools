/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author'm_s official duties as a United States Government employee and
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

#include "searchblock.hpp"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(SearchBlockTestSuite);

TEST_CASE ( FgrepDumb )
{
    FgrepSearch sb ( "CTA", FgrepSearch :: FgrepDumb );
    uint64_t hitStart = 0;
    uint64_t hitEnd = 0;
    const string Bases = "ACTGACTAGTCA";
    REQUIRE ( sb.FirstMatch ( Bases.c_str(), Bases.size(), & hitStart, & hitEnd ) );
    REQUIRE_EQ ( (uint64_t)5, hitStart );
    REQUIRE_EQ ( (uint64_t)8, hitEnd );
}

TEST_CASE ( SearchFgrepBoyerMoore )
{
    FgrepSearch sb ( "CTA", FgrepSearch :: FgrepBoyerMoore );
    uint64_t hitStart = 0;
    uint64_t hitEnd = 0;
    const string Bases = "ACTGACTAGTCA";
    REQUIRE ( sb.FirstMatch ( Bases.c_str(), Bases.size(), & hitStart, & hitEnd ) );
    REQUIRE_EQ ( (uint64_t)5, hitStart );
    REQUIRE_EQ ( (uint64_t)8, hitEnd );
}

TEST_CASE ( SearchFgrepAho )
{
    FgrepSearch sb ( "CTA", FgrepSearch :: FgrepAho );
    uint64_t hitStart = 0;
    uint64_t hitEnd = 0;
    const string Bases = "ACTGACTAGTCA";
    REQUIRE ( sb.FirstMatch ( Bases.c_str(), Bases.size(), & hitStart, & hitEnd ) );
    REQUIRE_EQ ( (uint64_t)5, hitStart );
    REQUIRE_EQ ( (uint64_t)8, hitEnd );
}

TEST_CASE ( SearchAgrepDP )
{
    AgrepSearch sb ( "CTA", AgrepSearch :: AgrepDP, 100 );
    uint64_t hitStart = 0;
    uint64_t hitEnd = 0;
    const string Bases = "ACTGACTAGTCA";
    REQUIRE ( sb.FirstMatch ( Bases.c_str(), Bases.size(), & hitStart, & hitEnd ) );
    REQUIRE_EQ ( (uint64_t)5, hitStart );
    REQUIRE_EQ ( (uint64_t)8, hitEnd );
}

TEST_CASE ( SearchNucStrstr_NoExpr_NoCoords )
{
    NucStrstrSearch sb ( "CTA", false );
    const string Bases = "ACTGACTAGTCA";
    REQUIRE ( sb.FirstMatch ( Bases.c_str(), Bases.size() ) );
}

TEST_CASE ( SearchNucStrstr_NoExpr_Coords_NotSupported )
{
    NucStrstrSearch sb ( "CTA", false );
    uint64_t hitStart = 0;
    uint64_t hitEnd = 0;
    const string Bases = "ACTGACTAGTCA";
    REQUIRE_THROW ( sb.FirstMatch ( Bases.c_str(), Bases.size(), & hitStart, & hitEnd ) ); // not supported
}

TEST_CASE ( SearchNucStrstr_Expr_Coords )
{
    NucStrstrSearch sb ( "CTA", true );
    uint64_t hitStart = 0;
    uint64_t hitEnd = 0;
    const string Bases = "ACTGACTAGTCA";
    REQUIRE ( sb.FirstMatch ( Bases.c_str(), Bases.size(), & hitStart, & hitEnd ) );
    REQUIRE_EQ ( (uint64_t)5, hitStart );
    REQUIRE_EQ ( (uint64_t)8, hitEnd );
}

TEST_CASE ( SearchSmithWaterman_Coords_NotSupported )
{
    SmithWatermanSearch sb ( "CTA", 100 );
    uint64_t hitStart = 0;
    uint64_t hitEnd = 0;
    const string Bases = "ACTGACTAGTCA";
    REQUIRE ( sb.FirstMatch ( Bases.c_str(), Bases.size(), & hitStart, & hitEnd ) );
    REQUIRE_EQ ( (uint64_t)5, hitStart );
    REQUIRE_EQ ( (uint64_t)8, hitEnd );
}

int
main( int argc, char *argv [] )
{
    return SearchBlockTestSuite(argc, argv);
}
