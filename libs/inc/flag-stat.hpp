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
 *  C++ implementation for SAM FLAG counts.
 */

#include <map>
#include <string>
#include <string_view>
#include <cstdio>
#include <cstring>
#include <cassert>
#include "hashing.hpp"
#include "JSON_ostream.hpp"

struct FLAG_Counter final
{
private:
    std::map<uint16_t, uint64_t> counter;
    
public:
    FLAG_Counter() = default;
    
    /// @brief Construct from a source of counts. This could be used to load from metadata.
    /// @tparam Source
    /// @param source a functional, which takes the flag value and returns the count, or zero.
    template <typename Source>
    explicit FLAG_Counter(Source && source)
    : counter({})
    {
        auto it = counter.begin();
        for (int flag = 0; flag <= 0xFFFF; ++flag) {
            uint64_t const count = source((uint16_t)flag);
            if (count > 0)
                it = counter.emplace_hint(it, (uint16_t)flag, count);
        }
    }
    
    /// @brief Visit the counts. This could be used to save the counts to metadata.
    /// @tparam F
    /// @param f a functional, which takes the flag value and the count.
    template <typename F>
    void for_each(F && f) const {
        for (auto const & c : counter)
            sink(c.first, c.second);
    }
    
    /// @brief Add a FLAG to the counters.
    /// @param flag the FLAG to add.
    void add(uint16_t const flag, uint64_t count = 1) {
        counter[flag] += count;
    }
};

struct FlagStatText {
    enum Version {
        v_1_3,
        v_1_13 ///< added primary, primary duplicates, and primary mapped
    };

private:
    std::string value;
    Version vers;
    
    char const *versString() const {
        return vers >= v_1_13 ? "1.13" : "1.3";
    }

    /// @brief These are the fields, in order, that `samtools flagstat` outputs.
    enum FlagStat {
        total = 0,
        primary,
        secondary,
        supplementary,
        duplicate,
        primary_duplicate,
        mapped,
        primary_mapped,
        paired,
        read1,
        read2,
        proper_pair,
        mapped_pair,
        singleton,
    };
    struct Flag {
        uint16_t value;

        bool isPaired()       const { return (value & 0x001) != 0; }
        bool isProperPair()   const { return (value & 0x002) != 0; }
        bool isUnmapped()     const { return (value & 0x004) != 0; }
        bool isMateUnmapped() const { return (value & 0x008) != 0; }
        bool isRevCmp()       const { return (value & 0x010) != 0; }
        bool isMateRevCmp()   const { return (value & 0x020) != 0; }
        bool isRead1()        const { return (value & 0x040) != 0; }
        bool isRead2()        const { return (value & 0x080) != 0; }
        bool isSecondary()    const { return (value & 0x100) != 0; }
        bool isFailing()      const { return (value & 0x200) != 0; }
        bool isDuplicate()    const { return (value & 0x400) != 0; }
        bool isSupplemental() const { return (value & 0x800) != 0; }

        bool isMapped()       const { return !isUnmapped(); }
        bool isMateMapped()   const { return !isMateUnmapped(); }
        bool isPrimary()      const { return !isSecondary() && !isSupplemental(); }
        bool isPassing()      const { return !isFailing(); }
        
        /// @brief Map FLAG set bits to flagstat categories. This is loosely patterned after `flagstat_loop` in `samtools/bam_stat.c`
        /// @tparam F
        /// @param fail result is filtered to matching 0x200 FLAG bit.
        /// @param f f is called with flagstat category if FLAG matches the criteria for that category.
        template <typename F>
        void flagStat(bool const fail, F && f) const {
            if (isFailing() == fail) {
                f(FlagStat::total);
                if (isMapped())
                    f(FlagStat::mapped);
                if (isDuplicate())
                    f(FlagStat::duplicate);
                if (isSecondary())
                    f(FlagStat::secondary);
                else if (isSupplemental())
                    f(FlagStat::supplementary);
                else {
                    f(FlagStat::primary);
                    if (isMapped())
                        f(FlagStat::primary_mapped);
                    if (isDuplicate())
                        f(FlagStat::primary_duplicate);
                    if (isPaired()) {
                        f(FlagStat::paired);
                        if (isRead1())
                            f(FlagStat::read1);
                        if (isRead2())
                            f(FlagStat::read2);
                        if (isMapped()) {
                            if (isProperPair())
                                f(FlagStat::proper_pair);
                            if (isMateMapped())
                                f(FlagStat::mapped_pair);
                            else
                                f(FlagStat::singleton);
                        }
                    }
                }
            }
        }
    };

    /// @brief Based on `percent` in `samtools/bam_stat.c`
    /// @note Should format the same as `samtools flagstat`.
    struct PctString {
        char value[16];
        PctString(long long num, long long denom) {
            if (denom)
                std::snprintf(value, sizeof(value), "%.2f%%", (float)(num * 100.0 / denom));
            else
                std::snprintf(value, sizeof(value), "N/A");
        }
    };

public:
    std::string const &get() const { return value; }

    /// @brief The equivalent of `samtools flagstat | head -n -2`
    /// @param counter contains the FLAG counts.
    /// @param version 1.13 added some fields to the output.
    /// @return should match `samtools flagstat | head -n -2`
    explicit FlagStatText(FLAG_Counter const &counter, FlagStatText::Version version = v_1_13)
    : value({})
    , vers(version)
    {
        static char const *const fmt[] = {
            "%lld + %lld in total (QC-passed reads + QC-failed reads)\n",
            "%lld + %lld primary\n",
            "%lld + %lld secondary\n",
            "%lld + %lld supplementary\n",
            "%lld + %lld duplicates\n",
            "%lld + %lld primary duplicates\n",
            "%lld + %lld mapped (%s : %s)\n",
            "%lld + %lld primary mapped (%s : %s)\n",
            "%lld + %lld paired in sequencing\n",
            "%lld + %lld read1\n",
            "%lld + %lld read2\n",
            "%lld + %lld properly paired (%s : %s)\n",
            "%lld + %lld with itself and mate mapped\n",
            "%lld + %lld singletons (%s : %s)\n"
        };
        long long pass[14];
        long long fail[14];
        auto addLine = [this, pass, fail](FlagStat which) {
            char line[1024];
            auto const n = std::snprintf(line, sizeof(line), fmt[which], pass[which], fail[which]);
            assert(0 < n && (size_t)n < sizeof(line));
            value.append(line, n);
        };
        auto addLinePct = [this, pass, fail](FlagStat which, FlagStat whichTotal) {
            char line[1024];
            PctString const passPct{pass[which], pass[whichTotal]};
            PctString const failPct{fail[which], fail[whichTotal]};
            auto const n = std::snprintf(line, sizeof(line), fmt, pass[which], fail[which], passPct.value, failPct.value);
            assert(0 < n && (size_t)n < sizeof(line));
            value.append(line, n);
        };

        value.reserve(1024); ///< should be more than enough, a typical `samtools flagstat` is less than 500 characters.
        std::memset(pass, 0, sizeof(pass));
        std::memset(fail, 0, sizeof(fail));
        
        counter.for_each([&](uint16_t const flag, uint64_t const count) {
            Flag{flag}.flagStat(false, [&](int i) { pass[i] += (long long)count; });
        });

        counter.for_each([&](uint16_t const flag, uint64_t const count) {
            Flag{flag}.flagStat(true, [&](int i) { fail[i] += (long long)count; });
        });

        addLine(FlagStat::total);
        if (version >= v_1_13)
            addLine(FlagStat::primary);
        addLine(FlagStat::secondary);
        addLine(FlagStat::supplementary);
        addLine(FlagStat::duplicate);
        if (version >= v_1_13)
            addLine(FlagStat::primary_duplicate);
        addLinePct(FlagStat::mapped, FlagStat::total);
        if (version >= v_1_13)
            addLinePct(FlagStat::primary_mapped, FlagStat::primary);
        addLine(FlagStat::paired);
        addLine(FlagStat::read1);
        addLine(FlagStat::read2);
        addLinePct(FlagStat::proper_pair, FlagStat::paired);
        addLine(FlagStat::mapped_pair);
        addLinePct(FlagStat::singleton, FlagStat::paired);

        value.shrink_to_fit();
    }
    
    /// @brief How the fingerprint can be generated.
    static std::string kind() {
        return "flagstat";
    }

    /// @brief How the fingerprint can be generated.
    static std::string format() {
        return "samtools flagstat | head -n -2";
    }
    
    /// @brief How the digest was computed.
    static std::string algorithm() {
        return "SHA-256";
    }
    
    std::string version() const {
        return versString();
    }
    
    std::string digest() const {
        auto result = SHA256::hash(value).string();
        for (auto && ch : result)
            ch = (char)std::tolower(ch);
        return result;
    }
    
    /// @brief Writes the canonical form to the stream.
    /// @param strm the stream to which to write.
    /// @return the stream which was passed in.
    /// @note It is incumbent on the caller to provide the JSON object context.
    JSON_ostream &canonicalForm(JSON_ostream &strm) {
        return strm
                << JSON_Member{"fingerprint-version"} << version()
                << JSON_Member{"fingerprint-format"} << format()
                << JSON_Member{"fingerprint-digest-algorithm"} << algorithm()
                << JSON_Member{"fingerprint-digest"} << digest();
    }
};
