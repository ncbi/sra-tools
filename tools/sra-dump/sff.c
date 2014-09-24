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

#include <kapp/main.h>
#include <sra/sradb.h>
#include <sra/sff.h>
#include <sysalloc.h>

#include <stdlib.h>

#include "debug.h"
#include "core.h"
#include "sff-dump.vers.h"

#define DATABUFFERINITSIZE 10240

typedef struct SFFFormatterSplitter_struct {
    const SFFReader* reader;
    spotid_t spots;
    KDataBuffer* b;
} SFFFormatterSplitter;

static const char* SFFFormatterSplitter_Dump_Ext = ".sff";

static
rc_t SFFFormatterSplitter_Dump(const SRASplitter* cself, spotid_t spot, const readmask_t* readmask)
{
    rc_t rc = 0;
    SFFFormatterSplitter* self = (SFFFormatterSplitter*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcNode, rcExecuting, rcParam, rcNull);
    } else {
        size_t writ = 0;

        if( (rc = SRASplitter_FileActivate(cself, SFFFormatterSplitter_Dump_Ext)) == 0 ) {
            if( self->spots == 0 ) {
                IF_BUF((SFFReaderHeader(self->reader, 0, self->b->base, KDataBufferBytes(self->b), &writ)), self->b, writ) {
                    rc = SRASplitter_FileWrite(cself, 0, self->b->base, writ);
                }
            }
            if( rc == 0 ) {
                if( (rc = SFFReaderSeekSpot(self->reader, spot)) == 0 ) {
                    IF_BUF((SFFReader_GetCurrentSpotData(self->reader, self->b->base, KDataBufferBytes(self->b), &writ)), self->b, writ) {
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ);
                        self->spots++;
                    }
                } else if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound ) {
                    SRA_DUMP_DBG (3, ("%s skipped %u row\n", __func__, spot));
                    self->spots++;
                    rc = 0;
                }
            }
        }
    }
    return rc;
}

static
rc_t SFFFormatterSplitter_Release(const SRASplitter* cself)
{
    rc_t rc = 0;
    SFFFormatterSplitter* self = (SFFFormatterSplitter*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcNode, rcReleasing, rcSelf, rcNull);
    } else if( (rc = SRASplitter_FileActivate(cself, SFFFormatterSplitter_Dump_Ext)) == 0 ) {
        if( self->spots > 0 ) {
            size_t writ = 0;
            /* tweak SFF file header with real number of reads per file */
            IF_BUF((SFFReaderHeader(self->reader, self->spots, self->b->base, KDataBufferBytes(self->b), &writ)), self->b, writ) {
                rc = SRASplitter_FileWritePos(cself, 0, 0, self->b->base, writ);
            }
        }
    }
    return rc;
}

typedef struct SFFFormatterFactory_struct {
    const char* accession;
    const SRATable* table;
    const SFFReader* reader;
    KDataBuffer kdbuf;
} SFFFormatterFactory;

static
rc_t SFFFormatterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    SFFFormatterFactory* self = (SFFFormatterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = KDataBufferMakeBytes(&self->kdbuf, DATABUFFERINITSIZE)) == 0 ) {
        rc = SFFReaderMake(&self->reader, self->table, self->accession, 0, 0);
    }
    return rc;
}

static
rc_t SFFFormatterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    SFFFormatterFactory* self = (SFFFormatterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(SFFFormatterSplitter), NULL, NULL, SFFFormatterSplitter_Dump, SFFFormatterSplitter_Release)) == 0 ) {
            ((SFFFormatterSplitter*)(*splitter))->reader = self->reader;
            ((SFFFormatterSplitter*)(*splitter))->b = &self->kdbuf;
        }
    }
    return rc;
}

static
void SFFFormatterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        SFFFormatterFactory* self = (SFFFormatterFactory*)cself;
        KDataBufferWhack(&self->kdbuf);
        SFFReaderWhack(self->reader);
    }
}

static
rc_t SFFFormatterFactory_Make(const SRASplitterFactory** cself, const char* accession, const SRATable* table)
{
    rc_t rc = 0;
    SFFFormatterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterFormat, sizeof(*obj),
                                             SFFFormatterFactory_Init,
                                             SFFFormatterFactory_NewObj,
                                             SFFFormatterFactory_Release)) == 0 ) {
        obj = (SFFFormatterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
    }
    return rc;
}

/* ### External entry points ##################################################### */
const char UsageDefaultName[] = "sff-dump";
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}


ver_t CC KAppVersion( void )
{
    return SFF_DUMP_VERS;
}

rc_t SFFDumper_Factories(const SRADumperFmt* fmt, const SRASplitterFactory** factory)
{
    if( fmt == NULL || factory == NULL ) {
        return RC(rcExe, rcFormatter, rcReading, rcParam, rcInvalid);
    }
    *factory = NULL;
    return SFFFormatterFactory_Make(factory, fmt->accession, fmt->table);
}

/* main entry point of the file */
rc_t SRADumper_Init(SRADumperFmt* fmt)
{
    if( fmt == NULL ) {
        return RC(rcExe, rcFileFormat, rcConstructing, rcParam, rcNull);
    }
    fmt->release = NULL;
    fmt->arg_desc = NULL;
    fmt->add_arg = NULL;
    fmt->get_factory = SFFDumper_Factories;
    fmt->gzip = false;
    fmt->bzip2 = false;

    return 0;
}
