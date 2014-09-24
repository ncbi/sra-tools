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
#ifndef _tools_cg_load_writer_evidence_dnbs_h_
#define _tools_cg_load_writer_evidence_dnbs_h_

#include <insdc/insdc.h>
#include <vdb/database.h>
#include <sra/sradb.h>

#include "defs.h"
#include "factory-cmn.h"

typedef struct TEvidenceDnbsData_dnb_struct {

    /* translated:
       slide+lane+file_num_in_lane+dnb_offset_in_lane_file --> SEQ_SPOT_ID */
    int64_t seq_spot_id;

    char chr[CG_CHROMOSOME_NAME];
    char slide[CG_SLIDE];
    char lane[CG_LANE];
    CGFIELD15_BATCH_FILE_NUMBER file_num_in_lane;
    uint64_t dnb_offset_in_lane_file; /* zero-based */
    uint16_t allele_index;
    char side;
    char strand;
    INSDC_coord_zero offset_in_allele;
    char allele_alignment[CG_EVDNC_ALLELE_CIGAR_LEN];
    /* strlen for allele_alignment[] buffer */
    size_t allele_alignment_length;
    uint8_t mapping_quality;
    INSDC_coord_zero offset_in_reference;
    /* moved into reading function since it is not used after reading anyway
    char reference_alignment[CG_EVDNC_ALLELE_CIGAR_LEN];
    INSDC_coord_zero mate_offset_in_reference;
    char mate_reference_alignment[CG_EVDNC_ALLELE_CIGAR_LEN];
    uint16_t score_allele[3];*/
    char read[CG_EVDNC_SPOT_LEN];
    size_t read_len;
    /*char qual[CG_EVDNC_SPOT_LEN];
    */
} TEvidenceDnbsData_dnb;

typedef struct TEvidenceDnbsData {
    char interval_id[CG_EVDNC_INTERVALID_LEN];
    uint16_t qty;
    uint16_t max_qty;
    TEvidenceDnbsData_dnb* dnbs;
    int64_t last_rowid; /* last used rowid in this table */
} TEvidenceDnbsData;

typedef struct CGWriterEvdDnbs CGWriterEvdDnbs;

struct TEvidenceIntervalsData;

rc_t CGWriterEvdDnbs_Make(const CGWriterEvdDnbs** cself, TEvidenceDnbsData** data,
                          VDatabase* db, const ReferenceMgr* rmgr, const uint32_t options);

rc_t CGWriterEvdDnbs_Whack(const CGWriterEvdDnbs* cself, bool commit, uint64_t* rows);

rc_t CGWriterEvdDnbs_SetSEQ(const CGWriterEvdDnbs* cself, uint16_t dnb, const int64_t seq_spot_id_1st);

rc_t CGWriterEvdDnbs_Write(const CGWriterEvdDnbs* cself, const struct TEvidenceIntervalsData* ref, int64_t ref_rowid);

#endif /* _tools_cg_load_writer_evidence_dnbs_h_ */
