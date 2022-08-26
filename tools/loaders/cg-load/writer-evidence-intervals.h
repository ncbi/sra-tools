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
#ifndef _tools_cg_load_writer_evidence_intervals
#define _tools_cg_load_writer_evidence_intervals

#include <vdb/database.h>
#include <vdb/manager.h>
#include <insdc/insdc.h>

#include "defs.h"
#include <align/writer-alignment.h>
#include <align/writer-reference.h>

typedef struct TEvidenceIntervalsData {
    char interval_id[CG_EVDNC_INTERVALID_LEN];
    char chr[CG_CHROMOSOME_NAME];
    INSDC_coord_zero offset;
    INSDC_coord_len length;
    uint16_t ploidy;
    char allele_indexes[16];
    /* translation from allele_indexes into read id for use in dnbs */
    uint8_t allele_indexes_to_read_number[CG_EVDNC_ALLELE_NUM];
    int32_t score;
    int32_t scoreVAF;
    int32_t scoreEAF;
    char allele[CG_EVDNC_ALLELE_NUM][CG_EVDNC_ALLELE_LEN];
    /* strlen for allele[3][] buffers */
    size_t allele_length[CG_EVDNC_ALLELE_NUM];
    char allele_alignment[CG_EVDNC_ALLELE_NUM][CG_EVDNC_ALLELE_CIGAR_LEN];
    /* strlen for allele_alignment[3][] buffers */
    size_t allele_alignment_length[CG_EVDNC_ALLELE_NUM];
} TEvidenceIntervalsData;

typedef struct CGWriterEvdInt CGWriterEvdInt;

struct TEvidenceDnbsData;

rc_t CGWriterEvdInt_Make(const CGWriterEvdInt** cself, TEvidenceIntervalsData** data,
                         VDatabase* db, const ReferenceMgr* rmgr, const uint32_t options);

rc_t CGWriterEvdInt_Whack(const CGWriterEvdInt* cself, bool commit, uint64_t* rows);

rc_t CGWriterEvdInt_Write(const CGWriterEvdInt* cself, const struct TEvidenceDnbsData* dnbs, int64_t* rowid);

#endif /* _tools_cg_load_writer_evidence_intervals */
