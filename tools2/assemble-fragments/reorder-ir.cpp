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
#include <array>
#include <map>
#include <string>
#include <stdexcept>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>
#include "utility.hpp"
#include "vdb.hpp"
#include "writer.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>

using namespace utility;

using HashFunc = HashFunction::FNV_1a;
using Hasher = HashFunc::Hasher;
static HashFunc const *hash = nullptr;

static inline Hasher &operator <<(Hasher &self, VDB::Cursor::RawData const &value) {
    self.add(value.elements, reinterpret_cast<uint8_t const *>(value.data));
    return self;
}

struct Index_Row {
    uint8_t key[8];
    VDB::Cursor::RowID row;
    
    uint64_t key64() const {
        union { uint8_t u8[4]; uint64_t u64; } u;
        std::copy(key, key + 8, u.u8);
        return u.u64;
    }
    static bool keyLess(Index_Row const &a, Index_Row const &b) {
        for (auto i = 0; i < 8; ++i) {
            if (a.key[i] < b.key[i]) return true;
            if (a.key[i] > b.key[i]) return false;
        }
        return false;
    }
    static bool rowLess(Index_Row const &a, Index_Row const &b) {
        return a.row < b.row;
    }
    
    Index_Row(VDB::Cursor::RowID row, VDB::Cursor::RawData const &group, VDB::Cursor::RawData const &name)
    : row(row)
    {
        auto constexpr RS = uint8_t(0x1e);
        auto constexpr US = uint8_t(0x1f);
        auto hasher = hash->hasher();
        hasher << group << US << name << RS;
        hasher >> key;
    }
};

struct RawRecord : public VDB::IndexedCursorBase::Record {
    struct IndexT : public Index_Row {
        VDB::Cursor::RowID row() const { return Index_Row::row; }
        bool operator ==(IndexT const &other) const { return key64() == other.key64(); }
        
        IndexT(VDB::Cursor::RowID row, VDB::Cursor::RawData const &group, VDB::Cursor::RawData const &name)
        : Index_Row(row, group, name)
        {}
    };
    typedef IndexT const *IndexIteratorT;
    
    RawRecord(VDB::IndexedCursorBase::Record const &base) : VDB::IndexedCursorBase::Record(base) {}
    bool write(std::vector<Writer2::Column> const &out) const {
        auto data = this->data;
        for (auto && i : out) {
            if (!i.setValue(data))
                return false;
            data = data->next();
        }
        return true;
    }
    friend bool operator <(RawRecord const &a, RawRecord const &b) {
        auto const agroup = a.data->string();
        auto const bgroup = b.data->string();
        auto const aname = a.data->next()->string();
        auto const bname = b.data->next()->string();
        
        if (agroup == bgroup && aname == bname) ///< this is the expected case
            return a.row < b.row;
        if (agroup < bgroup)
            return true;
        if (bgroup < agroup)
            return false;
        if (aname < bname)
            return true;
        if (bname < aname)
            return false;
        return a.row < b.row;
    }
    static VDB::Cursor open(VDB::Database const &inDb, Writer2 &writer) {
        static char const *columns[] = {
            "READ_GROUP",
            "NAME",
            "READNO",
            "SEQUENCE",
            "REFERENCE",
            "CIGAR",
            "STRAND",
            "POSITION",
            "QUALITY"
        };
        auto const N = sizeof(columns) / sizeof(columns[0]);
        unsigned which = 0;
        auto const result = inDb["RAW"].read(which, N, columns, N - 1, columns);
        switch (which) {
            case 1:
                writer.addTable("RAW", {
                    { "READ_GROUP"  , 1 },  ///< string
                    { "NAME"        , 1 },  ///< string
                    { "READNO"      , 4 },  ///< int32_t
                    { "SEQUENCE"    , 1 },  ///< string
                    { "REFERENCE"   , 1 },  ///< string
                    { "CIGAR"       , 1 },  ///< string
                    { "STRAND"      , 1 },  ///< char
                    { "POSITION"    , 4 },  ///< int32_t
                    { "QUALITY"     , 1 },  ///< string
                });
                break;
            case 2:
                writer.addTable("RAW", {
                    { "READ_GROUP"  , 1 },  ///< string
                    { "NAME"        , 1 },  ///< string
                    { "READNO"      , 4 },  ///< int32_t
                    { "SEQUENCE"    , 1 },  ///< string
                    { "REFERENCE"   , 1 },  ///< string
                    { "CIGAR"       , 1 },  ///< string
                    { "STRAND"      , 1 },  ///< char
                    { "POSITION"    , 4 },  ///< int32_t
                });
                break;
            default:
                assert(!"reachable");
                break;
        }
        return result;
    }
};

using IndexRow = RawRecord::IndexT;

struct WorkUnit {
    IndexRow *beg;
    IndexRow *end;
    IndexRow *out;
    int level;
    
    WorkUnit() : beg(0), end(0), out(0), level(-1) {}
    WorkUnit(IndexRow *const beg, IndexRow *const end, IndexRow *const scratch, int const level)
    : beg(beg)
    , end(end)
    , out(scratch)
    , level(level)
    {}
    size_t size() const { return end - beg; }
    
    void process(std::vector<WorkUnit> &result) const
    {
        static int const BPL[] = { 1, 3, 4, 8, 8, 8, 8, 8, 8, 8 };
        static int const KPL[] = { 0, 0, 0, 1, 2, 3, 4, 5, 6, 7 };
        static int const SPL[] = { 7, 4, 0, 0, 0, 0, 0, 0, 0, 0 };

        assert(level < sizeof(BPL)/sizeof(BPL[0]));

        auto const bins = 1 << BPL[level];
        auto const m = bins - 1;
        auto const k = KPL[level];
        auto const s = SPL[level];
        size_t start[256];
        IndexRow *cur = out;
        
        for (auto bin = 0; bin < bins; ++bin) {
            for (auto i = beg; i != end; ++i) {
                auto const key = (i->key[k] >> s) & m;
                if (key == bin)
                    *cur++ = *i;
            }
            start[bin] = cur - out;
        }
        
        for (auto bin = 0; bin < bins; ++bin) {
            auto const begin = (bin > 0 ? start[bin - 1] : 0);
            auto const count = start[bin] - begin;
            
            if (count > 0) {
                assert(level + 1 < sizeof(BPL)/sizeof(BPL[0]));
                result.push_back(WorkUnit(out + begin, out + begin + count, beg + begin, level + 1));
            }
        }
    }
};

struct Context {
    IndexRow *const src;
    IndexRow *const srcEnd;
    IndexRow *const out;
    IndexRow *const outEnd;
    size_t const smallSize; // chunk size above which more work units may be produced, else the sort is done in one shot
    
    pthread_mutex_t mutex; // protects the entire structure against mutation by other threads
    pthread_cond_t cond_running;
    unsigned running; // count of number of work units being processed, work units which might produce more work units; this is to prevent workers from quiting early, when the queue is empty but might not stay empty
    unsigned next; // next work unit to be processed; the queue is considered empty when next == queue.size()
    std::vector<WorkUnit> queue; // the queue is only ever appended to
    
    Context(IndexRow *Src, IndexRow *Out, size_t count, size_t smallSize)
    : src(Src)
    , srcEnd(src + count)
    , out(Out)
    , outEnd(out + count)
    , next(0)
    , running(0)
    , smallSize(smallSize)
    , mutex(PTHREAD_MUTEX_INITIALIZER)
    , cond_running(PTHREAD_COND_INITIALIZER)
    {
        queue.push_back(WorkUnit(src, srcEnd, out, 0));
    }
    
    void run(void) {
        auto newWork = std::vector<WorkUnit>();
        
        newWork.reserve(256);
        
        pthread_mutex_lock(&mutex);
        for ( ;; ) {
            if (next < queue.size()) {
                auto const unit = queue[next++];
                ++running;
                
                // the mutex is released before processing the work unit
                pthread_mutex_unlock(&mutex);
                {
                    newWork.clear();
                    if (unit.size() <= smallSize) {
                        // sort in one shot
                        std::sort(unit.beg, unit.end, IndexRow::keyLess);
                        if (unit.beg >= src && unit.end <= srcEnd)
                            std::copy(unit.beg, unit.end, out + (unit.beg - src));
                    }
                    else {
                        // partial sort, can generate more work units
                        unit.process(newWork);
                    }
                }
                // the mutex is re-acquired after processing the work unit
                pthread_mutex_lock(&mutex);
                std::copy(newWork.begin(), newWork.end(), std::back_inserter(queue));

                --running;
                pthread_cond_signal(&cond_running);
            }
            else if (running > 0) {
                pthread_cond_wait(&cond_running, &mutex);
            }
            else
                break;
            // it is an invariant that the mutex is held by the current thread regardless of the code path taken
        }
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&cond_running);
    }
};

static void *worker(void *p)
{
    static_cast<Context *>(p)->run();
    return nullptr;
}

#if __APPLE__
#include <sys/sysctl.h>

/* want one worker thread per physical core
 * could go with one per logical but that would just make bus contention worse
 * the bottleneck is I/O to memory
 */
static int getWorkerCount()
{
    size_t len;
    
    auto physCPU = int32_t(0);
    len = sizeof(physCPU);
    if (sysctlbyname("hw.physicalcpu", &physCPU, &len, 0, 0) == 0 && physCPU > 0) {
        return physCPU;
    }
    return 1;
}

/* This tries to take into account that different caches are shared amongst
 * different numbers of cores. It picks based on the largest cache per core.
 *
 * The idea is that, once a workunit fits entirely into cache, it's not
 * productive to break it down into smaller workunits. Instead, sort it
 * completely, in-place, on one thread.
 */
static size_t getSmallSize(int const workers)
{
    size_t len;
    /* these two are layed out as
     * [0]: RAM; [1]: L1 cache; [2]: L2 cache; etc.
     * an entry is 0 if there is no cache at that level
     */
    uint64_t cacheSize[16] = {0};
    // gives the number of logical cores which share a cache level
    uint64_t cacheSharing[16] = {0};
    
    len = sizeof(cacheSharing);
    sysctlbyname("hw.cacheconfig", cacheSharing, &len, 0, 0);
    sysctlbyname("hw.cachesize", cacheSize, &len, 0, 0);
    
    auto cache = size_t(0);
    auto const N = len / sizeof(cacheSize[0]);
    for (auto i = N < 4 ? N : 4; i; ) {
        auto j = --i;
        if (j < 2)
            break;
        if (cacheSize[j] == 0 || cacheSharing[j] == 0)
            continue;
        auto const cache1 = cacheSize[j] / cacheSharing[j];
        if (cache < cache1)
            cache = cache1;
    }
    cache /= sizeof(IndexRow);
    return (cache < 32 * 1024) ? (32 * 1024) : cache;
}
#else
static int getWorkerCount()
{
    return 2;
}
static size_t getSmallSize(int const workers)
{
    return (64 * 1024) / workers;
}
#endif

static void sortIndex(utility::Array<IndexRow> &index)
{
    auto const N = index.elements();
    auto scratch = utility::Array<IndexRow>(index.elements());
    if (!scratch) {
        perror("error: insufficient memory to create temporary index");
        exit(1);
    }
    {
        auto const workers = getWorkerCount();
        auto const smallSize = getSmallSize(workers);
        auto context = Context(index.base(), scratch.base(), N, smallSize);
        
        for (auto i = 1; i < workers; ++i) {
            pthread_t tid = 0;
            
            pthread_create(&tid, nullptr, worker, &context);
            pthread_detach(tid);
        }
        worker(&context);
    }
    auto const keys = scratch.reduce(std::make_pair(uint64_t(0), uint64_t(0)), [](std::pair<uint64_t, uint64_t> const &v, IndexRow const &i)
                                     {
                                         return (v.first > 0 && v.second == i.key64()) ? v : std::make_pair(v.first + 1, i.key64());
                                     }).first;
    
    auto tmp = utility::Array<IndexRow *>(keys);
    if (!tmp) {
        perror("error: insufficient memory to create temporary index");
        exit(1);
    }
    {
        auto last = scratch[0].key64();
        uint64_t j = 0;
        tmp[j++] = scratch.begin();
        for (auto i = uint64_t(0); i < N; ++i) {
            auto const k = &scratch[i];
            auto const key = k->key64();
            if (last == key) continue;
            last = key;
            tmp[j++] = k;
        }
        assert(j == keys);
    }
    tmp.sort([](IndexRow const *const a, IndexRow const *const b) { return a->row() < b->row(); });
    {
        uint64_t j = 0;
        for (auto i = uint64_t(0); i < keys; ++i) {
            auto ii = tmp[i];
            assert(scratch.contains(ii));
            auto const key = ii->key64();
            do { index[j++] = *ii++; } while (j < N && ii->key64() == key);
        }
    }
    std::cerr << "info: Number of keys " << keys << std::endl;
}

static utility::Array<IndexRow> makeIndex(VDB::Database const &run)
{
    static char const *const FLDS[] = { "READ_GROUP", "NAME" };
    auto const in = run["RAW"].read(2, FLDS);
    auto const range = in.rowRange();
    auto const N = size_t(range.count());
    auto result = utility::Array<IndexRow>(N);
    if (result) {
        auto const index = result.base();
        auto const freq = N / 10.0;
        auto nextReport = 1;
        
        in.foreach([&](VDB::Cursor::RowID row, std::vector<VDB::Cursor::RawData> const &data) {
            auto const i = row - range.beg();
            {
                auto const p = new(index + i) IndexRow(row, data[0], data[1]);
                (void)*p;
            }
            while (nextReport * freq <= i) {
                std::cerr << "prog: generating keys " << nextReport << "0%" << std::endl;;
                ++nextReport;
            }
        });
        std::cerr << "prog: processed " << N << " records" << std::endl;
        std::cerr << "prog: indexing" << std::endl;
        
        sortIndex(result);
    }
    else if (N) {
        perror("error: insufficient memory to create temporary index");
        exit(1);
    }
    return result;
}

#define VALIDATE 0
#if VALIDATE
#include <set>
#endif

static void validate(RawRecord const &r)
{
#if VALIDATE
    struct data : public std::pair<std::string, std::string>
    {
        data() {}
        data(RawRecord const &r) {
            first = r.data->string();
            second = r.data->next()->string();
        }
    };
    static std::set< data > s;
    static data lastOne;

    auto thisOne = data(r);
    if (s.empty() || lastOne != thisOne) {
        auto const inserted = s.insert(thisOne);
        assert(inserted.second == true);
        lastOne = thisOne;
    }
#endif
}

static int process(Writer2 const &out, VDB::Cursor const &in, utility::Array<RawRecord::IndexT> const &index, bool const quality)
{
    auto const otbl = out.table("RAW");
    auto const columns = otbl.columns();
    auto const range = in.rowRange();
    if (index.elements() != range.count()) {
        std::cerr << "error: index size doesn't match input table" << std::endl;
        return -1;
    }
    auto const freq = (range.count()) / 10.0;
    auto nextReport = 1;
    uint64_t written = 0;

    std::cerr << "info: processing " << (range.count()) << " records" << std::endl;
    
    auto const indexedCursor = VDB::IndexedCursor<RawRecord>(in, index.begin(), index.end());
    auto const rows = indexedCursor.foreach([&](RawRecord const &a) {
        validate(a);
        a.write(columns);
        otbl.closeRow();
        ++written;
        while (nextReport * freq <= written) {
            std::cerr << "prog: writing " << nextReport << "0%" << std::endl;
            ++nextReport;
        }
    });
    assert(rows == written);
    assert(rows == range.count());
    assert(rows == index.elements());
    return 0;
}

static int process(std::string const &irdb, FILE *out)
{
    auto const mgr = VDB::Manager();
    auto const inDb = mgr[irdb];

    std::cerr << "prog: creating clustering index" << std::endl;
    auto index = makeIndex(inDb);

    auto writer = Writer2(out);
    int result;
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("reorder-ir", "1.0.0");
    
    std::cerr << "prog: rewriting rows in clustered order" << std::endl;

    auto const in = RawRecord::open(inDb, writer);
    writer.beginWriting();
    result = process(writer, in, index, true);
    writer.endWriting();

    std::cerr << "prog: done" << std::endl;
    return result;
}

namespace reorderIR {
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout)
        << "usage: " << commandLine.program[0] << " [-stable] [-out=<path>] <ir db>"
        << std::endl;
        exit(error ? 3 : 0);
    }
    
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine, false);
            }
        }

        auto db = std::string();
        auto out = std::string();
        auto stable = false;

        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 7) == "-stable") {
                stable = true;
                continue;
            }
            if (arg.substr(0, 5) == "-out=") {
                out = arg.substr(5);
                continue;
            }
            if (db.empty()) {
                db = arg;
                continue;
            }
            usage(commandLine, true);
        }
        auto const hash_p = std::unique_ptr<HashFunc>(new HashFunc(!stable));
        hash = hash_p.get();
        
        if (db.empty())
            usage(commandLine, true);
        
        if (out.empty())
            return process(db, stdout);
        
        auto ofs = fopen(out.c_str(), "w");
        if (ofs)
            return process(db, ofs);
        
        std::cerr << "failed to open output file: " << out << std::endl;
        exit(3);
    }
}

int main(int argc, char *argv[])
{
    return reorderIR::main(CommandLine(argc, argv));
}
