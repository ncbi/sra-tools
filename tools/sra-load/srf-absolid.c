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
#include <kapp/main.h>

typedef struct SRFAbsolidLoaderFmt SRFAbsolidLoaderFmt;

#define SRALOADERFMT_IMPL SRFAbsolidLoaderFmt
#include "loader-fmt.h"

#include "srf.h"
#include "ztr-absolid.h"
#include "srf-fmt.h"
#include "pstring.h"
#include "writer-absolid.h"
#include "srf-load.vers.h"
#include "debug.h"

#include <endian.h>
#include <byteswap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

typedef struct fe_context_t_struct {
    SRF_context ctx;
    bool skip_signal;
    const SRAWriteAbsolid* writer;

    pstring name_prefix;
    
    struct {
        uint8_t nreads;
        uint16_t start[ABSOLID_FMT_MAX_NUM_READS];
        pstring label[ABSOLID_FMT_MAX_NUM_READS];
        char cs_key[ABSOLID_FMT_MAX_NUM_READS];
        EAbisolidReadType type[ABSOLID_FMT_MAX_NUM_READS];
    } region;
} fe_context_t;

static
rc_t set_label_type(const char* label, pstring* name, EAbisolidReadType* type)
{
    rc_t rc = 0;

    assert(name && type);

    *type = AbsolidRead_Suffix2ReadType(label);

    if( *type == eAbisolidReadType_Unknown ) {
        DEBUG_MSG(3, ("read label is not recognized: '%s'\n", label));
    } else {
        const char* l = AbisolidReadType2ReadLabel[*type];
        rc = pstring_assign(name, l, strlen(l));
    }
    return rc;
}

static
rc_t fe_new_region(fe_context_t *self, size_t region_count, const region_t region[])
{
    rc_t rc = 0;
    int i;

    self->region.nreads = region_count / 2;
    DEBUG_MSG(3, ("REGION: %u -> %u reads\n", region_count, self->region.nreads));
    if( self->region.nreads <= 0 || self->region.nreads > ABSOLID_FMT_MAX_NUM_READS ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnsupported);
        SRALoaderFile_LOG(self->ctx.file, klogErr, rc, "read count $(c)", PLOG_U8(c), self->region.nreads);
    }
    for(i = 0; rc == 0 && i < self->region.nreads; i++ ) {
        int j = i * 2 + 1;
        self->region.start[i] = region[j].start;
        if( (rc = set_label_type(region[j].name, &self->region.label[i], &self->region.type[i])) != 0 ) {
            break;
        }
        self->region.cs_key[i] = region[j - 1].name[0];
        DEBUG_MSG(3, ("REGION[%u]: '%s', %u, '%c', start: %u\n",
                      i, self->region.label[i].data, self->region.type[i], self->region.cs_key[i], self->region.start[i]));
        switch(region[j].type) {
            case Biological:
            case Normal:
            case Paired:
            case Technical:
                break;
            default:
                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                SRALoaderFile_LOG(self->ctx.file, klogErr, rc, "read #$(read_id) type mismatch; expected $(expected), got $(got)",
                        "read_id=%u,expected=%s,got=%u", i, "(B|N|P|T)", region[j].type);
                return rc;
        }
    }
    if( rc == 0 &&
        self->region.nreads > 1 && self->region.type[0] == self->region.type[1] ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcDuplicate);
        SRALoaderFile_LOG(self->ctx.file, klogErr, rc, "both reads have same type", NULL);
    }
    return rc;
}

static
rc_t parse_v1_header(SRF_context *ctx, ZTR_Context *ztr_ctx, const uint8_t *data, size_t size)
{
    rc_t rc = 0;
    size_t parsed;
    char prefixType;
    uint32_t counter;
    ztr_raw_t ztr_raw;
    ztr_t ztr;
    enum ztr_chunk_type type;
    fe_context_t* fe = (fe_context_t*)ctx;
    
    if( (rc = SRF_ParseDataChunk(data, size, &parsed, &fe->name_prefix, &prefixType, &counter)) != 0 ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "parse_v1_header - failed to parse SRF chunk", NULL);
    }
    DEBUG_MSG(3, ("HEADER PREFIX: '%s'\n", fe->name_prefix.data));
    if((rc = ABI_ZTR_AddToBuffer(ztr_ctx, data + parsed, size - parsed)) != 0) {
        return rc;
    }
    if((rc = ABI_ZTR_ParseHeader(ztr_ctx)) != 0) {
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "parse_v1_header - failed to parse ZTR header", NULL);
    }
    while(rc == 0 && !ABI_ZTR_BufferIsEmpty(ztr_ctx)) {
        if((rc = ABI_ZTR_ParseBlock(ztr_ctx, &ztr_raw)) != 0) {
            return SRALoaderFile_LOG(ctx->file, klogErr, rc, "parse_v1_header - failed to parse ZTR chunk", NULL);
        }
        if((rc = ABI_ZTR_ProcessBlock(ztr_ctx, &ztr_raw, &ztr, &type)) != 0) {
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "parse_v1_header - failed to process ZTR chunk", NULL);
        }
        if(type == REGN) {
            rc = fe_new_region(fe, ztr.region->count, ztr.region->region);
        }
        if(*(void **)&ztr != NULL) {
            free(*(void **)&ztr);
        }
    }
    return rc;
}

static
rc_t fe_new_read(fe_context_t *self, pstring *readId, EAbisolidReadType* type, pstring* label)
{
    rc_t rc = 0;
    pstring name_suffix;
    const char* p;

    assert(self && readId && type && label);
    DEBUG_MSG(3, ("READ_LABEL: '%s'\n", readId->data));
    /* spot name suffix may end with '_(F|R).+' */
    p = strrchr(readId->data, '_');
    if( p != NULL ) {
        rc = set_label_type(p + 1, label, type);
        if( rc == 0 && *type > eAbisolidReadType_SPOT) {
            /* cut label */
            readId->len -= label->len + 1;
        }
    } else {
        pstring_clear(label);
        *type = eAbisolidReadType_SPOT;
    }
    if( rc == 0 && (rc = pstring_copy(&name_suffix, readId)) == 0 ) {
        rc = SRAWriteAbsolid_MakeName(&self->name_prefix, &name_suffix, readId);
    }
    return rc;
}

static
rc_t parse_v1_read(SRF_context *ctx, ZTR_Context *ztr_ctx, const uint8_t *data, size_t size)
{
    rc_t rc = 0;
    size_t i, parsed;
    ztr_raw_t ztr_raw;
    ztr_t ztr;
    enum ztr_chunk_type type;
    fe_context_t* fe = (fe_context_t*)ctx;

    uint8_t flags;
    pstring readId;
    EAbisolidReadType read_type;
    pstring label;

    AbsolidRead read[ABSOLID_FMT_MAX_NUM_READS];
        
    if( fe->region.nreads == 0 ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcNotFound);
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "missing region chunk before 1st read chunk", NULL);
    }
    if( (rc = SRF_ParseReadChunk(data, size, &parsed, &flags, &readId)) != 0 ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rc);
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "SRF parsing failure", NULL);
    }
    ABI_ZTR_AddToBuffer(ztr_ctx, data + parsed, size - parsed);

    /* readId will have spotname */
    if( (rc = fe_new_read(fe, &readId, &read_type, &label)) != 0 ) {
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "parsing spot name suffix", NULL);
    }
    for(i = 0; i < sizeof(read) / sizeof(read[0]); i++) {
        AbsolidRead_Init(&read[i]);
    }
    while(!ABI_ZTR_BufferIsEmpty(ztr_ctx)) {
        if( (rc = ABI_ZTR_ParseBlock(ztr_ctx, &ztr_raw)) != 0 ||
            (rc = ABI_ZTR_ProcessBlock(ztr_ctx, &ztr_raw, &ztr, &type)) != 0 ) {
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "ZTR parsing failure", NULL);
            break;
        }
        switch (type) {
        case BASE:
            if(ztr.sequence->datatype != i8) {
                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "read: expected 8-bit datatype", NULL);
            } else if( read_type > eAbisolidReadType_SPOT ) {
                int read_number = AbisolidReadType2ReadNumber[read_type];
                if( (rc = pstring_assign(&read[read_number].seq, ztr.sequence->data, ztr.sequence->datasize)) == 0 ) {
                    /* grab 1st, may be the only cs_key */
                    read[read_number].cs_key = fe->region.cs_key[0];
                    for(i = 1; i < fe->region.nreads; i++) {
                        if( read_type == fe->region.type[i] ) {
                            read[read_number].cs_key = fe->region.cs_key[i];
                            break;
                        }
                    }
                    SRF_set_read_filter(&read[read_number].filter, flags);
                    rc = pstring_copy(&read[read_number].label, &label);
                    DEBUG_MSG(3, ("SRF READ: '%s'\n", read[read_number].seq.data));
                }
                if( rc != 0 ) {
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "copying read", NULL);
                }
            } else {
                for(i = 0; rc == 0 && i < fe->region.nreads; i++) {
                    int read_number = AbisolidReadType2ReadNumber[fe->region.type[i]];
                    size_t len = (i + 1 >= fe->region.nreads ? ztr.sequence->datasize : fe->region.start[i + 1]) - fe->region.start[i];
                    rc = pstring_assign(&read[read_number].seq, &ztr.sequence->data[fe->region.start[i]], len);
                    read[read_number].cs_key = fe->region.cs_key[i];
                    SRF_set_read_filter(&read[read_number].filter, flags);
                    if( fe->region.label[i].len != 0 ) {
                        rc = pstring_copy(&read[read_number].label, &fe->region.label[i]);
                    }
                    DEBUG_MSG(3, ("SRF READ[%u]: '%s'\n", i, read[read_number].seq.data));
                }
                if( rc != 0 ) {
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "copying reads", NULL);
                }
            }
            break;
        case CNF1:
            if(ztr.quality1->datatype != i8) {
                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "quality: expected 8-bit datatype", NULL);
            } else if( read_type > eAbisolidReadType_SPOT ) {
                int read_number = AbisolidReadType2ReadNumber[read_type];
                if( (rc = pstring_assign(&read[read_number].qual, ztr.quality1->data, ztr.quality1->datasize)) == 0 ) {
                    DEBUG_MSG(3, ("SRF QUAL: %u bytes\n", read[read_number].qual.len));
                }
                if( rc != 0 ) {
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "copying quality", NULL);
                }
            } else {
                for(i = 0; rc == 0 && i < fe->region.nreads; i++) {
                    int read_number = AbisolidReadType2ReadNumber[fe->region.type[i]];
                    size_t len = (i + 1 >= fe->region.nreads ? ztr.quality1->datasize : fe->region.start[i + 1]) - fe->region.start[i];
                    rc = pstring_assign(&read[read_number].qual, &ztr.quality1->data[fe->region.start[i]], len);
                    DEBUG_MSG(3, ("SRF QUAL[%u]: %u bytes\n", i, read[read_number].qual.len));
                }
                if( rc != 0 ) {
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "copying qualities", NULL);
                }
            }
            break;
        case SAMP:
            if( !fe->skip_signal ) {
                size_t i;
                int stype = ABSOLID_FMT_COLMASK_NOTSET;
                if(ztr.signal->datatype != f32) {
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "signal: expected 32-bit float datatype", NULL);
                } else if( (ztr.signal->datasize % sizeof(float)) != 0 ) {
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "signal: size not 32-bit float aligned", NULL);
                } else if (ztr.signal->channel == NULL) {
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcIncomplete);
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "SIGNAL column: missing channel type", NULL);
                } else if(strcmp(ztr.signal->channel, "0FAM") == 0) {
                    stype = ABSOLID_FMT_COLMASK_FAM;
                } else if(strcmp(ztr.signal->channel, "1CY3") == 0) {
                    stype = ABSOLID_FMT_COLMASK_CY3;
                } else if(strcmp(ztr.signal->channel, "2TXR") == 0) {
                    stype = ABSOLID_FMT_COLMASK_TXR;
                } else if(strcmp(ztr.signal->channel, "3CY5") == 0) {
                    stype = ABSOLID_FMT_COLMASK_CY5;
                } else {
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "SIGNAL column: unexpected channel type", NULL);
                }
#if __BYTE_ORDER == __LITTLE_ENDIAN
                for(i = 0; rc == 0 && i < ztr.signal->datasize; i += 4) {
                    uint32_t* r = (uint32_t*)&ztr.signal->data[i];
                    *r = bswap_32(*r);
                }
#endif
                if( rc == 0 ) {
                    if( read_type > eAbisolidReadType_SPOT ) {
                        int read_number = AbisolidReadType2ReadNumber[read_type];
                        pstring* d = NULL;
                        switch(stype) {
                            case ABSOLID_FMT_COLMASK_FAM:
                                read[read_number].fs_type = eAbisolidFSignalType_FAM;
                                d = &read[read_number].fxx;
                                break;
                            case ABSOLID_FMT_COLMASK_CY3:
                                d = &read[read_number].cy3;
                               break;
                            case ABSOLID_FMT_COLMASK_TXR:
                                d = &read[read_number].txr;
                                break;
                            case ABSOLID_FMT_COLMASK_CY5:
                                d = &read[read_number].cy5;
                                break;
                        }
                        if( d ) {
                            rc = pstring_assign(d, ztr.signal->data, ztr.signal->datasize);
                            DEBUG_MSG(3, ("SRF SIGNAL[%s]: %u bytes\n", ztr.signal->channel, d->len));
                        } else {
                            rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnrecognized);
                        }
                        if( rc != 0 ) {
                            SRALoaderFile_LOG(ctx->file, klogErr, rc, "copying signal", NULL);
                        }
                    } else {
                        for(i = 0; rc == 0 && i < fe->region.nreads; i++) {
                            pstring* d = NULL;
                            int read_number = AbisolidReadType2ReadNumber[fe->region.type[i]];
                            size_t len = (i + 1 >= fe->region.nreads) ? ztr.signal->datasize : (fe->region.start[i + 1] * sizeof(float));
                            len -= fe->region.start[i] * sizeof(float);
                            switch(stype) {
                                case ABSOLID_FMT_COLMASK_FAM:
                                    read[read_number].fs_type = eAbisolidFSignalType_FAM;
                                    d = &read[read_number].fxx;
                                    break;
                                case ABSOLID_FMT_COLMASK_CY3:
                                    d = &read[read_number].cy3;
                                   break;
                                case ABSOLID_FMT_COLMASK_TXR:
                                    d = &read[read_number].txr;
                                    break;
                                case ABSOLID_FMT_COLMASK_CY5:
                                    d = &read[read_number].cy5;
                                    break;
                            }
                            if( d ) {
                                rc = pstring_assign(d, &ztr.signal->data[fe->region.start[i] * sizeof(float)], len);
                                DEBUG_MSG(3, ("SRF SIGNAL[%s]: %u bytes\n", ztr.signal->channel, d->len));
                            } else {
                                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnrecognized);
                            }
                        }
                        if( rc != 0 ) {
                            SRALoaderFile_LOG(ctx->file, klogErr, rc, "copying signals", NULL);
                        }
                    }
                }
            }
            break;
        default:
            break;
        }
        if(type != none && type != ignore) {
            free(*(void **)&ztr);
        }
    }
    if(rc == 0) {
        if( read_type <= eAbisolidReadType_SPOT ) {
            rc = SRAWriteAbsolid_Write(fe->writer, ctx->file, &readId, NULL, &read[0], &read[1]);
        } else {
            switch( AbisolidReadType2ReadNumber[read_type] ) {
                case 0:
                    rc = SRAWriteAbsolid_Write(fe->writer, ctx->file, &readId, NULL, &read[0], NULL);
                    break;
                case 1:
                    rc = SRAWriteAbsolid_Write(fe->writer, ctx->file, &readId, NULL, NULL, &read[1]);
                    break;
                default:
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnsupported);
                    SRALoaderFile_LOG(ctx->file, klogErr, rc, "more than 2 reads", NULL);
                    break;
            }
        }
    }
    return rc;
}

struct SRFAbsolidLoaderFmt {
    SRALoaderFmt dad;
    fe_context_t fe;
};

static
rc_t SRFAbsolidLoaderFmt_WriteData(SRFAbsolidLoaderFmt *self, uint32_t argc, const SRALoaderFile *const argv [], int64_t* spots_bad_count)
{
    rc_t rc = 0;
    uint32_t i;
    
    for(i = 0; rc == 0 && i < argc; i++) {
        self->fe.ctx.file = argv[i];
        if( (rc = SRALoaderFileName(argv[i], &self->fe.ctx.file_name)) == 0 ) {
            rc = SRF_parse(&self->fe.ctx, parse_v1_header, parse_v1_read, ABI_ZTR_CreateContext, ABI_ZTR_ContextRelease);
        }
    }
    return rc;
}

static
rc_t SRFAbsolidLoaderFmt_Whack(SRFAbsolidLoaderFmt* self, SRATable** table)
{
    SRAWriteAbsolid_Whack(self->fe.writer, table);
    free(self);
    return 0;
}

static
rc_t SRFAbsolidLoaderFmt_Version( const SRFAbsolidLoaderFmt* self, uint32_t* vers, const char** name )
{
    *vers = SRF_LOAD_VERS;
    *name = "AB SOLiD SRF";
    return 0;
}

static SRALoaderFmt_vt_v1 SRFAbsolidLoaderFmt_vt_v1 =
{
    1, 0,
    SRFAbsolidLoaderFmt_Whack,
    SRFAbsolidLoaderFmt_Version,
    NULL,
    SRFAbsolidLoaderFmt_WriteData
};

rc_t SRFABSolidLoaderFmt_Make(SRALoaderFmt** self, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SRFAbsolidLoaderFmt* fmt;

    if( self == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }

    fmt = calloc(1, sizeof(*fmt));
    if(fmt == NULL) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        LOGERR(klogInt, rc, "failed to initialize; out of memory");
        return rc;
    }
    if( (rc = SRALoaderFmtInit(&fmt->dad, (const SRALoaderFmt_vt*)&SRFAbsolidLoaderFmt_vt_v1)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize parent object");
    } else if( (rc = SRAWriteAbsolid_Make(&fmt->fe.writer, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize writer");
    }
    if( rc == 0 ) {
        fmt->fe.skip_signal = (config->columnFilter & (efltrSIGNAL | efltrDEFAULT));
        *self = &fmt->dad;
    } else {
        free(fmt);
    }
    return rc;
}
