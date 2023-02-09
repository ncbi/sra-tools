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

struct HashResult64 {
    uint64_t value;

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
    friend std::ostream &operator <<(std::ostream &os, HashFunction const &cself) {
        auto self = cself;
        auto const value = self.finish();
        return os << value;
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
};
using FNV1a64 = FNV1a_impl<uint64_t, 0x100000001b3ull, 0xcbf29ce484222325ull>;

struct FieldHash_impl {
    using State = uint64_t;
    using Value = uint64_t;

    static Value const prime = 0x100000001b3ull;
    static State const offset = 0xcbf29ce484222325ull;

    static State init() {
        return offset;
    }
    static void update(State &state, size_t count, uint64_t const *v) {
        auto const result = (state * (*v)) % prime;
        state = (result != 0) ? result : offset;
    }
    static Value finalize(State &state) {
        return state & 0xFFFFFFFFFFull;
    }
};
using FieldHash = HashFunction<FieldHash_impl>;

struct SeqHash_impl {
    struct State {
        FieldHash base, unstrand;
        uint64_t
            fnv1a_fwd = FNV1a64::OFFSET,
            fnv1a_rev = FNV1a64::OFFSET;

        uint64_t fwd_rev_hash() const {
            uint64_t const flo = uint32_t(fnv1a_fwd);
            uint64_t const rlo = uint32_t(fnv1a_rev);
            uint64_t const fhi = fnv1a_fwd >> 32;
            uint64_t const rhi = fnv1a_rev >> 32;
            uint64_t const lo = (flo * rlo) % FNV1a64::PRIME + (fhi * rhi) % FNV1a64::PRIME;
            uint64_t const hi = (flo * rhi) % FNV1a64::PRIME + (fhi * rlo) % FNV1a64::PRIME;
            return (hi << (64 - 40)) ^ lo;
        }
    private:
        friend SeqHash_impl;
        enum SomePrimes {
            /**
             * A set of 17-bit primes.
             * These happen to be in a sequence but that's not important. It is not
             * important that they are primes, only that they're coprime, so that
             * there's no "mixing" between them in the arithmetic.
             *
             * The final result is:
             *   pA^A * pC^C * pG^G * pT^T * pN^N mod M
             *   pAT^AT * pCG^CG * pN^N mod M
             * where:
             *   A is the count of 'A's, C is the count of 'C's, etc. and N is the count of not ACGT.
             * It's easy to see how the order of the bases doesn't matter in the final result.
             */
            pN = 130981u,
            pA = 130987u,
            pC = 131009u,
            pG = 131011u,
            pT = 131023u,
            pAT = 131041u,
            pCG = 131059u,
        };
        void update_forward(size_t size, void const *v) {
            auto cur = reinterpret_cast<char const *>(v);
            auto const end = cur + size;

            while (cur < end) {
                auto baseValue = toupper(*cur++);
                uint64_t basePrime = pN;
                uint64_t unstranded = pN;

                switch (baseValue) {
                case 'A':
                    basePrime = pA;
                    unstranded = pAT;
                    break;
                case 'C':
                    basePrime = pC;
                    unstranded = pCG;
                    break;
                case 'G':
                    basePrime = pG;
                    unstranded = pCG;
                    break;
                case 'T':
                    basePrime = pT;
                    unstranded = pAT;
                    break;
                default:
                    baseValue = 'N';
                    break;
                }
                base.update(basePrime);
                unstrand.update(unstranded);
                fnv1a_fwd = (fnv1a_fwd ^ baseValue) * FNV1a64::PRIME;
            }
        }
        void update_reverse(size_t size, void const *v) {
            auto cur = reinterpret_cast<char const *>(v) + size;
            auto const end = reinterpret_cast<char const *>(v);

            while (cur != end) {
                auto base = toupper(*(--cur));

                switch (base) {
                case 'A':
                    base = 'T';
                    break;
                case 'C':
                    base = 'G';
                    break;
                case 'G':
                    base = 'C';
                    break;
                case 'T':
                    base = 'A';
                    break;
                default:
                    base = 'N';
                    break;
                }
                fnv1a_rev = (fnv1a_rev ^ base) * FNV1a64::PRIME;
            }
        }
    };
    struct Result {
        HashResult64 base, unstranded, fnv1a;

        Result()
        {}

        explicit Result(State const &state)
        : base{state.base.finished()}
        , unstranded{state.unstrand.finished()}
        , fnv1a{state.fwd_rev_hash()}
        {}

        void accumulate(Result const &other) {
            base ^= other.base;
            unstranded ^= other.unstranded;
            fnv1a ^= other.fnv1a;
        }
        friend std::ostream &operator <<(std::ostream &os, Result const &self) {
            os << "Bases: " << self.base
               << ", Unstranded: " << self.unstranded
               << ", Sequence: " << self.fnv1a;

            return os;
        }
    };
    static State init() { return State{}; }
    static Result finalize(State &state) {
        return Result(state);
    }
    static void update(State &state, size_t size, void const *v) {
        state.update_forward(size, v);
        state.update_reverse(size, v);
    }
};

struct ReadHash_impl {
    using Value = SeqHash_impl::Result;
    using State = SeqHash_impl::Result;

    static State init() { return State{}; }
    static Value finalize(State &state) { return state; }
    static void update(State &state, size_t size, void const *v) {
        if (size == sizeof(state))
            state.accumulate(*reinterpret_cast<State const *>(v));
        else
            throw std::logic_error("invalid type");
    }
};

using FNV1a = struct HashFunction<FNV1a64>;
using SeqHash = struct HashFunction<SeqHash_impl, SeqHash_impl::Result>;
using ReadHash = struct HashFunction<ReadHash_impl>;

#endif /* hashing_hpp */
