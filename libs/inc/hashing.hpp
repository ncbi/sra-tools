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

#pragma once

#ifndef hashing_hpp
#define hashing_hpp

#include <cstdint>
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <algorithm>

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
            *--cp = "0123456789ABCDEF"[x & 0xF];
            x >>= 4;
        } while (x != 0);
        return cp;
    }
    static int from_hex(int ch) {
        if (!std::isxdigit(ch))
            throw std::domain_error("invalid value");
        int const offset = ch < 'A' ? '0' : (ch < 'a' ? 'A' - 10 : 'a' - 10);
        return ch - offset;
    }
    static int from_hex(int hi, int lo) {
        return (from_hex(hi) << 4) | from_hex(lo);
    }
    friend std::ostream &operator <<(std::ostream &strm, HashResult64 const &self) {
        char buffer[32];
        return strm << HashResult64::to_hex(self.value, buffer);
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
    static Value hash(std::string_view str) {
        return hash(str.size(), str.data());
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
typedef struct HashFunction<FNV1a64> FNV1a;

namespace SHA2 {

template <typename Word>
struct TState {
    uint64_t length = 0;
    Word H[8];
    union {
        Word    w[16]; // algorithm assumes little-endian byte order
        uint8_t b[16 * sizeof(Word)];
    } buffer; // message block buffer, accumulated one byte at a time but proceded as little-endian
    unsigned cur = 0;

    /// Used to fill in the buffer so that the word values will have the correct byte order for the host.
    static constexpr unsigned littleEndianOffset(unsigned offset) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        auto constexpr BMask = sizeof(Word) - 1;
        auto constexpr WMask = (sizeof(buffer) - 1) ^ BMask;
        return (offset & WMask) | (BMask - (offset & BMask));
#else
        return offset;
#endif
    }
};

template <unsigned N>
struct Digest {
    uint8_t byte[N];

    Digest() = default;
    Digest(std::string_view const sv) {
        if (sv.size() != 2 * N)
            throw std::domain_error("invalid length");

        for (unsigned i = 0; i < N; ++i)
            byte[i] = HashResult64::from_hex(sv[i * 2 + 0], sv[i * 2 + 1]);
    }
    bool operator ==(Digest const &rhs) const {
        for (unsigned i = 0; i < N; ++i) {
            if (byte[i] != rhs.byte[i])
                return false;
        }
        return true;
    }
    uint8_t &operator [](int index) { return byte[index]; }
    uint8_t operator [](int index) const { return byte[index]; }
    std::string string() const {
        char buffer[32];
        std::string result;
        result.reserve(2 * N);

        for (unsigned i = 0; i < N; ++i)
            result.append(HashResult64::to_hex(byte[i], buffer));

        return result;
    }
    friend std::ostream &operator <<(std::ostream &strm, Digest const &self) {
        char buffer[32];
        for (unsigned i = 0; i < N; ++i)
            strm << HashResult64::to_hex(self.byte[i], buffer);
        return strm;
    }
};

/// Used for 32-bit SHA-2
struct SHA_32 {
    using State = TState<uint32_t>;

private:
    using Word = uint32_t;
    static constexpr Word ROL(Word const X, unsigned const N) { return (X << N) | (X >> (32 - N)); }
    static constexpr Word ROR(Word const X, unsigned const N) { return (X >> N) | (X << (32 - N)); }
    static constexpr Word SHR(Word const X, unsigned const N) { return X >> N; }
    static constexpr Word Sigma0(Word const X) { return ROR(X,  2) ^ ROR(X, 13) ^ ROR(X, 22); }
    static constexpr Word Sigma1(Word const X) { return ROR(X,  6) ^ ROR(X, 11) ^ ROR(X, 25); }
    static constexpr Word sigma0(Word const X) { return ROR(X,  7) ^ ROR(X, 18) ^ SHR(X,  3); }
    static constexpr Word sigma1(Word const X) { return ROR(X, 17) ^ ROR(X, 19) ^ SHR(X, 10); }
    static constexpr Word Ch    (Word const X, Word const Y, Word const Z) { return ((X & Y) ^ ((~X) & Z)); }
    static constexpr Word Parity(Word const X, Word const Y, Word const Z) { return (X ^ Y ^ Z); }
    static constexpr Word Maj   (Word const X, Word const Y, Word const Z) { return Parity(X & Y, X & Z, Y & Z); }

    static void stage(State &state) {
        static Word const K[] = {
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
            0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
            0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
            0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
            0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
            0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
            0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
            0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
        };
        auto H = state.H;
        auto W = state.buffer.w;
        auto a = H[0];
        auto b = H[1];
        auto c = H[2];
        auto d = H[3];
        auto e = H[4];
        auto f = H[5];
        auto g = H[6];
        auto h = H[7];
        unsigned t;

        for (t = 0; t < 16; ++t) {
            auto const T1 = h + Sigma1(e) + Ch(e, f, g) + K[t] + W[t];
            auto const T2 = Sigma0(a) + Maj(a, b, c);

            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }
        for ( ; t < 64; ++t) {
            auto const T1 = h + Sigma1(e) + Ch(e, f, g) + K[t] +
                (W[t%16] = sigma1(W[(t-2)%16])+W[(t-7)%16]+sigma0(W[(t-15)%16])+W[(t-16)%16]);
            auto const T2 = Sigma0(a) + Maj(a, b, c);

            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }
        H[0] += a;
        H[1] += b;
        H[2] += c;
        H[3] += d;
        H[4] += e;
        H[5] += f;
        H[6] += g;
        H[7] += h;
    }

    static void append(State &state, uint8_t byte) {
        state.buffer.b[State::littleEndianOffset(state.cur)] = byte;
        if (++state.cur == sizeof(state.buffer)) {
            stage(state);
            state.cur = 0;
        }
    }
    static void setLength(State &state, size_t bytes) {
        auto bits = bytes << 3;
        uint8_t buffer[sizeof(uint64_t)];
        auto cp = buffer + sizeof(buffer);

        for (size_t i = 0; i < sizeof(buffer); ++i, bits >>= 8)
            *--cp = (uint8_t)bits;
        while (state.cur + sizeof(buffer) != sizeof(state.buffer))
            append(state, 0x00);
        for (size_t i = 0; i < sizeof(buffer); ++i)
            append(state, buffer[i]);
    }

protected:
    static void update(State &state, size_t const bytes, void const *const data) {
        auto const D = reinterpret_cast<uint8_t const *>(data);
        for (size_t i = 0; i < bytes; ++i)
            append(state, D[i]);
        state.length += bytes;
    }
    static void finalize(State &state) {
        auto const length = state.length;
        append(state, 0x80);
        setLength(state, length);
    }
};

/// Used for 64-bit SHA-2
struct SHA_64 {
    using State = TState<uint64_t>;
private:
    using Word = uint64_t;

    static constexpr Word ROL(Word const X, unsigned const N) { return (X << N) | (X >> (64 - N)); }
    static constexpr Word ROR(Word const X, unsigned const N) { return (X >> N) | (X << (64 - N)); }
    static constexpr Word SHR(Word const X, unsigned const N) { return X >> N; }
    static constexpr Word Sigma0(Word const X) { return ROR(X, 28) ^ ROR(X, 34) ^ ROR(X, 39); }
    static constexpr Word Sigma1(Word const X) { return ROR(X, 14) ^ ROR(X, 18) ^ ROR(X, 41); }
    static constexpr Word sigma0(Word const X) { return ROR(X,  1) ^ ROR(X,  8) ^ SHR(X,  7); }
    static constexpr Word sigma1(Word const X) { return ROR(X, 19) ^ ROR(X, 61) ^ SHR(X,  6); }
    static constexpr Word Ch    (Word const X, Word const Y, Word const Z) { return ((X & Y) ^ ((~X) & Z)); }
    static constexpr Word Parity(Word const X, Word const Y, Word const Z) { return (X ^ Y ^ Z); }
    static constexpr Word Maj   (Word const X, Word const Y, Word const Z) { return Parity(X & Y, X & Z, Y & Z); }

    static void stage(State &state) {
        static Word const K[] = {
            0x428a2f98d728ae22UL, 0x7137449123ef65cdUL, 0xb5c0fbcfec4d3b2fUL, 0xe9b5dba58189dbbcUL,
            0x3956c25bf348b538UL, 0x59f111f1b605d019UL, 0x923f82a4af194f9bUL, 0xab1c5ed5da6d8118UL,
            0xd807aa98a3030242UL, 0x12835b0145706fbeUL, 0x243185be4ee4b28cUL, 0x550c7dc3d5ffb4e2UL,
            0x72be5d74f27b896fUL, 0x80deb1fe3b1696b1UL, 0x9bdc06a725c71235UL, 0xc19bf174cf692694UL,
            0xe49b69c19ef14ad2UL, 0xefbe4786384f25e3UL, 0x0fc19dc68b8cd5b5UL, 0x240ca1cc77ac9c65UL,
            0x2de92c6f592b0275UL, 0x4a7484aa6ea6e483UL, 0x5cb0a9dcbd41fbd4UL, 0x76f988da831153b5UL,
            0x983e5152ee66dfabUL, 0xa831c66d2db43210UL, 0xb00327c898fb213fUL, 0xbf597fc7beef0ee4UL,
            0xc6e00bf33da88fc2UL, 0xd5a79147930aa725UL, 0x06ca6351e003826fUL, 0x142929670a0e6e70UL,
            0x27b70a8546d22ffcUL, 0x2e1b21385c26c926UL, 0x4d2c6dfc5ac42aedUL, 0x53380d139d95b3dfUL,
            0x650a73548baf63deUL, 0x766a0abb3c77b2a8UL, 0x81c2c92e47edaee6UL, 0x92722c851482353bUL,
            0xa2bfe8a14cf10364UL, 0xa81a664bbc423001UL, 0xc24b8b70d0f89791UL, 0xc76c51a30654be30UL,
            0xd192e819d6ef5218UL, 0xd69906245565a910UL, 0xf40e35855771202aUL, 0x106aa07032bbd1b8UL,
            0x19a4c116b8d2d0c8UL, 0x1e376c085141ab53UL, 0x2748774cdf8eeb99UL, 0x34b0bcb5e19b48a8UL,
            0x391c0cb3c5c95a63UL, 0x4ed8aa4ae3418acbUL, 0x5b9cca4f7763e373UL, 0x682e6ff3d6b2b8a3UL,
            0x748f82ee5defb2fcUL, 0x78a5636f43172f60UL, 0x84c87814a1f0ab72UL, 0x8cc702081a6439ecUL,
            0x90befffa23631e28UL, 0xa4506cebde82bde9UL, 0xbef9a3f7b2c67915UL, 0xc67178f2e372532bUL,
            0xca273eceea26619cUL, 0xd186b8c721c0c207UL, 0xeada7dd6cde0eb1eUL, 0xf57d4f7fee6ed178UL,
            0x06f067aa72176fbaUL, 0x0a637dc5a2c898a6UL, 0x113f9804bef90daeUL, 0x1b710b35131c471bUL,
            0x28db77f523047d84UL, 0x32caab7b40c72493UL, 0x3c9ebe0a15c9bebcUL, 0x431d67c49c100d4cUL,
            0x4cc5d4becb3e42b6UL, 0x597f299cfc657e2aUL, 0x5fcb6fab3ad6faecUL, 0x6c44198c4a475817UL,
        };
        auto H = state.H;
        auto a = H[0];
        auto b = H[1];
        auto c = H[2];
        auto d = H[3];
        auto e = H[4];
        auto f = H[5];
        auto g = H[6];
        auto h = H[7];
        unsigned t;
        auto W = state.buffer.w;

        for (t = 0; t < 16; ++t) {
            auto const T1 = h + Sigma1(e) + Ch(e, f, g) + K[t] + W[t];
            auto const T2 = Sigma0(a) + Maj(a, b, c);

            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }
        for ( ; t < 80; ++t) {
            auto const T1 = h + Sigma1(e) + Ch(e, f, g) + K[t] +
                (W[t%16] = sigma1(W[(t-2)%16])+W[(t-7)%16]+sigma0(W[(t-15)%16])+W[(t-16)%16]);
            auto const T2 = Sigma0(a) + Maj(a, b, c);

            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }
        H[0] += a;
        H[1] += b;
        H[2] += c;
        H[3] += d;
        H[4] += e;
        H[5] += f;
        H[6] += g;
        H[7] += h;
    }

    static void append(State &state, uint8_t byte) {
        auto B = state.buffer.b;
        auto &cur = state.cur;

        B[State::littleEndianOffset(cur)] = byte;
        if (++cur == sizeof(state.buffer)) {
            stage(state);
            cur = 0;
        }
    }
    static void setLength(State &state, size_t bytes) {
        auto bits = bytes << 3;
        uint8_t buffer[sizeof(uint64_t) * 2];
        auto cp = buffer + sizeof(buffer);

        for (size_t i = 0; i < sizeof(buffer); ++i, bits >>= 8)
            *--cp = (uint8_t)bits;
        while (state.cur + sizeof(buffer) != sizeof(state.buffer))
            append(state, 0x00);
        for (size_t i = 0; i < sizeof(buffer); ++i)
            append(state, buffer[i]);
    }

protected:
    static void update(State &state, size_t const bytes, void const *const data) {
        auto const D = reinterpret_cast<uint8_t const *>(data);
        for (size_t i = 0; i < bytes; ++i)
            append(state, D[i]);
        state.length += bytes;
    }
    static void finalize(State &state) {
        auto const length = state.length;
        append(state, 0x80);
        setLength(state, length);
    }
};

struct SHA_224 : public SHA_32 {
    using Value = Digest<28>;

    static State init() {
        static uint32_t const H0[8] = {
            0xc1059ed8u,
            0x367cd507u,
            0x3070dd17u,
            0xf70e5939u,
            0xffc00b31u,
            0x68581511u,
            0x64f98fa7u,
            0xbefa4fa4u
        };
        State result{};
        std::copy(&H0[0], &H0[8], &result.H[0]);
        return result;
    };

    using SHA_32::update;

    static Value finalize(State &state) {
        union {
            uint32_t H[8];
            Value result;
        } u;
        SHA_32::finalize(state);
        std::copy(&state.H[0], &state.H[8], &u.H[0]);
#if __BYTE_ORDER == __LITTLE_ENDIAN
        for (auto i = 0; i < 28; i += 4) {
            auto const hh = u.result[i + 0];
            auto const hl = u.result[i + 1];
            auto const lh = u.result[i + 2];
            auto const ll = u.result[i + 3];
            u.result[i + 0] = ll;
            u.result[i + 1] = lh;
            u.result[i + 2] = hl;
            u.result[i + 3] = hh;
        }
#endif
        return u.result;
    }
};

struct SHA_256 : public SHA_32 {
    using Value = Digest<32>;

    static State init() {
        static uint32_t const H0[8] = {
            0x6a09e667u,
            0xbb67ae85u,
            0x3c6ef372u,
            0xa54ff53au,
            0x510e527fu,
            0x9b05688cu,
            0x1f83d9abu,
            0x5be0cd19u,
        };
        State result{};
        std::copy(&H0[0], &H0[8], &result.H[0]);
        return result;
    };

    using SHA_32::update;

    static Value finalize(State &state) {
        union {
            uint32_t H[8];
            Value result;
        } u;
        SHA_32::finalize(state);
        std::copy(&state.H[0], &state.H[8], &u.H[0]);
#if __BYTE_ORDER == __LITTLE_ENDIAN
        for (auto i = 0; i < 32; i += 4) {
            auto const hh = u.result[i + 0];
            auto const hl = u.result[i + 1];
            auto const lh = u.result[i + 2];
            auto const ll = u.result[i + 3];
            u.result[i + 0] = ll;
            u.result[i + 1] = lh;
            u.result[i + 2] = hl;
            u.result[i + 3] = hh;
        }
#endif
        return u.result;
    }
};

struct SHA_384 : public SHA_64 {
    using Value = Digest<48>;

    static State init() {
        static uint64_t const H0[] = {
            0xcbbb9d5dc1059ed8ul,
            0x629a292a367cd507ul,
            0x9159015a3070dd17ul,
            0x152fecd8f70e5939ul,
            0x67332667ffc00b31ul,
            0x8eb44a8768581511ul,
            0xdb0c2e0d64f98fa7ul,
            0x47b5481dbefa4fa4ul,
        };
        State result{};
        std::copy(&H0[0], &H0[8], &result.H[0]);
        return result;
    }

    using SHA_64::update;

    static Value finalize(State &state) {
        union {
            uint64_t H[8];
            Value result;
        } u;
        SHA_64::finalize(state);
        std::copy(&state.H[0], &state.H[8], &u.H[0]);
#if __BYTE_ORDER == __LITTLE_ENDIAN
        for (auto i = 0; i < 48; i += 8) {
            auto const hhh = u.result[i + 0];
            auto const hhl = u.result[i + 1];
            auto const hlh = u.result[i + 2];
            auto const hll = u.result[i + 3];
            auto const lhh = u.result[i + 4];
            auto const lhl = u.result[i + 5];
            auto const llh = u.result[i + 6];
            auto const lll = u.result[i + 7];
            u.result[i + 0] = lll;
            u.result[i + 1] = llh;
            u.result[i + 2] = lhl;
            u.result[i + 3] = lhh;
            u.result[i + 4] = hll;
            u.result[i + 5] = hlh;
            u.result[i + 6] = hhl;
            u.result[i + 7] = hhh;
        }
#endif
        return u.result;
    }
};

struct SHA_512 : public SHA_64 {
    using Value = Digest<64>;

    static State init() {
        static uint64_t const H0[] = {
            0x6a09e667f3bcc908ul,
            0xbb67ae8584caa73bul,
            0x3c6ef372fe94f82bul,
            0xa54ff53a5f1d36f1ul,
            0x510e527fade682d1ul,
            0x9b05688c2b3e6c1ful,
            0x1f83d9abfb41bd6bul,
            0x5be0cd19137e2179ul,
        };
        State result{};
        std::copy(&H0[0], &H0[8], &result.H[0]);
        return result;
    }

    using SHA_64::update;

    static Value finalize(State &state) {
        union {
            uint64_t H[8];
            Value result;
        } u;
        SHA_64::finalize(state);
        std::copy(&state.H[0], &state.H[8], &u.H[0]);
#if __BYTE_ORDER == __LITTLE_ENDIAN
        for (auto i = 0; i < 64; i += 8) {
            auto const hhh = u.result[i + 0];
            auto const hhl = u.result[i + 1];
            auto const hlh = u.result[i + 2];
            auto const hll = u.result[i + 3];
            auto const lhh = u.result[i + 4];
            auto const lhl = u.result[i + 5];
            auto const llh = u.result[i + 6];
            auto const lll = u.result[i + 7];
            u.result[i + 0] = lll;
            u.result[i + 1] = llh;
            u.result[i + 2] = lhl;
            u.result[i + 3] = lhh;
            u.result[i + 4] = hll;
            u.result[i + 5] = hlh;
            u.result[i + 6] = hhl;
            u.result[i + 7] = hhh;
        }
#endif
        return u.result;
    }
};

}

typedef struct HashFunction< SHA2::SHA_224 > SHA224;
typedef struct HashFunction< SHA2::SHA_256 > SHA256;
typedef struct HashFunction< SHA2::SHA_384 > SHA384;
typedef struct HashFunction< SHA2::SHA_512 > SHA512;

#endif /* hashing_hpp */
