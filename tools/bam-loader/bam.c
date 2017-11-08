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

#include <klib/defs.h>
#include <klib/debug.h>
#include <klib/sort.h>
#include <klib/rc.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/mmap.h>
#include <klib/printf.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/refcount.h>
#include <sysalloc.h>

#include <atomic32.h>
#include <strtol.h>

#include <vfs/path.h>
#include <vfs/path-priv.h>
#include <kfs/kfs-priv.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#if 1
/*_DEBUGGING*/
#include <stdio.h>
#include <os-native.h>
#endif

#include <endian.h>
#include <byteswap.h>

#include <zlib.h>

#include "bam-priv.h"

static rc_t BufferedFileRead(BufferedFile *const self)
{
    uint64_t const fpos = self->fpos + self->bmax;
    rc_t const rc = KFileRead(self->kf, fpos, self->buf, self->size, &self->bmax);
    if (rc) {
        DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("Error %R read %u bytes from SAM/BAM file at position %lu\n", rc, self->bmax, fpos));
        return rc;
    }
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("Read %u bytes from SAM/BAM file at position %lu\n", self->bmax, fpos));
    self->fpos = fpos;
    self->bpos = 0;
    return 0;
}

static rc_t BufferedFileInit(BufferedFile *const self, KFile const *const kf)
{
    rc_t const rc = KFileSize(kf, &self->fmax);
    if (rc || self->fmax == 0) {
        self->fmax = 0;
        self->size = PIPE_BUFFER_SIZE;
    }
    else
        self->size = self->fmax < RGLR_BUFFER_SIZE ? self->fmax : RGLR_BUFFER_SIZE;

    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("File size is %lu; Read buffer is %lu\n", self->fmax, self->size));
    self->buf = malloc(self->size);
    self->fpos = 0;
    self->bpos = 0;
    self->bmax = 0;
    self->kf = kf;
    KFileAddRef(kf);

    return self->buf != NULL ? 0 : RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
}

static void BufferedFileWhack(BufferedFile *const self)
{
    KFileRelease(self->kf);
    free(self->buf);
}

static uint64_t BufferedFileGetPos(BufferedFile const *const self)
{
    return self->fpos + self->bpos;
}

static float BufferedFileProPos(BufferedFile const *const self)
{
    return self->fmax == 0 ? -1.0 : (BufferedFileGetPos(self) / (double)self->fmax);
}

static rc_t BufferedFileSetPos(BufferedFile *const self, uint64_t const pos)
{
    if (pos >= self->fpos && pos < self->fpos + self->bmax) {
        self->bpos = (size_t)(pos - self->fpos);
    }
    else if (pos < self->fmax) {
        self->fpos = pos;
        self->bpos = 0;
        self->bmax = 0;
    }
    else
        return RC(rcAlign, rcFile, rcPositioning, rcParam, rcInvalid);
    return 0;
}

static uint64_t BufferedFileGetSize(BufferedFile const *const self)
{
    return self->fmax;
}

static int SAMFileRead1(SAMFile *const self)
{
    if (self->putback < 0) {
        if (self->file.bpos == self->file.bmax) {
            rc_t const rc = BufferedFileRead(&self->file);
            
            self->last = rc;
            if (rc || self->file.bmax == 0) return -1;
        }
        return ((char const *)self->file.buf)[self->file.bpos++];
    }
    else {
        int const x = self->putback;
        self->putback = -1;
        return x;
    }
}

static rc_t SAMFileLastError(SAMFile const *const self)
{
    return self->last ? self->last : SILENT_RC(rcAlign, rcFile, rcReading, rcData, rcInsufficient);
}

static void SAMFilePutBack(SAMFile *const self, int ch)
{
    if (self->file.bpos > 0)
        --self->file.bpos;
    else
        self->putback = ch;
}

static rc_t SAMFileInit(SAMFile *self, RawFile_vt *vt)
{
    static RawFile_vt const my_vt = {
        (rc_t (*)(void *, zlib_block_t, unsigned *))NULL,
        (uint64_t (*)(void const *))BufferedFileGetPos,
        (float (*)(void const *))BufferedFileProPos,
        (uint64_t (*)(void const *))BufferedFileGetSize,
        (rc_t (*)(void *, uint64_t))BufferedFileSetPos,
        (void (*)(void *))NULL
    };
    
    self->putback = -1;
    self->last = 0;
    *vt = my_vt;
    
    return 0;
}

/* MARK: BGZFile *** Start *** */

#define VALIDATE_BGZF_HEADER 1
#if (ZLIB_VERNUM < 0x1230)
#undef VALIDATE_BGZF_HEADER
#warning "zlib too old, inflateGetHeader not available, not validating BGZF headers"
#else
#endif

static rc_t BGZFileGetMoreBytes(BGZFile *self)
{
    rc_t const rc = BufferedFileRead(&self->file);
    if (rc)
        return rc;
    
    if (self->file.bmax == 0)
        return RC(rcAlign, rcFile, rcReading, rcData, rcInsufficient);

    self->zs.avail_in = (uInt)(self->file.bmax);
    self->zs.next_in = (Bytef *)self->file.buf;
    
    return 0;
}

static rc_t BGZFileRead(BGZFile *self, zlib_block_t dst, unsigned *pNumRead)
{
#if VALIDATE_BGZF_HEADER
    uint8_t extra[256];
    gz_header head;
#endif
    rc_t rc = 0;
    unsigned loops;
    int zr;
    
    *pNumRead = 0;
    if (self->file.bmax == 0 || self->zs.avail_in == 0) {
        rc = BGZFileGetMoreBytes(self);
        if (rc)
            return rc;
    }

#if VALIDATE_BGZF_HEADER
    memset(&head, 0, sizeof(head));
    head.extra = extra;
    head.extra_max = sizeof(extra);
    
    zr = inflateGetHeader(&self->zs, &head);
    assert(zr == Z_OK);
#endif
    
    self->zs.next_out = (Bytef *)dst;
    self->zs.avail_out = sizeof(zlib_block_t);

    for (loops = 0; loops != 2; ++loops) {
        {
            uLong const initial = self->zs.total_in;
            
            zr = inflate(&self->zs, Z_FINISH);
            {
                uLong const final = self->zs.total_in;
                uLong const len = final - initial;
                
                self->file.bpos += len;
            }
        }
        assert(self->zs.avail_in == self->file.bmax - self->file.bpos);
        
        switch (zr) {
        case Z_OK:
        case Z_BUF_ERROR:
            rc = BGZFileGetMoreBytes(self);
            if ( rc != 0 )
            {
                if ((int)GetRCObject(rc) == rcData && GetRCState(rc) == rcInsufficient)
                {
                    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("EOF in Zlib block after %lu bytes\n", self->file.fpos + self->file.bpos));
                    rc = RC(rcAlign, rcFile, rcReading, rcFile, rcTooShort);
                }
                return rc;
            }
            break;
        case Z_STREAM_END:
            DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("Zlib block size (before/after): %u/%u\n", self->zs.total_in, self->zs.total_out));
#if VALIDATE_BGZF_HEADER
            if (head.done) {
                unsigned const extra_len = head.extra_len;
                unsigned i;
                unsigned bsize = 0;
                
                DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("GZIP Header extra length: %u\n", extra_len));
                for (i = 0; i < extra_len; ) {
                    uint8_t const si1 = extra[i + 0];
                    uint8_t const si2 = extra[i + 1];
                    unsigned const slen = LE2HUI16(&extra[i + 2]);
                    
                    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("GZIP Header extra: %c%c(%u)\n", si1, si2, slen));
                    if (si1 == 'B' && si2 == 'C') {
                        bsize = 1 + LE2HUI16(&extra[i + 4]);
                        DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("BGZF Header extra field BC: bsize %u\n", bsize));
                        break;
                    }
                    i += slen + 4;
                }
                if (bsize == 0 || bsize != self->zs.total_in) {
                    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("BGZF Header extra field BC not found\n"));
                    rc = RC(rcAlign, rcFile, rcReading, rcFormat, rcInvalid); /* not BGZF */
                }
            }
            else {
                DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("GZIP Header not found\n"));
                rc = RC(rcAlign, rcFile, rcReading, rcFile, rcCorrupt);
            }
#endif
            *pNumRead = (unsigned)self->zs.total_out; /* <= 64k */
            zr = inflateReset(&self->zs);
            assert(zr == Z_OK);
            return rc;
        default:
            DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("Unexpected Zlib result %i: %s\n", zr, self->zs.msg ? self->zs.msg : "unknown"));
            return RC(rcAlign, rcFile, rcReading, rcFile, rcCorrupt);
        }
    }
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("Failed reading BAM file after %lu bytes\n", self->file.fpos + self->file.bpos));
    return RC(rcAlign, rcFile, rcReading, rcFile, rcTooShort);
}

static void BGZFileWhack(BGZFile *self)
{
    inflateEnd(&self->zs);
}

static rc_t BGZFileInit(BGZFile *const self, RawFile_vt *const vt)
{
    int i;
    static RawFile_vt const my_vt = {
        (rc_t (*)(void *, zlib_block_t, unsigned *))BGZFileRead,
        (uint64_t (*)(void const *))BufferedFileGetPos,
        (float (*)(void const *))BufferedFileProPos,
        (uint64_t (*)(void const *))BufferedFileGetSize,
        (rc_t (*)(void *, uint64_t))BufferedFileSetPos,
        (void (*)(void *))BGZFileWhack
    };
    
    *vt = my_vt;

    i = inflateInit2(&self->zs, MAX_WBITS + 16); /* max + enable gzip headers */
    switch (i) {
    case Z_OK:
        break;
    case Z_MEM_ERROR:
        return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    default:
        return RC(rcAlign, rcFile, rcConstructing, rcNoObj, rcUnexpected);
    }
    
    return 0;
}

static const char cigarChars[] = {
    ct_Match,
    ct_Insert,
    ct_Delete,
    ct_Skip,
    ct_SoftClip,
    ct_HardClip,
    ct_Padded,
    ct_Equal,
    ct_NotEqual
    /* ct_Overlap must not appear in actual BAM file */
};

static inline int opt_tag_cmp(char const a[2], char const b[2])
{
    int const d0 = (int)a[0] - (int)b[0];
    return d0 ? d0 : ((int)a[1] - (int)b[1]);
}

static int64_t OptTag_sort(void const *A, void const *B, void *ctx)
{
    BAM_Alignment const *const self = ctx;
    unsigned const a_off = ((struct offset_size_s const *)A)->offset;
    unsigned const b_off = ((struct offset_size_s const *)B)->offset;
    char const *const a = (char const *)&self->data->raw[a_off];
    char const *const b = (char const *)&self->data->raw[b_off];
    int const diff = opt_tag_cmp(a, b);
    
    if ( diff != 0 )
        return diff;
    else if ( a < b )
        return -1;
    else
        return a > b;
}

/* find the first occurence of tag OR if tag doesn't exist, where it should have been. */
static unsigned tag_search(BAM_Alignment const *const self, char const tag[2])
{
    unsigned f = 0;
    unsigned e = self->numExtra;
    
    while (f < e) {
        unsigned const m = f + ((e - f) >> 1);
        char const *const mtag = (char const *)&self->data->raw[self->extra[m].offset];
        int const d = opt_tag_cmp(tag, mtag);
        
        if (d > 0)
            f = m + 1;
        else
            e = m;
    }
    return f;
}

static unsigned tag_count(BAM_Alignment const *const self,
                              char const tag[2],
                              unsigned const at)
{
    unsigned n;
    
    for (n = 0; n + at < self->numExtra; ++n) {
        if (opt_tag_cmp(tag, (char const *)&self->data->raw[self->extra[n + at].offset]) != 0)
            break;
    }
    return n;
}

static unsigned Modulus(int i, unsigned N)
{
    while (i < 0)
        i += N;
    return i % N;
}

static struct offset_size_s const *getTag(BAM_Alignment const *const self,
                                          char const tag[2],
                                          int const which)
{
    unsigned const fnd = tag_search(self, tag);
    unsigned const run = tag_count(self, tag, fnd);
    return run > 0 ? &self->extra[fnd + Modulus(which, run)] : NULL;
}

static struct offset_size_s const *get_CS_info(BAM_Alignment const *cself)
{
    return getTag(cself, "CS", 0);
}

static struct offset_size_s const *get_CQ_info(BAM_Alignment const *cself)
{
    return getTag(cself, "CQ", 0);
}

static char const *get_RG(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "RG", 0);
    return (char const *)((x != NULL && cself->data->raw[x->offset + 2] == 'Z') ? &cself->data->raw[x->offset + 3] : NULL);
}

static char const *get_CS(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = get_CS_info(cself);
    return (char const *)((x != NULL && cself->data->raw[x->offset + 2] == 'Z') ? &cself->data->raw[x->offset + 3] : NULL);
}

static uint8_t const *get_CQ(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = get_CQ_info(cself);
    return (uint8_t const *)((x != NULL && cself->data->raw[x->offset + 2] == 'Z') ? &cself->data->raw[x->offset + 3] : NULL);
}

static struct offset_size_s const *get_OQ_info(BAM_Alignment const *cself)
{
    return getTag(cself, "OQ", 0);
}

static uint8_t const *get_OQ(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = get_OQ_info(cself);
    return (uint8_t const *)((x != NULL && cself->data->raw[x->offset + 2] == 'Z') ? &cself->data->raw[x->offset + 3] : NULL);
}

static char const *get_XT(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "XT", 0);
    return (char const *)((x != NULL && cself->data->raw[x->offset + 2] == 'Z') ? &cself->data->raw[x->offset + 3] : NULL);
}

static uint8_t const *get_XS(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "XS", -1); /* want last one */
    return (uint8_t const *)((x != NULL && cself->data->raw[x->offset + 2] == 'A') ? &cself->data->raw[x->offset + 3] : NULL);
}

static struct offset_size_s const *get_CG_ZA_info(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "ZA", 0);
    return x;
}

static struct offset_size_s const *get_CG_ZI_info(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "ZI", 0);
    return x;
}

static struct offset_size_s const *get_CG_GC_info(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "GC", 0);
    return x;
}

static struct offset_size_s const *get_CG_GS_info(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "GS", 0);
    return x;
}

static struct offset_size_s const *get_CG_GQ_info(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "GQ", 0);
    return x;
}

static char const *get_BX(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "BX", 0);
    return (char const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static char const *get_CB(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "CB", 0);
    return (char const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static char const *get_UB(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "UB", 0);
    return (char const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static char const *get_BC(BAM_Alignment const *cself)
{
    struct offset_size_s const *const x = getTag(cself, "BC", 0);
    return (char const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

/* MARK: BAM_File Reading functions */

/* returns (rcData, rcInsufficient) if eof */
static rc_t BAM_FileFillBuffer(BAM_File *self)
{
    rc_t const rc = self->vt.FileRead(&self->file, self->buffer, &self->bufSize);
    if (rc)
        return rc;
    if (self->bufSize == 0 || self->bufSize <= self->bufCurrent)
        return RC(rcAlign, rcFile, rcReading, rcData, rcInsufficient);
    return 0;
}

static rc_t BAM_FileReadn(BAM_File *self, const unsigned len, uint8_t dst[/* len */]) {
    rc_t rc;
    unsigned cur;
    unsigned n = 0;
    
    if (len == 0)
        return 0;
    
    for (cur = 0; ; cur += n) {
        if (self->bufSize > self->bufCurrent) {
            n = self->bufSize - self->bufCurrent;
            if (cur + n > len)
                n = len - cur;
            memmove(&dst[cur], &self->buffer[self->bufCurrent], n);
            self->bufCurrent += n;
        }
        if (self->bufCurrent != self->bufSize && self->bufSize != 0)
            return 0;
        if (self->bufSize != 0) {
            /* a seek has not just been done so update the file position.
             * if we didn't and a request for the position is made before the
             * next read, we will not have the position of the next read.
             *
             * if a seek had just been done then
             *    self->fpos_cur == BGZFileGetPos(&self->file)
             * is already true.
             */
            self->fpos_cur = self->vt.FileGetPos(&self->file);
            self->bufCurrent = 0;
            self->bufSize = 0;
            if (cur + n == len)
                return 0;
        }

        rc = BAM_FileFillBuffer(self);
        if (rc)
            return rc;
    }
}

static void const *BAM_FilePeek(BAM_File const *const self, unsigned const offset)
{
    return &self->buffer[self->bufCurrent + offset];
}

static unsigned BAM_FileMaxPeek(BAM_File const *self)
{
    return self->bufSize > self->bufCurrent ? self->bufSize - self->bufCurrent : 0;
}

static int32_t BAM_FilePeekI32(BAM_File *self)
{
    return LE2HI32(BAM_FilePeek(self, 0));
}

static rc_t BAM_FileReadI32(BAM_File *self, int32_t *rhs)
{
    uint8_t buf[sizeof(int32_t)];
    rc_t rc = BAM_FileReadn(self, sizeof(int32_t), buf);
    
    if (rc == 0)
        *rhs = LE2HI32(buf);
    return rc;
}

/* MARK: BAM File header parsing functions */

static unsigned ParseHD(char const **rslt, unsigned const hlen, char hdata[])
{
    unsigned i;
    unsigned tag;
    unsigned value;
    int st = 0;
    int ws = 1;

    for (i = 0; i < hlen; ++i) {
        char const cc = hdata[i];
        
        if (ws && isspace(cc))
            continue;
        ws = 0;
        
        switch (st) {
        case 0:
            tag = i;
            ++st;
            break;
        case 1:
            if (isspace(cc))
                return 0;
            ++st;
            break;
        case 2:
            if (cc != ':')
                continue;
            hdata[i] = '\0';
            value = i + 1;
            ++st;
            break;
        case 3:
            if (cc == '\t' || cc == '\r' || cc == '\n') {
                hdata[i] = '\0';
                
                if (strcmp(&hdata[tag], "VN") == 0)
                    *rslt = &hdata[value];
                
                ++st;
                ws = 1;
            }
            break;
        case 4:
            if (cc == '@')
                return i;
            tag = i;
            st = 1;
            break;
        }
    }
    return st == 4 ? i : 0;
}

static unsigned ParseSQ(BAMRefSeq *rs, unsigned const hlen, char hdata[])
{
    unsigned i;
    unsigned tag;
    unsigned value;
    int st = 0;
    int ws = 1;
    
    for (i = 0; i < hlen; ++i) {
        char const cc = hdata[i];
        
        if (ws && isspace(cc))
            continue;
        ws = 0;
        
        switch (st) {
        case 0:
            tag = i;
            ++st;
            break;
        case 1:
            if (isspace(cc))
                return 0;
            ++st;
            break;
        case 2:
#define HACKAMATIC 1
#if HACKAMATIC
            if (cc != ':') {
                if (i + 1 >= hlen || hdata[i+1] != ':')
                    return 0;
                else
                    ++i;
            }
#else
            if (cc != ':')
                continue;
#endif
            hdata[i] = '\0';
            value = i + 1;
            ++st;
            break;
        case 3:
            if (cc == '\t' || cc == '\r' || cc == '\n') {
                unsigned j;
                
                hdata[i] = '\0';
                
                while (value < i && isspace(hdata[value]))
                    ++value;
                for (j = i; value < j && isspace(hdata[j - 1]); )
                    hdata[--j] = '\0';
                
                if (strcmp(&hdata[tag], "SN") == 0)
                    rs->name = &hdata[value];
                else if (strcmp(&hdata[tag], "LN") == 0)
                    rs->length = strtou64(&hdata[value], NULL, 10);
                else if (strcmp(&hdata[tag], "AS") == 0)
                    rs->assemblyId = &hdata[value];
#if HACKAMATIC
                else if (strcmp(&hdata[tag], "M5") == 0 || strcmp(&hdata[tag], "MD5") == 0)
#else
                else if (strcmp(&hdata[tag], "M5") == 0)
#endif
#undef HACKAMATIC
                {
                    unsigned len = j - value;
                    
                    if ((hdata[value] == '\'' || hdata[value] == '"') && hdata[value + len - 1] == hdata[value]) {
                        ++value;
                        len -= 2;
                    }
                    if (len == 32) {
                        rs->checksum = &rs->checksum_array[0];
                        for (j = 0; j != 16; ++j) {
                            int const ch1 = toupper(hdata[value + j * 2 + 0]);
                            int const ch2 = toupper(hdata[value + j * 2 + 1]);
                            
                            if (isxdigit(ch1) && isxdigit(ch2)) {
                                rs->checksum_array[j] =
                                    ((ch1 > '9' ? (ch1 - ('A' - 10)) : (ch1 - '0')) << 4) +
                                     (ch2 > '9' ? (ch2 - ('A' - 10)) : (ch2 - '0'));
                            }
                            else {
                                rs->checksum = NULL;
                                break;
                            }
                        }
                    }
                }
                else if (strcmp(&hdata[tag], "UR") == 0)
                    rs->uri = &hdata[value];
                else if (strcmp(&hdata[tag], "SP") == 0)
                    rs->species = &hdata[value];
                
                ++st;
                ws = 1;
            }
            break;
        case 4:
            if (cc == '@')
                return i;
            tag = i;
            st = 1;
            break;
        }
    }
    return st == 4 ? i : 0;
}

static unsigned ParseRG(BAMReadGroup *dst, unsigned const hlen, char hdata[])
{
    unsigned i;
    unsigned tag;
    unsigned value;
    int st = 0;
    int ws = 1;
    
    for (i = 0; i < hlen; ++i) {
        char const cc = hdata[i];
        
        if (ws && isspace(cc))
            continue;
        ws = 0;
        
        switch (st) {
        case 0:
            tag = i;
            ++st;
            break;
        case 1:
            if (isspace(cc))
                return 0;
            ++st;
            break;
        case 2:
            if (cc != ':')
                continue;
            hdata[i] = '\0';
            value = i + 1;
            ++st;
            break;
        case 3:
            if (cc == '\t' || cc == '\r' || cc == '\n') {
                unsigned j = i;

                hdata[i] = '\0';

                while (value < i && isspace(hdata[value]))
                    ++value;
                while (value < j && isspace(hdata[j - 1]))
                    hdata[--j] = '\0';
                
                if ((hdata[value] == '\"' || hdata[value] == '\'') && hdata[value] == hdata[j - 1]) {
                    ++value;
                    hdata[j - 1] = '\0';
                }
                if (strcmp(&hdata[tag], "ID") == 0)
                    dst->name = &hdata[value];
                else if (strcmp(&hdata[tag], "SM") == 0)
                    dst->sample = &hdata[value];
                else if (strcmp(&hdata[tag], "LB") == 0)
                    dst->library = &hdata[value];
                else if (strcmp(&hdata[tag], "DS") == 0)
                    dst->description = &hdata[value];
                else if (strcmp(&hdata[tag], "PU") == 0)
                    dst->unit = &hdata[value];
                else if (strcmp(&hdata[tag], "PI") == 0)
                    dst->insertSize = &hdata[value];
                else if (strcmp(&hdata[tag], "CN") == 0)
                    dst->center = &hdata[value];
                else if (strcmp(&hdata[tag], "DT") == 0)
                    dst->runDate = &hdata[value];
                else if (strcmp(&hdata[tag], "PL") == 0)
                    dst->platform = &hdata[value];
                
                ++st;
                ws = 1;
            }
            break;
        case 4:
            if (cc == '@')
                return i;
            tag = i;
            st = 1;
            break;
        }
    }
    return st == 4 ? i : 0;
}

static bool ParseHeader(BAM_File *self, char hdata[], size_t hlen) {
    unsigned rg = 0;
    unsigned sq = 0;
    unsigned i;
    unsigned tag;
    int st = 0;
    int ws = 1;
    
    for (i = 0; i < hlen; ++i) {
        char const cc = hdata[i];
        
        if (ws && isspace(cc))
            continue;
        ws = 0;
        
        switch (st) {
        case 0:
            if (cc == '@')
                ++st;
            else
                return false;
            break;
        case 1:
            if (isspace(cc))
                return false;
            tag = i;
            ++st;
            break;
        case 2:
            if (isspace(cc)) {
                hdata[i] = '\0';
                if (i - tag == 2) {
                    if (strcmp(&hdata[tag], "HD") == 0) {
                        unsigned const used = ParseHD(&self->version, hlen - i - 1, &hdata[i + 1]);
                        if (used == 0) return false;
                        i += used;
                        st = 0;
                        break;
                    }
                    if (strcmp(&hdata[tag], "SQ") == 0) {
                        unsigned const used = ParseSQ(&self->refSeq[sq++], hlen - i - 1, &hdata[i + 1]);
                        if (used == 0) return false;
                        i += used;
                        st = 0;
                        break;
                    }
                    if (strcmp(&hdata[tag], "RG") == 0) {
                        unsigned const used = ParseRG(&self->readGroup[rg++], hlen - i - 1, &hdata[i + 1]);
                        if (used == 0) return false;
                        i += used;
                        st = 0;
                        break;
                    }
                }
                if (st == 2) {
                    ++st;
                    ws = 0;
                }
            }
            else if (i - tag > 2)
                ++st;
            break;
        case 3:
            if (cc == '\r' || cc == '\n') {
                st = 0;
                ws = 1;
            }
            break;
        }
    }
    return true;
}

static int64_t comp_ReadGroup(const void *A, const void *B, void *ignored) {
    BAMReadGroup const *const a = A;
    BAMReadGroup const *const b = B;

    /* make null names sort to the bottom */
    if (a->name == NULL || b->name == NULL)
        return 0;
    if (a->name == NULL)
        return 1;
    if (b->name == NULL)
        return -1;
    
    /* make empty names sort to the bottom */
    if (a->name[0] == '\0' || b->name[0] == '\0')
        return 0;
    if (a->name[0] == '\0')
        return 1;
    if (b->name[0] == '\0')
        return -1;
    
    return strcmp(a->name, b->name);
}

static int64_t comp_RefSeqName(const void *A, const void *B, void *ignored) {
    BAMRefSeq const *const a = A;
    BAMRefSeq const *const b = B;
    
    /* make null names sort to the bottom */
    if (a->name == NULL || b->name == NULL)
        return 0;
    if (a->name == NULL)
        return 1;
    if (b->name == NULL)
        return -1;
    
    /* make empty names sort to the bottom */
    if (a->name[0] == '\0' || b->name[0] == '\0')
        return 0;
    if (a->name[0] == '\0')
        return 1;
    if (b->name[0] == '\0')
        return -1;
    {
        int const cmp = strcmp(a->name, b->name);
        return cmp != 0 ? cmp : (int64_t)a->id - (int64_t)b->id;
    }
}

static unsigned MeasureHeader(unsigned *RG, unsigned *SQ, char const text[])
{
    unsigned size;
    unsigned cur = 0;

    for (size = 0; ; ++size) {
        int const ch = text[size];

        if (ch == '\0')
            return size;
        if (ch == '\n') {
            cur = 0;
            continue;
        }
        if (++cur == 3) {
            if (text[size - 1] == 'R' && ch == 'G') {
                ++*RG;
                continue;
            }
            if (text[size - 1] == 'S' && ch == 'Q') {
                ++*SQ;
                continue;
            }
        }
    }
}

static rc_t ProcessHeaderText(BAM_File *self, char const text[], bool makeCopy)
{
    unsigned RG = 0;
    unsigned SQ = 0;
    unsigned const size = MeasureHeader(&RG, &SQ, text);
    unsigned i;

    if (SQ) {
        self->refSeq = calloc(SQ, sizeof(self->refSeq[0]));
        if (self->refSeq == NULL)
            return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    self->refSeqs = SQ;

    if (RG) {
        self->readGroup = calloc(RG, sizeof(self->readGroup[0]));
        if (self->readGroup == NULL)
            return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    self->readGroups = RG;

    if (makeCopy) {
        void *const tmp = malloc(size + 1);
        if (tmp == NULL)
            return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
        self->header = tmp;  /* a const copy of the original */
        memmove(tmp, text, size + 1);
    }
    else
        self->header = text;
    {
    char *const copy = malloc(size + 1); /* an editable copy */
    if (copy == NULL)
        return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    self->headerData1 = copy; /* so it's not leaked */
    memmove(copy, text, size + 1);
    
    {
    bool const parsed = ParseHeader(self, copy, size);
    if (!parsed)
        return RC(rcAlign, rcFile, rcParsing, rcData, rcInvalid);
    }
    }
    for (i = 0; i < self->readGroups; ++i)
        self->readGroup[i].id = i;
    
    ksort(self->readGroup, self->readGroups, sizeof(self->readGroup[0]), comp_ReadGroup, NULL);
    
    /* remove read groups with missing and empty names */
    for (i = self->readGroups; i != 0; ) {
        BAMReadGroup const *const rg = &self->readGroup[--i];
        char const *const name = rg->name;
        if (name == NULL || name[0] == '\0') {
            (void)PLOGMSG(klogWarn, (klogWarn, "Read Group #$(rg) is missing ID in SAM header", "rg=%i", (int)rg->id));
            --self->readGroups;
        }
    }
    
    /* check for duplicate read groups names */
    for (i = 1; i < self->readGroups; ++i) {
        BAMReadGroup const *const a = &self->readGroup[i - 1];
        BAMReadGroup const *const b = &self->readGroup[i - 0];
        
        if (strcmp(a->name, b->name) == 0) {
            (void)PLOGMSG(klogWarn, (klogWarn, "Read Groups #$(r1) and #$(r2) have the same ID '$(id)' in SAM header",
                                     "r1=%i,r2=%i,id=%s", (int)a->id, (int)b->id, a->name));
        }
    }

    /* these id's are temporary, for reporting only
     * in BAM, they'll get the id from the second part of the header
     * in SAM, they'll get reassigned in alphabetical order
     */
    for (i = 0; i < self->refSeqs; ++i)
        self->refSeq[i].id = i;
    
    ksort(self->refSeq, self->refSeqs, sizeof(self->refSeq[0]), comp_RefSeqName, NULL);
    
    /* remove references with missing and empty names */
    for (i = self->refSeqs; i != 0; ) {
        BAMRefSeq const *const ref = &self->refSeq[--i];
        char const *const name = ref->name;
        if (name == NULL || name[0] == '\0') {
            (void)PLOGMSG(klogWarn, (klogWarn, "Reference #$(rg) is missing name in SAM header", "rg=%i", (int)ref->id));
            --self->refSeqs;
        }
    }
    
    /* check for and remove duplicate reference names */
    for (i = self->refSeqs; i > 1; --i) {
        BAMRefSeq *const a = &self->refSeq[i - 2];
        BAMRefSeq *const b = &self->refSeq[i - 1];
        
        if (strcmp(a->name, b->name) == 0) {
            (void)PLOGMSG(klogWarn, (klogWarn, "References #$(r1) and #$(r2) have the same name '$(id)' in SAM header",
                                     "r1=%i,r2=%i,id=%s", (int)a->id, (int)b->id, a->name));
            memmove(a, b, (self->refSeqs - i + 1) * sizeof(self->refSeq[0]));
            --self->refSeqs;
        }
    }
    
    /* check for zero-length references */
    for (i = 0; i != self->refSeqs; ++i) {
        BAMRefSeq const *const rs = &self->refSeq[i];
        
        if (rs->length == 0)
            (void)PLOGMSG(klogWarn, (klogWarn, "Reference '$(ref)' has zero length", "ref=%s", rs->name));
    }

    return 0;
}

static rc_t ReadMagic(BAM_File *self)
{
    uint8_t sig[4];
    rc_t rc = BAM_FileReadn(self, 4, sig);
    
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM signature: '%c%c%c' %u\n", sig[0], sig[1], sig[2], sig[3]));
    if (rc == 0 && (sig[0] != 'B' || sig[1] != 'A' || sig[2] != 'M' || sig[3] != 1))
        rc = RC(rcAlign, rcFile, rcReading, rcHeader, rcBadVersion);
    return rc;
}

static rc_t ReadHeaders(BAM_File *self,
                        char **headerText, size_t *headerTextLen,
                        char **refData, unsigned *numrefs)
{
    unsigned hlen;
    char *htxt = NULL;
    unsigned nrefs;
    char *rdat = NULL;
    unsigned rdsz;
    unsigned rdms;
    unsigned i;
    int32_t i32;
    rc_t rc = BAM_FileReadI32(self, &i32);
    
    if (rc) return rc;

    if (i32 < 0) {
        rc = RC(rcAlign, rcFile, rcReading, rcHeader, rcInvalid);
        goto BAILOUT;
    }
    hlen = i32;
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM Header text size: %u\n", hlen));

    htxt = malloc(hlen + 1);
    if (htxt == NULL) {
        rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
        goto BAILOUT;
    }
    
    rc = BAM_FileReadn(self, hlen, (uint8_t *)htxt); if (rc) goto BAILOUT;
    htxt[hlen] = '\0';

    rc = BAM_FileReadI32(self, &i32); if (rc) goto BAILOUT;
    if (i32 < 0) {
        rc = RC(rcAlign, rcFile, rcReading, rcHeader, rcInvalid);
        goto BAILOUT;
    }
    nrefs = i32;
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM Header reference count: %u\n", nrefs));
    if (nrefs) {
        rdms = nrefs * 16;
        if (rdms < 4096)
            rdms = 4096;
        rdat = malloc(rdms);
        if (rdat == NULL) {
            rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
            goto BAILOUT;
        }
        for (i = rdsz = 0; i < nrefs; ++i) {
            rc = BAM_FileReadI32(self, &i32); if (rc) goto BAILOUT;
            if (i32 <= 0) {
                rc = RC(rcAlign, rcFile, rcReading, rcHeader, rcInvalid);
                goto BAILOUT;
            }
            if (rdsz + i32 + 8 > rdms) {
                void *tmp;
                
                do { rdms <<= 1; } while (rdsz + i32 + 8 > rdms);
                tmp = realloc(rdat, rdms);
                if (tmp == NULL) {
                    rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
                    goto BAILOUT;
                }
                rdat = tmp;
            }
            memmove(rdat + rdsz, &i32, 4);
            rdsz += 4;
            rc = BAM_FileReadn(self, i32, (uint8_t *)&rdat[rdsz]); if (rc) goto BAILOUT;
            rdsz += i32;
            rc = BAM_FileReadI32(self, &i32); if (rc) goto BAILOUT;
            memmove(rdat + rdsz, &i32, 4);
            rdsz += 4;
        }
    }
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM Header reference size: %u\n", rdsz));
    
    *headerText = htxt;
    *headerTextLen = hlen;
    *refData = rdat;
    *numrefs = nrefs;
    return 0;
    
BAILOUT:
    if (htxt)
        free(htxt);
    if (rdat)
        free(rdat);
    
    return rc;
}

static unsigned FindRefSeqByName(char const name[], bool match, unsigned const N, BAMRefSeq const refSeq[])
{
    unsigned f = 0;
    unsigned e = N;
        
    while (f < e) {
        unsigned const m = f + ((e - f) >> 1);
        int const cmp = strcmp(name, refSeq[m].name);
            
        if (cmp < 0)
            e = m;
        else if (cmp > 0)
            f = m + 1;
        else
            return m;
    }
    return match ? N : f;
}

static void FindAndSetupRefSeq(BAMRefSeq *rs, unsigned const refSeqs, BAMRefSeq const refSeq[])
{
    unsigned const fnd = FindRefSeqByName(rs->name, true, refSeqs, refSeq);
    if (fnd != refSeqs) {
        rs->assemblyId = refSeq[fnd].assemblyId;
        rs->uri = refSeq[fnd].uri;
        rs->species = refSeq[fnd].species;
        if (refSeq[fnd].checksum) {
            rs->checksum = &rs->checksum_array[0];
            memmove(rs->checksum_array, refSeq[fnd].checksum_array, 16);
        }
        else
            rs->checksum = NULL;
    }
}

static rc_t ProcessBAMHeader(BAM_File *self, char const headerText[])
{
    unsigned i;
    unsigned cp;
    char *htxt;
    char *rdat;
    size_t hlen;
    unsigned nrefs;
    BAMRefSeq *refSeq;
    rc_t rc = ReadMagic(self);

    if (rc) return rc;

    rc = ReadHeaders(self, &htxt, &hlen, &rdat, &nrefs);
    if (rc) return rc;
    
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM Data records start at: %lu+%u\n", self->fpos_cur, self->bufCurrent));

    if (nrefs) {
        refSeq = calloc(nrefs, sizeof(self->refSeq[0]));
        if (refSeq == NULL)
            return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    }

    if (headerText) {
        free(htxt);
        rc = ProcessHeaderText(self, headerText, true);
    }
    else
        rc = ProcessHeaderText(self, htxt, false);

    if (rc) return rc;
        
    for (i = cp = 0; i < nrefs; ++i) {
        unsigned const nlen = LE2HUI32(rdat + cp);
        char *const name = rdat + cp + 4;
        unsigned const rlen = LE2HUI32(rdat + cp + nlen + 4);
        
        cp += nlen + 8;
        name[nlen] = '\0';

        refSeq[i].id = i;
        refSeq[i].name = name;
        refSeq[i].length = rlen;
        FindAndSetupRefSeq(&refSeq[i], self->refSeqs, self->refSeq);
    }
    free(self->refSeq);
    self->refSeq = refSeq;
    self->refSeqs = nrefs;
    self->headerData2 = rdat; /* so it's not leaked */
    
    return 0;
}

static rc_t ProcessSAMHeader(BAM_File *self, char const substitute[])
{
    SAMFile *const file = &self->file.sam;
    size_t headerSize = 0;
    char *headerText = NULL;
    rc_t rc;
    int st = 0;
    
    for ( ; ; ) {
        void *const tmp = headerText;
        int const ch = SAMFileRead1(file);
        
        if (ch == -1)
            return SAMFileLastError(file);
        
        if (st == 0) {
            if (ch != '@') {
                SAMFilePutBack(file, ch);
                break;
            }
            st = 1;
        }
        headerText = realloc(headerText, headerSize + 2);
        if (headerText == NULL) {
            free(tmp);
            return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
        }
        headerText[headerSize++] = ch;
        headerText[headerSize] = '\0';
        if (ch == '\n')
            st = 0;
    }

    if (substitute)
        rc = ProcessHeaderText(self, substitute, true);
    else if (headerText)
        rc = ProcessHeaderText(self, headerText, false);
    else {
        rc = RC(rcAlign, rcFile, rcConstructing, rcHeader, rcNotFound);
        (void)LOGERR(klogErr, rc, "SAM header required");
    }
    if (rc == 0) {
        unsigned i;

        for (i = 0; i < self->refSeqs; ++i)
            self->refSeq[i].id = i;
    }
    return rc;
}

/* MARK: BAM File destructor */

static void BAM_FileWhack(BAM_File *self) {
    if (self->refSeqs > 0 && self->refSeq)
        free(self->refSeq);
    if (self->readGroup)
        free(self->readGroup);
    if (self->header)
        free((void *)self->header);
    if (self->headerData1)
        free((void *)self->headerData1);
    if (self->headerData2)
        free((void *)self->headerData2);
    if (self->nocopy)
        free(self->nocopy);
    if (self->vt.FileWhack)
        self->vt.FileWhack(&self->file);
    KFileRelease(self->defer);
    BufferedFileWhack(&self->file.bam.file);
}

/* MARK: BAM File constructors */

/* file is retained */
static rc_t BAM_FileMakeWithKFileAndHeader(BAM_File **cself,
                                           KFile const *file,
                                           char const *headerText)
{
    BAM_File *self = calloc(1, sizeof(*self));
    rc_t rc;
    
    if (self == NULL)
        return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    
    rc = BufferedFileInit(&self->file.bam.file, file);
    if (rc) {
        free(self);
        return rc;
    }
    
    rc = BGZFileInit(&self->file.bam, &self->vt);
    if (rc == 0) {
        rc = ProcessBAMHeader(self, headerText);
        if (rc == 0) {
            *cself = self;
            return 0;
        }
    }
    BGZFileWhack(&self->file.bam);

    self->file.sam.file.bpos = 0;
    rc = SAMFileInit(&self->file.sam, &self->vt);
    if (rc == 0) {
        self->isSAM = true;
        rc = ProcessSAMHeader(self, headerText);
        if (rc == 0) {
            *cself = self;
            return 0;
        }
    }
    BufferedFileWhack(&self->file.sam.file);
    free(self);

    return rc;
}

rc_t BAM_FileMake(const BAM_File **cself,
                  KFile *defer,
                  char const headerText[],
                  char const path[], ... )
{
    KDirectory *dir;
    va_list args;
    rc_t rc;
    const KFile *kf;
    
    if (cself == NULL)
        return RC(rcAlign, rcFile, rcOpening, rcParam, rcNull);
    *cself = NULL;

    if (strcmp(path, "/dev/stdin") == 0) {
        rc = KFileMakeStdIn(&kf);
    }
    else {
        rc = KDirectoryNativeDir(&dir);
        if (rc) return rc;
        va_start(args, path);
        rc = KDirectoryVOpenFileRead(dir, &kf, path, args);
        va_end(args);
        KDirectoryRelease(dir);
    }
    if (rc == 0) {
        BAM_File *self = NULL;
        rc = BAM_FileMakeWithKFileAndHeader(&self, kf, headerText);
        if (rc == 0) {
            assert(self != NULL);
            KFileAddRef(defer);
            self->defer = defer;
        }
        *cself = self;
        KFileRelease(kf);
    }
    return rc;
}

/* MARK: BAM File ref-counting */

rc_t BAM_FileAddRef(const BAM_File *cself) {
    return 0;
}

rc_t BAM_FileRelease(const BAM_File *cself) {
    BAM_File *self = (BAM_File *)cself;
    
    if (cself != NULL) {
        BAM_FileWhack(self);
        free(self);
    }
    return 0;
}

/* MARK: BAM File positioning */

float BAM_FileGetProportionalPosition(const BAM_File *self)
{
    return self->vt.FileProPos(&self->file);
}

rc_t BAM_FileGetPosition(const BAM_File *self, BAM_FilePosition *pos) {
    *pos = (self->fpos_cur << 16) | self->bufCurrent;
    return 0;
}

static void BAM_FileAdvance(BAM_File *const self, unsigned distance)
{
    self->bufCurrent += distance;
    if (self->bufCurrent == self->bufSize) {
        self->fpos_cur = self->vt.FileGetPos(&self->file);
        self->bufCurrent = 0;
        self->bufSize = 0;
    }
}

/* MARK: BAM Alignment contruction */

static int TagTypeSize(int const type)
{
    switch (type) {
        case dt_ASCII:      /* A */
        case dt_INT8:       /* c */
        case dt_UINT8:      /* C */
            return 1;

        case dt_INT16:      /* s */
        case dt_UINT16:     /* S */
            return 2;

        case dt_INT:        /* i */
        case dt_UINT:       /* I */
        case dt_FLOAT32:    /* f */
            return 4;
#if 0
        case dt_FLOAT64:    /* d */
            return 8;
#endif
        case dt_CSTRING:    /* Z */
        case dt_HEXSTRING:  /* H */
            return -'S';

        case dt_NUM_ARRAY:  /* B */
            return -'A';
    }
    return 0;
}

static void ColorCheck(BAM_Alignment *const self, char const tag[2], unsigned const len)
{
    if (tag[0] == 'C' && len != 0) {
        int const ch = tag[1];
        int const flag = ch == 'Q' ? 2 : ch == 'S' ? 1 : 0;
        
        if (flag)
            self->hasColor ^= (len << 2) | flag;
    }
}

static rc_t ParseOptData(BAM_Alignment *const self, size_t const maxsize,
                         size_t const xtra, size_t const datasize)
{
    size_t const maxExtra = (maxsize - (sizeof(*self) - sizeof(self->extra))) / sizeof(self->extra[0]);
    char const *const base = (char const *)self->data->raw;
    unsigned i = 0;
    unsigned len;
    unsigned offset;
    
    self->numExtra = 0;
    for (len = 0, offset = (unsigned)xtra; offset < datasize; offset += len) {
        int const valuelen1 = TagTypeSize(base[offset + 2]);
        unsigned valuelen;
        
        if (valuelen1 < 0) {
            char const *const value = &base[offset + 3];
            
            if (-valuelen1 == 'S') {
                valuelen = 0;
                while (value[valuelen] != '\0') {
                    ++valuelen;
                    if (offset + valuelen >= datasize) {
                        rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
                        return rc;
                    }
                }
                ColorCheck(self, base + offset, valuelen);
                ++valuelen;
            }
            else {
                int const elem_size = TagTypeSize(value[0]);
                
                assert(-valuelen1 == 'A');
                if (elem_size <= 0) {
                    rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcUnexpected);
                    return rc;
                }
                else {
                    int const elem_count = LE2HI32(&value[1]);
                    
                    valuelen = elem_size * elem_count + 1 + 4;
                    if (offset + valuelen >= datasize) {
                        rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
                        return rc;
                    }
                }
            }
        }
        else if (valuelen1 == 0) {
            rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcUnexpected);
            return rc;
        }
        else
            valuelen = valuelen1;
        
        len = valuelen + 3;
        if (i < maxExtra) {
            self->extra[i].offset = offset;
            self->extra[i].size   = len;
        }
        ++i;
    }
    self->numExtra = i;
    if (2 <= i && i <= maxExtra)
        ksort(self->extra, i, sizeof(self->extra[0]), OptTag_sort, self);

    return 0;
}

static rc_t ParseOptDataLog(BAM_Alignment *const self, unsigned const maxsize,
                            unsigned const xtra, unsigned const datasize)
{
    unsigned const maxExtra = (maxsize - (sizeof(*self) - sizeof(self->extra))) / sizeof(self->extra[0]);
    char const *const base = (char const *)self->data->raw;
    unsigned i = 0;
    unsigned len;
    unsigned offset;
    
    self->numExtra = 0;
    for (len = 0, offset = (unsigned)xtra; offset < datasize; offset += len) {
        int const type = base[offset + 2];
        int const valuelen1 = TagTypeSize(type);
        unsigned valuelen;
        
        if (valuelen1 > 0)
            valuelen = valuelen1;
        else if (valuelen1 < 0) {
            char const *const value = &base[offset + 3];
            
            if (-valuelen1 == 'S') {
                valuelen = 0;
                while (value[valuelen] != '\0') {
                    ++valuelen;
                    if (offset + valuelen >= datasize) {
                        rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
                        (void)LOGERR(klogErr, rc,
                                     "Parsing BAM optional fields: "
                                     "unterminated string");
                        return rc;
                    }
                }
                ColorCheck(self, base + offset, valuelen);
                ++valuelen;
            }
            else {
                int const elem_type = value[0];
                int const elem_size = TagTypeSize(elem_type);
                
                assert(-valuelen1 == 'A');
                if (elem_size <= 0) {
                    rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcUnexpected);
                    (void)LOGERR(klogErr, rc,
                                 "Parsing BAM optional fields: "
                                 "unknown array type");
                    return rc;
                }
                else {
                    int const elem_count = LE2HI32(&value[1]);
                    
                    valuelen = elem_size * elem_count + 1 + 4;
                    if (offset + valuelen >= datasize) {
                        rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
                        (void)LOGERR(klogErr, rc,
                                     "Parsing BAM optional fields: "
                                     "array too big");
                        return rc;
                    }
                }
            }
        }
        else {
            rc_t const rc = RC(rcAlign, rcFile, rcReading, rcData, rcUnexpected);
            (void)LOGERR(klogErr, rc,
                                    "Parsing BAM optional fields: "
                                    "unknown type");
            return rc;
        }
        
        len = valuelen + 3;
        if (i < maxExtra) {
            self->extra[i].offset = offset;
            self->extra[i].size   = len;
        }
        ++i;
    }
    self->numExtra = i;
    if (2 <= i && i <= maxExtra)
        ksort(self->extra, i, sizeof(self->extra[0]), OptTag_sort, self);
    
    return 0;
}

static unsigned BAM_AlignmentSize(unsigned const max_extra_tags)
{
    BAM_Alignment const *const y = NULL;
    
    return sizeof(*y) + (max_extra_tags ? max_extra_tags - 1 : 0) * sizeof(y->extra);
}

static unsigned BAM_AlignmentSetOffsets(BAM_Alignment *const self)
{
    unsigned const nameLen = getReadNameLength(self);
    unsigned const cigCnt  = getCigarCount(self);
    unsigned const readLen = getReadLen(self);
    unsigned const cigar   = (unsigned)(&((struct bam_alignment_s const *)NULL)->read_name[nameLen] - (const char *)NULL);
    unsigned const seq     = cigar + 4 * cigCnt;
    unsigned const qual    = seq + (readLen + 1) / 2;
    unsigned const xtra    = qual + readLen;
    
    self->cigar = cigar;
    self->seq   = seq;
    self->qual  = qual;
    
    return xtra;
}

static bool BAM_AlignmentInit(BAM_Alignment *const self, unsigned const maxsize,
                             unsigned const datasize, void const *const data)
{
    memset(self, 0, sizeof(*self));
    self->data = data;
    self->datasize = datasize;
    {
        unsigned const xtra = BAM_AlignmentSetOffsets(self);
        
        if (   datasize >= xtra
            && datasize >= self->cigar
            && datasize >= self->seq
            && datasize >= self->qual)
        {
            rc_t const rc = ParseOptData(self, maxsize, xtra, datasize);

            if (rc == 0)
                return true;
        }
        return false;
    }
}

static void BAM_AlignmentDebugPrint(BAM_Alignment const *const self)
{
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("{"
                                                    "\"BAM record\": "
                                                    "{ "
                                                        "\"size\": %u, "
                                                        "\"name length\": %u, "
                                                        "\"cigar count\": %u, "
                                                        "\"read length\": %u, "
                                                        "\"extra count\": %u "
                                                    "}"
                                                "}\n",
                                                (unsigned)self->datasize,
                                                (unsigned)getReadNameLength(self),
                                                (unsigned)getCigarCount(self),
                                                (unsigned)getReadLen(self),
                                                (unsigned)self->numExtra));
}

static bool BAM_AlignmentInitLog(BAM_Alignment *const self, unsigned const maxsize,
                                unsigned const datasize, void const *const data)
{
    memset(self, 0, sizeof(*self));
    self->data = data;
    self->datasize = datasize;
    {
        unsigned const xtra = BAM_AlignmentSetOffsets(self);
        
        if (   datasize >= xtra
            && datasize >= self->cigar
            && datasize >= self->seq
            && datasize >= self->qual)
        {
            rc_t const rc = ParseOptDataLog(self, maxsize, xtra, datasize);
            
            if (rc == 0) {
                BAM_AlignmentDebugPrint(self);
                return true;
            }
        }
        return false;
    }
}

static void BAM_AlignmentLogParseError(BAM_Alignment const *self)
{
    char const *const reason = self->cigar > self->datasize ? "BAM Record CIGAR too long"
                             : self->seq   > self->datasize ? "BAM Record SEQ too long"
                             : self->qual  > self->datasize ? "BAM Record QUAL too long"
                             : self->qual + getReadLen(self) > self->datasize ? "BAM Record EXTRA too long"
                             : "BAM Record EXTRA parsing failure";
    
    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), reason);
}

/* MARK: BAM Alignment readers */

/* returns
 *  (rcAlign, rcFile, rcReading, rcBuffer, rcNotAvailable)
 * or
 *  (rcAlign, rcFile, rcReading, rcBuffer, rcInsufficient)
 * if should read with copy
 */
static
rc_t BAM_FileReadNoCopy(BAM_File *const self, unsigned actsize[], BAM_Alignment rhs[],
                       unsigned const maxsize)
{
    unsigned const maxPeek = BAM_FileMaxPeek(self);
    bool isgood;

    *actsize = 0;
    
    if (maxPeek == 0) {
        rc_t const rc = BAM_FileFillBuffer(self);

        if (rc == 0)
            return BAM_FileReadNoCopy(self, actsize, rhs, maxsize);

        if ((int)GetRCObject(rc) == rcData && GetRCState(rc) == rcInsufficient)
        {
            self->eof = true;
            return SILENT_RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound);
        }
        return rc;
    }
    if (maxPeek < 4)
        return SILENT_RC(rcAlign, rcFile, rcReading, rcBuffer, rcNotAvailable);
    else {
        int32_t const i32 = BAM_FilePeekI32(self);

        if (i32 <= 0)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        
        if (maxPeek < i32 + 4)
        return SILENT_RC(rcAlign, rcFile, rcReading, rcBuffer, rcNotAvailable);

        isgood = BAM_AlignmentInitLog(rhs, maxsize, i32, BAM_FilePeek(self, 4));
        rhs[0].parent = self;
    }
    *actsize = BAM_AlignmentSize(rhs[0].numExtra);
    if (isgood && *actsize > maxsize)
        return SILENT_RC(rcAlign, rcFile, rcReading, rcBuffer, rcInsufficient);

    BAM_FileAdvance(self, 4 + rhs->datasize);
    return isgood ? 0 : RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid);
}

static
unsigned BAM_AlignmentSizeFromData(unsigned const datasize, void const *data)
{
    BAM_Alignment temp;
    
    BAM_AlignmentInit(&temp, sizeof(temp), datasize, data);
    
    return BAM_AlignmentSize(temp.numExtra);
}

static bool BAM_AlignmentIsEmpty(BAM_Alignment const *const self)
{
    if (getReadNameLength(self) == 0)
        return true;
    if (getReadName(self)[0] == '\0')
        return true;
    if (self->hasColor == 3)
        return false;
    if (getReadLen(self) != 0)
        return false;
    if (getCigarCount(self) != 0)
        return false;
    return true;
}

static
rc_t BAM_FileReadCopy(BAM_File *const self, BAM_Alignment const *rslt[], bool const log)
{
    void *storage;
    void const *data;
    unsigned datasize;
    rc_t rc;
    
    rslt[0] = NULL;
    {
        int32_t i32;

        rc = BAM_FileReadI32(self, &i32);
        if ( rc != 0 )
        {
            if ((int)GetRCObject(rc) == rcData && GetRCState(rc) == rcInsufficient)
            {
                self->eof = true;
                rc = SILENT_RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound);
            }
            return rc;
        }
        if (i32 <= 0)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        
        datasize = i32;
    }
    if (BAM_FileMaxPeek(self) < datasize) {
        data = storage = malloc(datasize);
        if (storage == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
        
        rc = BAM_FileReadn(self, datasize, storage);
    }
    else {
        storage = NULL;
        data = (bam_alignment *)&self->buffer[self->bufCurrent];
        
        BAM_FileAdvance(self, datasize);
    }
    if (rc == 0) {
        unsigned const rsltsize = BAM_AlignmentSizeFromData(datasize, data);
        BAM_Alignment *const y = malloc(rsltsize);

        if (y) {
            if ((log ? BAM_AlignmentInitLog : BAM_AlignmentInit)(y, rsltsize, datasize, data)) {
                if (storage != NULL)
                    y->storage = storage;

                y->parent = self;
                rslt[0] = y;

                if (BAM_AlignmentIsEmpty(y))
                    return RC(rcAlign, rcFile, rcReading, rcRow, rcEmpty);
                return 0;
            }
            rc = RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid);
            free(y);
        }
        else
            rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
    }
    free(storage);

    return rc;
}

/* MARK: SAM code */

static void SAM2BAM_ConvertShort(void *const Dst, int value)
{
    uint8_t *const dst = Dst;
    dst[0] = (uint8_t)(value >> 0);
    dst[1] = (uint8_t)(value >> 8);
}

static void SAM2BAM_ConvertInt(void *const Dst, int value)
{
    uint8_t *const dst = Dst;
    dst[0] = (uint8_t)(value >>  0);
    dst[1] = (uint8_t)(value >>  8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static int SAM2BAM_CIGAR_OpCount(char const cigar[])
{
    unsigned i;
    unsigned n = 0;
    int st = 0;
    
    if (cigar[0] == '*' && cigar[1] == '\0')
        return 0;
    
    for (i = 0; ; ++i) {
        int const ch = cigar[i];
        
        if (ch == '\0')
            break;

        if (st == 0) {
            if (!isdigit(ch))
                return -1;
            st = 1;
        }
        else if (!isdigit(ch)) {
            ++n;
            st = 0;
        }
    }
    return st == 0 ? n : -1;
}

static int SAM2BAM_ConvertCIGAR1(uint8_t dst[4], char const value[])
{
    int len = 0;
    int code = 0;
    unsigned i = 0;
    
    if (value[0] == '\0')
        return 0;
    
    for ( ; ; ) {
        int const ch = value[i++];
        
        if (!isdigit(ch)) {
            switch (ch) {
            case 'M':
                code = 0;
                break;
            case 'I':
                code = 1;
                break;
            case 'D':
                code = 2;
                break;
            case 'N':
                code = 3;
                break;
            case 'S':
                code = 4;
                break;
            case 'H':
                code = 5;
                break;
            case 'P':
                code = 6;
                break;
            case '=':
                code = 7;
                break;
            case 'X':
                code = 8;
                break;
            default:
                return -1;
            }
            break;
        }
        len = (len * 10) + (ch - '0');
        if (len >= (1 << 24))
            return -2;
    }
    SAM2BAM_ConvertInt(dst, (len << 4) | code);
    return i;
}

static rc_t SAM2BAM_ConvertCIGAR(unsigned const insize, void /* inout */ *const data, void const *const endp)
{
    char *const value = data;
    uint8_t *const dst = (void *)(value + insize);
    unsigned j = 0;
    unsigned i;

    for (i = 0; i < insize; ++j) {
        if ((void const *)(dst + j * 4 + 4) >= endp)
            return RC(rcAlign, rcFile, rcReading, rcBuffer, rcInsufficient);
        {
        int const k = SAM2BAM_ConvertCIGAR1(dst + j * 4, value + i);
        if (k > 0)
            i += k;
        else
            return k == 0 ? 0 : RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        }
    }
    memmove(data, dst, 4 * j);
    return 0;
}

static int SAM2BAM_ConvertBase(int base)
{
    static char const tr[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,0x0, -1, -1,
        -1,0x1,0xE,0x2,0xD, -1, -1,0x4,0xB, -1, -1,0xC, -1,0x3,0xF, -1,
        -1, -1,0x5,0x6,0x8, -1,0x7,0x9, -1,0xA, -1, -1, -1, -1, -1, -1,
        -1,0x1,0xE,0x2,0xD, -1, -1,0x4,0xB, -1, -1,0xC, -1,0x3,0xF, -1,
        -1, -1,0x5,0x6,0x8, -1,0x7,0x9, -1,0xA, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    return tr[base];
}

static rc_t SAM2BAM_ConvertSEQ(unsigned const insize, void /* inout */ *const data, void const *const endp)
{
    char const *const value = data;
    uint8_t *const dst = data;
    unsigned const n = insize & ~((unsigned)1);
    unsigned j = 0;
    unsigned i;

    for (i = 0; i < n; i += 2, ++j) {
        int const hi = SAM2BAM_ConvertBase(value[i + 0]);
        int const lo = SAM2BAM_ConvertBase(value[i + 1]);
        
        if (hi < 0 || lo < 0)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);

        dst[j] = (hi << 4) | lo;
    }
    if (n != insize) {
        int const hi = SAM2BAM_ConvertBase(value[n]);
        int const lo = 0;
        
        if (hi < 0 || lo < 0)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        
        dst[j] = (hi << 4) | lo;
    }
    return 0;
}

static rc_t SAM2BAM_ConvertQUAL(unsigned const insize, void /* inout */ *const data, void const *const endp)
{
    char const *const value = data;
    uint8_t *const dst = data;
    unsigned i;

    for (i = 0; i < insize; ++i) {
        int const ch = value[i];
        
        if (ch < '!' || ch > '~')
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        dst[i] = ch - 33;
    }
    return 0;
}

static int SAM2BAM_ScanValue(void *const dst, char const *src, bool isFloat, bool isArray)
{
    int i = 0;
    int sgn = 0;
    uint64_t mantissa = 0;
    int shift = 0;
    int exponent = 0;
    int st = 0;

    if (isArray) {
        int const ch = src[i++];
        if (ch != ',')
            return -1;
    }
    for ( ; ; ++i) {
        int const ch = src[i];
        int const value = ch - '0';

        if (ch == '\0')
            break;
        if (isArray && ch == ',')
            break;
        switch (st) {
        case 0:
            ++st;
            if (ch == '-') {
                sgn = -1;
                break;
            }
            else if (ch == '+') {
                sgn = 1;
                break;
            }
            /* fallthrough; */
        case 1:
            if (ch == '.') {
                st = 2;
                break;
            }
            if (ch == 'e' || ch == 'E') {
                st = 3;
                break;
            }
            if (value < 0 || value > 9)
                return -1;
            mantissa = mantissa * 10 + value;
            break;
        case 2:
            if (ch == 'e' || ch == 'E') {
                st = 3;
                break;
            }
            if (value < 0 || value > 9)
                return -1;
            mantissa = mantissa * 10 + value;
            ++shift;
            break;
        case 3:
            ++st;
            if (ch == '-') {
                ++st;
                break;
            }
            else if (ch == '+') {
                break;
            }
            /* fallthrough; */
        case 4:
            if (value < 0 || value > 9)
                return -1;
            exponent = exponent * 10 + value;
            break;
        case 5:
            if (value < 0 || value > 9)
                return -1;
            exponent = exponent * 10 - value;
            break;
        }
    }
    {
        double const value = mantissa * pow(10, exponent - shift) * (sgn ? sgn : 1);
        union { int i; float f; } x;
        if (isFloat)
            x.f = value;
        else
            x.i = floor(value);
        SAM2BAM_ConvertInt(dst, x.i);
    }
    return i;
}

static int SAM2BAM_ConvertEXTRA(unsigned const insize, void /* inout */ *const data, void const *const endp)
{
    if (insize < 5) /* XX:T:\0 */
        return -1;
    {
        char const *const src = data;

        if (src[2] != ':' || src[4] != ':')
            return -3;
        {
            int const type = src[3];
            char *const dst = data;

            dst[2] = type;

            switch (type) {
            case 'A':
                dst[3] = src[5];
                return 4;
            case 'H':
            case 'Z':
                memmove(dst + 3, src + 5, insize - 5);
                dst[insize - 2] = '\0';
                return insize - 1;
            case 'i':
            case 'f': {
                if ((void const *)&dst[7] >= endp)
                    return -2;
                {
                    int const n = SAM2BAM_ScanValue(&dst[3], src + 5, type == 'f', false);
                    return (n < 0 || n + 5 != insize) ? -4 : 7;
                }
            }
            case 'B':
                break;
            default:
                return -3;
            }

            if (insize < 8) /* XX:B:T,x\0 */
                return -1;

            switch (src[5]) {
            case 'c':
            case 'C':
            case 's':
            case 'S':
            case 'i':
            case 'I':
            case 'f':
                break;
            default:
                return -3;
            }
            {
                uint8_t *const scratch = (void *)(src + insize);
                int const subtype = src[5] == 'f' ? 'f' : 'i';
                unsigned i;
                unsigned j;

                dst[3] = subtype;
                for (i = 6, j = 0; i < insize; ++j) {
                    if ((void const *)(scratch + 4 * j + 4) >= endp)
                        return -2;
                    {
                        int const n = SAM2BAM_ScanValue(scratch + 4 * j, src + i, subtype == 'f', true);
                        if (n < 0)
                            return -4;
                        i += n;
                    }
                }
                SAM2BAM_ConvertInt(&dst[4], j);
                memmove(&dst[8], scratch, 4 * j);
                return 8 + 4 * j;
            }
        }
    }
}

static rc_t BAM_FileReadSAM(BAM_File *const self, BAM_Alignment const **const rslt)
{
    void const *const endp = self->buffer + sizeof(self->buffer);
    struct bam_alignment_s *raw = (void *)self->buffer;
    struct {
        int namelen;
        int FLAG;
        int RNAME;
        int POS;
        int MAPQ;
        int cigars;
        int RNEXT;
        int PNEXT;
        int TLEN;
        
        int readlen;
        
        char *QNAME;
        uint32_t *CIGAR; /* probably not aligned */
        uint8_t *SEQ;
        uint8_t *QUAL;
        char *EXTRA;
    } temp;
    unsigned field = 1;
    char *scratch = &raw->read_name[0];
    int *intScratch = NULL;
    int sgn = 1;
    unsigned i = 0;

    memset(raw, 0, sizeof(*raw));
    memset(&temp, 0, sizeof(temp));
    temp.QNAME = scratch;
    
    for ( ; ; ) {
        int const ch = SAMFileRead1(&self->file.sam);
        if (ch < 0) {
            rc_t const rc = SAMFileLastError(&self->file.sam);
            return (i == 0 && field == 1 && (rc == 0 || GetRCState(rc) == rcInsufficient)) ? SILENT_RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound) : rc;
        }
        if ((void const *)&scratch[i] >= endp)
            return RC(rcAlign, rcFile, rcReading, rcBuffer, rcInsufficient);

        if (ch == '\n' && i > 0 && scratch[i - 1] == '\r') {
            /* handle \r\n line endings */
            --i;
        }
        if (!(ch == '\t' || ch == '\n')) {
            if (field != 0) {
                if (intScratch == NULL) {
                    scratch[i] = ch;
                }
                else {
                    int const value = ch - '0';
                    if (ch == '-' && i == 0) {
                        sgn = -1;
                    }
                    else if (ch == '-' && i == 0) {
                        sgn = 1;
                    }
                    else {
                        if (value < 0 || value > 9) {
                            LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing integer field");
                            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
                        }
                        *intScratch = *intScratch * 10 + value * sgn;
                    }
                }
            }
            ++i;
            continue;
        }
        if (intScratch == NULL)
            scratch[i] = '\0';
        else {
            intScratch = NULL;
            sgn = 1;
        }
        switch (field) {
            case 0:
                if (ch == '\n')
                    return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
                break;
            case 1:
                if (i == 1 && scratch[0] == '*') {
                    temp.namelen = 0;
                }
                else {
                    temp.namelen = i + 1; /* includes NULL terminator */
                    scratch += i + 1;     /* don't want to overwrite it */
                    if (temp.namelen > 255) {
                        LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error QNAME is too long");
                        field = 0;
                    }
                }
                temp.CIGAR = (void *)scratch;
                intScratch = &temp.FLAG;
                break;
            case 2:
                break;
            case 3:
                if (i == 1 && scratch[0] == '*')
                    temp.RNAME = -1;
                else {
                    unsigned const id = FindRefSeqByName(scratch, true, self->refSeqs, self->refSeq);
                    if (id < self->refSeqs)
                        temp.RNAME = id;
                    else {
                        LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record missing reference");
                        field = 0;
                    }
                }
                intScratch = &temp.POS;
                break;
            case 4:
                intScratch = &temp.MAPQ;
                break;
            case 5:
                if (temp.MAPQ > 255) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error MAPQ > 255");
                    field = 0;
                }
                break;
            case 6:
                temp.cigars = SAM2BAM_CIGAR_OpCount(scratch);
                if (temp.cigars < 0) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing CIGAR");
                    field = 0;
                }
                else if (temp.cigars > 0) {
                    scratch += 4 * temp.cigars;
                    temp.SEQ = (uint8_t *)scratch;
                    {
                        rc_t const rc = SAM2BAM_ConvertCIGAR(i, temp.CIGAR, endp);
                        if (rc) {
                            LOGERR(klogErr, rc, "SAM Record error parsing CIGAR");
                            field = 0;
                        }
                    }
                }
                else
                    temp.SEQ = (uint8_t *)scratch;
                break;
            case 7:
                if (i == 1 && scratch[0] == '*')
                    temp.RNEXT = -1;
                else if (i == 1 && scratch[0] == '=')
                    temp.RNEXT = temp.RNAME;
                else {
                    unsigned const id = FindRefSeqByName(scratch, true, self->refSeqs, self->refSeq);
                    if (id < self->refSeqs)
                        temp.RNEXT = id;
                    else {
                        LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record missing reference");
                        field = 0;
                    }
                }
                intScratch = &temp.PNEXT;
                break;
            case 8:
                intScratch = &temp.TLEN;
                break;
            case 9:
                break;
            case 10:
                if (i == 1 && scratch[0] == '*')
                    temp.readlen = 0;
                else {
                    temp.readlen = i;
                    scratch += (temp.readlen + 1) / 2;
                    temp.QUAL = (uint8_t *)scratch;
                    {
                        rc_t const rc = SAM2BAM_ConvertSEQ(i, temp.SEQ, endp);
                        if (rc) {
                            LOGERR(klogErr, rc, "SAM Record error converting SEQ");
                            field = 0;
                        }
                    }
                }
                break;
            case 11:
                if (temp.readlen == 0)
                    break;
                if (i == 1 && scratch[0] == '*')
                    memset(temp.QUAL, 0xFF, temp.readlen);
                else if (i == temp.readlen) {
                    rc_t const rc = SAM2BAM_ConvertQUAL(i, temp.QUAL, endp);
                    if (rc) {
                        LOGERR(klogErr, rc, "SAM Record error converting QUAL");
                        field = 0;
                    }
                }
                else {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error length of SEQ != length of QUAL");
                    field = 0;
                }
                scratch += temp.readlen;
                break;
            default:
            {
                int const n = SAM2BAM_ConvertEXTRA(i, scratch, endp);
                if (n < 0) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing optional field");
                    field = 0;
                }
                else {
                    if (n == 0)
                        --field;
                    else
                        scratch += n;
                }
                break;
            }
        }
        if (ch == '\n') {
            if (field < 11) {
                rc_t const rc = RC(rcAlign, rcFile, rcReading, rcRow, rcTooShort);
                LOGERR(klogErr, rc, "SAM Record error too few fields");
                return rc;
            }
            break;
        }
        i = 0;
        if (field > 0)
            ++field;
    }

    SAM2BAM_ConvertInt(raw->rID, temp.RNAME);
    SAM2BAM_ConvertInt(raw->pos, temp.POS - 1);
    raw->read_name_len = temp.namelen;
    raw->mapQual = temp.MAPQ;
    SAM2BAM_ConvertShort(raw->n_cigars, temp.cigars);
    SAM2BAM_ConvertShort(raw->flags, temp.FLAG);
    SAM2BAM_ConvertInt(raw->read_len, temp.readlen);
    SAM2BAM_ConvertInt(raw->mate_rID, temp.RNEXT);
    SAM2BAM_ConvertInt(raw->mate_pos, temp.PNEXT - 1);
    SAM2BAM_ConvertInt(raw->ins_size, temp.TLEN);
    {    
        unsigned const datasize = (char *)scratch - (char *)self->buffer;
        unsigned const rsltsize = BAM_AlignmentSizeFromData(datasize, self->buffer);
        BAM_Alignment *const y = malloc(rsltsize);
    
        if (y == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);

        if (BAM_AlignmentInitLog(y, rsltsize, datasize, self->buffer)) {
            y->parent = self;
            rslt[0] = y;
        
            if (BAM_AlignmentIsEmpty(y))
                return RC(rcAlign, rcFile, rcReading, rcRow, rcEmpty);
            return 0;
        }
        free(y);
    }
    return RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid);
}

static rc_t read2(BAM_File *const self, BAM_Alignment const **const rhs)
{
    unsigned actsize = 0;
    rc_t rc;
    
    if (self->bufCurrent >= self->bufSize && self->eof)
        return SILENT_RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound);

    if (self->isSAM) {
        rc = BAM_FileReadSAM(self, rhs);
        if (rc != 0 && GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
            self->eof = true;
        return rc;
    }
    if (self->nocopy == NULL) {
        size_t const size = 64u * 1024u;
        void *const temp = malloc(size);

        if (temp == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);

        self->nocopy = temp;
    }

    rc = BAM_FileReadNoCopy(self, &actsize, self->nocopy, 64u * 1024u);
    if (rc == 0) {
        *rhs = self->nocopy;
        if (BAM_AlignmentIsEmpty(self->nocopy)) {
            rc = RC(rcAlign, rcFile, rcReading, rcRow, rcEmpty);
            LOGERR(klogWarn, rc, "BAM Record contains no alignment or sequence data");
        }
    }
    else if ((int)GetRCObject(rc) == rcBuffer && GetRCState(rc) == rcInsufficient)
    {
        return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    }
    else if ((int)GetRCObject(rc) == rcBuffer && GetRCState(rc) == rcNotAvailable)
    {
        rc = BAM_FileReadCopy(self, rhs, true);
    }
    else if ((int)GetRCObject(rc) == rcRow && GetRCState(rc) == rcInvalid) {
        BAM_AlignmentLogParseError(self->nocopy);
    }
    return rc;
}

static rc_t readDefer(BAM_File *const self, BAM_Alignment const **const rslt)
{
    uint32_t datasize = 0;
    size_t nread = 0;
    rc_t rc = 0;

    rc = KFileReadAll(self->defer, self->deferPos, &datasize, 4, &nread);
    if (rc) return rc;
    if (nread == 0) {
        KFileRelease(self->defer);
        self->defer = NULL;
        return SILENT_RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound);
    }
    assert(nread == 4);
    assert(datasize < 64u * 1024u);
    if (self->nocopy == NULL) {
        size_t const size = 64u * 1024u;
        void *const temp = malloc(size);

        if (temp == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);

        self->nocopy = temp;
    }

    rc = KFileReadAll(self->defer, self->deferPos + 4, self->buffer, datasize, &nread);
    if (rc) return rc;
    assert(nread == datasize);
    self->deferPos += 4 + datasize;
    
    BAM_AlignmentInitLog(self->nocopy, 64u * 1024u, datasize, self->buffer);
    self->nocopy->parent = self;
    *rslt = self->nocopy;
    if (BAM_AlignmentIsEmpty(self->nocopy)) {
        rc = RC(rcAlign, rcFile, rcReading, rcRow, rcEmpty);
        LOGERR(klogWarn, rc, "BAM Record contains no alignment or sequence data");
    }
    return rc;
}

static rc_t writeExactly(KFile *const f, uint64_t const pos, void const *const data, size_t const size) {
    char const *const p = (char const *)data;
    size_t written = 0;

    while (written < size) {
        size_t num_writ = 0;
        rc_t const rc = KFileWrite(f, pos + written, p + written, size - written, &num_writ);
        if (rc) return rc;
        written += num_writ;
    }
    return 0;
}

static rc_t writeDefer(BAM_File *const self, BAM_Alignment const *const algn)
{
    rc_t rc = 0;

    rc = writeExactly(self->defer, self->deferPos, &algn->datasize, 4);
    if (rc) return rc;
    rc = writeExactly(self->defer, self->deferPos + 4, algn->data, algn->datasize);
    if (rc) return rc;
    self->deferPos += 4 + algn->datasize;
    return 0;
}

rc_t BAM_FileRead2(const BAM_File *cself, const BAM_Alignment **rhs)
{
    BAM_File *const self = (BAM_File *)cself;
    
    if (self == NULL || rhs == NULL)
        return RC(rcAlign, rcFile, rcReading, rcParam, rcNull);
    
    *rhs = NULL;

    if (self->eof && self->defer != NULL) {
        return readDefer(self, rhs);
    }
    for ( ; ; ) {
        rc_t const rc = read2(self, rhs);
        if (rc != 0) {
            if (self->eof && self->defer != NULL) {
                self->deferPos = 0;
                return readDefer(self, rhs);
            }
            return rc;
        }
        if (self->defer && BAM_AlignmentShouldDefer(*rhs)) {
            rc_t const rc = writeDefer(self, *rhs);
            if (rc) return rc;
        }
        else
            break;
    }
    return 0;
}

rc_t BAM_FileRead(const BAM_File *cself, const BAM_Alignment **rhs)
{
    assert(!"deprecated");
    abort();
}

/* MARK: BAM File header info accessor */

rc_t BAM_FileGetRefSeqById(const BAM_File *cself, int32_t id, const BAMRefSeq **rhs)
{
    *rhs = NULL;
    if (id >= 0 && id < cself->refSeqs)
        *rhs = &cself->refSeq[id];
    return 0;
}

rc_t BAM_FileGetReadGroupByName(const BAM_File *cself, const char *name, const BAMReadGroup **rhs)
{
    BAMReadGroup rg;
    
    *rhs = NULL;

    rg.name = name;
    if (rg.name != NULL)
        *rhs = kbsearch(&rg, cself->readGroup, cself->readGroups, sizeof(rg), comp_ReadGroup, NULL);

    return 0;
}

rc_t BAM_FileGetRefSeqCount(const BAM_File *cself, unsigned *rhs)
{
    *rhs = cself->refSeqs;
    return 0;
}

rc_t BAM_FileGetRefSeq(const BAM_File *cself, unsigned i, const BAMRefSeq **rhs)
{
    *rhs = NULL;
    if (i < cself->refSeqs)
        *rhs = &cself->refSeq[i];
    return 0;
}

rc_t BAM_FileGetReadGroupCount(const BAM_File *cself, unsigned *rhs)
{
    *rhs = cself->readGroups;
    return 0;
}

rc_t BAM_FileGetReadGroup(const BAM_File *cself, unsigned i, const BAMReadGroup **rhs)
{
    *rhs = NULL;
    if (i < cself->readGroups)
        *rhs = &cself->readGroup[i];
    return 0;
}

rc_t BAM_FileGetHeaderText(BAM_File const *cself, char const **header, size_t *header_len)
{
    *header = cself->header;
    *header_len = *header ? string_size( *header ) : 0;
    return 0;
}

/* MARK: BAM Alignment destructor */

static rc_t BAM_AlignmentWhack(BAM_Alignment *self)
{
    if (self != self->parent->nocopy) {
        free(self->storage);
        free(self);
    }
    return 0;
}

/* MARK: BAM Alignment ref-counting */

rc_t BAM_AlignmentAddRef(const BAM_Alignment *cself)
{
    return 0;
}

rc_t BAM_AlignmentRelease(const BAM_Alignment *cself)
{
    BAM_AlignmentWhack((BAM_Alignment *)cself);

    return 0;
}

rc_t BAM_AlignmentCopy(const BAM_Alignment *self, BAM_Alignment **rslt)
{
    unsigned const rsltsize = BAM_AlignmentSize(self->numExtra);
    unsigned const padded = (rsltsize + 15UL) & ~15UL;
    void *const tmp = malloc(padded + self->datasize);
    void *const tmp2 = &((char *)tmp)[padded];

    assert(tmp != NULL);
    if (tmp == NULL) {
        LOGMSG(klogFatal, "OUT OF MEMORY");
        abort();
    }
    memmove(tmp, self, rsltsize);
    memmove(tmp2, self->data, self->datasize);
    *rslt = tmp;
    (**rslt).data = tmp2;
    (**rslt).storage = NULL;

    return 0;
}

static bool BAM_AlignmentShouldDefer(BAM_Alignment const *const self)
{
    int const flags = getFlags(self);
    if (flags & BAMFlags_SelfIsUnmapped)
        return false;
    if (flags & BAMFlags_IsNotPrimary)
        return true;
    if (flags & BAMFlags_IsSupplemental)
        return true;
    return false;
}

#if 0
uint16_t BAM_AlignmentIffyFields(const BAM_Alignment *self)
{
}

uint16_t BAM_AlignmentBadFields(const BAM_Alignment *self)
{
}
#endif

/* MARK: BAM Alignment accessors */

static uint32_t BAM_AlignmentGetCigarElement(const BAM_Alignment *self, unsigned i)
{
    return LE2HUI32(&((uint8_t const *)getCigarBase(self))[i * 4]);
}

rc_t BAM_AlignmentGetRefSeqId(const BAM_Alignment *cself, int32_t *rhs)
{
    *rhs = getRefSeqId(cself);
    return 0;
}

rc_t BAM_AlignmentGetPosition(const BAM_Alignment *cself, int64_t *rhs)
{
    *rhs = getPosition(cself);
    return 0;
}

bool BAM_AlignmentIsMapped(const BAM_Alignment *cself)
{
    if (((getFlags(cself) & BAMFlags_SelfIsUnmapped) == 0) && getRefSeqId(cself) >= 0 && getPosition(cself) >= 0)
        return true;
    return false;
}

/* static bool BAM_AlignmentIsMateMapped(const BAM_Alignment *cself)
{
    if (((getFlags(cself) & BAMFlags_MateIsUnmapped) == 0) && getMateRefSeqId(cself) >= 0 && getMatePos(cself) >= 0)
        return true;
    return false;
} */

rc_t BAM_AlignmentGetAlignmentDetail(
                                                  const BAM_Alignment *self,
                                                  BAM_AlignmentDetail *rslt, uint32_t count, uint32_t *actual,
                                                  int32_t *pfirst, int32_t *plast
                                                  )
{
    unsigned i;
    unsigned ccnt; /* cigar count */
    int32_t  gpos; /* refSeq pos in global coordinates */
    unsigned rpos; /* read pos (always local coordinates) */
    uint32_t rlen; /* read length */
    int32_t first = -1;
    int32_t last = -1;

    if (!self)
        return RC(rcAlign, rcFile, rcReading, rcSelf, rcNull);

    rlen = getReadLen(self);
    ccnt = getCigarCount(self);
    gpos = getPosition(self);
    
    if (gpos < 0)
        ccnt = 0;
    
    if (actual)
        *actual = ccnt;
    
    if (pfirst)
        *pfirst = -1;

    if (plast)
        *plast = -1;

    if (ccnt == 0)
        return 0;
    
    if (rslt == NULL) {
        if (actual == NULL)
            return RC(rcAlign, rcFile, rcReading, rcParam, rcNull);
        count = 0;
    }
    
    if (count < ccnt)
        return RC(rcAlign, rcFile, rcReading, rcBuffer, rcInsufficient);
        
    for (rpos = 0, i = 0; i != ccnt; ++i) {
        uint32_t len = BAM_AlignmentGetCigarElement(self, i);
        int op = len & 0x0F;
        
        if (op > sizeof(cigarChars))
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        
        op = cigarChars[op];
        len >>= 4;
        
        rslt[i].refSeq_pos = gpos;
        rslt[i].read_pos = rpos;
        rslt[i].length = len;
        rslt[i].type = (BAMCigarType)op;
        
        switch ((BAMCigarType)op) {
        case ct_Match:
        case ct_Equal:
            if (first == -1)
                first = i;
            last = i;
            gpos += len;
            rpos += len;
            break;
        case ct_Insert:
        case ct_SoftClip:
            gpos += len;
            break;
        case ct_Delete:
        case ct_Skip:
            rpos += len;
            break;
        case ct_HardClip:
        case ct_Padded:
            rslt[i].refSeq_pos = -1;
            rslt[i].read_pos = -1;
            break;
        default:
            break;
        }
        
        if (rslt[i].read_pos > rlen)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    }
    if (pfirst)
        *pfirst = first;
    
    if (plast)
        *plast = last;
    
    return 0;
}

static
unsigned ReferenceLengthFromCIGAR(const BAM_Alignment *self)
{
    unsigned i;
    unsigned n = getCigarCount(self);
    unsigned y;
    
    for (i = 0, y = 0; i != n; ++i) {
        uint32_t const len = BAM_AlignmentGetCigarElement(self, i);
        
        switch (cigarChars[len & 0x0F]) {
        case ct_Match:
        case ct_Equal:
        case ct_NotEqual:
        case ct_Delete:
        case ct_Skip:
            y += len >> 4;
            break;
        default:
            break;
        }
    }
    return y;
}

#if 0
static unsigned SequenceLengthFromCIGAR(const BAM_Alignment *self)
{
    unsigned i;
    unsigned n = getCigarCount(self);
    unsigned y;
    
    for (i = 0, y = 0; i != n; ++i) {
        uint32_t const len = BAM_AlignmentGetCigarElement(self, i);
        
        switch (cigarChars[len & 0x0F]) {
        case ct_Match:
        case ct_Equal:
        case ct_NotEqual:
        case ct_Insert:
        case ct_SoftClip:
            y += len >> 4;
            break;
        default:
            break;
        }
    }
    return y;
}
#endif

rc_t BAM_AlignmentGetPosition2(const BAM_Alignment *cself, int64_t *rhs, uint32_t *length)
{
    *rhs = getPosition(cself);
    if (*rhs >= 0)
        *length = ReferenceLengthFromCIGAR(cself);
    return 0;
}

rc_t BAM_AlignmentGetReadGroupName(const BAM_Alignment *cself, const char **rhs)
{
    *rhs = get_RG(cself);
    return 0;
}

rc_t BAM_AlignmentGetReadName(const BAM_Alignment *cself, const char **rhs)
{
    *rhs = getReadName(cself);
    return 0;
}

rc_t BAM_AlignmentGetReadName2(const BAM_Alignment *cself, const char **rhs, size_t *length)
{
    *length = getReadNameLength(cself) - 1;
    *rhs = getReadName(cself);
    return 0;
}

rc_t BAM_AlignmentGetReadName3(const BAM_Alignment *cself, const char **rhs, size_t *length)
{
    char const *const name = getReadName(cself);
    size_t len = getReadNameLength(cself);
    size_t i;
    
    for (i = len; i; ) {
        int const ch = name[--i];
        
        if (ch == '/') {
            len = i;
            break;
        }
        if (!isdigit(ch))
            break;
    }
    *rhs = name;
    *length = len;

    return 0;
}

rc_t BAM_AlignmentGetFlags(const BAM_Alignment *cself, uint16_t *rhs)
{
    *rhs = getFlags(cself);
    return 0;
}

rc_t BAM_AlignmentGetMapQuality(const BAM_Alignment *cself, uint8_t *rhs)
{
    *rhs = getMapQual(cself);
    return 0;
}

rc_t BAM_AlignmentGetCigarCount(const BAM_Alignment *cself, unsigned *rhs)
{
    *rhs = getCigarCount(cself);
    return 0;
}

rc_t BAM_AlignmentGetRawCigar(const BAM_Alignment *cself, uint32_t const *rslt[], uint32_t *length)
{
    *rslt = getCigarBase(cself);
    *length = getCigarCount(cself);
    return 0;
}

rc_t BAM_AlignmentGetCigar(const BAM_Alignment *cself, uint32_t i, BAMCigarType *type, uint32_t *length)
{
    uint32_t x;
    
    if (i >= getCigarCount(cself))
        return RC(rcAlign, rcFile, rcReading, rcParam, rcInvalid);

    x = BAM_AlignmentGetCigarElement(cself, i);
    *type = (BAMCigarType)(cigarChars[x & 0x0F]);
    *length = x >> 4;
    return 0;
}

rc_t BAM_AlignmentGetReadLength(const BAM_Alignment *cself, uint32_t *rhs)
{
    *rhs = getReadLen(cself);
    return 0;
}

static int get1Base(BAM_Alignment const *const self, unsigned const i)
{
/*
 *   =    A    C    M    G    R    S    V    T    W    Y    H    K    D    B    N
 * 0000 0001 0010 0011 0100 0101 0110 0111 1000 1001 1010 1011 1100 1101 1110 1111
 * 1111 1000 0100 1100 0010 1010 0110 1110 0001 1001 0101 1101 0011 1011 0111 0000
 *   N    T    G    K    C    Y    S    B    A    W    R    D    M    H    V    =
 */
    static const char  tr[16] = "=ACMGRSVTWYHKDBN";
/*  static const char ctr[16] = "=TGKCYSBAWRDMHVN"; */
    uint8_t const *const seq = &self->data->raw[self->seq];
    unsigned const b4na2 = seq[i >> 1];
    unsigned const b4na = (i & 1) == 0 ? (b4na2 >> 4) : (b4na2 & 0x0F);
    
    return tr[b4na];
}

static int get1Qual(BAM_Alignment const *const self, unsigned const i)
{
    uint8_t const *const src = &self->data->raw[self->qual];
    
    return src[i];
}

rc_t BAM_AlignmentGetSequence2(const BAM_Alignment *cself, char *rhs, uint32_t start, uint32_t stop)
{
    unsigned const n = getReadLen(cself);
    unsigned si, di;
    
    if (stop == 0 || stop > n)
        stop = n;
    
    for (di = 0, si = start; si != stop; ++si, ++di) {
        rhs[di] = get1Base(cself, si);
    }
    return 0;
}

rc_t BAM_AlignmentGetSequence(const BAM_Alignment *cself, char *rhs)
{
    return BAM_AlignmentGetSequence2(cself, rhs, 0, 0);
}

bool BAM_AlignmentHasColorSpace(BAM_Alignment const *cself)
{
    return get_CS(cself) != NULL;
}

rc_t BAM_AlignmentGetCSKey(BAM_Alignment const *cself, char rhs[1])
{
    char const *const vCS = get_CS(cself);
    
    if (vCS)
        rhs[0] = vCS[0];
    return 0;
}

rc_t BAM_AlignmentGetCSSeqLen(BAM_Alignment const *cself, uint32_t *rhs)
{
    struct offset_size_s const *const vCS = get_CS_info(cself);
    
    *rhs = vCS ? vCS->size - 5 : 0;
    return 0;
}

rc_t BAM_AlignmentGetCSSequence(BAM_Alignment const *cself, char rhs[], uint32_t seqlen)
{
    char const *const vCS = get_CS(cself);
    
    if (vCS) {
        unsigned i;
        
        for (i = 0;i != seqlen; ++i) {
            char const ch = vCS[i+1];
            
            rhs[i] = (ch == '4') ? '.' : ch;
        }
    }
    return 0;
}

rc_t BAM_AlignmentGetQuality(const BAM_Alignment *cself, const uint8_t **rhs)
{
    *rhs = &cself->data->raw[cself->qual];
    return 0;
}

rc_t BAM_AlignmentGetQuality2(BAM_Alignment const *cself, uint8_t const **rhs, uint8_t *offset)
{
    uint8_t const *const OQ = get_OQ(cself);
    
    if (OQ) {
        struct offset_size_s const *const oq = get_OQ_info(cself);
        
        if (oq->size - 4 == getReadLen(cself)) {
            *offset = 33;
            *rhs = OQ;
        }
        else
            return RC(rcAlign, rcRow, rcReading, rcData, rcInconsistent);
    }
    else {
        *offset = 0;
        *rhs = &cself->data->raw[cself->qual];
    }
    return 0;
}

rc_t BAM_AlignmentGetCSQuality(BAM_Alignment const *cself, uint8_t const **rhs, uint8_t *offset)
{
    struct offset_size_s const *const cs = get_CS_info(cself);
    struct offset_size_s const *const cq = get_CQ_info(cself);
    uint8_t const *const CQ = get_CQ(cself);
    
    if (cs && cq && CQ) {
        if (cs->size == cq->size) {
            *offset = 33;
            *rhs = CQ + 1;
            return 0;
        }
        if (cs->size == cq->size + 1) {
            *offset = 33;
            *rhs = CQ;
            return 0;
        }
        return RC(rcAlign, rcRow, rcReading, rcData, rcInconsistent);
    }
    *offset = 0;
    *rhs = &cself->data->raw[cself->qual];
    return 0;
}

rc_t BAM_AlignmentGetMateRefSeqId(const BAM_Alignment *cself, int32_t *rhs)
{
    *rhs = getMateRefSeqId(cself);
    return 0;
}

rc_t BAM_AlignmentGetMatePosition(const BAM_Alignment *cself, int64_t *rhs)
{
    *rhs = getMatePos(cself);
    return 0;
}

rc_t BAM_AlignmentGetInsertSize(const BAM_Alignment *cself, int64_t *rhs)
{
    *rhs = getInsertSize(cself);
    return 0;
}

static int FormatOptData(BAM_Alignment const *const self,
                         size_t const maxsize,
                         char buffer[])
{
    char const *const base = (char const *)&self->data->raw[self->qual + getReadLen(self)];
    unsigned i;
    unsigned offset;
    unsigned cur = 0;
    int j;
    
    for (i = 0, offset = 0; i < self->numExtra; ++i) {
        int type;
        union { float f; uint32_t i; } fi;
        
        if (cur + 7 > maxsize)
            return -1;
        buffer[cur++] = '\t';
        buffer[cur++] = base[offset++];
        buffer[cur++] = base[offset++];
        buffer[cur++] = ':';
        type = base[offset++];

        switch (type) {
            case dt_ASCII:      /* A */
                buffer[cur++] = 'A';
                buffer[cur++] = ':';
                buffer[cur++] = base[offset++];
                break;

            case dt_INT8:       /* c */
                buffer[cur++] = 'i';
                buffer[cur++] = ':';
                j = snprintf(buffer + cur, maxsize - cur, "%i", (int)*((int8_t const *)(base + offset)));
                if ((cur += j) >= maxsize)
                    return -1;
                offset += 1;
                break;

            case dt_UINT8:      /* C */
                buffer[cur++] = 'i';
                buffer[cur++] = ':';
                j = snprintf(buffer + cur, maxsize - cur, "%u", (unsigned)*((uint8_t const *)(base + offset)));
                if ((cur += j) >= maxsize)
                    return -1;
                offset += 1;
                break;
                
            case dt_INT16:      /* s */
                buffer[cur++] = 'i';
                buffer[cur++] = ':';
                j = snprintf(buffer + cur, maxsize - cur, "%i", (int)LE2HI16(base + offset));
                if ((cur += j) >= maxsize)
                    return -1;
                offset += 2;
                break;

            case dt_UINT16:     /* S */
                buffer[cur++] = 'i';
                buffer[cur++] = ':';
                j = snprintf(buffer + cur, maxsize - cur, "%u", (unsigned)LE2HUI16(base + offset));
                if ((cur += j) >= maxsize)
                    return -1;
                offset += 2;
                break;
                
            case dt_INT:        /* i */
                buffer[cur++] = 'i';
                buffer[cur++] = ':';
                j = snprintf(buffer + cur, maxsize - cur, "%i", (int)LE2HI32(base + offset));
                if ((cur += j) >= maxsize)
                    return -1;
                offset += 4;
                break;

            case dt_UINT:       /* I */
                buffer[cur++] = 'i';
                buffer[cur++] = ':';
                j = snprintf(buffer + cur, maxsize - cur, "%i", (int)LE2HI32(base + offset));
                if ((cur += j) >= maxsize)
                    return -1;
                offset += 4;
                break;

            case dt_FLOAT32:    /* f */
                buffer[cur++] = 'f';
                buffer[cur++] = ':';
                fi.i = LE2HUI32(base + offset);
                j = snprintf(buffer + cur, maxsize - cur, "%f", fi.f);
                if ((cur += j) >= maxsize)
                    return -1;
                while (buffer[cur - 1] == '0')
                    --cur;
                if (buffer[cur - 1] == '.')
                    --cur;
                offset += 4;
                break;

            case dt_HEXSTRING:  /* H */
            case dt_CSTRING:    /* Z */
                buffer[cur++] = type == dt_CSTRING ? 'Z' : 'H';
                buffer[cur++] = ':';
                for ( ; ; ) {
                    int const ch = base[offset++];
                    
                    if (ch == '\0')
                        break;
                    if (cur >= maxsize)
                        return -1;
                    buffer[cur++] = ch;
                }
                break;

            case dt_NUM_ARRAY:  /* B */
                buffer[cur++] = 'B';
                buffer[cur++] = ':';
                {
                    int const elemtype = base[offset++];
                    unsigned const elemcount = LE2HUI32(base + offset);
                    unsigned k;

                    if (cur + 2 >= maxsize)
                        return -1;
                    buffer[cur++] = elemtype;
                    offset += 4;
                    for (k = 0; k < elemcount; ++k) {
                        buffer[cur++] = ',';
                        switch (elemtype) {
                            case dt_INT8:
                                j = snprintf(buffer + cur, maxsize - cur, "%i", (int)*((int8_t const *)(base + offset)));
                                if ((cur += j) >= maxsize)
                                    return -1;
                                offset += 1;
                                break;
                                
                            case dt_UINT8:
                                j = snprintf(buffer + cur, maxsize - cur, "%u", (unsigned)*((uint8_t const *)(base + offset)));
                                if ((cur += j) >= maxsize)
                                    return -1;
                                offset += 1;
                                break;
                                
                            case dt_INT16:
                                j = snprintf(buffer + cur, maxsize - cur, "%i", (int)LE2HI16(base + offset));
                                if ((cur += j) >= maxsize)
                                    return -1;
                                offset += 2;
                                break;
                                
                            case dt_UINT16:
                                j = snprintf(buffer + cur, maxsize - cur, "%u", (unsigned)LE2HUI16(base + offset));
                                if ((cur += j) >= maxsize)
                                    return -1;
                                offset += 2;
                                break;
                                
                            case dt_INT:
                                j = snprintf(buffer + cur, maxsize - cur, "%i", (int)LE2HI32(base + offset));
                                if ((cur += j) >= maxsize)
                                    return -1;
                                offset += 4;
                                break;
                                
                            case dt_UINT:
                                j = snprintf(buffer + cur, maxsize - cur, "%u", (unsigned)LE2HUI32(base + offset));
                                if ((cur += j) >= maxsize)
                                    return -1;
                                offset += 4;
                                break;
                                
                            case dt_FLOAT32:
                                fi.i = LE2HUI32(base + offset);
                                j = snprintf(buffer + cur, maxsize - cur, "%f", fi.f);
                                if ((cur += j) >= maxsize)
                                    return -1;
                                while (buffer[cur - 1] == '0')
                                    --cur;
                                if (buffer[cur - 1] == '.')
                                    --cur;
                                offset += 4;
                                break;

                            default:
                                return -1;
                                break;
                        }
                    }
                }
                break;

            default:
                return -1;
                break;
        }
    }
    return cur;
}

static rc_t FormatSAM(BAM_Alignment const *self,
                      size_t *const actsize,
                      size_t const maxsize,
                      char *const buffer)
{
    int i = 0;
    size_t cur = 0;
    unsigned j;
    int const refSeqId = getRefSeqId(self);
    int const refPos = getPosition(self);
    unsigned const cigCount = getCigarCount(self);
    uint32_t const *const cigar = getCigarBase(self);
    int const mateRefSeqId = getMateRefSeqId(self);
    int const mateRefPos = getMatePos(self);
    unsigned const readlen = getReadLen(self);

    i = snprintf(&buffer[cur], maxsize - cur,
                 "%s\t%i\t%s\t%i\t%i\t",
                 getReadName(self),
                 getFlags(self),
                 refSeqId < 0 ? "*" : self->parent->refSeq[refSeqId].name,
                 refPos < 0 ? 0 : refPos + 1,
                 getMapQual(self)
                 );
    if ((cur += i) > maxsize)
        return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);

    if (cigCount > 0) {
        for (j = 0; j < cigCount; ++j) {
            uint32_t const el = cigar[j];
            BAMCigarType const type = (BAMCigarType)(cigarChars[el & 0x0F]);
            unsigned const length = el >> 4;

            i = snprintf(&buffer[cur], maxsize - cur, "%u%c", length, type);
            if ((cur += i) > maxsize)
                return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);
        }
    }
    else {
        if ((cur + 1) > maxsize)
            return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);
        buffer[cur++] = '*';
    }
    i = snprintf(&buffer[cur], maxsize - cur,
                 "\t%s\t%i\t%i\t",
                 mateRefSeqId < 0 ? "*" : mateRefSeqId == refSeqId ? "=" : self->parent->refSeq[mateRefSeqId].name,
                 mateRefPos < 0 ? 0 : mateRefPos + 1,
                 getInsertSize(self)
                 );
    if ((cur += i) > maxsize)
        return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);
    if (readlen) {
        uint8_t const *const qual = &self->data->raw[self->qual];
        
        if (cur + 2 * readlen + 1 > maxsize)
            return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);
        BAM_AlignmentGetSequence(self, &buffer[cur]);
        cur += readlen;
        buffer[cur] = '\t';
        ++cur;
        
        for (j = 0; j < readlen; ++j) {
            if (qual[j] != 0xFF)
                goto HAS_QUAL;
        }
        if (1) {
            buffer[cur++] = '*';
        }
        else {
    HAS_QUAL:
            for (j = 0; j < readlen; ++j)
                buffer[cur++] = qual[j] + 33;
        }
    }
    else {
        i = snprintf(&buffer[cur], maxsize - cur, "*\t*");
        if ((cur += i) > maxsize)
            return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);
    }
    i = FormatOptData(self, maxsize - cur, &buffer[cur]);
    if (i < 0)
        return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);
    if ((cur += i) + 2 > maxsize)
        return RC(rcAlign, rcRow, rcReading, rcData, rcExcessive);
    buffer[cur++] = '\n';
    buffer[cur] = '\0';
    *actsize = cur;

    return 0;
}

#define FORMAT_SAM_SCRATCH_SIZE ((size_t)(64u * 1024u))
static rc_t FormatSAMBuffer(BAM_Alignment const *self,
                            size_t actSize[],
                            size_t const maxsize,
                            char *const buffer)
{
    char scratch[FORMAT_SAM_SCRATCH_SIZE];
    size_t actsize = 0;
    rc_t const rc = FormatSAM(self, &actsize, FORMAT_SAM_SCRATCH_SIZE, scratch);

    actSize[0] = actsize;
    if (rc) return rc;
    
    if (actsize > maxsize)
        return RC(rcAlign, rcReading, rcRow, rcBuffer, rcInsufficient);

    memmove(buffer, scratch, actsize);
    return 0;
}

rc_t BAM_AlignmentFormatSAM(BAM_Alignment const *self,
                                         size_t *const actSize,
                                         size_t const maxsize,
                                         char *const buffer)
{
    if (self == NULL)
        return RC(rcAlign, rcReading, rcRow, rcSelf, rcNull);
    if (buffer == NULL)
        return RC(rcAlign, rcReading, rcRow, rcParam, rcNull);
    else {
        size_t actsize = 0;
        rc_t const rc = (maxsize < FORMAT_SAM_SCRATCH_SIZE ? FormatSAMBuffer : FormatSAM)(self, &actsize, maxsize, buffer);

        if (actSize)
            *actSize = actsize;
        return rc;
    }
}

typedef struct OptForEach_ctx_s {
    BAMOptData *val;
    BAMOptData **alloced;
    size_t valsize;
    rc_t rc;
    BAMOptionalDataFunction user_f;
    void *user_ctx;
} OptForEach_ctx_t;

static bool i_OptDataForEach(BAM_Alignment const *cself, void *Ctx, char const tag[2], BAMOptDataValueType type, unsigned count, void const *value, unsigned size)
{
    OptForEach_ctx_t *ctx = (OptForEach_ctx_t *)Ctx;
    size_t const need = (size_t)&((BAMOptData const *)NULL)->u.f64[(count * size + sizeof(double) - 1)/sizeof(double)];
    
    if (need > ctx->valsize) {
        void *const temp = realloc(ctx->alloced, need);
        if (temp == NULL) {
            ctx->rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
            return true;
        }
        *ctx->alloced = ctx->val = temp;
        ctx->valsize = need;
    }
    ctx->val->type = type;
    ctx->val->element_count = (type == dt_CSTRING || type == dt_HEXSTRING) ? size - 1 : count;
    
    memmove(ctx->val->u.u8, value, size * count);
#if __BYTE_ORDER == __BIG_ENDIAN
    {{
        unsigned di;
        uint32_t elem_count = ctx->val->element_count;
        
        switch (size) {
        case 2:
            for (di = 0; di != elem_count; ++di)
                ctx->val->u.u16[di] = LE2HUI16(&ctx->val->u.u16[di]);
            break;
        case 4:
            for (di = 0; di != elem_count; ++di)
                ctx->val->u.u32[di] = LE2HUI32(&ctx->val->u.u32[di]);
            break;
        case 8:
            for (di = 0; di != elem_count; ++di)
                ctx->val->u.u64[di] = LE2HUI64(&ctx->val->u.u64[di]);
            break;
        }
    }}
#endif
    ctx->rc = ctx->user_f(ctx->user_ctx, tag, ctx->val);
    return ctx->rc != 0;
}

rc_t BAM_AlignmentOptDataForEach(const BAM_Alignment *cself, void *user_ctx, BAMOptionalDataFunction f)
{
    union u {
        BAMOptData value;
        uint8_t storage[4096];
    } value_auto;
    OptForEach_ctx_t ctx;
    rc_t rc = 0;
    unsigned i;
    
    ctx.val = &value_auto.value;
    ctx.alloced = NULL;
    ctx.valsize = sizeof(value_auto);
    ctx.rc = 0;
    ctx.user_f = f;
    ctx.user_ctx = user_ctx;
    
    for (i = 0; i != cself->numExtra; ++i) {
        char const *const tag = (char const *)&cself->data->raw[cself->extra[i].offset];
        uint8_t type = tag[2];
        uint8_t const *const vp = (uint8_t const *)&tag[3];
        unsigned len = cself->extra[i].size - 3;
        unsigned size = cself->extra[i].size - 3;
        unsigned count = 1;
        unsigned offset = 0;
        
        if (type == dt_NUM_ARRAY) {
            unsigned elem_size = 0;
            uint32_t elem_count = 0;
            
            offset = len = 5;
            switch (vp[0]) {
            case dt_INT8:
            case dt_UINT8:
                elem_size = 1;
                break;
            case dt_INT16:
            case dt_UINT16:
                elem_size = 2;
                break;
            case dt_FLOAT32:
            case dt_INT:
            case dt_UINT:
                elem_size = 4;
                break;
#if 0
            case dt_FLOAT64:
                elem_size = 8;
                break;
#endif
            default:
                rc = RC(rcAlign, rcFile, rcReading, rcData, rcUnexpected);
                break;
            }
            if (rc)
                break;
            elem_count = LE2HUI32(&vp[1]);
            len += elem_size * elem_count;
            type = vp[0];
            count = elem_count;
            size = elem_size;
            break;
        }
        if (i_OptDataForEach(cself, &ctx, tag, type, count, &vp[offset], size))
            break;
    }
    rc = rc ? rc : ctx.rc;
    if (ctx.alloced)
        free(ctx.alloced);
    return rc;
}

/* MARK: Complete Genomics stuff */

bool BAM_AlignmentHasCGData(BAM_Alignment const *self)
{
    return get_CG_GC_info(self) && get_CG_GS_info(self) && get_CG_GQ_info(self);
}

rc_t BAM_AlignmentCGReadLength(BAM_Alignment const *self, uint32_t *readlen)
{
    struct offset_size_s const *const GCi = get_CG_GC_info(self);
    struct offset_size_s const *const GSi = get_CG_GS_info(self);
    struct offset_size_s const *const GQi = get_CG_GQ_info(self);
    
    if (GCi && GSi && GQi) {
        char const *GS = (char const *)&self->data->raw[GSi->offset + 3];
        char const *GQ = (char const *)&self->data->raw[GQi->offset + 3];
        char const *GC = (char const *)&self->data->raw[GCi->offset + 3];
        unsigned oplen = 0;
        unsigned i;
        unsigned di = 0;
        unsigned si = 0;
        
        for (i = 0; ; ++i) {
            int const ch = GC[i];
            
            if (ch == '\0')
                break;
            if (isdigit(ch)) {
                oplen = oplen * 10 + (ch - '0');
            }
            else if (ch != 'S' && ch != 'G')
                return RC(rcAlign, rcRow, rcReading, rcData, rcUnexpected);
            else {
                unsigned const jmax = (ch == 'G') ? (oplen * 2) : oplen;
                unsigned j;
                
                if (ch == 'S') {
                    ;
                }
                else {
                    for (j = 0; j < jmax; ++j) {
                        int const base = *GS++;
                        int const qual = *GQ++;
                        
                        switch (base) {
                            case 'A':
                            case 'C':
                            case 'M':
                            case 'G':
                            case 'R':
                            case 'S':
                            case 'V':
                            case 'T':
                            case 'W':
                            case 'Y':
                            case 'H':
                            case 'K':
                            case 'D':
                            case 'B':
                            case 'N':
                                break;
                            default:
                                return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
                        }
                        if (qual < 33)
                            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
                    }
                }
                si += oplen;
                di += jmax;
                oplen = 0;
            }
        }
        if (*GS != '\0' || *GQ != '\0' || si != getReadLen(self))
            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
        
        *readlen = di;
        return 0;
    }
    return SILENT_RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

static unsigned BAM_AlignmentParseCGTag(BAM_Alignment const *self, size_t const max_cg_segs, unsigned cg_segs[/* max_cg_segs */])
{
    struct offset_size_s const *const GCi = get_CG_GC_info(self);
    char const *cur = (char const *)&self->data->raw[GCi->offset + 3];
    unsigned i = 0;
    int last_op = 0;

    memset(cg_segs, 0, max_cg_segs * sizeof(cg_segs[0]));
    
    while (*cur != '\0' && i < max_cg_segs) {
        char *endp;
        unsigned const op_len = (unsigned)strtol(cur, &endp, 10);
        int const op = *endp;
        
        cur = endp + 1;
        if (op == last_op)
            cg_segs[i - 1] += op_len;
        else
            cg_segs[i++] = op_len;
        last_op = op;
    }
    return i;
}

static
rc_t ExtractInt32(BAM_Alignment const *self, int32_t *result,
                  struct offset_size_s const *const tag)
{
    int64_t y;
    int const type = self->data->raw[tag->offset + 2];
    void const *const pvalue = &self->data->raw[tag->offset + 3];
    
    switch (type) {
    case 'c':
        if (tag->size == 4)
            y = *((int8_t const *)pvalue);
        else
            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
        break;
    case 'C':
        if (tag->size == 4)
            y = *((uint8_t const *)pvalue);
        else
            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
        break;
    case 's':
        if (tag->size == 5)
            y = LE2HI16(pvalue);
        else
            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
        break;
    case 'S':
        if (tag->size == 5)
            y = LE2HUI16(pvalue);
        else
            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
        break;
    case 'i':
        if (tag->size == 7)
            y = LE2HI32(pvalue);
        else
            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
        break;
    case 'I':
        if (tag->size == 7)
            y = LE2HUI32(pvalue);
        else
            return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
        break;
    default:
        return SILENT_RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
    }
    if (INT32_MIN <= y && y <= INT32_MAX) {
        *result = (int32_t)y;
        return 0;
    }
    return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
}

rc_t BAM_AlignmentGetCGAlignGroup(BAM_Alignment const *self,
                                    char buffer[],
                                    size_t max_size,
                                    size_t *act_size)
{
    struct offset_size_s const *const ZA = get_CG_ZA_info(self);
    struct offset_size_s const *const ZI = get_CG_ZI_info(self);
    
    if (ZA && ZI) {
        rc_t rc;
        int32_t za;
        int32_t zi;
        
        rc = ExtractInt32(self, &za, ZA); if (rc) return rc;
        rc = ExtractInt32(self, &zi, ZI); if (rc) return rc;
        return string_printf(buffer, max_size, act_size, "%i_%i", zi, za);
    }
    return SILENT_RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

rc_t BAM_AlignmentGetCGSeqQual(BAM_Alignment const *self,
                                 char sequence[],
                                 uint8_t quality[])
{
    struct offset_size_s const *const GCi = get_CG_GC_info(self);
    struct offset_size_s const *const GSi = get_CG_GS_info(self);
    struct offset_size_s const *const GQi = get_CG_GQ_info(self);
    
    if (GCi && GSi && GQi) {
        char const *GS = (char const *)&self->data->raw[GSi->offset + 3];
        char const *GQ = (char const *)&self->data->raw[GQi->offset + 3];
        char const *GC = (char const *)&self->data->raw[GCi->offset + 3];
        unsigned oplen = 0;
        unsigned di = 0;
        unsigned si = 0;
        
        for ( ; ; ) {
            int const ch = *GC++;

            if (ch == '\0')
                break;
            if (isdigit(ch)) {
                oplen = oplen * 10 + (ch - '0');
                continue;
            }
            if (ch == 'S') {
                unsigned i;
                
                for (i = 0; i < oplen; ++i, ++di, ++si) {
                    unsigned const base = get1Base(self, si);
                    unsigned const qual = get1Qual(self, si);
                    
                    sequence[di] = base;
                    quality [di] = qual;
                }
            }
            else {
                unsigned i;
                
                for (i = 0; i < oplen * 2; ++i, ++di) {
                    unsigned const base = *GS++;
                    unsigned const qual = *GQ++ - 33;
                    
                    sequence[di] = base;
                    quality [di] = qual;
                }
                si += oplen;
            }
            oplen = 0;
        }
        return 0;
    }
    return SILENT_RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}


static unsigned splice(uint32_t cigar[], unsigned n, unsigned at, unsigned out, unsigned in, uint32_t const new_values[/* in */])
{
    assert(at + out <= n);
    memmove(&cigar[at + in], &cigar[at + out], (n - at - out) * 4);
    if (in)
        memmove(&cigar[at], new_values, in * 4);
    return n + in - out;
}

#define OPCODE_2_FIX (0xF)

static unsigned insert_B(unsigned const T, unsigned const G, unsigned const n, uint32_t cigar[/* n */])
{
    unsigned i;
    unsigned pos;
    
    for (pos = i = 0; i < n; ++i) {
        int const opcode = cigar[i] & 0xF;
        
        switch (opcode) {
        case 0:
        case 1:
        case 4:
        case 7:
        case 8:
            {{
                unsigned const len = cigar[i] >> 4;
                unsigned const nxt = pos + len;
                
                if (pos <= T && T <= nxt) {
                    unsigned const l = T - pos;
                    unsigned const r = len - l;
                    uint32_t op[4];
                    
                    op[0] = (l << 4) | opcode;
                    op[1] = (G << 4) | 9; /* B */
                    op[2] = (G << 4) | 0; /* M this is not backwards */
                    op[3] = (r << 4) | opcode;
                    
                    return splice(cigar, n, i, 1,
                                   4 - (l == 0 ? 1 : 0) - (r == 0 ? 1 : 0),
                                  op + (l == 0 ? 1 : 0));
                }
                pos = nxt;
            }}
            break;
        default:
            break;
        }
    }
    return n;
}

static unsigned canonicalizeCIGAR(uint32_t cigar[], unsigned n)
{
    unsigned i;
    
    /* remove zero-length and P operations */
    for (i = n; i > 0; ) {
        --i;
        if (cigar[i] >> 4 == 0 || (cigar[i] & 0xF) == 6)
            n = splice(cigar, n, i, 1, 0, NULL);
    }
    /* merge adjacent operations of the same type */
    for (i = 1; i < n; ) {
        unsigned const opL = cigar[i-1] & 0xF;
        unsigned const opI = cigar[ i ] & 0xF;
        
        if (opI == opL) {
            unsigned const oplen = (cigar[i] >> 4) + (cigar[i-1] >> 4);
            uint32_t const op = (oplen << 4) | opI;

            n = splice(cigar, n, i-1, 2, 1, &op);
        }
        else
            ++i;
    }
    return n;
}

static unsigned GetCGCigar(BAM_Alignment const *self, unsigned const N, uint32_t cigar[/* N */])
{
    unsigned i;
    unsigned S;
    unsigned n = getCigarCount(self);
    unsigned seg[64];
    unsigned const segs = BAM_AlignmentParseCGTag(self, sizeof(seg)/sizeof(seg[0]), seg);
    unsigned const gaps = (segs - 1) >> 1;
    
    if (2 * gaps + 1 != segs)
        return RC(rcAlign, rcRow, rcReading, rcData, rcUnexpected);
    
    if (N < n + 2 * gaps)
        return RC(rcAlign, rcRow, rcReading, rcBuffer, rcInsufficient);
    
    memmove(cigar, getCigarBase(self), n * 4);

    if (n > 1)
        n = canonicalizeCIGAR(cigar, n); /* just in case */
    
    for (i = 0, S = 0; i < gaps; ++i) {
        unsigned const s = seg[2 * i + 0];
        unsigned const g = seg[2 * i + 1];

        S += s + g;
        if (g > 0)
            n = insert_B(S, g, n, cigar);
        S += g;
    }
    return n;
}

rc_t BAM_AlignmentGetCGCigar(BAM_Alignment const *self,
                               uint32_t *cigar,
                               uint32_t cig_max,
                               uint32_t *cig_act)
{
    struct offset_size_s const *const GCi = get_CG_GC_info(self);
    
    *cig_act = 0;
    
    if (GCi) {
        *cig_act = GetCGCigar(self, cig_max, cigar);
        return 0;
    }
    return SILENT_RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

/* MARK: end CG stuff */

rc_t BAM_AlignmentGetTI(BAM_Alignment const *self, uint64_t *ti)
{
    char const *const TI = get_XT(self);
    long long unsigned temp;
    
    if (TI && sscanf(TI, "ti|%llu", &temp) == 1) {
        *ti = (uint64_t)temp;
        return 0;
    }
    return SILENT_RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

rc_t BAM_AlignmentGetRNAStrand(BAM_Alignment const *const self, uint8_t *const rslt)
{
    if (rslt) {
        uint8_t const *const XS = get_XS(self);
        
	    *rslt = XS ? XS[0] : ' ';
    }
    return 0;
}

rc_t BAM_AlignmentGetLinkageGroup(BAM_Alignment const *const self,
                                  char const **const BX,
                                  char const **const CB,
                                  char const **const UB)
{
    *BX = get_BX(self);
    *CB = get_CB(self);
    *UB = get_UB(self);
    return 0;
}

rc_t BAM_AlignmentGetBarCode(BAM_Alignment const *self,
                                  char const **const BC)
{
    *BC = get_BC(self);
    return 0;
}
