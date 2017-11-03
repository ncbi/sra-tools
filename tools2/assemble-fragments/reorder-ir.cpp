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
#include "vdb.hpp"
#include "writer.hpp"
#include "IRIndex.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

static bool write(Writer2::Column const &out, VDB::Cursor::Data const *in)
{
    return out.setValue(in->elements, in->elem_bits / 8, in->data());
}

using namespace utility;

static int process(Writer2 const &out, VDB::Cursor const &in, IndexRow const *const beg, IndexRow const *const end)
{
    auto const otbl = out.table("RAW");
    auto const colGroup = otbl.column("READ_GROUP");
    auto const colName = otbl.column("NAME");
    auto const colSequence = otbl.column("SEQUENCE");
    auto const colReference = otbl.column("REFERENCE");
    auto const colStrand = otbl.column("STRAND");
    auto const colCigar = otbl.column("CIGAR");
    auto const colReadNo = otbl.column("READNO");
    auto const colPosition = otbl.column("POSITION");
    
    auto const range = in.rowRange();
    if (end - beg != range.second - range.first) {
        std::cerr << "index size doesn't match input table" << std::endl;
        return -1;
    }
    auto const freq = (range.second - range.first) / 100.0;
    auto nextReport = 1;
    uint64_t written = 0;
    size_t const bufferSize = 4ul * 1024ul * 1024ul * 1024ul;
    auto buffer = malloc(bufferSize);
    auto bufEnd = (void const *)((uint8_t const *)buffer + bufferSize);
    auto blockSize = 0;
    
    std::cerr << "info: processing " << (range.second - range.first) << " records" << std::endl;
    std::cerr << "info: record storage is " << bufferSize / 1024 / 1024 << "MB" << std::endl;
    for (auto i = beg; i != end; ) {
    AGAIN:
        auto j = i + (blockSize == 0 ? 1000000 : blockSize);
        if (j > end)
            j = end;
        auto m = std::map<int64_t, unsigned>();
        {
            auto v = std::vector<IndexRow>();
            
            v.reserve(j - i);
            if (j != end) {
                auto ii = i;
                while (ii < j) {
                    auto jj = ii;
                    do { ++jj; } while (jj < j && jj->key64() == ii->key64());
                    if (jj < j) {
                        v.insert(v.end(), ii, jj);
                        ii = jj;
                    }
                    else
                        break;
                }
                j = ii;
            }
            else {
                v.insert(v.end(), i, j);
            }
            std::sort(v.begin(), v.end(), IndexRow::rowLess);

            auto cp = buffer;
            unsigned count = 0;
            for (auto && r : v) {
                auto const row = r.row;
                auto const tmp = ((uint8_t const *)cp - (uint8_t const *)buffer) / 4;
                for (auto i = 0; i < 8; ++i) {
                    auto const data = in.read(row, i + 1);
                    auto p = data.copy(cp, bufEnd);
                    if (p == nullptr) {
                        std::cerr << "warn: filled buffer; reducing block size and restarting!" << std::endl;
                        blockSize = 0.99 * count;
                        std::cerr << "info: block size " << blockSize << std::endl;
                        goto AGAIN;
                    }
                    cp = p->end();
                }
                ++count;
                m[row] = (unsigned)tmp;
            }
            if (blockSize == 0) {
                auto const avg = double(((uint8_t *)cp - (uint8_t *)buffer)) / count;
                std::cerr << "info: average record size is " << size_t(avg + 0.5) << " bytes" << std::endl;
                blockSize = 0.95 * (double(((uint8_t *)bufEnd - (uint8_t *)buffer)) / avg);
                std::cerr << "info: block size " << blockSize << std::endl;
            }
        }
        for (auto k = i; k < j; ) {
            // the index is clustered by hash value, but there wasn't anything
            // in the index generation code to deal with hash collisions, that
            // is dealt with here, by reading all of the records with the same
            // hash value and then sorting by group and name (and reference and
            // position)
            auto v = std::vector<decltype(k->row)>();
            auto const key = k->key64();
            do { v.push_back(k->row); ++k; } while (k < j && k->key64() == key);
            
            std::sort(v.begin(), v.end(), [&](decltype(k->row) a, decltype(k->row) b) {
                auto adata = (VDB::Cursor::Data const *)((uint8_t const *)buffer + m[a] * 4);
                auto bdata = (VDB::Cursor::Data const *)((uint8_t const *)buffer + m[b] * 4);

                auto const agroup = adata->asString();
                auto const bgroup = bdata->asString();

                adata = adata->next();
                bdata = bdata->next();

                auto const aname = adata->asString();
                auto const bname = bdata->asString();
                
                auto const sameGroup = (agroup == bgroup);

                if (sameGroup && aname == bname)
                    return a < b; // it's expected that collisions are (exceedingly) rare

                return sameGroup ? (aname < bname) : (agroup < bgroup);
            });
            for (auto && row : v) {
                auto data = (VDB::Cursor::Data const *)((uint8_t const *)buffer + m[row] * 4);
                
                write(colGroup      , data);
                write(colName       , data = data->next());
                write(colSequence   , data = data->next());
                write(colReference  , data = data->next());
                write(colCigar      , data = data->next());
                write(colStrand     , data = data->next());
                write(colReadNo     , data = data->next());
                write(colPosition   , data = data->next());
                otbl.closeRow();
                
                ++written;
                if (nextReport * freq <= written) {
                    std::cerr << "prog: processed " << nextReport << "%" << std::endl;
                    ++nextReport;
                }
            }
        }
        i = j;
    }
    std::cerr << "prog: Done" << std::endl;
    
    return 0;
}

static ssize_t fsize(int const fd)
{
    struct stat stat = {0};
    if (fstat(fd, &stat) == 0)
        return stat.st_size;
    return -1;
}

static int process(std::string const &irdb, std::string const &indexFile, FILE *out)
{
    IndexRow const *index;
    size_t rows;
    {
        auto const fd = open(indexFile.c_str(), O_RDONLY);
        if (fd < 0) {
            perror("failed to open index file");
            exit(1);
        }
        auto const size = fsize(fd);
        auto const map = mmap(0, size, PROT_READ, MAP_FILE|MAP_PRIVATE, fd, 0);
        close(fd);
        if (map == MAP_FAILED) {
            perror("failed to mmap index file");
            exit(1);
        }
        index = (IndexRow *)map;
        rows = size / sizeof((*index));
    }
    auto writer = Writer2(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("reorder-ir", "1.0.0");

    writer.addTable("RAW", {
        { "READ_GROUP"  , 1 },  ///< string
        { "NAME"        , 1 },  ///< string
        { "SEQUENCE"    , 1 },  ///< string
        { "REFERENCE"   , 1 },  ///< string
        { "CIGAR"       , 1 },  ///< string
        { "STRAND"      , 1 },  ///< char
        { "READNO"      , 4 },  ///< int32_t
        { "POSITION"    , 4 },  ///< int32_t
    });
    writer.beginWriting();
    
    auto const mgr = VDB::Manager();
    auto const in = mgr[irdb]["RAW"].read({ "READ_GROUP", "NAME", "SEQUENCE", "REFERENCE", "CIGAR", "STRAND", "READNO", "POSITION" });
    auto const result = process(writer, in, index, index + rows);
    
    writer.endWriting();
    
    return result;
}

namespace reorderIR {
    static void usage(std::string const &program, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << program << "[-out=<path>]  <ir db> <clustering index>" << std::endl;
        exit(error ? 3 : 0);
    }
    
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine.program, false);
            }
        }
        auto db = std::string();
        auto ndx = std::string();
        auto out = std::string();
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                out = arg.substr(5);
                continue;
            }
            if (db.empty()) {
                db = arg;
                continue;
            }
            if (ndx.empty()) {
                ndx = arg;
                continue;
            }
            usage(commandLine.program, true);
        }
        if (db.empty() || ndx.empty())
            usage(commandLine.program, true);

        if (out.empty())
            return process(db, ndx, stdout);
        
        auto ofs = fopen(out.c_str(), "w");
        if (ofs)
            return process(db, ndx, ofs);
        
        std::cerr << "failed to open output file: " << out << std::endl;
        exit(3);
    }
}

int main(int argc, char *argv[])
{
    return reorderIR::main(CommandLine(argc, argv));
}
