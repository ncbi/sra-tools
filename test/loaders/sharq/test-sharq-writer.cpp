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

#include <tuple>

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE(SharQWriterTestSuite);

ostream cnull(0);

class test_writer : public Writer2
{
public:
    test_writer() : Writer2( cnull ) {}
    bool destination(string const &remoteDb) const override
    {
        m_destination = remoteDb;
        return true;
    }
    bool schema(string const &file, string const &dbSpec) const override
    {
        m_file = file;
        m_dbSpec = dbSpec;
        return true;
    }
    bool setMetadata(VDB::Writer::MetaNodeRoot const root, unsigned const oid, string const &name, string const &value) const override
    {
        m_metadata.push_back( std::make_tuple( root, oid, name, value) );
        return true;
    }
    bool setMetadataAttr(VDB::Writer::MetaNodeRoot const root, unsigned const oid, std::string const &path, std::string const &attr, std::string const &value) const override
    {
        m_metadataAttrs.push_back( std::make_tuple( root, oid, path, attr, value) );
        return true;
    }

    mutable string m_destination;
    mutable string m_file;
    mutable string m_dbSpec;

    mutable vector< tuple< VDB::Writer::MetaNodeRoot, unsigned int, string, string > > m_metadata;
    mutable vector< tuple< VDB::Writer::MetaNodeRoot, unsigned int, string, string, string > > m_metadataAttrs;
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
{   // input and ouput fingerprints recorded in the metadata
    vector<CFastqRead> reads;

    const string File1 = "file1";
    {
        CFastqRead r; r.SetSequence("A");
        Fingerprint fp(2);
        fp.record( r.Sequence());
        reads.push_back(r);
        m_w.set_fingerprint( File1, fp );
    }

    const string File2 = "file2";
    {
        CFastqRead r; r.SetSequence("C");
        Fingerprint fp(2);
        fp.record( r.Sequence());
        reads.push_back(r);
        m_w.set_fingerprint( File2, fp );
    }

    m_w.open();
    m_w.write_spot( "spot", reads );
    m_w.close();

    REQUIRE_EQ( 5, (int)m_tw->m_metadata.size() );  // 1 per input + 3 for output
    REQUIRE_EQ( 2, (int)m_tw->m_metadataAttrs.size() ); // 1 per input (file name)

    // input fingerprints, on the database per input file
    {   // file1
        REQUIRE_EQ( VDB::Writer::MetaNodeRoot::database, get<0>(m_tw->m_metadata[0]) );
        REQUIRE_EQ( 0u, get<1>(m_tw->m_metadata[0]) ); // root database
        REQUIRE_EQ( string("LOAD/QC/file_1"), get<2>(m_tw->m_metadata[0]) );

        const string Expected =
            R"({"maximum-position":1,"A":[1,0],"C":[0,0],"G":[0,0],"T":[0,0],"N":[0,0],"EoR":[0,1]})";
        REQUIRE_EQ( Expected, get<3>(m_tw->m_metadata[0]) );

        // path "LOAD/QC/file_1" attr "name" = File1
        REQUIRE_EQ( VDB::Writer::MetaNodeRoot::database, get<0>(m_tw->m_metadataAttrs[0]) );
        REQUIRE_EQ( 0u, get<1>(m_tw->m_metadataAttrs[0]) );
        REQUIRE_EQ( string("LOAD/QC/file_1"), get<2>(m_tw->m_metadataAttrs[0]) );
        REQUIRE_EQ( string("name"), get<3>(m_tw->m_metadataAttrs[0]) );
        REQUIRE_EQ( File1, get<4>(m_tw->m_metadataAttrs[0]) );
    }

    {   // file2
        REQUIRE_EQ( VDB::Writer::MetaNodeRoot::database, get<0>(m_tw->m_metadata[1]) );
        REQUIRE_EQ( 0u, get<1>(m_tw->m_metadata[1]) );
        REQUIRE_EQ( string("LOAD/QC/file_2"), get<2>(m_tw->m_metadata[1]) );

        const string Expected =
            R"({"maximum-position":1,"A":[0,0],"C":[1,0],"G":[0,0],"T":[0,0],"N":[0,0],"EoR":[0,1]})";
        REQUIRE_EQ( Expected, get<3>(m_tw->m_metadata[1]) );

        // path "LOAD/QC/file_2" attr "name" = File2
        REQUIRE_EQ( VDB::Writer::MetaNodeRoot::database, get<0>(m_tw->m_metadataAttrs[1]) );
        REQUIRE_EQ( 0u, get<1>(m_tw->m_metadataAttrs[1]) );
        REQUIRE_EQ( string("LOAD/QC/file_2"), get<2>(m_tw->m_metadataAttrs[1]) );
        REQUIRE_EQ( string("name"), get<3>(m_tw->m_metadataAttrs[1]) );
        REQUIRE_EQ( File2, get<4>(m_tw->m_metadataAttrs[1]) );
    }

    // output fingerprint, on the SEQUENCE table
    {
        //value
        REQUIRE_EQ( VDB::Writer::MetaNodeRoot::table, get<0>(m_tw->m_metadata[2]) );
        REQUIRE_EQ( 1u, get<1>(m_tw->m_metadata[2]) ); // tableId
        REQUIRE_EQ( string("QC/current/fingerprint"), get<2>(m_tw->m_metadata[2]) );
        const string Expected =
            R"({"maximum-position":1,"A":[1,0],"C":[1,0],"G":[0,0],"T":[0,0],"N":[0,0],"EoR":[0,2]})";
        REQUIRE_EQ( Expected, get<3>(m_tw->m_metadata[2]) );

        // hash
        REQUIRE_EQ( VDB::Writer::MetaNodeRoot::table, get<0>(m_tw->m_metadata[3]) );
        REQUIRE_EQ( 1u, get<1>(m_tw->m_metadata[3]) ); // tableId
        REQUIRE_EQ( string("QC/current/hash"), get<2>(m_tw->m_metadata[3]) );
        const string ExpectedOutputHash = "2944f448d685435cffa136126a7fd7975d9177b36369b480ddd64c0bf818a5e0";
        REQUIRE_EQ( ExpectedOutputHash, get<3>(m_tw->m_metadata[3]) );

        // timestamp
        REQUIRE_EQ( VDB::Writer::MetaNodeRoot::table, get<0>(m_tw->m_metadata[4]) );
        REQUIRE_EQ( 1u, get<1>(m_tw->m_metadata[4]) ); // tableId
        REQUIRE_EQ( string("QC/current/timestamp"), get<2>(m_tw->m_metadata[4]) );
        REQUIRE_NE( string(), get<3>(m_tw->m_metadata[4]) );
    }
}

int main (int argc, char *argv [])
{
    return SharQWriterTestSuite(argc, argv);
}
