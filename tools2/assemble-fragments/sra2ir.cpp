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
#include <cassert>
#include <climits>
#include <cinttypes>
#include "utility.hpp"
#include "vdb.hpp"
#include "writer.hpp"

static bool write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::RawData const &in)
{
    return out.value(cid, in.elements, in.elem_bits / 8, in.data);
}

#include <fstream>

struct ReferenceFilter {
    std::string name;
    int start;
    int end;
};

std::vector<ReferenceFilter> filter;
static bool filterInclude(std::string const &name, int position) {
    auto const beg = std::lower_bound(filter.cbegin(), filter.cend(), name, [](ReferenceFilter const &a, std::string const &b) { return a.name < b; });
    
    if (beg == filter.cend() || beg->name != name)
        return false;
    
    if (beg->start == 0) return true;

    auto const end = std::upper_bound(beg, filter.cend(), name, [](std::string const &b, ReferenceFilter const &a) { return b < a.name; });
    auto const lb = std::lower_bound(beg, end, position, [](ReferenceFilter const &a, int b) { return a.end < b; });
    return lb != end && (lb->start <= position && position < lb->end);
}

static ReferenceFilter parseFilterString(std::string arg)
{
    std::string name;
    int start = 0;
    int end = INT_MAX;
    
    auto const colon = arg.find_first_of(':');
    if (colon != std::string::npos) {
        name = arg.substr(0, colon);
        arg = arg.substr(colon + 1);
        
        std::string::size_type sz = 0;
        if ((start = std::stoi(arg, &sz)) <= 0)
            throw std::invalid_argument("expected start > 0");
        arg = arg.substr(sz);
        if (arg.empty() || arg[0] != '-')
            throw std::invalid_argument("expected '-'");
        arg = arg.substr(1);
        if (!arg.empty()) {
            end = std::stoi(arg, &sz);
            if (sz != arg.size())
                throw std::invalid_argument("expected a number");
            if (end <= 0 || end < start)
                throw std::invalid_argument("expected end > start > 0");
            ++end;
        }
    }
    else
        name = arg;
    return { name, start, end };
}

static bool addFilter(std::string const &arg)
{
    try {
        auto entry = parseFilterString(arg);

        auto prefix = std::find_if(filter.cbegin(), filter.cend(), [&](ReferenceFilter const &e)
        {
            return e.name == entry.name;
        });
        
        auto suffix = std::find_if(prefix, filter.cend(), [&](ReferenceFilter const &e)
        {
            return e.name != entry.name;
        });
        
        if (entry.start != 0) {
            for (auto i = prefix; i != suffix; ++i) {
                if (i->start == 0 || (i->start <= entry.start && entry.end <= i->end)) ///< fully contained in existing entry
                    return true;
            }
            
            for (auto i = prefix; i != suffix; ++i) {
                if ((i->start <= entry.start && entry.start <= i->end) || (i->start <= entry.end && entry.end <= i->end)) {
                    entry.start = std::min(entry.start, i->start);
                    entry.end = std::max(entry.end, i->end);
                }
            }
            
            while (prefix != suffix && prefix->start < entry.start)
                ++prefix;
            
            auto tmp = prefix;
            while (tmp != suffix && tmp->end <= entry.end)
                ++tmp;
            suffix = tmp;
        }
        std::vector<ReferenceFilter> newFilter;
        newFilter.reserve((suffix - prefix) + 1 + (prefix - filter.cbegin()));
        
        newFilter.insert(newFilter.end(), filter.cbegin(), prefix);
        newFilter.insert(newFilter.end(), entry);
        newFilter.insert(newFilter.end(), suffix, filter.cend());

        filter.swap(newFilter);
        return true;
    }
    catch (...) {
        return false;
    }
}

static void processAligned(VDB::Writer const &out, VDB::Database const &inDb, bool const primary)
{
    static char const *const FLDS[] = { "SEQ_SPOT_GROUP", "SEQ_SPOT_ID", "SEQ_READ_ID", "READ", "REF_NAME", "REF_ORIENTATION", "REF_POS", "CIGAR_SHORT" };
    auto const N = sizeof(FLDS)/sizeof(FLDS[0]);
    auto const tblName = primary ? "PRIMARY_ALIGNMENT" : "SECONDARY_ALIGNMENT";
    auto const in = inDb[tblName].read(N, FLDS);
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 100.0;
    int64_t written = 0;
    auto nextReport = 1;
    char buffer[32];
    auto const applyFilter = [](VDB::Cursor const &curs, int64_t row)
    {
        auto const refName = curs.read(row, 5);
        auto const refPos = curs.read(row, 7);
        return filterInclude(refName.asString(), refPos.value<int32_t>() + 1);
    };
    auto const keepAll = [](VDB::Cursor const &curs, int64_t row)
    {
        return true;
    };

    std::cerr << "processing " << (range.second - range.first) << " records from " << tblName << std::endl;
    in.foreach(filter.empty() ? keepAll : applyFilter,
               [&](int64_t row, bool keep, std::vector<VDB::Cursor::RawData> const &data)
               {
                   if (keep) {
                       auto const refName = data[4];
                       auto const refPos = data[6];
                       auto const n = snprintf(buffer, 32, "%" PRIi64, data[1].value<int64_t>());
                       auto const strand = char(data[5].value<int8_t>() == 0 ? '+' : '-');
                       
                       write(out, 1, data[0]);     ///< spot group
                       out.value(2, n, buffer);    ///< name
                       write(out, 3, data[2]);     ///< read number
                       write(out, 4, data[3]);     ///< sequence
                       write(out, 5, refName);
                       out.value(6, strand);
                       write(out, 7, refPos);
                       write(out, 8, data[7]);     ///< cigar
                       
                       out.closeRow(1);
                       ++written;
                   }
                   while (nextReport * freq <= row - range.first) {
                       std::cerr << "processed " << nextReport << "%" << std::endl;
                       ++nextReport;
                   }
               });
    while (nextReport * freq <= range.second - range.first) {
        std::cerr << "processed " << nextReport << "%" << std::endl;
        ++nextReport;
    }
    std::cerr << "imported " << written << " alignments from " << tblName << std::endl;
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
                
                auto const n = snprintf(buffer, 32, "%" PRIi64, row);
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
    if (filter.empty()) processUnaligned(out, inDb);
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
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << commandLine.program[0] << " [-out=<path>] <sra run> [reference[:start[-end]] ...]" << std::endl;
        exit(error ? 3 : 0);
    }

    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "--help" || arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine, false);
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
            if (!addFilter(arg))
                usage(commandLine, true);
        }
        if (run.empty()) {
            usage(commandLine, true);
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
