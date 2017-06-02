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

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

static float N_threshold = -1.0;
static float N_padding = 1.0;

template <typename T>
static std::ostream &write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::RawData const &in)
{
    return out.value(cid, in.elements, (T const *)in.data);
}

template <typename T>
static std::ostream &write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::Data const *in)
{
    return out.value(cid, in->elements, (T const *)in->data());
}

static std::string reverseComplement(std::string const &seq)
{
    auto rslt = std::string(seq.rbegin(), seq.rend());
    for (auto && base : rslt) {
        switch (base) {
            case 'A':
                base = 'T';
                break;
            case 'C':
                base = 'G';
                break;
            case 'G':
                base = 'C';
                break;
            case 'T':
                base = 'A';
                break;
            default:
                base = 'N';
                break;
        }
    }
    return rslt;
}

static bool isAmbiguity(char const b) {
    return b != 'A' && b != 'C' && b != 'G' && b != 'T';
}

static unsigned countAmbiguity(std::string const &sequence)
{
    unsigned n = 0;
    for (auto && base : sequence) {
        if (isAmbiguity(base))
            ++n;
    }
    return n;
}

static unsigned leftSoftClip(std::string const &cigar)
{
    auto const str = cigar.data();
    auto const endp = str + cigar.length();
    unsigned len = 0;
    auto op = str;
    
    while (op < endp && *op >= '0' && *op <= '9') {
        len = len * 10 + *op - '0';
        ++op;
    }
    return (op < endp && *op == 'S') ? len : 0;
}

static unsigned rightSoftClip(std::string const &cigar)
{
    auto const str = cigar.data();
    auto const endp = str + cigar.length();
    auto op = endp - 1;
    unsigned len = 0;
    
    while (--op >= str && *op >= '0' && *op <= '9')
        ;
    if (op++ == str)
        return 0;
    while (op < endp && *op >= '0' && *op <= '9') {
        len = len * 10 + *op - '0';
        ++op;
    }
    return (op < endp && *op == 'S') ? len : 0;
}

struct Alignment {
    std::string sequence;
    std::string reference;
    std::string cigar;
    int readNo;
    int position;
    char strand;
    bool aligned;
    unsigned ambiguous;
    unsigned leftClip;
    unsigned rightClip;
    
    Alignment(int readNo, std::string const &sequence)
    : readNo(readNo)
    , sequence(sequence)
    , aligned(false)
    , ambiguous(countAmbiguity(sequence))
    , reference("")
    , strand(0)
    , position(0)
    , cigar("")
    , leftClip(0)
    , rightClip(0)
    {}

    Alignment(int readNo, std::string const &sequence, std::string const &reference, char strand, int position, std::string const &cigar)
    : readNo(readNo)
    , sequence(sequence)
    , aligned(true)
    , ambiguous(countAmbiguity(sequence))
    , reference(reference)
    , strand(strand)
    , position(position)
    , cigar(cigar)
    , leftClip(leftSoftClip(cigar))
    , rightClip(rightSoftClip(cigar))
    {}

    friend bool operator <(Alignment const &a, Alignment const &b) {
        if (a.readNo < b.readNo)
            return true;
        if (!a.aligned && b.aligned)
            return true;
        if (a.aligned && b.aligned)
            return a.position < b.position;
        return false;
    }
};

struct Fragment {
    std::string group;
    std::string name;
    std::vector<Alignment> detail;
    unsigned reads;
    unsigned firstRead;
    unsigned lastRead;
    unsigned aligned;
    unsigned ambiguous;
    
    static Fragment read(VDB::Cursor const &in, int64_t const start, int64_t const endRow)
    {
        auto rslt = std::vector<Alignment>();
        auto spotGroup = std::string();
        auto spotName = std::string();
        
        while (start + int64_t(rslt.size()) < endRow) {
            auto const row = start + int64_t(rslt.size());
            auto const sg = in.read(row, 1).asString();
            auto const name = in.read(row, 2).asString();
            if (rslt.size() > 0) {
                if (name != spotName || sg != spotGroup)
                    break;
            }
            auto const readNo = in.read(row, 3).value<int32_t>();
            auto const sequence = in.read(row, 4).asString();
            if (in.read(row, 7).elements > 0) {
                auto const cigar = in.read(row, 8).asString();
                auto const algn = Alignment(readNo, sequence, in.read(row, 5).asString(), in.read(row, 6).value<char>(), in.read(row, 7).value<int32_t>(), cigar);
                rslt.push_back(algn);
            }
            else {
                auto const algn = Alignment(readNo, sequence);
                rslt.push_back(algn);
            }
        }
        if (rslt.size() < 2)
            return Fragment(spotGroup, spotName, rslt);
        
        std::sort(rslt.begin(), rslt.end());
        if (rslt.size() == 2 && rslt[0].readNo == 1 && rslt[1].readNo == 2)
            return Fragment(spotGroup, spotName, rslt);
        
        auto next = 0;
        while (next < rslt.size()) {
            auto const first = next;
            while (next < rslt.size() && rslt[next].readNo == rslt[first].readNo)
                ++next;

            auto const count = next - first;
            if (count == 1)
                continue;

            auto good = -1;
            for (auto i = first; i < next; ++i) {
                if (rslt[i].aligned && rslt[i].ambiguous == 0) {
                    good = i;
                    break;
                }
            }
            if (good >= first) {
                for (auto i = first; i < next; ++i) {
                    if (i == good) continue;
                    if (!rslt[i].aligned) {
                        rslt.erase(rslt.begin() + i);
                        --i;
                        --next;
                        continue;
                    }
                    if (rslt[i].ambiguous) {
                        rslt[i].sequence = "";
                        rslt[i].ambiguous = false;
                    }
                }
            }
        }
        
        return Fragment(spotGroup, spotName, rslt);
    }
    void write(VDB::Writer const &out, unsigned const table) const {
        for (auto && i : detail) {
            if (table == 1 && !i.aligned) continue;
            out.value(1 + (table - 1) * 8, group);
            out.value(2 + (table - 1) * 8, name);
            out.value(3 + (table - 1) * 8, int32_t(i.readNo));
            out.value(4 + (table - 1) * 8, i.sequence);
            if (i.aligned) {
                out.value(5 + (table - 1) * 8, i.reference);
                out.value(6 + (table - 1) * 8, i.strand);
                out.value(7 + (table - 1) * 8, int32_t(i.position));
                out.value(8 + (table - 1) * 8, i.cigar);
            }
        }
    }
    static bool isAligned(Fragment const &f) { return f.aligned; }
    
    static VDB::Cursor cursor(VDB::Table const &tbl) {
        static char const *const FLDS[] = { "READ_GROUP", "FRAGMENT", "READNO", "SEQUENCE", "REFERENCE", "STRAND", "POSITION", "CIGAR" };
        return tbl.read(8, FLDS);
    }

    Fragment(std::string const &group, std::string const &name, std::vector<Alignment> const &algn)
    : group(group)
    , name(name)
    , detail(algn)
    {
        auto r = 0;
        auto f = 0;
        auto l = 0;
        auto a = 0;
        auto N = 0;
        for (auto && i : detail) {
            if (i.aligned)
                ++a;
            if (i.ambiguous)
                ++N;
            if (r == 0 || i.readNo != l) {
                l = i.readNo;
                if (r == 0)
                    f = i.readNo;
                ++r;
            }
        }
        reads = r;
        firstRead = f;
        lastRead = l;
        aligned = a;
        ambiguous = N;
    }
};

static void process(VDB::Writer const &out, Fragment const &fragment)
{
    // not paired-end fragment, or missing mates, or missing pairing info
    if (fragment.reads != 2 || fragment.firstRead != 1 || fragment.lastRead != 2)
        goto DISCARD;

    // paired-end fragment with Read 1 and Read 2, maybe some N's, maybe some unaligned or extraneous records
    
    // there are no secondary or extraneous records to deal with, but some reads might be unaligned or have N's
    if (fragment.detail.size() == 2) {
        if (fragment.aligned == 2)
            goto KEEP;
        else
            goto DISCARD;
    }

    {
        auto next = 0;
        while (next < fragment.detail.size()) {
            auto const first = next;
            while (next < fragment.detail.size() && fragment.detail[next].readNo == fragment.detail[first].readNo)
                ++next;
            
            auto const count = next - first;
            if (count == 1)
                continue;
            
            // check for reads with non-matching sequences
            for (auto i = first; i < next; ++i) {
                if (fragment.detail[i].sequence.length() != 0) {
                    auto const seq = fragment.detail[i].sequence;
                    auto const rseq = reverseComplement(seq);
                    for (auto j = first; j < next; ++j) {
                        if (j == i) continue;
                        auto const &testSeq = fragment.detail[j].sequence;
                        if (testSeq == seq || testSeq == rseq)
                            continue;
                        goto DISCARD;
                    }
                }
            }
            
            // check for reads with no alignments
            auto aligned = 0;
            for (auto i = first; i < next; ++i) {
                if (fragment.detail[i].aligned)
                    ++aligned;
            }
            if (aligned == 0)
                goto DISCARD;
        }
    }
KEEP:
    fragment.write(out, 1);
    return;

DISCARD:
    fragment.write(out, 2);
    return;
}

static int process(VDB::Writer const &out, VDB::Database const &inDb)
{
    auto const in = Fragment::cursor(inDb["RAW"]);
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 100.0;
    auto nextReport = 1;
    uint64_t written = 0;
    
    std::cerr << "info: processing " << (range.second - range.first) << " records" << std::endl;
    for (auto row = range.first; ; ) {
        auto const spot = Fragment::read(in, row, range.second);
        if (spot.detail.empty())
            break;
        row += spot.detail.size();
        process(out, spot);
    }
    std::cerr << "Done" << std::endl;

    return 0;
}

static int process(char const *const irdb)
{
#if 0
    auto const writer = VDB::Writer(std::cout);
#else
    auto devNull = std::ofstream("/dev/null");
    auto const writer = VDB::Writer(devNull);
#endif
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("reorder-ir", "1.0.0");
    
    writer.openTable(1, "RAW");
    writer.openColumn(1, 1, 8, "READ_GROUP");
    writer.openColumn(2, 1, 8, "FRAGMENT");
    writer.openColumn(3, 1, 32, "READNO");
    writer.openColumn(4, 1, 8, "SEQUENCE");
    writer.openColumn(5, 1, 8, "REFERENCE");
    writer.openColumn(6, 1, 8, "STRAND");
    writer.openColumn(7, 1, 32, "POSITION");
    writer.openColumn(8, 1, 8, "CIGAR");
    
    writer.openTable(2, "DISCARDED");
    writer.openColumn(1 + 8, 2, 8, "READ_GROUP");
    writer.openColumn(2 + 8, 2, 8, "FRAGMENT");
    writer.openColumn(3 + 8, 2, 32, "READNO");
    writer.openColumn(4 + 8, 2, 8, "SEQUENCE");
    writer.openColumn(5 + 8, 2, 8, "REFERENCE");
    writer.openColumn(6 + 8, 2, 8, "STRAND");
    writer.openColumn(7 + 8, 2, 32, "POSITION");
    writer.openColumn(8 + 8, 2, 8, "CIGAR");
    
    writer.beginWriting();

    writer.defaultValue<char>(5, 0, 0);
    writer.defaultValue<char>(6, 0, 0);
    writer.defaultValue<int32_t>(7, 0, 0);
    writer.defaultValue<char>(8, 0, 0);
    
    writer.defaultValue<char>(5 + 8, 0, 0);
    writer.defaultValue<char>(6 + 8, 0, 0);
    writer.defaultValue<int32_t>(7 + 8, 0, 0);
    writer.defaultValue<char>(8 + 8, 0, 0);
    
    auto const mgr = VDB::Manager();
    auto const result = process(writer, mgr[irdb]);
    
    writer.endWriting();
    
    return result;
}

int main(int argc, char *argv[])
{
    if (argc == 3)
        return process(argv[1]);
    else {
        std::cerr << "usage: reorder-ir <ir db> <clustering index>" << std::endl;
        return 1;
    }
}
