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
#include <string>
#include <cmath>
#include <cassert>
#include <JSON_ostream.hpp>
#include <hashing.hpp>
#include <flag-stat.hpp>
#include <qname-stat.hpp>
#include "parameters.hpp"
#include "input.hpp"
#include "stats.hpp"
#include "../../../shared/toolkit.vers.h"

static inline
JSON_ostream &operator <<(JSON_ostream &out, CountBT const &bt) {
    return out
        << JSON_Member{"count"} << bt.total
        << JSON_Member{"biological"} << bt.biological
        << JSON_Member{"technical"} << bt.technical;
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, CountFR const &fr) {
    return out
        << JSON_Member{"total"} << fr.total
        << JSON_Member{"5'"} << fr.fwd
        << JSON_Member{"3'"} << fr.rev;
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
JSON_ostream &operator <<(JSON_ostream &out, BaseStats const &baseStats) {
    using Index = BaseStats::Index;
    auto const bioW = baseStats[Index{'A', false}] + baseStats[Index{'T', false}];
    auto const bioS = baseStats[Index{'C', false}] + baseStats[Index{'G', false}];
    auto const bioN = baseStats[Index{'N', false}];
    auto const techW = baseStats[Index{'A', true}] + baseStats[Index{'T', true}];
    auto const techS = baseStats[Index{'C', true}] + baseStats[Index{'G', true}];
    auto const techN = baseStats[Index{'N', true}];

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
JSON_ostream &operator <<(JSON_ostream &out, DistanceStats::DistanceStat const &distanceStats) {
    using Index = DistanceStats::DistanceStat::Index;
    distanceStats.forEach([&](Index i, uint64_t count, uint64_t total) {
        if (count > 0)
            out << DistanceStatEntry{ i, count, total };
    });
    return out;
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, std::pair < DistanceStats::DistanceStat const *, DistanceStats::DistanceStat const * > const &distanceStatsPair)
{
    using Index = DistanceStats::DistanceStat::Index;
    distanceStatsPair.first->forEach(*distanceStatsPair.second, [&](Index i, uint64_t count, uint64_t total) {
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
JSON_ostream &operator <<(JSON_ostream &out, ReferenceStats const &refStats) {
    for (auto &reference : refStats) {
        auto const refId = &reference - &refStats[0];
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

static JSON_ostream &operator <<(JSON_ostream &out, SpotLayouts const &layouts) {
    for (auto i = layouts.index.begin(); i != layouts.index.end(); ++i) {
        out << '{'
            << JSON_Member{"descriptor"} << i->first
            << JSON_Member{"count"} << layouts.count[i->second]
        << '}';
    }
    return out;
}

static JSON_ostream &operator <<(JSON_ostream &out, CountBTN const &btn) {
    return out
        << JSON_Member{"total"} << btn.total
        << JSON_Member{"biological"} << btn.biological
        << JSON_Member{"no-biological"} << btn.nobiological
        << JSON_Member{"technical"} << btn.technical
        << JSON_Member{"no-technical"} << btn.notechnical;
}

static
JSON_ostream &operator <<(JSON_ostream &out, SpotStats const &spotStats) {
    for (auto &[k, v] : spotStats) {
        out << '{'
            << JSON_Member{"nreads"} << k
            << v
        << '}';
    }
    return out;
}

static JSON_ostream &operator <<(JSON_ostream &out, Stats const &stats) {
    out << JSON_Member{"reads"}        << '{' << stats.reads      << '}';
    out << JSON_Member{"bases"}        << '[' << stats.bases      << ']';
    out << JSON_Member{"spots"}        << '[' << stats.spots      << ']';
    out << JSON_Member{"spot-layouts"} << '[' << stats.layouts    << ']';
    out << JSON_Member{"spectra"}      << '{' << stats.spectra    << '}';
    out << JSON_Member{"references"}   << '[' << stats.references << ']';

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

static JSON_ostream &operator <<(JSON_ostream &out, QNAME_Counter const &qnameCounter) {
    HashResult256 value;
    
    qnameCounter.getAll(value.value);
    out << JSON_Member{"QNAME"} << '"' << value << '"';
    
    qnameCounter.getGrouped(value.value);
    out << JSON_Member{"RG+QNAME"} << '"' << value << '"';
    
    return out;
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
        { "fingerprint", "f", nullptr, false },
        { "flag", nullptr, "1.3", false },
        { "name", nullptr, nullptr, false }
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
            if (param == "flag") {
                assert(value.has_value());
                auto const &vers = value.value();
                int maj = 0, min = 0;
                int end = 0;
                int n = sscanf(vers.c_str(), "%i.%i%n", &maj, &min, &end);

                if (n == 2 && (size_t)end == vers.size()) {
                    if (maj >= 1 && min >= 13) {
                        flagStatTextVersion = FlagStatText::v_1_13;
                    }
                }
                else {
                    std::cerr << "usage: --flag <major>.<minor>" << std::endl;
                    exit(1);
                }
                flagCounter = std::unique_ptr<FLAG_Counter>(new FLAG_Counter);
                continue;
            }
            if (param == "name") {
                countQNAME = true;
                continue;
            }
            if (param == "help") {
                std::cout << "usage: " << arguments.program << " [-f|--fingerprint] [--flag] [--name] [-p|--progress <seconds:=60>] [-t|--multithreaded] [-m|--mmap] [-o|--output <path>] [<path> ...]" << std::endl;
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
            else {
                std::cerr << "couldn't open " << path << std::endl;
                exit(1);
            }
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
    void printFlagStat(std::ostream &strm) {
        strm << FlagStatText{*flagCounter, flagStatTextVersion}.defaultText;
    }
    void print(std::ostream &strm) {
        if (flagCounter) {
            printFlagStat(strm);
            return;
        }
        auto out = JSON_ostream{strm};

        out << '{';
        if ( fingerprint )
        {
            stats.fingerprint.canonicalForm(out);
        }
        else if (countQNAME) {
            out << JSON_Member{"QNAME-fingerprint"} << '{' << qnameCounter << '}';
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
        strm << std::endl;
    }
    void gather() {
        auto const flagCntr = flagCounter.get();
        auto source = Input::newSource(inputStream(), multithreaded);
        while (*source) {
            try {
                auto const spot = source->get();

                if (flagCntr) {
                    for (auto const &read : spot.reads) {
                        flagCntr->add(read.flags);
                        if (read.flags < 0) {
                            std::cerr << "--flag can only be used with SAM input." << std::endl;
                            exit(1);
                        }
                    }
                }
                else if (countQNAME) {
                    if (!spot.qname.empty())
                        qnameCounter.add(spot.qname, spot.group >= 0 ? spot.groups[spot.group] : "");
                }
                else {
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
    bool countQNAME = false;
    FlagStatText::Version flagStatTextVersion = FlagStatText::v_1_3;
    std::optional<std::string> output;

    Stats stats;
    std::vector<Stats> spotGroup;
    std::unique_ptr<FLAG_Counter> flagCounter = nullptr;
    QNAME_Counter qnameCounter;

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
