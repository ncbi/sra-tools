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

struct DNABase {
    unsigned char value;
    
private:
    static DNABase fromValueUnchecked(int const value) {
        auto rslt = DNABase();
        rslt.value = value;
        return rslt;
    }

public:
    static DNABase A() { return fromValueUnchecked(1); }
    static DNABase C() { return fromValueUnchecked(2); }
    static DNABase G() { return fromValueUnchecked(4); }
    static DNABase T() { return fromValueUnchecked(8); }

    static DNABase fromValue(int const value) {
        return fromValueUnchecked((0 <= value && value <= 15) ? value : 15);
    }
    static DNABase fromCharacter(char const ch) {
        static signed char const tr[] = {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,0x0, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,0x0, -1, -1,
            -1,0x1,0xE,0x2,0xD, -1, -1,0x4,0xB, -1, -1,0xC, -1,0x3,0xF, -1,
            -1, -1,0x5,0x6,0x8, -1,0x7,0x9, -1,0xA, -1, -1, -1, -1, -1, -1,
            -1,0x1,0xE,0x2,0xD, -1, -1,0x4,0xB, -1, -1,0xC, -1,0x3,0xF, -1,
            -1, -1,0x5,0x6,0x8, -1,0x7,0x9, -1,0xA, -1, -1, -1, -1, -1, -1,
        };
        return fromValue((ch >= 0 && ch < sizeof(tr)) ? tr[ch] : -1);
    }
    DNABase transposed() const { return fromValueUnchecked((0xF7B3D591E6A2C480ull >> (4 * value)) & 0xF); }
    char character() const { return ".ACMGRSVTWYHKDBN"[value]; }
    operator bool() const { return ((1 << value) & 0x0116 /* 0b100010110 */) != 0; }
    bool ambiguous() const { return !bool(*this); }
    bool operator ==(DNABase const &other) const { return value == other.value; }
    bool operator !=(DNABase const &other) const { return value != other.value; }
};

struct DNASequence : public std::basic_string<DNABase> {
    typedef std::basic_string<DNABase> string_type;
private:
    static string_type fromString(std::string const &str) {
        auto rslt = string_type();
        rslt.reserve(str.length());
        for (auto && b : str) {
            rslt.push_back(DNABase::fromCharacter(b));
        }
        return rslt;
    }
public:
    bool ambiguous() const {
        for (auto && base : *this) {
            if (base.ambiguous())
                return true;
        }
        return false;
    }
    DNASequence transposed() const {
        auto rslt = string_type(rbegin(), rend());
        for (auto && base : rslt) {
            base = base.transposed();
        }
        return DNASequence(rslt);
    }
    operator std::string() const {
        auto rslt = std::string();
        rslt.reserve(length());
        for (auto && base : *this) {
            rslt.push_back(char(base));
        }
        return rslt;
    }
    DNASequence(string_type const &str) : string_type(str) {}
    DNASequence(std::string const &str) : string_type(fromString(str)) {}
    bool isEquivalentTo(DNASequence const &other) const {
        if (length() != other.length()) return false;
        if (*this == other) return true;

        auto j = other.rbegin();
        for (auto && base : *this) {
            if (base != j->transposed())
                return false;
            ++j;
        }
        return true;
    }
};

struct CIGAR_OP {
    uint32_t value;
    
    unsigned length() const { return value >> 4; }
    int opcode() const { return ("MIDN" "SHP=" "XB??" "????")[value & 15]; }
    unsigned qlength() const {
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
    unsigned rlength() const {
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

    static std::pair<int, int> parse(std::string const &str, unsigned &offset) {
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
    static std::string makeString(int length, int const opcode) {
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
    static CIGAR_OP compose(std::pair<int, int> const &in) {
        CIGAR_OP rslt; rslt.value = 0;
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

struct CIGAR : public std::vector<CIGAR_OP> {
    unsigned rlength; // aligned length on reference
    unsigned qfirst;  // first aligned base of the query
    unsigned qlength; // aligned length of query
    unsigned qclip;   // number of clipped bases of query

private:
    CIGAR(unsigned rlength, unsigned left_clip, unsigned qlength, unsigned right_clip, std::vector<CIGAR_OP> const &other)
    : qlength(qlength)
    , rlength(rlength)
    , qfirst(left_clip)
    , qclip(right_clip)
    , std::vector<CIGAR_OP>(other)
    {}
public:
    explicit CIGAR(std::string const &str)
    : qlength(0)
    , rlength(0)
    , qfirst(0)
    , qclip(0)
    {
        unsigned i = 0;
        auto isFirst = true;
        auto p = CIGAR_OP::parse(str, i);
        
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
                if (size() == 0) {
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
                
                qlength += elem.qlength();
                rlength += elem.rlength();

                if (size() == 0 || (back().value & 0xF) != (elem.value & 0xF))
                    push_back(elem);
                else
                    back().value += p.first << 4;
            }
        NEXT:
            p = CIGAR_OP::parse(str, i);
        }
        if (i == str.length() && size() != 0 && front().opcode() == 'M' && back().opcode() == 'M')
            return;
    INVALID:
        throw std::domain_error("Invalid CIGAR");
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
    CIGAR transposed() const {
        return CIGAR(rlength, qclip, qlength, qfirst, std::vector<CIGAR_OP>(rbegin(), rend()));
    }
    unsigned totalQueryLength() const { return qfirst + qlength + qclip; }
};

struct Alignment {
    DNASequence sequence;
    std::string reference;
    std::string cigar;
    int readNo;
    int position;
    char strand;
    bool aligned;
    
    Alignment(int readNo, std::string const &sequence)
    : readNo(readNo)
    , sequence(sequence)
    , aligned(false)
    , reference("")
    , strand(0)
    , position(0)
    , cigar("")
    {}

    Alignment(int readNo, std::string const &sequence, std::string const &reference, char strand, int position, std::string const &cigar)
    : readNo(readNo)
    , sequence(sequence)
    , aligned(true)
    , reference(reference)
    , strand(strand)
    , position(position)
    , cigar(cigar)
    {}
    
    Alignment truncated() const {
        if (aligned)
            return Alignment(readNo, "", reference, strand, position, cigar);
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

    struct Cursor : public VDB::Cursor {
    private:
        static VDB::Cursor cursor(VDB::Table const &tbl) {
            static char const *const FLDS[] = { "READ_GROUP", "FRAGMENT", "READNO", "SEQUENCE", "REFERENCE", "STRAND", "POSITION", "CIGAR" };
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
                auto const r = row++;
                auto const sg = in.read(r, 1).asString();
                auto const name = in.read(r, 2).asString();
                if (rslt.size() > 0) {
                    if (name != spotName || sg != spotGroup)
                        break;
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
            }
            return Fragment(spotGroup, spotName, rslt);
        }
        void foreachRow(void (*F)(Fragment const &), char const *message = nullptr) const
        {
            auto const range = rowRange();
            auto const rows = range.second - range.first;
            auto const freq = rows / 100.0;
            auto nextReport = 1;
            
            std::cerr << "info: processing " << rows << " records";
            if (message)
                std::cerr << "; " << message;
            std::cerr << std::endl;
            
            for (auto row = range.first; row < range.second; ) {
                auto const spot = read(row, range.second);
                if (spot.detail.empty())
                    continue;
                F(spot);
                while (nextReport * freq <= (row - range.first)) {
                    std::cerr << "prog: processed " << nextReport << "%" << std::endl;
                    ++nextReport;
                }
            }
        }
    };
};

#endif // __FRAGMENT_HPP_INCLUDED__
