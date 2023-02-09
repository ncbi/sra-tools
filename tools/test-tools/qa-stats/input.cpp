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

#define RECORD_LIMIT 1000

#include <iostream>
#include <string>
#include <optional>
#include <string_view>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cassert>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

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
        auto const n = whole.empty() ? 0 : (std::count(whole.begin(), whole.end(), separator) + 1);
        if (n == 0)
            return;

        part.reserve(n);
        part.emplace_back(whole);
        if (n == 1)
            return;

        for ( ; ; ) {
            auto &cur = part.back();
            auto const split = cur.find(separator);
            if (split == std::string_view::npos)
                break;
            auto next = cur.substr(split + 1);
            cur = cur.substr(0, split);
            part.emplace_back(next);
        }
        for (auto && cur : part) {
            auto end = cur.size();
            while (end > 0 && isspace(cur[end - 1]))
                --end;

            auto start = 0;
            while (start < end && isspace(cur[start]))
                ++start;

            cur = cur.substr(start, end - start);
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

static void cleanUpSegments(std::string &sequence, std::vector<int> const &lengths, std::vector<int> const &starts, std::vector<uint64_t> const &aligned)
{
    std::string::size_type totalReadLen = 0;
    std::string::size_type cmpReadLen = 0;
    for (auto const &len : lengths) {
        auto const &alignId = aligned[&len - &lengths[0]];

        totalReadLen += len;
        if (alignId == 0)
            cmpReadLen += len;
    }
    if (totalReadLen == cmpReadLen)
        return;

    std::string new_seq;
    {
        std::string_view view(sequence);
        
        new_seq.reserve(totalReadLen);
        for (auto const &len : lengths) {
            auto const i = &len - &lengths[0];
            
            if (aligned[i] == 0) {
                new_seq.append(view.substr(0, len));
                view = view.substr(len);
            }
            else
                new_seq.append(len, '.');
        }
    }
    sequence.swap(new_seq);
    return;
}

template <typename RR = int, typename WR = int>
struct RWLock {
    std::mutex mut;
    std::condition_variable cond;
    std::atomic<int> count = 0;

    template <typename F>
    RR reader(F && f) {
        auto const prv = count.fetch_add(2);
        if ((prv & 1) != 0) {
            count.fetch_sub(2);

            // wait for writer to leave
            std::unique_lock guard(mut);
            cond.wait(guard, [this]{return (count.load() & 1) != 0;});
            count.fetch_add(2);
        }
        RR result = f();
        count.fetch_sub(2);
        cond.notify_one();
        return result;
    }
    template <typename F>
    WR writer(F && f) {
        auto const prv = count.fetch_or(1); // signal that a writer is waiting
        if (prv != 0) {
            // wait for everyone to leave while continuously signalling that a writer is waiting.
            std::unique_lock guard(mut);
            cond.wait(guard, [this]{return count.fetch_or(1) != 0;});
        }
        WR result = f();
        count.fetch_xor(1);
        cond.notify_all();
        return result;
    }
};

int Input::getGroup(std::string const &named) {
    static RWLock lock;
    auto const found = lock.reader([&]{
        for (int i = 0; i < groups.size(); ++i) {
            if (groups[i] == named)
                return i;
        }
        return -1;
    });
    if (found >= 0)
        return found;
    return lock.writer([&]{
        int i = (int)groups.size();
        groups.push_back(named);
        return i;
    });
}

int Input::getReference(std::string const &named) {
    static RWLock lock;
    auto const found = lock.reader([&]{
        for (int i = 0; i < references.size(); ++i) {
            if (references[i] == named)
                return i;
        }
        return -1;
    });
    if (found >= 0)
        return found;
    return lock.writer([&]{
        int i = (int)references.size();
        references.push_back(named);
        return i;
    });
}

#define REPORT(MSG) if (!shouldReport(__LINE__)) {} else std::cerr << "info: " << lines << ": " << MSG << std::endl;

struct BasicSource: public Input::Source {
    using Read = Input::Read;
    using ReadType = Input::ReadType;
    using ReadOrientation = Input::ReadOrientation;

    uint64_t lines = 0;
    int fh = -1;
    uint8_t *buffer;
    size_t cur = 0, next = 0, block = 0, size = 0, bmax = 0;
    bool isEof = false;
    int lastReported = 0;

    bool shouldReport(int line) {
        if (lastReported == line)
            return false;
        lastReported = line;
        return true;
    }
    bool fill() {
        if (fh < 0 || isEof)
            return false;

        if (bmax - size < block) {
            auto const temp = realloc(buffer, bmax * 2);
            if (temp == nullptr)
                throw std::bad_alloc();
            buffer = (uint8_t *)temp;
            bmax *= 2;
        }
        auto const blks = (bmax - size) / block;
        auto const n = read(fh, &buffer[size], blks * block);
        if (n < 0)
            throw std::system_error(std::error_code(errno, std::system_category()), "read");
        if (n > 0) {
            size += n;
            return true;
        }
        isEof = true;
        return false;
    }
    /// get the current line, advancing if needed, and trimming whitespace
    std::string_view peek() {
        if (next > cur)
            goto CURRENT_LINE;

        for ( ; ; ) {
            while (next < size) {
                if (buffer[next++] == '\n')
                    goto CURRENT_LINE;
            }
            if (fh < 0) {
                isEof = true;
                return std::string_view();
            }
            if (cur >= block) {
                auto const blk = cur / block;
                auto dst = &buffer[0];
                auto src = &buffer[blk * block];
                auto const end = &buffer[size];
                auto const shift = src - dst;

                size -= shift;
                cur -= shift;
                next -= shift;
                while (src < end)
                    *dst++ = *src++;
            }
            if (!fill())
                return std::string_view();
        }
    CURRENT_LINE:
        auto end = next - 1;
        while (end != cur && isspace(buffer[end - 1]))
            --end;
        return end != cur ? std::string_view((char *)&buffer[cur], (next - 1) - cur) : std::string_view();
    }
    /// Get next line, skipping empty lines.
    std::string getline() {
        cur = next;
        auto const curline = peek();
        if (curline.empty() && isEof)
            throw std::ios_base::failure("no input");
        return !curline.empty() ? std::string(curline) : getline();
    }
    ~BasicSource() {
        if (fh > 0)
            close(fh);
    }
    BasicSource(Input::Source::Type const &src) {
        bool use_mmap = false;

        if (std::holds_alternative<StringLiteralType>(src)) {
            auto const &str = std::get<StringLiteralType>(src).data;
            block = bmax = size = str.size();
            buffer = (uint8_t *)malloc(size);
            if (buffer == nullptr)
                throw std::bad_alloc();
            std::copy(str.begin(), str.end(), buffer);
            return;
        }
        if (std::holds_alternative<StdInType>(src)) {
            fh = 0;
        }
        else if (std::holds_alternative<FilePathType>(src)) {
            auto const &path = std::get<FilePathType>(src).path;
            fh = open(path, O_RDONLY);
            if (fh < 0)
                throw std::system_error(std::error_code(errno, std::system_category()), path);
            use_mmap = std::get<FilePathType>(src).use_mmap;
            if (use_mmap)
                std::cerr << "info: will try to mmap " << path << std::endl;
        }
        else {
            throw std::bad_exception();
        }

        struct stat st = {};
        if (fstat(fh, &st))
            throw std::system_error(std::error_code(errno, std::system_category()), "can't stat input handle!?");

        if (use_mmap && (S_IFMT & st.st_mode) == S_IFREG) {
            auto const temp = mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fh, 0);
            if (temp != MAP_FAILED) {
                buffer = (uint8_t *)temp;
                block = size = bmax = st.st_size;
                close(fh);
                fh = -1;
                return;
            }
            std::cerr << "info: could not mmap file" << std::endl;
        }
        else if (use_mmap) {
            std::cerr << "info: could not mmap, was not a file" << std::endl;
        }

        block = st.st_blksize;
        if (block < 4 * 1024)
            block = 4 * 1024;
        buffer = (uint8_t *)malloc(bmax = block);
        if (buffer == nullptr)
            throw std::bad_alloc();
        fill();
    }
    virtual operator bool() const { return !eof(); }
    virtual bool eof() const {
        return
#if RECORD_LIMIT
        records >= RECORD_LIMIT ||
#endif
        isEof;
    }
    enum struct ParseError {
        notNumeric_ReadLength,
        notNumeric_ReadStart,
        notReadType,
        notNumeric_AlignmentID,
        inconsistent,
        notBoolean_RefOrientation,
        invalid_CIGAR,
        not_Alignment
    };
    Input readUnaligned(Delimited const &flds) {
        std::string_view const *group = nullptr;
        struct RawRead {
            std::string_view const *lengths
                                 , *starts
                                 , *types
                                 , *aligned;
        } read{};
        auto result = Input{std::string(flds.part[0])};

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
            break;
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
                throw ParseError::notNumeric_ReadLength;
            }
            if (decoded > 0 && read.starts) {
                try {
                    Delimited(*read.starts, ',') >> starts;
                    decoded += 2;
                }
                catch (std::ios_base::failure const &e) {
                    // the field might contain read types
                    if (group)
                        throw ParseError::notNumeric_ReadStart;
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
                        throw ParseError::notReadType;
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
                        throw ParseError::notNumeric_AlignmentID;
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
                throw ParseError::inconsistent;

            cleanUpSegments(result.sequence, lengths, starts, aligned);

            for (auto x : lengths) {
                if (x < 0 || x > result.sequence.size())
                    throw ParseError::inconsistent;
            }
            for (auto x : starts) {
                if (x < 0 || x > result.sequence.size())
                    throw ParseError::inconsistent;
            }
            for (auto const &len : lengths) {
                auto const i = &len - &lengths[0];
                auto const end = len + starts[i];
                if (end < 0 || end > result.sequence.size())
                    throw ParseError::inconsistent;
            }

            result.reads.reserve(lengths.size());
            for (auto const &len : lengths) {
                auto const i = &len - &lengths[0];
                result.reads.emplace_back(Read{starts[i], lengths[i], -1, -1, aligned[i] ? ReadType::aligned : types[i].type, types[i].strand});
            }
        }
        if (read.aligned) {
            REPORT("Aligned from SEQUENCE");
        }
        else {
            REPORT("Unaligned from SEQUENCE");
        }
        return result;
    }
    Input readAligned(Delimited const &flds) {
        struct RawAligned {
            std::string_view const *reference
                                 , *position
                                 , *orientation
                                 , *cigar;
        } aligned{};
        Read read{};
        std::string_view const *group = nullptr;

        read.start = 0;
        read.length = (int)flds.part[0].length();
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
            extract(*aligned.position, read.position);

            read.reference = Input::getReference(std::string(*aligned.reference));

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
        default:
            break;
        }

        Input result{std::string(flds.part[0]), {read}};
        if (group)
            result.group = Input::getGroup(std::string(*group));

        REPORT("Alignment");
        return result;
    }
    Input readSAM(Delimited const &flds) {
        Input result{};
        auto const FLAG = &flds.part[1];
        auto RNAME = &flds.part[2];
        auto POS = &flds.part[3];
        auto CIGAR = &flds.part[5];
        auto const SEQ = &flds.part[9];
        std::string_view RG;
        std::string_view const *group = nullptr;
        int flags = 0;
        int position = -1;

        extract(*FLAG, flags);
        if ((flags & 0x001) == 0) {
            flags ^= flags & 0x002 & 0x008 & 0x020 & 0x040 & 0x080;
        }
        if (*RNAME == "*" || *CIGAR == "*" || *POS == "0")
            flags |= 0x004;
        if ((flags & 0x004) != 0) {
            flags ^= flags & 0x002 & 0x100 & 0x800;
            RNAME = POS = CIGAR = nullptr;
        }
        if (POS)
            extract(*POS, position);

        for (auto i = 11; i < flds.part.size(); ++i) {
            if (flds.part[i].substr(0, 5) == "RG:Z:") {
                RG = flds.part[i].substr(5);
                group = &RG;
                break;
            }
        }

        result.sequence = *SEQ;
        if (group)
            result.group = Input::getGroup(std::string(*group));
        else
            result.group = -1;

        Input::Read read{};

        read.start = 0;
        read.length = (int)result.sequence.length();
        if (RNAME) {
            read.type = Input::ReadType::aligned;
            read.orientation = (flags & 0x010) == 0 ? Input::ReadOrientation::forward : Input::ReadOrientation::reverse;
            read.reference = Input::getReference(std::string(*RNAME));
            try {
                extract(*CIGAR, read.cigar);
            }
            catch (std::ios_base::failure const &e) {
                throw ParseError::invalid_CIGAR;
            }
            read.position = position - 1;
        }
        else {
            read.type = Input::ReadType::biological;
            read.orientation = (flags & 0x080) != 0 ? Input::ReadOrientation::reverse : Input::ReadOrientation::forward;
        }
        result.reads.emplace_back(std::move(read));
        return result;
    }
    Input readFASTQ() {
        auto const start = lines;
        auto const defline = std::string(peek());
        auto seq = getline();
        auto nextline = peek();
        while (defline.front() != nextline.front() && nextline.front() != '+') {
            seq.append(nextline.data(), nextline.size());
            nextline = getline();
        }
        if (nextline.front() == '+') {
            auto qual = getline();
            while (qual.size() < seq.size()) {
                nextline = getline();
                qual.append(nextline.data(), nextline.size());
            }
            if (qual.size() != seq.size())
                std::cerr << "warning: length of quality != length of sequence in read starting at line " << start << ":\n" << defline << std::endl;
        }
        return Input{seq};
    }
    virtual Input get() {
        auto nrecs = 0;
        Input result;

#if RECORD_LIMIT
        if (records >= RECORD_LIMIT) {
            REPORT("Record limit reached.");
            throw std::ios_base::failure("record limit reached");
        }
#endif
        // Skip comment lines.
        // If first line, determine if it's a SAM header.
        // If it is, continue until done with header. (Header lines start with '@').
    READ_LINE_LOOP:
        for ( ; ; ) {
            auto const &line = getline();
            ++lines;
            for (auto ch : line) {
                if (isspace(ch))
                    continue;
                if (ch == '#') {
                    std::cerr << line << std::endl;
                    goto READ_LINE_LOOP;
                }
                break;
            }
            ++nrecs;

            // only applies to first record
            if (records > 0)
                break;

            // could it be SAM or FASTQ
            if (line[0] != '@')
                break;

            // could it be a SAM header
            if (nrecs == 1 && line.substr(0, 7) != "@HD\tVN:")
                break;

            // it's a SAM header line
            Input::SAM_HeaderLine(line);
        }
        auto const line = peek();
        ++records;

        if (line[0] == '@' || line[0] == '>') {
            result = readFASTQ();
        }
        else {
            auto const flds = Delimited(line, '\t');
            if (flds.part.size() >= 11) {
                // SAM has at least 11 fields, the other formats have at most 6 fields.
                result = readSAM(flds);
                goto DONE;
            }
            if (flds.part.size() == 1) {
                result = Input{std::string(flds.part[0]), {{0, int(flds.part[0].size())}}};
                goto DONE;
            }
            try {
                result = readUnaligned(flds);
                goto DONE;
            }
            catch (ParseError const &e) {
                ((void)e);
            }
            try {
                result = readAligned(flds);
                goto DONE;
            }
            catch (ParseError const &e) {
                ((void)e);
            }
            std::cerr << lines << ": warning: unparsable input\n" << line << std::endl;
        }
    DONE:
        return result;
    }
};

struct ThreadedSource : public Input::Source {
    BasicSource source;
    std::queue<Input *> que;
    std::mutex mut;
    std::condition_variable condEmpty, condFull;
    bool volatile done = false;
    int quemax = 16;
    std::thread th;

    ThreadedSource(Input::Source::Type const &src)
    : source(src)
    {
        th = std::thread(mainLoop, this);
    }
    ~ThreadedSource() {
        if (!done) {
            done = true;
            std::cerr << "Signaling reader thread to stop ..." << std::endl;
            condFull.notify_one();
        }
        try { th.join(); } catch (...) {}
    }
    virtual operator bool() const {
        return eof() ? source : true;
    }
    virtual bool eof() const {
        return done && que.empty();
    }
    virtual Input get() {
        Input const *p = nullptr;

        if (done) {
            // the reader thread is not running
            if (!que.empty()) {
                p = que.front();
                que.pop();
            }
        }
        else {
            std::unique_lock guard(mut);
            while (!done) {
                if (!que.empty()) {
                    p = que.front();
                    que.pop();
                    break;
                }
                condEmpty.wait(guard);
            }
        }
        condFull.notify_one();
        if (p) {
            Input result(std::move(*p));
            delete p;
            return result;
        }
        else
            throw std::ios_base::failure("end of file");
    }
    static void mainLoop(ThreadedSource *self) {
        std::cerr << "Reader thread is running ..." << std::endl;
        for ( ; ; ) {
            try {
                auto p = new Input(std::move(self->source.get()));
                {
                    std::unique_lock guard(self->mut);
                    if (self->quemax < self->que.size()) {
                        if (self->quemax > 0x10)
                            self->quemax *= 0.9;
                        do { self->condFull.wait(guard); } while (self->quemax < self->que.size());
                    }
                    else if (self->quemax < 0x10000)
                        ++self->quemax;
                    self->que.push(p);
                }
                self->condEmpty.notify_one();
            }
            catch (std::ios_base::failure const &e) {
                self->done = true;
                self->condEmpty.notify_one();
                if (self->source.eof())
                    std::cerr << "Reader thread is done." << std::endl;
                else
                    std::cerr << "Reader thread caught exception: " << e.what() << std::endl;
                break;
            }
        }
    }
};

static std::unique_ptr<Input::Source> newSource(Input::Source::Type const &src) {
    return std::make_unique<BasicSource>(src);
}

static std::unique_ptr<Input::Source> newThreadedSource(Input::Source::Type const &src) {
    return std::make_unique<ThreadedSource>(src);
}

struct StringSource : public BasicSource {
    StringSource(std::string const &str) : BasicSource(Source::StringLiteralType{str}) {}
};

std::unique_ptr<Input::Source> Input::newSource(Input::Source::Type const &src, bool multithreaded) {
    if (multithreaded)
        return ::newThreadedSource(src);
    else
        return ::newSource(src);
}

static void Delimited_test() {
    auto strm = std::string_view("sequence \t 123, 456\t\n");
    auto raw = Delimited(strm, '\t');

    assert(raw.part.size() == 3);
    assert(raw.part[0] == "sequence");
    assert(raw.part[1] == "123, 456");
    assert(raw.part[2].empty());

    auto raw2 = Delimited(raw.part[1], ',');

    assert(raw2.part.size() == 2);
    assert(raw2.part[0] == "123");
    assert(raw2.part[1] == "456");
}

#if NDEBUG
void Input::runTests() {
}
#else
static void Input_test1() {
    auto const &reset = Input::getReset();
    auto src = StringSource("GTGGACATCCCTCTGTGTGNGTCANNNNNNNNNNCCAGNNNNNNNGGNNCCTCCCGANGCCNNNCNNNNNGGCTTCTAGATGGCGNNNNNNCCGTGTGNCNTCAAGTGGTCAACCCTCTGNGNGNNTCAGTGTCCTAATCCANTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL, SRA_READ_TYPE_TECHNICAL\n");
    auto const &reads = src.get().reads;
    assert(reads.size() == 2);
    assert(reads[0].length == 142);
    assert(reads[1].length == 151);
    assert(reads[0].type == Input::ReadType::biological);
    assert(reads[1].type == Input::ReadType::technical);
}

static void Input_test1a() {
    auto const &reset = Input::getReset();
    auto src = StringSource("NTGGATTAGGACACTGACANNNNNNNNNNNANNTCCACTCGAGGACACACGGANNNNNCNNCATCTAGNNNNNNNGGAGAGAGGCCTCGNNNNNNNCCAGCACNNCNGNNNTNNNNNNNNNACNNNNNNNNNNNNNNNACCANTTNAGGAC\t142, 151\t0, 142\tSRA_READ_TYPE_BIOLOGICAL, SRA_READ_TYPE_TECHNICAL\t1, 0\n");
    auto const &spot = src.get();
    auto const &reads = spot.reads;
    assert(reads.size() == 2);
    assert(spot.sequence.size() == 142+151);
    assert(reads[0].type == Input::ReadType::aligned);
    assert(reads[1].type == Input::ReadType::technical);
}

static void Input_test2() {
    auto const &reset = Input::getReset();
    auto src = StringSource(R"(
# the previous line was empty and this line is a comment
GTGGACATCCCTCTGTGTGNGTCAN	CM_000001	10001	0	24M1S
)");
    auto const &reads = src.get().reads;
    assert(reads.size() == 1);
    assert(reads[0].position == 10001);
}

static void Input_test3() {
    auto const &reset = Input::getReset();
    auto src = StringSource(R"(@HD	VN:1	SO:none
SPOT_1	0	CM_000001	10001	100	24M1S	*	0	0	GTGGACATCCCTCTGTGTGNGTCAN	*	RG:Z:FOO
)");
    auto const &sam = src.get();
    auto const &reads = sam.reads;
    assert(Input::groups[sam.group] == "FOO");
    assert(reads.size() == 1);
    assert(reads[0].position == 10000);
    assert(Input::references[reads[0].reference] == "CM_000001");
}

void Input::runTests() {
    auto const &reset = Input::getReset();

    Delimited_test();
    Input_test2();
    Input_test1();
    Input_test1a();
    Input_test3();
}
#endif

