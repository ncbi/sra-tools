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
 *  Generate SAM FLAG bits counts.
 */

#include <array>
#include <utility>
#include <cstdint>

class FlagStat {
    struct Counter {
        uint64_t value;
        Element() : value(0) {};
        void updateIf(bool b) {
            value += b ? 1 : 0;
        }
        Counter &operator ++() { value += 1; return *this; }
        operator uint64_t() const { return value; }
    };
    struct BitsCounter {
        std::array<Counter, 16> counter;
        
        void add(uint16_t bits) {
            for (auto i = 0; i < 16 && bits != 0; ++i, bits >>= 1)
                counter[i].updateIf(bits & 1);
        }
        void copy(uint64_t counts[16]) const {
            for (auto i = 0; i < 16; ++i)
                counts[i] = counter[i];
        }
    };
    BitsCounter raw, canonical;
public:
    
    /// @brief Add the set bits to the counter.
    /// returns true if flag bits were canonically set.
    bool add(uint16_t const bits) {
        uint16_t unmask = 0xF000;
        if ((bits & 0x001) == 0) {
            unmask |= 0x002 | 0x008 | 0x020 | 0x040 | 0x080; 
        }
        if ((bits & 0x004) != 0) {
            unmask |= 0x800 | 0x100 | 0x002;
        }
        uint16_t const mask = ~unmask;

        raw.add(bits);
        canonical.add(bits & mask);

        return (bits & mask) == bits;
    }
    void rawCounts(uint64_t counts[16]) const {
        raw.copy(counts);
    }
    void canonicalCounts(uint64_t counts[16]) const {
        canonical.copy(counts);
    }
    static char const *flagBitSymbolicName(uint16_t bitNumber) {
        static char const *const values[] = {
            "MULTIPLE_READS",
            "PROPERLY_ALIGNED",
            "UNMAPPED",
            "NEXT_UNMAPPED",
            "REVERSE_COMPLEMENTED",
            "NEXT_REVERSE_COMPLEMENTED",
            "FIRST_SEGMENT",
            "LAST_SEGMENT",
            "SECONDARY_ALIGNMENT",
            "QC_FAILED",
            "DUPLICATE",
            "SUPPLEMENTARY_ALIGNMENT",
            "BIT_12_UNUSED",
            "BIT_13_UNUSED",
            "BIT_14_UNUSED",
            "BIT_15_UNUSED",
            "INVALID_FLAG_BIT"
        };
        auto constexpr const N = sizeof(values)/sizeof(values[0]) - 1;
        return (size_t)bitNumber < N ? values[bitNumber] : values[N];
    }
    static char const *flagBitDescription(uint16_t bitNumber) {
        static char const *const values[] = {
            "template having multiple segments in sequencing",
            "each segment properly aligned according to the aligner",
            "segment unmapped",
            "next segment in the template unmapped",
            "SEQ being reverse complemented",
            "SEQ of the next segment in the template being reverse complemented",
            "the first segment in the template",
            "the last segment in the template",
            "secondary alignment",
            "not passing filters, such as platform/vendor quality controls",
            "PCR or optical duplicate",
            "supplementary alignment"
        };
        auto constexpr const N = sizeof(values)/sizeof(values[0]);
        return (size_t)bitNumber < N ? values[bitNumber] : flagBitSymbolicName(bitNumber);
    }
};
