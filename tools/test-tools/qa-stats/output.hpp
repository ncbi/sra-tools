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
 *  Format JSON output.
 */

#pragma once

#ifndef output_hpp
#define output_hpp

#include <iosfwd>
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <set>

struct JSON_Member {
    std::string name;
};

class JSON_ostream {
    std::ostream &strm;
    bool ws = true;         ///< was the last character inserted a whitespace character.
    bool newline = true;    ///< Is newline needed before the next item.
    bool comma = false;     ///< Was the character a ','? More importanly, will the next character belong to a new list item (or be the end of list).
    bool instr = false;     ///< In a string, therefore apply string escaping rules to the inserted characters.
    bool esc = false;       ///< In a string and the next character is escaped.
    
    /// A list is anything with components that are separated by ',', i.e. JSON Objects and Arrays
    std::vector<bool> listStack; ///< Records if list is empty. false mean empty. back() is the current list.

    /// Send the character to the output stream. Checks for whitespace.
    void insert_raw(char v);
    
    /// If needed, start a newline and indent it.
    void indentIfNeeded();
    
    /// Insert a character using string escaping rules.
    void insert_instr(char v);
    
    /// Insert a string using string escaping rules.
    void insert_instr(char const *const str);

    /// Insert a string without using any escaping rules.
    void insert_raw(char const *const str);

    // start a new list (array or object)
    void newList(char type);

    // start a new list item
    void listItem();

    // end a list (array or object)
    void endList(char type);

public:
    JSON_ostream(std::ostream &os) : strm(os) {}

    template <typename T>
    /// Insert some value (NB, use for number types)
    JSON_ostream &insert(T v) {
        std::ostringstream oss;

        oss << v;
        if (!ws)
            insert_raw(' ');
        insert_raw(oss.str().c_str());
        return *this;
    }
    
    /// Insert a Boolean value
    JSON_ostream &insert(bool const &v) {
        insert_raw(v ? "true" : "false");
        return *this;
    }

    /// Insert a character.
    ///
    /// When not in string mode, the stream reacts to the character.
    /// * '"' will toggle string mode.
    /// * '{' will add a list level and indent.
    /// * '}' will remove a list level and indent.
    /// * '[' will add a list level and indent.
    /// * ']' will remove a list level and indent.
    /// * ',' will end the current list item.
    ///
    /// There is no validation of syntax. But inserting sequences
    /// of the above characters should still produce valid (if weird) JSON.
    ///
    /// When in string mode, the stream reacts to the character inserted.
    /// * certain characters will be escaped as needed.
    /// * '"' will end string mode unless the previous character was the escape character.
    JSON_ostream &insert(char v);

    /// Insert a whole string of characters.
    /// In string mode, it will append to the current string.
    /// Otherwise, it will insert '"' (thus entering string mode),
    /// append the string, and insert '"' (hopefully exiting string mode)
    JSON_ostream &insert(char const *v);

    /// Insert a whole string of characters
    JSON_ostream &insert(std::string const &v) {
        return insert(v.c_str());
    }

    /// Insert an object member name, i.e. a string followed by a ':'
    JSON_ostream &insert(JSON_Member const &v);
};

/// Insert some value (NB, use for number types)
template <typename T>
JSON_ostream &operator <<(JSON_ostream &s, T const &v) {
    return s.insert(v);
}

#ifdef hashing_hpp

static inline
JSON_ostream &operator <<(JSON_ostream &s, HashResult64 const &v) {
    char buffer[32];
    return s.insert(HashResult64::to_hex(v.value, buffer));
}

#endif // hashing_hpp

#ifdef fingerprint_hpp

static inline
JSON_ostream &operator <<(JSON_ostream &out, Fingerprint::Accumulator const &self)
{
    for (size_t i = 0; i < self.size(); ++i) {
        out << '{'
            << JSON_Member{"base"} << self.base
            << JSON_Member{"pos"} << i
            << JSON_Member{"count"} << self[i]
        << '}';
    }
    return out;
}


static inline
JSON_ostream &operator <<(JSON_ostream &out, Fingerprint const &self)
{
    out << self.a
        << self.c
        << self.g
        << self.t
        << self.n
        << self.ool;
    return out;
}

#endif // fingerprint_hpp

#ifdef stats_hpp

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
    , length(i)
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
        out << DistanceStatEntry{ i, count, total };
    });
    return out;
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, std::pair < DistanceStats::DistanceStat, DistanceStats::DistanceStat > const &self)
{
    using Index = DistanceStats::DistanceStat::Index;
    self.first.forEach(self.second, [&](Index i, uint64_t count, uint64_t total) {
        out << DistanceStatEntry{ i, count, total };
    });
    return out;
}

static inline
JSON_ostream &operator <<(JSON_ostream &out, DistanceStats const &distance) {
    if (distance) {
        out << JSON_Member{"W"} << '[' << std::make_pair(distance.A, distance.T) << ']';
        out << JSON_Member{"S"} << '[' << std::make_pair(distance.C, distance.G) << ']';
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

#endif // stats_hpp

#endif /* output_hpp */
