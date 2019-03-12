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
 */

#ifndef __FRAGMENT_HPP_INCLUDED__
#define __FRAGMENT_HPP_INCLUDED__ 1

#include <string>
#include <vector>
#include "vdb.hpp"
#include "CIGAR.hpp"
#include "utility.hpp"

/** \class DNASequence
 * \brief A value type for a single nucleatide sequence
 */
struct DNASequence : public std::string {
    static inline char adjoint(char const ch) ///< truely adjoint if ch =~ /[.ACMGRSVTWYHKDBN]/
    {
        /// a short circuit for the common cases
        switch (ch) {
            case 'A': return 'T';
            case 'T': return 'A';
            case 'C': return 'G';
            case 'G': return 'C';
            case 'N': return 'N';
        }
        /// these almost never occur in real data
        switch (ch) {
            case '.': return '.';
            case 'M': return 'K';
            case 'K': return 'M';
            case 'R': return 'Y';
            case 'Y': return 'R';
            case 'S': return 'S';
            case 'V': return 'B';
            case 'B': return 'V';
            case 'W': return 'W';
            case 'H': return 'D';
            case 'D': return 'H';
        }
        return 'N';
    }
    
    static bool inline isAmbiguous(char const ch) {
        switch (ch) {
            case 'A':
            case 'C':
            case 'G':
            case 'T':
                return false;
            default:
                return true;
        }
    }

    bool ambiguous() const {
        for (auto ch : *this) {
            if (isAmbiguous(ch))
                return true;
        }
        return false;
    }

    DNASequence(std::string const &str) : std::string(str) {}
};

/** \class AlignmentShort
 *  \brief Alignment without bases or qualities
 */
struct AlignmentShort {
    std::string reference;
    std::string cigarString;
    CIGAR cigar;
    int64_t row;
    int readNo;
    int position; ///< reference position of first *aligned* base; query[cigar.qfirst] ~ reference[position]
    char strand;
    bool aligned;
    
    /// reference position of first base; query[0] ~ reference[qstart()]
    int qstart() const { return position - cigar.qfirst; }

    /// reference position after last *aligned* base
    int rended() const { return position + cigar.rlength; }

    /// reference position after last base
    int qended() const { return rended() + cigar.qclip; }
    
    int maxended() const { return std::max(qstart() + cigar.qlength, qended()); }
    
    /// this is in reference-space; it is influenced by clipping
    int templateLength(AlignmentShort const &other) const {
        return reference != other.reference
             ? 0
             : (std::max(rended(), other.rended()) - std::min(position, other.position));
    }
    
    /// total length in reference-space
    int referenceLength(AlignmentShort const &other) const { return cigar.rlength + other.cigar.rlength; }
    
    /// total length in query-space
    int queryLength(AlignmentShort const &other) const { return cigar.qlength + other.cigar.qlength; }

    /// this is in reference-space; it is NOT influenced by clipping
    int fragmentLength(AlignmentShort const &other) const {
        return reference != other.reference
             ? queryLength(other)
             : (std::max(qended(), other.qended()) - std::min(qstart(), other.qstart()));
    }

    AlignmentShort(int readNo, int64_t const source)
    : readNo(readNo)
    , row(source)
    , aligned(false)
    , strand(0)
    , position(0)
    {}
    
    AlignmentShort(int readNo, int64_t const source, std::string const &reference, char strand, int position, std::string const &CIGAR)
    : readNo(readNo)
    , row(source)
    , aligned(true)
    , reference(reference)
    , strand(strand)
    , position(position)
    , cigarString(CIGAR)
    , cigar(cigarString)
    {}
    
    friend std::ostream &operator <<(std::ostream &os, AlignmentShort const &a) {
        os << a.readNo;
        if (a.aligned)
            os << '\t' << a.reference << '\t' << a.strand << a.position << '\t' << a.cigarString;
        return os;
    }
    
    friend bool operator <(AlignmentShort const &a, AlignmentShort const &b) {
        if (a.readNo < b.readNo) return true;
        if (b.readNo < a.readNo) return false;
        if (!a.aligned && !b.aligned) return false; ///< they are equal
        if (!a.aligned) return true;
        if (!b.aligned) return false;
        if (a.reference < b.reference) return true;
        if (b.reference < a.reference) return false;
        return a.position < b.position;
    }
    
    bool isClipped(unsigned spos) const {
        return (spos < cigar.qfirst) || (spos > cigar.qfirst + cigar.qlength);
    }
    
    bool isBefore(AlignmentShort const &other) const {
        if (aligned && other.aligned) {
            if (reference < other.reference) return true;
            if (reference != other.reference) return false;
            if (qstart() < other.qstart()) return true;
            if (other.qstart() < qstart()) return false;
            return qended() <= other.qended();
        }
        throw std::logic_error("isBefore requires both arguments to be aligned");
    }
    
    /// both aligned
    bool isAlignedPair(AlignmentShort const &other) const {
        return aligned && other.aligned;
    }
    /// both aligned and not overlapping
    bool isGoodAlignedPair(AlignmentShort const &other) const {
        if (!isAlignedPair(other)) return false;
        auto const fl = fragmentLength(other);
        auto const ql = queryLength(other);
        return ql <= fl;
    }
};

/** \class Alignment
 *  \brief Alignment with bases and (maybe) qualities
 */
struct Alignment : public AlignmentShort {
    DNASequence sequence;
    std::string quality;
    bool syntheticQuality;
    
    static std::string const &SyntheticQuality() {
        static auto const synthetic = std::string();
        return synthetic;
    }
    
    Alignment(int readNo, std::string const &sequence, std::string const &quality, int64_t const source)
    : AlignmentShort(readNo, source)
    , sequence(sequence)
    , quality(quality)
    , syntheticQuality(&quality == &SyntheticQuality())
    {}

    Alignment(int readNo, std::string const &sequence, std::string const &quality, int64_t const source, std::string const &reference, char strand, int position, std::string const &CIGAR)
    : AlignmentShort(readNo, source, reference, strand, position, CIGAR)
    , sequence(sequence)
    , quality(quality)
    , syntheticQuality(&quality == &SyntheticQuality())
    {}

    Alignment truncated() const {
        if (aligned)
            return Alignment(readNo, "", "", row, reference, strand, position, cigarString);
        else
            return Alignment(readNo, "", "", row);
    }

    friend std::ostream &operator <<(std::ostream &os, Alignment const &a) {
        os << a.readNo << '\t' << a.sequence << '\t';
        if (!a.syntheticQuality)
            os << a.quality;
        if (a.aligned)
            os << '\t' << a.reference << '\t' << a.strand << a.position << '\t' << a.cigarString;
        return os;
    }

private:
    struct S {
        char const *seq;
        unsigned length, left, right;

    private:
        S() {}
        /**
         *  \brief the simplest, requires base for base exact match
         */
        static bool equivFwd(char const *a, char const *b, unsigned const n) {
            auto const e = a + n;
            while (a != e) {
                auto const A = *a++;
                auto const B = *b++;
                if (A == B)
                    continue;
                return false;
            }
            return true;
        }
        /**
         *  \brief requires base for base exact match or either base is ambiguous
         */
        static bool equivFwdClipped(char const *a, char const *b, unsigned const n) {
            auto const e = a + n;
            while (a != e) {
                auto const A = *a++;
                auto const B = *b++;
                if (A == B || DNASequence::isAmbiguous(A) || DNASequence::isAmbiguous(B))
                    continue;
                return false;
            }
            return true;
        }
        /**
         *  \brief performs the same comparison as equivFwd, but one of the sequences is processed as reverse complement
         */
        static bool equivRev(char const *a, char const *b, unsigned const n) {
            auto const e = a + n;
            while (a != e) {
                auto const A = *a++;
                auto const B = DNASequence::adjoint(*--b);
                if (A == B)
                    continue;
                return false;
            }
            return true;
        }
        /**
         *  \brief performs the same comparison as equivFwdClipped, but one of the sequences is processed as reverse complement
         */
        static bool equivRevClipped(char const *a, char const *b, unsigned const n) {
            auto const e = a + n;
            while (a != e) {
                auto const A = *a++;
                auto const B = DNASequence::adjoint(*--b);
                if (A == B || DNASequence::isAmbiguous(A) || DNASequence::isAmbiguous(B))
                    continue;
                return false;
            }
            return true;
        }
    public:
        explicit S(Alignment const &o) : seq(o.sequence.data()), length((unsigned)o.sequence.length()), left(o.cigar.qfirst), right(o.cigar.qlength - o.cigar.qclip) {}
        
        /**
         * \brief two sequences are equivalent if every corresponding position is equivalent, there may be clipped regions where ambiguity is allowed to be equivalent to any base
         */
        static bool equivalent(S const &a, S const &b, bool const adjoint) {
            if (a.length != b.length) return false;
            auto const length = a.length;

            if (adjoint) {
                auto const lclip = std::max(a.left, length - b.right);
                auto const rclip = std::min(a.right, length - b.left);
                if (!equivRevClipped(a.seq, b.seq + length, lclip))
                    return false;
                if (!equivRev(a.seq + lclip, b.seq + length - lclip, rclip - lclip))
                    return false;
                if (!equivRevClipped(a.seq + rclip, b.seq + length - rclip, length - rclip))
                    return false;
            }
            else {
                auto const lclip = std::max(a.left, b.left);
                auto const rclip = std::min(a.right, b.right);
                if (!equivFwdClipped(a.seq, b.seq, lclip))
                    return false;
                if (!equivFwd(a.seq + lclip, b.seq + lclip, rclip - lclip))
                    return false;
                if (!equivFwdClipped(a.seq + rclip, b.seq + rclip, length - rclip))
                    return false;
            }
            return true;
        }
        
#if 0
        static void testFwd() {
            auto a = S();
            auto b = S();
            
            a.seq = "TGTTTTGCAATGGAAATTCCTTCAACTGTTAATTTTTTTTTTTTTTTTTTTTAGGAGAAGTCCCCCTCTGTTGCCCACGCTCACATGACA";
            b.seq = "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNTTTTTTTTTTTTTTTTTTTTAGGAGAAGTCCCCCTCTGTTGCCCANNNNNNNNNNNNN";
            a.length = b.length = 90;
            a.left = 0; a.right = 90-41;
            b.left = 32; b.right = 90-13;
            
            auto const eq = S::equivalent(a, b, false);
            if (!eq)
                throw std::logic_error("assertion failed");
        }
        static void testRev() {
            auto a = S();
            auto b = S();
            
            a.seq = "TGTTTTGCAATGGAAATTCCTTCAACTGTTAATTTTTTTTTTTTTTTTTTTTAGGAGAAGTCCCCCTCTGTTGCCCACGCTCACATGACA";
            b.seq = "NNNNNNNNNNNNNTGGGCAACAGAGGGGGACTTCTCCTAAAAAAAAAAAAAAAAAAAANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN";
            a.length = b.length = 90;
            a.left = 0; a.right = 90-41;
            b.left = 13; b.right = 90-32;
            
            auto const eq = S::equivalent(a, b, true);
            if (!eq)
                throw std::logic_error("assertion failed");
        }
#endif
    };
public:
    bool sequenceEquivalentTo(Alignment const &other) const {
        return S::equivalent(S(*this), S(other), strand != other.strand);
    }
#if 0
    static void test() {
        S::testFwd();
        S::testRev();
    }
#endif
};

struct Fragment {
    std::string group;
    std::string name;
    std::vector<Alignment> detail;
    int64_t firstRow;
    
    Fragment(std::string const &group, std::string const &name, std::vector<Alignment> const &algn, int64_t const source)
    : group(group)
    , name(name)
    , detail(algn)
    , firstRow(source)
    {
        std::sort(detail.begin(), detail.end());
    }
    
    unsigned numberOfReads() const {
        unsigned nr = 0;
        int lr = 0;
        for (auto && i : detail) {
            if (nr == 0 || lr != i.readNo) {
                ++nr;
                lr = i.readNo;
            }
        }
        return nr;
    }
    unsigned countOfAlignments(int readNo) const {
        unsigned n = 0;
        for (auto && i : detail) {
            if (i.aligned && i.readNo == readNo)
                ++n;
        }
        return n;
    }
#if 0
    static Fragment testFragment() {
        auto const spotGroup = std::string("06A010111-46676E3D");
        auto const spotName = std::string("661961051");
        auto const firstRow = 1334114741;
        auto const read1 = Alignment(1
                                     , "GGGGTTAGGGTTAGGGTTAGGGGTTAGGGTTAGGGTTAGGGGTTAGGGTTAGGGGTAGGGGTTAGGGTTAGGGTTAGGGGTTAGGGGTGG"
                                     , "DDDC8EEECEEEGGGGCGFEGBGCGDBDE9DAD@D<5?C@EE9CC#############################################"
                                     , firstRow, "chrX", '+', 156030216, "13M1D19M7D56M2S");
        auto const read2 = Alignment(2
                                     , "GGGTTAGGGTTAGGGTTAGGGGTTAGGGTTAGGGTTAGGGGTTAGGGTTAGGGTTAGGGGTTAGGGTTAGGGTTAGGGGTTAGGGTTAGG"
                                     , "######A@=DF+>FDF?FFBFEFE.D@DA==FFFBFEFDCE.FF?FFFHFBHGHGGHFHFCHHHGHHHGHHHHHHHGHHFHHHHFHHEHH"
                                     , firstRow + 1, "chrX", '-', 156030616, "18M1I18M1I18M1I18M1I14M");
        auto const rslt = std::vector<Alignment>({read1, read2});
        return Fragment(spotGroup, spotName, rslt, firstRow);
    }
#endif
    friend std::ostream &operator <<(std::ostream &os, Fragment const &o) {
        os << o.group << '\t' << o.name << '\t' << o.firstRow << '\n';
        for (auto && i : o.detail) {
            os << '\t' << i << '\n';
        }
        return os;
    }
    
    int bestIndex(int readNo) const {
        int i;
        // since all of the sequences are equivalent,
        // the first unambiguous sequence is best
        for (i = 0; i < detail.size(); ++i) {
            if (detail[i].readNo != readNo) continue;
            if (detail[i].sequence.ambiguous()) continue;
            return i;
        }
        // there were no unambiguous sequences
        // the best one will have to the one with the longest query length
        int best = -1;
        int bestIndex = 0;
        for (i = 0; i < detail.size(); ++i) {
            if (detail[i].readNo != readNo) continue;
            auto const length = detail[i].cigar.qlength - detail[i].cigar.qfirst - detail[i].cigar.qclip;
            if (best < length) {
                best = length;
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    struct RowRange : std::pair<int64_t, int64_t> {
        
    };

    struct Cursor : public VDB::Cursor {
    private:
        enum {
            READ_GROUP = 1,
            NAME,
            READNO,
            SEQUENCE,
            REFERENCE,
            STRAND,
            POSITION,
            CIGAR,
            QUALITY
        };
        
        static VDB::Cursor cursor(VDB::Table const &tbl) {
            static char const *const FLDS[] = { "READ_GROUP", "NAME", "READNO", "SEQUENCE", "REFERENCE", "STRAND", "POSITION", "CIGAR", "QUALITY" };
            unsigned which = 0;
            return tbl.read(which, 9, FLDS, 8, FLDS);
        }
        
    public:
        Cursor(VDB::Table const &tbl) : VDB::Cursor(cursor(tbl)) {}

        bool hasQuality() const { return N == 9; }

        Fragment read(int64_t &row, int64_t endRow) const
        {
            auto &in = *static_cast<VDB::Cursor const *>(this);
            auto rslt = std::vector<Alignment>();
            auto spotGroup = std::string();
            auto spotName = std::string();
            auto firstRow = row;
            
            while (row < endRow) {
                auto const sg = in.read(row, READ_GROUP).string();
                auto const name = in.read(row, NAME).string();
                if (rslt.size() > 0) {
                    if (name != spotName || sg != spotGroup)
                        break;
                }
                else {
                    spotName = name;
                    spotGroup = sg;
                }

                auto const posColData = in.read(row, POSITION);
                auto const readNo = in.read(row, READNO).value<int32_t>();
                auto const sequence = in.read(row, SEQUENCE).string();
                auto const phys_qual = hasQuality() ? in.read(row, QUALITY).string() : std::string();
                auto const quality = hasQuality() ? &phys_qual : &Alignment::SyntheticQuality();
                if (posColData.elements > 0) {
                    auto const reference = in.read(row, REFERENCE).string();
                    auto const strand = in.read(row, STRAND).value<char>();
                    auto const position = posColData.value<int32_t>();
                    auto const cigar = in.read(row, CIGAR).string();
                    rslt.emplace_back(Alignment(  readNo
                                                , sequence
                                                , *quality
                                                , row
                                                , reference
                                                , strand
                                                , position
                                                , cigar
                                                ));
                }
                else {
                    rslt.emplace_back(Alignment(  readNo
                                                , sequence
                                                , *quality
                                                , row
                                                ));
                }
                ++row;
            }
            return Fragment(spotGroup, spotName, rslt, firstRow);
        }
        
        ///: Skips reading READ and QUALITY (the two biggest columns)
        Fragment readShort(int64_t &row, int64_t endRow) const
        {
            auto &in = *static_cast<VDB::Cursor const *>(this);
            auto rslt = std::vector<Alignment>();
            auto spotGroup = std::string();
            auto spotName = std::string();
            auto firstRow = row;
            
            while (row < endRow) {
                auto const sg = in.read(row, READ_GROUP).string();
                auto const name = in.read(row, NAME).string();
                if (rslt.size() > 0) {
                    if (name != spotName || sg != spotGroup)
                        break;
                }
                else {
                    spotName = name;
                    spotGroup = sg;
                }
                
                auto const posColData = in.read(row, POSITION);
                auto const readNo = in.read(row, READNO).value<int32_t>();
                if (posColData.elements > 0) {
                    auto const reference = in.read(row, REFERENCE).string();
                    auto const strand = in.read(row, STRAND).value<char>();
                    auto const position = posColData.value<int32_t>();
                    auto const cigar = in.read(row, CIGAR).string();
                    rslt.emplace_back(Alignment(  readNo, "", ""
                                                , row
                                                , reference
                                                , strand
                                                , position
                                                , cigar
                                                ));
                }
                else {
                    rslt.emplace_back(Alignment(readNo, "", "", row));
                }
                ++row;
            }
            return Fragment(spotGroup, spotName, rslt, firstRow);
        }
        Fragment read(VDB::Cursor::RowRange const &spotRange) const
        {
            auto &in = *static_cast<VDB::Cursor const *>(this);
            auto rslt = std::vector<Alignment>();
            auto spotGroup = std::string();
            auto spotName = std::string();

            rslt.reserve(spotRange.count());
            for (auto row = spotRange.beg(); row < spotRange.end(); ++row) {
                if (row == spotRange.beg()) {
                    spotGroup = in.read(row, READ_GROUP).string();
                    spotName = in.read(row, NAME).string();
                }
                
                auto const posColData = in.read(row, POSITION);
                auto const readNo = in.read(row, READNO).value<int32_t>();
                auto const sequence = in.read(row, SEQUENCE).string();
                auto const phys_qual = hasQuality() ? in.read(row, QUALITY).string() : std::string();
                auto const quality = hasQuality() ? &phys_qual : &Alignment::SyntheticQuality();
                if (posColData.elements > 0) {
                    auto const reference = in.read(row, REFERENCE).string();
                    auto const strand = in.read(row, STRAND).value<char>();
                    auto const position = posColData.value<int32_t>();
                    auto const cigar = in.read(row, CIGAR).string();
                    rslt.emplace_back(Alignment(  readNo
                                                , sequence
                                                , *quality
                                                , row
                                                , reference
                                                , strand
                                                , position
                                                , cigar
                                                ));
                }
                else {
                    rslt.emplace_back(Alignment(  readNo
                                                , sequence
                                                , *quality
                                                , row
                                                ));
                }
            }
            return Fragment(spotGroup, spotName, rslt, spotRange.beg());
        }
        
        ///: Skips reading READ and QUALITY (the two biggest columns)
        Fragment readShort(VDB::Cursor::RowRange const &spotRange) const
        {
            auto &in = *static_cast<VDB::Cursor const *>(this);
            auto rslt = std::vector<Alignment>();
            auto spotGroup = std::string();
            auto spotName = std::string();
            
            rslt.reserve(spotRange.count());
            for (auto row = spotRange.beg(); row < spotRange.end(); ++row) {
                if (row == spotRange.beg()) {
                    spotGroup = in.read(row, READ_GROUP).string();
                    spotName = in.read(row, NAME).string();
                }

                auto const posColData = in.read(row, POSITION);
                auto const readNo = in.read(row, READNO).value<int32_t>();
                if (posColData.elements > 0) {
                    auto const reference = in.read(row, REFERENCE).string();
                    auto const strand = in.read(row, STRAND).value<char>();
                    auto const position = posColData.value<int32_t>();
                    auto const cigar = in.read(row, CIGAR).string();
                    rslt.emplace_back(Alignment(  readNo, "", ""
                                                , row
                                                , reference
                                                , strand
                                                , position
                                                , cigar
                                                ));
                }
                else {
                    rslt.emplace_back(Alignment(readNo, "", "", row));
                }
            }
            return Fragment(spotGroup, spotName, rslt, spotRange.beg());
        }
        template <typename FUNC>
        void forEach(FUNC &&func) const {
            auto const range = rowRange();
            for (auto row = range.beg(); row < range.end(); ) {
                auto const spotGroup = VDB::Cursor::read(row, READ_GROUP).string();
                auto const spotName = VDB::Cursor::read(row, NAME).string();
                auto const firstRow = row;
                
                while (++row < range.end()) {
                    auto const name = VDB::Cursor::read(row, NAME);
                    if (!name.equal(spotName.cbegin(), spotName.cend()))
                        break;
                    auto const group = VDB::Cursor::read(row, READ_GROUP);
                    if (!group.equal(spotGroup.cbegin(), spotGroup.cend()))
                        break;
                }
                func(VDB::Cursor::RowRange(firstRow, row), spotGroup, spotName);
            }
        }
        int64_t count() const {
            int64_t spots = 0;
            
            forEach([&](VDB::Cursor::RowRange const &spotRange, std::string const &group, std::string const &name) {
                ++spots;
            });
            return spots;
        }
        template <typename FUNC>
        int64_t countIf(FUNC &&func) const {
            int64_t spots = 0;
            
            forEach([&](VDB::Cursor::RowRange const &spotRange, std::string const &group, std::string const &name) {
                if (func(group, name))
                    ++spots;
            });
            return spots;
        }
    };
};

#endif // __FRAGMENT_HPP_INCLUDED__
