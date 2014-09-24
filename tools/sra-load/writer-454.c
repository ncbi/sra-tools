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
#include <sra/types.h>
#include <insdc/insdc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "debug.h"
#include "writer-454.h"
#include "sra-writer.h"

struct SRAWriter454 {
    SRAWriter* base;
    pstring flows;
    uint32_t flow_count;
    pstring keys;

    uint32_t ci_flowc;
    uint32_t ci_keys;
    uint32_t ci_name;
    uint32_t ci_read;
    uint32_t ci_qual;
    uint32_t ci_signal;
    uint32_t ci_position;
    uint32_t ci_q_left;
    uint32_t ci_q_right;
    uint32_t ci_a_left;
    uint32_t ci_a_right;
};

rc_t SRAWriter454_Make(const SRAWriter454** cself, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SRAWriter454* self;
    const PlatformXML* platform;

    if( cself == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    if( (rc = Experiment_GetPlatform(config->experiment, &platform)) != 0 ) {
        return rc;
    }
    if( platform->id != SRA_PLATFORM_454 ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInvalid);
        LOGERR(klogErr, rc, "platform type");
        return rc;
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    } else if( (rc = SRAWriter_Make(&self->base, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize base writer");
    } else {
        if( platform->param.ls454.flow_sequence != NULL ) {
            rc = pstring_assign(&self->flows, platform->param.ls454.flow_sequence,
                                       strlen(platform->param.ls454.flow_sequence));
        }
        if( rc == 0 && platform->param.ls454.key_sequence != NULL ) {
            rc = pstring_assign(&self->keys, platform->param.ls454.key_sequence,
                                      strlen(platform->param.ls454.key_sequence));
        }
        self->flow_count = platform->param.ls454.flow_count;
    }
    if( rc == 0 ) {
        *cself = self;
    } else {
        SRAWriter454_Whack(self, NULL);
    }
    return rc;
}

void SRAWriter454_Whack(const SRAWriter454* cself, SRATable** table)
{
    if( cself != NULL ) {
        SRAWriter454* self = (SRAWriter454*)cself;
        SRAWriter_Whack(self->base, table);
        free(self);
    }
}

static
rc_t SRAWriter454_Open(const SRAWriter454* cself, bool sig_and_pos)
{
    rc_t rc = 0;
    SRAWriter454* self = (SRAWriter454*)cself;

    if( cself->ci_name == 0 ) {
        INSDC_coord_one d = 0;
        pstring x;

        if( (rc = SRAWriter_CreateTable(self->base, "NCBI:SRA:_454_:tbl:v2")) != 0 ) {
            return rc;
        }
        if( (rc = SRAWriter_WriteDefaults(self->base)) != 0 ) {
            LOGERR(klogInt, rc, "failed to write table defaults");
            return rc;
        }
        COL_OPEN(self->ci_name, "NAME", vdb_ascii_t);
        COL_OPEN(self->ci_read, "READ", insdc_fasta_t);
        COL_OPEN(self->ci_qual, "QUALITY", insdc_phred_t);
        COL_OPEN(self->ci_q_left, "CLIP_QUALITY_LEFT", "INSDC:coord:one");
        COL_OPEN(self->ci_q_right, "CLIP_QUALITY_RIGHT", "INSDC:coord:one");
        COL_OPEN(self->ci_a_left, "CLIP_ADAPTER_LEFT", "INSDC:coord:one");
        COL_OPEN(self->ci_a_right, "CLIP_ADAPTER_RIGHT", "INSDC:coord:one");
        COL_OPEN(self->ci_flowc, "FLOW_CHARS", insdc_fasta_t);
        COL_OPEN(self->ci_keys, "KEY_SEQUENCE", insdc_fasta_t);

        COL_WRITE_DEFAULT(self->ci_flowc, "FLOW_CHARS", self->flows);
        COL_WRITE_DEFAULT(self->ci_keys, "KEY_SEQUENCE", self->keys);
        if( (rc = pstring_assign(&x, &d, sizeof(d))) == 0 ) {
            COL_WRITE_DEFAULT(self->ci_q_left, "CLIP_QUALITY_LEFT", x);
            COL_WRITE_DEFAULT(self->ci_q_right, "CLIP_QUALITY_RIGHT", x);
            COL_WRITE_DEFAULT(self->ci_a_left, "CLIP_ADAPTER_LEFT", x);
            COL_WRITE_DEFAULT(self->ci_a_right, "CLIP_ADAPTER_RIGHT", x);
        }
    }
    if( sig_and_pos && (cself->ci_signal == 0 || cself->ci_position == 0) ) {
        COL_OPEN(self->ci_signal, "SIGNAL", ncbi_isamp1_t);
        COL_OPEN(self->ci_position, "POSITION", "INSDC:position:one");
    }
    return rc;
}

rc_t SRAWriter454_WriteHead(const SRAWriter454* cself, const pstring* flow_chars, const pstring* key_sequence)
{
    rc_t rc = 0;

    if( (rc = SRAWriter454_Open(cself, false)) != 0 ) {
        LOGERR(klogInt, rc, "failed to create table");
        return rc;
    }
    if( flow_chars && flow_chars->len != 0 ) {
        if( cself->flows.len != 0 && pstring_cmp(&cself->flows, flow_chars) != 0 ) {
            PLOGMSG(klogWarn, (klogWarn, "file FLOW_SEQUENCE do not match experiment: $(s)", PLOG_S(s), flow_chars->data));
        }
        /* update flow_count so signal data length will match below */
        ((SRAWriter454*)cself)->flow_count = flow_chars->len;
        COL_WRITE_DEFAULT(cself->ci_flowc, "FLOW_CHARS", (*flow_chars));
    }
    if( key_sequence && key_sequence->len != 0 ) {
        if( cself->keys.len != 0 && pstring_cmp(&cself->keys, key_sequence) != 0 ) {
            PLOGMSG(klogWarn, (klogWarn, "file KEY_SEQUENCE do not match experiment: $(s)", PLOG_S(s), key_sequence->data));
        }
        COL_WRITE_DEFAULT(cself->ci_keys, "KEY_SEQUENCE", (*key_sequence));
    }
    return 0;
}

rc_t SRAWriter454_WriteRead(const SRAWriter454* cself, const SRALoaderFile* data_block_ref,
                            const pstring* name, const pstring* sequence, const pstring* quality,
                            const pstring* signal, const pstring* position,
                            INSDC_coord_one q_left, INSDC_coord_one q_right,
                            INSDC_coord_one a_left, INSDC_coord_one a_right)
{
    rc_t rc = 0;
    SRAWriter454* self =(SRAWriter454*)cself;
    
    assert(self != NULL);
    assert(data_block_ref != NULL);

    DEBUG_MSG(3, ("spot name '%s'\n", name ? name->data : NULL));

    if( (rc = SRAWriter454_Open(cself, signal || position)) != 0 ) {
        LOGERR(klogInt, rc, "failed to open table");
        return rc;
    }
    if( !name || name->len == 0 ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcEmpty);
        LOGERR(klogErr, rc, "spot name");
    }
    if( !sequence || sequence->len == 0 ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcEmpty);
        LOGERR(klogErr, rc, "sequence");
    } else if( !quality || sequence->len != quality->len ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, quality ? rcInconsistent : rcEmpty);
        LOGERR(klogErr, rc, "quality");
    }
    if( signal && signal->len / self->flow_count != sizeof(uint16_t) ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
        LOGERR(klogErr, rc, "signal and flow chars");
    }
    if( position && position->len / sequence->len != sizeof(INSDC_position_one) ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
        LOGERR(klogErr, rc, "sequence and position");
    }
    if( q_left < 0 || (sequence && q_left > sequence->len) ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
        LOGERR(klogErr, rc, "clip quality left and sequence");
    }
    if( q_right < 0 || (sequence && q_right > sequence->len) ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
        LOGERR(klogErr, rc, "clip quality right and sequence");
    }
    if( a_left < 0 || (sequence && a_left > sequence->len) ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
        LOGERR(klogErr, rc, "clip adapter left and sequence");
    }
    if( a_right < 0 || (sequence && a_right > sequence->len) ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
        LOGERR(klogErr, rc, "clip adapter right and sequence");
    }
    if( rc != 0 || (rc = SRAWriter_NewSpot(self->base, NULL)) != 0 ) {
        return rc;
    }
    COL_WRITE(self->ci_name, "NAME", (*name));
    COL_WRITE(self->ci_read, "READ", (*sequence));
    COL_WRITE(self->ci_qual, "QUALITY", (*quality));
    COL_WRITE(self->ci_signal, "SIGNAL", (*signal));
    COL_WRITE(self->ci_position, "POSITION", (*position));

    if( (rc = SRAWriter_WriteIdxColumn(self->base, self->ci_q_left, &q_left, sizeof(q_left), false)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=CLIP_QUALITY_LEFT"));
        return rc;
    }
    if( (rc = SRAWriter_WriteIdxColumn(self->base, self->ci_q_right, &q_right, sizeof(q_right), false)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=CLIP_QUALITY_RIGHT"));
        return rc;
    }
    if( (rc = SRAWriter_WriteIdxColumn(self->base, self->ci_a_left, &a_left, sizeof(a_left), false)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=CLIP_ADAPTER_LEFT"));
        return rc;
    }
    if( (rc = SRAWriter_WriteIdxColumn(self->base, self->ci_a_right, &a_right, sizeof(a_right), false)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=CLIP_ADAPTER_RIGHT"));
        return rc;
    }
    if( (rc = SRAWriter_WriteSpotSegment(self->base, data_block_ref, NULL, sequence->len, sequence->data)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=SPOT_GROUP/READ_SEG"));
        return rc;
    }
    return SRAWriter_CloseSpot(self->base);
}
