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
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include "vdb.hpp"
#include "writer.hpp"

static bool write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::RawData const &in)
{
    return out.value(cid, in.elements, in.elem_bits / 8, in.data);
}

#include <fstream>

static void processAligned(VDB::Writer const &out, VDB::Database const &inDb, bool const primary)
{
    static char const *const FLDS[] = { "SEQ_SPOT_GROUP", "SEQ_SPOT_ID", "SEQ_READ_ID", "READ", "REF_NAME", "REF_ORIENTATION", "REF_POS", "CIGAR_SHORT" };
    auto const N = sizeof(FLDS)/sizeof(FLDS[0]);
    auto const tblName = primary ? "PRIMARY_ALIGNMENT" : "SECONDARY_ALIGNMENT";
    auto const in = inDb[tblName].read(N, FLDS);
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 100.0;
    auto nextReport = 1;
    char buffer[32];
    VDB::Cursor::RawData data[N];
    
    std::cerr << "processing " << (range.second - range.first) << " records from " << tblName << std::endl;
    for (int64_t row = range.first; row < range.second; ++row) {
        in.read(row, N, data);
        
        auto const spotId = (int64_t const *)data[1].data;
        auto const n = snprintf(buffer, 32, "%lli", *spotId);
        auto const readId = (int32_t const *)data[2].data;
        auto const strand = (int8_t const *)data[5].data;
        auto const refpos = (int32_t const *)data[6].data;
        
        write(out, 1, data[0]);
        out.value(2, n, buffer);
        out.value(3, 1, readId);
        write(out, 4, data[3]);
        write(out, 5, data[4]);
        out.value<char>(6, strand[0] == 0 ? '+' : '-');
        out.value(7, 1, refpos);
        write(out, 8, data[7]);
        
        out.closeRow(1);
        if (nextReport * freq <= row - range.first) {
            std::cerr << "processed " << nextReport << "%" << std::endl;
            ++nextReport;
        }
    }
    std::cerr << "processed 100%" << std::endl;
    std::cerr << "imported " << (range.second - range.first) << " alignments from " << tblName << std::endl;
}

static void processUnaligned(VDB::Writer const &out, VDB::Database const &inDb)
{
    static char const *const FLDS[] = { "SPOT_GROUP", "READ", "READ_START", "READ_LEN", "PRIMARY_ALIGNMENT_ID" };
    auto const N = sizeof(FLDS)/sizeof(FLDS[0]);
    auto const in = inDb["SEQUENCE"].read(N, FLDS);
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 100.0;
    auto nextReport = 1;
    char buffer[32];
    VDB::Cursor::RawData data[N];
    int64_t written = 0;
    
    std::cerr << "processing " << (range.second - range.first) << " records from SEQUENCE" << std::endl;
    for (int64_t row = range.first; row < range.second; ++row) {
        data[4] = in.read(row, 5);
        auto const nreads = data[4].elements;
        auto const pid = (int64_t const *)data[4].data;
        
        for (unsigned i = 0; i < nreads; ++i) {
            if (pid[i] == 0) {
                in.read(row, N - 1, data);
                
                auto const n = snprintf(buffer, 32, "%lli", row);
                auto const sequence = (char const *)data[1].data;
                auto const readStart = (int32_t const *)data[2].data;
                auto const readLen = (uint32_t const *)data[3].data;
                
                write(out, 1, data[0]);
                out.value(2, n, buffer);
                out.value(3, int32_t(i + 1));
                out.value(4, readLen[i], sequence + readStart[i]);
                out.closeRow(1);
                ++written;
            }
        }
        if (nextReport * freq <= row - range.first) {
            std::cerr << "processed " << nextReport << '%' << std::endl;;
            ++nextReport;
        }
    }
    std::cerr << "processed 100%; imported " << written << " unaligned reads" << std::endl;
}

static int process(VDB::Writer const &out, VDB::Database const &inDb)
{
    try {
        processAligned(out, inDb, false);
    }
    catch (...) {
        std::cerr << "an error occured trying to process secondary alignments" << std::endl;
    }
    processUnaligned(out, inDb);
    processAligned(out, inDb, true);
    return 0;
}

static int process(std::string const &run, FILE *const out) {
    auto const writer = VDB::Writer(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("sra2ir", "1.0.0");
    
    writer.openTable(1, "RAW");
    writer.openColumn(1, 1,  8, "READ_GROUP");
    writer.openColumn(2, 1,  8, "NAME");
    writer.openColumn(3, 1, 32, "READNO");
    writer.openColumn(4, 1,  8, "SEQUENCE");
    writer.openColumn(5, 1,  8, "REFERENCE");
    writer.openColumn(6, 1,  8, "STRAND");
    writer.openColumn(7, 1, 32, "POSITION");
    writer.openColumn(8, 1,  8, "CIGAR");
    
    writer.beginWriting();
    
    writer.defaultValue<char>(5, 0, 0);
    writer.defaultValue<char>(6, 0, 0);
    writer.defaultValue<int32_t>(7, 0, 0);
    writer.defaultValue<char>(8, 0, 0);
    
    writer.setMetadata(VDB::Writer::database, 0, "SOURCE", run);
    
    auto const mgr = VDB::Manager();
    auto const result = process(writer, mgr[run]);
    
    writer.endWriting();
    
    return result;
}

using namespace utility;
namespace sra2ir {
    static void usage(std::string const &program, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << program << " [-out=<path>] <sra run>" << std::endl;
        exit(error ? 3 : 0);
    }

    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine.program, false);
            }
        }
        auto out = std::string();
        auto run = std::string();
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                out = arg.substr(5);
                continue;
            }
            if (run.empty()) {
                run = arg;
                continue;
            }
            usage(commandLine.program, true);
        }
        if (run.empty()) {
            usage(commandLine.program, true);
        }
        if (out.empty())
            return process(run, stdout);
        
        auto stream = fopen(out.c_str(), "w");
        if (!stream) {
            std::cerr << "failed to open output file: " << out << std::endl;
            exit(3);
        }
        auto const rslt = process(run, stream);
        fclose(stream);
        return rslt;
    }
}

int main(int argc, char *argv[])
{
    return sra2ir::main(CommandLine(argc, argv));
}
