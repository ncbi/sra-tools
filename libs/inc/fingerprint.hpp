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

#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <cctype>
#include <JSON_ostream.hpp>
#include <klib/checksum.h>

class Fingerprint
{
public:
    static constexpr size_t DefaultMaxReadSize = 4096;

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
    
private:
    std::vector< Entry > accumulator;
    size_t maxPos = 0;

    Entry &operator[](size_t position) {
        auto const pos = position % size();
        maxPos = std::max(maxPos, pos);
        return accumulator[pos];
    }

public:
    explicit Fingerprint( size_t p_maxReadSize = DefaultMaxReadSize )
    : accumulator(p_maxReadSize, Entry{})
    {}

    size_t size() const { return accumulator.size(); }

    Entry const &operator[](size_t position) const {
        auto const pos = position % size();
        return accumulator[pos];
    }
    void record(char base, size_t position) {
        (*this)[position].record(base);
    }
    void recordEnd(size_t length) {
        (*this)[length].recordEnd();
    }
    void record(std::string_view const seq) {
        auto cur = accumulator.begin();
        auto const end = accumulator.end();
        
        for (auto && base : seq) {
            cur->record(base);
            if (++cur == end)
                cur = accumulator.begin();
        }
        cur->recordEnd();
        maxPos = std::max(maxPos, seq.length() % size());
    }

    friend
    struct Accumulator;

    struct Accumulator : public std::vector< uint64_t >
    {
        std::string base;

        Accumulator(std::vector< Entry > const &entry, std::string_view const p_base, size_t maxPos)
        : std::vector< uint64_t >(maxPos + 1, 0)
        , base(p_base)
        {
            auto const bc = base.length() == 1 ? base[0] 
                          : base == "OoL" ? 0 : 'N';
            for (size_t i = 0; i <= maxPos; ++i)
                (*this)[i] = entry[i][bc];
        }
        
        friend
        JSON_ostream &operator <<(JSON_ostream &out, Accumulator const &self)
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
    };
    
    Accumulator a() const { return Accumulator{ accumulator, "A"  , maxPos }; }
    Accumulator c() const { return Accumulator{ accumulator, "C"  , maxPos }; }
    Accumulator g() const { return Accumulator{ accumulator, "G"  , maxPos }; }
    Accumulator t() const { return Accumulator{ accumulator, "T"  , maxPos }; }
    Accumulator n() const { return Accumulator{ accumulator, "N"  , maxPos }; }
    Accumulator ool() const { return Accumulator{ accumulator, "OoL", maxPos }; }
    
    friend 
    JSON_ostream &operator <<(JSON_ostream &out, Fingerprint const &self)
    {
        out << self.a()
            << self.c()
            << self.g()
            << self.t()
            << self.n()
            << self.ool();
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
        auto result = std::string{64, '\0'};
        uint8_t buffer[32];
        {
            SHA256State state;
            
            SHA256StateInit(&state);
            {
                auto const json = JSON();
                SHA256StateAppend(&state, json.data(), json.size());
            }
            SHA256StateFinish(&state, buffer);
        }
        // convert digest to hex digits
        for (auto i = 0; i < 32; ++i) {
            result[i * 2 + 0] = "0123456789abcdef"[(buffer[i] >> 4) & 0x0F];
            result[i * 2 + 1] = "0123456789abcdef"[(buffer[i] >> 0) & 0x0F];
        }
        return result;
    }
};

#endif // fingerprint_hpp
