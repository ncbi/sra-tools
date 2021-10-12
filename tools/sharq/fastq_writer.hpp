#ifndef __FASTQ_WRITER_HPP__
#define __FASTQ_WRITER_HPP__
/* 
* ===========================================================================
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
* Author:  Many, by the time it's done.
*
* File Description:
*
* ===========================================================================
*/


#include "fastq_read.hpp"
#include "fastq_error.hpp"
#include "sra-tools/writer.hpp"
#include "spdlog/sinks/base_sink.h"
#define LOCALDEBUG

class generic_writer
{
public: 
    void set_platform(uint8_t platform) {}
//  -----------------------------------------------------------------------------
    void write_spot(const vector<CFastqRead>& reads)
//  -----------------------------------------------------------------------------
    {
        if (reads.empty())
            return;
        const auto& first_read = reads.front();
        cout << "Spot: " << first_read.Spot() << "\nreads:";
        for (const auto& read : reads) {
            //auto sz = read.Sequence().size();
            cout << " " << read.ReadNum() << "(" << (read.Type() == 0 ? "T" : "B") << ")" << "\n";
            cout << read.Sequence() << "\n";
            cout << "+\n";
            cout << read.Quality() << endl;
        }
        //cout << endl;
    }
};


template<typename Mutex>
class general_writer_sink : public spdlog::sinks::base_sink<Mutex>
{
public:    
    general_writer_sink(shared_ptr<Writer2>& writer_) 
        : //spdlog::sinks::base_sink<Mutex>(logger_name)
        writer(writer_)
    {}
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        if (msg.level >= SPDLOG_LEVEL_ERROR) 
            writer->errorMessage(fmt::to_string(formatted));
        else            
            writer->logMessage(fmt::to_string(formatted));
        //writer->progressMessage(fmt::to_string(formatted));
    }
    void flush_() override {}
    shared_ptr<Writer2> writer;
};

using general_writer_sink_mt = general_writer_sink<std::mutex>;


template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<spdlog::logger> general_writer_logger_mt(const std::string &logger_name, shared_ptr<Writer2>& writer)
{
    auto gw_logger = Factory::template create<general_writer_sink_mt>(logger_name, writer);
    //gw_logger->set_level(spdlog::level::off);
    return gw_logger;
}

class fastq_writer 
{
public: 
    fastq_writer() {};
    virtual ~fastq_writer() = default;

    virtual void open(const string& destination) {};
    virtual void close() {};
    virtual void set_platform(uint8_t platform) { m_platform = platform;}
    virtual void write_spot(const vector<CFastqRead>& reads) 
    {
        if (reads.empty())
            return;
        const auto& first_read = reads.front();
        string spot_name = first_read.Spot();
        spot_name += first_read.Suffix();
        cout << "Spot: " << spot_name << "\nreads " << reads.size() <<":\n";
        for (const auto& read : reads) {
            //auto sz = read.Sequence().size();
            cout << "num:" << read.ReadNum() << "(" << (read.Type() == 0 ? "T" : "B") << ")" << "\n";
            cout << read.Sequence() << "\n";
            cout << "+\n";
            cout << read.Quality() << endl;
        }
    }

protected:
    uint8_t m_platform{0};
};



/*
    VDB Writer 
*/
class fastq_writer_vdb : public fastq_writer
{
public: 
    fastq_writer_vdb();
    ~fastq_writer_vdb();

    void open(const string& destination) override;
    void close() override;
    void write_spot(const vector<CFastqRead>& reads) override;

private:
    shared_ptr<Writer2> m_writer; 
    std::shared_ptr<spdlog::logger> m_default_logger;
    Writer2::Column c_NAME;
    Writer2::Column c_SPOT_GROUP;
    Writer2::Column c_PLATFORM;
    Writer2::Column c_READ;
    Writer2::Column c_QUALITY;
    Writer2::Column c_READ_START;
    Writer2::Column c_READ_LEN;
    Writer2::Column c_READ_TYPE;
    Writer2::Column c_READ_FILTER;
};

//  -----------------------------------------------------------------------------
fastq_writer_vdb::fastq_writer_vdb() 
//  -----------------------------------------------------------------------------
{
    m_writer.reset(new Writer2(stdout));
    m_default_logger = spdlog::default_logger();
    auto logger = general_writer_logger_mt("general_writer", m_writer);
    spdlog::set_default_logger(logger);
}

//  -----------------------------------------------------------------------------
fastq_writer_vdb::~fastq_writer_vdb() 
//  -----------------------------------------------------------------------------
{
    if (m_writer) {
        close();
        spdlog::set_default_logger(m_default_logger);    
    }
}
//FILE *const stream, 
//  -----------------------------------------------------------------------------
void fastq_writer_vdb::open(const string& destination) 
//  -----------------------------------------------------------------------------
{
    static const string cSCHEMA = "sra/generic-fastq.vschema";
    static const string cDB = "NCBI:SRA:GenericFastq:db";

    //m_writer.reset(new Writer2(stream));
    m_writer->destination(destination); 
    m_writer->schema(cSCHEMA, cDB);
    m_writer->info("fastq-parse", "1.0");
    m_writer->addTable("SEQUENCE", {
        { "READ",               sizeof(char) }, // sequence literals 
//        { "CSREAD",             sizeof(char) }, // string
//        { "CS_KEY",             sizeof(char) }, // one character
        { "READ_START",         sizeof(int32_t) }, //one per read
        { "READ_LEN",           sizeof(int32_t) }, //one per read
        { "READ_TYPE",          sizeof(char) }, // one per read
        { "READ_FILTER",        sizeof(char) }, // one per read '0'or '1'
        { "QUALITY",            sizeof(char), "(INSDC:quality:text:phred_33)QUALITY" }, // quality string
        { "NAME",               sizeof(char), "(ascii)NAME" }, // spotName
        { "SPOT_GROUP",         sizeof(char) }, // barcode
//        { "CLIP_QUALITY_LEFT",  sizeof(int32_t) }, // one per read, default 0
//        { "CLIP_QUALITY_RIGHT", sizeof(int32_t) }, // one per read, default 0
//        { "LABEL",              sizeof(char) }, // concatenated labe string eg. 'FR'
//        { "LABEL_START",        sizeof(int32_t) }, // one per read
//        { "LABEL_LEN",          sizeof(int32_t) }, // one per read
        { "PLATFORM",           sizeof(char) } // platform code
        //{ "CHANNEL",            sizeof(int32_t) }, // nanopore
        //{ "READ_NO",            sizeof(int32_t) } // nanopore
    });
    auto table = m_writer->table("SEQUENCE");
    c_NAME = move(table.column("NAME"));
    c_SPOT_GROUP = move(table.column("SPOT_GROUP"));
    c_PLATFORM = move(table.column("PLATFORM"));
    c_READ = table.column("READ");
    c_QUALITY = table.column("QUALITY");
    c_READ_START = table.column("READ_START");
    c_READ_LEN = table.column("READ_LEN");
    c_READ_TYPE = table.column("READ_TYPE");
    c_READ_FILTER = table.column("READ_FILTER");

    m_writer->beginWriting();
}

//  -----------------------------------------------------------------------------
void fastq_writer_vdb::close() 
//  -----------------------------------------------------------------------------
{
    if (m_writer) {
        m_writer->endWriting();
        m_writer->flush();
        //spdlog::set_default_logger(m_default_logger);
        //m_writer.reset();
    }
}

/*
typedef INSDC_SRA_xread_type INSDC_read_type;
enum
{
    // read_type 
    SRA_READ_TYPE_TECHNICAL  = 0,
    SRA_READ_TYPE_BIOLOGICAL = 1,

    // orientation - applied as bits, e.g.: type = READ_TYPE_BIOLOGICAL | READ_TYPE_REVERSE 
    SRA_READ_TYPE_FORWARD = 2,
    SRA_READ_TYPE_REVERSE = 4,
};
*/
//  -----------------------------------------------------------------------------
void fastq_writer_vdb::write_spot(const vector<CFastqRead>& reads)
//  -----------------------------------------------------------------------------
{
    if (reads.empty())
        return;
    //auto table = m_writer->table("SEQUENCE");
    const auto& first_read = reads.front();
    string spot_name = first_read.Spot();
    spot_name += first_read.Suffix();
    c_NAME.setValue(spot_name);
    c_SPOT_GROUP.setValue(first_read.SpotGroup());
    c_PLATFORM.setValue(m_platform);
    string sequence;
    string quality;
    auto read_num = reads.size();
    int32_t read_start[read_num];
    size_t start  = 0;
    int32_t read_len[read_num];
    char read_type[read_num];
    char read_filter[read_num];

    read_num = 0;
    for (auto& read : reads) {
        sequence += read.Sequence();
        quality += read.Quality();
        read_start[read_num] = start;
        auto sz = read.Sequence().size();
        start += sz;
        read_len[read_num] = sz;
        //SRA_READ_TYPE_TECHNICAL = 0;
        //SRA_READ_TYPE_BIOLOGICAL = 1;
        read_type[read_num] = read.Type();
        read_filter[read_num] = (char)read.ReadFilter();
        ++read_num;
    }
    std::transform(sequence.begin(), sequence.end(), sequence.begin(), ::toupper);
    c_READ.setValue(sequence);
    c_QUALITY.setValue(quality);
    c_READ_START.setValue(read_num, sizeof(int32_t),&read_start);
    c_READ_LEN.setValue(read_num, sizeof(int32_t), read_len);
    c_READ_TYPE.setValue(read_num, sizeof(char), read_type);
    c_READ_FILTER.setValue(read_num, sizeof(char), read_filter);
    m_writer->table("SEQUENCE").closeRow();
}



#endif
