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
static strings_map groups;

struct ContigStats {
    struct stat {
        int64_t sourceRow;
        unsigned group;
        unsigned ref1, ref2;
        int start1, start2;
        int end1, end2;
        
        unsigned count;
        float length_average, length_std_dev;
        
        std::pair<double, double> basesPerRead() const {
            return std::make_pair(double(end1 - start1) / count, double(end2 - start2) / count);
        }
        
        friend bool operator <(stat const &a, stat const &b) {
            if (a.ref1 < b.ref1) return true;
            if (a.ref1 > b.ref1) return false;
            if (a.start1 < b.start1) return true;
            if (a.start1 > b.start1) return false;
            if (a.end1 < b.end1) return true;
            if (a.end1 > b.end1) return false;
            if (a.ref2 < b.ref2) return true;
            if (a.ref2 > b.ref2) return false;
            if (a.start2 < b.start2) return true;
            if (a.start2 > b.start2) return false;
            if (a.end2 < b.end2) return true;
            if (a.end2 > b.end2) return false;
            return a.group < b.group;
        }
        
        static stat load(VDB::Cursor const &curs, int64_t row) {
            auto const ref1 = references[curs.read(row, 1).asString()];
            auto const start1 = curs.read(row, 2).value<int32_t>();
            auto const end1 = curs.read(row, 3).value<int32_t>();
            
            auto const ref2 = references[curs.read(row, 4).asString()];
            auto const start2 = curs.read(row, 5).value<int32_t>();
            auto const end2 = curs.read(row, 6).value<int32_t>();

            auto const count = curs.read(row, 7).value<uint32_t>();
            auto const group = groups[curs.read(row, 8).asString()];
            auto const average = curs.read(row, 9).value<float>();
            auto const std_dev = curs.read(row, 10).value<float>();
            
            stat const o = { row, unsigned(group), unsigned(ref1), unsigned(ref2), int(start1), int(start2), int(end1), int(end2), unsigned(count), average, std_dev };
            
            return o;
        }
        
        friend std::ostream &operator <<(std::ostream &strm, stat const &o) {
            strm    << references[o.ref1]
            << '\t' << o.start1
            << '\t' << o.end1
            << '\t' << '(' << double(o.end1 - o.start1) / o.count << ')'
            << '\t' << references[o.ref2]
            << '\t' << o.start2
            << '\t' << o.end2
            << '\t' << '(' << double(o.end2 - o.start2) / o.count << ')'
            << '\t' << o.count
            << '\t' << '(' << double(o.end2 - o.start1) / o.length_average << ')'
            << '\t' << '(' << double(o.length_average * o.count) / double(o.end2 - o.start1) << ')';
            return strm;
        }
        
        friend std::string foo(stat const &o) {
            std::ostringstream oss;
            oss << o << std::endl;
            return oss.str();
        }
    };
    std::set<stat> stats;
    
    static ContigStats load(VDB::Database const &db) {
        ContigStats result;
        std::set<stat> temp;
        {
            auto const tbl = db["CONTIG_STATS"];
            auto const curs = tbl.read({ "REFERENCE_1", "START_1", "END_1", "REFERENCE_2", "START_2", "END_2", "COUNT", "READ_GROUP", "FRAGMENT_LENGTH_AVERAGE", "FRAGMENT_LENGTH_STD_DEV" });
            auto const range = curs.rowRange();
            
            for (auto row = range.first; row < range.second; ++row) {
#if !defined(_LIBCPP_HAS_NO_RVALUE_REFERENCES) && !defined(_LIBCPP_HAS_NO_VARIADICS)
                auto inserted = temp.emplace_hint(temp.end(), stat::load(curs, row))->sourceRow;
#else
                auto inserted = temp.insert(temp.end(), stat::load(curs, row))->sourceRow;
#endif
                if (inserted != row) {
                    std::cerr << "warning: row " << inserted << " duplicated by row " << row << std::endl;
                }
            }
            if (range.second - range.first != temp.size())
                throw std::logic_error("bad data");
        }
        for (auto i = temp.begin(); i != temp.end(); ++i) {
            auto const s = foo(*i);
        }
        return result;
    }
};

static void keep(VDB::Writer const &out, Fragment const &fragment, unsigned one, unsigned two)
{
    
}

static void reject(VDB::Writer const &out, Fragment const &fragment)
{
    
}

static std::vector<std::pair<unsigned, unsigned>>::const_iterator bestPair(std::vector<Alignment> const &alignments, std::vector<std::pair<unsigned, unsigned>> const &pairs)
{
    auto result = pairs.begin();
    if (pairs.size() > 1) {
        
    }
    return result;
}

static std::vector<std::pair<unsigned, unsigned>> makePairs(std::vector<Alignment> const &alignments)
{
    auto pairs = std::vector<std::pair<unsigned, unsigned>>();
    auto const n = alignments.size();
    
    for (auto one = unsigned(0); one < n; ++one) {
        if (alignments[one].readNo != 1 || alignments[one].aligned == false) continue;
        for (auto two = unsigned(0); two < n; ++two) {
            if (alignments[two].readNo != 2 || alignments[two].aligned == false) continue;
            
            pairs.push_back({ one, two });
        }
    }
    return pairs;
}

static int assemble(std::ostream &out, std::string const &data_run, std::string const &stats_run)
{
    auto writer = VDB::Writer(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("assemble-fragments", "1.0.0");

    auto const mgr = VDB::Manager();
    auto const stats = ContigStats::load(mgr[stats_run]);
    auto const inDb = mgr[data_run];
    auto const in = Fragment::Cursor(inDb["RAW"]);
    auto const range = in.rowRange();
    
    for (auto row = range.first; row < range.second; ) {
        auto const fragment = in.read(row, range.second);
        auto const pairs = makePairs(fragment.detail);
        auto const result = bestPair(fragment.detail, pairs);
        if (result != pairs.end())
            keep(writer, fragment, result->first, result->second);
        else
            reject(writer, fragment);
    }
    return 0;
}

namespace assembleFragments {
    static void usage(std::string const &program, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << program << " [-out=<path>] <sra run> [<stats run>]" << std::endl;
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
        auto source2 = std::string();
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                outPath = arg.substr(5);
                continue;
            }
            if (source.empty()) {
                source = arg;
                continue;
            }
            if (source2.empty()) {
                source2 = arg;
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

        return assemble(out, source, source2.empty() ? source : source2);
    }
}

int main(int argc, char *argv[]) {
    return assembleFragments::main(CommandLine(argc, argv));
}
