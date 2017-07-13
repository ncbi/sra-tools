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

#include "../../tools/sra-stat/assembly-statistics.c" /* Contigs */

#include <ktst/unit_test.hpp> // TEST_SUITE

TEST_SUITE ( TestAssemblyStatistics );

TEST_CASE ( empty ) {
    Contigs empty;
    memset ( & empty, 0, sizeof empty );

    Contigs contigs;
    ContigsInit ( & contigs );

    REQUIRE_EQ ( memcmp ( & contigs, & empty, sizeof empty ), 0 );

    ContigsCalculateStatistics ( & contigs );
    REQUIRE_EQ ( memcmp ( & contigs, & empty, sizeof empty ), 0 );

    ContigsFini ( & contigs );
    REQUIRE_EQ ( memcmp ( & contigs, & empty, sizeof empty ), 0 );
}

static rc_t Add ( Contigs * contigs ) {
    rc_t rc = ContigsAdd ( contigs, 70 );

    rc_t r2 = ContigsAdd ( contigs, 80 );
    if ( rc == 0 )
        rc = r2;

    r2      = ContigsAdd ( contigs, 20 );
    if ( rc == 0 )
        rc = r2;

    r2      = ContigsAdd ( contigs, 50 );
    if ( rc == 0 )
        rc = r2;

    r2      = ContigsAdd ( contigs, 30 );
    if ( rc == 0 )
        rc = r2;

    r2      = ContigsAdd ( contigs, 40);
    if ( rc == 0 )
        rc = r2;

    return rc;
}

/* examples from https://en.wikipedia.org/wiki/N50,_L50,_and_related_statistics
 */
TEST_CASE ( test5 ) {
    Contigs empty;
    memset ( & empty, 0, sizeof empty );

    Contigs contigs;
    ContigsInit ( & contigs );
    REQUIRE_EQ ( memcmp ( & contigs, & empty, sizeof empty ), 0 );
    REQUIRE_EQ ( contigs . assemblyLength, 0ul );
    REQUIRE_EQ ( contigs . contigLength, 0ul );

    REQUIRE_RC ( Add ( & contigs ) );

    REQUIRE_NE ( memcmp ( & contigs, & empty, sizeof empty ), 0 );

    REQUIRE_EQ ( contigs . assemblyLength, 290ul );
    REQUIRE_EQ ( contigs . contigLength, 6ul );

    ContigsCalculateStatistics ( & contigs );

    REQUIRE_EQ ( contigs . assemblyLength, 290ul );
    REQUIRE_EQ ( contigs . contigLength, 6ul );
    REQUIRE_EQ ( contigs . n50, 70ul );
    REQUIRE_EQ ( contigs . l50, 2ul );
    REQUIRE_EQ ( contigs . n90, 30ul );
    REQUIRE_EQ ( contigs . l90, 5ul );

    REQUIRE_NE ( memcmp ( & contigs, & empty, sizeof empty ), 0 );

    ContigsFini ( & contigs );
    REQUIRE_NE ( memcmp ( & contigs, & empty, sizeof empty ), 0 );
}

TEST_CASE ( test7 ) {
    Contigs empty;
    memset ( & empty, 0, sizeof empty );

    Contigs contigs;
    ContigsInit ( & contigs );
    REQUIRE_EQ ( memcmp ( & contigs, & empty, sizeof empty ), 0 );
    REQUIRE_EQ ( contigs . assemblyLength, 0ul );
    REQUIRE_EQ ( contigs . contigLength, 0ul );

    REQUIRE_RC ( Add ( & contigs ) );
    REQUIRE_RC ( ContigsAdd ( & contigs, 5 ) );
    REQUIRE_RC ( ContigsAdd ( & contigs, 10 ) );

    REQUIRE_NE ( memcmp ( & contigs, & empty, sizeof empty ), 0 );

    REQUIRE_EQ ( contigs . assemblyLength, 305ul );
    REQUIRE_EQ ( contigs . contigLength, 8ul );

    ContigsCalculateStatistics ( & contigs );

    REQUIRE_EQ ( contigs . assemblyLength, 305ul );
    REQUIRE_EQ ( contigs . contigLength, 8ul );
    REQUIRE_EQ ( contigs . n50, 50ul );
    REQUIRE_EQ ( contigs . l50, 3ul );
    REQUIRE_EQ ( contigs . n90, 20ul );
    REQUIRE_EQ ( contigs . l90, 6ul );

    REQUIRE_NE ( memcmp ( & contigs, & empty, sizeof empty ), 0 );

    ContigsFini ( & contigs );
    REQUIRE_NE ( memcmp ( & contigs, & empty, sizeof empty ), 0 );
}

extern "C" {
    ver_t CC KAppVersion ( void ) { return 0; }
    rc_t CC KMain ( int argc, char * argv [] ) {
        return TestAssemblyStatistics ( argc, argv );
    }
}
