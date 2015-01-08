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
 */

#include <klib/defs.h>
#include <klib/rc.h>

extern "C" {
#include "mem-bank.h"
}

#define USE_KMEMBANK 0

#if USE_KMEMBANK
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/pagefile.h>
#include <kfs/pmem.h>
#include <klib/log.h>
#include <klib/printf.h>

#define FRAG_CHUNK_SIZE (128)

struct MemBank {
    KMemBank *fragsOne;
    KMemBank *fragsBoth;
};

static rc_t OpenMBankFile(KMemBank **const mbank, KDirectory *const dir, int const pid, char const *const suffix, size_t const climit)
{
    KFile *file = NULL;
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "frag_data%s.%u", suffix, pid);
    
    if (rc)
        return rc;
    
    rc = KDirectoryCreateFile(dir, &file, true, 0600, kcmInit, "%s", fname);
    KDirectoryRemove(dir, 0, "%s", fname);
    if (rc == 0) {
        KPageFile *backing;
        
        rc = KPageFileMakeUpdate(&backing, file, climit, false);
        KFileRelease(file);
        if (rc == 0) {
            rc = KMemBankMake(mbank, FRAG_CHUNK_SIZE, 0, backing);
            KPageFileRelease(backing);
        }
    }
    return rc;
}

static rc_t MemBank_Make(MemBank **rslt, KDirectory *const dir, int const pid, size_t const climits[2])
{
    KMemBank *fragsOne;
    
    rc_t rc = OpenMBankFile(&fragsOne, dir, pid, "One", climits[0]);
    if (rc == 0) {
        KMemBank *fragsBoth;
        
        rc = OpenMBankFile(&fragsBoth, dir, pid, "Both", climits[1]);
        
        if (rc == 0) {
            MemBank *const self = reinterpret_cast<MemBank *>(malloc(sizeof(self)));

            if (self) {
                self->fragsOne = fragsOne;
                self->fragsBoth = fragsBoth;

                *rslt = self;
                return 0;
            }
            rc = RC(rcApp, rcFile, rcConstructing, rcMemory, rcExhausted);
            KMemBankRelease(fragsBoth);
        }
        KMemBankRelease(fragsOne);
    }
    return rc;
}

void MemBank_Release(MemBank *const self)
{
    KMemBankRelease(self->fragsBoth);
    KMemBankRelease(self->fragsOne);
    free(self);
}

rc_t MemBank_Alloc(MemBank *const self, uint32_t *const Id, size_t const size, bool const clear, bool const longlived)
{
    uint64_t id = 0;
    KMemBank *const mbank = longlived ? self->fragsOne : self->fragsBoth;
    rc_t rc = KMemBankAlloc(mbank, &id, size, clear);
    uint32_t const id_out = (uint32_t)((id << 1) + (longlived ? 1 : 0));

    if ((uint64_t)id_out != ((id << 1) + (longlived ? 1 : 0))) {
        PLOGMSG(klogFatal, (klogFatal, "membank '$(which)': id space overflow", longlived ? "fragsOne" : "fragsBoth"));
        abort();
    }
    Id[0] = id_out;
    return rc;
}

rc_t MemBank_Write(MemBank *const self, uint32_t const id, uint64_t const pos, void const *const buffer, size_t const size, size_t *const num_writ)
{
    uint64_t const myId = id >> 1;
    bool const longlived = (id & 1) ? true : false;
    KMemBank *const mbank = longlived ? self->fragsOne : self->fragsBoth;
    
    return KMemBankWrite(mbank, myId, pos, buffer, size, num_writ);
}

rc_t MemBank_Size(MemBank const *const self, uint32_t const id, size_t *const rslt)
{
    uint64_t const myId = id >> 1;
    bool const longlived = (id & 1) ? true : false;
    KMemBank const *const mbank = longlived ? self->fragsOne : self->fragsBoth;
    uint64_t size;
    rc_t const rc = KMemBankSize(mbank, myId, &size);
    
    *rslt = size;
    return rc;
}

rc_t MemBank_Read(MemBank const *const self, uint32_t const id, uint64_t const pos, void *const buffer, size_t const bsize, size_t *const num_read)
{
    uint64_t const myId = id >> 1;
    bool const longlived = (id & 1) ? true : false;
    KMemBank *const mbank = longlived ? self->fragsOne : self->fragsBoth;

    return KMemBankRead(mbank, myId, pos, buffer, bsize, num_read);
}

rc_t MemBank_Free(MemBank *const self, uint32_t const id)
{
    uint64_t const myId = id >> 1;
    bool const longlived = (id & 1) ? true : false;
    KMemBank *const mbank = longlived ? self->fragsOne : self->fragsBoth;

    return KMemBankFree(mbank, myId);
}

#else

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <algorithm>

class pmem
{
    struct allocation {
        void *memory;
        size_t size;
        
        allocation(int = 0) : memory(0), size(0) {}
    };
    typedef std::map<uint32_t, struct allocation> my_map_t;
    typedef std::set<uint32_t> my_set_t;
    my_map_t in_use;
    my_set_t no_use;
    my_map_t::size_type max_in_use;
    my_set_t::size_type max_no_use;
    my_map_t::size_type total_allocs;
    my_map_t::size_type total_frees;
    
    void *get(uint32_t const id) const
    {
        my_map_t::const_iterator const i = in_use.find(id);
        
        if (i == in_use.end())
            throw std::runtime_error("attempt to access invalid id");
        
        return i->second.memory;
    }
public:
    pmem(int = 0)
    : max_in_use(0)
    , max_no_use(0)
    , total_allocs(0)
    , total_frees(0)
    {}
    ~pmem() {
        my_map_t::iterator i;
        
        for (i = in_use.begin(); i != in_use.end(); ++i) {
            free(i->second.memory);
            ++total_frees;
        }
#if 0
        std::cerr << "max. used: " << max_in_use << std::endl;
        std::cerr << "max. free: " << max_no_use << std::endl;
        std::cerr << "num alloc: " << total_allocs << std::endl;
        std::cerr << "num frees: " << total_frees << std::endl;
#endif
    }
    
    void Write(uint32_t id, size_t const offset, size_t const size, void const *const data) const
    {
        if (offset + size <= this->Size(id)) {
            char *dst = reinterpret_cast<char *>(get(id)) + offset;
            char const *src = reinterpret_cast<char const *>(data);
            
            std::copy(src, src + size, dst);
            return;
        }
        throw std::runtime_error("attempt to write more than was allocated");
    }
    uint32_t Alloc(size_t const size, bool const clear = true)
    {
        my_map_t::key_type new_key;
        
        if (no_use.begin() == no_use.end()) {
            my_map_t::size_type const new_id = in_use.size() + 1;

            if (max_in_use < new_id)
                max_in_use = new_id;
            
            new_key = (my_map_t::key_type)new_id;
            if (new_key < new_id)
                throw std::runtime_error("pmem overflow");
        }
        else {
            my_set_t::const_iterator const j = no_use.begin();
            
            new_key = *j;
            no_use.erase(j);
        }
        my_map_t::iterator const i = in_use.insert(my_map_t::value_type(new_key, 0)).first;
        
        void *const alloc = calloc(1, size);
        if (alloc) {
            i->second.memory = alloc;
            i->second.size = size;
            
            ++total_allocs;
            return i->first;
        }
        throw std::bad_alloc();
    }
    void Free(uint32_t const id)
    {
        my_map_t::iterator const i = in_use.find(id);
        
        if (i == in_use.end())
            throw std::runtime_error("attempt to free invalid id");
        
        void *const alloc = i->second.memory;

        no_use.insert(id);
        if (max_no_use < no_use.size())
            max_no_use = no_use.size();

        in_use.erase(i);
        free(alloc);
        ++total_frees;
    }
    size_t Size(uint32_t const id) const
    {
        my_map_t::const_iterator const i = in_use.find(id);
        
        if (i == in_use.end())
            throw std::runtime_error("attempt to access invalid or freed id");
        
        return i->second.size;
    }
    void const *Read(uint32_t const id) const
    {
        return get(id);
    }
};

rc_t MemBank_Make(MemBank **bank, struct KDirectory * = 0, int = 0, size_t const * = 0)
{
    try {
        pmem *const rslt = new pmem;
        
        *bank = reinterpret_cast<MemBank *>(rslt);
        return 0;
    }
    catch (std::bad_alloc const &e) {
        return RC(rcApp, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    catch (std::exception const &e) {
        std::cout << e.what() << std::endl;
        abort();
    }
    catch (...) {
        std::cout << "this is bad!" << std::endl;
        abort();
    }
}

void MemBank_Release(MemBank *const self)
{
    delete reinterpret_cast<pmem *>(self);
}

rc_t MemBank_Alloc(MemBank *const Self, uint32_t *const id, size_t const bytes, bool const clear, bool const longlived)
{
    try {
        pmem *const self = reinterpret_cast<pmem *>(Self);
        
        *id = self->Alloc(bytes, clear);
        return 0;
    }
    catch (std::bad_alloc const &e) {
        return RC(rcApp, rcFile, rcAllocating, rcMemory, rcExhausted);
    }
    catch (std::exception const &e) {
        std::cerr << e.what() << std::endl;
        abort();
    }
    catch (...) {
        std::cout << "this is bad!" << std::endl;
        abort();
    }
}

rc_t MemBank_Write(MemBank *const Self, uint32_t const id, uint64_t const pos, void const *const buffer, size_t const bsize, size_t *const num_writ)
{
    try {
        pmem *const self = reinterpret_cast<pmem *>(Self);
        
        *num_writ = 0;

        size_t const size = self->Size((uint32_t)id);
        
        if (pos >= size)
            return 0;
        
        size_t const actsize = (bsize + pos > size) ? (size - pos) : size;
        
        *num_writ = actsize;
        self->Write(id, pos, actsize, buffer);
        
        return 0;
    }
    catch (std::exception const &e) {
        std::cerr << e.what() << std::endl;
        abort();
    }
    catch (...) {
        std::cout << "this is bad!" << std::endl;
        abort();
    }
}

rc_t MemBank_Size(MemBank const *const Self, uint32_t const id, size_t *const size)
{
    try {
        pmem const *const self = reinterpret_cast<pmem const *>(Self);
        
        *size = self->Size(id);
        return 0;
    }
    catch (std::exception const &e) {
        std::cerr << e.what() << std::endl;
        abort();
    }
    catch (...) {
        std::cout << "this is bad!" << std::endl;
        abort();
    }
}

rc_t MemBank_Read(MemBank const *const Self, uint32_t const id, uint64_t const pos, void *const buffer, size_t const bsize, size_t *const num_read)
{
    try {
        pmem const *const self = reinterpret_cast<pmem const *>(Self);
        
        *num_read = 0;
        
        size_t const size = self->Size(id);
        char const *data = reinterpret_cast<char const *>(self->Read(id)) + pos;
        
        if (pos >= size)
            return 0;
        
        size_t const actsize = (bsize + pos > size) ? (size - pos) : size;
        
        *num_read = actsize;
        
        char *dst = reinterpret_cast<char *>(buffer);
        std::copy(data, data + actsize, dst);
        
        return 0;
    }
    catch (std::exception const &e) {
        std::cerr << e.what() << std::endl;
        abort();
    }
    catch (...) {
        std::cout << "this is bad!" << std::endl;
        abort();
    }
}

rc_t MemBank_Free(MemBank *const Self, uint32_t const id)
{
    try {
        pmem *const self = reinterpret_cast<pmem *>(Self);
        
        self->Free(id);
        return 0;
    }
    catch (std::exception const &e) {
        std::cerr << e.what() << std::endl;
        abort();
    }
    catch (...) {
        std::cout << "this is bad!" << std::endl;
        abort();
    }
}
#endif

extern "C" {
    rc_t MemBankMake(MemBank **bank, struct KDirectory *dir, int pid, size_t const climits[2])
    {
      return MemBank_Make(bank, dir, pid, climits);
    }
    
    void MemBankRelease(MemBank *const self)
    {
        MemBank_Release(self);
    }
    
    rc_t MemBankAlloc(MemBank *const Self, uint32_t *const id, size_t const bytes, bool const clear, bool const longlived)
    {
        return MemBank_Alloc(Self, id, bytes, clear, longlived);
    }
    
    rc_t MemBankWrite(MemBank *const Self, uint32_t const id, uint64_t const pos, void const *const buffer, size_t const bsize, size_t *const num_writ)
    {
        return MemBank_Write(Self, id, pos, buffer, bsize, num_writ);
    }
    
    rc_t MemBankSize(MemBank const *const Self, uint32_t const id, size_t *const size)
    {
        return MemBank_Size(Self, id, size);
    }
    
    rc_t MemBankRead(MemBank const *const Self, uint32_t const id, uint64_t const pos, void *const buffer, size_t const bsize, size_t *const num_read)
    {
        return MemBank_Read(Self, id, pos, buffer, bsize, num_read);
    }
    
    rc_t MemBankFree(MemBank *const Self, uint32_t const id)
    {
        return MemBank_Free(Self, id);
    }
}

