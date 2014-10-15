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
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/container.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/checksum.h>
#include <klib/rc.h>
#include <kdb/manager.h>
#include <kdb/table.h>
#include <kdb/meta.h>
#include <kdb/index.h>

#include <sra/wsradb.h>
#include <sra/sradb-priv.h>
#include <sra/fastq.h>
#include <sra/sff.h>

#include "sra-makeidx.vers.h"
#include "zlib-simple.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

uint32_t g_file_block_sz = 32 * 1024;
const char* g_accession = NULL;
bool g_dump = false;
bool g_ungzip = false;

typedef struct SIndexObj_struct {
    KMDataNode* meta;
    const char* const file;
    const char* const format;
    const char* const index;
    rc_t (*func)(const SRATable* sratbl, struct SIndexObj_struct* obj, char* buffer, const size_t buffer_sz);
    uint64_t file_size;
    uint32_t buffer_sz;
    uint64_t minSpotId;
    uint64_t maxSpotId;
    SLList li;
    MD5State md5;
    uint8_t md5_digest[16];
} SIndexObj;

typedef struct SIndexNode_struct {
    SLNode n;
    uint64_t key;
    uint64_t key_size;
    int64_t id;
    uint64_t id_qty;
} SIndexNode;

typedef struct SIndexData_struct {
    rc_t rc;
    KIndex* kidx;
} SIndexData;

static
bool InsertIndexData( SLNode *node, void *data )
{
    SIndexNode* n = (SIndexNode*)node;
    SIndexData* d = (SIndexData*)data;

    d->rc = KIndexInsertU64(d->kidx, true, n->key, n->key_size, n->id, n->id_qty);
    return d->rc == 0 ? false : true;
}

static
void WhackIndexData( SLNode *n, void *data )
{
    free(n);
}

static
rc_t CommitIndex(KTable* ktbl, const char* name, const SLList* li)
{
    SIndexData data;

    STSMSG(0, ("Saving index %s", name));
    data.rc = KTableCreateIndex(ktbl, &data.kidx, kitU64, kcmInit, name);
    if( data.rc == 0 ) {
        if( !SLListDoUntil(li, InsertIndexData, &data) ) {
            data.rc = KIndexCommit(data.kidx);
        }
        KIndexRelease(data.kidx);
    }
    return data.rc;
}

rc_t WriteFileMeta(SIndexObj* obj)
{
    rc_t rc = 0;
    KMDataNode* nd = NULL;

    PLOGMSG(klogInfo, (klogInfo, "Meta $(f) on index $(i): file size $(s), buffer $(b)",
        PLOG_4(PLOG_S(f),PLOG_S(i),PLOG_U64(s),PLOG_U32(b)), obj->file, obj->index, obj->file_size, obj->buffer_sz));

    if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(obj->meta, &nd, "Format")) == 0 ) {
        KMDataNode* opt = NULL;
        rc = KMDataNodeWriteCString(nd, obj->format);
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(nd, &opt, "Options")) == 0 ) {
            KMDataNode* ond = NULL;
            if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &ond, "accession")) == 0 ) {
                rc = KMDataNodeWriteCString(ond, g_accession);
                KMDataNodeRelease(ond);
            }
            if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &ond, "minSpotId")) == 0 ) {
                rc = KMDataNodeWriteB64(ond, &obj->minSpotId);
                KMDataNodeRelease(ond);
            }
            if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &ond, "maxSpotId")) == 0 ) {
                rc = KMDataNodeWriteB64(ond, &obj->maxSpotId);
                KMDataNodeRelease(ond);
            }
            KMDataNodeRelease(opt);
        }
        KMDataNodeRelease(nd);
    }

    if( rc == 0 && obj->file_size > 0 && (rc = KMDataNodeOpenNodeUpdate(obj->meta, &nd, "Size")) == 0 ) {
        rc = KMDataNodeWriteB64(nd, &obj->file_size);
        KMDataNodeRelease(nd);
    }

    if( rc == 0 && obj->buffer_sz > 0 && (rc = KMDataNodeOpenNodeUpdate(obj->meta, &nd, "Buffer")) == 0 ) {
        rc = KMDataNodeWriteB32(nd, &obj->buffer_sz);
        KMDataNodeRelease(nd);
    }

    if( rc == 0 && strlen(obj->index) > 0 && (rc = KMDataNodeOpenNodeUpdate(obj->meta, &nd, "Index")) == 0 ) {
        rc = KMDataNodeWriteCString(nd, obj->index);
        KMDataNodeRelease(nd);
    }

    if( rc == 0 && obj->file_size > 0 && (rc = KMDataNodeOpenNodeUpdate(obj->meta, &nd, "md5")) == 0 ) {
        char x[5];
        int i;
        for( i = 0; rc == 0 && i < sizeof(obj->md5_digest); i++ ) {
            int l = snprintf(x, 4, "%02x", obj->md5_digest[i]);
            rc = KMDataNodeAppend(nd, x, l);
        }
        KMDataNodeRelease(nd);
    }
    return rc;
}

static
rc_t SFF_Idx(const SRATable* sratbl, SIndexObj* obj, char* buffer, const size_t buffer_sz)
{
    rc_t rc = 0;
    const SFFReader* reader = NULL;

    if( (rc = SFFReaderMake(&reader, sratbl, g_accession, obj->minSpotId, obj->maxSpotId)) != 0 ) {
        return rc;
    } else {
        size_t written = 0;
        uint32_t blk = 0;
        SIndexNode* inode = NULL;

        while( rc == 0 ) {
            rc = SFFReader_GetNextSpotData(reader, buffer, buffer_sz, &written);
            if( blk >= g_file_block_sz || (GetRCObject(rc) == rcRow && GetRCState(rc) == rcExhausted) ) {
                inode->key_size = blk;
                SLListPushTail(&obj->li, &inode->n);
                DEBUG_MSG(5, ("SFF index closed spots %lu, offset %lu, block size %lu\n", inode->id_qty, inode->key, inode->key_size));
                inode = NULL;
                if( blk > obj->buffer_sz ) {
                    obj->buffer_sz = blk;
                }
                blk = 0;
            }
            if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcExhausted ) {
                rc = 0;
                break;
            }
            if( inode == NULL ) {
                spotid_t spotid = 0;
                if( (rc = SFFReaderCurrentSpot(reader, &spotid)) != 0 ) {
                    break;
                }
                inode = malloc(sizeof(SIndexNode));
                if( inode == NULL ) {
                    rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcExhausted);
                    break;
                }
                inode->key = obj->file_size;
                inode->key_size = 0;
                inode->id = spotid;
                inode->id_qty = 0;
                DEBUG_MSG(5, ("SFF index opened spot %ld, offset %lu\n", inode->id, inode->key));
                if( spotid == 1 ) {
                    char hd[10240];
                    size_t hd_sz = 0;
                    if( (rc = SFFReaderHeader(reader, 0, hd, sizeof(hd), &hd_sz)) == 0 ) {
                        obj->file_size += hd_sz;
                        blk += hd_sz;
                        MD5StateAppend(&obj->md5, hd, hd_sz);
                        if( g_dump ) {
                            fwrite(hd, hd_sz, 1, stderr);
                        }
                    }
                }
            }
            obj->file_size += written;
            blk += written;
            inode->id_qty++;
            MD5StateAppend(&obj->md5, buffer, written);
            if( g_dump ) {
                fwrite(buffer, written, 1, stderr);
            }
        }
        rc = rc ? rc : Quitting();
        if( rc != 0 ) {
            spotid_t spot = 0;
            SFFReaderCurrentSpot(reader, &spot);
            PLOGERR(klogErr, (klogErr, rc, "spot $(s)", PLOG_U32(s), spot));
        }
    }
    SFFReaderWhack(reader);
    return rc;
}

static
rc_t SFFGzip_Idx(const SRATable* sratbl, SIndexObj* obj, char* buffer, const size_t buffer_sz)
{
    rc_t rc = 0;
    uint16_t zlib_ver = ZLIB_VERNUM;
    const SFFReader* reader = NULL;

    if( (rc = SFFReaderMake(&reader, sratbl, g_accession, obj->minSpotId, obj->maxSpotId)) != 0 ) {
        return rc;
    } else {
        size_t written = 0;
        uint32_t blk = 0, spots_per_block = 0, proj_id_qty = 0;
        SIndexNode* inode = NULL;
        size_t z_blk = 0;
        size_t spots_buf_sz = g_file_block_sz * 100;
        size_t zbuf_sz = spots_buf_sz + 100;

        char* zbuf = malloc(zbuf_sz);
        char* spots_buf = malloc(spots_buf_sz);
        bool eof = false;

        if( zbuf == NULL || spots_buf == NULL ) {
            rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcExhausted);
        }
        while( rc == 0 ) {
            if( (rc = SFFReader_GetNextSpotData(reader, buffer, buffer_sz, &written)) == 0 ) {
                if( inode == NULL ) {
                    spotid_t spotid = 0;
                    if( (rc = SFFReaderCurrentSpot(reader, &spotid)) != 0 ) {
                        break;
                    }
                    inode = malloc(sizeof(SIndexNode));
                    if( inode == NULL ) {
                        rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcExhausted);
                        break;
                    }
                    inode->key = obj->file_size;
                    inode->key_size = 0;
                    inode->id = spotid;
                    inode->id_qty = 0;
                    DEBUG_MSG(5, ("%s open key: spot %ld, offset %lu\n", obj->index, inode->id, inode->key));
                    if( spotid == 1 ) {
                        char hd[10240];
                        size_t hd_sz = 0;
                        if( (rc = SFFReaderHeader(reader, 0, hd, sizeof(hd), &hd_sz)) == 0 ) {
                            if( hd_sz + written > spots_buf_sz ) {
                                rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcInsufficient);
                                break;
                            }
                            memcpy(&spots_buf[blk], hd, hd_sz);
                            blk += hd_sz;
                            if( g_dump ) {
                                fwrite(hd, hd_sz, 1, stderr);
                            }
                        }
                    }

                }
                if( blk + written > spots_buf_sz ) {
                    rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcInsufficient);
                    break;
                }
                inode->id_qty++;
                memcpy(&spots_buf[blk], buffer, written);
                blk += written;
                if( g_dump ) {
                    fwrite(buffer, written, 1, stderr);
                }
            }
            if( (eof = (GetRCObject(rc) == rcRow && GetRCState(rc) == rcExhausted)) ) {
                rc = 0;
                if( inode == NULL ) {
                    break;
                }
            }
            if( rc == 0 && (eof || 
                            (proj_id_qty == 0 && inode->id_qty > (spots_per_block * 0.95)) || 
                            (proj_id_qty > 0 && inode->id_qty >= proj_id_qty) ) ) {
                rc = ZLib_DeflateBlock(spots_buf, blk, zbuf, zbuf_sz, &z_blk);
                if( z_blk < g_file_block_sz ) {
                    /* project needed id_qty */
                    proj_id_qty = g_file_block_sz * inode->id_qty / z_blk * 1.05;
                    DEBUG_MSG(5, ("%s: project id qty %lu\n", obj->index, proj_id_qty));
                } else {
                    DEBUG_MSG(10, ("%s: no projection %lu > %lu\n", obj->index, z_blk, g_file_block_sz));
                }
            }
            if( rc == 0 && (eof || z_blk >= g_file_block_sz) ) {
                obj->file_size += z_blk;
                MD5StateAppend(&obj->md5, zbuf, z_blk);
                inode->key_size = z_blk;
                SLListPushTail(&obj->li, &inode->n);
                DEBUG_MSG(5, ("%s close key: spots %lu, size %lu, ratio %hu%%, raw %lu\n",
                         obj->index, inode->id_qty, inode->key_size, (uint16_t)(((float)(blk - z_blk)/blk)*100), blk));
                spots_per_block = inode->id_qty;
                inode = NULL;
                if( blk > obj->buffer_sz ) {
                    obj->buffer_sz = blk;
                }
                blk = 0;
                z_blk = 0;
                proj_id_qty = 0;
            }
            if( eof ) {
                break;
            }
        }
        rc = rc ? rc : Quitting();
        if( rc != 0 ) {
            spotid_t spot = 0;
            SFFReaderCurrentSpot(reader, &spot);
            PLOGERR(klogErr, (klogErr, rc, "spot $(s)", PLOG_U32(s), spot));
        }
        free(zbuf);
        free(spots_buf);
    }
    if( rc == 0 ) {
        KMDataNode* opt = NULL, *nd = NULL;

        if( (rc = KMDataNodeOpenNodeUpdate(obj->meta, &opt, "Format/Options")) != 0 ) {
            return rc;
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "ZlibVersion")) == 0 ) {
            rc = KMDataNodeWriteB16(nd, &zlib_ver);
            KMDataNodeRelease(nd);
        }
        KMDataNodeRelease(opt);
    }
    SFFReaderWhack(reader);
    return rc;
}

static
rc_t Fastq_Idx(const SRATable* sratbl, SIndexObj* obj, char* buffer, const size_t buffer_sz)
{
    rc_t rc = 0;
    const FastqReader* reader = NULL;

    uint8_t colorSpace = false;
    char* colorSpaceKey = "\0";
    uint8_t origFormat = false;
    uint8_t printLabel = true;
    uint8_t printReadId = true;
    uint8_t clipQuality = true;
    uint32_t minReadLen = 0;
    uint16_t qualityOffset = 0;

    {{
        const SRAColumn* c = NULL;
        const uint8_t *platform = SRA_PLATFORM_UNDEFINED;
        bitsz_t o, z;

        if( (rc = SRATableOpenColumnRead(sratbl, &c, "PLATFORM", sra_platform_id_t)) != 0 ) {
            return rc;
        }
        if( (rc = SRAColumnRead(c, 1, (const void **)&platform, &o, &z)) != 0 ) {
            return rc;
        }
        if( *platform == SRA_PLATFORM_ABSOLID ) {
            colorSpace = true;
        }
        SRAColumnRelease(c);
    }}

    if( (rc = FastqReaderMake(&reader, sratbl, g_accession,
                        colorSpace, origFormat, false, printLabel, printReadId,
                        !clipQuality, minReadLen, qualityOffset, colorSpaceKey[0],
                        obj->minSpotId, obj->maxSpotId)) != 0 ) {
        return rc;
    } else {
        KMDataNode* opt = NULL, *nd = NULL;

        if( (rc = KMDataNodeOpenNodeUpdate(obj->meta, &opt, "Format/Options")) != 0 ) {
            return rc;
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "colorSpace")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &colorSpace);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "colorSpaceKey")) == 0 ) {
            rc = KMDataNodeWrite(nd, colorSpaceKey, 1);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "origFormat")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &origFormat);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "printLabel")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &printLabel);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "printReadId")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &printReadId);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "clipQuality")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &clipQuality);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "minReadLen")) == 0 ) {
            rc = KMDataNodeWriteB32(nd, &minReadLen);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "qualityOffset")) == 0 ) {
            rc = KMDataNodeWriteB16(nd, &qualityOffset);
            KMDataNodeRelease(nd);
        }
        KMDataNodeRelease(opt);
    }

    if( rc == 0 ) {
        size_t written = 0;
        uint32_t blk = 0;
        SIndexNode* inode = NULL;

        while( rc == 0 ) {
            rc = FastqReader_GetNextSpotSplitData(reader, buffer, buffer_sz, &written);
            if( blk >= g_file_block_sz || (GetRCObject(rc) == rcRow && GetRCState(rc) == rcExhausted) ) {
                inode->key_size = blk;
                SLListPushTail(&obj->li, &inode->n);
                DEBUG_MSG(5, ("Fastq index closed spots %lu, offset %lu, block size %lu\n",
                                                            inode->id_qty, inode->key, inode->key_size));
                inode = NULL;
                if( blk > obj->buffer_sz ) {
                    obj->buffer_sz = blk;
                }
                blk = 0;
            }
            if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcExhausted ) {
                rc = 0;
                break;
            }
            if( inode == NULL ) {
                spotid_t spotid = 0;
                if( (rc = FastqReaderCurrentSpot(reader, &spotid)) != 0 ) {
                    break;
                }
                inode = malloc(sizeof(SIndexNode));
                if( inode == NULL ) {
                    rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcExhausted);
                    break;
                }
                inode->key = obj->file_size;
                inode->key_size = 0;
                inode->id = spotid;
                inode->id_qty = 0;
                DEBUG_MSG(5, ("Fastq index opened spot %ld, offset %lu\n", inode->id, inode->key));
            }
            inode->id_qty++;
            obj->file_size += written;
            blk += written;
            MD5StateAppend(&obj->md5, buffer, written);
            if( g_dump ) {
                fwrite(buffer, written, 1, stderr);
            }
        }
        rc = rc ? rc : Quitting();
        if( rc != 0 ) {
            spotid_t spot = 0;
            FastqReaderCurrentSpot(reader, &spot);
            PLOGERR(klogErr, (klogErr, rc, "spot $(s)", PLOG_U32(s), spot));
        }
    }
    FastqReaderWhack(reader);
    return rc;
}

static
rc_t FastqGzip_Idx(const SRATable* sratbl, SIndexObj* obj, char* buffer, const size_t buffer_sz)
{
    rc_t rc = 0;
    const FastqReader* reader = NULL;

    uint16_t zlib_ver = ZLIB_VERNUM;
    uint8_t colorSpace = false;
    char* colorSpaceKey = "\0";
    uint8_t origFormat = false;
    uint8_t printLabel = true;
    uint8_t printReadId = true;
    uint8_t clipQuality = true;
    uint32_t minReadLen = 0;
    uint16_t qualityOffset = 0;

    {{
        const SRAColumn* c = NULL;
        const uint8_t *platform = SRA_PLATFORM_UNDEFINED;
        bitsz_t o, z;

        if( (rc = SRATableOpenColumnRead(sratbl, &c, "PLATFORM", sra_platform_id_t)) != 0 ) {
            return rc;
        }
        if( (rc = SRAColumnRead(c, 1, (const void **)&platform, &o, &z)) != 0 ) {
            return rc;
        }
        if( *platform == SRA_PLATFORM_ABSOLID ) {
            colorSpace = true;
        }
        SRAColumnRelease(c);
    }}

    if( (rc = FastqReaderMake(&reader, sratbl, g_accession,
                        colorSpace, origFormat, false, printLabel, printReadId,
                        !clipQuality, minReadLen, qualityOffset, colorSpaceKey[0],
                        obj->minSpotId, obj->maxSpotId)) != 0 ) {
        return rc;
    } else {
        size_t written = 0;
        uint32_t blk = 0, spots_per_block = 0, proj_id_qty = 0;
        SIndexNode* inode = NULL;
        size_t z_blk = 0;
        size_t spots_buf_sz = g_file_block_sz * 100;
        size_t zbuf_sz = spots_buf_sz + 100;
        char* zbuf = malloc(zbuf_sz);
        char* spots_buf = malloc(spots_buf_sz);
        bool eof = false;

        if( zbuf == NULL || spots_buf == NULL ) {
            rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcExhausted);
        }
        while( rc == 0 ) {
            if( (rc = FastqReader_GetNextSpotSplitData(reader, buffer, buffer_sz, &written)) == 0 ) {
                if( inode == NULL ) {
                    spotid_t spotid = 0;
                    if( (rc = FastqReaderCurrentSpot(reader, &spotid)) != 0 ) {
                        break;
                    }
                    inode = malloc(sizeof(SIndexNode));
                    if( inode == NULL ) {
                        rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcExhausted);
                        break;
                    }
                    inode->key = obj->file_size;
                    inode->key_size = 0;
                    inode->id = spotid;
                    inode->id_qty = 0;
                    DEBUG_MSG(5, ("%s open key: spot %ld, offset %lu\n", obj->index, inode->id, inode->key));
                }
                if( blk + written > spots_buf_sz ) {
                    rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcInsufficient);
                    break;
                }
                inode->id_qty++;
                memcpy(&spots_buf[blk], buffer, written);
                blk += written;
                if( g_dump ) {
                    fwrite(buffer, written, 1, stderr);
                }
            }
            if( (eof = (GetRCObject(rc) == rcRow && GetRCState(rc) == rcExhausted)) ) {
                rc = 0;
                if( inode == NULL ) {
                    break;
                }
            }
            if( rc == 0 && (eof || 
                            (proj_id_qty == 0 && inode->id_qty > (spots_per_block * 0.95)) || 
                            (proj_id_qty > 0 && inode->id_qty >= proj_id_qty) ) ) {
                rc = ZLib_DeflateBlock(spots_buf, blk, zbuf, zbuf_sz, &z_blk);
                if( z_blk < g_file_block_sz ) {
                    /* project needed id_qty */
                    proj_id_qty = g_file_block_sz * inode->id_qty / z_blk * 1.05;
                    DEBUG_MSG(5, ("%s: project id qty %u\n", obj->index, proj_id_qty));
                } else {
                    DEBUG_MSG(10, ("%s: no projection %u > %u\n", obj->index, z_blk, g_file_block_sz));
                }
            }
            if( rc == 0 && (eof || z_blk >= g_file_block_sz) ) {
                obj->file_size += z_blk;
                MD5StateAppend(&obj->md5, zbuf, z_blk);
                inode->key_size = z_blk;
                SLListPushTail(&obj->li, &inode->n);
                DEBUG_MSG(5, ("%s close key: spots %lu, size %lu, ratio %hu%%, raw %u\n",
                         obj->index, inode->id_qty, inode->key_size, (uint16_t)(((float)(blk - z_blk)/blk)*100), blk ));
                spots_per_block = inode->id_qty;
                inode = NULL;
                if( blk > obj->buffer_sz ) {
                    obj->buffer_sz = blk;
                }
                blk = 0;
                z_blk = 0;
                proj_id_qty = 0;
            }
            if( eof ) {
                break;
            }
        }
        rc = rc ? rc : Quitting();
        if( rc != 0 ) {
            spotid_t spot = 0;
            FastqReaderCurrentSpot(reader, &spot);
            PLOGERR(klogErr, (klogErr, rc, "spot $(s)", PLOG_U32(s), spot));
        }
        free(zbuf);
        free(spots_buf);
    }
    if( rc == 0 ) {
        KMDataNode* opt = NULL, *nd = NULL;

        if( (rc = KMDataNodeOpenNodeUpdate(obj->meta, &opt, "Format/Options")) != 0 ) {
            return rc;
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "ZlibVersion")) == 0 ) {
            rc = KMDataNodeWriteB16(nd, &zlib_ver);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "colorSpace")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &colorSpace);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "colorSpaceKey")) == 0 ) {
            rc = KMDataNodeWrite(nd, colorSpaceKey, 1);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "origFormat")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &origFormat);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "printLabel")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &printLabel);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "printReadId")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &printReadId);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "clipQuality")) == 0 ) {
            rc = KMDataNodeWriteB8(nd, &clipQuality);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "minReadLen")) == 0 ) {
            rc = KMDataNodeWriteB32(nd, &minReadLen);
            KMDataNodeRelease(nd);
        }
        if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(opt, &nd, "qualityOffset")) == 0 ) {
            rc = KMDataNodeWriteB16(nd, &qualityOffset);
            KMDataNodeRelease(nd);
        }
        KMDataNodeRelease(opt);
    }
    FastqReaderWhack(reader);
    return rc;
}

static
rc_t MakeIndexes(const SRATable* stbl, KTable* ktbl, KMetadata* meta)
{
    rc_t rc = 0;
    int i;
    char* buffer = NULL;
    size_t buffer_sz = g_file_block_sz * 100;

    SIndexObj idx[] = {
     /*  meta, file,        format,         index,          func,    file_size, buffer_sz, minSpotId, maxSpotId */
        {NULL, "fastq",    "fastq",      "fuse-fastq",    Fastq_Idx,     0, 0, 0, 0},
        {NULL, "sff",      "SFF",        "fuse-sff",      SFF_Idx,       0, 0, 0, 0},
        {NULL, "fastq.gz", "fastq-gzip", "fuse-fastq-gz", FastqGzip_Idx, 0, 0, 0, 0},
        {NULL, "sff.gz",   "SFF-gzip",   "fuse-sff-gz",   SFFGzip_Idx,   0, 0, 0, 0}
    };

    for(i = 0; rc == 0 && i < sizeof(idx) / sizeof(idx[0]); i++) {
        KMDataNode* parent = NULL;
        if( (rc = KMetadataOpenNodeUpdate(meta, &parent, "/FUSE")) == 0 ) {
            KMDataNodeDropChild(parent, "root"); /* drop old stuff */
            if( g_ungzip || strcmp(&idx[i].file[strlen(idx[i].file) - 3], ".gz") == 0 ) {
                STSMSG(0, ("Preparing index %s", idx[i].index));
                MD5StateInit(&idx[i].md5);
                SLListInit(&idx[i].li);
                KMDataNodeDropChild(parent, "%s.tmp", idx[i].file);
                if( (rc = KMDataNodeOpenNodeUpdate(parent, &idx[i].meta, "%s.tmp", idx[i].file)) == 0 ) {
                    if( idx[i].func != NULL ) {
                        if( buffer == NULL ) {
                            if( (buffer = malloc(buffer_sz)) == NULL ) {
                                rc = RC(rcExe, rcIndex, rcConstructing, rcMemory, rcExhausted);
                                break;
                            }
                        }
                        rc = idx[i].func(stbl, &idx[i], buffer, buffer_sz);
                        if( rc == 0 ) {
                            MD5StateFinish(&idx[i].md5, idx[i].md5_digest);
                            rc = CommitIndex(ktbl, idx[i].index, &idx[i].li);
                        }
                    }
                    if( rc == 0 ) {
                        rc = WriteFileMeta(&idx[i]);
                    }
                    KMDataNodeRelease(idx[i].meta);
                }
                if( GetRCState(rc) == rcUnsupported ) {
                    KMDataNodeDropChild(parent, "%s", idx[i].file);
                    PLOGERR(klogWarn, (klogWarn, rc, "Index $(i) is not supported for this table", PLOG_S(i), idx[i].index));
                    rc = 0;
                } else if( rc == 0 ) {
                    char f[4096];
                    strcpy(f, idx[i].file);
                    strcat(f, ".tmp");
                    KMDataNodeDropChild(parent, "%s", idx[i].file);
                    rc = KMDataNodeRenameChild(parent, f, idx[i].file);
                }
            } else if( !g_ungzip ) {
                KTableDropIndex(ktbl, idx[i].index);
                KMDataNodeDropChild(parent, "%s", idx[i].file);
            }
            KMDataNodeDropChild(parent, "%s.tmp", idx[i].file);
            KMDataNodeRelease(parent);
        }
        SLListWhack(&idx[i].li, WhackIndexData, NULL);
    }
    free(buffer);
    return rc;
}

ver_t CC KAppVersion(void)
{
    return SRA_MAKEIDX_VERS;
}
const char* blocksize_usage[] = {"Index block size", NULL};
const char* accession_usage[] = {"Accession", NULL};

/* this enum must have same order as MainArgs array below */
enum OptDefIndex {
    eopt_BlockSize = 0,
    eopt_Accession,
    eopt_DumpIndex,
    eopt_noGzip
};

OptDef MainArgs[] =
{
    /* if you change order in this array, rearrange enum above accordingly! */
    {"block-size", "b", NULL, blocksize_usage, 1, true, false},
    {"accession", "a", NULL, accession_usage, 1, true, false},
    {"hidden-dump", "d", NULL, NULL, 1, false, false},
    {"hidden-nogzip", "g", NULL, NULL, 1, false, false}
};
const char* MainParams[] =
{
    /* if you change order in this array, rearrange enum above accordingly! */
    "size",
    "accession",
    NULL,
    NULL
};
const size_t MainArgsQty = sizeof(MainArgs) / sizeof(MainArgs[0]);

const char UsageDefaultName[] = "sra-makeidx";

rc_t CC UsageSummary (const char * name)
{
    return 0;
}

rc_t CC Usage(const Args* args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;
    int i;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    OUTMSG(( "\nUsage:\n\t%s [options] <table>\n\n", progname));

    for(i = 0; i < MainArgsQty; i++ ) {
        if( MainArgs[i].required && MainArgs[i].help ) {
            HelpOptionLine(MainArgs[i].aliases, MainArgs[i].name, MainParams[i], MainArgs[i].help);
        }
    }
    OUTMSG(("\nOptions:\n"));
    for(i = 0; i < MainArgsQty; i++ ) {
        if( !MainArgs[i].required && MainArgs[i].help ) {
            HelpOptionLine(MainArgs[i].aliases, MainArgs[i].name, MainParams[i], MainArgs[i].help);
        }
    }
    OUTMSG(("\n"));
    HelpOptionsStandard();
    HelpVersion(fullpath, KAppVersion());
    return rc;
}
rc_t KMain(int argc, char *argv[])
{
    rc_t rc = 0;
    Args* args = NULL;
    const char* errmsg = NULL, *table_dir = NULL;
    char accn[1024];
    
    if( (rc = ArgsMakeAndHandle(&args, argc, argv, 1, MainArgs, MainArgsQty)) == 0 ) {
        const char* blksz = NULL;
        uint32_t count, dump = 0, gzip = 0;

        if( (rc = ArgsParamCount(args, &count)) != 0 || count != 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, count > 1 ? rcExcessive : rcInsufficient);
            errmsg = "table";

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_BlockSize].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_BlockSize].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_BlockSize].name, 0, &blksz)) != 0 ) {
            errmsg = MainArgs[eopt_BlockSize].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_Accession].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_Accession].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_Accession].name, 0, &g_accession)) != 0 ) {
            errmsg = MainArgs[eopt_Accession].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_DumpIndex].name, &dump)) != 0 ) {
            errmsg = MainArgs[eopt_DumpIndex].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_noGzip].name, &gzip)) != 0 ) {
            errmsg = MainArgs[eopt_noGzip].name;
        }
        while( rc == 0 ) {
            long val = 0;
            char* end = NULL;

            if( blksz != NULL ) {
                errno = 0;
                val = strtol(blksz, &end, 10);
                if( errno != 0 || blksz == end || *end != '\0' || val <= 0 ) {
                    rc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
                    errmsg = MainArgs[eopt_BlockSize].name;
                    break;
                } else if( val <= 128 || val > (1024 * 1024 * 1024) ) {
                    rc = RC(rcExe, rcArgv, rcValidating, rcParam, rcEmpty);
                    errmsg = "block size invalid";
                    break;
                }
                g_file_block_sz = val;
            }
            if( (rc = ArgsParamValue(args, 0, &table_dir)) != 0 ) {
                errmsg = "table";
                break;
            }
            if( g_accession == NULL ) {
                const char* p = strchr(table_dir, '/');
                size_t l = 0;

                g_accession = accn;
                if( p == NULL ) {
                    p = strchr(table_dir, '\\');
                }
                strncpy(accn, p == NULL ? table_dir : p + 1, sizeof(accn) - 1);
                if( accn[0] == '\0' ) {
                    rc = RC(rcExe, rcArgv, rcValidating, rcParam, rcEmpty);
                    errmsg = "accession";
                }
                l = strlen(accn);
                if( accn[l - 1] == '/' || accn[l - 1] == '\\') {
                    accn[--l] = '\0';
                }
                if( strncmp(&accn[l - 9], ".lite.sra", 9) == 0 ) {
                    accn[l - 9] = '\0';
                } else if( strncmp(&accn[l - 4], ".sra", 4) == 0 ) {
                    accn[l - 4] = '\0';
                }
            }
            g_dump = dump > 0;
            g_ungzip = gzip > 0;
            break;
        }
    }
    if( rc == 0 ) {
        SRAMgr* smgr = NULL;
        KDBManager* kmgr = NULL;

        DEBUG_MSG(5, ("table %s, accession %s\n", table_dir, g_accession));
        if( (rc = SRAMgrMakeUpdate(&smgr, NULL)) == 0 ) {
            if( (rc = KDBManagerMakeUpdate(&kmgr, NULL)) == 0 ) {
                bool relock = true;
                if( (rc = KDBManagerUnlock(kmgr, table_dir)) != 0 ) {
                    relock = false;
                    rc = GetRCState(rc) == rcUnlocked ? 0 : rc;
                } else {
                    PLOGMSG(klogInfo, (klogInfo, "Table $(p) locked, unlocking", PLOG_S(p), table_dir));
                }
                if( rc == 0 ) {
                    KTable* ktbl = NULL;
                    if( (rc = KDBManagerOpenTableUpdate(kmgr, &ktbl, table_dir)) == 0 ) {
                        KMetadata* meta = NULL;
                        if( (rc = KTableOpenMetadataUpdate(ktbl, &meta)) == 0 ) {
                            const SRATable* stbl = NULL;
                            if( (rc = SRAMgrOpenTableRead(smgr, &stbl, table_dir)) == 0 ) {
                                rc = MakeIndexes(stbl, ktbl, meta);
                                SRATableRelease(stbl);
                            }
                        }
                        KMetadataRelease(meta);
                    }
                    KTableRelease(ktbl);
                }
                if( rc == 0 && relock ) {
                    rc = KDBManagerLock(kmgr, table_dir);
                }
                KDBManagerRelease(kmgr);
            }
            SRAMgrRelease(smgr);
        }
    }
    if( rc != 0 && rc != KLogLastErrorCode() ) {
        if( errmsg ) {
            Usage(args);
        }
        LOGERR(klogErr, rc, errmsg ? errmsg : "stop");
    }
    ArgsWhack(args);
    return rc;
}
