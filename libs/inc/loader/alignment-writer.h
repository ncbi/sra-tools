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

#ifndef BAM_LOAD_ALIGNMENT_WRITER_H_
#define BAM_LOAD_ALIGNMENT_WRITER_H_ 1

#include <klib/text.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <insdc/insdc.h>

#include <align/writer-alignment.h>

typedef struct AlignmentWriter AlignmentWriter;
typedef struct AlignmentRecord AlignmentRecord;

struct AlignmentRecord {
    KDataBuffer buffer;
    TableWriterAlgnData data;
    int64_t alignId;
    bool isPrimary;

    INSDC_coord_one read_id;
    int64_t ref_id;
    INSDC_coord_zero ref_start;
    uint64_t global_ref_start;
    bool ref_orientation;
    uint32_t mapq;
    uint64_t tmp_key_id;

    INSDC_coord_zero read_start;
    INSDC_coord_len read_len;

    bool mate_ref_orientation;
    int64_t mate_ref_id;
    INSDC_coord_zero mate_ref_pos;
    int64_t mate_align_id;
    int32_t template_len;
};

#define AR_REF_ID(X) ((X).ref_id)
#define AR_REF_START(X) ((X).global_ref_start)
#define AR_REF_LEN(X) ((X).ref_len)
#define AR_REF_ORIENT(X) ((X).ref_orientation)
#define AR_READNO(X) ((X).read_id)
#define AR_MAPQ(X) ((X).mapq)
#define AR_KEY(X) ((X).tmp_key_id)

#define AR_BASECOUNT(X) ((X).data.has_mismatch.elements)
#define AR_HAS_MISMATCH(X) ((bool *)((X).data.has_mismatch.buffer))
#define AR_HAS_OFFSET(X) ((bool *)((X).data.has_ref_offset.buffer))

#define AR_NUM_MISMATCH(X) ((X).data.mismatch.elements)
#define AR_MISMATCH(X) ((char *)((X).data.mismatch.buffer))

#define AR_NUM_MISMATCH_QUAL(X) ((X).data.mismatch_qual.elements)
#define AR_MISMATCH_QUAL(X) ((uint8_t *)((X).data.mismatch_qual.buffer))

#define AR_NUM_OFFSET(X) ((X).data.ref_offset.elements)
#define AR_OFFSET(X) ((INSDC_coord_zero *)((X).data.ref_offset.buffer))
#define AR_OFFSET_TYPE(X) ((uint8_t *)((X).data.ref_offset_type.buffer))

AlignmentWriter *AlignmentMake(VDatabase *db);

rc_t AlignmentWriteRecord(AlignmentWriter * const self, AlignmentRecord * const data, bool expectUnsorted);

rc_t AlignmentStartUpdatingSpotIds(AlignmentWriter * const self);

rc_t AlignmentGetSpotKey(AlignmentWriter * const self, uint64_t *keyId, int64_t *alignId, bool *isPrimary);

rc_t AlignmentGetRefPos(AlignmentWriter *const self, int64_t row, ReferenceStart *const rslt);

rc_t AlignmentUpdateInfo(AlignmentWriter *const self, int64_t const spotId,
                         int64_t const mateId, ReferenceStart const *const mateRefPos);

rc_t AlignmentWhack(AlignmentWriter * const self, bool const commit);

rc_t AlignmentRecordInit(AlignmentRecord *self, unsigned readlen,
                         bool expectUnsorted,
                         bool hasMismatchQual
                         );

#endif
