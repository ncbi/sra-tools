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
#include <klib/rc.h>
#include <klib/log.h>

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "srf.h"
#include "ztr.h"
#include "srf-fmt.h"
#include "experiment-xml.h"
#include "debug.h"


const char UsageDefaultName[] = "srf-load";

extern rc_t SRFIlluminaLoaderFmt_Make(SRALoaderFmt **self, const SRALoaderConfig *config);

extern rc_t SRFABSolidLoaderFmt_Make(SRALoaderFmt **self, const SRALoaderConfig *config);

rc_t SRALoaderFmtMake(SRALoaderFmt **self, const SRALoaderConfig *config)
{
    rc_t rc = 0;
    const PlatformXML* platform;

    if( self == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    if( (rc = Experiment_GetPlatform(config->experiment, &platform)) == 0 ) {
        switch(platform->id) {
            case SRA_PLATFORM_ILLUMINA:
                rc = SRFIlluminaLoaderFmt_Make(self, config);
                break;
            case SRA_PLATFORM_ABSOLID:
                rc = SRFABSolidLoaderFmt_Make(self, config);
                break;

            default:
                rc = RC(rcSRA, rcFormatter, rcConstructing, rcFormat, rcUnknown);
        }
    }
    return rc;
}

/* BIGGER than internal SRALoaderFile buffer chunks handling
 * skips 'skipover' bytes in buffer and makes 'bsize' bytes available via 'data',
 * caller may want to skip 'skipover' returned back after 'data' is used
 * call with all params zero to deallocated internal buffer
 */
static
rc_t SRF_parse_prepdata(SRF_context* ctx, uint64_t bsize, const uint8_t** data, size_t* skipover)
{
    rc_t rc = 0;
    static size_t big_buffer_sz = 0;
    static uint8_t* big_buffer = NULL;

    if( ctx == NULL && bsize == 0 && data == NULL && skipover == NULL ) {
        free(big_buffer);
        big_buffer = NULL;
        big_buffer_sz = 0;
        return 0;
    }
    assert(ctx != NULL), assert(data != NULL), assert(skipover != NULL);

    /* prepare bsize bytes in buffer */
    rc = SRALoaderFileRead(ctx->file, *skipover, bsize, (const void**)&ctx->file_buf, &ctx->file_buf_sz);
    if( rc != 0 || ctx->file_buf == NULL || ctx->file_buf_sz == 0 || ctx->file_buf_sz > bsize ) {
        if( GetRCObject(rc) != (enum RCObject)rcBuffer || GetRCState(rc) != rcInsufficient ) {
            rc = rc ? rc : RC(rcSRA, rcFormatter, rcParsing, rcData, ctx->file_buf_sz > bsize ? rcExcessive : rcInsufficient);
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "expected $(expected) bytes chunk", PLOG_U64(expected), bsize);
            return rc;
        }
        rc = 0;
    }
    if( ctx->file_buf_sz == bsize ) {
        /* data fitted in file buffer, use it directly */
        *data = ctx->file_buf;
        *skipover = bsize;
    } else {
        size_t to_read = bsize, inbuf = 0;

        DEBUG_MSG(3, ("file buffer overflow on chunk %lu bytes\n", bsize));
        if( big_buffer_sz < bsize ) {
            uint8_t* d = realloc(big_buffer, bsize);
            DEBUG_MSG(3, ("grow internal chunk buffer to %lu bytes\n", bsize));
            if (d == NULL) {
                rc = RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "reading chunk of $(bytes) bytes", PLOG_U64(bytes), bsize);
            } else {
                big_buffer = d;
                big_buffer_sz = bsize;
            }
        }
        while( rc == 0 && to_read > 0 ) {
            size_t x = to_read > ctx->file_buf_sz ? ctx->file_buf_sz : to_read;
            memcpy(&big_buffer[inbuf], ctx->file_buf, x);
            to_read -= x;
            inbuf += x;
            rc = SRALoaderFileRead(ctx->file, x, to_read, (const void**)&ctx->file_buf, &ctx->file_buf_sz);
            if( rc != 0 || ((ctx->file_buf == NULL || ctx->file_buf_sz == 0) && to_read > 0) ) {
                if( GetRCObject(rc) != (enum RCObject)rcBuffer || GetRCState(rc) != rcInsufficient ) {
                    rc = rc ? rc : RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "expected $(expected) bytes of $(chunk) chunk",
                                      PLOG_2(PLOG_U64(expected),PLOG_U64(chunk)), to_read, bsize);
                    return rc;
                }
                rc = 0;
            }
        }
        *data = big_buffer;
        *skipover = 0;
    }
    return rc;
}

rc_t SRF_parse(SRF_context *ctx, SRF_parse_header_func* header, SRF_parse_read_func* read,
               rc_t (*zcreate)(ZTR_Context **ctx), rc_t (*zrelease)(ZTR_Context *self))
{
    rc_t rc = 0;
    bool first_block = true;
    enum SRF_ChunkTypes type = SRF_ChunkTypeUnknown;
    uint64_t bsize;
    size_t dataOffset;
    size_t skipover = 0;
    ZTR_Context *ztr_ctx = NULL;
    const char* errmsg;
    
    while( rc == 0 ) {
        errmsg = NULL;
        /* SRF_ParseChunk needs 5-16 bytes to be in buffer */
        if( (rc = SRALoaderFileRead(ctx->file, skipover, 16, (const void**)&ctx->file_buf, &ctx->file_buf_sz)) != 0 ) {
            errmsg = "chunk start";
            break;
        }
        if( ctx->file_buf == NULL ) {
            /* EOF */
            break;
        }
        rc = SRF_ParseChunk(ctx->file_buf, ctx->file_buf_sz, &bsize, &type, &dataOffset);
        if( rc == rcInvalid ) {
            /* not a chunk; might be an index pointer, if so then skip it */
            skipover = 8;
            /* reset to start in case files were concatinated */
            first_block = true;
            rc = 0;
            continue;
        } else if( rc != 0 ) {
            rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rc);
            errmsg = "chunk head";
            break;
        }
        if( first_block && type != SRF_ChunkTypeContainer ) {
            rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcCorrupt);
            errmsg = "expected SRF container header";
            break;
        }
        if( type == SRF_ChunkTypeIndex ) {
            /* index is at least 16 bytes */
            if( ctx->file_buf_sz < 16 ) {
                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
                errmsg = "index chunk";
                break;
            }
            /* rest of the chunk index ignored */
            skipover = bsize;
        } else if( type == SRF_ChunkTypeXML ) {
            /* ignore XML chunk */
            skipover = bsize;
        } else {
            const uint8_t* data;
            bsize -= dataOffset;
            skipover = dataOffset;
            if( (rc = SRF_parse_prepdata(ctx, bsize, &data, &skipover)) == 0 ) {
                switch (type) {
                    case SRF_ChunkTypeContainer:
                        {{
                            unsigned versMajor = 0;
                            unsigned versMinor = 0;
                            char contType = '\0';
                            if( (rc = SRF_ParseContainerHeader(data, bsize, &versMajor, &versMinor, &contType, NULL, NULL)) != 0 ) {
                                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rc);
                                errmsg = "container header";
                            }
                            if(versMajor != 1) {
                                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcBadVersion);
                            } else if(contType != 'Z') {
                                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
                                errmsg = "container type";
                            }
                            SRALoaderFile_LOG(ctx->file, klogInfo, 0, "parsing SRF v$(vers)", "vers=%u.%u", versMajor, versMinor);
                            first_block = false;
                        }}
                        break;

                    case SRF_ChunkTypeHeader:
                        if (ztr_ctx != NULL) {
                            (*zrelease)(ztr_ctx);
                            ztr_ctx = NULL;
                        }
                        if( (rc = (*zcreate)(&ztr_ctx)) == 0) {
                            rc = (*header)(ctx, ztr_ctx, data, bsize);
                        }
                        break;

                    case SRF_ChunkTypeRead:
                        rc = (*read)(ctx, ztr_ctx, data, bsize);
                        break;

                    default:
                        /* this is impossible */
                        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
                        errmsg = "unexpected chunk type";
                        break;
                }
            }
        }
    }
    SRF_parse_prepdata(NULL, 0, NULL, NULL); /* free internal buffer */
    (*zrelease)(ztr_ctx);
    if( rc != 0 ) {
        if( errmsg ) {
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "$(msg) - chunk type '$(type)'",
                             PLOG_2(PLOG_C(type),PLOG_S(msg)), type, errmsg);
        } else {
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "chunk type '$(type)'", PLOG_C(type), type);
        }
    }
    return rc;
}

void SRF_set_read_filter(uint8_t* filter, int SRF_flags)
{
    assert(filter != NULL);

    if( ( SRF_flags & SRF_READ_WITHDRAWN ) != 0 ) {
        *filter = SRA_READ_FILTER_REJECT;
    } else if( ( SRF_flags & SRF_READ_BAD ) != 0 ) {
        *filter = SRA_READ_FILTER_CRITERIA;
    } else {
        *filter = SRA_READ_FILTER_PASS;
    }
}
