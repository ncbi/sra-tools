#ifndef __FASTQ_WRITER_HPP__
#define __FASTQ_WRITER_HPP__

/**
 * @file fastq_writer.hpp
 * @brief FASTQ Writer
 *
 */

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
* Author:  Andrei Shkeda
*
* File Description: FASTQ Writer
*
* ===========================================================================
*/


#include "fastq_read.hpp"
#include "fastq_error.hpp"
#include "sra-tools/writer.hpp"
#include <spdlog/spdlog.h>
#include "spdlog/sinks/base_sink.h"
#include <spdlog/sinks/stdout_sinks.h>
#include <insdc/sra.h>
#define LOCALDEBUG

/**
 * @brief Generic writer with std output
 *
 */
class generic_writer
{
public:
    void set_platform(uint8_t platform) {}
    void write_spot(const vector<CFastqRead>& reads)
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

/**
 * @brief Redirects spdlog logging messages to general-loader
 *
 * @tparam Mutex
 */
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

/**
 * @brief Create MT logger with general-loader sinker
 *
 * @tparam Factory
 * @param logger_name
 * @param writer
 * @return std::shared_ptr<spdlog::logger>
 */
template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<spdlog::logger> general_writer_logger_mt(const std::string &logger_name, shared_ptr<Writer2>& writer)
{
    // use this one to create a logger that sinks to general-loader only
    //auto gw_logger = Factory::template create<general_writer_sink_mt>(logger_name, writer);

    // this logger uses two sinke: stderr and general-loader
    // each sinker has its own formatting pattern
    auto stderr_sinker = make_shared<spdlog::sinks::stderr_sink_mt>();
    stderr_sinker->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    auto gl_sinker = make_shared<general_writer_sink_mt>(writer);
    gl_sinker->set_pattern("%v");

    auto gw_logger = make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list({
        stderr_sinker,
        gl_sinker
    }));

    return gw_logger;
}


/**
 * @brief FASTQ Write Base class
 *
 * fastq_parser uses open(), close() and write_spot() methods
 */
class fastq_writer
{
public:
    fastq_writer() {};
    virtual ~fastq_writer() = default;

    virtual void open() {};
    virtual void close() {};

    /**
     * @brief Write the list of reads as one spot
     *
     * @param[in] reads
     */
    virtual void write_spot(const vector<CFastqRead>& reads);

    /**
     * @brief Set user-defined attributes
     *
     * @param[in] name
     * @param[in] value
     */
    void set_attr(const string& name, const string& value) {
        m_attr[name] = value;
    }
protected:
    using TAttributeName = string;
    using TAttributeValue = string;
    map<TAttributeName, TAttributeValue> m_attr;  ///< Attributes dictionary
};


void fastq_writer::write_spot(const vector<CFastqRead>& reads)
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


/**
 * @brief VDB Writer implementaion
 *
 * Constructor redirects logging to general_loader
 * Open() method sets up the VDB table using user defin attributes
 *
 *
 */
class fastq_writer_vdb : public fastq_writer
{
public:
    fastq_writer_vdb(ostream& stream, shared_ptr<Writer2> writer );
    ~fastq_writer_vdb();

    void open() override;
    void close() override;
    void write_spot(const vector<CFastqRead>& reads) override;

    const Writer2 & get_writer() const { return * m_writer; }
private:
    shared_ptr<Writer2> m_writer;    ///< VDB Writer
    std::shared_ptr<spdlog::logger> m_default_logger; ///< Saved default loger
    Writer2::Table SEQUENCE_TABLE;
    Writer2::Column c_NAME;
    Writer2::Column c_SPOT_GROUP;
    Writer2::Column c_PLATFORM;
    Writer2::Column c_READ;
    Writer2::Column c_QUALITY;
    Writer2::Column c_READ_START;
    Writer2::Column c_READ_LEN;
    Writer2::Column c_READ_TYPE;
    Writer2::Column c_READ_FILTER;
    Writer2::Column c_CHANNEL;
    Writer2::Column c_READ_NUMBER;
    uint8_t m_platform{0};
    bool m_is_writing{false};  ///< Flag to indicate if writing was initiated
};

//  -----------------------------------------------------------------------------
fastq_writer_vdb::fastq_writer_vdb(ostream& stream, shared_ptr<Writer2> writer = shared_ptr<Writer2>() )
{
    if ( writer.get() == nullptr )
    {
        m_writer.reset( new Writer2(stream) );
    }
    else
    {
        m_writer = writer;
    }

    m_default_logger = spdlog::default_logger();
    auto logger = general_writer_logger_mt("general_writer", m_writer);
    spdlog::set_default_logger(logger);
}

//  -----------------------------------------------------------------------------
fastq_writer_vdb::~fastq_writer_vdb()
{
    if (m_is_writing) {
        close();
    }

}

//  -----------------------------------------------------------------------------
void fastq_writer_vdb::open()
{
    static const string cSCHEMA = "sra/generic-fastq.vschema";
    static const string cGENERIC_DB = "NCBI:SRA:GenericFastq:db";
    static const string cILLUMINA_DB = "NCBI:SRA:Illumina:db";
    static const string cNANOPORE_DB = "NCBI:SRA:GenericFastqNanopore:db";

    string name_column_expression = "RAW_NAME";
    string name_column = "RAW_NAME";
    {
        auto name_column_it = m_attr.find("name_column");
        if (name_column_it != m_attr.end()) {
            name_column = name_column_it->second;
            name_column_expression.clear();
            if (name_column == "NAME") {
                name_column_expression = "(ascii)";
            }
            name_column_expression += name_column;
        }
    }

    string quality_expression = "(INSDC:quality:text:phred_33)QUALITY";
    {
        auto qual_it = m_attr.find("quality_expression");
        if (qual_it != m_attr.end()) {
            quality_expression = qual_it->second;
        }
    }

    {
        auto platform_it = m_attr.find("platform");
        if (platform_it != m_attr.end()) {
            m_platform = stoi(platform_it->second);
        }
    }

    string db = cGENERIC_DB;
    switch ( m_platform )
    {
    case SRA_PLATFORM_ILLUMINA:
        // use Illumina DB for illumina with standard NAME column
        if (name_column == "NAME")
            db = cILLUMINA_DB;
        break;
    case SRA_PLATFORM_OXFORD_NANOPORE:
        db = cNANOPORE_DB;
        break;
    }

    string destination{"sra.out"};
    {
        auto dest_it = m_attr.find("destination");
        if (dest_it != m_attr.end()) {
            destination = dest_it->second;
        }
    }

    string version{"1.0"};
    {
        auto dest_it = m_attr.find("version");
        if (dest_it != m_attr.end()) {
            version = dest_it->second;
        }
    }

    m_writer->destination(destination);
    m_writer->schema(cSCHEMA, db);
    m_writer->info("sharq", version);

    vector<Writer2::ColumnDefinition> SequenceCols = {
        { "READ",               sizeof(char) }, // sequence literals
//        { "CSREAD",             sizeof(char) }, // string
//        { "CS_KEY",             sizeof(char) }, // one character
        { "READ_START",         sizeof(int32_t) }, //one per read
        { "READ_LEN",           sizeof(int32_t) }, //one per read
        { "READ_TYPE",          sizeof(char) }, // one per read
        { "READ_FILTER",        sizeof(char) }, // one per read '0'or '1'
        { "QUALITY",            sizeof(char), quality_expression.c_str() }, // quality string
        { "NAME",               sizeof(char), name_column_expression.c_str() }, // spotName
        { "SPOT_GROUP",         sizeof(char) }, // barcode
//        { "CLIP_QUALITY_LEFT",  sizeof(int32_t) }, // one per read, default 0
//        { "CLIP_QUALITY_RIGHT", sizeof(int32_t) }, // one per read, default 0
//        { "LABEL",              sizeof(char) }, // concatenated labe string eg. 'FR'
//        { "LABEL_START",        sizeof(int32_t) }, // one per read
//        { "LABEL_LEN",          sizeof(int32_t) }, // one per read
        { "PLATFORM",           sizeof(char) } // platform code
    };
    if ( m_platform == SRA_PLATFORM_OXFORD_NANOPORE )
    {
        SequenceCols.push_back( Writer2::ColumnDefinition( "CHANNEL", sizeof(uint32_t) ) );
        SequenceCols.push_back( Writer2::ColumnDefinition( "READ_NUMBER", sizeof(uint32_t) ) );
    };

    m_writer->addTable("SEQUENCE", SequenceCols);
    SEQUENCE_TABLE = move(m_writer->table("SEQUENCE"));
    c_NAME = move(SEQUENCE_TABLE.column("NAME"));
    c_SPOT_GROUP = move(SEQUENCE_TABLE.column("SPOT_GROUP"));
    c_PLATFORM = move(SEQUENCE_TABLE.column("PLATFORM"));
    c_READ = move(SEQUENCE_TABLE.column("READ"));
    c_QUALITY = move(SEQUENCE_TABLE.column("QUALITY"));
    c_READ_START = move(SEQUENCE_TABLE.column("READ_START"));
    c_READ_LEN = move(SEQUENCE_TABLE.column("READ_LEN"));
    c_READ_TYPE = move(SEQUENCE_TABLE.column("READ_TYPE"));
    c_READ_FILTER = move(SEQUENCE_TABLE.column("READ_FILTER"));
    if ( m_platform == SRA_PLATFORM_OXFORD_NANOPORE )
    {
        c_CHANNEL = move(SEQUENCE_TABLE.column("CHANNEL"));
        c_READ_NUMBER = move(SEQUENCE_TABLE.column("READ_NUMBER"));
    }

    m_writer->beginWriting();
    m_is_writing = true;

}

//  -----------------------------------------------------------------------------
void fastq_writer_vdb::close()
{
    if (m_is_writing && m_writer) {
        m_writer->endWriting();
        m_is_writing = false;
        m_writer->flush();
        spdlog::set_default_logger(m_default_logger);
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
    vector<uint8_t>  qual_scores;
    auto read_num = reads.size();
    int32_t read_start[read_num];
    size_t start  = 0;
    int32_t read_len[read_num];
    char read_type[read_num];
    char read_filter[read_num];
    uint32_t channel[read_num];
    uint32_t read_no[read_num];

    read_num = 0;
    for (const auto& read : reads) {
        sequence += read.Sequence();
        read.GetQualScores(qual_scores);
        read_start[read_num] = start;
        auto sz = read.Sequence().size();
        start += sz;
        read_len[read_num] = sz;
        //SRA_READ_TYPE_TECHNICAL = 0;
        //SRA_READ_TYPE_BIOLOGICAL = 1;
        read_type[read_num] = read.Type();
        read_filter[read_num] = (char)read.ReadFilter();
        if ( m_platform == SRA_PLATFORM_OXFORD_NANOPORE )
        {
            if ( read.Channel().empty() )
            {
                channel[read_num] = 0;
            }
            else
            {
                channel[read_num] = stoul( read.Channel() );
            }
            if ( read.NanoporeReadNo().empty() )
            {
                read_no[read_num] = 0;
            }
            else
            {
                read_no[read_num] = stoul( read.NanoporeReadNo() );
            }
        }
        ++read_num;
    }
    std::transform(sequence.begin(), sequence.end(), sequence.begin(), ::toupper);
    c_READ.setValue(sequence);
    c_QUALITY.setValue(qual_scores.size(), sizeof(uint8_t), &qual_scores[0]);
    c_READ_START.setValue(read_num, sizeof(int32_t),&read_start);
    c_READ_LEN.setValue(read_num, sizeof(int32_t), read_len);
    c_READ_TYPE.setValue(read_num, sizeof(char), read_type);
    c_READ_FILTER.setValue(read_num, sizeof(char), read_filter);
    if ( m_platform == SRA_PLATFORM_OXFORD_NANOPORE )
    {
        c_CHANNEL.setValue(read_num, sizeof(uint32_t), channel);
        c_READ_NUMBER.setValue(read_num, sizeof(uint32_t), read_no);
    }
    SEQUENCE_TABLE.closeRow();
}



#endif
