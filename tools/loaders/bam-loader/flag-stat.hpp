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

#include <utility>
#include <cstdint>

class FlagStat {
    struct Counter {
        uint64_t value;
        Counter() : value(0) {};
        void updateIf(bool b) {
            value += b ? 1 : 0;
        }
        Counter &operator ++() { value += 1; return *this; }
        operator uint64_t() const { return value; }
    };
    struct BitsCounter {
        Counter counter[16];
        
        void add(uint16_t bits) {
            for (int i = 0; i < 16 && bits != 0; ++i, bits >>= 1)
                counter[i].updateIf(bits & 1);
        }
        void copy(uint64_t *const counts) const {
            for (int i = 0; i < 16; ++i)
                counts[i] = counter[i];
        }
    };
    BitsCounter raw, canonical;
public:
    
    /// @brief Add the set bits to the counter.
    /// returns true if flag bits were canonically set.
    bool add(uint16_t const bits) {
        // these bits are unassigned for not-paired-end tech
        uint16_t const singleReadMask = 0x002 | 0x008 | 0x020 | 0x040 | 0x080;
        // these bits are unassigned for unaligned records
        uint16_t const unalignedMask  = 0x002 | 0x100 | 0x800;
        auto const isSingleRead = (bits & 0x001) == 0;
        auto const isUnaligned  = (bits & 0x004) != 0;
        uint16_t const mask = (isSingleRead ? singleReadMask : 0)
                            | (isUnaligned  ? unalignedMask : 0)
                            | 0xF000; // these bits are unassigned
        uint16_t const cbits = bits & ~mask;

        raw.add(bits);
        canonical.add(cbits);

        return cbits == bits;
    }
    void rawCounts(uint64_t counts[16]) const {
        raw.copy(&counts[0]);
    }
    void canonicalCounts(uint64_t counts[16]) const {
        canonical.copy(&counts[0]);
    }
    static char const *flagBitSymbolicName(int const bitNumber) {
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
        };
        auto constexpr const N = unsigned(sizeof(values)/sizeof(values[0]));
        assert(0 <= bitNumber && bitNumber < N);
        return (bitNumber < N) ? values[bitNumber] : "INVALID_FLAG_BIT";
    }
    static char const *flagBitDescription(int const bitNumber) {
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
        auto constexpr const N = unsigned(sizeof(values)/sizeof(values[0]));
        assert(0 <= bitNumber);
        return (bitNumber < N) ? values[bitNumber] : flagBitSymbolicName(bitNumber);
    }
};
