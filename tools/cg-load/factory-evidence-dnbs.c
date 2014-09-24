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

typedef struct CGEvidenceDnbs15 CGEvidenceDnbs15;
#define CGFILETYPE_IMPL CGEvidenceDnbs15
#include "file.h"
#include "factory-cmn.h"
#include "factory-evidence-dnbs.h"
#include "debug.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct CGEvidenceDnbs15 {
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

void CGEvidenceDnbs15_Release(const CGEvidenceDnbs15* cself, uint64_t* records)
{
    if( cself != NULL ) {
        CGEvidenceDnbs15* self = (CGEvidenceDnbs15*)cself;
        if( records != NULL ) {
            *records = cself->records;
        }
        free(self);
    }
}

static
rc_t CC CGEvidenceDnbs15_Header(const CGEvidenceDnbs15* cself, const char* buf, const size_t len)
{
    rc_t rc = 0;
    size_t slen;
    CGEvidenceDnbs15* self = (CGEvidenceDnbs15*)cself;

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
rc_t CGEvidenceDnbs15_GetAssemblyId(const CGEvidenceDnbs15* cself, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id)
{
    if( cself->assembly_id[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *assembly_id = cself->assembly_id;
    return 0;
}

static
rc_t CGEvidenceDnbs15_GetSample(const CGEvidenceDnbs15* cself, const CGFIELD_SAMPLE_TYPE** sample)
{
    if( cself->sample[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *sample = cself->sample;
    return 0;
}

static
rc_t CGEvidenceDnbs15_GetChromosome(const CGEvidenceDnbs15* cself, const CGFIELD_CHROMOSOME_TYPE** chromosome)
{
    if( cself->chromosome[0] == '\0' ) {
        return RC(rcRuntime, rcFile, rcReading, rcFormat, rcInvalid);
    }
    *chromosome = cself->chromosome;
    return 0;
}

static
rc_t CC CGEvidenceDnbs_Read(const CGEvidenceDnbs15* cself, const char* interval_id, TEvidenceDnbsData* data, int score_allele_num)
{
    rc_t rc = 0;
    TEvidenceDnbsData_dnb* m = NULL;
    static TEvidenceDnbsData_dnb next_rec;
    static char next_interval_id[32] = "";

    /* local copy of unused TEvidenceDnbsData_dnb struct elements */
    char reference_alignment[CG_EVDNC_ALLELE_CIGAR_LEN];
    INSDC_coord_zero mate_offset_in_reference;
    char mate_reference_alignment[CG_EVDNC_ALLELE_CIGAR_LEN];
    uint16_t score_allele[4] = {0, 0, 0, 0}; /* v1.5 has ScoreAllele[012]; v2.0 - [0123] */
    char qual[CG_EVDNC_SPOT_LEN];

    strcpy(data->interval_id, interval_id);
    data->qty = 0;
    /* already read one rec for this interval_id */
    if( next_interval_id[0] != '\0' ) {
        if( strcmp(next_interval_id, interval_id) != 0 ) {
            /* nothing todo since next interval id is different */
            return rc;
        }
        m = &data->dnbs[data->qty++];
        memcpy(m, &next_rec, sizeof(next_rec));
        DEBUG_MSG(10, ("%3u evidenceDnbs: '%s'\t'%s'\t'%s'\t'%s'\t%u\t%lu\t%hu\t%c\t%c\t%i\t'%.*s'"
                        "\t%i\tnot_used\t0\tnot_used\t%c\t0\t0\t0\t'%.*s'\t'--'\n",
            data->qty, next_interval_id, m->chr, m->slide, m->lane, m->file_num_in_lane,
            m->dnb_offset_in_lane_file, m->allele_index, m->side, m->strand, m->offset_in_allele,
            m->allele_alignment_length, m->allele_alignment, m->offset_in_reference,
            m->mapping_quality, m->read_len, m->read));
    }
    do {
        int i = 0;
        char tmp[2];
        CG_LINE_START(cself->file, b, len, p);
        if( b == NULL || len == 0 ) {
            next_interval_id[0] = '\0';
            break; /* EOF */
        }
        if( data->qty >= data->max_qty ) {
            TEvidenceDnbsData_dnb* x;
            data->max_qty += 100;
            x = realloc(data->dnbs, sizeof(*(data->dnbs)) * data->max_qty);
            if( x == NULL ) {
                rc = RC(rcRuntime, rcFile, rcReading, rcMemory, rcExhausted);
                break;
            }
            data->dnbs = x;
        }
        m = &data->dnbs[data->qty++];

        /*DEBUG_MSG(10, ("%2hu evidenceDnbs: '%.*s'\n", data->qty, len, b));*/
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, next_interval_id, sizeof(next_interval_id));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, m->chr, sizeof(m->chr));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, m->slide, sizeof(m->slide));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, m->lane, sizeof(m->lane));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2u32(b, p - b, &m->file_num_in_lane);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2u64(b, p - b, &m->dnb_offset_in_lane_file);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2u16(b, p - b, &m->allele_index);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, tmp, sizeof(tmp));
        if( tmp[0] != 'L' && tmp[0] != 'R' ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
        }
        m->side = tmp[0];
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, tmp, sizeof(tmp));
        if( tmp[0] != '+' && tmp[0] != '-' ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
        }
        m->strand = tmp[0];
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i32(b, p - b, &m->offset_in_allele);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, m->allele_alignment, sizeof(m->allele_alignment));
        m->allele_alignment_length = p - b;
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i32(b, p - b, &m->offset_in_reference);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, reference_alignment, sizeof(reference_alignment));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2i32(b, p - b, &mate_offset_in_reference);
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, mate_reference_alignment, sizeof(mate_reference_alignment));
        CG_LINE_NEXT_FIELD(b, len, p);
        rc = str2buf(b, p - b, tmp, sizeof(tmp));
        if( tmp[0] < 33 || tmp[0] > 126 ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcOutofrange);
        }
        m->mapping_quality = tmp[0];
        for (i = 0; i < score_allele_num; ++i) {
            CG_LINE_NEXT_FIELD(b, len, p);
            rc = str2u16(b, p - b, &score_allele[i]);
	    if(rc){
		score_allele[i] =0;
		rc =0;
	    }
        }
        CG_LINE_NEXT_FIELD(b, len, p);
        m->read_len = p - b;
        rc = str2buf(b, m->read_len, m->read, sizeof(m->read));
        CG_LINE_LAST_FIELD(b, len, p);
        if( m->read_len != p - b ) {
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcInconsistent);
        } else {
            rc = str2buf(b, p - b, qual, sizeof(qual));
        }
        ((CGEvidenceDnbs15*)cself)->records++;
        if( strcmp(next_interval_id, data->interval_id) != 0 ) {
            if (score_allele_num == 3) {
              DEBUG_MSG(10, ("%3u evidenceDnbs: '%s'\t'%s'\t'%s'\t'%s'\t%u\t%lu\t%hu\t%c\t%c\t%i\t'%.*s'"
                            "\t%i\t'%s'\t%i\t'%s'\t%c\t%hu\t%hu\t%hu\t'%.*s'\t'%s'\n",
                data->qty, next_interval_id, m->chr, m->slide, m->lane, m->file_num_in_lane,
                m->dnb_offset_in_lane_file, m->allele_index, m->side, m->strand, m->offset_in_allele,
                m->allele_alignment_length, m->allele_alignment, m->offset_in_reference,
                reference_alignment, mate_offset_in_reference, mate_reference_alignment,
                m->mapping_quality, score_allele[0], score_allele[1], score_allele[2], m->read_len, m->read, qual));
            }
            else if (score_allele_num == 4) {
              DEBUG_MSG(10, ("%3u evidenceDnbs: '%s'\t'%s'\t'%s'\t'%s'\t%u\t%lu\t%hu\t%c\t%c\t%i\t'%.*s'"
                            "\t%i\t'%s'\t%i\t'%s'\t%c\t%hu\t%hu\t%hu\t%hu\t'%.*s'\t'%s'\n",
                data->qty, next_interval_id, m->chr, m->slide, m->lane, m->file_num_in_lane,
                m->dnb_offset_in_lane_file, m->allele_index, m->side, m->strand, m->offset_in_allele,
                m->allele_alignment_length, m->allele_alignment, m->offset_in_reference,
                reference_alignment, mate_offset_in_reference, mate_reference_alignment,
                m->mapping_quality, score_allele[0], score_allele[1], score_allele[2], score_allele[3], m->read_len, m->read, qual));
            }
            else { assert(0); }
        }
        CG_LINE_END();
        if( next_interval_id[0] == '\0' ) {
            break;
        }
        if( strcmp(next_interval_id, data->interval_id) != 0 ) {
            /* next record is from next interval, remeber it and stop */
            memcpy(&next_rec, m, sizeof(next_rec));
            data->qty--;
            break;
        }
    } while( rc == 0 );
    return rc;
}

static
rc_t CC CGEvidenceDnbs15_Read(const CGEvidenceDnbs15* self, const char* interval_id, TEvidenceDnbsData* data)
{   return CGEvidenceDnbs_Read(self, interval_id, data, 3); }

static
rc_t CC CGEvidenceDnbs20_Read(const CGEvidenceDnbs15* self, const char* interval_id, TEvidenceDnbsData* data)
{   return CGEvidenceDnbs_Read(self, interval_id, data, 4); }

static const CGFileType_vt CGEvidenceDnbs15_vt =
{
    CGEvidenceDnbs15_Header,
    NULL,
    NULL,
    NULL,
    NULL,
    CGEvidenceDnbs15_Read,
    NULL, /* tag_lfr */
    CGEvidenceDnbs15_GetAssemblyId,
    NULL,
    NULL,
    NULL,
    CGEvidenceDnbs15_GetSample,
    CGEvidenceDnbs15_GetChromosome,
    CGEvidenceDnbs15_Release
};

static const CGFileType_vt CGEvidenceDnbs20_vt =
{
    CGEvidenceDnbs15_Header,
    NULL,
    NULL,
    NULL,
    NULL,
    CGEvidenceDnbs20_Read,
    NULL, /* tag_lfr */
    CGEvidenceDnbs15_GetAssemblyId,
    NULL,
    NULL,
    NULL,
    CGEvidenceDnbs15_GetSample,
    CGEvidenceDnbs15_GetChromosome,
    CGEvidenceDnbs15_Release
};

static
rc_t CC CGEvidenceDnbs_Make(const CGFileType** cself, const CGLoaderFile* file,
                              const CGFileType_vt* vt)
{
    rc_t rc = 0;
    CGEvidenceDnbs15* obj = NULL;

    assert(vt);

    if( cself == NULL || file == NULL ) {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcParam, rcNull);
    } else {
        *cself = NULL;
        if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
            rc = RC(rcRuntime, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else {
            obj->file = file;
            obj->dad.type = cg_eFileType_EVIDENCE_DNBS;
            obj->dad.vt = vt;
        }
    }
    if( rc == 0 ) {
        *cself = &obj->dad;
    } else {
        CGEvidenceDnbs15_Release(obj, NULL);
    }
    return rc;
}

rc_t CC CGEvidenceDnbs15_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGEvidenceDnbs_Make(self, file, &CGEvidenceDnbs15_vt); }

rc_t CC CGEvidenceDnbs13_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGEvidenceDnbs15_Make(self, file); }

rc_t CC CGEvidenceDnbs20_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGEvidenceDnbs_Make(self, file, &CGEvidenceDnbs20_vt); }

rc_t CC CGEvidenceDnbs22_Make(const CGFileType** self, const CGLoaderFile* file)
{   return CGEvidenceDnbs_Make(self, file, &CGEvidenceDnbs20_vt); }
