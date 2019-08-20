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

#include "spot-assembler.h"

#include <klib/status.h>
#include <klib/printf.h>

#include <kfs/file.h>
#include <kfs/directory.h>

#include <kdb/btree.h>

#include <kapp/progressbar.h>

#include "sequence-writer.h"

#define MMA_ELEM_T ctx_value_t
#include "mmarray.c"
#undef MMA_ELEM_T

ctx_value_t * SpotAssemblerGetCtxValue(SpotAssembler * self, rc_t *const prc, uint64_t const keyId)
{
    return MMArrayGet(self->id2value, prc, keyId);
}

rc_t OpenKBTree(size_t cache_size, const char * tmpfs, uint64_t pid, struct KBTree **const rslt, size_t const n, size_t const max)
{
    size_t const cacheSize = (((cache_size - (cache_size / 2) - (cache_size / 8)) / max)
                            + 0xFFFFF) & ~((size_t)0xFFFFF);
    KFile *file = NULL;
    KDirectory *dir;
    char fname[4096];
    rc_t rc;

    rc = KDirectoryNativeDir(&dir);
    if (rc)
        return rc;

    rc = string_printf(fname, sizeof(fname), NULL, "%s/key2id.%u.%u", tmpfs, pid, n); if (rc) return rc;
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

rc_t GetKeyIDOld(SpotAssembler* const ctx, uint64_t *const rslt, bool *const wasInserted, char const key[], char const name[], size_t const namelen)
{
    size_t const keylen = strlen(key);
    rc_t rc;
    uint64_t tmpKey;

    if (ctx->key2id_count == 0) {
        rc = OpenKBTree(ctx->cache_size, ctx->tmpfs, ctx->pid, &ctx->key2id[0], 1, 1);
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

unsigned SeqHashKey(void const *const key, size_t const keylen)
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

rc_t SpotAssemblerGetKeyID(SpotAssembler *const ctx,
                           uint64_t *const rslt,
                           bool *const wasInserted,
                           char const key[],
                           char const name[],
                           size_t const o_namelen)
{
    rc_t rc;
    size_t const namelen = GetFixedNameLength(name, o_namelen);

    if (ctx->key2id_max == 1)
    {
        rc = GetKeyIDOld(ctx, rslt, wasInserted, key, name, namelen);
    }
    else
    {
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
            rc = OpenKBTree(ctx->cache_size, ctx->tmpfs, ctx->pid, &tree, ctx->key2id_count + 1, 1); /* ctx->key2id_max); */

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
                {
                    ++ctx->idCount[f];
                }
                assert(tmpKey < ctx->idCount[f]);
            }
        }
        else
        {
            rc = RC(rcExe, rcTree, rcAllocating, rcConstraint, rcViolated);
        }
    }

    if ( rc == 0 && *wasInserted )
    {   /* save the read name, to be used when the mate shows up, or in SpotAssemblerWriteSoloFragments() */
        rc = Id2Name_Add ( & ctx->id2name, *rslt, name );
    }

    return rc;
}

static int openTempFile(char const path[])
{
    int const fd = open(path, O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
    unlink(path);
    return fd;
}

static rc_t OpenMMapFile(SpotAssembler *const ctx, KDirectory *const dir)
{
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/id2value.%u", ctx->tmpfs, ctx->pid);

    if (rc == 0) {
        int const fd = openTempFile(fname);
        if (fd >= 0)
            ctx->id2value = MMArrayMake(&rc, fd);
        else
            rc = RC(rcExe, rcFile, rcCreating, rcFile, rcNotFound);
    }
    return rc;
}

static rc_t OpenMBankFile(SpotAssembler *const ctx, KDirectory *const dir)
{
    char fname[4096];
    rc_t rc = string_printf(fname, sizeof(fname), NULL, "%s/fragments.%u", ctx->tmpfs, ctx->pid);

    if (rc == 0) {
        int const fd = openTempFile(fname);
        if (fd < 0)
            rc = RC(rcExe, rcFile, rcCreating, rcFile, rcNotFound);
        else
            ctx->fragmentFd = fd;
    }
    return rc;
}

rc_t SpotAssemblerMake(SpotAssembler **p_self, size_t cache_size, const char * tmpfs, uint64_t pid)
{
    rc_t rc = 0;

    assert ( p_self != NULL );

    SpotAssembler * self = calloc ( 1, sizeof ( * self ) );
    if ( self == NULL )
    {
        return RC ( rcExe, rcName, rcAllocating, rcMemory, rcExhausted );
    }

    self -> fragment = calloc ( FRAGMENT_HOT_COUNT, sizeof ( self -> fragment [ 0 ] ) );
    if ( self -> fragment == NULL )
    {
        free(self);
        return RC ( rcExe, rcName, rcAllocating, rcMemory, rcExhausted );
    }

    self -> cache_size = cache_size;
    self -> tmpfs = tmpfs;
    self -> pid = pid;
    self -> key2id_max = 1; /* make sure to use GetKeyIDOld() */

    STSMSG(1, ("Cache size: %uM\n", cache_size / 1024 / 1024));

    {
        KDirectory *dir;
        rc = KDirectoryNativeDir(&dir);
        if (rc == 0)
            rc = OpenMMapFile(self, dir);
        if (rc == 0)
            rc = OpenMBankFile(self, dir);

        KDirectoryRelease(dir);
    }

    if ( rc == 0 )
    {
        rc = Id2Name_Init ( & self -> id2name );
    }

    if ( rc == 0 )
    {
        * p_self = self;
    }
    else
    {
        SpotAssemblerRelease ( self );
    }

    return rc;
}

void SpotAssemblerReleaseMemBank(SpotAssembler *self)
{
    int i;

    for (i = 0; i < FRAGMENT_HOT_COUNT; ++i)
        free(self->fragment[i].data);

    close(self->fragmentFd);
}

void SpotAssemblerRelease(SpotAssembler * self)
{
    size_t i;
    for (i = 0; i != self->key2id_count; ++i)
    {
        KBTreeDropBacking ( self->key2id[i] );
        KBTreeRelease ( self->key2id[i]) ;
        self->key2id[i] = NULL;
    }
    free ( self->key2id_names );
    self->key2id_names = NULL;

    MMArrayWhack ( self->id2value );
    Id2Name_Whack ( & self->id2name );

    free(self->fragment);
    free(self);
}

rc_t SpotAssemblerWriteSoloFragments(SpotAssembler* ctx,
                                     bool isColorSpace,
                                     INSDC_SRA_platform_id platform,
                                     bool keepMismatchQual,
                                     bool no_real_output,
                                     bool hasTI,
                                     const char * QualQuantizer,
                                     SequenceWriter *seq,
                                     const struct KLoadProgressbar *progress)
{
    uint32_t i;
    unsigned j;
    uint64_t idCount = 0;
    rc_t rc;
    KDataBuffer fragBuf;
    SequenceRecord srec;

    memset(&srec, 0, sizeof(srec));

    rc = KDataBufferMake(&fragBuf, 8, 0);
    if (rc) {
        (void)LOGERR(klogErr, rc, "KDataBufferMake failed");
        return rc;
    }
    for (idCount = 0, j = 0; j < ctx->key2id_count; ++j) {
        idCount += ctx->idCount[j];
    }
    KLoadProgressbar_Append(progress, idCount);

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
            KLoadProgressbar_Process(progress, 1, false);

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

            rc = Id2Name_Get ( & ctx->id2name, keyId, (const char**) & srec.spotName );
            if (rc)
            {
                (void)LOGERR(klogErr, rc, "Is2Name_Get failed");
                break;
            }
            srec.spotNameLen = strlen(srec.spotName);

            rc = SequenceWriteRecord(seq, &srec, isColorSpace, false, platform, keepMismatchQual, no_real_output, hasTI, QualQuantizer);
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

FragmentEntry *
SpotAssemblerGetFragmentEntry(SpotAssembler * self, uint64_t keyId)
{
    return & self -> fragment [ keyId % FRAGMENT_HOT_COUNT ];
}
