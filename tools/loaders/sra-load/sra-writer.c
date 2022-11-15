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
#include <kapp/main.h>
#include <klib/rc.h>
#include <klib/status.h>
#include <klib/log.h>
#include <sra/types.h>
#include <sra/wsradb.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "debug.h"
#include "loader-fmt.h"
#include "sra-writer.h"

#define SRAWRITER_MAX_READS 256

struct SRAWriter {
    const SRALoaderConfig* config;
    int64_t spots_to_run;

    SRATable* table;
    uint8_t nreads;
    bool defaults;

    uint32_t col_spot_group_id;
    uint32_t col_read_start_id;
    uint32_t col_read_len_id;
    SRASegment read_segs[SRAWRITER_MAX_READS];
};

rc_t SRAWriter_Make( SRAWriter** self, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SRAWriter* obj = NULL;
    uint8_t nreads;

    if( self == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    if( (rc = Experiment_GetReadNumber(config->experiment, &nreads)) != 0 ) {
        return rc;
    }
    obj = calloc(1, sizeof(*obj));
    if( obj == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    }
    obj->config = config;
    obj->nreads = nreads;
    obj->spots_to_run = config->spots_to_run;
    *self = obj;
    return 0;
}

void SRAWriter_Whack(SRAWriter* self, SRATable** table)
{
    if( self != NULL ) {
        if( table != NULL ) {
            *table = self->table;
        } else {
            SRATableRelease(self->table);
        }
        free(self);
    }
}

rc_t SRAWriter_CreateTable(SRAWriter* self, const char* schema)
{
    rc_t rc = 0;

    assert(self->table == NULL);
    if( self == NULL ) {
        rc = RC( rcSRA, rcFormatter, rcWriting, rcSelf, rcNull);
    } else {
retry:
        rc = SRAMgrCreateTable(self->config->sra_mgr, &self->table, schema, "%s", self->config->table_path);
        if( GetRCObject(rc) == (enum RCObject)rcTable && GetRCState(rc) == rcExists && self->config->force_table_overwrite ) {
            if( (rc = SRAMgrDropTable(self->config->sra_mgr, true, "%s", self->config->table_path)) == 0 ) {
                goto retry;
            }
        }
    }
    if( rc != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to create table with schema $(schema)", PLOG_S(schema), schema));
    } else {
        DEBUG_MSG (7, ("Created table with schema '%s'\n", schema));
    }
    return rc;
}

rc_t SRAWriter_OpenColumnWrite(SRAWriter* self, uint32_t *idx, const char *name, const char *datatype)
{
    if( self == NULL ) {
        return RC( rcSRA, rcFormatter, rcWriting, rcSelf, rcNull);
    }
    return SRATableOpenColumnWrite(self->table, idx, NULL, name, datatype);
}

rc_t SRAWriter_NewSpot(SRAWriter* self, spotid_t *id)
{
    rc_t rc = 0;
    spotid_t s;

    if( self == NULL ) {
        rc = RC( rcSRA, rcFormatter, rcWriting, rcSelf, rcNull);
    } else {
        id = id ? id : &s;
        rc = SRATableNewSpot(self->table, id);
        if(rc == 0 ) {
            DEBUG_MSG(3, ("writing spot %lu data\n", *id));
        }
        if( (*id % 2000000) == 0 ) {
            PLOGMSG(klogInt, (klogInfo, "spot $(spot_count)", "severity=status,spot_count=%u", *id));
        }
    }
    return rc;
}

rc_t SRAWriter_WriteIdxColumn(SRAWriter* self, uint32_t idx, const void *base, uint64_t bytes, bool as_dflt)
{
    if( self == NULL ) {
        return RC( rcSRA, rcFormatter, rcWriting, rcSelf, rcNull);
    }
    if( as_dflt ) {
        return SRATableSetIdxColumnDefault(self->table, idx, base, 0, bytes * 8);
    }
    return SRATableWriteIdxColumn(self->table, idx, base, 0, bytes * 8);
}

rc_t SRAWriter_CloseSpot(SRAWriter* self)
{
    rc_t rc = 0;
    if( self == NULL ) {
        rc = RC( rcSRA, rcFormatter, rcWriting, rcSelf, rcNull);
    } else {
        if( (rc = SRATableCloseSpot(self->table)) == 0 ) {
            rc = Quitting();
            if( self->spots_to_run > 0 ) {
                self->spots_to_run--;
            }
            if( rc == 0 && self->spots_to_run == 0 ) {
                rc = RC( rcSRA, rcFormatter, rcWriting, rcTransfer, rcDone);
            }
        }
    }
    return rc;
}

/* if data is NULL no default value is written, only column is open */
static
rc_t SRAWriter_WDflt(SRAWriter* self, const char *cname, const char *type, const void *data, size_t size)
{
    rc_t rc = 0;
    uint32_t ci = 0;

    if( (rc = SRATableOpenColumnWrite(self->table, &ci, 0, cname, type)) != 0 && GetRCState(rc) != rcExists ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to open column $(column)", PLOG_S(column), cname));
    } else if( data != NULL && (rc = SRATableSetIdxColumnDefault(self->table, ci, data, 0, size * 8)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write default to column $(column)", PLOG_S(column), cname));
    } else if( strcmp(cname, "SPOT_GROUP") == 0 ) {
        self->col_spot_group_id = ci;
    } else if( strcmp(cname, "READ_START") == 0 ) {
        self->col_read_start_id = ci;
    } else if( strcmp(cname, "READ_LEN") == 0 ) {
        self->col_read_len_id = ci;
    }
    return rc;
}

rc_t SRAWriter_WriteDefaults(SRAWriter* self)
{
    rc_t rc = 0;
    uint32_t i;
    const PlatformXML* plat;
    char labels[10240] = "\0";
    INSDC_SRA_xread_type read_types[SRAWRITER_MAX_READS];
    INSDC_coord_zero label_start[SRAWRITER_MAX_READS];
    INSDC_coord_len label_len[SRAWRITER_MAX_READS];
    bool has_read_seg = false;
    INSDC_coord_zero read_start[SRAWRITER_MAX_READS];
    INSDC_coord_len read_len[SRAWRITER_MAX_READS];

    if( self == NULL ) {
        return RC( rcSRA, rcFormatter, rcWriting, rcSelf, rcNull);
    }
    if( self->defaults ) {
        return 0;
    }
    ((SRAWriter*)self)->defaults = true;

    if( (rc = Experiment_GetPlatform(self->config->experiment, &plat)) == 0 ) {
        SRASegment read_segs[SRAWRITER_MAX_READS];
        if( (rc = Experiment_ReadSegDefault(self->config->experiment, read_segs)) == 0 ) {
            has_read_seg = true;
            for (i = 0; i != self->nreads; ++i) {
                read_start[i] = read_segs[i].start;
                read_len[i] = read_segs[i].len;
            }
        } else if( GetRCObject(rc) == (enum RCObject)rcData && GetRCState(rc) == rcUnsupported ) {
            rc = 0;
        }
    }
    for(i = 0; rc == 0 && i < self->nreads; i++) {
        const ReadSpecXML_read* rspec = NULL;
        if( (rc = Experiment_GetRead(self->config->experiment, i, &rspec)) != 0 ) {
            continue;
        }
        label_start[i] = strlen(labels);
        label_len[i] = 0;
        if( rspec->read_label != NULL ) {
            label_len[i] = strlen(rspec->read_label);
            strncat(labels, rspec->read_label, label_len[i]);
        }
        read_types[i] = rspec->read_class;
    }

    if( rc == 0 ) {
        uint8_t platform = plat->id;
#if _DEBUGGING
        DEBUG_MSG (7, ("Default: %hu reads per spot, labels: '%s'\n", self->nreads, labels));
        for( i = 0; i < self->nreads; i++) {
            DEBUG_MSG (5, ("Default: Read %u. %s: Label: [%2u:%2u]",
                           i + 1, ( read_types[i] & SRA_READ_TYPE_BIOLOGICAL ) ? " Bio" : "Tech",
                           label_start[i], label_len[i]));
            if( has_read_seg ) {
                DEBUG_MSG (5, (" Seg: [%3u:%3u]", read_start[i], read_len[i]));
            }
            DEBUG_MSG (5, ("\n"));
        }
#endif
        /* TBD: SPOT_GROUP on demand, remove NREADS!! */
        if( (rc = SRAWriter_WDflt(self, "PLATFORM", sra_platform_id_t, &platform, sizeof(platform))) != 0 ||
            (rc = SRAWriter_WDflt(self, "NREADS", vdb_uint8_t, &self->nreads, sizeof(self->nreads))) != 0 ||
            (rc = SRAWriter_WDflt(self, "LABEL", vdb_ascii_t, labels, strlen(labels))) != 0 ||
            (rc = SRAWriter_WDflt(self, "LABEL_START", "INSDC:coord:zero", label_start, self->nreads * sizeof(label_start[0]))) != 0 ||
            (rc = SRAWriter_WDflt(self, "LABEL_LEN", vdb_uint32_t, label_len, self->nreads * sizeof(label_len[0]))) != 0 ||
            (rc = SRAWriter_WDflt(self, "READ_TYPE", sra_read_type_t, read_types, self->nreads * sizeof(read_types[0]))) != 0 ||
            (rc = SRAWriter_WDflt(self, "SPOT_GROUP", vdb_ascii_t, "", 0)) != 0 ||
            (rc = SRAWriter_WDflt(self, "READ_START", "INSDC:coord:zero", has_read_seg ? read_start : NULL, self->nreads * sizeof(read_start[0]))) != 0 ||
            (rc = SRAWriter_WDflt(self, "READ_LEN", vdb_uint32_t, has_read_seg ? read_len : NULL, self->nreads * sizeof(read_len[0]))) != 0 ) {
            return rc;
        }
    }
    return rc;
}

rc_t SRAWriter_WriteSpotSegment(SRAWriter* self, const SRALoaderFile* file,
                                const char* file_member_name, uint32_t nbases, const char* bases)
{
    rc_t rc = 0;

    if( self == NULL || file == NULL || bases == NULL ) {
        rc = RC( rcSRA, rcFormatter, rcMultiplexing, rcParam, rcNull);
    } else if ( self->col_spot_group_id == 0 || self->col_read_start_id == 0 || self->col_read_len_id == 0 ) {
        rc = RC( rcSRA, rcFormatter, rcMultiplexing, rcSelf, rcCorrupt);
    } else {
        const char* data_block_member_name = NULL;
        const char* assigned_member_name = NULL;

        DEBUG_MSG (5, ("file spot member_name '%s'\n", file_member_name));
        if( file_member_name != NULL &&
            (strcasecmp(file_member_name, "default") == 0 || strcasecmp(file_member_name, "0") == 0) ) {
            file_member_name = "";
        }
        rc = SRALoaderFileMemberName(file, &data_block_member_name);
        DEBUG_MSG (5, ("data block member_name '%s'\n", data_block_member_name));

        if( rc == 0 && (rc = Experiment_MemberSeg(self->config->experiment, file_member_name, data_block_member_name,
                                                  nbases, bases, ((SRAWriter*)self)->read_segs, &assigned_member_name)) == 0 ) {
            DEBUG_MSG (5, ("Assigned member '%s'\n", assigned_member_name));
            if( assigned_member_name != NULL ) {
                if( (rc = SRATableWriteIdxColumn(self->table, self->col_spot_group_id, assigned_member_name, 0, strlen(assigned_member_name) * 8 )) != 0 ) {
                    PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=SPOT_GROUP"));
                }
            }
            if( rc == 0 ) {
                int32_t read_start[SRAWRITER_MAX_READS];
                uint32_t read_len[SRAWRITER_MAX_READS];
                int i;

                for (i = 0; i < self->nreads; i++) {
                    read_start[i] = self->read_segs[i].start;
                    read_len[i] = self->read_segs[i].len;
                }

                if( (rc = SRATableWriteIdxColumn(self->table, self->col_read_start_id, read_start, 0, sizeof(read_start[0]) * self->nreads * 8 )) != 0 ) {
                    PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=READ_START"));
                } else if( (rc = SRATableWriteIdxColumn(self->table, self->col_read_len_id, read_len, 0, sizeof(read_len[0]) * self->nreads * 8 )) != 0 ) {
                    PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=READ_LEN"));
                }
            }
        }
    }
    return rc;
}

rc_t SRAWriter_WriteSpotSegmentSimple(SRAWriter* self, const SRALoaderFile* file, const char* file_member_name, const SRASegment* read_seg)
{
    rc_t rc = 0;

    if( self == NULL || file == NULL ) {
        rc = RC( rcSRA, rcFormatter, rcMultiplexing, rcParam, rcNull);
    } else if ( self->col_spot_group_id == 0 || self->col_read_start_id == 0 || self->col_read_len_id == 0 ) {
        rc = RC( rcSRA, rcFormatter, rcMultiplexing, rcSelf, rcCorrupt);
    } else {
        const char* data_block_member_name = NULL;
        const char* assigned_member_name = NULL;

        DEBUG_MSG (5, ("file spot member_name '%s'\n", file_member_name));
        if( file_member_name != NULL &&
            (strcasecmp(file_member_name, "default") == 0 || strcasecmp(file_member_name, "0") == 0) ) {
            file_member_name = "";
        }
        rc = SRALoaderFileMemberName(file, &data_block_member_name);
        DEBUG_MSG (5, ("data block member_name '%s'\n", data_block_member_name));

        if( rc == 0 && (rc = Experiment_MemberSegSimple(self->config->experiment, file_member_name, data_block_member_name,
                                                        &assigned_member_name)) == 0 ) {
            DEBUG_MSG (5, ("Assigned member '%s'\n", assigned_member_name));
            if( assigned_member_name != NULL ) {
                if( (rc = SRATableWriteIdxColumn(self->table, self->col_spot_group_id, assigned_member_name, 0, strlen(assigned_member_name) * 8 )) != 0 ) {
                    PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=SPOT_GROUP"));
                }
            }
            if( rc == 0 ) {
                int32_t read_start[SRAWRITER_MAX_READS];
                uint32_t read_len[SRAWRITER_MAX_READS];
                int i;

                for (i = 0; i < self->nreads; i++) {
                    read_start[i] = read_seg[i].start;
                    read_len[i] = read_seg[i].len;
                }

                if( (rc = SRATableWriteIdxColumn(self->table, self->col_read_start_id, read_start, 0, sizeof(read_start[0]) * self->nreads * 8 )) != 0 ) {
                    PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=READ_START"));
                } else if( (rc = SRATableWriteIdxColumn(self->table, self->col_read_len_id, read_len, 0, sizeof(read_len[0]) * self->nreads * 8 )) != 0 ) {
                    PLOGERR(klogInt, (klogInt, rc, "failed to write $(column) column", "column=READ_LEN"));
                }
            }
        }
    }
    return rc;
}
