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
* Unit tests for SHARQ loader
*/
#include <ktst/unit_test.hpp>
#include <klib/rc.h>
#include <loader/common-writer.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <sstream>

#include <fingerprint.hpp>

#include "../../tools/loaders/sharq/fastq_utils.hpp"
#include "../../tools/loaders/sharq/fastq_parser.hpp"
#include "../../tools/loaders/sharq/fastq_read.hpp"
#include "../../tools/loaders/sharq/fastq_error.hpp"

#include <sysalloc.h>
#include <stdlib.h>
#include <cstring>
#include <stdexcept>
#include <list>

using namespace std;

TEST_SUITE(SharQParserTestSuite);

class LoaderFixture
{
public:
    LoaderFixture()
    {
    }
    ~LoaderFixture()
    {
    }

    shared_ptr<istream> create_stream(const string& str = "") {
        shared_ptr<stringstream> ss(new stringstream);
        *ss << str;
        return ss;
    }
    string filename;
};

const string cSPOT1 = "NB501550:336:H75GGAFXY:2:11101:10137:1038";
const string cSPOT_GROUP = "CTAGGTGA";
const string cDEFLINE1 = cSPOT1 + " 1:N:0:" + cSPOT_GROUP;
const string cSEQ1 = "GATT";
const string cSEQ2 = "CCAG";
const string cQUAL = "!''*";

#define _READ(defline, sequence, quality)\
    string(((defline[0] == '@' || defline[0] == '>') ? "" : "@") + string(defline)  + "\n" + sequence  + "\n+\n" + quality + "\n")\


FIXTURE_TEST_CASE(TestSequence, LoaderFixture)
{
    fastq_reader reader1("test", create_stream(_READ(cDEFLINE1, cSEQ1, cQUAL)));
    fastq_reader reader2("test", create_stream(_READ(cDEFLINE1, cSEQ2, cQUAL)));
    //fastq_parser([readr1,reader2])
    const Fingerprint & fp = reader1.fingerprint();
    REQUIRE_EQ( 0, (int)fp.a[0] );
    REQUIRE_EQ( 1, (int)fp.a[1] );
    REQUIRE_EQ( 0, (int)fp.a[2] );
    REQUIRE_EQ( 0, (int)fp.a[3] );
    REQUIRE_EQ( 0, (int)fp.c[0] );
    REQUIRE_EQ( 0, (int)fp.c[1] );
    REQUIRE_EQ( 0, (int)fp.c[2] );
    REQUIRE_EQ( 0, (int)fp.c[3] );
    REQUIRE_EQ( 1, (int)fp.g[0] );
    REQUIRE_EQ( 0, (int)fp.g[1] );
    REQUIRE_EQ( 0, (int)fp.g[2] );
    REQUIRE_EQ( 0, (int)fp.g[3] );
    REQUIRE_EQ( 0, (int)fp.t[0] );
    REQUIRE_EQ( 0, (int)fp.t[1] );
    REQUIRE_EQ( 1, (int)fp.t[2] );
    REQUIRE_EQ( 1, (int)fp.t[3] );
    REQUIRE_EQ( 0, (int)fp.n[0] );
    REQUIRE_EQ( 0, (int)fp.n[1] );
    REQUIRE_EQ( 0, (int)fp.n[2] );
    REQUIRE_EQ( 0, (int)fp.n[3] );
    REQUIRE_EQ( 0, (int)fp.ool[0] );
    REQUIRE_EQ( 0, (int)fp.ool[1] );
    REQUIRE_EQ( 0, (int)fp.ool[2] );
    REQUIRE_EQ( 0, (int)fp.ool[3] );
    REQUIRE_EQ( 1, (int)fp.ool[4] );
}
////////////////////////////////////////////

int main (int argc, char *argv [])
{
    return SharQParserTestSuite(argc, argv);
}
