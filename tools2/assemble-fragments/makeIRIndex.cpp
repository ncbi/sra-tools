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
// likely that values that differ by 1 bit
// will map to values that differ by 4 bits
static uint8_t const *makeHashTable(void)
{
    static uint8_t table[256];

    uint8_t I[256];
    uint8_t J[256];
    
    initialize(I, 1);
    initialize(J, 4);
    
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

static IndexRow makeIndexRow(int64_t row, VDB::Cursor::RawData const &readGroup, VDB::Cursor::RawData const &fragment)
{
    IndexRow y;
    uint8_t const *const H = hashTable;
    int i;
    
    for (i = 0; i < 8; ++i)
        y.key[i] = salt[i];

    for (i = 0; i < readGroup.elements; ++i) {
        auto const ii = ((uint8_t const *)readGroup.data)[i];
        y.key[0] = H[y.key[0] ^ ii];
        y.key[1] = H[y.key[1] ^ ii];
        y.key[2] = H[y.key[2] ^ ii];
        y.key[3] = H[y.key[3] ^ ii];
        y.key[4] = H[y.key[4] ^ ii];
        y.key[5] = H[y.key[5] ^ ii];
        y.key[6] = H[y.key[6] ^ ii];
        y.key[7] = H[y.key[7] ^ ii];
    }
    for (i = 0; i < fragment.elements; ++i) {
        auto const ii = ((uint8_t const *)fragment.data)[i];
        y.key[0] = H[y.key[0] ^ ii];
        y.key[1] = H[y.key[1] ^ ii];
        y.key[2] = H[y.key[2] ^ ii];
        y.key[3] = H[y.key[3] ^ ii];
        y.key[4] = H[y.key[4] ^ ii];
        y.key[5] = H[y.key[5] ^ ii];
        y.key[6] = H[y.key[6] ^ ii];
        y.key[7] = H[y.key[7] ^ ii];
    }
    y.row = row;
    return y;
}

static std::ostream &operator <<(std::ostream &os, IndexRow const &row)
{
    return os.write((char const *)row.key, 16);
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

static int process(char const *const run)
{
    auto const mgr = VDB::Manager();
    auto const result = process(std::cout, mgr[run]);
    return result;
}

int main(int argc, char *argv[])
{
    if (argc == 2)
        return process(argv[1]);
    else {
        std::cerr << "usage: makeIRIndex <run>" << std::endl;
        return 1;
    }
}
