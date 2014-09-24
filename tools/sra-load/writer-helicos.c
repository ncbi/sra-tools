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
#include "writer-helicos.h"
#include "sra-writer.h"

struct SRAWriterHelicos {
    SRAWriter* base;

    uint32_t ci_name;
    uint32_t ci_read;
    uint32_t ci_qual;
};

rc_t SRAWriterHelicos_Make(const SRAWriterHelicos** cself, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SRAWriterHelicos* self;
    const PlatformXML* platform;

    if( cself == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    if( (rc = Experiment_GetPlatform(config->experiment, &platform)) != 0 ) {
        return rc;
    }
    if( platform->id != SRA_PLATFORM_HELICOS ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcData, rcInvalid);
        LOGERR(klogErr, rc, "platform type");
        return rc;
    }
    self = calloc(1, sizeof(*self));
    if( self == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    } else if( (rc = SRAWriter_Make(&self->base, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize base writer");
    }

    if( rc == 0 ) {
        *cself = self;
    } else {
        SRAWriterHelicos_Whack(self, NULL);
    }
    return rc;
}

void SRAWriterHelicos_Whack(const SRAWriterHelicos* cself, SRATable** table)
{
    if( cself != NULL ) {
        SRAWriterHelicos* self = (SRAWriterHelicos*)cself;
        SRAWriter_Whack(self->base, table);
        free(self);
    }
}

rc_t SRAWriterHelicos_Write(const SRAWriterHelicos* cself, const SRALoaderFile* data_block_ref,
                            const pstring* name, const pstring* sequence, const pstring* quality)
{
    rc_t rc = 0;
    SRAWriterHelicos* self =(SRAWriterHelicos*)cself;
    
    assert(self != NULL);
    assert(data_block_ref != NULL);

    DEBUG_MSG(3, ("spot name '%s'\n", name ? name->data : NULL));

    if( self->ci_name == 0 ) {
        if( (rc = SRAWriter_CreateTable(self->base, "NCBI:SRA:Helicos:tbl:v2")) != 0 ) {
            return rc;
        }
        if( (rc = SRAWriter_WriteDefaults(self->base)) != 0 ) {
            LOGERR(klogInt, rc, "failed to write table defaults");
            return rc;
        }
        COL_OPEN(self->ci_name, "NAME", vdb_ascii_t);
        COL_OPEN(self->ci_read, "READ", insdc_fasta_t);
        COL_OPEN(self->ci_qual, "QUALITY", insdc_phred_t);
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
    if( rc != 0 || (rc = SRAWriter_NewSpot(self->base, NULL)) != 0 ) {
        return rc;
    }
    COL_WRITE(self->ci_name, "NAME", (*name));
    COL_WRITE(self->ci_read, "READ", (*sequence));
    COL_WRITE(self->ci_qual, "QUALITY", (*quality));
    if( (rc = SRAWriter_WriteSpotSegment(self->base, data_block_ref, NULL, sequence->len, sequence->data)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to write $(column) column", "column=SPOT_GROUP/READ_SEG"));
        return rc;
    }
    return SRAWriter_CloseSpot(self->base);
}
