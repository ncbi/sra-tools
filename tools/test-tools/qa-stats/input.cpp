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
 *  Loader QA Stats
 *
 * Purpose:
 *  Parse inputs.
 */


#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cassert>

#include "input.hpp"

std::vector<std::string> Input::references;
std::vector<std::string> Input::groups;

template < typename S, typename T >
static void extract(S const &src, T &out) {
    auto const s = std::string(src);
    auto iss = std::istringstream(s);

    iss >> out;
    iss.exceptions(std::ios_base::failbit);
    if (iss.eof())
        return;
    iss >> std::ws;
    iss.get();
    if (!iss.eof())
        throw std::ios_base::failure("Incomplete extraction");
}

std::istream &operator >>(std::istream &is, CIGAR::OP &out) {
    unsigned length = 0;
    char ch = 0;
    int count = 0;

    while (is.get(ch) && std::isdigit(ch)) {
        length = length * 10 + (ch - '0');
        if (length > (1ul << 28))
            throw std::ios_base::failure("Invalid CIGAR: excessing operation length");
        ++count;
    }
    switch (ch) {
    case '*':
        if (count != 0) // must be the first character
            throw std::ios_base::failure("Invalid CIGAR");
        // fall through
    case 'M': ch = 0; break;
    case 'I': ch = 1; break;
    case 'D': ch = 2; break;
    case 'N': ch = 3; break;
    case 'S': ch = 4; break;
    case 'H': ch = 5; break;
    case 'P': ch = 6; break;
    case 'X': ch = 7; break;
    case '=': ch = 8; break;
    case 'B': ch = 9; break;
    default:
        throw std::ios_base::failure("Invalid CIGAR opcode");
    }
    out.length = length;
    out.opcode = ch;
    return is;
}

std::istream &operator >>(std::istream &is, CIGAR &out) {
    int count = 0;

    out.operations.clear();
    while (is && !is.eof()) {
        CIGAR::OP op;

        is >> op;
        if (op.length == 0) {
            if (count != 0) // must be the first character
                throw std::ios_base::failure("Invalid CIGAR");
            break;
        }
        out.operations.push_back(std::move(op));
        is.peek();
    }
    return is;
}

struct Delimited {
    using Container = std::vector<std::string_view>;
    using Iterator = Container::const_iterator;
    std::vector<std::string_view> part;

    Delimited() {}
    Delimited(std::string_view const &whole, std::string_view::value_type separator)
    {
        auto cur = whole.data();
        auto const end = cur + whole.size();
        auto fbeg = decltype(cur)(0);
        auto fend = fbeg;

        while (cur < end) {
            auto const at = cur;
            auto const ch = *cur++;

            if (ch == separator || cur == end) {
                if (!fbeg) break;
                if (cur == end && !isspace(ch))
                    fend = cur;

                auto const &field = std::string_view(fbeg, fend - fbeg);
                part.emplace_back(field);
                fbeg = fend = nullptr;
                continue;
            }
            if (isspace(ch)) continue;

            fend = cur;
            if (fbeg == nullptr)
                fbeg = at;
        }
    }
    Iterator begin() const { return part.begin(); }
    Iterator end() const { return part.end(); }

    template <typename C, typename T = typename C::value_type>
    friend void operator >>(Delimited const &in, C &out) {
        for (auto && i : in) {
            T x;
            extract(i, x);
            out.emplace_back(x);
        }
    }
};

struct RawReadType {
    Input::ReadType type;
    Input::ReadOrientation strand;

    RawReadType()
    : type(Input::ReadType::biological)
    , strand(Input::ReadOrientation::forward)
    {}

    friend std::istream &operator >>(std::istream &is, RawReadType &out) {
        std::string str;

        is >> str;

        int fwd = 0;
        int rev = 0;
        int bio = 0;
        int tec = 0;

        for (auto & part : Delimited(str, '|')) {
            if (part == "SRA_READ_TYPE_BIOLOGICAL") {
                ++bio;
                continue;
            }
            if (part == "SRA_READ_TYPE_TECHNICAL") {
                ++tec;
                continue;
            }
            if (part == "SRA_READ_TYPE_FORWARD") {
                ++fwd;
                continue;
            }
            if (part == "SRA_READ_TYPE_REVERSE") {
                ++rev;
                continue;
            }
            // std::cerr << "Unrecognized READ_TYPE value: '" << part << "'\n";
            throw std::ios_base::failure("READ_TYPE");
        }
        if (fwd > 1 || rev > 1 || (fwd + rev) > 1 || bio > 1 || tec > 1 || (bio + tec) > 1) {
            // std::cerr << "Bad READ_TYPE: '" << str << "'\n";
            throw std::ios_base::failure("READ_TYPE");
        }
        if (rev)
            out.strand = Input::ReadOrientation::reverse;
        else
            out.strand = Input::ReadOrientation::forward;

        if (tec)
            out.type = Input::ReadType::technical;
        else
            out.type = Input::ReadType::biological;

        return is;
    }
};

static void cleanUpSegments(std::string &sequence, std::vector<int> &lengths, std::vector<int> &starts, std::vector<uint64_t> const &aligned)
{
    struct Segment {
        int start, length;
        bool aligned;
        int new_start, new_length;

        int end() const { return start + length; }
        int new_end() const { return new_start + new_length; }

        int adjust(int offset) {
            new_start = start - offset;
            new_length = length;
            if (aligned) {
                offset += length;
                new_length = 0;
            }
            return offset;
        }
        bool operator <(Segment const &rhs) const {
            if (start < rhs.start)
                return true;
            if (length < rhs.length)
                return true;
            return false;
        }
        static std::vector<Segment> load(std::vector<int> const &lengths, std::vector<int> const &starts, std::vector<uint64_t> const &aligned)
        {
            std::vector<Segment> segment;

            segment.reserve(lengths.size());
            for (auto const &x : starts) {
                auto const i = &x - &starts[0];
                auto const &len = lengths[i];
                auto const is_aligned = aligned[i] != 0;

                segment.emplace_back(Segment{x, len, is_aligned});
            }
            std::sort(segment.begin(), segment.end());
            return segment;
        }
    };
    bool someAligned = false;
    for (auto const &x : aligned) {
        someAligned |= x != 0;
    }
    if (!someAligned)
        return;

    std::vector<Segment> segment = Segment::load(lengths, starts, aligned);

    int offset = 0;
    for (auto &&x : segment) {
        offset = x.adjust(offset);
    }

    starts.clear();
    lengths.clear();
    for (auto && x : segment) {
        starts.push_back(x.new_start);
        lengths.push_back(x.new_length);
    }

    if (sequence.size() < segment.back().end()) {
        // the original sequence was CMP_READ
        ;
    }
    else {
        // the original sequence was READ and needs to be shortened
        std::string new_sequence;

        new_sequence.reserve(segment.back().new_end());
        for (auto && x : segment) {
            if (new_sequence.length() < x.new_start)
                new_sequence.append(x.new_start - new_sequence.length(), 'N');
            new_sequence.append(sequence.substr(x.start, x.new_length));
        }
        sequence.swap(new_sequence);
    }
}

static void readFromFASTQ(std::istream &is, std::string &line, Input &result) {
}

static void readFromSAM(Delimited const &flds, Input &result) {
    auto const FLAG = &flds.part[1];
    auto const RNAME = &flds.part[2];
    auto const POS = &flds.part[3];
    auto const CIGAR = &flds.part[5];
    auto RG = flds.part[10];
    int flags = 0;

    extract(flds.part[1], flags);

    for (auto i = 10; i < flds.part.size(); ++i) {
        if (flds.part[i].substr(0, 5) == "RG:Z:") {
            RG = flds.part[i].substr(5);
            break;
        }
    }
}

Input Input::readFrom(std::istream &is, bool isfirst) {
    auto result = Input{};
    std::string line;

    is.exceptions(std::ios_base::failbit | std::ios_base::eofbit);
    std::getline(is, line);

    if (isfirst && line.size() > 0 && line[0] == '@') {
        // may be FASTQ or SAM
        if (line.size() > 7 && line.substr(0, 7) == "@HD\tVN:") {
            // read past SAM header lines
            do { std::getline(is, line); } while (line.size() > 0 && line[0] == '@');
        }
    }

    if (line.size() > 0 && (line[0] == '@' || line[0] == '>')) {
        readFromFASTQ(is, line, result);
        return result;
    }
    auto const &flds = Delimited(line, '\t');
    std::string_view const *group = nullptr;
    struct RawRead {
        std::string_view const *lengths
                             , *starts
                             , *types
                             , *aligned;
    } read{};

    switch (flds.part.size()) {
    case 6:
        group = &flds.part[5];
    case 5:
        read.aligned = &flds.part[4];
    case 4:
        read.types = &flds.part[3];
    case 3:
        read.starts = &flds.part[2];
    case 2:
        read.lengths = &flds.part[1];
    case 1:
        result.sequence = flds.part[0];
        break;
    default:
        if (flds.part.size() >= 11) {
            // SAM has at least 11 fields, the other formats have at most 6 fields.
            readFromSAM(flds, result);
        }
        return result;
    }

    if (read.lengths) {
        int decoded = 0;
        std::vector<int> lengths;
        std::vector<int> starts;
        std::vector<RawReadType> types;
        std::vector<uint64_t> aligned; // some integer values

        try {
            Delimited(*read.lengths, ',') >> lengths;
            decoded += 1;
        }
        catch (std::ios_base::failure const &e) {
            // read lengths are not numeric, so it must be an aligned record
            (void)(e);
            goto NOT_A_READ;
        }
        if (decoded > 0 && read.starts) {
            try {
                Delimited(*read.starts, ',') >> starts;
                decoded += 2;
            }
            catch (std::ios_base::failure const &e) {
                // the field might contain read types
                if (group)
                    goto NOT_A_READ;
                group = read.aligned;
                read.aligned = read.types;
                read.types = read.starts;
                read.starts = nullptr;
                (void)(e);
            }
        }
        if (decoded > 0 && read.types) {
            try {
                Delimited(*read.types, ',') >> types;
                decoded += 4;
            }
            catch (std::ios_base::failure const &e) {
                if (group)
                    goto NOT_A_READ;
                group = read.aligned;
                read.aligned = read.types;
                read.types = nullptr;
                (void)(e);
            }
        }
        if (decoded > 0 && read.aligned) {
            try {
                Delimited(*read.aligned, ',') >> aligned;
                decoded += 8;
            }
            catch (std::ios_base::failure const &e) {
                if (group)
                    goto NOT_A_READ;
                group = read.aligned;
                read.aligned = nullptr;
                (void)(e);
            }
        }
        if (decoded == 0) {
            // sequence is one biological forward read
            lengths.push_back((int)result.sequence.size());
            starts.push_back(0);
            types.push_back(RawReadType());
        }
        if ((decoded & 2) == 0) {
            // synthesize the values
            starts.reserve(lengths.size());
            starts.push_back(0);
            for (auto len : lengths) {
                if (starts.size() == lengths.size())
                    break;
                starts.push_back(starts.back() + len);
            }
        }
        if ((decoded & 4) == 0) {
            // synthesize the values
            types.resize(lengths.size(), RawReadType());
        }
        if ((decoded & 8) == 0) {
            // synthesize the values
            aligned.resize(lengths.size(), 0);
        }
        if (lengths.size() != starts.size() || lengths.size() != types.size() || lengths.size() != aligned.size())
            goto NOT_A_READ;

        cleanUpSegments(result.sequence, lengths, starts, aligned);

        for (auto x : lengths) {
            if (x < 0 || x > result.sequence.size())
                goto NOT_A_READ;
        }
        for (auto x : starts) {
            if (x < 0 || x > result.sequence.size())
                goto NOT_A_READ;
        }
        for (auto const &len : lengths) {
            auto const i = &len - &lengths[0];
            auto const end = len + starts[i];
            if (end < 0 || end > result.sequence.size())
                goto NOT_A_READ;
        }

        result.reads.reserve(lengths.size());
        for (auto const &len : lengths) {
            auto const i = &len - &lengths[0];
            result.reads.emplace_back(Input::Read{starts[i], lengths[i], -1, -1, types[i].type, types[i].strand});
        }
    }
    else {
NOT_A_READ:
        struct RawAligned {
            std::string_view const *reference
                                 , *position
                                 , *orientation
                                 , *cigar;
        } aligned{};
        Read read{};

        read.start = 0;
        read.length = (int)result.sequence.length();
        read.type = ReadType::aligned;

        switch (flds.part.size()) {
        case 6:
            group = &flds.part[5];
        case 5:
            aligned.cigar = &flds.part[4];
        case 4:
            aligned.orientation = &flds.part[3];
            aligned.position = &flds.part[2];
            aligned.reference = &flds.part[1];
            break;
        default:
            return result;
        }

        extract(*aligned.position, read.position);

        for (read.reference = 0; read.reference < references.size(); ++read.reference) {
            if (references[read.reference] == *aligned.reference)
                break;
        }
        if (read.reference == references.size())
            references.push_back(std::string(*aligned.reference));

        if (*aligned.orientation == "0" || *aligned.orientation == "false")
            read.orientation = ReadOrientation::forward;
        else if (*aligned.orientation == "1" || *aligned.orientation == "true")
            read.orientation = ReadOrientation::reverse;
        else
            throw std::ios_base::failure("Invalid input");

        if (aligned.cigar) {
            try {
                extract(*aligned.cigar, read.cigar);
                if (read.length != read.cigar.sequenceLength())
                    throw std::ios_base::failure("Invalid CIGAR");
            }
            catch (std::ios_base::failure const &e) {
                if (group)
                    throw std::ios_base::failure("Invalid input");
                group = aligned.cigar;
                aligned.cigar = nullptr;
            }
        }

        result.reads.push_back(std::move(read));
    }
    if (group) {
        for (result.group = 0; result.group < groups.size(); ++result.group) {
            if (groups[result.group] == *group)
                break;
        }
        if (result.group == groups.size())
            groups.push_back(std::string(*group));
    }
    else
        result.group = -1;
    return result;
}

#include <sstream>
static void Delimited_test() {
    auto strm = std::string_view("sequence \t 123, 456\t\n");
    auto raw = Delimited(strm, '\t');

    assert(raw.part.size() == 2);
    assert(raw.part[0] == "sequence");
    assert(raw.part[1] == "123, 456");

    auto raw2 = Delimited(raw.part[1], ',');

    assert(raw2.part.size() == 2);
    assert(raw2.part[0] == "123");
    assert(raw2.part[1] == "456");
}

static void Input_test1() {
    auto src = std::istringstream("GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL, SRA_READ_TYPE_TECHNICAL\n");
    src.exceptions(std::ios_base::badbit | std::ios_base::eofbit | std::ios_base::failbit);
    auto const &reads = Input::readFrom(src, true).reads;
    assert(reads.size() == 2);
    assert(reads[0].length == 142);
    assert(reads[1].length == 151);
    assert(reads[0].type == Input::ReadType::biological);
    assert(reads[1].type == Input::ReadType::technical);
}

static void Input_test2() {
    auto src = std::istringstream("GTGGACATCCCTCTGTGTGNGTCAN\tCM_000001\t10001\t0\t24M1S\n");
    src.exceptions(std::ios_base::badbit | std::ios_base::eofbit | std::ios_base::failbit);
    auto const &reads = Input::readFrom(src, true).reads;
    assert(reads.size() == 1);
    assert(reads[0].position == 10001);
}

void Input::runTests() {
    Delimited_test();
    Input_test2();
    Input_test1();

    Input::references.clear();
    Input::groups.clear();
}
