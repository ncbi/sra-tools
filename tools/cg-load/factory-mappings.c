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

typedef struct CGMappings15 CGMappings15;
#define CGFILETYPE_IMPL CGMappings15
#include "file.h"
#include "factory-cmn.h"
#include "factory-mappings.h"
#include "debug.h"

#include <string.h>
#include <stdlib.h>

struct CGMappings15 {
    CGFileType dad;
    const CGLoaderFile* file;
    uint64_t records;
    /* headers */
    CGFIELD15_ASSEMBLY_ID assembly_id;
    CGFIELD15_BATCH_FILE_NUMBER batch_file_number;
    CGFIELD15_GENERATED_AT generated_at;
    CGFIELD15_GENERATED_BY generated_by;
    CGFIELD15_LANE lane;
    CGFIELD15_LIBRARY library;
    CGFIELD15_SAMPLE sample;
    CGFIELD15_SLIDE slide;
    CGFIELD15_SOFTWARE_VERSION software_version;
};

void CGMappings15_Release(const CGMappings15* cself, uint64_t* records)
{
    if( cself != NULL ) {
        CGMappings15* self = (CGMappings15*)cself;
        if( records != NULL ) {
            *records = cself->records;
        }
        free(self);
    }
}

static
rc_t CC CGMappings15_Header(const CGMappings15* cself, const char* buf, const size_t len)
{
    rc_t rc = 0;
    size_t slen;
    CGMappings15* self = (CGMappings15*)cself;

    if( strncmp("ASSEMBLY_ID\t", buf, slen = 12) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->assembly_id, sizeof(self->assembly_id));
    } else if( strncmp("BATCH_FILE_NUMBER\t", buf, slen = 18) == 0 ) {
        rc = str2u32(&buf[slen], len - slen, &self->batch_file_number);
        if( self->batch_file_number < 1 ) {
            rc = RC(rcRuntime, rcFile, rcConstructing, rcItem, rcOutofrange);
        }
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
rc_t CGMappings15_GetAssemblyId(const CGMappings15* cself, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id)
{
    if( cself->assembly_id[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *assembly_id = cself->assembly_id;
    return 0;
}

static
rc_t CGMappings15_GetSlide(const CGMappings15* cself, const CGFIELD_SLIDE_TYPE** slide)
{
    if( cself->slide[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *slide = cself->slide;
    return 0;
}

static
rc_t CGMappings15_GetLane(const CGMappings15* cself, const CGFIELD_LANE_TYPE** lane)
{
    if( cself->lane[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *lane = cself->lane;
    return 0;
}

static
rc_t CGMappings15_GetBatchFileNumber(const CGMappings15* cself, const CGFIELD_BATCH_FILE_NUMBER_TYPE** batch_file_number)
{
    *batch_file_number = &cself->batch_file_number;
    return 0;
}

static
rc_t CC CGMappings15_Read(const CGMappings15* cself, TMappingsData* data)
{
    rc_t rc = 0;
    TMappingsData_map* m = NULL;

    data->map_qty = 0;
    do {
        char tmp[2];
        CG_LINE_START(cself->file, b, len, p);
        if( b == NULL || len == 0 ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInsufficient);
            break;
        }
        m = &data->map[data->map_qty];
        m->saved = false;
        /*DEBUG_MSG(10, ("mappings %4u: '%.*s'\n", data->map_qty, len, b));*/
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2u16(b, p - b, &m->flags);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, m->chr, sizeof(m->chr));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i32(b, p - b, &m->offset);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i16(b, p - b, &m->gap[0]);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i16(b, p - b, &m->gap[1]);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i16(b, p - b, &m->gap[2]);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, tmp, sizeof(tmp));
        if( tmp[0] < 33 || tmp[0] > 126 ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
        }
        m->weight = tmp[0];
        CG_LINE_LAST_FIELD(b, len, p);
        if( (rc = str2u32(b, p - b, &m->mate)) != 0 ) {
        } else if( m->flags > 7 ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
        } else if( ++data->map_qty >= CG_MAPPINGS_MAX ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcBuffer, rcInsufficient);
        }
        ((CGMappings15*)cself)->records++;
        DEBUG_MSG(10, ("mappings %4u:  %u\t'%s'\t%u\t%i\t%i\t%i\t%c\t%u\n",
                        data->map_qty - 1, m->flags, m->chr, m->offset,
                        m->gap[0], m->gap[1], m->gap[2], m->weight, m->mate));
        CG_LINE_END();
    } while( rc == 0 && !(m->flags & cg_eLastDNBRecord) );
    if (rc == 0) {
        unsigned i;
        unsigned const n = data->map_qty;
        
        for (i = 0; i != n && rc == 0; ++i) {
            unsigned const mate = data->map[i].mate;
            
            if (mate > n)
                data->map[i].mate = i;
        }
    }
    return rc;
}

static
rc_t CC CGMappings22_Read(const CGMappings15* cself, TMappingsData* data)
{
    rc_t rc = 0;
    TMappingsData_map* m = NULL;

    data->map_qty = 0;
    do {
        char tmp[2];
        char armWeight = '\0';
        CG_LINE_START(cself->file, b, len, p);
        if( b == NULL || len == 0 ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInsufficient);
            break;
        }
        m = &data->map[data->map_qty];
        m->saved = false;
        /*DEBUG_MSG(10, ("mappings %4u: '%.*s'\n", data->map_qty, len, b));*/
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2u16(b, p - b, &m->flags);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, m->chr, sizeof(m->chr));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i32(b, p - b, &m->offset);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i16(b, p - b, &m->gap[0]);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i16(b, p - b, &m->gap[1]);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i16(b, p - b, &m->gap[2]);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, tmp, sizeof(tmp));
        if( tmp[0] < 33 || tmp[0] > 126 ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
        }
        m->weight = tmp[0];
        CG_LINE_NEXT_FIELD(b, len, p);
        if( (rc = str2u32(b, p - b, &m->mate)) != 0 ) {
        } else if( m->flags > 7 ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
        } else if( ++data->map_qty >= CG_MAPPINGS_MAX ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcBuffer, rcInsufficient);
        }
        CG_LINE_LAST_FIELD(b, len, p);
        rc = str2buf(b, p - b, tmp, sizeof(tmp));
        if (tmp[0] < 33 || tmp[0] > 126)
        {   rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange); }
        armWeight = tmp[0]; /* ignore armWeight */
        ((CGMappings15*)cself)->records++;
        DEBUG_MSG(10, ("mappings %4u:  %u\t'%s'\t%u\t%i\t%i\t%i\t%c\t%u\t%c\n",
            data->map_qty - 1, m->flags, m->chr, m->offset,
            m->gap[0], m->gap[1], m->gap[2], m->weight, m->mate, armWeight));
        CG_LINE_END();
    } while( rc == 0 && !(m->flags & cg_eLastDNBRecord) );
    if (rc == 0) {
        unsigned i;
        unsigned const n = data->map_qty;
        
        for (i = 0; i != n && rc == 0; ++i) {
            unsigned const mate = data->map[i].mate;
            
            if (mate > n)
                data->map[i].mate = n;
        }
    }
    return rc;
}

static const CGFileType_vt CGMappings15_vt =
{
    CGMappings15_Header,
    NULL,
    NULL,
    CGMappings15_Read,
    NULL,
    NULL,
    NULL, /* tag_lfr */
    CGMappings15_GetAssemblyId,
    CGMappings15_GetSlide,
    CGMappings15_GetLane,
    CGMappings15_GetBatchFileNumber,
    NULL,
    NULL,
    CGMappings15_Release
};

static const CGFileType_vt CGMappings22_vt =
{
    CGMappings15_Header,
    NULL,
    NULL,
    CGMappings22_Read,
    NULL,
    NULL,
    NULL, /* tag_lfr */
    CGMappings15_GetAssemblyId,
    CGMappings15_GetSlide,
    CGMappings15_GetLane,
    CGMappings15_GetBatchFileNumber,
    NULL,
    NULL,
    CGMappings15_Release
};

static
rc_t CC CGMappings_Make(const CGFileType** cself,
    const CGLoaderFile* file, const CGFileType_vt* vt)
{
    rc_t rc = 0;
    CGMappings15* obj = NULL;
    
    if( cself == NULL || file == NULL ) {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcParam, rcNull);
    } else {
        *cself = NULL;
        if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
            rc = RC(rcRuntime, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else {
            obj->file = file;
            obj->dad.type = cg_eFileType_MAPPINGS;
            obj->dad.vt = vt;
        }
    }
    if( rc == 0 ) {
        *cself = &obj->dad;
    } else {
        CGMappings15_Release(obj, NULL);
    }
    return rc;
}

rc_t CC CGMappings15_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGMappings_Make(self, file, &CGMappings15_vt); }

rc_t CC CGMappings13_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGMappings15_Make(self, file); }

rc_t CC CGMappings20_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGMappings15_Make(self, file); }

rc_t CC CGMappings22_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGMappings_Make(self, file, &CGMappings22_vt); }
