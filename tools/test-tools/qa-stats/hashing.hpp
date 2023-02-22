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
 *  Compute expected invariant hashes.
 */

#ifndef hashing_hpp
#define hashing_hpp

#include <cstdint>
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <functional>

static inline char complement_base(char base) {
    switch (base) {
    case 'A': return 'T';
    case 'C': return 'G';
    case 'G': return 'C';
    case 'T': return 'A';
    default:
        return 'N';
    }
}

static inline std::string complement_string(std::string const &str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), complement_base);
    return result;
}

struct HashResult64 {
    uint64_t value;

    bool operator ==(HashResult64 const &other) const { return value == other.value; }
    HashResult64 &operator ^=(HashResult64 const &other) {
        value ^= other.value;
        return *this;
    }
    static char const *to_hex(uint64_t x, char *buffer) {
        char *cp = buffer + 32;

        *--cp = '\0';
        do {
            *--cp = "0123456789ABCDEF"[x & 0xF];
            x >>= 4;
        } while (x != 0);
        return cp;
    }
    friend std::ostream &operator <<(std::ostream &strm, HashResult64 const &self) {
        char buffer[32];
        return strm << to_hex(self.value, buffer);
    }
};

template < typename IMPL, typename VALUE = typename IMPL::Value, typename STATE = typename IMPL::State >
struct HashFunction {
    using State = STATE;
    using Value = VALUE;
    State state;

    HashFunction() {
        state = IMPL::init();
    }
    template <typename T>
    void update(T v) {
        IMPL::update(state, sizeof(T), &v);
    }
    template <typename T>
    void update(size_t count, T const v[]) {
        IMPL::update(state, sizeof(T) * count, v);
    }
    void finish(Value &dest) {
        dest = IMPL::finalize(state);
    }
    Value finish() {
        Value value;
        finish(value);
        return value;
    }
    Value finished() const {
        State copy = this->state;
        return IMPL::finalize(copy);
    }
    template <typename T>
    static Value hash(size_t count, T const v[]) {
        auto h = HashFunction();
        h.update(count, v);
        return h.finish();
    }
    template <typename T>
    HashFunction operator ^=(T const &v) {
        IMPL::combine(state, v);
    }
    friend std::ostream &operator <<(std::ostream &os, HashFunction const &cself) {
        auto self = cself;
        auto const value = self.finish();
        return os << value;
    }
    static void test() {
#ifndef NDEBUG
        IMPL::test();
#endif
    }
};

template < typename T, T prime, T offset >
struct FNV1a_impl {
    using State = uint64_t;
    using Value = uint64_t;

    static Value const PRIME = prime;
    static State const OFFSET = offset;

    static State init() {
        return offset;
    }
    static State update1(State state, uint8_t const *v) {
        return (state ^ *v) * prime;
    }
    static void update(State &state, size_t size, void const *v) {
        auto beg = reinterpret_cast<uint8_t const *>(v);
        auto const end = beg + size;

        while (beg < end) {
            state = update1(state, beg++);
        }
    }
    static Value finalize(State &state) {
        return state;
    }
    static void test() {
#ifndef NDEBUG

#endif
    }
};
using FNV1a64 = FNV1a_impl<uint64_t, 0x100000001b3ull, 0xcbf29ce484222325ull>;
using FNV1a = struct HashFunction<FNV1a64>;

struct SeqHash_impl {
    using State = FNV1a;
    using Value = FNV1a::Value;
    static State init() { return State{}; }
    static Value finalize(State &state) {
        return state.state;
    }
    static void update(State &state, size_t size, void const *v) {
        auto const seq = reinterpret_cast<char const *>(v);
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

struct RevSeqHash_impl {
    using State = FNV1a;
    using Value = FNV1a::Value;
    static State init() { return State{}; }
    static Value finalize(State &state) {
        return state.state;
    }
    static void update(State &state, size_t size, void const *v) {
        auto const seq = reinterpret_cast<char const *>(v);
        auto const end = seq + size;

        for (auto i = end; i != seq; ) {
            state.update<char>(complement_base(*--i));
        }
    }
    static void test() {
#ifndef NDEBUG
#endif
    }
};
using RevSeqHash = struct HashFunction<SeqHash_impl>;

struct ReadHash_impl {
    using State = uint64_t;
    using Value = uint64_t;
    static State init() { return State{}; }
    static Value finalize(State &state) {
        return state;
    }
    static void update(State &state, size_t size, void const *v) {
        state ^= SeqHash::hash(size, (char const *)v);
        state ^= RevSeqHash::hash(size, (char const *)v);
    }
    static void combine(State &state, FNV1a::Value const &other) {
        state ^= other;
    }
    static void test() {
#ifndef NDEBUG
        std::string const fwd{"GTGGACATCCCTCTGTGTGGGTCAT"};
        std::string const rev(fwd.rbegin(), fwd.rend());
        std::string const r_c(complement_string(rev));
        std::string const f_n{"GTGGACATCCCTCTGTGTGNGTCAN"};
        auto const hfwd = HashFunction<ReadHash_impl>::hash(fwd.size(), fwd.data());
        auto const hrev = HashFunction<ReadHash_impl>::hash(rev.size(), rev.data());
        auto const hr_c = HashFunction<ReadHash_impl>::hash(r_c.size(), r_c.data());
        auto const hf_n = HashFunction<ReadHash_impl>::hash(f_n.size(), f_n.data());

        if (hfwd == hf_n)
            throw std::logic_error("different sequences hashed to the same value!");
        if (hfwd == hrev)
            throw std::logic_error("reversed sequences hashed to same value!");
        if (hfwd != hr_c)
            throw std::logic_error("reverse-complemented sequences hashed to different value!");
#endif
    }
};
using ReadHash = struct HashFunction<ReadHash_impl>;

#endif /* hashing_hpp */
