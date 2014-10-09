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
#include <klib/sort.h>
#include <kfs/file.h>
#include <insdc/insdc.h>
#include <align/dna-reverse-cmpl.h>
#include <align/align.h>

#include "defs.h"
#include "writer-algn.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

typedef struct CGWriterAlgn_match_struct {
    /* filled out by ReferenceMgr_Compress */
    INSDC_coord_zero read_start;
    INSDC_coord_len read_len;
    bool has_ref_offset[CG_READS_SPOT_LEN];
    int32_t ref_offset[CG_READS_SPOT_LEN];
    uint8_t ref_offset_type[CG_READS_SPOT_LEN];
    bool has_mismatch[CG_READS_SPOT_LEN];
    char mismatch[CG_READS_SPOT_LEN];
    int64_t ref_id;
    INSDC_coord_zero ref_start;
    /* fill oud here */
    int64_t seq_spot_id;
    INSDC_coord_one seq_read_id;
    bool ref_orientation;
    uint32_t mapq;
    /* used only only in secondary */
    bool mate_ref_orientation;
    int64_t mate_ref_id;
    INSDC_coord_zero mate_ref_pos;
    INSDC_coord_zero template_len;
    INSDC_coord_len ref_len;
} CGWriterAlgn_match;

struct CGWriterAlgn {
    const ReferenceMgr* rmgr;
    const TableWriterAlgn* primary;
    const TableWriterAlgn* secondary;
    TableWriterAlgnData algn[CG_MAPPINGS_MAX];
    CGWriterAlgn_match match[CG_MAPPINGS_MAX];
    TMappingsData data;
    uint32_t min_mapq;
    bool single_mate;
    uint64_t forced_pairs_cnt;
    uint64_t dropped_mates_cnt;
};
    

static uint32_t global_cluster_size;

rc_t CGWriterAlgn_Make(const CGWriterAlgn** cself, TMappingsData** data, VDatabase* db, const ReferenceMgr* rmgr,
                       uint32_t min_mapq, bool single_mate, uint32_t cluster_size)
{
    rc_t rc = 0;
    CGWriterAlgn* self;

    if( cself == NULL || db == NULL ) {
        return RC(rcExe, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    } else {
        if( (rc = TableWriterAlgn_Make(&self->primary, db,
                            ewalgn_tabletype_PrimaryAlignment, ewalgn_co_SEQ_SPOT_ID | ewalgn_co_unsorted)) != 0 ){
            LOGERR(klogErr, rc, "primary alignment table");
        } else if( (rc = TableWriterAlgn_Make(&self->secondary, db,
                            ewalgn_tabletype_SecondaryAlignment, ewalgn_co_SEQ_SPOT_ID | ewalgn_co_unsorted)) != 0 ) {
            LOGERR(klogErr, rc, "secondary alignment table");
        } else {
            int i;
            /* interconnect buffers */
            for(i = 0; i < CG_MAPPINGS_MAX; i++) {
                self->algn[i].seq_spot_id.buffer = &self->match[i].seq_spot_id;
                self->algn[i].seq_spot_id.elements = 1;
                
                self->algn[i].seq_read_id.buffer = &self->match[i].seq_read_id;
                self->algn[i].seq_read_id.elements = 1;

                self->algn[i].read_start.buffer = &self->match[i].read_start;
                
                self->algn[i].read_len.buffer = &self->match[i].read_len;
                
                self->algn[i].has_ref_offset.buffer = self->match[i].has_ref_offset;
                
                self->algn[i].ref_offset.buffer = self->match[i].ref_offset;
                
                self->algn[i].ref_offset_type.buffer = self->match[i].ref_offset_type;
                
                self->algn[i].ref_id.buffer = &self->match[i].ref_id;
                
                self->algn[i].ref_start.buffer = &self->match[i].ref_start;
                
                self->algn[i].has_mismatch.buffer = self->match[i].has_mismatch;
                
                self->algn[i].mismatch.buffer = self->match[i].mismatch;
                
                self->algn[i].ref_orientation.buffer = &self->match[i].ref_orientation;
                self->algn[i].ref_orientation.elements = 1;
                
                self->algn[i].mapq.buffer = &self->match[i].mapq;
                self->algn[i].mapq.elements = 1;
                
                self->algn[i].mate_ref_orientation.buffer = &self->match[i].mate_ref_orientation;
                self->algn[i].mate_ref_orientation.elements = 1;
                
                self->algn[i].mate_ref_id.buffer = &self->match[i].mate_ref_id;
                self->algn[i].mate_ref_id.elements = 1;
                
                self->algn[i].mate_ref_pos.buffer = &self->match[i].mate_ref_pos;
                self->algn[i].mate_ref_pos.elements = 1;
                
                self->algn[i].template_len.buffer = &self->match[i].template_len;
                self->algn[i].template_len.elements = 1;
            }
            self->rmgr = rmgr;
            self->min_mapq = min_mapq;
            self->single_mate = single_mate;
            global_cluster_size = cluster_size;
            *data = &self->data;
        }
    }
    if( rc == 0 ) {
        *cself = self;
    } else {
        CGWriterAlgn_Whack(self, false, NULL, NULL);
    }
    return rc;
}

rc_t CGWriterAlgn_Whack(const CGWriterAlgn* cself, bool commit, uint64_t* rows_1st, uint64_t* rows_2nd)
{
    rc_t rc = 0;
    if( cself != NULL ) {
        CGWriterAlgn* self = (CGWriterAlgn*)cself;
        rc_t rc1 = TableWriterAlgn_Whack(cself->primary, commit, rows_1st);
        rc_t rc2 = TableWriterAlgn_Whack(cself->secondary, commit, rows_2nd);
        if( self->forced_pairs_cnt > 0 ) {
            PLOGMSG(klogInfo, (klogInfo, "$(forced_pairs_cnt) forced pairs to PRIMARY", "forced_pairs_cnt=%lu", self->forced_pairs_cnt));
        }
        if( self->dropped_mates_cnt > 0 ) {
            PLOGMSG(klogInfo, (klogInfo, "$(dropped_mates_cnt) dropped duplicate mates in SECONDARY", "dropped_mates_cnt=%lu", self->dropped_mates_cnt));
        }
        rc = rc1 ? rc1 : rc2;
        free(self);
    }
    return rc;
}

static
rc_t CGWriterAlgn_Save(CGWriterAlgn *const self,
                       TReadsData *const rd,
                       TableWriterAlgn const *const writer,
                       uint32_t const mate,
                       int64_t *const rowid)
{
    rc_t rc = 0;
    TMappingsData_map *const map = &self->data.map[mate];

    if( !map->saved ) {
        CGWriterAlgn_match *const match = &self->match[mate];
        TableWriterAlgnData *const algn = &self->algn[mate];
        uint32_t* cigar, g;
        uint32_t left_cigar[] = { 5 << 4, 0, 10 << 4, 0, 10 << 4, 0, 10 << 4 };
        uint32_t right_cigar[] = { 10 << 4, 0, 10 << 4, 0, 10 << 4, 0, 5 << 4 };
        const uint32_t read_len = CG_READS_SPOT_LEN / 2;
        const char* read;

        if (match->seq_read_id == 2) {
            read = &((const char*)(rd->seq.sequence.buffer))[read_len];
            cigar = right_cigar;
            g = CG_READS_SPOT_LEN / 2;
        }
        else {
            read = rd->seq.sequence.buffer;
            cigar = left_cigar;
            g = 0;
        }
        if (match->ref_orientation) {
            if( rd->reverse[g] == '\0' ) {
                if( (rc = DNAReverseCompliment(read, &rd->reverse[g], read_len)) != 0) {
                    return rc;
                }
                DEBUG_MSG(10, ("'%.*s' -> cg_eRevDnbStrand: '%.*s'\n", read_len, read, read_len, &rd->reverse[g]));
            }
            read = &rd->reverse[g];
            cigar = (cigar == left_cigar) ? right_cigar : left_cigar;
        }
        for(g = 0; g < CG_READS_NGAPS; g++) {
            if( map->gap[g] > 0 ) {
                cigar[g * 2 + 1] = (map->gap[g] << 4) | 3; /* 'xN' */
            } else if( map->gap[g] < 0 ) {
                cigar[g * 2 + 1] = (-map->gap[g] << 4) | 9; /* 'xB' */
            } else {
                cigar[g * 2 + 1] = 0; /* '0M' */
            }
        }
        algn->ploidy = 0;
        if( (rc = ReferenceMgr_Compress(self->rmgr, ewrefmgr_cmp_Binary,
                    map->chr, map->offset, read, read_len, cigar, 7, 0, NULL, 0, 0, NULL, 0, NCBI_align_ro_complete_genomics, algn)) != 0 ) {
            PLOGERR(klogErr, (klogErr, rc, "compression failed $(id) $(o)",
                    PLOG_2(PLOG_S(id),PLOG_I32(o)), map->chr, map->offset));
        }
        else {
#if 1
            /* this is to try represent these alignments as unmated to match cgatools
             * axf uses the row length of MATE_REF_ORIENTATION as the indicator of 
             * mate presence
             */
            unsigned const save = algn->mate_ref_orientation.elements;
            
            if (map->mate == mate)
                algn->mate_ref_orientation.elements = 0;
            
            rc = TableWriterAlgn_Write(writer, algn, rowid);
            
            if (map->mate == mate)
                algn->mate_ref_orientation.elements = save;
#else
            rc = TableWriterAlgn_Write(writer, algn, rowid);
#endif
            map->saved = true;
        }
    }
    return rc;
}

#if 1
static
double JointQ(double const Q1, double const Q2)
{
    double const P1 =   1.0 - pow(10.0, Q1/-10.0);  /* prob that  1  is not incorrect */
    double const P2 =   1.0 - pow(10.0, Q2/-10.0);  /* prob that  2  is not incorrect */
    double const Pj =   1.0 - P1*P2;                /* prob that 1+2   is   incorrect */
    double const Q  = -10.0 * log10(Pj);
    
    return Q;
}

static
unsigned FindBestPair(TMappingsData *const data)
{
    unsigned const N = data->map_qty;
    unsigned i;
    double maxq;
    unsigned best = N;
    
    /* pick best of the reciprocal pairs */
    for (i = 0, maxq = -1.0; i != N; ++i) {
        unsigned const mate = data->map[i].mate;
        bool const is_left = (data->map[i].flags & cg_eRightHalfDnbMap) == 0;
        
        if (mate < N && mate != i && data->map[mate].mate == i && is_left) {
            double const Q1 = (int)data->map[i].weight - 33;
            double const Q2 = (int)data->map[mate].weight - 33;
            double const q = JointQ(Q1, Q2);

            assert(Q1 >= 0);
            assert(Q2 >= 0);
            assert(q >= 0);
            if (maxq < q) {
                maxq = q;
                best = i;
            }
        }
    }
    if (best < N)
        return best;
    
    /* no reciprocal pairs, pick best of any pair */
    for (i = 0, maxq = 0.0; i != N; ++i) {
        unsigned const mate = data->map[i].mate;
        if (mate < N && mate != i) {
            double const Q1 = (int)data->map[i].weight - 33;
            double const Q2 = (int)data->map[mate].weight - 33;
            double const q = JointQ(Q1, Q2);
            
            assert(Q1 >= 0);
            assert(Q2 >= 0);
            assert(q >= 0);
            if (maxq < q) {
                maxq = q;
                best = i;
            }
        }
    }
    if (best == N) {
        /* no pair with a joint Q > 0; pick best mapping */
        for (i = 0, maxq = 0.0; i != N; ++i) {
            unsigned const mate = data->map[i].mate;
            if (mate < N && mate != i) {
                double const q = (int)data->map[i].weight - 33;
                
                if (maxq < q) {
                    maxq = q;
                    best = i;
                }
            }
        }
        if (best == N) {
            /* no mapping with Q > 0; pick first */
            for (i = 0, maxq = 0.0; i != N; ++i) {
                unsigned const mate = data->map[i].mate;
                if (mate < N && mate != i) {
                    best = i;
                    break;
                }
            }
            if (best == N) {
                /* give up */
                return N;
            }
        }
    }
    {
        /* make the pair reciprocal */
        unsigned const mate = data->map[best].mate;
        
        if (mate < N) {
            data->map[mate].mate = best;
            return (data->map[best].flags & cg_eRightHalfDnbMap) ? mate : best;
        }
        return N;
    }
}

static
unsigned FindBestLeft(TMappingsData *const data)
{
    unsigned const N = data->map_qty;
    unsigned i;
    unsigned best;
    int maxq;
    
    for (best = N, maxq = -1, i = 0; i != N; ++i) {
        int const q = (int)data->map[i].weight;
        
        if ((data->map[i].flags & cg_eRightHalfDnbMap) == 0 && maxq < q) {
            maxq = q;
            best = i;
        }
    }
    return best;
}

static
unsigned FindBestRight(TMappingsData *const data)
{
    unsigned const N = data->map_qty;
    unsigned i;
    unsigned best;
    int maxq;
    
    for (best = N, maxq = -1, i = 0; i != N; ++i) {
        int const q = (int)data->map[i].weight;
        
        if ((data->map[i].flags & cg_eRightHalfDnbMap) != 0 && maxq < q) {
            maxq = q;
            best = i;
        }
    }
    return best;
}

static
bool check_in_cluster(TMappingsData_map const *const a, TMappingsData_map const *const b)
{
	if (   (a->flags & cg_eRightHalfDnbMap) == (b->flags & cg_eRightHalfDnbMap)
        && (strcmp(a->chr, b->chr) == 0)
        && abs((int)a->offset - (int)b->offset) <= global_cluster_size)
    {
        return true;
	}
	return false;
}

static
int clustering_sort_cb(void const *const A, void const *const B, void *const ctx)
{
    TMappingsData const *const data = (TMappingsData const *)ctx;
    unsigned const ia = *(unsigned const *)A;
    unsigned const ib = *(unsigned const *)B;
    TMappingsData_map const *const a = &data->map[ia];
    TMappingsData_map const *const b = &data->map[ib];
    int res;
    
	res = (int)(a->flags & cg_eRightHalfDnbMap) - (int)(b->flags & cg_eRightHalfDnbMap); /**** separate by DNP side ***/
	if (res) return res;
    
    res = strcmp(a->chr, b->chr); /* same chromosome ? **/
	if (res) return res;
    
	res = (a->offset - b->offset) / (global_cluster_size + 1); /***  is it within the range ***/
	if (res) return res;
    
	/**cluster is defined here; now pick the winner **/
	res = (int)a->saved - (int)b->saved; /*** if already saved **/
	if (res) return -res;
    
	res = (int)a->weight - (int)b->weight; /*** has  higher score **/
	if (res) return -res;
    
	res = (int)(a->gap[0] + a->gap[1] + a->gap[2])
    - (int)(b->gap[0] + b->gap[1] + b->gap[2]); /** has lower projection on the reference **/
    return res;
}

static
void cluster_mates(TMappingsData *const data)
{
    unsigned index[CG_MAPPINGS_MAX];
    unsigned i;
    unsigned j;
    
    for (i = 0; i != data->map_qty; ++i)
        index[i] = i;
    
    ksort(index, data->map_qty, sizeof(index[0]), clustering_sort_cb, data);
    for (i = 0, j = 1; j != data->map_qty; ++j) {
        unsigned const ii = index[i];
        unsigned const ij = index[j];
        TMappingsData_map *const a = &data->map[ij];
        TMappingsData_map const *const b = &data->map[ii];
        
        if (check_in_cluster(a, b)) {
            unsigned const a_mate = a->mate;
            unsigned const b_mate = b->mate;
            
            if (   a_mate == ij /** remove singletons **/
                || a_mate == b_mate) /** or cluster originator has the same mate **/
            {
                a->saved = true;
                DEBUG_MSG(10, ("mapping %u was dropped as a part of cluster at mapping %u\n", ij, ii));
            }
        }
        else
            i = j;
    }
}

static
INSDC_coord_zero template_length(unsigned const self_left,
                                 unsigned const mate_left,
                                 unsigned const self_len,
                                 unsigned const mate_len,
                                 unsigned const read_id)
{
    /* adapted from libs/axf/template_len.c */
    unsigned const self_right = self_left + self_len;
    unsigned const mate_right = mate_left + mate_len;
    unsigned const  leftmost  = (self_left  < mate_left ) ? self_left  : mate_left;
    unsigned const rightmost  = (self_right > mate_right) ? self_right : mate_right;
    unsigned const tlen = rightmost - leftmost;
    
    /* The standard says, "The leftmost segment has a plus sign and the rightmost has a minus sign." */
    if (   (self_left <= mate_left && self_right >= mate_right)     /* mate fully contained within self or */
        || (mate_left <= self_left && mate_right >= self_right))    /* self fully contained within mate; */
    {
        if (self_left < mate_left || (read_id == 1 && self_left == mate_left))
            return (INSDC_coord_zero)tlen;
        else
            return -(INSDC_coord_zero)tlen;
    }
    else if (   (self_right == mate_right && mate_left == leftmost) /* both are rightmost, but mate is leftmost */
             ||  self_right == rightmost)
    {
        return -(INSDC_coord_zero)tlen;
    }
    else
        return (INSDC_coord_zero)tlen;
}

static
rc_t CGWriterAlgn_Write_int(CGWriterAlgn *const self, TReadsData *const read)
{
    TMappingsData *const data = &self->data;
    unsigned const N = data->map_qty;
    rc_t rc = 0;

    if (N != 0) {
        unsigned left_prime = N;
        unsigned right_prime = N;
        unsigned i;
        unsigned countLeft  = 0;
        unsigned countRight = 0;
        
        for (i = 0; i != N; ++i) {
            char const *const refname = data->map[i].chr;
            unsigned j;
            INSDC_coord_len reflen = 35;
            ReferenceSeq const *rseq;
            bool shouldUnmap = false;
            
            memset(&self->match[i], 0, sizeof(self->match[i]));
            
            rc = ReferenceMgr_GetSeq(self->rmgr, &rseq, refname, &shouldUnmap);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "Failed accessing Reference '$(ref)'", "ref=%s", refname));
                break;
            }
            assert(shouldUnmap == false);
            rc = ReferenceSeq_Get1stRow(rseq, &self->match[i].ref_id); /* if the above worked, this is infallible */
            assert(rc == 0);
            ReferenceSeq_Release(rseq);
            
            for (j = 0; j != CG_READS_NGAPS; ++j)
                reflen += data->map[i].gap[j];
            
            self->match[i].seq_spot_id = read->rowid;
            self->match[i].mapq = data->map[i].weight - 33;
            self->match[i].ref_orientation = (data->map[i].flags & cg_eRevDnbStrand) ? true : false;
            self->match[i].ref_len = reflen;
            
            if (data->map[i].flags & cg_eRightHalfDnbMap) {
                self->match[i].seq_read_id = 2;
                ++countRight;
            }
            else {
                self->match[i].seq_read_id = 1;
                ++countLeft;
            }
        }

        if (countLeft > 0 && countRight > 0) {
            left_prime = FindBestPair(data);
            if (left_prime < N) {
                right_prime = data->map[left_prime].mate;
            }
            else { /* force the pairing */
                left_prime = FindBestLeft(data);
                right_prime = FindBestRight(data);
                data->map[left_prime].mate = right_prime;
                data->map[right_prime].mate = left_prime;
            }
            for (i = 0; i != N; ++i) {
                unsigned const mate = data->map[i].mate;
                
                if (mate < N && mate != i) {
                    INSDC_coord_zero const tlen = (self->match[i].ref_id == self->match[mate].ref_id)
                                                ? template_length(data->map[i].offset,
                                                                  data->map[mate].offset,
                                                                  self->match[i].ref_len,
                                                                  self->match[mate].ref_len,
                                                                  self->match[i].seq_read_id)
                                                : 0;

                    self->match[i].mate_ref_id = self->match[mate].ref_id;
                    self->match[i].mate_ref_orientation = self->match[mate].ref_orientation;
                    self->match[i].mate_ref_pos = data->map[mate].offset;
                    self->match[i].template_len = tlen;
                }
            }
        }
        else if (countLeft > 0) {
            left_prime = FindBestLeft(data);
        }
        else {
            assert(countRight > 0);
            right_prime = FindBestRight(data);
        }
        
        read->align_count[0] = countLeft  < 254 ? countLeft  : 255;
        read->align_count[1] = countRight < 254 ? countRight : 255;
        if (rc == 0 && left_prime < N) {
            rc = CGWriterAlgn_Save(self, read, self->primary, left_prime,
                                   &read->prim_algn_id[0]);
            read->prim_is_reverse[0] = self->match[left_prime].ref_orientation;
            data->map[left_prime].saved = 1;
        }
        if (rc == 0 && right_prime < N) {
            rc = CGWriterAlgn_Save(self, read, self->primary, right_prime,
                                   &read->prim_algn_id[1]);
            read->prim_is_reverse[1] = self->match[right_prime].ref_orientation;
            data->map[right_prime].saved = 1;
        }
        if (global_cluster_size > 0 && data->map_qty > 1 + (left_prime < N ? 1 : 0) + (right_prime < N ? 1 : 0)) {
            cluster_mates(data);
        }
        for (i = 0; i != N && rc == 0; ++i) {
            if (data->map[i].saved || self->match[i].mapq < self->min_mapq)
                continue;
            rc = CGWriterAlgn_Save(self, read, self->secondary, i, NULL);
        }
    }
    return rc;
}

rc_t CGWriterAlgn_Write(const CGWriterAlgn* cself, TReadsData* read)
{
    assert(cself != NULL);
    assert(read != NULL);
    assert(read->seq.sequence.buffer != NULL && read->seq.sequence.elements == CG_READS_SPOT_LEN);
    
    memset(read->prim_algn_id, 0, sizeof(read->prim_algn_id));
    memset(read->align_count, 0, sizeof(read->align_count));
    memset(read->prim_is_reverse, 0, sizeof(read->prim_is_reverse));
    
    return CGWriterAlgn_Write_int((CGWriterAlgn *)cself, read);
}

#else

rc_t CGWriterAlgn_Write(const CGWriterAlgn* cself, TReadsData* read)
{
    if( cself->data.map_qty != 0 ) {
        /* primary is-found indicator: weights are ASCII-33 so they can't be 0 if found */
        uint8_t left_weight = 0, right_weight = 0, pair_weight = 0;
        uint32_t i, left_prim = 0, right_prim = 0, paired = 0;

        CGWriterAlgn* self = (CGWriterAlgn*)cself;
        TMappingsData* data = &self->data;

        /* find best left, right and pair */
        for(i = 0; i < data->map_qty; i++) {
            int k = (data->map[i].flags & cg_eRightHalfDnbMap) ? 1 : 0;
            if( read->align_count[k] < 254 ) {
                read->align_count[k]++;
            }
            if( k == 0 ) {
                if( left_weight < data->map[i].weight ) {
                    left_prim = i;
                    left_weight = data->map[i].weight;
                }
            } else {
                if( right_weight < data->map[i].weight ) {
                    right_prim = i;
                    right_weight = data->map[i].weight;
                }
            }
            if( i != data->map[i].mate && pair_weight < data->map[i].weight ) {
                if( data->map[i].mate < data->map_qty ) {
                    /* note pair's left mate id */
                    paired = k == 0 ? i : data->map[i].mate;
                    pair_weight = data->map[i].weight;
                } else {
                    /* fail safe in case mate id is out of map boundaries */
                    data->map[i].mate = i;
                }
            }
        }
        /* choose primary pair */
        if( left_weight > right_weight && data->map[left_prim].mate != left_prim ) {
            /* left is better and has a mate -> choose left pair */
            right_prim = data->map[left_prim].mate;
            right_weight = data->map[right_prim].weight;
        } else if( right_weight > left_weight && data->map[right_prim].mate != right_prim ) {
            /* right is better and has a mate -> choose right pair */
            left_prim = data->map[right_prim].mate;
            left_weight = data->map[left_prim].weight;
        } else if( pair_weight > 0 ) {
            /* use paired as primary */
            left_prim = paired;
            left_weight = data->map[left_prim].weight;
            right_prim = data->map[left_prim].mate;
            right_weight = data->map[right_prim].weight;
        } else if( left_weight > 0 && right_weight > 0 ) {
            /* force best left and right to be mates */
            data->map[left_prim].mate = right_prim;
            data->map[right_prim].mate = left_prim;
            self->forced_pairs_cnt++;
            DEBUG_MSG(10, ("forced pair: %u %u\n", left_prim, right_prim));
        }
#if _DEBUGGING
        DEBUG_MSG(10, ("alignment_count [%hu,%hu]", read->align_count[0], read->align_count[1]));
        DEBUG_MSG(10, (" left primary: "));
        if( left_weight > 0 ) {
            DEBUG_MSG(10, ("weight %hu [%c], id %u", left_weight, left_weight, left_prim));
        } else {
            DEBUG_MSG(10, ("none"));
        }
        DEBUG_MSG(10, ("; right primary: "));
        if( right_weight > 0 ) {
            DEBUG_MSG(10, ("weight %hu [%c], id %u", right_weight, right_weight, right_prim));
        } else {
            DEBUG_MSG(10, ("none"));
        }
        DEBUG_MSG(10, ("\n"));
#endif
        assert((left_weight == 0 && read->align_count[0] == 0) || (left_weight > 0 && read->align_count[0] > 0));
        assert((right_weight == 0 && read->align_count[1] == 0) || (right_weight > 0 && read->align_count[1] > 0));

        /* write left primary */
        if( rc == 0 && left_weight > 0 ) {
            rc = CGWriterAlgn_Save(self, read, self->primary, left_prim, &read->prim_algn_id[0]);
            read->prim_is_reverse[0] = cself->match[left_prim].ref_orientation;
        }
        /* write right primary */
        if( rc == 0 && right_weight > 0 ) {
            rc = CGWriterAlgn_Save(self, read, self->primary, right_prim, &read->prim_algn_id[1]);
            read->prim_is_reverse[1] = cself->match[right_prim].ref_orientation;
        }
        DEBUG_MSG(10, ("prim_algn_rowid [%li,%li], ", read->prim_algn_id[0], read->prim_algn_id[1]));
        DEBUG_MSG(10, ("prim_is_reverse [%hu,%hu]\n", read->prim_is_reverse[0], read->prim_is_reverse[1]));
        if( rc == 0 ) {
            /* others go to secondary */
            int64_t row;
            
            rc = TableWriterAlgn_GetNextRowId(cself->secondary, &row);
            if( global_cluster_size > 0  && data->map_qty > 1) {
                cluster_mates(data);
            }
            if( cself->single_mate ) {
                /* we need to re-mate in case original mate's weight is lower */
                for(i = 0; rc == 0 && i < data->map_qty; i++ ) {
                    if( !data->map[i].saved && data->map[i].weight >= cself->min_mapq ) {
                        uint32_t mate = data->map[i].mate;
                        if( mate != i && data->map[mate].mate != i ) {
                            self->dropped_mates_cnt++;
                            if( data->map[data->map[mate].mate].weight < data->map[i].weight ) {
                                /* do not save my mate's mate */
                                DEBUG_MSG(10, ("mate %u dropped as pair of %u\n", data->map[mate].mate, mate));
                                data->map[data->map[mate].mate].saved = true;
                                /* repoint mate to me */
                                data->map[mate].mate = i;
                            } else {
                                /* do not save me */
                                DEBUG_MSG(10, ("mate %u dropped as pair of %u\n", i, mate));
                                data->map[i].saved = true;
                            }
                        }
                    }
                }
            }
            for(i = 0; rc == 0 && i < data->map_qty; i++ ) {
                if( !data->map[i].saved && data->map[i].weight >= cself->min_mapq ) {
                    uint32_t mate = data->map[i].mate;
                    /* no mate or mate is under-weigth */
                    if( mate == i || data->map[mate].weight < cself->min_mapq ||
                       /* or mate was saved in primary */
                       (left_weight > 0 && mate == left_prim) || (right_weight > 0 && mate == right_prim) ) {
                        self->match[i].mate_align_id = 0;
                        rc = CGWriterAlgn_Save(self, read, self->secondary, i, NULL);
                        row++;
                    } else {
                        self->match[mate].mate_align_id = row++;
                        self->match[i].mate_align_id = row++;
                        if( (rc = CGWriterAlgn_Save(self, read, self->secondary, i, NULL)) == 0 ) {
                            rc = CGWriterAlgn_Save(self, read, self->secondary, mate, NULL);
                        }
                    }
                }
            }
        }
    }
    return rc;
}
#endif
