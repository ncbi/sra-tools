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
#include <klib/log.h>
#include <klib/rc.h>
#include <os-native.h>

#include <stdlib.h>

#include <string.h>
#include <ctype.h>
#include <endian.h>
#include <byteswap.h>
#include <assert.h>

#include "ztr-illumina.h"
#include "ztr-huffman.h"
#include "debug.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define ltoh16(X) (X)
#define ltoh32(X) (X)

#define btoh16(X) (bswap_16(X))
#define btoh32(X) (bswap_32(X))

#else

#define ltoh16(X) (bswap_16(X))
#define ltoh32(X) (bswap_32(X))

#define btoh16(X) (X)
#define btoh32(X) (X)

#endif

struct ZTR_Context {
    int versMajor;
    int versMinor;
    uint32_t checksum;
    ztr_huffman_table huffman_table[128], special[3];
    size_t current, buflen[2];
    const uint8_t *buf[2];
};

static int ztr_read(ZTR_Context *ctx, void *Dst, size_t n) {
    char *dst = Dst;
    size_t i, j = 0;
    
    assert(ctx);
    do {
        if (n == 0 || ctx->buf[0] == NULL)
            break;
        i = ctx->buflen[0] - ctx->current;
        if (i > n)
            i = n;
        memcpy(dst + j, ctx->buf[0] + ctx->current, i);
        ctx->current += i;
        n -= i;
        j += i;
        if (ctx->current == ctx->buflen[0]) {
            ctx->current = 0;
            ctx->buflen[0] = ctx->buflen[1];
            ctx->buf[0] = ctx->buf[1];
            ctx->buflen[1] = 0;
            ctx->buf[1] = NULL;
        }
    } while (1);

    return j;
}

#define METANAME_COMP strcmp

static size_t strendlen(const char *src, const char *end) {
    size_t n = 0;
    
    while (*src && src != end) {
        ++src;
        ++n;
    }
    return n;
}

static int count_pairs(const char *src, uint32_t len) {
    int i, j;
    
    for (j = i = 0; i != len; ++i) {
        if (*src++ == 0)
            ++j;
    }
    return (j % 2 == 0) ? (j >> 1) : 0;
}

#if 0
static const char *meta_getValueForName(const nvp_t *meta, int count, const char *name) {
    int i;
    
    for (i = 0; i != count; ++i) {
        if (meta[i].name != NULL && METANAME_COMP(meta[i].name, name) == 0)
            return meta[i].value;
    }
    return NULL;
}
#endif

static const char *meta_getValueForNameAndRemove(nvp_t *meta, int count, const char *name) {
    int i;
    
    for (i = 0; i != count; ++i) {
        if (meta[i].name != NULL && METANAME_COMP(meta[i].name, name) == 0) {
            meta[i].name = NULL;
            return meta[i].value;
        }
    }
    return NULL;
}

static void parse_meta(nvp_t *meta, int count, const char *metadata, const char *end) {
    int i, j;
    
    for (i = 0; i != count && metadata <= end; ++i) {
        j = strendlen(metadata, end);
        if (j == 0)
            break;
        meta[i].name = metadata;
        metadata += j; ++metadata;
        meta[i].value = metadata;
        metadata += strendlen(metadata, end); ++metadata;
    }
}

bool ILL_ZTR_BufferIsEmpty(const ZTR_Context *ctx) {
	return (ctx->buf[0] == NULL || ctx->buflen[0] == 0) ? true : false;
}

static int isZTRchunk(const uint8_t type[4]) {
    static const char *types[] = {
        "BASE",
        "BPOS",
        "CLIP",
        "CNF1",
        "CNF4",
        "COMM",
        "CR32",
        "HUFF",
        "REGN",
        "SAMP",
        "SMP4",
        "TEXT"
    };
    int i;
    
    for (i = 0; i != sizeof(types) / sizeof(types[0]); ++i)
        if (*(const uint32_t *)type == *(const uint32_t *)types[i])
            return 1;
    
    return 0;
}

static rc_t parse_block(ZTR_Context *ctx, ztr_raw_t *ztr_raw) {
    if (ztr_read(ctx, &ztr_raw->type, 4) != 4)
        return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
    
    if (!isZTRchunk(ztr_raw->type))
        return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
    
    if (ztr_read(ctx, &ztr_raw->metalength, 4) != 4)
        return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
    
    if (ztr_raw->metalength != 0) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        ztr_raw->metalength = bswap_32(ztr_raw->metalength);
#endif
        ztr_raw->meta = malloc(ztr_raw->metalength);

        if (ztr_raw->meta == NULL)
            return RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);

        if (ztr_read(ctx, ztr_raw->meta, ztr_raw->metalength) != ztr_raw->metalength)
            return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
    }
    
    if (ztr_read(ctx, &ztr_raw->datalength, 4) != 4)
        return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
    
    if (ztr_raw->datalength != 0) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        ztr_raw->datalength = bswap_32(ztr_raw->datalength);
#endif
        ztr_raw->data = malloc(ztr_raw->datalength+1); /*** overallocating  **/

        if (ztr_raw->data == NULL)
            return RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);

        if (ztr_read(ctx, ztr_raw->data, ztr_raw->datalength) != ztr_raw->datalength)
            return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
    }
    
    return 0;
}

rc_t ILL_ZTR_ParseBlock(ZTR_Context *ctx, ztr_raw_t *ztr_raw)
{
    rc_t rc = 0;
    uint32_t current, len[2];
    const uint8_t *buf[2];
    
    assert(ctx);
    assert(ztr_raw);
    
    memset(ztr_raw, 0, sizeof(*ztr_raw));
	
	if (ctx->buf[0] == NULL || ctx->buflen[0] == 0 || ctx->current == ctx->buflen[0] + ctx->buflen[1])
		return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcEmpty );
	
    /* save input buffer state for restoration upon error exit */
    current = ctx->current;
    len[0] = ctx->buflen[0];
    len[1] = ctx->buflen[1];
    buf[0] = ctx->buf[0];
    buf[1] = ctx->buf[1];
    
    rc = parse_block(ctx, ztr_raw);
    if (rc) {
        /* clean up and restore buffer state */
        if (ztr_raw->data)
            free (ztr_raw->data);
        if (ztr_raw->meta)
            free (ztr_raw->meta);
        
        ctx->current = current;
        ctx->buflen[0] = len[0];
        ctx->buflen[1] = len[1];
        ctx->buf[0] = buf[0];
        ctx->buf[1] = buf[1];
    }
    return rc;
}

static rc_t undo_mode_4(uint8_t **Data, size_t *datasize) {
	const uint8_t *src = *Data, *end;
	size_t srclen = *datasize, dstlen = 0;
	size_t dalloc;
	uint8_t *dst, *Dst;
	uint8_t dup = 0;
	uint8_t esize;
	rc_t rc = 0;
	int st = 0;
	int i;
	
	esize = src[1];
	if (esize < 1 || srclen % esize != 0) {
		rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
        return rc;
    }
    if (esize > 2) {
        src += esize; srclen -= esize;
    }
    else {
        src += 2; srclen -= 2;
        if (srclen == 0) {
            rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
            return rc;
        }
    }
    Dst = dst = malloc(dalloc = srclen << 4);
    if (Dst == NULL) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);
        return rc;
    }

    end = src + srclen;
    while (src != end) {
        size_t out = esize;
        
        if (st == 2)
            out *= (dup = *src);
        if ((dstlen += out) >= dalloc) {
            void *temp;
            size_t n = dst - Dst;
            
            while (dalloc < dstlen)
                dalloc <<= 1;
            temp = realloc(Dst, dalloc);
            if (temp == NULL) {
                rc = RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);
                break;
            }
            Dst = temp;
            dst = Dst + n;
        }
        switch (st) {
            case 0:
                st = 1;
                memcpy(dst, src, esize);
                break;
            case 1:
                if (memcmp(dst, src, esize) == 0)
                    st = 2;
                dst += esize;
                memcpy(dst, src, esize);
                break;
            case 2:
                st = 0;
                dst += esize;
                for (i = 0; i != dup; ++i) {
                    memcpy(dst, src - esize, esize);
                    dst += esize;
                }
                break;
        }
        src += esize;
    }

	free((void *)*Data);
	*Data = Dst;
	*datasize = dstlen;
	return rc;
}

static rc_t undo_mode_79(uint8_t **Data, size_t *datasize) {
    uint8_t scratch[4 * 1024];
	const uint8_t *src = *Data;
	size_t srclen = *datasize;
	uint8_t *dst0, *dst1, *Dst;
	rc_t rc = 0;
	int i, j, k, n;

	if (srclen == 0) {
		rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
        return rc;
    }
    assert(srclen % 4 == 0);
    assert(srclen < sizeof(scratch));
    
    Dst = *Data;
    Dst[0] = 0;
    n = (srclen >> 2) - 1;
    dst0 = scratch;
    dst1 = dst0 + n;
    src += 4;
    for (k = i = j = 0; i != n; ++i, k += 3, j += 4) {
        dst0[i + 0] = src[j + 0];
        dst1[k + 0] = src[j + 1];
        dst1[k + 1] = src[j + 2];
        dst1[k + 2] = src[j + 3];
    }

    memcpy(Dst + 1, scratch, n << 2);
	*Data = Dst;
	*datasize = (n << 2) + 1;
	return rc;
}

#if 0
static rc_t undo_mode_80(uint8_t **Data, size_t *datasize, const uint8_t *sequence, size_t basecount) {
	uint16_t temp[4 * 1024];
	uint16_t *A = temp;
	uint16_t *C = A + basecount;
	uint16_t *G = C + basecount;
	uint16_t *T = G + basecount;
	uint8_t *dst;
	int i, n;
	const uint16_t *u;

	assert(basecount < 1024);
	dst = *Data;
	*dst = 0;
	u = (const uint16_t *)(dst + 8);
	for (n = basecount, i = 0; i != n; ++i, u += 4) {
		switch (sequence[i]) {
			default:
			case 'A':
			case 'a':
				A[i] = btoh16(u[0]);
				C[i] = btoh16(u[1]);
				G[i] = btoh16(u[2]);
				T[i] = btoh16(u[3]);
				break;					
			case 'C':
			case 'c':
				C[i] = btoh16(u[0]);
				A[i] = btoh16(u[1]);
				G[i] = btoh16(u[2]);
				T[i] = btoh16(u[3]);
				break;					
			case 'G':
			case 'g':
				G[i] = btoh16(u[0]);
				A[i] = btoh16(u[1]);
				C[i] = btoh16(u[2]);
				T[i] = btoh16(u[3]);
				break;					
			case 'T':
			case 't':
				T[i] = btoh16(u[0]);
				A[i] = btoh16(u[1]);
				C[i] = btoh16(u[2]);
				G[i] = btoh16(u[3]);
				break;
		}
	}
	memcpy(dst + 2, temp, 8 * basecount);
	*datasize = 2 + 8 * basecount;
	return 0;
}
#else
static rc_t undo_mode_80(uint8_t **Data, size_t *datasize, const uint8_t *sequence, size_t basecount, long offset) {
	const uint16_t *u = (const uint16_t *)(*Data + 8);
	int i;
    float *Dst = malloc(sizeof(Dst[0]) * 4 * basecount);
    float *dst = Dst;

    if (dst == NULL)
        return RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);
    
	for (i = 0; i != basecount; ++i, u += 4, dst += 4) {
		switch (sequence[i]) {
			default:
			case 'A':
			case 'a':
				dst[0] = (long)btoh16(u[0]) - offset;
				dst[1] = (long)btoh16(u[1]) - offset;
				dst[2] = (long)btoh16(u[2]) - offset;
				dst[3] = (long)btoh16(u[3]) - offset;
				break;					
			case 'C':
			case 'c':
				dst[1] = (long)btoh16(u[0]) - offset;
				dst[0] = (long)btoh16(u[1]) - offset;
				dst[2] = (long)btoh16(u[2]) - offset;
				dst[3] = (long)btoh16(u[3]) - offset;
				break;					
			case 'G':
			case 'g':
				dst[2] = (long)btoh16(u[0]) - offset;
				dst[0] = (long)btoh16(u[1]) - offset;
				dst[1] = (long)btoh16(u[2]) - offset;
				dst[3] = (long)btoh16(u[3]) - offset;
				break;					
			case 'T':
			case 't':
				dst[3] = (long)btoh16(u[0]) - offset;
				dst[0] = (long)btoh16(u[1]) - offset;
				dst[1] = (long)btoh16(u[2]) - offset;
				dst[2] = (long)btoh16(u[3]) - offset;
				break;
		}
	}
    free(*Data);
    *Data = (void *)Dst;
	*datasize =  sizeof(dst[0]) * 4 * basecount;
	return 0;
}
#endif

static void swab16(const void *Src, void *Dst, size_t len) {
	const uint16_t *src = Src;
	uint16_t *dst = Dst;
	size_t n = len >> 1;
	size_t i;
	
	assert((len & 1) == 0);
	for (i = 0; i != n; ++i)
		dst[i] = bswap_16(src[i]);
}

static void swab32(const void *Src, void *Dst, size_t len) {
	const uint32_t *src = Src;
	uint32_t *dst = Dst;
	size_t n = len >> 2;
	size_t i;
	
	assert((len & 3) == 0);
	for (i = 0; i != n; ++i)
		dst[i] = bswap_32(src[i]);
}

static void swab64(const void *Src, void *Dst, size_t len) {
	const uint64_t *src = Src;
	uint64_t *dst = Dst;
	size_t n = len >> 3;
	size_t i;
	
	assert((len & 7) == 0);
	for (i = 0; i != n; ++i)
		dst[i] = bswap_64(src[i]);
}

static rc_t fixup_read(uint8_t *read, size_t bases) {
	int i;
	
	for (i = 0; i != bases; ++i)
		switch (read[i]) {
			case 'A':
			case 'C':
			case 'G':
			case 'T':
			case 'a':
			case 'c':
			case 'g':
			case 't':
				break;
			default:
				read[i] = 'N';
				break;
		}
	return 0;
}

static rc_t fixup_quality4(uint8_t *qual, const uint8_t *read, size_t bases) {
	uint8_t src[4 * 1024];
	const uint8_t *src0 = src; /* called */
	const uint8_t *src1 = src0 + bases; /* not called */
	int i, j, k;
	
	assert(bases < 1024);
	memcpy(src, qual, bases * 4);
	
	for (k = j = i = 0; i != bases; ++i, j += 3, k += 4) {
		switch (read[i]) {
			default:
			case 'A':
			case 'a':
				qual[k + 0] = src0[i];
				qual[k + 1] = src1[j + 0];
				qual[k + 2] = src1[j + 1];
				qual[k + 3] = src1[j + 2];
				break;
			case 'C':
			case 'c':
				qual[k + 0] = src1[j + 0];
				qual[k + 1] = src0[i];
				qual[k + 2] = src1[j + 1];
				qual[k + 3] = src1[j + 2];
				break;
			case 'G':
			case 'g':
				qual[k + 0] = src1[j + 0];
				qual[k + 1] = src1[j + 1];
				qual[k + 2] = src0[i];
				qual[k + 3] = src1[j + 2];
				break;
			case 'T':
			case 't':
				qual[k + 0] = src1[j + 0];
				qual[k + 1] = src1[j + 1];
				qual[k + 2] = src1[j + 2];
				qual[k + 3] = src0[i];
				break;
		}
	}
	return 0;
}

static rc_t fixup_trace_reorder(float *dst, const uint16_t *src, long offset, size_t bases) {
	size_t i, j;
	const uint16_t *A = src;
	const uint16_t *C = A + bases;
	const uint16_t *G = C + bases;
	const uint16_t *T = G + bases;
	
	for (j = i = 0; i < bases; ++i, j += 4) {
		dst[j + 0] = ((long)A[i] - offset);
		dst[j + 1] = ((long)C[i] - offset);
		dst[j + 2] = ((long)G[i] - offset);
		dst[j + 3] = ((long)T[i] - offset);
	}
	
	return 0;
}

static rc_t fixup_trace(float *dst, const uint16_t *src, long offset, size_t bases) {
	size_t i;
	
    /*
     float x;
     float y;
     int t;
     
     y = clip(x + offset, 0, 64k);
     t = rint(y);
     write(t & 0xFFFF)
    */
    
    if (offset == 0) {
        for (i = 0; i != bases * 4; ++i)
            dst[i] = src[i];
        return 0;
    }
	for (i = 0; i != bases * 4; ++i)
		dst[i] = ((long)src[i] - offset);
	return 0;
}

rc_t ILL_ZTR_Decompress(ZTR_Context *ctx, enum ztr_chunk_type type, ztr_t ztr, const ztr_t base) {
	int padded = 1;
	size_t *datasize;
	uint8_t **data;
	enum ztr_data_type datatype;
	static char zero[8];
	rc_t rc = 0;
	
	if (ztr.sequence == NULL)
		return 0;
    switch (type) {
        case BASE:
			data = &ztr.sequence->data;
			datasize = &ztr.sequence->datasize;
			datatype = ztr.sequence->datatype;
            break;
        case BPOS:
			data = &ztr.position->data;
			datasize = &ztr.position->datasize;
			datatype = ztr.position->datatype;
            break;
        case CNF1:
			data = &ztr.quality1->data;
			datasize = &ztr.quality1->datasize;
			datatype = ztr.quality1->datatype;
            break;
        case CNF4:
			data = &ztr.quality4->data;
			datasize = &ztr.quality4->datasize;
			datatype = ztr.quality4->datatype;
            break;
        case SAMP:
			data = &ztr.signal->data;
			datasize = &ztr.signal->datasize;
			datatype = ztr.signal->datatype;
            break;
        case SMP4:
			data = &ztr.signal4->data;
			datasize = &ztr.signal4->datasize;
			datatype = ztr.signal4->datatype;
            break;
        default:
			return 0;
    }
	assert(*datasize);
	assert(*data);

    if ((*data)[0] == 0x4D) {
        switch ((*data)[1]) {
        case 1:
            rc = handle_special_huffman_codes(&ctx->special[0], 0);
            if (rc == 0)
                rc = decompress_huffman(&ctx->special[0], data, datasize);
            if (rc == 0)
                (*data)[0] = 0;
            break;
        case 2:
            rc = handle_special_huffman_codes(&ctx->special[1], 1);
            if (rc == 0)
                rc = decompress_huffman(&ctx->special[1], data, datasize);
            if (rc == 0)
                (*data)[0] = 0;
            break;
        case 3:
            rc = handle_special_huffman_codes(&ctx->special[2], 2);
            if (rc == 0)
                rc = decompress_huffman(&ctx->special[2], data, datasize);
            if (rc == 0)
                (*data)[0] = 0;
            break;
        default:
            if ((*data)[1] < 128)
                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
            else
                rc = decompress_huffman(&ctx->huffman_table[(*data)[1] - 128], data, datasize);
        }
    }
	while (rc == 0) {
		switch (**data) {
		case 0:
			switch (datatype) {
				case i16:
				case i16v4:
					if (memcmp(*data, zero, 2) == 0)
						padded = 2;
					*datasize -= padded;
#if __BYTE_ORDER == __LITTLE_ENDIAN
					swab16((*data) + padded, *data, *datasize);
#else
					memmove(*data, (*data) + padded, *datasize);
#endif
					break;
				case i32:
				case f32:
					if (memcmp(*data, zero, 4) == 0)
						padded = 4;
					*datasize -= padded;
#if __BYTE_ORDER == __LITTLE_ENDIAN
					swab32((*data) + padded, *data, *datasize);
#else
					memmove(*data, (*data) + padded, *datasize);
#endif
					break;
				case f64:
					if (memcmp(*data, zero, 8) == 0)
						padded = 8;
					*datasize -= padded;
#if __BYTE_ORDER == __LITTLE_ENDIAN
					swab64((*data) + padded, *data, *datasize);
#else
					memmove(*data, (*data) + padded, *datasize);
#endif
					break;
				default:
					*datasize -= padded;
					memmove(*data, (*data) + padded, *datasize);
					break;
			}
			switch (type) {
			case BASE:
				rc = fixup_read(*data, *datasize);
				break;
			case CNF4:
				rc = fixup_quality4(*data, base.sequence->data, base.sequence->datasize);
				break;
			case SMP4:
                {
                    float *dst = malloc(sizeof(*dst) * 4 * base.sequence->datasize);
                    if (dst == NULL)
                        return RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);
                    
                    if (ztr.signal4->Type != NULL && memcmp("SLXN", ztr.signal4->Type, 5) == 0)
                        rc = fixup_trace_reorder(dst, (const uint16_t *)*data, ztr.signal4->offset, base.sequence->datasize);
                    else
                        rc = fixup_trace(dst, (const uint16_t *)*data, ztr.signal4->offset, base.sequence->datasize);
                    free(*data);
                    *data = ( uint8_t* ) dst;
                    *datasize = sizeof(*dst) * 4 * base.sequence->datasize;
                }
				break;
			default:
				break;
			}
			return rc;
		case 4:
			rc = undo_mode_4(data, datasize);
			if (rc)
				return rc;
			break;
		case 79:
			rc = undo_mode_79(data, datasize);
			if (rc)
				return rc;
			break;
		case 80:
			if (base.sequence->data == NULL || base.sequence->datasize == 0) {
				**(uint16_t **)data = 0;
				memmove(*data + 2, *data + 8, *datasize - 8);
				*datasize -= 6;
			}
			else {
				rc = undo_mode_80(data, datasize, base.sequence->data, base.sequence->datasize, ztr.signal4->offset);
                if (rc == 0)
                    ztr.signal4->datatype = f32;
                return rc;
			}
			break;
		default:
			rc = RC ( rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected );
			PLOGERR ( klogInt, ( klogInt, rc, "$(func) - don't know how to handle format '$(format)'",
                                             "func=ZTR_Decompress,format=%02X", *data ));
		}
	}
	return rc;
}

#if ZTR_USE_EXTRA_META
rc_t ILL_ZTR_ProcessBlock(ZTR_Context *ctx, ztr_raw_t *ztr_raw, ztr_t *dst, enum ztr_chunk_type *chunkType, nvp_t **extraMeta, int *extraMetaCount)
#else
rc_t ILL_ZTR_ProcessBlock(ZTR_Context *ctx, ztr_raw_t *ztr_raw, ztr_t *dst, enum ztr_chunk_type *chunkType)
#endif
{
    rc_t rc;
    const uint8_t *ptype;
    size_t metasize, datasize;
    uint8_t *data;
    const char *metadata;
    int count;
    enum ztr_chunk_type type = 0;
    enum ztr_data_type datatype = 0;
    nvp_t *meta = NULL;
    ztr_t ztr;

    assert(ctx);
    assert(ztr_raw);
    assert(dst);
    assert(chunkType);

    *chunkType = none;
    *(void **)dst = NULL;
    *(void **)&ztr = NULL;
	
    ptype = ztr_raw->type;
    count = 0;
    metasize = ztr_raw->metalength;
    if (metasize != 0) {
        metadata = (char *)ztr_raw->meta;

        count = count_pairs(metadata, metasize);
        if (count < 0) {
            rc = RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );
            PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk metadata malformed pairs",
                                 "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
            return rc;
        }

		if (count == 0)
			meta = NULL;
        else {
			meta = calloc(count, sizeof(*meta));
			if (meta == NULL) {
				rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
				PLOGERR ( klogSys, ( klogSys, rc, "$(func) - '$(type)' chunk metadata pair",
						  "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
				return rc;
			}
			parse_meta(meta, count, metadata, metadata + metasize);
		}
    }
    
    if (memcmp(ptype, "BASE", 4) == 0) {
        type = BASE;
        datatype = i8;
    }
    else if (memcmp(ptype, "BPOS", 4) == 0) {
        type = BPOS;
        datatype = i32;
    }
    else if (memcmp(ptype, "CLIP", 4) == 0) {
        type = CLIP;
        datatype = i32;
    }
    else if (memcmp(ptype, "CNF1", 4) == 0) {
        type = CNF1;
        datatype = i8;
    }
    else if (memcmp(ptype, "CNF4", 4) == 0) {
        type = CNF4;
        datatype = i8;
    }
    else if (memcmp(ptype, "COMM", 4) == 0) {
        type = COMM;
    }
    else if (memcmp(ptype, "CR32", 4) == 0) {
        type = CR32;
        datatype = i32;
    }
    else if (memcmp(ptype, "HUFF", 4) == 0) {
        type = HUFF;
    }
    else if (memcmp(ptype, "REGN", 4) == 0) {
        type = REGN;
        datatype = i32;
    }
    else if (memcmp(ptype, "SAMP", 4) == 0) {
        type = SAMP;
        datatype = i16;
    }
    else if (memcmp(ptype, "SMP4", 4) == 0) {
        type = SMP4;
        datatype = i16v4;
    }
    else if (memcmp(ptype, "TEXT", 4) == 0) {
        type = TEXT;
    }

    metadata = meta_getValueForNameAndRemove(meta, count, "VALTYPE");
    if (metadata != NULL) {
        if (strcmp(metadata, "16 bit integer") == 0)
            datatype = i16;
        else if (strcmp(metadata, "32 bit integer") == 0)
            datatype = i32;
        else if (strcmp(metadata, "32 bit IEEE float") == 0)
            datatype = f32;
        else if (strcmp(metadata, "64 bit IEEE float") == 0)
            datatype = f64;
    }

    datasize = ztr_raw->datalength;
    data = ztr_raw->data;
#if 0
	if (datasize > 0 && *data == 0x4D) {
		if (data[1] < 128)
			return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
		rc = decompress_huffman(&ctx->huffman_table[data[1] - 128], (const uint8_t **)&data, &datasize);
		if (rc)
			return rc;
		ztr_raw->datalength = datasize;
		ztr_raw->data = (uint8_t *)data;
	}
#endif

    switch (type) {
        case BASE:
            ztr.sequence = malloc(sizeof(*ztr.sequence));
            if (ztr.sequence == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                                     "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.sequence->datatype = datatype;
            ztr.sequence->datasize = datasize;
            ztr.sequence->charset = (char *)meta_getValueForNameAndRemove(meta, count, "CSET");
            ztr.sequence->data = (uint8_t *)data;
            break;
        case BPOS:
            ztr.position = malloc(sizeof(*ztr.position));
            if (ztr.position == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                                     "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.position->datatype = datatype;
            ztr.position->datasize = datasize;
            ztr.position->data = (uint8_t *)data;
            break;
        case CLIP:
            ztr.clip = malloc(sizeof(ztr.clip));
            if (ztr.clip == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                                     "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.clip->left  = (((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3];
            ztr.clip->right = (((((data[4] << 8) | data[5]) << 8) | data[6]) << 8) | data[7];
            break;
        case CNF1:
            ztr.quality1 = malloc(sizeof(*ztr.quality1));
            if (ztr.quality1 == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                           "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.quality1->datatype = datatype;
            ztr.quality1->datasize = datasize;
            if ((ztr.quality1->scale = (char *)meta_getValueForNameAndRemove(meta, count, "SCALE")) == NULL)
                ztr.quality1->scale = "PH";
            ztr.quality1->data = (uint8_t *)data;
            break;
        case CNF4:
            ztr.quality4 = malloc(sizeof(*ztr.quality4));
            if (ztr.quality4 == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                           "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.quality4->datatype = datatype;
            ztr.quality4->datasize = datasize;
            if ((ztr.quality4->scale = (char *)meta_getValueForNameAndRemove(meta, count, "SCALE")) == NULL)
                ztr.quality4->scale = "PH";
            ztr.quality4->data = (uint8_t *)data;
            break;
        case COMM:
            ztr.comment = malloc(sizeof(*ztr.comment) + datasize);
            if (ztr.comment == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                           "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.comment->size = datasize;
            ztr.comment->text = (char *)data;
            break;
#if ZTR_USE_TEXT_CHUNK
#else
        case TEXT:
#endif
        case CR32:
            *chunkType = ignore;
#if 0
            if (ctx->checksum != ((((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3]))
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - checksum error", "func=ZTR_ProcessBlock" ));
                return rc;
            }
            ctx->checksum = 0;
#endif
            break;
        case HUFF:
            *chunkType = ignore;
			if (data[1] < 128)
				return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
			rc = handle_huffman_codes(&ctx->huffman_table[data[1] - 128], data + 2, datasize - 2);
			if (rc)
				return rc;
            break;
        case REGN:
        {
            int region_count;
            int n;
            
            metadata = meta_getValueForNameAndRemove(meta, count, "NAME");
            if (metadata) {
                const char *cp;
                
                region_count = 1;
                if (metadata[strlen(metadata) - 1] == ';')
                    region_count = 0;
                for (cp = metadata; (cp = strchr(cp, ';')) != NULL; ++cp)
                    ++region_count;
            }
            else
                region_count = 0;
            switch (datatype) {
                case i16:
                    if ((datasize & 1) != 0) {
                        --datasize;
                        data = ((uint8_t *)data) + 1;
                    }
                    if ((datasize >> 1) < (region_count - 1)) {
                        rc = RC ( rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient );
                        PLOGERR ( klogSys, ( klogSys, rc, "$(func) - '$(type)' chunk data",
                                  "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                        return rc;
                    }
                    break;
                case i32:
                    if ((datasize & 1) != 0) {
                        --datasize;
                        data = ((uint8_t *)data) + 1;
                    }
                    if ((datasize >> 2) < (region_count - 1)) {
                        rc = RC ( rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient );
                        PLOGERR ( klogSys, ( klogSys, rc, "$(func) - '$(type)' chunk data",
                                  "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                        return rc;
                    }
                    break;
                default:
                    break;
            }
            ztr.region = NULL;
            n = (size_t)(&ztr.region->region[region_count]) + (metadata ? (strlen(metadata) + 1) : 0);
            ztr.region = malloc(n);
            if (ztr.region == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogSys, ( klogSys, rc, "$(func) - '$(type)' chunk data",
                           "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.region->count = region_count;
            if (region_count > 0) {
                char *temp = (char *)&ztr.region->region[region_count];
                int i, st;
                
                strcpy(temp, metadata);
                for (i = st = 0; *temp != '\0'; ++temp) {
                    switch (st) {
                        case 0:
                            if (!isspace(*temp)) {
                                ztr.region->region[i].name = temp;
                                ++st;
                            }
                            break;
                        case 1:
                            if (*temp == ':') {
                                char *cp = temp - 1;
                                
                                while (isspace(*cp))
                                    *cp-- = '\0';
                                *temp = '\0';
                                ++st;
                            }
                            break;
                        case 2:
                            if (!isspace(*temp)) {
                                ztr.region->region[i].type = *temp;
                                if (i == region_count - 1)
                                    ztr.region->region[0].start = 0;
                                else
                                    switch (datatype) {
                                        case i16:
#if __BYTE_ORDER == __LITTLE_ENDIAN
                                            ztr.region->region[i + 1].start = bswap_16(((const int16_t *)data)[i]);
#else
                                            ztr.region->region[i + 1].start = ((const int16_t *)data)[i];
#endif
                                            break;
                                        case i32:
#if __BYTE_ORDER == __LITTLE_ENDIAN
                                            ztr.region->region[i + 1].start = bswap_32(((const int32_t *)data)[i]);
#else
                                            ztr.region->region[i + 1].start = ((const int32_t *)data)[i];
#endif
                                            break;
                                        default:
                                            break;
                                    }
                                ++i;
                                ++st;
                            }
                            break;
                        case 3:
                            if (*temp == ';')
                                st = 0;
                            break;
                    }
                }
            }
        }
            break;
        case SAMP:
            ztr.signal = malloc(sizeof(*ztr.signal));
            if (ztr.signal == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                           "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.signal->datatype = datatype;
            ztr.signal->datasize = datasize;
			metadata = (char *)meta_getValueForNameAndRemove(meta, count, "OFFS");
			ztr.signal->offset = metadata == NULL ? 0 : strtol(metadata, 0, 0);
            ztr.signal->channel = (char *)meta_getValueForNameAndRemove(meta, count, "TYPE");
            ztr.signal->data = (uint8_t *)data;
            break;
        case SMP4:
            ztr.signal4 = malloc(sizeof(*ztr.signal4));
            if (ztr.signal4 == NULL)
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                           "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                return rc;
            }
            *chunkType = type;
            ztr.signal4->datatype = datatype;
            ztr.signal4->datasize = datasize;
			metadata = (char *)meta_getValueForNameAndRemove(meta, count, "OFFS");
			ztr.signal->offset = metadata == NULL ? 0 : strtol(metadata, 0, 0);
            ztr.signal4->Type   = (char *)meta_getValueForNameAndRemove(meta, count, "TYPE");
            ztr.signal4->data = (uint8_t *)data;
            break;
#if ZTR_USE_TEXT_CHUNK
        case TEXT:
        {
            int n;
            const char *const end = (const char *)(data + datasize);
            const char *cp;

            for (cp = (const char *)data, n = 0; cp < end; ++n) {
                size_t len = strendlen(cp, end);
                
                if (len == 0)
                    break;
                cp += len + 1;
                len = strendlen(cp, end);
                cp += len + 1;
            }
            if (n != 0) {
                int i;

                ztr.xmeta = malloc(sizeof(*ztr.xmeta) + (n - 1) * sizeof(nvp_t));
                if (ztr.xmeta == NULL)
                {
                    rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                    PLOGERR ( klogErr, ( klogErr, rc, "$(func) - '$(type)' chunk data",
                               "func=ZTR_ProcessBlock,type=%.*s", 4, & ztr_raw->type ));
                    return rc;
                }
                *chunkType = type;
                ztr.xmeta->count = n;
                for (i = 0, cp = (const char *)data; i != n; ++i) {
                    size_t len = strendlen(cp, end);
                    
                    if (len == 0)
                        break;
                    ztr.xmeta->data[i].name = cp;
                    cp += len + 1;
                    ztr.xmeta->data[i].value = cp;
                    len = strendlen(cp, end);
                    cp += len + 1;
                }
            }
        }
            break;
#endif
        default:
            *chunkType = unknown;
            DEBUG_MSG(1, ("$(func) - chunk '$(type)' unrecognized", "func=ZTR_ProcessBlock,type=%.*s\n", 4, & ztr_raw->type ));
            break;
    }

    *dst = ztr;
#if ZTR_USE_EXTRA_META
    if (extraMeta != NULL && extraMetaCount != NULL) {
        int i, j, k;
        
        for (k = j = i = 0; i != count; ++i)
            if (meta[i].name != NULL) {
                k += strlen(meta[i].name) + strlen(meta[i].value) + 2;
                ++j;
            }
        *extraMetaCount = 0;
        *extraMeta = NULL;
        if (j > 0) {
            *extraMeta = malloc(sizeof(nvp_t) * j + k);
            if (*extraMeta != NULL) {
                char *temp = (char *)(*extraMeta + j);
                
                for (j = i = 0; i != count; ++i)
                    if (meta[i].name != NULL) {
                        strcpy(temp, meta[i].name);
                        (*extraMeta)[j].name = temp;
                        temp += strlen(temp) + 1;
                        strcpy(temp, meta[i].value);
                        (*extraMeta)[j].value = temp;
                        temp += strlen(temp) + 1;
                    }
            }
        }
    }
#endif
    if (meta != NULL)
        free(meta);
    return 0;
}

rc_t ILL_ZTR_CreateContext(ZTR_Context **ctx)
{
    rc_t rc;

    assert(ctx);

    *ctx = malloc(sizeof(**ctx));
    if (*ctx)
    {
        memset(*ctx, 0, sizeof(**ctx));
        return 0;
    }

    rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
    PLOGERR ( klogErr, ( klogErr, rc, "$(func)", "func=ZTR_CreateContext" ));
    return rc;
}

rc_t ILL_ZTR_AddToBuffer(ZTR_Context *ctx, const uint8_t *data, uint32_t datasize)
{
    rc_t rc;

    assert(ctx);
    if (ctx->buf[0] == NULL) {
        ctx->current = 0;
        ctx->buf[0] = data;
        ctx->buflen[0] = datasize;
        return 0;
    }
    if (ctx->buf[1] == NULL) {
        ctx->buf[1] = data;
        ctx->buflen[1] = datasize;
        return 0;
    }

    rc = RC ( rcSRA, rcFormatter, rcParsing, rcBuffer, rcExhausted );
    PLOGERR ( klogErr, ( klogErr, rc, "$(func) - both buffers are in use",
               "func=ZTR_AddToBuffer" ));
    return rc;
}


rc_t ILL_ZTR_ParseHeader(ZTR_Context *ctx)
{
    uint8_t src[10];
    int len;
    
    assert(ctx);
    
    len = ztr_read(ctx, src, 10);
    if (len != 10)
        return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient );

    if (memcmp(src, ZTR_SIG, 8) != 0)
        return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );

    if ((ctx->versMajor = src[8]) != 1)
        return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcBadVersion );

    ctx->versMinor = src[9];

    return 0;
}

rc_t ILL_ZTR_ContextRelease(ZTR_Context *self) {
    int i;
    
    if (self != NULL)
    {
        for (i = 0; i != 128; ++i) {
            free_huffman_table(self->huffman_table + i);
        }
        free(self);
    }
    return 0;
}

rc_t ILL_ZTR_BufferGetRemainder(ZTR_Context *ctx, const uint8_t **data, uint32_t *datasize)
{
    assert(ctx);
    assert(data);
    assert(datasize);
    
    *data = NULL;
    *datasize = ctx->buflen[0] + ctx->buflen[1] - ctx->current;
    if (*datasize != 0) {
        void *r = malloc(*datasize);
        if (r == NULL)
        {
            rc_t rc = RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
            PLOGERR ( klogErr, ( klogErr, rc, "$(func)", "func=ZTR_BufferGetRemainder" ));
            return rc;
        }
        ztr_read(ctx, r, *datasize);
        *data = r;
    }
    return 0;
}
