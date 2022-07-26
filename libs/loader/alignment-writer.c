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

#include <klib/rc.h>
#include <klib/log.h>
#include <sysalloc.h>
#include <klib/out.h>

#include <vdb/vdb-priv.h>
#include <klib/data-buffer.h>

#include <loader/alignment-writer.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum e_tables {
    tblPrimary,
    tblSecondary,
    tblN
};

struct AlignmentWriter {
    VDatabase *db;
    TableWriterAlgn const *tbl[tblN];
    int64_t rowId;
    int st;
};

AlignmentWriter *AlignmentMake(VDatabase *db) {
    AlignmentWriter *self = calloc(1, sizeof(*self));
    
    if (self) {
        self->db = db;
        VDatabaseAddRef(self->db);
    }
    return self;
}

static rc_t SetColumnDefaults(TableWriterAlgn const *tbl)
{
    return 0;
}

static rc_t WritePrimaryRecord(AlignmentWriter *const self, AlignmentRecord *const data, bool expectUnsorted)
{
    if (self->tbl[tblPrimary] == NULL) {
        rc_t rc = TableWriterAlgn_Make(&self->tbl[tblPrimary], self->db,
                                       ewalgn_tabletype_PrimaryAlignment,
                                       ewalgn_co_TMP_KEY_ID + 
                                       (expectUnsorted ? ewalgn_co_unsorted : 0));
        if (rc)
            return rc;
        rc = SetColumnDefaults(self->tbl[tblPrimary]);
        if (rc)
            return rc;
    }
    return TableWriterAlgn_Write(self->tbl[tblPrimary], &data->data, &data->alignId);
}

static rc_t WriteSecondaryRecord(AlignmentWriter *const self, AlignmentRecord *const data, bool expectUnsorted)
{
    if (self->tbl[tblSecondary] == NULL) {
        rc_t rc = TableWriterAlgn_Make(&self->tbl[tblSecondary], self->db,
                                       ewalgn_tabletype_SecondaryAlignment,
                                       ewalgn_co_TMP_KEY_ID + 
                                       (expectUnsorted ? ewalgn_co_unsorted : 0));
        if (rc)
            return rc;
        rc = SetColumnDefaults(self->tbl[tblSecondary]);
        if (rc)
            return rc;
    }
#if 1
    /* try to make consistent with cg-load */
    if (data->mate_ref_pos == 0) {
        data->data.mate_ref_orientation.elements = 0;
    }
#endif
    return TableWriterAlgn_Write(self->tbl[tblSecondary], &data->data, &data->alignId);
}

rc_t AlignmentWriteRecord(AlignmentWriter *const self, AlignmentRecord *const data, bool expectUnsorted)
{
    return data->isPrimary ? WritePrimaryRecord(self, data, expectUnsorted) : WriteSecondaryRecord(self, data, expectUnsorted);
}

rc_t AlignmentStartUpdatingSpotIds(AlignmentWriter *const self)
{
    return 0;
}

rc_t AlignmentGetSpotKey(AlignmentWriter * const self, uint64_t *keyId, int64_t *alignId, bool *isPrimary)
{
    rc_t rc;
    
    switch (self->st) {
    case 0:
        rc = TableWriterAlgn_TmpKeyStart(self->tbl[tblPrimary]);
        if (rc)
            break;
        self->rowId = 0;
        ++self->st;
    case 1:
        rc = TableWriterAlgn_TmpKey(self->tbl[tblPrimary], ++self->rowId, keyId);
        if (rc == 0) {
            *alignId = self->rowId;
            *isPrimary = true;
            break;
        }
        ++self->st;
        if (GetRCState(rc) != rcNotFound || GetRCObject(rc) != rcRow || self->tbl[tblSecondary] == NULL)
            break;
    case 2:
        rc = TableWriterAlgn_TmpKeyStart(self->tbl[tblSecondary]);
        if (rc)
            break;
        self->rowId = 0;
        ++self->st;
    case 3:
        rc = TableWriterAlgn_TmpKey(self->tbl[tblSecondary], ++self->rowId, keyId);
        if (rc == 0) {
            *alignId = self->rowId;
            *isPrimary = false;
            break;
        }
        if (GetRCState(rc) != rcNotFound || GetRCObject(rc) != rcRow)
            break;
        ++self->st;
        break;
    default:
        rc = RC(rcAlign, rcTable, rcUpdating, rcError, rcIgnored);
        break;
    }
    return rc;
}

rc_t AlignmentGetRefPos(AlignmentWriter *const self, int64_t row, ReferenceStart *const rslt)
{
    switch (self->st) {
    case 1:
        return TableWriterAlgn_RefStart(self->tbl[tblPrimary], self->rowId, rslt);
    case 3:
    default:
        return RC(rcAlign, rcTable, rcUpdating, rcSelf, rcInconsistent);
    }
}

rc_t AlignmentUpdateInfo(AlignmentWriter * const self, int64_t const spotId, int64_t const mateId, ReferenceStart const *mateRefPos)
{
    switch (self->st) {
    case 1:
        return TableWriterAlgn_Write_SpotInfo(self->tbl[tblPrimary], self->rowId, spotId, mateId, mateRefPos);
    case 3:
        return TableWriterAlgn_Write_SpotInfo(self->tbl[tblSecondary], self->rowId, spotId, 0, 0);
    default:
        return RC(rcAlign, rcTable, rcUpdating, rcSelf, rcInconsistent);
    }
}

rc_t AlignmentWhack(AlignmentWriter * const self, bool const commit) 
{
    rc_t const rc = TableWriterAlgn_Whack(self->tbl[tblPrimary], commit, NULL);
    rc_t const rc2 = self->tbl[tblSecondary] ? TableWriterAlgn_Whack(self->tbl[tblSecondary], commit | (rc == 0), NULL) : 0;

    VDatabaseRelease(self->db);
    free(self);
    return rc ? rc : rc2;
}

static size_t AlignmentRecordBufferSize(unsigned const readlen, bool const hasMismatchQual)
{
    AlignmentRecord const *const dummy = NULL;
    size_t const elemSize = sizeof(AR_OFFSET(*dummy)[0])
					      + sizeof(AR_OFFSET_TYPE(*dummy)[0])
                          + sizeof(AR_HAS_MISMATCH(*dummy)[0])
                          + sizeof(AR_HAS_OFFSET(*dummy)[0])
                          + sizeof(AR_MISMATCH(*dummy)[0])
                          + (hasMismatchQual ? sizeof(AR_MISMATCH_QUAL(*dummy)[0]) : 0);
    
    return elemSize * readlen;
}

rc_t AlignmentRecordInit(AlignmentRecord *self, unsigned readlen,
                         bool expectUnsorted,
                         bool hasMismatchQual
                         )
{
    KDataBuffer buffer = self->buffer;

    memset(self, 0, sizeof(*self));
    buffer.elem_bits = 8;
    {
        size_t const need = AlignmentRecordBufferSize(readlen, hasMismatchQual);
        rc_t const rc = KDataBufferResize(&buffer, need);
        if (rc) return rc;
    }
    self->buffer = buffer;

    self->data.seq_read_id.buffer = &self->read_id;
    self->data.seq_read_id.elements = 1;
    self->data.ref_id.buffer = &self->ref_id;
    self->data.ref_id.elements = 1;
    if (expectUnsorted) {
        self->data.ref_start.buffer = &self->ref_start;
        self->data.ref_start.elements = 1;
    }
    else {
        self->data.global_ref_start.buffer = &self->global_ref_start;
        self->data.global_ref_start.elements = 1;
    }
    self->data.ref_orientation.buffer = &self->ref_orientation;
    self->data.ref_orientation.elements = 1;
    self->data.mapq.buffer = &self->mapq;
    self->data.mapq.elements = 1;
    self->data.tmp_key_id.buffer = &self->tmp_key_id;
    self->data.tmp_key_id.elements = 1;
    
    self->data.read_start.buffer = &self->read_start;
    self->data.read_start.elements = 1;
    self->data.read_len.buffer = &self->read_len;
    self->data.read_len.elements = 1;
    
    self->data.mate_ref_orientation.buffer = &self->mate_ref_orientation;
    self->data.mate_ref_orientation.elements = 1;
    self->data.mate_ref_id.buffer = &self->mate_ref_id;
    self->data.mate_ref_id.elements = 1;
    self->data.mate_ref_pos.buffer = &self->mate_ref_pos;
    self->data.mate_ref_pos.elements = 1;
    self->data.mate_align_id.buffer = &self->mate_align_id;
    self->data.mate_align_id.elements = 1;
    self->data.template_len.buffer = &self->template_len;
    self->data.template_len.elements = 1;
    
    self->data.ref_offset.buffer = buffer.base;
    self->data.ref_offset_type.buffer = &AR_OFFSET(*self)[readlen];
    self->data.has_mismatch.buffer = &AR_OFFSET_TYPE(*self)[readlen];
    self->data.has_mismatch.elements = readlen;
    self->data.has_ref_offset.buffer = &AR_HAS_MISMATCH(*self)[readlen];
    self->data.has_ref_offset.elements = readlen;
    self->data.mismatch.buffer = &AR_HAS_OFFSET(*self)[readlen];
    
    if (hasMismatchQual)
        self->data.mismatch_qual.buffer = (uint8_t *)&AR_MISMATCH(*self)[readlen];
    
    return 0;
}
