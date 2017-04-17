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
#include <cstddef>
#include "vdb.hpp"
#include "IRIndex.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dispatch/dispatch.h>

struct WorkUnit;

class WorkQueue {
    std::queue<WorkUnit> *queue;
    dispatch_semaphore_t sema;
    dispatch_queue_t prot;
public:
    WorkQueue(std::queue<WorkUnit> *queue)
    : queue(queue)
    , sema(dispatch_semaphore_create(0))
    , prot(dispatch_queue_create(nullptr, DISPATCH_QUEUE_SERIAL))
    {}
    ~WorkQueue() {
        dispatch_release(prot);
        dispatch_release(sema);
    }
    void push(WorkUnit const *);
    bool pop(WorkUnit *);
};

static bool compare(IndexRow const &a, IndexRow const &b)
{
    auto const diff = memcmp(a.key, b.key, 8);
    if (diff < 0) return true;
    if (diff > 0) return false;
    return a.row < b.row;
}

struct WorkUnit {
    IndexRow *beg;
    IndexRow *scratch;
    IndexRow *end;
    int level;
    
    WorkUnit() : beg(0), end(0), scratch(0), level(-1) {}
    WorkUnit(IndexRow *const beg, IndexRow *const scratch, size_t count, int const level)
    : beg(beg)
    , end(beg + count)
    , scratch(scratch)
    , level(level)
    {}
    WorkUnit(void *const beg, void *const scratch, size_t size, int const level)
    {
        this->beg = (IndexRow *)beg;
        end = this->beg + (size / sizeof(IndexRow));
        this->scratch = (IndexRow *)scratch;
        this->level = level;
    }
    ptrdiff_t size() const { return end - beg; }
    void process(WorkQueue *const fifo) const
    {
        static int const BPL[] = { 2, 2, 4, 8, 8, 8, 8, 8, 8, 8 };
        static int const IPL[] = { 0, 0, 0, 1, 2, 3, 4, 5, 6, 7 };
        static int const SPL[] = { 6, 4, 0, 0, 0, 0, 0, 0, 0, 0 };
        int const b = 1 << BPL[level];
        int const m = b - 1;
        int const i = IPL[level];
        int const s = SPL[level];
        ptrdiff_t start[256];
        IndexRow *out = scratch;
        
        for (auto bin = 0; bin < b; ++bin) {
            for (auto cur = beg; cur != end; ++cur) {
                auto const key = (cur->key[i] >> s) & m;
                if (key == bin)
                    *out++ = *cur;
            }
            start[bin] = out - scratch;
        }
        memcpy(beg, scratch, (out - scratch) * sizeof(*out));

        if (level == sizeof(BPL)/sizeof(BPL[0]))
            return;
        
        for (auto bin = 0; bin < b; ++bin) {
            ptrdiff_t const begin = (bin > 0 ? start[bin - 1] : 0);
            ptrdiff_t const count = start[bin] - begin;

            if (count == 0) {
            }
            else if (count <= 32 * 1024) {
                std::sort(beg + begin, beg + begin + count, compare);
            }
            else {
                auto const wu = WorkUnit(beg + begin, scratch + begin, count, level + 1);
                fifo->push(&wu);
            }
        }
    }
};

void WorkQueue::push(WorkUnit const *wu)
{
    dispatch_sync(prot, ^{
        queue->push(*wu);
        dispatch_semaphore_signal(sema);
    });
}

auto const mainThread = pthread_self();
bool WorkQueue::pop(WorkUnit *const wu)
{
    if (dispatch_semaphore_wait(sema, DISPATCH_TIME_NOW) == 0) {
        dispatch_sync(prot, ^{
            *wu = queue->front();
            queue->pop();
            if (pthread_equal(mainThread, pthread_self()))
                std::cerr << queue->size() << " work units left in queue" << std::endl;
        });
        return true;
    }
    return false;
}

static void doWork(void *const vp)
{
    WorkQueue *const fifo = (WorkQueue *)vp;
    WorkUnit wu;
    
    while (fifo->pop(&wu)) {
        wu.process(fifo);
    }
}

static int process(char const *const indexFile)
{
    struct stat stat = {0};
    int const fd = open(indexFile, O_RDWR);
    if (fd < 0) {
        perror("failed to open index file");
        exit(1);
    }
    if (fstat(fd, &stat) < 0) {
        perror("failed to read index file");
        exit(1);
    }
    void *const map = mmap(0, stat.st_size, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) {
        perror("failed to read index file");
        exit(1);
    }
    void *const scratch = mmap(0, stat.st_size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, 0, 0);
    if (scratch == MAP_FAILED) {
        perror("failed to allocate scratch space");
        exit(1);
    }
    {
        std::queue<WorkUnit> queue;
        WorkQueue fifo(&queue);

        WorkUnit(map, scratch, stat.st_size, 0).process(&fifo);
        
        auto const group = dispatch_group_create();
        
        for (auto i = 1; i < 4; ++i) {
            dispatch_group_async_f(group, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0), (void *)&fifo, doWork);
        }
        dispatch_group_enter(group);
        {
            WorkUnit wu;
            
            while (fifo.pop(&wu)) {
                wu.process(&fifo);
            }
        }
        dispatch_group_leave(group);
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        dispatch_release(group);
    }
    munmap(scratch, stat.st_size);
    munmap(map, stat.st_size);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 2)
        return process(argv[1]);
    else {
        std::cerr << "usage: sortIndex <index file>" << std::endl;
        return 1;
    }
}
