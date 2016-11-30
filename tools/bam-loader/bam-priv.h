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
 *
 */

#include "bam.h"
#include "bam-alignment.h"

typedef struct BAMIndex BAMIndex;
typedef struct BufferedFile BufferedFile;
typedef struct SAMFile SAMFile;
typedef struct BGZFile BGZFile;

#define ZLIB_BLOCK_SIZE  (64u * 1024u)
#define RGLR_BUFFER_SIZE (16u * ZLIB_BLOCK_SIZE)
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

struct SAMFile {
    BufferedFile file;
    int putback;
    rc_t last;
};

struct BGZFile {
    BufferedFile file;
    z_stream zs;
};

struct BAM_File {
    union {
        BGZFile bam;
        SAMFile sam;
    } file;
    RawFile_vt vt;

    KFile *defer;
    
    BAMRefSeq *refSeq;          /* pointers into headerData1 except name points into headerData2 */ 
    BAMReadGroup *readGroup;    /* pointers into headerData1 */
    char const *version;
    char const *header;
    void *headerData1;          /* gets used for refSeq and readGroup */
    void *headerData2;          /* gets used for refSeq */
    BAM_Alignment *nocopy;       /* used to hold current record for BAM_FileRead2 */

    uint64_t fpos_cur;
    uint64_t deferPos;
    
    unsigned refSeqs;
    unsigned readGroups;
    unsigned bufSize;           /* current size of uncompressed buffer */
    unsigned bufCurrent;        /* location in uncompressed buffer of read head */
    bool eof;
    bool isSAM;
    zlib_block_t buffer;        /* uncompressed buffer */
};

#define CG_NUM_SEGS 4

#ifdef __GNUC__
static inline int getRefSeqId(BAM_Alignment const *) __attribute__((always_inline));
static inline int getPosition(BAM_Alignment const *) __attribute__((always_inline));
static inline int getReadNameLength(BAM_Alignment const *) __attribute__((always_inline));
static inline int getBin(BAM_Alignment const *) __attribute__((always_inline));
static inline int getMapQual(BAM_Alignment const *) __attribute__((always_inline));
static inline int getCigarCount(BAM_Alignment const *) __attribute__((always_inline));
static inline int getFlags(BAM_Alignment const *) __attribute__((always_inline));
static inline unsigned getReadLen(BAM_Alignment const *) __attribute__((always_inline));
static inline int getMateRefSeqId(BAM_Alignment const *) __attribute__((always_inline));
static inline int getMatePos(BAM_Alignment const *) __attribute__((always_inline));
static inline int getInsertSize(BAM_Alignment const *) __attribute__((always_inline));
static inline char const *getReadName(BAM_Alignment const *) __attribute__((always_inline));
static inline void const *getCigarBase(BAM_Alignment const *) __attribute__((always_inline));
static inline uint16_t LE2HUI16(void const *) __attribute__((always_inline));
static inline uint32_t LE2HUI32(void const *) __attribute__((always_inline));
static inline uint64_t LE2HUI64(void const *) __attribute__((always_inline));
static inline  int16_t  LE2HI16(void const *) __attribute__((always_inline));
static inline  int32_t  LE2HI32(void const *) __attribute__((always_inline));
static inline  int64_t  LE2HI64(void const *) __attribute__((always_inline));
#endif /* __GNUC__ */

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint16_t LE2HUI16(void const *X) { uint16_t y; memmove(&y, X, sizeof(y)); return y; }
static inline uint32_t LE2HUI32(void const *X) { uint32_t y; memmove(&y, X, sizeof(y)); return y; }
static inline uint64_t LE2HUI64(void const *X) { uint64_t y; memmove(&y, X, sizeof(y)); return y; }
static inline  int16_t  LE2HI16(void const *X) {  int16_t y; memmove(&y, X, sizeof(y)); return y; }
static inline  int32_t  LE2HI32(void const *X) {  int32_t y; memmove(&y, X, sizeof(y)); return y; }
static inline  int64_t  LE2HI64(void const *X) {  int64_t y; memmove(&y, X, sizeof(y)); return y; }
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
static inline uint16_t LE2HUI16(void const *X) { uint16_t y; memmove(&y, X, sizeof(y)); return (uint16_t)bswap_16(y); }
static inline uint32_t LE2HUI32(void const *X) { uint32_t y; memmove(&y, X, sizeof(y)); return (uint32_t)bswap_32(y); }
static inline uint64_t LE2HUI64(void const *X) { uint64_t y; memmove(&y, X, sizeof(y)); return (uint64_t)bswap_64(y); }
static inline  int16_t  LE2HI16(void const *X) {  int16_t y; memmove(&y, X, sizeof(y)); return ( int16_t)bswap_16(y); }
static inline  int32_t  LE2HI32(void const *X) {  int32_t y; memmove(&y, X, sizeof(y)); return ( int32_t)bswap_32(y); }
static inline  int64_t  LE2HI64(void const *X) {  int64_t y; memmove(&y, X, sizeof(y)); return ( int64_t)bswap_64(y); }
#endif

static inline int getRefSeqId(BAM_Alignment const *const self) {
    return LE2HI32(self->data->cooked.rID);
}

static inline int getPosition(BAM_Alignment const *const self) {
    return LE2HI32(self->data->cooked.pos);
}

static inline int getReadNameLength(BAM_Alignment const *const self) {
    return self->data->cooked.read_name_len;
}

static inline int getBin(BAM_Alignment const *const self) {
    return LE2HUI16(self->data->cooked.bin);
}

static inline int getMapQual(BAM_Alignment const *const self) {
    return self->data->cooked.mapQual;
}

static inline int getCigarCount(BAM_Alignment const *const self) {
    return LE2HUI16(self->data->cooked.n_cigars);
}

static inline int getFlags(BAM_Alignment const *const self) {
    return LE2HUI16(self->data->cooked.flags);
}

static inline unsigned getReadLen(BAM_Alignment const *const self) {
    return LE2HUI32(self->data->cooked.read_len);
}

static inline int getMateRefSeqId(BAM_Alignment const *const self) {
    return LE2HI32(self->data->cooked.mate_rID);
}

static inline int getMatePos(BAM_Alignment const *const self) {
    return LE2HI32(self->data->cooked.mate_pos);
}

static inline int getInsertSize(BAM_Alignment const *const self) {
    return LE2HI32(self->data->cooked.ins_size);
}

static inline char const *getReadName(BAM_Alignment const *const self) {
    return &self->data->cooked.read_name[0];
}

static inline void const *getCigarBase(BAM_Alignment const *const self)
{
    return &self->data->raw[self->cigar];
}

static bool BAM_AlignmentShouldDefer(BAM_Alignment const *);
