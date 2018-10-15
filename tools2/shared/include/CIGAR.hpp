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

#ifndef __CIGAR_HPP_INCLUDED__
#define __CIGAR_HPP_INCLUDED__ 1

#include <string>
#include <vector>
#include <utility>

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
    
    /** \brief parse a single operation
     * \param str the string to be parsed
     * \param offset the first character to be parsed; the value is in/out
     * \returns length and opcode
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
    /** \brief construct a new value
     * \param in the pair of length, opcode like as returned by parse
     * \returns a new value or a '0' value
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
    int rlength; ///< aligned length on reference; sum of matches, deletes, and introns
    int qfirst;  ///< first aligned base of the query
    int qlength; ///< total length of query; sum of matches, inserts, and soft-clips
    int qclip;   ///< number of clipped bases of query

    /// number of bases [first ... last] aligned bases
    int alignedLength() const {
        return qlength - (qfirst + qclip);
    }
    
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
        
        /* ([1-9][0-9]*H){0,1} #left hard clip
           ([1-9][0-9]*S){0,1} #left soft clip
           ([1-9][0-9]*[MIDNPX=])+ #regular CIGAR operations
           ([1-9][0-9]*S){0,1} #right soft clip
           ([1-9][0-9]*H){0,1} #right hard clip
         */
        while (p.first != 0 && p.second != 0) {
            auto const wasFirst = isFirst; isFirst = false;
            if (p.second == 'H') {
                if (wasFirst) goto NEXT;        ///< left hard clip; ignored
                if (i == str.length()) break;   ///< right hard clip; ignored
                goto INVALID;                   ///< invalid hard clip
            }
            if (qclip != 0)
                goto INVALID;                   ///< invalid, no operation other than 'H' should follow the right soft clip
            if (p.second == 'P') goto NEXT;     ///< P is ignored
            if (p.second == 'S') {
                if (v.size() == 0) {
                    if (qfirst != 0)
                        goto INVALID;           ///< invalid soft clip
                    qfirst = p.first;           ///< left soft clip
                }
                else
                    qclip = p.first;            ///< right soft clip
                
                goto NEXT;
            }
            if (p.second == '=' || p.second == 'X') p.second = 'M'; ///< convert 'X' & '=' to 'M'
            {
                auto const elem = CIGAR_OP::compose(p);
                if (elem.value == 0)
                    goto INVALID;
                
                if (size() == 0 || (v.back().value & 0xF) != (elem.value & 0xF))
                    v.push_back(elem);              ///< append to vector
                else
                    v.back().value += p.first << 4; ///< combine with last element
            }
        NEXT:
            p = CIGAR_OP::parse(str, i);
        }
        if (i != str.length()) goto INVALID;        ///< was the entire CIGAR string parsed?
        
        if (v.size() != 0) {
            int first = 0;
            
            // convert+remove any leading 'I' into left soft clip
            while (first != v.size() && v[first].opcode() == 'I') {
                qfirst += v[first].length();
                ++first;
            }

            // remove any trailing 'D' and convert+remove any trailing 'I' into right soft clip
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
            else
                goto INVALID;
        }
        if (size() != 0 && front().opcode() == 'M' && back().opcode() == 'M') {
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

#endif // __CIGAR_HPP_INCLUDED__
