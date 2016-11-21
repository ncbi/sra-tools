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

/* #include "bam-load.vers.h" */

#include <klib/callconv.h>
#include <klib/data-buffer.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/rc.h>
#include <klib/sort.h>
#include <klib/printf.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kdb/btree.h>
#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/meta.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>

#include <insdc/insdc.h>
#include <insdc/sra.h>
#include <align/dna-reverse-cmpl.h>
#include <align/align.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/loader-file.h>
#include <kapp/loader-meta.h>
#include <kapp/log-xml.h>
#include <kapp/progressbar.h>

#include <kproc/queue.h>
#include <kproc/thread.h>
#include <kproc/timeout.h>
#include <os-native.h>

#include <sysalloc.h>
#include <atomic32.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <zlib.h>
#include "bam.h"
#include "bam-alignment.h"
#include "Globals.h"
#include "sequence-writer.h"
#include "reference-writer.h"
#include "alignment-writer.h"
#include "mem-bank.h"
#include "low-match-count.h"

#define NUM_ID_SPACES (256u)

#define MMA_NUM_CHUNKS_BITS (20u)
#define MMA_NUM_SUBCHUNKS_BITS ((32u)-(MMA_NUM_CHUNKS_BITS))
#define MMA_SUBCHUNK_SIZE (1u << MMA_NUM_CHUNKS_BITS)
#define MMA_SUBCHUNK_COUNT (1u << MMA_NUM_SUBCHUNKS_BITS)

typedef struct {
    int fd;
    size_t elemSize;
    off_t fsize;
    uint8_t *current;
    struct mma_map_s {
        struct mma_submap_s {
            uint8_t *base;
        } submap[MMA_SUBCHUNK_COUNT];
    } map[NUM_ID_SPACES];
} MMArray;

typedef struct {
    uint32_t primaryId[2];
    uint32_t spotId;
    uint32_t fragmentId;
	uint8_t  fragment_len[2]; /*** lowest byte of fragment length to prevent different sizes of primary and secondary alignments **/
    uint8_t  platform;
    uint8_t  pId_ext[2];
    uint8_t  spotId_ext;
    uint8_t  alignmentCount[2]; /* 0..254; 254: saturated max; 255: special meaning "too many" */
    uint8_t  unmated: 1,
             pcr_dup: 1,
             unaligned_1: 1,
             unaligned_2: 1,
             hardclipped: 1,
			 primary_is_set: 1;
} ctx_value_t;

#define CTX_VALUE_SET_P_ID(O,N,V) do { int64_t tv = (V); (O).primaryId[N] = (uint32_t)tv; (O).pId_ext[N] = tv >> 32; } while(0);
#define CTX_VALUE_GET_P_ID(O,N) ((((int64_t)((O).pId_ext[N])) << 32) | (O).primaryId[N])

#define CTX_VALUE_SET_S_ID(O,V) do { int64_t tv = (V); (O).spotId = (uint32_t)tv; (O).spotId_ext = tv >> 32; } while(0);
#define CTX_VALUE_GET_S_ID(O) ((((int64_t)(O).spotId_ext) << 32) | (O).spotId)

typedef struct FragmentInfo {
    uint64_t ti;
    uint32_t readlen;
    uint8_t  aligned;
    uint8_t  is_bad;
    uint8_t  orientation;
    uint8_t  readNo;
    uint8_t  sglen;
    uint8_t  lglen;
    uint8_t  cskey;
} FragmentInfo;

typedef struct KeyToID {
    KBTree *key2id[NUM_ID_SPACES];
    char *key2id_names;

    uint32_t idCount[NUM_ID_SPACES];
    uint32_t key2id_hash[NUM_ID_SPACES];

    unsigned key2id_max;
    unsigned key2id_name_max;
    unsigned key2id_name_alloc;
    unsigned key2id_count;

    unsigned key2id_name[NUM_ID_SPACES];
    /* this array is kept in name order */
    /* this maps the names to key2id and idCount */
    unsigned key2id_oid[NUM_ID_SPACES];
} KeyToID;

typedef struct context_t {
    KeyToID keyToID;
    const KLoadProgressbar *progress[4];
    MMArray *id2value;
    MemBank *frags;
    int64_t spotId;
    int64_t primaryId;
    int64_t secondId;
    uint64_t alignCount;

    unsigned pass;
    bool isColorSpace;
} context_t;

static char const *Print_ctx_value_t(ctx_value_t const *const self)
{
    static char buffer[16384];
    rc_t rc = string_printf(buffer, sizeof(buffer), NULL, "pid: { %lu, %lu }, sid: %lu, fid: %u, alc: { %u, %u }, flg: %x", CTX_VALUE_GET_P_ID(*self, 0), CTX_VALUE_GET_P_ID(*self, 1), CTX_VALUE_GET_S_ID(*self), self->fragmentId, self->alignmentCount[0], self->alignmentCount[1], *(self->alignmentCount + sizeof(self->alignmentCount)/sizeof(self->alignmentCount[0])));

    if (rc)
        return 0;
    return buffer;
}

static rc_t MMArrayMake(MMArray **rslt, int fd, uint32_t elemSize)
{
    MMArray *const self = calloc(1, sizeof(*self));

    if (self == NULL)
        return RC(rcExe, rcMemMap, rcConstructing, rcMemory, rcExhausted);
    self->elemSize = (elemSize + 3) & ~(3u); /** align to 4 byte **/
    self->fd = fd;
    *rslt = self;
    return 0;
}

#define PERF 0
#define PROT 0

static rc_t MMArrayGet(MMArray *const self, void **const value, uint64_t const element)
{
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    unsigned const bin_no = element >> 32;
    unsigned const subbin = ((uint32_t)element) >> MMA_NUM_CHUNKS_BITS;
    unsigned const in_bin = (uint32_t)element & (MMA_SUBCHUNK_SIZE - 1);

    if (bin_no >= sizeof(self->map)/sizeof(self->map[0]))
        return RC(rcExe, rcMemMap, rcConstructing, rcId, rcExcessive);

    if (self->map[bin_no].submap[subbin].base == NULL) {
        off_t const cur_fsize = self->fsize;
        off_t const new_fsize = cur_fsize + chunk;

        if (ftruncate(self->fd, new_fsize) != 0)
            return RC(rcExe, rcFile, rcResizing, rcSize, rcExcessive);
        else {
            void *const base = mmap(NULL, chunk, PROT_READ|PROT_WRITE,
                                    MAP_FILE|MAP_SHARED, self->fd, cur_fsize);

            self->fsize = new_fsize;
            if (base == MAP_FAILED) {
                PLOGMSG(klogErr, (klogErr, "Failed to construct map for bin $(bin), subbin $(subbin)", "bin=%u,subbin=%u", bin_no, subbin));
                return RC(rcExe, rcMemMap, rcConstructing, rcMemory, rcExhausted);
            }
            else {
#if PERF
                static unsigned mapcount = 0;

                (void)PLOGMSG(klogInfo, (klogInfo, "Number of mmaps: $(cnt)", "cnt=%u", ++mapcount));
#endif
                self->map[bin_no].submap[subbin].base = base;
            }
        }
    }
    uint8_t *const next = self->map[bin_no].submap[subbin].base;
#if PROT
    if (next != self->current) {
        void *const current = self->current;

        if (current)
            mprotect(current, chunk, PROT_NONE);

        mprotect(self->current = next, chunk, PROT_READ|PROT_WRITE);
    }
#endif
    *value = &next[(size_t)in_bin * self->elemSize];
    return 0;
}

static rc_t MMArrayGetRead(MMArray *const self, void const **const value, uint64_t const element)
{
    unsigned const bin_no = element >> 32;
    unsigned const subbin = ((uint32_t)element) >> MMA_NUM_CHUNKS_BITS;
    unsigned const in_bin = (uint32_t)element & (MMA_SUBCHUNK_SIZE - 1);

    if (bin_no >= sizeof(self->map)/sizeof(self->map[0]))
        return RC(rcExe, rcMemMap, rcConstructing, rcId, rcExcessive);

    if (self->map[bin_no].submap[subbin].base == NULL)
        return RC(rcExe, rcMemMap, rcReading, rcId, rcInvalid);

    uint8_t *const next = self->map[bin_no].submap[subbin].base;
#if PROT
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    if (next != self->current) {
        void *const current = self->current;

        if (current)
            mprotect(current, chunk, PROT_NONE);

        mprotect(self->current = next, chunk, PROT_READ);
    }
#endif
    *value = &next[(size_t)in_bin * self->elemSize];
    return 0;
}

static void MMArrayLock(MMArray *const self)
{
#if PROT
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    void *const current = self->current;

    self->current = NULL;
    if (current)
        mprotect(current, chunk, PROT_NONE);
#endif
}

static void MMArrayClear(MMArray *self)
{
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    unsigned i;

    for (i = 0; i != sizeof(self->map)/sizeof(self->map[0]); ++i) {
        unsigned j;

        for (j = 0; j != sizeof(self->map[0].submap)/sizeof(self->map[0].submap[0]); ++j) {
            if (self->map[i].submap[j].base) {
#if PROT
                mprotect(self->map[i].submap[j].base, chunk, PROT_READ|PROT_WRITE);
#endif
            	memset(self->map[i].submap[j].base, 0, chunk);
#if PROT
                mprotect(self->map[i].submap[j].base, chunk, PROT_NONE);
#endif
            }
        }
    }
#if PROT
    self->current = NULL;
#endif
}

static void MMArrayWhack(MMArray *self)
{
    size_t const chunk = MMA_SUBCHUNK_SIZE * self->elemSize;
    unsigned i;

    for (i = 0; i != sizeof(self->map)/sizeof(self->map[0]); ++i) {
        unsigned j;

        for (j = 0; j != sizeof(self->map[0].submap)/sizeof(self->map[0].submap[0]); ++j) {
            if (self->map[i].submap[j].base)
            	munmap(self->map[i].submap[j].base, chunk);
        }
    }
    close(self->fd);
    free(self);
}

static rc_t OpenKBTree(KBTree **const rslt, unsigned n, unsigned max)
{
    size_t const cacheSize = (((G.cache_size - (G.cache_size / 2) - (G.cache_size / 8)) / max)
                            + 0xFFFFF) & ~((size_t)0xFFFFF);
    KFile *file = NULL;
    KDirectory *dir;
    char fname[4096];
    rc_t rc;

    rc = KDirectoryNativeDir(&dir);
    if (rc)
        return rc;

    rc = string_printf(fname, sizeof(fname), NULL, "%s/key2id.%u.%u", G.tmpfs, G.pid, n); if (rc) return rc;
    rc = KDirectoryCreateFile(dir, &file, true, 0600, kcmInit, "%s", fname);
    KDirectoryRemove(dir, 0, "%s", fname);
    KDirectoryRelease(dir);
    if (rc == 0) {
        rc = KBTreeMakeUpdate(rslt, file, cacheSize,
                              false, kbtOpaqueKey,
                              1, 255, sizeof ( uint32_t ),
                              NULL
                              );
        KFileRelease(file);
#if PERF
        if (rc == 0) {
            static unsigned treecount = 0;

            (void)PLOGMSG(klogInfo, (klogInfo, "Number of trees: $(cnt)", "cnt=%u", ++treecount));
        }
#endif
    }
    return rc;
}

static rc_t GetKeyIDOld(KeyToID *const ctx, uint64_t *const rslt, bool *const wasInserted, char const key[], char const name[], unsigned const namelen)
{
    unsigned const keylen = strlen(key);
    rc_t rc;
    uint64_t tmpKey;

    if (ctx->key2id_count == 0) {
        rc = OpenKBTree(&ctx->key2id[0], 1, 1);
        if (rc) return rc;
        ctx->key2id_count = 1;
    }
    if (memcmp(key, name, keylen) == 0) {
        /* qname starts with read group; no append */
        tmpKey = ctx->idCount[0];
        rc = KBTreeEntry(ctx->key2id[0], &tmpKey, wasInserted, name, namelen);
    }
    else {
        char sbuf[4096];
        char *buf = sbuf;
        char *hbuf = NULL;
        size_t bsize = sizeof(sbuf);
        size_t actsize;

        if (keylen + namelen + 2 > bsize) {
            hbuf = malloc(bsize = keylen + namelen + 2);
            if (hbuf == NULL)
                return RC(rcExe, rcName, rcAllocating, rcMemory, rcExhausted);
            buf = hbuf;
        }
        rc = string_printf(buf, bsize, &actsize, "%s\t%.*s", key, (int)namelen, name);

        tmpKey = ctx->idCount[0];
        rc = KBTreeEntry(ctx->key2id[0], &tmpKey, wasInserted, buf, actsize);
        if (hbuf)
            free(hbuf);
    }
    if (rc == 0) {
        *rslt = tmpKey;
        if (*wasInserted)
            ++ctx->idCount[0];
    }
    return rc;
}

static unsigned HashKey(void const *const key, unsigned const keylen)
{
#if 0
    /* There is nothing special about this hash. It was randomly generated. */
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
    unsigned h = 0x55;
    unsigned i = keylen;

    do { h = T1[h ^ ((uint8_t)i)]; } while ((i >>= 8) != 0);

    for (i = 0; i != keylen; ++i)
        h = T1[h ^ ((uint8_t const *)key)[i]];

    return h;
#else
    /* FNV-1a hash with folding */
    uint64_t h = 0xcbf29ce484222325;
    unsigned i;

    for (i = 0; i < keylen; ++i) {
        uint8_t const octet = ((uint8_t const *)key)[i];
        h = (h ^ octet) * 0x100000001b3ull;
    }
    return ((uint32_t)(h ^ (h >> 32))) % NUM_ID_SPACES;
#endif
}

#define USE_ILLUMINA_NAMING_CORRECTION 1

static size_t GetFixedNameLength(char const name[], size_t const namelen)
{
#if USE_ILLUMINA_NAMING_CORRECTION
    /*** Check for possible fixes to illumina names ****/
    size_t newlen=namelen;
    /*** First get rid of possible "/1" "/2" "/3" at the end - violates SAM spec **/
    if(newlen > 2  && name[newlen-2] == '/' &&  (name[newlen-1] == '1' || name[newlen-1] == '2' || name[newlen-1] == '3')){
        newlen -=2;
    }
    if(newlen > 2 && name[newlen-2] == '#' &&  (name[newlen-1] == '0')){ /*** Now, find "#0" ***/
        newlen -=2;
    } else if(newlen>10){ /*** find #ACGT ***/
        int i=newlen;
        for(i--;i>4;i--){ /*** stopping at 4 since the rest of record should still contain :x:y ***/
            char a=toupper(name[i]);
            if(a != 'A' && a != 'C' && a !='G' && a !='T'){
                break;
            }
        }
        if (name[i] == '#'){
            switch (newlen-i) { /** allowed values for illumina barcodes :5,6,8 **/
                case 5:
                case 6:
                case 8:
                    newlen=i;
                    break;
                default:
                    break;
            }
        }
    }
    if(newlen < namelen){ /*** check for :x:y at the end now - to make sure it is illumina **/
        int i=newlen;
        for(i--;i>0 && isdigit(name[i]);i--){}
        if(name[i]==':'){
            for(i--;i>0 && isdigit(name[i]);i--){}
            if(name[i]==':' && newlen > 0){ /*** some name before :x:y should still exist **/
                /*** looks like illumina ***/
                return newlen;
            }
        }
    }
#endif
    return namelen;
}

static
rc_t GetKeyID(KeyToID *const ctx,
              uint64_t *const rslt,
              bool *const wasInserted,
              char const key[],
              char const name[],
              size_t const o_namelen)
{
    size_t const namelen = GetFixedNameLength(name, o_namelen);

    if (ctx->key2id_max == 1)
        return GetKeyIDOld(ctx, rslt, wasInserted, key, name, namelen);
    else {
        unsigned const keylen = strlen(key);
        unsigned const h = HashKey(key, keylen);
        unsigned f;
        unsigned e = ctx->key2id_count;
        uint64_t tmpKey;

        *rslt = 0;
        {{
            uint32_t const bucket_value = ctx->key2id_hash[h];
            unsigned const n  = (uint8_t) bucket_value;
            unsigned const i1 = (uint8_t)(bucket_value >>  8);
            unsigned const i2 = (uint8_t)(bucket_value >> 16);
            unsigned const i3 = (uint8_t)(bucket_value >> 24);

            if (n > 0 && strcmp(key, ctx->key2id_names + ctx->key2id_name[i1]) == 0) {
                f = i1;
                /*
                ctx->key2id_hash[h] = (i3 << 24) | (i2 << 16) | (i1 << 8) | n;
                 */
                goto GET_ID;
            }
            if (n > 1 && strcmp(key, ctx->key2id_names + ctx->key2id_name[i2]) == 0) {
                f = i2;
                ctx->key2id_hash[h] = (i3 << 24) | (i1 << 16) | (i2 << 8) | n;
                goto GET_ID;
            }
            if (n > 2 && strcmp(key, ctx->key2id_names + ctx->key2id_name[i3]) == 0) {
                f = i3;
                ctx->key2id_hash[h] = (i2 << 24) | (i1 << 16) | (i3 << 8) | n;
                goto GET_ID;
            }
        }}
        f = 0;
        while (f < e) {
            unsigned const m = (f + e) / 2;
            unsigned const oid = ctx->key2id_oid[m];
            int const diff = strcmp(key, ctx->key2id_names + ctx->key2id_name[oid]);

            if (diff < 0)
                e = m;
            else if (diff > 0)
                f = m + 1;
            else {
                f = oid;
                goto GET_ID;
            }
        }
        if (ctx->key2id_count < ctx->key2id_max) {
            unsigned const name_max = ctx->key2id_name_max + keylen + 1;
            KBTree *tree;
            rc_t rc = OpenKBTree(&tree, ctx->key2id_count + 1, 1); /* ctx->key2id_max); */

            if (rc) return rc;

            if (ctx->key2id_name_alloc < name_max) {
                unsigned alloc = ctx->key2id_name_alloc;
                void *tmp;

                if (alloc == 0)
                    alloc = 4096;
                while (alloc < name_max)
                    alloc <<= 1;
                tmp = realloc(ctx->key2id_names, alloc);
                if (tmp == NULL)
                    return RC(rcExe, rcName, rcAllocating, rcMemory, rcExhausted);
                ctx->key2id_names = tmp;
                ctx->key2id_name_alloc = alloc;
            }
            if (f < ctx->key2id_count) {
                memmove(&ctx->key2id_oid[f + 1], &ctx->key2id_oid[f], (ctx->key2id_count - f) * sizeof(ctx->key2id_oid[f]));
            }
            ctx->key2id_oid[f] = ctx->key2id_count;
            ++ctx->key2id_count;
            f = ctx->key2id_oid[f];
            ctx->key2id_name[f] = ctx->key2id_name_max;
            ctx->key2id_name_max = name_max;

            memcpy(&ctx->key2id_names[ctx->key2id_name[f]], key, keylen + 1);
            ctx->key2id[f] = tree;
            ctx->idCount[f] = 0;
            if ((uint8_t)ctx->key2id_hash[h] < 3) {
                unsigned const n = (uint8_t)ctx->key2id_hash[h] + 1;

                ctx->key2id_hash[h] = (((ctx->key2id_hash[h] & ~(0xFFu)) | f) << 8) | n;
            }
            else {
                /* the hash function isn't working too well
                 * keep the 3 mru
                 */
                ctx->key2id_hash[h] = (((ctx->key2id_hash[h] & ~(0xFFu)) | f) << 8) | 3;
            }
        GET_ID:
            tmpKey = ctx->idCount[f];
            rc = KBTreeEntry(ctx->key2id[f], &tmpKey, wasInserted, name, namelen);
            if (rc == 0) {
                *rslt = (((uint64_t)f) << 32) | tmpKey;
                if (*wasInserted)
                    ++ctx->idCount[f];
                assert(tmpKey < ctx->idCount[f]);
            }
            return rc;
        }
        (void)PLOGMSG(klogErr, (klogErr, "too many read groups: max is $(max)", "max=%d", (int)ctx->key2id_count, (int)ctx->key2id_max));
        return RC(rcExe, rcTree, rcAllocating, rcConstraint, rcViolated);
    }
}

static rc_t OpenMMapFile(context_t *const ctx, KDirectory *const dir)
{
    int fd;
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/id2value.%u", G.tmpfs, G.pid);

    if (rc)
        return rc;

    fd = open(fname, O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
    if (fd < 0)
        return RC(rcExe, rcFile, rcCreating, rcFile, rcNotFound);
    unlink(fname);
    return MMArrayMake(&ctx->id2value, fd, sizeof(ctx_value_t));
}

static rc_t TmpfsDirectory(KDirectory **const rslt)
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir(&dir);
    if (rc == 0) {
        rc = KDirectoryOpenDirUpdate(dir, rslt, false, "%s", G.tmpfs);
        KDirectoryRelease(dir);
    }
    return rc;
}

static rc_t SetupContext(context_t *ctx, unsigned numfiles)
{
    rc_t rc = 0;

    // memset(ctx, 0, sizeof(*ctx));

    if (G.mode == mode_Archive) {
        KDirectory *dir;
        size_t fragSize[2];

        fragSize[1] = (G.cache_size / 8);
        fragSize[0] = fragSize[1] * 4;

        rc = TmpfsDirectory(&dir);
        if (rc == 0)
            rc = OpenMMapFile(ctx, dir);
        if (rc == 0)
            rc = MemBankMake(&ctx->frags, dir, G.pid, fragSize);
        KDirectoryRelease(dir);
    }
    else if (G.mode == mode_Remap) {
        KeyToID const save1 = ctx->keyToID;
        MMArray *const save2 = ctx->id2value;
        int64_t const save3 = ctx->spotId;

        memset(ctx, 0, sizeof(*ctx));
        ctx->keyToID = save1;
        ctx->id2value = save2;
        ctx->spotId = save3;
    }

    rc = KLoadProgressbar_Make(&ctx->progress[0], 0); if (rc) return rc;
    rc = KLoadProgressbar_Make(&ctx->progress[1], 0); if (rc) return rc;
    rc = KLoadProgressbar_Make(&ctx->progress[2], 0); if (rc) return rc;
    rc = KLoadProgressbar_Make(&ctx->progress[3], 0); if (rc) return rc;

    KLoadProgressbar_Append(ctx->progress[0], 100 * numfiles);

    return rc;
}

static void ContextReleaseMemBank(context_t *ctx)
{
    MemBankRelease(ctx->frags);
    ctx->frags = NULL;
}

static void ContextRelease(context_t *ctx, bool continuing)
{
    KLoadProgressbar_Release(ctx->progress[0], true);
    KLoadProgressbar_Release(ctx->progress[1], true);
    KLoadProgressbar_Release(ctx->progress[2], true);
    KLoadProgressbar_Release(ctx->progress[3], true);
    if (!continuing)
        MMArrayWhack(ctx->id2value);
    else
        MMArrayClear(ctx->id2value);
}

static
void COPY_QUAL(uint8_t D[], uint8_t const S[], unsigned const L, bool const R)
{
    if (R) {
        unsigned i;
        unsigned j;

        for (i = 0, j = L - 1; i != L; ++i, --j)
            D[i] = S[j];
    }
    else
        memcpy(D, S, L);
}

static
void COPY_READ(INSDC_dna_text D[], INSDC_dna_text const S[], unsigned const L, bool const R)
{
    static INSDC_dna_text const compl[] = {
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 , '.',  0 ,
        '0', '1', '2', '3',  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0
    };
    if (R) {
        unsigned i;
        unsigned j;

        for (i = 0, j = L - 1; i != L; ++i, --j)
            D[i] = compl[((uint8_t const *)S)[j]];
    }
    else
        memcpy(D, S, L);
}

static KFile *MakeDeferralFile() {
    if (G.deferSecondary) {
        char template[4096];
        int fd;
        KFile *f;
        KDirectory *d;
        size_t nwrit;

        KDirectoryNativeDir(&d);
        string_printf(template, sizeof(template), &nwrit, "%s/defer.XXXXXX", G.tmpfs);
        fd = mkstemp(template);
        KDirectoryOpenFileWrite(d, &f, true, template);
        close(fd);
        unlink(template);
        return f;
    }
    return NULL;
}

static rc_t OpenBAM(const BAM_File **bam, VDatabase *db, const char bamFile[])
{
    rc_t rc = 0;

    if (strcmp(bamFile, "/dev/stdin") == 0) {
        rc = BAM_FileMake(bam, MakeDeferralFile(), G.headerText, "/dev/stdin");
    }
    else {
        rc = BAM_FileMake(bam, MakeDeferralFile(), G.headerText, "%s", bamFile);
    }
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open '$(file)'", "file=%s", bamFile));
    }
    else if (db) {
        KMetadata *dbmeta;

        rc = VDatabaseOpenMetadataUpdate(db, &dbmeta);
        if (rc == 0) {
            KMDataNode *node;

            rc = KMetadataOpenNodeUpdate(dbmeta, &node, "BAM_HEADER");
            KMetadataRelease(dbmeta);
            if (rc == 0) {
                char const *header;
                size_t size;

                rc = BAM_FileGetHeaderText(*bam, &header, &size);
                if (rc == 0) {
                    rc = KMDataNodeWrite(node, header, size);
                }
                KMDataNodeRelease(node);
            }
        }
    }

    return rc;
}

static rc_t VerifyReferences(BAM_File const *bam, Reference const *ref)
{
    rc_t rc = 0;
    uint32_t n;
    unsigned i;

    BAM_FileGetRefSeqCount(bam, &n);
    for (i = 0; i != n; ++i) {
        BAMRefSeq const *refSeq;

        BAM_FileGetRefSeq(bam, i, &refSeq);
        if (G.refFilter && strcmp(refSeq->name, G.refFilter) != 0)
            continue;

        rc = ReferenceVerify(ref, refSeq->name, refSeq->length, refSeq->checksum);
        if (rc) {
            if (GetRCObject(rc) == rcChecksum && GetRCState(rc) == rcUnequal) {
#if NCBI
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); checksums do not match", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
#endif
            }
            else if (GetRCObject(rc) == rcSize && GetRCState(rc) == rcUnequal) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); lengths do not match", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
            else if (GetRCObject(rc) == rcSize && GetRCState(rc) == rcEmpty) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); fasta file is empty", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
            else if (GetRCObject(rc) == rcId && GetRCState(rc) == rcNotFound) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); no match found", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
            else {
                (void)PLOGERR(klogWarn, (klogWarn, rc, "Reference: '$(name)', Length: $(len); error", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
            }
        }
        else if (G.onlyVerifyReferences) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Reference: '$(name)', Length: $(len); match found", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
        }
    }
    return 0;
}

static uint8_t GetMapQ(BAM_Alignment const *rec)
{
    uint8_t mapQ;

    BAM_AlignmentGetMapQuality(rec, &mapQ);
    return mapQ;
}

static bool EditAlignedQualities(uint8_t qual[], bool const hasMismatch[], unsigned readlen)
{
    unsigned i;
    bool changed = false;

    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1= hasMismatch[i] ? G.alignedQualValue : q_0;
        
        if (q_0 != q_1) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return false;
    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1= hasMismatch[i] ? G.alignedQualValue : q_0;

        qual[i] = q_1;
    }
    return true;
}

static bool EditUnalignedQualities(uint8_t qual[], bool const hasMismatch[], unsigned readlen)
{
    unsigned i;
    bool changed = false;

    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1 = (q_0 & 0x7F) | (hasMismatch[i] ? 0x80 : 0);
        
        if (q_0 != q_1) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return false;
    for (i = 0; i < readlen; ++i) {
        uint8_t const q_0 = qual[i];
        uint8_t const q_1 = (q_0 & 0x7F) | (hasMismatch[i] ? 0x80 : 0);

        qual[i] = q_1;
    }
    return true;
}

static bool platform_cmp(char const platform[], char const test[])
{
    unsigned i;

    for (i = 0; ; ++i) {
        int ch1 = test[i];
        int ch2 = toupper(platform[i]);

        if (ch1 != ch2)
            break;
        if (ch1 == 0)
            return true;
    }
    return false;
}

static
INSDC_SRA_platform_id GetINSDCPlatform(BAM_File const *bam, char const name[]) {
    if (name) {
        BAMReadGroup const *rg;

        BAM_FileGetReadGroupByName(bam, name, &rg);
        if (rg && rg->platform) {
            switch (toupper(rg->platform[0])) {
            case 'C':
                if (platform_cmp(rg->platform, "COMPLETE GENOMICS"))
                    return SRA_PLATFORM_COMPLETE_GENOMICS;
                if (platform_cmp(rg->platform, "CAPILLARY"))
                    return SRA_PLATFORM_CAPILLARY;
                break;
            case 'H':
                if (platform_cmp(rg->platform, "HELICOS"))
                    return SRA_PLATFORM_HELICOS;
                break;
            case 'I':
                if (platform_cmp(rg->platform, "ILLUMINA"))
                    return SRA_PLATFORM_ILLUMINA;
                if (platform_cmp(rg->platform, "IONTORRENT"))
                    return SRA_PLATFORM_ION_TORRENT;
                break;
            case 'L':
                if (platform_cmp(rg->platform, "LS454"))
                    return SRA_PLATFORM_454;
                break;
            case 'N':
                if (platform_cmp(name, "NANOPORE"))
                    return SRA_PLATFORM_OXFORD_NANOPORE;
                break;
            case 'O':
                if (platform_cmp(name, "OXFORD_NANOPORE"))
                    return SRA_PLATFORM_OXFORD_NANOPORE;
                break;
            case 'P':
                if (platform_cmp(rg->platform, "PACBIO"))
                    return SRA_PLATFORM_PACBIO_SMRT;
                break;
            case 'S':
                if (platform_cmp(rg->platform, "SOLID"))
                    return SRA_PLATFORM_ABSOLID;
                if (platform_cmp(name, "SANGER"))
                    return SRA_PLATFORM_CAPILLARY;
                break;
            default:
                break;
            }
        }
    }
    return SRA_PLATFORM_UNDEFINED;
}

static
rc_t CheckLimitAndLogError(void)
{
    unsigned const count = ++G.errCount;

    if (G.maxErrCount > 0 && count > G.maxErrCount) {
        (void)PLOGERR(klogErr, (klogErr, SILENT_RC(rcAlign, rcFile, rcReading, rcError, rcExcessive), "Number of errors $(cnt) exceeds limit of $(max): Exiting", "cnt=%u,max=%u", count, G.maxErrCount));
        return RC(rcAlign, rcFile, rcReading, rcError, rcExcessive);
    }
    return 0;
}

static
void RecordNoMatch(char const readName[], char const refName[], uint32_t const refPos)
{
    if (G.noMatchLog) {
        static uint64_t lpos = 0;
        char logbuf[256];
        size_t len;

        if (string_printf(logbuf, sizeof(logbuf), &len, "%s\t%s\t%u\n", readName, refName, refPos) == 0) {
            KFileWrite(G.noMatchLog, lpos, logbuf, len, NULL);
            lpos += len;
        }
    }
}

static LowMatchCounter *lmc = NULL;

static
rc_t LogNoMatch(char const readName[], char const refName[], unsigned rpos, unsigned matches)
{
    rc_t const rc = CheckLimitAndLogError();
    static unsigned count = 0;

    if (lmc == NULL)
        lmc = LowMatchCounterMake();
    assert(lmc != NULL);
    LowMatchCounterAdd(lmc, refName);

    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGMSG(klogErr, (klogErr, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
        return rc;
    }
    if (G.maxWarnCount_NoMatch == 0 || count < G.maxWarnCount_NoMatch)
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    return 0;
}

struct rlmc_context {
    KMDataNode *node;
    unsigned node_number;
    rc_t rc;
};

static void RecordLowMatchCount(void *Ctx, char const name[], unsigned const count)
{
    struct rlmc_context *const ctx = Ctx;

    if (ctx->rc == 0) {
        KMDataNode *sub = NULL;

        ctx->rc = KMDataNodeOpenNodeUpdate(ctx->node, &sub, "LOW_MATCH_COUNT_%u", ++ctx->node_number);
        if (ctx->rc == 0) {
            uint32_t const count_temp = count;
            ctx->rc = KMDataNodeWriteAttr(sub, "REFNAME", name);
            if (ctx->rc == 0)
                ctx->rc = KMDataNodeWriteB32(sub, &count_temp);
            
            KMDataNodeRelease(sub);
        }
    }
}

static rc_t RecordLowMatchCounts(KMDataNode *const node)
{
    struct rlmc_context ctx;

    assert(lmc != NULL);
    if (node) {
        ctx.node = node;
        ctx.node_number = 0;
        ctx.rc = 0;

        LowMatchCounterEach(lmc, &ctx, RecordLowMatchCount);
    }
    return ctx.rc;
}

static
rc_t LogDupConflict(char const readName[])
{
    rc_t const rc = CheckLimitAndLogError();
    static unsigned count = 0;

    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    }
    else if (G.maxWarnCount_DupConflict == 0 || count < G.maxWarnCount_DupConflict)
        (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    return rc;
}

static char const *const CHANGED[] = {
    "FLAG changed",
    "QUAL changed",
    "SEQ changed",
    "record made unaligned",
    "record made unfragmented",
    "mate alignment lost",
    "record discarded",
    "reference name changed",
    "CIGAR changed"
};

#define FLAG_CHANGED (0)
#define QUAL_CHANGED (1)
#define SEQ_CHANGED (2)
#define MAKE_UNALIGNED (3)
#define MAKE_UNFRAGMENTED (4)
#define MATE_LOST (5)
#define DISCARDED (6)
#define REF_NAME_CHANGED (7)
#define CIGAR_CHANGED (8)

static char const *const REASONS[] = {
/* FLAG changed */
    "0x400 and 0x200 both set",                 /*  0 */
    "conflicting PCR Dup flags",                /*  1 */
    "primary alignment already exists",         /*  2 */
    "was already recorded as unaligned",        /*  3 */
/* QUAL changed */
    "original quality used",                    /*  4 */
    "unaligned colorspace",                     /*  5 */
    "aligned bases",                            /*  6 */
    "unaligned bases",                          /*  7 */
    "reversed",                                 /*  8 */
/* unaligned */
    "low MAPQ",                                 /*  9 */
    "low match count",                          /* 10 */
    "missing alignment info",                   /* 11 */
    "missing reference position",               /* 12 */
    "invalid alignment info",                   /* 13 */
    "invalid reference position",               /* 14 */
    "invalid reference",                        /* 15 */
    "unaligned reference",                      /* 16 */
    "unknown reference",                        /* 17 */
    "hard-clipped colorspace",                  /* 18 */
/* unfragmented */
    "missing fragment info",                    /* 19 */
    "too many fragments",                       /* 20 */
/* mate info lost */
    "invalid mate reference",                   /* 21 */
    "missing mate alignment info",              /* 22 */
    "unknown mate reference",                   /* 23 */
/* discarded */
    "conflicting PCR duplicate",                /* 24 */
    "conflicting fragment info",                /* 25 */
    "reference is skipped",                     /* 26 */
/* reference name changed */
    "reference was named more than once",       /* 27 */
/* CIGAR changed */
    "alignment overhanging end of reference",   /* 28 */
/* discarded */
    "hard-clipped secondary alignment",         /* 29 */
    "low-matching secondary alignment",         /* 30 */
};

static struct {
    unsigned what, why;
} const CHANGES[] = {
    {FLAG_CHANGED,  0},
    {FLAG_CHANGED,  1},
    {FLAG_CHANGED,  2},
    {FLAG_CHANGED,  3},
    {QUAL_CHANGED,  4},
    {QUAL_CHANGED,  5},
    {QUAL_CHANGED,  6},
    {QUAL_CHANGED,  7},
    {QUAL_CHANGED,  8},
    {SEQ_CHANGED,  8},
    {MAKE_UNALIGNED,  9},
    {MAKE_UNALIGNED, 10},
    {MAKE_UNALIGNED, 11},
    {MAKE_UNALIGNED, 12},
    {MAKE_UNALIGNED, 13},
    {MAKE_UNALIGNED, 14},
    {MAKE_UNALIGNED, 15},
    {MAKE_UNALIGNED, 16},
    {MAKE_UNALIGNED, 17},
    {MAKE_UNALIGNED, 18},
    {MAKE_UNFRAGMENTED, 19},
    {MAKE_UNFRAGMENTED, 20},
    {MATE_LOST, 21},
    {MATE_LOST, 22},
    {MATE_LOST, 23},
    {DISCARDED, 24},
    {DISCARDED, 25},
    {DISCARDED, 26},
    {DISCARDED, 17},
    {REF_NAME_CHANGED, 27},
    {CIGAR_CHANGED, 28},
    {DISCARDED, 29},
    {DISCARDED, 30},
};

#define NUMBER_OF_CHANGES ((unsigned)(sizeof(CHANGES)/sizeof(CHANGES[0])))
static unsigned change_counter[NUMBER_OF_CHANGES];

static void LOG_CHANGE(unsigned const change)
{
    ++change_counter[change];
}

static void PrintChangeReport(void)
{
    unsigned i;

    for (i = 0; i != NUMBER_OF_CHANGES; ++i) {
        if (change_counter[i] > 0) {
            char const *const what = CHANGED[CHANGES[i].what];
            char const *const why  = REASONS[CHANGES[i].why];

            PLOGMSG(klogInfo, (klogInfo, "$(what) $(times) times because $(reason)", "what=%s,reason=%s,times=%u", what, why, change_counter[i]));
        }
    }
}

static rc_t RecordChange(KMDataNode *const node,
                         char const node_name[],
                         unsigned const node_number,
                         char const what[],
                         char const why[],
                         unsigned const count)
{
    KMDataNode *sub = NULL;
    rc_t const rc_sub = KMDataNodeOpenNodeUpdate(node, &sub, "%s_%u", node_name, node_number);

    if (rc_sub) return rc_sub;
    {
        uint32_t const count_temp = count;
        rc_t const rc_attr1 = KMDataNodeWriteAttr(sub, "change", what);
        rc_t const rc_attr2 = KMDataNodeWriteAttr(sub, "reason", why);
        rc_t const rc_value = KMDataNodeWriteB32(sub, &count_temp);
        
        KMDataNodeRelease(sub);
        if (rc_attr1) return rc_attr1;
        if (rc_attr2) return rc_attr2;
        if (rc_value) return rc_value;
        
        return 0;
    }
}

static rc_t RecordChanges(KMDataNode *const node, char const name[])
{
    if (node) {
        unsigned i;
        unsigned j = 0;

        for (i = 0; i != NUMBER_OF_CHANGES; ++i) {
            if (change_counter[i] > 0) {
                char const *const what = CHANGED[CHANGES[i].what];
                char const *const why  = REASONS[CHANGES[i].why];
                rc_t const rc = RecordChange(node, name, ++j, what, why, change_counter[i]);

                if (rc) return rc;
            }
        }
    }
    return 0;
}

#define FLAG_CHANGED_400_AND_200   do { LOG_CHANGE( 0); } while(0)
#define FLAG_CHANGED_PCR_DUP       do { LOG_CHANGE( 1); } while(0)
#define FLAG_CHANGED_PRIMARY_DUP   do { LOG_CHANGE( 2); } while(0)
#define FLAG_CHANGED_WAS_UNALIGNED do { LOG_CHANGE( 3); } while(0)
#define QUAL_CHANGED_OQ            do { LOG_CHANGE( 4); } while(0)
#define QUAL_CHANGED_UNALIGNED_CS  do { LOG_CHANGE( 5); } while(0)
#define QUAL_CHANGED_ALIGNED_EDIT  do { LOG_CHANGE( 6); } while(0)
#define QUAL_CHANGED_UNALIGN_EDIT  do { LOG_CHANGE( 7); } while(0)
#define QUAL_CHANGED_REVERSED      do { LOG_CHANGE( 8); } while(0)
#define SEQ__CHANGED_REV_COMP      do { LOG_CHANGE( 9); } while(0)
#define UNALIGNED_LOW_MAPQ         do { LOG_CHANGE(10); } while(0)
#define UNALIGNED_LOW_MATCH_COUNT  do { LOG_CHANGE(11); } while(0)
#define UNALIGNED_MISSING_INFO     do { LOG_CHANGE(12); } while(0)
#define UNALIGNED_MISSING_REF_POS  do { LOG_CHANGE(13); } while(0)
#define UNALIGNED_INVALID_INFO     do { LOG_CHANGE(14); } while(0)
#define UNALIGNED_INVALID_REF_POS  do { LOG_CHANGE(15); } while(0)
#define UNALIGNED_INVALID_REF      do { LOG_CHANGE(16); } while(0)
#define UNALIGNED_UNALIGNED_REF    do { LOG_CHANGE(17); } while(0)
#define UNALIGNED_UNKNOWN_REF      do { LOG_CHANGE(18); } while(0)
#define UNALIGNED_HARD_CLIPPED_CS  do { LOG_CHANGE(19); } while(0)
#define UNFRAGMENT_MISSING_INFO    do { LOG_CHANGE(20); } while(0)
#define UNFRAGMENT_TOO_MANY        do { LOG_CHANGE(21); } while(0)
#define MATE_INFO_LOST_INVALID     do { LOG_CHANGE(22); } while(0)
#define MATE_INFO_LOST_MISSING     do { LOG_CHANGE(23); } while(0)
#define MATE_INFO_LOST_UNKNOWN_REF do { LOG_CHANGE(24); } while(0)
#define DISCARD_PCR_DUP            do { LOG_CHANGE(25); } while(0)
#define DISCARD_BAD_FRAGMENT_INFO  do { LOG_CHANGE(26); } while(0)
#define DISCARD_SKIP_REFERENCE     do { LOG_CHANGE(27); } while(0)
#define DISCARD_UNKNOWN_REFERENCE  do { LOG_CHANGE(28); } while(0)
#define RENAMED_REFERENCE          do { LOG_CHANGE(29); } while(0)
#define OVERHANGING_ALIGNMENT      do { LOG_CHANGE(30); } while(0)
#define DISCARD_HARDCLIP_SECONDARY do { LOG_CHANGE(31); } while(0)
#define DISCARD_BAD_SECONDARY      do { LOG_CHANGE(32); } while(0)

static bool isHardClipped(unsigned const ops, uint32_t const cigar[/* ops */])
{
    unsigned i;

    for (i = 0; i < ops; ++i) {
        uint32_t const op = cigar[i];
        int const code = op & 0x0F;

        if (code == 5)
            return true;
    }
    return false;
}

static rc_t FixOverhangingAlignment(KDataBuffer *cigBuf, uint32_t *opCount, uint32_t refPos, uint32_t refLen, uint32_t readlen)
{
    uint32_t const *cigar = cigBuf->base;
    int refend = refPos;
    int seqpos = 0;
    unsigned i;

    for (i = 0; i < *opCount; ++i) {
        uint32_t const op = cigar[i];
        int const len = op >> 4;
        int const code = op & 0x0F;

        switch (code) {
        case 0: /* M */
        case 7: /* = */
        case 8: /* X */
            seqpos += len;
            refend += len;
            break;
        case 2: /* D */
        case 3: /* N */
            refend += len;
            break;
        case 1: /* I */
        case 4: /* S */
        case 9: /* B */
            seqpos += len;
        default:
            break;
        }
        if (refend > refLen) {
            int const chop = refend - refLen;
            int const newlen = len - chop;
            int const left = seqpos - chop;
            if (left * 2 > readlen) {
                int const clip = readlen - left;
                rc_t rc;

                *opCount = i + 2;
                rc = KDataBufferResize(cigBuf, *opCount);
                if (rc) return rc;
                ((uint32_t *)cigBuf->base)[i  ] = (newlen << 4) | code;
                ((uint32_t *)cigBuf->base)[i+1] = (clip << 4) | 4;
                OVERHANGING_ALIGNMENT;
                break;
            }
        }
    }
    return 0;
}

static context_t GlobalContext;
static timeout_t bamq_tm;
static KQueue *bamq;
static KThread *bamread_thread;

static rc_t run_bamread_thread(const KThread *self, void *const file)
{
    rc_t rc = 0;
    size_t NR = 0;

    while (rc == 0) {
        BAM_Alignment const *crec = NULL;
        BAM_Alignment *rec = NULL;

        ++NR;
        rc = BAM_FileRead2(file, &crec);
        if ((int)GetRCObject(rc) == rcRow && (int)GetRCState(rc) == rcEmpty) {
            rc = CheckLimitAndLogError();
            continue;
        }
        if ((int)GetRCObject(rc) == rcRow && (int)GetRCState(rc) == rcNotFound) {
            /* EOF */
            rc = 0;
            --NR;
            break;
        }
        if (rc) break;
        rc = BAM_AlignmentCopy(crec, &rec);
        BAM_AlignmentRelease(crec);
        if (rc) break;

        {
            static char const dummy[] = "";
            char const *spotGroup;
            char const *name;
            size_t namelen;

            BAM_AlignmentGetReadName2(rec, &name, &namelen);
            BAM_AlignmentGetReadGroupName(rec, &spotGroup);
            rc = GetKeyID(&GlobalContext.keyToID, &rec->keyId, &rec->wasInserted, spotGroup ? spotGroup : dummy, name, namelen);
            if (rc) break;
        }

        for ( ; ; ) {
            rc = KQueuePush(bamq, rec, &bamq_tm);
            if (rc == 0 || (int)GetRCObject(rc) != rcTimeout)
                break;
        }
    }
    KQueueSeal(bamq);
    if (rc) {
        (void)LOGERR(klogErr, rc, "bamread_thread done");
    }
    else {
        (void)PLOGMSG(klogInfo, (klogInfo, "bamread_thread done; read $(NR) records", "NR=%lu", NR));
    }
    return rc;
}

/* call on main thread only */
static BAM_Alignment const *getNextRecord(BAM_File const *const bam, rc_t *const rc)
{
    if (bamq == NULL) {
        TimeoutInit(&bamq_tm, 10000); /* 10 seconds */
        *rc = KQueueMake(&bamq, 4096);
        if (*rc) return NULL;
        *rc = KThreadMake(&bamread_thread, run_bamread_thread, (void *)bam);
        if (*rc) {
            KQueueRelease(bamq);
            bamq = NULL;
            return NULL;
        }
    }
    while (*rc == 0 && (*rc = Quitting()) == 0) {
        BAM_Alignment const *rec = NULL;

        *rc = KQueuePop(bamq, (void **)&rec, &bamq_tm);
        if (*rc == 0)
            return rec; /* this is the normal return */

        if ((int)GetRCObject(*rc) == rcTimeout)
            *rc = 0;
        else {
            if ((int)GetRCObject(*rc) == rcData && (int)GetRCState(*rc) == rcDone)
                (void)LOGMSG(klogDebug, "KQueuePop Done");
            else
                (void)PLOGERR(klogWarn, (klogWarn, *rc, "KQueuePop Error", NULL));
        }
    }
	KQueueSeal(bamq);
    {
        rc_t rc2 = 0;
        KThreadWait(bamread_thread, &rc2);
        if (rc2 != 0)
            *rc = rc2;
    }
    KThreadRelease(bamread_thread);
    bamread_thread = NULL;
	KQueueRelease(bamq);
    bamq = NULL;
    return NULL;
}

static void getSpotGroup(BAM_Alignment const *const rec, char spotGroup[])
{
    char const *rgname;

    BAM_AlignmentGetReadGroupName(rec, &rgname);
    if (rgname)
        strcpy(spotGroup, rgname);
    else
        spotGroup[0] = '\0';
}

static char const *getLinkageGroup(BAM_Alignment const *const rec)
{
    static char linkageGroup[1024];
    char const *BX = NULL;
    char const *CB = NULL;
    char const *UB = NULL;

    linkageGroup[0] = '\0';
    BAM_AlignmentGetLinkageGroup(rec, &BX, &CB, &UB);
    if (BX == NULL) {
        if (CB != NULL && UB != NULL) {
            unsigned const cblen = strlen(CB);
            unsigned const ublen = strlen(UB);
            if (cblen + ublen + 8 < sizeof(linkageGroup)) {
                memcpy(&linkageGroup[        0], "CB:", 3);
                memcpy(&linkageGroup[        3], CB, cblen);
                memcpy(&linkageGroup[cblen + 3], "|UB:", 4);
                memcpy(&linkageGroup[cblen + 7], UB, ublen + 1);
            }
        }
    }
    else {
        unsigned const bxlen = strlen(BX);
        if (bxlen + 1 < sizeof(linkageGroup))
            memcpy(linkageGroup, BX, bxlen + 1);
    }
    return linkageGroup;
}

static rc_t ProcessBAM(char const bamFile[], context_t *ctx, VDatabase *db,
                        /* data outputs */
                       Reference *ref, Sequence *seq, Alignment *align,
                       /* output parameters */
                       bool *had_alignments, bool *had_sequences)
{
    const BAM_File *bam;
    const BAM_Alignment *rec;
    KDataBuffer buf;
    KDataBuffer fragBuf;
    KDataBuffer cigBuf;
    rc_t rc;
    int32_t lastRefSeqId = -1;
    bool wasRenamed = false;
    size_t rsize;
    uint64_t keyId = 0;
    uint64_t reccount = 0;
    char spotGroup[512];
    size_t namelen;
    float progress = 0.0;
    unsigned warned = 0;
    long     fcountBoth=0;
    long     fcountOne=0;
    int skipRefSeqID = -1;
    int unmapRefSeqId = -1;
    uint64_t recordsRead = 0;
    uint64_t recordsProcessed = 0;
    uint64_t filterFlagConflictRecords=0; /*** counts number of conflicts between flags 0x400 and 0x200 ***/
#define MAX_WARNINGS_FLAG_CONFLICT 10000 /*** maximum errors to report ***/

    bool isColorSpace = false;
    bool isNotColorSpace = G.noColorSpace;
    char alignGroup[32];
    size_t alignGroupLen;
    AlignmentRecord data;
    KDataBuffer seqBuffer;
    KDataBuffer qualBuffer;
    SequenceRecord srec;
    SequenceRecordStorage srecStorage;

    /* setting up buffers */
    memset(&data, 0, sizeof(data));
    memset(&srec, 0, sizeof(srec));

    srec.ti             = srecStorage.ti;
    srec.readStart      = srecStorage.readStart;
    srec.readLen        = srecStorage.readLen;
    srec.orientation    = srecStorage.orientation;
    srec.is_bad         = srecStorage.is_bad;
    srec.alignmentCount = srecStorage.alignmentCount;
    srec.aligned        = srecStorage.aligned;
    srec.cskey          = srecStorage. cskey;

    rc = OpenBAM(&bam, db, bamFile);
    if (rc) return rc;
    if (!G.noVerifyReferences && ref != NULL) {
        rc = VerifyReferences(bam, ref);
        if (G.onlyVerifyReferences) {
            BAM_FileRelease(bam);
            return rc;
        }
    }
    if (ctx->keyToID.key2id_max == 0) {
        uint32_t rgcount;
        unsigned rgi;
        
        BAM_FileGetReadGroupCount(bam, &rgcount);
        if (rgcount > (sizeof(ctx->keyToID.key2id)/sizeof(ctx->keyToID.key2id[0]) - 1))
            ctx->keyToID.key2id_max = 1;
        else
            ctx->keyToID.key2id_max = sizeof(ctx->keyToID.key2id)/sizeof(ctx->keyToID.key2id[0]);

        for (rgi = 0; rgi != rgcount; ++rgi) {
            BAMReadGroup const *rg;

            BAM_FileGetReadGroup(bam, rgi, &rg);
            if (rg && rg->platform && platform_cmp(rg->platform, "CAPILLARY")) {
                G.hasTI = true;
                break;
            }
        }
    }

    /* setting up more buffers */
    rc = KDataBufferMake(&cigBuf, 32, 0);
    if (rc)
        return rc;

    rc = KDataBufferMake(&fragBuf, 8, 1024);
    if (rc)
        return rc;

    rc = KDataBufferMake(&buf, 16, 0);
    if (rc)
        return rc;

    rc = KDataBufferMake(&seqBuffer, 8, 4096);
    if (rc)
        return rc;

    rc = KDataBufferMake(&qualBuffer, 8, 4096);
    if (rc)
        return rc;

    if (rc == 0) {
        (void)PLOGMSG(klogInfo, (klogInfo, "Loading '$(file)'", "file=%s", bamFile));
    }

    while ((rec = getNextRecord(bam, &rc)) != NULL) {
        bool aligned;
        uint32_t readlen;
        uint16_t flags;
        int64_t rpos=0;
        char *seqDNA;
        const BAMRefSeq *refSeq;
        ctx_value_t *value;
        bool wasInserted;
        int32_t refSeqId=-1;
        uint8_t *qual;
        bool mated;
        const char *name;
        char cskey = 0;
        bool originally_aligned;
        bool isPrimary;
        uint32_t opCount;
        bool hasCG = false;
        uint64_t ti = 0;
        uint32_t csSeqLen = 0;
        int lpad = 0;
        int rpad = 0;
        bool hardclipped = false;
        bool revcmp = false;
        unsigned readNo = 0;
        bool wasPromoted = false;
        char const *barCode = NULL;
        char const *linkageGroup;

        ++recordsRead;
        
        BAM_AlignmentGetReadName2(rec, &name, &namelen);

		keyId = rec->keyId;
		wasInserted = rec->wasInserted;

        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "MMArrayGet: failed on id '$(id)'", "id=%u", keyId));
            goto LOOP_END;
        }

        {
            float const new_value = BAM_FileGetProportionalPosition(bam) * 100.0;
            float const delta = new_value - progress;
            if (delta > 1.0) {
                KLoadProgressbar_Process(ctx->progress[0], delta, false);
                progress = new_value;
            }
        }

        BAM_AlignmentGetBarCode(rec, &barCode);
        linkageGroup = getLinkageGroup(rec);

        if (!G.noColorSpace) {
            if (BAM_AlignmentHasColorSpace(rec)) {
                if (isNotColorSpace) {
MIXED_BASE_AND_COLOR:
                    rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains base space and color space", "file=%s", bamFile));
                    goto LOOP_END;
                }
                /* COLORSPACE is disabled!
                 * ctx->isColorSpace = isColorSpace = true; */
            }
            else if (isColorSpace)
                goto MIXED_BASE_AND_COLOR;
            else
                isNotColorSpace = true;
        }
        BAM_AlignmentGetFlags(rec, &flags);

        originally_aligned = (flags & BAMFlags_SelfIsUnmapped) == 0;
        aligned = originally_aligned;

        mated = false;
        if (flags & BAMFlags_WasPaired) {
            if ((flags & BAMFlags_IsFirst) != 0)
                readNo |= 1;
            if ((flags & BAMFlags_IsSecond) != 0)
                readNo |= 2;
            switch (readNo) {
            case 1:
            case 2:
                mated = true;
                break;
            case 0:
                if ((warned & 1) == 0) {
                    (void)LOGMSG(klogWarn, "Spots without fragment info have been encountered");
                    warned |= 1;
                }
                UNFRAGMENT_MISSING_INFO;
                break;
            case 3:
                if ((warned & 2) == 0) {
                    (void)LOGMSG(klogWarn, "Spots with more than two fragments have been encountered");
                    warned |= 2;
                }
                UNFRAGMENT_TOO_MANY;
                break;
            }
        }
        if (!mated)
            readNo = 1;

        isPrimary = (flags & (BAMFlags_IsNotPrimary|BAMFlags_IsSupplemental)) == 0 ? true : false;
        if (G.deferSecondary && !isPrimary && aligned && CTX_VALUE_GET_P_ID(*value, readNo - 1) == 0) {
            /* promote to primary alignment */
            isPrimary = true;
            wasPromoted = true;
        }
        if (!isPrimary && G.noSecondary)
            goto LOOP_END;

        getSpotGroup(rec, spotGroup);

        rc = BAM_AlignmentCGReadLength(rec, &readlen);
        if (rc != 0 && GetRCState(rc) != rcNotFound) {
            (void)LOGERR(klogErr, rc, "Invalid CG data");
            goto LOOP_END;
        }
        if (rc == 0) {
            hasCG = true;
            BAM_AlignmentGetCigarCount(rec, &opCount);
            rc = KDataBufferResize(&cigBuf, opCount * 2 + 5);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                goto LOOP_END;
            }
            rc = AlignmentRecordInit(&data, readlen);
            if (rc == 0)
                rc = KDataBufferResize(&buf, readlen);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                goto LOOP_END;
            }

            seqDNA = buf.base;
            qual = (uint8_t *)&seqDNA[readlen];

            rc = BAM_AlignmentGetCGSeqQual(rec, seqDNA, qual);
            if (rc == 0) {
                rc = BAM_AlignmentGetCGCigar(rec, cigBuf.base, cigBuf.elem_count, &opCount);
            }
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to read CG data");
                goto LOOP_END;
            }
            data.data.align_group.elements = 0;
            data.data.align_group.buffer = alignGroup;
            if (BAM_AlignmentGetCGAlignGroup(rec, alignGroup, sizeof(alignGroup), &alignGroupLen) == 0)
                data.data.align_group.elements = alignGroupLen;
        }
        else {
            /* normal flow i.e. NOT CG */
            uint32_t const *tmp;

            /* resize buffers */
            BAM_AlignmentGetReadLength(rec, &readlen);
            BAM_AlignmentGetRawCigar(rec, &tmp, &opCount);
            rc = KDataBufferResize(&cigBuf, opCount);
            assert(rc == 0);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                goto LOOP_END;
            }
            memcpy(cigBuf.base, tmp, opCount * sizeof(uint32_t));

            hardclipped = isHardClipped(opCount, cigBuf.base);
            if (hardclipped) {
                if (isPrimary && !wasPromoted) {
                    /* when we promote a secondary to primary and it is hardclipped, we want to "fix" it */
                    if (!G.acceptHardClip) {
                        rc = RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains hard clipped primary alignments", "file=%s", bamFile));
                        goto LOOP_END;
                    }
                }
                else if (!G.acceptHardClip) { /* convert to soft clip */
                    uint32_t *const cigar = cigBuf.base;
                    uint32_t const lOp = cigar[0];
                    uint32_t const rOp = cigar[opCount - 1];

                    lpad = (lOp & 0xF) == 5 ? (lOp >> 4) : 0;
                    rpad = (rOp & 0xF) == 5 ? (rOp >> 4) : 0;

                    if (lpad + rpad == 0) {
                        rc = RC(rcApp, rcFile, rcReading, rcData, rcInvalid);
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains invalid CIGAR", "file=%s", bamFile));
                        goto LOOP_END;
                    }
                    if (lpad != 0) {
                        uint32_t const new_lOp = (((uint32_t)lpad) << 4) | 4;
                        cigar[0] = new_lOp;
                    }
                    if (rpad != 0) {
                        uint32_t const new_rOp = (((uint32_t)rpad) << 4) | 4;
                        cigar[opCount - 1] = new_rOp;
                    }
                }
            }

            if (G.deferSecondary && !isPrimary) {
                /*** try to see if hard-clipped secondary alignment can be salvaged **/
                if (readlen + lpad + rpad < 256 && readlen + lpad + rpad < value->fragment_len[readNo - 1]) {
                    rc = KDataBufferResize(&cigBuf, opCount + 1);
                    assert(rc == 0);
                    if (rc) {
                        (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                        goto LOOP_END;
                    }
                    if (rpad > 0 && lpad == 0) {
                        uint32_t *const cigar = cigBuf.base;
                        lpad =  value->fragment_len[readNo - 1] - readlen - rpad;
                        memmove(cigar + 1, cigar, opCount * sizeof(*cigar));
                        cigar[0] = (uint32_t)((lpad << 4) | 4);
                    }
                    else {
                        uint32_t *const cigar = cigBuf.base;
                        rpad += value->fragment_len[readNo - 1] - readlen - lpad;
                        cigar[opCount] = (uint32_t)((rpad << 4) | 4);
                    }
                    opCount++;
                }
            }
            rc = AlignmentRecordInit(&data, readlen + lpad + rpad);
            assert(rc == 0);
            if (rc == 0)
                rc = KDataBufferResize(&buf, readlen + lpad + rpad);
            assert(rc == 0);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                goto LOOP_END;
            }

            seqDNA = buf.base;
            qual = (uint8_t *)&seqDNA[(readlen | csSeqLen) + lpad + rpad];
            memset(seqDNA, 'N', (readlen | csSeqLen) + lpad + rpad);
            memset(qual, 0, (readlen | csSeqLen) + lpad + rpad);

            BAM_AlignmentGetSequence(rec, seqDNA + lpad);
            if (G.useQUAL) {
                uint8_t const *squal;

                BAM_AlignmentGetQuality(rec, &squal);
                memcpy(qual + lpad, squal, readlen);
            }
            else {
                uint8_t const *squal;
                uint8_t qoffset = 0;
                unsigned i;

                rc = BAM_AlignmentGetQuality2(rec, &squal, &qoffset);
                if (rc) {
                    (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of original quality does not match sequence", "name=%s", name));
                    goto LOOP_END;
                }
                if (qoffset) {
                    for (i = 0; i != readlen; ++i)
                        qual[i + lpad] = squal[i] - qoffset;
                    QUAL_CHANGED_OQ;
                }
                else
                    memcpy(qual + lpad, squal, readlen);
            }
            readlen = readlen + lpad + rpad;
            data.data.align_group.elements = 0;
            data.data.align_group.buffer = alignGroup;
        }
        if (G.hasTI) {
            rc = BAM_AlignmentGetTI(rec, &ti);
            if (rc)
                ti = 0;
            rc = 0;
        }

        rc = KDataBufferResize(&seqBuffer, readlen);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
            goto LOOP_END;
        }
        rc = KDataBufferResize(&qualBuffer, readlen);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
            goto LOOP_END;
        }
        AR_REF_ORIENT(data) = (flags & BAMFlags_SelfIsReverse) == 0 ? false : true;

        rpos = -1;
        if (aligned) {
            BAM_AlignmentGetPosition(rec, &rpos);
            BAM_AlignmentGetRefSeqId(rec, &refSeqId);
            if (refSeqId != lastRefSeqId) {
                refSeq = NULL;
                BAM_FileGetRefSeqById(bam, refSeqId, &refSeq);
            }
        }

        revcmp = (isColorSpace && !aligned) ? false : AR_REF_ORIENT(data);
        (void)PLOGMSG(klogDebug, (klogDebug, "Read '$(name)' is $(or) at $(ref):$(pos)", "name=%s,or=%s,ref=%s,pos=%i", name, revcmp ? "reverse" : "forward", refSeq ? refSeq->name : "(none)", rpos));
        COPY_READ(seqBuffer.base, seqDNA, readlen, revcmp);
        COPY_QUAL(qualBuffer.base, qual, readlen, revcmp);

        AR_MAPQ(data) = GetMapQ(rec);
        if (!isPrimary && AR_MAPQ(data) < G.minMapQual)
            goto LOOP_END;

        if (aligned && align == NULL) {
            rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
            (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains aligned records", "file=%s", bamFile));
            goto LOOP_END;
        }
        while (aligned) {
            if (rpos >= 0 && refSeqId >= 0) {
                if (refSeqId == skipRefSeqID) {
                    DISCARD_SKIP_REFERENCE;
                    goto LOOP_END;
                }
                if (refSeqId == unmapRefSeqId) {
                    aligned = false;
                    UNALIGNED_UNALIGNED_REF;
                    break;
                }
                unmapRefSeqId = -1;
                if (refSeq == NULL) {
                    rc = SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "File '$(file)': Spot '$(name)' refers to an unknown Reference number $(refSeqId)", "file=%s,refSeqId=%i,name=%s", bamFile, (int)refSeqId, name));
                    rc = CheckLimitAndLogError();
                    DISCARD_UNKNOWN_REFERENCE;
                    goto LOOP_END;
                }
                else {
                    bool shouldUnmap = false;

                    if (G.refFilter && strcmp(G.refFilter, refSeq->name) != 0) {
                        (void)PLOGMSG(klogInfo, (klogInfo, "Skipping Reference '$(name)'", "name=%s", refSeq->name));
                        skipRefSeqID = refSeqId;
                        DISCARD_SKIP_REFERENCE;
                        goto LOOP_END;
                    }

                    rc = ReferenceSetFile(ref, refSeq->name, refSeq->length, refSeq->checksum, &shouldUnmap, &wasRenamed);
                    if (rc == 0) {
                        lastRefSeqId = refSeqId;
                        if (shouldUnmap) {
                            aligned = false;
                            unmapRefSeqId = refSeqId;
                            UNALIGNED_UNALIGNED_REF;
                        }
                        break;
                    }
                    if (GetRCObject(rc) == rcConstraint && GetRCState(rc) == rcViolated) {
                        int const level = G.limit2config ? klogWarn : klogErr;

                        (void)PLOGMSG(level, (level, "Could not find a Reference to match { name: '$(name)', length: $(rlen) }", "name=%s,rlen=%u", refSeq->name, (unsigned)refSeq->length));
                    }
                    else if (!G.limit2config)
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)': Spot '$(sname)' refers to an unknown Reference '$(rname)'", "file=%s,rname=%s,sname=%s", bamFile, refSeq->name, name));
                    if (G.limit2config) {
                        rc = 0;
                        UNALIGNED_UNKNOWN_REF;
                    }
                    goto LOOP_END;
                }
            }
            else if (refSeqId < 0) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' was marked aligned, but reference id = $(id) is invalid", "name=%.*s,id=%i", namelen, name, refSeqId));
                if ((rc = CheckLimitAndLogError()) != 0) goto LOOP_END;
                UNALIGNED_INVALID_REF;
            }
            else {
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' was marked aligned, but reference position = $(pos) is invalid", "name=%.*s,pos=%i", namelen, name, rpos));
                if ((rc = CheckLimitAndLogError()) != 0) goto LOOP_END;
                UNALIGNED_INVALID_REF_POS;
            }

            aligned = false;
        }
        if (!aligned && (G.refFilter != NULL || G.limit2config)) {
            assert(!"this shouldn't happen");
            goto LOOP_END;
        }

        AR_KEY(data) = keyId;
        AR_READNO(data) = readNo;

        if (wasInserted) {
            /* first time spot is seen */
            if (G.mode == mode_Remap) {
                (void)PLOGERR(klogErr, (klogErr, rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)' is a new spot, not a remapping",
                                         "name=%s", name));
                goto LOOP_END;
            }
            memset(value, 0, sizeof(*value));
            value->unmated = !mated;
            if (isPrimary || G.assembleWithSecondary || G.deferSecondary) {
                value->pcr_dup = (flags & BAMFlags_IsDuplicate) == 0 ? 0 : 1;
                value->platform = GetINSDCPlatform(bam, spotGroup);
                value->primary_is_set = 1;
            }
        }
        else if (isPrimary || G.assembleWithSecondary || G.deferSecondary) {
            /* other times */
            int o_pcr_dup = value->pcr_dup;
            int const n_pcr_dup = (flags & BAMFlags_IsDuplicate) == 0 ? 0 : 1;

            if (!value->primary_is_set) {
                o_pcr_dup = n_pcr_dup;
                value->primary_is_set = 1;
            }

            value->pcr_dup = o_pcr_dup & n_pcr_dup;
            if (o_pcr_dup != (o_pcr_dup & n_pcr_dup)) {
                FLAG_CHANGED_PCR_DUP;
            }
            if (mated && value->unmated) {
                (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen without mate info, now has mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError();
                DISCARD_BAD_FRAGMENT_INFO;
                goto LOOP_END;
            }
            else if (!mated && !value->unmated) {
                (void)PLOGERR(klogWarn, (klogWarn, SILENT_RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen with mate info, now has no mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError();
                DISCARD_BAD_FRAGMENT_INFO;
                goto LOOP_END;
            }
        }
        if (isPrimary) {
            switch (readNo) {
            case 1:
                if (CTX_VALUE_GET_P_ID(*value, 0) != 0) {
                    isPrimary = false;
                    FLAG_CHANGED_PRIMARY_DUP;
                }
                else if (aligned && value->unaligned_1) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Read 1 of spot '$(name)', which was unmapped, is now being mapped at position $(pos) on reference '$(ref)'; this alignment will be considered as secondary", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                    isPrimary = false;
                    FLAG_CHANGED_WAS_UNALIGNED;
                }
                break;
            case 2:
                if (CTX_VALUE_GET_P_ID(*value, 1) != 0) {
                    isPrimary = false;
                    FLAG_CHANGED_PRIMARY_DUP;
                }
                else if (aligned && value->unaligned_2) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Read 2 of spot '$(name)', which was unmapped, is now being mapped at position $(pos) on reference '$(ref)'; this alignment will be considered as secondary", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                    isPrimary = false;
                    FLAG_CHANGED_WAS_UNALIGNED;
                }
                break;
            default:
                break;
            }
        }
        if (hardclipped) {
            value->hardclipped = 1;
        }
#if 0 /** EY TO REVIEW **/
        if (!isPrimary && value->hardclipped) {
            DISCARD_HARDCLIP_SECONDARY;
            goto LOOP_END;
        }
#endif

        /* input is clean */
        ++recordsProcessed;

        data.isPrimary = isPrimary;
        if (aligned) {
            uint32_t matches = 0;
            uint32_t misses = 0;
            uint8_t rna_orient = ' ';

            FixOverhangingAlignment(&cigBuf, &opCount, rpos, refSeq->length, readlen);
            BAM_AlignmentGetRNAStrand(rec, &rna_orient);
            {
                int const intronType = rna_orient == '+' ? NCBI_align_ro_intron_plus :
                                       rna_orient == '-' ? NCBI_align_ro_intron_minus :
                                                   hasCG ? NCBI_align_ro_complete_genomics :
                                                           NCBI_align_ro_intron_unknown;
                rc = ReferenceRead(ref, &data, rpos, cigBuf.base, opCount, seqDNA, readlen, intronType, &matches, &misses);
            }
            if (rc == 0) {
                int const i = readNo - 1;
                int const clipped_rl = readlen < 255 ? readlen : 255;
                if (i >= 0 && i < 2) {
                    int const rl = value->fragment_len[i];

                    if (rl == 0)
                        value->fragment_len[i] = clipped_rl;
                    else if (rl != clipped_rl) {
                        if (isPrimary) {
                            rc = RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
                            (void)PLOGERR(klogErr, (klogErr, rc, "Primary alignment for '$(name)' has different length ($(len)) than previously recorded secondary alignment. Try to defer secondary alignment processing.",
                                                    "name=%s,len=%d", name, readlen));
                        }
                        else {
                            rc = SILENT_RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
                            (void)PLOGERR(klogWarn, (klogWarn, rc, "Secondary alignment for '$(name)' has different length ($(len)) than previously recorded primary alignment; discarding secondary alignment.",
                                                     "name=%s,len=%d", name, readlen));
                            DISCARD_BAD_SECONDARY;
                            rc = CheckLimitAndLogError();
                        }
                        goto LOOP_END;
                    }
                }
            }
            if (rc == 0 && (matches < G.minMatchCount || (matches == 0 && !G.acceptNoMatch))) {
                if (isPrimary) {
                    if (misses > matches) {
                        RecordNoMatch(name, refSeq->name, rpos);
                        rc = LogNoMatch(name, refSeq->name, (unsigned)rpos, (unsigned)matches);
                        if (rc)
                            goto LOOP_END;
                    }
                }
                else {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos); discarding secondary alignment",
                                             "name=%s,ref=%s,pos=%u,count=%u", name, refSeq->name, (unsigned)rpos, (unsigned)matches));
                    DISCARD_BAD_SECONDARY;
                    rc = 0;
                    goto LOOP_END;
                }
            }
            if (rc) {
                aligned = false;

                if (((int)GetRCObject(rc)) == ((int)rcData) && GetRCState(rc) == rcNotAvailable) {
                    /* because of code above converting hard clips to soft clips, this should be unreachable */
                    abort();
                }
                else if (((int)GetRCObject(rc)) == ((int)rcData)) {
                    UNALIGNED_INVALID_INFO;
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': bad alignment to reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                    /* Data errors may get reset; alignment will be unmapped at any rate */
                    rc = CheckLimitAndLogError();
                }
                else {
                    UNALIGNED_INVALID_REF_POS;
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': error reading reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                    rc = CheckLimitAndLogError();
                }
                if (rc) goto LOOP_END;
            }
        }

        if (!aligned && isPrimary) {
            switch (readNo) {
            case 1:
                value->unaligned_1 = 1;
                break;
            case 2:
                value->unaligned_2 = 1;
                break;
            default:
                break;
            }
        }
        if (isPrimary) {
            switch (readNo) {
            case 1:
                if (CTX_VALUE_GET_P_ID(*value, 0) == 0 && aligned) {
                    data.alignId = ++ctx->primaryId;
                    CTX_VALUE_SET_P_ID(*value, 0, data.alignId);
                }
                break;
            case 2:
                if (CTX_VALUE_GET_P_ID(*value, 1) == 0 && aligned) {
                    data.alignId = ++ctx->primaryId;
                    CTX_VALUE_SET_P_ID(*value, 1, data.alignId);
                }
                break;
            default:
                break;
            }
        }
        if (G.mode == mode_Archive)
            goto WRITE_SEQUENCE;
        else
            goto WRITE_ALIGNMENT;
        if (0) {
WRITE_SEQUENCE:
            if (barCode) {
                if (spotGroup[0] != '\0' && value->platform == SRA_PLATFORM_UNDEFINED) {
                    /* don't use bar code */
                }
                else {
                    unsigned const sglen = strlen(barCode);
                    if (sglen + 1 < sizeof(spotGroup))
                        memcpy(spotGroup, barCode, sglen + 1);
                }
            }
            if (mated) {
                int64_t const spotId = CTX_VALUE_GET_S_ID(*value);
                uint32_t const fragmentId = value->fragmentId;
                bool const spotHasBeenWritten = (spotId != 0);
                bool const spotHasFragmentInfo = (fragmentId != 0);
                bool const spotIsFirstSeen = (spotHasBeenWritten || spotHasFragmentInfo) ? false : true;

                if (spotHasBeenWritten) {
                    /* do nothing */
                }
                else if (spotIsFirstSeen) {
                    /* start spot assembly */
                    unsigned sz;
                    FragmentInfo fi;
                    int32_t mate_refSeqId = -1;
                    int64_t pnext = 0;

                    if (!isPrimary) {
                        if ( (!G.assembleWithSecondary || hardclipped) && !G.deferSecondary ) { 
                            goto WRITE_ALIGNMENT;
                        }
                        (void)PLOGMSG(klogDebug, (klogDebug, "Spot '$(name)' (id $(id)) is being constructed from secondary alignment information", "id=%lx,name=%s", keyId, name));
                    }
                    memset(&fi, 0, sizeof(fi));

                    fi.aligned = isPrimary ? aligned : 0;
                    fi.ti = ti;
                    fi.orientation = AR_REF_ORIENT(data);
                    fi.readNo = readNo;
                    fi.sglen = strlen(spotGroup);
                    fi.lglen = strlen(linkageGroup);

                    fi.readlen = readlen;
                    fi.cskey = cskey;
                    fi.is_bad = (flags & BAMFlags_IsLowQuality) != 0;
                    sz = sizeof(fi) + 2*fi.readlen + fi.sglen + fi.lglen;
                    if (align) {
                        BAM_AlignmentGetMateRefSeqId(rec, &mate_refSeqId);
                        BAM_AlignmentGetMatePosition(rec, &pnext);
                    }
                    if(align && mate_refSeqId == refSeqId && pnext > 0 && pnext!=rpos /*** weird case in some bams**/){
                        rc = MemBankAlloc(ctx->frags, &value->fragmentId, sz, 0, false);
                        fcountBoth++;
                    } else {
                        rc = MemBankAlloc(ctx->frags, &value->fragmentId, sz, 0, true);
                        fcountOne++;
                    }
                    if (rc) {
                        (void)LOGERR(klogErr, rc, "KMemBankAlloc failed");
                        goto LOOP_END;
                    }
                    /*printf("IN:%10d\tcnt2=%ld\tcnt1=%ld\n",value->fragmentId,fcountBoth,fcountOne);*/
                    
                    rc = KDataBufferResize(&fragBuf, sz);
                    if (rc) {
                        (void)LOGERR(klogErr, rc, "Failed to resize fragment buffer");
                        goto LOOP_END;
                    }
                    {{
                        uint8_t *dst = (uint8_t*) fragBuf.base;
                        
                        memcpy(dst,&fi,sizeof(fi));
                        dst += sizeof(fi);
                        memcpy(dst, seqBuffer.base, readlen);
                        dst += readlen;
                        memcpy(dst, qualBuffer.base, readlen);
                        dst += fi.readlen;
                        memcpy(dst, spotGroup, fi.sglen);
                        dst += fi.sglen;
                        memcpy(dst, linkageGroup, fi.lglen);
                        dst += fi.lglen;
                    }}
                    rc = MemBankWrite(ctx->frags, value->fragmentId, 0, fragBuf.base, sz, &rsize);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankWrite failed writing fragment $(id)", "id=%u", value->fragmentId));
                        goto LOOP_END;
                    }
                    if (revcmp) {
                        QUAL_CHANGED_REVERSED;
                        SEQ__CHANGED_REV_COMP;
                    }
                }
                else if (spotHasFragmentInfo) {
                    /* continue spot assembly */
                    FragmentInfo *fip;
                    {
                        size_t size1;
                        size_t size2;
                        
                        rc = MemBankSize(ctx->frags, fragmentId, &size1);
                        if (rc) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankSize failed on fragment $(id)", "id=%u", fragmentId));
                            goto LOOP_END;
                        }
                        
                        rc = KDataBufferResize(&fragBuf, size1);
                        fip = (FragmentInfo *)fragBuf.base;
                        if (rc) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to resize fragment buffer", ""));
                            goto LOOP_END;
                        }
                        
                        rc = MemBankRead(ctx->frags, fragmentId, 0, fragBuf.base, size1, &size2);
                        if (rc) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankRead failed on fragment $(id)", "id=%u", fragmentId));
                            goto LOOP_END;
                        }
                        assert(size1 == size2);
                    }
                    if (readNo == fip->readNo) {
                        /* is a repeat of the same read; do nothing */
                    }
                    else {
                        /* mate found; finish spot assembly */
                        unsigned read1 = 0;
                        unsigned read2 = 1;
                        char const *const seq1 = (void *)&fip[1];
                        char const *const qual1 = (void *)(seq1 + fip->readlen);
                        char const *const sg1 = (void *)(qual1 + fip->readlen);
                        char const *const bx1 = (void *)(sg1 + fip->sglen);

                        if (!isPrimary) {
                            if ((!G.assembleWithSecondary || hardclipped) && !G.deferSecondary ) {
                                goto WRITE_ALIGNMENT;
                            }
                            (void)PLOGMSG(klogDebug, (klogDebug, "Spot '$(name)' (id $(id)) is being constructed from secondary alignment information", "id=%lx,name=%s", keyId, name));
                        }
                        rc = KDataBufferResize(&seqBuffer, readlen + fip->readlen);
                        if (rc) {
                            (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                            goto LOOP_END;
                        }
                        rc = KDataBufferResize(&qualBuffer, readlen + fip->readlen);
                        if (rc) {
                            (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                            goto LOOP_END;
                        }
                        if (readNo < fip->readNo) {
                            read1 = 1;
                            read2 = 0;
                        }

                        memset(&srecStorage, 0, sizeof(srecStorage));
                        srec.numreads = 2;
                        srec.readLen[read1] = fip->readlen;
                        srec.readLen[read2] = readlen;
                        srec.readStart[1] = srec.readLen[0];
                        {
                            char const *const s1 = seq1;
                            char const *const s2 = seqBuffer.base;
                            char *const d = seqBuffer.base;
                            char *const d1 = d + srec.readStart[read1];
                            char *const d2 = d + srec.readStart[read2];

                            srec.seq = seqBuffer.base;
                            if (d2 != s2) {
                                memmove(d2, s2, readlen);
                            }
                            memcpy(d1, s1, fip->readlen);
                        }
                        {
                            char const *const s1 = qual1;
                            char const *const s2 = qualBuffer.base;
                            char *const d = qualBuffer.base;
                            char *const d1 = d + srec.readStart[read1];
                            char *const d2 = d + srec.readStart[read2];

                            srec.qual = qualBuffer.base;
                            if (d2 != s2) {
                                memmove(d2, s2, readlen);
                            }
                            memcpy(d1, s1, fip->readlen);
                        }

                        srec.ti[read1] = fip->ti;
                        srec.ti[read2] = ti;

                        srec.aligned[read1] = fip->aligned;
                        srec.aligned[read2] = isPrimary ? aligned : 0;

                        srec.is_bad[read1] = fip->is_bad;
                        srec.is_bad[read2] = (flags & BAMFlags_IsLowQuality) != 0;

                        srec.orientation[read1] = fip->orientation;
                        srec.orientation[read2] = AR_REF_ORIENT(data);

                        srec.cskey[read1] = fip->cskey;
                        srec.cskey[read2] = cskey;

                        srec.keyId = keyId;

                        srec.spotGroup = sg1;
                        srec.spotGroupLen = fip->sglen;

                        srec.linkageGroup = bx1;
                        srec.linkageGroupLen = fip->lglen;

                        srec.seq = seqBuffer.base;
                        srec.qual = qualBuffer.base;

                        rc = SequenceWriteRecord(seq, &srec, isColorSpace, value->pcr_dup, value->platform);
                        if (rc) {
                            (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                            goto LOOP_END;
                        }
                        CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
                        if(fragmentId & 1){
                            fcountOne--;
                        } else {
                            fcountBoth--;
                        }
                        /*	printf("OUT:%9d\tcnt2=%ld\tcnt1=%ld\n",fragmentId,fcountBoth,fcountOne);*/
                        rc = MemBankFree(ctx->frags, fragmentId);
                        if (rc) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankFree failed on fragment $(id)", "id=%u", fragmentId));
                            goto LOOP_END;
                        }
                        value->fragmentId = 0;
                        if (revcmp) {
                            QUAL_CHANGED_REVERSED;
                            SEQ__CHANGED_REV_COMP;
                        }
                        if (value->pcr_dup && (srec.is_bad[0] || srec.is_bad[1])) {
                            FLAG_CHANGED_400_AND_200;
                            filterFlagConflictRecords++;
                            if (filterFlagConflictRecords < MAX_WARNINGS_FLAG_CONFLICT) {
                                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                            }
                            else if (filterFlagConflictRecords == MAX_WARNINGS_FLAG_CONFLICT) {
                                (void)PLOGMSG(klogWarn, (klogWarn, "Last reported warning: Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                            }
                        }
                    }
                }
                else {
                    (void)PLOGMSG(klogErr, (klogErr, "Spot '$(name)' has caused the loader to enter an illogical state", "name=%s", name));
                    assert("this should never happen");
                }
            }
            else if (CTX_VALUE_GET_S_ID(*value) == 0) {
                /* new unmated fragment - no spot assembly */
                if (!isPrimary) {
                    if ((!G.assembleWithSecondary || hardclipped) && !G.deferSecondary ) {
                        goto WRITE_ALIGNMENT;
                    }
                    (void)PLOGMSG(klogDebug, (klogDebug, "Spot '$(name)' (id $(id)) is being constructed from secondary alignment information", "id=%lx,name=%s", keyId, name));
                }
                memset(&srecStorage, 0, sizeof(srecStorage));
                srec.numreads = 1;

                srec.readLen[0] = readlen;
                srec.ti[0] = ti;
                srec.aligned[0] = isPrimary ? aligned : 0;
                srec.is_bad[0] = (flags & BAMFlags_IsLowQuality) != 0;
                srec.orientation[0] = AR_REF_ORIENT(data);
                srec.cskey[0] = cskey;

                srec.keyId = keyId;

                srec.spotGroup = spotGroup;
                srec.spotGroupLen = strlen(spotGroup);

                srec.linkageGroup = linkageGroup;
                srec.linkageGroupLen = strlen(linkageGroup);

                srec.seq = seqBuffer.base;
                srec.qual = qualBuffer.base;

                rc = SequenceWriteRecord(seq, &srec, isColorSpace, value->pcr_dup, value->platform);
                if (rc) {
                    (void)PLOGERR(klogErr, (klogErr, rc, "SequenceWriteRecord failed", ""));
                    goto LOOP_END;
                }
                CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
                value->fragmentId = 0;
                if (value->pcr_dup && srec.is_bad[0]) {
                    FLAG_CHANGED_400_AND_200;
                    filterFlagConflictRecords++;
                    if (filterFlagConflictRecords < MAX_WARNINGS_FLAG_CONFLICT) {
                        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                    }
                    else if (filterFlagConflictRecords == MAX_WARNINGS_FLAG_CONFLICT) {
                        (void)PLOGMSG(klogWarn, (klogWarn, "Last reported warning: Spot '$(name)': both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "name=%s", name));
                    }
                }
                if (revcmp) {
                    QUAL_CHANGED_REVERSED;
                    SEQ__CHANGED_REV_COMP;
                }
            }
        }
WRITE_ALIGNMENT:
        if (aligned) {
            if (mated && !isPrimary) {
                int32_t bam_mrid;
                int64_t mpos;
                int64_t mrid = 0;
                int64_t tlen;

                BAM_AlignmentGetMatePosition(rec, &mpos);
                BAM_AlignmentGetMateRefSeqId(rec, &bam_mrid);
                BAM_AlignmentGetInsertSize(rec, &tlen);

                if (mpos >= 0 && bam_mrid >= 0 && tlen != 0) {
                    BAMRefSeq const *mref;

                    BAM_FileGetRefSeq(bam, bam_mrid, &mref);
                    if (mref) {
                        rc_t rc_temp = ReferenceGet1stRow(ref, &mrid, mref->name);
                        if (rc_temp == 0) {
                            data.mate_ref_pos = mpos;
                            data.template_len = tlen;
                            data.mate_ref_orientation = (flags & BAMFlags_MateIsReverse) ? 1 : 0;
                        }
                        else {
                            (void)PLOGERR(klogWarn, (klogWarn, rc_temp, "Failed to get refID for $(name)", "name=%s", mref->name));
                            MATE_INFO_LOST_UNKNOWN_REF;
                        }
                        data.mate_ref_id = mrid;
                    }
                    else {
                        MATE_INFO_LOST_INVALID;
                    }
                }
                else if (mpos >= 0 || bam_mrid >= 0 || tlen != 0) {
                    MATE_INFO_LOST_MISSING;
                }
            }

            if (wasRenamed) {
                RENAMED_REFERENCE;
            }
            if (value->alignmentCount[readNo - 1] < 254)
                ++value->alignmentCount[readNo - 1];
            ++ctx->alignCount;

            assert(keyId >> 32 < ctx->keyToID.key2id_count);
            assert((uint32_t)keyId < ctx->keyToID.idCount[keyId >> 32]);

            if (linkageGroup[0] != '\0') {
                AR_LINKAGE_GROUP(data).elements = strlen(linkageGroup);
                AR_LINKAGE_GROUP(data).buffer = linkageGroup;
            }

            rc = AlignmentWriteRecord(align, &data);
            if (rc == 0) {
                if (!isPrimary)
                    data.alignId = ++ctx->secondId;

                rc = ReferenceAddAlignId(ref, data.alignId, isPrimary);
                if (rc) {
                    (void)PLOGERR(klogErr, (klogErr, rc, "ReferenceAddAlignId failed", ""));
                }
                else {
                    *had_alignments = true;
                }
            }
            else {
                (void)PLOGERR(klogErr, (klogErr, rc, "AlignmentWriteRecord failed", ""));
            }
        }
        /**************************************************************/

    LOOP_END:
        BAM_AlignmentRelease(rec);
        ++reccount;
        if (G.maxAlignCount > 0 && reccount >= G.maxAlignCount)
            break;
        if (rc == 0)
            *had_sequences = true;
    }
    if (rc) {
        if (   (GetRCModule(rc) == rcCont && (int)GetRCObject(rc) == rcData && GetRCState(rc) == rcDone)
            || (GetRCModule(rc) == rcAlign && GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound))
        {
            (void)PLOGMSG(klogInfo, (klogInfo, "EOF '$(file)'; processed $(proc)", "file=%s,read=%lu,proc=%lu", bamFile, (unsigned long)recordsRead, (unsigned long)recordsProcessed));
            rc = 0;
        }
        else {
            (void)PLOGERR(klogInfo, (klogInfo, rc, "Error '$(file)'; read $(read); processed $(proc)", "file=%s,read=%lu,proc=%lu", bamFile, (unsigned long)recordsRead, (unsigned long)recordsProcessed));
        }
    }
    if (filterFlagConflictRecords > 0) {
        (void)PLOGMSG(klogWarn, (klogWarn, "$(cnt1) out of $(cnt2) records contained warning : both 0x400 and 0x200 flag bits set, only 0x400 will be saved", "cnt1=%lu,cnt2=%lu", filterFlagConflictRecords,recordsProcessed));
    }
    if (rc == 0 && recordsProcessed == 0) {
        (void)LOGMSG(klogWarn, (G.limit2config || G.refFilter != NULL) ?
                     "All records from the file were filtered out" :
                     "The file contained no records that were processed.");
        rc = RC(rcAlign, rcFile, rcReading, rcData, rcEmpty);
    }

    BAM_FileRelease(bam);
    MMArrayLock(ctx->id2value);
    KDataBufferWhack(&buf);
    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&cigBuf);
    KDataBufferWhack(&data.buffer);
    return rc;
}

static rc_t WriteSoloFragments(context_t *ctx, Sequence *seq)
{
    uint32_t i;
    unsigned j;
    uint64_t idCount = 0;
    rc_t rc;
    KDataBuffer fragBuf;
    SequenceRecordStorage srecStorage;
    SequenceRecord srec;

    ++ctx->pass;
    memset(&srec, 0, sizeof(srec));

    srec.ti             = srecStorage.ti;
    srec.readStart      = srecStorage.readStart;
    srec.readLen        = srecStorage.readLen;
    srec.orientation    = srecStorage.orientation;
    srec.is_bad         = srecStorage.is_bad;
    srec.alignmentCount = srecStorage.alignmentCount;
    srec.aligned        = srecStorage.aligned;
    srec.cskey          = srecStorage. cskey;

    rc = KDataBufferMake(&fragBuf, 8, 0);
    if (rc) {
        (void)LOGERR(klogErr, rc, "KDataBufferMake failed");
        return rc;
    }
    for (idCount = 0, j = 0; j < ctx->keyToID.key2id_count; ++j) {
        idCount += ctx->keyToID.idCount[j];
    }
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], idCount);

    for (idCount = 0, j = 0; j < ctx->keyToID.key2id_count; ++j) {
        for (i = 0; i != ctx->keyToID.idCount[j]; ++i, ++idCount) {
            uint64_t const keyId = ((uint64_t)j << 32) | i;
            ctx_value_t *value;
            size_t rsize;
            size_t sz;
            char const *src;
            FragmentInfo const *fip;

            rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
            if (rc)
                break;
            KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
            if (value->fragmentId == 0)
                continue;

            rc = MemBankSize(ctx->frags, value->fragmentId, &sz);
            if (rc) {
                (void)LOGERR(klogErr, rc, "KMemBankSize failed");
                break;
            }
            rc = KDataBufferResize(&fragBuf, (size_t)sz);
            if (rc) {
                (void)LOGERR(klogErr, rc, "KDataBufferResize failed");
                break;
            }
            rc = MemBankRead(ctx->frags, value->fragmentId, 0, fragBuf.base, sz, &rsize);
            if (rc) {
                (void)LOGERR(klogErr, rc, "KMemBankRead failed");
                break;
            }
            assert( rsize == sz );

            fip = fragBuf.base;
            src = (char const *)&fip[1];

            memset(&srecStorage, 0, sizeof(srecStorage));
            if (value->unmated) {
                srec.numreads = 1;
                srec.readLen[0] = fip->readlen;
                srec.ti[0] = fip->ti;
                srec.aligned[0] = fip->aligned;
                srec.is_bad[0] = fip->is_bad;
                srec.orientation[0] = fip->orientation;
                srec.cskey[0] = fip->cskey;
            }
            else {
                unsigned const read = ((fip->aligned && CTX_VALUE_GET_P_ID(*value, 0) == 0) || value->unaligned_2) ? 1 : 0;

                srec.numreads = 2;
                srec.readLen[read] = fip->readlen;
                srec.readStart[1] = srec.readLen[0];
                srec.ti[read] = fip->ti;
                srec.aligned[read] = fip->aligned;
                srec.is_bad[read] = fip->is_bad;
                srec.orientation[read] = fip->orientation;
                srec.cskey[0] = srec.cskey[1] = 'N';
                srec.cskey[read] = fip->cskey;
            }
            srec.seq = (char *)src;
            srec.qual = (uint8_t *)(src + fip->readlen);
            srec.spotGroup = (char *)(src + 2 * fip->readlen);
            srec.spotGroupLen = fip->sglen;
            srec.linkageGroup = (char *)(src + 2 * fip->readlen * fip->sglen);
            srec.linkageGroupLen = fip->lglen;
            srec.keyId = keyId;
            rc = SequenceWriteRecord(seq, &srec, ctx->isColorSpace, value->pcr_dup, value->platform);
            if (rc) {
                (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                break;
            }
            /*rc = KMemBankFree(frags, id);*/
            CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
        }
    }
    MMArrayLock(ctx->id2value);
    KDataBufferWhack(&fragBuf);
    return rc;
}

static rc_t SequenceUpdateAlignInfo(context_t *ctx, Sequence *seq)
{
    rc_t rc = 0;
    uint64_t row;
    uint64_t keyId;

    ++ctx->pass;
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->spotId + 1);

    for (row = 1; row <= ctx->spotId; ++row) {
        ctx_value_t *value;

        rc = SequenceReadKey(seq, row, &keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to get key for row $(row)", "row=%u", (unsigned)row));
            break;
        }
        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read info for row $(row), index $(idx)", "row=%u,idx=%u", (unsigned)row, (unsigned)keyId));
            break;
        }
        if (G.mode == mode_Remap) {
            CTX_VALUE_SET_S_ID(*value, row);
        }
        if (row != CTX_VALUE_GET_S_ID(*value)) {
            rc = RC(rcApp, rcTable, rcWriting, rcData, rcUnexpected);
            (void)PLOGMSG(klogErr, (klogErr, "Unexpected spot id $(spotId) for row $(row), index $(idx)", "spotId=%u,row=%u,idx=%u", (unsigned)CTX_VALUE_GET_S_ID(*value), (unsigned)row, (unsigned)keyId));
            break;
        }
        {{
            int64_t primaryId[2];
            int const logLevel = klogWarn; /*G.assembleWithSecondary ? klogWarn : klogErr;*/

            primaryId[0] = CTX_VALUE_GET_P_ID(*value, 0);
            primaryId[1] = CTX_VALUE_GET_P_ID(*value, 1);

            if (primaryId[0] == 0 && value->alignmentCount[0] != 0) {
                rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                (void)PLOGERR(logLevel, (logLevel, rc, "Spot id $(id) read 1 never had a primary alignment", "id=%lx", keyId));
            }
            if (!value->unmated && primaryId[1] == 0 && value->alignmentCount[1] != 0) {
                rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                (void)PLOGERR(logLevel, (logLevel, rc, "Spot id $(id) read 2 never had a primary alignment", "id=%lx", keyId));
            }
            if (rc != 0 && logLevel == klogErr)
                break;

            rc = SequenceUpdateAlignData(seq, row, value->unmated ? 1 : 2,
                                         primaryId,
                                         value->alignmentCount);
        }}
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failed updating Alignment data in sequence table");
            break;
        }
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
    }
    MMArrayLock(ctx->id2value);
    return rc;
}

static rc_t AlignmentUpdateSpotInfo(context_t *ctx, Alignment *align)
{
    rc_t rc;
    uint64_t keyId;

    ++ctx->pass;

    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->alignCount);

    rc = AlignmentStartUpdatingSpotIds(align);
    while (rc == 0 && (rc = Quitting()) == 0) {
        ctx_value_t *value;

        rc = AlignmentGetSpotKey(align, &keyId);
        if (rc) {
            if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                rc = 0;
            break;
        }
        assert(keyId >> 32 < ctx->keyToID.key2id_count);
        assert((uint32_t)keyId < ctx->keyToID.idCount[keyId >> 32]);
        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc == 0) {
            int64_t const spotId = CTX_VALUE_GET_S_ID(*value);

            if (spotId == 0) {
                rc = RC(rcApp, rcTable, rcWriting, rcConstraint, rcViolated);
                (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(id)' was never assigned a spot id, probably has no primary alignments", "id=%lx", keyId));
                break;
            }
            rc = AlignmentWriteSpotId(align, spotId);
        }
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
    }
    MMArrayLock(ctx->id2value);
    return rc;
}


static rc_t ArchiveBAM(VDBManager *mgr, VDatabase *db,
                       unsigned bamFiles, char const *bamFile[],
                       unsigned seqFiles, char const *seqFile[],
                       bool *has_alignments,
                       bool continuing)
{
    rc_t rc = 0;
    rc_t rc2;
    Reference ref;
    Sequence seq;
    Alignment *align;
    static context_t *ctx = &GlobalContext;
    bool has_sequences = false;
    unsigned i;

    *has_alignments = false;
    rc = ReferenceInit(&ref, mgr, db);
    if (rc)
        return rc;

    if (G.onlyVerifyReferences) {
        for (i = 0; i < bamFiles && rc == 0; ++i) {
            rc = ProcessBAM(bamFile[i], NULL, db, &ref, NULL, NULL, NULL, NULL);
        }
        ReferenceWhack(&ref, false);
        return rc;
    }
    SequenceInit(&seq, db);
    align = AlignmentMake(db);

    rc = SetupContext(ctx, bamFiles + seqFiles);
    if (rc)
        return rc;

    ctx->pass = 1;
    for (i = 0; i < bamFiles && rc == 0; ++i) {
        bool this_has_alignments = false;
        bool this_has_sequences = false;

        rc = ProcessBAM(bamFile[i], ctx, db, &ref, &seq, align, &this_has_alignments, &this_has_sequences);
        *has_alignments |= this_has_alignments;
        has_sequences |= this_has_sequences;
    }
    for (i = 0; i < seqFiles && rc == 0; ++i) {
        bool this_has_alignments = false;
        bool this_has_sequences = false;

        rc = ProcessBAM(seqFile[i], ctx, db, &ref, &seq, align, &this_has_alignments, &this_has_sequences);
        *has_alignments |= this_has_alignments;
        has_sequences |= this_has_sequences;
    }
    if (!continuing) {
/*** No longer need memory for key2id ***/
        for (i = 0; i != ctx->keyToID.key2id_count; ++i) {
            KBTreeDropBacking(ctx->keyToID.key2id[i]);
            KBTreeRelease(ctx->keyToID.key2id[i]);
            ctx->keyToID.key2id[i] = NULL;
        }
        free(ctx->keyToID.key2id_names);
        ctx->keyToID.key2id_names = NULL;
/*******************/
    }

    if (has_sequences) {
        if (rc == 0 && (rc = Quitting()) == 0) {
            if (G.mode == mode_Archive) {
                (void)LOGMSG(klogInfo, "Writing unpaired sequences");
                rc = WriteSoloFragments(ctx, &seq);
                ContextReleaseMemBank(ctx);
            }
            if (rc == 0) {
                rc = SequenceDoneWriting(&seq);
                if (rc == 0) {
                    (void)LOGMSG(klogInfo, "Updating sequence alignment info");
                    rc = SequenceUpdateAlignInfo(ctx, &seq);
                }
            }
        }
    }

    if (*has_alignments && rc == 0 && (rc = Quitting()) == 0) {
        (void)LOGMSG(klogInfo, "Writing alignment spot ids");
        rc = AlignmentUpdateSpotInfo(ctx, align);
    }
    rc2 = AlignmentWhack(align, *has_alignments && rc == 0 && (rc = Quitting()) == 0);
    if (rc == 0)
        rc = rc2;

    rc2 = ReferenceWhack(&ref, *has_alignments && rc == 0 && (rc = Quitting()) == 0);
    if (rc == 0)
        rc = rc2;

    SequenceWhack(&seq, rc == 0);

    ContextRelease(ctx, continuing);

    if (rc == 0) {
        (void)LOGMSG(klogInfo, "Successfully loaded all files");
    }
    return rc;
}

rc_t WriteLoaderSignature(KMetadata *meta, char const progName[])
{
    KMDataNode *node;
    rc_t rc = KMetadataOpenNodeUpdate(meta, &node, "/");

    if (rc == 0) {
        rc = KLoaderMeta_Write(node, progName, __DATE__, "BAM", KAppVersion());
        KMDataNodeRelease(node);
    }
    if (rc) {
        (void)LOGERR(klogErr, rc, "Cannot update loader meta");
    }
    return rc;
}

rc_t OpenPath(char const path[], KDirectory **dir)
{
    KDirectory *p;
    rc_t rc = KDirectoryNativeDir(&p);

    if (rc == 0) {
        rc = KDirectoryOpenDirUpdate(p, dir, false, "%s", path);
        KDirectoryRelease(p);
    }
    return rc;
}

static
rc_t ConvertDatabaseToUnmapped(VDatabase *db)
{
    VTable* tbl;
    rc_t rc = VDatabaseOpenTableUpdate(db, &tbl, "SEQUENCE");
    if (rc == 0)
    {
        VTableRenameColumn(tbl, false, "CMP_ALTREAD", "ALTREAD");
        VTableRenameColumn(tbl, false, "CMP_READ", "READ");
        VTableRenameColumn(tbl, false, "CMP_ALTCSREAD", "ALTCSREAD");
        VTableRenameColumn(tbl, false, "CMP_CSREAD", "CSREAD");
        rc = VTableRelease(tbl);
    }
    return rc;
}

rc_t run(char const progName[],
         unsigned bamFiles, char const *bamFile[],
         unsigned seqFiles, char const *seqFile[],
         bool continuing)
{
    VDBManager *mgr;
    rc_t rc;
    rc_t rc2;
    char const *db_type = G.expectUnsorted ? "NCBI:align:db:alignment_unsorted" : "NCBI:align:db:alignment_sorted";

    rc = VDBManagerMakeUpdate(&mgr, NULL);
    if (rc) {
        (void)LOGERR (klogErr, rc, "failed to create VDB Manager!");
    }
    else {
        bool has_alignments = false;

        /* VDBManagerDisableFlushThread(mgr); */
        rc = VDBManagerDisablePagemapThread(mgr);
        if (rc == 0)
        {
            if (G.onlyVerifyReferences) {
                rc = ArchiveBAM(mgr, NULL, bamFiles, bamFile, 0, NULL, &has_alignments, continuing);
            }
            else {
                VSchema *schema;

                rc = VDBManagerMakeSchema(mgr, &schema);
                if (rc) {
                    (void)LOGERR (klogErr, rc, "failed to create schema");
                }
                else {
                    (void)(rc = VSchemaAddIncludePath(schema, "%s", G.schemaIncludePath));
                    rc = VSchemaParseFile(schema, "%s", G.schemaPath);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "failed to parse schema file $(file)", "file=%s", G.schemaPath));
                    }
                    else {
                        VDatabase *db;

                        rc = VDBManagerCreateDB(mgr, &db, schema, db_type,
                                                kcmInit + kcmMD5, "%s", G.outpath);
                        VSchemaRelease(schema);
                        if (rc == 0) {
                            rc = ArchiveBAM(mgr, db, bamFiles, bamFile, seqFiles, seqFile, &has_alignments, continuing);
                            if (rc == 0)
                                PrintChangeReport();
                            if (rc == 0 && !has_alignments) {
                                rc = ConvertDatabaseToUnmapped(db);
                            }
                            else if (rc == 0 && lmc != NULL) {
                                VTable *tbl = NULL;
                                KTable *ktbl = NULL;
                                KMetadata *meta = NULL;
                                KMDataNode *node = NULL;

                                VDatabaseOpenTableUpdate(db, &tbl, "REFERENCE");
                                VTableOpenKTableUpdate(tbl, &ktbl);
                                VTableRelease(tbl);

                                KTableOpenMetadataUpdate(ktbl, &meta);
                                KTableRelease(ktbl);

                                KMetadataOpenNodeUpdate(meta, &node, "LOW_MATCH_COUNT");
                                KMetadataRelease(meta);

                                RecordLowMatchCounts(node);

                                KMDataNodeRelease(node);

                                LowMatchCounterFree(lmc);
                                lmc = NULL;
                            }
                            VDatabaseRelease(db);

                            if (rc == 0 && G.globalMode == mode_Remap && !continuing) {
                                VTable *tbl = NULL;

                                VDBManagerOpenDBUpdate(mgr, &db, NULL, G.firstOut);
                                VDatabaseOpenTableUpdate(db, &tbl, "SEQUENCE");
                                VDatabaseRelease(db);
                                VTableDropColumn(tbl, "TMP_KEY_ID");
                                VTableDropColumn(tbl, "READ");
                                VTableDropColumn(tbl, "ALTREAD");
                                VTableRelease(tbl);
                            }

                            if (rc == 0) {
                                KMetadata *meta = NULL;

                                {
                                    KDBManager *kmgr = NULL;

                                    rc = VDBManagerOpenKDBManagerUpdate(mgr, &kmgr);
                                    if (rc == 0) {
                                        KDatabase *kdb;

                                        rc = KDBManagerOpenDBUpdate(kmgr, &kdb, "%s", G.outpath);
                                        if (rc == 0) {
                                            rc = KDatabaseOpenMetadataUpdate(kdb, &meta);
                                            KDatabaseRelease(kdb);
                                        }
                                        KDBManagerRelease(kmgr);
                                    }
                                }
                                if (rc == 0) {
                                    rc = WriteLoaderSignature(meta, progName);
                                    if (rc == 0) {
                                        KMDataNode *changes = NULL;
                                        
                                        rc = KMetadataOpenNodeUpdate(meta, &changes, "CHANGES");
                                        if (rc == 0)
                                            RecordChanges(changes, "CHANGE");
                                        KMDataNodeRelease(changes);
                                    }
                                    KMetadataRelease(meta);
                                }
                            }
                        }
                    }
                }
            }
        }
        rc2 = VDBManagerRelease(mgr);
        if (rc2)
            (void)LOGERR(klogWarn, rc2, "Failed to release VDB Manager");
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}
