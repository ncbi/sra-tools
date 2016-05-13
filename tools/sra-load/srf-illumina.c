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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

typedef struct SRFIlluminaLoaderFmt SRFIlluminaLoaderFmt;

#define SRALOADERFMT_IMPL SRFIlluminaLoaderFmt
#include "loader-fmt.h"

#include "srf.h"
#include "ztr.h"
#include "srf-fmt.h"
#include "ztr-illumina.h"
#include "writer-illumina.h"
#include "debug.h"

typedef struct fe_context_t_struct {

    SRF_context ctx;
    bool skip_intensity;
    bool skip_signal;
    bool skip_noise;

    const uint8_t *defered;
    uint32_t defered_len;
    
    const SRAWriterIllumina* writer;

    pstring name_prefix;

    IlluminaRead read;

    ztr_t sequence;
    ztr_t quality1;
    ztr_t quality4;
    ztr_t signal;
    ztr_t noise;
    ztr_t intensity;
} fe_context_t;

static
rc_t fe_new_read(fe_context_t *self, int flags, pstring *readId )
{
    rc_t rc;
    char *suffix;
    pstring readName, spotGroup;
    static IlluminaSpot spot;

    /* look for spot group */
    suffix = strchr(readId->data, '#');
    if( suffix != NULL ) {
        readId->len = suffix++ - readId->data;
        if( (rc = pstring_assign(&spotGroup, suffix, strlen(suffix))) != 0 ) {
            SRALoaderFile_LOG(self->ctx.file, klogInt, rc,
                "extracting barcode from spot '$(spotname)'", "spotname=%s", readId->data);
            return rc;
        }
    } else {
        pstring_clear(&spotGroup);
    }

    /* build the read name from prefix (self->name_prefix) and read id */
    if(self->name_prefix.len > 0 ) {
        if( (rc = pstring_copy(&readName, &self->name_prefix)) == 0 ) {
            if( isdigit(readName.data[readName.len - 1]) ) {
                rc = pstring_append(&readName, ":", 1);
            }
            if( rc == 0 ) {
                rc = pstring_concat(&readName, readId);
            }
        }
    } else {
        rc = pstring_copy(&readName, readId);
    }
    if( rc != 0 ) {
        SRALoaderFile_LOG(self->ctx.file, klogErr, rc,
            "preparing spot name $(spotname)", "spotname=%s", readId->data);
        return rc;
    }
    SRF_set_read_filter(&self->read.filter, flags);

    IlluminaSpot_Init(&spot);
    if( (rc = IlluminaSpot_Add(&spot, &readName, &spotGroup, &self->read)) == 0 ) {
        rc = SRAWriterIllumina_Write(self->writer, self->ctx.file, &spot);
    }
    return rc;
}

static
rc_t parse_header(SRF_context *ctx, ZTR_Context *ztr_ctx, const uint8_t *data, size_t size)
{
    rc_t rc = 0;
    size_t parsed;
    char prefixType;
    uint32_t counter;
    ztr_raw_t ztr_raw;
    ztr_t ztr;
    enum ztr_chunk_type type;
    fe_context_t* fe = (fe_context_t*)ctx;
    
    rc = SRF_ParseDataChunk(data, size, &parsed, &fe->name_prefix, &prefixType, &counter);
    if(rc) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "corrupt", NULL);
    }
    if(fe->defered != NULL) {
        free((void *)fe->defered);
        fe->defered = NULL;
    }
    if(parsed == size)
        return 0;
    
    rc = ZTR_AddToBuffer(ztr_ctx, data + parsed, size - parsed);
    if(rc)
        return rc;

    if((rc = ZTR_ParseHeader(ztr_ctx)) != 0) {
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "corrupt", NULL);
    }

    while (rc == 0 && !ZTR_BufferIsEmpty(ztr_ctx)) {
        if((rc = ZTR_ParseBlock(ztr_ctx, &ztr_raw)) != 0) {
            if(GetRCState(rc) == rcInsufficient && GetRCObject(rc) == (enum RCObject)rcData)
                rc = ZTR_BufferGetRemainder(ztr_ctx, &fe->defered, &fe->defered_len);
            break;
        }

        if((rc = ZTR_ProcessBlock(ztr_ctx, &ztr_raw, &ztr, &type)) != 0) {
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "corrupt", NULL);
            break;
        }
        if(*(void **)&ztr != NULL)
            free(*(void **)&ztr);

        if(ztr_raw.meta != NULL)
            free(ztr_raw.meta);
        if(ztr_raw.data != NULL)
            free(ztr_raw.data);
    }
    return rc;
}

static
rc_t parse_read(SRF_context *ctx, ZTR_Context *ztr_ctx, const uint8_t *data, size_t size)
{
    rc_t rc = 0;
    size_t parsed;
    uint8_t flags;
    pstring readId;
    ztr_raw_t ztr_raw;
    ztr_t ztr;
    enum ztr_chunk_type type;
    fe_context_t* fe = (fe_context_t*)ctx;

    *(void **)&fe->sequence =
    *(void **)&fe->quality1 =
    *(void **)&fe->quality4 =
    *(void **)&fe->signal =
    *(void **)&fe->intensity = 
    *(void **)&fe->noise = NULL;
    
    rc = SRF_ParseReadChunk(data, size, &parsed, &flags, &readId);
    if(rc) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rc);
        return SRALoaderFile_LOG(ctx->file, klogErr, rc, "corrupt", NULL);
    }
    if(fe->defered != NULL)
        ZTR_AddToBuffer(ztr_ctx, fe->defered, fe->defered_len);
    ZTR_AddToBuffer(ztr_ctx, data + parsed, size - parsed);
    if(fe->defered == NULL) {
        rc = ZTR_ParseBlock(ztr_ctx, &ztr_raw);
        if(rc == 0)
            goto PARSE_BLOCK;
        rc = ZTR_ParseHeader(ztr_ctx);
        if(rc) {
            return SRALoaderFile_LOG(ctx->file, klogErr, rc, "corrupt", NULL);
        }
    }
    
    while (!ZTR_BufferIsEmpty(ztr_ctx)) {
        rc = ZTR_ParseBlock(ztr_ctx, &ztr_raw);
    PARSE_BLOCK:
        if(rc != 0 || (rc = ZTR_ProcessBlock(ztr_ctx, &ztr_raw, &ztr, &type)) != 0 ) {
            return SRALoaderFile_LOG(ctx->file, klogErr, rc, "corrupt", NULL);
        }
        
        switch (type) {
            case READ:
                if(ztr.sequence->datatype != i8) {
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                    return SRALoaderFile_LOG(ctx->file, klogErr, rc, "invalid data type for sequence data", NULL);
                }
                fe->sequence = ztr;
                break;
            case QUALITY1:
                if(ztr.quality1->datatype != i8) {
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                    return SRALoaderFile_LOG(ctx->file, klogErr, rc, "invalid data type for quality1 data", NULL);
                }
                fe->quality1 = ztr;
                break;
            case QUALITY4:
                if(ztr.quality4->datatype != i8) {
                    rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnexpected);
                    return SRALoaderFile_LOG(ctx->file, klogErr, rc, "invalid data type for quality4 data", NULL);
                }
                fe->quality4 = ztr;
                break;
            case SIGNAL4:
                if(ztr.signal4->Type != NULL && strncmp(ztr.signal4->Type, "SLXI", 4) == 0 ) {
                    if( !fe->skip_intensity ) {
                        fe->intensity = ztr;
                    } else if(ztr.signal4){
			if(ztr.signal4->data) free(ztr.signal4->data);
			free(ztr.signal4);
		    }
                } else if(ztr.signal4->Type != NULL && strncmp(ztr.signal4->Type, "SLXN", 4) == 0 ) {
                    if( !fe->skip_noise ) {
                        fe->noise = ztr;
                    } else if(ztr.signal4){
			if(ztr.signal4->data) free(ztr.signal4->data);
			free(ztr.signal4);
                    }
                } else if( !fe->skip_signal ) {
                    fe->signal = ztr;
		} else if(ztr.signal4){
			if(ztr.signal4->data) free(ztr.signal4->data);
			free(ztr.signal4);
                }
                break;

            default:
                free(*(void **)&ztr);

            case none:
            case ignore:
                if(ztr_raw.data) {
                    free(ztr_raw.data);
                }
                break;
        }
	if(ztr_raw.meta){
		free(ztr_raw.meta);
		ztr_raw.meta=NULL;
	}
    }
    
    while(rc == 0) {
        if(*(void **)&fe->sequence == NULL) {
            rc = RC(rcSRA, rcFormatter, rcParsing, rcConstraint, rcViolated);
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "missing sequence data", NULL);
            break;
        }
        if(*(void **)&fe->quality4 == NULL && *(void **)&fe->quality1 == NULL) {
            rc = RC(rcSRA, rcFormatter, rcParsing, rcConstraint, rcViolated);
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "missing quality data", NULL);
            break;
        }

        if( (rc = ILL_ZTR_Decompress(ztr_ctx, BASE, fe->sequence, fe->sequence)) != 0 ||
            (rc = pstring_assign(&fe->read.seq, fe->sequence.sequence->data, fe->sequence.sequence->datasize)) != 0 ) {
            SRALoaderFile_LOG(ctx->file, klogErr, rc, "failed to decompress sequence data", NULL);
            break;
        }
        
        if( *(void **)&fe->quality4 != NULL ) {
            if( (rc = ILL_ZTR_Decompress(ztr_ctx, CNF4, fe->quality4, fe->sequence)) != 0 ||
                (rc = pstring_assign(&fe->read.qual, fe->quality4.quality4->data, fe->quality4.quality4->datasize)) != 0 ) {
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "failed to decompress quality4 data", NULL);
                break;
            }
            fe->read.qual_type = ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4;
        } else if( *(void **)&fe->quality1 != NULL ) {
            if( (rc = ILL_ZTR_Decompress(ztr_ctx, CNF1, fe->quality1, fe->sequence)) != 0 ||
                (rc = pstring_assign(&fe->read.qual, fe->quality1.quality1->data, fe->quality1.quality4->datasize)) != 0 ) {
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "failed to decompress quality1 data", NULL);
                break;
            }
            fe->read.qual_type = ILLUMINAWRITER_COLMASK_QUALITY_PHRED;
        }
        if( *(void **)&fe->signal != NULL ) {
            if( (rc = ILL_ZTR_Decompress(ztr_ctx, SMP4, fe->signal, fe->sequence)) != 0 ||
                (rc = pstring_assign(&fe->read.signal, fe->signal.signal4->data, fe->signal.signal4->datasize)) != 0 ) {
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "failed to decompress signal data", NULL);
                break;
            }
        }
        if( *(void **)&fe->intensity != NULL ) {
            if( (rc = ILL_ZTR_Decompress(ztr_ctx, SMP4, fe->intensity, fe->sequence)) != 0 ||
                (rc = pstring_assign(&fe->read.intensity, fe->intensity.signal4->data, fe->intensity.signal4->datasize)) != 0 ) {
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "failed to decompress intensity data", NULL);
                break;
            }
        }
        if( *(void **)&fe->noise != NULL ) {
            if( (rc = ILL_ZTR_Decompress(ztr_ctx, SMP4, fe->noise, fe->sequence)) != 0 ||
                (rc = pstring_assign(&fe->read.noise, fe->noise.signal4->data, fe->noise.signal4->datasize)) != 0 ) {
                SRALoaderFile_LOG(ctx->file, klogErr, rc, "failed to decompress noise data", NULL);
                break;
            }
        }
        rc = fe_new_read(fe, flags, &readId);
        break;
    }
    if(fe->sequence.sequence) {
        if(fe->sequence.sequence->data)
            free(fe->sequence.sequence->data);
        free(fe->sequence.sequence);
    }
    if(fe->quality1.quality1) {
        if(fe->quality1.quality1->data)
            free(fe->quality1.quality1->data);
        free(fe->quality1.quality1);
    }
    if(fe->quality4.quality4) {
        if(fe->quality4.quality4->data)
            free(fe->quality4.quality4->data);
        free(fe->quality4.quality4);
    }
    if(fe->signal.signal4) {
        if(fe->signal.signal4->data)
            free(fe->signal.signal4->data);
        free(fe->signal.signal4);
    }
    if(fe->intensity.signal4) {
        if(fe->intensity.signal4->data)
            free(fe->intensity.signal4->data);
        free(fe->intensity.signal4);
    }
    if(fe->noise.signal4) {
        if(fe->noise.signal4->data)
            free(fe->noise.signal4->data);
        free(fe->noise.signal4);
    }
    return rc;
}

struct SRFIlluminaLoaderFmt {
    SRALoaderFmt dad;
    fe_context_t fe;
};

static
rc_t SRFIlluminaLoaderFmt_WriteData(SRFIlluminaLoaderFmt *self, uint32_t argc, const SRALoaderFile *const argv [], int64_t* spots_bad_count)
{
    rc_t rc = 0;
    uint32_t i;

    for(i = 0; rc == 0 && i < argc; i++) {
        self->fe.ctx.file = argv[i];
        if( (rc = SRALoaderFileName(argv[i], &self->fe.ctx.file_name)) == 0 ) {
            rc = SRF_parse(&self->fe.ctx, parse_header, parse_read, ZTR_CreateContext, ZTR_ContextRelease);
        }
    }
    return rc;
}

static
rc_t SRFIlluminaLoaderFmt_Whack(SRFIlluminaLoaderFmt *self, SRATable** table)
{
    SRAWriterIllumina_Whack(self->fe.writer, table);
    free(self);
    return 0;
}

static
rc_t SRFIlluminaLoaderFmt_Version( const SRFIlluminaLoaderFmt *self, uint32_t *vers, const char** name )
{
    *vers = KAppVersion();
    *name = "Illumina SRF";
    return 0;
}

static SRALoaderFmt_vt_v1 SRFIlluminaLoaderFmt_vt_v1 =
{
    1, 0,
    SRFIlluminaLoaderFmt_Whack,
    SRFIlluminaLoaderFmt_Version,
    NULL,
    SRFIlluminaLoaderFmt_WriteData
};

rc_t SRFIlluminaLoaderFmt_Init(SRFIlluminaLoaderFmt* self, const SRALoaderConfig* config)
{
    rc_t rc = 0;

    self->fe.skip_signal = (config->columnFilter & (efltrSIGNAL | efltrDEFAULT));
    self->fe.skip_noise = (config->columnFilter & (efltrNOISE | efltrDEFAULT));
    self->fe.skip_intensity = (config->columnFilter & (efltrINTENSITY | efltrDEFAULT));

    if( (rc = SRAWriterIllumina_Make(&self->fe.writer, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize writer");
    }
    return rc;
}

rc_t SRFIlluminaLoaderFmt_Make(SRALoaderFmt** self, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SRFIlluminaLoaderFmt* fmt;

    if( self == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    fmt = calloc(1, sizeof(*fmt));
    if(fmt == NULL) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        LOGERR(klogInt, rc, "failed to initialize; out of memory");
        return rc;
    }
    if( (rc = SRFIlluminaLoaderFmt_Init(fmt, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize");
    } else if( (rc = SRALoaderFmtInit(&fmt->dad, (const SRALoaderFmt_vt*)&SRFIlluminaLoaderFmt_vt_v1)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize parent object");
    }
    if( rc == 0 ) {
        *self = &fmt->dad;
    } else {
        free(fmt);
    }
    return rc;
}
