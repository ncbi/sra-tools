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

typedef struct HashTable HashTable;
typedef struct HashTableIterator HashTableIterator;

typedef int (*HashTableKeyCompFunc)(const void *value, const void *key, uint32_t key_length, void *context);
typedef void (*HashTableFreeFunc)(void *value, void *context);

struct HashTableIterator {
    const void *root;
    const void *bucket;
    uint64_t id;
};

rc_t HashTableMake(HashTable **rslt, uint32_t initialSize, HashTableKeyCompFunc kf, const void *context);

void HashTableForEach(const HashTable *self, void (*fn)(void *value, void *context), void *context);

bool HashTableDoUntil(const HashTable *self, bool (*fn)(void *value, void *context), void *context);

HashTableIterator HashTableLookup(HashTable *self, const void *key, uint32_t key_length);

void HashTableWhack(HashTable *self, HashTableFreeFunc fn, void *context);

void HashTableRemove(HashTable *self, const HashTableIterator *iter, HashTableFreeFunc whack, void *context);

bool HashTableIteratorHasValue(const HashTableIterator *iter);

const void *HashTableIteratorGetValue(const HashTableIterator *iter);

rc_t HashTableIteratorSetValue(HashTableIterator *iter, const void *value);
