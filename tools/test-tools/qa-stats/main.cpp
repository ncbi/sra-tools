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

#include <iostream>
#include <fstream>
#include <map>
#include <functional>
#include <sstream>
#include <chrono>
#include <tuple>
#include <cmath>
#include "parameters.hpp"
#include "input.hpp"
#include "hashing.hpp"
#include "output.hpp"

JSON_ostream &operator <<(JSON_ostream &s, HashResult64 const &v) {
    char buffer[32];
    return s.insert(HashResult64::to_hex(v.value, buffer));
}

struct Event {
    unsigned
        spot: 1,
        bio: 1,
        tech: 1,
        read: 1,
        base: 1, /// not one of ACGT
        A: 1, C: 1, G: 1, T: 1;

    /// is a **w**eakly bound base, i.e. A or T
    bool isaW() const {
        return (A != 0 || T != 0);
    }
    /// is a a**m**ino base, i.e. A or C
    bool isaM() const {
        return (A != 0 || C != 0);
    }
    /// is a **s**trongly bound base, i.e. C or G
    bool isaS() const {
        return (C != 0 || G != 0);
    }
    /// is a **k**etone base, i.e. G or T
    bool isaK() const {
        return (G != 0 || T != 0);
    }
    /// Is one of `self` or `other` a **s**trongly bound base and the other a **w**eakly bound base.
    bool isSW(Event const &other) const {
        return (isaS() && other.isaW()) || (isaW() && other.isaS());
    }
    /// Is one of `self` or `other` a **k**ketone base and the other an a**m**ine base.
    bool isKM(Event const &other) const {
        return (isaK() && other.isaM()) || (isaM() && other.isaK());
    }
    bool isaACGT() const {
        return (A != 0 || C != 0 || G != 0 || T != 0);
    }
    char baseValue() const {
        return A != 0 ? 'A' : C != 0 ? 'C' : G != 0 ? 'G' : T != 0 ? 'T' : base ? 'N' : 0;
    }
    int base4na() const {
        return (A ? 1 : 0) | (C ? 2 : 0) | (G ? 4 : 0) | (T ? 8 : 0) | (base ? 0x0F : 0);
    }
    int base2na() const {
        switch (base4na()) {
        case 0:
            return -1;
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            return 4;
        }
    }
    int index() const {
        auto result = base2na();
        if (result >= 0 && (bio || tech))
            result = (result << 1) | tech;
        return result;
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
    static constexpr Event startSpot() {
        Event e = {};
        e.spot = 1;
        return e;
    }
    static constexpr Event make(bool isBio, bool isTech) {
        Event e = {};
        if (isBio)
            e.bio = 1;
        if (isTech)
            e.tech = 1;
        return e;
    }
    static constexpr Event startRead(bool isBio = false, bool isTech = false) {
        Event e = make(isBio, isTech);
        e.read = 1;
        // std::cout << "startRead: " << e << std::endl;
        return e;
    }
    static constexpr Event makeBase(char base, bool isBio = false, bool isTech = false) {
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
    char const *label;

    SpotStats(char const *lbl)
    : label(lbl)
    {}

    friend JSON_ostream &operator <<(JSON_ostream &out, SpotStats const &stats) {
        for (auto &[k, v] : stats) {
            out << '{'
                << JSON_Member{stats.label} << k
                << JSON_Member{"total"} << v.total
                << JSON_Member{"biological"} << v.biological
                << JSON_Member{"technical"} << v.technical
            << '}';
        }
        return out;
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
    static void print(JSON_ostream &out, char const *base, uint64_t bio, uint64_t tech) {
        if (bio || tech) {
            out << '{' << JSON_Member{"base"} << base;
            if (bio)
                out << JSON_Member{"biological"} << bio;
            if (tech)
                out << JSON_Member{"technical"} << tech;
            out << '}';
        }
    }
    friend JSON_ostream &operator <<(JSON_ostream &out, BaseStats const &stats) {
        auto const bioW = stats[Index('A', false)] + stats[Index('T', false)];
        auto const bioS = stats[Index('C', false)] + stats[Index('G', false)];
        auto const bioN = stats[Index('N', false)];
        auto const techW = stats[Index('A', true)] + stats[Index('T', true)];
        auto const techS = stats[Index('C', true)] + stats[Index('G', true)];
        auto const techN = stats[Index('N', true)];

        BaseStats::print(out, "N", bioN, techN);
        BaseStats::print(out, "W", bioW, techW);
        BaseStats::print(out, "S", bioS, techS);

        return out;
    }
};

struct DistanceStats {
    struct DistanceStat {
        unsigned last = 0, n = 0;
        std::vector<unsigned> counts;

        operator bool() const { return !counts.empty(); }
        void reset() {
            n = 0;
        }
        void accumulate(unsigned position) {
            if (n > 0 && position > last) {
                auto index = (position - last) - 1;
                if (counts.size() <= index)
                    counts.resize(index + 1, 0);
                counts[index] += 1;
            }
            last = position;
            ++n;
        }
        unsigned operator[](decltype(counts)::size_type index) const {
            return index < counts.size() ? counts[index] : 0;
        }
        static void print(JSON_ostream &out, DistanceStat const &self) {
            auto const n = self.counts.size();
            uint64_t total = 0;
            for (auto i = decltype(counts)::size_type(0); i < n; ++i) {
                total += self[i];
            }
            for (auto i = decltype(counts)::size_type(0); i < n; ++i) {
                auto const sum = self[i];
                if (sum == 0) continue;

                out << '{'
                    << JSON_Member{"wavelength"} << i + 1
                    << JSON_Member{"power"} << double(sum)/total
                << '}';
            }
        }
        static void print(JSON_ostream &out, DistanceStat const &self, DistanceStat const &complement) {
            auto const n = std::max(self.counts.size(), complement.counts.size());
            uint64_t total = 0;
            for (auto i = decltype(counts)::size_type(0); i < n; ++i) {
                total += self[i] + complement[i];
            }
            for (auto i = decltype(counts)::size_type(0); i < n; ++i) {
                auto const sum = self[i] + complement[i];
                if (sum == 0) continue;

                out << '{'
                    << JSON_Member{"wavelength"} << i + 1
                    << JSON_Member{"power"} << double(sum)/total
                << '}';
            }
        }
    };
    DistanceStat A, C, G, T, SW, KM;
    Event last = {};
    int maxpos = -1;

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
        last = {};
        maxpos = -1;
    }
    void accumulate(char base, int pos) {
        if (pos < 0) {
            reset();
            return;
        }

        if (maxpos < pos)
            maxpos = pos;

        switch (base) {
        case 'A':
            A.accumulate(pos);
            break;
        case 'C':
            C.accumulate(pos);
            break;
        case 'G':
            G.accumulate(pos);
            break;
        case 'T':
            T.accumulate(pos);
            break;
        default:
            reset();
            return;
        }
        auto const next = Event::makeBase(base);
        if (last.isSW(next))
            SW.accumulate(pos);
        if (last.isKM(next))
            KM.accumulate(pos);
        last = next;
    }

    friend JSON_ostream &operator <<(JSON_ostream &out, DistanceStats const &distance) {
        if (distance) {
            out << JSON_Member{"W"} << '[';
                DistanceStat::print(out, distance.A, distance.T);
            out << ']'
            << JSON_Member{"S"} << '[';
                DistanceStat::print(out, distance.C, distance.G);
            out << ']';
            out << JSON_Member{"S-W"} << '[';
                DistanceStat::print(out, distance.SW);
            out << ']';
            out << JSON_Member{"K-M"} << '[';
                DistanceStat::print(out, distance.KM);
            out << ']';
        }
        return out;
    }
};

struct ReferenceStats : public std::vector<std::vector<Count>> {
    static constexpr int chunkSize = (1 << 14);

    void accumulate(int refId, int position, int strand, std::string const &sequence, CIGAR const &cigar) {
        auto const ps = (uint64_t(position) << 1) | (strand & 1);

        while (refId >= size()) {
            auto value = std::vector<Count>();
            value.push_back(Count());
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

            if (reference[0].total == 0)
                reference[0].total = 0xcbf29ce484222325ull % 0x1000001b3ull;
            reference[0].total = (reference[0].total * ps) % 0x1000001b3ull;
            reference[0].biological ^= SeqHash::hash(sequence.size(), sequence.data());
            reference[0].technical ^= cigar.hashValue();
        }
    }
    friend JSON_ostream &operator <<(JSON_ostream &out, ReferenceStats const &self) {
        for (auto &reference : self) {
            auto const refId = &reference - &self[0];
            auto const refName = Input::references[refId];
            Count total;
            auto const &posHash = reference[0].total;
            auto const &seqHash = reference[0].biological;
            auto const &cigHash = reference[0].technical;

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
            out << ']'
            << JSON_Member{"alignments"} << '{'
                << JSON_Member{"total"} << total.total
                << JSON_Member{"5'"} << total.biological
                << JSON_Member{"3'"} << total.technical
            << '}'
            << JSON_Member{"position"} << HashResult64{posHash}
            << JSON_Member{"sequence"} << HashResult64{seqHash}
            << JSON_Member{"cigar"} << HashResult64{cigHash}
            << '}';
        }
        return out;
    }
};

struct SpotLayout : public std::string {
    Count numreads;
    SpotLayout(Input const &spot) {
        for (auto & read : spot.reads) {
            std::string desc = std::to_string(read.length);
            char bt = '.', fr = '.';
            if (read.type == Input::ReadType::biological || read.type == Input::ReadType::aligned) {
                numreads.biological += 1;
                bt = 'B';

                if (read.orientation == Input::ReadOrientation::forward)
                    fr = 'F';
                else if (read.orientation == Input::ReadOrientation::reverse)
                    fr = 'R';
            }
            else if (read.type == Input::ReadType::technical) {
                numreads.technical += 1;
                bt = 'T';
            }

            desc += char(bt);
            desc += char(fr);

            numreads.total += 1;
            append(desc);
        }
    }
};

struct SpotLayouts {
    std::vector<uint64_t> count;
    std::map<std::string, unsigned> index;

    uint64_t &operator [](SpotLayout const &layout) {
        auto const at = index.find(layout);
        if (at != index.end())
            return count[at->second];

        auto const i = (unsigned)count.size();
        count.push_back(0);
        index[layout] = i;
        return count[i];
    }
    friend JSON_ostream &operator <<(JSON_ostream &out, SpotLayouts const &value) {
        for (auto i = value.index.begin(); i != value.index.end(); ++i) {
            auto desc = i->first;
            auto count = value.count[i->second];
            out << '{'
                << JSON_Member{"descriptor"} << desc
                << JSON_Member{"count"} << count
            << '}';
        }
        return out;
    }
};

struct Stats {
    Count reads; // read counts
    SpotStats spots; // spot counts by number of reads
    BaseStats event;
    SpotLayouts layouts;

    DistanceStats distance;
    ReferenceStats reference;

    Stats()
    : spots("nreads")
    {}

    void accumulate(char base, bool tech, int32_t readpos) {
        event[BaseStats::Index(base, tech)] += 1;
        distance.accumulate(base, readpos);
    }
    void accumulateOneSpot(SpotLayout const &desc) {
        auto const nreads_bio = desc.numreads.biological;
        auto const nreads_tech = desc.numreads.technical;
        auto const nreads = desc.numreads.total;

        if (nreads_bio) {
            spots[(unsigned)nreads_bio].biological += 1;
            reads.biological += nreads_bio;
        }
        if (nreads_tech) {
            spots[(unsigned)nreads_tech].technical += 1;
            reads.technical += nreads_tech;
        }
        if (nreads) {
            spots[(unsigned)nreads].total += 1;
            reads.total += nreads;
        }
        layouts[desc] += 1;
    }
    void accumulateOneRead(std::string_view const &seq, Input::ReadType type) {
        auto const bio = type == Input::ReadType::biological || type == Input::ReadType::aligned;
        auto const tech = type == Input::ReadType::technical;
        unsigned pos = 0;
        auto const hash = SeqHash::hash(seq.size(), seq.data());

        distance.reset();
        for (auto && ch : seq) {
            accumulate(ch, !bio, pos++);
        }
    }
    void accumulateOneAlignment(int refId, int position, int strand, std::string const &sequence, CIGAR const &cigar) {
        reference.accumulate(refId, position, strand, sequence, cigar);
    }
    struct ReadsInfo {
        decltype(Stats::reads) const &reads;

        ReadsInfo(Stats const &stats)
        : reads(stats.reads)
        {}

        friend JSON_ostream &operator <<(JSON_ostream &out, ReadsInfo const &self) {
            out << JSON_Member{"count"} << self.reads.total
                << JSON_Member{"biological"} << self.reads.biological
                << JSON_Member{"technical"} << self.reads.technical;
            return out;
        }
    };
    ReadsInfo readsInfo() const { return ReadsInfo(*this); }

    friend JSON_ostream &operator <<(JSON_ostream &out, Stats const &self) {
        out << JSON_Member{"reads"}        << '{' << self.readsInfo() << '}';
        out << JSON_Member{"bases"}        << '[' << self.event << ']';
        out << JSON_Member{"spots"}        << '[' << self.spots << ']';
        out << JSON_Member{"spot-layouts"} << '[' << self.layouts << ']';
        out << JSON_Member{"spectra"}      << '{' << self.distance << '}';
        out << JSON_Member{"references"}   << '[' << self.reference << ']';

        return out;
    }
};

struct Reporter {
private:
    using Clock = decltype(std::chrono::steady_clock::now());
    using Duration = Clock::duration;
    Clock start;
    Duration freq;
    Clock nextReport;
    uint64_t threshold;
    static Clock now() {
        return std::chrono::steady_clock::now();
    }
public:
    Reporter(int seconds = 0)
    : start(now())
    , freq(seconds ? Duration(std::chrono::seconds(seconds)) : Duration::zero())
    , nextReport(start + freq)
    , threshold(2)
    {}
    void setFrequency(int seconds) {
        nextReport = now() + (freq = Duration(std::chrono::seconds(seconds)));
    }
    template <typename T>
    double report(T const &progress, Clock const &now = Reporter::now()) const {
        std::chrono::duration<double> const elapsed = now - start;
        if (progress == 0 || elapsed.count() < 1)
            return 0;
        auto const rps = progress / elapsed.count();
        std::cerr << "progress: records per second: " << rps << std::endl;
        return rps;
    }
    template <typename T>
    void update(T const &progress) {
        if (freq == Duration::zero()) return;
        if (progress < threshold) return;
        auto const rps = report(progress);
        if (rps > 1) {
            threshold = progress + rps * std::chrono::duration_cast<std::chrono::seconds>(freq).count();
        }
        return;
        auto n = now();
        if (n >= nextReport) {
            nextReport = n + freq;
            auto const rps = report(progress, n);
            if (rps > 1) {
                threshold = progress + (1 << int(log2(rps)));
            }
        }
    }
};

struct App {
    App(int argc, char *argv[])
    : arguments(argc, argv, {
        { "progress", "p", "60" },
        { "multithreaded", "t", "1" },
        { "mmap", "m", "1" },
        { "output", "o", nullptr, true }
    })
    , nextInput(arguments.begin())
    , currentInput(arguments.end())
    {
        for (auto const &[param, value] : arguments.parameters) {
            if (param == "progress" && value.has_value()) {
                reporter.setFrequency(std::stoi(value.value()));
                continue;
            }
            if (param == "multithreaded" && value.has_value()) {
                multithreaded = std::stoi(value.value());
                continue;
            }
            if (param == "mmap" && value.has_value()) {
                use_mmap = std::stoi(value.value()) != 0;
                continue;
            }
            if (param == "output") {
                if (value) {
                    output = value;
                    continue;
                }
                std::cerr << "error: output parameter needs a path" << std::endl;
                exit(1);
            }
            if (param == "help") {
                std::cout << "usage: " << arguments.program << " [-p|--progress <seconds:=60>] [-t|--multithreaded] [-m|--mmap] [-o|--output <path>] [<path> ...]" << std::endl;
                exit(0);
            }
            std::cerr << "error: Unrecognized parameter " << param << std::endl;
            exit(1);
        }
    }
    int run() {
        while (processNextInput())
            ;
        if (output) {
            auto const &path = output.value();
            auto strm = std::ofstream(path.c_str());
            if (strm)
                print(strm);
            else
                std::cerr << "couldn't open " << path << std::endl;
        }
        else {
            print(std::cout);
        }
        return 0;
    }
private:
    bool processNextInput() {
        gather();
        return !(arguments.empty() || nextInput == arguments.end());
    }
    void print(std::ostream &strm) {
        auto out = JSON_ostream(strm);

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
        reporter.report(processed);
    }
    void gather() {
        auto source = Input::newSource(inputStream(), multithreaded);
        while (*source) {
            try {
                auto const spot = source->get();
                int naligned = 0;

                if (spotGroup.size() < Input::groups.size())
                    spotGroup.resize(Input::groups.size(), Stats{});

                auto const group = spot.group >= 0 ? &spotGroup[spot.group] : nullptr;

                for (auto const &read : spot.reads) {
                    if (read.type == Input::ReadType::aligned && !read.cigar) {
                        // This is the sequence record for an aligned read.
                        // The sequence and alignment details have/will be handled
                        // by the alignment record.
                        continue;
                    }
                    auto const seq = spot.sequence.substr(read.start, read.length);

                    stats.accumulateOneRead(seq, read.type);
                    if (group)
                        group->accumulateOneRead(seq, read.type);
                    if (read.type == Input::ReadType::aligned) {
                        stats.accumulateOneAlignment(read.reference, read.position, read.orientation == Input::ReadOrientation::reverse ? 1 : 0, seq, read.cigar);
                        if (group)
                            group->accumulateOneAlignment(read.reference, read.position, read.orientation == Input::ReadOrientation::reverse ? 1 : 0, seq, read.cigar);
                        ++naligned;
                    }
                }
                if (spot.reads.size() > 0 && spot.reads.size() != naligned) {
                    SpotLayout const &layout(spot);
                    stats.accumulateOneSpot(layout);
                    if (group)
                        group->accumulateOneSpot(layout);
                }
                reporter.update(++processed);
            }
            catch (std::ios_base::failure const &e) {
                if (!source->eof())
                    throw e;
                return;
            }
        }
    }
    Input::Source::Type inputStream() {
        if (arguments.empty())
            return Input::Source::StdInType();

        currentInput = nextInput++;
        return Input::Source::FilePathType{currentInput->c_str(), use_mmap};
    }
    uint64_t processed = 0;
    int multithreaded = 0;
    bool use_mmap = false;
    std::optional<std::string> output;

    Stats stats;
    std::vector<Stats> spotGroup;

    Reporter reporter;

    Arguments arguments;
    Arguments::const_iterator nextInput;
    Arguments::const_iterator currentInput;
};

int main(int argc, char * argv[]) {
    Input::runTests();
    SeqHash::test();
    
    auto app = App(argc, argv);

    return app.run();
}

