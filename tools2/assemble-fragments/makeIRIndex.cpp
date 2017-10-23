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
#include "IRIndex.h"

static int random_uniform(int lower_bound, int upper_bound) {
    if (upper_bound <= lower_bound) return lower_bound;
#if __APPLE__
    return arc4random_uniform(upper_bound - lower_bound) + lower_bound;
#else
    struct seed_rand {
        seed_rand() {
            srand((unsigned)time(0));
        }
    };
    auto const M = upper_bound - lower_bound;
    auto const once = seed_rand();
    auto const m = RAND_MAX - RAND_MAX % M;
    for ( ; ; ) {
        auto const r = rand();
        if (r < m) return r % M + lower_bound;
    }
#endif
}

struct HashState {
private:
    union {
        uint64_t u;
        char ch[sizeof(uint64_t)];
    } key;
    uint64_t sr;
    
    static uint8_t popcount(uint8_t const byte) {
        static uint8_t const bits_set[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
        return bits_set[byte >> 4] + bits_set[byte & 15];
    }

    // this table is constructed such that it is more
    // likely that two values that differ by 1 bit (the smallest difference)
    // will map to two values that differ by 4 bits (the biggest difference)
    // the desired effect is to amplify small differences
    static uint8_t const *makeHashTable(void)
    {
        static uint8_t table[256];
        
        uint8_t I[256];
        uint8_t J[256];

        for (auto i = 0; i < 256; ++i)
            I[i] = i;
        for (auto i = 0; i < 256; ++i) {
            auto const j = random_uniform(i, 256);
            auto const ii = I[i];
            auto const jj = I[j];
            I[i] = jj;
            I[j] = ii;
        }
        for (auto i = 1; i < 256; ++i) {
            auto const ii = I[i - 1];
            for (auto j = i; j < 256; ++j) {
                auto const jj = I[j];
                auto const diff = ii ^ jj;
                auto const popcount(diff);
                if (popcount == 1) {
                    if (j != i) {
                        I[j] = I[i];
                        I[i] = jj;
                    }
                    break;
                }
            }
        }

        for (auto i = 0; i < 256; ++i)
            J[i] = i;
        for (auto i = 0; i < 256; ++i) {
            auto const j = random_uniform(i, 256);
            auto const ii = J[i];
            auto const jj = J[j];
            J[i] = jj;
            J[j] = ii;
        }
        for (auto i = 1; i < 256; ++i) {
            auto const ii = J[i - 1];
            for (auto j = i; j < 256; ++j) {
                auto const jj = J[j];
                auto const diff = ii ^ jj;
                auto const popcount(diff);
                if (popcount == 4) {
                    if (j != i) {
                        J[j] = J[i];
                        J[i] = jj;
                    }
                    break;
                }
            }
        }
        
        for (int k = 0; k < 256; ++k) {
            auto const i = I[k];
            auto const j = J[k];
            table[i] = j;
        }
        
        return table;
    }
    
    static decltype(makeHashTable()) const hashTable;
public:
    HashState() : sr(0) {
        key.u = 0;
    }
    void append(uint8_t const byte) {
        uint8_t const *const H = hashTable;
        sr = (sr << 8) | byte;
        key.u ^= sr;
        key.ch[0] = H[key.ch[0]];
        key.ch[1] = H[key.ch[1]];
        key.ch[2] = H[key.ch[2]];
        key.ch[3] = H[key.ch[3]];
        key.ch[4] = H[key.ch[4]];
        key.ch[5] = H[key.ch[5]];
        key.ch[6] = H[key.ch[6]];
        key.ch[7] = H[key.ch[7]];
    }
    HashState &append(VDB::Cursor::RawData const &data) {
        for (auto i = 0; i < data.elements; ++i) {
            append(((uint8_t const *)data.data)[i]);
        }
        append(0x80);
        return *this;
    }
    void end(uint8_t rslt[8]) {
        append(0x80);
        while (sr != 0)
            append(0);
        std::copy(key.ch, key.ch + 8, rslt);
    }
};

auto const HashState::hashTable = HashState::makeHashTable();

static IndexRow makeIndexRow(int64_t row, VDB::Cursor::RawData const &group, VDB::Cursor::RawData const &name)
{
    IndexRow y;

    y.row = row;
    HashState().append(group).append(name).end(y.key);
    return y;
}

static std::ostream &operator <<(std::ostream &os, IndexRow const &row)
{
    return os.write((char const *)row.key, 16); ///< 8 bytes for key + 8 bytes for row number
}

static int process(std::ostream &out, VDB::Database const &run)
{
    static char const *const FLDS[] = { "READ_GROUP", "FRAGMENT" };
    auto const in = run["RAW"].read(2, FLDS);
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 100.0;
    auto nextReport = 1;
    
    for (auto row = range.first; row < range.second; ++row) {
        auto const readGroup = in.read(row, 1);
        auto const fragment = in.read(row, 2);
        auto const indexRow = makeIndexRow(row, readGroup, fragment);
        
        out << indexRow;

        if (nextReport * freq <= row - range.first) {
            std::cerr << "processed " << nextReport << '%' << std::endl;;
            ++nextReport;
        }
    }
    std::cerr << "processed 100% " << (range.second - range.first) << " records" << std::endl;
    
    return 0;
}

static int process(std::string const &run, std::ostream &out)
{
    auto const mgr = VDB::Manager();
    return process(out, mgr[run]);
}

using namespace utility;
namespace makeIRIndex {
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

int main(int argc, char *argv[])
{
    return makeIRIndex::main(CommandLine(argc, argv));
}
