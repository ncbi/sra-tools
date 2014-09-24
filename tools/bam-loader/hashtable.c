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

#include <klib/rc.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#if TESTING
#include <stdio.h>
#include <string.h>
#endif

#include "hashtable.h"

typedef struct HTBucket HTBucket;
typedef struct HTEntry HTEntry;

#define HASH_TABLE_ENTRIES_MAX (0x80000)
#define HASH_TABLE_PREALLOC_MAX 1

struct HTEntry {
    const void *payload;
    uint32_t hashValue;
};

struct HTBucket {
    HTBucket *overflow;
    uint32_t used;      /* one bit per entry */
    HTEntry entry[32];
};

struct HashTable {
    size_t hashMask;    /* the "size" of the table is hashMask + 1 */
    size_t entryCount;  /* total number of entries in table */

    HashTableKeyCompFunc comp_func;
    const void *ctx;
    
#if HASH_TABLE_PREALLOC_MAX
    HTBucket table[HASH_TABLE_ENTRIES_MAX];
#else
    HTBucket *table;
#endif
};

typedef struct HashTableRealIterator {
    HashTable *table;
    HTBucket *bucket;
    uint32_t slot;
    uint32_t hashValue;
} HashTableRealIterator;

typedef union HashTableImplIterator {
    HashTableRealIterator real;
    HashTableIterator user;
} HashTableImplIterator;

static unsigned FirstSet(unsigned bits)
{
    assert(bits != 0);
    if ((bits & 0xFFFF) != 0) {
        if ((bits & 0xFF) != 0) {
            if ((bits & 0xF) != 0) {
                if ((bits & 0x3) != 0) {
                    if ((bits & 0x1) != 0)
                        return 0;
                    else
                        return 1;
                }
                else if ((bits & 0x4) != 0)
                    return 2;
                else
                    return 3;
            }
            else if ((bits & 0x30) != 0) {
                if ((bits & 0x10) != 0)
                    return 4;
                else
                    return 5;
            }
            else if ((bits & 0x40) != 0)
                return 6;
            else
                return 7;
        }
        else if ((bits & 0xF00) != 0) {
            if ((bits & 0x300) != 0) {
                if ((bits & 0x100) != 0)
                    return 8;
                else
                    return 9;
            }
            else if ((bits & 0x400) != 0)
                return 10;
            else
                return 11;
        }
        else if ((bits & 0x3000) != 0) {
            if ((bits & 0x1000) != 0)
                return 12;
            else
                return 13;
        }
        else if ((bits & 0x4000) != 0)
            return 14;
        else
            return 15; 
    }
    else if ((bits & 0xFF0000) != 0) {
        if ((bits & 0xF0000) != 0) {
            if ((bits & 0x30000) != 0) {
                if ((bits & 0x10000) != 0)
                    return 16;
                else
                    return 17;
            }
            else if ((bits & 0x40000) != 0)
                return 18;
            else
                return 19;
        }
        else if ((bits & 0x300000) != 0) {
            if ((bits & 0x100000) != 0)
                return 20;
            else
                return 21;
        }
        else if ((bits & 0x400000) != 0)
            return 22;
        else
            return 23;
    }
    else if ((bits & 0xF000000) != 0) {
        if ((bits & 0x3000000) != 0) {
            if ((bits & 0x1000000) != 0)
                return 24;
            else
                return 25;
        }
        else if ((bits & 0x4000000) != 0)
            return 26;
        else
            return 27;
    }
    else if ((bits & 0x30000000) != 0) {
        if ((bits & 0x10000000) != 0)
            return 28;
        else
            return 29;
    }
    else if ((bits & 0x40000000) != 0)
        return 30;
    else
        return 31;
    
}

static rc_t HTBucketNextOpenSlot(HTBucket *self, HTBucket **rslt, unsigned *slot)
{
    while (self->used == 0xFFFFFFFF) {
        if (self->overflow != NULL) {
            self = self->overflow;
            continue;
        }
        self->overflow = malloc(sizeof(*self->overflow));
        if (self->overflow == NULL)
            return RC(rcCont, rcBuffer, rcAllocating, rcMemory, rcExhausted);
        self->overflow->overflow = NULL;
        self->overflow->used = 0;
        *rslt = self->overflow;
        *slot = 0;
        return 0;
    }
    *rslt = self;
    *slot = FirstSet(~self->used);
    
    return 0;
}

static rc_t HTBucketInsert(HTBucket *self, const HTEntry *entry)
{
    HTBucket *found;
    unsigned slot;
    rc_t rc = HTBucketNextOpenSlot(self, &found, &slot);
    
    if (rc)
        return rc;
    found->used |= (1 << slot);
    found->entry[slot] = *entry;
    return 0;
}

static bool HTBucketFreeSlot(HTBucket *self, unsigned slot)
{
    unsigned i;
    
    self->used &= ~(1 << slot);
#if _DEBUGGING
    self->entry[slot].payload = NULL;
#endif
    if (self->overflow == NULL)
        return true;
    
    /* pull something out of the overflow */
    i = FirstSet(self->overflow->used);
    self->entry[slot] = self->overflow->entry[i];
    self->used |= (1 << slot);
    HTBucketFreeSlot(self->overflow, i);
    if (self->overflow->used == 0) {
        free(self->overflow);
        self->overflow = NULL;
    }
    return false;
}

static void HTBucketForEach(const HTBucket *self, void (*fn)(void *payload, void *ctx), void *ctx)
{
    do {
        uint32_t bits = self->used;
        unsigned i;
        
        for (i = 0; i != 32 && bits != 0; ++i, bits >>= 1) {
            if ((bits & 1) == 0)
                continue;
            fn((void *)self->entry[i].payload, ctx);
        }
        self = self->overflow;
    } while (self != NULL);
}

static bool HTBucketDoUntil(const HTBucket *self, bool (*fn)(void *payload, void *ctx), void *ctx)
{
    do {
        uint32_t bits = self->used;
        unsigned i;
        
        for (i = 0; i != 32 && bits != 0; ++i, bits >>= 1) {
            if ((bits & 1) == 0)
                continue;
            if (fn((void *)self->entry[i].payload, ctx))
                return true;
        }
        self = self->overflow;
    } while (self != NULL);
    return false;
}

static bool HTBucketValidate(const HTBucket *self, uint32_t hash, uint32_t mask)
{
    do {
        uint32_t bits = self->used;
        unsigned i;
        
        for (i = 0; i != 32 && bits != 0; ++i, bits >>= 1) {
            const HTEntry *entry = &self->entry[i];
            bool is_free = (bits & 1) == 0;
#if _DEBUGGING
            bool is_null = entry->payload == NULL;
            
            if (is_free != is_null)
                return true;
#endif
            if (is_free)
                continue;
            if ((entry->hashValue & mask) != hash)
                return true;
        }
        if (self->overflow && self->overflow->used == 0)
            return true;
        self = self->overflow;
    } while (self != NULL);
    return false;
}

static void HTBucketWhack(HTBucket *self, void (*fn)(void *payload, void *ctx), void *ctx)
{
    uint32_t bits;
    unsigned i;
    
    if (self->overflow) {
        HTBucketWhack(self->overflow, fn, ctx);
        free(self->overflow);
    }
    for (i = 0, bits = self->used; i != 32 && bits != 0 && fn; ++i, bits >>= 1) {
        if ((bits & 1) == 0)
            continue;
        fn((void *)self->entry[i].payload, ctx);
    }
}

static uint32_t GetHashValue(const uint8_t *key, unsigned len)
{
    /*
     * My basic hash function is Pearson's hash.
     * The tables below contain each value between 0 and 255 inclusive, in
     * random order.  The tables were generated with the following perl script.
     *
     *   #!perl -w
     *   my @x = (0 .. 255); # each value between 0 and 255 inclusive
     *   foreach (0 .. 10000) { # randomize the order
     *       my $i = $_ % 256;                    # a sequential index
     *       my $r = rand(256);                   # a random index
     *       ($x[$i], $x[$r]) = ($x[$r], $x[$i]); # swap x[i] and x[r]
     *   }
     *   print join(', ', map { sprintf '%3u', $_; } @x);
     *
     * You can generate lots of different 8-bit hash functions this way.
     *
     * I generate four 8-bit hashes and combine them to make a 32-bit one.
     * Even though I may not use all bits for the hash (e.g. the limit on the
     * table's size is much less than UINT32_MAX), computing and storing the
     * full hash reduces the number of string compares I have to do when buckets
     * have lots of items in them.
     */
    static const uint8_t T1[] = {
         64, 186,  39, 203,  54, 211,  98,  32,  26,  23, 219,  94,  77,  60,  56, 184,
        129, 242,  10,  91,  84, 192,  19, 197, 231, 133, 125, 244,  48, 176, 160, 164,
         17,  41,  57, 137,  44, 196, 116, 146, 105,  40, 122,  47, 220, 226, 213, 212,
        107, 191,  52, 144,   9, 145,  81, 101, 217, 206,  85, 134, 143,  58, 128,  20,
        236, 102,  83, 149, 148, 180, 167, 163,  12, 239,  31,   0,  73, 152,   1,  15,
         75, 200,   4, 165,   5,  66,  25, 111, 255,  70, 174, 151,  96, 126, 147,  34,
        112, 161, 127, 181, 237,  78,  37,  74, 222, 123,  21, 132,  95,  51, 141,  45,
         61, 131, 193,  68,  62, 249, 178,  33,   7, 195, 228,  82,  27,  46, 254,  90,
        185, 240, 246, 124, 205, 182,  42,  22, 198,  69, 166,  92, 169, 136, 223, 245,
        118,  97, 115,  80, 252, 209,  49,  79, 221,  38,  28,  35,  36, 208, 187, 248,
        158, 201, 202, 168,   2,  18, 189, 119, 216, 214,  11,   6,  89,  16, 229, 109,
        120,  43, 162, 106, 204,   8, 199, 235, 142, 210,  86, 153, 227, 230,  24, 100,
        224, 113, 190, 243, 218, 215, 225, 173,  99, 238, 138,  59, 183, 154, 171, 232,
        157, 247, 233,  67,  88,  50, 253, 251, 140, 104, 156, 170, 150, 103, 117, 110,
        155,  72, 207, 250, 159, 194, 177, 130, 135,  87,  71, 175,  14,  55, 172, 121,
        234,  13,  30, 241,  93, 188,  53, 114,  76,  29,  65,   3, 179, 108,  63, 139
    };
    static const uint8_t T2[] = {
        119, 174,  44, 225,  94, 226, 141, 197,  35, 241, 179, 154,  26, 161, 129,  10,
        104,  53,  97,  12, 243,  80,  51, 123, 185,  13,  66, 214, 114,  17,  74, 165,
        110, 143, 170, 222, 242,  73,  40, 101,  56, 163,  68,  86, 198,   9,  21,  90,
          8,   1, 146, 121, 103,  70,   5, 160, 149,  50, 145, 236, 100,  75, 148,  58,
         55,  15,  33, 131, 127, 186,   6, 109, 115,   2, 108,  52, 209, 128, 208,  47,
        220, 173,  45,  72, 156, 212, 152, 102,  77,  36, 134, 137, 162,  54, 235, 167,
         28, 255, 155, 240, 135, 176, 246, 244,  22, 157,  20, 224, 210, 192,  71, 140,
         14, 211, 251, 204,  30, 213, 132, 248, 178, 187, 142,  62, 215, 106, 229, 125,
         41, 216,  49,  29,  98,  32,  18, 237,  79, 206,  63, 227,  25,  99, 150, 223,
        171, 118,   4, 139,  81,   0, 133,  95, 188, 120,  27, 124,  61, 195, 231, 207,
         31, 202,  91, 252,  46,  39, 250, 144, 238, 254, 219, 200, 130, 201,  48, 107,
        205, 177,  88,  16, 147, 221, 164, 184,  89,  67, 190,  11, 175,  42, 136, 189,
        245, 117,  83,   3, 180, 172,  76,  43, 183,  84, 218, 169, 253, 159, 196,  59,
        193, 249, 181, 116, 153,  24,  87, 239, 138, 199, 111,  82,  69, 105,  78, 151,
        122, 168, 126,  85, 228, 233,  34, 113, 194, 191,  19, 230, 182,  96, 217,  38,
        234, 232,  92,  23,  57, 203,  65,  60,  64,  37, 158, 247,  93, 166, 112,   7
    };
    static const uint8_t T3[] = {
        103,  33, 228,  32, 183, 231, 139,  60,  67,  80, 184, 100,  70,  19,  14,  58,
        104,  48, 140,  10,  91, 137,  31,  96, 102, 180,  77, 121, 251, 167,  37, 206,
         79, 151, 109,  15,  92, 174, 108,  16,  95, 145,  18, 236,  49, 177, 135,   0,
        165,  86,  83, 107, 144,  44,  27, 116, 168, 193, 119, 230,  56,  97, 138, 149,
        132,  21, 176,  39, 205,  12,  69, 179, 210,  25, 213, 194, 239, 209, 248,  28,
         93,  38, 224,  66, 105, 160,  40,  17,  23, 129, 212,   8, 178, 118, 163,  73,
          3,  63,  57,  94, 254, 186, 131, 166,  61, 191, 133, 192,  78, 114, 250, 200,
        222, 136, 188, 204, 101,  11, 247, 146, 142, 152, 171, 157, 110, 141, 199,  29,
        147,  88, 211, 240, 164, 196, 189,  50,  65, 112, 219, 237, 197, 115, 126, 154,
        246, 170, 227, 235,  42, 148,  84, 201, 242,  85,  35,   6, 173, 122, 215, 218,
         75,  82,   1, 169, 143, 195,  51, 155,  54,   9,  71,  24,  53,   7,  47, 229,
        158,  26, 245,  89, 150, 181, 255, 249, 221, 217, 223,  76,  90,  36, 216, 130,
        220, 125, 182,  55, 202, 234, 203, 123,  34, 172,  74,  30,   4, 241, 162, 233,
         64,  62, 127,  20,  52, 106, 190, 253, 185, 111, 232,   2, 187,  13,  68, 117,
        128,  98, 208,  41, 159,  46, 207, 244, 226,  22, 252, 214,   5, 243,  99, 113,
        120, 225, 175, 124, 134, 161, 156,  45,  72,  87,  43,  59, 198, 153, 238,  81
    };
    static const uint8_t T4[] = {
        103, 124,  53, 254,  91, 106, 131,  69, 158,  23, 249, 171, 241,  30, 194, 217,
         64, 123, 129, 118,  35,  16, 209, 190, 155, 104, 148, 163,  78, 188, 174, 245,
        218, 125, 210, 250, 107, 212, 117,  17, 109,  44,  71,  38, 253, 177,  11, 114,
         60, 192, 133, 219,  97, 246,  41, 228, 127, 151,  62, 213, 224, 234, 138, 162,
         31, 110, 222, 141,  25, 135,  18,  49, 132, 242, 203, 207,  68, 130, 116,  59,
         63,  67, 169, 147,  94,   0, 134,  77,  51, 128, 247,  21,  88, 145, 122,  19,
         40,  58,   9,  39, 252,   8, 156, 126, 146,  80, 159, 201, 186, 175,  74,  89,
         54, 173, 208,  86, 239,   6,  61, 139,  66,  87, 150, 231,  82,  42, 197, 202,
        199, 196, 121, 112, 161,  99,  90, 144, 105, 195, 255,  27,   3, 216, 187, 172,
         12,   4, 221, 182, 215, 214,  76,  98,  15,  33,  47, 200, 113, 140, 227, 191,
        100,  24, 251, 185, 152, 237, 235,  92,  96,   1, 193,   2, 154,  72, 198, 223,
         28,  46,  13,  56, 153, 232, 233, 143, 108, 184,  43, 119,  84, 189, 120, 211,
         10, 164,  73, 230, 149, 238,  20,  37,  95, 166, 240,  29, 204, 167, 243, 236,
         45, 170, 220,   5,   7,  50, 160,  83, 157,  22, 168, 111,  55, 137, 206,  36,
         85, 180,  79, 142, 102, 181,  14, 115,  57,  34,  75, 244, 248,  48,  93, 183,
        229, 101, 205,  70, 226, 225, 165, 176, 136, 179,  52,  32,  26,  81,  65, 178
    };
    unsigned h1 = 0;
    unsigned h2 = 0;
    unsigned h3 = 0;
    unsigned h4 = 0;
    unsigned i;
    
    for (i = 0; i != len; ++i) {
        uint8_t ch = key[i];
        h1 = T1[h1 ^ ch];
        h2 = T2[h2 ^ ch];
        h3 = T3[h3 ^ ch];
        h4 = T4[h4 ^ ch];
    }
    
    return (h4 << 24) | (h3 << 16) | (h2 << 8) | h1;
}

rc_t HashTableMake(HashTable **rslt, uint32_t initialSize, HashTableKeyCompFunc kf, const void *ctx)
{
    HashTable *self;
    unsigned i;

    *rslt = NULL;
    
    if (initialSize == 0)
        initialSize = 2;
    else {
        uint32_t m;
        
        for (m = 2; m < initialSize; m <<= 1)
            ;
        initialSize = m;
        
        if (initialSize > HASH_TABLE_ENTRIES_MAX)
            initialSize = HASH_TABLE_ENTRIES_MAX;
    }
    
    self = malloc(sizeof(*self));
    if (self == NULL)
        return RC(rcCont, rcSelf, rcAllocating, rcMemory, rcExhausted);
    
#if HASH_TABLE_PREALLOC_MAX
#else
    self->table = malloc(initialSize * sizeof(self->table[0]));
    if (self == NULL) {
        free(self);
        return RC(rcCont, rcSelf, rcAllocating, rcMemory, rcExhausted);
    }
#endif
    
    for (i = 0; i != initialSize; ++i) {
        self->table[i].used = 0;
        self->table[i].overflow = NULL;
    }
    
    self->hashMask = initialSize - 1;
    self->entryCount = 0;
    
    self->comp_func = kf;
    self->ctx = ctx;

    *rslt = self;
    
    return 0;
}

static bool HashTableValidate(const HashTable *self)
{
    unsigned i;
    
    for (i = 0; i != self->hashMask + 1; ++i) {
        if (HTBucketValidate(&self->table[i], i, self->hashMask))
            return true;
    }
    return false;
}

static rc_t HashTableExpand(HashTable *self)
{
    const uint32_t mask = (self->hashMask << 1) | 1;
    size_t size;
    unsigned i;
#if HASH_TABLE_PREALLOC_MAX
#else
    HTBucket *temp;
#endif
    
    /* the table must be smaller than the limit */
    if (self->hashMask >= HASH_TABLE_ENTRIES_MAX - 1)
        return 0;
    
    size = self->hashMask + 1;
    
    /* the density of used slots needs to be high enough */
    if (self->entryCount * 4 < size * 3 * 32)
        return 0;
    
#if HASH_TABLE_PREALLOC_MAX
#else
    temp = realloc(self->table, size * 2);
    if (temp == NULL)
        return RC(rcCont, rcTable, rcResizing, rcMemory, rcExhausted);
    self->table = temp;
#endif
    
    /* move any items that should be moved
     * to the new buckets (about half should)
     */
    for (i = 0; i != size; ++i) {
        HTBucket *bucket = &self->table[i];
        HTBucket *other  = &self->table[i + size];
        
        other->overflow = NULL;
        other->used = 0;
        do {
            unsigned j;
            uint32_t bits = bucket->used;
            
            for (j = 0; j != 32 && bits != 0; ++j, bits >>= 1) {
                /* slot j occupied? */
                if ((bits & 1) != 0) {
                    HTEntry *entry = &bucket->entry[j];
                    
                    assert((entry->hashValue & self->hashMask) == i);
                    
                    while ((entry->hashValue & mask) != i) {
                        rc_t rc;

                        /* move to new bucket */
                        assert((entry->hashValue & mask) == i + size);
                        
                        rc = HTBucketInsert(other, entry);
                        if (rc) return rc;
                        if (HTBucketFreeSlot(bucket, j))
                            break;
                        
                        /* FreeSlot decided to reuse the slot
                         * for something from the overflow,
                         * so process the new thing
                         */
                        assert((entry->hashValue & self->hashMask) == i);
                    }
                }
            }
            bucket = bucket->overflow;
        } while (bucket);
    }
    self->hashMask = mask;
#if 0
    HashTableValidate(self);
#endif    
    return 0;
}

static rc_t HashTableShrink(HashTable *self)
{
    const uint32_t mask = self->hashMask >> 1;
    size_t size;
    unsigned i;
    unsigned j;
    
    if (self->hashMask > 255)
        return 0;
    
    size = self->hashMask + 1;
    
    if (self->entryCount * 4 >= size * 3 * 32)
        return 0;
    
    for (j = 0, i = mask + 1; i != size; ++i, ++j) {
        HTBucket *bucket = &self->table[i];
        HTBucket *other  = &self->table[j];
        
        do {
            unsigned k;
            uint32_t bits = bucket->used;
            
            for (k = 0; k != 32 && bits != 0; ++k, bits >>= 1) {
                if ((bits & 1) != 0) {
                    rc_t rc;
                    HTEntry *entry = &bucket->entry[k];
                    
                    assert((entry->hashValue & self->hashMask) == i);
                    assert((entry->hashValue & mask) == j);
                        
                    rc = HTBucketInsert(other, entry);
                    if (rc) return rc;
                }
            }
            bucket = bucket->overflow;
        } while (bucket);
    }
    self->hashMask = mask;
#if 0
    HashTableValidate(self);
#endif    
    return 0;
}

static bool HashTableFindSlot(const HashTable *self,
                              const void *key,
                              uint32_t key_len,
                              HashTableRealIterator *iter
                              )
{
    uint32_t h = GetHashValue(key, key_len);
    const HTBucket *bucket = &self->table[h & self->hashMask];
    const HTBucket *open_bucket = NULL;
    int slot = 0;
    
    iter->hashValue = h;
    do {
        unsigned i;
        uint32_t bits = bucket->used;
        
        for (i = 0; i != 32 && (bits != 0 || open_bucket == NULL); ++i, bits >>= 1) {
            if ((bits & 1) == 0) {
                if (open_bucket == NULL) {
                    open_bucket = bucket;
                    slot = i;
                }
                continue;
            }
            if (bucket->entry[i].hashValue != h)
                continue;
            if (self->comp_func(bucket->entry[i].payload, key, key_len, (void *)self->ctx) != 0)
                continue;
            
            iter->bucket = (HTBucket *)bucket;
            iter->slot = i;
            return true;
        }
        if (bucket->overflow == NULL)
            break;
        bucket = bucket->overflow;
    } while (1);
    if (open_bucket == NULL) {
        iter->bucket = (HTBucket *)bucket;
        iter->slot = 32;
    }
    else {
        iter->bucket = (HTBucket *)open_bucket;
        iter->slot = slot;
    }

    return false;
}

HashTableIterator HashTableLookup(HashTable *self, const void *key, uint32_t key_length)
{
    HashTableImplIterator rslt;
    
    rslt.real.table = self;
    HashTableFindSlot(self, key, key_length, &rslt.real);
    
    return rslt.user;
}

void HashTableRemove(HashTable *self, const HashTableIterator *iter,
                        HashTableFreeFunc whack, void *ctx)
{
    const HashTableRealIterator *i = &((const HashTableImplIterator *)iter)->real;
    uint32_t hv;
    
    assert(i->table == self);
    if (i->slot > 32)
        return;
    if ((i->bucket->used & (1 << i->slot)) == 0)
        return;
    
    hv = i->hashValue;
    assert(hv == i->bucket->entry[i->slot].hashValue);
    
    if (whack)
        whack((void *)i->bucket->entry[i->slot].payload, ctx);
    
    HTBucketFreeSlot(i->bucket, i->slot);
    if (i->bucket->used == 0) {
        HTBucket *parent = &self->table[hv & self->hashMask];
        
        if (i->bucket != parent) {
            while (parent->overflow != i->bucket)
                parent = parent->overflow;
            assert(parent->overflow == i->bucket);
            assert(i->bucket->overflow == NULL);
            parent->overflow = NULL;
            free(i->bucket);
        }
    }
    --self->entryCount;
}

void HashTableForEach(const HashTable *self, void (*fn)(void *payload, void *ctx), void *ctx)
{
    unsigned i;
    
    for (i = 0; i != self->hashMask + 1; ++i) {
        HTBucketForEach(&self->table[i], fn, ctx);
    }
}

bool HashTableDoUntil(const HashTable *self, bool (*fn)(void *payload, void *ctx), void *ctx)
{
    unsigned i;
    
    for (i = 0; i != self->hashMask + 1; ++i) {
        if (HTBucketDoUntil(&self->table[i], fn, ctx))
            return true;
    }
    return false;
}

void HashTableWhack(HashTable *self, HashTableFreeFunc fn, void *ctx)
{
    unsigned i;
    
    if (self) {
        for (i = 0; i != self->hashMask + 1; ++i)
            HTBucketWhack(&self->table[i], fn, ctx);
#if HASH_TABLE_PREALLOC_MAX
#else
        free(self->table);
#endif
        free(self);
    }
}

bool HashTableIteratorHasValue(const HashTableIterator *iter)
{
    const HashTableRealIterator *i = &((const HashTableImplIterator *)iter)->real;
    
    if (i->slot < 32 && (i->bucket->used & (1 << i->slot)) != 0)
        return true;
    return false;
}

const void *HashTableIteratorGetValue(const HashTableIterator *iter)
{
    const HashTableRealIterator *i = &((const HashTableImplIterator *)iter)->real;
    return i->bucket->entry[i->slot].payload;
}

rc_t HashTableIteratorSetValue(HashTableIterator *iter, const void *value)
{
    HashTableRealIterator *i = &((HashTableImplIterator *)iter)->real;
    HashTable *self = i->table;
    
    if (i->slot >= 32) {
        rc_t rc;
        
        assert(i->bucket->used == 0xFFFFFFFF);
        rc = HashTableExpand(self);
        if (rc)
            return rc;
        rc = HTBucketNextOpenSlot(&self->table[i->hashValue & self->hashMask], &i->bucket, &i->slot);
        if (rc)
            return rc;
    }
    assert((i->bucket->used & (1 << i->slot)) == 0);
    i->bucket->entry[i->slot].hashValue = i->hashValue;
    i->bucket->entry[i->slot].payload = value;
    i->bucket->used |= (1 << i->slot);
    ++self->entryCount;
    return 0;
}

#define TESTING 0
#if TESTING
#include <stdio.h>
#include <string.h>

struct pstring {
    uint8_t len;
    char str[255];
};

static int comp_func(const void *Obj, const void *key, uint32_t len, void *ignore)
{
    const struct pstring *obj = Obj;
    return obj->len == len ? memcmp(obj->str, key, len) : obj->len - len;
}

static void print_key(void *Obj, void *ignore)
{
    const struct pstring *obj = Obj;
    
    printf("%s\n", obj->str);
    return;
}

static struct pstring **LoadTestData(unsigned *count) {
    FILE *fp = fopen("spot_name.txt", "r");
    unsigned N = 0;
    unsigned bytes = 0;
    struct pstring **rslt;
    
    *count = 0;
    if (fp == NULL) {
        perror("failed to open test data file");
        return NULL;
    }
    do {
        char buf[256];
        int i = fscanf(fp, " %256s ", buf);
        if (i != 1)
            break;
        ++N;
        bytes += strlen(buf);
    } while (1);
    rewind(fp);
    
    rslt = malloc(N * (sizeof(*rslt) + 2) + bytes);
    if (rslt) {
        unsigned i;
        char *cp = (char *)&rslt[N];
        
        for (i = 0; i != N; ++i) {
            rslt[i] = (struct pstring *)cp;
            fscanf(fp, " %s ", rslt[i]->str);
            rslt[i]->len = strlen(rslt[i]->str);
            cp += rslt[i]->len + 2;
        }
        *count = N;
    }
    fclose(fp);
    return rslt;
}

bool is_not_consistent(void *payload, void *context)
{
    return payload == NULL;
}

void test(void) {
    HashTable *ht;
    struct pstring **testdata;
    unsigned N;
    unsigned i;
    
    srand(time(0));
    
    testdata = LoadTestData(&N);
   
    if (HashTableMake(&ht, 0, comp_func, 0) != 0)
        return;
    
    for (i = 0; i != N; ++i) {
        const struct pstring *val = testdata[i % N];
        HashTableIterator iter = HashTableLookup(ht, val->str, val->len);
        
        if (HashTableIteratorHasValue(&iter)) {
            HashTableRemove(ht, &iter, 0, 0);
        }
        else {
            HashTableIteratorSetValue(&iter, val);
        }
    }
#if 1
    HashTableValidate(ht);
#else
    HashTableForEach(ht, print_key, 0);
#endif
    HashTableWhack(ht, 0, 0);
}
#endif

