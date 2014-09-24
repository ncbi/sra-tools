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
#include <sra/wsradb.h>
#include <sra/types.h>

#include "debug.h"
#include "writer-absolid.h"
#include "sra-writer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

const int AbisolidReadType2ReadNumber [] = {0, 0, 0, 1, 1, 1, 1, 1, 1, 1};
const char *AbisolidReadType2ReadLabel [] = {"", "", "F3", "R3", "F5-P2", "F5-BC", "F5-RNA", "F5-DNA", "F3-DNA", "BC"};

void AbsolidRead_Init(AbsolidRead* read)
{
    if( read ) {
        pstring_clear(&read->label);
        read->cs_key = 0;
        pstring_clear(&read->seq);
        pstring_clear(&read->qual);
        read->fs_type = eAbisolidFSignalType_NotSet;
        pstring_clear(&read->fxx);
        pstring_clear(&read->cy3);
        pstring_clear(&read->txr);
        pstring_clear(&read->cy5);
        read->filter = 0;
    }
}

EAbisolidReadType AbsolidRead_Suffix2ReadType(const char* s)
{
    EAbisolidReadType type;
    size_t len;

    assert(s != NULL);

    len = strlen(s);
    if( len == 0 ) {
        type = eAbisolidReadType_SPOT;
    } else if( len > 1 && strcmp(&s[len - 2], "F3") == 0 ) {
        type = eAbisolidReadType_F3;
    } else if( len > 1 && strcmp(&s[len - 2], "R3") == 0 ) {
        type = eAbisolidReadType_R3;
    } else if( len > 4 && strcmp(&s[len - 5], "F5-P2") == 0 ) {
        type = eAbisolidReadType_F5_P2;
    } else if( len > 4 && strcmp(&s[len - 5], "F5-BC") == 0 ) {
        type = eAbisolidReadType_F5_BC;
    } else if( len > 5 && strcmp(&s[len - 6], "F5-RNA") == 0 ) {
        type = eAbisolidReadType_F5_RNA;
    } else if( len > 5 && strcmp(&s[len - 6], "F5-DNA") == 0 ) {
        type = eAbisolidReadType_F5_DNA;
    } else if( len > 5 && strcmp(&s[len - 6], "F3-DNA") == 0 ) {
        type = eAbisolidReadType_F3_DNA;
    } else if( len > 1 && strcmp(&s[len - 2], "BC") == 0 ) {
        type = eAbisolidReadType_BC;
    } else {
        int i;
        for(i = 0; i < len; i++) {
            if( !isdigit(s[i]) ) {
                break;
            }
        }
        type = i < len ? eAbisolidReadType_Unknown : eAbisolidReadType_SPOT;
    }
    return type;
}

struct SRAWriteAbsolid {
    SRAWriter* base;
    const PlatformXML* platform;
    uint8_t nreads;
    SRASegment read_seg[ABSOLID_FMT_MAX_NUM_READS];

    uint32_t ci_name;
    uint32_t ci_cs_key;
    uint32_t ci_label;
    uint32_t ci_label_start;
    uint32_t ci_label_len;
    uint32_t ci_csread;
    uint32_t ci_qual;
    uint32_t ci_fxx;
    uint32_t ci_cy3;
    uint32_t ci_txr;
    uint32_t ci_cy5;
    uint32_t ci_read_filter;
};

rc_t SRAWriteAbsolid_Make(const SRAWriteAbsolid** cself, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SRAWriteAbsolid* self;
    const PlatformXML* platform;
    const ReadSpecXML_read* read;
    uint32_t sequence_length;
    uint8_t nreads;

    if( cself == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    if( (rc = Experiment_GetPlatform(config->experiment, &platform)) != 0 ||
        (rc = Experiment_GetSpotLength(config->experiment, &sequence_length)) != 0 ||
        (rc = Experiment_GetReadNumber(config->experiment, &nreads)) != 0 ) {
        return rc;
    }
    if( platform->id != SRA_PLATFORM_ABSOLID ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
        LOGERR(klogErr, rc, "platform type");
        return rc;
    }
    if( nreads > ABSOLID_FMT_MAX_NUM_READS ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcUnsupported);
        PLOGERR(klogErr, (klogErr, rc, "expected up to $(max) reads", "max=%u", ABSOLID_FMT_MAX_NUM_READS));
        return rc;
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        return rc;
    }
    if( (rc = SRAWriter_Make(&self->base, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize base writer");
    } else if( (rc = Experiment_GetRead(config->experiment, 0, &read)) != 0 ) {
        LOGERR(klogInt, rc, "failed to get read 1 descriptor");
    } else if( read->coord_type != rdsp_RelativeOrder_ct && read->coord_type != rdsp_BaseCoord_ct ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInvalid);
        LOGERR(klogErr, rc, "1st read can be BASE_COORD or RELATIVE_ORDER only");
    } else if( read->read_type != rdsp_Forward_rt && read->read_type != rdsp_Reverse_rt ) {
        LOGMSG(klogWarn, "expected 1st read to be of type 'Forward' or 'Reverse'");
    }
    self->read_seg[0].start = 0;
    self->read_seg[0].len = sequence_length;

    if( rc == 0 && nreads >= ABSOLID_FMT_MAX_NUM_READS ) {
        if( nreads > ABSOLID_FMT_MAX_NUM_READS ) {
            rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcUnsupported);
            LOGERR(klogErr, rc, "more than 2 reads");
        } else if( (rc = Experiment_GetRead(config->experiment, 1, &read)) != 0 ) {
            LOGERR(klogErr, rc, "failed to get read 2 descriptor");
        } else if( read->coord_type != rdsp_BaseCoord_ct ) {
            rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInvalid);
            LOGERR(klogErr, rc, "2nd read can be BASE_COORD only");
        } else if( read->read_type != rdsp_Forward_rt && read->read_type != rdsp_Reverse_rt ) {
            LOGMSG(klogWarn, "expected 2ndt read to be of type 'Forward' or 'Reverse'");
        }
		if( rc == 0 ) {
			self->read_seg[1].start = read->coord.start_coord - 1;
			self->read_seg[1].len = sequence_length - self->read_seg[1].start;
			self->read_seg[0].len = self->read_seg[1].start;
		}
    }
#if _DEBUGGING
    DEBUG_MSG(3, ("%s READ_SEG[%hu,%hu]\n", __func__, self->read_seg[0].start, self->read_seg[0].len));
    if( nreads > 1 ) {
        DEBUG_MSG(3, ("%s READ_SEG[%hu,%hu]\n", __func__, self->read_seg[1].start, self->read_seg[1].len));
    }
#endif
    if( rc == 0 ) {
        self->platform = platform;
        self->nreads = nreads;
        *cself = self;
    } else {
        SRAWriteAbsolid_Whack(self, NULL);
    }
    return rc;
}

rc_t SRAWriteAbsolid_MakeName(const pstring* prefix, const pstring* suffix, pstring* name)
{
    rc_t rc = 0;
    if( prefix == NULL || name == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcParam, rcNull);
    } else if( (rc = pstring_copy(name, prefix)) == 0 ) {
        if( suffix && suffix->len > 0 ) {
            if( name->len > 0 && name->data[name->len - 1] != '_' && suffix->data[0] != '_' ) {
                rc = pstring_append(name, "_", 1);
            }
            if( rc == 0 ) {
                pstring_concat(name, suffix);
            }
        }
    }
    if( rc != 0 ) {
        LOGERR(klogErr, rc, "preparing spot name");
    }
    return rc;
}

void SRAWriteAbsolid_Whack(const SRAWriteAbsolid* cself, SRATable** table)
{
    if( cself != NULL ) {
        SRAWriteAbsolid* self = (SRAWriteAbsolid*)cself;
        SRAWriter_Whack(self->base, table);
        free(self);
    }
}

typedef struct AbsolidSpot_struct {
    const pstring* spot_name;
    uint32_t nreads;
    pstring label;
    SRASegment read_seg[ABSOLID_FMT_MAX_NUM_READS];
    INSDC_coord_zero label_start[ABSOLID_FMT_MAX_NUM_READS];
    uint32_t label_len[ABSOLID_FMT_MAX_NUM_READS];
    char cs_key[ABSOLID_FMT_MAX_NUM_READS];
    pstring seq;
    pstring qual;
    EAbisolidFSignalType fs_type;
    pstring fxx;
    pstring cy3;
    pstring txr;
    pstring cy5;
    uint8_t filter[ABSOLID_FMT_MAX_NUM_READS];
} AbsolidSpot;

static
void AbsolidSpot_Init(AbsolidSpot* spot, const pstring* spot_name,
                      const uint16_t nreads, const SRASegment* read_seg)
{
    assert(spot != NULL);
    assert(spot_name != NULL);
    assert(nreads != 0);

    spot->spot_name = spot_name;
    pstring_clear(&spot->label);
    memcpy(spot->read_seg, read_seg, sizeof(spot->read_seg[0]) * nreads);
    memset(spot->label_start, 0, sizeof(spot->label_start[0]) * nreads);
    memset(spot->label_len, 0, sizeof(spot->label_len[0]) * nreads);
    memset(spot->cs_key, 0, sizeof(spot->cs_key[0]) * nreads);
    pstring_clear(&spot->seq);
    pstring_clear(&spot->qual);
    spot->fs_type = eAbisolidFSignalType_NotSet;
    pstring_clear(&spot->fxx);
    pstring_clear(&spot->cy3);
    pstring_clear(&spot->txr);
    pstring_clear(&spot->cy5);
    memset(spot->filter, 0, sizeof(spot->filter[0]) * nreads);
}

static
rc_t  AbsolidSpot_AddRead(AbsolidSpot* spot, const int id, const AbsolidRead* read)
{
    rc_t rc = 0;
    const char* what = NULL;

    if( read == NULL ) {
        if( id == 0 ) {
            spot->read_seg[id].start = 0;
            spot->label_start[id] = 0;
        } else {
            spot->read_seg[id].start = spot->read_seg[id - 1].start + spot->read_seg[id - 1].len;
            spot->label_start[id] = spot->label_start[id - 1] + spot->label_len[id - 1];
        }
        spot->read_seg[id].len = 0;
        spot->label_len[id] = 0;
        spot->cs_key[id] = (id % 2) ? 'G' : 'T';
        spot->filter[id] = SRA_READ_FILTER_REJECT;
    } else {
        if( read->seq.len != spot->read_seg[id].len ) {
            rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
            what = "length do not match experiment";
        } else if( read->qual.len != read->seq.len ) {
            rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
            what = "quality length";
        } else if( spot->fs_type != eAbisolidFSignalType_NotSet && spot->fs_type != read->fs_type ) {
            rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInconsistent);
            what = "signal structure";
        } else if( read->fs_type != eAbisolidFSignalType_NotSet ) {
            if( read->fxx.len / read->seq.len != sizeof(float) ) {
                rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
                what = read->fs_type == eAbisolidFSignalType_FTC ? "signal FTC length" : "signal FAM length";
            } else if( read->cy3.len / read->seq.len != sizeof(float) ) {
                rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
                what = "signal CY3 length";
            } else if( read->txr.len / read->seq.len != sizeof(float) ) {
                rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
                what = "signal TXR length";
            } else if( read->cy5.len / read->seq.len != sizeof(float) ) {
                rc = RC(rcSRA, rcFormatter, rcWriting, rcData, rcInconsistent);
                what = "signal CY5 length";
            }
        }
        if( rc == 0 ) {
            if( (rc = pstring_concat(&spot->seq, &read->seq)) == 0 &&
                (rc = pstring_concat(&spot->qual, &read->qual)) == 0 &&
                (rc = pstring_concat(&spot->label, &read->label)) == 0 ) {
                if( id == 0 ) {
                    spot->read_seg[id].start = 0;
                    spot->label_start[id] = 0;
                } else {
                    spot->read_seg[id].start = spot->read_seg[id - 1].start + spot->read_seg[id - 1].len;
                    spot->label_start[id] = spot->label_start[id - 1] + spot->label_len[id - 1];
                }
                spot->label_len[id] = read->label.len;
                spot->cs_key[id] = read->cs_key;
                spot->filter[id] = read->filter;
                if( read->fs_type != eAbisolidFSignalType_NotSet &&
                    (rc = pstring_concat(&spot->fxx, &read->fxx)) == 0 &&
                    (rc = pstring_concat(&spot->cy3, &read->cy3)) == 0 &&
                    (rc = pstring_concat(&spot->txr, &read->txr)) == 0 &&
                    (rc = pstring_concat(&spot->cy5, &read->cy5)) == 0 &&
                    id == 0 ) {
                    spot->fs_type = read->fs_type;
                }
            }
        }
    }
    if( rc != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "read '$(label)' $(problem) in spot '$(spot_name)'",
                "label=%.*s,problem=%s,spot_name=%s", read->label.len, read->label.data, what, spot->spot_name->data));
    }
    return rc;
}

rc_t SRAWriteAbsolid_Write(const SRAWriteAbsolid* cself, const SRALoaderFile* file, 
                           const pstring* spot_name, const pstring* spot_group,
                           const AbsolidRead* f3, const AbsolidRead* r3)
{
    rc_t rc = 0;
    uint32_t nreads = ((f3 ? 1 : 0) + (r3 ? 1 : 0));
    AbsolidSpot spot;

    assert(cself != NULL);
    assert(sizeof(cself->read_seg) == sizeof(spot.read_seg));
    assert(file != NULL);
    assert(spot_name);

    DEBUG_MSG(3, ("spot name '%s', group '%s'\n", spot_name->data, spot_group ? spot_group->data : NULL));

    if( nreads == 0 || nreads > cself->nreads ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcExcessive);
        PLOGERR(klogErr, (klogErr, rc, "read count do not match experiment in spot $(spot): $(c)",
                "spot=%s,c=%u", spot_name->data, nreads));
        return rc;
    }

    AbsolidSpot_Init(&spot, spot_name, cself->nreads, cself->read_seg);
    if( cself->nreads == 1 ) {
        rc = AbsolidSpot_AddRead(&spot, 0, f3 ? f3 : r3);
    } else if( cself->nreads == 2 ) {
        rc = AbsolidSpot_AddRead(&spot, 0, f3);
        rc = rc ? rc : AbsolidSpot_AddRead(&spot, 1, r3);
    } else {
        rc = RC(rcSRA, rcFormatter, rcReading, rcFunction, rcCorrupt);
        PLOGERR(klogInt, (klogInt, rc, "cannot handle reads: $(n) > 2!!!", "n=%hu", cself->nreads));
        return rc;
    }
    if( rc == 0 && cself->ci_name == 0 ) {
        SRAWriteAbsolid* self =(SRAWriteAbsolid*)cself;
        if( (rc = SRAWriter_CreateTable(cself->base, "NCBI:SRA:ABI:tbl:v2")) != 0 ) {
            return rc;
        }
        if( (rc = SRAWriter_WriteDefaults(cself->base)) != 0 ) {
            LOGERR(klogInt, rc, "failed to write table defaults");
            return rc;
        }
        COL_OPEN(self->ci_name, "NAME", vdb_ascii_t);
        COL_OPEN(self->ci_cs_key, "CS_KEY", insdc_fasta_t);
        COL_OPEN(self->ci_csread, "CSREAD", insdc_csfasta_t);
        COL_OPEN(self->ci_qual, "QUALITY", insdc_phred_t);
        COL_OPEN(self->ci_read_filter, "READ_FILTER", sra_read_filter_t);

        if( spot.label.len != 0 ) {
            COL_OPEN(self->ci_label, "LABEL", vdb_ascii_t);
            COL_OPEN(self->ci_label_start, "LABEL_START", "INSDC:coord:zero");
            COL_OPEN(self->ci_label_len, "LABEL_LEN", "U32");
        }
        if( spot.fs_type != eAbisolidFSignalType_NotSet ) {
            if( spot.fs_type == eAbisolidFSignalType_FTC ) {
                COL_OPEN(self->ci_fxx, "FTC", ncbi_fsamp1_t);
            } else {
                COL_OPEN(self->ci_fxx, "FAM", ncbi_fsamp1_t);
            }
            COL_OPEN(self->ci_cy3, "CY3", ncbi_fsamp1_t);
            COL_OPEN(self->ci_txr, "TXR", ncbi_fsamp1_t);
            COL_OPEN(self->ci_cy5, "CY5", ncbi_fsamp1_t);
        }
    }
    if( rc != 0 || (rc = SRAWriter_NewSpot(cself->base, NULL)) != 0 ) {
        return rc;
    }
#if _DEBUGGING
    {{
        uint32_t i;

        DEBUG_MSG(13, ("NAME: '%.*s'\n", spot.spot_name->len, spot.spot_name->data));
        DEBUG_MSG(13, ("CSREAD: len:%u %.*s\n", spot.seq.len, spot.seq.len, spot.seq.data));
        DEBUG_MSG(13, ("QUAL: len:%u", spot.qual.len));
        for(i = 0; i < spot.qual.len; i++ ) {
            DEBUG_MSG(13, (" %i", spot.qual.data[i]));
        }
        DEBUG_MSG(13, ("\nLABEL: '%.*s'\n", spot.label.len, spot.label.data));
        for(i = 0; i < cself->nreads; i++) {
            DEBUG_MSG(13, ("READ_%02u ", i));
            DEBUG_MSG(13, ("READ_SEG: [%2u,%2u] ", spot.read_seg[i].start, spot.read_seg[i].len));
            DEBUG_MSG(13, ("LABEL_SEG: [%2u,%2u] ", spot.label_start[i], spot.label_len[i]));
            DEBUG_MSG(13, ("CS_KEY: %c ", spot.cs_key[i]));
            DEBUG_MSG(13, ("READ_FLT: %s", spot.filter[i] == SRA_READ_FILTER_PASS ? "PASS" : "REJECT"));
            DEBUG_MSG(13, ("\n"));
        }
        if( spot.fs_type != eAbisolidFSignalType_NotSet ) {
            float* f;
            DEBUG_MSG(13, ("FXX len:%u", spot.fxx.len));
            f = (float*)spot.fxx.data;
            for(i = 0; i < spot.fxx.len / sizeof(*f); i++) {
                DEBUG_MSG(13, (" %f", f[i]));
            }
            DEBUG_MSG(13, ("\nCY3: len%u:", spot.cy3.len));
            f = (float*)spot.cy3.data;
            for(i = 0; i < spot.cy3.len / sizeof(*f); i++) {
                DEBUG_MSG(13, (" %f", f[i]));
            }
            DEBUG_MSG(13, ("\nTXR: len:%u:", spot.txr.len));
            f = (float*)spot.txr.data;
            for(i = 0; i < spot.txr.len / sizeof(*f); i++) {
                DEBUG_MSG(13, (" %f", f[i]));
            }
            DEBUG_MSG(13, ("\nCY5: len:%u", spot.cy5.len));
            f = (float*)spot.cy5.data;
            for(i = 0; i < spot.cy5.len / sizeof(*f); i++) {
                DEBUG_MSG(13, (" %f", f[i]));
            }
            DEBUG_MSG(13, ("\n"));
        }
    }}
#endif
    /* this column is the one that is most likely to fail as it is required to be unique
     * so we put it first so that we can still close the spot if it fails
     */
    COL_WRITE(cself->ci_name, "NAME", (*spot_name));
    COL_WRITE(cself->ci_csread, "CSREAD", spot.seq);
    COL_WRITE(cself->ci_qual, "QUALITY", spot.qual);
    if( cself->ci_fxx != 0 ) {
        if( spot.fs_type == eAbisolidFSignalType_FTC ) {
            COL_WRITE(cself->ci_fxx, "FTC", spot.fxx);
        } else {
            COL_WRITE(cself->ci_fxx, "FAM", spot.fxx);
        }
        COL_WRITE(cself->ci_cy3, "CY3", spot.cy3);
        COL_WRITE(cself->ci_txr, "TXR", spot.txr);
        COL_WRITE(cself->ci_cy5, "CY5", spot.cy5);
    }
    if( (rc = SRAWriter_WriteSpotSegmentSimple(cself->base, file, spot_group ? spot_group->data : NULL, spot.read_seg)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=SPOT_GROUP/READ_SEG"));
        return rc;
    }
    if( cself->ci_label != 0 ) {
        COL_WRITE(cself->ci_label, "LABEL", spot.label);
        rc = SRAWriter_WriteIdxColumn(cself->base, cself->ci_label_start, spot.label_start, cself->nreads * sizeof(spot.label_start[0]), false);
        if(rc) {
            PLOGERR(klogErr, (klogErr, rc, "failed to write '$(column)' column", "column=LABEL_START"));
            return rc;
        }
        rc = SRAWriter_WriteIdxColumn(cself->base, cself->ci_label_len, spot.label_len, cself->nreads * sizeof(spot.label_len[0]), false);
        if(rc) {
            PLOGERR(klogErr, (klogErr, rc, "failed to write '$(column)' column", "column=LABEL_LEN"));
            return rc;
        }
    }

    rc = SRAWriter_WriteIdxColumn(cself->base, cself->ci_cs_key, spot.cs_key, cself->nreads * sizeof(spot.cs_key[0]), false);
    if(rc) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write '$(column)' column", "column=CS_KEY"));
        return rc;
    }
    rc = SRAWriter_WriteIdxColumn(cself->base, cself->ci_read_filter, spot.filter, cself->nreads * sizeof(spot.filter[0]), false);
    if(rc) {
        PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=READ_FILTER"));
        return rc;
    }
    return SRAWriter_CloseSpot(cself->base);
}
