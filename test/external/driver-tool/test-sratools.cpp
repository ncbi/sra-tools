/* ===========================================================================
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
 * Project:
 *  sratools command line tool
 *
 * Purpose:
 *  Unit tests for accession pattern tests
 *
 */

#if WINDOWS
#pragma warning(disable: 4100)
#pragma warning(disable: 4101)
#endif

// C main is at the end of the file

#define TESTING 1
#include "sratools.cpp"

using namespace sratools;

static AccessionType accessionType(std::string const &accession)
{
    return Accession(accession).type();
}

static void testAccessionType() {
    // asserts because these are all hard-coded values
    assert(accessionType("SRR000000") == run);
    assert(accessionType("ERR000000") == run);
    assert(accessionType("DRR000000") == run);
    assert(accessionType("srr000000") == run);

    assert(accessionType("SRA000000") == submitter);
    assert(accessionType("SRP000000") == project);
    assert(accessionType("SRS000000") == study);
    assert(accessionType("SRX000000") == experiment);

    assert(accessionType("SRR000000.2") == run); // not certain of this one

    assert(accessionType("SRR00000") == unknown); // too short
    assert(accessionType("SRF000000") == unknown); // bad type
    assert(accessionType("ZRR000000") == unknown); // bad issuer
    assert(accessionType("SRRR00000") == unknown); // not digits
}

int main ( int argc, char *argv[], char *envp[])
{
    try {
        testAccessionType();
        uuid_test();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
    return 0;
}
