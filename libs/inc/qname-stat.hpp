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
    class Counter {
    public:
        static constexpr auto MASK_BITS = 30;
        static constexpr auto MASK = (1UL << MASK_BITS) - 1;
        uint64_t total; ///< total number of `add`s
        uint64_t uniq[4];
        uint64_t matches[3];
        uint64_t dups;
        double sum_false_positive;
    private:
        std::vector<uint64_t> filter;
        
        void set(uint32_t bit) {
            auto const elem = (bit & MASK) >> 6;
            auto const mask = UINT64_C(1) << (bit & 0x3F);
            filter[elem] |= mask;
        }
        template<typename... Ts>
        void set(uint32_t bit1, Ts... bits) {
            set(bit1);
            set(bits...);
        }
        int test(uint32_t bit) {
            auto const elem = (bit & MASK) >> 6;
            auto const mask = UINT64_C(1) << (bit & 0x3F);
            return (filter[elem] & mask) ? 1 : 0;
        }
        template<typename... Ts>
        int test(uint32_t bit1, Ts... bits) {
            return test(bit1) + test(bits...);
        }
        size_t size() const {
            return (1ULL << (MASK_BITS - 6)) * sizeof(uint64_t);
        }
        double capacity() const {
            return (1ULL << MASK_BITS);
        }
        double filled() const {
            return double(uniq[0]) / capacity();
        }
        double false_positive_rate() const {
            return pow(1.0 - exp(-4.0 * filled()), 4.0);
        }
    public:
        Counter()
        : total(0)
        , dups(0)
        , sum_false_positive(0.0)
        {
            matches[0] = matches[1] = matches[2] = 0;
            uniq[0] = uniq[1] = uniq[2] = uniq[3] = 0;

            static_assert(MASK_BITS < sizeof(int) * 8);
            filter.resize(1UL << (MASK_BITS - 6), 0);
        }
        void add(std::string_view qname, std::string_view group, bool grouped) {
            auto const size = (uint32_t)qname.size() + (grouped ? (uint32_t)(group.size() + 1) : 0);

            // two 64-bit FNV-1a hashes
            uint64_t h1{UINT64_C(0xcbf29ce484222325)};
            uint64_t h2{UINT64_C(0xcbf29ce484222325)};
            
            // a 32-bit FNV-1a hash, initialized with the length of the name, used like a counter
            uint32_t h3{size ^ UINT32_C(0x811c9dc5)};
            
            auto const && append_byte = [&](char const ch) {
                auto const chu = (uint8_t)ch;

                // normal FNV-1a
                h1 = (h1 ^ chu) * UINT64_C(0x100000001B3);
            
                // add h3 to make h1 and h2 diverge
                h2 = (h2 ^ chu ^ h3) * UINT64_C(0x100000001B3);

                h3 *= UINT32_C(0x01000193);
            };
            auto const && append = [&](std::string_view from) {
                for (auto ch : from)
                    append_byte(ch);
            };
            
            if (grouped) {
                append(group);
                append_byte('\t');
            }
            append(qname);

            // FNV-1a hashes have poor mixing in the low order byte
            // and should be folded; there are 2 hashes and thus
            // 4 halves that can be mixed together to make 4 32-bit hashes.

            uint32_t const h1_hi = h1 >> 32;
            uint32_t const h1_lo = h1;
            uint32_t const h2_hi = h2 >> 32;
            uint32_t const h2_lo = h2;

            auto const bit1 = h1_hi ^ h1_lo;
            auto const bit2 = h2_hi ^ h2_lo;
            auto const bit3 = h2_hi ^ h1_lo;
            auto const bit4 = h1_hi ^ h2_lo;
            auto const same = test(bit1, bit2, bit3, bit4);

            if (same == 4) {
                // a possibly-false hit
                auto const fpr = false_positive_rate();
                sum_false_positive += fpr;
                ++dups;
                if (fpr < 0.001)
                    ++uniq[1];
                else if (fpr < 0.01)
                    ++uniq[2];
                else if (fpr < 0.1)
                    ++uniq[3];
            }
            else {
                set(bit1, bit2, bit3, bit4);
                ++uniq[0];
                if (same > 0)
                    matches[same - 1] += 1;
            }
            ++total;
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
