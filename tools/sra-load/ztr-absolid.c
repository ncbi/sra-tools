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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <endian.h>
#include <byteswap.h>
#include <assert.h>

#include "ztr-absolid.h"
#include "debug.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define BE2H16(X) (bswap_16(X))
#define BE2H32(X) (bswap_32(X))
#else
#define BE2H16(X) (X)
#define BE2H32(X) (X)
#endif

struct ZTR_Context {
    int versMajor;
    int versMinor;
    size_t current, buflen;
    const uint8_t *buf;
};

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
    return j >> 1;
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

bool ABI_ZTR_BufferIsEmpty(const ZTR_Context *ctx) {
	return (ctx->buf == NULL || ctx->buflen == 0 || ctx->current == ctx->buflen) ? true : false;
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

rc_t ABI_ZTR_ParseBlock(ZTR_Context *ctx, ztr_raw_t *ztr_raw) {
  assert(ctx);
  assert(ztr_raw);
    
  memset(ztr_raw, 0, sizeof(*ztr_raw));
	
  if (ABI_ZTR_BufferIsEmpty(ctx))
    return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcEmpty );
	
  if (ctx->current + 4 > ctx->buflen)
    return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient );

  *(uint32_t *)(ztr_raw->type) = *(uint32_t *)(ctx->buf + ctx->current);
  ctx->current += 4;
  if (ctx->current + 4 >= ctx->buflen)
    return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient );
  if (!isZTRchunk(ztr_raw->type))
    return RC(rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt);

  ztr_raw->metalength = BE2H32(*(uint32_t *)(ctx->buf + ctx->current));
  ctx->current += 4;
  if (ztr_raw->metalength != 0) {
    if (ctx->current + ztr_raw->metalength > ctx->buflen)
      return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );

    ztr_raw->meta = ctx->buf + ctx->current;
    ctx->current += ztr_raw->metalength;
    if (memchr(ztr_raw->meta, 0, ztr_raw->metalength) == NULL)
      return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );
  }
  if (ctx->current + 4 > ctx->buflen)
    return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient );

  ztr_raw->datalength = BE2H32(*(uint32_t *)(ctx->buf + ctx->current));
  ctx->current += 4;
  if (ztr_raw->datalength != 0) {
    if (ctx->current + ztr_raw->datalength > ctx->buflen)
      return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );

    ztr_raw->data = ctx->buf + ctx->current;
    ctx->current += ztr_raw->datalength;
  }
  return 0;
}

#if ZTR_USE_EXTRA_META
rc_t ABI_ZTR_ProcessBlock(ZTR_Context *ctx, const ztr_raw_t *ztr_raw, ztr_t *dst, enum ztr_chunk_type *chunkType, nvp_t **extraMeta, int *extraMetaCount)
#else
rc_t ABI_ZTR_ProcessBlock(ZTR_Context *ctx, const ztr_raw_t *ztr_raw, ztr_t *dst, enum ztr_chunk_type *chunkType)
#endif
{
    rc_t rc;
    const uint8_t *ptype;
    uint8_t format;
    size_t metasize, datasize;
    const uint8_t *data;
    const char *metadata;
    int count;
    enum ztr_chunk_type type;
    enum ztr_data_type datatype = i8;
    nvp_t *meta = NULL;
    ztr_t ztr;
	int padded = 0;

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
        assert(metadata);
        count = count_pairs(metadata, metasize);
        if (count < 0)
            return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );
		if (count != 0) {
			meta = malloc(sizeof(*meta) * count);
			if (meta == NULL)
				return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );

			parse_meta(meta, count, metadata, metadata + metasize);
		}
		else
			meta = NULL;
    }
    
    if (memcmp(ptype, "BASE", 4) == 0) {
        type = BASE;
        datatype = i8;
		padded = 1;
    }
    else if (memcmp(ptype, "BPOS", 4) == 0) {
        type = BPOS;
        datatype = i32;
		padded = 1;
    }
    else if (memcmp(ptype, "CLIP", 4) == 0) {
        type = CLIP;
        datatype = i32;
    }
    else if (memcmp(ptype, "CNF1", 4) == 0) {
        type = CNF1;
        datatype = i8;
		padded = 1;
    }
    else if (memcmp(ptype, "CNF4", 4) == 0) {
        type = CNF4;
        datatype = i8;
		padded = 1;
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
		padded = 1;
    }
    else if (memcmp(ptype, "SMP4", 4) == 0) {
        type = SMP4;
        datatype = i16v4;
		padded = 1;
    }
    else if (memcmp(ptype, "TEXT", 4) == 0) {
        type = TEXT;
    }
    else {
        type = unknown;
        DEBUG_MSG(1, ("$(func) - '$(type)' chunk unrecognized", "func=ZTR_ProcessBlock,type=%.*s\n", 4, & ztr_raw->type ));
    }
    {
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
    }
    datasize = ztr_raw->datalength;
    if (datasize == 0)
        data = NULL;
    else
    {
		static const char zero[8];
		
        data = ztr_raw->data;
        assert(data);
        format = *data;
		switch (format) {
			case 0:
				if (padded) {
					switch (datatype) {
						case i16:
						case i16v4:
							if (memcmp(data, zero, 2) == 0)
								padded = 2;
							break;
						case i32:
						case f32:
							if (memcmp(data, zero, 4) == 0)
								padded = 4;
							break;
						case f64:
							if (memcmp(data, zero, 8) == 0)
								padded = 8;
							break;
						default:
							break;
					}
					data += padded;
					datasize -= padded;
				}
				else {
					data += 1;
					datasize -= 1;
				}
				break;
			default:
				rc = RC ( rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected );
				PLOGERR ( klogWarn, ( klogWarn, rc,
                                                      "$(func) - '$(type)' unexpected ZTR chunk format '$(format)'",
                                                      "func=ZTR_ProcessBlock,type=%.*s,format=%02X", 4,
                                                      & ztr_raw->type, format ));
				return rc;
				break;
		}
    }
	
    switch (type) {
        case BASE:
            ztr.sequence = malloc(sizeof(*ztr.sequence));
            if (ztr.sequence == NULL)
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );

            *chunkType = type;
            ztr.sequence->datatype = datatype;
            ztr.sequence->datasize = datasize;
            ztr.sequence->charset = (char *)meta_getValueForNameAndRemove(meta, count, "CSET");
            ztr.sequence->data = (uint8_t *)data;
            break;
        case BPOS:
            ztr.position = malloc(sizeof(*ztr.position));
            if (ztr.position == NULL)
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
            
            *chunkType = type;
            ztr.position->datatype = datatype;
            ztr.position->datasize = datasize;
            ztr.position->data = (uint8_t *)data;
            break;
        case CLIP:
            ztr.clip = malloc(sizeof(ztr.clip));
            if (ztr.clip == NULL)
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
            
            *chunkType = type;
            ztr.clip->left  = (((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3];
            ztr.clip->right = (((((data[4] << 8) | data[5]) << 8) | data[6]) << 8) | data[7];
            break;
        case CNF1:
            ztr.quality1 = malloc(sizeof(*ztr.quality1));
            if (ztr.quality1 == NULL)
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
            
            *chunkType = type;
            ztr.quality1->datatype = datatype;
            ztr.quality1->datasize = datasize;
            if ((ztr.quality1->scale = (char *)meta_getValueForNameAndRemove(meta, count, "SCALE")) == NULL)
                ztr.quality1->scale = "PH";
            ztr.quality1->data = (uint8_t *)data;
            break;
        case CNF4:
            metadata = meta_getValueForNameAndRemove(meta, count, "SCALE");
            ztr.quality4 = malloc(sizeof(*ztr.quality4));
            if (ztr.quality4 == NULL)
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );

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
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
            *chunkType = type;
            ztr.comment->size = datasize;
            ztr.comment->text = (char *)data;
            break;
        case CR32:
            *chunkType = ignore;
#if 0
            if (ctx->checksum != ((((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3]))
            {
                rc = RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );
                PLOGERR (( klogErr, rc, "$(func) - checksum error", "func=ZTR_ProcessBlock" ));
                return rc;
            }
            ctx->checksum = 0;
#endif
            break;
        case HUFF:
            *chunkType = ignore;
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
            ztr.region = NULL;
            n = (size_t)(&ztr.region->region[region_count]) + (metadata ? (strlen(metadata) + 1) : 0);
            ztr.region = malloc(n);
            if (ztr.region == NULL)
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );

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
                                            ztr.region->region[i + 1].start = BE2H16(((const int16_t *)data)[i]);
                                            break;
                                        case i32:
                                            ztr.region->region[i + 1].start = BE2H32(((const int32_t *)data)[i]);
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
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
            
            *chunkType = type;
            ztr.signal->datatype = datatype;
            ztr.signal->datasize = datasize;
			metadata = meta_getValueForNameAndRemove(meta, count, "OFFS");
            ztr.signal->offset  = metadata == NULL ? 0 : strtol(metadata, NULL, 0);
            ztr.signal->channel = (char *)meta_getValueForNameAndRemove(meta, count, "TYPE");
            ztr.signal->data = (uint8_t *)data;
            break;
        case SMP4:
            ztr.signal4 = malloc(sizeof(*ztr.signal4));
            if (ztr.signal4 == NULL)
                return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
            
            *chunkType = type;
            ztr.signal4->datatype = datatype;
            ztr.signal4->datasize = datasize;
			metadata = meta_getValueForNameAndRemove(meta, count, "OFFS");
            ztr.signal->offset  = metadata == NULL ? 0 : strtol(metadata, NULL, 0);
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
                    return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );
                
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
            *chunkType = ignore;
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

rc_t ABI_ZTR_CreateContext(ZTR_Context **ctx)
{
    assert(ctx);

    if ((*ctx = malloc(sizeof(**ctx))) == NULL)
        return RC ( rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted );

    memset(*ctx, 0, sizeof(**ctx));
    return 0;
}

rc_t ABI_ZTR_AddToBuffer(ZTR_Context *ctx, const uint8_t *data, uint32_t datasize) {
    assert(ctx);
	ctx->current = 0;
	ctx->buf = data;
	ctx->buflen = datasize;
	return 0;
}


rc_t ABI_ZTR_ParseHeader(ZTR_Context *ctx) {
    assert(ctx);
    
	if (ctx->current + 10 > ctx->buflen)
        return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient );

    if (memcmp(ctx->buf + ctx->current, ZTR_SIG, 8) != 0)
        return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt );

    if ((ctx->versMajor = ((uint8_t *)(ctx->buf + ctx->current))[8]) != 1)
        return RC ( rcSRA, rcFormatter, rcParsing, rcData, rcBadVersion );

    ctx->versMinor = ((uint8_t *)(ctx->buf + ctx->current))[9];    
	ctx->current += 10;
    return 0;
}

rc_t ABI_ZTR_ContextRelease(ZTR_Context *self) {
    if (self != NULL) {
        free(self);
    }
    return 0;
}
