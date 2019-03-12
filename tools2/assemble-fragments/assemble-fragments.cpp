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

    unsigned ref2;
    int start2, end2;
    
    int gap, mingap;
    int count;
};

using ContigArray = utility::Array<unsigned>;
static constexpr auto unassignedContig = ContigArray::Element(0);
static constexpr auto assignedToNone = ~unassignedContig;

using SpotArray = utility::Array<VDB::Cursor::RowID>;

static std::vector<Mapulet> mapulets = {{ "invalid" }};

static unsigned createMapulet(unsigned ref1, int start1, int end1, unsigned ref2, int start2, int end2)
{
    std::string name = references[ref1] + ":" + std::to_string(start1 + 1) + "-" + std::to_string(end1)
               + "+" + references[ref2] + ":" + std::to_string(start2 + 1) + "-" + std::to_string(end2);
    Mapulet o = { name, unsigned(mapulets.size()), ref1, start1, end1, ref2, start2, end2 };
    mapulets.emplace_back(o);
    return o.id;
}

struct Contig {
    ContigArray::Element sourceRow;
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
    
    bool isUngapped() const { return mapulet == 0; }
    bool isGapped() const { return !isUngapped(); }
    
    ///* computes fragment length while accounting for any gap or structural rearrangement
    int adjustedFragmentLength(AlignmentShort const &one, AlignmentShort const &two) const {
        return (two.qended() - start2) + (end1 - one.qstart()); // if ungapped; start2 == end1 == 0
    }
    
    bool contains(AlignmentShort const &one, AlignmentShort const &two) const {
        if (isUngapped())
            return one.reference == references[ref1]
                && start1 <= one.qstart() && one.qended() <= end2
                && start1 <= two.qstart() && two.qended() <= end2;
        if (ref1 == ref2)
            return one.reference == references[ref1]
                && start1 <= one.qstart() && one.qended() <= end1
                && start2 <= two.qstart() && two.qended() <= end2;
        return one.reference == references[ref1]
            && two.reference == references[ref2]
            && start1 <= one.qstart() && one.qended() <= end1
            && start2 <= two.qstart() && two.qended() <= end2;
    }
    
    int betterFit(AlignmentShort const &a1, AlignmentShort const &a2, AlignmentShort const &b1, AlignmentShort const &b2) const
    {
        if (isUngapped()) {
            auto const fla = a2.qended() - a1.qstart();
            auto const flb = b2.qended() - b1.qstart();
            auto const Ta = std::abs(fla - average) / (std_dev == 0.0 ? 1.0 : std_dev);
            auto const Tb = std::abs(flb - average) / (std_dev == 0.0 ? 1.0 : std_dev);
            if (Ta < Tb) return 1;
            if (Tb < Ta) return 2;
        }
        return 0;
    }
    
    ///* these functions below are ONLY valid after statistics have been gathered
    double averageCoverage() const {
        assert(qlength1.count() == qlength2.count() && qlength2.count() == length.count());
        auto const width = (end1 - start1) + (end2 - start2);
        auto const bases = (qlength1.average() + qlength2.average()) * length.count();
        return bases / width;
    }
};

struct Contigs {
    std::vector<Contig> stats;
    std::vector<unsigned> refIndex;

    std::vector<Contig>::const_iterator end() const { return stats.end(); }
    template <typename F>
    void forEachIntersecting(unsigned ref, int start, F func) const {
        auto const first = stats.begin() + refIndex[ref];
        if (first == stats.end()) return;
        assert(first->ref1 == ref);
        auto const end = stats.begin() + refIndex[ref + 1];
        auto const upper = std::upper_bound(first, end, start, [](int v, Contig const &a) { return v < a.start1; });
        auto const beg = (upper != end) ? (upper - upper->window) : ((upper - 1) - (upper - 1)->window);
        for (auto i = beg; i != upper; ++i) {
            func(*i);
        }
    }
    void generateRefIndex() {
        std::map<unsigned, unsigned> first;
        unsigned index = 0;
        for (auto && i : stats) {
            first.insert({i.ref1, index});
            ++index;
        }
        auto const refMax = first.rbegin()->first;
        refIndex.clear();
        refIndex.resize(refMax + 2, index);
        for (auto && i : first) {
            refIndex[i.first] = i.second;
        }
    }
    void generateWindow() {
        for (auto i = stats.begin(); i != stats.end(); ++i) {
            i->window = 0;
        }
        for (auto i = stats.begin(); i != stats.end(); ++i) {
            auto const end = i->isUngapped() ? i->end2 : i->end1;
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

    bool cleanup(ContigArray &spots) {
        auto clean = decltype(stats)();
        auto change = std::map<ContigArray::Element, ContigArray::Element>();
        clean.reserve(stats.size());
        for (auto && i : stats) {
            Contig o = i;
            bool const lostAlignments = i.count > i.length.count();

            o.sourceRow = 0;
            if (i.length.count() >= 5.0 && i.averageCoverage() >= 5.0) {
                o.sourceRow = (unsigned)(clean.size() + 1);
                
                // copy over new stats
                o.count = i.length.count();
                o.average = i.length.average();
                o.std_dev = sqrt(i.length.variance());

                if (lostAlignments) {
                    // reset stats
                    o.length = StatisticsAccumulator();
                    o.rlength1 = StatisticsAccumulator();
                    o.rlength2 = StatisticsAccumulator();
                    o.qlength1 = StatisticsAccumulator();
                    o.qlength2 = StatisticsAccumulator();
                }
                clean.emplace_back(o);
            }
            if (o.sourceRow == 0 || lostAlignments) {
                change[i.sourceRow] = unassignedContig; ///< any spots which had aligned to this contig will need to be re-evaluated
            }
            else if (o.sourceRow != i.sourceRow) {
                change[i.sourceRow] = o.sourceRow; ///< any spots which had aligned to this contig will need to be updated with the new contig id
            }
        }
        if (change.size() > 0) {
            for (auto && i : spots) {
                auto const ch = change.find(i);
                if (ch != change.end())
                    i = ch->second;
            }
        }
        stats.swap(clean);
        generateWindow();
        generateRefIndex();
        return change.size() != 0;
    }

    void computeGaps() {
        auto median = groupMedians();
        for (auto && i : stats) {
            if (i.mapulet) {
                auto const m = i.mapulet;
                mapulets[m].count = i.length.count();
                if (mapulets[m].count == 0) continue;
                
                auto const length = i.length.average();
                auto const median_length = median[i.group].average();
                // minimum length + gap length >= sum(maximum read lengths)
                auto const mingap = (i.qlength1.maximum() + i.qlength2.maximum()) - i.length.minimum();
                // average length + gap length = median length (roughly)
                auto const gap = median_length < length ? -ceil(length - median_length) : ceil(median_length - length);
                mapulets[m].gap = gap;
                mapulets[m].mingap = mingap < 0 ? 0 : ceil(mingap);
            }
        }
    }
    
    static Contigs load(VDB::Database const &db) {
        auto const tbl = db["CONTIGS"];
        auto const curs = tbl.read({ "REFERENCE_1", "START_1", "END_1", "REFERENCE_2", "START_2", "END_2", "READ_GROUP" /*, "COUNT" */ });
        auto const range = curs.rowRange();
        auto result = Contigs();

        result.stats.reserve(range.count());
        curs.foreach([&](VDB::Cursor::RowID row, std::vector<VDB::Cursor::RawData> const &data) {
            auto const ref1 = data[0].string();
            auto const refId1 = references[ref1];
            auto const start1 = data[1].value<int32_t>();
            auto const end1 = data[2].value<int32_t>();
            auto const ref2 = data[3].string();
            auto const refId2 = references[ref2];
            auto const start2 = data[4].value<int32_t>();
            auto const end2 = data[5].value<int32_t>();
            auto const rg = data[6].string();

            Contig o = { (unsigned)row, 0, 0 };
            
            o.ref1 = refId1;
            o.start1 = start1;
            o.end1 = end1;
            
            o.ref2 = refId2;
            o.start2 = start2;
            o.end2 = end2;

            if (refId2 == refId1 && start2 < end1)
                o.start2 = o.end1 = 0;
            else if (end1 != 0 && start2 != 0)
                o.mapulet = createMapulet(refId1, start1, end1, refId2, start2, end2);

            o.group = groups[rg];
            
            result.stats.emplace_back(o);
        });
        std::sort(result.stats.begin(), result.stats.end(), [](Contig const &a, Contig const &b) {
            if (a.ref1 < b.ref1) return true;
            if (b.ref1 < a.ref1) return false;
            if (a.start1 < b.start1) return true;
            return false;
        });
        result.generateRefIndex();
        result.generateWindow();
        return result;
    }
    
    static void addTableTo(Writer2 &writer) {
        writer.addTable("CONTIGS", {
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
            
            { "REFERENCE_1", sizeof(char) },
            { "START_1", sizeof(int32_t) },
            { "END_1", sizeof(int32_t) },
            { "GAP", sizeof(int32_t) },
            { "MIN_GAP", sizeof(int32_t) },
            { "REFERENCE_2", sizeof(char) },
            { "START_2", sizeof(int32_t) },
            { "END_2", sizeof(int32_t) },
        });
    }

    void write(Writer2 const &out) const
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
        
        auto const REF_1 = table.column("REFERENCE_1");
        auto const STR_1 = table.column("START_1");
        auto const END_1 = table.column("END_1");
        auto const GAP = table.column("GAP");
        auto const MIN_GAP = table.column("MIN_GAP");
        auto const REF_2 = table.column("REFERENCE_2");
        auto const STR_2 = table.column("START_2");
        auto const END_2 = table.column("END_2");
        
        float qlength_average[2];
        float qlength_std_dev[2];
        float rlength_average[2];
        float rlength_std_dev[2];
        
        for (auto && i : stats) {
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
                STR.setValue(int32_t(i.start1));
                END.setValue(int32_t(i.end2));
                AVERAGE.setValue(float(i.length.average()));
                
                REF_1.setValueEmpty();
                STR_1.setValueEmpty();
                END_1.setValueEmpty();
                GAP.setValueEmpty();
                MIN_GAP.setValueEmpty();
                REF_2.setValueEmpty();
                STR_2.setValueEmpty();
                END_2.setValueEmpty();
            }
            else {
                auto const &mapulet = mapulets[i.mapulet];
                REF.setValueEmpty();
                STR.setValue(int32_t(0));
                END.setValue(int32_t((i.end1 - i.start1) + mapulet.gap + (i.end2 - i.start2)));
                AVERAGE.setValue(float(i.length.average() + mapulet.gap));
                
                REF_1.setValue(references[i.ref1]);
                STR_1.setValue(int32_t(i.start1));
                END_1.setValue(int32_t(i.end1));
                GAP.setValue(int32_t(mapulet.gap));
                MIN_GAP.setValue(int32_t(mapulet.mingap));
                REF_2.setValue(references[i.ref2]);
                STR_2.setValue(int32_t(i.start2));
                END_2.setValue(int32_t(i.end2));
            }
            table.closeRow();
        }
    }
};

struct AlignedPair {
    Contig const *contig;
    Alignment const *a;
    Alignment const *b;
    int fragmentLength;
    
    bool isSameRef() const {
        return contig->ref1 == contig->ref2;
    }
    int alignedLength() const {
        return a->cigar.alignedLength() + b->cigar.alignedLength();
    }
    double Tscore() const {
        return (isSameRef() && contig->std_dev > 0.0) ? std::abs(fragmentLength - contig->average) / contig->std_dev : -1.0;
    }

    static std::vector<AlignedPair> candidates(Fragment const &fragment, Contigs const &contigs, unsigned const group, bool simple)
    {
        struct alignment {
            Alignment const *mom;
            int pos, end;
            unsigned ref;
            
            alignment(Alignment const *mom) : mom(mom) {
                if (!references.contains(mom->reference, ref))
                    throw std::logic_error("");
                pos = mom->qstart();
                end = mom->qended();
            }
            bool operator <(alignment const &other) const {
                return (ref == other.ref) ? (pos < other.pos) : (ref < other.ref);
            }
            bool isContainedByFirst(Contig const &contig) const {
                auto const cend = contig.isUngapped() ? contig.end2 : contig.end1;
                return ref == contig.ref1 && contig.start1 <= pos && end <= cend;
            }
            bool isContainedBySecond(Contig const &contig) const {
                auto const start = contig.isUngapped() ? contig.start1 : contig.start2;
                return ref == contig.ref2 && start <= pos && end <= contig.end2;
            }
        };
        std::vector<AlignedPair> rslt;
        auto const add = [&](alignment const &a, alignment const &b) -> void {
            auto const fragmentLength = (b.ref == a.ref) ? (b.end - a.pos) : 0;

            contigs.forEachIntersecting(a.ref, a.pos, [&](Contig const &i) ->void {
                if (i.group == group && a.isContainedByFirst(i) && b.isContainedBySecond(i)) {
                    auto const isGoodAlignedPair = i.isGapped() || a.mom->isGoodAlignedPair(*b.mom);
                    if (isGoodAlignedPair)
                        rslt.emplace_back(AlignedPair { &i, a.mom, b.mom, fragmentLength });
                }
            });
        };
        
        rslt.reserve(2);
        if (simple) {
            auto one = fragment.detail.begin();
            auto two = one + 1;
            
            assert(one->readNo == 1 && one->aligned);
            assert(two->readNo == 2 && two->aligned);

            auto const a1 = alignment(&(*one));
            auto const a2 = alignment(&(*two));
            if (a1 < a2)
                add(a1, a2);
            else
                add(a2, a1);
        }
        else {
            auto const &alignments = fragment.detail;
            
            for (auto one = alignments.begin(); one < alignments.end(); ++one) {
                if (one->readNo != 1 || one->aligned == false) continue;
                
                auto const a1 = alignment(&(*one));
                
                for (auto two = alignments.begin(); two < alignments.end(); ++two) {
                    if (two->readNo != 2 || !one->isAlignedPair(*two)) continue;
                    
                    auto const a2 = alignment(&(*two));
                    if (a1 < a2)
                        add(a1, a2);
                    else
                        add(a2, a1);
                }
            }
        }
        return rslt;
    }

    static AlignedPair bestSpotAlignment(Fragment const &fragment, Contigs const &contigs, int const loopNumber, unsigned const group)
    {
        AlignedPair result = { nullptr, nullptr, nullptr, 0 };
        
        if (fragment.numberOfReads() == 2) {
            auto const n1 = fragment.countOfAlignments(1);
            auto const n2 = fragment.countOfAlignments(2);
            
            if (n1 == 0 || n2 == 0)
                ;
            else {
                auto v = candidates(fragment, contigs, group, n1 == 1 && n2 == 1);
#if 0 /* this is no longer true; overlapping reads have not been removed */
                if (v.size() == 0 && loopNumber == 1) {
                    std::cerr << fragment << std::endl;
                    throw std::logic_error("no fully-aligned spots should be dropped on the first pass!");
                }
#endif
                if (v.size() > 1) {
                    std::sort(v.begin(), v.end(), [](AlignedPair const &a, AlignedPair const &b) {
                        auto const aT = a.Tscore();
                        auto const bT = b.Tscore();
                        auto const a_qlength = a.alignedLength();
                        auto const b_qlength = b.alignedLength();
                        
                        auto aScore = (aT >= 0.0 && bT >= 0.0 && aT < bT ? 16 : 0)
                                    | (a.contig->isUngapped()             ?  8 : 0)
                                    | (a.isSameRef()                     ?  4 : 0)
                                    | (a.contig->count > b.contig->count ?  2 : 0)
                                    | (a_qlength > b_qlength             ?  1 : 0);
                        
                        auto bScore = (aT >= 0.0 && bT >= 0.0 && bT < aT ? 16 : 0)
                                    | (b.contig->isUngapped()             ?  8 : 0)
                                    | (b.isSameRef()                     ?  4 : 0)
                                    | (b.contig->count > a.contig->count ?  2 : 0)
                                    | (b_qlength > a_qlength             ?  1 : 0);
                        
                        return aScore < bScore;
                    });
                }
                if (v.size() > 0)
                    result = v.back();
            }
        }
        return result;
    }
};

static std::pair<int, int> bestPair(Fragment const &fragment, Contig const &contig)
{
    using Result = std::pair<int, int>;
    if (fragment.numberOfReads() == 2) {
        auto const n1 = fragment.countOfAlignments(1);
        auto const n2 = fragment.countOfAlignments(2);
        
        if (n1 == 0 || n2 == 0)
            ;
        else if (n1 == 1 && n2 == 1) {
            // since each read has only 1 alignment, there is only one alignment for the fragment
            auto const &one = fragment.detail[0];
            auto const &two = fragment.detail[1];
            
            if ((one.reference == two.reference && one.position > two.position) || one.reference > two.reference)
                return Result(1, 0);
            else
                return Result(0, 1);
        }
        else {
            std::vector<Result> v;
            auto j2 = 0;
            for (j2 = 0; j2 < fragment.detail.size(); ++j2) {
                if (fragment.detail[j2].readNo == 2) break;
            }
            for (auto i = 0; i < fragment.detail.size(); ++i) {
                if (fragment.detail[i].readNo != 1) break;
                if (!fragment.detail[i].aligned) continue;
                for (auto j = j2; j < fragment.detail.size(); ++j) {
                    if (contig.isUngapped() && !fragment.detail[i].isGoodAlignedPair(fragment.detail[j])) continue;
                    
                    auto const rev = (fragment.detail[j].reference == fragment.detail[i].reference && fragment.detail[j].position < fragment.detail[i].position) || fragment.detail[j].reference < fragment.detail[i].reference;
                    auto const &one = fragment.detail[rev ? j : i];
                    auto const &two = fragment.detail[rev ? i : j];
                    if (contig.contains(one, two))
                        v.emplace_back(Result(rev ? j : i, rev ? i : j));
                }
            }
            assert(v.size() > 0);
            if (v.size() > 1) {
                if (contig.isUngapped()) {
                    std::sort(v.begin(), v.end(), [&](decltype(v)::const_reference a, decltype(a) b) {
                        auto const &a1 = fragment.detail[a.first];
                        auto const &a2 = fragment.detail[a.second];
                        auto const &b1 = fragment.detail[b.first];
                        auto const &b2 = fragment.detail[b.second];
                        auto const cmp = contig.betterFit(a1, a2, b1, b2);
                        if (cmp == 1) return true;
                        return false;
                    });
                }
                else {
                    std::cerr << "warn: randomly choosing an alignment for fragment" << fragment.name << " to contig " << contig.sourceRow << std::endl;
                    std::random_shuffle(v.begin(), v.end());
                }
            }
            return v.front();
        }
    }
    return Result(-1, -1);
}

struct QualityStats {
private:
    using InnerStats = std::map<uint32_t, uint64_t>;
    std::map<uint32_t, InnerStats> stats;

    static uint32_t makeKey(unsigned const upper, unsigned const lower) {
        return uint32_t(upper << 8) | uint32_t(lower & 0xFF);
    }
    static std::pair<unsigned, unsigned> splitKey(uint32_t const key) {
        return { unsigned(key >> 8) , unsigned(key & 0xFF) };
    }
    InnerStats &getInner(unsigned group, unsigned read) {
        auto const key = makeKey(group, read);
        auto i = stats.find(key);
        if (i != stats.end()) return i->second;
        return stats.insert({ key, InnerStats() }).first->second;
    }
public:
    void add(unsigned group, unsigned read, bool reversed, std::string const &quality) {
        auto &m = getInner(group, read);
        auto const last = quality.length() - 1;
        auto I = 0;
        for (auto && phred : quality) {
            auto i = I++;
            auto const cycle = unsigned(reversed ? (last - i) : i);
            auto const key = makeKey(cycle, phred);
            auto const j = m.insert({ key, 0 });
            j.first->second += 1;
        }
    }
    
    void write(Writer2 const &out) const {
        auto const table = out.table("QUALITY_STATISTICS");
        auto const READ_GROUP = table.column("READ_GROUP");
        auto const READNO = table.column("READNO");
        auto const CYCLE = table.column("READ_CYCLE");
        auto const QUALITY = table.column("QUALITY");
        auto const COUNT = table.column("COUNT");

        for (auto && i : stats) {
            auto const &inner = i.second;
            auto const rgrn = splitKey(i.first);
            auto const &readGroup = groups[rgrn.first];
            auto const readNo = int32_t(rgrn.second);
            
            READ_GROUP.setValue(readGroup);
            READNO.setValue<int32_t>(readNo);
            for (auto && j : inner) {
                auto const &count = j.second;
                auto const cyqv = splitKey(j.first);
                auto const cycle = uint32_t(cyqv.first);
                auto const qual = cyqv.second;
                CYCLE.setValue<uint32_t>(cycle);
                QUALITY.setValue<char>(qual);
                COUNT.setValue<uint64_t>(count);
            }
            table.closeRow();
        }
    }
    
    static void addTableTo(Writer2 &writer) {
        writer.addTable("QUALITY_STATISTICS", {
            { "READ_GROUP", sizeof(char) },
            { "READNO", sizeof(int32_t) },
            { "READ_CYCLE", sizeof(uint32_t) },
            { "QUALITY", sizeof(char) },
            { "COUNT", sizeof(uint64_t) },
        });
    }
};

static void assignContigs(ContigArray &contigIdArray, Fragment::Cursor const &fragments, Contigs &trialContigs, SpotArray const &spotArray)
{
    auto const spots = spotArray.elements();
    auto const range = fragments.rowRange();
    auto loops = 0;
    auto changed = false;
    
    std::cerr << "info: assigning alignments to trial contigs" << std::endl;
    do {
        size_t aligned = 0;
        auto progress = ProgressTracker<10>(spots);

        ++loops;
        for (auto i = decltype(spots)(0); i < spots; ++i) {
            if (loops > 1 && contigIdArray[i] == assignedToNone) {
                goto REPORT_1;
            }
            if (loops == 1 || contigIdArray[i] == unassignedContig) {
                auto row = spotArray[i];
                auto const fragment = fragments.readShort(row, range.end());
                auto const best = AlignedPair::bestSpotAlignment(fragment, trialContigs, loops, groups[fragment.group]);
                if (best.contig != nullptr) {
                    ++aligned;
                    contigIdArray[i] = best.contig->sourceRow;
                    best.contig->length.add(best.fragmentLength);
                    best.contig->qlength1.add(best.a->cigar.qlength);
                    best.contig->qlength2.add(best.b->cigar.qlength);
                }
                else {
                    contigIdArray[i] = assignedToNone;
                }
            }
            else {
                ++aligned;
            }
        REPORT_1:
            progress.current(i, [&](int cur) {
                std::cerr << "prog: pass " << loops << ", analyzing, processed " << cur << "%" << std::endl;
            });
        }
        std::cerr << "prog: pass " << loops << " done" << std::endl;
        
        auto const before = trialContigs.stats.size();
        changed = trialContigs.cleanup(contigIdArray); ///< remove low coverage contigs
        auto const after = trialContigs.stats.size();
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
    } while (changed);
    std::cerr << "info: convergence achieved" << std::endl;
}

static int write(FILE *out, Fragment::Cursor const &fragments, Contigs &contigs, SpotArray const &spotArray, ContigArray &contigIdArray)
{
    auto const spots = spotArray.elements();
    auto const range = fragments.rowRange();
    auto const quality = fragments.hasQuality();
    auto qstats = QualityStats();
    auto const &rejectColumns = quality
    ? std::initializer_list<Writer2::ColumnDefinition> {
            { "READ_GROUP", sizeof(char) },
            { "NAME", sizeof(char) },
            { "READNO", sizeof(int32_t) },
            { "SEQUENCE", sizeof(char) },
            { "QUALITY", sizeof(char) },
            { "REFERENCE", sizeof(char) },
            { "STRAND", sizeof(char) },
            { "POSITION", sizeof(int32_t) },
            { "CIGAR", sizeof(char) },
            { "REJECT_REASON", sizeof(char) },
        }
    : std::initializer_list<Writer2::ColumnDefinition> {
            { "READ_GROUP", sizeof(char) },
            { "NAME", sizeof(char) },
            { "READNO", sizeof(int32_t) },
            { "SEQUENCE", sizeof(char) },
            { "REFERENCE", sizeof(char) },
            { "STRAND", sizeof(char) },
            { "POSITION", sizeof(int32_t) },
            { "CIGAR", sizeof(char) },
            { "REJECT_REASON", sizeof(char) },
        };

    auto writer = Writer2(out);
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:aligned");
    writer.info("assemble-fragments", "1.0.0");

    writer.addTable("FRAGMENTS", {
                        { "NAME", sizeof(char) },
                        { "LAYOUT", sizeof(char) },
                        { "POSITION", sizeof(int32_t) },
                        { "LENGTH", sizeof(int32_t) },
                        { "CIGAR", sizeof(char) },
                        { "SEQUENCE", sizeof(char) },
                        { "CONTIG", sizeof(int64_t) },
                    });

    writer.addTable("REJECTS", rejectColumns);

    Contigs::addTableTo(writer);

    if (quality)
        QualityStats::addTableTo(writer);

    writer.beginWriting();
    writer.flush();
    
    auto const keepTable = writer.table("FRAGMENTS");
    auto const keepName = keepTable.column("NAME");
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
    auto const reason = badTable.column("REJECT_REASON");
    auto const &badQuality = quality ? badTable.column("QUALITY") : reason;
    
    auto good = int64_t(0);
    auto reject = int64_t(0);
    auto progress = ProgressTracker<10>(spots);

    reason.setDefault(std::string("too few corroborating alignments"));

    for (auto i = decltype(spots)(0); i < spots; ++i) {
        auto row = spotArray[i];
        auto const fragment = fragments.read(row, range.end());
        auto const contigId = contigIdArray[i];
        unsigned group;
        if (!groups.contains(fragment.group, group)) continue;

        if (contigId != assignedToNone) {
            assert(contigId != unassignedContig);
            auto &contig = contigs.stats[contigId - 1];
            auto const best = bestPair(fragment, contig);
            auto const &first = fragment.detail[best.first];
            auto const &second = fragment.detail[best.second];
            auto const layout = std::to_string(first.readNo) + first.strand + std::to_string(second.readNo) + second.strand; ///< encodes order and strand, e.g. "1+2-" or "2+1-" for normal Illumina
            auto const cigar = first.cigarString + "0P" + second.cigarString; ///< 0P is like a double-no-op; used here to mark the division between the two CIGAR strings; also represents the mate-pair gap, the length of which is inferred from the fragment length
            auto const best1 = fragment.bestIndex(first.readNo);
            auto const best2 = fragment.bestIndex(second.readNo);
            auto const sequence = fragment.detail[best1].sequence + fragment.detail[best2].sequence; ///< just concatenate them
            int32_t const pos = first.qstart() - contig.start1;
            int32_t const len = contig.adjustedFragmentLength(first, second);
            
            if (!(sequence.length() <= len) && contig.isUngapped()) {
                std::cerr << "fragment length: " << len << " is less than sequence length: " << sequence.length() << std::endl;
                std::cerr << fragment;
                if (fragment.detail.size() > 2) {
                    std::cerr << "chosen fragment alignment is:\n" << first << '\n' << second << std::endl;
                }
                abort();
            }
            keepName.setValue(fragment.name);
            keepLayout.setValue(layout);
            keepCIGAR.setValue(cigar);
            keepSequence.setValue(sequence);
            keepContig.setValue<int64_t>(contig.sourceRow);
            keepPosition.setValue<int32_t>(pos);
            keepLength.setValue<int32_t>(len);
            
            keepTable.closeRow();
            
            //* update statistics about contig
            contig.length.add(len);
            contig.qlength1.add(first.cigar.qlength);
            contig.qlength2.add(second.cigar.qlength);
            contig.rlength1.add(first.cigar.rlength);
            contig.rlength2.add(second.cigar.rlength);
            
            if (quality) {
                qstats.add(group, first.readNo, first.strand == '-', fragment.detail[best1].quality);
                qstats.add(group, second.readNo, second.strand == '-', fragment.detail[best2].quality);
            }
            ++good;
        }
        else {
            for (auto && i : fragment.detail) {
                badGroup.setValue(fragment.group);
                badName.setValue(fragment.name);
                badReadNo.setValue<int32_t>(i.readNo);
                badRef.setValue(i.reference);
                badStrand.setValue<char>(i.strand);
                badPosition.setValue<int32_t>(i.position);
                badCIGAR.setValue(i.cigarString);
                badSequence.setValue(static_cast<std::string const &>(i.sequence));
                if (quality)
                    badQuality.setValue(i.quality);
                badTable.closeRow();
            }
            ++reject;
        }
        progress.current(i, [](int pct) {
            std::cerr << "prog: generating fragment alignments, processed " << pct << "%" << std::endl;
        });
    }
    std::cerr << "info: generated " << good << " fragment alignments, rejected all alignments for " << reject << " fragments" << std::endl;
    std::cerr << "prog: adjusting virtual references" << std::endl;
    contigs.computeGaps(); ///< adjust for previously unknown gap, now that it's been computed

    std::cerr << "prog: writing reference and contiguous region info" << std::endl;
    contigs.write(writer);

    if (quality) {
        std::cerr << "prog: writing quality statistics" << std::endl;
        qstats.write(writer);
    }
    
    writer.endWriting();
    std::cerr << "prog: DONE" << std::endl;
    return 0;
}

static int assemble(FILE *out, std::string const &data_run, std::string const &stats_run)
{
    auto const mgr = VDB::Manager();
    auto contigs = Contigs::load(mgr[stats_run]); ///< this populates the list of spot groups too
    auto const inDb = mgr[data_run];
    auto const fragments = Fragment::Cursor(inDb["RAW"]);
    auto const range = fragments.rowRange();
    auto tmp = SpotArray(range.count()); ///< this is ~2x over-allocation, it will be resized
    if (!tmp)
        throw std::bad_alloc();
    
    std::cerr << "info: generating spot list" << std::endl;
    size_t i = 0;
    int nextReport = 1;
    auto const freq = range.count() / 10.0;
    
    fragments.forEach([&](VDB::Cursor::RowRange const &spotRange, std::string const &group, std::string const &name) {
        unsigned gid = 0;
        if (groups.contains(group, &gid)) {
            tmp[i] = spotRange.beg();
            ++i;
        }
        if (nextReport * freq <= spotRange.end()) {
            std::cerr << "prog: " << nextReport << "0%" << std::endl;
            ++nextReport;
        }
    });
    assert(i <= tmp.elements());

    auto spotArray = tmp.resize(i); ///< free excess; this is likely to free 1/2 the space of the array
    auto contigIdArray = ContigArray(i); ///< this will likely fit entirely into the just-free'd space
    if (!spotArray || !contigIdArray)
        throw std::bad_alloc();

    assignContigs(contigIdArray, fragments, contigs, spotArray);
    return write(out, fragments, contigs, spotArray, contigIdArray);
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
        auto source2 = std::string();
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
                std::cerr << "error: failed to open output file: " << outPath << std::endl;
                exit(3);
            }
        }
        auto out = outPath.empty() ? stdout : ofs;

        return assemble(out, source, source2.empty() ? source : source2);
    }
}

int main(int argc, char *argv[])
{
    return assembleFragments::main(CommandLine(argc, argv));
}
