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
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>
#include "utility.hpp"
#include "vdb.hpp"
#include "writer.hpp"
#include "fragment.hpp"

using namespace utility;

static strings_map references;
static strings_map groups = {""};

template <typename T>
static bool string_to_i(T &result, char const *const beg, char const *const end, int radix = 0)
{
    char *endp = 0;
    auto const temp = strtol(beg, &endp, radix);
    result = T(temp);
    return temp == decltype(temp)(result) && endp == end;
}

template <typename T>
static bool string_to_u(T &result, char const *const beg, char const *const end, int radix = 0)
{
    char *endp = 0;
    auto const temp = strtoul(beg, &endp, radix);
    result = T(temp);
    return temp == decltype(temp)(result) && endp == end;
}

namespace POSIX {
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
}

#define USE_MMAP 1

struct LineBuffer {
    int fd;
    void *buffer;
    size_t cur;
    size_t size;
    size_t maxSize;
    blksize_t blksize;
    
    LineBuffer(int fd) : fd(fd) {
        struct POSIX::stat st;
        
        blksize = 4 * 1024; ///< our default value
        if (POSIX::fstat(fd, &st) == 0) {
#if USE_MMAP
            maxSize = st.st_size;
            buffer = POSIX::mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
            if (buffer != MAP_FAILED) {
                cur = 0;
                size = maxSize;
                this->fd = -1;
                POSIX::close(fd);
                return;
            }
#endif
            blksize = st.st_blksize;
        }
        maxSize = blksize * 2;
        buffer = malloc(maxSize);
        if (buffer) {
            cur = size = 0;
            return;
        }
        throw std::bad_alloc();
    }
    ~LineBuffer() {
        if (fd < 0) {
            POSIX::munmap(buffer, maxSize);
        }
        else {
            free(buffer);
            POSIX::close(fd);
        }
    }
    double position() const {
        return fd < 0 ? double(cur) / maxSize : -1.0;
    }
    std::pair<char const *, char const *> get() {
        if (fd < 0) {
            if (cur >= maxSize) return {nullptr, nullptr};
            auto const start = reinterpret_cast<char const *>(buffer) + cur;
            auto endp = start;
            while (cur < maxSize) {
                ++cur;
                if (*endp == '\n')
                    return {start, endp};
                ++endp;
            }
            return {start, endp};
        }
        if (maxSize == 0) return {nullptr, nullptr};
        if (cur > blksize) {
            auto start = reinterpret_cast<char const *>(buffer);
            auto const endp = start + size;
            auto const remain = size - cur;
            auto newCur = cur % blksize;
            start += cur;
            start -= newCur;
            std::copy(start, endp, reinterpret_cast<char *>(buffer));
            cur = newCur;
            size = cur + remain;
        }
        auto const beg = cur;
        for ( ; ; ) {
            auto const base = reinterpret_cast<char const *>(buffer);
            if (cur < maxSize) {
                if (cur == size) {
                    auto const nread = POSIX::read(fd, (void *)(base + size), maxSize - size);
                    if (nread <= 0) {
                        maxSize = 0;
                        return {nullptr, nullptr};
                    }
                    size += nread;
                }
                while (cur < size) {
                    auto const at = cur++;
                    if (base[at] == '\n')
                        return {base + beg, base + at};
                }
            }
            else {
                auto const temp = realloc(buffer, maxSize * 2);
                if (temp == nullptr)
                    throw std::bad_alloc();
                buffer = temp;
                maxSize *= 2;
            }
        }
    }
};

struct ContigPair { ///< a pair of contigs that are *known* to be joined, e.g. the two reads of a paired-end fragment
    struct Contig { ///< a contig is nothing more than a contiguous region on some reference; a read is a contig by this definition
        unsigned ref;
        int start;
        int end;
        
        Contig() {}

        Contig(Alignment const &algn, CIGAR const &cigar)
        : ref(unsigned(references[algn.reference]))
        , start(algn.position - cigar.qfirst)
        , end(algn.position + cigar.rlength + cigar.qclip)
        {
            assert(start < end);
        }
        
        int length() const { return end - start; }
        bool operator ==(Contig const &other) const {
            return ref == other.ref && start == other.start && end == other.end;
        }
        
        friend Contig operator +(Contig a, Contig b) ///< returns the union
        {
            Contig result;
            result.ref = a.ref;
            result.start = std::min(a.start, b.start);
            result.end = std::max(a.end, b.end);
            return result;
        }
    };
    Contig first, second;
    unsigned group;
    unsigned count;

    bool operator ==(ContigPair const &other) const {
        return first == other.first && second == other.second && group == other.group;
    }
    
    ContigPair() {}
    
    friend ContigPair operator +(ContigPair a, ContigPair b) ///< create the union of the two pairs; it is assumed that they intersect
    {
        assert(a.group == b.group);
        
        ContigPair result;
        result.first = a.first + b.first;
        result.second = a.second + b.second;
        result.count = a.count + b.count;
        result.group = a.group;
        return result;
    }
    
    ContigPair(Alignment const &one, Alignment const &two, std::string const &group)
    : group(unsigned(groups[group]))
    {
        auto const &c1 = Contig(one, one.cigar);
        auto const &c2 = Contig(two, two.cigar);
        
        if (two.reference < one.reference || (c2.ref == c1.ref && (c2.start < c1.start || (c2.start == c1.start && c2.end < c1.end)))) {
            first = c2;
            second = c1;
        }
        else {
            first = c1;
            second = c2;
        }
        count = 1;
    }
    
    explicit ContigPair(LineBuffer &source)
    : count(0)
    {
        auto const line = source.get();
        if (line.first == nullptr) return;

        auto n = 0;
        for (auto i = line.first, end = i; ; ++end) {
            if (end == line.second || *end == '\t') {
                switch (n) {
                    case 0:
                        first.ref = unsigned(references[std::string(i, end)]);
                        break;
                    case 1:
                        if (!string_to_i(first.start, i, end)) goto CONVERSION_ERROR;
                        break;
                    case 2:
                        if (!string_to_i(first.end, i, end)) goto CONVERSION_ERROR;
                        break;
                    case 3:
                        second.ref = unsigned(references[std::string(i, end)]);
                        break;
                    case 4:
                        if (!string_to_i(second.start, i, end)) goto CONVERSION_ERROR;
                        break;
                    case 5:
                        if (!string_to_i(second.end, i, end)) goto CONVERSION_ERROR;
                        break;
                    case 6:
                        group = unsigned(groups[std::string(i, end)]);
                        break;
                    case 7:
                        std::cerr << "extra data in record: " << std::string(line.first, line.second) << std::endl;
                        return;
                }
                ++n;
                if (end == line.second)
                    break;
                i = end + 1;
            }
        }
        if (n < 6) {
            std::cerr << "truncated record: " << std::string(line.first, line.second) << std::endl;
            return;
        }
        if (n < 7)
            group = groups[""];

        count = 1;
        return;
        
    CONVERSION_ERROR:
        std::cerr << "error parsing record: " << std::string(line.first, line.second) << std::endl;
        return;
    }
    
    bool write(FILE *fp) const {
        auto const &ref1 = references[first.ref];
        auto const &ref2 = references[second.ref];
        auto const &grp = groups[group];
        return fprintf(fp, "%s\t%i\t%i\t%s\t%i\t%i\t%s\n", ref1.c_str(), first.start, first.end, ref2.c_str(), second.start, second.end, grp.c_str()) > 0;
    }
    friend std::ostream &operator <<(std::ostream &strm, ContigPair const &i) {
        auto const &ref1 = references[i.first.ref];
        auto const &ref2 = references[i.second.ref];
        auto const &grp = groups[i.group];
        strm
             << ref1 << '\t' << i.first.start << '\t' << i.first.end << '\t'
             << ref2 << '\t' << i.second.start << '\t' << i.second.end << '\t'
             << grp;
        return strm;
    }
    void write(VDB::Writer const &out) const {
        out.value(1, references[first.ref]);
        out.value(2, (int32_t)first.start);
        out.value(3, (int32_t)first.end);

        out.value(4, references[second.ref]);
        out.value(5, (int32_t)second.start);
        out.value(6, (int32_t)second.end);
        
        out.value(7, groups[group]);

        out.value(8, (uint32_t)count);

        out.closeRow(1);
    }
    static void setup(VDB::Writer const &writer) {
        writer.openTable(1, "CONTIGS");

        writer.openColumn(1, 1,  8, "REFERENCE_1");
        writer.openColumn(2, 1, 32, "START_1");
        writer.openColumn(3, 1, 32, "END_1");
        
        writer.openColumn(4, 1,  8, "REFERENCE_2");
        writer.openColumn(5, 1, 32, "START_2");
        writer.openColumn(6, 1, 32, "END_2");

        writer.openColumn(7, 1,  8, "READ_GROUP");

        writer.openColumn(8, 1, 32, "COUNT");
    }
};

static int process(VDB::Writer const &out, LineBuffer &ifs)
{
    auto active = std::vector<ContigPair>();
    
    auto ref = decltype(active.front().first.ref)(0); ///< the active reference (first read)
    auto end = decltype(active.front().first.end)(0); ///< the largest ending position (first read) seen so far; the is the end of the active window

    unsigned long long in_count = 0;
    unsigned long long out_count = 0;
    unsigned long long gapless_count = 0;
    auto time0 = time(nullptr);
    auto freq = 0.1;
    auto report = freq;

    for ( ; ; ) {
        auto pair = ContigPair(ifs);
        auto const isEOF = pair.count == 0;
        
        if ((!active.empty() && (pair.first.ref != ref || pair.first.start >= end)) || isEOF) {
            // new pair is outside the active window (or EOF);
            // output the active contig pairs and empty the window
            
            for (auto && i : active) {
                if (i.first.ref == i.second.ref && i.second.start < i.first.end) {
                    // the region is gapless, i.e. the mate-pair gap has been filled in
                    i.first.end = i.second.start = 0;
                }
            }
            for (auto i = decltype(active.size())(0); i < active.size(); ++i) {
                if (active[i].first.end != 0 || active[i].second.end != 0) continue;
                // active[i] is gapless

                auto const group = active[i].group;
                auto start = active[i].first.start;
                auto end = active[i].second.end;
            AGAIN:
                for (auto j = decltype(i)(0); j < active.size(); ++j) {
                    if (j == i) continue;
                    auto const &J = active[j];
                    if (J.group != group || J.second.ref != ref || J.first.start >= end || J.second.end <= start) continue;
                    
                    // active[j] overlaps active[i]
                    if ((J.first.end == 0 && J.second.start == 0) ///< active[j] is also gapless
                        || (start < J.first.end && J.second.start < end)) ///< or active[i] covers active[j]'s gap
                    {
                        start = std::min(start, J.first.start);
                        end = std::max(end, J.second.end);
                        active[i].first.start = start;
                        active[i].second.end = end;
                        active[i].count += J.count;
                        if (j < i)
                            --i;
                        active.erase(active.begin() + j);
                        goto AGAIN;
                    }
                }
            }
            std::sort(active.begin(), active.end(), ///< want order to be canonical; should be mostly in-order already
                      [](ContigPair const &a, ContigPair const &b) {
                          if (a.first.start < b.first.start) return true;
                          if (a.first.start > b.first.start) return false;
                          if (a.first.end == 0 && a.second.start == 0) {
                              if (b.first.end == 0 && b.second.start == 0) {
                                  if (a.second.end < b.second.end) return false; ///< longer one goes first
                                  if (a.second.end > b.second.end) return true;
                              }
                              else if (a.second.ref == b.second.ref) {
                                  return true; ///< gapless one goes first
                              }
                              else {
                                  return a.second.ref < b.second.ref;
                              }
                          }
                          else if (b.first.end == 0 && b.second.start == 0) {
                              if (a.second.ref == b.second.ref) {
                                  return false; ///< gapless one goes first
                              }
                              else {
                                  return a.second.ref < b.second.ref;
                              }
                          }
                          else {
                              // both have a gap
                              if (a.first.end < b.first.end) return true;
                              if (a.first.end > b.first.end) return false;
                              if (a.second.ref < b.second.ref) return true;
                              if (a.second.ref > b.second.ref) return false;
                              if (a.second.start < b.second.start) return true;
                              if (a.second.start > b.second.start) return false;
                              if (a.second.end < b.second.end) return true;
                              if (a.second.end > b.second.end) return false;
                          }
                          return a.group < b.group;
                      });
            for (auto && i : active) {
                if (i.second.start == 0 && i.first.end == 0)
                    ++gapless_count;
                i.write(out);
                ++out_count;
            }
            active.clear();
            if (isEOF) goto REPORT;
        }
        if (active.empty()) {
            ref = pair.first.ref;
            end = pair.first.end;
            active.emplace_back(pair);
        }
        else {
            for ( ; ; ) {
                unsigned maxOverlap = 0;
                auto merge = active.size(); ///< index of an existing contig pair into which the new pair should be merged
                
                for (auto i = active.size(); i != 0; ) {  ///< the best overlap is probably near the end of the list, so start at the back
                    --i;                            ///< and loop backwards
                    auto const &j = active[i];
                    if (j.group == pair.group && j.second.ref == pair.second.ref) {
                        if (j == pair) {
                            /// found an exact match, and since the list is unique, we're done
                            merge = i;
                            break;
                        }
                        
                        auto const start1 = std::max(pair.first.start, j.first.start);
                        auto const start2 = std::max(pair.second.start, j.second.start);
                        auto const end1 = std::min(pair.first.end, j.first.end);
                        auto const end2 = std::min(pair.second.end, j.second.end);
                        
                        /// the regions of overlap are [start1 - end1), [start2 - end2)
                        /// if either are empty (start >= end) then we aren't interested in the contig pair
                        if (start1 < end1 && start2 < end2) {
                            unsigned const overlap = (end1 - start1) + (end2 - start2);
                            if (maxOverlap < overlap) {
                                maxOverlap = overlap;
                                merge = i;
                            }
                        }
                    }
                }
                if (merge == active.size()) {
                    end = std::max(end, pair.first.end);
                    active.emplace_back(pair);
                    break;
                }
                auto const mergedPair = active[merge] + pair;
                if (active[merge] == mergedPair) {
                    active[merge] = mergedPair;
                    break;
                }
                pair = mergedPair;
                active.erase(active.begin() + merge);
            }
        }

        ++in_count;
        if (ifs.position() >= report) {
            report += freq;
        REPORT:
            auto elapsed = double(time(nullptr) - time0);
            if (elapsed > 0)
                std::cerr << "prog: " << unsigned(ifs.position() * 100.0) << "%; " << in_count << " alignments processed (" << in_count / elapsed << " per sec); (" << gapless_count << " gapless) " << out_count << " contig pairs generated (" << out_count / elapsed << " per sec); ratio: " << double(in_count) / out_count << std::endl;
            else
                std::cerr << "prog: " << unsigned(ifs.position() * 100.0) << "%; " << in_count << " alignments processed; (" << gapless_count << " gapless) " << out_count << " contig pairs generated; ratio: " << double(in_count) / out_count << std::endl;
            if (isEOF)
                return 0;
        }
    }
}

static int reduce(FILE *out, std::string const &source)
{
    int fd = 0;
    if (source != "-") {
        fd = POSIX::open(source.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cerr << "failed to open pairs file: " << source << std::endl;
            exit(3);
        }
    }
    LineBuffer in(fd);
    auto const writer = VDB::Writer(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("summarize-pairs", "1.0.0");
    
    ContigPair::setup(writer);

    writer.beginWriting();
    auto const result = process(writer, in);
    writer.endWriting();
    
    return result;
}

static int map(FILE *out, std::string const &run)
{
    auto const mgr = VDB::Manager();
    auto const inDb = mgr[run];
    auto const in = Fragment::Cursor(inDb["RAW"]);
    auto const range = in.rowRange();
    
    for (auto row = range.first; row < range.second; ) {
        auto const fragment = in.read(row, range.second);
        
        for (auto && one : fragment.detail) {
            if (one.readNo != 1 || !one.aligned) continue;

            for (auto && two : fragment.detail) {
                if (two.readNo != 2 || !two.aligned) continue;
                
                ContigPair(one, two, fragment.group).write(out);
            }
        }
    }
    return 0;
}

namespace pairsStatistics {
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << commandLine.program[0] << " [-out=<path>] (map <sra run> | reduce <pairs>)" << std::endl;
        exit(error ? 3 : 0);
    }
    
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine, false);
            }
        }
        auto outPath = std::string();
        auto source = std::string();
        auto verb = decltype(&reduce)(nullptr);
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                outPath = arg.substr(5);
                continue;
            }
            if (verb == nullptr) {
                if (arg == "map")
                    verb = &map;
                else if (arg == "reduce")
                    verb = &reduce;
                else
                    usage(commandLine, true);
                continue;
            }
            if (source.empty()) {
                source = arg;
                continue;
            }
            usage(commandLine, true);
        }
        
        if (source.empty())
            usage(commandLine, true);
        
        FILE *ofs = nullptr;
        if (!outPath.empty()) {
            ofs = fopen(outPath.c_str(), "w");
            if (!ofs) {
                std::cerr << "failed to open output file: " << outPath << std::endl;
                exit(3);
            }
        }
        auto rslt = (*verb)(ofs ? ofs : stdout, source);
        if (ofs) fclose(ofs);
        return rslt;
    }
}

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    
    return pairsStatistics::main(CommandLine(argc, argv));
}
