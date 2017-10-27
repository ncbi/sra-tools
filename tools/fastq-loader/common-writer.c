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
#include <stdio.h>
#include <ctype.h>

#include <klib/log.h>
#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/status.h>

#include <kdb/btree.h>

#include <kapp/progressbar.h>
#include <kapp/main.h>

#include <kproc/queue.h>
#include <kproc/thread.h>
#include <kproc/timeout.h>
#include <os-native.h>

#include <kfs/file.h>
#include <kfs/pagefile.h>

#include <vdb/manager.h>
#include <vdb/database.h>

#include "sequence-writer.h"
#include "common-writer.h"
#include "common-reader-priv.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define USE_READER_THREAD (1)

/*--------------------------------------------------------------------------
 * ctx_value_t, FragmentInfo
 */
typedef struct {
/*    uint64_t spotId; */
    int64_t fragmentOffset;
    uint16_t fragmentSize;
    uint16_t seqHash[2];
    uint8_t  unmated: 1,
             has_a_read: 1,
             written: 1;
} ctx_value_t;

#define CTX_VALUE_SET_S_ID(A, B) ((void)(B))

typedef struct FragmentInfo {
    uint32_t readlen;
    uint8_t  is_bad;
    uint8_t  orientation;
    uint8_t  otherReadNo;
    uint8_t  sglen;
    uint8_t  cskey;
} FragmentInfo;

#define MMA_ELEM_T ctx_value_t
#include "mmarray.c"
#undef MMA_ELEM_T

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

static unsigned HashValue(unsigned const len, unsigned char const value[])
{
    /* FNV-1a hash with folding */
    unsigned long long h = 0xcbf29ce484222325ull;
    unsigned i;

    for (i = 0; i < len; ++i) {
        int const octet = value[i];
        h = (h ^ octet) * 0x100000001b3ull;
    }
    return (unsigned)(h ^ (h >> 32));
}

static unsigned HashKey(void const *const key, size_t const keylen)
{
    return HashValue(keylen, key) % NUM_ID_SPACES;
}

static unsigned SeqHashKey(void const *const key, size_t const keylen)
{
    return HashValue(keylen, key) % 0x10000;
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

static int openTempFile(char const path[])
{
    int const fd = open(path, O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
    unlink(path);
    return fd;
}

static rc_t OpenMMapFile(const CommonWriterSettings* settings, SpotAssembler *const ctx, KDirectory *const dir)
{
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/id2value.%u", settings->tmpfs, settings->pid);

    if (rc == 0) {
        int const fd = openTempFile(fname);
        if (fd >= 0)
            ctx->id2value = MMArrayMake(&rc, fd);
        else
            rc = RC(rcExe, rcFile, rcCreating, rcFile, rcNotFound);
    }
    return rc;
}

static rc_t OpenMBankFile(const CommonWriterSettings* settings, SpotAssembler *const ctx, KDirectory *const dir)
{
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/fragments.%u", settings->tmpfs, settings->pid);

    if (rc == 0) {
        int const fd = openTempFile(fname);
        if (fd < 0)
            rc = RC(rcExe, rcFile, rcCreating, rcFile, rcNotFound);
        else
            ctx->fragmentFd = fd;
    }
    return rc;
}

rc_t SetupContext(const CommonWriterSettings* settings, SpotAssembler *ctx)
{
    rc_t rc = 0;

    memset(ctx, 0, sizeof(*ctx));

    ctx->fragment = calloc(FRAGMENT_HOT_COUNT, sizeof(ctx->fragment[0]));
    assert(ctx->fragment != NULL);
    if (ctx->fragment == NULL)
        abort();

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
    int i;

    for (i = 0; i < FRAGMENT_HOT_COUNT; ++i)
        free(ctx->fragment[i].data);

    close(ctx->fragmentFd);
}

void ContextRelease(SpotAssembler *ctx)
{
    KLoadProgressbar_Release(ctx->progress[0], true);
    KLoadProgressbar_Release(ctx->progress[1], true);
    KLoadProgressbar_Release(ctx->progress[2], true);
    KLoadProgressbar_Release(ctx->progress[3], true);
    MMArrayWhack(ctx->id2value);
    free(ctx->fragment);
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
    int const pass = ctx->pass;

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
    KLoadProgressbar_Append(ctx->progress[pass], idCount);

    for (idCount = 0, j = 0; j < ctx->key2id_count; ++j) {
        for (i = 0; i != ctx->idCount[j]; ++i, ++idCount) {
            uint64_t const keyId = ((uint64_t)j << 32) | i;
            ctx_value_t *value;
            unsigned readLen[2];
            unsigned read = 0;
            FragmentInfo const *fip;
            uint8_t const *src;

            value = MMArrayGet(ctx->id2value, &rc, keyId);
            if (value == NULL)
                break;
            KLoadProgressbar_Process(ctx->progress[pass], 1, false);

            if (value->written)
                continue;

            assert(!value->unmated);
            if (ctx->fragment[keyId % FRAGMENT_HOT_COUNT].id == keyId) {
                fip = (FragmentInfo const *)ctx->fragment[keyId % FRAGMENT_HOT_COUNT].data;
            }
            else {
                rc = KDataBufferResize(&fragBuf, (size_t)value->fragmentSize);
                if (rc == 0) {
                    int64_t const nread = pread(ctx->fragmentFd, fragBuf.base, value->fragmentSize, value->fragmentOffset);
                    if (nread == value->fragmentSize)
                        fip = (FragmentInfo const *)fragBuf.base;
                    else {
                        (void)LOGERR(klogErr, rc = RC(rcExe, rcFile, rcReading, rcData, rcNotFound), "KMemBankRead failed");
                        break;
                    }
                }
                else {
                    (void)LOGERR(klogErr, rc, "KDataBufferResize failed");
                    break;
                }
            }
            src = (uint8_t const *)&fip[1];

            readLen[0] = readLen[1] = 0;
            read = fip->otherReadNo - 1;

            readLen[read] = fip->readlen;
            rc = SequenceRecordInit(&srec, 2, readLen);
            if (rc) {
                (void)LOGERR(klogErr, rc, "SequenceRecordInit failed");
                break;
            }

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

            rc = SequenceWriteRecord(seq, &srec, ctx->isColorSpace, false, settings->platform,
                                    settings->keepMismatchQual, settings->no_real_output, settings->hasTI, settings->QualQuantizer);
            if (rc) {
                (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                break;
            }
            /*rc = KMemBankFree(frags, id);*/
            CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
            value->written = 1;
        }
    }
    /*printf("DONE_SOLO:\tcnt2=%d\tcnt1=%d\n",fcountBoth,fcountOne);*/
    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&srec.storage);
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

void ParseSpotName(char const name[], size_t* namelen)
{ /* remove trailing #... */
    const char* hash = string_chr(name, *namelen, '#');
    if (hash)
        *namelen = hash - name;
}

static int8_t LogOddsToPhred1(int8_t logOdds)
{ /* conversion table copied from interface/ncbi/seq.vschema */
    static int8_t const toPhred[] = {
                  0, 1, 1, 2, 2, 3, 3,
         4, 4, 5, 5, 6, 7, 8, 9,10,10,
        11,12,13,14,15,16,17,18,19,20,
        21,22,23,24,25,26,27,28,29,30,
        31,32,33,34,35,36,37,38,39,40
    };
    if (logOdds < -6)
        return 0;
    if (logOdds > 40)
        return 40;
    return toPhred[logOdds + 6];
}

static void LogOddsToPhred(unsigned const readLen, uint8_t dst[], int8_t const src[], int offset)
{
    unsigned i;

    for (i = 0; i < readLen; ++i)
        dst[i] = LogOddsToPhred1(src[i] - offset);
}

static void PhredToPhred(unsigned const readLen, uint8_t dst[], int8_t const src[], int offset)
{
    unsigned i;

    for (i = 0; i < readLen; ++i)
        dst[i] = src[i] - offset;
}

struct ReadResult {
    float progress;
    uint64_t recordNo;
    enum {
        rr_undefined = 0,
        rr_sequence,
        rr_rejected,
        rr_done,
        rr_error
    } type;
    union {
        struct sequence {
            char *name;
            char *spotGroup;
            char *seqDNA;
            unsigned char *quality;
            uint64_t id;
            unsigned readLen;
            unsigned readNo;
            int mated;
            int orientation;
            int bad;
            int inserted;
            int colorspace;
            char cskey;
        } sequence;
        struct reject {
            char *message;
            uint64_t line;
            unsigned column;
            int fatal;
        } reject;
        struct error {
            char const *message;
            rc_t rc;
        } error;
    } u;
};

static char *getSpotGroup(Sequence const *const sequence)
{
    char *rslt = NULL;
    char const *tmp = NULL;
    size_t len = 0;

    SequenceGetSpotGroup(sequence, &tmp, &len);
    if (tmp) {
        rslt = malloc(len + 1);
        if (rslt) {
            memmove(rslt, tmp, len);
            rslt[len] = '\0';
        }
    }
    else {
        rslt = malloc(1);
        if (rslt) {
            rslt[0] = '\0';
        }
    }
    return rslt;
}

static char *getName(Sequence const *const sequence, bool const parseSpotName)
{
    char *rslt = NULL;
    char const *tmp = NULL;
    size_t len = 0;

    SequenceGetSpotName(sequence, &tmp, &len);
    if (parseSpotName)
        ParseSpotName(tmp, &len);
    rslt = malloc(len + 1);
    if (rslt) {
        memmove(rslt, tmp, len);
        rslt[len] = '\0';
    }
    return rslt;
}

static char const kReaderFileGetRecord[] = "ReaderFileGetRecord";
static char const kRecordGetSequence[] = "RecordGetSequence";
static char const kRecordGetRejected[] = "RecordGetRejected";
static char const kRejectedGetError[] = "RejectedGetError";
static char const kSequenceGetQuality[] = "SequenceGetQuality";
static char const kGetKeyID[] = "GetKeyID";
static char const kSequenceGetRead[] = "SequenceGetRead";
static char const kQuitting[] = "Quitting";

static void readSequence(CommonWriterSettings *const G, SpotAssembler *const ctx, Sequence const *const sequence, struct ReadResult *const rslt)
{
    char *seqDNA = NULL;
    uint8_t *qual = NULL;
    char *name = NULL;
    char *spotGroup = NULL;
    uint32_t readLen = 0;
    unsigned readNo = 0;
    int mated = 0;
    int orientation = 0;
    int colorspace = 0;
    char cskey[2];
    uint64_t keyId = 0;
    bool wasInserted = 0;
    bool bad = 0;
    rc_t rc = 0;

    memset(cskey, 0, sizeof(cskey));
    colorspace = SequenceIsColorSpace(sequence);
    orientation = SequenceGetOrientationSelf(sequence);
    bad = SequenceIsLowQuality(sequence);
    if (colorspace)
        rc = SequenceGetCSReadLength(sequence, &readLen);
    else
        rc = SequenceGetReadLength(sequence, &readLen);
    assert(rc == 0);
    seqDNA = malloc(readLen);
    qual = malloc(readLen);
    name = getName(sequence, G->parseSpotName);
    spotGroup = getSpotGroup(sequence);

    if (   seqDNA == NULL
        || qual == NULL
        || name == NULL
        || spotGroup == NULL
        )
    {
        fprintf(stderr, "OUT OF MEMORY!!!");
        abort();
    }

    if (!colorspace) {
        rc = SequenceGetRead(sequence, seqDNA);
    }
    else {
        SequenceGetCSKey(sequence, cskey);
        rc = SequenceGetCSRead(sequence, seqDNA);
    }
    if (rc != 0) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': failed to get sequence", "%s", name));
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kSequenceGetRead;
        goto CLEANUP;
    }
    {
        int8_t const *squal = NULL;
        uint8_t qoffset = 0;
        int qualType = 0;

        if (!colorspace || G->useQUAL)
            rc = SequenceGetQuality(sequence, &squal, &qoffset, &qualType);
        else {
            rc = SequenceGetCSQuality(sequence, &squal, &qoffset, &qualType);
            qualType = QT_Phred;
        }
        if (rc != 0) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of original quality does not match sequence", "name=%s", name));
            rslt->type = rr_error;
            rslt->u.error.rc = rc;
            rslt->u.error.message = kSequenceGetQuality;
            goto CLEANUP;
        }
        if (squal) {
            if (qualType == QT_Phred)
                PhredToPhred(readLen, qual, squal, qoffset);
            else if (qualType == QT_LogOdds)
                LogOddsToPhred(readLen, qual, squal, qoffset);
            else
                memmove(qual, squal, readLen);
        }
        else if (!colorspace)
            memset(qual, 30, readLen);
        else
            memset(qual, 0, readLen);
    }
    if (SequenceWasPaired(sequence)) {
        if (SequenceIsFirst(sequence))
            readNo |= 1;
        if (SequenceIsSecond(sequence))
            readNo |= 2;
        if (readNo == 1 || readNo == 2)
            mated = 1;
    }
    if (!mated)
        readNo = 1;

    rc = GetKeyID(G, ctx, &keyId, &wasInserted, spotGroup, name, strlen(name));
    if (rc != 0) {
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kGetKeyID;
    }
CLEANUP:
    if (rslt->type == rr_error) {
        free(seqDNA);
        free(qual);
        free(name);
        free(spotGroup);
    }
    else {
        rslt->type = rr_sequence;
        rslt->u.sequence.name = name;
        rslt->u.sequence.spotGroup = spotGroup;
        rslt->u.sequence.seqDNA = seqDNA;
        rslt->u.sequence.quality = qual;
        rslt->u.sequence.id = keyId;
        rslt->u.sequence.readLen = readLen;
        rslt->u.sequence.readNo = readNo;
        rslt->u.sequence.mated = mated;
        rslt->u.sequence.orientation = orientation;
        rslt->u.sequence.bad = bad;
        rslt->u.sequence.inserted = wasInserted;
        rslt->u.sequence.colorspace = colorspace;
        rslt->u.sequence.cskey = cskey[0];
    }
    return;
}

static void freeReadResultSequence(struct ReadResult const *const rslt)
{
    free(rslt->u.sequence.name);
    free(rslt->u.sequence.spotGroup);
    free(rslt->u.sequence.seqDNA);
    free(rslt->u.sequence.quality);
}

static void readRejected(CommonWriterSettings *const G, SpotAssembler *const ctx, Rejected const *const reject, struct ReadResult *const rslt)
{
    char const *message;
    uint64_t line = 0;
    uint64_t col = 0;
    bool fatal = 0;
    rc_t const rc = RejectedGetError(reject, &message, &line, &col, &fatal);

    if (rc == 0) {
        rslt->type = rr_rejected;
        rslt->u.reject.message = strdup(message);
        rslt->u.reject.line = line;
        rslt->u.reject.column = col;
        rslt->u.reject.fatal = fatal;
    }
    else {
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kRejectedGetError;
    }
}

static void freeReadResultRejected(struct ReadResult const *const rslt)
{
    free(rslt->u.reject.message);
}

static void readRecord(CommonWriterSettings *const G, SpotAssembler *const ctx, Record const *const record, struct ReadResult *const rslt)
{
    rc_t rc = 0;
    Rejected const *rej = NULL;

    rc = RecordGetRejected(record, &rej);
    if (rc == 0 && rej == NULL) {
        Sequence const *sequence = NULL;
        rc = RecordGetSequence(record, &sequence);
        if (rc == 0) {
            assert(sequence != NULL);
            readSequence(G, ctx, sequence, rslt);
            SequenceRelease(sequence);
            return;
        }
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kRecordGetSequence;
        return;
    }
    if (rc != 0) {
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kRecordGetRejected;
        return;
    }
    assert(rej != NULL);
    readRejected(G, ctx, rej, rslt);
    RejectedRelease(rej);
}

static struct ReadResult *threadGetNextRecord(CommonWriterSettings *const G, SpotAssembler *const ctx, struct ReaderFile const *reader, uint64_t *reccount)
{
    rc_t rc = 0;
    Record const *record = NULL;
    struct ReadResult *const rslt = calloc(1, sizeof(*rslt));

    assert(rslt != NULL);
    if (rslt != NULL) {
        rslt->progress = ReaderFileGetProportionalPosition(reader);
        rslt->recordNo = ++*reccount;
        if (G->maxAlignCount > 0 && rslt->recordNo > G->maxAlignCount) {
            (void)PLOGMSG(klogDebug, (klogDebug, "reached limit of $(max) records read", "max=%u", (unsigned)G->maxAlignCount));
            rslt->type = rr_done;
            return rslt;
        }
        rc = ReaderFileGetRecord(reader, &record);
        if (rc != 0) {
            rslt->type = rr_error;
            rslt->u.error.rc = rc;
            rslt->u.error.message = kReaderFileGetRecord;
            return rslt;
        }
        if (record != NULL) {
            readRecord(G, ctx, record, rslt);
            RecordRelease(record);
            return rslt;
        }
        else {
            rslt->type = rr_done;
            return rslt;
        }
    }
    abort();
}

static void freeReadResultError(struct ReadResult const *const rslt)
{
    (void)0;
}

static void freeReadResult(struct ReadResult const *const rslt)
{
    if (rslt->type == rr_sequence)
        freeReadResultSequence(rslt);
    else if (rslt->type == rr_rejected)
        freeReadResultRejected(rslt);
    else if (rslt->type == rr_error)
        freeReadResultError(rslt);
}

struct ReadThreadContext {
    KThread *th;
    KQueue *que;
    CommonWriterSettings *settings;
    SpotAssembler *ctx;
    ReaderFile const *reader;
    uint64_t reccount;
};

static rc_t readThread(KThread const *const th, void *const ctx)
{
    struct ReadThreadContext *const self = ctx;
    rc_t rc = 0;

    while (Quitting() == 0) {
        struct ReadResult *const rr = threadGetNextRecord(self->settings, self->ctx, self->reader, &self->reccount);
        int const rr_type = rr->type;

        while (Quitting() == 0) {
            timeout_t tm;
            TimeoutInit(&tm, 10000);
            rc = KQueuePush(self->que, rr, &tm);
            if (rc == 0)
                break;
            if ((int)GetRCObject(rc) == rcTimeout) {
                continue;
            }
            break;
        }
        if (rc) {
            free(rr);
            if ((int)GetRCState(rc) == rcReadonly && (int)GetRCObject(rc) == rcQueue) {
                (void)LOGMSG(klogDebug, "readThread: consumer closed queue");
                rc = 0;
            }
            else {
                (void)LOGERR(klogErr, rc, "readThread: failed to push next record into queue");
            }
            break;
        }
        else if (rr_type == rr_done) {
            /* normal exit from end of file */
            (void)LOGMSG(klogDebug, "readThread done : end of file");
            break;
        }
    }
    KQueueSeal(self->que);
    return rc;
}

static struct ReadResult getNextRecord(struct ReadThreadContext *const self)
{
    struct ReadResult rslt;

    memset(&rslt, 0, sizeof(rslt));
#if USE_READER_THREAD
    if (self->th == NULL) {
        rslt.u.error.rc = KQueueMake(&self->que, 1024);
        if (rslt.u.error.rc) {
            rslt.type = rr_error;
            rslt.u.error.message = "KQueueMake";
            return rslt;
        }
        rslt.u.error.rc = KThreadMake(&self->th, readThread, (void *)self);
        if (rslt.u.error.rc) {
            rslt.type = rr_error;
            rslt.u.error.message = "KThreadMake";
            return rslt;
        }
    }
    while ((rslt.u.error.rc = Quitting()) == 0) {
        timeout_t tm;
        void *rr = NULL;
        
        TimeoutInit(&tm, 10000);
        rslt.u.error.rc = KQueuePop(self->que, &rr, &tm);
        if (rslt.u.error.rc == 0) {
            memmove(&rslt, rr, sizeof(rslt));
            free(rr);
            if (rslt.type == rr_done)
                goto DONE;
            return rslt;
        }
        if ((int)GetRCObject(rslt.u.error.rc) == rcTimeout) {
            (void)LOGMSG(klogDebug, "KQueuePop timed out");
        }
        else {
            (void)LOGERR(klogErr, rslt.u.error.rc, "KQueuePop failed");
            rslt.type = rr_done;
            goto DONE;
        }
    }
    rslt.type = rr_error;
    rslt.u.error.message = kQuitting;
DONE:
    {
        rc_t rc = 0;
        KThreadWait(self->th, &rc);
        KThreadRelease(self->th);
        KQueueRelease(self->que);
        self->que = NULL;
        self->th = NULL;
    }
#else
    if ((rslt.u.error.rc = Quitting()) == 0) {
        struct ReadResult *const rr = threadGetNextRecord(self->settings, self->ctx, self->reader, &self->reccount);
        rslt = *rr;
        free(rr);
    }
    else {
        rslt.type = rr_error;
        rslt.u.error.message = kQuitting;
    }
#endif
    return rslt;
}

rc_t ArchiveFile(const struct ReaderFile *const reader,
                 CommonWriterSettings *const G,
                 struct SpotAssembler *const ctx,
                 struct SequenceWriter *const seq,
                 bool *const had_sequences)
{
    KDataBuffer buf;
    KDataBuffer fragBuf;
    rc_t rc;
    SequenceRecord srec;
    unsigned progress = 0;
    uint64_t recordsProcessed = 0;
    uint64_t filterFlagConflictRecords=0; /*** counts number of conflicts between flags 'duplicate' and 'lowQuality' ***/
#define MAX_WARNINGS_FLAG_CONFLICT 10000 /*** maximum errors to report ***/

    bool isColorSpace = false;
    bool isNotColorSpace = G->noColorSpace;
    char const *const fileName = ReaderFileGetPathname(reader);
    struct ReadThreadContext threadCtx;
    uint64_t fragmentsAdded = 0;
    uint64_t spotsCompleted = 0;
    uint64_t fragmentsEvicted = 0;

    if (ctx->key2id_max == 0) {
        ctx->key2id_max = 1;
    }

    memset(&srec, 0, sizeof(srec));
    memset(&threadCtx, 0, sizeof(threadCtx));
    threadCtx.settings = G;
    threadCtx.ctx = ctx;
    threadCtx.reader = reader;

    rc = KDataBufferMake(&fragBuf, 8, 4096);
    if (rc)
        return rc;

    rc = KDataBufferMake(&buf, 16, 0);
    if (rc)
        return rc;

    if (rc == 0) {
        (void)PLOGMSG(klogInfo, (klogInfo, "Loading '$(file)'", "file=%s", fileName));
    }

    *had_sequences = false;

    while (rc == 0) {
        ctx_value_t *value;
        struct ReadResult const rr = getNextRecord(&threadCtx);

        if ((unsigned)(rr.progress * 100.0) > progress) {
            unsigned new_value = rr.progress * 100.0;
            KLoadProgressbar_Process(ctx->progress[0], new_value - progress, false);
            progress = new_value;
        }
        if (rr.type == rr_done)
            break;
        if (rr.type == rr_error) {
            rc = rr.u.error.rc;
            if (rr.u.error.message == kQuitting) {
                (void)LOGMSG(klogInfo, "Exiting read loop");
                break;
            }
            if (rr.u.error.message == kReaderFileGetRecord) {
                if (GetRCObject(rc) == rcRow && (GetRCState(rc) == rcInvalid || GetRCState(rc) == rcEmpty)) {
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "ArchiveFile: '$(file)' - row $(row)", "file=%s,row=%lu", fileName, rr.recordNo));
                    rc = CheckLimitAndLogError(G);
                }
                /* else fail */
            }
            else {
                (void)PLOGERR(klogErr, (klogErr, rc, "ArchiveFile: $(func) failed", "func=%s", rr.u.error.message));
                rc = CheckLimitAndLogError(G);
            }
            goto LOOP_END;
        }
        if (rr.type == rr_rejected) {
            char const *const message = rr.u.reject.message;
            uint64_t const line = rr.u.reject.line;
            uint64_t const col = rr.u.reject.column;
            bool const fatal = rr.u.reject.fatal;

            (void)PLOGMSG(fatal ? klogErr : klogWarn, (fatal ? klogErr : klogWarn,
                                                       "$(file):$(l):$(c):$(msg)", "file=%s,l=%lu,c=%lu,msg=%s",
                                                       fileName, line, col, message));
            rc = CheckLimitAndLogError(G);
            if (fatal)
                rc = RC(rcExe, rcFile, rcParsing, rcFormat, rcUnsupported);
            goto LOOP_END;
        }
        if (rr.type == rr_sequence) {
            uint64_t const keyId = rr.u.sequence.id;
            bool const wasInserted = !!rr.u.sequence.inserted;
            bool const colorspace = !!rr.u.sequence.colorspace;
            bool const mated = !!rr.u.sequence.mated;
            unsigned const readNo = rr.u.sequence.readNo;
            char const *const seqDNA = rr.u.sequence.seqDNA;
            uint8_t const *const qual = rr.u.sequence.quality;
            unsigned const readlen = rr.u.sequence.readLen;
            int const readOrientation = !!rr.u.sequence.orientation;
            bool const reverse = isColorSpace ? false : (readOrientation == ReadOrientationReverse);
            char const cskey = rr.u.sequence.cskey;
            bool const bad = !!rr.u.sequence.bad;
            char const *const spotGroup = rr.u.sequence.spotGroup;
            char const *const name = rr.u.sequence.name;
            int const namelen = strlen(name);

            if (!G->noColorSpace) {
                if (colorspace) {
                    if (isNotColorSpace) {
                    MIXED_BASE_AND_COLOR:
                        rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' contains base space and color space", "file=%s", fileName));
                        goto LOOP_END;
                    }
                    ctx->isColorSpace = isColorSpace = true;
                }
                else if (isColorSpace)
                    goto MIXED_BASE_AND_COLOR;
                else
                    isNotColorSpace = true;
            }

            value = MMArrayGet(ctx->id2value, &rc, keyId);
            if (value == NULL) {
                (void)PLOGERR(klogErr, (klogErr, rc, "MMArrayGet: failed on id '$(id)'", "id=%u", keyId));
                goto LOOP_END;
            }
            if (wasInserted) {
                memset(value, 0, sizeof(*value));
                value->fragmentOffset = -1;
                value->unmated = !mated;
            }
            else {
                if (mated && value->unmated) {
                    (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                             "Spot '$(name)', which was first seen without mate info, now has mate info",
                                             "name=%s", name));
                    rc = CheckLimitAndLogError(G);
                    goto LOOP_END;
                }
            }

            ++recordsProcessed;
            if (mated) {
                if (value->written) {
                    (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' has already been assigned a spot id", "name=%s", name));
                }
                else if (!value->has_a_read) {
                    /* new mated fragment - do spot assembly */
                    unsigned sz;
                    FragmentInfo fi;
                    int64_t const victimId = ctx->fragment[keyId % FRAGMENT_HOT_COUNT].id;
                    void *const victimData = ctx->fragment[keyId % FRAGMENT_HOT_COUNT].data;
                    void *myData = NULL;

                    ++fragmentsAdded;
                    value->seqHash[readNo - 1] = SeqHashKey(seqDNA, readlen);

                    memset(&fi, 0, sizeof(fi));
                    fi.orientation = readOrientation;
                    fi.otherReadNo = readNo;
                    fi.sglen   = strlen(spotGroup);
                    fi.readlen = readlen;
                    fi.cskey = cskey;
                    fi.is_bad = bad;
                    sz = sizeof(fi) + 2*fi.readlen + fi.sglen;
                    myData = malloc(sz);
                    if (myData == NULL) {
                        (void)LOGERR(klogErr, RC(rcExe, rcFile, rcReading, rcMemory, rcExhausted), "OUT OF MEMORY!");
                        abort();
                        goto LOOP_END;
                    }
                    {{
                        uint8_t *dst = (uint8_t*)myData;

                        memmove(dst,&fi,sizeof(fi));
                        dst += sizeof(fi);
                        COPY_READ((char *)dst, seqDNA, fi.readlen, reverse);
                        dst += fi.readlen;
                        COPY_QUAL(dst, qual, fi.readlen, reverse);
                        dst += fi.readlen;
                        memmove(dst,spotGroup,fi.sglen);
                    }}
                    ctx->fragment[keyId % FRAGMENT_HOT_COUNT].id = keyId;
                    ctx->fragment[keyId % FRAGMENT_HOT_COUNT].data = myData;
                    value->has_a_read = 1;
                    value->fragmentSize = sz;
                    *had_sequences = true;

                    if (victimData != NULL) {
                        ctx_value_t *const victim = MMArrayGet(ctx->id2value, &rc, victimId);
                        if (victim == NULL) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "MMArrayGet: failed on id '$(id)'", "id=%u", victimId));
                            abort();
                            goto LOOP_END;
                        }
                        if (victim->fragmentOffset < 0) {
                            int64_t const pos = ctx->nextFragment;
                            int64_t const nwrit = pwrite(ctx->fragmentFd, victimData, victim->fragmentSize, pos);

                            if (nwrit == victim->fragmentSize) {
                                ctx->nextFragment += victim->fragmentSize;
                                victim->fragmentOffset = pos;
                                free(victimData);
                                ++fragmentsEvicted;
                            }
                            else {
                                (void)LOGMSG(klogFatal, "Failed to write fragment data");
                                abort();
                                goto LOOP_END;
                            }
                        }
                        else {
                            (void)LOGMSG(klogFatal, "PROGRAMMER ERROR!!");
                            abort();
                            goto LOOP_END;
                        }
                    }
                }
                else {
                    /* might be second fragment */
                    uint64_t const sz = value->fragmentSize;
                    bool freeFip = false;
                    FragmentInfo *fip = NULL;

                    if (ctx->fragment[keyId % FRAGMENT_HOT_COUNT].id == keyId) {
                        fip = ctx->fragment[keyId % FRAGMENT_HOT_COUNT].data;
                        freeFip = true;
                    }
                    else {
                        int64_t nread = 0;
                        rc = KDataBufferResize(&fragBuf, (size_t)sz);
                        if (rc) {
                            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to resize fragment buffer", ""));
                            abort();
                            goto LOOP_END;
                        }
                        nread = pread(ctx->fragmentFd, fragBuf.base, sz, value->fragmentOffset);
                        if (nread == sz) {
                            fip = (FragmentInfo *) fragBuf.base;
                        }
                        else {
                            (void)PLOGMSG(klogFatal, (klogFatal, "Failed to read fragment data; spot:'$(name)'; "
                                "size: $(size); pos: $(pos); read: $(read)", "name=%s,size=%lu,pos=%lu,read=%lu",
                                name, sz, value->fragmentOffset, nread));
                            abort();
                            goto LOOP_END;
                        }
                    }
                    if (readNo != fip->otherReadNo) {
                        /* mate found */
                        unsigned readLen[2];
                        unsigned read1 = 0;
                        unsigned read2 = 1;
                        uint8_t  *src  = (uint8_t*) fip + sizeof(*fip);

                        ++spotsCompleted;
                        value->seqHash[readNo - 1] = SeqHashKey(seqDNA, readlen);

                        if (readNo < fip->otherReadNo) {
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
                                  reverse);
                        COPY_QUAL(srec.qual + srec.readStart[read2],
                                  qual,
                                  srec.readLen[read2],
                                  reverse);

                        srec.keyId = keyId;
                        srec.is_bad[read2] = bad;
                        srec.cskey[read2] = cskey;

                        srec.spotGroup = (char *)spotGroup;
                        srec.spotGroupLen = strlen(spotGroup);
                        rc = SequenceWriteRecord(seq, &srec, isColorSpace, false, G->platform,
                                                 G->keepMismatchQual, G->no_real_output, G->hasTI, G->QualQuantizer);
                        if (freeFip) {
                            free(fip);
                            ctx->fragment[keyId % FRAGMENT_HOT_COUNT].data = NULL;
                        }
                        if (rc) {
                            (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                            goto LOOP_END;
                        }
                        CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
                        value->written = 1;
                    }
                }
            }
            else if (value->written) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' has already been assigned a spot id", "name=%s", name));
            }
            else if (value->has_a_read) {
                if (ctx->fragment[keyId % FRAGMENT_HOT_COUNT].id == keyId) {
                    free(ctx->fragment[keyId % FRAGMENT_HOT_COUNT].data);
                    ctx->fragment[keyId % FRAGMENT_HOT_COUNT].data = NULL;
                }
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                         "Spot '$(name)', which was first seen with mate info, now has no mate info",
                                         "name=%s", name));
                rc = CheckLimitAndLogError(G);
            }
            else {
                /* new unmated fragment - no spot assembly */
                unsigned readLen[1];

                value->seqHash[0] = SeqHashKey(seqDNA, readlen);

                readLen[0] = readlen;
                rc = SequenceRecordInit(&srec, 1, readLen);
                if (rc) {
                    (void)LOGERR(klogErr, rc, "Failed resizing sequence record buffer");
                    goto LOOP_END;
                }
                srec.is_bad[0] = bad;
                srec.orientation[0] = readOrientation;
                srec.cskey[0] = cskey;
                COPY_READ(srec.seq  + srec.readStart[0], seqDNA, readlen, false);
                COPY_QUAL(srec.qual + srec.readStart[0], qual, readlen, false);

                srec.keyId = keyId;

                srec.spotGroup = (char *)spotGroup;
                srec.spotGroupLen = strlen(spotGroup);

                srec.spotName = (char *)name;
                srec.spotNameLen = namelen;

                rc = SequenceWriteRecord(seq, &srec, isColorSpace, false, G->platform,
                                         G->keepMismatchQual, G->no_real_output, G->hasTI, G->QualQuantizer);
                if (rc) {
                    (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                    goto LOOP_END;
                }
                CTX_VALUE_SET_S_ID(*value, ++ctx->spotId);
                value->written = 1;
                *had_sequences = true;
            }
        }
        else
            abort();

LOOP_END:
        freeReadResult(&rr);
    }

    if (threadCtx.que != NULL && threadCtx.th != NULL) {
        /* this means the exit was triggered in here, so the producer thread
         * needs to be notified and allowed to exit
         *
         * if the exit were triggered by the context setup, then
         * only one of que or th would be NULL
         *
         * it the exit were triggered by the getNextRecord, then both
         * que and th would be NULL
         */
        KQueueSeal(threadCtx.que);
        for ( ; ; ) {
            timeout_t tm;
            void *rr = NULL;
            rc_t rc;

            TimeoutInit(&tm, 1000);
            rc = KQueuePop(threadCtx.que, &rr, &tm);
            if (rc == 0)
                free(rr);
            else
                break;
        }
        KThreadWait(threadCtx.th, NULL);
    }
    KThreadRelease(threadCtx.th);
    KQueueRelease(threadCtx.que);

    if (filterFlagConflictRecords > 0) {
        (void)PLOGMSG(klogWarn, (klogWarn, "$(cnt1) out of $(cnt2) records contained warning : both 'duplicate' and 'lowQuality' flag bits set, only 'duplicate' will be saved", "cnt1=%lu,cnt2=%lu", filterFlagConflictRecords,recordsProcessed));
    }
    if (rc == 0 && recordsProcessed == 0) {
        (void)LOGMSG(klogWarn, (G->limit2config || G->refFilter != NULL) ?
                     "All records from the file were filtered out" :
                     "The file contained no records that were processed.");
        rc = RC(rcAlign, rcFile, rcReading, rcData, rcEmpty);
    }
    if (rc == 0 && threadCtx.reccount > 0) {
        uint64_t const reccount = threadCtx.reccount - 1;
        double const percentage = ((double)G->errCount) / reccount;
        double const allowed = G->maxErrPct/ 100.0;
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
    (void)PLOGMSG(klogDebug, (klogDebug, "Fragments added to spot assembler: $(added). Fragments evicted to disk: $(evicted). Spots completed: $(completed)",
        "added=%lu,evicted=%lu,completed=%lu", fragmentsAdded, fragmentsEvicted, spotsCompleted));

    KDataBufferWhack(&buf);
    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&srec.storage);
    return rc;
}

/*--------------------------------------------------------------------------
 * CommonWriter
 */

rc_t CommonWriterInit(CommonWriter* self, struct VDBManager *mgr, struct VDatabase *db, const CommonWriterSettings* G)
{
    rc_t rc = 0;
    assert(self);
    assert(mgr);
    assert(db);

    memset(self, 0, sizeof(*self));
    if (G)
        self->settings = *G;

    {
        self->seq = malloc(sizeof(*self->seq));
        if (self->seq == 0)
        {
            return RC(rcAlign, rcArc, rcAllocating, rcMemory, rcExhausted);
        }
        SequenceWriterInit(self->seq, db);

        rc = SetupContext(&self->settings, &self->ctx);
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
    bool has_sequences = false;

    assert(self);
    rc = ArchiveFile(reader,
                     &self->settings,
                     &self->ctx,
                     self->seq,
                     &has_sequences);
    if (rc)
        self->commit = false;
    else
        self->had_sequences |= has_sequences;

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
            }
        }
        else
            ContextReleaseMemBank(&self->ctx);
    }

    return rc;
}

rc_t CommonWriterWhack(CommonWriter* self)
{
    rc_t rc = 0;
    assert(self);

    ContextRelease(&self->ctx);

    if (self->seq)
    {
        SequenceWhack(self->seq, self->commit);
        free(self->seq);
    }

    return rc;
}

