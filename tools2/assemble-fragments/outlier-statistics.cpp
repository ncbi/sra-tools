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
#include <map>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>
#include "vdb.hpp"
#include "writer.hpp"

#include "fragment.hpp"

using namespace utility;

struct LayoutStats;
typedef std::map<std::string, LayoutStats> LayoutStatsMap;
struct LayoutStats {
    int layout;
    double freqPlus;
    double freqMinus;
    double average;
    double stdev;
    uint32_t mode;
    uint32_t median;
    uint32_t partition[11];
    
    std::pair<Layout, Layout> expectedLayout() const {
        auto const &l = Layout(layout);
        return std::make_pair(l, l.transposed());
    }
    
    unsigned bin(unsigned const length) const {
        unsigned const N = sizeof(partition)/sizeof(partition[0]);
        for (unsigned i = 0; i < N; ++i) {
            if (length < partition[i]) return i;
        }
        return N + 1;
    }
    
    struct Cursor : public VDB::Cursor {
    private:
        static VDB::Cursor cursor(VDB::Table const &tbl) {
            static char const *FLDS[] = {
                "READ_GROUP",
                "LAYOUT",
                "FREQ_FORWARD",
                "FREQ_REVERSE",
                "FRAGMENT_LENGTH_AVERAGE",
                "FRAGMENT_LENGTH_STD_DEV",
                "FRAGMENT_LENGTH_MODE",
                "FRAGMENT_LENGTH_MEDIAN",
                "FRAGMENT_LENGTH_EQUIPART"
            };
            return tbl.read(sizeof(FLDS)/sizeof(FLDS[0]), FLDS);
        }
    public:
        Cursor(VDB::Table const &tbl) : VDB::Cursor(cursor(tbl)) {}
        
        LayoutStatsMap::value_type read(int64_t row) const
        {
            auto &in = *static_cast<VDB::Cursor const *>(this);
            LayoutStats rslt = {
                (int)in.read(row, 2).value<uint8_t>(),
                in.read(row, 3).value<double>(),
                in.read(row, 4).value<double>(),
                in.read(row, 5).value<double>(),
                in.read(row, 6).value<double>(),
                in.read(row, 7).value<uint32_t>(),
                in.read(row, 8).value<uint32_t>(),
                { 0 }
            };
            auto const partition = in.read(row, 9).asVector<uint32_t>();
            auto const N = std::min(partition.size(), sizeof(rslt.partition)/sizeof(rslt.partition[0]));
            
            std::copy_n(partition.begin(), N, rslt.partition);
            
            return std::make_pair(in.read(row, 1).asString(), rslt);
        }
    };
    
    static LayoutStatsMap load(VDB::Database const &db) {
        auto const in = LayoutStats::Cursor(db["LAYOUT_STATS"]);
        auto const range = in.rowRange();
        auto rslt = LayoutStatsMap();
        
        for (auto row = range.first; row < range.second; ++row) {
            auto const &stats = in.read(row);
            if (!rslt.insert(stats).second)
                throw std::runtime_error("duplicate group name: " + stats.first);
        }
        return rslt;
    }
};

static int process(VDB::Writer const &out, VDB::Database const &inDb)
{
    auto const allStats = LayoutStats::load(inDb);
    auto const in = Fragment::Cursor(inDb["RAW"]);
    auto const range = in.rowRange();
    
    for (auto row = range.first; row < range.second; ) {
        auto const fragment = in.read(row, range.second);
        if (fragment.detail.empty())
            continue;
        
        auto const i = allStats.find(fragment.group);
        if (i == allStats.end())
            continue;
        
        auto const &stats = i->second;
        auto const expectedLayout = stats.expectedLayout();
        
        for (auto && one : fragment.detail) {
            if (one.readNo != 1 || !one.aligned) continue;
            for (auto && two : fragment.detail) {
                if (two.readNo != 2 || !two.aligned) continue;
                
                char status = 0;
                bool reverse = false;
                if (one.reference != two.reference) {
                    reverse = one.reference > two.reference;
                    status = '-';
                }
                else {
                    auto const layout = Alignment::layout(one, two);
                    auto const bin = stats.bin(layout.second);
                    
                    reverse = one.position > two.position;
                    if (layout.first != expectedLayout.first && layout.first != expectedLayout.second)
                        status = '-';
                    else if (bin > 11 || layout.second < one.sequence.length() + two.sequence.length())
                        status = '-';
                    else
                        status = '+';
                }
                if (reverse)
                    printf("%c %s %u %c %s %u %c\n", status, two.reference.c_str(), two.position, two.strand, one.reference.c_str(), one.position, one.strand);
                else
                    printf("%c %s %u %c %s %u %c\n", status, one.reference.c_str(), one.position, one.strand, two.reference.c_str(), two.position, two.strand);
            }
        }
    }
    
    return 0;
}

static int process(std::string const &irdb, std::ostream &out)
{
    auto const writer = VDB::Writer(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("layout-stats", "1.0.0");
    
    writer.beginWriting();
    
    auto const mgr = VDB::Manager();
    auto const result = process(writer, mgr[irdb]);
    
    writer.endWriting();
    
    return result;
}

namespace outlierStatistics {
    static void usage(std::string const &program, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << program << " [-out=<path>] <ir db>" << std::endl;
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
            return process(run, std::cout);
        
        auto ofs = std::ofstream(out);
        if (ofs.bad()) {
            std::cerr << "failed to open output file: " << out << std::endl;
            exit(3);
        }
        return process(run, ofs);
    }
}

int main(int argc, char *argv[]) {
    return outlierStatistics::main(CommandLine(argc, argv));
}
