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

struct ContigPair { ///< a pair of contigs that are KNOWN to be joined, e.g. the two reads of a paired-end fragment
    struct Contig { ///< a contig is nothing more than a contiguous region on some reference; a read is a contig by this definition
        unsigned length;
        unsigned ref;
        int start;
        int end;
        
        Contig() {}
        Contig(unsigned ref, int start, int end, unsigned length)
        : ref(ref)
        , start(start)
        , end(end)
        , length(length)
        {}

        Contig(Alignment const &algn, CIGAR const &cigar)
        : ref(unsigned(references[algn.reference]))
        , start(algn.position - cigar.qfirst)
        , end(algn.position + cigar.rlength + cigar.qclip)
        , length(cigar.qlength)
        {}
    };
    Contig first, second;
    double sum; ///< the sum of the fragment lengths
    double M2; ///< used to calculate variance
    unsigned count; ///< number of constituent contigs
    unsigned group;
    
    double mean() const {
        return sum / count;
    }
    double variance() const {
        return M2 / count;
    }
    
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
    operator bool() const { return bool(count); }
    
    static double M2_delta(ContigPair const &a, ContigPair const &b) {
        double const d = a.mean() - b.mean();
        return d * d * a.count * b.count / (a.count + b.count);
    }
    
    ContigPair(ContigPair const &a, ContigPair const &b) ///< create a new pair that is the union of the two pairs; it is assumed that they overlap properly
    : first(a.first.ref, std::min(a.first.start, b.first.start), std::max(a.first.end, b.first.end), a.first.length + b.first.length)
    , second(a.second.ref, std::min(a.second.start, b.second.start), std::max(a.second.end, b.second.end), a.second.length + b.second.length)
    , count(a.count + b.count)
    , group(a.group)
    , sum(a.sum + b.sum)
    , M2(a.M2 + b.M2 + M2_delta(a, b))
    {}
    
    explicit ContigPair(std::istream &in)
    : count(0)
    , M2(0.0)
    {
        std::string line;
        if (std::getline(in, line)) {
            auto in = std::istringstream(line);
            std::string str;;
            
            if (!(in >> str)) return; first.ref = unsigned(references[str]);
            if (!(in >> first.start)) return;
            if (!(in >> first.end)) return;
            if (!(in >> first.length)) return;
            
            if (!(in >> str)) return; second.ref = unsigned(references[str]);
            if (!(in >> second.start)) return;
            if (!(in >> second.end)) return;
            if (!(in >> second.length)) return;

            if (in >> str) {
                group = unsigned(groups[str]);
            }
            else {
                group = unsigned(groups[""]);
            }
            
            sum = first.ref == second.ref ? second.end - first.start : 0.0;
            count = 1;
        }
    }
    
    ContigPair(Alignment const &one, CIGAR const &oneCIGAR, Alignment const &two, CIGAR const &twoCIGAR, std::string const &group)
    : first(one, oneCIGAR)
    , second(two, twoCIGAR)
    , group(unsigned(groups[group]))
    , count(1)
    , M2(0.0)
    {
        sum = first.ref == second.ref ? second.end - first.start : 0.0;
    }
    
    friend std::ostream &operator <<(std::ostream &strm, ContigPair const &i) {
        auto const &ref1 = references[i.first.ref];
        auto const &ref2 = references[i.second.ref];
        auto const &grp = groups[i.group];
        strm << ref1 << '\t' << i.first.start << '\t' << i.first.end << '\t' << i.first.length << '\t'
             << ref2 << '\t' << i.second.start << '\t' << i.second.end << '\t' << i.second.length << '\t'
             << grp;
        return strm;
    }
    void write(VDB::Writer const &out) const {
        out.value( 1, references[first.ref]);
        out.value( 2, (int32_t)first.start);
        out.value( 3, (int32_t)first.end);
        out.value( 4, float(first.length) / count);

        out.value( 5, references[second.ref]);
        out.value( 6, (int32_t)second.start);
        out.value( 7, (int32_t)second.end);
        out.value( 8, float(second.length) / count);
        
        out.value( 9, (uint32_t)count);
        out.value(10, (float)mean());
        out.value(11, (float)sqrt(variance()));
        out.value(12, groups[group]);
        
        out.closeRow(1);
    }
    static void setup(VDB::Writer const &writer) {
        writer.openTable(1, "CONTIG_STATS");

        writer.openColumn( 1, 1,  8, "REFERENCE_1");
        writer.openColumn( 2, 1, 32, "START_1");
        writer.openColumn( 3, 1, 32, "END_1");
        writer.openColumn( 4, 1, 32, "LENGTH_AVERAGE_1");
        
        writer.openColumn( 5, 1,  8, "REFERENCE_2");
        writer.openColumn( 6, 1, 32, "START_2");
        writer.openColumn( 7, 1, 32, "END_2");
        writer.openColumn( 8, 1, 32, "LENGTH_AVERAGE_2");

        writer.openColumn( 9, 1, 32, "COUNT");
        writer.openColumn(10, 1, 32, "FRAGMENT_LENGTH_AVERAGE");
        writer.openColumn(11, 1, 32, "FRAGMENT_LENGTH_STD_DEV");
        writer.openColumn(12, 1,  8, "READ_GROUP");
    }
};

struct Stats {
    std::set<ContigPair> active;
    std::set<ContigPair> held; ///< held here for sorting

private:
    void deactivateBefore(unsigned position) { ///< move inactive entries to the sort buffer
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
            if (one.readNo != 1 || !one.aligned) continue;

            auto const oneCIGAR = CIGAR(one.cigar);
            
            for (auto && two : fragment.detail) {
                if (two.readNo != 2 || !two.aligned) continue;
                
                auto const twoCIGAR = CIGAR(two.cigar);
                auto reverse = false;
                
                if (one.reference != two.reference)
                    reverse = two.reference < one.reference;
                else
                    reverse = (two.position - twoCIGAR.qfirst) < (one.position - oneCIGAR.qfirst);
                
                auto const pair = reverse ? ContigPair(two, twoCIGAR, one, oneCIGAR, fragment.group)
                                          : ContigPair(one, oneCIGAR, two, twoCIGAR, fragment.group);
                
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
