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
 
#include <sysalloc.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include <klib/log.h>
#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/status.h>

#include <kdb/btree.h>

#include <kfs/pmem.h>
#include <kfs/file.h>
#include <kfs/pagefile.h>

#include <kapp/main.h>

#include <vdb/manager.h>
#include <vdb/database.h>

#include <loader/progressbar.h>
#include <loader/sequence-writer.h>
#include <loader/alignment-writer.h>
#include <loader/reference-writer.h>
#include <loader/common-writer.h>
#include <loader/common-reader-priv.h>

/*--------------------------------------------------------------------------
 * ctx_value_t, FragmentInfo
 */
typedef struct {
    uint32_t primaryId[2];
    uint32_t spotId;
    uint32_t fragmentId;
    uint16_t seqHash[2];
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


rc_t OpenKBTree(const CommonWriterSettings* settings, struct KBTree **const rslt, size_t const n, size_t const max)
{
    size_t const cacheSize = (((settings->cache_size - (settings->cache_size / 2) - (settings->cache_size / 8)) / max)
                            + 0xFFFFF) & ~((size_t)0xFFFFF);
    KFile *file = NULL;
    KDirectory *dir;
    char fname[4096];
    rc_t rc;

    rc = KDirectoryNativeDir(&dir);
    if (rc)
        return rc;
    
    rc = string_printf(fname, sizeof(fname), NULL, "%s/key2id.%u.%u", settings->tmpfs, settings->pid, n); if (rc) return rc;
    STSMSG(1, ("Path for scratch files: %s\n", fname));
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

rc_t GetKeyIDOld(const CommonWriterSettings* settings, SpotAssembler* const ctx, uint64_t *const rslt, bool *const wasInserted, char const key[], char const name[], size_t const namelen)
{
    size_t const keylen = strlen(key);
    rc_t rc;
    uint64_t tmpKey;

    if (ctx->key2id_count == 0) {
        rc = OpenKBTree(settings, &ctx->key2id[0], 1, 1);
        if (rc) return rc;
        ctx->key2id_count = 1;
    }
    if (keylen == 0 || memcmp(key, name, keylen) == 0) {
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

#if _DEBUGGING
/*
static char const *Print_ctx_value_t(ctx_value_t const *const self)
{
    static char buffer[4096];
    uint8_t const *const flag = self->alignmentCount + 2;
    rc_t rc = string_printf(buffer, sizeof(buffer), NULL,
                            "pid: { %lu, %lu }, sid: %lu, fid: %u, alc: { %u, %u }, flg: %x",
                            CTX_VALUE_GET_P_ID(*self, 0),
                            CTX_VALUE_GET_P_ID(*self, 1),
                            CTX_VALUE_GET_S_ID(*self),
                            self->fragmentId,
                            self->alignmentCount[0],
                            self->alignmentCount[1],
                            *flag);

    if (rc)
        return 0;
    return buffer;
}
*/
#endif

static unsigned HashKey(void const *const key, size_t const keylen)
{
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
    size_t i = keylen;
    
    do { h = T1[h ^ ((uint8_t)i)]; } while ((i >>= 8) != 0);

    for (i = 0; i != keylen; ++i)
        h = T1[h ^ ((uint8_t const *)key)[i]];

    return h;
}

static unsigned SeqHashKey(void const *const key, size_t const keylen)
{
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
    unsigned h1 = 0x55;
    unsigned h2 = 0x22;
    size_t i = keylen;
    
    do { h1 = T1[h1 ^ ((uint8_t)i)]; } while ((i >>= 8) != 0);

    for (i = 0; i != keylen; ++i)
    {
        unsigned temp = ((uint8_t const *)key)[i];
        h1 = T1[h1 ^ temp];
        h2 = T1[h2 ^ temp];
    }

    return (h1 << 8) | h2;
}

#define USE_ILLUMINA_NAMING_POUND_NUMBER_SLASH_HACK 1

static size_t GetFixedNameLength(char const name[], size_t const namelen)
{
#if USE_ILLUMINA_NAMING_POUND_NUMBER_SLASH_HACK
    char const *const pound = string_chr(name, namelen, '#');
    
    if (pound && pound + 2u < name + namelen && pound[1] >= '0' && pound[1] <= '9' && pound[2] == '/') {
        return (size_t)(pound - name) + 2u;
    }
#endif
    return namelen;
}

rc_t GetKeyID(CommonWriterSettings *const settings,
              SpotAssembler *const ctx,
              uint64_t *const rslt,
              bool *const wasInserted,
              char const key[],
              char const name[],
              size_t const o_namelen)
{
    size_t const namelen = GetFixedNameLength(name, o_namelen);

    if (ctx->key2id_max == 1)
        return GetKeyIDOld(settings, ctx, rslt, wasInserted, key, name, namelen);
    else {
        size_t const keylen = strlen(key);
        unsigned const h = HashKey(key, keylen);
        size_t f;
        size_t e = ctx->key2id_count;
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
            size_t const m = (f + e) / 2;
            size_t const oid = ctx->key2id_oid[m];
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
            size_t const name_max = ctx->key2id_name_max + keylen + 1;
            KBTree *tree;
            rc_t rc = OpenKBTree(settings, &tree, ctx->key2id_count + 1, 1); /* ctx->key2id_max); */
            
            if (rc) return rc;
            
            if (ctx->key2id_name_alloc < name_max) {
                size_t alloc = ctx->key2id_name_alloc;
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

            memmove(&ctx->key2id_names[ctx->key2id_name[f]], key, keylen + 1);
            ctx->key2id[f] = tree;
            ctx->idCount[f] = 0;
            if ((uint8_t)ctx->key2id_hash[h] < 3) {
                unsigned const n = (uint8_t)ctx->key2id_hash[h] + 1;
                
                ctx->key2id_hash[h] = (uint32_t)((((ctx->key2id_hash[h] & ~(0xFFu)) | f) << 8) | n);
            }
            else {
                /* the hash function isn't working too well
                 * keep the 3 mru
                 */
                ctx->key2id_hash[h] = (uint32_t)((((ctx->key2id_hash[h] & ~(0xFFu)) | f) << 8) | 3);
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

static rc_t OpenMMapFile(const CommonWriterSettings* settings, SpotAssembler *const ctx, KDirectory *const dir)
{
    KFile *file = NULL;
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/id2value.%u", settings->tmpfs, settings->pid);
    
    if (rc)
        return rc;
    
    rc = KDirectoryCreateFile(dir, &file, true, 0600, kcmInit, "%s", fname);
    KDirectoryRemove(dir, 0, "%s", fname);
    if (rc == 0)
        rc = MMArrayMake(&ctx->id2value, file, sizeof(ctx_value_t));
    KFileRelease(file);
    return rc;
}

static rc_t OpenMBankFile(const CommonWriterSettings* settings, SpotAssembler *const ctx, KDirectory *const dir)
{
    KFile *file = NULL;
    char fname[4096];
    KMemBank **const mbank = &ctx->fragsBoth;
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/fragments.%u", settings->tmpfs, settings->pid);
    
    if (rc)
        return rc;
    
    rc = KDirectoryCreateFile(dir, &file, true, 0600, kcmInit, "%s", fname);
    KDirectoryRemove(dir, 0, "%s", fname);
    if (rc == 0) {
        rc = KMemBankMake(mbank, 0, 0, file);
        KFileRelease(file);
    }
    return rc;
}

rc_t SetupContext(const CommonWriterSettings* settings, SpotAssembler *ctx)
{
    rc_t rc = 0;

    memset(ctx, 0, sizeof(*ctx));
    
    ctx->pass = 1;
    
    if (settings->mode == mode_Archive) {
        KDirectory *dir;

        STSMSG(1, ("Cache size: %uM\n", settings->cache_size / 1024 / 1024));
        
        rc = KLoadProgressbar_Make(&ctx->progress[0], 0); if (rc) return rc;
        rc = KLoadProgressbar_Make(&ctx->progress[1], 0); if (rc) return rc;
        rc = KLoadProgressbar_Make(&ctx->progress[2], 0); if (rc) return rc;
        rc = KLoadProgressbar_Make(&ctx->progress[3], 0); if (rc) return rc;
        
        KLoadProgressbar_Append(ctx->progress[0], 100 * settings->numfiles);
        
        rc = KDirectoryNativeDir(&dir);
        if (rc == 0)
            rc = OpenMMapFile(settings, ctx, dir);
        if (rc == 0)
            rc = OpenMBankFile(settings, ctx, dir);
        KDirectoryRelease(dir);
    }
    return rc;
}

void ContextReleaseMemBank(SpotAssembler *ctx)
{
    KMemBankRelease(ctx->fragsBoth);
    ctx->fragsBoth = NULL;
}

void ContextRelease(SpotAssembler *ctx)
{
    KLoadProgressbar_Release(ctx->progress[0], true);
    KLoadProgressbar_Release(ctx->progress[1], true);
    KLoadProgressbar_Release(ctx->progress[2], true);
    KLoadProgressbar_Release(ctx->progress[3], true);
    MMArrayWhack(ctx->id2value);
}

static
rc_t WriteSoloFragments(const CommonWriterSettings* settings, SpotAssembler* ctx, SequenceWriter *seq)
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
            uint64_t id;
            uint64_t sz;
            unsigned readLen[2];
            unsigned read = 0;
            FragmentInfo const *fip;
            uint8_t const *src;
            KMemBank *frags = ctx->fragsBoth;
            
            rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
            if (rc)
                break;
            KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);

            id = value->fragmentId;
            if (id == 0)
                continue;
            
            rc = KMemBankSize(frags, id, &sz);
            if (rc) {
                (void)LOGERR(klogErr, rc, "KMemBankSize failed");
                break;
            }
            rc = KDataBufferResize(&fragBuf, (size_t)sz);
            if (rc) {
                (void)LOGERR(klogErr, rc, "KDataBufferResize failed");
                break;
            }
            rc = KMemBankRead(frags, id, 0, fragBuf.base, sz, &rsize);
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
            memmove(srec.seq + srec.readStart[read], src, srec.readLen[read]);
            src += fip->readlen;
            
            memmove(srec.qual + srec.readStart[read], src, srec.readLen[read]);
            src += fip->readlen;
            srec.spotGroup = (char *)src;
            srec.spotGroupLen = fip->sglen;
            srec.keyId = keyId;
            
            rc = SequenceWriteRecord(seq, &srec, ctx->isColorSpace, value->pcr_dup, value->platform, 
                                    settings->keepMismatchQual, settings->no_real_output, settings->hasTI, settings->QualQuantizer);
            if (rc) {
                (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                break;
            }
            /*rc = KMemBankFree(frags, id);*/
            CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
        }
    }
    /*printf("DONE_SOLO:\tcnt2=%d\tcnt1=%d\n",fcountBoth,fcountOne);*/
    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&srec.storage);
    return rc;
}

static
rc_t AlignmentUpdateSpotInfo(SpotAssembler *ctx, struct AlignmentWriter *align, uint64_t maxDistance)
{
    rc_t rc;
    
    ++ctx->pass;

    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->alignCount);

    rc = AlignmentStartUpdatingSpotIds(align);
    while (rc == 0 && (rc = Quitting()) == 0) {
        ctx_value_t const *value;
        uint64_t keyId;
        int64_t alignId;
        bool isPrimary;
        
        rc = AlignmentGetSpotKey(align, &keyId, &alignId, &isPrimary);
        if (rc) {
            if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                rc = 0;
            break;
        }
        assert(keyId >> 32 < ctx->key2id_count);
        assert((uint32_t)keyId < ctx->idCount[keyId >> 32]);
        rc = MMArrayGet(ctx->id2value, (void **)&value, keyId);
        if (rc == 0) {
            int64_t const spotId = CTX_VALUE_GET_S_ID(*value);
            int64_t const id[] = {
                CTX_VALUE_GET_P_ID(*value, 0),
                CTX_VALUE_GET_P_ID(*value, 1)
            };
            int64_t mateId = 0;
            ReferenceStart mateGlobalRefPos;
            
            memset(&mateGlobalRefPos, 0, sizeof(mateGlobalRefPos));
            if (spotId == 0) {
                assert(!isPrimary);
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(id)' was never assigned a spot id, probably has no primary alignments", "id=%lx", keyId));
                /* (void)PLOGMSG(klogWarn, (klogWarn, "Spot #$(i): { $(s) }", "i=%lu,s=%s", keyId, Print_ctx_value_t(value))); */
            }
            if (isPrimary) {
                if (id[0] != 0 && id[1] != 0)
                    mateId = alignId == id[0] ? id[1] : id[0];
                
                if (mateId && maxDistance && (id[0] > id[1] ? id[0] - id[1] : id[1] - id[0]) > maxDistance) {
                    rc = AlignmentGetRefPos(align, mateId, &mateGlobalRefPos);
                    if (rc) break;
                }
            }
            rc = AlignmentUpdateInfo(align, spotId, mateId, &mateGlobalRefPos);
        }
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
    }
    return rc;
}

rc_t SequenceUpdateAlignInfo(SpotAssembler *ctx, struct SequenceWriter *seq)
{
    rc_t rc = 0;
    uint64_t row;
    const ctx_value_t *value;
    uint64_t keyId;
    
    ++ctx->pass;
    KLoadProgressbar_Append(ctx->progress[ctx->pass - 1], ctx->spotId + 1);
    
    for (row = 1; row <= ctx->spotId; ++row) {
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
        if (row != CTX_VALUE_GET_S_ID(*value)) {
            rc = RC(rcApp, rcTable, rcWriting, rcData, rcUnexpected);
            (void)PLOGMSG(klogErr, (klogErr, "Unexpected spot id $(spotId) for row $(row), index $(idx)", "spotId=%u,row=%u,idx=%u", (unsigned)CTX_VALUE_GET_S_ID(*value), (unsigned)row, (unsigned)keyId));
            break;
        }
        {{
            int64_t primaryId[2];
            /*uint64_t const spotId = CTX_VALUE_GET_S_ID(*value);*/
            
            primaryId[0] = CTX_VALUE_GET_P_ID(*value, 0);
            primaryId[1] = CTX_VALUE_GET_P_ID(*value, 1);
            
            rc = SequenceUpdateAlignData(seq, row, value->unmated ? 1 : 2,
                                         primaryId,
                                         value->alignmentCount);
        }}
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failed updating AlignmentWriter data in sequence table");
            break;
        }
        KLoadProgressbar_Process(ctx->progress[ctx->pass - 1], 1, false);
    }
    return rc;
}

void EditAlignedQualities(const CommonWriterSettings* settings, uint8_t qual[], bool const hasMismatch[], unsigned readlen) /* generic */
{
    unsigned i;
    
    for (i = 0; i < readlen; ++i) {
        uint8_t const q = hasMismatch[i] ? settings->alignedQualValue : qual[i];
        
        qual[i] = q;
    }
}

void EditUnalignedQualities(uint8_t qual[], bool const hasMismatch[], unsigned readlen) /* generic */
{
    unsigned i;
    
    for (i = 0; i < readlen; ++i) {
        uint8_t const q = (qual[i] & 0x7F) | (hasMismatch[i] ? 0x80 : 0);
        
        qual[i] = q;
    }
}

bool platform_cmp(char const platform[], char const test[])
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

rc_t CheckLimitAndLogError(CommonWriterSettings* settings)
{
    ++settings->errCount;
    if (settings->maxErrCount > 0 && settings->errCount > settings->maxErrCount) {
        (void)PLOGERR(klogErr, (klogErr, RC(rcAlign, rcFile, rcReading, rcError, rcExcessive), 
                                "Number of errors $(cnt) exceeds limit of $(max): Exiting", "cnt=%lu,max=%lu", 
                                settings->errCount, settings->maxErrCount));
        return RC(rcAlign, rcFile, rcReading, rcError, rcExcessive);
    }
    return 0;
}

void RecordNoMatch(const CommonWriterSettings* settings, char const readName[], char const refName[], uint32_t const refPos)
{
    if (settings->noMatchLog) {
        static uint64_t lpos = 0;
        char logbuf[256];
        size_t len;
        
        if (string_printf(logbuf, sizeof(logbuf), &len, "%s\t%s\t%u\n", readName, refName, refPos) == 0) {
            KFileWrite(settings->noMatchLog, lpos, logbuf, len, NULL);
            lpos += len;
        }
    }
}

rc_t LogNoMatch(CommonWriterSettings* settings, char const readName[], char const refName[], unsigned rpos, unsigned matches)
{
    rc_t const rc = CheckLimitAndLogError(settings);
    static unsigned count = 0;
    
    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    }
    else if (settings->maxWarnCount_NoMatch == 0 || count < settings->maxWarnCount_NoMatch)
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    return rc;
}

rc_t LogDupConflict(CommonWriterSettings* settings, char const readName[])
{
    rc_t const rc = CheckLimitAndLogError(settings);
    static unsigned count = 0;
    
    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    }
    else if (settings->maxWarnCount_DupConflict == 0 || count < settings->maxWarnCount_DupConflict)
        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    return rc;
}

void COPY_QUAL(uint8_t D[], uint8_t const S[], unsigned const L, bool const R)
{
    if (R) {
        unsigned i;
        unsigned j;
        
        for (i = 0, j = L - 1; i != L; ++i, --j)
            D[i] = S[j];
    }
    else
        memmove(D, S, L);
}

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
        memmove(D, S, L);
}

/*--------------------------------------------------------------------------
 * ArchiveFile
 */
static uint8_t GetMapQ(Alignment const *rec)
{
    uint8_t mapQ;
    
    AlignmentGetMapQuality(rec, &mapQ);
    return mapQ;
}

INSDC_SRA_platform_id PlatformToId(const char* name)
{
    if (name != NULL)
    {
        switch (toupper(name[0])) {
        case 'C':
            if (platform_cmp(name, "COMPLETE GENOMICS") || platform_cmp(name, "COMPLETE_GENOMICS"))
                return SRA_PLATFORM_COMPLETE_GENOMICS;
            if (platform_cmp(name, "CAPILLARY"))
                return SRA_PLATFORM_CAPILLARY;
            break;
        case 'H':
            if (platform_cmp(name, "HELICOS"))
                return SRA_PLATFORM_HELICOS;
            break;
        case 'I':
            if (platform_cmp(name, "ILLUMINA"))
                return SRA_PLATFORM_ILLUMINA;
            if (platform_cmp(name, "IONTORRENT"))
                return SRA_PLATFORM_ION_TORRENT;
            break;
        case 'L':
            if (platform_cmp(name, "LS454"))
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
            if (platform_cmp(name, "PACBIO"))
                return SRA_PLATFORM_PACBIO_SMRT;
            break;
        case 'S':
            if (platform_cmp(name, "SOLID"))
                return SRA_PLATFORM_ABSOLID;
            if (platform_cmp(name, "SANGER"))
                return SRA_PLATFORM_CAPILLARY;
            break;
        default:
            break;
        }
    }
    return SRA_PLATFORM_UNDEFINED;
}

INSDC_SRA_platform_id GetINSDCPlatform(ReferenceInfo const *ref, char const name[]) {
    if (ref != NULL && name != NULL) {
        ReadGroup rg;

        rc_t rc = ReferenceInfoGetReadGroupByName(ref, name, &rg);
        if (rc == 0 && rg.platform) 
            return PlatformToId(rg.platform) ;
    }
    return SRA_PLATFORM_UNDEFINED;
}

void ParseSpotName(char const name[], size_t* namelen)
{ /* remove trailing #... */
    const char* hash = string_chr(name, *namelen, '#');
    if (hash)
        *namelen = hash - name;
}


static const int8_t toPhred[] =
{
              0, 1, 1, 2, 2, 3, 3,
     4, 4, 5, 5, 6, 7, 8, 9,10,10,
    11,12,13,14,15,16,17,18,19,20,
    21,22,23,24,25,26,27,28,29,30,
    31,32,33,34,35,36,37,38,39,40
};

static int8_t LogOddsToPhred ( int8_t logOdds )
{ /* conversion table copied from interface/ncbi/seq.vschema */
    if (logOdds < -6)
        return 0;
    if (logOdds > 40)
        return 40;
    return toPhred[logOdds + 6];
}

rc_t ArchiveFile(const struct ReaderFile *const reader,
                 CommonWriterSettings *const G,
                 struct SpotAssembler *const ctx,
                 struct Reference *const ref,
                 struct SequenceWriter *const seq,
                 struct AlignmentWriter *const align,
                 bool *const had_alignments,
                 bool *const had_sequences)
{
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
    int skipRefSeqId = -1;
    int unmapRefSeqId = -1;
    uint64_t recordsProcessed = 0;
    uint64_t filterFlagConflictRecords=0; /*** counts number of conflicts between flags 'duplicate' and 'lowQuality' ***/
#define MAX_WARNINGS_FLAG_CONFLICT 10000 /*** maximum errors to report ***/

    bool isColorSpace = false;
    bool isNotColorSpace = G->noColorSpace;
    char alignGroup[32];
    size_t alignGroupLen;
    AlignmentRecord data;
    
    const ReferenceInfo* header = NULL;

    memset(&data, 0, sizeof(data));
    
    rc = ReaderFileGetReferenceInfo(reader, &header);
    if (rc)
        return rc;

    if (ctx->key2id_max == 0) {
        if (header != NULL) {
            uint32_t rgcount;
            unsigned rgi;
            
            ReferenceInfoGetReadGroupCount(header, &rgcount);
            if (rgcount > (sizeof(ctx->key2id)/sizeof(ctx->key2id[0]) - 1))
                ctx->key2id_max = 1;
            else
                ctx->key2id_max = sizeof(ctx->key2id)/sizeof(ctx->key2id[0]);
            
            for (rgi = 0; rgi != rgcount; ++rgi) {
                ReadGroup rg;
                
                rc_t rc2 = ReferenceInfoGetReadGroup(header, rgi, &rg);
                if (rc2 == 0 && rg.platform && platform_cmp(rg.platform, "CAPILLARY")) {
                    G->hasTI = true;
                    break;
                }
            }
        }
        else
            ctx->key2id_max = 1;        
    }
    
    memset(&srec, 0, sizeof(srec));
    
    rc = KDataBufferMake(&cigBuf, 32, 0);
    if (rc)
        return rc;
    
    rc = KDataBufferMake(&fragBuf, 8, FRAG_CHUNK_SIZE);
    if (rc)
        return rc;
    
    rc = KDataBufferMake(&buf, 16, 0);
    if (rc)
        return rc;
    
    if (rc == 0) {
        (void)PLOGMSG(klogInfo, (klogInfo, "Loading '$(file)'", "file=%s", ReaderFileGetPathname(reader)));
    }
    
    *had_alignments = false;
    *had_sequences = false;
    
    while (rc == 0 && (rc = Quitting()) == 0) {
        bool aligned;
        uint32_t readlen;
        int64_t rpos=0;
        char *seqDNA;
        ReferenceSequence refSeq;
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
        int readOrientation;
        uint8_t rna_orient = ' ';
        
        const Record* record;
        const Sequence* sequence = NULL;
        const Alignment* alignment = NULL;
        const CGData* cg = NULL;

        rc = ReaderFileGetRecord(reader, &record);
        if (GetRCObject(rc) == rcRow && (GetRCState(rc) == rcInvalid || GetRCState(rc) == rcEmpty)) {
            (void)PLOGERR(klogWarn, (klogWarn, rc, "ArchiveFile: '$(file)' - row $(row)", "file=%s,row=%lu", ReaderFileGetPathname(reader), reccount + 1));
            rc = CheckLimitAndLogError(G);
        }
        else if (rc || record == 0)
            break;

        {
            const Rejected* rej;
            rc = RecordGetRejected(record, &rej);
            if (rc)
            {
                (void)LOGERR(klogErr, rc, "ArchiveFile: RecordGetSequence failed");
                break;
            }
            if (rej != NULL)
            {
                const char* message;
                uint64_t line;
                uint64_t col;
                bool fatal;
                rc = RejectedGetError(rej, &message, &line, &col, &fatal);
                if (rc)
                {
                    (void)LOGERR(klogErr, rc, "ArchiveFile: RejectedGetError failed");
                    break;
                }
                (void)PLOGMSG(fatal ? klogErr : klogWarn, (fatal ? klogErr : klogWarn, 
                              "$(file):$(l):$(c):$(msg)", "file=%s,l=%lu,c=%lu,msg=%s", 
                              ReaderFileGetPathname(reader), line, col, message));
                rc = CheckLimitAndLogError(G);
                RejectedRelease(rej);
                
                if (fatal)
                {
                    rc = RC(rcExe, rcFile, rcParsing, rcFormat, rcUnsupported);
                    break;
                }
                    
                goto LOOP_END;
            }
        }
        rc = RecordGetSequence(record, &sequence);
        if (rc)
        {
            (void)LOGERR(klogErr, rc, "ArchiveFile: RecordGetSequence failed");
            break;
        }
        rc = RecordGetAlignment(record, &alignment);
        if (rc)
        {
            (void)LOGERR(klogErr, rc, "ArchiveFile: RecordGetAlignment failed");
            break;
        }
        if (alignment != NULL)
        {
            rc = AlignmentGetCGData(alignment, &cg);      
            if (rc)
            {
                (void)LOGERR(klogErr, rc, "ArchiveFile: AlignmentGetCG failed");
                break;
            }
        }
        
        if ((unsigned)(ReaderFileGetProportionalPosition(reader) * 100.0) > progress) { 
            unsigned new_value = ReaderFileGetProportionalPosition(reader) * 100.0;
            KLoadProgressbar_Process(ctx->progress[0], new_value - progress, false);
            progress = new_value;
        }


        /**************************************************************/
        if (!G->noColorSpace) {
            if (SequenceIsColorSpace(sequence)) {
                if (isNotColorSpace) {
                MIXED_BASE_AND_COLOR:
                    rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);  
                    (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains base space and color space", "file=%s", ReaderFileGetPathname(reader)));
                    goto LOOP_END;
                }
                ctx->isColorSpace = isColorSpace = true;
            }
            else if (isColorSpace)
                goto MIXED_BASE_AND_COLOR;
            else
                isNotColorSpace = true;
        }
        hasCG = cg != NULL;
        if (hasCG) {
            AlignmentGetAlignOpCount(alignment, &opCount);
            rc = KDataBufferResize(&cigBuf, opCount * 2 + 5);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                goto LOOP_END;
            }
            
            rc = AlignmentRecordInit(&data, readlen = 35, G->expectUnsorted, G->compressQuality);
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
            if (alignment != 0)
            {
                uint32_t const *tmp;
                AlignmentGetBAMCigar(alignment, &tmp, &opCount);
                rc = KDataBufferResize(&cigBuf, opCount);
                if (rc) {
                    (void)LOGERR(klogErr, rc, "Failed to resize CIGAR buffer");
                    goto LOOP_END;
                }
                memmove(cigBuf.base, tmp, opCount * sizeof(uint32_t));
            }
            
            SequenceGetReadLength(sequence, &readlen);
            if (isColorSpace) {
                SequenceGetCSReadLength(sequence, &csSeqLen);
                if (readlen > csSeqLen) {
                    rc = RC(rcAlign, rcRow, rcReading, rcData, rcInconsistent);
                    (void)LOGERR(klogErr, rc, "SequenceWriter length and CS SequenceWriter length are inconsistent");
                    goto LOOP_END;
                }
                else if (readlen < csSeqLen)
                {
                    readlen = 0; /* will be hard clipped */
                }
            }
            else if (readlen == 0) {
            }
            rc = AlignmentRecordInit(&data, readlen | csSeqLen, G->expectUnsorted, G->compressQuality);
            if (rc == 0)
	            rc = KDataBufferResize(&buf, readlen | csSeqLen);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to resize record buffer");
                goto LOOP_END;
            }
            
            seqDNA = buf.base;
            qual = (uint8_t *)&seqDNA[readlen | csSeqLen];
        }
        SequenceGetSpotName(sequence, &name, &namelen);
        if (G->parseSpotName)
            ParseSpotName(name, &namelen);
        SequenceGetRead(sequence, seqDNA);
        
        {
            int8_t const *squal;
            uint8_t qoffset = 0;
            int qualType=0;            
            unsigned i;
            
            rc = SequenceGetQuality(sequence, &squal, &qoffset, &qualType);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of original quality does not match sequence", 
                                                        "name=%.*s", (uint32_t)namelen, name));
                rc = CheckLimitAndLogError(G);
                goto LOOP_END;
            }
            else if (squal == NULL)
                memset(qual, 30, readlen); /* SRA-2932: 30 is the preferred quality value for fasta */
            else 
            {  
                if (G->useQUAL)
                    memmove(qual, squal, readlen);
                else
                {
                    switch (qualType)
                    {
                    case QT_LogOdds:
                        for (i = 0; i != readlen; ++i)
                            qual[i] = LogOddsToPhred(squal[i] - qoffset);
                        break;
                        
                    case QT_Phred:
                        if (qoffset) {
                            for (i = 0; i != readlen; ++i)
                                qual[i] = squal[i] - qoffset;
                        }
                        else
                            memmove(qual, squal, readlen);
                        break;
                        
                    default:
                        memmove(qual, squal, readlen);
                        break;
                    }
                }
            }
        }
        
        if (hasCG) {
            rc = CGDataGetSeqQual(cg, seqDNA, qual);
            if (rc == 0)
                rc = CGDataGetCigar(cg, cigBuf.base, cigBuf.elem_count, &opCount);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed to read CG data");
                goto LOOP_END;
            }
        }
        if (G->hasTI) {
            rc = SequenceGetTI(sequence, &ti);
            if (rc)
                ti = 0;
            rc = 0;
        }
        data.data.align_group.buffer = alignGroup;
        if (hasCG && CGDataGetAlignGroup(cg, alignGroup, sizeof(alignGroup), &alignGroupLen) == 0)
            data.data.align_group.elements = alignGroupLen;
        else
            data.data.align_group.elements = 0;

        if (alignment != 0)
            AR_MAPQ(data) = GetMapQ(alignment);
        {{
            char const *rgname;
            size_t rgnamelen;

            SequenceGetSpotGroup(sequence, &rgname, &rgnamelen);
            if (rgname)
            {
                string_copy(spotGroup, sizeof(spotGroup), rgname, rgnamelen);
                spotGroup[rgnamelen] = '\0';
            }
            else
                spotGroup[0] = '\0';
        }}        
        readOrientation = SequenceGetOrientationSelf(sequence);
        AR_REF_ORIENT(data) = readOrientation == ReadOrientationReverse;
        isPrimary = alignment == 0 || ! AlignmentIsSecondary(alignment);
        if (G->noSecondary && !isPrimary)
            goto LOOP_END;
        originally_aligned = (alignment != 0);
        aligned = originally_aligned && (AR_MAPQ(data) >= G->minMapQual);
        
        if (isColorSpace && readlen == 0)   /* detect hard clipped colorspace   */
            aligned = false;                /* reads and make unaligned         */

        if (aligned && align == NULL) {
            rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
            (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains aligned records", "file=%s", ReaderFileGetPathname(reader)));
            goto LOOP_END;
        }
        if (header != NULL)
        {
            while (aligned) {
                AlignmentGetPosition(alignment, &rpos);
                AlignmentGetRefSeqId(alignment, &refSeqId);
                if (rpos >= 0 && refSeqId >= 0) {
                    if (refSeqId == skipRefSeqId)
                        goto LOOP_END;
                    if (refSeqId == unmapRefSeqId) {
                        aligned = false;
                        break;
                    }
                    if (refSeqId == lastRefSeqId)
                        break;
                    rc = ReferenceInfoGetRefSeq(header, refSeqId, &refSeq);
                    if (rc != 0) {
                        rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                        (void)PLOGERR(klogWarn, (klogWarn, rc, "File '$(file)': Spot '$(name)' refers to an unknown Reference number $(refSeqId)", "file=%s,refSeqId=%i,name=%s", ReaderFileGetPathname(reader), (int)refSeqId, name));
                        rc = CheckLimitAndLogError(G);
                        goto LOOP_END;
                    }
                    else {
                        bool shouldUnmap = false;
                        
                        if (G->refFilter && strcmp(G->refFilter, refSeq.name) != 0) {
                            (void)PLOGMSG(klogInfo, (klogInfo, "Skipping Reference '$(name)'", "name=%s", refSeq.name));
                            skipRefSeqId = refSeqId;
                            goto LOOP_END;
                        }
                        
                        rc = ReferenceSetFile(ref, refSeq.name, refSeq.length, refSeq.checksum, G->maxSeqLen, &shouldUnmap);
                        if (rc == 0) {
                            lastRefSeqId = refSeqId;
                            if (shouldUnmap) {
                                unmapRefSeqId = refSeqId;
                                aligned = false;
                            }
                            break;
                        }
                        if (GetRCObject(rc) == rcConstraint && GetRCState(rc) == rcViolated) {
                            int const level = G->limit2config ? klogWarn : klogErr;
                            
                            (void)PLOGMSG(level, (level, "Could not find a Reference to match { name: '$(name)', length: $(rlen) }", "name=%s,rlen=%u", refSeq.name, (unsigned)refSeq.length));
                        }
                        else if (!G->limit2config)
                            (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)': Spot '$(sname)' refers to an unknown Reference '$(rname)'", "file=%s,rname=%s,sname=%s", ReaderFileGetPathname(reader), refSeq.name, name));
                        if (G->limit2config)
                            rc = 0;
                        goto LOOP_END;
                    }
                }
                else {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' was marked aligned, but reference id = $(id) and reference position = $(pos) are invalid", "name=%.*s,id=%i,pos=%i", namelen, name, refSeqId, rpos));
                    if ((rc = CheckLimitAndLogError(G)) != 0) goto LOOP_END;
                }

                aligned = false;
            }
        }
        if (!aligned && (G->refFilter != NULL || G->limit2config))
            goto LOOP_END;
        
        rc = GetKeyID(G, ctx, &keyId, &wasInserted, spotGroup, name, namelen);
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
        if (SequenceWasPaired(sequence)) {
            if (SequenceIsFirst(sequence))
                AR_READNO(data) |= 1;
            if (SequenceIsSecond(sequence))
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
                break;
            case 3:
                if ((warned & 2) == 0) {
                    (void)LOGMSG(klogWarn, "Spots with more than two fragments have been encountered");
                    warned |= 2;
                }
                break;
            }
        }
        if (!mated)
            AR_READNO(data) = 1;
        
        if (wasInserted) {
            memset(value, 0, sizeof(*value));
            value->unmated = !mated;
            value->pcr_dup = SequenceIsDuplicate(sequence);
            value->platform = GetINSDCPlatform(header, spotGroup);
            if (value->platform == SRA_PLATFORM_UNDEFINED)
                value->platform = G->platform;
        }
        else {
            if (!G->acceptBadDups && value->pcr_dup != SequenceIsDuplicate(sequence)) {
                rc = LogDupConflict(G, name);
                goto LOOP_END; /* TODO: is this correct? */
            }
            value->pcr_dup &= SequenceIsDuplicate(sequence);
            if (mated && value->unmated) {
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen without mate info, now has mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError(G);
                goto LOOP_END;
            }
            else if (!mated && !value->unmated) {
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen with mate info, now has no mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError(G);
                goto LOOP_END;
            }
        }
        
        ++recordsProcessed;

        if (isPrimary) {
            switch (AR_READNO(data)) {
            case 1:
                if (CTX_VALUE_GET_P_ID(*value, 0) != 0)
                {
                    isPrimary = false;
                    if (value->seqHash[0] != SeqHashKey(seqDNA, readlen))
                    {
                        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                                 "Read 1 of spot '$(name)', possible sequence mismatch", "name=%s", name));
                        rc = CheckLimitAndLogError(G);
                        goto LOOP_END;
                    }
                }
                else if (aligned && value->unaligned_1) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Read 1 of spot '$(name)', which was unmapped, is now being mapped at position $(pos) on reference '$(ref)'; this alignment will be considered as secondary", "name=%s,ref=%s,pos=%u", name, refSeq.name, rpos));
                    isPrimary = false;
                }
                break;
            case 2:
                if (CTX_VALUE_GET_P_ID(*value, 1) != 0)
                {
                    isPrimary = false;
                    if (value->seqHash[1] != SeqHashKey(seqDNA, readlen))
                    {
                        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                                 "Read 2 of spot '$(name)', possible sequence mismatch", "name=%s", name));
                        rc = CheckLimitAndLogError(G);
                        goto LOOP_END;
                    }
                }
                else if (aligned && value->unaligned_2) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Read 2 of spot '$(name)', which was unmapped, is now being mapped at position $(pos) on reference '$(ref)'; this alignment will be considered as secondary", "name=%s,ref=%s,pos=%u", name, refSeq.name, rpos));
                    isPrimary = false;
                }
                break;
            default:
                break;
            }
        }
        data.isPrimary = isPrimary;
        if (aligned) {
            uint32_t matches = 0;
            
            /* TODO: get rna orientation from XS:A tag */
            rc = ReferenceRead(ref, &data, rpos, cigBuf.base, opCount,
                               seqDNA, readlen, rna_orient,
                               &matches, G->acceptNoMatch, G->minMatchCount, G->maxSeqLen);
            if (rc) {
                aligned = false;
                
                if (   (GetRCState(rc) == rcViolated  && GetRCObject(rc) == rcConstraint)
                    || (GetRCState(rc) == rcExcessive && GetRCObject(rc) == rcRange))
                {
                    RecordNoMatch(G, name, refSeq.name, rpos);
                }
                if (GetRCState(rc) == rcViolated && GetRCObject(rc) == rcConstraint) {
                    rc = LogNoMatch(G, name, refSeq.name, (unsigned)rpos, (unsigned)matches);
                }
                else if (GetRCObject(rc) == (enum RCObject)rcData && GetRCState(rc) == rcInvalid) {
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': bad alignment to reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq.name, rpos));
                    rc = CheckLimitAndLogError(G);
                }
                else if (GetRCObject(rc) == (enum RCObject)rcData) {
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': bad alignment to reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq.name, rpos));
                    rc = CheckLimitAndLogError(G);
                }
                else {
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "Spot '$(name)': error reading reference '$(ref)' at $(pos)", "name=%s,ref=%s,pos=%u", name, refSeq.name, rpos));
                    rc = CheckLimitAndLogError(G);
                }
                if (rc) goto LOOP_END;
            }
        }
        if (isColorSpace) {
            /* must be after ReferenceRead */
            SequenceGetCSKey(sequence, &cskey);
            SequenceGetCSRead(sequence, seqDNA);
            if (!aligned && !G->useQUAL) {
                int8_t const *squal;
                uint8_t qoffset = 0;
                int qualType=0;                
                
                rc = SequenceGetCSQuality(sequence, &squal, &qoffset, &qualType);
                if (rc) {
                    (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of colorspace quality does not match sequence", "name=%s", name));
                    goto LOOP_END;
                }
                if (qoffset) {
                    unsigned i;
                    
                    for (i = 0; i < csSeqLen; ++i)
                        qual[i] = squal[i] - qoffset;
                }
                else if (squal != NULL)
                    memmove(qual, squal, csSeqLen);
                else
                    memset(qual, 0, csSeqLen);
                readlen = csSeqLen;
            }
        }
        
        if (aligned) {
            if (G->editAlignedQual ) EditAlignedQualities  (G, qual, AR_HAS_MISMATCH(data), readlen);
            if (G->keepMismatchQual) EditUnalignedQualities(qual, AR_HAS_MISMATCH(data), readlen);

            if (AR_MISMATCH_QUAL(data) == NULL) {
                AR_NUM_MISMATCH_QUAL(data) = 0;
            }
            else {
                size_t i;
                size_t n;
                bool const *const has_mismatch = AR_HAS_MISMATCH(data);
                uint8_t *const mismatch = AR_MISMATCH_QUAL(data);
                
                for (n = i = 0; i < readlen; ++i) {
                    if (has_mismatch[i])
                        mismatch[n++] = qual[i];
                }
                AR_NUM_MISMATCH_QUAL(data) = n;
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
                    uint64_t    fragmentId;
                    FragmentInfo fi;
                    KMemBank *frags = ctx->fragsBoth;
                    int32_t mate_refSeqId = -1;
                    int64_t pnext = 0;
                    
                    value->seqHash[AR_READNO(data) - 1] = SeqHashKey(seqDNA, readlen);
                    
                    memset(&fi, 0, sizeof(fi));
                    fi.aligned = aligned;
                    fi.ti = ti;
                    fi.orientation = readOrientation;
                    fi.otherReadNo = AR_READNO(data);
                    fi.sglen   = strlen(spotGroup);
                    fi.readlen = readlen;
                    fi.cskey = cskey;
                    fi.is_bad = SequenceIsLowQuality(sequence);
                    sz = sizeof(fi) + 2*fi.readlen + fi.sglen;
                    if (align && aligned) {
                        AlignmentGetMateRefSeqId(alignment, &mate_refSeqId);
                        AlignmentGetMatePosition(alignment, &pnext);
                    }
                    rc = KMemBankAlloc(frags, &fragmentId, sz, 0);
                    value->fragmentId = fragmentId;
                    if (rc) {
                        (void)LOGERR(klogErr, rc, "KMemBankAlloc failed");
                        goto LOOP_END;
                    }
                    
                    rc = KDataBufferResize(&fragBuf, sz);
                    if (rc) {
                        (void)LOGERR(klogErr, rc, "Failed to resize fragment buffer");
                        goto LOOP_END;
                    }
                    {{
                        uint8_t *dst = (uint8_t*) fragBuf.base;
                        memmove(dst,&fi,sizeof(fi));
                        dst += sizeof(fi);
                        COPY_READ((char *)dst, seqDNA, fi.readlen, (isColorSpace && !aligned) ? 0 : fi.orientation == ReadOrientationReverse);
                        dst += fi.readlen;
                        COPY_QUAL(dst, qual, fi.readlen, (isColorSpace && !aligned) ? 0 : fi.orientation == ReadOrientationReverse);
                        dst += fi.readlen;
                        memmove(dst,spotGroup,fi.sglen);
                    }}
                    rc = KMemBankWrite(frags, fragmentId, 0, fragBuf.base, sz, &rsize);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankWrite failed writing fragment $(id)", "id=%u", fragmentId));
                        goto LOOP_END;
                    }
                    value->has_a_read = 1;
                }
                else if (value->fragmentId != 0 ) {
                    /* might be second fragment */
                    uint64_t sz;
                    FragmentInfo *fip;
                    KMemBank *frags = ctx->fragsBoth;
                    
                    rc = KMemBankSize(frags, value->fragmentId, &sz);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "KMemBankSize failed on fragment $(id)", "id=%u", value->fragmentId));
                        goto LOOP_END;
                    }
                    rc = KDataBufferResize(&fragBuf, (size_t)sz);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to resize fragment buffer", ""));
                        goto LOOP_END;
                    }
                    rc = KMemBankRead(frags, value->fragmentId, 0, fragBuf.base, sz, &rsize);
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
                        
                        value->seqHash[AR_READNO(data) - 1] = SeqHashKey(seqDNA, readlen);
                        
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
                        memmove(srec.seq + srec.readStart[read1], src, fip->readlen);
                        src += fip->readlen;
                        memmove(srec.qual + srec.readStart[read1], src, fip->readlen);
                        src += fip->readlen;
                        
                        srec.orientation[read2] = readOrientation;
                        COPY_READ(srec.seq + srec.readStart[read2], 
                                  seqDNA, 
                                  srec.readLen[read2], 
                                  (isColorSpace && !aligned) ? 0 : srec.orientation[read2] == ReadOrientationReverse);
                        COPY_QUAL(srec.qual + srec.readStart[read2], 
                                  qual, 
                                  srec.readLen[read2],  
                                  (isColorSpace && !aligned) ? 0 : srec.orientation[read2] == ReadOrientationReverse);

                        srec.keyId = keyId;
                        srec.is_bad[read2] = SequenceIsLowQuality(sequence);
                        srec.aligned[read2] = aligned;
                        srec.cskey[read2] = cskey;
                        srec.ti[read2] = ti;
                        
                        srec.spotGroup = spotGroup;
                        srec.spotGroupLen = strlen(spotGroup);
                        if (value->pcr_dup && (srec.is_bad[0] || srec.is_bad[1])) {
                            filterFlagConflictRecords++;
                            if (filterFlagConflictRecords < MAX_WARNINGS_FLAG_CONFLICT) {
                                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)': both 'duplicate' and 'lowQuality' flag bits set, only 'duplicate' will be saved", "name=%s", name));
                            }
                            else if(filterFlagConflictRecords == MAX_WARNINGS_FLAG_CONFLICT) {
                                (void)PLOGMSG(klogWarn, (klogWarn, "Last reported warning: Spot '$(name)': both 'duplicate' and 'lowQuality' flag bits set, only 'duplicate' will be saved", "name=%s", name));
                            }
                        }
                        rc = SequenceWriteRecord(seq, &srec, isColorSpace, value->pcr_dup, value->platform, 
                                                G->keepMismatchQual, G->no_real_output, G->hasTI, G->QualQuantizer);
                        if (rc) {
                            (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                            goto LOOP_END;
                        }
                        CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
                        rc = KMemBankFree(frags, value->fragmentId);
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
                int64_t mrid;
                int64_t tlen;
                
                AlignmentGetMatePosition(alignment, &mpos);
                AlignmentGetMateRefSeqId(alignment, &bam_mrid);
                AlignmentGetInsertSize(alignment, &tlen);
                
                if (mpos >= 0 && bam_mrid >= 0 && tlen != 0 && header != NULL) {
                    ReferenceSequence mref;
                    if (ReferenceInfoGetRefSeq(header, bam_mrid, &mref) == 0) {
                        rc_t rc_temp = ReferenceGet1stRow(ref, &mrid, mref.name);
                        if (rc_temp == 0) {
                            data.mate_ref_pos = mpos;
                            data.template_len = tlen;
                            data.mate_ref_orientation = (SequenceGetOrientationMate(sequence) == ReadOrientationReverse);
                        }
                        else {
                            (void)PLOGERR(klogWarn, (klogWarn, rc_temp, "Failed to get refID for $(name)", "name=%s", mref.name));
                            mrid = 0;
                        }
                        data.mate_ref_id = mrid;
                    }
                }
            }
        }
        else if (CTX_VALUE_GET_S_ID(*value) == 0 && (isPrimary || !originally_aligned)) {
            /* new unmated fragment - no spot assembly */
            unsigned readLen[1];

            value->seqHash[0] = SeqHashKey(seqDNA, readlen);
            
            readLen[0] = readlen;
            rc = SequenceRecordInit(&srec, 1, readLen);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "Failed resizing sequence record buffer", ""));
                goto LOOP_END;
            }
            srec.ti[0] = ti;
            srec.aligned[0] = aligned;
            srec.is_bad[0] = SequenceIsLowQuality(sequence);
            srec.orientation[0] = readOrientation;
            srec.cskey[0] = cskey;
            COPY_READ(srec.seq  + srec.readStart[0], seqDNA, readlen, (isColorSpace && !aligned) ? 0 : srec.orientation[0] == ReadOrientationReverse);
            COPY_QUAL(srec.qual + srec.readStart[0], qual, readlen, (isColorSpace && !aligned) ? 0 : srec.orientation[0] == ReadOrientationReverse);
         
            srec.keyId = keyId;
            
            srec.spotGroup = spotGroup;
            srec.spotGroupLen = strlen(spotGroup);
            if (value->pcr_dup && srec.is_bad[0]) {
                filterFlagConflictRecords++;
                if (filterFlagConflictRecords < MAX_WARNINGS_FLAG_CONFLICT) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)': both 'duplicate' and 'lowQuality' flag bits set, only 'duplicate' will be saved", "name=%s", name));
                }
                else if (filterFlagConflictRecords == MAX_WARNINGS_FLAG_CONFLICT) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Last reported warning: Spot '$(name)': both 'duplicate' and 'lowQuality' flag bits set, only 'duplicate' will be saved", "name=%s", name));
                }
            }
            
            srec.spotName = (char*)name;
            srec.spotNameLen = namelen;
            
            rc = SequenceWriteRecord(seq, &srec, isColorSpace, value->pcr_dup, value->platform, 
                                                G->keepMismatchQual, G->no_real_output, G->hasTI, G->QualQuantizer);
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
            
            rc = AlignmentWriteRecord(align, &data, G->expectUnsorted);
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
        RecordRelease(record);
        if (sequence != NULL)
            SequenceRelease(sequence);
        if (alignment != NULL)
            AlignmentRelease(alignment);
        if (cg != NULL)
            CGDataRelease(cg);
        
        ++reccount;
        if (G->maxAlignCount > 0 && reccount >= G->maxAlignCount)
            break;
        if (rc == 0)
            *had_sequences = true;
    }
    
    if (header != NULL)
        ReferenceInfoRelease(header);
    
    if (filterFlagConflictRecords > 0) {
        (void)PLOGMSG(klogWarn, (klogWarn, "$(cnt1) out of $(cnt2) records contained warning : both 'duplicate' and 'lowQuality' flag bits set, only 'duplicate' will be saved", "cnt1=%lu,cnt2=%lu", filterFlagConflictRecords,recordsProcessed));
    }
    if (rc == 0 && recordsProcessed == 0) {
        (void)LOGMSG(klogWarn, (G->limit2config || G->refFilter != NULL) ? 
                     "All records from the file were filtered out" :
                     "The file contained no records that were processed.");
        rc = RC(rcAlign, rcFile, rcReading, rcData, rcEmpty);
    }
    if (rc == 0 && reccount > 0) {
        double percentage = ((double)G->errCount) / reccount; 
        double allowed = G->maxErrPct/ 100.0;
        if (percentage > allowed) {
            rc = RC(rcExe, rcTable, rcClosing, rcData, rcInvalid);
            (void)PLOGERR(klogErr, 
                            (klogErr, rc,
                             "Too many bad records: "
                                 "records: $(records), bad records: $(bad_records), "
                                 "bad records percentage: $(percentage), "
                                 "allowed percentage: $(allowed)",
                             "records=%lu,bad_records=%lu,percentage=%.2f,allowed=%.2f",
                             reccount, G->errCount, percentage, allowed));
        }
    }

    KDataBufferWhack(&buf);
    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&srec.storage);
    KDataBufferWhack(&cigBuf);
    KDataBufferWhack(&data.buffer);
    return rc;
}

/*--------------------------------------------------------------------------
 * CommonWriter
 */
 
rc_t CommonWriterInit(CommonWriter* self, struct VDBManager *mgr, struct VDatabase *db, const CommonWriterSettings* G)
{
    rc_t rc; 
    assert(self);
    assert(mgr);
    assert(db);
    
    memset(self, 0, sizeof(*self));
    if (G)
        self->settings = *G;

    self->ref = malloc(sizeof(*self->ref));
    if (self->ref == 0)
        return RC(rcAlign, rcArc, rcAllocating, rcMemory, rcExhausted);
        
    rc = ReferenceInit(self->ref, 
                       mgr, 
                       db, 
                       self->settings.expectUnsorted, 
                       self->settings.acceptHardClip, 
                       self->settings.refXRefPath, 
                       self->settings.inpath, 
                       self->settings.maxSeqLen, 
                       self->settings.refFiles);
    if (rc == 0)
    {
        self->seq = malloc(sizeof(*self->seq));
        if (self->seq == 0)
        {
            ReferenceWhack(self->ref, false, 0, NULL);
            free(self->ref);
            return RC(rcAlign, rcArc, rcAllocating, rcMemory, rcExhausted);
        }
        SequenceWriterInit(self->seq, db);
        
        self->align = AlignmentMake(db);
        if (self->align == 0)
        {
            ReferenceWhack(self->ref, false, 0, NULL);
            free(self->ref);
            
            SequenceWhack(self->seq, false);
            free(self->seq);
            
            return RC(rcAlign, rcArc, rcAllocating, rcMemory, rcExhausted);
        }
        
        rc = SetupContext(&self->settings, &self->ctx);
        if (rc != 0)
        {
            ReferenceWhack(self->ref, false, 0, NULL);
            free(self->ref);
            
            SequenceWhack(self->seq, false);
            free(self->seq);
            
            AlignmentWhack(self->align, false);
        }
    }
    if (self->settings.tmpfs == NULL)
        self->settings.tmpfs = "/tmp";

    self->commit = true;
    
    return rc;
}

rc_t CommonWriterArchive(CommonWriter *const self,
                         const struct ReaderFile *const reader)
{
    rc_t rc;

    assert(self);
    rc = ArchiveFile(reader,
                     &self->settings,
                     &self->ctx,
                     self->ref,
                     self->seq,
                     self->align,
                     &self->had_alignments,
                     &self->had_sequences);
    if (rc)
        self->commit = false;
    
    self->err_count += self->settings.errCount;
    return rc;
}

rc_t CommonWriterComplete(CommonWriter* self, bool quitting, uint64_t maxDistance)
{
    rc_t rc=0;
    /*** No longer need memory for key2id ***/
    size_t i;
    for (i = 0; i != self->ctx.key2id_count; ++i) {
        KBTreeDropBacking(self->ctx.key2id[i]);
        KBTreeRelease(self->ctx.key2id[i]);
        self->ctx.key2id[i] = NULL;
    }
    free(self->ctx.key2id_names);
    self->ctx.key2id_names = NULL;
    /*******************/

    if (self->had_sequences) {
        if (!quitting) {
            (void)LOGMSG(klogInfo, "Writing unpaired sequences");
            rc = WriteSoloFragments(&self->settings, &self->ctx, self->seq);
            ContextReleaseMemBank(&self->ctx);
            if (rc == 0) {
                rc = SequenceDoneWriting(self->seq);
                if (rc == 0) {
                    (void)LOGMSG(klogInfo, "Updating sequence alignment info");
                    rc = SequenceUpdateAlignInfo(&self->ctx, self->seq);
                }
            }
        }
        else
            ContextReleaseMemBank(&self->ctx);
    }
    
    if (self->had_alignments && !quitting) {
        (void)LOGMSG(klogInfo, "Writing alignment spot ids");
        rc = AlignmentUpdateSpotInfo(&self->ctx, self->align, maxDistance);
    }

    return rc;
}

rc_t CommonWriterWhack(CommonWriter* self)
{
    rc_t rc = 0;
    assert(self);
    
    ContextRelease(&self->ctx);
    
    if (self->align)
        rc = AlignmentWhack(self->align, self->commit);
        
    if (self->seq)
    {
        SequenceWhack(self->seq, self->commit);
        free(self->seq);
    }

    if (self->ref)
    {
        rc_t rc2 = ReferenceWhack(self->ref, self->commit, self->settings.maxSeqLen, Quitting);
        if (rc == 0)
            rc = rc2;
        free(self->ref);
    }

    return rc;
}

