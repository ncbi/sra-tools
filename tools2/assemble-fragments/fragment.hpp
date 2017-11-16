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

/** \class CIGAR_OP
 * \ingroup CIGAR
 * \brief A value type for a single alignment operation
 */
struct CIGAR_OP {
    uint32_t value; ///< equivalent to BAM encoding
    
    unsigned length() const ///< extract the operation length (or count)
    {
        return value >> 4;
    }
    int opcode() const ///< extract the operation type
    {
        return ("MIDN" "SHP=" "XB??" "????")[value & 15];
    }
    unsigned qlength() const ///< effects how many bases of the query sequence
    {
        switch (value & 15) {
            case 0:
            case 1:
            case 4:
            case 7:
            case 8:
            case 9:
                return length();
            default:
                return 0;
        }
    }
    unsigned rlength() const ///< effects how many bases of the reference sequence
    {
        switch (value & 15) {
            case 0:
            case 2:
            case 3:
            case 7:
            case 8:
                return length();
            default:
                return 0;
        }
    }

    /** parse a single operation
     * /param str the string to be parsed
     * /offset the first character to be parsed; the value is in/out
     * /returns length and opcode
     */
    static std::pair<int, int> parse(std::string const &str, unsigned &offset)
    {
        auto const N = str.length();
        int length = 0;
        int opcode = 0;
        
        while (offset < N) {
            opcode = str[offset++];
            if (opcode < '0' || opcode > '9')
                break;
            length = length * 10 + (opcode - '0');
            opcode = 0;
        }
        return std::make_pair(length, opcode);
    }
    static std::string makeString(int length, int const opcode) ///< convert to string
    {
        char buffer[16];
        char *cp = buffer + 16;
        
        if (length == 0) return std::string();
        
        *--cp = '\0';
        *--cp = opcode;
        while (length > 0) {
            auto const ch = (length % 10) + '0';
            *--cp = ch;
            length /= 10;
        }
        return std::string(cp);
    }
    /** construct a new value
     * /param in the pair of length, opcode like as returned by parse
     * /returns a new value or 0
     */
    static CIGAR_OP compose(std::pair<int, int> const &in)
    {
        CIGAR_OP rslt; rslt.value = 0; ///< an erroneous value
        switch (in.second) {
            case 'M': rslt.value = (in.first << 4) | 0; break;
            case 'I': rslt.value = (in.first << 4) | 1; break;
            case 'D': rslt.value = (in.first << 4) | 2; break;
            case 'N': rslt.value = (in.first << 4) | 3; break;
            case 'S': rslt.value = (in.first << 4) | 4; break;
            case 'H': rslt.value = (in.first << 4) | 5; break;
            case 'P': rslt.value = (in.first << 4) | 6; break;
            case '=': rslt.value = (in.first << 4) | 7; break;
            case 'X': rslt.value = (in.first << 4) | 8; break;
            case 'B': rslt.value = (in.first << 4) | 9; break;
        }
        return rslt;
    }
};

/** \class CIGAR
 * \ingroup CIGAR
 * \brief A value type for a sequence of alignment operations
 */
struct CIGAR : public std::vector<CIGAR_OP> {
    int rlength; ///< aligned length on reference
    int qfirst;  ///< first aligned base of the query
    int qlength; ///< aligned length of query
    int qclip;   ///< number of clipped bases of query

private:
    CIGAR(unsigned rlength, unsigned left_clip, unsigned qlength, unsigned right_clip, std::vector<CIGAR_OP> const &other)
    : qlength(qlength)
    , rlength(rlength)
    , qfirst(left_clip)
    , qclip(right_clip)
    , std::vector<CIGAR_OP>(other)
    {}
public:
    CIGAR()
    : qlength(0)
    , rlength(0)
    , qfirst(0)
    , qclip(0)
    {}
    explicit CIGAR(std::string const &str)
    : qlength(0)
    , rlength(0)
    , qfirst(0)
    , qclip(0)
    {
        unsigned i = 0;
        auto isFirst = true;
        auto p = CIGAR_OP::parse(str, i);
        auto v = std::vector<CIGAR_OP>();
        
        while (p.first != 0 && p.second != 0) {
            auto const wasFirst = isFirst; isFirst = false;
            if (p.second == 'H') {
                if (wasFirst) goto NEXT;
                if (i == str.length()) break;
                goto INVALID;
            }
            if (qclip != 0)
                goto INVALID;
            if (p.second == 'P') continue;
            if (p.second == 'S') {
                if (v.size() == 0) {
                    if (qfirst != 0)
                        goto INVALID;
                    qfirst = p.first;
                }
                else
                    qclip = p.first;

                goto NEXT;
            }
            if (p.second == '=' || p.second == 'X') p.second = 'M';
            {
                auto const elem = CIGAR_OP::compose(p);
                if (elem.value == 0)
                    goto INVALID;
                
                if (size() == 0 || (v.back().value & 0xF) != (elem.value & 0xF))
                    v.push_back(elem);
                else
                    v.back().value += p.first << 4;
            }
        NEXT:
            p = CIGAR_OP::parse(str, i);
        }
        if (i == str.length() && v.size() != 0) {
            int first = 0;
            while (first != v.size() && v[first].opcode() == 'I') {
                qfirst += v[first].length();
                ++first;
            }
            int end = (int)v.size();
            while (end - 1 >= first) {
                auto const opcode = v[end - 1].opcode();
                if (opcode != 'I' && opcode != 'D') break;
                if (opcode == 'I')
                    qclip += v[end - 1].length();
                --end;
            }
            if (end > first)
                assign(v.begin() + first, v.begin() + end);
        }
        if (i == str.length() && size() != 0 && front().opcode() == 'M' && back().opcode() == 'M') {
            qlength = qfirst + qclip;
            for (auto && op : *this) {
                auto const length = op.length();
                switch (op.value & 0xF) {
                    case 0:
                        rlength += length;
                        // fallthrough;
                    case 1:
                    case 9:
                        qlength += length;
                        break;
                    case 2:
                    case 3:
                        rlength += length;
                    default:
                        break;
                }
            }
            return;
        }
    INVALID:
        qclip = qfirst = qlength = rlength = 0;
        clear();
    }
    operator std::string() const {
        if (size() == 0) return "*";
        auto rslt = std::string();
        if (qfirst > 0)
            rslt += CIGAR_OP::makeString(qfirst, 'S');
        for (auto && i : *this) {
            rslt += CIGAR_OP::makeString(i.length(), i.opcode());
        }
        if (qclip > 0)
            rslt += CIGAR_OP::makeString(qclip, 'S');
        return rslt;
    }
    CIGAR adjoint() const {
        return CIGAR(rlength, qclip, qlength, qfirst, std::vector<CIGAR_OP>(rbegin(), rend()));
    }
};

struct Alignment {
    DNASequence sequence;
    std::string reference;
    std::string cigarString;
    CIGAR cigar;
    int readNo;
    int position;
    char strand;
    bool aligned;
    
    Alignment(int readNo, std::string const &sequence)
    : readNo(readNo)
    , sequence(sequence)
    , aligned(false)
    , strand(0)
    , position(0)
    {}

    Alignment(int readNo, std::string const &sequence, std::string const &reference, char strand, int position, std::string const &CIGAR)
    : readNo(readNo)
    , sequence(sequence)
    , aligned(true)
    , reference(reference)
    , strand(strand)
    , position(position)
    , cigarString(CIGAR)
    , cigar(cigarString)
    {}

    Alignment truncated() const {
        if (aligned)
            return Alignment(readNo, "", reference, strand, position, cigarString);
        else
            return Alignment(readNo, "");
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
    
    DNASequence const &sequence(int readNo) const {
        // since all of the sequences are equivalent,
        // the first unambiguous sequence is best
        for (auto && i : detail) {
            if (i.readNo != readNo) continue;
            if (i.sequence.ambiguous()) continue;
            return i.sequence;
        }
        // there were no unambiguous sequences
        // the best one will have to the one with the longest query length
        int best = -1;
        int bestIndex = 0;
        for (auto i = 0; i < detail.size(); ++i) {
            if (detail[i].readNo != readNo) continue;
            auto const length = detail[i].cigar.qlength - detail[i].cigar.qfirst - detail[i].cigar.qclip;
            if (best < length) {
                best = length;
                bestIndex = i;
            }
        }
        return detail[bestIndex].sequence;
    }

    struct Cursor : public VDB::Cursor {
    private:
        static VDB::Cursor cursor(VDB::Table const &tbl) {
            static char const *const FLDS[] = { "READ_GROUP", "NAME", "READNO", "SEQUENCE", "REFERENCE", "STRAND", "POSITION", "CIGAR" };
            return tbl.read(8, FLDS);
        }
        
    public:
        Cursor(VDB::Table const &tbl) : VDB::Cursor(cursor(tbl)) {}
        
        Fragment read(int64_t &row, int64_t endRow) const
        {
            auto &in = *static_cast<VDB::Cursor const *>(this);
            auto rslt = std::vector<Alignment>();
            auto spotGroup = std::string();
            auto spotName = std::string();
            
            while (row < endRow) {
                auto const r = row;
                auto const sg = in.read(r, 1).asString();
                auto const name = in.read(r, 2).asString();
                if (rslt.size() > 0) {
                    if (name != spotName || sg != spotGroup)
                        break;
                }
                else {
                    spotName = name;
                    spotGroup = sg;
                }
                auto const readNo = in.read(r, 3).value<int32_t>();
                auto const sequence = in.read(r, 4).asString();
                if (in.read(r, 7).elements > 0) {
                    auto const cigar = in.read(r, 8).asString();
                    auto const algn = Alignment(readNo, sequence, in.read(r, 5).asString(), in.read(r, 6).value<char>(), in.read(r, 7).value<int32_t>(), cigar);
                    rslt.push_back(algn);
                }
                else {
                    auto const algn = Alignment(readNo, sequence);
                    rslt.push_back(algn);
                }
                ++row;
            }
            return Fragment(spotGroup, spotName, rslt);
        }
    };
};

#endif // __FRAGMENT_HPP_INCLUDED__
