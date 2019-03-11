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
    
    enum Operation {
        match,
        insertion,
        deletion,
        intron,
        softClip,
        hardClip,
        padding,
        equals,
        notEquals,
        back
    };
private:
    explicit CIGAR_OP(uint32_t value) : value(value) {}
    static uint16_t constexpr matchMask() {
        return (1 << match) | (1 << equals) | (1 << notEquals);
    }
    static uint16_t constexpr QBits() {
        return matchMask() | (1 << insertion) | (1 << softClip) | (1 << back);
    }
    static uint16_t constexpr RBits() {
        return matchMask() | (1 << deletion) | (1 << intron);
    }
    uint16_t opBit() const {
        return (value & 0x0F) <= back ? (1 << (value & 0x0F)) : 0;
    }
public:
    CIGAR_OP(int length, Operation op) : value(uint32_t(length << 4) | uint32_t(op)) {}
    bool isa(Operation op) const {
        return (value & 0x0F) == op;
    }
    static CIGAR_OP invalid() {
        return CIGAR_OP(UINT32_MAX);
    }
    operator bool() const { return opBit() != 0; }
    bool isMatch() const {
        return (opBit() & matchMask()) != 0;
    }
    CIGAR_OP changeOperation(Operation op) const {
        auto const orig_op = value & 0x0F;
        return CIGAR_OP(value ^ orig_op ^ op);
    }
    bool isSameOperation(CIGAR_OP const &other) const {
        return (value & 0x0F) == (other.value & 0x0F);
    }
    CIGAR_OP merge(CIGAR_OP const &other) const {
        auto const op = value & 0x0F;
        assert(op == (other.value & 0x0F));
        return CIGAR_OP(((value ^ op) + (other.value ^ op)) ^ op);
    }
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
        return (opBit() & QBits()) != 0 ? length() : 0;
    }
    unsigned rlength() const ///< effects how many bases of the reference sequence
    {
        return (opBit() & RBits()) != 0 ? length() : 0;
    }

    void appendToString(std::string &out) const {
        if ((value & 0x0F) <= back && (value >> 4) > 0) {
            char buffer[16] = { '\0' };
            auto cp = buffer + 15;
            auto const endp = cp;
            auto len = length();
            
            *--cp = opcode();
            do { *--cp = '0' + (len % 10); len /= 10; } while (len > 0);
            out.reserve(out.size() + (endp - cp));
            out.append(cp, endp);
        }
    }
    std::string toString() const {
        auto result = std::string();
        appendToString(result);
        return result;
    }
    
    class Parser {
    public:
        using Iterator = std::string::const_iterator;
    private:
        Iterator pos;
        Iterator const end;
    public:
        Parser(Iterator const &beg, Iterator const &end)
        : pos(beg), end(end) {}
        
        bool atEnd() const { return pos == end; }
        CIGAR_OP next() {
            if (pos == end) return invalid();

            uint32_t length = 0;
            
            while (!atEnd()) {
                int const ch = *pos++;
                if (ch >= '0' && ch <= '9') {
                    length = length * 10 + (ch - '0');
                    continue;
                }
                switch (ch) {
                    case 'M': return CIGAR_OP(length, match);
                    case 'I': return CIGAR_OP(length, insertion);
                    case 'D': return CIGAR_OP(length, deletion);
                    case 'N': return CIGAR_OP(length, intron);
                    case 'S': return CIGAR_OP(length, softClip);
                    case 'H': return CIGAR_OP(length, hardClip);
                    case 'P': return CIGAR_OP(length, padding);
                    case '=': return CIGAR_OP(length, equals);
                    case 'X': return CIGAR_OP(length, notEquals);
                    case 'B': return CIGAR_OP(length, back);
                    default:
                        throw std::domain_error("invalid opcode");
                }
            }
            throw std::domain_error("truncated CIGAR");
        }
    };
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
    
    using ParseResult = std::pair<std::pair<unsigned, unsigned>, std::vector<CIGAR_OP>>;
    static ParseResult parse(std::string const &cigar) {
        auto clip = std::pair<unsigned, unsigned>(0,0);
        auto v = std::vector<CIGAR_OP>();
        auto parser = CIGAR_OP::Parser(cigar.begin(), cigar.end());
        auto count = 0;
        auto hasLeftClip = false;
        auto hasRightClip = false;
        auto countNotClip = 0;

        /* ([1-9][0-9]*H){0,1} #left hard clip
         * ([1-9][0-9]*S){0,1} #left soft clip
         * ([1-9][0-9]*[MIDNPX=])+ #regular CIGAR operations
         * ([1-9][0-9]*S){0,1} #right soft clip
         * ([1-9][0-9]*H){0,1} #right hard clip
         */
        while (auto op = parser.next()) {
            count += 1;
            if (op.isa(CIGAR_OP::hardClip)) {
                if (count == 1) continue; ///< it is the left hard clip
                break;                    ///< it is the right hard clip and should be the last operation
            }
            if (hasRightClip)
                throw std::domain_error("bad soft clip");
            if (op.isa(CIGAR_OP::softClip)) {
                if (hasLeftClip) {
                    clip.second = op.length();
                    hasRightClip = true;
                    continue;
                }
                if (countNotClip == 0) {
                    clip.first = op.length();
                    hasLeftClip = true;
                    continue;
                }
                throw std::domain_error("bad soft clip");
            }
            countNotClip += 1;
            if (op.isa(CIGAR_OP::padding))
                continue;
            if (op.isMatch())
                op = op.changeOperation(CIGAR_OP::match);
            if (v.empty() || !op.isSameOperation(v.back()))
                v.emplace_back(op);
            else
                v.back().merge(op);
        }
        if (!parser.atEnd())
            throw std::domain_error("invalid CIGAR");

        // v contains only [MIDNB]; [SHP] were removed; [X=] were converted to M
        // delete leading D/N; remove leading I/B and add to left soft clip
        while (!v.empty() && !(v.front().isa(CIGAR_OP::match))) {
            clip.first += v.front().qlength();
            v.erase(v.begin());
        }
        // delete trailing D/N; remove trailing I/B and add to right soft clip
        while (!v.empty() && !(v.front().isa(CIGAR_OP::match))) {
            clip.second += v.front().qlength();
            v.pop_back();
        }
        return { clip, v };
    }
    static ParseResult parseUnsafeUnchecked(std::string const &cigar) {
        auto clip = std::pair<unsigned, unsigned>(0,0);
        auto v = std::vector<CIGAR_OP>();
        auto parser = CIGAR_OP::Parser(cigar.begin(), cigar.end());
        
        /*
         * ([1-9][0-9]*S){0,1} #left soft clip
         * ([1-9][0-9]*[MIDN])+ #regular CIGAR operations
         * ([1-9][0-9]*S){0,1} #right soft clip
         */
        while (auto op = parser.next()) {
            if (!op.isa(CIGAR_OP::softClip))
                v.emplace_back(op);
            else {
                if (clip.first)
                    clip.second = op.length();
                else
                    clip.first = op.length();
            }
        }
        return { clip, v };
    }
public:
    CIGAR()
    : qlength(0)
    , rlength(0)
    , qfirst(0)
    , qclip(0)
    {}
    explicit CIGAR(std::string const &cigar, bool isClean = false)
    : qlength(0)
    , rlength(0)
    , qfirst(0)
    , qclip(0)
    {
        try {
            auto const parsed = (isClean ? CIGAR::parseUnsafeUnchecked : CIGAR::parse)(cigar);
            if (!parsed.second.empty()) {
                qfirst = parsed.first.first;
                qclip = parsed.first.second;
                assign(parsed.second.begin(), parsed.second.end());
            }
        }
        catch (...) {}

        qlength = qfirst + qclip;
        for (auto && op : *this) {
            rlength += op.rlength();
            qlength += op.qlength();
        }
    }
    operator std::string() const {
        auto rslt = std::string("*");
        if (size() > 0) {
            rslt.clear();
            if (qfirst > 0)
                CIGAR_OP(qfirst, CIGAR_OP::softClip).appendToString(rslt);
            for (auto && i : *this)
                i.appendToString(rslt);
            if (qclip > 0)
                CIGAR_OP(qclip, CIGAR_OP::softClip).appendToString(rslt);
        }
        return rslt;
    }
    CIGAR adjoint() const {
        return CIGAR(rlength, qclip, qlength, qfirst, std::vector<CIGAR_OP>(rbegin(), rend()));
    }
};

#endif // __CIGAR_HPP_INCLUDED__
