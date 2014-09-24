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

typedef struct CGReads15 CGReads15;
#define CGFILETYPE_IMPL CGReads15
#include "file.h"
#include "factory-cmn.h"
#include "factory-reads.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <os-native.h>

struct CGReads15 {
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
};

static
rc_t CC CGReads15_Header(const CGReads15* cself, const char* buf, const size_t len)
{
    rc_t rc = 0;
    size_t slen;
    CGReads15* self = (CGReads15*)cself;

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
rc_t CGReads15_GetAssemblyId(const CGReads15* cself, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id)
{
    if( cself->assembly_id[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *assembly_id = cself->assembly_id;
    return 0;
}

static
rc_t CGReads15_GetSlide(const CGReads15* cself, const CGFIELD_SLIDE_TYPE** slide)
{
    if( cself->slide[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *slide = cself->slide;
    return 0;
}

static
rc_t CGReads15_GetLane(const CGReads15* cself, const CGFIELD_LANE_TYPE** lane)
{
    if( cself->lane[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *lane = cself->lane;
    return 0;
}

static
rc_t CGReads15_GetBatchFileNumber(const CGReads15* cself, const CGFIELD_BATCH_FILE_NUMBER_TYPE** batch_file_number)
{
    *batch_file_number = &cself->batch_file_number;
    return 0;
}

static
rc_t CC CGReads15_Read(const CGReads15* cself, TReadsData* data)
{
    rc_t rc = 0;

    if( cself->start_rowid == 0 ) {
        ((CGReads15*)cself)->start_rowid = data->rowid;
    }
    CG_LINE_START(cself->file, b, len, p);
    if( b == NULL || len == 0) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcDone);
        break;
    }
    /*DEBUG_MSG(10, ("reads: '%.*s'\n", len, b));*/
    CG_LINE_NEXT_FIELD(b, len, p);
    if( (rc = str2u16(b, p - b, &data->flags)) != 0 ) {
    } else if( data->flags > 10 ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
    } else if( (data->flags & 0x03) == 3 || (data->flags & 0x07) == 7 ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInvalid);
    }
    CG_LINE_NEXT_FIELD(b, len, p);
    data->seq.sequence.elements = p - b;
    if( data->seq.sequence.elements != CG_READS_SPOT_LEN ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInvalid);
    } else {
        rc = str2buf(b, data->seq.sequence.elements, data->read, sizeof(data->read));
        /* clear cache, set in algnment writer */
        data->reverse[0] = '\0';
        data->reverse[CG_READS_SPOT_LEN / 2] = '\0';
    }
    CG_LINE_LAST_FIELD(b, len, p);
    data->seq.quality.elements = p - b;
    if( data->seq.quality.elements != CG_READS_SPOT_LEN ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInvalid);
    } else {
        rc = str2buf(b, data->seq.quality.elements, data->qual, sizeof(data->qual));
    }
    if( cself->records == 0 ) {
        size_t w;
        
#if 0
        rc = string_printf(((CGReads15*)cself)->spot_group, sizeof(cself->spot_group), &w, "%s:%s:%s:%04u",
                           cself->assembly_id, cself->slide, cself->lane, cself->batch_file_number);
#else
        rc = string_printf(((CGReads15*)cself)->spot_group, sizeof(cself->spot_group), &w, "%s-%s",
                           cself->slide, cself->lane);
#endif
        data->seq.spot_group.buffer = cself->spot_group;
        data->seq.spot_group.elements = w;
    }
    ((CGReads15*)cself)->records++;
    DEBUG_MSG(10, ("reads:  %u\t'%s'\t'%s'\n", data->flags, data->read, data->qual));
    CG_LINE_END();
    return rc;
}

static
rc_t CGReads15_GetStartRow(const CGReads15* cself, int64_t* rowid)
{
    *rowid = cself->start_rowid;
    return 0;
}

static
void CC CGReads15_Release(const CGReads15* cself, uint64_t* records)
{
    if( cself != NULL ) {
        CGReads15* self = (CGReads15*)cself;
        if( records != NULL ) {
            *records = cself->records;
        }
        free(self);
    }
}

static const CGFileType_vt CGReads15_vt =
{
    CGReads15_Header,
    CGReads15_Read,
    CGReads15_GetStartRow,
    NULL,
    NULL,
    NULL,
    NULL, /* tag_lfr */
    CGReads15_GetAssemblyId,
    CGReads15_GetSlide,
    CGReads15_GetLane,
    CGReads15_GetBatchFileNumber,
    NULL,
    NULL,
    CGReads15_Release
};

rc_t CC CGReads15_Make(const CGFileType** cself, const CGLoaderFile* file)
{
    rc_t rc = 0;
    CGReads15* obj = NULL;
    
    if( cself == NULL || file == NULL ) {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcParam, rcNull);
    }
    if( rc == 0 ) {
        *cself = NULL;
        if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
            rc = RC(rcRuntime, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else {
            obj->file = file;
            obj->dad.type = cg_eFileType_READS;
            obj->dad.vt = &CGReads15_vt;
        }
    }
    if( rc == 0 ) {
        *cself = &obj->dad;
    } else {
        CGReads15_Release(obj, NULL);
    }
    return rc;
}

rc_t CC CGReads13_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGReads15_Make(self, file); }

rc_t CC CGReads20_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGReads15_Make(self, file); }

rc_t CC CGReads22_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGReads15_Make(self, file); }
