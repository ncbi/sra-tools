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
#include <kfs/file.h>

#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <align/writer-refseq.h>

#include <loader/reference-writer.h>

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define SORTED_OPEN_TABLE_LIMIT (2)
#define SORTED_CACHE_SIZE ((2 * 1024 * 1024)/(SORTED_OPEN_TABLE_LIMIT))

#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#define UNSORTED_OPEN_TABLE_LIMIT (8)
#define UNSORTED_CACHE_SIZE ((1024 * 1024 * 1024)/(UNSORTED_OPEN_TABLE_LIMIT))
#else
#define UNSORTED_OPEN_TABLE_LIMIT (64)
#define UNSORTED_CACHE_SIZE (350 * 1024 * 1024)
#endif

#if _DEBUGGING
#define DUMP_CONFIG 1
#endif

struct overlap_s {
    uint32_t min; /* minimum start pos of any alignment that ends in this chunk */
    uint32_t max; /* maximum end pos of any alignment that starts before this chunk and ends in this chunk */
};

extern void ReferenceMgr_DumpConfig(ReferenceMgr const *const self);

rc_t ReferenceInit(Reference *self, 
                   const VDBManager *mgr, 
                   VDatabase *db, 
                   bool expectUnsorted,
                   bool acceptHardClip,
                   char const *refXRefPath,
                   char const *inpath,
                   uint32_t maxSeqLen,
                   char const** refFiles
                   )
{
    rc_t rc;
    size_t const cache = expectUnsorted ? UNSORTED_CACHE_SIZE : SORTED_CACHE_SIZE;
    unsigned const open_count = expectUnsorted ? UNSORTED_OPEN_TABLE_LIMIT : SORTED_OPEN_TABLE_LIMIT;
    
    memset(self, 0, sizeof(*self));
    
    self->coverage.elem_bits = self->mismatches.elem_bits = self->indels.elem_bits = 32;
    self->pri_align.elem_bits = self->sec_align.elem_bits = 64;
    self->pri_overlap.elem_bits = self->sec_overlap.elem_bits = sizeof(struct overlap_s) * 8;
    
    self->acceptHardClip = acceptHardClip;
    
    rc = ReferenceMgr_Make(&self->mgr, db, mgr, 
                           ewrefmgr_co_Coverage,
                           refXRefPath, inpath,
                           maxSeqLen, cache, open_count);
    if (rc == 0 && refFiles != NULL) {
        unsigned i;
        
        for (i = 0; refFiles[i]; ++i) {
            rc = ReferenceMgr_FastaPath(self->mgr, refFiles[i]);
            if (rc) {
                (void)PLOGERR(klogWarn, (klogWarn, rc, "fasta file '$(file)'", "file=%s", refFiles[i]));
                break;
            }
        }
#if DUMP_CONFIG
        if (rc == 0) {
            ReferenceMgr_DumpConfig(self->mgr);
        }
#endif
    }
    return rc;
}

static
void Unsorted(Reference *self) {
    bool dummy1 = false;
    bool dummy2 = false;
    
    (void)LOGMSG(klogWarn, "Alignments are unsorted");
    
    self->out_of_order = true;
    
    ReferenceSeq_Release(self->rseq);
    ReferenceMgr_SetCache(self->mgr, UNSORTED_CACHE_SIZE, UNSORTED_OPEN_TABLE_LIMIT);
    ReferenceMgr_GetSeq(self->mgr, &self->rseq, self->last_id, &dummy1, true, &dummy2);
    
    KDataBufferWhack(&self->sec_align);
    KDataBufferWhack(&self->pri_align);
    KDataBufferWhack(&self->mismatches);
    KDataBufferWhack(&self->indels);
    KDataBufferWhack(&self->coverage);
    KDataBufferWhack(&self->pri_overlap);
    KDataBufferWhack(&self->sec_overlap);
}

#define BAIL_ON_FAIL(STMT) do { rc_t const rc__ = (STMT); if (rc__) return rc__; } while(0)

static rc_t FlushBuffers(Reference *self, uint64_t upto, bool full, bool final, uint32_t maxSeqLen)
{
    if (!self->out_of_order && upto > 0) {
        size_t offset = 0;
        unsigned *const miss = (unsigned *)self->mismatches.base;
        unsigned *const indel = (unsigned *)self->indels.base;
        unsigned *const cov = (unsigned *)self->coverage.base;
        struct overlap_s *const pri_overlap = (struct overlap_s *)self->pri_overlap.base;
        struct overlap_s *const sec_overlap = (struct overlap_s *)self->sec_overlap.base;
        unsigned chunk = 0;
        
        while ((self->curPos + offset + (full ? 0 : maxSeqLen)) <= upto) {
            ReferenceSeqCoverage data;
            uint64_t const curPos = self->curPos + offset;
            unsigned const n = self->endPos > (curPos + maxSeqLen) ?
                               maxSeqLen : (self->endPos - curPos);
            unsigned const m = curPos + n > upto ? upto - curPos : n;
            unsigned i;
            unsigned hi;
            unsigned lo;
            
            if (n == 0) break;
            
            memset(&data, 0, sizeof(data));
            
            data.ids[ewrefcov_primary_table].elements = self->pri_align.elem_count;
            data.ids[ewrefcov_primary_table].buffer = self->pri_align.base;
            data.overlap_ref_pos[ewrefcov_primary_table] = pri_overlap[chunk].min;
            data.overlap_ref_len[ewrefcov_primary_table] = pri_overlap[chunk].max ? pri_overlap[chunk].max - curPos : 0;
            
            data.ids[ewrefcov_secondary_table].elements = self->sec_align.elem_count;
            data.ids[ewrefcov_secondary_table].buffer = self->sec_align.base;
            data.overlap_ref_pos[ewrefcov_secondary_table] = sec_overlap[chunk].min;
            data.overlap_ref_len[ewrefcov_secondary_table] = sec_overlap[chunk].max ? sec_overlap[chunk].max - curPos : 0;
            
            for (hi = 0, lo = UINT_MAX, i = 0; i != m; ++i) {
                unsigned const coverage = cov[offset + i];
                
                if (hi < coverage)
                    hi = coverage;
                if (lo > coverage)
                    lo = coverage;
            }
            data.low  = lo > 255 ? 255 : lo;
            data.high = hi > 255 ? 255 : hi;
            
            for (i = 0; i != m; ++i)
                data.mismatches += miss[offset + i];

            for (i = 0; i != m; ++i)
                data.indels += indel[offset + i];
            
            {
                rc_t rc = ReferenceSeq_AddCoverage(self->rseq, curPos, &data);
                
                if (rc) {
                    Unsorted(self);
                    return 0;
                }
            }
            
            KDataBufferResize(&self->pri_align, 0);
            KDataBufferResize(&self->sec_align, 0);
            offset += n;
            ++chunk;
        }
        if (!final && offset > 0) {
            unsigned const newChunkCount = self->pri_overlap.elem_count - chunk;
            unsigned const newBaseCount = self->endPos - self->curPos - offset;
            
            memmove(self->pri_overlap.base, pri_overlap + chunk, newChunkCount * sizeof(pri_overlap[0]));
            memmove(self->sec_overlap.base, sec_overlap + chunk, newChunkCount * sizeof(sec_overlap[0]));
            memmove(self->mismatches.base, miss + offset, newBaseCount * sizeof(miss[0]));
            memmove(self->indels.base, indel + offset, newBaseCount * sizeof(indel[0]));
            memmove(self->coverage.base, cov + offset, newBaseCount * sizeof(cov[0]));

            KDataBufferResize(&self->pri_overlap, newChunkCount);
            KDataBufferResize(&self->sec_overlap, newChunkCount);
            
            self->curPos += offset;
        }
    }
    return 0;
}

rc_t ReferenceSetFile(Reference *self, const char id[],
                      uint64_t length, uint8_t const md5[16],
                      uint32_t maxSeqLen, bool *shouldUnmap)
{
    ReferenceSeq const *rseq;
    bool wasRenamed = false;
    unsigned n;
    
    for (n = 0; ; ++n) {
        if (self->last_id[n] != id[n])
            break;
        if (self->last_id[n] == 0 && id[n] == 0)
            return 0;
    }
    while (id[n])
        ++n;
    if (n >= sizeof(self->last_id))
        return RC(rcApp, rcTable, rcAccessing, rcParam, rcTooLong);
    
    BAIL_ON_FAIL(FlushBuffers(self, self->length, true, true, maxSeqLen));
    BAIL_ON_FAIL(ReferenceMgr_GetSeq(self->mgr, &rseq, id, shouldUnmap, true, &wasRenamed));

    if (self->rseq)
        ReferenceSeq_Release(self->rseq);

    self->rseq = rseq;
    
    memmove(self->last_id, id, n + 1);
    self->curPos = self->endPos = 0;
    self->length = length;
    self->lastOffset = 0;

    if(!self->out_of_order) (void)PLOGMSG(klogInfo, (klogInfo, "Processing Reference '$(id)'", "id=%s", id));
    
    return 0;
}

rc_t ReferenceVerify(Reference const *self, char const id[], uint64_t length, uint8_t const md5[16])
{
    bool dummy = false;
    return ReferenceMgr_Verify(self->mgr, id, length, md5, true, &dummy);
}

rc_t ReferenceGet1stRow(Reference const *self, int64_t *refID, char const refName[])
{
    ReferenceSeq const *rseq;
    bool shouldUnmap = false;
    bool wasRenamed = false;
    rc_t rc = ReferenceMgr_GetSeq(self->mgr, &rseq, refName, &shouldUnmap, true, &wasRenamed);
    if (rc == 0) {
        assert(shouldUnmap == false);
        rc = ReferenceSeq_Get1stRow(rseq, refID);
        ReferenceSeq_Release(rseq);
    }
    return rc;
}

static
rc_t ReferenceAddCoverage(Reference *const self,
                          unsigned const refStart,
                          unsigned const refLength,
                          uint32_t const mismatches,
                          uint32_t const indels,
                          bool const isPrimary,
                          uint32_t maxSeqLen
                          )
{
    unsigned const refEnd = refStart + refLength;
    
    if (refEnd > self->endPos) {
        unsigned const t1 = refEnd + (maxSeqLen - 1);
        unsigned const adjust = t1 % maxSeqLen;
        unsigned const newEndPos = t1 - adjust;
        unsigned const baseCount = self->endPos - self->curPos;
        unsigned const newBaseCount = newEndPos - self->curPos;
        
        BAIL_ON_FAIL(KDataBufferResize(&self->coverage, newBaseCount));
        BAIL_ON_FAIL(KDataBufferResize(&self->mismatches, newBaseCount));
        BAIL_ON_FAIL(KDataBufferResize(&self->indels, newBaseCount));
        
        memset(&((unsigned *)self->coverage.base)[baseCount], 0, (newBaseCount - baseCount) * sizeof(unsigned));
        memset(&((unsigned *)self->mismatches.base)[baseCount], 0, (newBaseCount - baseCount) * sizeof(unsigned));
        memset(&((unsigned *)self->indels.base)[baseCount], 0, (newBaseCount - baseCount) * sizeof(unsigned));
        self->endPos = newEndPos;
    }
    if ((refEnd - self->curPos) / maxSeqLen >= self->pri_overlap.elem_count) {
        unsigned const chunks = (refEnd - self->curPos) / maxSeqLen + 1;
        unsigned const end = self->pri_overlap.elem_count;
        
        BAIL_ON_FAIL(KDataBufferResize(&self->pri_overlap, chunks));
        BAIL_ON_FAIL(KDataBufferResize(&self->sec_overlap, chunks));
        
        memset(&((struct overlap_s *)self->pri_overlap.base)[end], 0, (chunks - end) * sizeof(struct overlap_s));
        memset(&((struct overlap_s *)self->sec_overlap.base)[end], 0, (chunks - end) * sizeof(struct overlap_s));
    }
    BAIL_ON_FAIL(FlushBuffers(self, refStart, false, false, maxSeqLen));
    if (!self->out_of_order) {
        unsigned const startBase = refStart - self->curPos;
        unsigned const endChunk = (startBase + refLength) / maxSeqLen;
        KDataBuffer *const overlapBuffer = isPrimary ? &self->pri_overlap : &self->sec_overlap;
        unsigned *const cov = &((unsigned *)self->coverage.base)[startBase];
        unsigned i;
        
        ((unsigned *)self->mismatches.base)[startBase] += mismatches;
        ((unsigned *)self->indels.base)[startBase] += indels;
        
        if (((struct overlap_s *)overlapBuffer->base)[endChunk].min == 0 || 
            ((struct overlap_s *)overlapBuffer->base)[endChunk].min > refStart)
        {
            ((struct overlap_s *)overlapBuffer->base)[endChunk].min = refStart;
        }
        if (endChunk != 0 &&
            ((struct overlap_s *)overlapBuffer->base)[endChunk].max < refStart + refLength)
        {
            ((struct overlap_s *)overlapBuffer->base)[endChunk].max = refStart + refLength;
        }
        
        for (i = 0; i != refLength; ++i) {
            if (cov[i] < UINT_MAX)
                ++cov[i];
        }
    }
    return 0;
}

static void GetCounts(AlignmentRecord const *data, unsigned const seqLen,
                      unsigned *const nMatch,
                      unsigned *const nMiss,
                      unsigned *const nIndels)
{
    bool const *const has_mismatch = data->data.has_mismatch.buffer;
    bool const *const has_offset = data->data.has_ref_offset.buffer;
    int32_t const *const ref_offset = data->data.ref_offset.buffer;
    uint8_t const *const ref_offset_type = data->data.ref_offset_type.buffer;
    unsigned misses = 0;
    unsigned matchs = 0;
    unsigned insert = 0;
    unsigned delete = 0;
    unsigned j = 0;
    unsigned i;
    
    for (i = 0; i < seqLen; ) {
        if (has_offset[i]) {
            int const offs = ref_offset[j];
            int const type = ref_offset_type[j];
            
            ++j;
            if (type == 0) {
                if (offs < 0)
                    ++insert;
                else
                    ++delete;
            }
            if (offs < 0) {
                i += (unsigned)(-offs);
                continue;
            }
        }
        if (has_mismatch[i])
            ++misses;
        else
            ++matchs;
        ++i;
    }
    *nMatch = matchs;
    *nMiss  = misses;
    *nIndels = insert + delete;
}

rc_t ReferenceRead(Reference *self, AlignmentRecord *data,
                   uint64_t const pos,
                   uint32_t const rawCigar[], uint32_t const cigCount,
                   char const seqDNA[], uint32_t const seqLen,
                   uint8_t const rna_orient, uint32_t *matches,
                   bool acceptNoMatch, unsigned minMatchCount, uint32_t maxSeqLen)
{
    *matches = 0;
    BAIL_ON_FAIL(ReferenceSeq_Compress(self->rseq, 
                                       (self->acceptHardClip ? ewrefmgr_co_AcceptHardClip : 0) + ewrefmgr_cmp_Binary, 
                                       pos,
                                       seqDNA, seqLen, rawCigar, cigCount, 0, NULL, 0, 0, NULL, 0, rna_orient, &data->data));

    if (!acceptNoMatch && data->data.ref_len == 0)
        return RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
    
    if (!self->out_of_order && pos < self->lastOffset) {
        Unsorted(self);
    }
    if (self->out_of_order)
        return 0;
    else {
        unsigned nmis;
        unsigned nmatch;
        unsigned indels;

        self->lastOffset = data->data.effective_offset;
        GetCounts(data, seqLen, &nmatch, &nmis, &indels);
        *matches = nmatch;
        
        if (acceptNoMatch || nmatch >= minMatchCount)
            return ReferenceAddCoverage(self, data->data.effective_offset,
                                        data->data.ref_len, nmis, indels,
                                        data->isPrimary, maxSeqLen);
        else
            return RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
    }
}

static rc_t IdVecAppend(KDataBuffer *vec, uint64_t id)
{
    uint64_t const end = vec->elem_count;
    
    BAIL_ON_FAIL(KDataBufferResize(vec, end + 1));
    ((uint64_t *)vec->base)[end] = id;
    return 0;
}

rc_t ReferenceAddAlignId(Reference *self,
                         int64_t align_id,
                         bool is_primary
                         )
{
    if (self->out_of_order)
        return 0;
    return IdVecAppend(is_primary ? &self->pri_align : &self->sec_align, align_id);
}

rc_t ReferenceWhack(Reference *self, bool commit, uint32_t maxSeqLen,
                    rc_t (*const quitting)(void)
                    )
{
    rc_t rc = 0;
    
    if (self) {
#if DUMP_CONFIG
        if (self->mgr)
            ReferenceMgr_DumpConfig(self->mgr);
#endif
        if (commit) {
            rc = FlushBuffers(self, self->length, true, true, maxSeqLen);
            if (rc != 0)
                commit = false;
        }
        KDataBufferWhack(&self->sec_align);
        KDataBufferWhack(&self->pri_align);
        KDataBufferWhack(&self->mismatches);
        KDataBufferWhack(&self->indels);
        KDataBufferWhack(&self->coverage);
        KDataBufferWhack(&self->pri_overlap);
        KDataBufferWhack(&self->sec_overlap);
        if (self->rseq)
            rc = ReferenceSeq_Release(self->rseq);
        if (self->out_of_order) {
            (void)LOGMSG(klogInfo, "Starting coverage calculation");
            rc = ReferenceMgr_Release(self->mgr, commit, NULL, true, quitting);
        }
        else {
            rc = ReferenceMgr_Release(self->mgr, commit, NULL, false, NULL);
        }
    }
    return rc;
}
