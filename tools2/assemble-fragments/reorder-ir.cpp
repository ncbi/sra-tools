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
#include <map>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include "utility.hpp"
#include "vdb.hpp"
#include "writer.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>

struct IndexRow {
    uint8_t key[8];
    int64_t row;
    
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

static int random_uniform(int lower_bound, int upper_bound) {
    if (upper_bound <= lower_bound) return lower_bound;
#if __APPLE__
    return arc4random_uniform(upper_bound - lower_bound) + lower_bound;
#else
    struct seed_rand {
        seed_rand() {
            srand((unsigned)time(0));
        }
    };
    auto const M = upper_bound - lower_bound;
    auto const once = seed_rand();
    auto const m = RAND_MAX - RAND_MAX % M;
    for ( ; ; ) {
        auto const r = rand();
        if (r < m) return r % M + lower_bound;
    }
#endif
}

struct HashState {
private:
    uint8_t key[8];
    
    static uint8_t popcount(uint8_t const byte) {
        static uint8_t const bits_set[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
        return bits_set[byte >> 4] + bits_set[byte & 15];
    }
    
    // this table is constructed such that it is more
    // likely that two values that differ by 1 bit (the smallest difference)
    // will map to two values that differ by 4 bits (the biggest difference)
    // the desired effect is to amplify small differences
    static uint8_t const *makeHashTable(int n)
    {
        static uint8_t tables[256 * 8];
        auto table = tables + 256 * n;
#if 1
        for (auto i = 0; i < 256; ++i)
            table[i] = i;
        for (auto i = 0; i < 256; ++i) {
            auto const j = random_uniform(i, 256);
            auto const ii = table[i];
            auto const jj = table[j];
            table[i] = jj;
            table[j] = ii;
        }
#else
        uint8_t J[256];
        
        for (auto i = 0; i < 256; ++i)
            J[i] = i;
        for (auto i = 0; i < 256; ++i) {
            auto const j = random_uniform(i, 256);
            auto const ii = J[i];
            auto const jj = J[j];
            J[i] = jj;
            J[j] = ii;
        }
        
        for (auto i = 1; i < 256; ++i) {
            auto const ii = J[i - 1];
            for (auto j = i; j < 256; ++j) {
                auto const jj = J[j];
                auto const diff = ii ^ jj;
                auto const popcount(diff);
                if (popcount == 4) {
                    if (j != i) {
                        J[j] = J[i];
                        J[i] = jj;
                    }
                    break;
                }
            }
        }
        
        uint8_t I[256];
        
        for (auto i = 0; i < 256; ++i)
            I[i] = i;
        for (auto i = 0; i < 256; ++i) {
            auto const j = random_uniform(i, 256);
            auto const ii = I[i];
            auto const jj = I[j];
            I[i] = jj;
            I[j] = ii;
        }
        
        for (auto i = 1; i < 256; ++i) {
            auto const ii = I[i - 1];
            for (auto j = i; j < 256; ++j) {
                auto const jj = I[j];
                auto const diff = ii ^ jj;
                auto const popcount(diff);
                if (popcount == 1) {
                    if (j != i) {
                        I[j] = I[i];
                        I[i] = jj;
                    }
                    break;
                }
            }
        }
        
        for (int k = 0; k < 256; ++k) {
            auto const i = I[k];
            auto const j = J[k];
            table[i] = j;
        }
#endif
        return table;
    }
    static uint8_t const *makeSalt(void)
    {
        // all of the 8-bit values with popcount == 4
        static uint8_t table[] = {
            0x33, 0x35, 0x36, 0x39, 0x3A, 0x3C,
            0x53, 0x55, 0x56, 0x59, 0x5A, 0x5C,
            0x63, 0x65, 0x66, 0x69, 0x6A, 0x6C,
            0x93, 0x95, 0x96, 0x99, 0x9A, 0x9C,
            0xA3, 0xA5, 0xA6, 0xA9, 0xAA, 0xAC,
            0xC3, 0xC5, 0xC6, 0xC9, 0xCA, 0xCC
        };
        
        for (auto i = 0; i < 36; ++i) {
            auto const j = random_uniform(i, 36);
            auto const ii = table[i];
            auto const jj = table[j];
            table[i] = jj;
            table[j] = ii;
        }
        return table;
    }
    
    static decltype(makeHashTable(0)) const H1, H2, H3, H4, H5, H6, H7, H8;
    static decltype(makeSalt()) const H0;
    
public:
    HashState() {
#if 1
        std::copy(H0, H0 + 8, key);
#else
        *(uint64_t *)key = 0xcbf29ce484222325;
#endif
    }
    void append(uint8_t const byte) {
#if 1
        key[0] = H1[key[0] ^ byte];
        key[1] = H2[key[1] ^ byte];
        key[2] = H3[key[2] ^ byte];
        key[3] = H4[key[3] ^ byte];
        key[4] = H5[key[4] ^ byte];
        key[5] = H6[key[5] ^ byte];
        key[6] = H7[key[6] ^ byte];
        key[7] = H8[key[7] ^ byte];
#else
        uint64_t hash = *(uint64_t *)key;
        hash ^= byte;
        hash *= 0x100000001b3;
        *(uint64_t *)key = hash;
#endif
    }
    HashState &append(VDB::Cursor::RawData const &data) {
        for (auto i = 0; i < data.elements; ++i) {
            append(((uint8_t const *)data.data)[i]);
        }
        return *this;
    }
    HashState &append(std::string const &data) {
        for (auto && ch : data) {
            append(ch);
        }
        return *this;
    }
    void end(uint8_t rslt[8]) {
        std::copy(key, key + 8, rslt);
    }
};

auto const HashState::H0 = HashState::makeSalt();
auto const HashState::H1 = HashState::makeHashTable(0);
auto const HashState::H2 = HashState::makeHashTable(1);
auto const HashState::H3 = HashState::makeHashTable(2);
auto const HashState::H4 = HashState::makeHashTable(3);
auto const HashState::H5 = HashState::makeHashTable(4);
auto const HashState::H6 = HashState::makeHashTable(5);
auto const HashState::H7 = HashState::makeHashTable(6);
auto const HashState::H8 = HashState::makeHashTable(7);

static IndexRow makeIndexRow(int64_t row, VDB::Cursor::RawData const &group, VDB::Cursor::RawData const &name)
{
    IndexRow y;
    
    HashState().append(group).append(name).end(y.key);
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
    
    std::vector<WorkUnit> process() const
    {
        static int const BPL[] = { 1, 3, 4, 8, 8, 8, 8, 8, 8, 8 };
        static int const KPL[] = { 0, 0, 0, 1, 2, 3, 4, 5, 6, 7 };
        static int const SPL[] = { 7, 4, 0, 0, 0, 0, 0, 0, 0, 0 };
        
        std::vector<WorkUnit> result;
        
        if (level < sizeof(BPL)/sizeof(BPL[0])) {
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
                    result.push_back(WorkUnit(out + begin, out + begin + count, beg + begin, level + 1));
                }
            }
        }
        return result;
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
    
    Context(IndexRow *Src, IndexRow *Out, size_t count, size_t SmallSize)
    : src(Src)
    , srcEnd(src + count)
    , out(Out)
    , outEnd(out + count)
    , next(0)
    , running(0)
    , smallSize(SmallSize)
    , mutex(PTHREAD_MUTEX_INITIALIZER)
    , cond_running(PTHREAD_COND_INITIALIZER)
    {
        queue = WorkUnit(src, srcEnd, out, 0).process();
        queue.reserve(65536);
    }
    
    void run(void) {
        auto const tid = pthread_self();
        pthread_mutex_lock(&mutex);
        std::cerr << "Thread " << tid << ": started" << std::endl;
        for ( ;; ) {
            if (next < queue.size()) {
                auto const unit = queue[next];
                ++next;
                if (unit.size() <= smallSize) {
                    // the mutex is released before processing the work unit
                    pthread_mutex_unlock(&mutex);
                    
                    // sort in one shot
                    std::sort(unit.beg, unit.end, IndexRow::keyLess);
                    if (unit.beg >= src && unit.end <= srcEnd)
                        std::copy(unit.beg, unit.end, out + (unit.beg - src));
                    
                    // the mutex is re-acquired after processing the work unit
                    pthread_mutex_lock(&mutex);
                }
                else {
                    ++running;
                    
                    // the mutex is released before processing the work unit
                    pthread_mutex_unlock(&mutex);
                    
                    // partial sort and generate more work units
                    auto const nw = unit.process();
                    
                    // the mutex is re-acquired after processing the work unit
                    pthread_mutex_lock(&mutex);
                    queue.insert(queue.end(), nw.cbegin(), nw.cend());
                    --running;
                    pthread_cond_signal(&cond_running);
                }
            }
            else if (running > 0) {
                std::cerr << "Thread " << tid << ": waiting" << std::endl;
                pthread_cond_wait(&cond_running, &mutex);
                std::cerr << "Thread " << tid << ": awake" << std::endl;
            }
            else
                break;
            // it is an invariant that the mutex is held by the current thread regardless of the code path taken
        }
        pthread_mutex_unlock(&mutex);
        std::cerr << "Thread " << tid << ": done" << std::endl;
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
    auto const scratch = reinterpret_cast<IndexRow *>(malloc(N * sizeof(*index)));
    if (scratch == NULL) {
        perror("error: insufficient memory to create temporary index");
        exit(1);
    }
    {
        auto const workers = getWorkerCount();
        auto context = Context(index, scratch, N, getSmallSize(workers));
        
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
    }
    std::sort(tmp, tmp + keys, [](IndexRow const *a, IndexRow const *b) { return a->row < b->row; });
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
    
    auto const map = malloc(N * sizeof(IndexRow));
    if (map == MAP_FAILED) {
        perror("error: insufficient memory to create temporary index");
        exit(1);
    }
    auto const index = (IndexRow *)map;
    auto const freq = N / 10.0;
    auto nextReport = 1;
    
    for (auto row = range.first; row < range.second; ++row) {
        auto const readGroup = in.read(row, 1);
        auto const fragment = in.read(row, 2);
        
        index[row - range.first] = makeIndexRow(row, readGroup, fragment);
        
        if (nextReport * freq <= row - range.first) {
            std::cerr << "progress: generating keys " << nextReport << "0%" << std::endl;;
            ++nextReport;
        }
    }
    std::cerr << "status: processed " << N << " records" << std::endl;
    std::cerr << "status: indexing" << std::endl;
    
    sortIndex(N, index);

    return std::make_pair(index, N);
}

static bool write(Writer2::Column const &out, VDB::Cursor::Data const *in)
{
    return out.setValue(in->elements, in->elem_bits / 8, in->data());
}

static int process(Writer2 const &out, VDB::Cursor const &in, IndexRow const *const beg, IndexRow const *const end)
{
    auto const otbl = out.table("RAW");
    auto const colGroup = otbl.column("READ_GROUP");
    auto const colName = otbl.column("NAME");
    auto const colSequence = otbl.column("SEQUENCE");
    auto const colReference = otbl.column("REFERENCE");
    auto const colStrand = otbl.column("STRAND");
    auto const colCigar = otbl.column("CIGAR");
    auto const colReadNo = otbl.column("READNO");
    auto const colPosition = otbl.column("POSITION");
    
    auto const range = in.rowRange();
    if (end - beg != range.second - range.first) {
        std::cerr << "error: index size doesn't match input table" << std::endl;
        return -1;
    }
    auto const freq = (range.second - range.first) / 10.0;
    auto nextReport = 1;
    uint64_t written = 0;
    size_t const bufferSize = 4ul * 1024ul * 1024ul * 1024ul;
    auto buffer = malloc(bufferSize);
    auto bufEnd = (void const *)((uint8_t const *)buffer + bufferSize);
    auto blockSize = 0;
    
    std::cerr << "info: processing " << (range.second - range.first) << " records" << std::endl;
    std::cerr << "info: record storage is " << bufferSize / 1024 / 1024 << "MB" << std::endl;
    for (auto i = beg; i != end; ) {
    AGAIN:
        auto j = i + (blockSize == 0 ? 1000000 : blockSize);
        if (j > end)
            j = end;
        auto m = std::map<int64_t, unsigned>();
        {
            auto v = std::vector<IndexRow>();
            
            v.reserve(j - i);
            if (j != end) {
                auto ii = i;
                while (ii < j) {
                    auto jj = ii;
                    do { ++jj; } while (jj < j && jj->key64() == ii->key64());
                    if (jj < j) {
                        v.insert(v.end(), ii, jj);
                        ii = jj;
                    }
                    else
                        break;
                }
                j = ii;
            }
            else {
                v.insert(v.end(), i, j);
            }
            std::sort(v.begin(), v.end(), IndexRow::rowLess);

            auto cp = buffer;
            unsigned count = 0;
            for (auto && r : v) {
                auto const row = r.row;
                auto const tmp = ((uint8_t const *)cp - (uint8_t const *)buffer) / 4;
                for (auto i = 0; i < 8; ++i) {
                    auto const data = in.read(row, i + 1);
                    auto p = data.copy(cp, bufEnd);
                    if (p == nullptr) {
                        std::cerr << "warn: filled buffer; reducing block size and restarting!" << std::endl;
                        blockSize = 0.99 * count;
                        std::cerr << "info: block size " << blockSize << std::endl;
                        goto AGAIN;
                    }
                    cp = p->end();
                }
                ++count;
                m[row] = (unsigned)tmp;
            }
            if (blockSize == 0) {
                auto const avg = double(((uint8_t *)cp - (uint8_t *)buffer)) / count;
                std::cerr << "info: average record size is " << size_t(avg + 0.5) << " bytes" << std::endl;
                blockSize = 0.95 * (double(((uint8_t *)bufEnd - (uint8_t *)buffer)) / avg);
                std::cerr << "info: block size " << blockSize << std::endl;
            }
        }
        for (auto k = i; k < j; ) {
            // the index is clustered by hash value, but there wasn't anything
            // in the index generation code to deal with hash collisions, that
            // is dealt with here, by reading all of the records with the same
            // hash value and then sorting by group and name (and reference and
            // position)
            auto v = std::vector<decltype(k->row)>();
            auto const key = k->key64();
            do { v.push_back(k->row); ++k; } while (k < j && k->key64() == key);
            
            std::sort(v.begin(), v.end(), [&](decltype(k->row) a, decltype(k->row) b) {
                auto adata = (VDB::Cursor::Data const *)((uint8_t const *)buffer + m[a] * 4);
                auto bdata = (VDB::Cursor::Data const *)((uint8_t const *)buffer + m[b] * 4);

                auto const agroup = adata->asString();
                auto const bgroup = bdata->asString();

                adata = adata->next();
                bdata = bdata->next();

                auto const aname = adata->asString();
                auto const bname = bdata->asString();
                
                auto const sameGroup = (agroup == bgroup);

                if (sameGroup && aname == bname)
                    return a < b; // it's expected that collisions are (exceedingly) rare

                return sameGroup ? (aname < bname) : (agroup < bgroup);
            });
            for (auto && row : v) {
                auto data = (VDB::Cursor::Data const *)((uint8_t const *)buffer + m[row] * 4);
                
                write(colGroup      , data);
                write(colName       , data = data->next());
                write(colSequence   , data = data->next());
                write(colReference  , data = data->next());
                write(colCigar      , data = data->next());
                write(colStrand     , data = data->next());
                write(colReadNo     , data = data->next());
                write(colPosition   , data = data->next());
                otbl.closeRow();
                
                ++written;
                if (nextReport * freq <= written) {
                    std::cerr << "progress: writing " << nextReport << "0%" << std::endl;
                    ++nextReport;
                }
            }
        }
        i = j;
    }
    
    return 0;
}

static int process(std::string const &irdb, FILE *out)
{
    auto const mgr = VDB::Manager();
    auto const inDb = mgr[irdb];
    
    IndexRow *index;
    size_t rows;
    {
        std::cerr << "status: creating clustering index" << std::endl;
        auto const p = makeIndex(inDb);
        index = p.first;
        rows = p.second;
    }
    auto writer = Writer2(out);
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("reorder-ir", "1.0.0");

    writer.addTable("RAW", {
        { "READ_GROUP"  , 1 },  ///< string
        { "NAME"        , 1 },  ///< string
        { "SEQUENCE"    , 1 },  ///< string
        { "REFERENCE"   , 1 },  ///< string
        { "CIGAR"       , 1 },  ///< string
        { "STRAND"      , 1 },  ///< char
        { "READNO"      , 4 },  ///< int32_t
        { "POSITION"    , 4 },  ///< int32_t
    });
    writer.beginWriting();
    
    auto const in = inDb["RAW"].read({ "READ_GROUP", "NAME", "SEQUENCE", "REFERENCE", "CIGAR", "STRAND", "READNO", "POSITION" });

    std::cerr << "status: rewriting rows in clustered order" << std::endl;
    auto const result = process(writer, in, index, index + rows);
    std::cerr << "status: done" << std::endl;

    writer.endWriting();
    free(index);
    return result;
}

using namespace utility;

namespace reorderIR {
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout)
        << "usage: " << commandLine.program[0] << " [-out=<index>] <ir db>"
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
        for (auto && arg : commandLine.argument) {
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
