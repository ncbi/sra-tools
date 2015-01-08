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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "debug.h"
#include "writer-illumina.h"
#include "sra-writer.h"

void IlluminaRead_Init(IlluminaRead* read, bool name_only)
{
    assert(read != NULL);
    read->read_id = ILLUMINAWRITER_READID_NONE;
    if( !name_only ) {
        pstring_clear(&read->seq);
        read->qual_type = ILLUMINAWRITER_COLMASK_NOTSET;
        pstring_clear(&read->qual);
        pstring_clear(&read->noise);
        pstring_clear(&read->intensity);
        pstring_clear(&read->signal);
        read->filter = SRA_READ_FILTER_PASS;
    }
}

void IlluminaSpot_Init(IlluminaSpot* spot)
{
    memset(spot, 0, sizeof(*spot));
    spot->qual_type = ILLUMINAWRITER_COLMASK_NOTSET;
}

static
rc_t IlluminaSpot_Set(IlluminaSpot* spot, int r, const pstring* name, const pstring* barcode, const IlluminaRead* read)
{
    rc_t rc = 0;
    if( r >= ILLUMINAWRITER_MAX_NUM_READS ) {
        rc = RC(rcSRA, rcFormatter, rcCopying, rcData, rcOutofrange);
    } else {
        DEBUG_MSG(3, ("starting spot '%s' bc:'%s'\n", name->data, barcode->data));
        spot->name = name;
        spot->barcode = barcode;
        spot->reads[r].read_id = read->read_id;
        spot->reads[r].seq = read->seq.len > 0 ? &read->seq : NULL;
        if( (read->qual_type == ILLUMINAWRITER_COLMASK_QUALITY_PHRED) ||
            (read->qual_type == ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS1) ||
            (read->qual_type == ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4) ) {
            spot->qual_type = read->qual_type;
            spot->reads[r].qual = &read->qual;
        } else if( read->qual_type != ILLUMINAWRITER_COLMASK_NOTSET ) {
            rc = RC(rcSRA, rcFormatter, rcCopying, rcRange, rcUnrecognized);
        }
        spot->reads[r].noise = read->noise.len > 0 ? &read->noise : NULL;
        spot->reads[r].intensity = read->intensity.len > 0 ? &read->intensity : NULL;
        spot->reads[r].signal = read->signal.len > 0 ? &read->signal : NULL;
        spot->reads[r].filter = &read->filter;
    }
    return rc;
}

static
rc_t IlluminaSpot_Append(IlluminaSpot* spot, int r, const pstring* barcode, const IlluminaRead* read, const char** field)
{
    rc_t rc = 0;
    /* spot->name and spot->reads[r].read_id assumed to be matched with read before call!!! */
    if( r >= ILLUMINAWRITER_MAX_NUM_READS ) {
        *field = "number of reads";
        return RC(rcSRA, rcFormatter, rcCopying, rcData, rcOutofrange);
    }
    if( (spot->barcode == NULL && barcode != NULL) ||
        (spot->barcode != NULL && barcode == NULL) ||
        (spot->barcode != barcode && pstring_cmp(spot->barcode, barcode) != 0) ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInconsistent);
        *field = "barcode";
        return rc;
    }
    if( read->seq.len > 0 ) {
        if( spot->reads[r].seq ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
            *field = "sequence";
            return rc;
        } else {
            spot->reads[r].seq = &read->seq;
        }
    }
    if( read->qual_type != ILLUMINAWRITER_COLMASK_NOTSET ) {
        if( spot->reads[r].qual ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
            *field = "quality";
            return rc;
        }
        if( spot->qual_type != ILLUMINAWRITER_COLMASK_NOTSET && spot->qual_type != read->qual_type ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInconsistent);
            *field = "quality type";
            return rc;
        }
        spot->qual_type = read->qual_type;
        spot->reads[r].qual = &read->qual;
    }

    if( read->noise.len > 0 ) {
        if( spot->reads[r].noise ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
            *field = "noise";
            return rc;
        } else {
            spot->reads[r].noise = &read->noise;
        }
    }
    if( read->intensity.len > 0 ) {
        if( spot->reads[r].intensity ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
            *field = "intensity";
            return rc;
        } else {
            spot->reads[r].intensity = &read->intensity;
        }
    }
    if( read->signal.len > 0 ) {
        if( spot->reads[r].signal ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
            *field = "signal";
            return rc;
        } else {
            spot->reads[r].signal = &read->signal;
        }
    }
    if( spot->reads[r].filter != NULL ) {
        if( *(spot->reads[r].filter) != read->filter ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInconsistent);
            *field = "read_filter";
            return rc;
        }
    } else {
        spot->reads[r].filter = &read->filter;
    }
    return rc;
}

rc_t IlluminaSpot_Add(IlluminaSpot* spot, const pstring* name, const pstring* barcode, const IlluminaRead* read)
{
    rc_t rc = 0;

    if( spot->nreads == 0 ) {
        rc = IlluminaSpot_Set(spot, spot->nreads++, name, barcode, read);
    } else if( pstring_cmp(spot->name, name) == 0 ) {
        /* look if same read_id was already seen in this spot */
        int32_t k;
        for(k = 0; k < spot->nreads; k++) {
            if( spot->reads[k].read_id == read->read_id ) {
                const char* field;
                rc = IlluminaSpot_Append(spot, k, barcode, read, &field);
                if( GetRCState(rc) == rcDuplicate && read->read_id == ILLUMINAWRITER_READID_NONE ) {
                    /* may be it is the case when readids are missing on defline and these are separate reads */
                    k = spot->nreads + 1;
                    rc = 0;
                } else if( rc != 0 ) {
                    PLOGERR(klogErr, (klogErr, rc, "$(field) for spot '$(s)'", PLOG_2(PLOG_S(field),PLOG_S(s)), field, spot->name->data));
                }
                break;
            }
        }
        if( rc == 0 && k >= spot->nreads ) {
            /* read was not found, adddind new read to this spot */
            rc = IlluminaSpot_Set(spot, spot->nreads++, name, barcode, read);
        }
    } else {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcIgnored);
    }
    return rc;
}

struct SRAWriterIllumina {
    SRAWriter* base;
    const SRALoaderConfig* config;
    const PlatformXML* platform;
    uint8_t nreads;

    SRASegment read_seg[ILLUMINAWRITER_MAX_NUM_READS];
    int16_t barcode_read_id;
    bool fixed_read_seg;
    const ReadSpecXML_read* last_read;
    uint32_t sequence_length;
    
    uint16_t col_mask;
    uint32_t ci_name;
    uint32_t ci_read;
    uint32_t ci_qual;
    uint32_t ci_signal;
    uint32_t ci_noise;
    uint32_t ci_intensity;
    uint32_t ci_read_filter;

};

rc_t SRAWriterIllumina_Make(const SRAWriterIllumina** cself, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SRAWriterIllumina* self;
    const PlatformXML* platform;
    const ReadSpecXML_read* last_read;
    int32_t spot_len;
    uint32_t sequence_length;
    uint8_t nreads;

    if( cself == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    if( (rc = Experiment_GetPlatform(config->experiment, &platform)) != 0 ||
        (rc = Experiment_GetReadNumber(config->experiment, &nreads)) != 0 ||
        (rc = Experiment_GetSpotLength(config->experiment, &sequence_length)) != 0 ||
        (rc = Experiment_GetRead(config->experiment, nreads - 1, &last_read)) != 0 ) {
        return rc;
    }
    if( platform->id != SRA_PLATFORM_ILLUMINA ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInvalid);
        LOGERR(klogErr, rc, "platform type");
        return rc;
    }
    if( nreads > ILLUMINAWRITER_MAX_NUM_READS ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcUnsupported);
        PLOGERR(klogErr, (klogErr, rc, "more than $(max) reads", PLOG_U8(max), (uint8_t)ILLUMINAWRITER_MAX_NUM_READS));
        return rc;
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        return rc;
    }
    if( (rc = SRAWriter_Make(&self->base, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize base writer");
    }
    self->config = config;
    self->platform = platform;
    self->nreads = nreads;
    self->barcode_read_id = ILLUMINAWRITER_READID_NONE;
    self->last_read = last_read;
    self->fixed_read_seg = true;
    self->col_mask = ILLUMINAWRITER_COLMASK_NOTSET;
    self->sequence_length = sequence_length;
    spot_len = sequence_length;

    do {
        const ReadSpecXML_read* read_spec;
        int16_t len = 0;
        --nreads;
        if( (rc = Experiment_GetRead(config->experiment, nreads, &read_spec)) != 0 ) {
            break;
        }
        if( read_spec->read_type == rdsp_BarCode_rt ) {
            if( self->barcode_read_id == ILLUMINAWRITER_READID_NONE ) {
                self->barcode_read_id = nreads;
            } else {
                rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcDuplicate);
                LOGERR(klogErr, rc, "only on BarCode READ_TYPE per spot supported");
                break;
            }
        }
        if( self->fixed_read_seg ) {
            switch(read_spec->coord_type) {
                case rdsp_BaseCoord_ct:
                case rdsp_CycleCoord_ct:
                    len = spot_len - read_spec->coord.start_coord + 1;
                    break;
                case rdsp_ExpectedBaseCall_ct:
                case rdsp_ExpectedBaseCallTable_ct:
                    if( read_spec->coord.expected_basecalls.default_length > 0 ) {
                        len = read_spec->coord.expected_basecalls.default_length;
                    } else {
                        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcUnsupported);
                    }
                    break;
                case rdsp_RelativeOrder_ct:
                    if( nreads == 0 ) {
                        len = spot_len - 1 + 1; /* as if BASE_COORD == 1 */
                        break;
                    }
                default:
                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcUnsupported);
            }
            if( rc == 0 ) {
                spot_len -= len;
                if( spot_len < 0 || len <= 0 ) {
                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInconsistent);
                    LOGERR(klogErr, rc, "SPOT_DECODE_SPEC and SEQUENCE_LENGTH");
                } else {
                    SRASegment* seg = &self->read_seg[nreads];
                    seg->start = spot_len;
                    seg->len = len;
                    DEBUG_MSG(3, ("#%u read fixed length = %i\n", nreads, len));
                }
            } else if( GetRCState(rc) == rcUnsupported ) {
                self->fixed_read_seg = false;
                DEBUG_MSG(3, ("not fixed spot segmentation"));
                rc = 0;
            }
        }
    } while( rc == 0 && nreads > 0 );

    if( rc == 0 ) {
        *cself = self;
    } else {
        SRAWriterIllumina_Whack(self, NULL);
    }
    return rc;
}

void SRAWriterIllumina_Whack(const SRAWriterIllumina* cself, SRATable** table)
{
    if( cself != NULL ) {
        SRAWriterIllumina* self = (SRAWriterIllumina*)cself;
        SRAWriter_Whack(self->base, table);
        free(self);
    }
}

typedef struct IlluminaWriterSpot_struct {
    pstring name;
    const char* spot_group;
    uint16_t col_mask;
    bool adjust_last_read_len;
    pstring seq;
    pstring qual;
    pstring noise;
    pstring intensity;
    pstring signal;
    SRAReadFilter filter[ILLUMINAWRITER_MAX_NUM_READS+1];
} IlluminaWriterSpot;

static
rc_t SRAWriterIllumina_Check(SRAWriterIllumina* self,
    IlluminaSpot* spot, IlluminaWriterSpot* final)
{
    rc_t rc = 0;
    bool done, inject_barcode = false;
    int32_t i, flt_qty, spot_len;
    const char* what = NULL, *member_basecall = NULL;

    assert(self != NULL);
    assert(spot != NULL);
    assert(final != NULL);

    /* reorder reads ascending by file's read_ids */
    do {
        IlluminaSpot tmp;
        done = true;
        for(i = 1; i < spot->nreads; i++) {
            if( spot->reads[i].read_id < spot->reads[i - 1].read_id ) {
                memcpy(&tmp.reads[0], &spot->reads[i], sizeof(tmp.reads[0]));
                memcpy(&spot->reads[i], &spot->reads[i - 1], sizeof(tmp.reads[0]));
                memcpy(&spot->reads[i - 1], &tmp.reads[0], sizeof(tmp.reads[0]));
                done = false;
            }
        }
    } while( !done );

    /* get total spot length in bases */
    spot_len = 0;
    for(i = 0; rc == 0 && i < spot->nreads; i++) {
        if( spot->reads[i].seq == NULL ) {
            rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcNotFound);
            what = "sequence";
        } else {
            spot_len += spot->reads[i].seq->len;
            if( spot->reads[i].qual == NULL ) {
                rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcNotFound);
                what = "quality";
            }
        }
    }
    if( rc == 0 ) {
        if( spot->name == NULL || spot->name->len < 1 ) {
            rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcEmpty);
            what = "spot name";
        } else {
            rc = pstring_copy(&final->name, spot->name);
        }
    }
    if( rc == 0 ) {
        final->spot_group = NULL;
        if( spot->barcode != NULL && spot->barcode->len > 0 ) {
            if( self->barcode_read_id != ILLUMINAWRITER_READID_NONE ) {
                rc = Experiment_FindReadInTable(self->config->experiment, self->barcode_read_id,
                                                spot->barcode->data, &member_basecall, &final->spot_group);
                if( GetRCState(rc) == rcNotFound ) {
                    /* experiment knows nothing about this barcode, so may be they assigned membership themselves, reset */
                    member_basecall = NULL;
                    final->spot_group = NULL;
                    rc = 0;
                } else if( rc != 0 ) {
                    what = "barcode";
                }
            }
            if( final->spot_group == NULL ) {
                /* reset back to file value */
                final->spot_group = spot->barcode->data;
            }
            if( member_basecall == NULL && pstring_is_fasta(spot->barcode) ) {
                /* keep original barcode from file if it is fasta */
                member_basecall = spot->barcode->data;
            }
        }
    }
    if( rc == 0 ) {
        final->adjust_last_read_len = false;
        if( self->sequence_length != spot_len ) {
            uint16_t barc_len = member_basecall ? strlen(member_basecall) : 0;
            if ((self->sequence_length != (spot_len + barc_len))

             || (self->barcode_read_id == ILLUMINAWRITER_READID_NONE))
      /* Otherwise inject_barcode is set
         and when it is inserted below, it uses read_seg[self->barcode_read_id].
         I.e. barcode_read_id should be >= 0
         while ILLUMINAWRITER_READID_NONE == -1 */

            {
                if( spot_len > self->sequence_length ) {
                    what = "spot too long";
                    rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcExcessive);
                    PLOGERR(klogErr, (klogErr, rc,
                        "cumulative length of reads data in file(s): $(l) is greater than"
                        " spot length declared in experiment: $(e) in spot '$(spot)'",
                        "l=%d,e=%u,spot=%.*s", spot_len, self->sequence_length,
                        (uint32_t)spot->name->len, spot->name->data));
                } else if( self->fixed_read_seg && (self->last_read->read_class & SRA_READ_TYPE_BIOLOGICAL) &&
                           spot_len > (self->sequence_length - self->read_seg[self->nreads - 1].len) &&
                           (self->barcode_read_id == ILLUMINAWRITER_READID_NONE ||
                            self->sequence_length - spot_len != self->read_seg[self->barcode_read_id].len) ) {
                    final->adjust_last_read_len = true;
                } else {
                    what = "spot too short";
                    rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcInconsistent);
                    PLOGERR(klogErr, (klogErr, rc,
                        "cumulative length of reads data in file(s): $(l) is less than"
                        " spot length declared in experiment: $(e), most probably $(x) is absent in spot '$(spot)'",
                        "l=%d,e=%u,spot=%.*s,x=%s", spot_len, self->sequence_length,
                        (uint32_t)spot->name->len, spot->name->data,
                        (self->barcode_read_id == ILLUMINAWRITER_READID_NONE ||
                         self->sequence_length - spot_len != self->read_seg[self->barcode_read_id].len) ? "mate-pair" : "barcode" ));
                }
            } else {
                spot_len += barc_len;
                inject_barcode = true;
                DEBUG_MSG(3, ("barcode needs to be injected into spot data\n"));
            }
        }
    }

    /* prepare seq/qual/signal/noise/intensity */
    pstring_clear(&final->seq);
    pstring_clear(&final->qual);
    pstring_clear(&final->noise);
    pstring_clear(&final->intensity);
    pstring_clear(&final->signal);
    final->col_mask = ILLUMINAWRITER_COLMASK_READ | spot->qual_type;
    for(flt_qty = i = 0; rc == 0 && i < spot->nreads; i++, flt_qty++) {
        /* presence of seq and qual data is checked above */
        if( (rc = pstring_concat(&final->seq, spot->reads[i].seq)) != 0 ) {
            what = "sequence";
            break;
        } else if( (rc = pstring_concat(&final->qual, spot->reads[i].qual)) != 0 ) {
            what = "quality";
            break;
        }
        if( spot->reads[i].signal != NULL ) {
            if( (rc = pstring_concat(&final->signal, spot->reads[i].signal)) != 0 ) {
                what = "signal";
                break;
            }
            final->col_mask |= ILLUMINAWRITER_COLMASK_SIGNAL; 
        }
        if( spot->reads[i].noise != NULL ) {
            if( (rc = pstring_concat(&final->noise, spot->reads[i].noise)) != 0 ) {
                what = "noise";
                break;
            }
            final->col_mask |= ILLUMINAWRITER_COLMASK_NOISE; 
        }
        if( spot->reads[i].intensity != NULL ) {
            if( (rc = pstring_concat(&final->intensity, spot->reads[i].intensity)) != 0 ) {
                what = "intensity";
                break;
            }
            final->col_mask |= ILLUMINAWRITER_COLMASK_INTENSITY; 
        }
        if( spot->reads[i].filter != NULL && i == 0 ) {
            final->col_mask |= ILLUMINAWRITER_COLMASK_RDFILTER; 
        } else if( ((final->col_mask & ILLUMINAWRITER_COLMASK_RDFILTER) && spot->reads[i].filter == NULL) ||
                   (!(final->col_mask & ILLUMINAWRITER_COLMASK_RDFILTER) && spot->reads[i].filter != NULL) ) {
            rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcInconsistent);
            what = "read filter";
            break;
        }
        if( spot->reads[i].filter != NULL ) {
            final->filter[flt_qty] = *(spot->reads[i].filter);
        }
    }
    if( rc == 0 &&
        (final->qual.len / final->seq.len) != ((final->col_mask & ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4) ? 4 : 1) ) {
        rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcInconsistent);
        what = "quality length vs sequence";
    }
    if( rc == 0 &&
        ((final->col_mask & ILLUMINAWRITER_COLMASK_SIGNAL) && (final->signal.len != self->sequence_length * 16)) ) {
        rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcInconsistent);
        what = "signal data length";
    }
    if( rc == 0 &&
        ((final->col_mask & ILLUMINAWRITER_COLMASK_NOISE) && (final->noise.len != self->sequence_length * 16)) ) {
        rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcInconsistent);
        what = "noise data length";
    }
    if( rc == 0 &&
        ((final->col_mask & ILLUMINAWRITER_COLMASK_INTENSITY) && (final->intensity.len != self->sequence_length * 16)) ) {
        rc = RC(rcSRA, rcFormatter, rcValidating, rcData, rcInconsistent);
        what = "intensity data length";
    }

    if( rc == 0 && inject_barcode ) {
        /* insert barcode into spot data using read_seg information */
        /* verified above to be present, to be fasta, and length as in XML */
        INSDC_coord_zero from = self->read_seg[self->barcode_read_id].start;
        INSDC_coord_len l = self->read_seg[self->barcode_read_id].len;
        INSDC_coord_len carry = final->seq.len - from;
        const uint32_t qual_x = ((final->col_mask & ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4) ? 4 : 1);
        pstring tmp;

        DEBUG_MSG(3, ("injecting barcodr as read %i [%i,%u] \n", self->barcode_read_id, from, l));
        final->seq.len = from;
        final->qual.len = from * qual_x;
        if( (rc = pstring_assign(&tmp, &final->seq.data[final->seq.len], carry)) != 0 ||
            (rc = pstring_append(&final->seq, member_basecall, l)) != 0 ||
            (rc = pstring_concat(&final->seq, &tmp)) != 0 ) {
            what = "inserting barcode into sequence";
        } else if( (rc = pstring_assign(&tmp, &final->qual.data[final->qual.len], carry * qual_x)) != 0 ||
                   (rc = pstring_append_chr(&final->qual, 1, l * qual_x)) != 0 ||
                   (rc = pstring_concat(&final->qual, &tmp)) != 0 ) {
            what = "inserting barcode into quality";
        }
        if( rc == 0 && (final->col_mask & ILLUMINAWRITER_COLMASK_SIGNAL) ) {
            final->signal.len = from * 16;
            if( (rc = pstring_assign(&tmp, &final->signal.data[final->signal.len], carry * 16)) != 0 ||
                (rc = pstring_append_chr(&final->signal, 0, l * 16)) != 0 ||
                (rc = pstring_concat(&final->signal, &tmp)) != 0 ) {
                what = "inserting barcode into signal";
            }
        }
        if( rc == 0 && (final->col_mask & ILLUMINAWRITER_COLMASK_INTENSITY) ) {
            final->intensity.len = from * 16;
            if( (rc = pstring_assign(&tmp, &final->intensity.data[final->intensity.len], carry * 16)) != 0 ||
                (rc = pstring_append_chr(&final->intensity, 0, l * 16)) != 0 ||
                (rc = pstring_concat(&final->intensity, &tmp)) != 0 ) {
                what = "inserting barcode into intensity";
            }
        }
        if( rc == 0 && (final->col_mask & ILLUMINAWRITER_COLMASK_NOISE) ) {
            final->noise.len = from * 16;
            if( (rc = pstring_assign(&tmp, &final->noise.data[final->noise.len], carry * 16)) != 0 ||
                (rc = pstring_append_chr(&final->noise, 0, l * 16)) != 0 ||
                (rc = pstring_concat(&final->noise, &tmp)) != 0 ) {
                what = "inserting barcode into noise";
            }
        }
        for(i = flt_qty - 1; rc == 0 && i >= self->barcode_read_id; i--) {
            final->filter[i + 1] = final->filter[i];
        }
        final->filter[self->barcode_read_id] = final->filter[0];
        flt_qty++;
    }
    /* propogate filter value in case number of reads in files is less than in XML */
    for(i = flt_qty; rc == 0 && i < self->nreads; i++) {
        final->filter[i] = final->filter[0];
    }

#if _DEBUGGING
    DEBUG_MSG(3, ("Ready: READ: %s\n", final->seq.data));
    DEBUG_MSG(3, ("Ready: QUAL%c:", (final->col_mask & ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4) ? '4' : '1'));
    for(i = 0; i < final->qual.len; i++ ) {
        DEBUG_MSG(3, (" %i", final->qual.data[i]));
    }
    DEBUG_MSG(3, ("\nReady: SGRUP: %s\n", final->spot_group));
    for(i = 0; i < self->nreads; i++ ) {
        DEBUG_MSG(3, ("Ready: READ_FLT[%u]: %s\n", i, final->filter[i] == SRA_READ_FILTER_PASS ? "PASS" : "REJECT"));
    }
#endif
    if( rc != 0 && rc != KLogLastErrorCode() ) {
        PLOGERR(klogErr, (klogErr, rc, "$(msg) in spot '$(spot)'", "msg=%s,spot=%.*s", what, spot->name->len, spot->name->data));
    }
    return rc;
}

rc_t SRAWriterIllumina_Write(const SRAWriterIllumina* cself, const SRALoaderFile* data_block_ref, IlluminaSpot* spot)
{
    rc_t rc = 0;
    SRAWriterIllumina* self =(SRAWriterIllumina*)cself;
    IlluminaWriterSpot final;
    
    assert(self != NULL);
    assert(data_block_ref != NULL);

    DEBUG_MSG(3, ("spot name '%s', group '%s'\n", spot->name->data, spot->barcode ? spot->barcode->data : NULL));

    if( (rc = SRAWriterIllumina_Check(self, spot, &final)) != 0 ) {
        return rc;
    }
    if( self->col_mask == ILLUMINAWRITER_COLMASK_NOTSET ) {
        const char* schema;

        if( final.col_mask & ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4 ) {
            schema = "NCBI:SRA:Illumina:tbl:q4:v2";
        } else if( final.col_mask & ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS1 ) {
            schema = "NCBI:SRA:Illumina:tbl:q1:v2";
        } else if( final.col_mask & ILLUMINAWRITER_COLMASK_QUALITY_PHRED ) {
            schema = "NCBI:SRA:Illumina:tbl:phred:v2";
        } else {
            rc = RC(rcExe, rcTable, rcCreating, rcType, rcUnknown);
            return rc;
        }
        if( (rc = SRAWriter_CreateTable(self->base, schema)) != 0 ) {
            return rc;
        }
        if( (rc = SRAWriter_WriteDefaults(self->base)) != 0 ) {
            LOGERR(klogInt, rc, "failed to write table defaults");
            return rc;
        }
        COL_OPEN(self->ci_name, "NAME", vdb_ascii_t);
        COL_OPEN(self->ci_read, "READ", insdc_fasta_t);
        if( final.col_mask & ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4 ) {
            COL_OPEN(self->ci_qual, "QUALITY", ncbi_qual4_t);
        } else if( final.col_mask & ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS1 ) {
            COL_OPEN(self->ci_qual, "QUALITY", insdc_logodds_t);
        } else if( final.col_mask & ILLUMINAWRITER_COLMASK_QUALITY_PHRED ) {
            COL_OPEN(self->ci_qual, "QUALITY", insdc_phred_t);
        }
        if( final.col_mask & ILLUMINAWRITER_COLMASK_SIGNAL ) {
            COL_OPEN(self->ci_signal, "SIGNAL", ncbi_fsamp4_t);
        }
        if( final.col_mask & ILLUMINAWRITER_COLMASK_NOISE ) {
            COL_OPEN(self->ci_noise, "NOISE", ncbi_fsamp4_t);
        }
        if( final.col_mask & ILLUMINAWRITER_COLMASK_INTENSITY ) {
            COL_OPEN(self->ci_intensity, "INTENSITY", ncbi_fsamp4_t);
        }
        if( final.col_mask & ILLUMINAWRITER_COLMASK_RDFILTER ) {
            COL_OPEN(self->ci_read_filter, "READ_FILTER", sra_read_filter_t);
        }
        self->col_mask = final.col_mask;
    }
    if( self->col_mask != final.col_mask ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInconsistent);
        LOGERR(klogInt, rc, "columns do not match to 1st spot");
        return rc;
    }
    if( (rc = SRAWriter_NewSpot(self->base, NULL)) != 0 ) {
        return rc;
    }
    COL_WRITE(self->ci_name, "NAME", final.name);
    COL_WRITE(self->ci_read, "READ", final.seq);
    COL_WRITE(self->ci_qual, "QUALITY", final.qual);
    COL_WRITE(self->ci_noise, "NOISE", final.noise);
    COL_WRITE(self->ci_intensity, "INTENSITY", final.intensity);
    COL_WRITE(self->ci_signal, "SIGNAL", final.signal);

    if( self->fixed_read_seg ) {
        uint16_t tmp = 0;
        if( final.adjust_last_read_len ) {
            tmp = cself->read_seg[cself->nreads - 1].len;
            self->read_seg[cself->nreads - 1].len -= cself->sequence_length - final.seq.len;
        }
        if( (rc = SRAWriter_WriteSpotSegmentSimple(self->base, data_block_ref, final.spot_group, cself->read_seg)) != 0 ) {
            PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=SPOT_GROUP/READ_SEG"));
            return rc;
        }
        if( final.adjust_last_read_len ) {
            self->read_seg[cself->nreads - 1].len = tmp;
        }
    } else {
        if( (rc = SRAWriter_WriteSpotSegment(self->base, data_block_ref, final.spot_group, final.seq.len, final.seq.data)) != 0 ) {
            PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=SPOT_GROUP/READ_SEG"));
            return rc;
        }
    }
    if( self->ci_read_filter != 0 ) {
        rc = SRAWriter_WriteIdxColumn(self->base, self->ci_read_filter, final.filter, self->nreads * sizeof(final.filter[0]), false);
        if(rc) {
            PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=READ_FILTER"));
            return rc;
        }
    }
    return SRAWriter_CloseSpot(self->base);
}
