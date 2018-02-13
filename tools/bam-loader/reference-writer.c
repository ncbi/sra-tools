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

#include <kapp/main.h> /* for Quitting */

#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <align/writer-refseq.h>

#include "reference-writer.h"
#include "Globals.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define SORTED_OPEN_TABLE_LIMIT (2)
/*#define SORTED_CACHE_SIZE ((2 * 1024 * 1024)/(SORTED_OPEN_TABLE_LIMIT)) TODO: use line below until switch to unsorted is fixed */
#define SORTED_CACHE_SIZE (350 * 1024 * 1024)

#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#define UNSORTED_OPEN_TABLE_LIMIT (8)
#define UNSORTED_CACHE_SIZE ((1024 * 1024 * 1024)/(UNSORTED_OPEN_TABLE_LIMIT))
#else
#define UNSORTED_OPEN_TABLE_LIMIT (255)
#define UNSORTED_CACHE_SIZE (350 * 1024 * 1024)
#endif

#if _DEBUGGING
#define DUMP_CONFIG 1
#endif

struct overlap_s {
    uint32_t min; /* minimum start pos of any alignment that ends in this chunk */
    uint32_t max; /* maximum end pos of any alignment that starts before this chunk and ends in this chunk */
};

struct s_reference_info {
    unsigned name;          /* offset of start of name in ref_names */
    unsigned id;
    unsigned lastOffset;
};

extern void ReferenceMgr_DumpConfig(ReferenceMgr const *const self);

rc_t ReferenceInit(Reference *self, const VDBManager *mgr, VDatabase *db)
{
    rc_t rc;
    size_t const cache = G.expectUnsorted ? UNSORTED_CACHE_SIZE : SORTED_CACHE_SIZE;
    unsigned const open_count = G.expectUnsorted ? UNSORTED_OPEN_TABLE_LIMIT : SORTED_OPEN_TABLE_LIMIT;
    
    memset(self, 0, sizeof(*self));
    
    self->coverage.elem_bits = self->mismatches.elem_bits = self->indels.elem_bits = 32;
    self->pri_align.elem_bits = self->sec_align.elem_bits = 64;
    self->pri_overlap.elem_bits = self->sec_overlap.elem_bits = sizeof(struct overlap_s) * 8;
    self->ref_names.elem_bits = 8;
    self->ref_info.elem_bits = 8 * sizeof(struct s_reference_info);
    
    rc = ReferenceMgr_Make(&self->mgr, db, mgr, ewrefmgr_co_Coverage,
                           G.refXRefPath, G.inpath,
                           G.maxSeqLen, cache, open_count);
    if (rc == 0) {
        unsigned i;
        
        for (i = 0; G.refFiles[i]; ++i) {
            rc = ReferenceMgr_FastaPath(self->mgr, G.refFiles[i]);
            if (rc) {
                (void)PLOGERR(klogWarn, (klogWarn, rc, "fasta file '$(file)'", "file=%s", G.refFiles[i]));
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
rc_t Unsorted(Reference *self) {
    if (G.requireSorted) {
        rc_t const rc = RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
        (void)LOGERR(klogWarn, rc, "Alignments are unsorted");
        return rc;
    }
    /* do not ever change this message */
    (void)LOGMSG(klogWarn, "Alignments are unsorted");

    self->out_of_order = true;
    
    ReferenceMgr_SetCache(self->mgr, UNSORTED_CACHE_SIZE, UNSORTED_OPEN_TABLE_LIMIT);
    
    KDataBufferWhack(&self->sec_align);
    KDataBufferWhack(&self->pri_align);
    KDataBufferWhack(&self->mismatches);
    KDataBufferWhack(&self->indels);
    KDataBufferWhack(&self->coverage);
    KDataBufferWhack(&self->pri_overlap);
    KDataBufferWhack(&self->sec_overlap);

    return 0;
}

#define BAIL_ON_FAIL(STMT) do { rc_t const rc__ = (STMT); if (rc__) return rc__; } while(0)

static rc_t FlushBuffers(Reference *self, unsigned upto, bool full, bool final)
{
    if (!self->out_of_order && upto > 0) {
        unsigned offset = 0;
        unsigned *const miss = (unsigned *)self->mismatches.base;
        unsigned *const indel = (unsigned *)self->indels.base;
        unsigned *const cov = (unsigned *)self->coverage.base;
        struct overlap_s *const pri_overlap = (struct overlap_s *)self->pri_overlap.base;
        struct overlap_s *const sec_overlap = (struct overlap_s *)self->sec_overlap.base;
        unsigned chunk = 0;
        
        while ((self->curPos + offset + (full ? 0 : G.maxSeqLen)) <= upto) {
            ReferenceSeqCoverage data;
            unsigned const curPos = self->curPos + offset;
            unsigned const n = self->endPos > (curPos + G.maxSeqLen) ?
                               G.maxSeqLen : (self->endPos - curPos);
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
                    return Unsorted(self);
                }
            }
            
            KDataBufferResize(&self->pri_align, 0);
            KDataBufferResize(&self->sec_align, 0);
            offset += n;
            ++chunk;
        }
        if (!final && offset > 0) {
            unsigned const newChunkCount = (unsigned)self->pri_overlap.elem_count - chunk;
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

static int str__cmp(char const A[], char const B[])
{
    unsigned i;
    
    for (i = 0; ; ++i) {
        int const a = A[i];
        int const b = B[i];
        
        if (a != b)
            return a - b;
        if (a == 0)
            return 0;
    }
}

static int str__equal(char const A[], char const B[])
{
    unsigned i;
    
    for (i = 0; ; ++i) {
        int const a = A[i];
        int const b = B[i];
        
        if (a != b)
            return 0;
        if (a == 0)
            return 1;
    }
}

static unsigned str__len(char const A[])
{
    unsigned i;
    
    for (i = 0; ; ++i) {
        int const a = A[i];
        
        if (a == 0)
            return i;
    }
}

static unsigned bsearch_name(char const qry[], char const names[],
                             unsigned const count,
                             struct s_reference_info const refInfo[],
                             int found[])
{
    unsigned f = 0;
    unsigned e = count;
    
    while (f < e) {
        unsigned const m = f + ((e - f) >> 1);
        char const *const name = &names[refInfo[m].name];
        int const diff = str__cmp(qry, name);
        
        if (diff < 0)
            e = m;
        else if (diff > 0)
            f = m + 1;
        else {
            found[0] = 1;
            return m;
        }
    }
    return f;
}

static struct s_reference_info s_reference_info_make(unsigned const name, unsigned const id)
{
    struct s_reference_info rslt;
    
    rslt.name = name;
    rslt.id = id;
    rslt.lastOffset = 0;
    
    return rslt;
}

static unsigned GetLastOffset(Reference const *const self)
{
    if (self->last_id < self->ref_info.elem_count) {
        struct s_reference_info const *const refInfoBase = self->ref_info.base;
        return refInfoBase[self->last_id].lastOffset;
    }
    return 0;
}

static void SetLastOffset(Reference *const self, unsigned const newValue)
{
    if (self->last_id < self->ref_info.elem_count) {
        struct s_reference_info *const refInfoBase = self->ref_info.base;
        refInfoBase[self->last_id].lastOffset = newValue;
    }
}

rc_t ReferenceSetFile(Reference *const self, char const id[],
                      uint64_t const length, uint8_t const md5[16],
                      bool *const shouldUnmap,
                      bool *const wasRenamed)
{
    ReferenceSeq const *rseq;
    int found = 0;
    unsigned at = 0;

    if (self->last_id < self->ref_info.elem_count) {
        struct s_reference_info const *const refInfoBase = self->ref_info.base;
        struct s_reference_info const refInfo = refInfoBase[self->last_id];
        char const *const nameBase = self->ref_names.base;
        char const *const last = nameBase + refInfo.id;
        
        if (str__equal(id, last)) {
            return 0;
        }
    }

    BAIL_ON_FAIL(FlushBuffers(self, self->length, true, true));
    BAIL_ON_FAIL(ReferenceMgr_GetSeq(self->mgr, &rseq, id, shouldUnmap, G.allowMultiMapping, wasRenamed));
    
    self->rseq = rseq;

    at = bsearch_name(id, self->ref_names.base, self->ref_info.elem_count, self->ref_info.base, &found);
    if (!found) {
        unsigned const len = str__len(id);
        unsigned const name_at = self->ref_names.elem_count;
        unsigned const id_at = name_at;
        struct s_reference_info const new_elem = s_reference_info_make(name_at, id_at);
        rc_t const rc = KDataBufferResize(&self->ref_names, name_at + len + 1);
        
        if (rc)
            return rc;
        else {
            unsigned const count = (unsigned)self->ref_info.elem_count;
            rc_t const rc = KDataBufferResize(&self->ref_info, count + 1);
            struct s_reference_info *const refInfoBase = self->ref_info.base;
            
            if (rc)
                return rc;
            
            memmove(((char *)self->ref_names.base) + name_at, id, len + 1);
            memmove(refInfoBase + at + 1, refInfoBase + at, (count - at) * sizeof(*refInfoBase));
            refInfoBase[at] = new_elem;
        }
        (void)PLOGMSG(klogInfo, (klogInfo, "Processing Reference '$(id)'", "id=%s", id));
        if (*wasRenamed) {
            char const *actid = NULL;
            ReferenceSeq_GetID(rseq, &actid);
            (void)PLOGMSG(klogInfo, (klogInfo, "Reference '$(id)' was renamed to '$(actid)'", "id=%s,actid=%s", id, actid));
        }
    }
    else if (!self->out_of_order)
        Unsorted(self);
        
    self->last_id = at;
    self->curPos = self->endPos = 0;
    self->length = (unsigned)length;
    KDataBufferResize(&self->pri_overlap, 0);
    KDataBufferResize(&self->sec_overlap, 0);

    return 0;
}

rc_t ReferenceVerify(Reference const *const self,
                     char const id[],
                     uint64_t const length,
                     uint8_t const md5[16])
{
    bool wasRenamed = false;
    return ReferenceMgr_Verify(self->mgr, id, (unsigned)length, md5, G.allowMultiMapping, &wasRenamed);
}

rc_t ReferenceGet1stRow(Reference const *self, int64_t *refID, char const refName[])
{
    return ReferenceMgr_Get1stRow(self->mgr, refID, refName);
}

static
rc_t ReferenceAddCoverage(Reference *const self,
                          unsigned const refStart,
                          unsigned const refLength,
                          uint32_t const mismatches,
                          uint32_t const indels,
                          bool const isPrimary
                          )
{
    unsigned const refEnd = refStart + refLength;

    if (refEnd > self->endPos || self->endPos == 0) {
        unsigned const t1 = refEnd + (G.maxSeqLen - 1);
        unsigned const adjust = t1 % G.maxSeqLen;
        unsigned const t2 = t1 - adjust;
        unsigned const newEndPos = t2 != 0 ? t2 : G.maxSeqLen;
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
    if ((refEnd - self->curPos) / G.maxSeqLen >= self->pri_overlap.elem_count) {
        unsigned const chunks = (refEnd - self->curPos) / G.maxSeqLen + 1;
        unsigned const end = (unsigned)self->pri_overlap.elem_count;
        
        BAIL_ON_FAIL(KDataBufferResize(&self->pri_overlap, chunks));
        BAIL_ON_FAIL(KDataBufferResize(&self->sec_overlap, chunks));
        
        memset(&((struct overlap_s *)self->pri_overlap.base)[end], 0, (chunks - end) * sizeof(struct overlap_s));
        memset(&((struct overlap_s *)self->sec_overlap.base)[end], 0, (chunks - end) * sizeof(struct overlap_s));
    }
    BAIL_ON_FAIL(FlushBuffers(self, refStart, false, false));
    if (!self->out_of_order) {
        unsigned const startBase = refStart - self->curPos;
        unsigned const endChunk = (startBase + refLength) / G.maxSeqLen;
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

rc_t ReferenceRead(Reference *self, AlignmentRecord *data, uint64_t const pos,
                   uint32_t const rawCigar[], uint32_t const cigCount,
                   char const seqDNA[], uint32_t const seqLen,
                   uint8_t rna_orient, uint32_t *matches, uint32_t *misses)
{
    unsigned nmis = 0;
    unsigned nmatch = 0;
    unsigned indels = 0;
       
    *matches = 0;
    BAIL_ON_FAIL(ReferenceSeq_Compress(self->rseq,
                                       (G.acceptHardClip ? ewrefmgr_co_AcceptHardClip : 0) + ewrefmgr_cmp_Binary,
                                       (INSDC_coord_len)pos,
                                       seqDNA, seqLen,
                                       rawCigar, cigCount,
                                       0, NULL, 0, 0, NULL, 0,
                                       rna_orient,
                                       &data->data));

    GetCounts(data, seqLen, &nmatch, &nmis, &indels);
    *matches = nmatch;
	*misses  = nmis;
/* removed before more comlete implementation - EY 
    if (!G.acceptNoMatch && data->data.ref_len == 0)
        return RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated);
***********************/
    
    if (!self->out_of_order && pos < GetLastOffset(self)) {
        return Unsorted(self);
    }
    if (!self->out_of_order) {
        SetLastOffset(self, data->data.effective_offset);
        
        /* if (G.acceptNoMatch || nmatch >= G.minMatchCount)    --- removed before more comlete implementation - EY ***/
            return ReferenceAddCoverage(self, data->data.effective_offset,
                                        data->data.ref_len, nmis, indels,
                                        data->isPrimary);
       /* else return RC(rcApp, rcFile, rcReading, rcConstraint, rcViolated); --- removed before more comlete implementation - EY ***/
    }
    return 0;
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

rc_t ReferenceWhack(Reference *self, bool commit)
{
    rc_t rc = 0;
    
    if (self) {
#if DUMP_CONFIG
        if (self->mgr)
            ReferenceMgr_DumpConfig(self->mgr);
#endif
        if (commit) {
            rc = FlushBuffers(self, self->length, true, true);
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
        KDataBufferWhack(&self->ref_names);
        KDataBufferWhack(&self->ref_info);
        if (self->rseq)
            rc = ReferenceSeq_Release(self->rseq);
        if (self->out_of_order) {
            (void)LOGMSG(klogInfo, "Starting coverage calculation");
            rc = ReferenceMgr_Release(self->mgr, commit, NULL, true, Quitting);
        }
        else {
            rc = ReferenceMgr_Release(self->mgr, commit, NULL, false, Quitting);
        }
    }
    return rc;
}
