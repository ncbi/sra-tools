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

struct Alignment {
    DNASequence sequence;
    std::string quality;
    std::string reference;
    std::string cigarString;
    CIGAR cigar;
    int readNo;
    int position;
    char strand;
    bool aligned;
    bool syntheticQuality;
    
    int qstart() const {
        return position - cigar.qfirst;
    }
    int qended() const {
        return position + cigar.rlength + cigar.qclip;
    }
    
    static std::string const &SyntheticQuality() {
        static auto const synthetic = std::string();
        return synthetic;
    }
    
    Alignment(int readNo, std::string const &sequence, std::string const &quality)
    : readNo(readNo)
    , sequence(sequence)
    , quality(quality)
    , aligned(false)
    , strand(0)
    , position(0)
    , syntheticQuality(&quality == &SyntheticQuality())
    {}

    Alignment(int readNo, std::string const &sequence, std::string const &quality, std::string const &reference, char strand, int position, std::string const &CIGAR)
    : readNo(readNo)
    , sequence(sequence)
    , quality(quality)
    , aligned(true)
    , reference(reference)
    , strand(strand)
    , position(position)
    , cigarString(CIGAR)
    , cigar(cigarString)
    , syntheticQuality(&quality == &SyntheticQuality())
    {}

    Alignment truncated() const {
        if (aligned)
            return Alignment(readNo, "", "", reference, strand, position, cigarString);
        else
            return Alignment(readNo, "", "");
    }

    friend bool operator <(Alignment const &a, Alignment const &b) {
        if (a.readNo < b.readNo)
            return true;
        if (!a.aligned && b.aligned)
            return true;
        if (a.aligned && b.aligned) {
            if (a.strand == '+' && b.strand == '-')
                return true;
            if (a.strand == '-' && b.strand == '+')
                return false;
            return a.position < b.position;
        }
        return false;
    }
    
    bool isClipped(unsigned spos) const {
        return (spos < cigar.qfirst) || (spos > cigar.qfirst + cigar.qlength);
    }

    bool sequenceEquivalentTo(Alignment const &other) const {
        auto const n = sequence.length();
        if (n != other.sequence.length()) return false;
        
        auto const adjoint = other.strand != strand;
        int equal = 0;
        for (auto i = 0; i < n; ++i) {
            auto const b1 = sequence[i];
            auto const j = adjoint ? (n - i - 1) : i;
            auto const ob = other.sequence[j];
            auto const b2 = adjoint ? DNASequence::adjoint(ob) : ob;
            
            if (b1 == b2)
                ++equal;
            else if ((isClipped(i) && DNASequence::isAmbiguous(b1)) || (other.isClipped(i) && DNASequence::isAmbiguous(b2)))
                ;
            else
                return false;
        }
        return equal > 0;
    }
};

struct Fragment {
    std::string group;
    std::string name;
    std::vector<Alignment> detail;
    
    Fragment(std::string const &group, std::string const &name, std::vector<Alignment> const &algn)
    : group(group)
    , name(name)
    , detail(algn)
    {
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
            try {
                return tbl.read(9, FLDS);
            }
            catch (...) {}
            return tbl.read(8, FLDS);
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
            
            while (row < endRow) {
                auto const r = row;
                auto const sg = in.read(r, READ_GROUP).string();
                auto const name = in.read(r, NAME).string();
                if (rslt.size() > 0) {
                    if (name != spotName || sg != spotGroup)
                        break;
                }
                else {
                    spotName = name;
                    spotGroup = sg;
                }

                auto const position = in.read(r, POSITION);
                if (position.elements > 0) {
                    auto const algn = Alignment(  in.read(r, READNO).value<int32_t>()
                                                , in.read(r, SEQUENCE).string()
                                                , hasQuality() ? in.read(r, QUALITY).string() : Alignment::SyntheticQuality()
                                                , in.read(r, REFERENCE).string()
                                                , in.read(r, STRAND).value<char>()
                                                , position.value<int32_t>()
                                                , in.read(r, CIGAR).string()
                                                );
                    rslt.push_back(algn);
                }
                else {
                    auto const algn = Alignment(  in.read(r, READNO).value<int32_t>()
                                                , in.read(r, SEQUENCE).string()
                                                , hasQuality() ? in.read(r, QUALITY).string() : Alignment::SyntheticQuality()
                                                );
                    rslt.push_back(algn);
                }
                ++row;
            }
            return Fragment(spotGroup, spotName, rslt);
        }
    };
};

#endif // __FRAGMENT_HPP_INCLUDED__
