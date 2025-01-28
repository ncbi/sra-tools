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
* Unit tests for SHARQ loader's writer
*/

#include "../../../tools/loaders/sharq/fastq_writer.hpp"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(SharQWriterTestSuite);

ostream cnull(0);

class test_writer : public Writer2
{
public:
    test_writer() : Writer2( cnull ) {}
    bool destination(std::string const &remoteDb) const override
    {
        m_destination = remoteDb;
        return true;
    }
    bool schema(std::string const &file, std::string const &dbSpec) const override
    {
        m_file = file;
        m_dbSpec = dbSpec;
        return true;
    }

    mutable string m_destination;
    mutable string m_file;
    mutable string m_dbSpec;
};

class test_fastq_writer_vdb : public fastq_writer_vdb
{
public:
    test_fastq_writer_vdb( shared_ptr<Writer2> w = shared_ptr<Writer2>() ) : fastq_writer_vdb( cnull, w ) {}
    using fastq_writer::m_attr;
};

TEST_CASE(Construct)
{
    test_fastq_writer_vdb w;
}

TEST_CASE(SetAttr)
{
    test_fastq_writer_vdb w;
    const string Dest = "a.out";

    w.set_attr("destination", Dest);

    auto dest_it = w.m_attr.find("destination");
    REQUIRE( w.m_attr.end() != dest_it );
    REQUIRE_EQ( Dest, dest_it->second );
}

class VdbWriterFixture
{
public:
    shared_ptr<test_writer> m_tw { new test_writer() };
    test_fastq_writer_vdb m_w { m_tw };
};

const string NanoporePlatform = "9";

FIXTURE_TEST_CASE(Destination, VdbWriterFixture)
{
    const string Dest = "a.out";
    m_w.set_attr("destination", Dest);

    m_w.open();

    REQUIRE_EQ( Dest, m_tw->m_destination );
}

FIXTURE_TEST_CASE(DefaultSchema, VdbWriterFixture)
{
    m_w.open();

    REQUIRE_EQ( string("sra/generic-fastq.vschema"), m_tw->m_file );
    REQUIRE_EQ( string("NCBI:SRA:GenericFastq:db"), m_tw->m_dbSpec );
}

FIXTURE_TEST_CASE(IlluminaSchema, VdbWriterFixture)
{
    m_w.set_attr("platform", "2");
    m_w.set_attr("name_column", "NAME");

    m_w.open();

    REQUIRE_EQ( string("NCBI:SRA:Illumina:db"), m_tw->m_dbSpec );
}

FIXTURE_TEST_CASE(NanoporeSchema, VdbWriterFixture)
{
    m_w.set_attr("platform", NanoporePlatform);

    m_w.open();

    REQUIRE_EQ( string("NCBI:SRA:GenericFastqNanopore:db"), m_tw->m_dbSpec );
}

FIXTURE_TEST_CASE(NanoporeSpecificColumns, VdbWriterFixture)
{
    m_w.set_attr("platform", NanoporePlatform);

    m_w.open();

    //REQUIRE_EQ( string("NCBI:SRA:GenericFastqNanopore:db"), m_tw->m_dbSpec );

}

FIXTURE_TEST_CASE(Fingerprinting, VdbWriterFixture)
{
    CFastqRead read;
    read.SetSequence( "GATT" );
    m_w.open();
    vector<CFastqRead> v { read };
    m_w.write_spot( "", v );


    const Fingerprint & fp = m_w.get_read_fingerprint();
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

FIXTURE_TEST_CASE(Fingerprinting_exp, VdbWriterFixture)
{
    json json_exp;
    json_exp = json::parse(
        R"(
{"EXPERIMENT":
    {"DESIGN":
        {"SPOT_DESCRIPTOR":
            {"SPOT_DECODE_SPEC":
                {"READ_SPEC":
                    {"BASE_COORD":"0","READ_CLASS":"b"}
                }
            }
        }
    }
})"
    );
    fastq_writer_exp w( json_exp, cnull) ;

    CFastqRead read;
    read.SetSequence( "GATT" );
    w.open();
    vector<CFastqRead> v { read };
    w.write_spot( "", v );

    const Fingerprint & fp = w.get_read_fingerprint();
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

int main (int argc, char *argv [])
{
    return SharQWriterTestSuite(argc, argv);
}
