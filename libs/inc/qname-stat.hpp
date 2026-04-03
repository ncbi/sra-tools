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
 *  Loader Quality Checks.
 *
 * Purpose:
 *  C++ implementation for SAM QNAME counts.
 */

#include <string_view>
#include <vector>
#include <map>
#include <tuple>
#include <cstring>
#include <algorithm>
#include <cassert>

struct GF_Poly_2_8 {
    uint8_t coeff; // the 8 coefficients are bit values packed into one byte

private:
    /// @brief One bit at a time, 8-bit carry-less multiply mod {11b}
    /// @param a 
    /// @param b 
    /// @return a * b mod {11b}
    static uint8_t multiply(uint8_t a, uint8_t b) {
        uint8_t y{0};

        while (b != 0) {
            y ^= (b & 1) * a;
            a = (a << 1) ^ (0x1b * (a >> 7));
            b >>= 1;
        }
        return y;
    }
    friend struct ExpTable;
    struct ExpTable {
        uint8_t table[256];

        ExpTable()
        {
            auto const g = uint8_t{3}; ///< x + 1
            auto y = uint8_t{1}; ///< g^0

            for (auto i = 0; i < 256; ++i) {
                table[i] = y; ///< g^i
                y = GF_Poly_2_8::multiply(y, g);
            }
            assert(isValid(table));
        }
        GF_Poly_2_8 operator [](int const i) const
        {
            assert(i >= 0 && i < 256);
            return GF_Poly_2_8{table[i]};
        }
    private:
        static bool isValid(uint8_t const *table)
        {
            uint8_t tbl[256];
            std::copy(table, table + 256, tbl);
            if (tbl[0] != 1) return false;
            tbl[0] = 0;
            std::sort(tbl, tbl + 256);
            for (auto i = 0; i < 256; ++i) {
                if (tbl[i] != i) return false;
            }
            return true;
        }
    };

    static GF_Poly_2_8 exp(unsigned x) {
        static auto const expTable = ExpTable{};
        return expTable[x < 256 ? x : (x - 255)];
    }

    friend struct LogTable;
    struct LogTable {
        uint8_t table[256];

        LogTable()
        {
            table[0] = 0; ///< an invalid value that is never used.
            for (auto j = 0; j < 256; ++j) {
                auto const i = GF_Poly_2_8::exp(j).coeff;
                table[i] = j;
            }
            assert(isValid(table));
        }
        uint8_t operator [](uint8_t i) const {
            assert(i != 0);
            return table[i];
        }
    private:
        static bool isValid(uint8_t const *log) {
            for (auto i = 1; i < 256; ++i) {
                if (GF_Poly_2_8::exp(log[i]).coeff != i)
                    return false;
            }
            for (auto i = 1; i < 256; ++i) {
                if (log[GF_Poly_2_8::exp(i).coeff] != i)
                    return false;
            }
            return true;
        }
    };
    uint8_t log() const {
        static auto const logTable = LogTable{};
        return logTable[coeff];
    }
    GF_Poly_2_8 inverse() const {
        return coeff < 2 ? *this : GF_Poly_2_8::exp(255 - log());
    }

public:
    GF_Poly_2_8() = default;
    GF_Poly_2_8(uint8_t x) : coeff{x} {}
    GF_Poly_2_8(size_t count, uint8_t const *const bytes) {
        assert(count == 1);
        coeff = bytes[0];
    }
    static GF_Poly_2_8 zero() {
        return GF_Poly_2_8{0};
    }
    static GF_Poly_2_8 one() {
        return GF_Poly_2_8{1};
    }
    friend GF_Poly_2_8 operator +(GF_Poly_2_8 a, GF_Poly_2_8 b) {
        return GF_Poly_2_8{(uint8_t)(a.coeff ^ b.coeff)};
    }
    friend GF_Poly_2_8 operator -(GF_Poly_2_8 a, GF_Poly_2_8 b) {
        return a + b;
    }
    friend GF_Poly_2_8 operator *(GF_Poly_2_8 a, GF_Poly_2_8 b) {
        if (a.coeff == 0 || b.coeff == 0) return zero();
        if (a.coeff == 1) return b;
        if (b.coeff == 1) return a;
        return GF_Poly_2_8::exp(a.log() + b.log());
    }
    friend GF_Poly_2_8 operator /(GF_Poly_2_8 a, GF_Poly_2_8 b) {
        assert(b.coeff != 0);
        return a * b.inverse();
    }
    friend bool operator ==(GF_Poly_2_8 a, GF_Poly_2_8 b) {
        return a.coeff == b.coeff;
    }
    friend bool operator !=(GF_Poly_2_8 a, GF_Poly_2_8 b) {
        return a.coeff != b.coeff;
    }
    operator bool() const { return coeff != 0; }
    void getBytes(size_t count, uint8_t *const bytes) const {
        assert(count == 1);
        bytes[0] = coeff;
    }
    static void test() {
#if !NDEBUG
        for (auto i = 0; i < 256; ++i) {
            auto const a = GF_Poly_2_8{(uint8_t)i};
            for (auto j = 0; j < 256; ++j) {
                auto const b = GF_Poly_2_8{(uint8_t)j};
                auto const ab = a * b;
                assert(ab.coeff == GF_Poly_2_8::multiply(i, j));

                if (j == 0) continue;
                assert(ab / b == a);

                if (i == 0) continue;
                assert(ab / a == b);
            }
        }
#endif
    }
};

template <typename COEFF, int Degree>
struct Polynomial {
    COEFF coeff[Degree];

    Polynomial() = default;
    operator bool() const {
        for (auto i = 0; i < Degree; ++i) {
            if (coeff[i]) return true;
        }
        return false;
    }
    void getBytes(size_t count, uint8_t *const bytes) const {
        assert(count == sizeof(COEFF) * Degree);
        for (auto i = 0; i < Degree; ++i)
            coeff[i].getBytes(sizeof(COEFF), bytes + i * sizeof(COEFF));
    }

    static Polynomial createFromMask(uint64_t mask) {
        Polynomial result;
        for (auto i = 0; i < Degree; ++i, mask >>= 1)
            result.coeff[i] = (mask & 1) == 0 ? COEFF::zero() : COEFF::one();
        return result;
    }
    
    Polynomial(size_t count, uint8_t const *const bytes) {
        for (auto i = 0; i < Degree; ++i) {
            coeff[i] = COEFF(sizeof(COEFF), bytes + i * sizeof(COEFF));
        }
    }

    /// @brief Multiply-add
    /// @param a 
    /// @param b 
    /// @param s 
    /// @return a + b * s
    static Polynomial multiplyAdd(Polynomial const &a, Polynomial const &b, COEFF const &s) {
        Polynomial result;
        for (auto i = 0; i < Degree; ++i)
            result.coeff[i] = a.coeff[i] + b.coeff[i] * s;
        return result;
    }
    /// @brief Multiply-subtract
    /// @param a 
    /// @param b 
    /// @param s 
    /// @return a - b * s
    static Polynomial multiplySubtract(Polynomial const &a, Polynomial const &b, COEFF const &s) {
        Polynomial result;
        for (auto i = 0; i < Degree; ++i)
            result.coeff[i] = a.coeff[i] - b.coeff[i] * s;
        return result;
    }
    friend Polynomial operator +(Polynomial const &a, Polynomial const &b) {
        Polynomial result;
        for (auto i = 0; i < Degree; ++i)
            result.coeff[i] = a.coeff[i] + b.coeff[i];
        return result;
    }
    friend Polynomial operator -(Polynomial const &a, Polynomial const &b) {
        Polynomial result;
        for (auto i = 0; i < Degree; ++i)
            result.coeff[i] = a.coeff[i] - b.coeff[i];
        return result;
    }
    static Polynomial zero() {
        Polynomial result;
        for (auto i = 0; i < Degree; ++i)
            result.coeff[i] = COEFF::zero();
        return result;
    }
    static Polynomial one() {
        Polynomial result{zero()};
        result.coeff[0] = COEFF::one();
        return result;
    }
    /// @brief Shift all coefficients left by one place, i.e. Multiply by x.
    /// @return the coefficient that was shifted out.
    COEFF shiftLeft() {
        auto const result = coeff[Degree - 1];
        for (auto i = Degree - 1; i > 0; --i)
            coeff[i] = coeff[i - 1];
        coeff[0] = COEFF::zero();
        return result;
    }
    /// @brief Shift all coefficients right by one place, i.e. Divide by x.
    /// @return the coefficient that was shifted out.
    COEFF shiftRight() {
        auto const result = coeff[0];
        for (auto i = 0; i < Degree - 1; ++i)
            coeff[i] = coeff[i + 1];
        coeff[Degree - 1] = COEFF::zero();
        return result;
    } 
};

template <typename COEFF, int Degree, int ModulusMask>
struct GF_Poly: public Polynomial<COEFF, Degree> {
    using Base = Polynomial<COEFF, Degree>;

    static Base const &Modulus() {
        static auto const modulus = Base::createFromMask(ModulusMask);
        return modulus;
    }

    GF_Poly() = default;
    GF_Poly(size_t count, uint8_t const *bytes)
    : Base(count, bytes)
    {}
    GF_Poly(Base &&other) : Base{other} {};

    friend GF_Poly operator *(GF_Poly a, GF_Poly b) {
        auto y = GF_Poly{Base::zero()};
        while (b) {
            y = Base::multiplyAdd(y, a, b.shiftRight());
            auto const a_hi = a.shiftLeft();
            a = Base::multiplySubtract(a, Modulus(), a_hi);
        }
        return y;
    }
};

struct QNAME_Counter final
{
private:
    static_assert(sizeof(GF_Poly_2_8) == 1);

    /// @brief A 64-bit field extension, made out of 8 8-bit ones. The modulus is * x^8 + x^4 + x^3 + x + 1 *.
    using GF_Poly_2_8x8 = GF_Poly<GF_Poly_2_8, 8, 0x1b>;
    static_assert(sizeof(GF_Poly_2_8x8) == 8);

    /// @brief A 256-bit field extension, made out of 4 64-bit ones. The modulus is * x^4 + x^3 + 1 *.
    using GF_Poly_2_8x8x4 = GF_Poly<GF_Poly_2_8x8, 4, 0x9>;
    static_assert(sizeof(GF_Poly_2_8x8x4) == 32);

    struct FNV1a {
        uint64_t h;

        FNV1a()
        : h(UINT64_C(0xcbf29ce484222325))
        {}
        
        void update(char const ch) {
            auto const chu = (uint8_t)ch;

            // normal FNV-1a
            h = (h ^ chu) * UINT64_C(0x100000001B3);
        }
        void update(std::string_view from) {
            for (auto ch : from)
                update(ch);
        }
        void getValue(uint8_t *result) const {
            auto x = h;
            for (auto i = 0; i < 8; ++i) {
                result[i] = (uint8_t)x;
                x >>= 8;
            }
        }
        static void hash(uint8_t *result, std::string_view qname, std::string_view group, bool grouped) {
            auto hasher = FNV1a{};
            
            if (grouped) {
                hasher.update(group);
                hasher.update('\t');
            }
            hasher.update(qname);
            hasher.getValue(result);
        }
    };
    class Counter {
        static GF_Poly_2_8x8x4 computeInitializationVector() {
            static char const *iv[] = {
                "This is the initialization vector:",
                "There is nothing up my sleeve",
                "but some pi.",
                "314159265358979323846"
            };
            GF_Poly_2_8x8x4 result{GF_Poly_2_8x8x4::one()};
            uint8_t u[32];

            GF_Poly_2_8::test();
            std::memset(u, GF_Poly_2_8::zero().coeff, sizeof(u));

            for (auto i = 0; i < 4; ++i) {
                FNV1a::hash(u, iv[i], "", false);
                result = result * GF_Poly_2_8x8x4(sizeof(u), u);
            }

            return result;
        }
        static GF_Poly_2_8x8x4 initializationVector() { 
            static auto const iv{computeInitializationVector()};
            return iv;
        };
        static void removeIV(uint8_t *value) {
            uint8_t u[32];
            initializationVector().getBytes(32, u);
            for (auto i = 0; i < 32; ++i)
                value[i] ^= u[i];
        }
    public:
        GF_Poly_2_8x8x4 all, grp;

        Counter()
        : all{initializationVector()}
        , grp{initializationVector()}
        {}
        void add(std::string_view qname, std::string_view group, bool hasGroup) {
            uint8_t u[32];

            std::memset(u, GF_Poly_2_8::zero().coeff, sizeof(u));
            u[0] = u[9] = GF_Poly_2_8::one().coeff;

            FNV1a::hash(u + 1, qname, group, false);
            all = all * GF_Poly_2_8x8x4(sizeof(u), u);

            if (hasGroup) {
                FNV1a::hash(u + 1, qname, group, true);
                grp = grp * GF_Poly_2_8x8x4(sizeof(u), u);
            }
        }
        void getAll(uint8_t *value) const {
            all.getBytes(32, value);
            removeIV(value);
        }
        void getGrouped(uint8_t *value) const {
            grp.getBytes(32, value);
            removeIV(value);
        }
    };
    Counter counter;
public:
    /// @brief Add a QNAME to the counter.
    /// @param qname the QNAME to add.
    void add(std::string_view qname) {
        counter.add(qname, qname, false);
    }

    /// @brief Add a grouped QNAME to the counter.
    /// @param qname the QNAME to add.
    /// @param group the group of the QNAME.
    void add(std::string_view qname, std::string_view group) {
        counter.add(qname, group, !group.empty());
    }
    
    void getAll(uint8_t *value) const { counter.getAll(value); }
    void getGrouped(uint8_t *value) const { counter.getGrouped(value); }
};
