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
 *  Collect Statistics.
 */

#pragma once

#ifndef stats_hpp
#define stats_hpp

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <string_view>
#include <fingerprint.hpp>
#include <hashing.hpp>

struct SeqHash_impl {
    using State = FNV1a;
    using Value = FNV1a::Value;
    static State init() { return State{}; }
    static Value finalize(State &state) {
        return state.state;
    }
    static void update(State &state, size_t size, char const *seq) {
        auto const end = seq + size;

        for (auto i = seq; i != end; ++i) {
            auto ch = *i;
            switch (ch) {
            case 'A':
            case 'C':
            case 'G':
            case 'T':
                break;
            default:
                ch = 'N';
                break;
            }
            state.update<char>(ch);
        }
    }
    static void test() {
#ifndef NDEBUG
#endif
    }
};
using SeqHash = struct HashFunction<SeqHash_impl>;

struct CountBT {
    uint64_t total, biological, technical;
    CountBT() : total(0), biological(0), technical(0) {}

    CountBT &operator +=(CountBT const &addend) {
        total += addend.total;
        biological += addend.biological;
        technical += addend.technical;
        return *this;
    }
};

struct CountFR {
    uint64_t total, fwd, rev;
    CountFR() : total(0), fwd(0), rev(0) {}
    void addStrand(bool reverse) {
        total += 1;
        if (reverse)
            rev += 1;
        else
            fwd += 1;
    }
    CountFR &operator +=(CountFR const &addend) {
        total += addend.total;
        fwd += addend.fwd;
        rev += addend.rev;
        return *this;
    }
};

struct BaseStats {
    struct Index {
    private:
        unsigned index;
        friend struct BaseStats;
    public:
        Index(char base, bool tech) {
            switch (base) {
            case 'A':
                index = 0;
                break;
            case 'C':
                index = 2;
                break;
            case 'G':
                index = 4;
                break;
            case 'T':
                index = 6;
                break;
            default:
                index = 8;
                break;
            }
            if (tech)
                index |= 1;
        }
    };
    std::vector<uint64_t> counts{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint64_t &operator [](Index const &i) {
        return counts[i.index];
    }
    uint64_t operator [](Index const &i) const {
        return counts[i.index];
    }
};

struct DistanceStats {
    struct DistanceStat {
        std::vector<uint64_t> counts;
        uint64_t n = 0;
        unsigned last = 0;
        using Index = decltype(counts)::size_type;

        operator bool() const { return !counts.empty(); }
        Index size() const { return counts.size(); }
        void reset() {
            n = 0;
        }
        void record(unsigned const position) {
            if (n > 0 && position > last) {
                auto index = (position - last) - 1;
                if (counts.size() <= index)
                    counts.resize(index + 1, 0);
                counts[index] += 1;
            }
            last = position;
            ++n;
        }
        uint64_t operator[](Index const index) const {
            return index < counts.size() ? counts[index] : 0;
        }
        uint64_t total() const {
            uint64_t result = 0;
            for (auto v : counts)
                result += v;
            return result;
        }

        template < typename FUNC >
        void forEach(FUNC && func) const {
            auto const n = size();
            auto const sum = total();
            for (auto i = Index{ 0 }; i < n; ++i)
                func(i, (*this)[i], sum);
        }

        template < typename FUNC >
        void forEach(DistanceStat const &complement, FUNC && func) const {
            auto const n = std::max(size(), complement.size());
            auto const sum = total() + complement.total();
            for (auto i = Index{ 0 }; i < n; ++i)
                func(i, (*this)[i] + complement[i], sum);
        }
    };
    DistanceStat A, C, G, T, SW, KM;
    char last = 0;
    int maxpos = -1;

    /// is `C` or `G` (a `S`trong base with 3 H bonds).
    bool isS(char base) const {
        return (base == 'C' || base == 'G');
    }
    /// is `A` or `T` (a `W`eak base with 2 H bonds).
    bool isW(char base) const {
        return (base == 'A' || base == 'T');
    }
    /// is `S` to `W` transition.
    bool isS2W(char base) const {
        return isS(last) && isW(base);
    }
    /// is `W` to `S` transition.
    bool isW2S(char base) const {
        return isW(last) && isS(base);
    }
    /// is `S` to `W` or `W` to `S` transition.
    bool isSW(char base) const {
        return isS2W(base) || isW2S(base);
    }
    /// is `A` or `C` (an a`M`ino base)
    bool isM(char base) const {
        return (base == 'A' || base == 'C');
    }
    /// is `G` or `T` (a `K`etone base)
    bool isK(char base) const {
        return (base == 'G' || base == 'T');
    }
    /// is `K` to `M` transition
    bool isK2M(char base) const {
        return isK(last) && isM(base);
    }
    /// is `M` to `K` transition
    bool isM2K(char base) const {
        return isM(last) && isK(base);
    }
    /// is `K` to `M` or `M` to `K` transition
    bool isKM(char base) const {
        return isK2M(base) || isM2K(base);
    }
    /// is any counter > 0?
    operator bool() const {
        return A || C || G || T || SW || KM;
    }
    void reset() {
        A.reset();
        C.reset();
        G.reset();
        T.reset();
        SW.reset();
        KM.reset();
        last = 0;
        maxpos = -1;
    }
    void record(char base, int pos) {
        if (pos < 0) {
            reset();
            return;
        }

        if (maxpos < pos)
            maxpos = pos;

        switch (base) {
        case 'A':
            A.record(pos);
            break;
        case 'C':
            C.record(pos);
            break;
        case 'G':
            G.record(pos);
            break;
        case 'T':
            T.record(pos);
            break;
        default:
            reset();
            return;
        }
        if (isSW(base))
            SW.record(pos);
        if (isKM(base))
            KM.record(pos);
        last = base;
    }
};

struct ReferenceStats : public std::vector<std::vector<CountFR>> {
    static constexpr unsigned chunkSize = (1u << 14);

    void record(unsigned refId, unsigned position, unsigned strand, std::string const &sequence, CIGAR const &cigar) {
        auto const ps = (uint64_t{position} << 1) | (strand & 1); ///< position + strand

        while (refId >= size()) {
            auto value = std::vector<CountFR>();
            value.push_back(CountFR());
            push_back(value);
        }
        auto &&reference = (*this)[refId];
        if (position >= 0) {
            auto const chunk = 1 + position / chunkSize;

            while (chunk >= reference.size())
                reference.push_back(CountFR{});

            reference[chunk].addStrand(strand != 0);

            if (reference[0].total == 0)
                reference[0].total = 0xcbf29ce484222325ull % 0x1000001b3ull;
            reference[0].total = (reference[0].total * ps) % 0x1000001b3ull;
            reference[0].fwd ^= SeqHash::hash(sequence.size(), sequence.data());
            reference[0].rev ^= cigar.hashValue();
        }
    }
};

struct SpotLayouts {
    using Key = std::string;
    std::vector<uint64_t> count;
    std::map<Key, unsigned> index;

    uint64_t &operator [](Key const &key) {
        auto const at = index.find(key);
        if (at != index.end())
            return count[at->second];

        auto const i = (unsigned)count.size();
        count.push_back(0);
        index[key] = i;
        return count[i];
    }
};

struct SpotLayout : public std::string {
    unsigned nreads = 0;
    unsigned biological = 0;
    unsigned technical = 0;

    static char const *description(char buffer[16], unsigned length, bool biological, bool technical, bool forward, bool reverse)
    {
        auto cp = buffer + 16;
        *--cp = '\0';

        auto fr = --cp; *fr = '_';
        auto bt = --cp; *bt = '_';

        if (biological) {
            *bt = 'B';
            if (forward)
                *fr = 'F';
            else if (reverse)
                *fr = 'R';
        }
        else if (technical)
            *bt = 'T';

        do {
            *--cp = '0' + length % 10;
            length /= 10;
        } while (length > 0);

        return cp;
    }
    void appendRead(unsigned len, bool bio, bool tec, bool fwd, bool rev)
    {
        char buffer[16];
        append(description(buffer, len, bio, tec, fwd, rev));
        if (bio)
            biological += 1;
        if (tec)
            technical += 1;
        nreads += 1;
    }
};

struct CountBTN {
    uint64_t total = 0, biological = 0, technical = 0, nobiological = 0, notechnical = 0;

    CountBTN &operator +=(SpotLayout const &layout) {
        auto &bio = layout.biological == 0 ? nobiological : biological;
        auto &tech = layout.technical == 0 ? notechnical : technical;

        total += 1;
        bio += 1;
        tech += 1;
        return *this;
    }
};

struct SpotStats : public std::map<unsigned, CountBTN> {
};

struct Stats {
    CountBT reads; // read counts
    SpotStats spots; // spot counts by number of reads
    BaseStats bases;
    SpotLayouts layouts;
    DistanceStats spectra;
    ReferenceStats references;
    Fingerprint fingerprint = Fingerprint{};

    /// Record reference alignment stats.
    void record(int refId, int position, int strand, std::string const &sequence, CIGAR const &cigar, Stats *group = nullptr)
    {
        references.record(refId, position, strand, sequence, cigar);
        if (group)
            group->record(refId, position, strand, sequence, cigar);
    }

    /// Record sequence stats.
    void record(std::string const &sequence, bool biological, Stats* group = nullptr) {
        unsigned pos = 0;
        for (auto && ch : sequence) {
            bases[BaseStats::Index{ ch, !biological }] += 1;
            spectra.record(ch, pos);
            fingerprint.record(ch, pos);
            ++pos;
        }
        fingerprint.recordEnd(pos);
        spectra.reset();

        if (group)
            group->record(sequence, biological);
    }

    /// Record spot layout stats.
    void record(SpotLayout const &desc, Stats *group = nullptr) {
        reads.biological += desc.biological;
        reads.technical += desc.technical;
        reads.total += desc.nreads;

        spots[desc.nreads] += desc;

        layouts[desc] += 1;

        if (group)
            group->record(desc);
    }
};

#endif /* stats_hpp */
