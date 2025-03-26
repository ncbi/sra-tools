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
 * Purpose:
 *  Unit tests for fingerprint.hpp
 */

#include <sstream>
#include <JSON_ostream.hpp>
#include <fingerprint.hpp>

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(QaStatsFingerprintTestSuite);

TEST_CASE(Version)
{
    REQUIRE_EQ(Fingerprint::version(), string{"1.0.0"});
}

TEST_CASE(Algorithm)
{
    REQUIRE_EQ(Fingerprint::algorithm(), string{"SHA-256"});
}

TEST_CASE(Format)
{
    auto const format = Fingerprint::format();
    REQUIRE_NE(format.find("json"), format.npos);
    REQUIRE_NE(format.find("utf-8"), format.npos);
    REQUIRE_NE(format.find("compact"), format.npos);
}

TEST_CASE(Empty)
{
    Fingerprint fp(1);
    ostringstream outStr;
    JSON_ostream out{outStr, true};
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":0,"A":[0],"C":[0],"G":[0],"T":[0],"N":[0],"EoR":[0]})"
    };
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(EmptyRead)
{
    Fingerprint fp(1);
    fp.record("");

    ostringstream outStr;
    JSON_ostream out(outStr, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":0,"A":[0],"C":[0],"G":[0],"T":[0],"N":[0],"EoR":[1]})"
    };
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(OneBase)
{
    Fingerprint fp(1);
    fp.record("A");

    ostringstream outStr;
    JSON_ostream out(outStr, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":1,"A":[1],"C":[0],"G":[0],"T":[0],"N":[0],"EoR":[1]})"
    };
    REQUIRE_EQ( expected, outStr.str() );
}

TEST_CASE(AllBases)
{
    Fingerprint fp(6);
    fp.record("ACGTN");

    ostringstream strm;
    JSON_ostream out(strm, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":5,"A":[1,0,0,0,0,0],"C":[0,1,0,0,0,0],"G":[0,0,1,0,0,0],"T":[0,0,0,1,0,0],"N":[0,0,0,0,1,0],"EoR":[0,0,0,0,0,1]})"
    };
    REQUIRE_EQ( expected, strm.str() );
}

TEST_CASE(MultiRead)
{
    Fingerprint fp(6);
    //         0123456
    fp.record("ACGTN");
    fp.record("AACCGT");

    ostringstream strm;
    JSON_ostream out(strm, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":6,"A":[2,1,0,0,0,0],"C":[0,1,1,1,0,0],"G":[0,0,1,0,1,0],"T":[0,0,0,1,0,1],"N":[0,0,0,0,1,0],"EoR":[1,0,0,0,0,1]})"
    };
    REQUIRE_EQ( expected, strm.str() );
}

TEST_CASE(ReadHash_example_Fig_1_Fig_2)
{
    Fingerprint fp{9};

    // from Fig. 1
    //         01234567
    fp.record("AATGCCT");
    fp.record("AACTTNGG");
    fp.record("TATATATA");
    fp.record("GCTA");

    ostringstream strm;
    JSON_ostream out(strm, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":8,"A":[2,3,0,2,0,1,0,1,0],"C":[0,1,1,0,1,1,0,0,0],"G":[1,0,0,1,0,0,1,1,0],"T":[1,0,3,1,2,0,2,0,0],"N":[0,0,0,0,0,1,0,0,0],"EoR":[0,0,0,0,1,0,0,1,2]})"
    };
    auto const expectedDigest = std::string{
        "858fee7fd1220acf4d89cffe5a22ee0ea3f547e6636dafa6c240f3ea6ce5b08e"
    };
    REQUIRE_EQ( expected, strm.str() );
    REQUIRE_EQ( expectedDigest, fp.digest() );
}

TEST_CASE(WrapAround)
{
    Fingerprint fp(3);
    fp.record("ACGTN");
             //ACG
             //TN$
    fp.record("AACCGT");
             //AAC
             //CGT
             //$
    fp.record("AC");
             //AC$

    ostringstream strm;
    JSON_ostream out(strm, true);
    out << fp;
    auto const expected = std::string{
        R"({"maximum-position":6,"A":[3,1,0],"C":[1,2,1],"G":[0,1,1],"T":[1,0,1],"N":[0,1,0],"EoR":[1,0,2]})"
    };
    REQUIRE_EQ( expected, strm.str() );
}

int main (int argc, char *argv [])
{
    return QaStatsFingerprintTestSuite(argc, argv);
}


