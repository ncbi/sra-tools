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

static int popcount(int x)
{
    int y = 0;
    while (x) {
        x &= x - 1;
        ++y;
    }
    return y;
}

template <unsigned N, typename T>
static void shuffle(T a[N])
{
    for (int i = 0; i < N; ++i) {
        auto const j = arc4random_uniform(N - i) + i;
        auto const ii = a[i];
        auto const jj = a[j];
        a[i] = jj;
        a[j] = ii;
    }
}

static void initialize(uint8_t a[256], int adjacentDelta)
{
    int i, j;
    
    for (i = 0; i < 256; ++i)
        a[i] = i;
    
    shuffle<256>(a);

    // this makes it more likely that adjacent values
    // will differ by the desired bit count
    for (i = 0, j = 1; j < 256; ++i, ++j) {
        auto const ii = a[i];
        
        for (int k = j; k < 256; ++k) {
            auto const jj = a[k];
            if (popcount(ii ^ jj) == adjacentDelta) {
                a[i] = jj;
                a[k] = ii;
                break;
            }
        }
    }
}

// this table is constructed such that it is more
// likely that two values that differ by 1 bit
// will map to two values that differ by 4 bits
static uint8_t const *makeHashTable(void)
{
    static uint8_t table[256];

    uint8_t I[256];
    uint8_t J[256];
    
    initialize(I, 1); // adjacent values of I are biased to differ by 1-bit
    initialize(J, 4); // adjacent values of J are biased to differ by 4-bits
    
    for (int i = 0; i < 256; ++i)
        table[I[i]] = J[i];
    
    return table;
}

// all of the bytes in the salt MUST be different values
// this function insures that (but doesn't check it)
static uint8_t const *makeSalt(void)
{
    // all of the 8-bit values with popcount == 4
    static uint8_t sb4[] = {
        0x33, 0x35, 0x36, 0x39, 0x3A, 0x3C,
        0x53, 0x55, 0x56, 0x59, 0x5A, 0x5C,
        0x63, 0x65, 0x66, 0x69, 0x6A, 0x6C,
        0x93, 0x95, 0x96, 0x99, 0x9A, 0x9C,
        0xA3, 0xA5, 0xA6, 0xA9, 0xAA, 0xAC,
        0xC3, 0xC5, 0xC6, 0xC9, 0xCA, 0xCC
    };
    
    shuffle<36>(sb4);

    return sb4;
}

static uint8_t const *const hashTable = makeHashTable();
static uint8_t const *const salt = makeSalt();

static IndexRow makeIndexRow(int64_t row, VDB::Cursor::RawData const &group, VDB::Cursor::RawData const &name)
{
    union {
        uint64_t u;
        char ch[sizeof(uint64_t)];
    } key;
    uint64_t sr;
    uint8_t const *const H = hashTable;
    int i;
    
    for (i = 0; i < sizeof(uint64_t); ++i) {
        key.ch[i] = salt[i];
    }

    for (sr = 0, i = 0; i < group.elements; ++i) {
        auto const ch = ((uint8_t const *)group.data)[i];
        sr = (sr << 8) | ch;
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
    sr = (sr << 8) | '\0';
    key.u ^= sr;
    key.ch[0] = H[key.ch[0]];
    key.ch[1] = H[key.ch[1]];
    key.ch[2] = H[key.ch[2]];
    key.ch[3] = H[key.ch[3]];
    key.ch[4] = H[key.ch[4]];
    key.ch[5] = H[key.ch[5]];
    key.ch[6] = H[key.ch[6]];
    key.ch[7] = H[key.ch[7]];
    for (i = 0; i < name.elements; ++i) {
        auto const ch = ((uint8_t const *)name.data)[i];
        sr = (sr << 8) | ch;
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
    while (sr != 0) {
        sr = sr << 8;
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

    IndexRow y;
    y.row = row;
    for (i = 0; i < sizeof(uint64_t); ++i) {
        y.key[i] = key.ch[i];
    }
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
