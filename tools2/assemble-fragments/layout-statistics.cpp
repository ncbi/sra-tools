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
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>
#include "vdb.hpp"
#include "writer.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "fragment.hpp"

template <typename T>
static std::ostream &write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::RawData const &in)
{
    return out.value(cid, in.elements, (T const *)in.data);
}

template <typename T>
static std::ostream &write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::Data const *in)
{
    return out.value(cid, in->elements, (T const *)in->data());
}

struct LayoutStats {
    int64_t count[12];
    
    LayoutStats() {
        memset(count, 0, sizeof(count));
    }
    
    double totalCount() const {
        auto total = 0.0;
        for (auto && i : count) {
            total += i;
        }
        return total;
    }
    
    std::pair<double, double> frequency(Layout const &layout) const {
        auto const n = totalCount();
        auto const n1 = count[layout.value] / n;
        auto const n2 = count[layout.transposed().value] / n;
        return std::make_pair(n1, n2);
    }

    // which layout (+ its transposed layout) occurs most often
    Layout mode() const {
        auto mc = int64_t(0);
        auto rslt = Layout::invalid();
        for (auto layout = Layout(0); layout; ++layout) {
            auto const comp = layout.transposed();
            if (comp < layout) {
                auto const c = count[layout.value] + count[comp.value];
                if (mc < c) {
                    mc = c;
                    rslt = layout;
                }
            }
        }
        return rslt;
    }
    void record(unsigned position1, char strand1, unsigned position2, char strand2)
    {
        ++count[Layout(position1, strand1, position2, strand2).value];
    }
};

struct FragmentStats {
    Layout layout;
    std::map<uint32_t, unsigned> length;

    std::pair<double, double> meanAndVariance() const {
        auto const N = sum();
        auto const M = mean(N);
        auto const V = variance(M, N);
        return std::make_pair(M, V);
    }
    unsigned mode() const {
        unsigned c = 0;
        unsigned m = 0;
        for (auto && n : length) {
            if (m == 0 || n.second > c) {
                m = n.first;
                c = n.second;
            }
        }
        return m;
    }
    unsigned equipartition(unsigned const n, uint32_t *part) const {
        auto const N = sum();
        auto const D = N / (n + 2);
        unsigned M = 0;
        double Q = 0.0;
        auto T = D;
        unsigned j = 0;
        
        for (auto && i : length) {
            auto const nQ = Q + i.second;
            auto x = i.first;
            if (M == 0 && nQ * 2 >= N)
                M = x;
            while (nQ >= T) {
                part[j] = x;
                if (++j == n) return M;
                T += D;
            }
            Q = nQ;
        }
        while (j < n) {
            part[++j] = 0;
        }
        return M;
    }
private:
    double mean(double const N) const {
        double m = 0.0;
        for (auto && i : length) {
            m += double(i.first) * double(i.second);
        }
        return m / N;
    }
    
    double sum() const {
        double N = 0.0;
        for (auto && i : length) {
            N += i.second;
        }
        return N;
    }
    
    double variance(double const mean, double const N) const {
        double var = 0.0;
        for (auto && i : length) {
            auto const diff = (double(i.first) - mean) * double(i.second);
            var += diff * diff;
        }
        return var / N;
    }
};

static std::map<std::string, LayoutStats> layouts;
static std::map<std::string, FragmentStats> fragments;

static void gatherLayoutStats(Fragment const &fragment)
{
    auto &counts = layouts[fragment.group];
    
    for (auto && one : fragment.detail) {
        if (one.readNo != 1 || !one.aligned) continue;
        for (auto && two : fragment.detail) {
            if (two.readNo != 2 || !two.aligned || one.reference != two.reference) continue;
            counts.record(one.position, one.strand, two.position, two.strand);
        }
    }
}

static void gatherLengthStats(Fragment const &fragment)
{
    if (fragments.find(fragment.group) == fragments.end()) {
        auto layout = layouts[fragment.group].mode();
        if (layout.order() == 0)
            layout = Layout::invalid();
        fragments[fragment.group].layout = layout;
    }
    auto &stats = fragments[fragment.group];
    auto const layout = stats.layout;
    if (!stats.layout)
        return;

    auto const clayout = layout.transposed();
    
    for (auto && one : fragment.detail) {
        if (one.readNo != 1 || !one.aligned) continue;

        for (auto && two : fragment.detail) {
            if (two.readNo != 2 || !two.aligned || one.reference != two.reference) continue;
            auto const thisLayout = Alignment::layout(one, two);

            if (thisLayout.first == layout || thisLayout.first == clayout)
                stats.length[thisLayout.second] += 1;
        }
    }
}

static int process(VDB::Writer const &out, VDB::Database const &inDb)
{
    {
        auto const in = Fragment::Cursor(inDb["RAW"]);
        
        in.foreachRow(gatherLayoutStats, "gathering fragment layout statistics ...");
        in.foreachRow(gatherLengthStats, "gathering fragment length statistics ...");
    }
    std::cerr << "info: generating output" << std::endl;
    for (auto const &i : fragments) {
        unsigned equi[12];
        auto const group = i.first;
        auto const layout = i.second.layout;
        auto const freq = layouts[group].frequency(layout);
        auto const stats = i.second.meanAndVariance();
        auto const mode = i.second.mode();
        auto const median = i.second.equipartition(sizeof(equi)/sizeof(equi[0]), equi);
        
        out.value(1, group);
        out.value(2, layout.value);
        out.value(3, freq.first);
        out.value(4, freq.second);
        out.value(5, stats.first);
        out.value(6, sqrt(stats.second));
        out.value(7, uint32_t(mode));
        out.value(8, uint32_t(median));
        out.value(9, sizeof(equi)/sizeof(equi[0]), equi);
        out.closeRow(1);
    }
    std::cerr << "info: done" << std::endl;

    return 0;
}

static int process(std::string const &irdb, std::ostream &out)
{
    auto const writer = VDB::Writer(out);

    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("layout-stats", "1.0.0");
    
    writer.openTable(1, "LAYOUT_STATS");
    writer.openColumn(1, 1, 8, "READ_GROUP");
    writer.openColumn(2, 1, 8, "LAYOUT");
    writer.openColumn(3, 1, 64, "FREQ_FORWARD");
    writer.openColumn(4, 1, 64, "FREQ_REVERSE");
    writer.openColumn(5, 1, 64, "FRAGMENT_LENGTH_AVERAGE");
    writer.openColumn(6, 1, 64, "FRAGMENT_LENGTH_STD_DEV");
    writer.openColumn(7, 1, 32, "FRAGMENT_LENGTH_MODE");
    writer.openColumn(8, 1, 32, "FRAGMENT_LENGTH_MEDIAN");
    writer.openColumn(9, 1, 32, "FRAGMENT_LENGTH_EQUIPART");

    writer.beginWriting();

    auto const mgr = VDB::Manager();
    auto const result = process(writer, mgr[irdb]);
    
    writer.endWriting();
    
    return result;
}

using namespace utility;
namespace layoutStatistics {
    static void usage(std::string const &program, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << program << " [-out=<path>] <ir db>" << std::endl;
        exit(error ? 3 : 0);
    }
    
    static int main(CommandLine const &commandLine) {
        CIGAR::test();
        
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

int main(int argc, char *argv[])
{
    return layoutStatistics::main(CommandLine(argc, argv));
}
