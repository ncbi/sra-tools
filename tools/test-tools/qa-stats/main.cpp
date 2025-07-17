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
#include <functional>
#include <sstream>
#include <chrono>
#include <tuple>
#include <cmath>
#include <cassert>
#include <JSON_ostream.hpp>
#include <hashing.hpp>
#include "parameters.hpp"
#include "input.hpp"
#include "stats.hpp"
#include "../../../shared/toolkit.vers.h"

static inline
JSON_ostream &operator <<(JSON_ostream &s, HashResult64 const &v) {
    char buffer[32];
    return s << HashResult64::to_hex(v.value, buffer);
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, CountBT const &value) {
    return out
        << JSON_Member{"count"} << value.total
        << JSON_Member{"biological"} << value.biological
        << JSON_Member{"technical"} << value.technical;
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, CountFR const &value) {
    return out
        << JSON_Member{"total"} << value.total
        << JSON_Member{"5'"} << value.fwd
        << JSON_Member{"3'"} << value.rev;
}

struct BaseStatsEntry {
    char const *base;
    uint64_t bio, tech;

    friend
    JSON_ostream &operator <<(JSON_ostream &out, BaseStatsEntry const &self) {
        if (self.bio || self.tech) {
            out << '{' << JSON_Member{"base"} << self.base;
            if (self.bio)
                out << JSON_Member{"biological"} << self.bio;
            if (self.tech)
                out << JSON_Member{"technical"} << self.tech;
            out << '}';
        }
        return out;
    }
};

static inline
JSON_ostream &operator <<(JSON_ostream &out, BaseStats const &stats) {
    using Index = BaseStats::Index;
    auto const bioW = stats[Index{'A', false}] + stats[Index{'T', false}];
    auto const bioS = stats[Index{'C', false}] + stats[Index{'G', false}];
    auto const bioN = stats[Index{'N', false}];
    auto const techW = stats[Index{'A', true}] + stats[Index{'T', true}];
    auto const techS = stats[Index{'C', true}] + stats[Index{'G', true}];
    auto const techN = stats[Index{'N', true}];

    return out
        << BaseStatsEntry{"N", bioN, techN}
        << BaseStatsEntry{"W", bioW, techW}
        << BaseStatsEntry{"S", bioS, techS};
}

struct DistanceStatEntry {
    double power;
    unsigned length;

    DistanceStatEntry(DistanceStats::DistanceStat::Index i, uint64_t count, uint64_t total)
    : power(double(count)/total)
    , length((unsigned)i)
    {}

    friend
    JSON_ostream &operator <<(JSON_ostream &out, DistanceStatEntry const &self) {
        return out
        << '{'
            << JSON_Member{"length"} << self.length
            << JSON_Member{"power"} << self.power
        << '}';
    }
};

static inline
JSON_ostream &operator <<(JSON_ostream &out, DistanceStats::DistanceStat const &self) {
    using Index = DistanceStats::DistanceStat::Index;
    self.forEach([&](Index i, uint64_t count, uint64_t total) {
        if (count > 0)
            out << DistanceStatEntry{ i, count, total };
    });
    return out;
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, std::pair < DistanceStats::DistanceStat const *, DistanceStats::DistanceStat const * > const &self)
{
    using Index = DistanceStats::DistanceStat::Index;
    self.first->forEach(*self.second, [&](Index i, uint64_t count, uint64_t total) {
        if (count > 0)
            out << DistanceStatEntry{ i, count, total };
    });
    return out;
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, DistanceStats const &distance) {
    if (distance) {
        out << JSON_Member{"W"} << '[' << std::make_pair(&distance.A, &distance.T) << ']';
        out << JSON_Member{"S"} << '[' << std::make_pair(&distance.C, &distance.G) << ']';
        out << JSON_Member{"S-W"} << '[' << distance.SW << ']';
        out << JSON_Member{"K-M"} << '[' << distance.KM << ']';
    }
    return out;
}

static
JSON_ostream &operator <<(JSON_ostream &out, ReferenceStats const &self) {
    for (auto &reference : self) {
        auto const refId = &reference - &self[0];
        auto const refName = Input::references[refId];
        CountFR total;
        auto const &posHash = reference[0].total;
        auto const &seqHash = reference[0].fwd;
        auto const &cigHash = reference[0].rev;

        out << '{'
            << JSON_Member{"name"} << refName
            << JSON_Member{"chunks"} << '[';
        for (auto &counter : reference) {
            if (&counter == &reference[0] || counter.total == 0) continue;

            auto const chunk = (&counter - &reference[0]) - 1;
            total += counter;

            out
            << '{'
                << JSON_Member{"start"} << chunk * ReferenceStats::chunkSize + 1
                << JSON_Member{"last"} << (chunk + 1) * ReferenceStats::chunkSize
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

static JSON_ostream &operator <<(JSON_ostream &out, SpotLayouts const &value) {
    for (auto i = value.index.begin(); i != value.index.end(); ++i) {
        out << '{'
            << JSON_Member{"descriptor"} << i->first
            << JSON_Member{"count"} << value.count[i->second]
        << '}';
    }
    return out;
}

static JSON_ostream &operator <<(JSON_ostream &out, CountBTN const &value) {
    return out
        << JSON_Member{"total"} << value.total
        << JSON_Member{"biological"} << value.biological
        << JSON_Member{"no-biological"} << value.nobiological
        << JSON_Member{"technical"} << value.technical
        << JSON_Member{"no-technical"} << value.notechnical;
}

static
JSON_ostream &operator <<(JSON_ostream &out, SpotStats const &stats) {
    for (auto &[k, v] : stats) {
        out << '{'
            << JSON_Member{"nreads"} << k
            << v
        << '}';
    }
    return out;
}

static JSON_ostream &operator <<(JSON_ostream &out, Stats const &self) {
    out << JSON_Member{"reads"}        << '{' << self.reads      << '}';
    out << JSON_Member{"bases"}        << '[' << self.bases      << ']';
    out << JSON_Member{"spots"}        << '[' << self.spots      << ']';
    out << JSON_Member{"spot-layouts"} << '[' << self.layouts    << ']';
    out << JSON_Member{"spectra"}      << '{' << self.spectra    << '}';
    out << JSON_Member{"references"}   << '[' << self.references << ']';
    // fingerprint is only output separately
    // out << JSON_Member{"fingerprint"}  << '[' << self.fingerprint << ']';

    return out;
}

static
SpotLayout spotLayout(Input const &spot) {
    SpotLayout result;

    for (auto && read : spot.reads) {
        result.appendRead(read.length
                          , read.type == Input::ReadType::biological || read.type == Input::ReadType::aligned
                          , read.type == Input::ReadType::technical
                          , read.orientation == Input::ReadOrientation::forward
                          , read.orientation == Input::ReadOrientation::reverse);
    }
    return result;
}

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
    void finalReport(T const &progress, Clock const &now = Reporter::now()) const {
        auto const elapsed = std::chrono::duration<double>{now - start};
        if (progress == 0 || elapsed.count() <= 0) {
            std::cerr << "progress: " << progress << " records" << std::endl;
        }
        else {
            auto const rps = progress / elapsed.count();
            std::cerr << "progress: " << progress << " records, per second: " << rps << std::endl;
        }
    }
    template <typename T>
    double report(T const &progress, Clock const &now = Reporter::now()) const {
        auto const elapsed = std::chrono::duration<double>{now - start};
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
        { "output", "o", nullptr, true },
        { "fingerprint", "f", nullptr, false }
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
            if (param == "fingerprint") {
                fingerprint = true;
                continue;
            }
            if (param == "help") {
                std::cout << "usage: " << arguments.program << " [-f|--fingerprint] [-p|--progress <seconds:=60>] [-t|--multithreaded] [-m|--mmap] [-o|--output <path>] [<path> ...]" << std::endl;
                exit(0);
            }
            if (param == "version") {
                int const rev = (TOOLKIT_VERS >>  0) & 0xFFFF;
                int const min = (TOOLKIT_VERS >> 16) & 0xFF;
                int const maj = (TOOLKIT_VERS >> 24) & 0xFF;
                
                std::cout << '\n' << arguments.program << " : 1.0.0 ( " << maj << '.' << min << '.' << rev
#if _DEBUG || DEBUGGING
                    << "-dev"
#endif
                 << " )\n" << std::endl;
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
        auto out = JSON_ostream{strm};

        out << '{';
        if ( fingerprint )
        {
            stats.fingerprint.canonicalForm(out);
        }
        else
        {
            out << JSON_Member{"total"} << '{' << stats << '}';

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
        }
        out << '}';
        std::cout << std::endl;
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
                    auto const bio = read.type == Input::ReadType::biological || read.type == Input::ReadType::aligned;

                    stats.record(seq, bio, group);
                    if (read.type == Input::ReadType::aligned) {
                        assert(read.reference >= 0);
                        assert(read.position >= 0);
                        unsigned const strand = read.orientation == Input::ReadOrientation::reverse ? 1 : 0;
                        stats.record(read.reference, read.position, strand, seq, read.cigar, group);
                        ++naligned;
                    }
                }
                if (spot.reads.size() > 0 && spot.reads.size() != naligned) {
                    stats.record(spotLayout(spot), group);
                }

                reporter.update(++processed);
            }
            catch (std::ios_base::failure const &e) {
                ((void)(e));
            }
        }
        reporter.finalReport(processed);
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
    bool fingerprint = false;
    std::optional<std::string> output;

    Stats stats;
    std::vector<Stats> spotGroup;

    Reporter reporter;

    Arguments arguments;
    Arguments::const_iterator nextInput;
    Arguments::const_iterator currentInput;
};

int main(int argc, char * argv[]) {
#if 0
    Input::runTests();
    exit(0);
#endif
    auto app = App{argc, argv};

    return app.run();
}
