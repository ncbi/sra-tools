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
 *  Fingerprinting reads
 */

#pragma once

#ifndef fingerprint_hpp
#define fingerprint_hpp

//#include <algorithm> /* std::max on Windows */
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <cctype>
#include <JSON_ostream.hpp>
#include <hashing.hpp>

class Fingerprint
{
private:
    struct Entry {
        uint64_t eor = 0, n = 0, a = 0, c = 0, g = 0, t = 0;

        uint64_t operator[](char base) const {
            switch (std::toupper(base))
            {
                case 'A': return a;
                case 'C': return c;
                case 'G': return g;
                case 'T': return t;
            }
            return base ? n : eor;
        }
        uint64_t &operator[](char base) {
            switch (std::toupper(base))
            {
                case 'A': return a;
                case 'C': return c;
                case 'G': return g;
                case 'T': return t;
            }
            return base ? n : eor;
        }
        void record(char base) {
            (*this)[base] += 1;
        }
        void recordEnd() {
            eor += 1;
        }
    };
    
    struct Accumulator : std::vector< Entry >
    {
        using Base = std::vector< Entry >;
        using Size = Base::size_type;
        Size max = 0;           ///< the maximum index updated
        Size max_effective = 0; ///< the maximum actual index updated, i.e. max % size

        Accumulator(Size size)
        : Base(size, Entry{})
        {}

        Entry &operator[](Size index) {
            auto const effective = index % size();
            max = std::max(max, index);
            max_effective = std::max(max_effective, effective);
            return Base::operator[](effective);
        }
        Entry operator[](Size index) const {
            return Base::operator[](index % size());
        }

        struct Formatter {
            std::vector< Entry > const *entry;
            size_t N;
            char which;

            Formatter(Accumulator const *accum, char p_which = 0)
            : entry(accum)
            , N(accum->max_effective)
            , which(p_which)
            {}
            uint64_t operator[](int i) const {
                return (*entry)[i][which];
            }
            friend JSON_ostream &operator<<(JSON_ostream &strm, Formatter const &self) {
                strm << '[';
                for (size_t i = 0; i <= self.N; ++i)
                    strm << self[i] << ',';
                strm << ']';
                return strm;
            }
        };
        Formatter a() const { return Formatter{this, 'A'}; }
        Formatter c() const { return Formatter{this, 'C'}; }
        Formatter g() const { return Formatter{this, 'G'}; }
        Formatter t() const { return Formatter{this, 'T'}; }
        Formatter n() const { return Formatter{this, 'N'}; }
        Formatter eor() const { return Formatter{this}; }
    };

    Accumulator accum;

public:
    static constexpr size_t DefaultMaxReadSize = 4096;

    explicit Fingerprint( size_t p_maxReadSize = DefaultMaxReadSize )
    : accum{p_maxReadSize}
    {}

    void record(char base, size_t position) {
        accum[position].record(base);
    }
    void recordEnd(size_t length) {
        accum[length].recordEnd();
    }
    void record(std::string_view const seq) {
        size_t index = 0;
        for (auto && base : seq)
            accum[index++].record(base);
        accum[index].recordEnd();
    }

    friend 
    JSON_ostream &operator <<(JSON_ostream &out, Fingerprint const &self)
    {
        out << '{'
            << JSON_Member{"maximum-position"} << self.accum.max
            << JSON_Member{"A"}   << self.accum.a()
            << JSON_Member{"C"}   << self.accum.c()
            << JSON_Member{"G"}   << self.accum.g()
            << JSON_Member{"T"}   << self.accum.t()
            << JSON_Member{"N"}   << self.accum.n()
            << JSON_Member{"EoR"} << self.accum.eor()
            << '}';
        return out;
    }

    std::string JSON(bool compact = true) const {
        std::ostringstream strm;
        {
            auto && wrapper = JSON_ostream{strm, compact};
            wrapper << *this;
        }
        return strm.str();
    }

    /// SHA-256 Digest; a 64 hex digit string.
    std::string digest() const {
        auto result = SHA256::hash(JSON()).string();
        for (auto && ch : result)
            ch = (char)std::tolower(ch);
        return result;
    }

    /// The version of the fingerprint algorithm.
    static std::string version() {
        return "1.0.0";
    }

    /// The algorithm used to produce the digest.
    static std::string algorithm() {
        return "SHA-256";
    }

    /// How the fingerprint was formated before producing the digest.
    static std::string format() {
        return "json utf-8 compact";
    }

    /// @brief Writes the canonical form to the stream.
    /// @param strm the stream to which to write.
    /// @return the stream which was passed in.
    /// @note It is incumbent on the caller to provide the JSON object context.
    JSON_ostream &canonicalForm(JSON_ostream &strm) {
        auto const save = strm.is_compact();
        return strm
                << JSON_Member{"fingerprint-version"} << version()
                << JSON_Member{"fingerprint-format"} << format()
                << JSON_Member{"fingerprint-digest-algorithm"} << algorithm()
                << JSON_Member{"fingerprint"} << JSON_ostream::Compact{true} << (*this) << JSON_ostream::Compact{save}
                << JSON_Member{"fingerprint-digest"} << digest();

    }
};

#endif // fingerprint_hpp
