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

struct CountBT {
    uint64_t total, biological, technical;
    CountBT() : total(0), biological(0), technical(0) {}

    CountBT &operator +=(CountBT const &addend) {
        total += addend.total;
        biological += addend.biological;
        technical += addend.technical;
        return *this;
    }
    friend JSON_ostream &operator <<(JSON_ostream &out, CountBT const &value) {
        return out
            << JSON_Member{"count"} << value.total
            << JSON_Member{"biological"} << value.biological
            << JSON_Member{"technical"} << value.technical;
    }
};

struct CountFR : public CountBT {
    void addStrand(bool rev) {
        total += 1;
        if (rev)
            technical += 1;
        else
            biological += 1;
    }
    friend JSON_ostream &operator <<(JSON_ostream &out, CountFR const &value) {
        return out
            << JSON_Member{"total"} << value.total
            << JSON_Member{"5'"} << value.biological
            << JSON_Member{"3'"} << value.technical;
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
        using Index = decltype(counts)::size_type;

        operator bool() const { return !counts.empty(); }
        void reset() {
            n = 0;
        }
        void record(unsigned position) {
            if (n > 0 && position > last) {
                auto index = (position - last) - 1;
                if (counts.size() <= index)
                    counts.resize(index + 1, 0);
                counts[index] += 1;
            }
            last = position;
            ++n;
        }
        unsigned operator[](Index index) const {
            return index < counts.size() ? counts[index] : 0;
        }
        /// Used for already binned stats, like K-M and S-W.
        static void print(JSON_ostream &out, DistanceStat const &self) {
            auto const n = self.counts.size();
            uint64_t total = 0;
            for (auto i = Index(0); i < n; ++i) {
                total += self[i];
            }
            for (auto i = Index(0); i < n; ++i) {
                auto const sum = self[i];
                if (sum == 0) continue;

                out << '{'
                    << JSON_Member{"wavelength"} << i + 1
                    << JSON_Member{"power"} << double(sum)/total
                << '}';
            }
        }
        /// Used for base-to-base stats, bins complementary pairs.
        static void print(JSON_ostream &out, DistanceStat const &self, DistanceStat const &complement) {
            auto const n = std::max(self.counts.size(), complement.counts.size());
            uint64_t total = 0;
            for (auto i = Index(0); i < n; ++i) {
                total += self[i] + complement[i];
            }
            for (auto i = Index(0); i < n; ++i) {
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
    char last = 0;
    int maxpos = -1;

    bool isS2W(char base) {
        return (last == 'C' || last == 'G') && (base == 'A' || base == 'T');
    }
    bool isW2S(char base) {
        return (last == 'A' || last == 'T') && (base == 'C' || base == 'G');
    }
    bool isSW(char base) {
        return isS2W(base) || isW2S(base);
    }
    bool isK2M(char base) {
        return (last == 'G' || last == 'T') && (base == 'A' || base == 'C');
    }
    bool isM2K(char base) {
        return (last == 'A' || last == 'C') && (base == 'G' || base == 'T');
    }
    bool isKM(char base) {
        return isK2M(base) || isM2K(base);
    }
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

struct ReferenceStats : public std::vector<std::vector<CountFR>> {
    static constexpr unsigned chunkSize = (1u << 14);

    void record(unsigned refId, unsigned position, unsigned strand, std::string const &sequence, CIGAR const &cigar) {
        auto const ps = (uint64_t(position) << 1) | (strand & 1);

        while (refId >= size()) {
            auto value = std::vector<CountFR>();
            value.push_back(CountFR());
            push_back(value);
        }
        auto &reference = (*this)[refId];
        if (position >= 0) {
            auto const chunk = 1 + position / chunkSize;

            while (chunk >= reference.size())
                reference.push_back(CountFR{});

            reference[chunk].addStrand(strand != 0);

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
            CountFR total;
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
                    << JSON_Member{"alignments"} << '{' << counter << '}'
                << '}';
            }
            out << ']'
            << JSON_Member{"alignments"} << '{' << total << '}'
            << JSON_Member{"position"} << HashResult64{posHash}
            << JSON_Member{"sequence"} << HashResult64{seqHash}
            << JSON_Member{"cigar"} << HashResult64{cigHash}
            << '}';
        }
        return out;
    }
};

struct SpotLayout : public std::string {
    unsigned nreads, biological, technical;

    SpotLayout(Input const &spot)
    : nreads(0), biological(0), technical(0)
    {
        for (auto & read : spot.reads) {
            std::string desc = std::to_string(read.length);
            char bt = '.', fr = '.';
            if (read.type == Input::ReadType::biological || read.type == Input::ReadType::aligned) {
                biological += 1;
                bt = 'B';

                if (read.orientation == Input::ReadOrientation::forward)
                    fr = 'F';
                else if (read.orientation == Input::ReadOrientation::reverse)
                    fr = 'R';
            }
            else if (read.type == Input::ReadType::technical) {
                technical += 1;
                bt = 'T';
            }

            desc += char(bt);
            desc += char(fr);

            nreads += 1;
            append(desc);
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
    friend JSON_ostream &operator <<(JSON_ostream &out, SpotLayouts const &value) {
        for (auto i = value.index.begin(); i != value.index.end(); ++i) {
            auto const &desc = i->first;
            auto const &count = value.count[i->second];
            out << '{'
                << JSON_Member{"descriptor"} << desc
                << JSON_Member{"count"} << count
            << '}';
        }
        return out;
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
    friend JSON_ostream &operator <<(JSON_ostream &out, CountBTN const &value) {
        return out
            << JSON_Member{"total"} << value.total
            << JSON_Member{"biological"} << value.biological
            << JSON_Member{"no-biological"} << value.nobiological
            << JSON_Member{"technical"} << value.technical
            << JSON_Member{"no-technical"} << value.notechnical;
    }
};

struct SpotStats : public std::map<unsigned, CountBTN> {
    friend JSON_ostream &operator <<(JSON_ostream &out, SpotStats const &stats) {
        for (auto &[k, v] : stats) {
            out << '{'
                << JSON_Member{"nreads"} << k
                << v
            << '}';
        }
        return out;
    }
};

struct Stats {
    CountBT reads; // read counts
    SpotStats spots; // spot counts by number of reads
    BaseStats bases;
    SpotLayouts layouts;
    DistanceStats spectra;
    ReferenceStats references;

    void record(SpotLayout const &desc, Stats *group = nullptr) {
        reads.biological += desc.biological;
        reads.technical += desc.technical;
        reads.total += desc.nreads;

        spots[desc.nreads] += desc;

        layouts[desc] += 1;

        if (group)
            group->record(desc);
    }
    void record(std::string_view const &seq, Input::ReadType type, Stats *group = nullptr) {
        auto const bio = type == Input::ReadType::biological || type == Input::ReadType::aligned;
        unsigned pos = 0;

        for (auto && ch : seq) {
            bases[BaseStats::Index(ch, !bio)] += 1;
            spectra.record(ch, pos++);
        }
        spectra.reset();

        if (group)
            group->record(seq, type);
    }
    void record(int refId, int position, int strand, std::string const &sequence, CIGAR const &cigar, Stats *group = nullptr) {
        references.record(refId, position, strand, sequence, cigar);
        if (group)
            group->record(refId, position, strand, sequence, cigar);
    }

    friend JSON_ostream &operator <<(JSON_ostream &out, Stats const &self) {
        out << JSON_Member{"reads"}        << '{' << self.reads      << '}';
        out << JSON_Member{"bases"}        << '[' << self.bases      << ']';
        out << JSON_Member{"spots"}        << '[' << self.spots      << ']';
        out << JSON_Member{"spot-layouts"} << '[' << self.layouts    << ']';
        out << JSON_Member{"spectra"}      << '{' << self.spectra    << '}';
        out << JSON_Member{"references"}   << '[' << self.references << ']';

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
                unsigned naligned = 0;

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

                    stats.record(seq, read.type, group);
                    if (read.type == Input::ReadType::aligned) {
                        assert(read.reference >= 0);
                        assert(read.position >= 0);
                        unsigned const strand = read.orientation == Input::ReadOrientation::reverse ? 1 : 0;
                        stats.record(read.reference, read.position, strand, seq, read.cigar, group);
                        ++naligned;
                    }
                }
                if (spot.reads.size() > 0 && spot.reads.size() != naligned) {
                    auto const &layout = SpotLayout(spot);
                    stats.record(layout, group);
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
/*
    Input::runTests();
    SeqHash::test();
 */
    auto app = App(argc, argv);

    return app.run();
}

