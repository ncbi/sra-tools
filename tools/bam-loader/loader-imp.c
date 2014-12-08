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

#include <align/bam.h>

#include "Globals.h"
#include "sequence-writer.h"
#include "reference-writer.h"
#include "alignment-writer.h"
#include "mem-bank.h"

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
    uint8_t  platform;
    uint8_t  pId_ext[2];
    uint8_t  spotId_ext;
    uint8_t  alignmentCount[2]; /* 0..254; 254: saturated max; 255: special meaning "too many" */
    uint8_t  unmated: 1,
             pcr_dup: 1,
             has_a_read: 1,
             unaligned_1: 1,
             unaligned_2: 1;
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
    uint8_t  otherReadNo;
    uint8_t  sglen;
    uint8_t  cskey;
} FragmentInfo;

typedef struct context_t {
    const KLoadProgressbar *progress[4];
    KBTree *key2id[NUM_ID_SPACES];
    char *key2id_names;
    MMArray *id2value;
    MemBank *frags;
    int64_t spotId;
    int64_t primaryId;
    int64_t secondId;
    uint64_t alignCount;

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

    unsigned pass;
    bool isColorSpace;
} context_t;

static char const *Print_ctx_value_t(ctx_value_t const *const self)
{
    static char buffer[4096];
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

static rc_t GetKeyIDOld(context_t *const ctx, uint64_t *const rslt, bool *const wasInserted, char const key[], char const name[], unsigned const namelen)
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
rc_t GetKeyID(context_t *const ctx,
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

    memset(ctx, 0, sizeof(*ctx));

    if (G.mode == mode_Archive) {
        KDirectory *dir;
        size_t fragSize[2];

        fragSize[1] = (G.cache_size / 8);
        fragSize[0] = fragSize[1] * 4;

        rc = KLoadProgressbar_Make(&ctx->progress[0], 0); if (rc) return rc;
        rc = KLoadProgressbar_Make(&ctx->progress[1], 0); if (rc) return rc;
        rc = KLoadProgressbar_Make(&ctx->progress[2], 0); if (rc) return rc;
        rc = KLoadProgressbar_Make(&ctx->progress[3], 0); if (rc) return rc;

        KLoadProgressbar_Append(ctx->progress[0], 100 * numfiles);

        rc = TmpfsDirectory(&dir);
        if (rc == 0)
            rc = OpenMMapFile(ctx, dir);
        if (rc == 0)
            rc = MemBankMake(&ctx->frags, dir, G.pid, fragSize);
        KDirectoryRelease(dir);
    }
    return rc;
}

static void ContextReleaseMemBank(context_t *ctx)
{
    MemBankRelease(ctx->frags);
    ctx->frags = NULL;
}

static void ContextRelease(context_t *ctx)
{
    KLoadProgressbar_Release(ctx->progress[0], true);
    KLoadProgressbar_Release(ctx->progress[1], true);
    KLoadProgressbar_Release(ctx->progress[2], true);
    KLoadProgressbar_Release(ctx->progress[3], true);
    MMArrayWhack(ctx->id2value);
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

static rc_t OpenBAM(const BAMFile **bam, VDatabase *db, const char bamFile[])
{
    rc_t rc = BAMFileMakeWithHeader(bam, G.headerText, "%s", bamFile);
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

                rc = BAMFileGetHeaderText(*bam, &header, &size);
                if (rc == 0) {
                    rc = KMDataNodeWrite(node, header, size);
                }
                KMDataNodeRelease(node);
            }
        }
    }

    return rc;
}

static rc_t VerifyReferences(BAMFile const *bam, Reference const *ref)
{
    rc_t rc = 0;
    uint32_t n;
    unsigned i;

    BAMFileGetRefSeqCount(bam, &n);
    for (i = 0; i != n; ++i) {
        BAMRefSeq const *refSeq;

        BAMFileGetRefSeq(bam, i, &refSeq);
        if (G.refFilter && strcmp(refSeq->name, G.refFilter) != 0)
            continue;

        rc = ReferenceVerify(ref, refSeq->name, refSeq->length, refSeq->checksum);
        if (rc) {
            if (GetRCObject(rc) == rcChecksum && GetRCState(rc) == rcUnequal) {
#if NCBI
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); checksums do not match", "name=%s,len=%u", refSeq->name, (unsigned)refSeq->length));
#endif
            }
            else
            if (GetRCObject(rc) == rcSize && GetRCState(rc) == rcUnequal) {
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

static uint8_t GetMapQ(BAMAlignment const *rec)
{
    uint8_t mapQ;

    BAMAlignmentGetMapQuality(rec, &mapQ);
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
INSDC_SRA_platform_id GetINSDCPlatform(BAMFile const *bam, char const name[]) {
    if (name) {
        BAMReadGroup const *rg;

        BAMFileGetReadGroupByName(bam, name, &rg);
        if (rg && rg->platform) {
            switch (toupper(rg->platform[0])) {
            case 'C':
                if (platform_cmp(rg->platform, "COMPLETE GENOMICS"))
                    return SRA_PLATFORM_COMPLETE_GENOMICS;
                if (platform_cmp(rg->platform, "CAPILLARY"))
                    return SRA_PLATFORM_SANGER;
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
            case 'P':
                if (platform_cmp(rg->platform, "PACBIO"))
                    return SRA_PLATFORM_PACBIO_SMRT;
                break;
            case 'S':
                if (platform_cmp(rg->platform, "SOLID"))
                    return SRA_PLATFORM_ABSOLID;
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
    ++G.errCount;
    if (G.maxErrCount > 0 && G.errCount > G.maxErrCount) {
        (void)PLOGERR(klogErr, (klogErr, RC(rcAlign, rcFile, rcReading, rcError, rcExcessive), "Number of errors $(cnt) exceeds limit of $(max): Exiting", "cnt=%u,max=%u", G.errCount, G.maxErrCount));
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

static
rc_t LogNoMatch(char const readName[], char const refName[], unsigned rpos, unsigned matches)
{
    rc_t const rc = CheckLimitAndLogError();
    static unsigned count = 0;

    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    }
    else if (G.maxWarnCount_NoMatch == 0 || count < G.maxWarnCount_NoMatch)
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    return rc;
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
        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    }
    else if (G.maxWarnCount_DupConflict == 0 || count < G.maxWarnCount_DupConflict)
        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
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
    "record discarded"
};

static char const *const REASONS[] = {
/* FLAG changed */
    "0x400 and 0x200 both set",
    "conflicting PCR Dup flags",
    "primary alignment already exists",
    "was already recorded as unaligned",
/* QUAL changed */
    "original quality used",
    "unaligned colorspace",
    "aligned bases",
    "unaligned bases",
    "reversed",
/* unaligned */
    "low MAPQ",
    "low match count",
    "missing alignment info",
    "missing reference position",
    "invalid alignment info",
    "invalid reference position",
    "invalid reference",
    "unaligned reference",
    "unknown reference",
    "hard-clipped colorspace",
/* unfragmented */
    "missing fragment info",
    "too many fragments",
/* mate info lost */
    "invalid mate reference",
    "missing mate alignment info",
    "unknown mate reference",
/* discarded */
    "conflicting PCR duplicate",
    "conflicting fragment info",
    "reference is skipped"
};

static struct {
    unsigned what, why;
} const CHANGES[] = {
    {0,  0},
    {0,  1},
    {0,  2},
    {0,  3},
    {1,  4},
    {1,  5},
    {1,  6},
    {1,  7},
    {1,  8},
    {2,  8},
    {3,  9},
    {3, 10},
    {3, 11},
    {3, 12},
    {3, 13},
    {3, 14},
    {3, 15},
    {3, 16},
    {3, 17},
    {3, 18},
    {4, 19},
    {4, 20},
    {5, 21},
    {5, 22},
    {5, 23},
    {6, 24},
    {6, 25},
    {6, 26},
    {6, 17}
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

static rc_t ProcessBAM(char const bamFile[], context_t *ctx, VDatabase *db,
                       Reference *ref, Sequence *seq, Alignment *align,
                       bool *had_alignments, bool *had_sequences)
{
    const BAMFile *bam;
    const BAMAlignment *rec;
    KDataBuffer buf;
    KDataBuffer fragBuf;
    KDataBuffer cigBuf;
    rc_t rc;
    int32_t lastRefSeqId = -1;
    size_t rsize;
    uint64_t keyId = 0;
    uint64_t reccount = 0;
    SequenceRecord srec;
    char spotGroup[512];
    size_t namelen;
    unsigned progress = 0;
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

    memset(&data, 0, sizeof(data));

    rc = OpenBAM(&bam, db, bamFile);
    if (rc) return rc;
    if (!G.noVerifyReferences && ref != NULL) {
        rc = VerifyReferences(bam, ref);
        if (G.onlyVerifyReferences) {
            BAMFileRelease(bam);
            return rc;
        }
    }
    if (ctx->key2id_max == 0) {
        uint32_t rgcount;
        unsigned rgi;

        BAMFileGetReadGroupCount(bam, &rgcount);
        if (rgcount > (sizeof(ctx->key2id)/sizeof(ctx->key2id[0]) - 1))
            ctx->key2id_max = 1;
        else
            ctx->key2id_max = sizeof(ctx->key2id)/sizeof(ctx->key2id[0]);

        for (rgi = 0; rgi != rgcount; ++rgi) {
            BAMReadGroup const *rg;

            BAMFileGetReadGroup(bam, rgi, &rg);
            if (rg && rg->platform && platform_cmp(rg->platform, "CAPILLARY")) {
                G.hasTI = true;
                break;
            }
        }
    }
    memset(&srec, 0, sizeof(srec));

    rc = KDataBufferMake(&cigBuf, 32, 0);
    if (rc)
        return rc;

    rc = KDataBufferMake(&fragBuf, 8, 1024);
    if (rc)
        return rc;

    rc = KDataBufferMake(&buf, 16, 0);
    if (rc)
        return rc;

    if (rc == 0) {
        (void)PLOGMSG(klogInfo, (klogInfo, "Loading '$(file)'", "file=%s", bamFile));
    }
    while (rc == 0 && (rc = Quitting()) == 0) {
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
        bool hasCG;
        uint64_t ti = 0;
        uint32_t csSeqLen = 0;

        rc = BAMFileRead2(bam, &rec);
        if (rc) {
            if (GetRCModule(rc) == rcAlign && GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                rc = 0;
            else if (GetRCModule(rc) == rcAlign && GetRCObject(rc) == rcRow && GetRCState(rc) == rcEmpty) {
                ++recordsRead;
                (void)PLOGERR(klogWarn, (klogWarn, rc, "File '$(file)'; record $(recno)", "file=%s,recno=%lu", bamFile, recordsRead));
                rc = CheckLimitAndLogError();
                goto LOOP_END;
            }
            break;
        }
        ++recordsRead;
        if ((unsigned)(BAMFileGetProportionalPosition(bam) * 100.0) > progress) {
            unsigned new_value = BAMFileGetProportionalPosition(bam) * 100.0;
            KLoadProgressbar_Process(ctx->progress[0], new_value - progress, false);
            progress = new_value;
        }


        /**************************************************************/
        if (!G.noColorSpace) {
            if (BAMAlignmentHasColorSpace(rec)) {/*BAM*/
                if (isNotColorSpace) {
MIXED_BASE_AND_COLOR:
                    rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains base space and color space", "file=%s", bamFile));
                    goto LOOP_END;
                }
                ctx->isColorSpace = isColorSpace = true;
            }
            else if (isColorSpace)
                goto MIXED_BASE_AND_COLOR;
            else
                isNotColorSpace = true;
        }
        hasCG = BAMAlignmentHasCGData(rec);/*BAM*/
        if (hasCG) {
            BAMAlignmentGetCigarCount(rec, &opCount);/*BAM*/
            rc = KDataBufferResize(&cigBuf, opCount * 2 + 5);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                goto LOOP_END;
            }

            rc = AlignmentRecordInit(&data, readlen = 35);
            if (rc == 0)
                rc = KDataBufferResize(&buf, readlen);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                goto LOOP_END;
            }

            seqDNA = buf.base;
            qual = (uint8_t *)&seqDNA[readlen];
        }
        else {
            uint32_t const *tmp;

            BAMAlignmentGetRawCigar(rec, &tmp, &opCount);/*BAM*/
            rc = KDataBufferResize(&cigBuf, opCount);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                goto LOOP_END;
            }
            memcpy(cigBuf.base, tmp, opCount * sizeof(uint32_t));

            BAMAlignmentGetReadLength(rec, &readlen);/*BAM*/
            if (isColorSpace) {
                BAMAlignmentGetCSSeqLen(rec, &csSeqLen);
                if (readlen > csSeqLen) {
                    rc = RC(rcAlign, rcRow, rcReading, rcData, rcInconsistent);
                    (void)LOGERR(klogErr, rc, "Sequence length and CS Sequence length are inconsistent");
                    goto LOOP_END;
                }
                else if (readlen < csSeqLen)
                    readlen = 0;
            }
            else if (readlen == 0) {
            }

            rc = AlignmentRecordInit(&data, readlen | csSeqLen);
            if (rc == 0)
                rc = KDataBufferResize(&buf, readlen | csSeqLen);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                goto LOOP_END;
            }

            seqDNA = buf.base;
            qual = (uint8_t *)&seqDNA[readlen | csSeqLen];
        }
        BAMAlignmentGetReadName2(rec, &name, &namelen);/*BAM*/
        BAMAlignmentGetSequence(rec, seqDNA);/*BAM*/
        if (G.useQUAL) {
            uint8_t const *squal;

            BAMAlignmentGetQuality(rec, &squal);/*BAM*/
            memcpy(qual, squal, readlen);
        }
        else {
            uint8_t const *squal;
            uint8_t qoffset = 0;
            unsigned i;

            rc = BAMAlignmentGetQuality2(rec, &squal, &qoffset);/*BAM*/
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of original quality does not match sequence", "name=%s", name));
                goto LOOP_END;
            }
            if (qoffset) {
                for (i = 0; i != readlen; ++i)
                    qual[i] = squal[i] - qoffset;
                QUAL_CHANGED_OQ;
            }
            else
                memcpy(qual, squal, readlen);
        }
        if (hasCG) {
            rc = BAMAlignmentGetCGSeqQual(rec, seqDNA, qual);/*BAM*/
            if (rc == 0)
                rc = BAMAlignmentGetCGCigar(rec, cigBuf.base, cigBuf.elem_count, &opCount);/*BAM*/
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to read CG data");
                goto LOOP_END;
            }
        }
        if (G.hasTI) {
            rc = BAMAlignmentGetTI(rec, &ti);/*BAM*/
            if (rc)
                ti = 0;
            rc = 0;
        }
        data.data.align_group.buffer = alignGroup;
        if (BAMAlignmentGetCGAlignGroup(rec, alignGroup, sizeof(alignGroup), &alignGroupLen) == 0)/*BAM*/
            data.data.align_group.elements = alignGroupLen;
        else
            data.data.align_group.elements = 0;

        AR_MAPQ(data) = GetMapQ(rec);
        BAMAlignmentGetFlags(rec, &flags);/*BAM*/
        BAMAlignmentGetReadName2(rec, &name, &namelen);/*BAM*/
        {{
            char const *rgname;

            BAMAlignmentGetReadGroupName(rec, &rgname);/*BAM*/
            if (rgname)
                strcpy(spotGroup, rgname);
            else
                spotGroup[0] = '\0';
        }}
        AR_REF_ORIENT(data) = (flags & BAMFlags_SelfIsReverse) == 0 ? false : true;/*BAM*/
        isPrimary = (flags & BAMFlags_IsNotPrimary) == 0 ? true : false;/*BAM*/
        if (G.noSecondary && !isPrimary)
            goto LOOP_END;
        originally_aligned = (flags & BAMFlags_SelfIsUnmapped) == 0;/*BAM*/
        aligned = originally_aligned;
        if (originally_aligned && AR_MAPQ(data) < G.minMapQual) {
            aligned = false;
            UNALIGNED_LOW_MAPQ;
        }
        if (aligned && isColorSpace && readlen == 0) {
            /* detect hard clipped colorspace   */
            /* reads and make unaligned         */
            aligned = false;
            UNALIGNED_HARD_CLIPPED_CS;
        }

        if (aligned && align == NULL) {
            rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
            (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains aligned records", "file=%s", bamFile));
            goto LOOP_END;
        }
        while (aligned) {
            BAMAlignmentGetPosition(rec, &rpos);/*BAM*/
            BAMAlignmentGetRefSeqId(rec, &refSeqId);/*BAM*/
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
                if (refSeqId == lastRefSeqId)
                    break;
                refSeq = NULL;
                BAMFileGetRefSeqById(bam, refSeqId, &refSeq);/*BAM*/
                if (refSeq == NULL) {
                    rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
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

                    rc = ReferenceSetFile(ref, refSeq->name, refSeq->length, refSeq->checksum, &shouldUnmap);
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
            assert("this shouldn't happen");
            goto LOOP_END;
        }
        rc = GetKeyID(ctx, &keyId, &wasInserted, spotGroup, name, namelen);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "KBTreeEntry: failed on key '$(key)'", "key=%.*s", namelen, name));
            goto LOOP_END;
        }
        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "MMArrayGet: failed on id '$(id)'", "id=%u", keyId));
            goto LOOP_END;
        }

        AR_KEY(data) = keyId;

        mated = false;
        if (flags & BAMFlags_WasPaired) {/*BAM*/
            if ((flags & BAMFlags_IsFirst) != 0)/*BAM*/
                AR_READNO(data) |= 1;
            if ((flags & BAMFlags_IsSecond) != 0)/*BAM*/
                AR_READNO(data) |= 2;
            switch (AR_READNO(data)) {
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
            AR_READNO(data) = 1;

        if (wasInserted) {
            memset(value, 0, sizeof(*value));
            value->unmated = !mated;
            value->pcr_dup = (flags & BAMFlags_IsDuplicate) == 0 ? 0 : 1;/*BAM*/
            value->platform = GetINSDCPlatform(bam, spotGroup);
        }
        else {
            int const o_pcr_dup = value->pcr_dup;
            int const n_pcr_dup = (flags & BAMFlags_IsDuplicate) == 0 ? 0 : 1;
            
            if (!G.acceptBadDups && o_pcr_dup != n_pcr_dup) {
                rc = LogDupConflict(name);
                DISCARD_PCR_DUP;
                goto LOOP_END;
            }
            value->pcr_dup = o_pcr_dup & n_pcr_dup;
            if (o_pcr_dup != (o_pcr_dup & n_pcr_dup)) {
                FLAG_CHANGED_PCR_DUP;
            }
            if (mated && value->unmated) {
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen without mate info, now has mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError();
                DISCARD_BAD_FRAGMENT_INFO;
                goto LOOP_END;
            }
            else if (!mated && !value->unmated) {
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen with mate info, now has no mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError();
                DISCARD_BAD_FRAGMENT_INFO;
                goto LOOP_END;
            }
        }

        ++recordsProcessed;

        if (isPrimary) {
            switch (AR_READNO(data)) {
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
        data.isPrimary = isPrimary;
        if (aligned) {
            uint32_t matches = 0;
            uint8_t rna_orient = ' ';

            BAMAlignmentGetRNAStrand(rec, &rna_orient);
            rc = ReferenceRead(ref, &data, rpos, cigBuf.base, opCount, seqDNA, readlen,
                               rna_orient == '+' ? NCBI_align_ro_intron_plus :
                               rna_orient == '-' ? NCBI_align_ro_intron_minus :
                                           hasCG ? NCBI_align_ro_complete_genomics :
                                                   NCBI_align_ro_intron_unknown, &matches);
            if (rc) {
                aligned = false;

                if (   (GetRCState(rc) == rcViolated  && GetRCObject(rc) == rcConstraint)
                    || (GetRCState(rc) == rcExcessive && GetRCObject(rc) == rcRange))
                {
                    RecordNoMatch(name, refSeq->name, rpos);
                }
                if (GetRCState(rc) == rcViolated && GetRCObject(rc) == rcConstraint) {
                    rc = LogNoMatch(name, refSeq->name, (unsigned)rpos, (unsigned)matches);
                    UNALIGNED_LOW_MATCH_COUNT;
                }
#define DATA_INVALID_ERRORS_ARE_DEADLY 0
#if DATA_INVALID_ERRORS_ARE_DEADLY
                else if (((int)GetRCObject(rc)) == ((int)rcData) && GetRCState(rc) == rcInvalid) {
                    UNALIGNED_INVALID_INFO;
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': bad alignment to reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq->name, rpos));
                    CheckLimitAndLogError();
                }
#endif
                else if (((int)GetRCObject(rc)) == ((int)rcData) && GetRCState(rc) == rcNotAvailable) {
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': sequence was hard clipped", "name=%s", name));
                    CheckLimitAndLogError();
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
        if (isColorSpace) {
            /* must be after ReferenceRead */
            BAMAlignmentGetCSKey(rec, &cskey);/*BAM*/
            BAMAlignmentGetCSSequence(rec, seqDNA, csSeqLen);/*BAM*/
            if (!aligned && !G.useQUAL) {
                uint8_t const *squal;
                uint8_t qoffset = 0;

                rc = BAMAlignmentGetCSQuality(rec, &squal, &qoffset);/*BAM*/
                if (rc) {
                    (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of colorspace quality does not match sequence", "name=%s", name));
                    goto LOOP_END;
                }
                if (qoffset) {
                    unsigned i;

                    QUAL_CHANGED_UNALIGNED_CS;
                    for (i = 0; i < csSeqLen; ++i)
                        qual[i] = squal[i] - qoffset;
                }
                else
                    memcpy(qual, squal, csSeqLen);
                readlen = csSeqLen;
            }
        }

        if (aligned) {
            if (G.editAlignedQual && EditAlignedQualities  (qual, AR_HAS_MISMATCH(data), readlen)) {
                QUAL_CHANGED_ALIGNED_EDIT;
            }
            if (G.keepMismatchQual && EditUnalignedQualities(qual, AR_HAS_MISMATCH(data), readlen)) {
                QUAL_CHANGED_UNALIGN_EDIT;
            }
        }
        else if (isPrimary) {
            switch (AR_READNO(data)) {
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
            switch (AR_READNO(data)) {
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
        if (mated) {
            if (isPrimary || !originally_aligned) {
                if (CTX_VALUE_GET_S_ID(*value) != 0) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' has already been assigned a spot id", "name=%.*s", namelen, name));
                }
                else if (!value->has_a_read) {
                    /* new mated fragment - do spot assembly */
                    unsigned sz;
                    FragmentInfo fi;
                    int32_t mate_refSeqId = -1;
                    int64_t pnext = 0;

                    memset(&fi, 0, sizeof(fi));
                    fi.aligned = aligned;
                    fi.ti = ti;
                    fi.orientation = AR_REF_ORIENT(data);
                    fi.otherReadNo = AR_READNO(data);
                    fi.sglen   = strlen(spotGroup);
                    fi.readlen = readlen;
                    fi.cskey = cskey;
                    fi.is_bad = (flags & BAMFlags_IsLowQuality) != 0;/*BAM*/
                    sz = sizeof(fi) + 2*fi.readlen + fi.sglen;
                    if (align) {
                        BAMAlignmentGetMateRefSeqId(rec, &mate_refSeqId);/*BAM*/
                        BAMAlignmentGetMatePosition(rec, &pnext);/*BAM*/
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
                        int const revcmp = (isColorSpace && !aligned) ? 0 : fi.orientation;
                        uint8_t *dst = (uint8_t*) fragBuf.base;
                        
                        if (revcmp) {
                            QUAL_CHANGED_REVERSED;
                            SEQ__CHANGED_REV_COMP;
                        }
                        memcpy(dst,&fi,sizeof(fi));
                        dst += sizeof(fi);
                        COPY_READ((char *)dst, seqDNA, fi.readlen, revcmp);
                        dst += fi.readlen;
                        COPY_QUAL(dst, qual, fi.readlen, revcmp);
                        dst += fi.readlen;
                        memcpy(dst,spotGroup,fi.sglen);
                    }}
                    rc = MemBankWrite(ctx->frags, value->fragmentId, 0, fragBuf.base, sz, &rsize);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankWrite failed writing fragment $(id)", "id=%u", value->fragmentId));
                        goto LOOP_END;
                    }
                    value->has_a_read = 1;
                }
                else if (value->fragmentId != 0 ) {
                    /* might be second fragment */
                    size_t sz;
                    FragmentInfo *fip;

                    rc = MemBankSize(ctx->frags, value->fragmentId, &sz);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankSize failed on fragment $(id)", "id=%u", value->fragmentId));
                        goto LOOP_END;
                    }
                    rc=KDataBufferResize(&fragBuf, (size_t)sz);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to resize fragment buffer", ""));
                        goto LOOP_END;
                    }
                    rc = MemBankRead(ctx->frags, value->fragmentId, 0, fragBuf.base, sz, &rsize);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankRead failed on fragment $(id)", "id=%u", value->fragmentId));
                        goto LOOP_END;
                    }

                    assert( rsize == sz );
                    fip = (FragmentInfo *) fragBuf.base;
                    if(AR_READNO(data) != fip->otherReadNo) {
                        /* mate found */
                        unsigned readLen[2];
                        unsigned read1 = 0;
                        unsigned read2 = 1;
                        uint8_t  *src  = (uint8_t*) fip + sizeof(*fip);

                        if (AR_READNO(data) < fip->otherReadNo) {
                            read1 = 1;
                            read2 = 0;
                        }
                        readLen[read1] = fip->readlen;
                        readLen[read2] = readlen;
                        rc = SequenceRecordInit(&srec, 2, readLen);
                        if (rc) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "Failed resizing sequence record buffer", ""));
                            goto LOOP_END;
                        }
                        srec.ti[read1] = fip->ti;
                        srec.aligned[read1] = fip->aligned;
                        srec.is_bad[read1] = fip->is_bad;
                        srec.orientation[read1] = fip->orientation;
                        srec.cskey[read1] = fip->cskey;
                        memcpy(srec.seq + srec.readStart[read1], src, fip->readlen);
                        src += fip->readlen;
                        memcpy(srec.qual + srec.readStart[read1], src, fip->readlen);
                        src += fip->readlen;

                        srec.orientation[read2] = AR_REF_ORIENT(data);
                        {
                            int const revcmp = (isColorSpace && !aligned) ? 0 : srec.orientation[read2];
                            
                            if (revcmp) {
                                QUAL_CHANGED_REVERSED;
                                SEQ__CHANGED_REV_COMP;
                            }
                            COPY_READ(srec.seq + srec.readStart[read2], seqDNA, srec.readLen[read2], revcmp);
                            COPY_QUAL(srec.qual + srec.readStart[read2], qual, srec.readLen[read2],  revcmp);
                        }
                        srec.keyId = keyId;
                        srec.is_bad[read2] = (flags & BAMFlags_IsLowQuality) != 0;
                        srec.aligned[read2] = aligned;
                        srec.cskey[read2] = cskey;
                        srec.ti[read2] = ti;

                        srec.spotGroup = spotGroup;
                        srec.spotGroupLen = strlen(spotGroup);
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
                        rc = SequenceWriteRecord(seq, &srec, isColorSpace, value->pcr_dup, value->platform);
                        if (rc) {
                            (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                            goto LOOP_END;
                        }
                        CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
                        if(value->fragmentId & 1){
                            fcountOne--;
                        } else {
                            fcountBoth--;
                        }
                        /*	printf("OUT:%9d\tcnt2=%ld\tcnt1=%ld\n",value->fragmentId,fcountBoth,fcountOne);*/
                        rc = MemBankFree(ctx->frags, value->fragmentId);
                        if (rc) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankFree failed on fragment $(id)", "id=%u", value->fragmentId));
                            goto LOOP_END;
                        }
                        value->fragmentId = 0;
                    }
                }
            }
            if (!isPrimary && aligned) {
                int32_t bam_mrid;
                int64_t mpos;
                int64_t mrid = 0;
                int64_t tlen;

                BAMAlignmentGetMatePosition(rec, &mpos);/*BAM*/
                BAMAlignmentGetMateRefSeqId(rec, &bam_mrid);/*BAM*/
                BAMAlignmentGetInsertSize(rec, &tlen);/*BAM*/

                if (mpos >= 0 && bam_mrid >= 0 && tlen != 0) {
                    BAMRefSeq const *mref;/*BAM*/

                    BAMFileGetRefSeq(bam, bam_mrid, &mref);/*BAM*/
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
        }
        else if (CTX_VALUE_GET_S_ID(*value) == 0 && (isPrimary || !originally_aligned)) {
            /* new unmated fragment - no spot assembly */
            unsigned readLen[1];

            readLen[0] = readlen;
            rc = SequenceRecordInit(&srec, 1, readLen);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "Failed resizing sequence record buffer", ""));
                goto LOOP_END;
            }
            srec.ti[0] = ti;
            srec.aligned[0] = aligned;
            srec.is_bad[0] = (flags & BAMFlags_IsLowQuality) != 0;
            srec.orientation[0] = AR_REF_ORIENT(data);
            srec.cskey[0] = cskey;
            {
                int const revcmp = (isColorSpace && !aligned) ? 0 : srec.orientation[0];
                
                if (revcmp) {
                    QUAL_CHANGED_REVERSED;
                    SEQ__CHANGED_REV_COMP;
                }
                COPY_READ(srec.seq  + srec.readStart[0], seqDNA, readlen, revcmp);
                COPY_QUAL(srec.qual + srec.readStart[0],   qual, readlen, revcmp);
            }

            srec.keyId = keyId;

            srec.spotGroup = spotGroup;
            srec.spotGroupLen = strlen(spotGroup);
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
            rc = SequenceWriteRecord(seq, &srec, isColorSpace, value->pcr_dup, value->platform);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "SequenceWriteRecord failed", ""));
                goto LOOP_END;
            }
            CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
            value->fragmentId = 0;
        }

        if (aligned) {
            if (value->alignmentCount[AR_READNO(data) - 1] < 254)
                ++value->alignmentCount[AR_READNO(data) - 1];
            ++ctx->alignCount;

            assert(keyId >> 32 < ctx->key2id_count);
            assert((uint32_t)keyId < ctx->idCount[keyId >> 32]);

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
        BAMAlignmentRelease(rec);
        ++reccount;
        if (G.maxAlignCount > 0 && reccount >= G.maxAlignCount)
            break;
        if (rc == 0)
            *had_sequences = true;
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
    BAMFileRelease(bam);
    MMArrayLock(ctx->id2value);
    KDataBufferWhack(&buf);
    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&srec.storage);
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
    SequenceRecord srec;

    ++ctx->pass;
    memset(&srec, 0, sizeof(srec));

    rc = KDataBufferMake(&fragBuf, 8, 0);
    if (rc) {
        (void)LOGERR(klogErr, rc, "KDataBufferMake failed");
        return rc;
    }
    for (idCount = 0, j = 0; j < ctx->key2id_count; ++j) {
        idCount += ctx->idCount[j];
    }
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], idCount);

    for (idCount = 0, j = 0; j < ctx->key2id_count; ++j) {
        for (i = 0; i != ctx->idCount[j]; ++i, ++idCount) {
            uint64_t const keyId = ((uint64_t)j << 32) | i;
            ctx_value_t *value;
            size_t rsize;
            size_t sz;
            unsigned readLen[2];
            unsigned read = 0;
            FragmentInfo const *fip;
            uint8_t const *src;

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
            fip = (FragmentInfo const *)fragBuf.base;
            src = (uint8_t const *)&fip[1];

            readLen[0] = readLen[1] = 0;
            if (!value->unmated && (   (fip->aligned && CTX_VALUE_GET_P_ID(*value, 0) == 0)
                                    || (value->unaligned_2)))
            {
                read = 1;
            }

            readLen[read] = fip->readlen;
            rc = SequenceRecordInit(&srec, value->unmated ? 1 : 2, readLen);
            if (rc) {
                (void)LOGERR(klogErr, rc, "SequenceRecordInit failed");
                break;
            }

            srec.ti[read] = fip->ti;
            srec.aligned[read] = fip->aligned;
            srec.is_bad[read] = fip->is_bad;
            srec.orientation[read] = fip->orientation;
            srec.cskey[read] = fip->cskey;
            memcpy(srec.seq + srec.readStart[read], src, srec.readLen[read]);
            src += fip->readlen;
            memcpy(srec.qual + srec.readStart[read], src, srec.readLen[read]);
            src += fip->readlen;
            srec.spotGroup = (char *)src;
            srec.spotGroupLen = fip->sglen;
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
    KDataBufferWhack(&srec.storage);
    return rc;
}

static rc_t SequenceUpdateAlignInfo(context_t *ctx, Sequence *seq)
{
    rc_t rc = 0;
    uint64_t row;
    ctx_value_t const *value;
    uint64_t keyId;

    ++ctx->pass;
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->spotId + 1);

    for (row = 1; row <= ctx->spotId; ++row) {
        rc = SequenceReadKey(seq, row, &keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to get key for row $(row)", "row=%u", (unsigned)row));
            break;
        }
        rc = MMArrayGetRead(ctx->id2value, (void const **)&value, keyId);
        if (rc) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read info for row $(row), index $(idx)", "row=%u,idx=%u", (unsigned)row, (unsigned)keyId));
            break;
        }
        if (row != CTX_VALUE_GET_S_ID(*value)) {
            rc = RC(rcApp, rcTable, rcWriting, rcData, rcUnexpected);
            (void)PLOGMSG(klogErr, (klogErr, "Unexpected spot id $(spotId) for row $(row), index $(idx)", "spotId=%u,row=%u,idx=%u", (unsigned)CTX_VALUE_GET_S_ID(*value), (unsigned)row, (unsigned)keyId));
            break;
        }
        {{
            int64_t primaryId[2];

            primaryId[0] = CTX_VALUE_GET_P_ID(*value, 0);
            primaryId[1] = CTX_VALUE_GET_P_ID(*value, 1);

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
        ctx_value_t const *value;

        rc = AlignmentGetSpotKey(align, &keyId);
        if (rc) {
            if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                rc = 0;
            break;
        }
        assert(keyId >> 32 < ctx->key2id_count);
        assert((uint32_t)keyId < ctx->idCount[keyId >> 32]);
        rc = MMArrayGetRead(ctx->id2value, (void const **)&value, keyId);
        if (rc == 0) {
            int64_t const spotId = CTX_VALUE_GET_S_ID(*value);

            if (spotId == 0) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(id)' was never assigned a spot id, probably has no primary alignments", "id=%lx", keyId));
                /* (void)PLOGMSG(klogWarn, (klogWarn, "Spot #$(i): { $(s) }", "i=%lu,s=%s", keyId, Print_ctx_value_t(value))); */
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
                       bool *has_alignments)
{
    rc_t rc = 0;
    rc_t rc2;
    Reference ref;
    Sequence seq;
    Alignment *align;
    context_t ctx;
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

    rc = SetupContext(&ctx, bamFiles + seqFiles);
    if (rc)
        return rc;

    ++ctx.pass;
    for (i = 0; i < bamFiles && rc == 0; ++i) {
        bool this_has_alignments = false;
        bool this_has_sequences = false;

        rc = ProcessBAM(bamFile[i], &ctx, db, &ref, &seq, align, &this_has_alignments, &this_has_sequences);
        *has_alignments |= this_has_alignments;
        has_sequences |= this_has_sequences;
    }
    for (i = 0; i < seqFiles && rc == 0; ++i) {
        bool this_has_alignments = false;
        bool this_has_sequences = false;

        rc = ProcessBAM(seqFile[i], &ctx, db, &ref, &seq, align, &this_has_alignments, &this_has_sequences);
        *has_alignments |= this_has_alignments;
        has_sequences |= this_has_sequences;
    }
/*** No longer need memory for key2id ***/
    for (i = 0; i != ctx.key2id_count; ++i) {
        KBTreeDropBacking(ctx.key2id[i]);
        KBTreeRelease(ctx.key2id[i]);
        ctx.key2id[i] = NULL;
    }
    free(ctx.key2id_names);
    ctx.key2id_names = NULL;
/*******************/

    if (has_sequences) {
        if (rc == 0 && (rc = Quitting()) == 0) {
            (void)LOGMSG(klogInfo, "Writing unpaired sequences");
            rc = WriteSoloFragments(&ctx, &seq);
            ContextReleaseMemBank(&ctx);
            if (rc == 0) {
                rc = SequenceDoneWriting(&seq);
                if (rc == 0) {
                    (void)LOGMSG(klogInfo, "Updating sequence alignment info");
                    rc = SequenceUpdateAlignInfo(&ctx, &seq);
                }
            }
        }
    }

    if (*has_alignments && rc == 0 && (rc = Quitting()) == 0) {
        (void)LOGMSG(klogInfo, "Writing alignment spot ids");
        rc = AlignmentUpdateSpotInfo(&ctx, align);
    }
    rc2 = AlignmentWhack(align, *has_alignments && rc == 0 && (rc = Quitting()) == 0);
    if (rc == 0)
        rc = rc2;

    rc2 = ReferenceWhack(&ref, *has_alignments && rc == 0 && (rc = Quitting()) == 0);
    if (rc == 0)
        rc = rc2;

    SequenceWhack(&seq, rc == 0);

    ContextRelease(&ctx);

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
         unsigned seqFiles, char const *seqFile[])
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

        rc = VDBManagerDisablePagemapThread(mgr);
        if (rc == 0)
        {

            if (G.onlyVerifyReferences) {
                rc = ArchiveBAM(mgr, NULL, bamFiles, bamFile, 0, NULL, &has_alignments);
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
                        rc2 = VSchemaRelease(schema);
                        if (rc2)
                            (void)LOGERR(klogWarn, rc2, "Failed to release schema");
                        if (rc == 0)
                            rc = rc2;
                        if (rc == 0) {
                            rc = ArchiveBAM(mgr, db, bamFiles, bamFile, seqFiles, seqFile, &has_alignments);
                            if (rc == 0)
                                PrintChangeReport();
                            if (rc == 0 && !has_alignments) {
                                rc = ConvertDatabaseToUnmapped(db);
                            }

                            rc2 = VDatabaseRelease(db);
                            if (rc2)
                                (void)LOGERR(klogWarn, rc2, "Failed to close database");
                            if (rc == 0)
                                rc = rc2;

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
