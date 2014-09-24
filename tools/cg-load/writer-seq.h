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
#ifndef _tools_cg_load_writer_seq_h_
#define _tools_cg_load_writer_seq_h_

#include <insdc/insdc.h>
#include <vdb/database.h>
#include <sra/sradb.h>
#include <align/writer-sequence.h>

typedef struct TReadsData_struct {
    uint16_t flags;
    char read[CG_READS_SPOT_LEN + 1];
    char qual[CG_READS_SPOT_LEN + 1];

    /* reverse read cache by half-dnb */
    INSDC_dna_text reverse[CG_READS_SPOT_LEN];

    TableWriterSeqData seq;
    int64_t rowid;
    uint8_t align_count[CG_READS_NREADS];
    int64_t prim_algn_id[CG_READS_NREADS];
    bool prim_is_reverse[CG_READS_NREADS];
    SRAReadTypes read_type[CG_READS_NREADS];
} TReadsData;

typedef struct CGWriterSeq CGWriterSeq;

rc_t CGWriterSeq_Make(const CGWriterSeq** cself, TReadsData** data, VDatabase* db, const uint32_t options, const char* quality_quantization);

rc_t CGWriterSeq_Whack(const CGWriterSeq* cself, bool commit, uint64_t* rows);

rc_t CGWriterSeq_Write(const CGWriterSeq* cself);

#endif /* _tools_cg_load_writer_seq_h_ */
