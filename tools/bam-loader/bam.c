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

#include <align/extern.h>
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

#include <align/bam.h>
#include "bam-priv.h"

#include <vfs/path.h>
#include <vfs/path-priv.h>
#include <kfs/kfs-priv.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#if 1
/*_DEBUGGING*/
#include <stdio.h>
#include <os-native.h>
#endif

#include <endian.h>
#include <byteswap.h>

#include <zlib.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static uint16_t LE2HUI16(void const *X) { uint16_t y; memcpy(&y, X, sizeof(y)); return y; }
static uint32_t LE2HUI32(void const *X) { uint32_t y; memcpy(&y, X, sizeof(y)); return y; }
static uint64_t LE2HUI64(void const *X) { uint64_t y; memcpy(&y, X, sizeof(y)); return y; }
static  int16_t  LE2HI16(void const *X) {  int16_t y; memcpy(&y, X, sizeof(y)); return y; }
static  int32_t  LE2HI32(void const *X) {  int32_t y; memcpy(&y, X, sizeof(y)); return y; }
/* static  int64_t  LE2HI64(void const *X) {  int64_t y; memcpy(&y, X, sizeof(y)); return y; } */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
static uint16_t LE2HUI16(void const *X) { uint16_t y; memcpy(&y, X, sizeof(y)); return (uint16_t)bswap_16(y); }
static uint32_t LE2HUI32(void const *X) { uint32_t y; memcpy(&y, X, sizeof(y)); return (uint32_t)bswap_32(y); }
static uint64_t LE2HUI64(void const *X) { uint64_t y; memcpy(&y, X, sizeof(y)); return (uint64_t)bswap_64(y); }
static  int16_t  LE2HI16(void const *X) {  int16_t y; memcpy(&y, X, sizeof(y)); return ( int16_t)bswap_16(y); }
static  int32_t  LE2HI32(void const *X) {  int32_t y; memcpy(&y, X, sizeof(y)); return ( int32_t)bswap_32(y); }
static  int64_t  LE2HI64(void const *X) {  int64_t y; memcpy(&y, X, sizeof(y)); return ( int64_t)bswap_64(y); }
#endif

typedef struct BAMIndex BAMIndex;
typedef struct BufferedFile BufferedFile;
typedef struct SAMFile SAMFile;
typedef struct BGZFile BGZFile;

#define ZLIB_BLOCK_SIZE  (64u * 1024u)
#define RGLR_BUFFER_SIZE (256u * ZLIB_BLOCK_SIZE)
#define PIPE_BUFFER_SIZE (4096u)

typedef uint8_t zlib_block_t[ZLIB_BLOCK_SIZE];

typedef struct RawFile_vt_s {
    rc_t (*FileRead)(void *, zlib_block_t, unsigned *);
    uint64_t (*FileGetPos)(void const *);
    float (*FileProPos)(void const *);
    uint64_t (*FileGetSize)(void const *);
    rc_t (*FileSetPos)(void *, uint64_t);
    void (*FileWhack)(void *);
} RawFile_vt;

/* MARK: SAMFile */

struct BufferedFile {
    KFile const *kf;
    void *buf;
    uint64_t fmax;      /* file size if known or 0 */
    uint64_t fpos;      /* position in file of first byte in buffer */
    size_t bpos;        /* position in buffer of read head */
    size_t bmax;        /* number of valid bytes in buffer */
    size_t size;        /* maximum number of that can be read into buffer */
};

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

struct SAMFile {
    BufferedFile file;
    int putback;
    rc_t last;
};

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

static
rc_t SAMFileInit(SAMFile *self, RawFile_vt *vt)
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

#define CG_NUM_SEGS 4

struct BGZFile {
    BufferedFile file;
    z_stream zs;
};

static
rc_t BGZFileGetMoreBytes(BGZFile *self)
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

static
rc_t BGZFileRead(BGZFile *self, zlib_block_t dst, unsigned *pNumRead)
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
                if ( GetRCObject( rc ) == (enum RCObject)rcData && GetRCState( rc ) == rcInsufficient )
                {
                    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BGZF), ("EOF in Zlib block after %lu bytes\n", self->file.fpos + self->file.bpos));
                    rc = RC( rcAlign, rcFile, rcReading, rcFile, rcTooShort );
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

static rc_t BGZFileSetPos(BGZFile *const self, uint64_t const pos)
{
    rc_t const rc = BufferedFileSetPos(&self->file, pos);
    if (rc == 0) {
        self->zs.avail_in = (uInt)(self->file.bmax - self->file.bpos);
        self->zs.next_in = &((Bytef *)self->file.buf)[self->file.bpos];
    }
    return rc;
}

typedef rc_t (*BGZFileWalkBlocks_cb)(void *ctx, const BGZFile *file,
                                     rc_t rc, uint64_t fpos,
                                     const zlib_block_t data, unsigned dsize);

/* Without Decompression */
static rc_t BGZFileWalkBlocksND(BGZFile *const self, BGZFileWalkBlocks_cb const cb, void *const ctx)
{
    rc_t rc = 0;
#if VALIDATE_BGZF_HEADER
    uint8_t extra[256];
    char dummy[64];
    gz_header head;
    int zr;

    memset(&head, 0, sizeof(head));
    head.extra = extra;
    head.extra_max = sizeof(extra);
    
    do {
        unsigned loops;
        unsigned hsize = 0;
        unsigned bsize = 0;
        unsigned bsize2;
        uint64_t const fpos = self->file.fpos + self->file.bpos;
        
        self->zs.next_out = (Bytef *)dummy;
        self->zs.avail_out = sizeof(dummy);
        
        zr = inflateGetHeader(&self->zs, &head);
        assert(zr == Z_OK);
        
        for (loops = 0; loops != 2; ++loops) {
            {
                uLong const orig = self->zs.total_in;
                
                zr = inflate(&self->zs, Z_BLOCK); /* Z_BLOCK stops at end of header */
                {
                    uLong const final = self->zs.total_in;
                    uLong const bytes = final - orig;
                    
                    self->file.bpos += bytes;
                    hsize += bytes;
                }
            }
            if (head.done) {
                unsigned i;
                
                for (i = 0; i < head.extra_len; ) {
                    if (extra[i] == 'B' && extra[i + 1] == 'C') {
                        bsize = 1 + LE2HUI16(&extra[i + 4]);
                        break;
                    }
                    i += LE2HUI16(&extra[i + 2]);
                }
                break;
            }
            else if (self->zs.avail_in == 0) {
                rc = BGZFileGetMoreBytes(self);
                if (rc) {
                    rc = RC(rcAlign, rcFile, rcReading, rcFile, rcTooShort);
                    goto DONE;
                }
            }
            else {
                rc = RC(rcAlign, rcFile, rcReading, rcFile, rcCorrupt);
                goto DONE;
            }
        }
        if (bsize == 0) {
            rc = RC(rcAlign, rcFile, rcReading, rcFormat, rcInvalid); /* not BGZF */
            break;
        }
        bsize2 = bsize;
        bsize -= hsize;
        for ( ; ; ) {
            unsigned const max = (unsigned)(self->file.bmax - self->file.bpos); /* <= 64k */
            unsigned const len = bsize > max ? max : bsize;
            
            self->file.bpos += len;
            bsize -= len;
            if (self->file.bpos == self->file.bmax) {
                rc = BGZFileGetMoreBytes(self);
                if (rc) {
                    if (bsize)
                        rc = RC(rcAlign, rcFile, rcReading, rcFile, rcTooShort);
                    goto DONE;
                }
            }
            else {
                zr = inflateReset(&self->zs);
                assert(zr == Z_OK);
                self->zs.avail_in = (uInt)(self->file.bmax - self->file.bpos);
                self->zs.next_in = &((Bytef *)self->file.buf)[self->file.bpos];
                rc = cb(ctx, self, 0, fpos, NULL, bsize2);
                break;
            }
        }
    } while (rc == 0);
DONE:
    if ( GetRCState( rc ) == rcInsufficient && GetRCObject( rc ) == (enum RCObject)rcData )
        rc = 0;
    rc = cb( ctx, self, rc, self->file.fpos + self->file.bpos, NULL, 0 );
#endif
    return rc;
}

static rc_t BGZFileWalkBlocksUnzip(BGZFile *const self, zlib_block_t *const bufp, BGZFileWalkBlocks_cb const cb, void *const ctx)
{
    rc_t rc;
    rc_t rc2;
    
    do {
        uint64_t const fpos = self->file.fpos + self->file.bpos;
        unsigned dsize;
        
        rc2 = BGZFileRead(self, *bufp, &dsize);
        rc = cb(ctx, self, rc2, fpos, *bufp, dsize);
    } while (rc == 0 && rc2 == 0);
    if ( GetRCState( rc2 ) == rcInsufficient && GetRCObject( rc2 ) == (enum RCObject)rcData )
        rc2 = 0;
    rc = cb( ctx, self, rc2, self->file.fpos + self->file.bpos, NULL, 0 );
    return rc ? rc : rc2;
}

static rc_t BGZFileWalkBlocks(BGZFile *self, bool decompress, zlib_block_t *bufp,
                              BGZFileWalkBlocks_cb cb, void *ctx)
{
    rc_t rc;
    
#if VALIDATE_BGZF_HEADER
#else
    decompress = true;
#endif
    self->file.fpos = 0;
    self->file.bpos = 0;
    
    rc = BGZFileGetMoreBytes(self);
    if (rc)
        return rc;
    
    if (decompress)
        return BGZFileWalkBlocksUnzip(self, bufp, cb, ctx);
    else
        return BGZFileWalkBlocksND(self, cb, ctx);
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

/* MARK: BAMFile structures */

struct BAMIndex {
    BAMFilePosition *refSeq[1];
};

struct BAMFile {
    union {
        BGZFile bam;
        SAMFile sam;
    } file;
    RawFile_vt vt;
    
    BAMRefSeq *refSeq;          /* pointers into headerData1 except name points into headerData2 */ 
    BAMReadGroup *readGroup;    /* pointers into headerData1 */
    char const *version;
    char const *header;
    void *headerData1;          /* gets used for refSeq and readGroup */
    void *headerData2;          /* gets used for refSeq */
    BAMAlignment *bufLocker;
    BAMAlignment *nocopy;       /* used to hold current record for BAMFileRead2 */
    BAMIndex const *ndx;
    
    uint64_t fpos_first;
    uint64_t fpos_cur;
    
    size_t nocopy_size;
    
    KRefcount refcount;
    unsigned refSeqs;
    unsigned readGroups;
    unsigned ucfirst;           /* offset of first record in uncompressed buffer */
    unsigned bufSize;           /* current size of uncompressed buffer */
    unsigned bufCurrent;        /* location in uncompressed buffer of read head */
    bool eof;
    bool isSAM;
    zlib_block_t buffer;        /* uncompressed buffer */
};

/* MARK: Alignment structures */

struct bam_alignment_s {
    uint8_t rID[4];
    uint8_t pos[4];
    uint8_t read_name_len;
    uint8_t mapQual;
    uint8_t bin[2];
    uint8_t n_cigars[2];
    uint8_t flags[2];
    uint8_t read_len[4];
    uint8_t mate_rID[4];
    uint8_t mate_pos[4];
    uint8_t ins_size[4];
    char read_name[1 /* read_name_len */];
/* if you change length of read_name,
 * adjust calculation of offsets in BAMAlignmentSetOffsets */
/*  uint32_t cigar[n_cigars];
 *  uint8_t seq[(read_len + 1) / 2];
 *  uint8_t qual[read_len];
 *  uint8_t extra[...];
 */
};

typedef union bam_alignment_u {
    struct bam_alignment_s cooked;
    uint8_t raw[sizeof(struct bam_alignment_s)];
} bam_alignment;

struct offset_size_s {
    unsigned offset;
    unsigned size; /* this is the total length of the tag; length of data is size - 3 */
};

struct BAMAlignment {
    KRefcount refcount;
    
    BAMFile *parent;
    bam_alignment const *data;
    uint8_t *storage;
    unsigned datasize;
        
    unsigned cigar;
    unsigned seq;
    unsigned qual;
    unsigned numExtra;
    unsigned hasColor;
    struct offset_size_s extra[1];
};

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

/* MARK: Alignment accessors */

static int32_t getRefSeqId(const BAMAlignment *cself) {
    return LE2HI32(cself->data->cooked.rID);
}

static int32_t getPosition(const BAMAlignment *cself) {
    return LE2HI32(cself->data->cooked.pos);
}

static uint8_t getReadNameLength(const BAMAlignment *cself) {
    return cself->data->cooked.read_name_len;
}

static uint16_t getBin(const BAMAlignment *cself) {
    return LE2HUI16(cself->data->cooked.bin);
}

static uint8_t getMapQual(const BAMAlignment *cself) {
    return cself->data->cooked.mapQual;
}

static uint16_t getCigarCount(const BAMAlignment *cself) {
    return LE2HUI16(cself->data->cooked.n_cigars);
}

static uint16_t getFlags(const BAMAlignment *cself) {
    return LE2HUI16(cself->data->cooked.flags);
}

static uint32_t getReadLen(const BAMAlignment *cself) {
    return LE2HUI32(cself->data->cooked.read_len);
}

static int32_t getMateRefSeqId(const BAMAlignment *cself) {
    return LE2HI32(cself->data->cooked.mate_rID);
}

static int32_t getMatePos(const BAMAlignment *cself) {
    return LE2HI32(cself->data->cooked.mate_pos);
}

static int32_t getInsertSize(const BAMAlignment *cself) {
    return LE2HI32(cself->data->cooked.ins_size);
}

static char const *getReadName(const BAMAlignment *cself) {
    return &cself->data->cooked.read_name[0];
}

static void const *getCigarBase(BAMAlignment const *cself)
{
    return &cself->data->raw[cself->cigar];
}

static int opt_tag_cmp(char const a[2], char const b[2])
{
    int const d0 = (int)a[0] - (int)b[0];
    return d0 ? d0 : ((int)a[1] - (int)b[1]);
}

static int CC OptTag_sort(void const *A, void const *B, void *ctx)
{
    BAMAlignment const *const self = ctx;
    unsigned const a_off = ((struct offset_size_s const *)A)->offset;
    unsigned const b_off = ((struct offset_size_s const *)B)->offset;
    char const *const a = (char const *)&self->data->raw[a_off];
    char const *const b = (char const *)&self->data->raw[b_off];
    int const diff = opt_tag_cmp(a, b);
    
    return diff ? diff : (int)(a - b);
}

static unsigned tag_findfirst(BAMAlignment const *const self, char const tag[2])
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

static unsigned tag_runlength(BAMAlignment const *const self,
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

static struct offset_size_s const *tag_search(BAMAlignment const *const self,
                                              char const tag[2],
                                              int const which)
{
    unsigned const fnd = tag_findfirst(self, tag);
    unsigned const run = tag_runlength(self, tag, fnd);
    unsigned const want = which < 0 ? (run + which) : which;
    
    return run == 0 ? NULL : &self->extra[fnd + (want % run)];
}

static char const *get_RG(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "RG", 0);
    return (char const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static struct offset_size_s const *get_CS_info(BAMAlignment const *cself)
{
    return tag_search(cself, "CS", 0);
}

static struct offset_size_s const *get_CQ_info(BAMAlignment const *cself)
{
    return tag_search(cself, "CQ", 0);
}

static char const *get_CS(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = get_CS_info(cself);
    return (char const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static uint8_t const *get_CQ(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = get_CQ_info(cself);
    return (uint8_t const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static struct offset_size_s const *get_OQ_info(BAMAlignment const *cself)
{
    return tag_search(cself, "OQ", 0);
}

static uint8_t const *get_OQ(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = get_OQ_info(cself);
    return (uint8_t const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static char const *get_XT(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "XT", 0);
    return (char const *)(x && cself->data->raw[x->offset + 2] == 'Z' ? &cself->data->raw[x->offset + 3] : NULL);
}

static uint8_t const *get_XS(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "XS", -1); /* want last one */
    return (uint8_t const *)(x && cself->data->raw[x->offset + 2] == 'A' ? &cself->data->raw[x->offset + 3] : NULL);
}

static struct offset_size_s const *get_CG_ZA_info(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "ZA", 0);
    return x;
}

static struct offset_size_s const *get_CG_ZI_info(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "ZI", 0);
    return x;
}

static struct offset_size_s const *get_CG_GC_info(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "GC", 0);
    return x;
}

static struct offset_size_s const *get_CG_GS_info(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "GS", 0);
    return x;
}

static struct offset_size_s const *get_CG_GQ_info(BAMAlignment const *cself)
{
    struct offset_size_s const *const x = tag_search(cself, "GQ", 0);
    return x;
}

/* MARK: BAMFile Reading functions */

/* returns (rcData, rcInsufficient) if eof */
static rc_t BAMFileFillBuffer(BAMFile *self)
{
    rc_t const rc = self->vt.FileRead(&self->file, self->buffer, &self->bufSize);
    if (rc)
        return rc;
    if (self->bufSize == 0 || self->bufSize <= self->bufCurrent)
        return RC(rcAlign, rcFile, rcReading, rcData, rcInsufficient);
    return 0;
}

static rc_t BAMFileReadn(BAMFile *self, const unsigned len, uint8_t dst[/* len */]) {
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
            memcpy(&dst[cur], &self->buffer[self->bufCurrent], n);
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

        rc = BAMFileFillBuffer(self);
        if (rc)
            return rc;
    }
}

static void const *BAMFilePeek(BAMFile const *const self, unsigned const offset)
{
    return &self->buffer[self->bufCurrent + offset];
}

static unsigned BAMFileMaxPeek(BAMFile const *self)
{
    return self->bufSize > self->bufCurrent ? self->bufSize - self->bufCurrent : 0;
}

static int32_t BAMFilePeekI32(BAMFile *self)
{
    return LE2HI32(BAMFilePeek(self, 0));
}

static rc_t BAMFileReadI32(BAMFile *self, int32_t *rhs)
{
    uint8_t buf[sizeof(int32_t)];
    rc_t rc = BAMFileReadn(self, sizeof(int32_t), buf);
    
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
                return 0;
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
                return 0;
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
                return 0;
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

static bool ParseHeader(BAMFile *self, char hdata[], size_t hlen) {
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

static int CC comp_ReadGroup(const void *A, const void *B, void *ignored) {
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

static int CC comp_RefSeqName(const void *A, const void *B, void *ignored) {
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
        return cmp == 0 ? a->id - b->id : cmp;
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

static rc_t ProcessHeaderText(BAMFile *self, char const text[], bool makeCopy)
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
        memcpy(tmp, text, size + 1);
    }
    else
        self->header = text;
    {
    char *const copy = malloc(size + 1); /* an editable copy */
    if (copy == NULL)
        return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    self->headerData1 = copy; /* so it's not leaked */
    memcpy(copy, text, size + 1);
    
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

static rc_t ReadMagic(BAMFile *self)
{
    uint8_t sig[4];
    rc_t rc = BAMFileReadn(self, 4, sig);
    
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM signature: '%c%c%c' %u\n", sig[0], sig[1], sig[2], sig[3]));
    if (rc == 0 && (sig[0] != 'B' || sig[1] != 'A' || sig[2] != 'M' || sig[3] != 1))
        rc = RC(rcAlign, rcFile, rcReading, rcHeader, rcBadVersion);
    return rc;
}

static rc_t ReadHeaders(BAMFile *self,
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
    rc_t rc = BAMFileReadI32(self, &i32);
    
    if (rc) return rc;

    if (i32 < 0) {
        rc = RC(rcAlign, rcFile, rcReading, rcHeader, rcInvalid);
        goto BAILOUT;
    }
    hlen = i32;
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM Header text size: %u\n", hlen));
    if (hlen) {
        htxt = malloc(hlen + 1);
        if (htxt == NULL) {
            rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
            goto BAILOUT;
        }
        
        rc = BAMFileReadn(self, hlen, (uint8_t *)htxt); if (rc) goto BAILOUT;
        htxt[hlen] = '\0';
    }
    rc = BAMFileReadI32(self, &i32); if (rc) goto BAILOUT;
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
            rc = BAMFileReadI32(self, &i32); if (rc) goto BAILOUT;
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
            memcpy(rdat + rdsz, &i32, 4);
            rdsz += 4;
            rc = BAMFileReadn(self, i32, (uint8_t *)&rdat[rdsz]); if (rc) goto BAILOUT;
            rdsz += i32;
            rc = BAMFileReadI32(self, &i32); if (rc) goto BAILOUT;
            memcpy(rdat + rdsz, &i32, 4);
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
            memcpy(rs->checksum_array, refSeq[fnd].checksum_array, 16);
        }
        else
            rs->checksum = NULL;
    }
}

static rc_t ProcessBAMHeader(BAMFile *self, char const headerText[])
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
    
    self->fpos_first = self->fpos_cur;
    self->ucfirst = self->bufCurrent;
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("BAM Data records start at: %lu+%u\n", self->ucfirst, self->fpos_first));

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

static rc_t ProcessSAMHeader(BAMFile *self, char const substitute[])
{
    SAMFile *const file = &self->file.sam;
    size_t headerSize = 0;
    char *headerText = NULL;
    rc_t rc;
    int st = 0;
    
    for ( ; ; ) {
        void *const tmp = headerText;
        int const ch = SAMFileRead1(file);
        
        if (ch < 0)
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

static rc_t BAMIndexWhack(const BAMIndex *);

static void BAMFileWhack(BAMFile *self) {
    if (self->refSeq)
        free(self->refSeq);
    if (self->readGroup)
        free(self->readGroup);
    if (self->header)
        free((void *)self->header);
    if (self->headerData1)
        free((void *)self->headerData1);
    if (self->headerData2)
        free((void *)self->headerData2);
    if (self->ndx)
        BAMIndexWhack(self->ndx);
    if (self->nocopy)
        free(self->nocopy);
    if (self->vt.FileWhack)
        self->vt.FileWhack(&self->file);
    BufferedFileWhack(&self->file.bam.file);
}

/* MARK: BAM File constructors */

/* file is retained */
static rc_t BAMFileMakeWithKFileAndHeader(BAMFile const **cself,
                                          KFile const *file,
                                          char const *headerText)
{
    BAMFile *self = calloc(1, sizeof(*self));
    rc_t rc;
    
    if (self == NULL)
        return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    
    rc = BufferedFileInit(&self->file.bam.file, file);
    if (rc) {
        free(self);
        return rc;
    }
    
    KRefcountInit(&self->refcount, 1, "BAMFile", "new", "");
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
    KRefcountInit(&self->refcount, 1, "SAMFile", "new", "");
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

/* file is retained */
LIB_EXPORT rc_t CC BAMFileMakeWithKFile(const BAMFile **cself, const KFile *file)
{
    return BAMFileMakeWithKFileAndHeader(cself, file, NULL);
}

LIB_EXPORT rc_t CC BAMFileVMakeWithDir(const BAMFile **result,
                                         const KDirectory *dir,
                                         const char *path,
                                         va_list args
                                         )
{
    rc_t rc;
    const KFile *kf;
    
    if (result == NULL)
        return RC(rcAlign, rcFile, rcOpening, rcParam, rcNull);
    *result = NULL;
    rc = KDirectoryVOpenFileRead(dir, &kf, path, args);
    if (rc == 0) {
        rc = BAMFileMakeWithKFile(result, kf);
        KFileRelease(kf);
    }
    return rc;
}

LIB_EXPORT rc_t CC BAMFileMakeWithDir(const BAMFile **result,
                                        const KDirectory *dir,
                                        const char *path, ...
                                        )
{
    va_list args;
    rc_t rc;
    
    va_start(args, path);
    rc = BAMFileVMakeWithDir(result, dir, path, args);
    va_end(args);
    return rc;
}

LIB_EXPORT rc_t CC BAMFileMake(const BAMFile **cself, const char *path, ...)
{
    KDirectory *dir;
    va_list args;
    rc_t rc;
    
    if (cself == NULL)
        return RC(rcAlign, rcFile, rcOpening, rcParam, rcNull);
    *cself = NULL;
    
    rc = KDirectoryNativeDir(&dir);
    if (rc) return rc;
    va_start(args, path);
    rc = BAMFileVMakeWithDir(cself, dir, path, args);
    va_end(args);
    KDirectoryRelease(dir);
    return rc;
}

LIB_EXPORT rc_t CC BAMFileMakeWithHeader ( const BAMFile **cself,
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
    
    rc = KDirectoryNativeDir(&dir);
    if (rc) return rc;
    va_start(args, path);
    rc = KDirectoryVOpenFileRead(dir, &kf, path, args);
    if (rc == 0) {
        rc = BAMFileMakeWithKFileAndHeader(cself, kf, headerText);
        KFileRelease(kf);
    }
    va_end(args);
    KDirectoryRelease(dir);
    return rc;
}


LIB_EXPORT rc_t CC BAMFileMakeWithVPath(const BAMFile **cself, const VPath *kpath)
{
    char path[4096];
    size_t nread;
    rc_t rc;

    rc = VPathReadPath(kpath, path, sizeof(path), &nread);
    if (rc == 0)
        rc = BAMFileMake(cself, "%.*s", (int)nread, path);
    return rc;
}

/* MARK: BAM File ref-counting */

LIB_EXPORT rc_t CC BAMFileAddRef(const BAMFile *cself) {
    if (cself != NULL)
        KRefcountAdd(&cself->refcount, "BAMFile");
    return 0;
}

LIB_EXPORT rc_t CC BAMFileRelease(const BAMFile *cself) {
    BAMFile *self = (BAMFile *)cself;
    
    if (cself != NULL) {
        if (KRefcountDrop(&self->refcount, "BAMFile") == krefWhack) {
            BAMFileWhack(self);
            free(self);
        }
    }
    return 0;
}

/* MARK: BAM File positioning */

LIB_EXPORT float CC BAMFileGetProportionalPosition(const BAMFile *self)
{
    return self->vt.FileProPos(&self->file);
}

LIB_EXPORT rc_t CC BAMFileGetPosition(const BAMFile *self, BAMFilePosition *pos) {
    *pos = (self->fpos_cur << 16) | self->bufCurrent;
    return 0;
}

static rc_t BAMFileSetPositionInt(const BAMFile *cself, uint64_t fpos, uint16_t bpos)
{
    rc_t rc;
    BAMFile *self = (BAMFile *)cself;
    
    if (cself->fpos_first > fpos || fpos > cself->vt.FileGetSize(&cself->file) ||
        (fpos == cself->fpos_first && bpos < cself->ucfirst))
    {
        return RC(rcAlign, rcFile, rcPositioning, rcParam, rcInvalid);
    }
    if (cself->fpos_cur == fpos) {
        if (bpos <= cself->bufSize) {
            self->eof = false;
            self->bufCurrent = bpos;
            return 0;
        }
        return RC(rcAlign, rcFile, rcPositioning, rcParam, rcInvalid);
    }
    rc = self->vt.FileSetPos(&self->file, fpos);
    if (rc == 0) {
        self->eof = false;
        self->bufSize = 0; /* force re-read */
        self->bufCurrent = bpos;
        self->fpos_cur = fpos;
    }
    return rc;
}

LIB_EXPORT rc_t CC BAMFileSetPosition(const BAMFile *cself, const BAMFilePosition *pos)
{
    return BAMFileSetPositionInt(cself, *pos >> 16, (uint16_t)(*pos));
}

LIB_EXPORT rc_t CC BAMFileRewind(const BAMFile *cself)
{
    return BAMFileSetPositionInt(cself, cself->fpos_first, cself->ucfirst);
}

static void BAMFileAdvance(BAMFile *const self, unsigned distance)
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

static void ColorCheck(BAMAlignment *const self, char const tag[2], unsigned const len)
{
    if (tag[0] == 'C' && len != 0) {
        int const ch = tag[1];
        int const flag = ch == 'Q' ? 2 : ch == 'S' ? 1 : 0;
        
        if (flag)
            self->hasColor ^= (len << 2) | flag;
    }
}

static rc_t ParseOptData(BAMAlignment *const self, size_t const maxsize,
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

static rc_t ParseOptDataLog(BAMAlignment *const self, unsigned const maxsize,
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

static unsigned CC BAMAlignmentSize(unsigned const max_extra_tags)
{
    BAMAlignment const *const y = NULL;
    
    return sizeof(*y) + (max_extra_tags ? max_extra_tags - 1 : 0) * sizeof(y->extra);
}

static unsigned BAMAlignmentSetOffsets(BAMAlignment *const self)
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

static bool BAMAlignmentInit(BAMAlignment *const self, unsigned const maxsize,
                             unsigned const datasize, void const *const data)
{
    memset(self, 0, sizeof(*self));
    self->data = data;
    self->datasize = datasize;
    {
        unsigned const xtra = BAMAlignmentSetOffsets(self);
        
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

static bool BAMAlignmentInitLog(BAMAlignment *const self, unsigned const maxsize,
                                unsigned const datasize, void const *const data)
{
    memset(self, 0, sizeof(*self));
    self->data = data;
    self->datasize = datasize;
    {
        unsigned const xtra = BAMAlignmentSetOffsets(self);
        
        if (   datasize >= xtra
            && datasize >= self->cigar
            && datasize >= self->seq
            && datasize >= self->qual)
        {
            rc_t const rc = ParseOptDataLog(self, maxsize, xtra, datasize);
            
            if (rc == 0) {
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
                                                            (unsigned)datasize,
                                                            (unsigned)getReadNameLength(self),
                                                            (unsigned)getCigarCount(self),
                                                            (unsigned)getReadLen(self),
                                                            (unsigned)self->numExtra));
                return true;
            }
        }
        DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("{"
                                                        "\"BAM record\": "
                                                        "{ "
                                                            "\"size\": %u, "
                                                            "\"name length\": %u, "
                                                            "\"cigar count\": %u, "
                                                            "\"read length\": %u "
                                                        "}"
                                                    "}\n",
                                                    (unsigned)datasize,
                                                    (unsigned)getReadNameLength(self),
                                                    (unsigned)getCigarCount(self),
                                                    (unsigned)getReadLen(self)));
        return false;
    }
}

static void BAMAlignmentLogParseError(BAMAlignment const *self)
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
rc_t BAMFileReadNoCopy(BAMFile *const self, unsigned actsize[], BAMAlignment rhs[],
                       unsigned const maxsize)
{
    unsigned const maxPeek = BAMFileMaxPeek(self);
    bool isgood;

    *actsize = 0;
    
    if (maxPeek == 0) {
        rc_t const rc = BAMFileFillBuffer(self);

        if (rc == 0)
            return BAMFileReadNoCopy(self, actsize, rhs, maxsize);

        if ( GetRCObject( rc ) == (enum RCObject)rcData && GetRCState( rc ) == rcInsufficient )
        {
            self->eof = true;
            return RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound);
        }
        return rc;
    }
    if (maxPeek < 4)
        return RC(rcAlign, rcFile, rcReading, rcBuffer, rcNotAvailable);
    else {
        int32_t const i32 = BAMFilePeekI32(self);

        if (i32 <= 0)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        
        if (maxPeek < i32 + 4)
            return RC(rcAlign, rcFile, rcReading, rcBuffer, rcNotAvailable);
        
        isgood = BAMAlignmentInitLog(rhs, maxsize, i32, BAMFilePeek(self, 4));
        rhs[0].parent = self;
        KRefcountInit(&rhs->refcount, 1, "BAMAlignment", "ReadNoCopy", "");
    }
    *actsize = BAMAlignmentSize(rhs[0].numExtra);
    if (isgood && *actsize > maxsize)
        return RC(rcAlign, rcFile, rcReading, rcBuffer, rcInsufficient);

    BAMFileAdvance(self, 4 + rhs->datasize);
    return isgood ? 0 : RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid);
}

static
unsigned BAMAlignmentSizeFromData(unsigned const datasize, void const *data)
{
    BAMAlignment temp;
    
    BAMAlignmentInit(&temp, sizeof(temp), datasize, data);
    
    return BAMAlignmentSize(temp.numExtra);
}

static bool BAMAlignmentIsEmpty(BAMAlignment const *const self)
{
    if (getReadNameLength(self) == 0)
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
rc_t BAMFileReadCopy(BAMFile *const self, BAMAlignment const *rslt[], bool const log)
{
    void *storage;
    void const *data;
    unsigned datasize;
    rc_t rc;
    
    rslt[0] = NULL;
    {
        int32_t i32;

        rc = BAMFileReadI32(self, &i32);
        if ( rc != 0 )
        {
            if ( GetRCObject( rc ) == (enum RCObject)rcData && GetRCState( rc ) == rcInsufficient )
            {
                self->eof = true;
                rc = RC( rcAlign, rcFile, rcReading, rcRow, rcNotFound );
            }
            return rc;
        }
        if (i32 <= 0)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        
        datasize = i32;
    }
    if (BAMFileMaxPeek(self) < datasize) {
        data = storage = malloc(datasize);
        if (storage == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
        
        rc = BAMFileReadn(self, datasize, storage);
    }
    else {
        storage = NULL;
        data = (bam_alignment *)&self->buffer[self->bufCurrent];
        
        BAMFileAdvance(self, datasize);
    }
    if (rc == 0) {
        unsigned const rsltsize = BAMAlignmentSizeFromData(datasize, data);
        BAMAlignment *const y = malloc(rsltsize);

        if (y) {
            if ((log ? BAMAlignmentInitLog : BAMAlignmentInit)(y, rsltsize, datasize, data)) {
                if (storage == NULL)
                    self->bufLocker = y;
                else
                    y->storage = storage;

                y->parent = self;
                KRefcountInit(&y->refcount, 1, "BAMAlignment", "ReadCopy", "");
                BAMFileAddRef(self);
                rslt[0] = y;

                if (BAMAlignmentIsEmpty(y))
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

static
rc_t BAMFileBreakLock(BAMFile *const self)
{
    if (self->bufLocker != NULL) {
        if (self->bufLocker->storage == NULL)
            self->bufLocker->storage = malloc(self->bufLocker->datasize);
        if (self->bufLocker->storage == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
        
        memcpy(self->bufLocker->storage, self->bufLocker->data, self->bufLocker->datasize);
        self->bufLocker->data = (bam_alignment *)&self->bufLocker->storage[0];
        self->bufLocker = NULL;
    }
    return 0;
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
    static char const *Fmt[] = {
        ",%f%n",
        ",%i%n"
    };
    char const *const fmt = Fmt[isFloat ? 0 : 1] + (isArray ? 0 : 1);
    union { int i; float f; } x;
    int n = 0;

    x.i = 0;
    if (sscanf(src, fmt, &x, &n) != 1)
        return -1;
    SAM2BAM_ConvertInt(dst, x.i);
    return n;
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
                uint8_t *scratch = (void *)(src + insize);
                int const subtype = src[5] == 'f' ? 'f' : 'i';
                unsigned i;

                dst[3] = subtype;
                for (i = 6; i < insize; ) {
                    if ((void const *)&scratch[4] >= endp)
                        return -2;
                    {
                    int const n = SAM2BAM_ScanValue(scratch, src + 5, subtype == 'f', true);
                    if (n < 0)
                        return -4;
                    i += n;
                    }
                    scratch += 4;
                }
                {
                    unsigned const written = scratch - (uint8_t const *)(src + insize);
                    memmove(dst + 4, src + insize, written);
                    return written + 5;
                }
            }
        }
    }
}

static rc_t BAMFileReadSAM(BAMFile *const self, BAMAlignment const **const rslt)
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
    char *scratch;
    unsigned i = 0;
    int n = 0;
    
    memset(raw, 0, sizeof(*raw));
    memset(&temp, 0, sizeof(temp));
    scratch = temp.QNAME = &raw->read_name[0];
    
    for ( ; ; ) {
        int const ch = SAMFileRead1(&self->file.sam);
        if (ch < 0) {
            rc_t const rc = SAMFileLastError(&self->file.sam);
            return (i == 0 && field == 1 && (rc == 0 || GetRCState(rc) == rcInsufficient)) ? RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound) : rc;
        }
        if ((void const *)&scratch[i] >= endp)
            return RC(rcAlign, rcFile, rcReading, rcBuffer, rcInsufficient);

        if (!(ch == '\t' || ch == '\n')) {
            if (field != 0)
                scratch[i++] = ch;
            continue;
        }
        scratch[i] = '\0';

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
                break;
            case 2:
                if (sscanf(scratch, "%i%n", &temp.FLAG, &n) != 1 || n != i) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing FLAG");
                    field = 0;
                }
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
                break;
            case 4:
                if (sscanf(scratch, "%i%n", &temp.POS, &n) != 1 || n != i) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing POS");
                    field = 0;
                }
                break;
            case 5:
                if (sscanf(scratch, "%i%n", &temp.MAPQ, &n) != 1 || n != i) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing MAPQ");
                    field = 0;
                }
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
                break;
            case 8:
                if (sscanf(scratch, "%i%n", &temp.PNEXT, &n) != 1 || n != i) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing PNEXT");
                    field = 0;
                }
                break;
            case 9:
                if (sscanf(scratch, "%i%n", &temp.TLEN, &n) != 1 || n != i) {
                    LOGERR(klogErr, RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid), "SAM Record error parsing TLEN");
                    field = 0;
                }
                break;
            case 10:
                if (i == 1 && scratch[0] == '*')
                    temp.readlen = 0;
                else {
                    temp.readlen = i;
                    scratch += (i + 1) >> 1;
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
        unsigned const rsltsize = BAMAlignmentSizeFromData(datasize, self->buffer);
        BAMAlignment *const y = malloc(rsltsize);
    
        if (y == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);

        if (BAMAlignmentInitLog(y, rsltsize, datasize, self->buffer)) {
            y->parent = self;
            KRefcountInit(&y->refcount, 1, "BAMAlignment", "ReadSAM", "");
            BAMFileAddRef(self);
            rslt[0] = y;
        
            if (BAMAlignmentIsEmpty(y))
                return RC(rcAlign, rcFile, rcReading, rcRow, rcEmpty);
            return 0;
        }
        free(y);
    }
    return RC(rcAlign, rcFile, rcReading, rcRow, rcInvalid);
}

LIB_EXPORT rc_t CC BAMFileRead2(const BAMFile *cself, const BAMAlignment **rhs)
{
    BAMFile *const self = (BAMFile *)cself;
    unsigned actsize = 0;
    rc_t rc;
    
    if (self == NULL || rhs == NULL)
        return RC(rcAlign, rcFile, rcReading, rcParam, rcNull);
    
    *rhs = NULL;
    
    if (self->bufCurrent >= self->bufSize && self->eof)
        return RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound);

    if (self->isSAM) return BAMFileReadSAM(self, rhs);

    rc = BAMFileBreakLock(self);
    if (rc)
        return rc;

    if (self->nocopy_size == 0) {
        size_t const size = 4096u;
        void *const temp = malloc(size);

        if (temp == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);

        self->nocopy = temp;
        self->nocopy_size = size;
    }

AGAIN:
    rc = BAMFileReadNoCopy(self, &actsize, self->nocopy, (unsigned)self->nocopy_size);
    if (rc == 0) {
        *rhs = self->nocopy;
        if (BAMAlignmentIsEmpty(self->nocopy)) {
            rc = RC(rcAlign, rcFile, rcReading, rcRow, rcEmpty);
            LOGERR(klogWarn, rc, "BAM Record contains no alignment or sequence data");
        }
    }
    else if ( GetRCObject( rc ) == (enum RCObject)rcBuffer && GetRCState( rc ) == rcInsufficient )
    {
        unsigned const size = (actsize + 4095u) & ~4095u;
        void *const temp = realloc(self->nocopy, size);

        if (temp == NULL)
            return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
        
        self->nocopy = temp;
        self->nocopy_size = size;

        goto AGAIN;
    }
    else if ( GetRCObject( rc ) == (enum RCObject)rcBuffer && GetRCState( rc ) == rcNotAvailable )
    {
        rc = BAMFileReadCopy( self, rhs, true );
    }
    else if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcInvalid) {
        BAMAlignmentLogParseError(self->nocopy);
    }
    return rc;
}

LIB_EXPORT rc_t CC BAMFileRead(const BAMFile *cself, const BAMAlignment **rhs)
{
    BAMFile *const self = (BAMFile *)cself;
    
    if (self == NULL || rhs == NULL)
        return RC(rcAlign, rcFile, rcReading, rcParam, rcNull);
    
    *rhs = NULL;
    
    if (self->bufCurrent >= self->bufSize && self->eof)
        return RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound);
    else {
        rc_t const rc = BAMFileBreakLock(self);
        if (rc)
            return rc;
    }
    return BAMFileReadCopy(self, rhs, false);
}

/* MARK: BAM File header info accessor */

LIB_EXPORT rc_t CC BAMFileGetRefSeqById(const BAMFile *cself, int32_t id, const BAMRefSeq **rhs)
{
    *rhs = NULL;
    if (id >= 0 && id < cself->refSeqs)
        *rhs = &cself->refSeq[id];
    return 0;
}

LIB_EXPORT rc_t CC BAMFileGetReadGroupByName(const BAMFile *cself, const char *name, const BAMReadGroup **rhs)
{
    BAMReadGroup rg;
    
    *rhs = NULL;

    rg.name = name;
    if (rg.name != NULL)
        *rhs = kbsearch(&rg, cself->readGroup, cself->readGroups, sizeof(rg), comp_ReadGroup, NULL);

    return 0;
}

LIB_EXPORT rc_t CC BAMFileGetRefSeqCount(const BAMFile *cself, unsigned *rhs)
{
    *rhs = cself->refSeqs;
    return 0;
}

LIB_EXPORT rc_t CC BAMFileGetRefSeq(const BAMFile *cself, unsigned i, const BAMRefSeq **rhs)
{
    *rhs = NULL;
    if (i < cself->refSeqs)
        *rhs = &cself->refSeq[i];
    return 0;
}

LIB_EXPORT rc_t CC BAMFileGetReadGroupCount(const BAMFile *cself, unsigned *rhs)
{
    *rhs = cself->readGroups;
    return 0;
}

LIB_EXPORT rc_t CC BAMFileGetReadGroup(const BAMFile *cself, unsigned i, const BAMReadGroup **rhs)
{
    *rhs = NULL;
    if (i < cself->readGroups)
        *rhs = &cself->readGroup[i];
    return 0;
}

LIB_EXPORT rc_t CC BAMFileGetHeaderText(BAMFile const *cself, char const **header, size_t *header_len)
{
    *header = cself->header;
    *header_len = *header ? string_size( *header ) : 0;
    return 0;
}

/* MARK: BAM Alignment destructor */

static rc_t BAMAlignmentWhack(BAMAlignment *self)
{
    if (self->parent->bufLocker == self)
        self->parent->bufLocker = NULL;
    if (self != self->parent->nocopy) {
        BAMFileRelease(self->parent);
        free(self->storage);
        free(self);
    }
    return 0;
}

/* MARK: BAM Alignment ref-counting */

LIB_EXPORT rc_t CC BAMAlignmentAddRef(const BAMAlignment *cself)
{
    if (cself != NULL)
        KRefcountAdd(&cself->refcount, "BAMAlignment");
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentRelease(const BAMAlignment *cself)
{
    if (cself && KRefcountDrop(&cself->refcount, "BAMAlignment") == krefWhack)
        BAMAlignmentWhack((BAMAlignment *)cself);

    return 0;
}

#if 0
LIB_EXPORT uint16_t CC BAMAlignmentIffyFields(const BAMAlignment *self)
{
}

LIB_EXPORT uint16_t CC BAMAlignmentBadFields(const BAMAlignment *self)
{
}
#endif

/* MARK: BAM Alignment accessors */

static uint32_t BAMAlignmentGetCigarElement(const BAMAlignment *self, unsigned i)
{
    return LE2HUI32(&((uint8_t const *)getCigarBase(self))[i * 4]);
}

LIB_EXPORT rc_t CC BAMAlignmentGetRefSeqId(const BAMAlignment *cself, int32_t *rhs)
{
    *rhs = getRefSeqId(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetPosition(const BAMAlignment *cself, int64_t *rhs)
{
    *rhs = getPosition(cself);
    return 0;
}

LIB_EXPORT bool CC BAMAlignmentIsMapped(const BAMAlignment *cself)
{
    if (((getFlags(cself) & BAMFlags_SelfIsUnmapped) == 0) && getRefSeqId(cself) >= 0 && getPosition(cself) >= 0)
        return true;
    return false;
}

/* static bool BAMAlignmentIsMateMapped(const BAMAlignment *cself)
{
    if (((getFlags(cself) & BAMFlags_MateIsUnmapped) == 0) && getMateRefSeqId(cself) >= 0 && getMatePos(cself) >= 0)
        return true;
    return false;
} */

LIB_EXPORT rc_t CC BAMAlignmentGetAlignmentDetail(
                                                  const BAMAlignment *self,
                                                  BAMAlignmentDetail *rslt, uint32_t count, uint32_t *actual,
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
        uint32_t len = BAMAlignmentGetCigarElement(self, i);
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
unsigned ReferenceLengthFromCIGAR(const BAMAlignment *self)
{
    unsigned i;
    unsigned n = getCigarCount(self);
    unsigned y;
    
    for (i = 0, y = 0; i != n; ++i) {
        uint32_t const len = BAMAlignmentGetCigarElement(self, i);
        
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

static unsigned SequenceLengthFromCIGAR(const BAMAlignment *self)
{
    unsigned i;
    unsigned n = getCigarCount(self);
    unsigned y;
    
    for (i = 0, y = 0; i != n; ++i) {
        uint32_t const len = BAMAlignmentGetCigarElement(self, i);
        
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

LIB_EXPORT rc_t CC BAMAlignmentGetPosition2(const BAMAlignment *cself, int64_t *rhs, uint32_t *length)
{
    *rhs = getPosition(cself);
    if (*rhs >= 0)
        *length = ReferenceLengthFromCIGAR(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetReadGroupName(const BAMAlignment *cself, const char **rhs)
{
    *rhs = get_RG(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetReadName(const BAMAlignment *cself, const char **rhs)
{
    *rhs = getReadName(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetReadName2(const BAMAlignment *cself, const char **rhs, size_t *length)
{
    *length = getReadNameLength(cself) - 1;
    *rhs = getReadName(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetReadName3(const BAMAlignment *cself, const char **rhs, size_t *length)
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

LIB_EXPORT rc_t CC BAMAlignmentGetFlags(const BAMAlignment *cself, uint16_t *rhs)
{
    *rhs = getFlags(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetMapQuality(const BAMAlignment *cself, uint8_t *rhs)
{
    *rhs = getMapQual(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetCigarCount(const BAMAlignment *cself, unsigned *rhs)
{
    *rhs = getCigarCount(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetRawCigar(const BAMAlignment *cself, uint32_t const *rslt[], uint32_t *length)
{
    *rslt = getCigarBase(cself);
    *length = getCigarCount(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetCigar(const BAMAlignment *cself, uint32_t i, BAMCigarType *type, uint32_t *length)
{
    uint32_t x;
    
    if (i >= getCigarCount(cself))
        return RC(rcAlign, rcFile, rcReading, rcParam, rcInvalid);

    x = BAMAlignmentGetCigarElement(cself, i);
    *type = (BAMCigarType)(cigarChars[x & 0x0F]);
    *length = x >> 4;
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetReadLength(const BAMAlignment *cself, uint32_t *rhs)
{
    *rhs = getReadLen(cself);
    return 0;
}

static int get1Base(BAMAlignment const *const self, unsigned const i)
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

static int get1Qual(BAMAlignment const *const self, unsigned const i)
{
    uint8_t const *const src = &self->data->raw[self->qual];
    
    return src[i];
}

LIB_EXPORT rc_t CC BAMAlignmentGetSequence2(const BAMAlignment *cself, char *rhs, uint32_t start, uint32_t stop)
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

LIB_EXPORT rc_t CC BAMAlignmentGetSequence(const BAMAlignment *cself, char *rhs)
{
    return BAMAlignmentGetSequence2(cself, rhs, 0, 0);
}

LIB_EXPORT bool CC BAMAlignmentHasColorSpace(BAMAlignment const *cself)
{
    return get_CS(cself) != NULL;
}

LIB_EXPORT rc_t CC BAMAlignmentGetCSKey(BAMAlignment const *cself, char rhs[1])
{
    char const *const vCS = get_CS(cself);
    
    if (vCS)
        rhs[0] = vCS[0];
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetCSSeqLen(BAMAlignment const *cself, uint32_t *rhs)
{
    struct offset_size_s const *const vCS = get_CS_info(cself);
    
    *rhs = vCS ? vCS->size - 5 : 0;
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetCSSequence(BAMAlignment const *cself, char rhs[], uint32_t seqlen)
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

LIB_EXPORT rc_t CC BAMAlignmentGetQuality(const BAMAlignment *cself, const uint8_t **rhs)
{
    *rhs = &cself->data->raw[cself->qual];
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetQuality2(BAMAlignment const *cself, uint8_t const **rhs, uint8_t *offset)
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

LIB_EXPORT rc_t CC BAMAlignmentGetCSQuality(BAMAlignment const *cself, uint8_t const **rhs, uint8_t *offset)
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

LIB_EXPORT rc_t CC BAMAlignmentGetMateRefSeqId(const BAMAlignment *cself, int32_t *rhs)
{
    *rhs = getMateRefSeqId(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetMatePosition(const BAMAlignment *cself, int64_t *rhs)
{
    *rhs = getMatePos(cself);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentGetInsertSize(const BAMAlignment *cself, int64_t *rhs)
{
    *rhs = getInsertSize(cself);
    return 0;
}

static int FormatOptData(BAMAlignment const *const self,
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
                                offset += 4;

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

static rc_t FormatSAM(BAMAlignment const *self,
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
        BAMAlignmentGetSequence(self, &buffer[cur]);
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
static rc_t FormatSAMBuffer(BAMAlignment const *self,
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

    memcpy(buffer, scratch, actsize);
    return 0;
}

LIB_EXPORT rc_t CC BAMAlignmentFormatSAM(BAMAlignment const *self,
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

static bool i_OptDataForEach(BAMAlignment const *cself, void *Ctx, char const tag[2], BAMOptDataValueType type, unsigned count, void const *value, unsigned size)
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
    
    memcpy(ctx->val->u.u8, value, size * count);
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

LIB_EXPORT rc_t CC BAMAlignmentOptDataForEach(const BAMAlignment *cself, void *user_ctx, BAMOptionalDataFunction f)
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

LIB_EXPORT bool CC BAMAlignmentHasCGData(BAMAlignment const *self)
{
    return get_CG_GC_info(self) && get_CG_GS_info(self) && get_CG_GQ_info(self);
}

LIB_EXPORT rc_t CC BAMAlignmentCGReadLength(BAMAlignment const *self, uint32_t *readlen)
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
    return RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

static unsigned BAMAlignmentParseCGTag(BAMAlignment const *self, size_t const max_cg_segs, unsigned cg_segs[/* max_cg_segs */])
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
rc_t ExtractInt32(BAMAlignment const *self, int32_t *result,
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
        return RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
    }
    if (INT32_MIN <= y && y <= INT32_MAX) {
        *result = (int32_t)y;
        return 0;
    }
    return RC(rcAlign, rcRow, rcReading, rcData, rcInvalid);
}

LIB_EXPORT
rc_t CC BAMAlignmentGetCGAlignGroup(BAMAlignment const *self,
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
    return RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

LIB_EXPORT
rc_t CC BAMAlignmentGetCGSeqQual(BAMAlignment const *self,
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
    return RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}


static unsigned splice(uint32_t cigar[], unsigned n, unsigned at, unsigned out, unsigned in, uint32_t const new_values[/* in */])
{
    assert(at + out <= n);
    memmove(&cigar[at + in], &cigar[at + out], (n - at - out) * 4);
    if (in)
        memcpy(&cigar[at], new_values, in * 4);
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

static unsigned canonicalize(uint32_t cigar[], unsigned n)
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

static unsigned GetCGCigar(BAMAlignment const *self, unsigned const N, uint32_t cigar[/* N */])
{
    unsigned i;
    unsigned S;
    unsigned n = getCigarCount(self);
    unsigned seg[64];
    unsigned const segs = BAMAlignmentParseCGTag(self, sizeof(seg)/sizeof(seg[0]), seg);
    unsigned const gaps = (segs - 1) >> 1;
    
    if (2 * gaps + 1 != segs)
        return RC(rcAlign, rcRow, rcReading, rcData, rcUnexpected);
    
    if (N < n + 2 * gaps)
        return RC(rcAlign, rcRow, rcReading, rcBuffer, rcInsufficient);
    
    memcpy(cigar, getCigarBase(self), n * 4);

    if (n > 1)
        n = canonicalize(cigar, n); /* just in case */
    
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

LIB_EXPORT
rc_t CC BAMAlignmentGetCGCigar(BAMAlignment const *self,
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
    return RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

/* MARK: end CG stuff */

LIB_EXPORT rc_t BAMAlignmentGetTI(BAMAlignment const *self, uint64_t *ti)
{
    char const *const TI = get_XT(self);
    long long unsigned temp;
    
    if (TI && sscanf(TI, "ti|%llu", &temp) == 1) {
        *ti = (uint64_t)temp;
        return 0;
    }
    return RC(rcAlign, rcRow, rcReading, rcData, rcNotFound);
}

LIB_EXPORT rc_t BAMAlignmentGetRNAStrand(BAMAlignment const *const self, uint8_t *const rslt)
{
    if (rslt) {
        uint8_t const *const XS = get_XS(self);
        
	    *rslt = XS ? XS[0] : ' ';
    }
    return 0;
}

/* MARK: BAMIndex stuff */

static uint64_t get_pos(uint8_t const buf[])
{
    return LE2HUI64(buf);
}

#define MAX_BIN 37449
static uint16_t bin2ival(uint16_t bin)
{
    if (bin < 1)
        return 0; /* (bin - 0) << 15; */
    
    if (bin < 9)
        return (bin - 1) << 12;
    
    if (bin < 73)
        return (bin - 9) << 9;
    
    if (bin < 585)
        return (bin - 73) << 6;
    
    if (bin < 4681)
        return (bin - 585) << 3;
    
    if (bin < 37449)
        return (bin - 4681) << 0;
    
    return 0;
}

static uint16_t bin_ival_count(uint16_t bin)
{
    if (bin < 1)
        return 1 << 15;
    
    if (bin < 9)
        return 1 << 12;
    
    if (bin < 73)
        return 1 << 9;
    
    if (bin < 585)
        return 1 << 6;
    
    if (bin < 4681)
        return 1 << 3;
    
    if (bin < 37449)
        return 1;
    
    return 0;
}

enum BAMIndexStructureTypes {
    bai_StartStopPairs,
    bai_16kIntervals
};

typedef rc_t (*WalkIndexStructureCallBack)(const uint8_t data[], size_t dlen,
                                           unsigned refNo,
                                           unsigned refs,
                                           enum BAMIndexStructureTypes type,
                                           unsigned binNo,
                                           unsigned bins,
                                           unsigned elements,
                                           void *ctx);

static
rc_t WalkIndexStructure(uint8_t const buf[], size_t const blen,
                        WalkIndexStructureCallBack func,
                        void *ctx
                        )
{
    unsigned cp = 0;
    int32_t nrefs;
    unsigned i;
    rc_t rc;
    
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("Index data length: %u", blen));

    if (cp + 4 > blen)
        return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);
    
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("Index signature: '%c%c%c%u'", buf[cp+0], buf[cp+1], buf[cp+2], buf[cp+3]));
    if (memcmp(buf + cp, "BAI\1", 4) != 0)
        return RC(rcAlign, rcIndex, rcReading, rcFormat, rcUnknown);
    
    cp += 4;
    if (cp + 4 > blen)
        return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);

    nrefs = LE2HI32(buf + cp); cp += 4;
    DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("Index reference count: %i", nrefs));
    
    if (nrefs == 0)
        return RC(rcAlign, rcIndex, rcReading, rcData, rcEmpty);
    
    for (i = 0; i < nrefs; ++i) {
        int32_t bins;
        int32_t chunks;
        int32_t intervals;
        unsigned di;
        
        DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("Index reference %u: starts at %u", i, cp));
        if (cp + 4 > blen)
            return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);
        
        bins = LE2HI32(buf + cp); cp += 4;
        DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("Index reference %u: %i bins", i, nrefs));

        for (di = 0; di < bins; ++di) {
            uint32_t binNo;
            
            if (cp + 8 > blen)
                return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);

            binNo = LE2HUI32(buf + cp); cp += 4;
            chunks = LE2HI32(buf + cp); cp += 4;
            DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("Index reference %u, bin %u: %i chunks", i, binNo, chunks));
            
            if (cp + 16 * chunks > blen)
                return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);
            rc = func(&buf[cp], 16 * chunks, i, nrefs, bai_StartStopPairs, binNo, bins, chunks, ctx);
            if (rc)
                return rc;
            cp += 16 * chunks;
        }
        if (cp + 4 > blen)
            return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);

        intervals = LE2HI32(buf + cp); cp += 4;
        DBGMSG(DBG_ALIGN, DBG_FLAG(DBG_ALIGN_BAM), ("Index reference %u: %i intervals", i, intervals));

        if (cp + 8 * intervals > blen)
            return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);
        rc = func(&buf[cp], 8 * intervals, i, nrefs, bai_16kIntervals, ~(unsigned)0, bins, intervals, ctx);
        if (rc)
            return rc;
        cp += 8 * intervals;
    }
    if (cp > blen)
        return RC(rcAlign, rcIndex, rcReading, rcData, rcInsufficient);
    return 0;
}

struct LoadIndex1_s {
    const BAMFile *self;
    int refNo;
    unsigned refs;
    unsigned intervals;
    unsigned total_interval_count;
};

static
rc_t LoadIndex1(const uint8_t data[], size_t dlen, unsigned refNo,
                unsigned refs, enum BAMIndexStructureTypes type,
                unsigned binNo, unsigned bins,
                unsigned elements, void *Ctx)
{
    struct LoadIndex1_s *ctx = (struct LoadIndex1_s *)Ctx;
    
    ctx->refs = refs;
    if (refNo != ctx->refNo) {
        ctx->total_interval_count += ctx->intervals;
        ctx->intervals = 0;
        ctx->refNo = refNo;
    }
    if (elements != 0) {
        if (refNo > ctx->self->refSeqs)
            return RC(rcAlign, rcIndex, rcReading, rcData, rcInvalid);
        ctx->intervals = (ctx->self->refSeq[refNo].length + 16383) >> 14;
        if (type == bai_16kIntervals && elements > ctx->intervals)
            return RC(rcAlign, rcIndex, rcReading, rcData, rcExcessive);
    }
    return 0;
}

struct LoadIndex2_s {
    const BAMFile *self;
    BAMFilePosition **refSeq;
    BAMFilePosition *cur;
#if _DEBUGGING
    BAMFilePosition *end;
#endif
    const uint8_t *base;
    unsigned bins[MAX_BIN + 1];
    bool hasData;
};

static
rc_t LoadIndex2a(const uint8_t data[], size_t dlen, unsigned refNo,
                 unsigned refs, enum BAMIndexStructureTypes type,
                 unsigned binNo, unsigned bins,
                 unsigned elements, struct LoadIndex2_s *ctx)
{
    const unsigned max_ival = (ctx->self->refSeq[refNo].length + 16383) >> 14;
    unsigned i;
    unsigned cp;
    unsigned k;
    uint32_t chunk_count;
    uint64_t minOffset[1u << 15];

    assert(ctx->refSeq[refNo] == NULL);
    ctx->refSeq[refNo] = ctx->cur;
    ctx->cur += max_ival;
    
#if _DEBUGGING
    assert(refNo < ctx->self->refSeqs);
    assert(ctx->cur <= ctx->end);
    assert(elements <= max_ival);
#endif
    /* get the positions of the first records in the 16kbp intervals */
    for (cp = i = 0; i != elements; ++i, cp += 8)
        ctx->refSeq[refNo][i] = get_pos(&data[cp]);
    /* get the positions of the first records in the 16kbp bins */
    for (i = MAX_BIN; i != 0; ) {
        const unsigned ival = bin2ival(--i);
        const unsigned n_ival = bin_ival_count(i);
        uint64_t found;
        
        cp = ctx->bins[i];
        if (cp == 0)
            continue;
        if (n_ival > 1)
            break;
        
        assert(i == LE2HI32(ctx->base + cp));
        cp += 4;
        chunk_count = LE2HI32(ctx->base + cp); cp += 4;
        found = ctx->refSeq[refNo][ival];
        for (k = 0; k < chunk_count; ++k) {
            const uint64_t start = get_pos(ctx->base + cp);
            
            cp += 16;
            if (found == 0 || start < found)
                found = start;
        }
        ctx->refSeq[refNo][ival] = found;
    }
    /* The interval list now contains the offsets to the first alignment
     * that starts at or after the interval's starting position.
     * An interval's starting position is 16kpb * interval number.
     *
     * We will now use the information from the bigger bins to find the
     * offsets of the first chunk of alignments that ends after an
     * interval's first alignment.
     */
    memset(minOffset, 0, sizeof(minOffset));
    for (i = 0; i != MAX_BIN; ++i) {
        const unsigned ival = bin2ival(i);
        unsigned n_ival = bin_ival_count(i);
        
        cp = ctx->bins[i];
        if (cp == 0)
            continue;
        if (n_ival <= 1)
            break;
        
        if (ival + n_ival > max_ival)
            n_ival = max_ival - ival;
        
        chunk_count = LE2HI32(ctx->base + cp + 4); cp += 8;
        for (k = 0; k < chunk_count; ++k) {
            const uint64_t start = get_pos(ctx->base + cp);
            const uint64_t end   = get_pos(ctx->base + cp + 8);
            unsigned l;
            
            cp += 16;
            for (l = 0; l != n_ival; ++l) {
                if (start < ctx->refSeq[refNo][ival + l] &&
                    ctx->refSeq[refNo][ival + l] <= end &&
                    (start < minOffset[ival + l] ||
                     minOffset[ival + l] == 0
                     )
                    )
                {
                    minOffset[ival + l] = start;
                }
            }
        }
    }
    /* update the intervals to the new earlier offsets if any */
    for (i = 0; i != max_ival; ++i) {
        if (minOffset[i] != 0)
            ctx->refSeq[refNo][i] = minOffset[i];
    }
    memset(ctx->bins, 0, sizeof(ctx->bins));
    ctx->hasData = false;
    return 0;
}

static
rc_t LoadIndex2(const uint8_t data[], size_t dlen, unsigned refNo,
                unsigned refs, enum BAMIndexStructureTypes type,
                unsigned binNo, unsigned bins,
                unsigned elements, void *Ctx)
{
    struct LoadIndex2_s *ctx = (struct LoadIndex2_s *)Ctx;
    
    if (type == bai_StartStopPairs) {
        if (binNo < MAX_BIN && elements != 0) {
            ctx->bins[binNo] = &data[-8] - ctx->base;
            ctx->hasData = true;
        }
    }
    else if (elements != 0 || ctx->hasData)
        return LoadIndex2a(data, dlen, refNo, refs, type, binNo, bins,
                           elements, (struct LoadIndex2_s *)Ctx);
    return 0;
}    

static
rc_t LoadIndex(BAMFile *self, const uint8_t buf[], size_t blen)
{
    BAMIndex *idx;
    rc_t rc;
    struct LoadIndex1_s loadIndex1ctx;
    unsigned const posArray = ((uintptr_t)&((const BAMFilePosition **)(NULL))[self->refSeqs]) / sizeof(BAMFilePosition *);

    memset(&loadIndex1ctx, 0, sizeof(loadIndex1ctx));
    loadIndex1ctx.refNo = -1;
    loadIndex1ctx.self = self;
    
    rc = WalkIndexStructure(buf, blen, LoadIndex1, &loadIndex1ctx);
    if (rc == 0) {
        loadIndex1ctx.total_interval_count += loadIndex1ctx.intervals;
        idx = calloc(1, posArray * sizeof(BAMFilePosition *) +
                     loadIndex1ctx.total_interval_count * sizeof(BAMFilePosition));
        if (idx == NULL)
            rc = RC(rcAlign, rcIndex, rcReading, rcMemory, rcExhausted);
        else {
            struct LoadIndex2_s *loadIndex2ctx;
            
            if (self->ndx)
                BAMIndexWhack(self->ndx);
            self->ndx = idx;
            
            loadIndex2ctx = malloc(sizeof(*loadIndex2ctx));
            if (loadIndex2ctx == NULL) {
                rc = RC(rcAlign, rcIndex, rcReading, rcMemory, rcExhausted);
                free(idx);
                self->ndx = NULL;
            }
            else {
                memset(loadIndex2ctx->bins, 0, sizeof(loadIndex2ctx->bins));
                loadIndex2ctx->self = self;
                loadIndex2ctx->refSeq = &idx->refSeq[0];
                loadIndex2ctx->base = buf;
                loadIndex2ctx->hasData = false;
                loadIndex2ctx->cur = (BAMFilePosition *)&idx->refSeq[posArray];
#if _DEBUGGING
                loadIndex2ctx->end = loadIndex2ctx->cur + loadIndex1ctx.total_interval_count;
#endif
                
                WalkIndexStructure(buf, blen, LoadIndex2, loadIndex2ctx);
                free(loadIndex2ctx);
            }
        }
    }
    return rc;
}

static
rc_t BAMFileOpenIndexInternal(const BAMFile *self, const char *path)
{
    const KFile *kf;
    rc_t rc;
    size_t fsize;
    uint8_t *buf;
    KDirectory *dir;
    
    rc = KDirectoryNativeDir(&dir);
    if (rc) return rc;
    rc = KDirectoryOpenFileRead(dir, &kf, "%s", path);
    KDirectoryRelease(dir);
    if (rc) return rc;
    {
        uint64_t u64;

        rc = KFileSize(kf, &u64);
        if (sizeof(size_t) < sizeof(u64) && (size_t)u64 != u64) {
            KFileRelease(kf);
            return RC(rcAlign, rcIndex, rcReading, rcData, rcExcessive);
        }
        fsize = u64;
    }
    if (rc == 0) {
        buf = malloc(fsize);
        if (buf != NULL) {
            size_t nread;
            
            rc = KFileRead(kf, 0, buf, fsize, &nread);
            KFileRelease(kf);
            if (rc == 0) {
                if (nread == fsize) {
                    rc = LoadIndex((BAMFile *)self, buf, nread);
                    free(buf);
                    return rc;
                }
                rc = RC(rcAlign, rcIndex, rcReading, rcData, rcInvalid);
            }
            free(buf);
        }
        else
            rc = RC(rcAlign, rcIndex, rcReading, rcMemory, rcExhausted);
    }
    return rc;
}

LIB_EXPORT rc_t CC BAMFileOpenIndex(const BAMFile *self, const char *path)
{
    return BAMFileOpenIndexInternal(self, path);
}

LIB_EXPORT rc_t CC BAMFileOpenIndexWithVPath(const BAMFile *self, const VPath *kpath)
{
    char path[4096];
    size_t nread;
    rc_t rc = VPathReadPath(kpath, path, sizeof(path), &nread);

    if (rc == 0) {
        path[nread] = '\0';
        rc = BAMFileOpenIndexInternal(self, path);
    }
    return rc;
}

LIB_EXPORT bool CC BAMFileIsIndexed(const BAMFile *self)
{
	if (self && self->ndx)
		return true;
	return false;
}

LIB_EXPORT bool CC BAMFileIndexHasRefSeqId(const BAMFile *self, uint32_t refSeqId)
{
	if (self && self->ndx && self->ndx->refSeq[refSeqId])
		return true;
	return false;
}

static void BAMAlignmentAlignInfo(BAMAlignment *const self,
                                  int32_t ref[],
                                  int32_t beg[],
                                  int32_t end[])
{
    (void)BAMAlignmentSetOffsets(self);
    
    ref[0] = getRefSeqId(self);
    end[0] = (beg[0] = getPosition(self)) + ReferenceLengthFromCIGAR(self);
}

static
rc_t BAMFileGetAlignPosAtFilePos(BAMFile *const self,
                                 BAMFilePosition const *const fpos,
                                 int32_t ref[],
                                 int32_t beg[],
                                 int32_t end[])
{
    rc_t rc = BAMFileSetPosition(self, fpos);
    
    if (rc == 0) {
        BAMAlignment x;
        int32_t i32;
        
        rc = BAMFileReadI32(self, &i32); if (rc) return rc;
        if (i32 <= 0)
            return RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);

        memset(&x, 0, sizeof(x));
        x.datasize = i32;
        if (x.datasize <= BAMFileMaxPeek(self)) {
            x.data = (void *)&self->buffer[self->bufCurrent];
            BAMFileAdvance(self, x.datasize);

            BAMAlignmentAlignInfo(&x, ref, beg, end);
        }
        else {
            void *const temp = malloc(x.datasize);
            
            if (temp) {
                x.data = temp;
                
                rc = BAMFileReadn(self, x.datasize, temp);
                if (rc == 0)
                    BAMAlignmentAlignInfo(&x, ref, beg, end);
                
                free(temp);
            }
            else
                rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
        }
    }
    return rc;
}

LIB_EXPORT rc_t CC BAMFileSeek(const BAMFile *self, uint32_t refSeqId, uint64_t alignStart, uint64_t alignEnd)
{
    BAMFilePosition rpos = 0;
    rc_t rc;
    int32_t prev_alignPos;
    int32_t alignPos;
    int32_t alignEndPos;
    int32_t refSeq;
    
    if (self->ndx == NULL)
        return RC(rcAlign, rcFile, rcPositioning, rcIndex, rcNotFound);
    if (refSeqId >= self->refSeqs)
        return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
    if (self->ndx->refSeq[refSeqId] == NULL)
        return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
    if (alignStart >= self->refSeq[refSeqId].length)
        return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
    if (alignEnd > self->refSeq[refSeqId].length)
        alignEnd = self->refSeq[refSeqId].length;
    
    {
        unsigned adjust = 0;
        uint32_t ival_start = (uint32_t)(alignStart >> 14);
        {
            uint32_t const ival_end = (uint32_t)((alignEnd + 16383) >> 14);
            
            /* find the first interval >= alignStart that has an alignment */
            while (ival_start != ival_end && (rpos = self->ndx->refSeq[refSeqId][ival_start]) == 0)
                ++ival_start;
        }
        if (rpos == 0)
            return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
        do {
            rc = BAMFileGetAlignPosAtFilePos((BAMFile *)self, &rpos, &refSeq, &alignPos, &alignEndPos);
            if (rc)
                return RC(rcAlign, rcFile, rcPositioning, rcIndex, rcInvalid);
            if (refSeq != refSeqId)
                return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
            if (alignPos <= alignEnd)
                break; /* we found the interval we were looking for */
            
            /* we over-shot */
            if (++adjust >= ival_start)
                return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
            if ((rpos = self->ndx->refSeq[refSeqId][ival_start - adjust]) == 0)
                return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
        } while (1);
    }
    prev_alignPos = alignPos;
    
    do {
        if (alignPos > alignEnd)
            return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
        
        /* if the alignment overlaps the target range then we are done */
        if (alignPos >= alignStart || alignEndPos >= alignStart)
            return BAMFileSetPosition(self, &rpos);
        
        /* start linear scan */
        BAMFileGetPosition(self, &rpos);
        rc = BAMFileGetAlignPosAtFilePos((BAMFile *)self, &rpos, &refSeq, &alignPos, &alignEndPos);
        if (rc) return rc;
        if (refSeq != refSeqId)
            return RC(rcAlign, rcFile, rcPositioning, rcData, rcNotFound);
        
        /*  indexed BAM must be sorted by position
         *  so verify that we are not out of order
         *  whether this means that the index is bad
         *  or the file is bad, likely both
         *  fix the file and regenerate the index
         */
        if (prev_alignPos > alignPos)
            return RC(rcAlign, rcFile, rcPositioning, rcIndex, rcInvalid);
        prev_alignPos = alignPos;
    } while (1);
}

static rc_t BAMIndexWhack(const BAMIndex *cself) {
    free((void *)cself);
    return 0;
}

/* MARK: BAM Validation Stuff */

static rc_t OpenVPathRead(const KFile **fp, struct VPath const *path)
{
    char buffer[4096];
    size_t blen;
    rc_t rc = VPathReadPath(path, buffer, sizeof(buffer), &blen);
    
    if (rc == 0) {
        KDirectory *dir;
        
        rc = KDirectoryNativeDir(&dir);
        if (rc == 0) {
            rc = KDirectoryOpenFileRead(dir, fp, "%.*s", (int)blen, buffer);
            KDirectoryRelease(dir);
        }
    }
    return rc;
}

static rc_t ReadVPath(void **data, size_t *dsize, struct VPath const *path)
{
    const KFile *fp;
    rc_t rc = OpenVPathRead(&fp, path);
    
    if (rc == 0) {
        uint8_t *buff;
        uint64_t fsz;
        size_t bsz = 0;
        
        rc = KFileSize(fp, &fsz);
        if (rc == 0) {
            if ((size_t)fsz != fsz)
                return RC(rcAlign, rcFile, rcReading, rcFile, rcTooBig);
            buff = malloc(fsz);
            if (buff == NULL)
                return RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
            do {
                size_t nread;
                
                rc = KFileRead(fp, 0, buff + bsz, fsz - bsz, &nread);
                if (rc)
                    break;
                bsz += nread;
            } while (bsz < (size_t)fsz);
            if (rc == 0) {
                *data = buff;
                *dsize = bsz;
                return 0;
            }
            free(buff);
        }
    }
    return rc;
}

static rc_t VPath2BGZF(BGZFile *bgzf, struct VPath const *path)
{
#if 0
    const KFile *fp;
    RawFile_vt dummy;
    rc_t rc = OpenVPathRead(&fp, path);

    if (rc == 0) {
        rc = BGZFileInit(bgzf, fp, &dummy);
        KFileRelease(fp);
    }
    return rc;
#else
    return -1;
#endif
}

struct index_data {
    uint64_t position;
    unsigned refNo;
    unsigned binNo;
    bool found;
};

struct buffer_data {
    uint64_t position;
    size_t size;
};

typedef struct BAMValidate_ctx_s BAMValidate_ctx_t;
struct BAMValidate_ctx_s {
    BAMValidateCallback callback;
    void *ctx;
    BAMValidateStats *stats;
    const uint8_t *bai;
    int32_t *refLen;
    struct index_data *position;
    uint8_t *buf;
    uint8_t *nxt;
    size_t bsize;
    size_t alloced;
    size_t dnext;
    uint32_t options;
    uint32_t lastRefId;
    uint32_t lastRefPos;
    unsigned npositions;
    unsigned mpositions;
    unsigned nrefs;
    bool cancelled;
};

static
rc_t IndexValidateStructure(const uint8_t data[], size_t dlen,
                            unsigned refNo,
                            unsigned refs,
                            enum BAMIndexStructureTypes type,
                            unsigned binNo,
                            unsigned bins,
                            unsigned elements,
                            void *Ctx)
{
    BAMValidate_ctx_t *ctx = Ctx;
    rc_t rc = 0;
    
    ctx->stats->baiFilePosition = data - ctx->bai;
    rc = ctx->callback(ctx->ctx, 0, ctx->stats);
    if (rc)
        ctx->cancelled = true;
    return rc;
}

static int CC comp_index_data(const void *A, const void *B, void *ignored)
{
    const struct index_data *a = A;
    const struct index_data *b = B;
    
    if (a->position < b->position)
        return -1;
    else if (a->position > b->position)
        return 1;
    else
        return 0;
}

static
rc_t BAMValidateLoadIndex(const uint8_t data[], size_t dlen,
                          unsigned refNo,
                          unsigned refs,
                          enum BAMIndexStructureTypes type,
                          unsigned binNo,
                          unsigned bins,
                          unsigned elements,
                          void *Ctx)
{
    BAMValidate_ctx_t *ctx = Ctx;
    unsigned const n = type == bai_16kIntervals ? elements : elements * 2;
    unsigned i;
    unsigned j;
    
    if (type == bai_StartStopPairs && binNo >= MAX_BIN)
        return 0;
    
    if (ctx->npositions + elements > ctx->mpositions) {
        void *temp;
        
        do { ctx->mpositions <<= 1; } while (ctx->npositions + elements > ctx->mpositions);
        temp = realloc(ctx->position, ctx->mpositions * sizeof(ctx->position[0]));
        if (temp == NULL)
            return RC(rcAlign, rcIndex, rcReading, rcMemory, rcExhausted);
        ctx->position = temp;
    }
    for (j = i = 0; i != n; ++i) {
        uint64_t const pos = get_pos(&data[i * 8]);
        
        if (type == bai_StartStopPairs && (i & 1) != 0)
            continue;
        
        if (pos) {
            ctx->position[ctx->npositions + j].refNo = refNo;
            ctx->position[ctx->npositions + j].binNo = binNo;
            ctx->position[ctx->npositions + j].position = pos;
            ++j;
        }
    }
    ctx->npositions += j;
    return 0;
}

static
rc_t BAMValidateHeader(const uint8_t data[],
                       unsigned dsize,
                       unsigned *header_len,
                       unsigned *refs_start,
                       unsigned *nrefs,
                       unsigned *data_start
                       )
{
    int32_t hlen;
    int32_t refs;
    unsigned i;
    unsigned cp;
    
    if (dsize < 8)
        return RC(rcAlign, rcFile, rcValidating, rcData, rcIncomplete);
    
    if (memcmp(data, "BAM\1", 4) != 0)
        return RC(rcAlign, rcFile, rcValidating, rcFormat, rcUnrecognized);
    
    hlen = LE2HI32(&data[4]);
    if (hlen < 0)
        return RC(rcAlign, rcFile, rcValidating, rcData, rcInvalid);
    
    if (dsize < hlen + 12)
        return RC(rcAlign, rcFile, rcValidating, rcData, rcIncomplete);
    
    refs = LE2HI32(&data[hlen + 8]);
    if (refs < 0)
        return RC(rcAlign, rcFile, rcValidating, rcData, rcInvalid);
    
    for (cp = hlen + 12, i = 0; i != refs; ++i) {
        int32_t nlen;
        
        if (dsize < cp + 4)
            return RC(rcAlign, rcFile, rcValidating, rcData, rcIncomplete);
        
        nlen = LE2HI32(&data[cp]);
        if (nlen < 0)
            return RC(rcAlign, rcFile, rcValidating, rcData, rcInvalid);
        
        if (dsize < cp + nlen + 4)
            return RC(rcAlign, rcFile, rcValidating, rcData, rcIncomplete);
        
        cp += nlen + 4;
    }
    
    *nrefs = refs;
    *refs_start = 12 + (*header_len = hlen);
    *data_start = cp;
    return 0;
}

static rc_t BAMValidateIndex(struct VPath const *bampath,
                             struct VPath const *baipath,
                             BAMValidateOption options,
                             BAMValidateCallback callback,
                             void *callbackContext
                             )
{
    rc_t rc = 0;
    BGZFile bam;
    uint8_t *bai = NULL;
    size_t bai_size;
    BAMValidateStats stats;
    BAMValidate_ctx_t ctx;
    uint8_t data[2 * ZLIB_BLOCK_SIZE];
    uint32_t dsize = 0;
    uint64_t pos = 0;
    uint32_t temp;
    int32_t ref = -1;
    int32_t rpos = -1;
    
    if ((options & bvo_IndexOptions) == 0)
        return callback(callbackContext, 0, &stats);

    rc = ReadVPath((void **)&bai, &bai_size, baipath);
    if (rc)
        return rc;
    
    memset(&stats, 0, sizeof(stats));
    memset(&ctx, 0, sizeof(ctx));
    
    ctx.bai = bai;
    ctx.stats = &stats;
    ctx.options = options;
    ctx.ctx = callbackContext;
    ctx.callback = callback;
    
    if ((options & bvo_IndexOptions) == bvo_IndexStructure)
        return WalkIndexStructure(bai, bai_size, IndexValidateStructure, &ctx);

    rc = VPath2BGZF(&bam, bampath);
    if (rc == 0) {
        ctx.mpositions = 1024 * 32;
        ctx.position = malloc(ctx.mpositions * sizeof(ctx.position[0]));
        if (ctx.position == NULL)
            return RC(rcAlign, rcIndex, rcReading, rcMemory, rcExhausted);
        
        rc = WalkIndexStructure(bai, bai_size, BAMValidateLoadIndex, &ctx);
        free(bai);
        if (rc) {
            stats.indexStructureIsBad = true;
            rc = callback(callbackContext, rc, &stats);
        }
        else {
            unsigned i = 0;
            
            stats.indexStructureIsGood = true;
            stats.baiFileSize = ctx.npositions;
            
            ksort(ctx.position, ctx.npositions, sizeof(ctx.position[0]), comp_index_data, 0);
            
            stats.bamFileSize = bam.file.size;
            
            while (i < ctx.npositions) {
                uint64_t const ifpos = ctx.position[i].position >> 16;
                uint16_t const bpos = (uint16_t)ctx.position[i].position;
                
                stats.baiFilePosition = i;
                if (i == 0 || ifpos != pos) {
                    stats.bamFilePosition = pos = ifpos;
                    rc = BGZFileSetPos(&bam, pos);
                    if (rc == 0)
                        rc = BGZFileRead(&bam, data, &dsize);
                    if (rc) {
                        ++stats.indexFileOffset.error;
                        do {
                            ++i;
                            if (i == ctx.npositions)
                                break;
                            if (ctx.position[i].position >> 16 != pos)
                                break;
                            ++stats.indexFileOffset.error;
                        } while (1);
                    }
                    else
                        ++stats.indexFileOffset.good;

                    rc = callback(callbackContext, rc, &stats);
                    if (rc)
                        break;
                }
                else
                    ++stats.indexFileOffset.good;
                if ((options & bvo_IndexOptions) > bvo_IndexOffsets1) {
                    int32_t rsize = 0;
                    BAMAlignment algn;
                    
                    if (bpos >= dsize)
                        goto BAD_BLOCK_OFFSET;
                    if (dsize - bpos < 4) {
                    READ_MORE:
                        if (dsize > ZLIB_BLOCK_SIZE)
                            goto BAD_BLOCK_OFFSET;

                        rc = BGZFileRead(&bam, data + dsize, &temp);
                        if (rc) {
                            ++stats.blockCompression.error;
                            goto BAD_BLOCK_OFFSET;
                        }
                        dsize += temp;
                        if (dsize - bpos < 4 || dsize - bpos < rsize)
                            goto BAD_BLOCK_OFFSET;
                    }
                    rsize = LE2HI32(data + bpos);
                    if (rsize <= 0)
                        goto BAD_BLOCK_OFFSET;
                    if (rsize > 0xFFFF) {
                        ++stats.indexBlockOffset.warning;
                        ++i;
                        continue;
                    }
                    if (dsize - bpos < rsize)
                        goto READ_MORE;
/*                    rc = BAMAlignmentParse(&algn, data + bpos + 4, rsize); */
                    if (rc)
                        goto BAD_BLOCK_OFFSET;
                    ++stats.indexBlockOffset.good;
                    if ((options & bvo_IndexOptions) > bvo_IndexOffsets2) {
                        int32_t const refSeqId = getRefSeqId(&algn);
                        uint16_t const binNo = getBin(&algn);
                        int32_t const position = getPosition(&algn);
                        
                        if (ctx.position[i].refNo == refSeqId &&
                            (ctx.position[i].binNo == binNo ||
                             ctx.position[i].binNo == ~((unsigned)0)
                        ))
                            ++stats.indexBin.good;
                        else if (ctx.position[i].refNo == refSeqId)
                            ++stats.indexBin.warning;
                        else
                            ++stats.indexBin.error;
                        
                        if (refSeqId < ref || position < rpos)
                            ++stats.inOrder.error;
                        
                        ref = refSeqId;
                        rpos = position;
                    }
                }
                if (0) {
                BAD_BLOCK_OFFSET:
                    ++stats.indexBlockOffset.error;
                }
                ++i;
            }
        }
        
        free(ctx.position);
        BGZFileWhack(&bam);
    }
    stats.bamFilePosition = stats.bamFileSize;
    return callback(callbackContext, rc, &stats);
}

static rc_t BAMValidate3(BAMValidate_ctx_t *ctx,
                         BAMAlignment const *algn
                         )
{
    rc_t rc = 0;
    uint16_t const flags = getFlags(algn);
    int32_t const refSeqId = getRefSeqId(algn);
    int32_t const refPos = getPosition(algn);
    unsigned const mapQ = getMapQual(algn);
    bool const aligned =
        ((flags & BAMFlags_SelfIsUnmapped) == 0) && 
        (refSeqId >= 0) && (refSeqId < ctx->nrefs) &&
        (refPos >= 0) && (refPos < ctx->refLen[refSeqId]) && (mapQ > 0);
    
    if (ctx->options & bvo_ExtraFields) {
    }
    if (aligned) {
        if ((ctx->options & bvo_Sorted) != 0) {
            if (ctx->lastRefId < refSeqId || (ctx->lastRefId == refSeqId && ctx->lastRefPos <= refPos))
                ++ctx->stats->inOrder.good;
            else
                ++ctx->stats->inOrder.error;
            ctx->lastRefId = refSeqId;
            ctx->lastRefPos = refPos;
        }
        if (ctx->options & bvo_CIGARConsistency) {
        }
        if (ctx->options & bvo_BinConsistency) {
        }
    }
    if (ctx->options & bvo_FlagsConsistency) {
    }
    if (ctx->options & bvo_QualityValues) {
    }
    if (ctx->options & bvo_MissingSequence) {
    }
    if (ctx->options & bvo_MissingQuality) {
    }
    if (ctx->options & bvo_FlagsStats) {
    }
    return rc;
}

static rc_t BAMValidate2(void *Ctx, const BGZFile *file,
                         rc_t rc, uint64_t fpos,
                         const zlib_block_t data, unsigned dsize)
{
    BAMValidate_ctx_t *ctx = Ctx;
    rc_t rc2;
    bool fatal = false;
    
    ctx->stats->bamFilePosition = fpos;
    if (rc) {
        if (ctx->options == bvo_BlockHeaders)
            ++ctx->stats->blockHeaders.error;
        else
            ++ctx->stats->blockCompression.error;
    }
    else if (ctx->options == bvo_BlockHeaders) {
        ++ctx->stats->blockHeaders.good;
    }
    else if (ctx->options == bvo_BlockCompression) {
        ++ctx->stats->blockHeaders.good;
        ++ctx->stats->blockCompression.good;
    }
    else if (dsize) {
        ctx->bsize += dsize;
        if (!ctx->stats->bamHeaderIsBad && !ctx->stats->bamHeaderIsGood) {
            unsigned header_len;
            unsigned refs_start;
            unsigned nrefs;
            unsigned data_start;
            
            rc2 = BAMValidateHeader(ctx->buf, ctx->bsize,
                                       &header_len, &refs_start,
                                       &nrefs, &data_start);
            
            if (rc2 == 0) {
                ctx->stats->bamHeaderIsGood = true;
                if (ctx->options & bvo_BinConsistency) {
                    ctx->refLen = malloc(nrefs * sizeof(ctx->refLen[0]));
                    if (ctx->refLen == NULL) {
                        rc = RC(rcAlign, rcFile, rcValidating, rcMemory, rcExhausted);
                        fatal = true;
                    }
                    else {
                        unsigned cp;
                        unsigned i;
                        
                        ctx->nrefs = nrefs;
                        for (i = 0, cp = refs_start; cp != data_start; ++i) {
                            int32_t len;
                            
                            memcpy(&len, &ctx->buf[cp], 4);
                            memcpy(&ctx->refLen[i], &ctx->buf[cp + 4 + len], 4);
                            cp += len + 8;
                        }
                    }
                }
                ctx->dnext = data_start;
            }
            else if ( GetRCState( rc2 ) != rcIncomplete || GetRCObject( rc2 ) != (enum RCObject)rcData)
            {
                ctx->stats->bamHeaderIsBad = true;
                ctx->options = bvo_BlockCompression;
                rc = rc2;
            }
            else
                ctx->dnext = ctx->bsize;
        }
        if (rc == 0) {
            if (ctx->stats->bamHeaderIsGood) {
                unsigned cp = ctx->dnext;
                
                while (cp + 4 < ctx->bsize) {
                    int32_t rsize;
                    
                    rsize = LE2HI32(&ctx->buf[cp]);
                    if (rsize < 0) {
                        ++ctx->stats->blockStructure.error;
                        ctx->options = bvo_BlockStructure;
                        
                        /* throw away the rest of the current buffer */
                        if (cp >= ctx->bsize - dsize)
                            cp = ctx->bsize;
                        else
                            cp = ctx->bsize - dsize;
                        
                        rc = RC(rcAlign, rcFile, rcValidating, rcData, rcInvalid);
                        break;
                    }
                    else if (cp + 4 + rsize < ctx->bsize) {
                        if (rsize > UINT16_MAX)
                            ++ctx->stats->blockStructure.warning;
                        else
                            ++ctx->stats->blockStructure.good;
                        if (ctx->options > bvo_BlockStructure) {
                            BAMAlignment algn;
                            
/*                            rc = BAMAlignmentParse(&algn, &ctx->buf[cp + 4], rsize); */
                            if (rc == 0) {
                                ++ctx->stats->recordStructure.good;
                                if (ctx->options > bvo_RecordStructure)
                                    rc = BAMValidate3(ctx, &algn);
                            }
                            else
                                ++ctx->stats->recordStructure.error;
                        }
                        cp += 4 + rsize;
                    }
                    else
                        break;
                }
                if (&ctx->buf[cp] >= data) {
                    if (cp < ctx->bsize) {
                        ctx->bsize -= cp;
                        memmove(ctx->buf, &ctx->buf[cp], ctx->bsize);
                        cp = ctx->bsize;
                    }
                    else {
                        assert(cp == ctx->bsize);
                        cp = ctx->bsize = 0;
                    }
                }
                ctx->dnext = cp;
            }
            if (ctx->alloced < ctx->bsize + ZLIB_BLOCK_SIZE) {
                void *temp;
                
                temp = realloc(ctx->buf, ctx->alloced + ZLIB_BLOCK_SIZE);
                if (temp == NULL) {
                    rc = RC(rcAlign, rcFile, rcValidating, rcMemory, rcExhausted);
                    fatal = true;
                }
                else {
                    ctx->buf = temp;
                    ctx->alloced += ZLIB_BLOCK_SIZE;
                }
            }
            ctx->nxt = &ctx->buf[ctx->dnext];
        }
    }
    rc2 = ctx->callback(ctx->ctx, rc, ctx->stats);
    ctx->cancelled |= rc2 != 0;
    return fatal ? rc : rc2;
}

static rc_t BAMValidateBAM(struct VPath const *bampath,
                           BAMValidateOption options,
                           BAMValidateCallback callback,
                           void *callbackContext
                           )
{
    rc_t rc;
    BGZFile bam;
    BAMValidate_ctx_t ctx;
    BAMValidateStats stats;

    if (bampath == NULL)
        return RC(rcAlign, rcFile, rcValidating, rcParam, rcNull);
    
    memset(&ctx, 0, sizeof(ctx));
    memset(&stats, 0, sizeof(stats));
    
    ctx.callback = callback;
    ctx.ctx = callbackContext;
    ctx.options = options;
    ctx.stats = &stats;
    
    if (options > bvo_BlockCompression) {
        ctx.alloced = ZLIB_BLOCK_SIZE * 2;
        ctx.nxt = ctx.buf = malloc(ctx.alloced);
        
        if (ctx.buf == NULL)
            return RC(rcAlign, rcFile, rcValidating, rcMemory, rcExhausted);
    }
    
    if (options > bvo_RecordStructure)
        options = bvo_RecordStructure | (options & 0xFFF0);
    
    rc = VPath2BGZF(&bam, bampath);
    if (rc == 0) {
        stats.bamFileSize = bam.file.size;
        if ((options & 7) > bvo_BlockHeaders)
            rc = BGZFileWalkBlocks(&bam, true, (zlib_block_t *)&ctx.nxt, BAMValidate2, &ctx);
        else
            rc = BGZFileWalkBlocks(&bam, false, NULL, BAMValidate2, &ctx);
    }
    BGZFileWhack(&bam);
    return rc;
}

static rc_t CC dummy_cb(void *ctx, rc_t result, const BAMValidateStats *stats)
{
    return 0;
}

LIB_EXPORT rc_t CC BAMValidate(struct VPath const *bampath,
                               struct VPath const *baipath,
                               BAMValidateOption options,
                               BAMValidateCallback callback,
                               void *callbackContext
                               )
{
    if (callback == NULL)
        callback = dummy_cb;
    if (bampath == NULL)
        return RC(rcAlign, rcFile, rcValidating, rcParam, rcNull);
    if (baipath == NULL) {
        if (options & bvo_IndexOptions)
            return RC(rcAlign, rcFile, rcValidating, rcParam, rcNull);
        return BAMValidateBAM(bampath, options, callback, callbackContext);
    }
    return BAMValidateIndex(bampath, baipath, options, callback, callbackContext);
}
