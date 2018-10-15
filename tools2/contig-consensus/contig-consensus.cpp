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

#include <iostream>
#include <map>
#include <vector>
#include <array>
#include <set>
#include <utility>
#include <numeric>
#include <cassert>
#include "utility.hpp"
#include "CIGAR.hpp"
#include "vdb.hpp"
#include "writer.hpp"

using namespace utility;

static strings_map groups = { "" };
static strings_map references = { "" };

struct Contigs {
    struct Mapulet {
        strings_map::index_t ref1, ref2;
        int start1, start2;
        int end1, end2;
        int gap;
        
        std::string referenceName() const {
            return  references[ref1] + ':' + std::to_string(start1 + 1) + '(' + std::to_string(end1 - start1) + ')'
            + '+' + references[ref2] + ':' + std::to_string(start2 + 1) + '(' + std::to_string(end2 - start2) + ')';
        }
    };
    struct Contig {
        VDB::Cursor::RowID sourceRow;
        strings_map::index_t reference, group;
        int start, end;
        unsigned map;
        
        std::string referenceName() const {
            return references[reference] + ':' + std::to_string(start + 1) + '(' + std::to_string(end - start) + ')';
        }
    };
    std::vector<Contig> contigs;
    std::vector<Mapulet> mapulets;
    
    static Contigs const load(VDB::Database const &db) {
        enum { REFERENCE, READ_GROUP, START, END, REFERENCE_1, START_1, END_1, GAP, REFERENCE_2, START_2, END_2 };
        auto const curs = db["CONTIGS"].read({ "REFERENCE", "READ_GROUP", "START", "END", "REFERENCE_1", "START_1", "END_1", "GAP", "REFERENCE_2", "START_2", "END_2" });
        Contigs result;
        
        result.mapulets.emplace_back( Mapulet { 0, 0, -1, -1, -1, -1, 0 });
        curs.foreach([&](VDB::Cursor::RowID row, std::vector<VDB::Cursor::RawData> const &data) {
            Contig contig = { row, 0, groups[data[READ_GROUP].string()], -1, -1, unsigned(result.mapulets.size()) };

            if (data[REFERENCE].elements == 0) {
                result.mapulets.emplace_back( Mapulet
                                            { references[data[REFERENCE_1].string()]
                                            , references[data[REFERENCE_2].string()]
                                            , data[START_1].value<int32_t>()
                                            , data[START_2].value<int32_t>()
                                            , data[END_1].value<int32_t>()
                                            , data[END_2].value<int32_t>()
                                            , data[GAP].value<int32_t>()
                                            });
            }
            else {
                contig.map = 0;
                contig.reference = references[data[REFERENCE].string()];
                contig.start = data[START].value<int32_t>();
                contig.end = data[END].value<int32_t>();
            }
            result.contigs.emplace_back(contig);
        });
        return result;
    }
    Contig const &contig(VDB::Cursor::RowID id) const {
        if (id < 1 || id > contigs.size())
            throw std::range_error("id out of range");
        auto &y = contigs[id - 1];
        assert(y.sourceRow == id);
        return y;
    }
    Mapulet const &mapulet(Contig const &contig) const {
        return mapulets[contig.map];
    }
    unsigned length(VDB::Cursor::RowID id) const {
        auto const &c = contig(id);
        if (c.map == 0) return c.end - c.start;
        auto const &m = mapulet(c);
        return (m.end1 - m.start1) + (m.end2 - m.start2);
    }
    static void addTableTo(Writer2 &writer) {
        writer.addTable("CONTIGS", {
            { "FWD_FIRST", sizeof(uint32_t) },
            { "FWD_LAST", sizeof(uint32_t) },
            { "FWD_GAP_START", sizeof(uint32_t) },
            { "FWD_GAP_ENDED", sizeof(uint32_t) },
            { "FWD_A", sizeof(uint32_t) },
            { "FWD_C", sizeof(uint32_t) },
            { "FWD_G", sizeof(uint32_t) },
            { "FWD_T", sizeof(uint32_t) },
            
            { "REV_FIRST", sizeof(uint32_t) },
            { "REV_LAST", sizeof(uint32_t) },
            { "REV_GAP_START", sizeof(uint32_t) },
            { "REV_GAP_ENDED", sizeof(uint32_t) },
            { "REV_A", sizeof(uint32_t) },
            { "REV_C", sizeof(uint32_t) },
            { "REV_G", sizeof(uint32_t) },
            { "REV_T", sizeof(uint32_t) },
        });
    }
    static std::array<Writer2::Column, 16> openColumns(Writer2 const &out) {
        auto const &tbl = out.table("CONTIGS");
        return {
            tbl.column("FWD_FIRST"),
            tbl.column("FWD_LAST"),
            tbl.column("FWD_GAP_START"),
            tbl.column("FWD_GAP_ENDED"),
            tbl.column("FWD_A"),
            tbl.column("FWD_C"),
            tbl.column("FWD_G"),
            tbl.column("FWD_T"),
            tbl.column("REV_FIRST"),
            tbl.column("REV_LAST"),
            tbl.column("REV_GAP_START"),
            tbl.column("REV_GAP_ENDED"),
            tbl.column("REV_A"),
            tbl.column("REV_C"),
            tbl.column("REV_G"),
            tbl.column("REV_T"),
        };
    }
};

struct Cursor : public VDB::Cursor {
    enum { CONTIG, POSITION, LAYOUT, LENGTH, COL_CIGAR, SEQUENCE };
    Cursor(VDB::Table const &tbl) : VDB::Cursor(tbl.read({ "CONTIG", "POSITION", "LAYOUT", "LENGTH", "CIGAR", "SEQUENCE" })) {}
    
    struct Row : public std::vector<VDB::Cursor::RawData> {
        using ContigID = VDB::Cursor::RowID;
        using Position = int32_t;
        using Length = int32_t;
        
        struct Layout {
            char value[4];
            explicit Layout(VDB::Cursor::RawData const &raw) {
                assert(raw.elements == 4);
                auto const base = reinterpret_cast<char const *>(raw.data);
                std::copy(base, base + 4, value);
            }
            char &operator[](int i) { return value[i]; }
            char operator[](int i) const { return value[i]; }
        };
        
        ContigID contigID() const {
            return (*this)[0].value<ContigID>();
        }
        Position position() const {
            return (*this)[1].value<Position>();
        }
        Layout layout() const {
            return Layout((*this)[2]);
        }
        Length length() const {
            return (*this)[3].value<Length>();
        }
        std::pair<CIGAR, CIGAR> CIGAR() const {
            auto const cigar = (*this)[4].string();
            auto const split = cigar.find("0P");
            assert(split != std::string::npos);
            return { ::CIGAR(cigar.substr(0, split)), ::CIGAR(cigar.substr(split + 2)) };
        }
        std::string sequence() const {
            return (*this)[5].string();
        }
    };
};

/**
 Packed base and count
 */
struct Packed {
    uint32_t value;
    
    int baseValue() const { return value & 0x03; }
    int base() const { return "ACGT"[baseValue()]; }
    int count() const { return value >> 2; }
    
    Packed() : value(0) {}
    Packed(int base, unsigned count) : value(0) {
        switch (base) {
            case 'A': value = (count << 2) | 0; break;
            case 'C': value = (count << 2) | 1; break;
            case 'G': value = (count << 2) | 2; break;
            case 'T': value = (count << 2) | 3; break;
        }
    }
    bool operator <(Packed const &rhs) const {
        return value < rhs.value;
    }
};

struct Count {
    unsigned A, C, G, T, N;
    unsigned a, c, g, t, n;

    explicit Count(int = 0) : A(0), C(0), G(0), T(0), N(0), a(0), c(0), g(0), t(0), n(0) {}
    void add(int base) {
        switch (base) {
            case 'A': A += 1; N += 1; break;
            case 'C': C += 1; N += 1; break;
            case 'G': G += 1; N += 1; break;
            case 'T': T += 1; N += 1; break;
            case 'a': a += 1; n += 1; break;
            case 'c': c += 1; n += 1; break;
            case 'g': g += 1; n += 1; break;
            case 't': t += 1; n += 1; break;
        }
    }
    void remove(int base) {
        assert(N + n > 0);
        switch (base) {
            case 'A': A -= 1; N -= 1; break;
            case 'C': C -= 1; N -= 1; break;
            case 'G': G -= 1; N -= 1; break;
            case 'T': T -= 1; N -= 1; break;
            case 'a': a -= 1; n -= 1; break;
            case 'c': c -= 1; n -= 1; break;
            case 'g': g -= 1; n -= 1; break;
            case 't': t -= 1; n -= 1; break;
        }
    }
    bool isA() const { return (N + n) > 0 && (N + n) == (A + a); }
    bool isC() const { return (N + n) > 0 && (N + n) == (C + c); }
    bool isG() const { return (N + n) > 0 && (N + n) == (G + g); }
    bool isT() const { return (N + n) > 0 && (N + n) == (T + t); }
    bool isAllSame() const { return isA() || isC() || isG() || isT(); }
    struct Sorted {
        //  0 ..< n0 : bases which don't occur
        // n0 ..< n1 : bases which occur once
        // n1 ..<  4 : bases which occur more than once
        unsigned n0, n1;
        Packed count[4];
        
        Sorted(unsigned A, unsigned C, unsigned G, unsigned T) {
            count[0] = Packed('A', A);
            count[1] = Packed('C', C);
            count[2] = Packed('G', G);
            count[3] = Packed('T', T);
            std::sort(count, count + 4);
            
            n0 =  0; while (n0 < 4 && count[n0].count() == 0) { ++n0; }
            n1 = n0; while (n1 < 4 && count[n1].count() == 1) { ++n1; }
        }
    };
    Sorted sorted() const {
        return Sorted(A + a, C + c, G + g, T + t);
    }
};

struct CycleStats {
    struct Key {
        unsigned group;
        unsigned readNo;
        int position;
        
        bool operator <(Key const &rhs) const {
            if (group < rhs.group) return true;
            if (rhs.group < group) return false;
            if (readNo < rhs.readNo) return true;
            if (rhs.readNo < readNo) return false;
            return position < rhs.position;
        }
        
        Key(unsigned group, unsigned readNo, int position) : group(group), readNo(readNo), position(position) {}
    };
    struct Value {
        uint64_t occurence, consensus;
        
        Value() : occurence(0), consensus(0) {}
    };
    std::map<Key, Value> stats;
    
    void add(unsigned group, unsigned readNo, int position, bool consensus) {
        auto const key = Key(group, readNo, position);
        auto &value = stats[key];
        value.occurence += 1;
        if (consensus)
            value.consensus += 1;
    }

    void write(Writer2 const &out) const {
        auto const table = out.table("ALIGNED_STATISTICS");
        auto const READ_GROUP = table.column("READ_GROUP");
        auto const READNO = table.column("READNO");
        auto const CYCLE = table.column("READ_CYCLE");
        auto const OCCURENCE = table.column("OCCURENCE");
        auto const CONSENSUS = table.column("CONSENSUS");
        
        for (auto && i : stats) {
            auto const &readGroup = groups[i.first.group];
            
            READ_GROUP.setValue(readGroup);
            READNO.setValue(int32_t(i.first.readNo));
            CYCLE.setValue(uint32_t(i.first.position));
            OCCURENCE.setValue(i.second.occurence);
            CONSENSUS.setValue(i.second.consensus);
            
            table.closeRow();
        }
    }
    static void addTableTo(Writer2 &writer) {
        writer.addTable("ALIGNED_STATISTICS", {
            { "READ_GROUP", sizeof(char) },
            { "READNO", sizeof(int32_t) },
            { "READ_CYCLE", sizeof(uint32_t) },
            { "OCCURENCE", sizeof(uint64_t) },
            { "CONSENSUS", sizeof(uint64_t) },
        });
    }
};

struct HexamerStats {
    struct Key {
        unsigned group;
        unsigned hexamer;
        
        bool operator <(Key const &rhs) const {
            if (group < rhs.group) return true;
            if (rhs.group < group) return false;
            if (hexamer < rhs.hexamer) return true;
            if (rhs.hexamer < hexamer) return false;
            return false;
        }
        
        Key(unsigned group, unsigned hexamer) : group(group), hexamer(hexamer & 0xFFF) {}
        
        char *kmer(char *const rslt) const {
            rslt[6] = '\0';
            rslt[5] = "ACGT"[(hexamer >>  0) & 0x03];
            rslt[4] = "ACGT"[(hexamer >>  2) & 0x03];
            rslt[3] = "ACGT"[(hexamer >>  4) & 0x03];
            rslt[2] = "ACGT"[(hexamer >>  6) & 0x03];
            rslt[1] = "ACGT"[(hexamer >>  8) & 0x03];
            rslt[0] = "ACGT"[(hexamer >> 10) & 0x03];
            return rslt;
        }
    };
    struct Value {
        uint64_t occurence, consensus;
        
        Value() : occurence(0), consensus(0) {}
    };
    std::map<Key, Value> stats;
    
    void update(unsigned const group, unsigned const length, char const *const sequence, char const *const consensus) {
        assert(length > 0);
        if (std::islower(sequence[0])) {
            unsigned kmer = 0;
            unsigned n = 0;
            
            for (unsigned i = length; i > 0; ) {
                auto const base = sequence[--i];
                switch (base) {
                    case 'a': kmer = (kmer << 2) + 0; ++n; break;
                    case 'c': kmer = (kmer << 2) + 1; ++n; break;
                    case 'g': kmer = (kmer << 2) + 2; ++n; break;
                    case 't': kmer = (kmer << 2) + 3; ++n; break;
                    default:
                        kmer = n = 0;
                        break;
                }
                if (n < 6) continue;
                if (i > 0) {
                    auto const key = Key(group, kmer);
                    auto &value = stats[key];
                    value.occurence += 1;
                    if (consensus[i - 1] != '.')
                        value.consensus += 1;
                }
                n = 5;
            }
        }
        else {
            unsigned kmer = 0;
            unsigned n = 0;
            
            for (unsigned i = 0; i < length; ++i) {
                auto const base = sequence[i];
                switch (base) {
                case 'A': kmer = (kmer << 2) + 0; ++n; break;
                case 'C': kmer = (kmer << 2) + 1; ++n; break;
                case 'G': kmer = (kmer << 2) + 2; ++n; break;
                case 'T': kmer = (kmer << 2) + 3; ++n; break;
                default:
                    kmer = n = 0;
                    break;
                }
                if (n < 6) continue;
                if (i + 1 < length) {
                    auto const key = Key(group, kmer);
                    auto &value = stats[key];
                    value.occurence += 1;
                    if (consensus[i + 1] != '.')
                        value.consensus += 1;
                }
                n = 5;
            }
        }
    }
    std::vector<Value> get(unsigned const group, unsigned const length, char const *const sequence) const {
        assert(length > 0);

        auto result = std::vector<Value>(length, Value());
        
        if (std::islower(sequence[0])) {
            unsigned kmer = 0;
            unsigned n = 0;
            
            for (unsigned i = length; i > 0; ) {
                auto const base = sequence[--i];
                switch (base) {
                    case 'a': kmer = (kmer << 2) + 0; ++n; break;
                    case 'c': kmer = (kmer << 2) + 1; ++n; break;
                    case 'g': kmer = (kmer << 2) + 2; ++n; break;
                    case 't': kmer = (kmer << 2) + 3; ++n; break;
                    default:
                        kmer = n = 0;
                        break;
                }
                if (n < 6) continue;
                if (i > 0) {
                    auto const key = Key(group, kmer);
                    auto const fnd = stats.find(key);
                    if (fnd != stats.end())
                        result[i - 1] = fnd->second;
                }
                n = 5;
            }
        }
        else {
            unsigned kmer = 0;
            unsigned n = 0;
            
            for (unsigned i = 0; i < length; ++i) {
                auto const base = sequence[i];
                switch (base) {
                    case 'A': kmer = (kmer << 2) + 0; ++n; break;
                    case 'C': kmer = (kmer << 2) + 1; ++n; break;
                    case 'G': kmer = (kmer << 2) + 2; ++n; break;
                    case 'T': kmer = (kmer << 2) + 3; ++n; break;
                    default:
                        kmer = n = 0;
                        break;
                }
                if (n < 6) continue;
                if (i + 1 < length) {
                    auto const key = Key(group, kmer);
                    auto const fnd = stats.find(key);
                    if (fnd != stats.end())
                        result[i + 1] = fnd->second;
                }
                n = 5;
            }
        }
        return result;
    }
    static void addTableTo(Writer2 &writer) {
        writer.addTable("ALIGNED_KMER_STATISTICS", {
            { "READ_GROUP", sizeof(char) },
            { "KMER", sizeof(char) },
            { "OCCURENCE", sizeof(uint64_t) },
            { "CONSENSUS", sizeof(uint64_t) },
        });
    }

    void write(Writer2 const &out) const {
        auto const table = out.table("ALIGNED_KMER_STATISTICS");
        auto const READ_GROUP = table.column("READ_GROUP");
        auto const KMER = table.column("KMER");
        auto const OCCURENCE = table.column("OCCURENCE");
        auto const CONSENSUS = table.column("CONSENSUS");
        char kmer[8];
        
        for (auto && i : stats) {
            auto const &readGroup = groups[i.first.group];
            
            READ_GROUP.setValue(readGroup);
            KMER.setValue(std::string(i.first.kmer(kmer)));
            OCCURENCE.setValue(i.second.occurence);
            CONSENSUS.setValue(i.second.consensus);
            
            table.closeRow();
        }
    }
};

struct Fragment {
    std::string sequence;
    unsigned split;
    int pos[2];
    char layout[4];
    int phase = 0;
    int edits = 0;
    
    int localToGlobal(int local) const {
        return local + (local < split ? pos[0] : (pos[1] - split));
    }
    unsigned intersectsRead(int global) const {
        auto const end1 = pos[0] + split;
        auto const end2 = pos[1] + unsigned(sequence.length()) - split;

        if (pos[0] <= global && global < end1) return 1;
        if (pos[1] <= global && global < end2) return 2;
        return 0;
    }
    class iterator {
        friend Fragment;
        Fragment const &parent;
        unsigned current;
        
        iterator(Fragment const &parent) : parent(parent), current(unsigned(parent.sequence.length())) {}
        iterator(Fragment const &parent, unsigned current) : parent(parent), current(current) {}
    public:
        iterator &operator ++() {
            if (current < parent.sequence.length())
                ++current;
            return *this;
        }
        bool operator !=(iterator const &other) {
            return current != other.current;
        }
        using value_type = std::pair<int, char>;
        value_type operator *() const {
            return std::make_pair(parent.localToGlobal(current), parent.sequence[current]);
        }
        value_type operator ->() const {
            return std::make_pair(parent.localToGlobal(current), parent.sequence[current]);
        }
    };
    iterator begin() const { return iterator(*this, 0); }
    iterator end() const { return iterator(*this); }
    
    Fragment(Cursor::Row const &data) {
        {
            auto const tmp = data.layout();
            std::copy(tmp.value, tmp.value + 4, layout);
        }
        auto const cigar = data.CIGAR();
        auto const length = data.length();
        auto const l1 = cigar.first.qlength;
        auto const l2 = cigar.second.qlength;
        auto const s = data.sequence();
        assert(length >= l1 + l2);
        assert(s.size() == l1 + l2);
        auto s1 = s.substr(0, l1);
        auto s2 = s.substr(l1);
        
        split = l1;
        pos[0] = data.position();
        pos[1] = length + pos[0] - l2;
        
        if ((layout[0] == '1' && layout[1] == '-') || (layout[2] == '1' && layout[3] == '-')) {
            for (auto && ch : s1)
                ch = std::tolower(ch);
        }
        if ((layout[0] == '2' && layout[1] == '-') || (layout[2] == '2' && layout[3] == '-')) {
            for (auto && ch : s2)
                ch = std::tolower(ch);
        }
        sequence = s1 + s2;
    }
    std::string read(unsigned const n) const {
        return n == 0 ? sequence.substr(0, split) : n == 1 ? sequence.substr(split) : sequence.substr(0, 0);
    }
    unsigned readNo(unsigned const n) const {
        auto const num = n == 0 ? layout[0] : n == 1 ? layout[2] : '\0';
        return num == '1' ? 1 : num == '2' ? 2 : 0;
    }
    bool fwd(unsigned readNo) const {
        if (readNo == 1)
            return (layout[0] == '1' && layout[1] == '+') || (layout[2] == '1' && layout[3] == '+');
        if (readNo == 2)
            return (layout[0] == '2' && layout[1] == '+') || (layout[2] == '2' && layout[3] == '+');
        return false;
    }
    bool rev(unsigned readNo) const {
        if (readNo == 1)
            return (layout[0] == '1' && layout[1] == '-') || (layout[2] == '1' && layout[3] == '-');
        if (readNo == 2)
            return (layout[0] == '2' && layout[1] == '-') || (layout[2] == '2' && layout[3] == '-');
        return false;
    }
    bool fwd() const {
        return (layout[0] == '1' && layout[1] == '+') || (layout[2] == '1' && layout[3] == '+');
    }
    bool rev() const {
        return (layout[0] == '1' && layout[1] == '-') || (layout[2] == '1' && layout[3] == '-');
    }
    /**
     The global position of the begin and end of the sequence
     */
    std::pair<int, int> bounds() const {
        return std::make_pair(localToGlobal(0), localToGlobal((int)sequence.length()));
    }
    bool operator <(Fragment const &other) const {
        return pos[0] < other.pos[0];
    }
    
    void update6merStats(unsigned const group, HexamerStats &stats, std::string const &consensus) {
        auto const s1 = sequence.data();
        auto const l1 = split;
        auto const s2 = s1 + l1;
        auto const l2 = decltype(l1)(sequence.size() - l1);
        
        stats.update(group, l1, s1, consensus.data() + pos[0]);
        stats.update(group, l2, s2, consensus.data() + pos[1]);
    }
    std::vector<int> getStats(unsigned const group, HexamerStats const &hexamer, CycleStats const &cycle) const {
        auto result = std::vector<int>(sequence.length(), -1);
        auto const s1 = sequence.data();
        auto const l1 = split;
        auto const s2 = s1 + l1;
        auto const l2 = decltype(l1)(sequence.length() - l1);
        {
            auto const r = hexamer.get(group, l1, s1);
            for (auto && v : r) {
                auto const i = &v - &r[0];
                if (v.occurence > 0) {
                    result[i] = 0;
                    if (v.consensus > 0) {
                        auto const ratio = double(v.consensus) / v.occurence;
                        auto const q = -256.0 * log2(1.0 - ratio);
                        result[i] = q < INT_MAX ? int(lround(q)) : INT_MAX;
                    }
                }
                else {
                    auto const c = rev(1) ? (split - 1) - int(i) : int(i);
                    auto const key = CycleStats::Key(group, 1, c);
                    auto const value = cycle.stats.find(key);
                    if (value != cycle.stats.end()) {
                        auto const ratio = double(value->second.consensus) / value->second.occurence;
                        auto const q = -256.0 * log2(1.0 - ratio);
                        result[i] = q < INT_MAX ? int(lround(q)) : INT_MAX;
                    }
                }
            }
        }
        {
            auto const r = hexamer.get(group, l2, s2);
            for (auto && v : r) {
                auto const i = (&v - &r[0]) + l1;
                if (v.occurence > 0) {
                    result[i] = 0;
                    if (v.consensus > 0) {
                        auto const ratio = double(v.consensus) / v.occurence;
                        auto const q = -256.0 * log2(1.0 - ratio);
                        result[i] = q < INT_MAX ? int(lround(q)) : INT_MAX;
                    }
                }
                else {
                    auto const c = rev(2) ? int((sequence.length() - 1) - i) : int(i - l1);
                    auto const key = CycleStats::Key(group, 2, c);
                    auto const value = cycle.stats.find(key);
                    if (value != cycle.stats.end()) {
                        auto const ratio = double(value->second.consensus) / value->second.occurence;
                        auto const q = -256.0 * log2(1.0 - ratio);
                        result[i] = q < INT_MAX ? int(lround(q)) : INT_MAX;
                    }
                }
            }
        }
        return result;
    }
};

static int process(std::string const &inpath, Writer2 const &out) {
    auto const mgr = VDB::Manager();
    auto const db = mgr[inpath];
    auto const contigs = Contigs::load(db);
    
    class Pileup {
        struct Edit {
            unsigned position;
            char from, to;
        };
        std::vector<Fragment> pileup;
        std::vector<Edit> edits;
        HexamerStats hexamerStats;
        CycleStats cycleStats;
        
        using CountsWithPosition = std::vector<std::pair<unsigned, Count>>;
        CountsWithPosition getCountsWithPosition() const {
            using Result = CountsWithPosition;
            using Pair = Result::value_type;
            using Index = Pair::first_type;

            auto maxpos = Index(0);
            for (auto && frag : pileup) {
                auto const endpos = Index(frag.pos[1] + frag.sequence.length() - frag.split);
                maxpos = std::max(maxpos, endpos);
            }
            auto result = Result(maxpos, Pair({ Index(0), Count(0) }));
            for (auto i = Index(0); i < maxpos; ++i) {
                result[i].first = i;
            }
            for (auto && frag : pileup) {
                for (auto posBase : frag) {
                    assert(posBase.first >= 0 && posBase.first < maxpos);
                    auto & accum = result[posBase.first].second;
                    accum.add(posBase.second);
                }
            }
            return result;
        }
        static std::string consensusFrom(CountsWithPosition const &counts) {
            auto consensus = std::string(counts.size(), '_');
            for (auto && c : counts) {
                auto &ch = consensus[c.first];
                if (!c.second.isAllSame()) {
                    ch = '.';
                    continue;
                }
                if (c.second.isA())
                    ch = 'A';
                else if (c.second.isC())
                    ch = 'C';
                else if (c.second.isG())
                    ch = 'G';
                else if (c.second.isT())
                    ch = 'T';
            }
            return consensus;
        }
    public:
        void add(Fragment frag) {
            if (frag.pos[0] + frag.split > frag.pos[1])
                return;
            pileup.emplace_back(frag);
        }
        void processStats(VDB::Cursor::RowID const contigID) {
            auto const group = unsigned(0);
            auto const counts = getCountsWithPosition();
            auto const consensus = consensusFrom(counts);
            
            for (auto && frag : pileup) {
                auto const r1 = frag.rev(1);
                auto const r2 = frag.rev(2);
                auto const split = frag.split;
                
                frag.update6merStats(group, hexamerStats, consensus);
                for (auto && base : frag.sequence) {
                    auto const local = unsigned(&base - &frag.sequence[0]);
                    auto const global = frag.localToGlobal(local);
                    auto const consensus = counts[global].second.isAllSame();
                    if (local < split) {
                        auto const first = 0;
                        auto const last = split - 1;
                        auto const cycle = r1 ? (last - local) : (local - first);
                        cycleStats.add(group, 1, cycle, consensus);
                    }
                    else {
                        auto const first = split;
                        auto const last = unsigned(frag.sequence.length()) - 1;
                        auto const cycle = r2 ? (last - local) : (local - first);
                        cycleStats.add(group, 2, cycle, consensus);
                    }
                }
            }
        }
        void process(VDB::Cursor::RowID const contigID) {
            unsigned const group = 0;
            auto counts = getCountsWithPosition();
            for (auto && c : counts) {
                if (!c.second.isAllSame()) {
                    auto const s = c.second.sorted();
                    if (s.n1 == s.n0 || s.n1 != 3) {
                        continue;
                    }
                    // this position has some one-of bases and
                    // it has only one base that occurs more than once
                    auto edit = Edit({c.first, 0, char(s.count[3].base())});
                    for (auto && frag : pileup) {
                        if (frag.intersectsRead(edit.position)) {
                            for (auto && base : frag.sequence) {
                                if (frag.localToGlobal(int(&base - &frag.sequence[0])) != edit.position) continue;

                                edit.from = std::toupper(base);
                                if (edit.from != edit.to) {
                                    c.second.remove(base);
                                    base = std::islower(base) ? std::tolower(edit.to) : edit.to;
                                    c.second.add(base);
                                    frag.edits += 1;
                                    edits.push_back(edit);
                                }
                            }
                        }
                    }
                    assert(c.second.isAllSame());
                }
            }
            {
                auto q = std::vector<std::vector<int>>(pileup.size(), std::vector<int>());
                for (auto && c : counts) {
                    if (c.second.isAllSame()) continue;
                    for (auto j = 0; j < pileup.size(); ++j) {
                        if (q[j].size() != 0) continue;
                        auto const &frag = pileup[j];
                        if (frag.intersectsRead(c.first)) {
                            q[j] = frag.getStats(group, hexamerStats, cycleStats);
                            assert(frag.sequence.length() == q[j].size());
                        }
                    }
                }
                for (auto && c : counts) {
                    if (c.second.isAllSame() || c.second.N == 0 || c.second.n == 0) continue;
                    
                    auto const plus = Count::Sorted(c.second.A, c.second.C, c.second.G, c.second.T);
                    if (plus.count[3].count() < plus.count[2].count()) continue;
                    
                    auto const minus = Count::Sorted(c.second.a, c.second.c, c.second.g, c.second.t);
                    if (minus.count[3].count() < minus.count[2].count()) continue;
                    
                    if (plus.count[3].base() == minus.count[3].base()) continue;
                    
                    auto const bplus = char(plus.count[3].base());
                    auto const bminus = char(std::tolower(minus.count[3].base()));
                    double qplus = 0, qminus = 0;
                    for (auto j = 0; j < pileup.size(); ++j) {
                        auto const &frag = pileup[j];
                        if (!frag.intersectsRead(c.first)) continue;
                        auto const &Q = q[j];
                        assert(Q.size() == frag.sequence.length());
                        for (auto k = 0; k < Q.size(); ++k) {
                            if (frag.localToGlobal(k) != c.first) continue;
                            auto const qscore = Q[k];
                            auto const base = frag.sequence[k];
                            if (base == bplus)
                                qplus += qscore;
                            else if (base == bminus)
                                qminus += qscore;
                        }
                    }
                    qplus /= plus.count[3].count();
                    qminus /= minus.count[3].count();
                    
                    auto edit = Edit({c.first, 0, char(qplus > qminus ? plus.count[3].base() : qminus > qplus ? minus.count[3].base() : 'N')});
                    for (auto && frag : pileup) {
                        if (frag.intersectsRead(edit.position)) {
                            for (auto && base : frag.sequence) {
                                if (frag.localToGlobal(int(&base - &frag.sequence[0])) != edit.position) continue;
                                
                                edit.from = std::toupper(base);
                                if (edit.from != edit.to) {
                                    c.second.remove(base);
                                    base = edit.to != 'N' ? std::islower(base) ? std::tolower(edit.to) : edit.to : 'N';
                                    c.second.add(base);
                                    frag.edits += 1;
                                    edits.push_back(edit);
                                }
                            }
                        }
                    }
                    assert((c.second.N == 0 && c.second.n == 0) || c.second.isAllSame());
                }
            }
            {
                for (auto && c : counts) {
                    if (c.second.isAllSame()) continue;
                    
                    auto const s = c.second.sorted();
                    auto const N = s.count[0].count() + s.count[1].count() + s.count[2].count() + s.count[3].count();
                    if (s.n1 == 3 || 3 * s.count[3].count() >= 2 * N || s.count[3].count() >= 2 * s.count[2].count()) {
                        auto edit = Edit({c.first, 0, char(s.count[3].base())});
                        for (auto && frag : pileup) {
                            if (frag.intersectsRead(edit.position)) {
                                for (auto && base : frag.sequence) {
                                    if (frag.localToGlobal(int(&base - &frag.sequence[0])) != edit.position) continue;
                                    
                                    edit.from = std::toupper(base);
                                    if (edit.from != edit.to) {
                                        c.second.remove(base);
                                        base = edit.to != 'N' ? std::islower(base) ? std::tolower(edit.to) : edit.to : 'N';
                                        c.second.add(base);
                                        frag.edits += 1;
                                        edits.push_back(edit);
                                    }
                                }
                            }
                        }
                        assert(c.second.isAllSame());
                    }
                }
            }
            {
                auto consensus = std::string(counts.size() > 0 ? counts.back().first + 1 : 0, '_');
                for (auto && c : counts) {
                    auto const i = c.first;
                    if (consensus.size() < i + 1)
                        consensus.resize(i + 1, '_');
                    if (c.second.N == 0 && c.second.n == 0) {
                        consensus[i] = 'N';
                        continue;
                    }
                    if (c.second.A == c.second.N && c.second.a == c.second.n) {
                        consensus[i] = 'A';
                        continue;
                    }
                    if (c.second.C == c.second.N && c.second.c == c.second.n) {
                        consensus[i] = 'C';
                        continue;
                    }
                    if (c.second.G == c.second.N && c.second.g == c.second.n) {
                        consensus[i] = 'G';
                        continue;
                    }
                    if (c.second.T == c.second.N && c.second.t == c.second.n) {
                        consensus[i] = 'T';
                        continue;
                    }
                    consensus[i] = '.';
                }
                printf("%.*s\n", int(consensus.size()), consensus.data());
                for (auto && c : counts) {
                    if (c.second.isAllSame()) continue;
                    printf("%uA %ua %uC %uc %uG %ug %uT %ut\n", c.second.A, c.second.a, c.second.C, c.second.c, c.second.G, c.second.g, c.second.T, c.second.t);
                }
            }
        }
        void clear() {
            pileup.clear();
            edits.clear();
        }
        void finalizeStats(Writer2 const &out) {
            cycleStats.write(out);
            hexamerStats.write(out);
        }
    };
    
    auto pile = Pileup();
    auto activeContigID = VDB::Cursor::RowID(0);
    auto const curs = Cursor(db["FRAGMENTS"]);
    auto const range = curs.rowRange();
    
    curs.foreach([&](VDB::Cursor::RowID row, std::vector<VDB::Cursor::RawData> const &rawData) {
        auto const last = (row + 1) == range.end();
        auto &data = static_cast<Cursor::Row const &>(rawData);
        auto const contigID = data.contigID();
        enum { add, flush } command = (activeContigID == contigID || row == range.beg()) ? add : flush ;

        do {
            switch (command) {
            case add:
                activeContigID = contigID;
                pile.add(data);
                if (!last)
                    return;
            case flush:
                pile.processStats(activeContigID);
                pile.clear();
                command = add;
            }
        } while (!last);
    });
    pile.finalizeStats(out);

#if 0
    curs.foreach([&](VDB::Cursor::RowID row, std::vector<VDB::Cursor::RawData> const &rawData) {
        auto const last = (row + 1) == range.end();
        auto &data = static_cast<Cursor::Row const &>(rawData);
        auto const contigID = data.contigID();
        enum { add, flush } command = (activeContigID == contigID || row == range.beg()) ? add : flush ;
        
        do {
            switch (command) {
                case add:
                    activeContigID = contigID;
                    pile.add(data);
                    if (!last)
                        return;
                case flush:
                    pile.process(activeContigID);
                    pile.clear();
                    command = add;
            }
        } while (!last);
    });
#endif
    return 0;
}

static int process(std::string const &inpath, FILE *const out)
{
    auto writer = Writer2(out);
    
    writer.destination("consensus.db");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:aligned");
    writer.info("contig-consensus", "1.0.0");
    
    Contigs::addTableTo(writer);
    CycleStats::addTableTo(writer);
    HexamerStats::addTableTo(writer);

    writer.beginWriting();
    auto const rslt = process(inpath, writer);
    writer.endWriting();
    return rslt;
}

namespace contig_consensus {
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << commandLine.program[0] << " [-out=<path>] <database>" << std::endl;
        exit(error ? 3 : 0);
    }
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "--help" || arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine, false);
            }
        }
        auto inpath = std::string();
        auto outfile = std::string();
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                outfile = arg.substr(5);
                continue;
            }
            if (inpath.empty()) {
                inpath = arg;
                continue;
            }
            usage(commandLine, true);
        }
        if (inpath.empty())
            usage(commandLine, true);
        if (outfile.empty())
            return process(inpath, stdout);
        
        auto strm = fopen(outfile.c_str(), "w");
        if (strm == nullptr) {
            perror(outfile.c_str());
            exit(3);
        }
        auto const rslt = process(inpath, strm);
        fclose(strm);
        return rslt;
    }
}

int main(int argc, const char * argv[]) {
    return contig_consensus::main(CommandLine(argc, argv));
}
