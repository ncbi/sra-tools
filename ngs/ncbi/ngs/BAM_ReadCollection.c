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

#include <klib/rc.h>
#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <kfs/file.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <klib/sort.h>

#include <strtol.h> /* for strtoi64 */

#include <stddef.h>
#include <assert.h>
#include <string.h>

#include <sysalloc.h>

#include <zlib.h>

#define NGS_REFCOUNT void

#include "NGS_ReadCollection.h"
#include "NGS_ReadGroup.h"
#include "NGS_Reference.h"
#include "NGS_Alignment.h"
#include "NGS_Read.h"
#include "NGS_String.h"

#include "BAM_Record.h"

#define IO_BLK_SIZE (1024u * 1024u)
#define BAM_BLK_MAX (64u * 1024u)

typedef struct BAM_rec BAM_rec;
struct BAM_rec {
    struct {
        uint8_t
        refID[4],
        pos[4],
        bin_mq_nl[4],
        flag_nc[4],
        l_seq[4],
        next_refID[4],
        next_pos[4],
        tlen[4],
        readname[1];
    } const *data;
    void *allocated;
    uint64_t pos;
    size_t size;
};

#define MAX_INDEX_SEQ_LEN ((1u << 29) - 1)
#define MAX_BIN  (37449u)
#define NUMINTV ((MAX_INDEX_SEQ_LEN + 1) >> 14)

typedef struct RefIndexBinInfo RefIndexBinInfo;
struct RefIndexBinInfo {
    unsigned offset, count;
};

typedef struct RefIndexChunk RefIndexChunk;
struct RefIndexChunk {
    uint64_t first /*, last */;
};

typedef struct RefIndex RefIndex;
struct RefIndex {
    uint64_t off_beg, off_end, n_mapped, n_unmapped;
    uint32_t numintv, numchnk;
    uint64_t interval[NUMINTV];
    RefIndexBinInfo bins[MAX_BIN];
    RefIndexChunk chunk[1];
};

typedef struct HeaderRefInfo HeaderRefInfo;
struct HeaderRefInfo {
    RefIndex *index;
    char *name;
    int32_t length;
    int32_t ordinal;
};

typedef struct BAM_ReferenceInfo BAM_ReferenceInfo;
struct BAM_ReferenceInfo {
    int32_t count;
    HeaderRefInfo ref[1];
};

typedef struct BAM_ReadCollection BAM_ReadCollection;
struct BAM_ReadCollection
{
    NGS_ReadCollection dad;
    struct KFile const *fp;
    char *path;                     /* path used to open the BAM file       */
    uint8_t *iobuffer;              /* raw io buffer, compressed BAM data   */

    BAM_ReferenceInfo *references;
    char *headerText;

    uint64_t cpos;                  /* file position of iobuffer  */
    uint64_t bpos;                  /* file position of bambuffer */
    uint64_t fpos;                  /* file position of next read */
    z_stream zs;

    /* name is substr(path, namestart, namelen) */
    unsigned namestart;
    unsigned namelen;
    unsigned bam_cur;               /* current offset in bambuffer */

    uint8_t bambuffer[BAM_BLK_MAX];
};

typedef struct BAM_Reference BAM_Reference;
struct BAM_Reference
{
    NGS_Reference dad;
    BAM_ReadCollection *parent;
    int32_t cur;
    int state;
};

typedef struct BAM_RefIndexSlice BAM_RefIndexSlice;
struct BAM_RefIndexSlice
{
    NGS_Refcount dad;
    BAM_ReadCollection *parent;
    uint64_t start;
    uint64_t end;
    int32_t chunks;
    int32_t cur;
    unsigned refID;
    uint64_t chunk[1];
};

extern NGS_Alignment *BAM_AlignmentMake(ctx_t, bool, bool, struct BAM_Record *(*)(NGS_Refcount *, ctx_t), NGS_Refcount *, char const *);

static void BAM_ReferenceInfoWhack(BAM_ReferenceInfo *);

static void BAM_ReadCollectionWhack(void *const object, ctx_t ctx) {
    BAM_ReadCollection *const self = (BAM_ReadCollection *)object;

    if (self->references) {
        BAM_ReferenceInfoWhack(self->references);
        free(self->references);
    }
    free(self->headerText);
    free(self->iobuffer);
    free(self->path);
    KFileRelease(self->fp);
}

static NGS_String * BAM_ReadCollectionName(NGS_ReadCollection *const vp, ctx_t ctx) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_ReadCollection *const self = (BAM_ReadCollection *)vp;

    return NGS_StringMake(ctx, self->path + self->namestart, self->namelen);
}

static NGS_ReadGroup * BAM_ReadCollectionReadGroups(NGS_ReadCollection *const vp, ctx_t ctx) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return NULL;
}

static NGS_ReadGroup * BAM_ReadCollectionReadGroup(NGS_ReadCollection *const vp, ctx_t ctx, char const spec[]) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return NULL;
}

static NGS_String *BAM_ReferenceGetCommonName(NGS_Reference *const base, ctx_t ctx)
{
    BAM_Reference *const self = (BAM_Reference *)base;
    char const *name = self->parent->references->ref[self->cur].name;

    return NGS_StringMakeCopy(ctx, name, strlen(name));
}

static NGS_String *BAM_ReferenceGetCanonicalName(NGS_Reference *const base, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    USER_ERROR(xcFunctionUnsupported, "not supported for SAM/BAM");
    return NULL;
}

static bool BAM_ReferenceGetIsCircular(NGS_Reference const *const base, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    USER_ERROR(xcFunctionUnsupported, "not supported for SAM/BAM");
    return NULL;
}

static uint64_t BAM_ReferenceGetLength(NGS_Reference *const base, ctx_t ctx)
{
    BAM_Reference *const self = (BAM_Reference *)base;

    return self->parent->references->ref[self->cur].length;
}

static NGS_String *BAM_ReferenceGetBases(NGS_Reference *const base, ctx_t ctx, uint64_t offset, uint64_t size)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    USER_ERROR(xcFunctionUnsupported, "not supported for SAM/BAM");
    return NULL;
}

static NGS_String *BAM_ReferenceGetChunk(NGS_Reference *const base, ctx_t ctx, uint64_t offset, uint64_t size)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    USER_ERROR(xcFunctionUnsupported, "not supported for SAM/BAM");
    return NULL;
}

static NGS_Alignment *BAM_ReferenceGetAlignment(NGS_Reference *const base, ctx_t ctx, char const spec[])
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    USER_ERROR(xcFunctionUnsupported, "not supported for SAM/BAM");
    return NULL;
}

static NGS_Alignment *BAM_ReferenceGetAlignments(NGS_Reference *const base, ctx_t ctx, bool wants_primary, bool wants_secondary)
{
    BAM_Reference *const self = (BAM_Reference *)base;
    HeaderRefInfo const *const ref = &self->parent->references->ref[self->cur];

    if (!wants_primary || !wants_secondary || ref->index == NULL) {
        USER_ERROR(xcFunctionUnsupported, "not supported for unindexed BAM");
        return 0;
    }
    UNIMPLEMENTED();
    return NULL;
}

static uint64_t BAM_ReferenceGetAlignmentCount(NGS_Reference const *const base, ctx_t ctx, bool wants_primary, bool wants_secondary)
{
    BAM_Reference const *const self = (BAM_Reference *)base;
    HeaderRefInfo const *const ref = &self->parent->references->ref[self->cur];

    if (!wants_primary || !wants_secondary || ref->index == NULL) {
        USER_ERROR(xcFunctionUnsupported, "not supported for unindexed BAM");
        return 0;
    }
    return ref->index->n_mapped;
}

static int IndexSlice(RefIndex const *const self,
                      uint64_t **rslt,
                      unsigned const beg,
                      unsigned const end);

static BAM_Record *BAM_GetRecordSliced(NGS_Refcount *const self, ctx_t ctx);

static void BAM_RefIndexSliceWhack(void *const object, ctx_t ctx)
{
    BAM_RefIndexSlice *const self = (BAM_RefIndexSlice *)object;

    NGS_RefcountRelease(&self->parent->dad.dad, ctx);
}

static NGS_Alignment *BAM_ReferenceGetAlignmentSlice(NGS_Reference *const object,
                                                     ctx_t ctx,
                                                     bool wants_primary,
                                                     bool wants_secondary,
                                                     uint64_t offset,
                                                     uint64_t size)
{
    BAM_Reference *const self = (BAM_Reference *)object;

    if (!wants_primary && !wants_secondary)
EMPTY_ITERATOR: {
        return NGS_AlignmentMakeNull(ctx, self->parent->path + self->parent->namestart, self->parent->namelen);
    }
    else {
        HeaderRefInfo const *const ref = &self->parent->references->ref[self->cur];

        if (ref->index == NULL) {
            USER_ERROR(xcFunctionUnsupported, "not supported for unindexed BAM");
            return 0;
        }
        if (offset > ref->length) {
            goto EMPTY_ITERATOR;
        }
        if (offset + size > ref->length) {
            size = ref->length - offset;
        }
        uint64_t *chunk;
        int const chunks = IndexSlice(ref->index, &chunk, (uint32_t)offset, (uint32_t)(offset + size));

        if (chunks >= 0) {
            BAM_RefIndexSlice *const slice = calloc(1, ((uint8_t const *)&((BAM_RefIndexSlice const *)NULL)->chunk[chunks]) - ((uint8_t const *)NULL));

            if (slice) {
                static NGS_Refcount_vt const vt = {
                    BAM_RefIndexSliceWhack
                };

                NGS_RefcountInit(ctx, &slice->dad, &vt, "BAM_RefIndexSlice", ref->name);
                slice->parent = NGS_RefcountDuplicate(&self->parent->dad.dad, ctx);
                slice->start = offset;
                slice->end = offset + size;
                slice->chunks = chunks;
                slice->refID = self->cur;
                if (chunks)
                    memmove(slice->chunk, chunk, chunks * sizeof(*chunk));

                free(slice);

                NGS_Alignment *const rslt = BAM_AlignmentMake(ctx, wants_primary, wants_secondary, BAM_GetRecordSliced,
                                                              NGS_RefcountDuplicate(&self->dad.dad, ctx),
                                                              self->parent->path + self->parent->namestart);

                return rslt;
            }
        }
        USER_ABORT(xcNoMemory, "out of memory allocating index slice");
        return NULL;
    }
}

static struct NGS_Pileup *BAM_ReferenceGetPileups(NGS_Reference *const base, ctx_t ctx, bool wants_primary, bool wants_secondary)
{
    USER_ERROR(xcFunctionUnsupported, "not supported for SAM/BAM");
    return NULL;
}

static void BAM_ReferenceWhack(void *const base, ctx_t ctx)
{
    BAM_Reference *const self = (BAM_Reference *)base;

    NGS_RefcountRelease(&self->parent->dad.dad, ctx);
}

bool BAM_ReferenceIteratorNext(NGS_Reference *const base, ctx_t ctx)
{
    BAM_Reference *const self = (BAM_Reference *)base;

    switch (self->state) {
        case 0:
            self->state = 1;
        case 1:
            ++self->cur;
            if (self->cur < self->parent->references->count)
                return true;
            else
                self->state = 2;
        case 2:
            break;
        case 3:
            USER_ERROR(xcCursorExhausted, "No more rows available");
    }
    return false;
}

BAM_Reference *BAM_ReferenceMake(BAM_ReadCollection *parent, ctx_t ctx, char const name[])
{
    static NGS_Reference_vt const vt =
    {
        /* NGS_Refcount */
        { BAM_ReferenceWhack },

        /* NGS_Reference */
        BAM_ReferenceGetCommonName,
        BAM_ReferenceGetCanonicalName,
        BAM_ReferenceGetIsCircular,
        BAM_ReferenceGetLength,
        BAM_ReferenceGetBases,
        BAM_ReferenceGetChunk,
        BAM_ReferenceGetAlignment,
        BAM_ReferenceGetAlignments,
        BAM_ReferenceGetAlignmentCount,
        BAM_ReferenceGetAlignmentSlice,
        BAM_ReferenceGetPileups,

        /* NGS_ReferenceIterator */
        BAM_ReferenceIteratorNext,
    };
    FUNC_ENTRY(ctx, rcSRA, rcCursor, rcConstructing);

    BAM_Reference *const rslt = calloc(1, sizeof(*rslt));
    if (rslt) {
        NGS_RefcountInit(ctx, &rslt->dad.dad, &vt.dad, "BAM_Reference", name);
        rslt->parent = (BAM_ReadCollection *)NGS_RefcountDuplicate(&parent->dad.dad, ctx);
    }
    else
        SYSTEM_ABORT(xcNoMemory, "allocating BAM_Reference %u bytes", sizeof(*rslt));

    return rslt;
}

static NGS_Reference * BAM_ReadCollectionReferences(NGS_ReadCollection *const vp, ctx_t ctx) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_ReadCollection *const self = (BAM_ReadCollection *)vp;

    BAM_Reference *const rslt = BAM_ReferenceMake(self, ctx, self->path + self->namestart);

    return &rslt->dad;
}

static int32_t FindReference(BAM_ReadCollection const *const self, char const name[])
{
    int32_t i;
    int32_t const n = self->references->count;
    size_t const nlen = strlen(name);

    for (i = 0; i < n; ++i) {
        char const *const fnd = self->references->ref[i].name;
        size_t const flen = strlen(fnd);

        if (flen == nlen && strcase_cmp(name, nlen, fnd, nlen, (uint32_t)nlen) == 0)
            return i;
    }
    return -1;
}

static NGS_Reference * BAM_ReadCollectionReference(NGS_ReadCollection *const vp, ctx_t ctx, char const spec[]) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_ReadCollection *const self = (BAM_ReadCollection *)vp;

    int32_t const fnd = FindReference(self, spec);
    if (fnd >= 0) {
        BAM_Reference *const rslt = BAM_ReferenceMake(self, ctx, self->path + self->namestart);

        if (!FAILED()) {
            rslt->state = 3;
            rslt->cur = fnd;
        }
        return &rslt->dad;
    }
    else {
        USER_ERROR(xcStringNotFound, "Read Group '%s' is not found", spec);
        return NULL;
    }
}

static NGS_Alignment * BAM_ReadCollectionAlignments(NGS_ReadCollection *const vp, ctx_t ctx, bool const wants_primary, bool const wants_secondary) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_ReadCollection *const self = (BAM_ReadCollection *)vp;

    if (!wants_primary && !wants_secondary) {
        return NGS_AlignmentMakeNull(ctx, self->path + self->namestart, self->namelen);
    }
    else {
        NGS_Alignment *const rslt = BAM_AlignmentMake(ctx, wants_primary, wants_secondary, BAM_GetRecord, NGS_RefcountDuplicate(&self->dad.dad, ctx), self->path + self->namestart);

        return rslt;
    }
}

static NGS_Alignment * BAM_ReadCollectionAlignment(NGS_ReadCollection *const vp, ctx_t ctx, char const alignmentIdStr[]) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return NULL;
}

static uint64_t BAM_ReadCollectionAlignmentCount(NGS_ReadCollection *const vp, ctx_t ctx, bool const wants_primary, bool const wants_secondary) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return 0;
}

static NGS_Alignment * BAM_ReadCollectionAlignmentRange(NGS_ReadCollection *const vp, ctx_t ctx, uint64_t const first, uint64_t const count, bool const wants_primary, bool const wants_secondary) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return NULL;
}

static struct NGS_Read * BAM_ReadCollectionReads(NGS_ReadCollection *const vp, ctx_t ctx, bool const wants_full, bool const wants_partial, bool const wants_unaligned) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return NULL;
}

static struct NGS_Read * BAM_ReadCollectionRead(NGS_ReadCollection *const vp, ctx_t ctx, char const readIdStr[]) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return NULL;
}

static uint64_t BAM_ReadCollectionReadCount(NGS_ReadCollection *const vp, ctx_t ctx) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return 0;
}

static struct NGS_Read * BAM_ReadCollectionReadRange(NGS_ReadCollection *const vp, ctx_t ctx, uint64_t const first, uint64_t const count, bool const wants_full, bool const wants_partial, bool const wants_unaligned) {
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    UNIMPLEMENTED();
    return NULL;
}

static void *Alloc(ctx_t ctx, size_t const size, bool const clear)
{
    void *const rslt = clear ? calloc(1, size) : malloc(size);

    if (rslt == NULL) {
        SYSTEM_ABORT(xcNoMemory, "allocating %u bytes", size);
    }
    return rslt;
}

static bool FillBuffer(BAM_ReadCollection *const self, ctx_t ctx, uint64_t const fpos)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcReading);
    size_t nread;
    rc_t const rc = KFileRead(self->fp, fpos,
                              self->iobuffer,
                              IO_BLK_SIZE, &nread);

    if (rc == 0) {
        self->cpos = fpos;
        self->fpos = fpos + nread;
        self->zs.avail_in = (uInt)nread;
        self->zs.next_in = (Bytef *)self->iobuffer;

        return true;
    }

    SYSTEM_ABORT(xcUnexpected, "reading '%s' rc = %R", self->path, rc);
    return false;
}

static bool ReadZlib(BAM_ReadCollection *self, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcData, rcDecoding);
    uint64_t const bpos = (self->cpos << 16) + (self->zs.next_in ? (self->zs.next_in - self->iobuffer) : 0);
    int zrc;

    self->zs.next_out = self->bambuffer;
    self->zs.avail_out = BAM_BLK_MAX;
    self->zs.total_out = 0;
    self->bam_cur = 0;

    if (self->zs.avail_in == 0) {
FILL_BUFFER:
        FillBuffer(self, ctx, self->fpos);
        if (self->zs.avail_in == 0) /* EOF */
            return true;
    }
    zrc = inflate(&self->zs, Z_FINISH);
    if (zrc == Z_STREAM_END) {
        /* inflateReset clobbers this value but we want it */
        uLong const total_out = self->zs.total_out;

        zrc = inflateReset(&self->zs);
        assert(zrc == Z_OK);
        self->zs.total_out = total_out;
        self->bpos = bpos;
        return true;
    }
    if (zrc != Z_OK && zrc != Z_BUF_ERROR) {
        INTERNAL_ABORT(xcUnexpected, "reading '%s' zrc = %i, message = '%s'",
                       self->path, zrc, self->zs.msg);
        return false;
    }
    assert(self->zs.avail_in == 0);
    goto FILL_BUFFER;
}

static void Seek(BAM_ReadCollection *self, ctx_t ctx, uint64_t const ipos)
{
    uint64_t const fpos = (uint64_t)(ipos / BAM_BLK_MAX);
    unsigned const bpos = (unsigned)(ipos % BAM_BLK_MAX);

    if (fpos < self->cpos || fpos >= self->fpos || self->zs.next_in == NULL) {
        uint64_t const fudg = fpos % IO_BLK_SIZE;

        FillBuffer(self, ctx, fpos - fudg);
        if ((unsigned)self->zs.avail_in < fudg)
            return;
        self->zs.next_in += fudg;
        self->zs.avail_in -= fudg;
        self->zs.total_out = 0;
    }
    if (self->zs.total_out <= bpos)
        ReadZlib(self, ctx);

    self->bam_cur = bpos;
}

static size_t ReadN(BAM_ReadCollection *self, ctx_t ctx, size_t const N, void *Dst)
{
    uint8_t *const dst = Dst;
    size_t n = 0;

    while (n < N) {
        size_t const avail_out = N - n;
        size_t const avail_in = self->zs.total_out - self->bam_cur;

        if (avail_in) {
            size_t const copy = avail_out < avail_in ? avail_out : avail_in;
            memmove(dst + n, self->bambuffer + self->bam_cur, copy);
            self->bam_cur += copy;
            n += copy;
            if (n == N)
                break;
        }
        if (!ReadZlib(self, ctx) || self->zs.total_out == 0)
            break;
    }
    return n;
}

static uint16_t LE2UInt16(void const *src)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    union {
        uint8_t ch[2];
        uint16_t u16;
    } u;
    memmove(&u, src, 2);
    return u.u16;
#else
    return ((uint16_t)((uint8_t const *)src)[0]) | (((uint16_t)((uint8_t const *)src)[1]) << 8);
#endif
}

static uint32_t LE2UInt32(void const *src)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    union {
        uint8_t ch[4];
        uint32_t u32;
    } u;
    memmove(&u, src, 4);
    return u.u32;
#else
    return (uint32_t)(LE2UInt16(src)) || (((uint32_t)(LE2UInt16(((uint8_t const *)src) + 2))) << 16);
#endif
}

static uint64_t LE2UInt64(void const *src)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    union {
        uint8_t ch[8];
        uint64_t u64;
    } u;
    memmove(&u, src, 8);
    return u.u64;
#else
    return (uint64_t)(LE2UInt32(src)) || (((uint64_t)(LE2UInt32(((uint8_t const *)src) + 4))) << 32);
#endif
}

static int16_t LE2Int16(void const *src)
{
    return (int16_t)LE2UInt16(src);
}

static int32_t LE2Int32(void const *src)
{
    return (int32_t)LE2UInt32(src);
}

static int32_t ReadI32(BAM_ReadCollection *self, ctx_t ctx)
{
    int8_t ch[4];
    size_t const n = ReadN(self, ctx, 4, ch);

    if (FAILED())
        return 0;

    if (n == 4)
        return LE2Int32(ch);

    if (n)
        USER_ERROR(xcUnexpected, "reading '%s'; premature end of file", self->path);

    return 0;
}

static uint32_t ReadU32(BAM_ReadCollection *self, ctx_t ctx)
{
    return (uint32_t)ReadI32(self, ctx);
}

static int32_t get_refID(BAM_rec const *const rec)
{
    return LE2Int32(rec->data->refID);
}

static int32_t get_pos(BAM_rec const *const rec)
{
    return LE2Int32(rec->data->pos);
}

static uint8_t get_mq(BAM_rec const *const rec)
{
    return rec->data->bin_mq_nl[1];
}

static uint8_t get_nl(BAM_rec const *const rec)
{
    return rec->data->bin_mq_nl[0];
}

static uint16_t get_flag(BAM_rec const *const rec)
{
    return LE2Int16(&rec->data->flag_nc[2]);
}

static uint16_t get_nc(BAM_rec const *const rec)
{
    return LE2Int16(&rec->data->flag_nc[0]);
}

static int32_t get_lseq(BAM_rec const *const rec)
{
    return LE2Int32(rec->data->l_seq);
}

static int32_t get_next_refID(BAM_rec const *const rec)
{
    return LE2Int32(rec->data->next_refID);
}

static int32_t get_next_pos(BAM_rec const *const rec)
{
    return LE2Int32(rec->data->next_pos);
}

static int32_t get_tlen(BAM_rec const *const rec)
{
    return LE2Int32(rec->data->tlen);
}

static bool ReadBAMRecord(BAM_ReadCollection *const self, ctx_t ctx, BAM_rec out[1])
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    memset(out, 0, sizeof(out[0]));
    TRY(int32_t const datasize = ReadI32(self, ctx)) {
        if (datasize < 0) {
            USER_ABORT(xcUnexpected, "reading '%s', BAM record size < 0", self->path);
            return false;
        }
        if (datasize == 0)  /* EOF */
            return false;

        out[0].pos = (self->bpos << 16) | ((uint16_t)self->bam_cur - 4);
        if (datasize <= 32) {
            USER_ABORT(xcUnexpected, "reading '%s', BAM record too small, only %u bytes", self->path, (unsigned)datasize);
            return false;
        }
        out[0].size = datasize;
        if (self->bam_cur + datasize <= self->zs.total_out) {
            out[0].data = (void const *)(self->bambuffer + self->bam_cur);
            self->bam_cur += datasize;
            return true;
        }
        /* this probably never happens; it might even be an error */
        TRY(void *const tmp = Alloc(ctx, datasize, false)) {
            if (datasize != ReadN(self, ctx, datasize, tmp) && !FAILED()) {
                free(tmp);
                USER_ERROR(xcUnexpected, "reading '%s'; premature end of file", self->path);
                return false;
            }
            out[0].data = out[0].allocated = tmp;
            return true;
        }
    }
    return false;
}

static void CopyCIGAR(uint32_t dst[], uint32_t const src[], unsigned const count)
{
    unsigned i;

    for (i = 0; i < count; ++i) {
        uint32_t const value = LE2Int32(src + i);

        dst[i] = value;
    }
}

static void CopySEQ(char dst[], uint8_t const src[], unsigned const count)
{
    static char const tr[16] = "=ACMGRSVTWYHKDBN";
    unsigned i;
    unsigned const n = count >> 1;

    for (i = 0; i < n; ++i) {
        uint8_t const value = src[i];
        uint8_t const lo = value & 0x0F;
        uint8_t const hi = value >> 4;

        dst[2 * i + 0] = tr[hi];
        dst[2 * i + 1] = tr[lo];
    }
    if (count & 1) {
        uint8_t const value = src[n];
        uint8_t const hi = value >> 4;

        dst[count - 1] = tr[hi];
    }
}

static void CopyQUAL(char dst[], uint8_t const src[], unsigned const n)
{
    unsigned i;

    for (i = 0; i < n; ++i) {
        int const ch = src[i] + 33;

        dst[i] = ch < 0xFF ? ch : -1;
    }
}

BAM_Record *BAM_GetRecord(NGS_Refcount *const object, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    BAM_ReadCollection *const self = (BAM_ReadCollection *)object;
    BAM_rec raw;

    if (ReadBAMRecord(self, ctx, &raw)) {
        BAM_Record *rslt = NULL;
        bool self_unmapped = false;
        bool mate_unmapped = false;
        uint16_t const flag = get_flag(&raw);
        uint16_t const raw_nc = get_nc(&raw);
        int32_t const refID = get_refID(&raw);
        int32_t const pos = get_pos(&raw);
        int32_t const next_refID = get_next_refID(&raw);
        int32_t const next_pos = get_next_pos(&raw);

        if ((flag & 0x0004) != 0 || 0 > refID || refID >= self->references->count || pos < 0 || raw_nc == 0)
            self_unmapped = true;
        if ((flag & 0x0001) == 0 || (flag & 0x0008) != 0 || 0 > next_refID || next_refID >= self->references->count || next_pos < 0)
            mate_unmapped = true;
        {
            uint8_t const raw_nl = get_nl(&raw);
            unsigned const nl = raw_nl < 2 ? 0 : raw_nl;
            unsigned const nc = self_unmapped ? 0 : raw_nc;
            unsigned const sl = get_lseq(&raw);
            char const *read_name = (void const *)raw.data->readname;
            uint32_t const *cigar = (void const *)(&read_name[nl]);
            uint8_t const *seq    = (void const *)(&cigar[nc]);
            uint8_t const *qual   = (void const *)(&seq[(sl + 1) >> 1]);
            uint8_t const *extra  = (void const *)(&qual[sl]);
            size_t const extralen = &raw.data->refID[raw.size] - extra;
            size_t const need = (uint8_t const *)&(((BAM_Record const *)NULL)->cigar[nc]) - ((uint8_t const *)NULL) + 2 * sl + nl + extralen;

            rslt = Alloc(ctx, need, false);
            rslt->seqlen   = sl;
            rslt->ncigar   = nc;
            rslt->extralen = (unsigned)extralen;
            rslt->SEQ      = (void const *)&rslt->cigar[nc];
            rslt->QUAL     = (void const *)&rslt->SEQ[sl];
            rslt->QNAME    = (void const *)&rslt->QUAL[sl];
            rslt->extra    = (void const *)&rslt->QNAME[nl];
            if (nl == 0)
                rslt->QNAME = NULL;

            rslt->TLEN = get_tlen(&raw);
            rslt->FLAG = flag;
            rslt->MAPQ = get_mq(&raw);
            if (self_unmapped) {
                rslt->POS = 0;
                rslt->RNAME = NULL;
                rslt->REFID = -1;
            }
            else {
                rslt->POS = pos + 1;
                rslt->REFID = refID;
                rslt->RNAME = self->references->ref[refID].name;
            }
            if (mate_unmapped) {
                rslt->PNEXT = 0;
                rslt->RNEXT = NULL;
            }
            else {
                rslt->PNEXT = next_pos + 1;
                rslt->RNEXT = self->references->ref[next_refID].name;
            }

            memmove((void *)rslt->extra, extra, extralen);
            CopySEQ((void *)rslt->SEQ, seq, sl);
            CopyQUAL((void *)rslt->QUAL, qual, sl);

            if (nl)
                memmove((void *)rslt->QNAME, read_name, nl);

            if (!self_unmapped)
                CopyCIGAR((void *)rslt->cigar, cigar, nc);
        }
        free(raw.allocated);
        return rslt;
    }
    return NULL;
}

static unsigned ComputeRefLen(size_t const count, uint32_t const cigar[])
{
    unsigned rslt = 0;
    unsigned i;

    for (i = 0; i < count; ++i) {
        uint32_t const op = cigar[i];
        unsigned const len = op >> 4;
        int const code = op & 0x0F;

        switch (code) {
            case 0: /* M */
            case 2: /* D */
            case 3: /* N */
            case 7: /* = */
            case 8: /* X */
                rslt += len;
                break;
        }
    }
    return rslt;
}

static BAM_Record *BAM_GetRecordSliced(NGS_Refcount *const object, ctx_t ctx)
{
    BAM_RefIndexSlice *const self = (BAM_RefIndexSlice *)object;

    if (self->chunk == NULL || self->chunks == self->cur)
        return NULL;
    for ( ; ; ) {
        BAM_Record *const rec = BAM_GetRecord(&self->parent->dad.dad, ctx);
        bool done = true;

        if (rec) do {
            if (rec->POS > 0) {
                if (rec->REFID != self->refID)
                    break;

                uint64_t const pos = rec->POS - 1;
                if (pos >= self->end)
                    break;

                unsigned const reflen = ComputeRefLen(rec->ncigar, rec->cigar);
                if (pos + reflen > self->start)
                    return rec;

                if (self->cur + 1 >= self->chunks)
                    break;

                Seek(self->parent, ctx, self->chunk[++self->cur]);
            }
            done = false;
        } while (0);
        free(rec);
        if (done)
            return NULL;
    }
}

static unsigned CountWhereLess(uint64_t const max,
                               unsigned const N,
                               RefIndexChunk const chunk[])
{
    if (max) {
        unsigned count = 0;
        unsigned i;

        for (i = 0; i < N; ++i) {
            if (chunk[i].first < max)
                ++count;
        }
        return count;
    }
    else
        return N;
}

static unsigned CopyWhereLess(uint64_t rslt[], uint64_t const max,
                              unsigned const N, RefIndexChunk const chunk[])
{
    if (max) {
        unsigned count = 0;
        unsigned i;

        for (i = 0; i < N; ++i) {
            uint64_t const first = chunk[i].first;

            if (first < max) {
                rslt[count] = first;
                ++count;
            }
        }
        return count;
    }
    else {
        unsigned i;

        for (i = 0; i < N; ++i)
            rslt[i] = chunk[i].first;

        return N;
    }
}

static void SortPositions(unsigned const N, uint64_t array[])
{
#define SAVE(A) uint64_t const tmp = array[A]
#define LOAD(A) ((void)(array[A] = tmp))
#define COPY(DST, SRC) ((void)(array[(DST)] = array[(SRC)]))
#define LESS(A, B) (array[A] < array[B])
    ELEMSORT(N);
#undef SAVE
#undef LOAD
#undef COPY
#undef LESS
}

static int IndexSlice(RefIndex const *const self,
                      uint64_t **rslt,
                      unsigned const beg,
                      unsigned const end)
{
    unsigned const first[] = { 1, 9, 73, 585, 4681 };
    unsigned const cnt = end - 1 - beg;
    unsigned const maxintvl = (end >> 14) + 1;
    uint64_t const maxpos = maxintvl < self->numintv ?
                            self->interval[maxintvl] : self->off_end;
    unsigned int_beg[5], int_cnt[5];
    unsigned i;
    unsigned count = CountWhereLess(maxpos, self->bins[0].count,
                                    &self->chunk[self->bins[0].offset]);

    for (i = 0; i < 5; ++i) {
        unsigned const shift = 14 + 3 * (4 - i);

        int_beg[i] = (beg >> shift) + first[i];
        int_cnt[i] = (cnt >> shift) + 1;
    }
    for (i = 0; i < 5; ++i) {
        unsigned const beg = int_beg[i];
        unsigned const N = int_cnt[i];
        unsigned j;

        for (j = 0; j < N; ++j) {
            RefIndexBinInfo const bin = self->bins[beg + j];

            count += CountWhereLess(maxpos, bin.count, &self->chunk[bin.offset]);
        }
    }
    if (count == 0)
        return 0;
    else {
        uint64_t array[count];
        unsigned j = CopyWhereLess(array, maxpos, self->bins[0].count,
                                   &self->chunk[self->bins[0].offset]);

        for (i = 0; i < 5; ++i) {
            unsigned const beg = int_beg[i];
            unsigned const N = int_cnt[i];
            unsigned ii;

            for (ii = 0; ii < N; ++ii) {
                RefIndexBinInfo const bin = self->bins[beg + ii];
                unsigned const copied = CopyWhereLess(&array[j], maxpos,
                                                      bin.count,
                                                      &self->chunk[bin.offset]);

                j += copied;
            }
        }
        SortPositions(count, array);

        if ((*rslt = malloc(count * sizeof(array[0]))) == NULL)
            return -1;

        memmove(*rslt, array, count * sizeof(array[0]));
        return count;
    }
}

static void LoadIndex_Bins(RefIndex *const self,
                           unsigned const N, char const data[])
{
    unsigned i;
    unsigned j = 0;
    size_t offset = 0;

    for (i = 0; i < N; ++i) {
        uint32_t const bin    = LE2UInt32(data + offset + 0);
        int32_t  const n_chunk = LE2Int32(data + offset + 4);

        if (bin == MAX_BIN && n_chunk == 2) {
            uint64_t const off_beg    = LE2UInt64(data + offset +  8);
            uint64_t const off_end    = LE2UInt64(data + offset + 16);
            uint64_t const n_mapped   = LE2UInt64(data + offset + 24);
            uint64_t const n_unmapped = LE2UInt64(data + offset + 32);

            self->off_beg    = off_beg;
            self->off_end    = off_end;
            self->n_mapped   = n_mapped;
            self->n_unmapped = n_unmapped;
        }
        else if (bin < MAX_BIN) {
            unsigned ii;

            self->bins[bin].count = n_chunk;
            self->bins[bin].offset = j;
            for (ii = 0; ii < n_chunk; ++ii) {
                uint64_t const beg = LE2UInt64(data + offset + 16 * ii +  8);
             /* uint64_t const end = LE2UInt64(data + offset + 16 * ii + 16); */

                self->chunk[j + ii].first = beg;
             /* self->chunk[j + ii].last  = end; */
            }
            j += n_chunk;
        }
        offset += 8 + 16 * n_chunk;
    }
    self->numchnk = j;
}

static void LoadIndex_Intervals(RefIndex *const self,
                                unsigned const N, char const data[])
{
    uint64_t last = 0;
    unsigned i;

    for (i = 0; i < N; ++i) {
        uint64_t const intvl = LE2UInt64(data + 8 * i);

        self->interval[i] = intvl == last ? 0 : intvl;
        last = intvl;
    }
    while (i > 0 && self->interval[i - 1] == 0)
        --i;
    self->numintv = i;
}

static size_t LoadIndex_3(HeaderRefInfo *const self, char const data[],
                          void const *const endp)
{
    if ((void const *)(data + 4) < endp) {
        int32_t const n_bin = LE2Int32(data);
        unsigned chunks = 0;
        size_t offset = 4;
        unsigned i;

        if (n_bin < 0)
            return 0;

        for (i = 0; i < n_bin; ++i) {
            if ((void const *)(data + offset + 8) < endp) {
                int32_t const n_chunk = LE2Int32(data + offset + 4);

                if (n_chunk < 0)
                    return 0;

                chunks += n_chunk;
                offset += 8 + 16 * n_chunk;
                continue;
            }
            return 0;
        }
        if ((void const *)(data + offset + 4) < endp) {
            int32_t const n_intv = LE2Int32(data + offset);

            if ((void const *)(data + offset + 4 + n_intv * 8) <= endp) {
                self->index = calloc(1, ((uint8_t const *)&((RefIndex const *)NULL)->chunk[chunks])-((uint8_t const *)NULL));
                if (self->index) {
                    LoadIndex_Bins(self->index, n_bin, data + 4);
                    LoadIndex_Intervals(self->index, n_intv, data + offset + 4);

                    return offset + 4 + n_intv * 8;
                }
            }
        }
    }
    return 0;
}

static void LoadIndex_2(BAM_ReadCollection *const self, ctx_t ctx,
                        size_t const datasize, char const data[])
{
    void const *const endp = data + datasize;

    if (datasize >= 8 && memcmp(data, "BAI\1", 4) == 0) {
        int32_t const n_ref = LE2Int32(data + 4);

        if (n_ref == self->references->count) {
            size_t offset = 8;
            unsigned i;

            for (i = 0; i < n_ref; ++i) {
                size_t const size = LoadIndex_3(&self->references->ref[i], data + offset, endp);

                if (size == 0)
                    goto BAD;

                offset += size;
            }
        }
    }
    return;

BAD:
    {
        unsigned i;

        for (i = 0; i < self->references->count; ++i) {
            free(self->references->ref[i].index);
            self->references->ref[i].index = NULL;
        }
    }
}

static rc_t OpenIndex(KFile const **const rslt, char const basename[])
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir(&dir);

    if (rc == 0) {
        rc = KDirectoryOpenFileRead(dir, rslt, "%s.bai", basename);
        KDirectoryRelease(dir);
    }
    return rc;
}

static void LoadIndex(BAM_ReadCollection *const self, ctx_t ctx)
{
    KFile const *fp;
    rc_t rc = OpenIndex(&fp, self->path);

    if (rc == 0) {
        uint64_t fsize;
        size_t nread;
        char *data;

        rc = KFileSize(fp, &fsize);
        data = malloc(fsize);
        if (data) {
            rc = KFileReadAll(fp, 0, data, fsize, &nread);
            if (rc == 0 && nread == fsize) {
                LoadIndex_2(self, ctx, fsize, data);
            }
            else {
                INTERNAL_ERROR ( xcUnexpected, "KFileReadAll(%lu) rc = %R nread = %lu", fsize, rc, (uint64_t)nread );
            }
            free(data);
        }
        KFileRelease(fp);
    }
    else {
        INTERNAL_ERROR ( xcUnexpected, "OpenIndex(%s) rc = %R", self->path, rc );
    }
}

static void BAM_ReferenceInfoWhack(BAM_ReferenceInfo *const self)
{
    unsigned i;

    for (i = 0; i < self->count; ++i) {
        free(self->ref[i].name);
        free(self->ref[i].index);
    }
}

static void LoadHeaderRefs(BAM_ReadCollection *const self, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    unsigned i;
    unsigned const nrefs = self->references->count;
    HeaderRefInfo *const ref = &self->references->ref[0];

    for (i = 0; i < nrefs; ++i) {
        TRY(uint32_t const namelen = ReadU32(self, ctx)) {
            if (namelen > INT32_MAX) {
                USER_ERROR(xcUnexpected, "reading '%s', reference name length < 0", self->path);
                break;
            }
            ref[i].ordinal = i;
            ON_FAIL(ref[i].name = Alloc(ctx, namelen, false)) break;
            ON_FAIL(ReadN(self, ctx, namelen, ref[i].name)) break;
            ON_FAIL(ref[i].length = ReadU32(self, ctx)) break;
        }
        else
            break;
    }
    if (FAILED()) {
        BAM_ReferenceInfoWhack(self->references);
    }
}

static void ReadHeaderRefs(BAM_ReadCollection *const self, ctx_t ctx, char text[], unsigned const lines, unsigned const ends[])
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    TRY(uint32_t const nrefs = ReadU32(self, ctx)) {
        if (nrefs > INT32_MAX) {
            USER_ERROR(xcUnexpected, "reading '%s', reference count < 0", self->path);
            return;
        }
        TRY(self->references = Alloc(ctx, ((uint8_t const *)&(((BAM_ReferenceInfo const *)NULL)->ref[nrefs]))-((uint8_t const *)NULL), true)) {
            self->references->count = nrefs;
            TRY(LoadHeaderRefs(self, ctx)) {
                return;
            }
            free(self->references);
            self->references = NULL;
        }
    }
}

static char *ReadHeaderText(BAM_ReadCollection *const self, ctx_t ctx, size_t *const length)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    TRY(uint32_t const textlen = ReadU32(self, ctx)) {
        if (textlen > INT32_MAX) {
            USER_ERROR(xcUnexpected, "reading '%s', header length < 0", self->path);
        }
        else if (textlen > 0) {
            TRY(char *const text = Alloc(ctx, textlen, false)) {
                if (textlen == ReadN(self, ctx, textlen, text) && !FAILED()) {
                    *length = textlen;
                    return text;
                }
                free(text);
            }
        }
    }
    *length = 0;
    return NULL;
}

static bool HeaderCheckSignature(BAM_ReadCollection *const self, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    static char const sig[4] = "BAM\1";
    char act[4];

    if (ReadN(self, ctx, 4, act) == 4)
        return memcmp(sig, act, 4) == 0;
    return false;
}

static void ReadHeader(BAM_ReadCollection *self, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);

    if (HeaderCheckSignature(self, ctx)) {
        size_t textlen;
        TRY(char *const text = ReadHeaderText(self, ctx, &textlen)) {
            self->headerText = text;
            ReadHeaderRefs(self, ctx, NULL, 0, NULL);
        }
    }
    else {
        USER_ERROR(xcUnexpected, "reading '%s', invalid BAM signature", self->path);
    }
}

static KDirectory *GetCWD(ctx_t ctx)
{
    KDirectory *dir;
    rc_t const rc = KDirectoryNativeDir(&dir);
    if (rc) {
        INTERNAL_ABORT(xcUnexpected, "KDirectoryNativeDir failed rc = %R", rc);
        return NULL;
    }
    return dir;
}

static KFile const *OpenRead(ctx_t ctx, char const path[])
{
    KFile const *fp = NULL;
    TRY(KDirectory *const dir = GetCWD(ctx))
    {
        rc_t const rc = KDirectoryOpenFileRead(dir, &fp, path);

        KDirectoryRelease(dir);
        if (rc) {
            USER_ERROR(xcUnexpected, "'%s' failed to open for read rc = %R", path, rc);
            return NULL;
        }
    }
    return fp;
}

static void *AllocIOBuffer(ctx_t ctx)
{
    return Alloc(ctx, IO_BLK_SIZE, false);
}

static char *DuplicatePath(ctx_t ctx, char const path[], size_t const n)
{
    TRY(void *const rslt = Alloc(ctx, n + 1, false)) {
        memmove(rslt, path, n + 1);
        return rslt;
    }
    return NULL;
}

static size_t LastPathElement(size_t const length, char const path[/* length */])
{
    size_t i = length;

    while (i) {
        size_t const j = i - 1;
        int const ch = path[j];

        if (ch == '/')
            return i;

        i = j;
    }
    return 0;
}

static void InflateInit(BAM_ReadCollection *const self, ctx_t ctx)
{
    int const zrc = inflateInit2(&self->zs, MAX_WBITS + 16);

    switch (zrc) {
    case Z_OK:
        break;
    case Z_MEM_ERROR:
        SYSTEM_ABORT(xcNoMemory, "allocating decompressor");
        break;
    default:
        INTERNAL_ABORT(xcUnexpected, "allocating decompressor zlib rc = %i", zrc);
        break;
    }
}

static
BAM_ReadCollection *BAM_ReadCollectionInit(BAM_ReadCollection *const self, ctx_t ctx, char const path[])
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    size_t const namelen = strlen(path);

    TRY(char *const pathcopy = DuplicatePath(ctx, path, namelen)) {
        size_t const namestart = LastPathElement(namelen, pathcopy);

        self->path = pathcopy;
        self->namelen = (unsigned)(namelen - namestart);
        self->namestart = (unsigned)namestart;

        TRY(self->fp = OpenRead(ctx, path)) {
            TRY(self->iobuffer = AllocIOBuffer(ctx)) {
                TRY(InflateInit(self, ctx)) {
                    TRY(ReadHeader(self, ctx)) {
                        LoadIndex(self, ctx);
                    }
                }
            }
        }
    }
    return self;
}

NGS_ReadCollection * NGS_ReadCollectionMakeBAM(ctx_t ctx, char const path[])
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcOpening);
    void *self = malloc(sizeof(BAM_ReadCollection));
    if (self) {
        static NGS_ReadCollection_vt const vt =
        {
            /* NGS_Refcount */
            { BAM_ReadCollectionWhack },

            /* NGS_ReadCollection */
            BAM_ReadCollectionName,
            BAM_ReadCollectionReadGroups,
            BAM_ReadCollectionReadGroup,
            BAM_ReadCollectionReferences,
            BAM_ReadCollectionReference,
            BAM_ReadCollectionAlignments,
            BAM_ReadCollectionAlignment,
            BAM_ReadCollectionAlignmentCount,
            BAM_ReadCollectionAlignmentRange,
            BAM_ReadCollectionReads,
            BAM_ReadCollectionRead,
            BAM_ReadCollectionReadCount,
            BAM_ReadCollectionReadRange,
        };
        NGS_ReadCollection *const super = &((BAM_ReadCollection *)self)->dad;

        memset(self, 0, sizeof(BAM_ReadCollection));
        TRY(NGS_ReadCollectionInit(ctx, super, &vt, "BAM_ReadCollection", path)) {
            TRY(BAM_ReadCollectionInit(self, ctx, path)) {
                return super;
            }
        }
        free(self);
    }
    else {
        SYSTEM_ABORT(xcNoMemory, "allocating BAM_ReadCollection ( '%s' )", path);
    }
    return NULL;
}
