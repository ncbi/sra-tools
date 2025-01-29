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

#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <JSON_ostream.hpp>

class Fingerprint
{
public:
    static constexpr size_t DefaultMaxReadSize = 4096;

    struct Accumulator : public std::vector< uint64_t >
    {
        std::string base;

        Accumulator(std::string_view p_base, size_t p_reserve)
        : std::vector< uint64_t >( p_reserve, 0 )
        , base(p_base)
        {}

        void record(size_t position) {
            (*this)[position % size()] += 1;
        }
    };

    explicit Fingerprint( size_t p_maxReadSize = DefaultMaxReadSize )
    : a( "A", p_maxReadSize )
    , c( "C", p_maxReadSize )
    , g( "G", p_maxReadSize )
    , t( "T", p_maxReadSize )
    , n( "N", p_maxReadSize )
    , ool( "OoL", p_maxReadSize )
    {}

    void record(char base, size_t position)
    {
        auto accum = &n;
        switch (std::toupper(base))
        {
            case 'A': accum = &a; break;
            case 'C': accum = &c; break;
            case 'G': accum = &g; break;
            case 'T': accum = &t; break;
        }
        accum->record(position);
    }
    void recordEnd(size_t length) {
        ool.record(length);
    }
    void record(std::string_view value)
    {
        size_t count = value.length();
        for(size_t i = 0; i < count; ++i)
        {
            record(value[i], i);
        }
        recordEnd(count);
    };

    Accumulator a;
    Accumulator c;
    Accumulator g;
    Accumulator t;
    Accumulator n; // everything that is not AGCT
    Accumulator ool; // out of read length
};

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

