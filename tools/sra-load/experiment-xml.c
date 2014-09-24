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
#include <klib/debug.h>
#include <klib/container.h>
#include <klib/trie.h>
#include <sra/types.h>
#include <os-native.h>

#include "experiment-xml.h"
#include "debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define MAX_POOL_MEMBER_BASECALLS 64
#define MAX_SPOT_DESCRIPTOR_READS (SRALOAD_MAX_READS + 2)

typedef struct PoolReadSpec_struct {
    int16_t start;
    int16_t length;
    /* chosen basecalls for EXPECTED_BASECALL_TABLE for defined member_name, NULL-terminated array */
    const ReadSpecXML_read_BASECALL* item[MAX_POOL_MEMBER_BASECALLS];
    int32_t score;
} PoolReadSpec;

typedef struct PoolMember_struct {
    BSTNode node;
    const char *name;
    PoolReadSpec spec[MAX_SPOT_DESCRIPTOR_READS];
} PoolMember;

struct ExperimentXML {
    uint32_t spot_length;
    const PlatformXML* platform;
    ProcessingXML processing;
    const ReadSpecXML* reads;
    BSTree* member_pool;
    PoolMember* member_null;
    SLList member_stats;
};

static
void CC PoolMember_Whack(BSTNode* node, void* data)
{
    if( node != NULL ) {
        const PoolMember* n = (const PoolMember*)node;
        free((void*)n->name);
        free(node);
    }
}

static
int CC PoolMember_StrCmp(const char* s1, const char* s2)
{
    if( s1 == NULL && s2 == NULL ) {
        return 0;
    } else if( s1 == NULL ) {
        return -32767;
    } else if( s2 == NULL ) {
        return 32768;
    }
    return strcmp(s1, s2);
}

static
int CC PoolMember_Cmp(const BSTNode* item, const BSTNode* node)
{
    const PoolMember* i = (const PoolMember*)item;
    const PoolMember* n = (const PoolMember*)node;

    return PoolMember_StrCmp(i->name, n->name);
}

static
int CC PoolMember_FindByName(const void* item, const BSTNode* node)
{
    const char* name = (const char*)item;
    const PoolMember* n = (const PoolMember*)node;

    return PoolMember_StrCmp(name, n->name);
}

#if _DEBUGGING
static
void CC PoolMember_Dump( BSTNode *node, void *data )
{
    const PoolMember* n = (const PoolMember*)node;
    const ExperimentXML* self = (const ExperimentXML*)data;
    int r;

    DEBUG_MSG(9, ("Member '%s'\n", n->name));

    for(r = 0; r <= self->reads->nreads + 1; r++) {
        DEBUG_MSG(9, ("Read %d [%hd:%hd]", r + 1, n->spec[r].start, n->spec[r].length));
        if( self->reads->spec[r].coord_type == rdsp_ExpectedBaseCall_ct ) {
            DEBUG_MSG(9, (" ebc:'%s'", n->spec[r].item[0]->basecall));
        } else if( self->reads->spec[r].coord_type == rdsp_ExpectedBaseCallTable_ct ) {
            int i = -1;
            while( n->spec[r].item[++i] != NULL ) {
                DEBUG_MSG(9, (" bc:'%s', tag:'%s';", n->spec[r].item[i]->basecall, n->spec[r].item[i]->read_group_tag));
            }
        }
        DEBUG_MSG(9, ("\n"));
    }
}
#endif

static
rc_t PoolMember_Add(ExperimentXML* self, const char* name)
{
    rc_t rc = 0;
    PoolMember* member = NULL;

    if( self == NULL) {
        return RC(rcSRA, rcFormatter, rcAllocating, rcSelf, rcNull);
    }
    member = calloc(1, sizeof(*member));
    if( member == NULL ) {
        return RC(rcSRA, rcFormatter, rcAllocating, rcMemory, rcExhausted);
    }
    if( name == NULL ) {
        if( self->member_null == NULL ) {
            self->member_null = member;
        } else {
            rc = RC(rcSRA, rcFormatter, rcAllocating, rcId, rcDuplicate);
        }
    } else {
        member->name = strdup(name);
        if( member->name == NULL ) {
            rc = RC(rcSRA, rcFormatter, rcAllocating, rcMemory, rcExhausted);
        } else {
            if( self->member_pool == NULL ) {
                self->member_pool = calloc(1, sizeof(BSTree));
                if( self->member_pool == NULL ) {
                    rc = RC(rcSRA, rcFormatter, rcAllocating, rcMemory, rcExhausted);
                } else {
                    BSTreeInit(self->member_pool);
                }
            }
            if( rc == 0 ) {
                rc = BSTreeInsertUnique(self->member_pool, &member->node, NULL, PoolMember_Cmp);
            }
        }
    }
    if( rc == 0 ) {
        uint32_t i;
        for(i = 0; i <= self->reads->nreads + 1; i++) {
            switch(self->reads->spec[i].coord_type) {
                case rdsp_FIXED_BRACKET_ct:
                    member->spec[i].start = self->reads->spec[i].coord.start_coord - 1;
                    member->spec[i].length = (i == 0 ? 0 : -1);
                    break;
                case rdsp_RelativeOrder_ct:
                    member->spec[i].start = -1;
                    member->spec[i].length = -1;
                    break;

                case rdsp_BaseCoord_ct:
                case rdsp_CycleCoord_ct:
                    member->spec[i].start = self->reads->spec[i].coord.start_coord - 1;
                    member->spec[i].length = -1;
                    break;

                case rdsp_ExpectedBaseCall_ct:
                    if( name == NULL || name[0] == '\0' ) {
                        member->spec[i].start = self->reads->spec[i].coord.expected_basecalls.base_coord;
                        member->spec[i].length = self->reads->spec[i].coord.expected_basecalls.default_length;
                    }
                    if( member->spec[i].start == 0 ) {
                        member->spec[i].start = -1;
                    }
                    member->spec[i].item[0] = self->reads->spec[i].coord.expected_basecalls.table;
                    if( member->spec[i].length == 0 ) {
                        member->spec[i].length = strlen(member->spec[i].item[0]->basecall);
                    }
                    break;

                case rdsp_ExpectedBaseCallTable_ct:
                    if( name == NULL || name[0] == '\0' ) {
                        member->spec[i].start = self->reads->spec[i].coord.expected_basecalls.base_coord;
                        member->spec[i].length = self->reads->spec[i].coord.expected_basecalls.default_length;
                    }
                    if( member->spec[i].start == 0 ) {
                        member->spec[i].start = -1;
                    }
                    if( member->spec[i].length == 0 ) {
                        member->spec[i].length = (!self->reads->spec[i].coord.expected_basecalls.pooled || name == NULL) ? -1 : 0;
                    }
                    break;
            }
        }
    } else {
        PoolMember_Whack(&member->node, NULL);
    }
    return rc;
}

static
rc_t PoolMember_AddBasecalls(const ExperimentXML* self, const char* member, int readid, const ReadSpecXML_read_BASECALL* basecalls[])
{
    rc_t rc = 0;

    if( self == NULL || basecalls == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcParam, rcNull);
    } else if( self->reads->reads[readid].coord_type != rdsp_ExpectedBaseCallTable_ct ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcId, rcIncorrect);
    } else {
        PoolMember* pm = (PoolMember*)BSTreeFind(self->member_pool, member, PoolMember_FindByName);
        if( pm == NULL ) {
            rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcNotFound);
            PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)'", PLOG_S(m), member));
        } else {
            int k = -1, i = -1;
            while( pm->spec[readid + 1].item[++k] != NULL );
            while( rc == 0 && basecalls[++i] != NULL ) {
                if( k >= MAX_POOL_MEMBER_BASECALLS ) {
                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcTooLong);
                    PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' basecalls", PLOG_S(m), member));
                } else {
                    pm->spec[readid + 1].item[k++] = basecalls[i];
                }
            }
        }
    }
    return rc;
}

rc_t parse_POOL(const KXMLNode* EXPERIMENT, ExperimentXML* obj)
{
    rc_t rc = 0;

    /* create unspecified member -> NULL */
    if( (rc = PoolMember_Add(obj, NULL)) == 0 ) {
        const KXMLNodeset* MEMBERS;
        if( (rc = KXMLNodeOpenNodesetRead(EXPERIMENT, &MEMBERS, "DESIGN/SAMPLE_DESCRIPTOR/POOL/MEMBER")) == 0 ) {
            uint32_t i, num_members = 0;
            if( (rc = KXMLNodesetCount(MEMBERS, &num_members)) == 0 && num_members > 0 ) {
                /* create default member -> '' */
                rc = PoolMember_Add(obj, "");
                for(i = 0; rc == 0 && i < num_members; i++) {
                    const KXMLNode* MEMBER;
                    if( (rc = KXMLNodesetGetNodeRead(MEMBERS, &MEMBER, i)) == 0 ) {
                        char* member_name = NULL;
                        if( (rc = KXMLNodeReadAttrCStr(MEMBER, "member_name", &member_name, NULL)) == 0 ) {
                            const KXMLNodeset* LABELS;
                            if( strcasecmp(member_name, "default") == 0 ) {
                                member_name[0] = '\0';
                            }
                            if( member_name[0] != '\0' ) {
                                if( (rc = PoolMember_Add(obj, member_name)) != 0 ) {
                                    PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)'", PLOG_S(m), member_name));
                                }
                            }
                            if( rc == 0 && (rc = KXMLNodeOpenNodesetRead(MEMBER, &LABELS, "READ_LABEL")) == 0 ) {
                                uint32_t l, num_labels = 0;
                                KXMLNodesetCount(LABELS, &num_labels);
                                if( num_labels > 0 ) {
                                    const ReadSpecXML_read_BASECALL* basecalls[MAX_POOL_MEMBER_BASECALLS];
                                    for(l = 0; rc == 0 && l < num_labels; l++) {
                                        const KXMLNode* LABEL;
                                        if( (rc = KXMLNodesetGetNodeRead(LABELS, &LABEL, l)) == 0 ) {
                                            char* label = NULL, *tag = NULL;
                                            if( (rc = KXMLNodeReadCStr(LABEL, &label, NULL)) != 0 || label == NULL || label[0] == '\0' ) {
                                                rc = rc ? rc : RC(rcSRA, rcFormatter, rcConstructing, rcData, rcEmpty);
                                                PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' READ_LABEL value", PLOG_S(m), member_name));
                                            } else {
                                                if( (rc = KXMLNodeReadAttrCStr(LABEL, "read_group_tag", &tag, NULL)) != 0 ) {
                                                    if( GetRCState(rc) != rcNotFound ) {
                                                        PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' READ_LABEL '$(l)' 'read_group_tag' attribute",
                                                                                        PLOG_2(PLOG_S(m),PLOG_S(l)), member_name, label));
                                                    } else {
                                                        rc = 0;
                                                    }
                                                } else if( tag != NULL && tag[0] == '\0' )  {
                                                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcEmpty);
                                                    PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' READ_LABEL '$(l)' 'read_group_tag' attribute",
                                                                                   PLOG_2(PLOG_S(m),PLOG_S(l)), member_name, label));
                                                }
                                                if( rc == 0 ) {
                                                    /* find read_spec based on label and update basecall table */
                                                    uint32_t r, read_found = 0;
                                                    for(r = 0; rc == 0 && r < obj->reads->nreads; r++) {
                                                        if( obj->reads->reads[r].read_label != NULL && strcmp(label, obj->reads->reads[r].read_label) == 0 ) {
                                                            read_found++;
                                                            if( tag != NULL && tag[0] != '\0' ) {
                                                                if( obj->reads->reads[r].coord_type != rdsp_ExpectedBaseCallTable_ct ) {
                                                                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInconsistent);
                                                                    PLOGERR(klogErr, (klogErr, rc,
                                                                            "MEMBER '$(m)' READ_LABEL '$(l)' READ_SPEC must have EXPECTED_BASECALL_TABLE type",
                                                                            PLOG_2(PLOG_S(m),PLOG_S(l)), member_name, label));
                                                                } else {
                                                                    uint32_t b, bcall_found = 0;
                                                                    memset(basecalls, 0, sizeof(basecalls));
                                                                    /* find tag in table */
                                                                    for(b = 0; rc == 0 && b < obj->reads->reads[r].coord.expected_basecalls.count; b++) {
                                                                        if( obj->reads->reads[r].coord.expected_basecalls.table[b].read_group_tag != NULL &&
                                                                            strcmp(tag, obj->reads->reads[r].coord.expected_basecalls.table[b].read_group_tag) == 0 ) {
                                                                            if( bcall_found >= MAX_POOL_MEMBER_BASECALLS ) {
                                                                                rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcExcessive);
                                                                                PLOGERR(klogErr, (klogErr, rc,
                                                                                        "MEMBER '$(m)' READ_LABEL '$(l)' read_group_tag='$(t)' too many basecalls",
                                                                                         PLOG_3(PLOG_S(m),PLOG_S(l),PLOG_S(t)), member_name, label, tag));
                                                                            }
                                                                            basecalls[bcall_found++] = &obj->reads->reads[r].coord.expected_basecalls.table[b];
                                                                        }
                                                                    }
                                                                    if( rc == 0 && bcall_found == 0 ) {
                                                                        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcNotFound);
                                                                        PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' READ_LABEL '$(l)' read_group_tag='$(t)' in READ_SPEC",
                                                                                 PLOG_3(PLOG_S(m),PLOG_S(l),PLOG_S(t)), member_name, label, tag));
                                                                    } else {
                                                                        rc = PoolMember_AddBasecalls(obj, member_name, r, basecalls);
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                    if( read_found == 0 ) {
                                                        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcNotFound);
                                                        PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' READ_LABEL '$(l)' in READ_SPEC", PLOG_2(PLOG_S(l),PLOG_S(m)), label, member_name));
                                                    } else if( read_found > 1 ) {
                                                        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInconsistent);
                                                        PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' READ_LABEL '$(l)' in READ_SPEC", PLOG_2(PLOG_S(l),PLOG_S(m)), label, member_name));
                                                    }
                                                }
                                            }
                                            free(tag);
                                            free(label);
                                            KXMLNodeRelease(LABEL);
                                        } else {
                                            PLOGERR(klogErr, (klogErr, rc, "MEMBER '$(m)' READ_LABEL #$(i)", PLOG_2(PLOG_S(m),PLOG_U32(i)), member_name, l));
                                        }
                                    }
                                } else {
                                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInvalid);
                                    PLOGERR(klogErr, (klogErr, rc, "missing READ_LABEL element(s) in MEMBER '$(m)'", PLOG_S(m), member_name));
                                }
                                KXMLNodesetRelease(LABELS);
                            }
                            free(member_name);
                        } else {
                            PLOGERR(klogErr, (klogErr, rc, "'member_name' in MEMBER node #$(i)", PLOG_U32(i), i));
                        }
                        KXMLNodeRelease(MEMBER);
                    } else {
                        PLOGERR(klogErr, (klogErr, rc, "MEMBER node #$(i)", PLOG_U32(i), i));
                    }
                }
            } else if( rc != 0 ) {
                LOGERR(klogErr, rc, "POOL/MEMBER(s)");
            }
            KXMLNodesetRelease(MEMBERS);
        } else {
            LOGERR(klogErr, rc, "POOL/MEMBER(s)");
        }
    }
#if _DEBUGGING
    PoolMember_Dump(&obj->member_null->node, obj);
    BSTreeForEach(obj->member_pool, false, PoolMember_Dump, obj);
#endif
    return rc;
}

typedef struct ExpectedTableMatch_struct {
    /* in */
    const char* seq;
    uint32_t len;
    const char* tag;
    enum {
        etm_Forward = 1,
        etm_Reverse,
        etm_Both
    } direction;
    bool left_adjusted;
    /* out */
    AgrepMatch match;
    ReadSpecXML_read_BASECALL* bc;
} ExpectedTableMatch;

static
bool Experiment_ExpectedTableMatch(const ReadSpecXML_read_BASECALL_TABLE* table, ExpectedTableMatch* data)
{
    uint32_t i;

    assert(table && data && data->seq );

    data->match.score = -1;
    for(i = 0; i < table->count; i++) {
        AgrepMatch match;
        ReadSpecXML_read_BASECALL* bc = &table->table[i];

        if( data->tag != NULL && strcmp(data->tag, bc->read_group_tag) != 0 ) {
            continue;
        }
        if( data->direction != etm_Both ) {
            if( bc->match_edge == match_edge_End ) {
                if( data->direction == etm_Forward ) {
                    continue;
                }
            } else if( data->direction != etm_Forward ) {
                continue;
            }
        }
        match.score = -1;
        if( data->left_adjusted ) {
            if( AgrepFindFirst(bc->agrep, bc->max_mismatch, data->seq, data->len, &match) ) {
                if( data->direction == etm_Reverse ) {
                    int32_t right_tail = data->len - match.position - strlen(bc->basecall);
                    if( right_tail > bc->max_mismatch - match.score ) {
                        match.score = -1;
                    } else if( match.position > 0 ) {
                        match.score += right_tail;
                        match.length += right_tail;
                        match.position = 0;
                    }
                } else {
                    if( match.position > bc->max_mismatch - match.score ) {
                        match.score = -1;
                    } else if( match.position > 0 ) {
                        match.score += match.position;
                        match.length += match.position;
                        match.position = 0;
                    }
                }
            }
        } else {
            AgrepFindBest(bc->agrep, bc->max_mismatch, data->seq, data->len, &match);
        }
        if( !(match.score < 0) ) {
            DEBUG_MSG(3, ("agrep match bc %s, min: %u, max: %u, edge: %s%s, [%d:%d] score %d\n", bc->basecall, bc->min_match, bc->max_mismatch,
                       bc->match_edge == match_edge_Full ? "Full" : (bc->match_edge == match_edge_Start ? "Start" : "End"),
                       data->left_adjusted ? " left adjusted" : "",
                       match.position, match.length, match.score));
            if( (data->match.score < 0 || data->match.score > match.score) ) {
                memcpy(&data->match, &match, sizeof(data->match));
                data->bc = bc;
            }
        }
    }
    return !(data->match.score < 0);
}

static
rc_t Experiment_FillRead(const ReadSpecXML_read* spec, PoolReadSpec* pool,
                         const char** tag, const uint32_t nreads, int16_t start, const char* bases, uint32_t nbases)
{
    rc_t rc = 0;

    switch(spec->coord_type) {
        case rdsp_RelativeOrder_ct:
            if( pool[-1].length >= 0 ) {
                if( pool[-1].start >= 0 ) {
                    pool->start = pool[-1].start + pool[-1].length;
                } else {
                    pool->start = start;
                }
            } else {
                pool->start = -1;
            }
            pool->length = -1;
            if( nreads > 1 ) {
                if( (rc = Experiment_FillRead(spec + 1, pool + 1, tag + 1, nreads - 1, start, bases, nbases)) != 0 ) {
                    break;
                }
            }
            if( pool->start < 0 ) {
                pool->start = pool[1].start;
            }
            pool->length = pool[1].start - pool->start;
            break;

        case rdsp_BaseCoord_ct:
        case rdsp_CycleCoord_ct:
            pool->start = spec->coord.start_coord - 1;
            pool->length = -1;
            if( nreads > 1 ) {
                if( (rc = Experiment_FillRead(spec + 1, pool + 1, tag + 1, nreads - 1, pool->start, bases, nbases)) != 0 ) {
                    break;
                }
            }
            pool->length = pool[1].start - pool->start;
            break;

        case rdsp_ExpectedBaseCall_ct:
        case rdsp_ExpectedBaseCallTable_ct:
            {{
            int32_t gap;
            if( spec->coord.expected_basecalls.base_coord > 0 ) {
                pool->start = spec->coord.expected_basecalls.base_coord - 1;
                start = pool->start;
            } else {
                pool->start = -1;
            }
            pool->length = -1;
            pool->item[0] = NULL;
            pool->score = -1;
            if( spec->coord.expected_basecalls.match_start > 0 ) {
                uint32_t i, stop = nbases;
                ExpectedTableMatch data;
                /* find terminator, if any */
                for(i = 1; i < nreads; i++) {
                    if( spec[i].coord_type == rdsp_BaseCoord_ct ||
                        spec[i].coord_type == rdsp_CycleCoord_ct ||
                        spec[i].coord_type == rdsp_FIXED_BRACKET_ct ) {
                        stop = spec[i].coord.start_coord - 1;
                        break;
                    }
                }
                data.seq = &bases[start];
                data.len = stop - start;
                data.tag = *tag;
                data.direction = etm_Forward;
                data.left_adjusted = !(pool->start < 0) || !(pool[-1].length < 0);
                DEBUG_MSG(3, ("agrep find read %u fwd in: '%.*s' = %u\n", nreads, data.len, data.seq, data.len));
                if( Experiment_ExpectedTableMatch(&spec->coord.expected_basecalls, &data) ) {
                    DEBUG_MSG(3, ("agrep found from %hd match %d:%d - %d '%s' in '%.*s...' \n",
                                   start, data.match.position, data.match.length,
                                   data.match.score, data.bc->basecall,
                                   start + data.match.position + data.match.length * 3 + data.match.score, &bases[start]));
                    pool->start = start + data.match.position;
                    pool->length = data.match.length;
                    pool->item[0] = data.bc;
                    pool->score = data.match.score;
                    start = pool->start + pool->length;
                } else {
                    DEBUG_MSG(3, ("agrep nothing found\n"));
                }
            }
            if( pool[-1].length >= 0 && pool->length < 0 && spec->coord.expected_basecalls.match_end == 0 ) {
                pool->length = spec->coord.expected_basecalls.default_length;
                start += pool->length;
            }
            if( nreads > 1 ) {
                if( (rc = Experiment_FillRead(spec + 1, pool + 1, tag + 1, nreads - 1, start, bases, nbases)) != 0 ) {
                    break;
                }
            }
            if( spec->coord.expected_basecalls.match_end > 0 ) {
                /* match_end can have a better match even if match_start found smth before
                   normally table has start or end match edge, not both */
                char r[4096];
                uint32_t i;
                ExpectedTableMatch data;
                
                data.len = pool[1].start - start;
                DEBUG_MSG(3, ("agrep find read %u rev in: '%.*s' = %u\n", nreads, data.len, &bases[start], data.len));
                if( data.len > sizeof(r) ) {
                    rc = RC(rcSRA, rcFormatter, rcResolving, rcBuffer, rcInsufficient);
                    break;
                }
                for(i = 0; i < data.len; i++) {
                    /* reverse a portion of seq */
                    r[i] = bases[pool[1].start - i - 1];
                }
                data.seq = r;
                data.tag = *tag;
                data.direction = etm_Reverse;
                data.left_adjusted = spec[1].coord_type != rdsp_RelativeOrder_ct && pool[1].start > 0;
                DEBUG_MSG(3, ("agrep find read %u rev in: '%.*s' = %u\n", nreads, data.len, data.seq, data.len));
                if( Experiment_ExpectedTableMatch(&spec->coord.expected_basecalls, &data) ) {
                    DEBUG_MSG(3, ("agrep found rev from %hd match %d:%d - %d '%s' in '%.*s' \n",
                                    start, data.match.position, data.match.length, data.match.score,
                                    data.bc->basecall, data.len, &bases[pool[1].start - data.len]));
                    if( pool->length < 0 || data.match.score < pool->score ) {
                        pool->start = pool[1].start - data.match.position - data.match.length;
                        pool->length = data.match.length;
                        pool->item[0] = data.bc;
                        pool->score = data.match.score;
                    }
                } else {
                    DEBUG_MSG(3, ("agrep nothing found rev\n"));
                }
            }
            if( pool->length < 0 ) {
                pool->length = spec->coord.expected_basecalls.default_length;
            }
            if( pool->start < 0 ) {
                pool->start = pool[1].start - pool->length;
            }
            gap = pool[1].start - (pool->start + pool->length);
            if( gap > 0 ) {
                /* right gap */
                if( spec[1].coord_type == rdsp_RelativeOrder_ct ) {
                    /* adjust right read's start to the tail of current read */
                    pool[1].start -= gap;
                    pool[1].length += gap;
                    gap = 0;
                } else if( spec[1].coord_type >= rdsp_ExpectedBaseCall_ct &&
                    spec[1].coord.expected_basecalls.base_coord <= 0 && pool[1].item[0] != NULL ) {
                    /* move left read's start to left as much as possible */
                    int32_t left = pool[1].item[0]->max_mismatch - pool[1].score;
                    if( left > 0 ) {
                        if( left > gap ) {
                            left = gap;
                        }
                        pool[1].length += left;
                        pool[1].score += left;
                        pool[1].start -= left;
                        gap -= left;
                    }
                }
                if( gap > 0 && pool->item[0] != NULL ) {
                    /* add gap to end of self */
                    int32_t left = pool->item[0]->max_mismatch - pool->score;
                    if( left > 0 ) {
                        if( left > gap ) {
                            left = gap;
                        }
                        pool->length += left;
                        pool->score += left;
                    }
                }
            }
            break;
            }}
        default:
            rc = RC(rcSRA, rcFormatter, rcResolving, rcItem, rcUnexpected);
            break;
    }
    return rc;
}

typedef struct PoolMember_FindReverse_Data_struct {
    const char* tags[MAX_SPOT_DESCRIPTOR_READS];
    uint8_t nreads;
    const PoolMember* member;
} PoolMember_FindReverse_Data;

static
bool CC PoolMember_FindReverse(BSTNode* n, void* d)
{
    const PoolMember* node = (const PoolMember*)n;
    PoolMember_FindReverse_Data* data = (PoolMember_FindReverse_Data*)d;
    int r, j, found;

    for(r = 1; r <= data->nreads; r++) {
        /* comparing pointers here because tags contains same pointers */
        if( node->spec[r].item[0] == NULL && data->tags[r] == NULL ) {
            continue;
        }
        found = 0;
        for(j = 0; node->spec[r].item[j] != NULL; j++ ) {
            if( node->spec[r].item[j]->read_group_tag == data->tags[r] ) {
                found = 1;
                break;
            }
        }
        if( found == 0 ) {
            return false;
        }
    }
    data->member = node;
    return true;
}

static
rc_t Experiment_FillSegAgrep(const ExperimentXML* self, PoolReadSpec* pool, const PoolMember* src,
                             uint32_t nbases, const char* bases,
                             const char** const new_member_name)
{
    rc_t rc = 0;
    uint32_t i;
    PoolMember_FindReverse_Data data;

    /* close left and right brackets tech reads as spot terminators */
    pool[0].start = 0;
    pool[0].length = 0;
    pool[self->reads->nreads + 1].start = nbases;
    pool[self->reads->nreads + 1].length = 0;

    /* setup known tags if it is predefined member search */
    for(i = 0; i <= self->reads->nreads + 1; i++) {
        data.tags[i] = src->spec[i].item[0] ? src->spec[i].item[0]->read_group_tag : NULL;
    }
    /* start from read, not bracket read */
    rc = Experiment_FillRead(self->reads->reads, &pool[1], &data.tags[1], self->reads->nreads, 0, bases, nbases);

    if( rc != 0 ) {
        DEBUG_MSG(3, ("ERROR: %R\n", rc));
    }
    DEBUG_MSG(3, ("%s: bases %u: '%.*s'\n", new_member_name ? *new_member_name : NULL, nbases, nbases, bases));
    for(i = 1; i <= self->reads->nreads; i++) {
        const char* b = NULL;
        if( pool[i].start >= 0 ) {
            b = &bases[pool[i].start];
        }
        DEBUG_MSG(3, ("Read %u. [%hd:%hd] - '%.*s'",
                       i, pool[i].start, pool[i].length, pool[i].length < 0 ? 0 : pool[i].length, b));
        if( self->reads->spec[i].coord_type == rdsp_FIXED_BRACKET_ct ) {
            DEBUG_MSG(3, (" terminator???"));
        }
        if( self->reads->spec[i].coord_type >= rdsp_ExpectedBaseCall_ct ) {
            if( pool[i].item[0] ) {
                if( pool[i].item[0]->match_edge == match_edge_End ) {
                    DEBUG_MSG(3, (" in reverse"));
                }
                DEBUG_MSG(3, (" matches %s", pool[i].item[0]->basecall));
                if( pool[i].item[0]->read_group_tag ) {
                    DEBUG_MSG(3, (" tagged %s", pool[i].item[0]->read_group_tag));
                }
                DEBUG_MSG(3, (" with %d mismatches", pool[i].score));
            } else {
                DEBUG_MSG(3, (" no match"));
            }
        }
        DEBUG_MSG(3, ("\n"));
    }

    if( rc == 0 && new_member_name != NULL ) {
        data.nreads = self->reads->nreads;
        data.member = NULL;
        for(i = 1; i <= self->reads->nreads; i++) {
            if( self->reads->spec[i].coord_type == rdsp_ExpectedBaseCallTable_ct && pool[i].item[0] != NULL ) {
                data.tags[i] = pool[i].item[0]->read_group_tag;
            } else {
                data.tags[i] = NULL;
            }
        }
        if( BSTreeDoUntil(self->member_pool, false, PoolMember_FindReverse, &data) ) {
            *new_member_name = data.member->name;
            DEBUG_MSG(2, ("Assigned member_name '%s'\n", *new_member_name));
        } else {
#if _DEBUGGING
            DEBUG_MSG(2, ("member_name reverse lookup failed for tags:"));
            for(i = 1; i <= self->reads->nreads; i++) {
                DEBUG_MSG(2, (" '%s'", data.tags[i]));
            }
            DEBUG_MSG(2, ("\n"));
#endif
        }
    }
    return rc;
}

rc_t Experiment_MemberSeg(const ExperimentXML* self,
                          const char* const file_member_name, const char* const data_block_member_name,
                          uint32_t nbases, const char* bases,
                          SRASegment* seg, const char** const new_member_name)
{
    rc_t rc = 0;
    uint32_t i;
    PoolReadSpec pool[MAX_SPOT_DESCRIPTOR_READS];

    if( self == NULL || bases == NULL || seg == NULL || new_member_name == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcResolving, rcParam, rcNull);
    } else if( nbases < 1 ) {
        rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcEmpty);
    }

    if( rc == 0 ) {
        const char* mm = NULL;
        switch(self->processing.barcode_rule) {
            case eBarcodeRule_not_set:
                mm = data_block_member_name ? data_block_member_name : file_member_name;
                break;

            case eBarcodeRule_use_file_spot_name:
                mm = file_member_name;
                break;

            case eBarcodeRule_use_table_in_experiment:
            case eBarcodeRule_ignore_barcode:
                mm = NULL;
                break;
        }
        if( self->member_pool == NULL || self->processing.barcode_rule == eBarcodeRule_ignore_barcode ) {
            if( (rc = Experiment_FillSegAgrep(self, pool, self->member_null, nbases, bases, NULL)) == 0 ) {
                *new_member_name = mm;
            }
        } else if( mm == NULL ) {
            *new_member_name = NULL;
	    rc = Experiment_FillSegAgrep(self, pool, self->member_null, nbases, bases, new_member_name);
        } else {
            PoolMember* pm = (PoolMember*)BSTreeFind(self->member_pool, mm, PoolMember_FindByName);
            if( pm == NULL ) {
                rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcNotFound);
                PLOGERR(klogErr, (klogErr, rc, "specified member_name '$(m)'", PLOG_S(m), mm));
            } else {
                if( (rc = Experiment_FillSegAgrep(self, pool, pm, nbases, bases, NULL)) == 0 ) {
                    *new_member_name = mm;
                }
            }
        }
    }

    if( rc == 0 ) {
        int32_t new_bases = 0;

        for(i = 1; i <= self->reads->nreads; i++) {
            if( pool[i].start < 0 || pool[i].length < 0 ) {
                rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcIncomplete);
                PLOGERR(klogErr, (klogErr, rc, "Read #$(i) boundaries", PLOG_U32(i), i));
            } else if( i == 1 ) {
                if( pool[i].start != 0 ) {
                    rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcInvalid);
                    PLOGERR(klogErr, (klogErr, rc, "Read #$(i) do not start at 0", PLOG_U32(i), i));
                }
            } else {
                int16_t delta = pool[i].start - pool[i - 1].length - pool[i - 1].start;
                /* try to close gap by failng back to default length IF:
                   prev read was EXPECTED_BASECALL OR
                                 EXPECTED_BASECALL_TABLE and there an BASECALL chosen
                   AND
                    defailt_length != 0 (was set)
                   */
                if( delta != 0 &&
                    (self->reads->spec[i - 1].coord_type == rdsp_ExpectedBaseCall_ct ||
                     (self->reads->spec[i - 1].coord_type == rdsp_ExpectedBaseCallTable_ct && pool[i - 1].item[0] != NULL)) &&
                    self->reads->spec[i - 1].coord.expected_basecalls.default_length > 0 ) {
                    new_bases -= pool[i - 1].length;
                    pool[i - 1].length = self->reads->spec[i - 1].coord.expected_basecalls.default_length;
                    DEBUG_MSG(2, ("trying to close delta %hi for read %u: length reset default %hi\n", delta, i - 1, pool[i - 1].length));
                    if( pool[i - 1].item[0] != NULL  && pool[i - 1].item[0]->read_group_tag != NULL ) {
                        DEBUG_MSG(2, ("member_name '%s' reset to ''\n", *new_member_name));
                        *new_member_name = NULL;
                    }
                    delta = pool[i].start - pool[i - 1].length - pool[i - 1].start;
                    new_bases += pool[i - 1].length;
                    seg[i - 2].len = pool[i - 1].length;
                }
                if( delta > 0 ) {
                    rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcInconsistent);
                    PLOGERR(klogErr, (klogErr, rc, "Gap between reads #$(p) and #$(i)", PLOG_2(PLOG_U32(p),PLOG_U32(i)), i - 1, i));
                } else if( delta < 0 ) {
                    rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcInconsistent);
                    PLOGERR(klogErr, (klogErr, rc, "Reads #$(p) and #$(i) overlap", PLOG_2(PLOG_U32(p),PLOG_U32(i)), i - 1, i));
                }
            }
            seg[i - 1].start = pool[i].start;
            seg[i - 1].len = pool[i].length;
            new_bases += pool[i].length;
        }
        if( rc == 0 && (new_bases < 0 || nbases != new_bases) ) {
            rc = RC(rcSRA, rcFormatter, rcResolving, rcData, rcInconsistent);
            PLOGERR(klogErr, (klogErr, rc, "total read length $(c)", PLOG_U32(c), new_bases));
        }
        if( rc != 0 ) {
            char err_buf[8192], *p = err_buf;
            size_t max = sizeof(err_buf);

            for(i = 1; i <= self->reads->nreads; i++) {
                int x = snprintf(p, max, "[%hd:%d, len = %hd", pool[i].start, pool[i].start + pool[i].length, pool[i].length);
                if( x > 0 && x < max ) {
                    max -= x;
                    p += x;
                    if( self->reads->spec[i].coord_type == rdsp_ExpectedBaseCallTable_ct ) {
                        if( pool[i].item[0] != NULL ) {
                            if( pool[i].item[0]->read_group_tag != NULL ) {
                                x = snprintf(p, max, ", matched tag '%s', mismatches %d]", pool[i].item[0]->read_group_tag, pool[i].score);
                            } else {
                                x = snprintf(p, max, ", mismatches %d]", pool[i].score);
                            }
                        } else {
                            x = snprintf(p, max, ", no match]");
                        }
                    } else {
                        x = snprintf(p, max, "]");
                    }
                }
                if( x <= 0 || x >= max ) {
                    max = strlen(err_buf);
                    err_buf[max - 4] = '\0';
                    strcat(err_buf, "... ");
                    break;
                }
                max -= x;
                p += x;
            }
            PLOGMSG(klogErr, (klogErr, "segments: $(seg) spot length actual: $(actual), calculated $(calc)",
                "seg=%s,actual=%u,calc=%d", err_buf, nbases, new_bases));
        }
    }
    return rc;
}

rc_t Experiment_MemberSegSimple(const ExperimentXML* self,
                                const char* const file_member_name, const char* const data_block_member_name,
                                const char** const new_member_name)
{
    rc_t rc = 0;
    if( self == NULL || new_member_name == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcResolving, rcParam, rcNull);
    } else {
        switch(self->processing.barcode_rule) {
            case eBarcodeRule_not_set:
                *new_member_name = data_block_member_name ? data_block_member_name : file_member_name;
                break;

            case eBarcodeRule_use_file_spot_name:
                *new_member_name = file_member_name;
                break;

            case eBarcodeRule_use_table_in_experiment:
            case eBarcodeRule_ignore_barcode:
                *new_member_name = NULL;
                break;
        }
    }
    return rc;
}

static
rc_t Experiment_Apply_RUN_ATTRIBUTES(const ExperimentXML* cself, RunAttributes* attr, uint32_t spot_length)
{
    rc_t rc = 0;

    if( cself == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcEvaluating, rcSelf, rcNull);
    } else if( attr != NULL ) {
        ExperimentXML* obj = (ExperimentXML*)cself;

        obj->processing.barcode_rule = attr->barcode_rule;
        if( attr->quality_offset != '\0' ) {
            obj->processing.quality_offset = attr->quality_offset;
        }
        if( attr->quality_type != eExperimentQualityType_Undefined ) {
            obj->processing.quality_type = attr->quality_type;
        }
        obj->spot_length = attr->spot_length ? attr->spot_length : spot_length;
        if( obj->spot_length == 0 &&
            (obj->platform->id == SRA_PLATFORM_ILLUMINA || obj->platform->id == SRA_PLATFORM_ABSOLID) ) {
            PLOGMSG(klogWarn, (klogWarn,
                "SPOT_LENGTH is not found or specified as $(l)", PLOG_U32(l), obj->spot_length));
        }
    }
    return rc;
}

rc_t Experiment_Make(const ExperimentXML** self, const KXMLDoc* doc, RunAttributes* attr)
{
    rc_t rc = 0;
    ExperimentXML* obj = NULL;
    const KXMLNodeset* ns = NULL;
    uint32_t p_spot_length = 0, r_spot_length = 0;

    if( self == NULL || doc == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    obj = calloc(1, sizeof(*obj));
    if( obj == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    }
    if( (rc = KXMLDocOpenNodesetRead(doc, &ns, "/EXPERIMENT_SET/EXPERIMENT | /EXPERIMENT")) == 0 ) {
        uint32_t count = 0;
        if( (rc = KXMLNodesetCount(ns, &count)) == 0 && count != 1 ) {
            rc = RC(rcSRA, rcFormatter, rcConstructing, rcTag, count ? rcExcessive : rcNotFound);
            LOGERR(klogErr, rc, "EXPERIMENT");
        } else {
            const KXMLNode* EXPERIMENT = NULL;
            if( (rc = KXMLNodesetGetNodeRead(ns, &EXPERIMENT, 0)) == 0 ) {
                if( (rc = PlatformXML_Make(&obj->platform, EXPERIMENT, &p_spot_length)) == 0 ) {
                    if( attr->platform != NULL ) {
                        if( obj->platform->id != attr->platform->id ) {
                            rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInconsistent);
                            LOGERR(klogErr, rc, "EXPERIMENT and RUN platforms differ");
                        } else {
                            PlatformXML_Whack(obj->platform);
                            obj->platform = attr->platform;
                            /* take ownership */
                            attr->platform = NULL;
                        }
                    }
                    if( rc == 0 ) {
                        if( attr->reads == NULL ) {
                            if( (rc = ReadSpecXML_Make(&obj->reads, EXPERIMENT, "DESIGN/SPOT_DESCRIPTOR", &r_spot_length)) != 0 ) {
                                LOGERR(klogErr, rc, "EXPERIMENT/.../READ_SPEC");
                            }
                        } else {
                            obj->reads = attr->reads;
                            /* take ownership */
                            attr->reads = NULL;
                        }
                        if( rc == 0 && (rc = parse_POOL(EXPERIMENT, obj)) == 0 ) {
                            rc = Experiment_Apply_RUN_ATTRIBUTES(obj, attr, r_spot_length ? r_spot_length : p_spot_length);
                        }
                        if( rc == 0 && p_spot_length != 0 && r_spot_length != 0 && p_spot_length != r_spot_length ) {
                            PLOGMSG(klogWarn, (klogWarn,
                                "in EXPERIMENT sequence length (cycle count) in PLATFORM $(p) differ"
                                " from SPOT_LENGTH $(l) in SPOT_DECODE_SPEC",
                                PLOG_2(PLOG_U32(p),PLOG_U32(l)), p_spot_length, r_spot_length));
                        }
                    }
                } else {
                    LOGERR(klogErr, rc, "EXPERIMENT/PLATFORM");
                }
                KXMLNodeRelease(EXPERIMENT);
            }
        }
        KXMLNodesetRelease(ns);
    }
    if( rc != 0 ) {
        *self = NULL;
        Experiment_Whack(obj);
    } else {
        *self = obj;
    }
    return rc;
}

rc_t Experiment_GetPlatform(const ExperimentXML* cself, const PlatformXML** platform)
{
    rc_t rc = 0;
    if( cself == NULL || platform == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else {
        *platform = cself->platform;
    }
    return rc;
}

rc_t Experiment_GetProcessing(const ExperimentXML* cself, const ProcessingXML** processing)
{
    rc_t rc = 0;
    if( cself == NULL || processing == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else {
        *processing = &cself->processing;
    }
    return rc;
}

rc_t Experiment_GetReadNumber(const ExperimentXML* cself, uint8_t* nreads)
{
    rc_t rc = 0;
    if( cself == NULL || nreads == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else {
        *nreads = cself->reads->nreads;
    }
    return rc;
}

rc_t Experiment_GetSpotLength(const ExperimentXML* cself, uint32_t* spot_length)
{
    rc_t rc = 0;
    if( cself == NULL || spot_length == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else {
        *spot_length= cself->spot_length;
    }
    return rc;
}

rc_t Experiment_GetRead(const ExperimentXML* cself, uint8_t read_id, const ReadSpecXML_read** read_spec)
{
    rc_t rc = 0;
    if( cself == NULL || read_spec == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else if( read_id >= cself->reads->nreads ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcId, rcOutofrange);
    } else {
        *read_spec = &(cself->reads->reads[read_id]);
    }
    return rc;
}

typedef struct PoolMember_FindByTag_Data_struct {
    /* in */
    const char* tag;
    uint8_t readid;
    /* out */
    const char* member_name;
} PoolMember_FindByTag_Data;

static
bool CC PoolMember_FindByTag(BSTNode *node, void *data)
{
    PoolMember_FindByTag_Data* d = (PoolMember_FindByTag_Data*)data;
    const PoolMember* n = (const PoolMember*)node;
    int i = 0;

    const ReadSpecXML_read_BASECALL* bc = n->spec[d->readid].item[i];
    while( bc != NULL ) {
        if( strcmp(bc->read_group_tag, d->tag) == 0 ) {
            d->member_name = n->name;
            return true;
        }
        bc = n->spec[d->readid].item[++i];
    }
    return false;
}

rc_t Experiment_FindReadInTable(const ExperimentXML* cself, uint8_t read_id, const char* key, const char** basecall, const char** member_name)
{
    rc_t rc = 0;
    if( cself == NULL || key == NULL || (basecall == NULL && member_name == NULL) ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else if( read_id >= cself->reads->nreads ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcId, rcOutofrange);
    } else {
        const ReadSpecXML_read* read_spec = &(cself->reads->reads[read_id++]);
        if( read_spec->coord_type != rdsp_ExpectedBaseCallTable_ct ) {
            rc = RC(rcSRA, rcFormatter, rcAccessing, rcName, rcNotFound);
        } else {
            ExpectedTableMatch match;

            rc = RC(rcSRA, rcFormatter, rcAccessing, rcName, rcNotFound);
            /* try to find key as basecall */
            match.seq = key;
            match.len = strlen(key);
            match.tag = NULL;
            match.direction = etm_Both;
            match.left_adjusted = true;
            if( Experiment_ExpectedTableMatch(&read_spec->coord.expected_basecalls, &match) ) {
                rc = 0;
                if( basecall != NULL ) {
                    *basecall = match.bc->basecall;
                }
                if( member_name != NULL ) {
                    if( cself->member_pool != NULL ) {
                        PoolMember_FindByTag_Data data;
                        data.readid = read_id;
                        data.tag = match.bc->read_group_tag;
                        if( BSTreeDoUntil(cself->member_pool, false, PoolMember_FindByTag, &data) ) {
                            *member_name = data.member_name;
                        } else {
                            rc = RC(rcSRA, rcFormatter, rcAccessing, rcSelf, rcCorrupt);
                        }
                    } else {
                        *member_name = NULL;
                    }
                }
            } else if( cself->member_pool != NULL ) {
                /* try to find key as member_name */
                PoolMember* pm = (PoolMember*)BSTreeFind(cself->member_pool, key, PoolMember_FindByName);
                if( pm != NULL ) {
                    rc = 0;
                    if( member_name != NULL ) {
                        *member_name = pm->name;
                    }
                    if( basecall != NULL ) {
                        if( pm->spec[read_id].item[0] != NULL ) {
                            *basecall = pm->spec[read_id].item[0]->basecall;
                            if( pm->spec[read_id].item[1] != NULL ) {
                                rc = RC(rcSRA, rcFormatter, rcAccessing, rcData, rcAmbiguous);
                            }
                        } else {
                            rc = RC(rcSRA, rcFormatter, rcAccessing, rcData, rcViolated);
                        }
                    }
                }
            }
        }
    }
    return rc;
}

rc_t Experiment_HasPool(const ExperimentXML* cself, bool* has_pool)
{
    rc_t rc = 0;
    if( cself == NULL || has_pool == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else {
        *has_pool = cself->member_pool != NULL;
    }
    return rc;
}

rc_t Experiment_ReadSegDefault(const ExperimentXML* self, SRASegment* seg)
{
    rc_t rc = 0;

    /* TBD to do memberseg call based on junk seq of known length */
    if( self == NULL || seg == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcResolving, rcParam, rcNull);
    } else if( self->spot_length == 0 ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcUnsupported);
    } else {
        uint32_t i;
        int32_t spot_len = self->spot_length;

        i = self->reads->nreads;
        do {
            int16_t len = 0;
            --i;
            switch(self->reads->reads[i].coord_type) {
                case rdsp_RelativeOrder_ct:
                    if( i == 0 ) {
                        len = spot_len;
                    } else {
                        len = 0;
                    }
                    break;
                case rdsp_BaseCoord_ct:
                case rdsp_CycleCoord_ct:
                    len = spot_len - (self->reads->reads[i].coord.start_coord - 1);
                    break;
                case rdsp_ExpectedBaseCall_ct:
                case rdsp_ExpectedBaseCallTable_ct:
                    if( self->reads->reads[i].coord.expected_basecalls.base_coord > 0 ) {
                        len = spot_len - (self->reads->reads[i].coord.expected_basecalls.base_coord - 1);
                    }
                    len += self->reads->reads[i].coord.expected_basecalls.default_length;
                    break;
                default:
                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcUnexpected);
                    LOGERR(klogErr, rc, "read type in default");
                    break;
            }
            spot_len -= len;
            if( spot_len < 0 || len < 0 ) {
                rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInconsistent);
                LOGERR(klogErr, rc, "cumulative read lengths and SEQUENCE_LENGTH");
                return rc;
            } else {
                seg[i].start = spot_len;
                seg[i].len = len;
            }
        } while( i > 0 );
    }
    return rc;
}

void Experiment_Whack(const ExperimentXML* cself)
{
    if( cself != NULL ) {
        ExperimentXML* self = (ExperimentXML*)cself;

        free(self->processing.quality_scorer);
        ReadSpecXML_Whack(self->reads);
        BSTreeWhack(self->member_pool, PoolMember_Whack, NULL);
        free(self->member_pool);
        PoolMember_Whack(&self->member_null->node, NULL);
        PlatformXML_Whack(self->platform);
        free(self);
    }
}
