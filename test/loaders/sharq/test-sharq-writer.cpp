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

    void verifyMeta(
        size_t idx,
        VDB::Writer::MetaNodeRoot root,
        size_t id,
        const string & key,
        const string & value
    ) const
    {
        THROW_ON_FALSE( root    == get<0>(m_tw->m_metadata[idx]) );
        THROW_ON_FALSE( id      == get<1>(m_tw->m_metadata[idx]) );
        THROW_ON_FALSE( key     == get<2>(m_tw->m_metadata[idx]) );
        THROW_ON_FALSE( value   == get<3>(m_tw->m_metadata[idx]) );
    }

    void verifyMetaAttr(
        size_t idx,
        VDB::Writer::MetaNodeRoot root,
        size_t id,
        const string & key,
        const string & attr,
        const string & value
    ) const
    {
        THROW_ON_FALSE( root    == get<0>(m_tw->m_metadataAttrs[idx]) );
        THROW_ON_FALSE( id      == get<1>(m_tw->m_metadataAttrs[idx]) );
        THROW_ON_FALSE( key     == get<2>(m_tw->m_metadataAttrs[idx]) );
        THROW_ON_FALSE( attr    == get<3>(m_tw->m_metadataAttrs[idx]) );
        THROW_ON_FALSE( value   == get<4>(m_tw->m_metadataAttrs[idx]) );
    }
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

    REQUIRE_EQ( 8, (int)m_tw->m_metadata.size() );  // 1 per input + 6 for output
    REQUIRE_EQ( 10, (int)m_tw->m_metadataAttrs.size() ); // 5 per input (file name, fp digest, fp algorithm, fp version, fp format)

    // input fingerprints, on the database per input file
    {   // file1
        const string Expected =
            R"({"maximum-position":1,"A":[1,0],"C":[0,0],"G":[0,0],"T":[0,0],"N":[0,0],"EoR":[0,1]})";
        verifyMeta(0, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_1", Expected);

        verifyMetaAttr(0, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_1", "name", File1);
        verifyMetaAttr(1, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_1", "digest", "33a38a4e3554e8261d4b770efd0abbb1d2bee38b7c43400bf814da22b0d517d8");
        verifyMetaAttr(2, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_1", "algorithm",Fingerprint::algorithm());
        verifyMetaAttr(3, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_1", "version", Fingerprint::version());
        verifyMetaAttr(4, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_1", "format", Fingerprint::format());
    }
    {   // file2
        const string Expected =
            R"({"maximum-position":1,"A":[0,0],"C":[1,0],"G":[0,0],"T":[0,0],"N":[0,0],"EoR":[0,1]})";
        verifyMeta(1, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_2", Expected);

        verifyMetaAttr(5, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_2", "name", File2);
        verifyMetaAttr(6, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_2", "digest", "1754487c258a1cd0f82a45195dd2656abc02ae011a8bc52e29f0215f97929363");
        verifyMetaAttr(7, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_2", "algorithm", Fingerprint::algorithm());
        verifyMetaAttr(8, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_2", "version", Fingerprint::version());
        verifyMetaAttr(9, VDB::Writer::MetaNodeRoot::database, 0u, "LOAD/QC/file_2", "format", Fingerprint::format());
    }
    // output fingerprint, on the SEQUENCE table
    {
        const string Expected =
            R"({"maximum-position":1,"A":[1,0],"C":[1,0],"G":[0,0],"T":[0,0],"N":[0,0],"EoR":[0,2]})";
        verifyMeta(2, VDB::Writer::MetaNodeRoot::table, 1u, "QC/current/fingerprint", Expected);

        const string ExpectedOutputHash = "2944f448d685435cffa136126a7fd7975d9177b36369b480ddd64c0bf818a5e0";
        verifyMeta(3, VDB::Writer::MetaNodeRoot::table, 1u, "QC/current/digest", ExpectedOutputHash);

        REQUIRE_EQ( string("QC/current/algorithm"), get<2>(m_tw->m_metadata[4]) );
        REQUIRE_EQ( string("SHA-256"), get<3>(m_tw->m_metadata[4]) );

        REQUIRE_EQ( string("QC/current/version"), get<2>(m_tw->m_metadata[5]) );
        REQUIRE_EQ( Fingerprint::version(), get<3>(m_tw->m_metadata[5]) );

        REQUIRE_EQ( string("QC/current/format"), get<2>(m_tw->m_metadata[6]) );
        REQUIRE_EQ( Fingerprint::format(), get<3>(m_tw->m_metadata[6]) );

        // timestamp changes from execution to execution
        REQUIRE_EQ( string("QC/current/timestamp"), get<2>(m_tw->m_metadata[7]) );
        REQUIRE_NE( string(), get<3>(m_tw->m_metadata[7]) );
    }
}

int main (int argc, char *argv [])
{
    return SharQWriterTestSuite(argc, argv);
}
