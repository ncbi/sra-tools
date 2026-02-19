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

struct QNAME_Counter final
{
private:
    struct NameHasher {
        /// two 64-bit FNV-1a hashes
        uint64_t h1, h2;

        /// a 32-bit FNV-1a hash,
        /// initialized with the length of the name,
        /// used like a counter,
        /// to make h2 diverge from h1.
        uint32_t h3;
        
        NameHasher(size_t const size)
        : h1(UINT64_C(0xcbf29ce484222325))
        , h2(UINT64_C(0xcbf29ce484222325))
        , h3((uint32_t)size ^ UINT32_C(0x811c9dc5))
        {}
        
        void update(char const ch) {
            auto const chu = (uint8_t)ch;

            // normal FNV-1a
            h1 = (h1 ^ chu) * UINT64_C(0x100000001B3);
        
            // add h3 to make h1 and h2 diverge
            h2 = (h2 ^ chu ^ h3) * UINT64_C(0x100000001B3);

            h3 *= UINT32_C(0x01000193);
        }
        void update(std::string_view from) {
            for (auto ch : from)
                update(ch);
        }
        /// FNV-1a hashes have poor mixing in the low order byte
        /// and should be folded; there are 2 hashes and thus
        /// 4 halves that can be mixed together to make 4 32-bit hashes.
        void getValue(uint32_t result[4]) const {
            uint32_t const h1_hi = h1 >> 32;
            uint32_t const h1_lo = h1;
            uint32_t const h2_hi = h2 >> 32;
            uint32_t const h2_lo = h2;

            result[0] = h1_hi ^ h1_lo;
            result[1] = h2_hi ^ h2_lo;
            result[2] = h2_hi ^ h1_lo;
            result[3] = h1_hi ^ h2_lo;
        }
        static void hash(uint32_t result[4], std::string_view qname, std::string_view group, bool grouped) {
            auto const size = (uint32_t)qname.size() + (grouped ? (uint32_t)(group.size() + 1) : 0);
            auto hasher = NameHasher(size);
            
            if (grouped) {
                hasher.update(group);
                hasher.update('\t');
            }
            hasher.update(qname);
            hasher.getValue(result);
        }
    };
    class Counter {
        class gf128 {
            uint64_t lo, hi;
            
            void mod() {
                uint64_t constexpr mod[2] = { /** TODO: values for the modulus */ };
                lo ^= mod[0];
                hi ^= mod[1];
            }
            void timesX() {
                auto const m = (bool)(hi >> 63);
                hi <<= 1;
                hi ^= lo >> 63;
                lo <<= 1;
                if (m) mod();
            }
            bool divX() {
                auto const m = lo & 1;
                lo >>= 1;
                lo ^= hi << 63;
                hi >>= 1;
                return m == 1;
            }
            gf128 operator ^=(gf128 const &rhs) {
                lo ^= other.lo;
                hi ^= other.hi;
            }
        public:
            gf128(gf128 const &rhs) : lo(rhs.lo), hi(rhs.hi) {}
            explicit gf128(uint64_t p_lo, uint64_t p_hi = 0)
            : lo(p_lo), hi(p_hi) {}
            bool operator !() const { return lo == 0 && hi == 0; }
            operator bool() const { return !!*this; }
            gf128 &operator *=(gf128 const &rhs) {
                auto a = *this;
                auto b = rhs;
                lo = hi = 0;
                while (a && b) {
                    if (b.divX())
                        *this ^= a;
                    a.timesX();
                }
                return *this;
            }
        };
    public:
        Counter()
        {}
        void add(std::string_view qname, std::string_view group, bool grouped) {
            uint32_t bit[4]; NameHasher::hash(bit, qname, group, grouped);
            uint64_t u64[4] = {
                ((UINT64_C(0x100000000) ^ bit[0]) << 1) ^ 1,
                ((UINT64_C(0x100000000) ^ bit[1]) << 1) ^ 1,
                ((UINT64_C(0x100000000) ^ bit[2]) << 1) ^ 1,
                ((UINT64_C(0x100000000) ^ bit[3]) << 1) ^ 1,
            };
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

    /// @brief Get the counts.
    /// @param counts result
    /// @note The indicies of the counts match the indices for `getName` and `getDescription`.
    void getCounts(uint64_t counts[10]) const {
        std::memset(counts, 0, 10 * sizeof(counts[0]));
        counts[0] = counter.total;
        counts[1] = counter.uniq[0];
        counts[2] = counter.dups;
        std::memmove(&counts[3], counter.matches, 3 * sizeof(counts[3]));
        counts[6] = std::floor(counter.sum_false_positive);
        std::memmove(&counts[7], &counter.uniq[1], 3 * sizeof(counts[7]));
    }

    /// @brief Get the unique names of the counts.
    /// @param i the index of the count.
    /// @return the unique name of the count, a static value (do not free).
    /// @note The indicies of the counts match the indices in `getCounts` and `getDescription`.
    static char const *getName(int const i)
    {
        static char const *names[] = {
            "Total",
            "Unique",
            "Duplicates",
            "HashMatch_1",
            "HashMatch_2",
            "HashMatch_3",
            "FalsePositives",
            "FP_999",
            "FP_99",
            "FP_9",
        };
        return (i >= 0 && i < 10) ? names[i] : nullptr;
    }

    /// @brief Get the informative descriptions of the counts.
    /// @param i the index of the count.
    /// @return the informative description of the count, a static value (do not free).
    /// @note The indicies of the counts match the indices in `getName` and `getCounts`.
    static char const *getDescription(int const i)
    {
        static char const *descriptions[] = {
            "Total number of QNAMEs counted",
            "Number of definitely unique QNAMEs",
            "Number of QNAMEs with all matching hash values",
            "Number of QNAMEs with 1 matching hash value",
            "Number of QNAMEs with 2 matching hash values",
            "Number of QNAMEs with 3 matching hash values",
            "Likely number of falsely matching QNAMEs",
            "Number of likely (0.999 <= P < 1.0) unique QNAMEs",
            "Number of likely (0.99 <= P < 0.999) unique QNAMEs",
            "Number of likely (0.9 <= P < 0.99) unique QNAMEs",
        };
        return (i >= 0 && i < 10) ? descriptions[i] : nullptr;
    }
};
