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
#include <insdc/insdc.h>

#include "debug.h"
#include "defs.h"
#include "writer-seq.h"

#include <stdlib.h>
#include <assert.h>

const INSDC_SRA_platform_id DFTL_platform = SRA_PLATFORM_COMPLETE_GENOMICS;
const char DFTL_label[] = "LeftRight";
const INSDC_coord_zero DFTL_label_start[CG_READS_NREADS] = {0, 4};
const INSDC_coord_len DFTL_label_len[CG_READS_NREADS] =    {4, 5};
const INSDC_coord_zero DFTL_read_start[CG_READS_NREADS] =  {0, 35};
const INSDC_coord_len DFTL_read_len[CG_READS_NREADS] =    {35, 35};
const INSDC_SRA_read_filter DFTL_read_filter[CG_READS_NREADS] =  {SRA_READ_FILTER_PASS, SRA_READ_FILTER_PASS};

struct CGWriterSeq {
    const TableWriterSeq* base;
    TReadsData data;
};

rc_t CGWriterSeq_Make(const CGWriterSeq** cself, TReadsData** data, VDatabase* db, const uint32_t options, const char* quality_quantization)
{
    rc_t rc = 0;
    CGWriterSeq* self;

    if( cself == NULL || db == NULL ) {
        return RC(rcExe, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    } else {
        TableWriterData p;
        p.buffer = &DFTL_platform;
        p.elements = 1;
        if( (rc = TableWriterSeq_Make(&self->base, db, options | ewseq_co_AlignData | ewseq_co_SpotGroup, quality_quantization)) != 0 ) {
            LOGERR(klogErr, rc, "sequence table");
        } else if( (rc = TableWriteSeq_WriteDefault(self->base, ewseq_cn_PLATFORM, &p)) == 0 ) {
            /* attach data pointer to data */
            self->data.seq.nreads = CG_READS_NREADS;
            self->data.seq.alignment_count.buffer = self->data.align_count;
            self->data.seq.alignment_count.elements = CG_READS_NREADS;
            self->data.seq.primary_alignment_id.buffer = self->data.prim_algn_id;
            self->data.seq.primary_alignment_id.elements = CG_READS_NREADS;
            self->data.seq.sequence.buffer = self->data.read;
            self->data.seq.quality.buffer = self->data.qual;
            self->data.seq.label.buffer = DFTL_label;
            self->data.seq.label.elements = sizeof(DFTL_label) - 1;
            self->data.seq.label_start.buffer = DFTL_label_start;
            self->data.seq.label_start.elements = CG_READS_NREADS;
            self->data.seq.label_len.buffer = DFTL_label_len;
            self->data.seq.label_len.elements = CG_READS_NREADS;
            self->data.seq.read_type.buffer = self->data.read_type;
            self->data.seq.read_type.elements = CG_READS_NREADS;
            self->data.seq.read_start.buffer = DFTL_read_start;
            self->data.seq.read_start.elements = CG_READS_NREADS;
            self->data.seq.read_len.buffer = DFTL_read_len;
            self->data.seq.read_len.elements = CG_READS_NREADS;
            self->data.seq.read_filter.buffer = DFTL_read_filter;
            self->data.seq.read_filter.elements = CG_READS_NREADS;

            /* set to 1st row for aligment to refer */
            self->data.rowid = 1;
            *data = &self->data;
        }
    }
    if( rc == 0 ) {
        *cself = self;
    } else {
        CGWriterSeq_Whack(self, false, NULL);
    }
    return rc;
}

rc_t CGWriterSeq_Whack(const CGWriterSeq* cself, bool commit, uint64_t* rows)
{
    rc_t rc = 0;
    if( cself != NULL ) {
        CGWriterSeq* self = (CGWriterSeq*)cself;
        rc = TableWriterSeq_Whack(cself->base, commit, rows);
        free(self);
    }
    return rc;
}

rc_t CGWriterSeq_Write(const CGWriterSeq* cself)
{
    uint64_t i;
    int64_t r;
    rc_t rc;
    INSDC_quality_phred* b;
    CGWriterSeq* self = (CGWriterSeq*)cself;

    assert(cself != NULL);

    b = (INSDC_quality_phred*)(self->data.seq.quality.buffer);
    for(i = 0; i < cself->data.seq.quality.elements; i++ ) {
        b[i] -= 33;
    }
    for(i = 0; i < CG_READS_NREADS; i++) {
        self->data.read_type[i] = SRA_READ_TYPE_BIOLOGICAL |
            (cself->data.prim_is_reverse[i] ? SRA_READ_TYPE_REVERSE : SRA_READ_TYPE_FORWARD);
    }
    rc = TableWriterSeq_Write(cself->base, &cself->data.seq, &r);
    return rc ? rc : (self->data.rowid++ == r ? 0 : RC(rcExe, rcTable, rcWriting, rcData, rcInconsistent));
}
