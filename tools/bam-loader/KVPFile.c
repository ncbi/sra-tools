/*===========================================================================
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

#include "KVPFile.h"

#include <kfs/file.h>
#include <klib/rc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define USE_KDB_BTREE 1

#if USE_KDB_BTREE
#include <kdb/btree.h>
#else
#include "hashtable.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif



#if USE_KDB_BTREE

/* going to make an index with:                                                                        
 *  a) backing file                                                                                     
 *  b) page size is fixed at 32K, otherwise the page size would have to be stored somewhere             
 *  c) page cache - need cache limit in bytes                                                           
 *  d) 2 chunked value streams - need key chunking and value chunking                                   
 *  e) B-Tree-ish index, requiring a comparison function. this interface uses opaque keys.              
 *     the "matched-count" parameter is useful for trie traversal, but this will be built-in            
 */
rc_t KVPFileMake ( KVPFile **rslt, KFile *file, size_t climit,
    size_t kchunk_size, size_t vchunk_size,
    size_t min_ksize, size_t max_ksize, size_t min_vsize, size_t max_vsize,
    KVPFileCompareFunc cmp )
{
    return KBTreeMakeUpdate ( ( KBTree** ) rslt, file, climit, false, kbtOpaqueKey,
        kchunk_size, vchunk_size, min_ksize, max_ksize, min_vsize, max_vsize, cmp );
}


/* this whacker works fine
 */
rc_t KVPFileWhack ( KVPFile *xself, bool commit )
{
    KBTree *self = ( KBTree* ) xself;

    rc_t rc = 0;

    if ( self != NULL )
    {
        /* drop the backing file unless commiting */
        if ( ! commit )
            rc = KBTreeDropBacking ( self );

        /* drop the b-tree */
        rc = KBTreeRelease ( self );
    }

    return rc;
}


/* whacker */
void KVPValueWhack ( KVPValue *self )
{
    KBTreeValueWhack ( self );
}

/* return the pointer to thingie */
rc_t KVPValueAccessRead ( const KVPValue *self, const void **mem, size_t *bytes )
{
    return KBTreeValueAccessRead ( self, mem, bytes );
}

rc_t KVPValueAccessUpdate ( KVPValue *self, void **mem, size_t *bytes )
{
    return KBTreeValueAccessUpdate ( self, mem, bytes );
}


/* find will not modify the index in any way
 *  returns a pointer into a page, meaning that the values cannot exceed 32K in size
 *  this is done
 */
rc_t KVPFileFind ( const KVPFile *self, KVPValue *val, const void *key, size_t ksize )
{
    return KBTreeFind ( ( const KBTree* ) self, val, key, ksize );
}



/* find or insert will return an existing or create a new entry
 * note that the new entry is initially zeroed
 */
rc_t KVPFileFindOrInsert ( KVPFile *self, KVPValue *val,
                          bool *was_inserted, size_t alloc_size,
                          const void *key, size_t ksize )
{
    return KBTreeEntry ( ( KBTree* ) self, val, was_inserted, alloc_size, key, ksize );
}

rc_t KVPFileForEach ( const KVPFile *self,
                      void ( CC * f )( const void *key, size_t ksize, KVPValue *val, void *ctx ),
                      void *ctx )
{
    return KBTreeForEach((KBTree *)self, 0, f, ctx);
}


#else
typedef struct KTempMMap {
    void *base;
    size_t sz;
    int fd;
    unsigned elemsize;
    unsigned fileNo;
} KTempMMap;

static rc_t KTempMMapMake(KTempMMap *rslt, size_t initSize, size_t elemSize)
{
    char fname[4096];
    
    memset(rslt, 0, sizeof(*rslt));
    rslt->elemsize = elemSize;
    
    sprintf(fname, "/tmp/kvp.%u.XXXXXX", getpid());
    rslt->fd = mkstemp(fname);
    if (rslt->fd < 0) {
        perror(fname);
        return RC(rcApp, rcFile, rcCreating, rcFile, rcUnknown);
    }    
    unlink(fname);

    if (ftruncate(rslt->fd, initSize * elemSize) != 0)
        return RC(rcApp, rcFile, rcResizing, rcFile, rcUnknown);
    
    rslt->sz = initSize;
    
    rslt->base = mmap(0, initSize * elemSize, PROT_READ | PROT_WRITE, MAP_SHARED, rslt->fd, 0);
    if (rslt->base == MAP_FAILED)
        return RC(rcApp, rcMemMap, rcCreating, rcMemory, rcInsufficient);
    
    return 0;
}

static rc_t KTempMMapWhack(KTempMMap *self)
{
    close(self->fd);
    madvise(self->base, self->sz, MADV_DONTNEED);
    munmap(self->base, self->sz);
    return 0;
}

static void *KTempMMapPointer(KTempMMap *self, size_t offset, size_t count)
{
    void *temp;
    size_t newsz;
    
    if (offset + count <= self->sz)
        return &((uint8_t *)self->base)[offset * self->elemsize];

    newsz = self->sz;
    while (newsz < offset + count)
        newsz <<= 1;
    
    if (ftruncate(self->fd, newsz * self->elemsize) != 0)
        return NULL;

    temp = mmap(0, newsz * self->elemsize, PROT_READ | PROT_WRITE, MAP_SHARED, self->fd, 0);
    if (temp == MAP_FAILED)
        return NULL;
    
    munmap(self->base, self->sz);
    self->base = temp;
    self->sz = newsz;
    
    return &((uint8_t *)self->base)[offset * self->elemsize];
}

typedef struct key_t {
    uint8_t len;
    char key[255];
} my_key_t;

struct KVPFile {
    HashTable *ht;
    size_t next_obj;
    KTempMMap keys;
    KTempMMap values;
};

typedef struct KVPValueImpl KVPValueImpl;
struct KVPValueImpl
{
    KVPFile * file;
    size_t offset;
    size_t size;
};

static int KeyComp(const void *id, const void *key, uint32_t len, void *ctx)
{
    KVPFile *self = ctx;
    const my_key_t *test = KTempMMapPointer(&self->keys, (intptr_t)id, 1);
    int r = memcmp(test->key, key, test->len < len ? test->len : len);
    if (r == 0)
        r = test->len - len;
    return r;
}

rc_t KVPFileMake(KVPFile **rslt,
                 KFile *file,
                 size_t climit,
                 size_t kchunk_size, size_t vchunk_size,
                 size_t min_ksize, size_t max_ksize,
                 size_t min_vsize, size_t max_vsize,
                 KVPFileCompareFunc cmp
                 )
{
    rc_t rc;
    KVPFile *self = calloc(1, sizeof(*self));
    
    if (self == NULL)
        return RC(rcApp, rcMemMap, rcAllocating, rcMemory, rcExhausted);
    
    rc = KTempMMapMake(&self->keys, 256, sizeof(my_key_t));
    if (rc == 0) {
        rc = KTempMMapMake(&self->values, 256, max_vsize);
        if (rc == 0) {
            rc = HashTableMake(&self->ht, 256, KeyComp, self);
            if (rc == 0) {
                *rslt = self;
                return 0;
            }
            KTempMMapWhack(&self->values);
        }
        KTempMMapWhack(&self->keys);
    }
    free(self);
    return rc;
}

rc_t KVPFileWhack(KVPFile *self, bool commit)
{
    if (self) {
        KTempMMapWhack(&self->values);
        KTempMMapWhack(&self->keys);
        HashTableWhack(self->ht, 0, 0);
        free(self);
    }
    return 0;
}

rc_t KVPFileFind(const KVPFile *self, KVPValue *xval, const void *key, size_t ksize)
{
    KVPValueImpl *val = ( KVPValueImpl* ) xval;
    const HashTableIterator iter = HashTableLookup(self->ht, key, ksize);
    if (HashTableIteratorHasValue(&iter)) {
        val->file = (void *)self;
        val->offset = (intptr_t)HashTableIteratorGetValue(&iter);
        val->size = 1;
        return 0;
    }
    return RC(rcApp, rcMemMap, rcReading, rcId, rcNotFound);
}

rc_t KVPFileFindOrInsert(KVPFile *self, KVPValue *xval,
                         bool *was_inserted, size_t alloc_size,
                         const void *key, size_t ksize
                         )
{
    KVPValueImpl *val = ( KVPValueImpl* ) xval;
    HashTableIterator iter = HashTableLookup(self->ht, key, ksize);
    
    if (HashTableIteratorHasValue(&iter)) {
        val->file = (void *)self;
        val->offset = (intptr_t)HashTableIteratorGetValue(&iter);
        val->size = 1;
        *was_inserted = false;
        return 0;
    }
    else {
        size_t new_id = self->next_obj++;
        my_key_t *new_key = KTempMMapPointer(&self->keys, new_id, 1);
        
        new_key->len = ksize;
        memcpy(new_key->key, key, ksize);
        
        *was_inserted = true;
        val->file = (void *)self;
        val->offset = new_id;
        val->size = 1;
        HashTableIteratorSetValue(&iter, (void *)(val->offset));
    }
    return 0;
}

static rc_t KVPValueAccess(const KVPValueImpl *value, void **mem, size_t *bytes)
{
    KVPFile *self = value->file;
    void *temp = KTempMMapPointer(&self->values, value->offset, value->size);
    
    if (temp != NULL) {
        *bytes = self->values.elemsize * value->size;
        *mem = temp;
        return 0;
    }
    *bytes = 0;
    *mem = NULL;
    return RC(rcApp, rcMemMap, rcReading, rcMemory, rcNotFound);
}

rc_t KVPValueAccessRead(const KVPValue *self, const void **mem, size_t *bytes)
{
    return KVPValueAccess((const KVPValueImpl*)self, (void **)mem, bytes);
}

rc_t KVPValueAccessUpdate(KVPValue *self, void **mem, size_t *bytes)
{
    return KVPValueAccess((KVPValueImpl*)self, mem, bytes);
}

void KVPValueWhack(KVPValue *self)
{
}

#endif /* USE_KDB_BTREE */
