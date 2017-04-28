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
#include <queue>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include "vdb.hpp"
#include "IRIndex.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>

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

static bool compareKey(IndexRow const &a, IndexRow const &b)
{
    auto const diff = memcmp(a.key, b.key, 8);
    if (diff < 0) return true;
    if (diff > 0) return false;
    return a.row < b.row;
}

static bool compareRow(IndexRow const *a, IndexRow const *b)
{
    return a->row < b->row;
}

struct Context {
    IndexRow *const map;
    IndexRow *const mapEnd;
    IndexRow *const tmp;
    IndexRow *const tmpEnd;
    size_t const smallSize; // chunk size above which more work units may be produced, else the sort is done in one shot
    
    pthread_mutex_t mutex; // protects the entire structure against mutation by other threads
    pthread_cond_t cond_running;
    unsigned running; // count of number of work units being processed, work units which might produce more work units; this is to prevent workers from quiting early, when the queue is empty but might not stay empty
    unsigned next; // next work unit to be processed; the queue is considered empty when next == queue.size()
    std::vector<WorkUnit> queue; // the queue is only ever appended to
    
    Context(void *Map, void *Tmp, size_t count, size_t SmallSize)
    : map((IndexRow *)Map)
    , mapEnd(map + count)
    , tmp((IndexRow *)Tmp)
    , tmpEnd(tmp + count)
    , next(0)
    , running(0)
    , smallSize(SmallSize)
    , mutex(PTHREAD_MUTEX_INITIALIZER)
    , cond_running(PTHREAD_COND_INITIALIZER)
    {
        queue = WorkUnit(map, mapEnd, tmp, 0).process();
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
                    std::sort(unit.beg, unit.end, compareKey);
                    if (unit.beg >= tmp && unit.end <= tmpEnd)
                        memcpy(map + (tmp - unit.beg), unit.beg, unit.size());

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
 * could go with one per logical but that would
 * just make memory contention worse
 * the bottleneck is the memory bus
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

static ssize_t fsize(int const fd)
{
    struct stat stat = {0};
    if (fstat(fd, &stat) == 0)
        return stat.st_size;
    return -1;
}

static ssize_t fblocksize(int const fd)
{
    struct stat stat = {0};
    if (fstat(fd, &stat) == 0)
        return stat.st_blksize;
    return -1;
}

static int process(char const *const indexFile, bool const isSorted)
{
    size_t size = 0;
    void *inmap = MAP_FAILED;
    {
        int const fd = open(indexFile, O_RDWR);
        if (fd < 0) {
            perror("failed to open index file");
            exit(1);
        }
        ssize_t const tmp = fsize(fd);
        if (tmp < 0) {
            perror("failed to access index file");
            exit(1);
        }
        size = tmp;
        inmap = mmap(0, size, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fd, 0);
        close(fd);
        if (inmap == MAP_FAILED) {
            perror("failed to read index file");
            exit(1);
        }
    }
    void *const scratch = mmap(0, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, 0, 0);
    if (scratch == MAP_FAILED) {
        perror("failed to allocate scratch space");
        exit(1);
    }
    int const sz = snprintf((char *)scratch, size, "%s.out", indexFile);
    if (sz >= size) {
        perror("index is too small");
        exit(1);
    }
    int const fd = open((char *)scratch, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        perror("failed to open output file");
        exit(1);
    }
    auto const blockSize = fblocksize(fd);
    if (blockSize < 0) {
        perror("failed to access output file");
        exit(1);
    }
    auto const N = size / sizeof(IndexRow);

    if (!isSorted) {
        auto const workers = getWorkerCount();
        auto context = Context(inmap, scratch, N, getSmallSize(workers));
        
        for (auto i = 1; i < workers; ++i) {
            pthread_t tid = 0;
        
            pthread_create(&tid, nullptr, worker, &context);
        }
        worker(&context);
    }

    {
        auto const map = (IndexRow *)inmap;
        auto const mapEnd = map + N;
        auto last = *(uint64_t *)(&map->key[0]);
        auto const rindex = (IndexRow **)(scratch);
        auto out = rindex;
        for (auto i = map; i != mapEnd; ++i) {
            auto key = *(uint64_t *)(&i->key[0]);
            if (last == key) continue;
            *out++ = i;
            last = key;
        }
        std::cerr << "Number of records: " << N << std::endl;
        std::cerr << "Number of fragments: " << (out - rindex) << std::endl;
        std::cerr << "Records per fragment: " << (double)N / (double)(out - rindex) << std::endl;
        
        auto const buffer = (int64_t *)out;
        auto const bufEnd = (int64_t const *)(rindex + N);
        auto cp = buffer;
        
        std::cerr << "Sorting fragments ..." << std::endl;
        std::sort(rindex, out, compareRow);
        std::cerr << "Writing final index ..." << std::endl;
        for (auto i = rindex; i != out; ++i) {
            auto j = *i;
            auto const key = *(uint64_t *)(&j->key[0]);
            do {
                auto const sz = (cp - buffer) * sizeof(*cp);
                if (sz >= blockSize || cp == bufEnd) {
                    auto rc = write(fd, buffer, sz);
                    if (rc < 0) {
                        perror("failed to write output");
                        exit(1);
                    }
                    cp = buffer;
                }
                *cp++ = (*j++).row;
            } while (*(uint64_t *)(&j->key[0]) == key);
        }
        write(fd, buffer, (cp - buffer) * sizeof(*cp));
        std::cerr << "Done" << std::endl;
        close(fd);
    }
    munmap(scratch, size);
    munmap(inmap, size);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 2)
        return process(argv[1], false);
    else {
        std::cerr << "usage: sortIndex <index file>" << std::endl;
        return 1;
    }
}
