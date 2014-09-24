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
#include <klib/printf.h>
#include <kfs/file.h>
#include <vdb/table.h>
#include <align/align.h>

#include "defs.h"
#include "writer-evidence-intervals.h"
#include "writer-evidence-dnbs.h"
#include "debug.h"
#include "file.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

typedef struct CGWriterEvdInt_match_struct {

    /* filled out by ReferenceMgr_Compress */
    INSDC_coord_zero read_start[CG_EVDNC_PLOIDY];
    INSDC_coord_len read_len[CG_EVDNC_PLOIDY];
    bool has_ref_offset[CG_EVDNC_ALLELE_LEN];
    int32_t ref_offset[CG_EVDNC_ALLELE_LEN];
    uint8_t ref_offset_type[CG_EVDNC_ALLELE_LEN];
    bool has_mismatch[CG_EVDNC_ALLELE_LEN];
    char mismatch[CG_EVDNC_ALLELE_LEN];
    int64_t ref_id;
    INSDC_coord_zero ref_start;
    bool ref_orientation;
    uint32_t mapq;

} CGWriterEvdInt_match;

struct CGWriterEvdInt {
    const ReferenceMgr* rmgr;
    const TableWriterAlgn* writer;
    TableWriterAlgnData algn;
    TEvidenceIntervalsData data;
    CGWriterEvdInt_match match;
    int64_t* dnbs_ids;
    uint64_t dnbs_ids_max;
};

rc_t CGWriterEvdInt_Make(const CGWriterEvdInt** cself, TEvidenceIntervalsData** data,
                         VDatabase* db, const ReferenceMgr* rmgr, const uint32_t options)
{
    rc_t rc = 0;
    CGWriterEvdInt* self;

    if( cself == NULL || db == NULL ) {
        return RC(rcExe, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    } else {
        if( (rc = TableWriterAlgn_Make(&self->writer, db, ewalgn_tabletype_EvidenceInterval, 0)) == 0 ) {
            self->algn.read_start.buffer = &self->match.read_start;
            self->algn.read_len.buffer = &self->match.read_len;
            self->algn.has_ref_offset.buffer = self->match.has_ref_offset;
            self->algn.ref_offset.buffer = self->match.ref_offset;
            self->algn.ref_offset_type.buffer = self->match.ref_offset_type;
            self->algn.has_mismatch.buffer = self->match.has_mismatch;
            self->algn.mismatch.buffer = self->match.mismatch;
            self->algn.ref_id.buffer = &self->match.ref_id;
            self->algn.ref_id.elements = 1;
            self->algn.ref_start.buffer = &self->match.ref_start;
            self->algn.ref_start.elements = 1;
            self->match.ref_orientation = false;
            self->algn.ref_orientation.buffer = &self->match.ref_orientation;
            self->algn.ref_orientation.elements = 1;
            self->algn.mapq.buffer = &self->match.mapq;
            self->algn.mapq.elements = 1;
            self->algn.alingment_ids.buffer = self->dnbs_ids;
            self->rmgr = rmgr;
            *data = &self->data;
        }
    }
    if( rc == 0 ) {
        *cself = self;
    } else {
        CGWriterEvdInt_Whack(self, false, NULL);
    }
    return rc;
}

rc_t CGWriterEvdInt_Whack(const CGWriterEvdInt* cself, bool commit, uint64_t* rows)
{
    rc_t rc = 0;
    if( cself != NULL ) {
        CGWriterEvdInt* self = (CGWriterEvdInt*)cself;
        rc = TableWriterAlgn_Whack(cself->writer, commit, rows);
        free(self->dnbs_ids);
        free(self);
    }
    return rc;
}

rc_t CGWriterEvdInt_Write(const CGWriterEvdInt* cself, const TEvidenceDnbsData* dnbs, int64_t* rowid)
{
    rc_t rc = 0;
    CGWriterEvdInt* self = (CGWriterEvdInt*)cself;
    
    assert(cself != NULL);

    memset(self->data.allele_indexes_to_read_number, 0, sizeof(self->data.allele_indexes_to_read_number));
    if( self->data.ploidy == 1 ) {

        uint32_t i = self->data.allele_indexes[0] - '0';

        if( self->data.allele_indexes[1] != '\0' || i > 2 ) {
            rc = RC(rcExe, rcFormatter, rcWriting, rcData, rcOutofrange);
        } else if( self->data.allele_alignment_length[i] == 0 ) {
            rc = RC(rcExe, rcFormatter, rcWriting, rcData, rcEmpty);
        } else {
            if( i == 0 ) {
                rc = string_printf(self->data.allele_alignment[0], sizeof(self->data.allele_alignment[0]),
                                   &self->data.allele_alignment_length[0], "%uM", self->data.length);
            }
            if( rc == 0 ) {
                self->algn.ploidy = 0;
                rc = ReferenceMgr_Compress(cself->rmgr, ewrefmgr_cmp_Exact, self->data.chr, self->data.offset, self->data.allele[i], self->data.allele_length[i],
                                           self->data.allele_alignment[i], self->data.allele_alignment_length[i],
                                           0, NULL, 0, 0, NULL, 0, NCBI_align_ro_intron_unknown, &self->algn);
                self->data.allele_indexes_to_read_number[i] = 1; /* 1st read */
            }
        }

    } else if( self->data.ploidy == 2 ) {  /** possibilities: 0;1 1;2 and 1;1 - single ploidy but recorded as dual **/
        uint32_t i1 = self->data.allele_indexes[0] - '0';
        uint32_t i2 = self->data.allele_indexes[2] - '0';

        if( self->data.allele_indexes[1] != ';' || self->data.allele_indexes[3] != '\0' || i1 > 2 || i2 > 2) {
            rc = RC(rcExe, rcFormatter, rcWriting, rcData, rcOutofrange);
        } else {
            if( i1 == 0 || i2 == 0 ) {
                rc = string_printf(self->data.allele_alignment[0], sizeof(self->data.allele_alignment[0]),
                                   &self->data.allele_alignment_length[0], "%uM", self->data.length);
            }
            if( self->data.allele_alignment_length[i1] == 0 || self->data.allele_alignment_length[i2] == 0 ) {
                rc = RC(rcExe, rcFormatter, rcWriting, rcData, rcEmpty);
            }
            if( rc == 0 ) {
                self->algn.ploidy = 0;
                rc = ReferenceMgr_Compress(cself->rmgr, ewrefmgr_cmp_Exact, self->data.chr, self->data.offset, self->data.allele[i1], self->data.allele_length[i1],
                                           self->data.allele_alignment[i1], self->data.allele_alignment_length[i1],
                                           0, NULL, 0, 0, NULL, 0, NCBI_align_ro_intron_unknown, &self->algn);
                self->data.allele_indexes_to_read_number[i1] = 1; /* 1st read */
            }
            if( rc == 0 ) {
		if ( i2 != i1 ) {
			rc = ReferenceMgr_Compress(cself->rmgr, ewrefmgr_cmp_Exact, self->data.chr, self->data.offset, self->data.allele[i2], self->data.allele_length[i2],
                                           self->data.allele_alignment[i2], self->data.allele_alignment_length[i2],
                                           0, NULL, 0, 0, NULL, 0, NCBI_align_ro_intron_unknown, &self->algn);
			self->data.allele_indexes_to_read_number[i2] = 2; /* 2nd read */
		} else {
			self->data.ploidy = 1;
		}
            }
        }

    }  else if( self->data.ploidy == 3 ) { /** possibilities: 0;1;2 1;2;3 **/
        uint32_t i1 = self->data.allele_indexes[0] - '0';
        uint32_t i2 = self->data.allele_indexes[2] - '0';
        uint32_t i3 = self->data.allele_indexes[4] - '0';


        if( self->data.allele_indexes[1] != ';'  || self->data.allele_indexes[3] != ';' || self->data.allele_indexes[5] != '\0' || i1 > 3 || i2 > 3 || i3 > 3) {
            rc = RC(rcExe, rcFormatter, rcWriting, rcData, rcOutofrange);
        } else {
            if( i1 == 0 || i2 == 0 || i3 == 0) {
                rc = string_printf(self->data.allele_alignment[0], sizeof(self->data.allele_alignment[0]),
                                   &self->data.allele_alignment_length[0], "%uM", self->data.length);
            }
            if( self->data.allele_alignment_length[i1] == 0 || self->data.allele_alignment_length[i2] == 0 || self->data.allele_alignment_length[i3]==0) {
                rc = RC(rcExe, rcFormatter, rcWriting, rcData, rcEmpty);
            }
            if( rc == 0 ) {
                self->algn.ploidy = 0;
                rc = ReferenceMgr_Compress(cself->rmgr, ewrefmgr_cmp_Exact, self->data.chr, self->data.offset, self->data.allele[i1], self->data.allele_length[i1],
                                           self->data.allele_alignment[i1], self->data.allele_alignment_length[i1],
                                           0, NULL, 0, 0, NULL, 0, NCBI_align_ro_intron_unknown, &self->algn);
                self->data.allele_indexes_to_read_number[i1] = 1; /* 1st read */
            }
            if( rc == 0 ) {
                rc = ReferenceMgr_Compress(cself->rmgr, ewrefmgr_cmp_Exact, self->data.chr, self->data.offset, self->data.allele[i2], self->data.allele_length[i2],
                                           self->data.allele_alignment[i2], self->data.allele_alignment_length[i2],
                                           0, NULL, 0, 0, NULL, 0, NCBI_align_ro_intron_unknown, &self->algn);
                self->data.allele_indexes_to_read_number[i2] = 2; /* 2nd read */
            }
            if( rc == 0 ) {
                rc = ReferenceMgr_Compress(cself->rmgr, ewrefmgr_cmp_Exact, self->data.chr, self->data.offset, self->data.allele[i3], self->data.allele_length[i3],
                                           self->data.allele_alignment[i3], self->data.allele_alignment_length[i3],
                                           0, NULL, 0, 0, NULL, 0, NCBI_align_ro_intron_unknown, &self->algn);
                self->data.allele_indexes_to_read_number[i3] = 3; /* 3rd read */
            }
        }

    } else {
        rc = RC(rcExe, rcFormatter, rcWriting, rcData, rcUnrecognized);
    }
    if( rc == 0 ) { /*** a bit careful here - were are predicting what EvidenceDnb writer will do ***/
	uint32_t i,valid_qty;
	for(valid_qty = i = 0;i< dnbs->qty;i++){
		if(self->data.allele_indexes_to_read_number[dnbs->dnbs[i].allele_index] > 0){
			valid_qty++;
		}
	}
        if( self->dnbs_ids_max < valid_qty ) {
            void* p = realloc(self->dnbs_ids, sizeof(*(self->dnbs_ids)) * valid_qty);
            if( p == NULL ) {
                rc = RC(rcExe, rcFormatter, rcWriting, rcMemory, rcExhausted);
            } else {
                self->dnbs_ids = p;
                self->dnbs_ids_max = valid_qty;
                self->algn.alingment_ids.buffer = self->dnbs_ids;
            }
        }
        if( rc == 0 ) {
            self->algn.alingment_ids.elements = valid_qty;
	    for(i=0;i<valid_qty;i++){
		self->dnbs_ids[i] = dnbs->last_rowid + i;
            }
        }
    }
    if( rc == 0 ) {
        self->match.mapq = self->data.score;
        rc = TableWriterAlgn_Write(self->writer, &self->algn, rowid);
    }
    return rc;
}
