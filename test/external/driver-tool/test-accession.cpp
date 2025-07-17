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
 *  Unit tests for accession pattern detection
 *
 */

#if WINDOWS
#pragma warning(disable: 4100)
#pragma warning(disable: 4101)
#endif

#include <string>
#include "accession.cpp"

struct test_failure: public std::exception
{
    char const *test_name;
    test_failure(char const *test) : test_name(test) {}
    char const *what() const throw() { return test_name; }
};

struct assertion_failure: public std::exception
{
    std::string message;
    assertion_failure(char const *expr, char const *function, int line)
    {
        message = std::string(__FILE__) + ":" + std::to_string(line) + " in function " + function + " assertion failed: " + expr;
    }
    char const *what() const throw() { return message.c_str(); }
};

#define S_(X) #X
#define S(X) S_(X)
#define ASSERT(X) do { if (X) break; throw assertion_failure(#X, __FUNCTION__, __LINE__); } while (0)

using namespace sratools;

static AccessionType accessionType(std::string const &accession)
{
    return Accession(accession).type();
}

static void testAccessionType() {
    ASSERT(accessionType("SRR000000") == run);
    ASSERT(accessionType("ERR000000") == run);
    ASSERT(accessionType("DRR000000") == run);
    ASSERT(accessionType("srr000000") == run);

    ASSERT(accessionType("SRA000000") == submitter);
    ASSERT(accessionType("SRP000000") == project);
    ASSERT(accessionType("SRS000000") == study);
    ASSERT(accessionType("SRX000000") == experiment);

    ASSERT(accessionType("SRR000000.2") == run); // not certain of this one

    ASSERT(accessionType("SRR00000") == unknown); // too short
    ASSERT(accessionType("SRF000000") == unknown); // bad type
    ASSERT(accessionType("ZRR000000") == unknown); // bad issuer
    ASSERT(accessionType("SRRR00000") == unknown); // not digits
}

int main ( int argc, char *argv[], char *envp[])
{
    try {
        testAccessionType();
        return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 3;
}
