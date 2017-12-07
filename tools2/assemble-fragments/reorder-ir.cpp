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

static uint8_t SBox[256];

static unsigned random_uniform(int upper_bound) {
#if __APPLE__ && 0
    return arc4random_uniform(upper_bound);
#else
    static double *next = NULL;
    static double entropy[256];
    
    if (next == NULL) {
        FILE *fp;
        
        if ((fp = fopen("/dev/urandom", "r")) != NULL || (fp = fopen("/dev/random", "r")) != NULL) {
            uint32_t tmp[256];
            fread(&tmp, 4, 256, fp);
            for (auto i = 0; i < 256; ++i) {
                entropy[i] = double(tmp[i]) / exp2(32);
            }
            fclose(fp);
        }
        else {
            srand(unsigned(time(0)));
            for (auto i = 0; i < 256; ++i)
                entropy[i] = double(rand()) / RAND_MAX;
        }
        next = entropy;
    }
    return unsigned(floor(*next++ * upper_bound));
#endif
}

static void unrandomizeSBox(void) {
    for (auto i = 0; i < 256; ++i) {
        SBox[i] = i;
    }
}

static void randomizeSBox(void) {
    SBox[0] = 0;
    for (auto i = 1; i < 256; ++i) {
        auto const j = random_uniform(i + 1);
        SBox[i] = SBox[j];
        SBox[j] = i;
    }
}

struct IndexRow {
    uint8_t key[8];
    VDB::Cursor::RowID row;
    
    uint64_t key64() const {
        return *reinterpret_cast<uint64_t const *>(&key[0]);
    }
    static bool keyLess(IndexRow const &a, IndexRow const &b) {
        for (auto i = 0; i < 8; ++i) {
            if (a.key[i] < b.key[i]) return true;
            if (a.key[i] > b.key[i]) return false;
        }
        return false;
    }
    static bool rowLess(IndexRow const &a, IndexRow const &b) {
        return a.row < b.row;
    }
};

static IndexRow makeIndexRow(VDB::Cursor::RowID row, VDB::Cursor::RawData const &group, VDB::Cursor::RawData const &name)
{
    IndexRow y;
    union {
        uint8_t u8[8];
        uint64_t u64;
    } h;
    
    h.u64 = 0xcbf29ce484222325ull;
    for (auto i = 0; i < group.elements; ++i) {
        auto const ch = reinterpret_cast<uint8_t const *>(group.data)[i];
        h.u64 = (h.u64 ^ SBox[ch]) * 0x100000001b3ull;
    }
    for (auto i = 0; i < name.elements; ++i) {
        auto const ch = reinterpret_cast<uint8_t const *>(name.data)[i];
        h.u64 = (h.u64 ^ SBox[ch]) * 0x100000001b3ull;
    }
    std::copy(h.u8, h.u8 + 8, y.key);
    y.row = row;
    return y;
}

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

static void sortIndex(uint64_t const N, IndexRow *const index)
{
    auto const scratch = reinterpret_cast<IndexRow *>(malloc(N * sizeof(IndexRow)));
    if (scratch == NULL) {
        perror("error: insufficient memory to create temporary index");
        exit(1);
    }
    {
        auto const workers = getWorkerCount();
        auto const smallSize = getSmallSize(workers);
        auto context = Context(index, scratch, N, smallSize);
        
        for (auto i = 1; i < workers; ++i) {
            pthread_t tid = 0;
            
            pthread_create(&tid, nullptr, worker, &context);
            pthread_detach(tid);
        }
        worker(&context);
    }
    uint64_t keys = 1;
    {
        auto last = scratch->key64();
        for (auto i = uint64_t(1); i < N; ++i) {
            auto const key = scratch[i].key64();
            if (last == key) continue;
            last = key;
            ++keys;
        }
    }
    
    auto const tmp = reinterpret_cast<IndexRow **>(malloc(keys * sizeof(IndexRow *)));
    if (tmp == NULL) {
        perror("error: insufficient memory to create temporary index");
        exit(1);
    }
    {
        auto last = scratch->key64();
        uint64_t j = 0;
        tmp[j++] = scratch;
        for (auto i = uint64_t(0); i < N; ++i) {
            auto const key = scratch[i].key64();
            if (last == key) continue;
            last = key;
            tmp[j++] = scratch + i;
        }
        assert(j == keys);
    }
    {
        struct foo { static bool less(IndexRow const *a, IndexRow const *b) { return a->row < b->row; } };
        std::sort(tmp, tmp + keys, foo::less);
    }
    {
        uint64_t j = 0;
        for (auto i = uint64_t(0); i < keys; ++i) {
            auto ii = tmp[i];
            auto const key = ii->key64();
            do { index[j++] = *ii++; } while (j < N && ii->key64() == key);
        }
    }
    std::cerr << "info: Number of keys " << keys << std::endl;
    free(tmp);
    free(scratch);
}

static std::pair<IndexRow *, size_t> makeIndex(VDB::Database const &run)
{
    static char const *const FLDS[] = { "READ_GROUP", "NAME" };
    auto const in = run["RAW"].read(2, FLDS);
    auto const range = in.rowRange();
    auto const N = size_t(range.second - range.first);
    if (N == 0) return std::make_pair(nullptr, N);
    
    auto const index = new IndexRow[N];
    auto const freq = N / 10.0;
    auto nextReport = 1;
    
    in.foreach([&](VDB::Cursor::RowID row, std::vector<VDB::Cursor::RawData> const &data) {
        auto const i = row - range.first;
        index[i] = makeIndexRow(row, data[0], data[1]);
        while (nextReport * freq <= i) {
            std::cerr << "progress: generating keys " << nextReport << "0%" << std::endl;;
            ++nextReport;
        }
    });
    std::cerr << "status: processed " << N << " records" << std::endl;
    std::cerr << "status: indexing" << std::endl;
    
    sortIndex(N, index);

    return std::make_pair(index, N);
}

struct RawRecord : public VDB::IndexedCursorBase::Record {
    struct IndexT : public IndexRow {
        VDB::Cursor::RowID row() const { return IndexRow::row; }
        bool operator ==(IndexT const &other) const { return key64() == other.key64(); }
    };
    typedef IndexT const *IndexIteratorT;

    RawRecord(VDB::IndexedCursorBase::Record const &base) : VDB::IndexedCursorBase::Record(base) {}
    bool write(std::array<Writer2::Column, 8> const &out) const {
        auto data = this->data;
        for (auto i = 0; i < 8; ++i) {
            if (!out[i].setValue(data))
                return false;
            data = data->next();
        }
        return true;
    }
    static std::initializer_list<char const *> columns() {
        return { "READ_GROUP", "NAME", "READNO", "SEQUENCE", "REFERENCE", "CIGAR", "STRAND", "POSITION" };
    }
    friend bool operator <(RawRecord const &a, RawRecord const &b) {
        auto const agroup = a.data->asString();
        auto const bgroup = b.data->asString();
        auto const aname = a.data->next()->asString();
        auto const bname = b.data->next()->asString();
        
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
};

#define VALIDATE 0
#if VALIDATE
#include <set>

void validate(RawRecord const &r) {
    using datatype = std::pair<std::string, std::string>;
    static std::set< datatype > s;
    static datatype lastOne;

    auto const group = r.data->asString();
    auto const name = r.data->next()->asString();
    auto const thisOne = std::make_pair(group, name);
    if (s.empty() || lastOne != thisOne) {
        auto const inserted = s.insert(thisOne);
        assert(inserted.second == true);
        lastOne = thisOne;
    }
}
#else
void validate(RawRecord const &r) {
}
#endif

static int process(Writer2 const &out, VDB::Cursor const &in, RawRecord::IndexT const *const beg, RawRecord::IndexT const *const end)
{
    auto const otbl = out.table("RAW");
    std::array<Writer2::Column, 8> const columns = {
        otbl.column("READ_GROUP"),
        otbl.column("NAME"),
        otbl.column("READNO"),
        otbl.column("SEQUENCE"),
        otbl.column("REFERENCE"),
        otbl.column("CIGAR"),
        otbl.column("STRAND"),
        otbl.column("POSITION")
    };
    
    auto const range = in.rowRange();
    if (end - beg != range.second - range.first) {
        std::cerr << "error: index size doesn't match input table" << std::endl;
        return -1;
    }
    auto const freq = (range.second - range.first) / 10.0;
    auto nextReport = 1;
    uint64_t written = 0;

    std::cerr << "info: processing " << (range.second - range.first) << " records" << std::endl;
    
    auto const indexedCursor = VDB::CollidableIndexedCursor<RawRecord>(in, beg, end);
    auto const rows = indexedCursor.foreach([&](RawRecord const &a) {
        validate(a);
        a.write(columns);
        otbl.closeRow();
        ++written;
        if (nextReport * freq <= written) {
            std::cerr << "progress: writing " << nextReport << "0%" << std::endl;
            ++nextReport;
        }
    });
    assert(rows == written);
    assert(rows == range.second - range.first);
    assert(rows == end - beg);
    return 0;
}

static int process(std::string const &irdb, FILE *out)
{
    auto const mgr = VDB::Manager();
    auto const inDb = mgr[irdb];
    auto writer = Writer2(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("reorder-ir", "1.0.0");
    
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
    writer.beginWriting();

    RawRecord::IndexT *index;
    size_t rows;
    {
        std::cerr << "status: creating clustering index" << std::endl;
        auto const p = makeIndex(inDb);
        index = static_cast<RawRecord::IndexT *>(p.first);
        rows = p.second;
    }
    
    auto const in = inDb["RAW"].read(RawRecord::columns());

    std::cerr << "status: rewriting rows in clustered order" << std::endl;
    auto const result = process(writer, in, index, index + rows);
    std::cerr << "status: done" << std::endl;

    writer.endWriting();
    delete [] index;
    return result;
}

using namespace utility;

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

        randomizeSBox();
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 7) == "-stable") {
                unrandomizeSBox();
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
