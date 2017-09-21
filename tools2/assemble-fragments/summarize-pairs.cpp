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
#include <set>
#include <map>
#include <string>
#include <sstream>
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

static strings_map references;
static strings_map groups = {""};

template <typename T>
static bool string_to_i(T &result, std::string const &str, int radix = 0)
{
    try {
        std::string::size_type end = 0;
        auto const temp = std::stoll(str, &end, radix);
        result = T(temp);
        return temp == decltype(temp)(result) && str.begin() + end == str.end();
    }
    catch (...) {
        return false;
    }
}

template <typename T>
static bool string_to_u(T &result, std::string const &str, int radix = 0)
{
    try {
        std::string::size_type end = 0;
        auto const temp = std::stoul(str, &end, radix);
        result = T(temp);
        return temp == decltype(temp)(result) && str.begin() + end == str.end();
    }
    catch (...) {
        return false;
    }
}

struct StatisticsAccumulator {
    unsigned long long N;
    double sum;
    double M2;
    
    StatisticsAccumulator() : N(0) {}
    explicit StatisticsAccumulator(double const value) : sum(value), M2(0.0), N(1) {}
    StatisticsAccumulator(StatisticsAccumulator const &a, StatisticsAccumulator const &b)
    : N(a.N + b.N)
    , sum(a.sum + b.sum)
    , M2(a.M2 + b.M2)
    {
        if (a.N != 0 && b.N != 0) {
            double const d = a.average() - b.average();
            M2 += d * d * a.N * b.N / N;
        }
    }

    double average() const { return sum / N; }
    double variance() const { return M2 / N; }
    operator bool() const { return N != 0; }
};

struct ContigPair { ///< a pair of contigs that are KNOWN to be joined, e.g. the two reads of a paired-end fragment
    struct Contig { ///< a contig is nothing more than a contiguous region on some reference; a read is a contig by this definition
        unsigned ref;
        int start;
        int end;
        StatisticsAccumulator qlength;
        StatisticsAccumulator rlength;
        
        Contig() {}

        Contig(Contig const &a, Contig const &b)
        : ref(a.ref)
        , start(std::min(a.start, b.start))
        , end(std::max(a.end, b.end))
        , qlength(a.qlength, b.qlength)
        , rlength(a.rlength, b.rlength)
        {}

        Contig(Alignment const &algn, CIGAR const &cigar)
        : ref(unsigned(references[algn.reference]))
        , start(algn.position - cigar.qfirst)
        , end(algn.position + cigar.rlength + cigar.qclip)
        , qlength(cigar.qlength)
        , rlength(cigar.rlength)
        {}
        
        bool operator<(Contig const &other) const {
            if (ref > other.ref) return false;
            if (ref < other.ref) return true;
            if (start > other.start) return false;
            if (start < other.start) return true;
            return end < other.end;
        }
    };
    Contig first, second;
    StatisticsAccumulator fragmentLength;
    unsigned group;
    
    double mean() const { return fragmentLength.average(); }
    double variance() const { return fragmentLength.variance(); }
    unsigned count() const { return unsigned(fragmentLength.N); }
    
    int compare(ContigPair const &other) const
    {
        if (first.ref > other.first.ref)
            return 1;
        if (first.ref < other.first.ref)
            return -1;
        
        if (first.start > other.first.start)
            return 1;
        if (first.start < other.first.start)
            return -1;
        
        if (first.end > other.first.end)
            return 1;
        if (first.end < other.first.end)
            return -1;
        
        if (second.ref > other.second.ref)
            return 1;
        if (second.ref < other.second.ref)
            return -1;
        
        if (second.start > other.second.start)
            return 1;
        if (second.start < other.second.start)
            return -1;
        
        if (second.end > other.second.end)
            return 1;
        if (second.end < other.second.end)
            return -1;
        
        if (group > other.group)
            return 1;
        if (group < other.group)
            return -1;
        
        return 0;
    }
    bool operator<(ContigPair const &other) const { return compare(other) < 0; }
    operator bool() const { return bool(fragmentLength); }
    
    ContigPair(ContigPair const &a, ContigPair const &b) ///< create a new pair that is the union of the two pairs; it is assumed that they overlap properly
    : first(a.first, b.first)
    , second(a.second, b.second)
    , group(a.group)
    , fragmentLength(a.fragmentLength + b.fragmentLength)
    {}
    
    ContigPair(Alignment const &one, Alignment const &two, std::string const &group)
    : group(unsigned(groups[group]))
    {
        auto const &c1 = Contig(one, CIGAR(one.cigar));
        auto const &c2 = Contig(two, CIGAR(two.cigar));
        
        if (two.reference < one.reference || (c2.ref == c1.ref && c2 < c1)) {
            first = c2;
            second = c1;
        }
        else {
            first = c1;
            second = c2;
        }
        fragmentLength = StatisticsAccumulator(first.ref == second.ref ? double(second.end - first.start) : 0.0);
    }
    
    explicit ContigPair(std::istream &in)
    {
        std::string line;
        if (std::getline(in, line)) {
            int n = 0;
            {
                std::string::size_type i = 0, j = 0;
                while (n < 11) {
                    unsigned u;
                    
                    j = line.find('\t', i);

                    auto const &fld = j != std::string::npos ? line.substr(i, j - i) : line.substr(i);
                    switch (n) {
                        case 0:
                            first.ref = unsigned(references[fld]);
                            break;
                        case 1:
                            if (!string_to_i(first.start, fld)) goto CONVERSION_ERROR;
                            break;
                        case 2:
                            if (!string_to_i(first.end, fld)) goto CONVERSION_ERROR;
                            break;
                        case 3:
                            if (!string_to_u(u, fld)) goto CONVERSION_ERROR;
                            first.qlength = StatisticsAccumulator(u);
                            break;
                        case 4:
                            if (!string_to_u(u, fld)) goto CONVERSION_ERROR;
                            first.rlength = StatisticsAccumulator(u);
                            break;
                        case 5:
                            second.ref = unsigned(references[fld]);
                            break;
                        case 6:
                            if (!string_to_i(second.start, fld)) goto CONVERSION_ERROR;
                            break;
                        case 7:
                            if (!string_to_i(second.end, fld)) goto CONVERSION_ERROR;
                            break;
                        case 8:
                            if (!string_to_u(u, fld)) goto CONVERSION_ERROR;
                            second.qlength = StatisticsAccumulator(u);
                            break;
                        case 9:
                            if (!string_to_u(u, fld)) goto CONVERSION_ERROR;
                            second.rlength = StatisticsAccumulator(u);
                            break;
                        case 10:
                            group = unsigned(groups[fld]);
                            break;
                    }
                    ++n;
                    if (j == std::string::npos)
                        break;
                    i = j + 1;
                }
                if (j != std::string::npos) {
                    std::cerr << "extra data in record: " << line << std::endl;
                }
                else if (n < 8) {
                    std::cerr << "truncated record: " << line << std::endl;
                    return;
                }
            }
            fragmentLength = StatisticsAccumulator(first.ref == second.ref ? double(second.end - first.start) : 0.0);
            
            return;
            
        CONVERSION_ERROR:
            std::cerr << "error parsing record: " << line << std::endl;
            return;
        }
    }
    
    friend std::ostream &operator <<(std::ostream &strm, ContigPair const &i) {
        auto const &ref1 = references[i.first.ref];
        auto const &ref2 = references[i.second.ref];
        auto const &grp = groups[i.group];
        strm
             << ref1 << '\t' << i.first.start << '\t' << i.first.end << '\t' << (unsigned)i.first.qlength.average() << '\t' << (unsigned)i.first.rlength.average() << '\t'
             << ref2 << '\t' << i.second.start << '\t' << i.second.end << '\t' << (unsigned)i.second.qlength.average() << '\t' << (unsigned)i.second.rlength.average() << '\t'
             << grp;
        return strm;
    }
    void write(VDB::Writer const &out) const {
        out.value( 1, references[first.ref]);
        out.value( 2, (int32_t)first.start);
        out.value( 3, (int32_t)first.end);
        out.value( 4, float(first.qlength.average()));
        out.value( 5, float(sqrt(first.qlength.variance())));
        out.value( 6, float(first.rlength.average()));
        out.value( 7, float(sqrt(first.rlength.variance())));

        out.value( 8, references[second.ref]);
        out.value( 9, (int32_t)second.start);
        out.value(10, (int32_t)second.end);
        out.value(11, float(second.qlength.average()));
        out.value(12, float(sqrt(second.qlength.variance())));
        out.value(13, float(second.rlength.average()));
        out.value(14, float(sqrt(second.rlength.variance())));
        
        out.value(15, (uint32_t)count());
        out.value(16, (float)mean());
        out.value(17, (float)sqrt(variance()));
        out.value(18, groups[group]);
        
        out.closeRow(1);
    }
    static void setup(VDB::Writer const &writer) {
        writer.openTable(1, "CONTIG_STATS");

        writer.openColumn( 1, 1,  8, "REFERENCE_1");
        writer.openColumn( 2, 1, 32, "START_1");
        writer.openColumn( 3, 1, 32, "END_1");
        writer.openColumn( 4, 1, 32, "SEQ_LENGTH_AVERAGE_1");
        writer.openColumn( 5, 1, 32, "SEQ_LENGTH_STD_DEV_1");
        writer.openColumn( 6, 1, 32, "REF_LENGTH_AVERAGE_1");
        writer.openColumn( 7, 1, 32, "REF_LENGTH_STD_DEV_1");
        
        writer.openColumn( 8, 1,  8, "REFERENCE_2");
        writer.openColumn( 9, 1, 32, "START_2");
        writer.openColumn(10, 1, 32, "END_2");
        writer.openColumn(11, 1, 32, "SEQ_LENGTH_AVERAGE_2");
        writer.openColumn(12, 1, 32, "SEQ_LENGTH_STD_DEV_2");
        writer.openColumn(13, 1, 32, "REF_LENGTH_AVERAGE_2");
        writer.openColumn(14, 1, 32, "REF_LENGTH_STD_DEV_2");

        writer.openColumn(15, 1, 32, "COUNT");
        writer.openColumn(16, 1, 32, "FRAGMENT_LENGTH_AVERAGE");
        writer.openColumn(17, 1, 32, "FRAGMENT_LENGTH_STD_DEV");
        writer.openColumn(18, 1,  8, "READ_GROUP");
    }
};

struct Stats {
    std::set<ContigPair> active;
    std::set<ContigPair> held; ///< held here for sorting

private:
    void deactivateBefore(int position) { ///< move inactive entries to the sort buffer
        auto i = active.begin();
        while (i != active.end()) {
            auto const p = *i++;
            if (p.first.end <= position) {
                held.insert(p);
                active.erase(p);
            }
        }
    }
    void merge(ContigPair pair) { ///< attempt to merge a new pair with an existing entry (try from newest-to-oldest)
        auto i = active.rbegin();
        while (i != active.rend()) {
            auto j = *i++;
            if (j.group != pair.group) continue;
            if (j.second.ref != pair.second.ref) continue;
            if (pair.second.start >= j.second.end) continue;
            if (pair.second.end <= j.second.start) continue;
            
            pair = ContigPair(j, pair); ///< the merged pair becomes the new pair
            active.erase(j); ///< the old pair is removed from the list
        }
        active.insert(pair);
    }
    std::vector<ContigPair> removeLessThan(ContigPair const &p) { ///< remove (and return) any entries from the sort buffer
        auto result = std::vector<ContigPair>();
        auto const &first = *active.begin();
        auto i = held.begin();
        while (i != held.end()) {
            auto j = *i++;
            if (first < j) break;
            result.push_back(j);
            held.erase(j);
        }
        return result;
    }
public:
    std::vector<ContigPair> add(ContigPair candidate) {
        auto result = std::vector<ContigPair>();
        
        if (active.empty()) {
            active.insert(candidate);
        }
        else if (candidate.first.ref != active.begin()->first.ref) {
            result = clear();
            active.insert(candidate);
        }
        else {
            deactivateBefore(candidate.first.start);
            merge(candidate);
            result = removeLessThan(*active.begin());
        }
        return result;
    }
    std::vector<ContigPair> clear() {
        held.insert(active.begin(), active.end());
        active.clear();
        auto result = std::vector<ContigPair>(held.begin(), held.end());
        held.clear();
        return result;
    }
};

static int process(VDB::Writer const &out, std::istream &ifs)
{
    auto stats = Stats();

    while (auto pair = ContigPair(ifs))
    {
        auto const &dead = stats.add(pair);

        for (auto && i : dead) {
            i.write(out);
        }
    }
    for (auto && i : stats.clear()) {
        i.write(out);
    }
    return 0;
}

static int reduce(std::ostream &out, std::string const &source)
{
    std::ifstream ifs;
    if (source != "-") {
        ifs.open(source);
        if (!ifs) {
            std::cerr << "failed to open pairs file: " << source << std::endl;
            exit(3);
        }
    }
    std::istream &in = source == "-" ? std::cin : ifs;
    auto const writer = VDB::Writer(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("summarize-pairs", "1.0.0");
    
    ContigPair::setup(writer);

    writer.beginWriting();
    auto const result = process(writer, in);
    writer.endWriting();
    
    return result;
}

static int map(std::ostream &out, std::string const &run)
{
    auto const mgr = VDB::Manager();
    auto const inDb = mgr[run];
    auto const in = Fragment::Cursor(inDb["RAW"]);
    auto const range = in.rowRange();
    
    for (auto row = range.first; row < range.second; ) {
        auto const fragment = in.read(row, range.second);
        
        for (auto && one : fragment.detail) {
#if 1
            if (one.reference != "chrUn_JTFH01001899v1_decoy") continue;
#endif
            if (one.readNo != 1 || !one.aligned) continue;

            auto const oneCIGAR = CIGAR(one.cigar);
            
            for (auto && two : fragment.detail) {
                if (two.readNo != 2 || !two.aligned) continue;
                
                auto const &pair = ContigPair(one, two, fragment.group);
                out << pair << std::endl;
            }
        }
    }
    return 0;
}

namespace pairsStatistics {
    static void usage(std::string const &program, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << program << " [-out=<path>] (map <sra run> | reduce <pairs>)" << std::endl;
        exit(error ? 3 : 0);
    }
    
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine.program, false);
            }
        }
        auto outPath = std::string();
        auto source = std::string();
        auto verb = decltype(&reduce)(nullptr);
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                outPath = arg.substr(5);
                continue;
            }
            if (verb == nullptr) {
                if (arg == "map")
                    verb = &map;
                else if (arg == "reduce")
                    verb = &reduce;
                else
                    usage(commandLine.program, true);
                continue;
            }
            if (source.empty()) {
                source = arg;
                continue;
            }
            usage(commandLine.program, true);
        }
        
        if (source.empty())
            usage(commandLine.program, true);
        
        std::ofstream ofs;
        if (!outPath.empty()) {
            ofs.open(outPath);
            if (!ofs) {
                std::cerr << "failed to open output file: " << outPath << std::endl;
                exit(3);
            }
        }
        std::ostream &out = outPath.empty() ? std::cout : ofs;

        return (*verb)(out, source);
    }
}

int main(int argc, char *argv[]) {
    return pairsStatistics::main(CommandLine(argc, argv));
}
