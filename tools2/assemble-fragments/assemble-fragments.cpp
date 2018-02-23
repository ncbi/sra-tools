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
#include "utility.hpp"
#include "vdb.hpp"
#include "writer.hpp"
#include "fragment.hpp"

using namespace utility;

static strings_map references;
static strings_map groups = {""};

struct Mapulet {
    std::string name;
    unsigned id;

    unsigned ref1;
    int start1, end1;

    int gap;

    unsigned ref2;
    int start2, end2;
    
    int count;
};

static std::vector<Mapulet> mapulets = {{ "invalid" }};

static unsigned createMapulet(unsigned ref1, int start1, int end1, unsigned ref2, int start2, int end2)
{
    std::string name = references[ref1] + ":" + std::to_string(start1 + 1) + "-" + std::to_string(end1)
               + "+" + references[ref2] + ":" + std::to_string(start2 + 1) + "-" + std::to_string(end2);
    Mapulet o = { name, unsigned(mapulets.size()), ref1, start1, end1, 1, ref2, start2, end2 };
    mapulets.push_back(o);
    return o.id;
}

struct ContigStats {
    struct stat {
        int64_t sourceRow;
        unsigned window;
        unsigned count;

        float average;
        float std_dev;
        
        unsigned group;
        unsigned ref1, ref2;
        int start1, start2;
        int end1, end2;

        unsigned mapulet;

        mutable StatisticsAccumulator length, rlength1, rlength2, qlength1, qlength2;
        
        bool isGapless() const {
            return end1 == 0 && start2 == 0;
        }
        
        ///* these functions below are ONLY valid after statistics have been gathered
        double pileUpDepth(int readNo) const {
            auto const n = length.count();
            auto const start = readNo == 1 ? start1 : start2;
            auto const end = readNo == 1 ? end1 : end2;
            auto const avglen = readNo == 1 ? qlength1.average() : qlength2.average();
            auto const slope = n / (end - start - avglen);
            return slope * avglen;
        }
        double coverage() const {
            if (isGapless()) {
                auto const n = length.count();
                auto const start = start1;
                auto const end = end2;
                auto const avglen = length.average();
                auto const slope = n / (end - start - avglen);
                auto const fcov = slope * avglen;
                auto const conv = (qlength1.average() + qlength2.average()) / avglen;
                return fcov * conv;
            }
            return (pileUpDepth(1) + pileUpDepth(2)) / 2.0;
        }
    };
    std::vector<stat> stats;
    std::vector<decltype(stats.size())> refIndex;

    std::vector<stat>::const_iterator end() const { return stats.end(); }
    std::vector<stat>::const_iterator find_start_of(unsigned ref, int start) const {
        auto f = refIndex[ref];
        auto e = refIndex[ref + 1];
        while (f < e) {
            auto const m = f + ((e - f) >> 1);
            auto const &fnd = stats[m];
            if (fnd.start1 < start)
                f = m + 1;
            else
                e = m;
        }
        if (f >= refIndex[ref + 1])
            f = refIndex[ref + 1] - 1;
        else
            f -= stats[f].window;
        return stats.begin() + f;
    }
    void generateRefIndex() {
        int lastRef = -1;
        refIndex.clear();
        for (auto i = 0; i < stats.size(); ++i) {
            if (stats[i].ref1 != lastRef) {
                lastRef = stats[i].ref1;
                refIndex.push_back(i);
            }
        }
        refIndex.push_back(stats.size());
    }
    void generateWindow() {
        for (auto i = stats.begin(); i != stats.end(); ++i) {
            i->window = 0;
        }
        for (auto i = stats.begin(); i != stats.end(); ++i) {
            auto const end = i->isGapless() ? i->end2 : i->end1;
            unsigned window = 0;
            for (auto j = i; j != stats.end() && j->ref1 == i->ref1; ++j, ++window) {
                j->window = std::max(j->window, window);
                if (j->start1 > end)
                    break;
            }
        }
    }

    std::map<unsigned, StatisticsAccumulator> groupMedians() const {
        std::map<unsigned, StatisticsAccumulator> rslt;

        auto v = std::vector<unsigned>();
        for (auto i = stats.begin(); i != stats.end(); ++i) {
            if (i->ref2 == i->ref1)
                v.push_back((unsigned)(i - stats.begin()));
        }
        std::sort(v.begin(), v.end(), [this](decltype(v)::value_type a, decltype(v)::value_type b)
                  {
                      auto const &A = stats[a];
                      auto const &B = stats[b];
                      if (A.group < B.group) return true;
                      if (B.group < A.group) return false;
                      return A.length.average() < B.length.average();
                  });
        for (auto i = v.begin(); i != v.end(); ) {
            auto const group = stats[*i].group;
            auto j = i;

            double sum = 0.0;
            do {
                auto const &ii = stats[*i++];
                if (ii.mapulet == 0 && ii.ref1 == ii.ref2)
                    sum += ii.length.count();
            } while (i != v.end() && group == stats[*i].group);

            auto const limit = sum / 2.0;
            double accum = 0.0;
            for (auto k = j; k != i; ++k) {
                auto const &kk = stats[*k];
                if (kk.mapulet == 0 && kk.ref1 == kk.ref2) {
                    accum += kk.length.count();
                    if (accum >= limit) {
                        rslt[group] = kk.length;
                        break;
                    }
                }
            }
        }
        return rslt;
    }

    bool cleanup() {
        auto changed = false;
        std::vector<stat> clean;
        clean.reserve(stats.size());
        for (auto && i : stats) {
            if (i.length.count() < 5.0 || i.coverage() < 5.0) continue;
            
            stat o = i;
            o.sourceRow = clean.size() + 1;
            
            // copy over new stats
            changed |= o.count != i.length.count();
            o.count = i.length.count();
            o.average = i.length.average();
            o.std_dev = sqrt(i.length.variance());

            // reset stats
            o.length = StatisticsAccumulator();
            o.rlength1 = StatisticsAccumulator();
            o.rlength2 = StatisticsAccumulator();
            o.qlength1 = StatisticsAccumulator();
            o.qlength2 = StatisticsAccumulator();

            clean.emplace_back(o);
        }
        changed |= stats.size() != clean.size();
        stats.swap(clean);
        generateWindow();
        generateRefIndex();
        return changed;
    }

    void adjustMapulets() {
        auto median = groupMedians();
        for (auto && i : stats) {
            if (i.mapulet) {
                mapulets[i.mapulet].count = i.length.count();
                if (mapulets[i.mapulet].count == 0) continue;
                
                auto const length = i.length.average();
                auto const median_length = median[i.group].average();
                // average length + gap length = median length (roughly)
                auto const gap = median_length < length ? -ceil(length - median_length) : ceil(median_length - length);
                mapulets[i.mapulet].gap = gap;
            }
        }
    }
    
    static ContigStats load(VDB::Database const &db) {
        ContigStats result;
        auto const tbl = db["CONTIGS"];
        auto const curs = tbl.read({ "REFERENCE_1", "START_1", "END_1", "REFERENCE_2", "START_2", "END_2", "READ_GROUP", "COUNT" });
        auto const range = curs.rowRange();
        
        result.stats.reserve(range.second - range.first);
        for (auto row = range.first; row < range.second; ++row) {
            auto count = curs.read(row, 8).value<uint32_t>();
            
            stat o = { row, 0, count };
            
            o.ref1 = references[curs.read(row, 1).asString()];
            o.start1 = curs.read(row, 2).value<int32_t>();
            o.end1 = curs.read(row, 3).value<int32_t>();
            
            o.ref2 = references[curs.read(row, 4).asString()];
            o.start2 = curs.read(row, 5).value<int32_t>();
            o.end2 = curs.read(row, 6).value<int32_t>();
            
            if (o.ref2 == o.ref1 && o.start2 < o.end1)
                o.start2 = o.end1 = 0;
            else if (o.end1 != 0 && o.start2 != 0)
                o.mapulet = createMapulet(o.ref1, o.start1, o.end1, o.ref2, o.start2, o.end2);

            o.group = groups[curs.read(row, 7).asString()];
            
            result.stats.emplace_back(o);
        }
        result.generateRefIndex();
        result.generateWindow();
        return result;
    }
};

struct BestAlignment {
    decltype(ContigStats::stats)::const_iterator contig;
    decltype(Fragment::detail)::const_iterator a, b;
    int pos1, pos2;
    int fragmentLength;
    
    bool isBetterThan(BestAlignment const &other) const
    {
        auto const &a = *this;
        auto const &b = other;
        auto const aIsSameRef = a.contig->ref1 == a.contig->ref2;
        auto const bIsSameRef = b.contig->ref1 == b.contig->ref2;
        
        auto aScore = 0;
        auto bScore = 0;
        
        auto const a_qlength = a.a->cigar.qlength + a.b->cigar.qlength
                             - (a.a->cigar.qclip + a.a->cigar.qfirst + a.b->cigar.qclip + a.b->cigar.qfirst);
        auto const b_qlength = b.a->cigar.qlength + b.b->cigar.qlength
                             - (b.a->cigar.qclip + b.a->cigar.qfirst + b.b->cigar.qclip + b.b->cigar.qfirst);

        if (a.contig->isGapless())              aScore += 8;
        if (aIsSameRef)                         aScore += 4;
        if (a.contig->count > b.contig->count)  aScore += 2;
        if (a_qlength > b_qlength)              aScore += 1;

        if (b.contig->isGapless())              bScore += 8;
        if (bIsSameRef)                         bScore += 4;
        if (b.contig->count > a.contig->count)  bScore += 2;
        if (b_qlength > a_qlength)              bScore += 1;

        if (aIsSameRef && bIsSameRef && a.contig->std_dev > 0 && b.contig->std_dev > 0) {
            auto const aStDev = a.contig->std_dev;
            auto const bStDev = b.contig->std_dev;
            auto const aAvg = a.contig->average;
            auto const bAvg = b.contig->average;
            auto const aT = std::abs(a.fragmentLength - aAvg) / aStDev;
            auto const bT = std::abs(b.fragmentLength - bAvg) / bStDev;
            
            if (aT < bT) aScore += 16;
            if (bT < aT) bScore += 16;
        }
        return aScore > bScore;
    }
};

std::vector<BestAlignment> candidateFragmentAlignments(Fragment const &fragment, ContigStats const &contigs, int const loopNumber)
{
    struct alignment {
        decltype(fragment.detail.cbegin()) mom;
        int pos, end;
        decltype(references[fragment.detail[0].reference]) ref;
        
        alignment(decltype(fragment.detail.cbegin()) mom, decltype(ref) ref) : mom(mom), ref(ref) {
            pos = mom->position - mom->cigar.qfirst;
            end = mom->position + mom->cigar.rlength + mom->cigar.qclip;
        }
    };
    
    std::vector<BestAlignment> rslt;
    auto const &alignments = fragment.detail;
    int r1 = 0, r2 = 0;
    
    decltype(groups[fragment.group]) group;
    if (!groups.contains(fragment.group, group))
        return rslt;

    for (auto one = alignments.begin(); one < alignments.end(); ++one) {
        decltype(references[one->reference]) ref1;
        
        if (one->readNo != 1 || one->aligned == false) continue;
        ++r1;
        if (!references.contains(one->reference, ref1)) continue;
        
        auto const a1 = alignment(one, ref1);
        
        for (auto two = alignments.begin(); two < alignments.end(); ++two) {
            decltype(references[one->reference]) ref2;
            
            if (two->readNo != 2 || two->aligned == false) continue;
            ++r2;
            if (!references.contains(two->reference, ref2)) continue;
            
            auto const a2 = alignment(two, ref2);
            auto const pair = (two->reference < one->reference || (two->reference == one->reference && a2.pos < a1.pos)) ? std::make_pair(a2, a1) : std::make_pair(a1, a2);
            auto i = contigs.find_start_of(pair.first.ref, pair.first.pos);
            while (i != contigs.end() && i->ref1 == pair.first.ref && i->start1 <= pair.first.pos) {
                if (i->group == group && i->ref2 == pair.second.ref && pair.second.end <= i->end2 && (i->isGapless() || i->start2 <= pair.second.pos))
                {
                    BestAlignment const best = {
                        i, pair.first.mom, pair.second.mom,
                        pair.first.pos, pair.second.pos,
                        pair.first.ref == pair.second.ref ? pair.second.end - pair.first.pos : 0
                    };
                    rslt.push_back(best);
                }
                ++i;
            }
        }
    }
    if (rslt.size() == 0 && r1 > 0 && r2 > 0 && loopNumber == 1) {
        throw std::logic_error("no fully-aligned spots should be dropped on the first pass!");
    }
    return rslt;
}

static BestAlignment bestPair(Fragment const &fragment, ContigStats const &contigs, int const loopNumber)
{
    BestAlignment result = { contigs.end(), fragment.detail.end(), fragment.detail.end(), 0 };
    std::vector<BestAlignment> all = candidateFragmentAlignments(fragment, contigs, loopNumber);

    std::sort(all.begin(), all.end(), [](BestAlignment const &a, BestAlignment const &b) { return a.isBetterThan(b); });
    if (!all.empty())
        result = all.front();
    return result;
}

static void writeReferences(Writer2 const &out, strings_map const &realRefs, std::vector<Mapulet> const &virtualRefs)
{
    auto const table = out.table("REFERENCES");
    auto const NAME = table.column("NAME");
    auto const REF1 = table.column("REFERENCE_1");
    auto const STR1 = table.column("START_1");
    auto const END1 = table.column("END_1");
    auto const GAP  = table.column("GAP");
    auto const REF2 = table.column("REFERENCE_2");
    auto const STR2 = table.column("START_2");
    auto const END2 = table.column("END_2");
    
    REF1.setDefaultEmpty();
    STR1.setDefaultEmpty();
    END1.setDefaultEmpty();
    GAP.setDefaultEmpty();
    REF2.setDefaultEmpty();
    STR2.setDefaultEmpty();
    END2.setDefaultEmpty();
    
    for (auto i = 0; i < realRefs.count(); ++i) {
        NAME.setValue(realRefs[i]);
        table.closeRow();
    }

    for (auto && i : virtualRefs) {
        if (i.id == 0) continue; // id 0 is the bogus one
        if (i.count == 0) continue; // skip unused one
        NAME.setValue(i.name);
        REF1.setValue(references[i.ref1]);
        STR1.setValue(i.start1);
        END1.setValue(i.end1);
        REF2.setValue(references[i.ref2]);
        STR2.setValue(i.start2);
        END2.setValue(i.end2);
        GAP.setValue(i.gap);
        table.closeRow();
    }
}

static void writeContigs(Writer2 const &out, std::vector<ContigStats::stat> const &contigs)
{
    auto const table = out.table("CONTIGS");
    auto const REF = table.column("REFERENCE");
    auto const STR = table.column("START");
    auto const END = table.column("END");
    auto const SEQ_LENGTH_AVERAGE = table.column("SEQ_LENGTH_AVERAGE");
    auto const SEQ_LENGTH_STD_DEV = table.column("SEQ_LENGTH_STD_DEV");
    auto const REF_LENGTH_AVERAGE = table.column("REF_LENGTH_AVERAGE");
    auto const REF_LENGTH_STD_DEV = table.column("REF_LENGTH_STD_DEV");
    auto const COUNT = table.column("FRAGMENT_COUNT");
    auto const AVERAGE = table.column("FRAGMENT_LENGTH_AVERAGE");
    auto const STD_DEV = table.column("FRAGMENT_LENGTH_STD_DEV");
    auto const GROUP = table.column("READ_GROUP");

    float qlength_average[2];
    float qlength_std_dev[2];
    float rlength_average[2];
    float rlength_std_dev[2];
    
    for (auto && i : contigs) {
        if (i.length.count() == 0) continue;
        
        qlength_average[0] = i.qlength1.average();
        qlength_std_dev[0] = sqrt(i.qlength1.variance());
        qlength_average[1] = i.qlength2.average();
        qlength_std_dev[1] = sqrt(i.qlength2.variance());

        rlength_average[0] = i.rlength1.average();
        rlength_std_dev[0] = sqrt(i.rlength1.variance());
        rlength_average[1] = i.rlength2.average();
        rlength_std_dev[1] = sqrt(i.rlength2.variance());

        SEQ_LENGTH_AVERAGE.setValue(2, qlength_average);
        SEQ_LENGTH_STD_DEV.setValue(2, qlength_std_dev);

        REF_LENGTH_AVERAGE.setValue(2, rlength_average);
        REF_LENGTH_STD_DEV.setValue(2, rlength_std_dev);
        
        COUNT.setValue(uint32_t(i.length.count()));
        STD_DEV.setValue(float(sqrt(i.length.variance())));
        GROUP.setValue(groups[i.group]);

        if (i.mapulet == 0) {
            REF.setValue(references[i.ref1]);
            STR.setValue(i.start1);
            END.setValue(i.end2);
            AVERAGE.setValue(float(i.length.average()));
        }
        else {
            auto const &mapulet = mapulets[i.mapulet];
            REF.setValue(mapulet.name);
            STR.setValue(0);
            END.setValue((mapulet.end1 - mapulet.start1) + mapulet.gap + (mapulet.end2 - mapulet.start2));
            AVERAGE.setValue(float(i.length.average() + mapulet.gap));
        }
        table.closeRow();
    }
}

static int assemble(FILE *out, std::string const &data_run, std::string const &stats_run)
{
    auto writer = Writer2(out);
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:aligned");
    writer.info("assemble-fragments", "1.0.0");

    writer.addTable(
                    "REFERENCES", {
                        { "NAME", sizeof(char) },
                        { "REFERENCE_1", sizeof(char) },
                        { "START_1", sizeof(int32_t) },
                        { "END_1", sizeof(int32_t) },
                        { "GAP", sizeof(int32_t) },
                        { "REFERENCE_2", sizeof(char) },
                        { "START_2", sizeof(int32_t) },
                        { "END_2", sizeof(int32_t) },
                    });
    writer.addTable(
                    "FRAGMENTS", {
                        { "READ_GROUP", sizeof(char) },
                        { "NAME", sizeof(char) },
                        { "REFERENCE", sizeof(char) },
                        { "LAYOUT", sizeof(char) },
                        { "POSITION", sizeof(int32_t) },
                        { "LENGTH", sizeof(int32_t) },
                        { "CIGAR", sizeof(char) },
                        { "SEQUENCE", sizeof(char) },
                        { "CONTIG", sizeof(int64_t) },
                    });
    writer.addTable(
                    "REJECTS", {
                        { "READ_GROUP", sizeof(char) },
                        { "NAME", sizeof(char) },
                        { "READNO", sizeof(int32_t) },
                        { "SEQUENCE", sizeof(char) },
                        { "REFERENCE", sizeof(char) },
                        { "STRAND", sizeof(char) },
                        { "POSITION", sizeof(int32_t) },
                        { "CIGAR", sizeof(char) },
                    });
    writer.addTable(
                    "CONTIGS", {
                        { "REFERENCE", sizeof(char) },
                        { "START", sizeof(int32_t) },
                        { "END", sizeof(int32_t) },
                        
                        { "SEQ_LENGTH_AVERAGE", sizeof(float) },
                        { "SEQ_LENGTH_STD_DEV", sizeof(float) },
                        { "REF_LENGTH_AVERAGE", sizeof(float) },
                        { "REF_LENGTH_STD_DEV", sizeof(float) },
                        
                        { "FRAGMENT_COUNT", sizeof(uint32_t) },
                        { "FRAGMENT_LENGTH_AVERAGE", sizeof(float) },
                        { "FRAGMENT_LENGTH_STD_DEV", sizeof(float) },
                        { "READ_GROUP", sizeof(char) },
                    });
    
    writer.beginWriting();
    writer.flush();
    
    auto const keepTable = writer.table("FRAGMENTS");
    auto const keepGroup = keepTable.column("READ_GROUP");
    auto const keepName = keepTable.column("NAME");
    auto const keepRef = keepTable.column("REFERENCE");
    auto const keepLayout = keepTable.column("LAYOUT");
    auto const keepPosition = keepTable.column("POSITION");
    auto const keepLength = keepTable.column("LENGTH");
    auto const keepCIGAR = keepTable.column("CIGAR");
    auto const keepSequence = keepTable.column("SEQUENCE");
    auto const keepContig = keepTable.column("CONTIG");
    
    auto const badTable = writer.table("REJECTS");
    auto const badGroup = badTable.column("READ_GROUP");
    auto const badName = badTable.column("NAME");
    auto const badReadNo = badTable.column("READNO");
    auto const badRef = badTable.column("REFERENCE");
    auto const badStrand = badTable.column("STRAND");
    auto const badPosition = badTable.column("POSITION");
    auto const badCIGAR = badTable.column("CIGAR");
    auto const badSequence = badTable.column("SEQUENCE");
    
    auto const mgr = VDB::Manager();
    auto stats = ContigStats::load(mgr[stats_run]);
    auto const inDb = mgr[data_run];
    auto const in = Fragment::Cursor(inDb["RAW"]);
    auto const range = in.rowRange();
    auto const freq = (range.second - range.first) / 10.0;

    int nextReport;
    int loops;
    
    for (loops = 1; ; ++loops) {
        decltype(range.first) aligned = 0, spots = 0;
        nextReport = 1;

        for (auto row = range.first; row < range.second; ) {
            auto const fragment = in.read(row, range.second);
            auto const best = bestPair(fragment, stats, loops);
            ++spots;
            if (best.contig != stats.end()) {
                ++aligned;
                best.contig->length.add(best.fragmentLength);
                best.contig->qlength1.add(best.a->cigar.qlength);
                best.contig->qlength2.add(best.b->cigar.qlength);
            }
            if (nextReport * freq <= (row - range.first)) {
                std::cerr << "prog: pass " << loops << ", analyzing, processed " << nextReport << "0%" << std::endl;
                ++nextReport;
            }
        }
        auto const before = stats.stats.size();
        auto const changed = stats.cleanup(); ///< remove low coverage contigs
        auto const after = stats.stats.size();
        std::cerr << "info: " << floor(100.0 * double(aligned)/spots) << "% aligned" << std::endl;
        if (changed) {
            auto const eliminated = int(floor((before - after) * 100.0 / before));
            if (eliminated > 0)
                std::cerr << "info: eliminated " << floor((before - after) * 100.0 / before) << "% candidate contiguous regions; reanalysing to get convergence" << std::endl;
            else
                std::cerr << "info: eliminated " << (before - after) << " candidate contiguous regions; reanalysing to get convergence" << std::endl;
            if (loops >= 3)
                std::cerr << "warn: convergence not achieved on pass " << loops << std::endl;
        }
        else {
            std::cerr << "info: convergence achieved" << std::endl;
            break;
        }
    }
    
    nextReport = 1;
    for (auto row = range.first; row < range.second; ) {
        auto const fragment = in.read(row, range.second);
        auto const best = bestPair(fragment, stats, 0);
        if (best.a != fragment.detail.end() && best.b != fragment.detail.end()) {
            auto &contig = *best.contig;
            auto const &first = *best.a;
            auto const &second = *best.b;
            auto const layout = std::to_string(first.readNo) + first.strand + std::to_string(second.readNo) + second.strand; ///< encodes order and strand, e.g. "1+2-" or "2+1-" for normal Illumina
            auto const cigar = first.cigarString + "0P" + second.cigarString; ///< 0P is like a double-no-op; used here to mark the division between the two CIGAR strings; also represents the mate-pair gap, the length of which is inferred from the fragment length
            auto const sequence = fragment.sequence(first.readNo) + fragment.sequence(second.readNo); ///< just concatenate them
            
            keepGroup.setValue(fragment.group);
            keepName.setValue(fragment.name);
            keepLayout.setValue(layout);
            keepCIGAR.setValue(cigar);
            keepSequence.setValue(static_cast<std::string>(sequence));
            keepContig.setValue(contig.sourceRow);

            ///* update statistics about contig
            contig.qlength1.add(best.a->cigar.qlength);
            contig.qlength2.add(best.b->cigar.qlength);
            contig.rlength1.add(best.a->cigar.rlength);
            contig.rlength2.add(best.b->cigar.rlength);

            if (contig.mapulet == 0) {
                keepRef.setValue(references[contig.ref1]);
                keepPosition.setValue(best.pos1);
                keepLength.setValue(best.fragmentLength);
                
                contig.length.add(best.fragmentLength);
            }
            else {
                ///* modify the placement according to mapping
                auto const &mapulet = mapulets[contig.mapulet];
                auto const pos1 = best.pos1 - mapulet.start1;
                auto const end1 = pos1 + best.a->cigar.qlength;
                auto const pos2 = best.pos2 - mapulet.start2 + (mapulet.end1 - mapulet.start1) + mapulet.gap;
                auto const end2 = pos2 + best.b->cigar.qlength;
                auto const length = end2 - pos1;
                
                keepRef.setValue(mapulet.name); ///< reference name changes
                keepPosition.setValue(pos1); ///< aligned position changes
                keepLength.setValue(length); ///< fragment length changes; N.B. there is a gap in the mapping that has not been computed yet, this length doesn't take that into account, the gap needs to be added in on read
                (void)end1;
                
                contig.length.add(length);
            }
            keepTable.closeRow();
        }
        else {
            ///* there's no good contig for this fragment; probably the coverage was too low
            for (auto && i : fragment.detail) {
                badGroup.setValue(fragment.group);
                badName.setValue(fragment.name);
                badReadNo.setValue(i.readNo);
                badRef.setValue(i.reference);
                badStrand.setValue(i.strand);
                badPosition.setValue(i.position);
                badCIGAR.setValue(i.cigarString);
                badSequence.setValue(static_cast<std::string>(i.sequence));
                badTable.closeRow();
            }
        }
        if (nextReport * freq <= (row - range.first)) {
            std::cerr << "prog: generating fragment alignments, processed " << nextReport << "0%" << std::endl;
            ++nextReport;
        }
    }
    std::cerr << "prog: adjusting virtual references" << std::endl;
    stats.adjustMapulets(); ///< adjust for previously unknown gap, now that it's been computed

    std::cerr << "prog: writing reference info" << std::endl;
    writeReferences(writer, references, mapulets);

    std::cerr << "prog: writing contiguous region info" << std::endl;
    writeContigs(writer, stats.stats);

    writer.endWriting();
    std::cerr << "prog: DONE" << std::endl;
    return 0;
}

namespace assembleFragments {
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << commandLine.program[0] << " [-out=<path>] <sra run> [<stats run>]" << std::endl;
        exit(error ? 3 : 0);
    }
    
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine, false);
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
            usage(commandLine, true);
        }
        
        if (source.empty())
            usage(commandLine, true);
        
        FILE *ofs = nullptr;
        if (!outPath.empty()) {
            ofs = fopen(outPath.c_str(), "w");
            if (!ofs) {
                std::cerr << "failed to open output file: " << outPath << std::endl;
                exit(3);
            }
        }
        auto out = outPath.empty() ? stdout : ofs;

        return assemble(out, source, source2.empty() ? source : source2);
    }
}

int main(int argc, char *argv[]) {
    return assembleFragments::main(CommandLine(argc, argv));
}
