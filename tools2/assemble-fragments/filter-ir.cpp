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
#include "utility.hpp"
#include "vdb.hpp"
#include "writer.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "fragment.hpp"

void write(VDB::Writer const &out, unsigned const table, Fragment const &self, char const *const reason, bool const filter, bool const quality)
{
    auto const groupCID     = 1 + (table - 1) * 8;
    auto const nameCID      = groupCID + 1;
    auto const readNoCID    = nameCID + 1;
    auto const sequenceCID  = readNoCID + 1;
    auto const referenceCID = sequenceCID + 1;
    auto const strandCID    = referenceCID + 1;
    auto const positionCID  = strandCID + 1;
    auto const cigarCID     = positionCID + 1;
    auto const reasonCID    = (reason && table == 2) ? 17 : 0;
    auto const qualityCID   = (quality && table == 1) ? 18 : (quality && table == 2) ? 19 : 0;

    for (auto && i : self.detail) {
        if (filter && !i.aligned) continue;
        out.value(groupCID, self.group);
        out.value(nameCID, self.name);
        out.value(readNoCID, int32_t(i.readNo));
        out.value(sequenceCID, std::string(i.sequence));
        if (i.aligned) {
            out.value(referenceCID, i.reference);
            out.value(strandCID, i.strand);
            out.value(positionCID, int32_t(i.position));
            out.value(cigarCID, i.cigarString);
        }
        if (reasonCID)
            out.value(reasonCID, std::string(reason));
        if (qualityCID)
            out.value(qualityCID, i.quality);
        out.closeRow(table);
    }
}

static bool shouldKeep(Fragment const &fragment, char const **const reason)
{
    /* a spot is not kept if:
     *  any read has an invalid CIGAR
     *  any read has no alignment
     *  any read has more than one alignment and the sequences don't all match
     */

    // check for invalid CIGAR strings
    for (auto && i : fragment.detail) {
        if (i.aligned && (i.cigar.size() == 0 || i.cigar.qlength != i.sequence.length())) {
            *reason = "invalid CIGAR string";
            return false;
        }
    }
    
    auto const n = int(fragment.detail.size());
    auto next = 0;
    auto numreads = 0;
    while (next < n) {
        auto const first = next;
        auto const readNo = fragment.detail[first].readNo;

        ++numreads;
        while (next < n && fragment.detail[next].readNo == readNo)
            ++next;
        
        auto aligned = 0;
        for (auto i = first; i < next; ++i) {
            if (fragment.detail[i].aligned)
                ++aligned;
        }
        if (aligned == 0) {
            *reason = "partially aligned";
            return false;
        }

        for (auto i = first; i < next - 1; ++i) {
            if (!fragment.detail[i].aligned) continue;
            
            for (auto j = i + 1; j < next; ++j) {
                if (!fragment.detail[j].aligned) continue;
                
                if (!fragment.detail[i].sequenceEquivalentTo(fragment.detail[j])) {
                    *reason = "non-equivalent sequences";
                    return false;
                }
            }
        }
    }
    
    if (numreads == 2) {
        auto goodPairs = 0;
        for (auto && one : fragment.detail) {
            if (one.readNo != 1 || !one.aligned) continue;
            for (auto && two : fragment.detail) {
                if (two.readNo != 2 || !two.aligned) continue;
                if (one.isGoodAlignedPair(two))
                    ++goodPairs;
            }
        }
        if (goodPairs == 0) {
            *reason = "no good whole fragment alignment";
            return false;
        }
    }
    return true;
}

static void process(VDB::Writer const &out, Fragment const &fragment, bool const quality)
{
    char const *reason = nullptr;
    if (shouldKeep(fragment, &reason))
        write(out, 1, fragment, nullptr, true, quality);
    else
        write(out, 2, fragment, reason, false, quality);
}

static int process(VDB::Writer const &out, Fragment::Cursor const &in, bool const quality)
{
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 10.0;
    auto nextReport = 1;
    
    std::cerr << "info: processing " << (range.second - range.first) << " records" << std::endl;
    for (auto row = range.first; row < range.second; ) {
        auto spot = in.read(row, range.second);
        if (spot.detail.empty())
            continue;
        std::sort(spot.detail.begin(), spot.detail.end());
        process(out, spot, quality);
        while (nextReport * freq <= (row - range.first)) {
            std::cerr << "prog: processed " << nextReport << "0%" << std::endl;
            ++nextReport;
        }
    }
    std::cerr << "prog: Done" << std::endl;

    return 0;
}

static int process(std::string const &irdb, FILE *out)
{
    auto const mgr = VDB::Manager();
    VDB::Database const &inDb = mgr[irdb];
    auto const in = Fragment::Cursor(inDb["RAW"]);
    auto const writer = VDB::Writer(out);
    auto const quality = in.hasQuality();

    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("reorder-ir", "1.0.0");
    
    writer.openTable(1, "RAW");
    writer.openColumn(1, 1, 8, "READ_GROUP");
    writer.openColumn(2, 1, 8, "NAME");
    writer.openColumn(3, 1, 32, "READNO");
    writer.openColumn(4, 1, 8, "SEQUENCE");
    writer.openColumn(5, 1, 8, "REFERENCE");
    writer.openColumn(6, 1, 8, "STRAND");
    writer.openColumn(7, 1, 32, "POSITION");
    writer.openColumn(8, 1, 8, "CIGAR");
    
    writer.openTable(2, "DISCARDED");
    writer.openColumn(1 + 8, 2, 8, "READ_GROUP");
    writer.openColumn(2 + 8, 2, 8, "NAME");
    writer.openColumn(3 + 8, 2, 32, "READNO");
    writer.openColumn(4 + 8, 2, 8, "SEQUENCE");
    writer.openColumn(5 + 8, 2, 8, "REFERENCE");
    writer.openColumn(6 + 8, 2, 8, "STRAND");
    writer.openColumn(7 + 8, 2, 32, "POSITION");
    writer.openColumn(8 + 8, 2, 8, "CIGAR");
    writer.openColumn(9 + 8, 2, 8, "REJECT_REASON");

    if (quality) {
        writer.openColumn(18, 1, 8, "QUALITY");
        writer.openColumn(19, 2, 8, "QUALITY");
    }
    writer.beginWriting();

    writer.defaultValue<char>(5, 0, 0);
    writer.defaultValue<char>(6, 0, 0);
    writer.defaultValue<int32_t>(7, 0, 0);
    writer.defaultValue<char>(8, 0, 0);
    
    writer.defaultValue<char>(5 + 8, 0, 0);
    writer.defaultValue<char>(6 + 8, 0, 0);
    writer.defaultValue<int32_t>(7 + 8, 0, 0);
    writer.defaultValue<char>(8 + 8, 0, 0);
    writer.defaultValue<char>(9 + 8, 0, 0);

    auto const result = process(writer, in, quality);
    
    writer.endWriting();
    
    return result;
}

using namespace utility;
namespace filterIR {
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << commandLine.program[0] << " [-out=<path>] <ir db>" << std::endl;
        exit(error ? 3 : 0);
    }
    
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine, false);
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
            usage(commandLine, true);
        }
        if (run.empty()) {
            usage(commandLine, true);
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
    Alignment::test();
    return filterIR::main(CommandLine(argc, argv));
}

