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
    static std::string layoutName(int layout) {
        char const *names[] = {
            "xFxR",
            "xFxF",
            "xRxF",
            "xRxR",
            "1F2R",
            "1F2F",
            "1R2F",
            "1R2R",
            "2F1R",
            "2F1F",
            "2R1F",
            "2R1R",
        };
        return names[layout];
    }
    static unsigned layout(unsigned position1, char strand1, unsigned position2, char strand2)
    {
        auto const order(position1 < position2 ? 1 : position2 < position1 ? 2 : 0);
        auto const one_plus(strand1 == '+' ? 0 : 1);
        auto const same_strand(strand1 == strand2 ? 1 : 0);
        return same_strand | (one_plus << 1) | (order << 2);
    }
    static unsigned transposedLayout(unsigned const layout) {
        auto const m = (layout & 0xC) == 0 ? 0x3 : 0xF;
        return (layout ^ 0xE) & m;
    }
    
    double totalCount() const {
        auto total = 0.0;
        for (auto && i : count) {
            total += i;
        }
        return total;
    }
    
    std::pair<double, double> frequency(int const layout) const {
        auto const n = totalCount();
        auto const n1 = count[layout] / n;
        auto const n2 = count[transposedLayout(layout)] / n;
        return std::make_pair(n1, n2);
    }

    // which layout (+ its transposed layout) occurs most often
    int mode() const {
        auto mc = int64_t(0);
        auto m = -1;
        for (auto layout = 0; layout < 12; ++layout) {
            auto const comp = transposedLayout(layout);
            if (comp < layout) {
                auto const c = count[layout] + count[comp];
                if (mc < c) {
                    mc = c;
                    m = layout;
                }
            }
        }
        return m;
    }
    void record(unsigned position1, char strand1, unsigned position2, char strand2)
    {
        ++count[layout(position1, strand1, position2, strand2)];
    }
};

struct FragmentStats {
    int layout;
    std::map<unsigned, unsigned> length;

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
    unsigned equipartition(unsigned const n, unsigned *part) const {
        auto const N = sum();
        auto const D = N / (n + 1);
        unsigned M = 0;
        unsigned Q = 0;
        auto T = unsigned(D);
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
                x = 0;
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
            auto const p = double(i.second) / N;
            auto const x = double(i.first);
            m += x * p;
        }
        return m;
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
            auto const p = double(i.second) / N;
            auto const x = double(i.first);
            auto const diff = x - mean;
            var += diff * diff * p;
        }
        return var;
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
        auto const order = layout >> 2;
        if (order != 1 && order != 2)
            layout = -1;
        fragments[fragment.group].layout = layout;
    }
    auto &stats = fragments[fragment.group];
    auto const layout = stats.layout;
    if (stats.layout < 0)
        return;

    auto const clayout = LayoutStats::transposedLayout(layout);
    
    for (auto && one : fragment.detail) {
        if (one.readNo != 1 || !one.aligned) continue;

        auto const c1 = CIGAR(one.cigar);
        auto const f1 = one.position - c1.qfirst;
        auto const e1 = one.position + c1.rlength + c1.qclip;
        
        for (auto && two : fragment.detail) {
            if (two.readNo != 2 || !two.aligned || one.reference != two.reference) continue;
            auto const thisLayout = LayoutStats::layout(one.position, one.strand, two.position, two.strand);

            if (thisLayout != layout && thisLayout != clayout) continue;

            auto const c2 = CIGAR(one.cigar);
            auto const f2 = two.position - c2.qfirst;
            auto const e2 = two.position + c2.rlength + c2.qclip;
            auto const min = std::min(f1, std::min(f2, std::min(e1, e2)));
            auto const max = std::max(f1, std::max(f2, std::max(e1, e2)));
            auto const length = max - min;
            if (length > 0)
                stats.length[length] += 1;
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
        out.value(2, LayoutStats::layoutName(layout));
        out.value(3, freq.first);
        out.value(4, freq.second);
        out.value(5, stats.first);
        out.value(6, sqrt(stats.second));
        out.value(7, uint32_t(mode));
        out.value(8, uint32_t(median));
        out.value(9, sizeof(equi)/sizeof(equi[0]), (uint32_t const *)equi);
        out.closeRow(1);
    }
    std::cerr << "info: done" << std::endl;

    return 0;
}

static int process(char const *const irdb)
{
#if 1
    auto const writer = VDB::Writer(std::cout);
#else
    auto devNull = std::ofstream("/dev/null");
    auto const writer = VDB::Writer(devNull);
#endif
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
    writer.openColumn(9, 1, 32 * 12, "FRAGMENT_LENGTH_EQUIPART");

    writer.beginWriting();

    auto const mgr = VDB::Manager();
    auto const result = process(writer, mgr[irdb]);
    
    writer.endWriting();
    
    return result;
}

int main(int argc, char *argv[])
{
    if (argc == 2)
        return process(argv[1]);
    else {
        std::cerr << "usage: " << VDB::programNameFromArgv0(argv[0]) << " <ir db>" << std::endl;
        return 1;
    }
}
