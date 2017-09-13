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

static void write(VDB::Writer const &out, Fragment const &fragment, unsigned one, unsigned two)
{
    
}

struct Fragment2 {
    unsigned one, two;
    unsigned score;
    
    friend bool operator <(Fragment2 const &a, Fragment2 const &b)
    {
        if (a.score < b.score)
            return true;
        if (a.score > b.score)
            return false;
        if (a.one < b.one)
            return true;
        if (a.one > b.one)
            return false;
        if (a.two < b.two)
            return true;
        return false;
    }
};

static int assemble(std::ostream &out, std::string const &run)
{
    auto const mgr = VDB::Manager();
    auto const inDb = mgr[run];
    auto const stats = inDb["CONTIG_STATS"];
    auto const in = Fragment::Cursor(inDb["RAW"]);
    auto const range = in.rowRange();
    auto writer = VDB::Writer(out);
    
    for (auto row = range.first; row < range.second; ) {
        auto const fragment = in.read(row, range.second);
        auto const n = fragment.detail.size();
        auto pairs = std::set<Fragment2>();

        for (auto one = unsigned(0); one < n; ++one) {
            if (fragment.detail[one].readNo != 1 || fragment.detail[one].aligned == false) continue;
            for (auto two = unsigned(0); two < n; ++two) {
                if (fragment.detail[two].readNo != 2 || fragment.detail[two].aligned == false) continue;
                
                Fragment2 pair = { one, two, 0 };
                pairs.insert(pair);
            }
        }
        auto result = pairs.begin();
        if (pairs.size() > 1) {
            
        }
        if (result != pairs.end())
            write(writer, fragment, result->one, result->two);
    }
    return 0;
}

namespace assembleFragments {
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
        auto outPath = std::string();
        auto source = std::string();
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                outPath = arg.substr(5);
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

        return assemble(out, source);
    }
}

int main(int argc, char *argv[]) {
    return assembleFragments::main(CommandLine(argc, argv));
}
