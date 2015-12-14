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

#include <vdb/database.h>
#include <vdb/vdb-priv.h>

#include <kdb/manager.h>

#include <insdc/sra.h>
#include <insdc/insdc.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "Globals.h"
#include <align/writer-sequence.h>
#include "sequence-writer.h"

/* MARK: Sequence Object */

Sequence *SequenceInit(Sequence *self, VDatabase *db) {
    memset(self, 0, sizeof(*self));
    self->db = db;
    VDatabaseAddRef(db);
    return self;
}

static rc_t getTable(Sequence *self, bool color)
{
    int const options = (color ? ewseq_co_ColorSpace : 0)
                      | (G.hasTI ? ewseq_co_TI : 0)
                      | (G.globalMode == mode_Remap ? (ewseq_co_SaveRead | ewseq_co_KeepKey) : 0)
                      | ewseq_co_NoLabelData
                      | ewseq_co_SpotGroup;

    if (self->tbl) return 0;

    return TableWriterSeq_Make(&self->tbl, self->db,
                               options,
                               G.QualQuantizer);
}

rc_t SequenceWriteRecord(Sequence *self,
                         SequenceRecord const *rec,
                         bool color,
                         bool isDup,
                         INSDC_SRA_platform_id platform
                         )
{
    rc_t rc = 0;
    uint8_t nreads = rec->numreads;
    unsigned i;
    unsigned seqLen;
    int64_t dummyRowId;

    uint8_t readInfo[4096];
    void *h_readInfo = NULL;

    INSDC_coord_zero *readStart = (void *)readInfo;
    INSDC_coord_len *readLen;
    uint8_t *alcnt;
    INSDC_SRA_xread_type *readType;
    INSDC_SRA_read_filter *readFilter;
    bool *mask = NULL;
    size_t const elemSize = sizeof(alcnt[0]) + sizeof(readType[0])
                          + sizeof(readStart[0]) + sizeof(readLen[0])
                          + sizeof(readFilter[0]);

    TableWriterSeqData data;

    assert(G.mode == mode_Archive);

    for (i = seqLen = 0; i != nreads; ++i) {
        seqLen += rec->readLen[i];
    }

    if (nreads * elemSize + G.keepMismatchQual * seqLen * sizeof(mask[0]) > sizeof(readInfo))
    {
        h_readInfo = malloc(nreads * elemSize + G.keepMismatchQual * seqLen * sizeof(mask[0]));
        if (h_readInfo == NULL)
            return RC(rcAlign, rcTable, rcWriting, rcMemory, rcExhausted);
        readStart = h_readInfo;
    }
    readLen = (INSDC_coord_len *)&readStart[nreads];
    alcnt = (uint8_t *)&readLen[nreads];
    readType = (INSDC_SRA_xread_type *)&alcnt[nreads];
    readFilter = (INSDC_SRA_read_filter *)&readType[nreads];

    if (G.keepMismatchQual) {
        mask = (bool *)&readFilter[nreads];

        for (i = 0; i != seqLen; ++i) {
            mask[i] = (rec->qual[i] & 0x80) != 0;
        }
    }

    for (i = 0; i != nreads; ++i) {
        int const count = rec->aligned[i] ? 1 : 0;
        int const len = rec->readLen[i];
        int const start = rec->readStart[i];
        int const type = len == 0 ? SRA_READ_TYPE_TECHNICAL : SRA_READ_TYPE_BIOLOGICAL;
        int const dir = len == 0 ? 0 : rec->orientation[i] ? SRA_READ_TYPE_REVERSE : SRA_READ_TYPE_FORWARD;
        int const filter = isDup ? SRA_READ_FILTER_CRITERIA : rec->is_bad[i] ? SRA_READ_FILTER_REJECT : SRA_READ_FILTER_PASS;

        alcnt[i] = count;
        readLen[i] = len;
        readStart[i] = start;
        readType[i] = type | dir;
        readFilter[i] = filter;
    }

    memset(&data, 0, sizeof(data));

    data.sequence.buffer = rec->seq;
    data.sequence.elements = seqLen;

    data.quality.buffer = rec->qual;
    data.quality.elements = seqLen;

    if (G.keepMismatchQual) {
        data.no_quantize_mask.buffer = mask;
        data.no_quantize_mask.elements = seqLen;
    }

    data.alignment_count.buffer = alcnt;
    data.alignment_count.elements = nreads;

    data.nreads = nreads;

    data.read_type.buffer = readType;
    data.read_type.elements = nreads;

    data.read_start.buffer = readStart;
    data.read_start.elements = nreads;

    data.read_len.buffer = readLen;
    data.read_len.elements = nreads;

    data.tmp_key_id = rec->keyId;

    data.spot_group.buffer = rec->spotGroup;
    data.spot_group.elements = rec->spotGroupLen;

    data.cskey.buffer = rec->cskey;
    data.cskey.elements = nreads;

    data.read_filter.buffer = readFilter;
    data.read_filter.elements = nreads;

    data.platform.buffer = &platform;
    data.platform.elements = 1;

    data.ti.buffer = rec->ti;
    data.ti.elements = nreads;

    if (!G.no_real_output) {
        rc = getTable(self, color);
        if (rc == 0) {
            rc = TableWriterSeq_Write(self->tbl, &data, &dummyRowId);
        }
    }

    if (h_readInfo)
        free(h_readInfo);

    return rc;
}

static rc_t ReadSequenceData(TableWriterSeqData *const data, VCursor const *const curs, int64_t const row, uint32_t const colId[])
{
    int i;
    
    memset(data, 0, sizeof(*data));
    
    for (i = 0; i <= 8; ++i) {
        uint32_t elem_bits = 0;
        uint32_t row_len = 0;
        uint32_t boff = 0;
        void const *base = NULL;
        rc_t const rc = VCursorCellDataDirect(curs, row, colId[i], &elem_bits, &base, &boff, &row_len);
        if (rc == 0) {
            TableWriterData *tdata = NULL;
            
            switch (i) {
                case 0:
                    assert(elem_bits == sizeof(data->tmp_key_id) * 8);
                    assert(row_len == 1);
                    memcpy(&data->tmp_key_id, base, sizeof(data->tmp_key_id));
                    break;
                case 1:
                    tdata = &data->sequence;
                    break;
                case 2:
                    tdata = &data->quality;
                    break;
                case 3:
                    tdata = &data->read_type;
                    break;
                case 4:
                    tdata = &data->read_start;
                    break;
                case 5:
                    tdata = &data->read_len;
                    break;
                case 6:
                    tdata = &data->spot_group;
                    break;
                case 7:
                    tdata = &data->read_filter;
                    break;
                case 8:
                    tdata = &data->platform;
                    break;
                default:
                    assert(!"reachable");
                    break;
            }
            if (tdata) {
                tdata->buffer = base;
                tdata->elements = row_len;
            }
        }
        else
            return rc;
    }
    return 0;
}

rc_t SequenceDoneWriting(Sequence *self)
{
    if (G.mode == mode_Remap) {
        /* copy the SEQUENCE table from the first output */
        VDBManager *mgr = NULL;
        rc_t rc;
        
        getTable(self, false);

        rc = VDatabaseOpenManagerUpdate(self->db, &mgr);
        assert(rc == 0);
        
        if (rc == 0) {
            VDatabase const *db = NULL;
            
            rc = VDBManagerOpenDBRead(mgr, &db, NULL, G.firstOut);
            assert(rc == 0);
            
            VDBManagerRelease(mgr);
            if (rc == 0) {
                VTable const *tbl = NULL;
                
                rc = VDatabaseOpenTableRead(db, &tbl, "SEQUENCE");
                assert(rc == 0);
                VDatabaseRelease(db);
                if (rc == 0) {
                    VCursor const *curs = NULL;
                    rc = VTableCreateCursorRead(tbl, &curs);
                    assert(rc == 0);
                    VTableRelease(tbl);
                    if (rc == 0) {
                        uint32_t colId[9];
                        
                        rc = VCursorAddColumn(curs, &colId[0], "TMP_KEY_ID");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[1], "(INSDC:dna:text)READ");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[2], "QUALITY");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[3], "READ_TYPE");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[4], "READ_START");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[5], "READ_LEN");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[6], "SPOT_GROUP");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[7], "READ_FILTER");
                        assert(rc == 0);
                        rc = VCursorAddColumn(curs, &colId[8], "PLATFORM");
                        assert(rc == 0);
                        if (rc == 0) {
                            rc = VCursorOpen(curs);
                            assert(rc == 0);
                            if (rc == 0) {
                                int64_t first;
                                uint64_t count;
                                uint64_t row;
                                TableWriterSeqData data;
                                
                                rc = VCursorIdRange(curs, colId[0], &first, &count);
                                assert(rc == 0);
                                for (row = 0; row < count; ++row) {
                                    int64_t dummyRowId = 0;
                                    
                                    rc = ReadSequenceData(&data, curs, row+first, colId);
                                    assert(rc == 0);
                                    if (rc) return rc;

                                    data.nreads = data.read_start.elements;

                                    rc = TableWriterSeq_Write(self->tbl, &data, &dummyRowId);
                                    assert(rc == 0);
                                    if (rc) return rc;
                                }
                            }
                        }
                        VCursorRelease(curs);
                    }
                }
            }
        }
    }
    return TableWriterSeq_TmpKeyStart(self->tbl);
}

rc_t SequenceReadKey(const Sequence *cself, int64_t row, uint64_t *keyId)
{
    return TableWriterSeq_TmpKey(cself->tbl, row, keyId);
}

rc_t SequenceUpdateAlignData(Sequence *self, int64_t rowId, unsigned nreads,
                             int64_t const primeId[/* nreads */],
                             uint8_t const algnCnt[/* nreads */])
{
    TableWriterData data[2];
    
    data[0].buffer = primeId; data[0].elements = nreads;
    data[1].buffer = algnCnt; data[1].elements = nreads;

    return TableWriterSeq_WriteAlignmentData(self->tbl, rowId, &data[0], &data[1]);
}

void SequenceWhack(Sequence *self, bool commit) {
    uint64_t dummyRows;
    
    if (self->tbl == NULL)
        return;
    
    (void)TableWriterSeq_Whack(self->tbl, commit, &dummyRows);
    if (G.mode == mode_Remap) {
        /* This only happens for the second and subsequent loads.
         * Cleaning up the first load is handled by the bam-load itself
         * when everything is done.
         */
        VTable *tbl = NULL;
        rc_t rc = VDatabaseOpenTableUpdate(self->db, &tbl, "SEQUENCE");
        assert(rc == 0);
        VTableDropColumn(tbl, "TMP_KEY_ID");
        VTableDropColumn(tbl, "READ");
        VTableRelease(tbl);
    }
    VDatabaseRelease(self->db);
}

/* MARK: SequenceRecord Object */
static
rc_t SequenceRecordResize(SequenceRecord *self,
                          KDataBuffer *storage,
                          unsigned numreads,
                          unsigned seqLen)
{
    size_t sz;
    rc_t rc;
    
    sz = seqLen * (sizeof(self->seq[0]) + sizeof(self->qual[0])) +
         numreads * (sizeof(self->ti) +
                     sizeof(self->readStart[0]) +
                     sizeof(self->readLen[0]) +
                     sizeof(self->aligned[0]) + 
                     sizeof(self->orientation[0]) +
                     sizeof(self->alignmentCount[0]) +
                     sizeof(self->cskey[0])
                     );
    storage->elem_bits = 8;
    rc = KDataBufferResize(storage, sz);
    if (rc)
        return rc;
    self->numreads = numreads;
    
    self->ti = (uint64_t *)storage->base;
    self->readStart = (uint32_t *)&self->ti[numreads];
    self->readLen = (uint32_t *)&self->readStart[numreads];
    self->aligned = (bool *)&self->readLen[numreads];
    self->orientation = (uint8_t *)&self->aligned[numreads];
    self->is_bad = (uint8_t *)&self->orientation[numreads];
    self->alignmentCount = (uint8_t *)&self->is_bad[numreads];
    self->cskey = (char *)&self->alignmentCount[numreads];
    self->seq = (char *)&self->cskey[numreads];
    self->qual = (uint8_t *)&self->seq[seqLen];

    self->spotGroup = NULL;
    self->spotGroupLen = 0;
    
    return 0;
}

rc_t SequenceRecordInit(SequenceRecord *self, unsigned numreads, unsigned readLen[])
{
    unsigned i;
    unsigned seqlen = 0;
    rc_t rc;
    
    for (i = 0; i != numreads; ++i) {
        seqlen += readLen[i];
    }
    rc = SequenceRecordResize(self, &self->storage, numreads, seqlen);
    if (rc)
        return rc;
    memset(self->storage.base, 0, KDataBufferBytes(&self->storage));
    
    for (seqlen = 0, i = 0; i != numreads; ++i) {
        self->readLen[i] = readLen[i];
        self->readStart[i] = seqlen;
        seqlen += readLen[i];
    }
    self->numreads = numreads;
    memset(self->cskey, 'T', numreads);
    return 0;
}

rc_t SequenceRecordAppend(SequenceRecord *self,
                          const SequenceRecord *other
                          )
{
    /* save the locations of the original data */
    unsigned const seq = (uint8_t const *)self->seq - (uint8_t const *)self->storage.base;
    unsigned const qual = (uint8_t const *)self->qual - (uint8_t const *)self->storage.base;
    unsigned const cskey = (uint8_t const *)self->cskey - (uint8_t const *)self->storage.base;
    unsigned const alignmentCount = (uint8_t const *)self->alignmentCount - (uint8_t const *)self->storage.base;
    unsigned const is_bad = (uint8_t const *)self->is_bad - (uint8_t const *)self->storage.base;
    unsigned const orientation = (uint8_t const *)self->orientation - (uint8_t const *)self->storage.base;
    unsigned const aligned = (uint8_t const *)self->aligned - (uint8_t const *)self->storage.base;
    unsigned const ti = (uint8_t const *)self->ti - (uint8_t const *)self->storage.base;
    unsigned const readLen = (uint8_t const *)self->readLen - (uint8_t const *)self->storage.base;
    unsigned const readStart = (uint8_t const *)self->readStart - (uint8_t const *)self->storage.base;

    rc_t rc;
    unsigned seqlen;
    unsigned otherSeqlen;
    unsigned i;
    unsigned numreads = self->numreads;
    
    for (seqlen = 0, i = 0; i != numreads; ++i) {
        seqlen += self->readLen[i];
    }
    for (otherSeqlen = 0, i = 0; i != other->numreads; ++i) {
        otherSeqlen += other->readLen[i];
    }

    rc = SequenceRecordResize(self, &self->storage, self->numreads + other->numreads, seqlen + otherSeqlen);
    if (rc)
        return rc;
    /* this needs to be reverse order from assignment in Resize function
     * these regions can overlap
     */
    memmove(self->qual,             &((uint8_t const *)self->storage.base)[qual],              seqlen);
    memmove(self->seq,              &((uint8_t const *)self->storage.base)[seq],               seqlen);
    memmove(self->cskey,            &((uint8_t const *)self->storage.base)[cskey],             numreads * sizeof(self->cskey[0]));
    memmove(self->alignmentCount,   &((uint8_t const *)self->storage.base)[alignmentCount],    numreads * sizeof(self->alignmentCount[0]));
    memmove(self->is_bad,           &((uint8_t const *)self->storage.base)[is_bad],            numreads * sizeof(self->is_bad[0]));
    memmove(self->orientation,      &((uint8_t const *)self->storage.base)[orientation],       numreads * sizeof(self->orientation[0]));
    memmove(self->aligned,          &((uint8_t const *)self->storage.base)[aligned],           numreads * sizeof(self->aligned[0]));
    memmove(self->readLen,          &((uint8_t const *)self->storage.base)[readLen],           numreads * sizeof(self->readLen[0]));
    memmove(self->ti,               &((uint8_t const *)self->storage.base)[ti],                numreads * sizeof(self->ti[0]));
    
    memcpy(&self->ti[numreads],             other->ti,              other->numreads * sizeof(self->ti[0]));
    memcpy(&self->readLen[numreads],        other->readLen,         other->numreads * sizeof(self->readLen[0]));
    memcpy(&self->aligned[numreads],        other->aligned,         other->numreads * sizeof(self->aligned[0]));
    memcpy(&self->orientation[numreads],    other->orientation,     other->numreads * sizeof(self->orientation[0]));
    memcpy(&self->is_bad[numreads],         other->is_bad,          other->numreads * sizeof(self->is_bad[0]));
    memcpy(&self->alignmentCount[numreads], other->alignmentCount,  other->numreads * sizeof(self->alignmentCount[0]));
    memcpy(&self->cskey[numreads],          other->cskey,           other->numreads * sizeof(self->cskey[0]));
    memcpy(&self->seq[seqlen],              other->seq,             otherSeqlen);
    memcpy(&self->qual[seqlen],             other->qual,            otherSeqlen);
    
    for (i = 0, seqlen = 0; i != self->numreads; ++i) {
        self->readStart[i] = seqlen;
        seqlen += self->readLen[i];
    }
    
    return 0;
}
