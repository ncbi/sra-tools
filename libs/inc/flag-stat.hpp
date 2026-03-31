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

struct FLAG_Counter final
{
private:
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

        /// @brief Map FLAG set bits to flagstat categories. This is loosely patterned after flagstat_loop in samtools/bam_stat.c
        /// @tparam F 
        /// @param fail result is filtered to matching 0x200 FLAG bit.
        /// @param f f is called with flagstat category if FLAG matches the criteria for that category.
        template <typename F>
        void flagStat(bool fail, F && f) const {
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
    using Counter = std::map<uint16_t, uint64_t>;
    Counter raw, canonical;
    uint64_t totalAdds, nonCanonical;
    
    void add(Counter &counter, uint16_t flag) {
        counter[flag] += 1;
    }
    uint16_t canonicalized(uint16_t flag) {
        if ((flag & 0x004) != 0)
            flag ^= flag & (0x002 | 0x100 | 0x800);
        if ((flag & 0x001) == 0)
            flag ^= flag & (0x002 | 0x008 | 0x020 | 0x040 | 0x080);
        return flag & 0xFFF;
    }
    static void getCounts(uint64_t counts[14], Counter const &counter, bool QC_fail) {
        std::memset(counts, 0, 14 * sizeof(counts[0]));
        for (auto c : counter) {
            Flag{c.first}.flagStat(QC_fail, [&](int i) {
                counts[i] += c.second;
            });
        }
    }
public:
    /// @brief Add a FLAG to the counters.
    /// @param flag the FLAG to add.
    void add(uint16_t const flag) {
        auto const cflag = canonicalized(flag);
        totalAdds += 1;
        if (cflag != flag)
            nonCanonical += 1;
        add(raw, flag);
        add(canonical, cflag);
    }

    /// @brief Get the raw flag stats (equivalent to samtools flagstats).
    /// @param counts result.
    /// @param QC_fail filter to matching 0x200 FLAG bit.
    /// @note The indicies of the counts match the indices for `getName` and `getDescription`.
    void getFlagStat(uint64_t counts[14], bool QC_fail) const {
        getCounts(counts, raw, QC_fail);
    }

    /// @brief Get the canonical flag stats.
    /// @param counts result.
    /// @param QC_fail filter to matching 0x200 FLAG bit.
    /// @note The indicies of the counts match the indices for `getName` and `getDescription`.
    void getCanonicalFlagStat(uint64_t counts[14], bool QC_fail) const {
        getCounts(counts, canonical, QC_fail);
    }

    /// @brief Get the unique names of the counts.
    /// @param i the index of the count.
    /// @return the unique name of the count, a static value (do not free).
    /// @note The indicies of the counts match the indices in `getCounts` and `getDescription`.
    static char const *getName(int const i)
    {
        static char const *names[] = {
            "reads",
            "primary",
            "secondary",
            "supplementary",
            "duplicate",
            "primary_dup",
            "mapped",
            "primary_mapped",
            "paired",
            "read1",
            "read2",
            "proper_pair",
            "both_mapped",
            "singletons"
        };
        auto constexpr N = sizeof(names) / sizeof(names[0]);
        return (i >= 0 && i < N) ? names[i] : nullptr;
    }

    /// @brief Get the informative descriptions of the counts.
    /// @param i the index of the count.
    /// @return the informative description of the count, a static value (do not free).
    /// @note The indicies of the counts match the indices in `getName` and `getCounts`.
    static char const *getDescription(int const i)
    {
        static char const *descriptions[] = {
            "in total",
            "primary",
            "secondary",
            "supplementary",
            "duplicates",
            "primary duplicates",
            "mapped",
            "primary_mapped",
            "paired in sequencing",
            "read1",
            "read2",
            "properly paired",
            "with itself and mate mapped",
            "singletons"
        };
        auto constexpr N = sizeof(descriptions) / sizeof(descriptions[0]);
        return (i >= 0 && i < N) ? descriptions[i] : nullptr;
    }

    friend struct FlagStatText;
};

struct FlagStatText {
    enum Version {
        v_1_3,
        v_1_13 ///< added primary, primary duplicates, and primary mapped
    };

    std::string defaultText;

private:
    using FlagStat = FLAG_Counter::FlagStat;

    struct Line {
        char value[1024];
    private:
        /// @brief Based on `percent` in samtools/bam_stat.c
        /// @note Should format the same as `samtools flagstat`.
        struct PctString {
            char value[16];
            PctString(uint64_t num, uint64_t denom) {
                if (denom)
                    std::snprintf(value, sizeof(value), "%.2f%%", (float)(num * 100.0 / denom));
                else
                    std::snprintf(value, sizeof(value), "N/A");
            }
        };
    public:
        Line(char const *const fmt, uint64_t pass, uint64_t fail) {
            auto const n = std::snprintf(value, sizeof(value), fmt, (long long)pass, (long long)fail);
            assert(n < sizeof(value));
        }
        Line(char const *const fmt, uint64_t pass, uint64_t passTotal, uint64_t fail, uint64_t failTotal) {
            PctString const passPct{pass, passTotal};
            PctString const failPct{fail, failTotal};
            auto const n = std::snprintf(value, sizeof(value), fmt, (long long)pass, (long long)fail, passPct.value, failPct.value);
            assert(n < sizeof(value));
        }
        void appendTo(std::string &output) const {
            output.append(value);
        }
    };

public:
    /// @brief The equivalent of `samtools flagstat | head -n -2`
    /// @param counter contains the FLAG counts.
    /// @param version 1.13 added some fields to the output.
    /// @return should match `samtools flagstat | head -n -2`
    explicit FlagStatText(FLAG_Counter const &counter, FlagStatText::Version version = v_1_13)
    : defaultText({})
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
        uint64_t pass[14];
        uint64_t fail[14];

        counter.getFlagStat(pass, false);
        counter.getFlagStat(fail, true );

        auto addLine = [this, pass, fail](FlagStat which) {
            Line{fmt[which], pass[which], fail[which]}
                .appendTo(defaultText);
        };
        auto addLinePct = [this, pass, fail](FlagStat which, FlagStat whichTotal) {
            Line{fmt[which], pass[which], pass[whichTotal], fail[which], fail[whichTotal]}
                .appendTo(defaultText);
        };

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
    }
};
