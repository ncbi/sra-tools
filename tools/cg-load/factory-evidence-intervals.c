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

typedef struct CGEvidenceIntervals15 CGEvidenceIntervals15;
#define CGFILETYPE_IMPL CGEvidenceIntervals15
#include "file.h"
#include "factory-cmn.h"
#include "factory-evidence-intervals.h"
#include "debug.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct CGEvidenceIntervals15 {
    CGFileType dad;
    const CGLoaderFile* file;
    uint64_t records;
    /* headers */
    CGFIELD15_ASSEMBLY_ID assembly_id;
    CGFIELD15_CHROMOSOME chromosome;
    CGFIELD15_GENERATED_AT generated_at;
    CGFIELD15_GENERATED_BY generated_by;
    CGFIELD15_SAMPLE sample;
    CGFIELD15_SOFTWARE_VERSION software_version;
};

static
rc_t CC CGEvidenceIntervals15_Header(const CGEvidenceIntervals15* cself, const char* buf, const size_t len)
{
    rc_t rc = 0;
    size_t slen;
    CGEvidenceIntervals15* self = (CGEvidenceIntervals15*)cself;

    if( strncmp("ASSEMBLY_ID\t", buf, slen = 12) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->assembly_id, sizeof(self->assembly_id));
    } else if( strncmp("CHROMOSOME\t", buf, slen = 11) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->chromosome, sizeof(self->chromosome));
    } else if( strncmp("GENERATED_AT\t", buf, slen = 13) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->generated_at, sizeof(self->generated_at));
    } else if( strncmp("GENERATED_BY\t", buf, slen = 13) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->generated_by, sizeof(self->generated_by));
    } else if( strncmp("SAMPLE\t", buf, slen = 7) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->sample, sizeof(self->sample));
    } else if( strncmp("SOFTWARE_VERSION\t", buf, slen = 17) == 0 ) {
        rc = str2buf(&buf[slen], len - slen, self->software_version, sizeof(self->software_version));
    } else {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcName, rcUnrecognized);
    }
    return rc;
}

static
rc_t CGEvidenceIntervals15_GetAssemblyId(const CGEvidenceIntervals15* cself, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id)
{
    if( cself->assembly_id[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *assembly_id = cself->assembly_id;
    return 0;
}

static
rc_t CGEvidenceIntervals15_GetSample(const CGEvidenceIntervals15* cself, const CGFIELD_SAMPLE_TYPE** sample)
{
    if( cself->sample[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *sample = cself->sample;
    return 0;
}

static
rc_t CGEvidenceIntervals15_GetChromosome(const CGEvidenceIntervals15* cself, const CGFIELD_CHROMOSOME_TYPE** chromosome)
{
    if( cself->chromosome[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *chromosome = cself->chromosome;
    return 0;
}

static
rc_t CC CGEvidenceIntervals15_Read(const CGEvidenceIntervals15* cself, TEvidenceIntervalsData* data)
{
    rc_t rc = 0;

    CG_LINE_START(cself->file, b, len, p);
    if( b == NULL || len == 0) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcDone);
        break;
    }
    /*DEBUG_MSG(10, ("evidenceIntervals: '%.*s'\n", len, b));*/
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->interval_id, sizeof(data->interval_id));
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->chr, sizeof(data->chr));
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2i32(b, p - b, &data->offset);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2u32(b, p - b, &data->length);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2u16(b, p - b, &data->ploidy);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele_indexes, sizeof(data->allele_indexes));
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2i32(b, p - b, &data->score);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele[0], sizeof(data->allele[0]));
    data->allele_length[0] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele[1], sizeof(data->allele[1]));
    data->allele_length[1] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele[2], sizeof(data->allele[2]));
    data->allele_length[2] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele_alignment[1], sizeof(data->allele_alignment[1]));
    data->allele_alignment_length[1] = p - b;
    CG_LINE_LAST_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele_alignment[2], sizeof(data->allele_alignment[2]));
    data->allele_alignment_length[2] = p - b;
    ((CGEvidenceIntervals15*)cself)->records++;
    DEBUG_MSG(10, ("evidenceIntervals: '%s'\t'%s'\t%i\t%u\t%u\t%s\t%u\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'\n",
        data->interval_id, data->chr, data->offset, data->length, data->ploidy,
        data->allele_indexes, data->score, data->allele[0], data->allele[1], data->allele[2],
        data->allele_alignment[1], data->allele_alignment[2]));
    CG_LINE_END();
    return rc;
}

static
rc_t CC CGEvidenceIntervals20_Read(const CGEvidenceIntervals15* cself, TEvidenceIntervalsData* data)
{
    rc_t rc = 0;

    CG_LINE_START(cself->file, b, len, p);
    if( b == NULL || len == 0) {
        rc = RC(rcRuntime, rcFile, rcReading, rcData, rcDone);
        break;
    }
    /*DEBUG_MSG(10, ("evidenceIntervals: '%.*s'\n", len, b));*/
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->interval_id, sizeof(data->interval_id));
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->chr, sizeof(data->chr));
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2i32(b, p - b, &data->offset);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2u32(b, p - b, &data->length);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2u16(b, p - b, &data->ploidy);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele_indexes, sizeof(data->allele_indexes));
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2i32(b, p - b, &data->scoreVAF);
    data->score = data->scoreVAF; /***TODO: do we need re-calculation? ***/
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2i32(b, p - b, &data->scoreEAF);
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele[0], sizeof(data->allele[0]));
    data->allele_length[0] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele[1], sizeof(data->allele[1]));
    data->allele_length[1] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele[2], sizeof(data->allele[2]));
    data->allele_length[2] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele[3], sizeof(data->allele[3]));
    data->allele_length[3] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele_alignment[1], sizeof(data->allele_alignment[1]));
    data->allele_alignment_length[1] = p - b;
    CG_LINE_NEXT_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele_alignment[2], sizeof(data->allele_alignment[2]));
    data->allele_alignment_length[2] = p - b;
    CG_LINE_LAST_FIELD(b, len, p);
    rc = str2buf(b, p - b, data->allele_alignment[3], sizeof(data->allele_alignment[3]));
    data->allele_alignment_length[3] = p - b;
    ((CGEvidenceIntervals15*)cself)->records++;
    DEBUG_MSG(10, (
        "evidenceIntervals: '%s'\t'%s'\t%i\t%u\t%u\t%s\t%u\t%u\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'\t'%s'\n",
        data->interval_id, data->chr, data->offset, data->length, data->ploidy,
        data->allele_indexes, data->scoreVAF, data->scoreEAF,
        data->allele[0], data->allele[1], data->allele[2], data->allele[3],
        data->allele_alignment[1], data->allele_alignment[2], data->allele_alignment[3]));
    CG_LINE_END();
    return rc;
}

static
void CC CGEvidenceIntervals15_Release(const CGEvidenceIntervals15* cself, uint64_t* records)
{
    if( cself != NULL ) {
        CGEvidenceIntervals15* self = (CGEvidenceIntervals15*)cself;
        if( records != NULL ) {
            *records = cself->records;
        }
        free(self);
    }
}

static const CGFileType_vt CGEvidenceIntervals15_vt =
{
    CGEvidenceIntervals15_Header,
    NULL,
    NULL,
    NULL,
    CGEvidenceIntervals15_Read,
    NULL,
    NULL, /* tag_lfr */
    CGEvidenceIntervals15_GetAssemblyId,
    NULL,
    NULL,
    NULL,
    CGEvidenceIntervals15_GetSample,
    CGEvidenceIntervals15_GetChromosome,
    CGEvidenceIntervals15_Release
};

static const CGFileType_vt CGEvidenceIntervals20_vt =
{
    CGEvidenceIntervals15_Header,
    NULL,
    NULL,
    NULL,
    CGEvidenceIntervals20_Read,
    NULL,
    NULL, /* tag_lfr */
    CGEvidenceIntervals15_GetAssemblyId,
    NULL,
    NULL,
    NULL,
    CGEvidenceIntervals15_GetSample,
    CGEvidenceIntervals15_GetChromosome,
    CGEvidenceIntervals15_Release
};

static
rc_t CGEvidenceIntervals_Make(const CGFileType** cself, const CGLoaderFile* file, const CGFileType_vt* vt)
{
    rc_t rc = 0;
    CGEvidenceIntervals15* obj = NULL;

    assert(vt);

    if( cself == NULL || file == NULL ) {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcParam, rcNull);
    }
    if( rc == 0 ) {
        *cself = NULL;
        if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
            rc = RC(rcRuntime, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else {
            obj->file = file;
            obj->dad.type = cg_eFileType_EVIDENCE_INTERVALS;
            obj->dad.vt = vt;
        }
    }
    if( rc == 0 ) {
        *cself = &obj->dad;
    } else {
        CGEvidenceIntervals15_Release(obj, NULL);
    }
    return rc;
}

rc_t CGEvidenceIntervals15_Make(const CGFileType** self,
    const CGLoaderFile* file)
{   return CGEvidenceIntervals_Make(self, file, &CGEvidenceIntervals15_vt); }

rc_t CGEvidenceIntervals13_Make(const CGFileType** self,
    const CGLoaderFile* file)
{   return CGEvidenceIntervals15_Make(self, file); }

rc_t CGEvidenceIntervals20_Make(const CGFileType** self,
    const CGLoaderFile* file)
{   return CGEvidenceIntervals_Make(self, file, &CGEvidenceIntervals20_vt); }

rc_t CGEvidenceIntervals22_Make(const CGFileType** self,
    const CGLoaderFile* file)
{   return CGEvidenceIntervals20_Make(self, file); }
