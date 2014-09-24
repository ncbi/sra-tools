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
#include <klib/container.h>
#include <kfs/file.h>
#include <insdc/insdc.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <align/writer-alignment.h>
#include <align/dna-reverse-cmpl.h>
#include <align/align.h>

#include "debug.h"
#include "defs.h"
#include "writer-evidence-intervals.h"
#include "writer-evidence-dnbs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

typedef struct CGWriterEvdDnb_match_struct {

    int64_t seq_spot_id;
    INSDC_coord_one seq_read_id;

    /* filled out by ReferenceMgr_Compress */
    INSDC_coord_zero read_start;
    INSDC_coord_len read_len;
    bool has_ref_offset[CG_EVDNC_ALLELE_LEN];
    int32_t ref_offset[CG_EVDNC_ALLELE_LEN];
    uint8_t ref_offset_type[CG_EVDNC_ALLELE_LEN];
    int64_t ref_id;
    INSDC_coord_zero ref_start;
    bool has_mismatch[CG_EVDNC_ALLELE_LEN];
    char mismatch[CG_EVDNC_ALLELE_LEN];

    bool ref_orientation;
    uint32_t ref_ploidy;
    uint32_t mapq;

} CGWriterEvdDnb_match;

struct CGWriterEvdDnbs {
    const ReferenceMgr* rmgr;
    const TableWriterAlgn* writer;

    TableWriterAlgnData algn;
    CGWriterEvdDnb_match match;
    
    TEvidenceDnbsData data;

    uint64_t bad_allele_index;
};

rc_t CGWriterEvdDnbs_Make(const CGWriterEvdDnbs** cself, TEvidenceDnbsData** data,
                          VDatabase* db, const ReferenceMgr* rmgr, const uint32_t options)
{
    rc_t rc = 0;
    CGWriterEvdDnbs* self;

    if( cself == NULL || db == NULL ) {
        return RC(rcExe, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    } else {
        if( (rc = TableWriterAlgn_Make(&self->writer, db, ewalgn_tabletype_EvidenceAlignment, ewalgn_co_SEQ_SPOT_ID)) == 0 ) {
            self->algn.seq_spot_id.buffer = &self->match.seq_spot_id;
            self->algn.seq_spot_id.elements = 1;
            self->algn.seq_read_id.buffer = &self->match.seq_read_id;
            self->algn.seq_read_id.elements = 1;

            self->algn.read_start.buffer = &self->match.read_start;
            self->algn.read_len.buffer = &self->match.read_len;
            self->algn.has_ref_offset.buffer = self->match.has_ref_offset;
            self->algn.ref_offset.buffer = self->match.ref_offset;
            self->algn.ref_offset_type.buffer = self->match.ref_offset_type;
            self->algn.ref_id.buffer = &self->match.ref_id;
            self->algn.ref_id.elements = 1;
            self->algn.ref_start.buffer = &self->match.ref_start;
            self->algn.ref_start.elements = 1;
            self->algn.has_mismatch.buffer = self->match.has_mismatch;
            self->algn.mismatch.buffer = self->match.mismatch;
            self->algn.ref_orientation.buffer = &self->match.ref_orientation;
            self->algn.ref_orientation.elements = 1;
            self->algn.ref_ploidy.buffer = &self->match.ref_ploidy;
            self->algn.ref_ploidy.elements = 1;
            self->algn.mapq.buffer = &self->match.mapq;
            self->algn.mapq.elements = 1;
            self->rmgr = rmgr;

            /* set to 1st row for evidence_interval to collect ids */
            self->data.last_rowid = 1;
            *data = &self->data;
        }
    }
    if( rc == 0 ) {
        *cself = self;
    } else {
        CGWriterEvdDnbs_Whack(self, false, NULL);
    }
    return rc;
}

rc_t CGWriterEvdDnbs_Whack(const CGWriterEvdDnbs* cself, bool commit, uint64_t* rows)
{
    rc_t rc = 0;
    if( cself != NULL ) {
        CGWriterEvdDnbs* self = (CGWriterEvdDnbs*)cself;
        if( self->bad_allele_index > 0 ) {
            PLOGMSG(klogInfo, (klogInfo, "$(bad_allele_index) bad allele_indexes in evidence dnbs",
                    "bad_allele_index=%lu", self->bad_allele_index));
        }
        rc = TableWriterAlgn_Whack(cself->writer, commit, rows);
        free(self->data.dnbs);
        free(self);
    }
    return rc;
}

rc_t CGWriterEvdDnbs_SetSEQ(const CGWriterEvdDnbs* cself, uint16_t dnb, const int64_t seq_spot_id_1st)
{
    rc_t rc = 0;
    CGWriterEvdDnbs* self =(CGWriterEvdDnbs*)cself;

    assert(cself != NULL);

    self->data.dnbs[dnb].seq_spot_id = seq_spot_id_1st + self->data.dnbs[dnb].dnb_offset_in_lane_file;

    return rc;
}

rc_t CGWriterEvdDnbs_Write(const CGWriterEvdDnbs* cself, const TEvidenceIntervalsData* ref, int64_t ref_rowid)
{
    rc_t rc = 0;
    uint16_t i;
    int64_t last_rowid;
    CGWriterEvdDnbs* self =(CGWriterEvdDnbs*)cself;

    assert(cself != NULL);
    assert(ref != NULL);

    for(i = 0; rc == 0 && i < cself->data.qty; i++) {

        /* align against allele */
        uint16_t ai = cself->data.dnbs[i].allele_index;
        self->match.ref_ploidy = ref->allele_indexes_to_read_number[ai];
        if( self->match.ref_ploidy == 0 ) {
            DEBUG_MSG(3, ("bad allele_index for interval %s %s[%hu]\n", cself->data.dnbs[i].chr, cself->data.interval_id, i + 1));
            self->bad_allele_index++;
            continue;
        } else {
            const char* read;
            char reversed[CG_READS_SPOT_LEN];
            uint32_t read_len = CG_READS_SPOT_LEN / 2;

            if( cself->data.dnbs[i].side == 'L' ) {
                self->match.seq_read_id = 1;
                read = cself->data.dnbs[i].read;
            } else {
                self->match.seq_read_id = 2;
                read = &cself->data.dnbs[i].read[read_len];
            }
            if( cself->data.dnbs[i].strand == '-' ) {
                if( (rc = DNAReverseCompliment(read, reversed, read_len)) != 0 ) {
                    break;
                }
                read = reversed;
                self->match.ref_orientation = true;
            } else {
                self->match.ref_orientation = false;
            }
            self->algn.ploidy = 0;
            if( (rc = ReferenceMgr_Compress(cself->rmgr, 0, cself->data.dnbs[i].chr,
                                            cself->data.dnbs[i].offset_in_reference, read, read_len,
                                            cself->data.dnbs[i].allele_alignment, cself->data.dnbs[i].allele_alignment_length,
                                            ref->offset,
                                            ref->allele[ai], ref->allele_length[ai],
					    cself->data.dnbs[i].offset_in_allele,
                                            ref->allele_alignment[ai], ref->allele_alignment_length[ai],
                                            NCBI_align_ro_complete_genomics, &self->algn)) == 0 ) {
                self->match.mapq = cself->data.dnbs[i].mapping_quality - 33;
                /* pointer to SEQUENCE table spot and read */
                self->match.seq_spot_id = cself->data.dnbs[i].seq_spot_id;
                self->match.ref_id = ref_rowid;
                self->match.ref_start = cself->data.dnbs[i].offset_in_allele;
                DEBUG_MSG(3, ("REF_ID: %li, REF_START: %i, REF_PLOIDY: %hu\n",
                    self->match.ref_id, self->match.ref_start, self->match.ref_ploidy));
                rc = TableWriterAlgn_Write(cself->writer, &self->algn, &last_rowid);
		if( rc == 0 && self->data.last_rowid++ != last_rowid ) {
		    rc = RC(rcExe, rcTable, rcWriting, rcData, rcInconsistent);
		}
            } else if( GetRCObject(rc) == rcOffset && GetRCState(rc) == rcOutofrange ) {
                PLOGERR(klogErr, (klogErr, rc, "evidence dnb for $(chr) interval $(interval) OffsetInReference $(offset) skipped",
                    "chr=%s,interval=%s,offset=%i", ref->chr, ref->interval_id, cself->data.dnbs[i].offset_in_reference));
                rc = 0;
            }
        }
    }
    return rc;
}
