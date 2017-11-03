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
    uint8_t key[8];
    
    static uint8_t popcount(uint8_t const byte) {
        static uint8_t const bits_set[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
        return bits_set[byte >> 4] + bits_set[byte & 15];
    }

    // this table is constructed such that it is more
    // likely that two values that differ by 1 bit (the smallest difference)
    // will map to two values that differ by 4 bits (the biggest difference)
    // the desired effect is to amplify small differences
    static uint8_t const *makeHashTable(int n)
    {
        static uint8_t tables[256 * 8];
        auto table = tables + 256 * n;
#if 1
        for (auto i = 0; i < 256; ++i)
            table[i] = i;
        for (auto i = 0; i < 256; ++i) {
            auto const j = random_uniform(i, 256);
            auto const ii = table[i];
            auto const jj = table[j];
            table[i] = jj;
            table[j] = ii;
        }
#else
        uint8_t J[256];

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

        uint8_t I[256];
        
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

        for (int k = 0; k < 256; ++k) {
            auto const i = I[k];
            auto const j = J[k];
            table[i] = j;
        }
#endif
        return table;
    }
    static uint8_t const *makeSalt(void)
    {
        // all of the 8-bit values with popcount == 4
        static uint8_t table[] = {
            0x33, 0x35, 0x36, 0x39, 0x3A, 0x3C,
            0x53, 0x55, 0x56, 0x59, 0x5A, 0x5C,
            0x63, 0x65, 0x66, 0x69, 0x6A, 0x6C,
            0x93, 0x95, 0x96, 0x99, 0x9A, 0x9C,
            0xA3, 0xA5, 0xA6, 0xA9, 0xAA, 0xAC,
            0xC3, 0xC5, 0xC6, 0xC9, 0xCA, 0xCC
        };
        
        for (auto i = 0; i < 36; ++i) {
            auto const j = random_uniform(i, 36);
            auto const ii = table[i];
            auto const jj = table[j];
            table[i] = jj;
            table[j] = ii;
        }
        return table;
    }

    static decltype(makeHashTable(0)) const H1, H2, H3, H4, H5, H6, H7, H8;
    static decltype(makeSalt()) const H0;

public:
    HashState() {
#if 1
        std::copy(H0, H0 + 8, key);
#else
        *(uint64_t *)key = 0xcbf29ce484222325;
#endif
    }
    void append(uint8_t const byte) {
#if 1
        key[0] = H1[key[0] ^ byte];
        key[1] = H2[key[1] ^ byte];
        key[2] = H3[key[2] ^ byte];
        key[3] = H4[key[3] ^ byte];
        key[4] = H5[key[4] ^ byte];
        key[5] = H6[key[5] ^ byte];
        key[6] = H7[key[6] ^ byte];
        key[7] = H8[key[7] ^ byte];
#else
        uint64_t hash = *(uint64_t *)key;
        hash ^= byte;
        hash *= 0x100000001b3;
        *(uint64_t *)key = hash;
#endif
    }
    HashState &append(VDB::Cursor::RawData const &data) {
        for (auto i = 0; i < data.elements; ++i) {
            append(((uint8_t const *)data.data)[i]);
        }
        return *this;
    }
    HashState &append(std::string const &data) {
        for (auto && ch : data) {
            append(ch);
        }
        return *this;
    }
    void end(uint8_t rslt[8]) {
        std::copy(key, key + 8, rslt);
    }
};

auto const HashState::H0 = HashState::makeSalt();
auto const HashState::H1 = HashState::makeHashTable(0);
auto const HashState::H2 = HashState::makeHashTable(1);
auto const HashState::H3 = HashState::makeHashTable(2);
auto const HashState::H4 = HashState::makeHashTable(3);
auto const HashState::H5 = HashState::makeHashTable(4);
auto const HashState::H6 = HashState::makeHashTable(5);
auto const HashState::H7 = HashState::makeHashTable(6);
auto const HashState::H8 = HashState::makeHashTable(7);

static IndexRow makeIndexRow(int64_t row, VDB::Cursor::RawData const &group, VDB::Cursor::RawData const &name)
{
    IndexRow y;

    HashState().append(group).append(name).end(y.key);
    y.row = row;
    return y;
}

static int process(FILE *out, VDB::Database const &run)
{
    static char const *const FLDS[] = { "READ_GROUP", "NAME" };
    auto const in = run["RAW"].read(2, FLDS);
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 100.0;
    auto nextReport = 1;
    
    for (auto row = range.first; row < range.second; ++row) {
        auto const readGroup = in.read(row, 1);
        auto const fragment = in.read(row, 2);
        auto const indexRow = makeIndexRow(row, readGroup, fragment);
        
        fwrite(&indexRow, sizeof(indexRow), 1, out);

        if (nextReport * freq <= row - range.first) {
            std::cerr << "processed " << nextReport << '%' << std::endl;;
            ++nextReport;
        }
    }
    std::cerr << "processed 100% " << (range.second - range.first) << " records" << std::endl;
    
    return 0;
}

static int process(std::string const &run, FILE *out)
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
            return process(run, stdout);
        
        auto ofs = fopen(out.c_str(), "w");
        if (ofs == nullptr) {
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
