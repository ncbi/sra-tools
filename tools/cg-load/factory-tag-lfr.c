/*==============================================================================
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
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/printf.h>

typedef struct CGTagLfr CGTagLfr;
#define CGFILETYPE_IMPL CGTagLfr
#include "file.h"
#include "factory-cmn.h"
#include "factory-tag-lfr.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <os-native.h>

struct CGTagLfr {
    CGFileType dad;
    const CGLoaderFile* file;
    int64_t start_rowid;
    char spot_group[512];
    uint64_t records;
    /* headers */
    CGFIELD15_ASSEMBLY_ID assembly_id;
    CGFIELD15_BATCH_FILE_NUMBER batch_file_number;
    CGFIELD15_BATCH_OFFSET batch_offset;
    CGFIELD15_FIELD_SIZE field_size;
    CGFIELD15_GENERATED_AT generated_at;
    CGFIELD15_GENERATED_BY generated_by;
    CGFIELD15_LANE lane;
    CGFIELD15_LIBRARY library;
    CGFIELD15_SAMPLE sample;
    CGFIELD15_SLIDE slide;
    CGFIELD15_SOFTWARE_VERSION software_version;
    CGFIELD_WELL_ID wellId;
};

static
rc_t CC CGTagLfr_Header(const CGTagLfr* cself, const char* buf, const size_t len)
{
    rc_t rc = 0;
    size_t slen;
    CGTagLfr* self = (CGTagLfr*)cself;

    if( strncmp("ASSEMBLY_ID\t", buf, slen = 12) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->assembly_id, sizeof(self->assembly_id));
    } else if( strncmp("BATCH_FILE_NUMBER\t", buf, slen = 18) == 0 ) {
        rc = str2u32(&buf[slen], len - slen, &self->batch_file_number);
        if( self->batch_file_number < 1 ) {
            rc = RC(rcRuntime, rcFile, rcConstructing, rcItem, rcOutofrange);
        }
    } else if( strncmp("BATCH_OFFSET\t", buf, slen = 13) == 0 ) {
        rc = str2u64(&buf[slen], len - slen, &self->batch_offset);
    } else if( strncmp("FIELD_SIZE\t", buf, slen = 11) == 0 ) {
        rc = str2u32(&buf[slen], len - slen, &self->field_size);
    } else if( strncmp("GENERATED_AT\t", buf, slen = 13) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->generated_at, sizeof(self->generated_at));
    } else if( strncmp("GENERATED_BY\t", buf, slen = 13) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->generated_by, sizeof(self->generated_by));
    } else if( strncmp("LANE\t", buf, slen = 5) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->lane, sizeof(self->lane));
    } else if( strncmp("LIBRARY\t", buf, slen = 8) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->library, sizeof(self->library));
    } else if( strncmp("SAMPLE\t", buf, slen = 7) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->sample, sizeof(self->sample));
    } else if( strncmp("SLIDE\t", buf, slen = 6) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->slide, sizeof(self->slide));
    } else if( strncmp("SOFTWARE_VERSION\t", buf, slen = 17) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->software_version, sizeof(self->software_version));
    } else {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcName, rcUnrecognized);
    }
    return rc;
}

static
rc_t CGTagLfr_GetAssemblyId(const CGTagLfr* cself, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id)
{
    if( cself->assembly_id[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *assembly_id = cself->assembly_id;
    return 0;
}

static
rc_t CGTagLfr_GetSlide(const CGTagLfr* cself, const CGFIELD_SLIDE_TYPE** slide)
{
    if( cself->slide[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *slide = cself->slide;
    return 0;
}

static
rc_t CGTagLfr_GetLane(const CGTagLfr* cself, const CGFIELD_LANE_TYPE** lane)
{
    if( cself->lane[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *lane = cself->lane;
    return 0;
}

static
rc_t CGTagLfr_GetBatchFileNumber(const CGTagLfr* cself, const CGFIELD_BATCH_FILE_NUMBER_TYPE** batch_file_number)
{
    *batch_file_number = &cself->batch_file_number;
    return 0;
}

static
rc_t CGTagLfr_GetStartRow(const CGTagLfr* cself, int64_t* rowid)
{
    *rowid = cself->start_rowid;
    return 0;
}

static
rc_t CC CGTagLfr_GetTagLfr(const CGFILETYPE_IMPL* cself, TReadsData* data)
{
    rc_t rc = 0;
    uint16_t wellScore = 0;
    if( cself->start_rowid == 0 ) {
        ((CGTagLfr*)cself)->start_rowid = data->rowid;
    }
    CG_LINE_START(cself->file, b, len, p);
    if( b == NULL || len == 0) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInsufficient);
        break;
    }
    /* reads */
    CG_LINE_NEXT_FIELD(b, len, p);
    if (p - b != CG_TAG_LFR_DATA_LEN) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInvalid);
    }
    /* scores */
    CG_LINE_NEXT_FIELD(b, len, p);
    if (p - b != CG_TAG_LFR_DATA_LEN) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInvalid);
    }

    /* wellId */
    CG_LINE_NEXT_FIELD(b, len, p);
    if ((rc = str2u16(b, p - b, &((CGTagLfr*)cself)->wellId)) != 0) {
    }
    else if (cself->wellId < 0 || cself->wellId > 384) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
    }

    /* wellScore */
    CG_LINE_LAST_FIELD(b, len, p);
    rc = str2u16(b, p - b, &wellScore);

    if (rc == 0) {
        size_t w = 0;
        if (cself->wellId != 0) {
            rc = string_printf(((CGTagLfr*)cself)->spot_group,
                sizeof(cself->spot_group), &w, "%s-%s#%03d",
                cself->slide, cself->lane, cself->wellId);
        }
        else {
            rc = string_printf(((CGTagLfr*)cself)->spot_group,
                sizeof(cself->spot_group), &w, "%s-%s",
                cself->slide, cself->lane);
        }
        data->seq.spot_group.buffer = cself->spot_group;
        data->seq.spot_group.elements = w;
    }

    ((CGTagLfr*)cself)->records++;

    DEBUG_MSG(10, ("tag-lfr:  '%.*s'\t%d\n",
        (int32_t)data->seq.spot_group.elements, data->seq.spot_group.buffer, 
        cself->wellId));

    CG_LINE_END();

    return rc;
}

static
void CC CGTagLfr_Release(const CGTagLfr* cself, uint64_t* records)
{
    if( cself != NULL ) {
        CGTagLfr* self = (CGTagLfr*)cself;
        if( records != NULL ) {
            *records = cself->records;
        }
        free(self);
    }
}

static const CGFileType_vt CGTagLfr_vt =
{
    CGTagLfr_Header,
    NULL,
    CGTagLfr_GetStartRow,
    NULL,
    NULL,
    NULL,
    CGTagLfr_GetTagLfr, /* tag_lfr */
    CGTagLfr_GetAssemblyId,
    CGTagLfr_GetSlide,
    CGTagLfr_GetLane,
    CGTagLfr_GetBatchFileNumber,
    NULL,
    NULL,
    CGTagLfr_Release
};

rc_t CC CGTagLfr_Make(const CGFileType** cself, const CGLoaderFile* file)
{
    rc_t rc = 0;
    CGTagLfr* obj = NULL;
    
    if( cself == NULL || file == NULL ) {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcParam, rcNull);
    }
    if( rc == 0 ) {
        *cself = NULL;
        if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
            rc = RC(rcRuntime, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else {
            obj->file = file;
            obj->dad.type = cg_eFileType_TAG_LFR;
            obj->dad.vt = &CGTagLfr_vt;
        }
    }
    if( rc == 0 ) {
        *cself = &obj->dad;
    } else {
        CGTagLfr_Release(obj, NULL);
    }
    return rc;
}

