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
#include <sra/types.h>
#include <os-native.h>

#include "common-xml.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

static
rc_t parse_READ_LABEL(const KXMLNode* READ_SPEC, ReadSpecXML_read* read)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;

    if( (rc = KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "READ_LABEL")) == 0 ) {
        rc = KXMLNodeReadCStr(node, &read->read_label, NULL);
        if( rc != 0 ) {
            LOGERR(klogErr, rc, "READ_LABEL");
        } else if( strlen(read->read_label) == 0 ) {
            free(read->read_label);
            read->read_label = NULL;
        }
        KXMLNodeRelease(node);
    } else {
        /* label is optional */
        rc = 0;
    }
    return rc;
}

static
rc_t parse_READ_CLASS(const KXMLNode* READ_SPEC, ReadSpecXML_read* read)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;
    char* rclass = NULL;

    if( (rc = KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "READ_CLASS")) == 0 ) {
        rc = KXMLNodeReadCStr(node, &rclass, NULL);
        KXMLNodeRelease(node);
    }
    if( rc != 0 ) {
        LOGERR(klogErr, rc, "READ_CLASS");
    } else if( strcmp(rclass, "Application Read") == 0 ) {
        read->read_class = SRA_READ_TYPE_BIOLOGICAL;
    } else if( strcmp(rclass, "Technical Read") == 0 ) {
        read->read_class = SRA_READ_TYPE_TECHNICAL;
    } else {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "READ_CLASS: $(c)", PLOG_S(c), rclass));
    }
    free(rclass);
    return rc;
}

static
rc_t parse_READ_TYPE(const KXMLNode* READ_SPEC, ReadSpecXML_read* read)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;
    char* rtype = NULL;

    if( (rc = KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "READ_TYPE")) == 0 ) {
        rc = KXMLNodeReadCStr(node, &rtype, "Forward");
        KXMLNodeRelease(node);
    }
    if( rc != 0 ) {
        LOGERR(klogErr, rc, "READ_TYPE");
    } else if( strcmp(rtype, "Forward") == 0 ) {
        read->read_type = rdsp_Forward_rt;
    } else if( strcmp(rtype, "Reverse") == 0 ) {
        read->read_type = rdsp_Reverse_rt;
    } else if( strcmp(rtype, "Adapter") == 0 ) {
        read->read_type = rdsp_Adapter_rt;
    } else if( strcmp(rtype, "Primer") == 0 ) {
        read->read_type = rdsp_Primer_rt;
    } else if( strcmp(rtype, "Linker") == 0 ) {
        read->read_type = rdsp_Linker_rt;
    } else if( strcmp(rtype, "BarCode") == 0 ) {
        read->read_type = rdsp_BarCode_rt;
    } else if( strcmp(rtype, "Other") == 0 ) {
        read->read_type = rdsp_Other_rt;
    } else {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "READ_CLASS: $(c)", PLOG_S(c), rtype));
    }
    free(rtype);
    return rc;
}

static
rc_t parse_RELATIVE_ORDER(const KXMLNode* READ_SPEC, ReadSpecXML_read* read, uint32_t* counter)
{
    rc_t rc1 = 0, rc2 = 0;
    const KXMLNode *node = NULL;

    if( KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "RELATIVE_ORDER") != 0 ) {
        return 0;
    }
    *counter = *counter + 1;
    read->coord.relative_order.follows = -1; /* not set */
    read->coord.relative_order.precedes = -1; /* not set */
    rc1 = KXMLNodeReadAttrAsI16(node, "follows_read_index", &read->coord.relative_order.follows);
    rc2 = KXMLNodeReadAttrAsI16(node, "precedes_read_index", &read->coord.relative_order.precedes);
    KXMLNodeRelease(node);
    if( (rc1 != 0 && GetRCState(rc1) != rcNotFound) || (rc2 != 0 && GetRCState(rc2) != rcNotFound) ) {
        rc1 = rc1 ? rc1 : rc2;
        LOGERR(klogErr, rc1, "RELATIVE_ORDER attributes");
    } else if( (read->coord.relative_order.follows < 0 && read->coord.relative_order.precedes < 0) ||
               (read->coord.relative_order.follows >= 0 && read->coord.relative_order.precedes >= 0) ) {
        rc1 = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInconsistent);
        LOGERR(klogErr, rc1, "RELATIVE_ORDER attributes");
    } else {
        rc1 = 0;
        read->coord_type = rdsp_RelativeOrder_ct;
    }
    return rc1;
}

static
rc_t parse_BASE_COORD(const KXMLNode* READ_SPEC, ReadSpecXML_read* read, uint32_t* counter)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;

    if( KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "BASE_COORD") != 0 ) {
        return 0;
    }
    *counter = *counter + 1;
    rc = KXMLNodeReadAsI16(node, &read->coord.start_coord);
    KXMLNodeRelease(node);
    if( rc != 0 ) {
        LOGERR(klogErr, rc, "BASE_COORD");
    } else if( read->coord.start_coord < 1 ) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "BASE_COORD: $(b)", PLOG_I32(b), read->coord.start_coord));
    } else {
        read->coord_type = rdsp_BaseCoord_ct;
    }
    return rc;
}

static
rc_t parse_CYCLE_COORD(const KXMLNode* READ_SPEC, ReadSpecXML_read* read, uint32_t* counter)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;

    if( KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "CYCLE_COORD") != 0 ) {
        return 0;
    }
    *counter = *counter + 1;
    rc = KXMLNodeReadAsI16(node, &read->coord.start_coord);
    KXMLNodeRelease(node);
    if( rc != 0 ) {
        LOGERR(klogErr, rc, "CYCLE_COORD");
    } else if( read->coord.start_coord < 1 ) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "CYCLE_COORD: $(b)", PLOG_I32(b), read->coord.start_coord));
    } else {
        read->coord_type = rdsp_CycleCoord_ct;
    }
    return rc;
}

rc_t parse_BASECALL_TABLE_add(ReadSpecXML_read_BASECALL_TABLE* table, const ReadSpecXML_read_BASECALL* src, const char* bc)
{
    rc_t rc = 0;

    if( table->size == table->count ) {
        const int inc = 20;
        ReadSpecXML_read_BASECALL* nt = realloc(table->table, (table->size + inc) * sizeof(*src));
        if( nt == NULL ) {
            rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        } else {
            table->size += inc;
            table->table = nt;
        }
    }
    if( rc == 0 ) {
        uint32_t i;
        for(i = 0; rc == 0 && i < table->count; i++) {
            if( strcmp(table->table[i].basecall, bc) == 0 ) {
                rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcDuplicate);
            }
        }
    }
    if( rc == 0 ) {
        table->table[table->count].basecall = bc ? strdup(bc) : NULL;
        table->table[table->count].read_group_tag = src->read_group_tag ? strdup(src->read_group_tag) : NULL;
        table->table[table->count].min_match = src->min_match;
        table->table[table->count].max_mismatch = src->max_mismatch;
        table->table[table->count].match_edge = src->match_edge;
        if( table->table[table->count].basecall == NULL ||
            (src->read_group_tag != NULL && table->table[table->count].read_group_tag == NULL) ) {
            rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        } else {
            uint32_t k = strlen(table->table[table->count].basecall);
            k += src->max_mismatch;
            switch(src->match_edge) {
                case match_edge_End:
                    if( table->match_end < k ) {
                        table->match_end = k;
                    }
                    break;
                case match_edge_Start:
                case match_edge_Full:
                    if( table->match_start < k ) {
                        table->match_start = k;
                    }
                    break;
            }

            rc = AgrepMake(&table->table[table->count].agrep,
                           AGREP_PATTERN_4NA |
                           AGREP_ALG_MYERS |
                           AGREP_EXTEND_BETTER, table->table[table->count].basecall);
        }
        table->count++;
    }
    return rc;
}

static
rc_t parse_BASECALL_new4na(ReadSpecXML_read_BASECALL_TABLE* table, const ReadSpecXML_read_BASECALL* bc)
{
    rc_t rc = 0;

    if( table == NULL || bc == NULL || bc->basecall[0] == '\0' ||
        strspn(bc->basecall, "ACGTURYSWKMBDHVNacgturyswkmbdhvn.-") != strlen(bc->basecall) ) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
    }
    if( rc == 0 ) {
        char r[4096];
        size_t l = strlen(bc->basecall);

        if( l >= sizeof(r) ) {
            rc = RC(rcExe, rcFormatter, rcConstructing, rcBuffer, rcInsufficient);
        } else {
            int i;
            for(i = 0; i < l; i++) {
                if( bc->match_edge == match_edge_End ) {
                    /* need to reverse BC because match will happen on reversed seq */
                    r[i] = bc->basecall[l - i - 1];
                } else {
                    r[i] = bc->basecall[i];
                }
                /*r[i] = isalpha(r[i]) ? toupper(r[i]) : 'N';*/
            }
            r[i] = '\0';
            rc = parse_BASECALL_TABLE_add(table, bc, r);
        }
    }
    return rc;
}

static
rc_t parse_EXPECTED_BASECALL(const KXMLNode* READ_SPEC, ReadSpecXML_read* read, uint32_t* counter)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;
    ReadSpecXML_read_BASECALL bc;

    if( KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "EXPECTED_BASECALL") != 0 ) {
        return 0;
    }
    *counter = *counter + 1;
    if( (rc = KXMLNodeReadCStr(node, &bc.basecall, NULL)) != 0 ) {
        LOGERR(klogErr, rc, "EXPECTED_BASECALL value");
    } else if( (rc = KXMLNodeReadAttrAsU32(node, "default_length", &read->coord.expected_basecalls.default_length)) != 0 &&
        GetRCState(rc) != rcNotFound ) {
        LOGERR(klogErr, rc, "EXPECTED_BASECALL 'default_length' attribute");
    } else if( (rc = KXMLNodeReadAttrAsU32(node, "base_coord", &read->coord.expected_basecalls.base_coord)) != 0 &&
         GetRCState(rc) != rcNotFound ) {
        LOGERR(klogErr, rc, "EXPECTED_BASECALL 'base_coord' attribute");
    } else {
        bc.read_group_tag = NULL;
        bc.min_match = strlen(bc.basecall);
        /* allow 10% mismatches */
        bc.max_mismatch = bc.min_match * 0.1;
        bc.match_edge = match_edge_Full;
        if( (rc = parse_BASECALL_new4na(&read->coord.expected_basecalls, &bc)) == 0 ) {
            read->coord_type = rdsp_ExpectedBaseCall_ct;
        }
    }
    if( rc != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "EXPECTED_BASECALL '$(c)'", PLOG_S(c), bc.basecall));
    }
    free(bc.basecall);
    KXMLNodeRelease(node);
    return rc;
}

static
rc_t parse_BASECALL(const KXMLNode* node, ReadSpecXML_read_BASECALL_TABLE* table)
{
    rc_t rc = 0;
    char* match_edge = NULL;
    ReadSpecXML_read_BASECALL bc;


    memset(&bc, 0, sizeof(bc));
    if( (rc = KXMLNodeReadCStr(node, &bc.basecall, NULL)) != 0 ) {
        LOGERR(klogErr, rc, "BASECALL");
        return rc;
    }
    rc = KXMLNodeReadAttrCStr(node, "read_group_tag", &bc.read_group_tag, NULL);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "BASECALL @read_group_tag");
        return rc;
    }
    rc = KXMLNodeReadAttrAsU32(node, "min_match", &bc.min_match);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "BASECALL @min_match");
        return rc;
    } else if(bc.min_match > strlen(bc.basecall)) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "BASECALL @min_match more than basecall length: $(bc)", PLOG_S(bc), bc.basecall));
        return rc;
    } else if( bc.min_match == 0 ) {
        bc.min_match = strlen(bc.basecall);
    }
    rc = KXMLNodeReadAttrAsU32(node, "max_mismatch", &bc.max_mismatch);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "BASECALL @max_mismatch");
        return rc;
    } else if(bc.max_mismatch >= strlen(bc.basecall)) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "BASECALL @max_mismatch value too big: $(bc)", PLOG_S(bc), bc.basecall));
        return rc;
    }
    if( (bc.min_match + bc.max_mismatch) < strlen(bc.basecall) ) {
        if( bc.min_match != 0 ) {
            PLOGMSG(klogWarn, (klogWarn, "BASECALL @min_match is too small: $(bc)", PLOG_S(bc), bc.basecall));
        }
        bc.min_match = strlen(bc.basecall) - bc.max_mismatch;
    }
    rc = KXMLNodeReadAttrCStr(node, "match_edge", &match_edge, NULL);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "BASECALL @match_edge");
    } else if( match_edge == NULL || strcmp(match_edge, "full") == 0 ) {
        bc.match_edge = match_edge_Full;
    } else if( strcmp(match_edge, "start") == 0 ) {
        bc.match_edge = match_edge_Start;
    } else if( strcmp(match_edge, "end") == 0 ) {
        bc.match_edge = match_edge_End;
    } else {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "BASECALL @match_edge: $(c)", PLOG_S(c), match_edge));
    }
    free(match_edge);

    if( (rc = parse_BASECALL_new4na(table, &bc)) != 0 ) {
        LOGERR(klogErr, rc, "BASECALL");
    }
    free(bc.basecall);
    free(bc.read_group_tag);
    return rc;
}

static
rc_t parse_EXPECTED_BASECALL_TABLE(const KXMLNode* READ_SPEC, ReadSpecXML_read* read, uint32_t* counter)
{
    rc_t rc = 0;
    const KXMLNode* EXPECTED_BASECALL_TABLE;
    const KXMLNodeset* BASECALLS;
   
    if( KXMLNodeGetFirstChildNodeRead(READ_SPEC, &EXPECTED_BASECALL_TABLE, "EXPECTED_BASECALL_TABLE") != 0 ) {
        return 0;
    }
    *counter = *counter + 1;

    if( (rc = KXMLNodeReadAttrAsU32(EXPECTED_BASECALL_TABLE, "default_length", &read->coord.expected_basecalls.default_length)) != 0 &&
        GetRCState(rc) != rcNotFound ) {
        LOGERR(klogErr, rc, "EXPECTED_BASECALL_TABLE 'default_length' attribute");
    } else if( (rc = KXMLNodeReadAttrAsU32(EXPECTED_BASECALL_TABLE, "base_coord", &read->coord.expected_basecalls.base_coord)) != 0 &&
         GetRCState(rc) != rcNotFound ) {
        LOGERR(klogErr, rc, "EXPECTED_BASECALL_TABLE 'base_coord' attribute");
    } else if( (rc = KXMLNodeOpenNodesetRead(EXPECTED_BASECALL_TABLE, &BASECALLS, "BASECALL")) == 0 ) {
        uint32_t count = 0;
        KXMLNodesetCount(BASECALLS, &count);
        if( count > 0 ) {
            uint32_t i, tagged;
            for(i = 0; rc == 0 && i < count; i++) {
                const KXMLNode* BASECALL = NULL;
                if( (rc = KXMLNodesetGetNodeRead(BASECALLS, &BASECALL, i)) == 0 ) {
                        if( (rc = parse_BASECALL(BASECALL, &read->coord.expected_basecalls)) != 0 ) {
                            PLOGERR(klogErr, (klogErr, rc, "BASECALL $(i)", PLOG_U32(i), i));
                        }
                        KXMLNodeRelease(BASECALL);
                } else {
                    PLOGERR(klogErr, (klogErr, rc, "BASECALL node #$(i)", PLOG_U32(i), i));
                }
            }
            for(i = 0, tagged = 0; rc == 0 && i < read->coord.expected_basecalls.count; i++) {
                if( read->coord.expected_basecalls.table[i].read_group_tag != NULL ) {
                    tagged++;
                }
            }
            if( tagged > 0 ) {
                if( read->coord.expected_basecalls.count != tagged ) {
                    LOGMSG(klogWarn, "mixed tagged/untagged BASECALLs in table");
                } else {
                    read->coord.expected_basecalls.pooled = true;
                }
            }
            if( rc == 0 ) {
                read->coord_type = rdsp_ExpectedBaseCallTable_ct;
            } else {
                free(read->coord.expected_basecalls.table);
            }
        } else {
            rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcIncomplete);
            LOGERR(klogErr, rc, "need at leaset one BASECALL element");
        }
        KXMLNodesetRelease(BASECALLS);
    } else {
        LOGERR(klogErr, rc, "missing element(s) BASECALL");
    }
    KXMLNodeRelease(EXPECTED_BASECALL_TABLE);
    return rc;
}

static
rc_t parse_READ_SPEC(const KXMLNode* READ_SPEC, ReadSpecXML_read* read, uint32_t index)
{
    rc_t rc = 0;
    uint32_t coord = 0;

    if( (rc = parse_READ_LABEL(READ_SPEC, read)) != 0 ||
        (rc = parse_READ_CLASS(READ_SPEC, read)) != 0 ||
        (rc = parse_READ_TYPE(READ_SPEC, read)) != 0 ||
        (rc = parse_RELATIVE_ORDER(READ_SPEC, read, &coord)) != 0 ||
        (rc = parse_BASE_COORD(READ_SPEC, read, &coord)) != 0 ||
        (rc = parse_CYCLE_COORD(READ_SPEC, read, &coord)) != 0 ||
        (rc = parse_EXPECTED_BASECALL(READ_SPEC, read, &coord)) != 0 ||
        (rc = parse_EXPECTED_BASECALL_TABLE(READ_SPEC, read, &coord)) != 0 ) {
        return rc;
    }
    if( coord != 1 || read->coord_type == 0) {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "bad or missing coordinate descriptor in read #$(index)",
                        PLOG_U32(index), index));
    }
    return rc;
}

static
rc_t parse_READ_INDEX(const KXMLNode* READ_SPEC, uint32_t* index)
{
    rc_t rc = 0;
    const KXMLNode* node = NULL;

    if( (rc = KXMLNodeGetFirstChildNodeRead(READ_SPEC, &node, "READ_INDEX")) == 0 ) {
        if( (rc = KXMLNodeReadAsU32(node, index)) != 0 ) {
            LOGERR(klogErr, rc, "READ_INDEX");
        }
        KXMLNodeRelease(node);
    } else {
        LOGERR(klogErr, rc, "missing required node READ_INDEX");
    }
    return rc;
}

static
rc_t XMLNode_get_subnode(const KXMLNode* node, const char* child, const KXMLNode** sub)
{
    rc_t rc = 0;
    const KXMLNodeset* ns = NULL; 

    *sub = NULL;
    if( (rc = KXMLNodeOpenNodesetRead(node, &ns, child)) == 0 ) {
        uint32_t count = 0;
        if( (rc = KXMLNodesetCount(ns, &count)) == 0 ) {
            if( count == 0 ) {
                rc = RC(rcExe, rcFormatter, rcParsing, rcNode, rcNotFound);
            } else if( count > 1 ) {
                rc = RC(rcExe, rcFormatter, rcParsing, rcNode, rcDuplicate);
            } else {
                rc = KXMLNodesetGetNodeRead(ns, sub, 0);
            }
        }
        KXMLNodesetRelease(ns);
    }
    return rc;
}

rc_t XMLNode_get_strnode(const KXMLNode* node, const char* child, bool optional, char** value)
{
    rc_t rc = 0;
    const KXMLNode* n = NULL;
    
    *value = NULL;
    if( (rc = XMLNode_get_subnode(node, child, &n)) == 0 ) {
        if( (rc = KXMLNodeReadCStr(n, value, NULL)) == 0 && *value != NULL && **value != '\0' ) {
            /* rtrim and ltrim */
            char* start = *value, *end;
            while( *start != '\0' && isspace(*start) ) {
                start++;
            }
            for(end = start; *end != '\0'; end++){}
            while( --end > start && isspace(*end) ){}
            memmove(*value, start, end - start + 1);
            (*value)[end - start + 1] = '\0';
        }
        KXMLNodeRelease(n);
    } else if( optional && GetRCState(rc) == rcNotFound ) {
        rc = 0;
    }
    return rc;
}

static
rc_t XMLNode_get_u32node(const KXMLNode* node, const char* child, bool optional, uint32_t* value)
{
    rc_t rc = 0;
    const KXMLNode* n = NULL;
    
    if( (rc = XMLNode_get_subnode(node, child, &n)) == 0 ) {
        rc = KXMLNodeReadAsU32(n, value);
        KXMLNodeRelease(n);
    } else if( optional && GetRCState(rc) == rcNotFound ) {
        rc = 0;
    }
    return rc;
}

rc_t PlatformXML_Make(const PlatformXML** cself, const KXMLNode* node, uint32_t* spot_length)
{
    rc_t rc = 0;
    PlatformXML* platform;
    const KXMLNodeset* ns = NULL;

    platform = calloc(1, sizeof(*platform));
    if( platform != NULL ) {
        platform->id = SRA_PLATFORM_UNDEFINED;
        if( (rc = KXMLNodeOpenNodesetRead(node, &ns, "PLATFORM/*")) == 0 ) {
            uint32_t count = 0;
            if( (rc = KXMLNodesetCount(ns, &count)) != 0 || count != 1 ) {
                rc = rc ? rc : RC(rcExe, rcFormatter, rcConstructing, rcTag, count ? rcExcessive : rcNotFound);
            } else {
                const KXMLNode* n = NULL;
                if( (rc = KXMLNodesetGetNodeRead(ns, &n, 0)) == 0 ) {
                    const char* name = NULL;
                    if( (rc = KXMLNodeElementName(n, &name)) == 0 ) {
                        DEBUG_MSG (4, ("PLATFORM: %s\n", name));
                        if( strcmp(name, "LS454") == 0 ) {
                            if( (rc = XMLNode_get_strnode(n, "KEY_SEQUENCE", true, &platform->param.ls454.key_sequence)) == 0 &&
                                (rc = XMLNode_get_strnode(n, "FLOW_SEQUENCE", true, &platform->param.ls454.flow_sequence)) == 0 &&
                                (rc = XMLNode_get_u32node(n, "FLOW_COUNT", true, &platform->param.ls454.flow_count)) == 0 ) {
                                platform->id = SRA_PLATFORM_454;
                                if( platform->param.ls454.key_sequence == NULL || platform->param.ls454.key_sequence[0] == '\0' ) {
                                    PLOGMSG(klogWarn, (klogWarn, "$(n)/KEY_SEQUENCE is not specified", PLOG_S(n), name));
                                    free(platform->param.ls454.key_sequence);
                                    platform->param.ls454.key_sequence = NULL;
                                }
                                if( platform->param.ls454.flow_sequence == NULL || platform->param.ls454.flow_sequence[0] == '\0' ) {
                                    PLOGMSG(klogWarn, (klogWarn, "$(n)/FLOW_SEQUENCE is not specified", PLOG_S(n), name));
                                    free(platform->param.ls454.flow_sequence);
                                    platform->param.ls454.flow_sequence = NULL;
                                } else if( platform->param.ls454.flow_count % strlen(platform->param.ls454.flow_sequence) != 0 ) {
                                    rc = RC(rcExe, rcFormatter, rcConstructing, rcId, rcInconsistent);
                                } else {
                                    char* flows = platform->param.ls454.flow_sequence;
                                    platform->param.ls454.flow_sequence = malloc(platform->param.ls454.flow_count + 1);
                                    if( platform->param.ls454.flow_sequence != NULL ) {
                                        uint32_t i = platform->param.ls454.flow_count / strlen(flows);
                                        platform->param.ls454.flow_sequence[0] = '\0';
                                        while( i > 0 ) {
                                            strcat(platform->param.ls454.flow_sequence, flows);
                                            i--;
                                        }
                                        free(flows);
                                    } else {
                                        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
                                    }
                                }
                            }
                        } else if( strcmp(name, "ION_TORRENT") == 0 ) {
                            if( (rc = XMLNode_get_strnode(n, "KEY_SEQUENCE", true, &platform->param.ion_torrent.key_sequence)) == 0 &&
                                (rc = XMLNode_get_strnode(n, "FLOW_SEQUENCE", true, &platform->param.ion_torrent.flow_sequence)) == 0 &&
                                (rc = XMLNode_get_u32node(n, "FLOW_COUNT", true, &platform->param.ion_torrent.flow_count)) == 0 ) {
                                platform->id = SRA_PLATFORM_ION_TORRENT;
                                if( platform->param.ion_torrent.key_sequence == NULL || platform->param.ion_torrent.key_sequence[0] == '\0' ) {
                                    PLOGMSG(klogWarn, (klogWarn, "$(n)/KEY_SEQUENCE is not specified", PLOG_S(n), name));
                                    free(platform->param.ion_torrent.key_sequence);
                                    platform->param.ion_torrent.key_sequence = NULL;
                                }
                                if( platform->param.ion_torrent.flow_sequence == NULL || platform->param.ion_torrent.flow_sequence[0] == '\0' ) {
                                    PLOGMSG(klogWarn, (klogWarn, "$(n)/FLOW_SEQUENCE is not specified", PLOG_S(n), name));
                                    free(platform->param.ion_torrent.flow_sequence);
                                    platform->param.ion_torrent.flow_sequence = NULL;
                                } else if( platform->param.ion_torrent.flow_count % strlen(platform->param.ion_torrent.flow_sequence) != 0 ) {
                                    rc = RC(rcExe, rcFormatter, rcConstructing, rcId, rcInconsistent);
                                } else {
                                    char* flows = platform->param.ion_torrent.flow_sequence;
                                    platform->param.ion_torrent.flow_sequence = malloc(platform->param.ion_torrent.flow_count + 1);
                                    if( platform->param.ion_torrent.flow_sequence != NULL ) {
                                        uint32_t i = platform->param.ion_torrent.flow_count / strlen(flows);
                                        platform->param.ion_torrent.flow_sequence[0] = '\0';
                                        while( i > 0 ) {
                                            strcat(platform->param.ion_torrent.flow_sequence, flows);
                                            i--;
                                        }
                                        free(flows);
                                    } else {
                                        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
                                    }
                                }
                            }
                        } else if( strcmp(name, "ILLUMINA") == 0 ) {
                            if( (rc = XMLNode_get_u32node(n, "SEQUENCE_LENGTH", false, spot_length)) != 0 &&
                                GetRCState(rc) == rcNotFound ) {
                                rc = XMLNode_get_u32node(n, "CYCLE_COUNT", true, spot_length);
                            }
                            if( rc == 0 ) {
                                platform->id = SRA_PLATFORM_ILLUMINA;
                            }
                        } else if( strcmp(name, "HELICOS") == 0 ) {
                            if( (rc = XMLNode_get_strnode(n, "FLOW_SEQUENCE", true, &platform->param.helicos.flow_sequence)) == 0 &&
                                (rc = XMLNode_get_u32node(n, "FLOW_COUNT", true, &platform->param.helicos.flow_count)) == 0 ) {
                                platform->id = SRA_PLATFORM_HELICOS;
                            }
                        } else if( strcmp(name, "ABI_SOLID") == 0 ) {
                            if( (rc = XMLNode_get_u32node(n, "SEQUENCE_LENGTH", false, spot_length)) != 0 &&
                                GetRCState(rc) == rcNotFound ) {
                                rc = XMLNode_get_u32node(n, "CYCLE_COUNT", true, spot_length);
                            }
                            if( rc == 0 ) {
                                platform->id = SRA_PLATFORM_ABSOLID;
                            }
                        } else if( strcmp(name, "PACBIO_SMRT") == 0 ) {
                            platform->id = SRA_PLATFORM_PACBIO_SMRT;
                        } else if( strcmp(name, "COMPLETE_GENOMICS") == 0 ) {
                            platform->id = SRA_PLATFORM_COMPLETE_GENOMICS;
                        }
                        if( rc != 0 || platform->id == SRA_PLATFORM_UNDEFINED ) {
                            rc = rc ? rc : RC(rcExe, rcFormatter, rcConstructing, rcId, rcUnrecognized);
                            PLOGERR(klogErr, (klogErr, rc, "PLATFORM '$(n)'", PLOG_S(n), name));
                        }
                    }
                    KXMLNodeRelease(n);
                }
            }
            KXMLNodesetRelease(ns);
        }
        if( rc == 0 ) {
            *cself = platform;
        } else {
            PlatformXML_Whack(platform);
        }
    } else {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    }
    return rc;
}

void PlatformXML_Whack(const PlatformXML* cself)
{
    if( cself != NULL ) {
        PlatformXML* self = (PlatformXML*)cself;
        switch(self->id) {
            case SRA_PLATFORM_454:
                free(self->param.ls454.key_sequence);
                free(self->param.ls454.flow_sequence);
                break;
            case SRA_PLATFORM_ION_TORRENT:
                free(self->param.ion_torrent.key_sequence);
                free(self->param.ion_torrent.flow_sequence);
                break;
            case SRA_PLATFORM_ILLUMINA:
                break;
            case SRA_PLATFORM_HELICOS:
                free(self->param.helicos.flow_sequence);
                break;
            case SRA_PLATFORM_ABSOLID:
                break;
            case SRA_PLATFORM_PACBIO_SMRT:
                break;
            case SRA_PLATFORM_COMPLETE_GENOMICS:
                break;
            case SRA_PLATFORM_UNDEFINED:
                break;
        }
        free(self);
    }
}

rc_t ReadSpecXML_Make(const ReadSpecXML** cself, const KXMLNode* node, const char* path, uint32_t* spot_length)
{
    rc_t rc = 0;
    ReadSpecXML* obj;

    assert(node != NULL);

    obj = calloc(1, sizeof(*obj));
    if( obj != NULL ) {
        const KXMLNodeset* READ_SPECS = NULL;
        const KXMLNode* nsplen = NULL;
        obj->nreads = 0;
        obj->reads = &obj->spec[1]; /* set actual read to next one after fake fixed size read */
        if( (rc = KXMLNodeGetFirstChildNodeRead(node, &nsplen, "%s/SPOT_DECODE_SPEC/SPOT_LENGTH", path)) == 0 ) {
            rc = KXMLNodeReadAsU32(nsplen, spot_length);
            KXMLNodeRelease(nsplen);
        }
        if( rc != 0 ) {
            if( GetRCState(rc) != rcNotFound) {
                LOGERR(klogErr, rc, "node SPOT_LENGTH");
            } else {
                rc = 0;
            }
        }
        if( rc == 0 &&
            (rc = KXMLNodeOpenNodesetRead(node, &READ_SPECS, "%s/SPOT_DECODE_SPEC/READ_SPEC", path)) == 0 ) {
            if( (rc = KXMLNodesetCount(READ_SPECS, &obj->nreads)) != 0 || obj->nreads < 1 || obj->nreads > SRALOAD_MAX_READS ) {
                rc = rc ? rc : RC(rcExe, rcFormatter, rcConstructing, rcTag, obj->nreads ? rcExcessive : rcNotFound);
            } else {
                const uint8_t class_none = ~0;
                uint32_t i;
                for(i = 0; i < obj->nreads; i++) {
                    /* set all read's mandatory READ_CLASS to 'unset' value */
                    obj->reads[i].read_class = class_none;
                }
                for(i = 0; rc == 0 && i < obj->nreads; i++) {
                    const KXMLNode* READ_SPEC = NULL;
                    uint32_t index = ~0;
                    if( (rc = KXMLNodesetGetNodeRead(READ_SPECS, &READ_SPEC, i)) == 0 &&
                        (rc = parse_READ_INDEX(READ_SPEC, &index)) == 0 ) {
                        if( obj->reads[index].read_class == class_none ) {
                            if( (rc = parse_READ_SPEC(READ_SPEC, &obj->reads[index], index)) != 0 ) {
                                PLOGERR(klogErr, (klogErr, rc, "READ_INDEX $(i)", PLOG_U32(i), index));
                            }
                        }
                        KXMLNodeRelease(READ_SPEC);
                    } else {
                        PLOGERR(klogErr, (klogErr, rc, "READ_SPEC node #$(i)", PLOG_U32(i), i));
                    }
                }
                if(rc == 0) {
                    for(i = 0; i < obj->nreads; i++) {
                        if(obj->reads[i].read_class == class_none ) {
                            rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
                            PLOGERR(klogErr, (klogErr, rc, "missing READ_SPEC for READ_INDEX #$(i)", PLOG_U32(i), i));
                        } else {
                            switch(obj->reads[i].coord_type) {
                                case rdsp_RelativeOrder_ct:
                                    if( obj->nreads == 1 ) {
                                        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInconsistent);
                                        PLOGERR(klogErr, (klogErr, rc, "#$(i) single RELATIVE_ORDER not allowed", PLOG_U32(i), i));
                                    } else if( i > 0 && obj->reads[i - 1].coord_type != rdsp_ExpectedBaseCall_ct &&
                                                        obj->reads[i - 1].coord_type != rdsp_ExpectedBaseCallTable_ct ) {
                                        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInconsistent);
                                        PLOGERR(klogErr, (klogErr, rc, "#$(i) RELATIVE_ORDER cannot follow read of $(type) type",
                                                PLOG_2(PLOG_U32(i),PLOG_S(type)), i, 
                                                obj->reads[i - 1].coord_type == rdsp_BaseCoord_ct ? "BASE_COORD" :
                                                    obj->reads[i - 1].coord_type == rdsp_CycleCoord_ct ? "CYCLE_COORD" :
                                                        obj->reads[i - 1].coord_type == rdsp_RelativeOrder_ct ? "RELATIVE_ORDER" : "????"));
                                    } else if( (obj->reads[i].coord.relative_order.follows >= 0 &&
                                                (i == 0 || obj->reads[i].coord.relative_order.follows != i - 1)) ||
                                               (obj->reads[i].coord.relative_order.precedes >= 0 &&
                                                (i == obj->nreads - 1 || obj->reads[i].coord.relative_order.precedes != i + 1)) ) {
                                        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
                                        PLOGERR(klogErr, (klogErr, rc, "#$(i) RELATIVE_ORDER attribute value(s)", PLOG_U32(i), i));
                                    }
                                    break;
                                case rdsp_BaseCoord_ct:
                                case rdsp_CycleCoord_ct:
                                    if( i == 0 && obj->reads[i].coord.start_coord != 1 ) {
                                        rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcInvalid);
                                        PLOGERR(klogErr, (klogErr, rc, "#$(i) 1st read coordinate must be 1", PLOG_U32(i), i));
                                    }
                                    break;
                                case rdsp_ExpectedBaseCall_ct:
                                case rdsp_ExpectedBaseCallTable_ct:
                                    break;
                                default:
                                    rc = RC(rcExe, rcFormatter, rcConstructing, rcData, rcUnexpected);
                                    PLOGERR(klogErr, (klogErr, rc, "read #$(i) type", PLOG_U32(i), i));
                                    break;
                            }
                        }
                    }
                }
            }
            KXMLNodesetRelease(READ_SPECS);
        }
    } else {
        rc = RC(rcExe, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    }
    if( rc == 0 ) {
        obj->spec[0].coord_type = rdsp_FIXED_BRACKET_ct;
        obj->spec[0].read_class = SRA_READ_TYPE_TECHNICAL;
        obj->spec[0].read_type = rdsp_Other_rt;
        obj->spec[0].coord.start_coord = 1;
        obj->spec[obj->nreads + 1].coord_type = rdsp_FIXED_BRACKET_ct;
        obj->spec[obj->nreads + 1].read_class = SRA_READ_TYPE_TECHNICAL;
        obj->spec[obj->nreads + 1].read_type = rdsp_Other_rt;
        obj->spec[obj->nreads + 1].coord.start_coord = 0;
        *cself = obj;
    } else {
        ReadSpecXML_Whack(obj);
    }
    return rc;
}

void ReadSpecXML_Whack(const ReadSpecXML* cself)
{
    if( cself != NULL ) {
        uint32_t i, j;
        ReadSpecXML* self = (ReadSpecXML*)cself;

        for(i = 0; i < self->nreads; i++) {
            free(self->reads[i].read_label);
            switch(self->reads[i].coord_type) {
                case rdsp_ExpectedBaseCall_ct:
                case rdsp_ExpectedBaseCallTable_ct:
                    for(j = 0; j < self->reads[i].coord.expected_basecalls.count; j++) {
                        free(self->reads[i].coord.expected_basecalls.table[j].basecall);
                        free(self->reads[i].coord.expected_basecalls.table[j].read_group_tag);
                        AgrepWhack(self->reads[i].coord.expected_basecalls.table[j].agrep);
                    }
                    free(self->reads[i].coord.expected_basecalls.table);
                    break;
                default:
                    break;
            }
        }
        free(self);
    }
}
