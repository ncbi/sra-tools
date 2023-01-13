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
 *
 * Project:
 *  Loader QA Stats
 *
 * Purpose:
 *  Compute expected invariant statistics.
 */

// FIXME: Ref Hashing is not order-insensitive
#define USE_REF_HASH 0

#include <iostream>
#include <map>
#include <functional>
#include <sstream>
#include "input.hpp"
#include "hashing.hpp"
#include "output.hpp"

struct Event {
    unsigned
        spot: 1,
        bio: 1,
        tech: 1,
        read: 1,
        base: 1,
        A: 1, C: 1, G: 1, T: 1;

    bool isAT() const {
        return A != 0 || T != 0;
    }
    bool isCG() const {
        return C != 0 || G != 0;
    }
    bool isACGT() const {
        return isAT() || isCG();
    }
    char baseValue() const {
        return A != 0 ? 'A' : C != 0 ? 'C' : G != 0 ? 'G' : T != 0 ? 'T' : base ? 'N' : 0;
    }
    char const *type() const {
        if (bio) return "biological";
        if (tech) return "technical";
        return "unknown";
    }
    bool operator <(Event const &other) const {
        return *reinterpret_cast<unsigned const *>(this) < reinterpret_cast<unsigned const &>(other);
    }
    friend std::ostream &operator <<(std::ostream &os, Event const &that) {
        return os << reinterpret_cast<unsigned const &>(that);
    }
    static Event startSpot() {
        auto e = Event{1};
        // std::cout << "startSpot: " << e << std::endl;
        return e;
    }
    static Event make(bool isBio, bool isTech) {
        Event e = {};
        if (isBio)
            e.bio = 1;
        else if (isTech)
            e.tech = 1;
        return e;
    }
    static Event startRead(bool isBio = false, bool isTech = false) {
        Event e = make(isBio, isTech);
        e.read = 1;
        // std::cout << "startRead: " << e << std::endl;
        return e;
    }
    static Event makeBase(char base, bool isBio = false, bool isTech = false) {
        Event e = make(isBio, isTech);
        switch (base) {
        case 'A': e.A = 1; break;
        case 'C': e.C = 1; break;
        case 'G': e.G = 1; break;
        case 'T': e.T = 1; break;
        default:
            e.base = 1; break;
        }
        // std::cout << "makeBase: " << e << std::endl;
        return e;
    }
    operator bool() const { return *reinterpret_cast<unsigned const *>(this) != 0; }
};

struct Count {
    uint64_t total, biological, technical;
    Count() : total(0), biological(0), technical(0) {}

    Count &operator +=(Count const &addend) {
        total += addend.total;
        biological += addend.biological;
        technical += addend.technical;
        return *this;
    }
};

struct SpotStats : public std::map<unsigned, Count> {
    friend JSON_ostream &operator <<(JSON_ostream &out, SpotStats const &spots) {
        for (auto &[k, v] : spots) {
            out << '{'
                << JSON_Member{"nreads"} << k
                << JSON_Member{"total"} << v.total
                << JSON_Member{"biological"} << v.biological
                << JSON_Member{"technical"} << v.technical
            << '}';
        }
        return out;
    }
};

struct BaseStats : public std::map<Event, uint64_t> {
    friend JSON_ostream &operator <<(JSON_ostream &out, BaseStats const &event) {
        for (auto &[k, v] : event) {
            out << '{'
                << JSON_Member{"type"} << k.type();
            if (k.isACGT())
                out << JSON_Member{"base"} << std::string(1, k.baseValue());
                out << JSON_Member{"count"} << v;
            out << '}';
        }
        return out;
    }
};

struct DistanceStats : public std::map<std::pair<char, unsigned>, unsigned> {
    int lastA = -1;
    int lastC = -1;
    int lastG = -1;
    int lastT = -1;

    void reset() {
        lastA = -1;
        lastC = -1;
        lastG = -1;
        lastT = -1;
    }
    void accumulate(char base, int pos) {
        int *last = nullptr;
        if (pos >= 0) {
            switch (base) {
            case 'A':
                last = &lastA;
                break;
            case 'C':
                last = &lastC;
                break;
            case 'G':
                last = &lastG;
                break;
            case 'T':
                last = &lastT;
                break;
            }
        }
        if (last) {
            if (*last >= 0)
                (*this)[{base, pos - *last}] += 1;
            *last = pos;
        }
        else
            reset();
    }

    static char const *combinedBase(char base) {
        switch (base) {
        case 'A':
            return "AT";
        case 'C':
            return "CG";
        default:
            return nullptr;
        }
    }
    std::pair<char const *, unsigned> combined(key_type const &key, mapped_type count) const {
        auto &[base, distance] = key;
        auto const both = combinedBase(base);
        unsigned combined = count;

        if (both) {
            auto const fnd = find({both[1], distance});
            if (fnd != end())
                combined += fnd->second;
        }
        return {both, combined};
    }
    friend JSON_ostream &operator <<(JSON_ostream &out, DistanceStats const &distance) {
        for (auto &[key, count] : distance) {
            auto const &[both, counts] = distance.combined(key, count);
            if (both == nullptr) continue;

            out << '{'
                << JSON_Member{"base"} << both
                << JSON_Member{"distance"} << key.second
                << JSON_Member{"count"} << counts
            << '}';
        }
        return out;
    }
};

struct ReferenceStats : public std::vector<std::vector<Count>> {
    static constexpr int chunkSize = (1 << 14);

    void accumulate(int refId, int position, int strand, std::string const &sequence, CIGAR const &cigar) {
        while (refId >= size()) {
            auto value = std::vector<Count>();
            auto hach = Count();
#if USE_REF_HASH
            static_assert(sizeof(hach.total) == sizeof(FNV1a)
                          && sizeof(hach.biological) == sizeof(FNV1a)
                          && sizeof(hach.technical) == sizeof(FNV1a), "expected 64 bit counters");
            *reinterpret_cast<FNV1a *>(&hach.total) = FNV1a();
            *reinterpret_cast<FNV1a *>(&hach.biological) = FNV1a();
            *reinterpret_cast<FNV1a *>(&hach.technical) = FNV1a();
#endif
            value.push_back(hach);
            push_back(value);
        }
        auto &reference = (*this)[refId];
        if (position >= 0) {
            auto const chunk = 1 + position / chunkSize;

            while (chunk >= reference.size())
                reference.push_back(Count{});

            auto &counter = reference[chunk];

            counter.total += 1;
            if (strand == 0)
                counter.biological += 1;
            if (strand == 1)
                counter.technical += 1;
#if USE_REF_HASH
            auto &hashPos = *reinterpret_cast<FNV1a *>(&reference[0].total);
            auto &hashSeq = *reinterpret_cast<FNV1a *>(&reference[0].biological);
            auto &hashCIGAR = *reinterpret_cast<FNV1a *>(&reference[0].technical);
            hashPos.update(position);
            hashSeq.update(sequence.size(), sequence.data());
            hashCIGAR.update(cigar.operations.size(), cigar.operations.data());
#endif
        }
    }
    friend JSON_ostream &operator <<(JSON_ostream &out, ReferenceStats const &self) {
        char buffer[32];

        for (auto &reference : self) {
            auto const refId = &reference - &self[0];
            auto const refName = Input::references[refId];
            Count total;
#if USE_REF_HASH
            auto hashPos = *reinterpret_cast<FNV1a const *>(&reference[0].total);
            auto hashSeq = *reinterpret_cast<FNV1a const *>(&reference[0].biological);
            auto hashCIGAR = *reinterpret_cast<FNV1a const *>(&reference[0].technical);
            auto const posHash = hashPos.finish();
            auto const seqHash = hashSeq.finish();
            auto const cigHash = hashCIGAR.finish();
#endif
            out << '{';
            out << JSON_Member{"name"} << refName;
            out << JSON_Member{"chunks"} << '[';
            for (auto &counter : reference) {
                if (&counter == &reference[0] || counter.total == 0) continue;

                auto const chunk = (&counter - &reference[0]) - 1;
                total += counter;

                out
                << '{'
                    << JSON_Member{"start"} << chunk * chunkSize + 1
                    << JSON_Member{"last"} << (chunk + 1) * chunkSize
                    << JSON_Member{"alignments"} << '{'
                        << JSON_Member{"total"} << counter.total
                        << JSON_Member{"5'"} << counter.biological
                        << JSON_Member{"3'"} << counter.technical
                    << '}'
                << '}';
            }
            out << JSON_Member{"alignments"} << '{'
                    << JSON_Member{"total"} << total.total
                    << JSON_Member{"5'"} << total.biological
                    << JSON_Member{"3'"} << total.technical
                << '}'
            << ']';
#if USE_REF_HASH
            out << JSON_Member{"position-hash"} << SeqHash_impl::Result::to_hex(posHash, buffer);
            out << JSON_Member{"sequence-hash"} << SeqHash_impl::Result::to_hex(seqHash, buffer);
            out << JSON_Member{"cigar-hash"} << SeqHash_impl::Result::to_hex(cigHash, buffer);
#endif
            out << '}';
        }
        return out;
    }
};

struct Stats {
    template < typename HASH >
    struct Hash {
        HASH total, biological, technical;

        void update(SeqHash::Value const &value, bool bio, bool tech) {
            total.update(value);
            if (bio)
                biological.update(value);
            else if (tech)
                technical.update(value);
        }
        friend std::ostream &operator <<(std::ostream &os, Hash const &self) {
            os << "Hash of all " << self.total << std::endl;
            os << "Hash of biological " << self.biological << std::endl;
            os << "Hash of technical " << self.technical << std::endl;
            return os;
        }
    };

    Count reads; // read counts
    Hash<ReadHash> readHash;
    SpotStats spots; // spot counts by number of reads
    BaseStats event;

    using ReadStats = std::map<Event, unsigned>;
    ReadStats read;
    DistanceStats distance;
    ReferenceStats reference;

    Stats() {}

    Count countReads() {
        auto result = Count();

        for (auto [k, v] : read) {
            if (k.bio)
                result.biological += v;
            if (k.tech)
                result.technical += v;
            result.total += v;
        }
        read.clear();

        return result;
    }
    void accumulate(Event const &e, int32_t readpos = -1, int64_t refpos = -1) {
        if (e.spot) {
            auto const read = countReads();

            if (read.biological)
                spots[(unsigned)read.biological].biological += 1;

            if (read.technical)
                spots[(unsigned)read.technical].technical += 1;

            if (read.total)
                spots[(unsigned)read.total].total += 1;

            reads.biological += read.biological;
            reads.technical += read.technical;
            reads.total += read.total;

            return;
        }
        if (e.read) {
            read[e] += 1;
            distance.reset();
            return;
        }
        event[e] += 1;
        distance.accumulate(e.baseValue(), readpos);
    }
    void accumulateOneRead(std::string_view const &seq, bool bio, bool tech) {
        unsigned pos = 0;
        auto const hash = SeqHash::hash(seq.size(), seq.data());

        for (auto && ch : seq) {
            accumulate(Event::makeBase(ch, bio, tech), pos++);
        }
        accumulate(Event::startRead(bio, tech));
        readHash.update(hash, bio, tech);
    }
    void accumulateOneAlignment(int refId, int position, int strand, std::string const &sequence, CIGAR const &cigar) {
        reference.accumulate(refId, position, strand, sequence, cigar);
    }
    static JSON_ostream &print(JSON_ostream &out, ReadHash const &hash) {
        auto temp = hash;
        auto const value = temp.finish();
        char buffer[32];

        return out << '{'
            << JSON_Member{"bases"} << ReadHash::Value::to_hex(value.base, buffer)
            << JSON_Member{"unstranded"} << ReadHash::Value::to_hex(value.unstranded, buffer)
            << JSON_Member{"sequence"} << ReadHash::Value::to_hex(value.fnv1a, buffer)
        << '}';
    }
    struct ReadsInfo {
        decltype(Stats::reads) const &reads;
        decltype(Stats::readHash) const &hash;

        ReadsInfo(Stats const &stats)
        : reads(stats.reads)
        , hash(stats.readHash)
        {}

        friend JSON_ostream &operator <<(JSON_ostream &out, ReadsInfo const &self) {
            out << JSON_Member{"total"}
            << '{'
                << JSON_Member{"count"} << self.reads.total
                << JSON_Member{"hash"}; print(out, self.hash.total)
            << '}';

            if (self.reads.biological > 0)
            out << JSON_Member{"biological"}
            << '{'
                << JSON_Member{"count"} << self.reads.biological
                << JSON_Member{"hash"}; print(out, self.hash.biological)
            << '}';

            if (self.reads.technical > 0)
            out << JSON_Member{"technical"}
            << '{'
                << JSON_Member{"count"} << self.reads.technical
                << JSON_Member{"hash"}; print(out, self.hash.technical)
            << '}';

            return out;
        }
    };
    ReadsInfo readsInfo() const { return ReadsInfo(*this); }

    friend JSON_ostream &operator <<(JSON_ostream &out, Stats const &self) {
        out << JSON_Member{"reads"}      << '{' << self.readsInfo() << '}';
        out << JSON_Member{"bases"}      << '[' << self.event << ']';
        out << JSON_Member{"spots"}      << '[' << self.spots << ']';
        out << JSON_Member{"distances"}  << '[' << self.distance << ']';
        out << JSON_Member{"references"} << '[' << self.reference << ']';

        return out;
    }
};

static void gather(std::istream &in, Stats &stats, std::vector<Stats> &spotGroup) {
    in.exceptions(std::ios::badbit | std::ios::failbit);
    while (in) {
        auto const spot = Input::readFrom(in);
        int naligned = 0;

        if (spotGroup.size() < Input::groups.size())
            spotGroup.resize(Input::groups.size(), Stats{});

        for (auto const &read : spot.reads) {
            auto const bio = read.type == Input::ReadType::biological || read.type == Input::ReadType::aligned;
            auto const tech = read.type == Input::ReadType::technical;
            auto const &seq = spot.sequence.substr(read.start, read.length);

            stats.accumulateOneRead(seq, bio, tech);
            if (spot.group >= 0)
                spotGroup[spot.group].accumulateOneRead(seq, bio, tech);
            if (read.type == Input::ReadType::aligned) {
                stats.accumulateOneAlignment(read.reference, read.position, read.orientation == Input::ReadOrientation::reverse ? 1 : 0, seq, read.cigar);
                ++naligned;
            }
        }
        if (spot.reads.size() > 0 && spot.reads.size() != naligned) {
            stats.accumulate(Event::startSpot());
            if (spot.group >= 0)
                spotGroup[spot.group].accumulate(Event::startSpot());
        }
    }
}

int main(int argc, const char * argv[]) {
    // Input::runTests();

#if 1
    auto &in = std::cin;
#else
    auto in = std::istringstream("GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL, SRA_READ_TYPE_TECHNICAL\n"
                                 "GTGGACATCCCTCTGTGTGNGTCAN\tCM_000001\t10001\t0\t24M1S\n");
#endif
    Stats stats{};
    std::vector<Stats> spotGroup;

    try { gather(in, stats, spotGroup); }
    catch (...) {}

    auto out = JSON_ostream(std::cout);

    out << '{'
        << JSON_Member{"total"} << '{' << stats << '}';

    if (!spotGroup.empty()) {
        out << JSON_Member{"groups"} << '[';
        for (auto const &stats : spotGroup) {
            auto const &group = Input::groups[&stats - &spotGroup[0]];

            out << '{'
                << JSON_Member{"group"} << group
                << stats
            << '}';
        }
        out << ']';
    }
    out << '}';
    std::cout << std::endl;

    return 0;
}
