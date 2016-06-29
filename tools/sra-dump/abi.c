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
#include <klib/container.h>
#include <klib/printf.h>
#include <kapp/main.h>

#include <sra/sradb.h>
#include <sra/abi.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "debug.h"

#define DATABUFFERINITSIZE 10240

/* ============== Absolid read type (bio/tech) filter ============================ */

typedef struct Absolid2BioFilter_struct {
    const AbsolidReader* reader;
} Absolid2BioFilter;

/* leave only up to 2 first bio reads */
static
rc_t Absolid2BioFilter_GetKey(const SRASplitter* cself, const char** key, spotid_t spot, readmask_t* readmask)
{
    rc_t rc = 0;
    Absolid2BioFilter* self = (Absolid2BioFilter*)cself;

    if( self == NULL || key == NULL ) {
        rc = RC(rcExe, rcNode, rcExecuting, rcParam, rcInvalid);
    } else {
        if( (rc = AbsolidReaderSeekSpot(self->reader, spot)) == 0 ) {
            uint32_t num_reads = 0;
            if( (rc = AbsolidReader_SpotInfo(self->reader, NULL, NULL, NULL, &num_reads)) == 0 ) {
                uint32_t q, readId;
                make_readmask(new_mask);
                SRAReadTypes read_type = SRA_READ_TYPE_TECHNICAL;

                clear_readmask(new_mask);
                for(q = 0, readId = 0; rc == 0 && readId < num_reads && q < 2; readId++) {
                    if( (rc = AbsolidReader_SpotReadInfo(self->reader, readId + 1, &read_type, NULL, NULL, NULL, NULL)) == 0 ) {
                        if( read_type & SRA_READ_TYPE_BIOLOGICAL ) {
                            set_readmask(new_mask, readId);
                            q++;
                        }
                    }
                }
                *key = "";
                copy_readmask(new_mask, readmask);
            }
        } else if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound ) {
            SRA_DUMP_DBG (3, ("%s skipped %u row\n", __func__, spot));
            *key = NULL;
            rc = 0;
        }
    }
    return rc;
}

typedef struct Absolid2BioFilterFactory_struct {
    const char* accession;
    const SRATable* table;
    const AbsolidReader* reader;
} Absolid2BioFilterFactory;

static
rc_t Absolid2BioFilterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    Absolid2BioFilterFactory* self = (Absolid2BioFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else {
        rc = AbsolidReaderMake(&self->reader, self->table, self->accession,
                               false, false, 0, 0, 0, false);
    }
    return rc;
}

static
rc_t Absolid2BioFilterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    Absolid2BioFilterFactory* self = (Absolid2BioFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(Absolid2BioFilter), Absolid2BioFilter_GetKey, NULL, NULL, NULL)) == 0 ) {
            ((Absolid2BioFilter*)(*splitter))->reader = self->reader;
        }
    }
    return rc;
}

static
void Absolid2BioFilterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        Absolid2BioFilterFactory* self = (Absolid2BioFilterFactory*)cself;
        AbsolidReaderWhack(self->reader);
    }
}

static
rc_t Absolid2BioFilterFactory_Make(const SRASplitterFactory** cself, const char* accession, const SRATable* table)
{
    rc_t rc = 0;
    Absolid2BioFilterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterSpot, sizeof(*obj),
                                             Absolid2BioFilterFactory_Init,
                                             Absolid2BioFilterFactory_NewObj,
                                             Absolid2BioFilterFactory_Release)) == 0 ) {
        obj = (Absolid2BioFilterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
    }
    return rc;
}

/* ============== Absolid label assigning ============================ */

typedef struct AbsolidLabelerFilter_struct {
    const AbsolidReader* reader;
    bool is_platform_cs_native;
    SRASplitter_Keys keys[8];
    size_t key_sz[8];
} AbsolidLabelerFilter;

static
rc_t AbsolidLabelerFilter_GetKeySet(const SRASplitter* cself, const SRASplitter_Keys** key, uint32_t* keys, spotid_t spot, const readmask_t* readmask)
{
    rc_t rc = 0;
    AbsolidLabelerFilter* self = (AbsolidLabelerFilter*)cself;

    if( self == NULL || key == NULL ) {
        rc = RC(rcExe, rcNode, rcExecuting, rcParam, rcInvalid);
    } else {
        *keys = 0;
        if( rc == 0 && (rc = AbsolidReaderSeekSpot(self->reader, spot)) == 0 ) {
            uint32_t num_reads = 0;
            if( (rc = AbsolidReader_SpotInfo(self->reader, NULL, NULL, NULL, &num_reads)) == 0 ) {
                uint32_t readId, q;
                const char* read_label;
                INSDC_coord_len read_label_sz = 0;
                INSDC_coord_len read_len = 0;

                *key = self->keys;
                *keys = sizeof(self->keys) / sizeof(self->keys[0]);
                for(q = 0; q < *keys; q++) {
                    clear_readmask(self->keys[q].readmask);
                }
                for(q = 0, readId = 0; rc == 0 && readId < num_reads; readId++) {
                    if( !isset_readmask(readmask, readId) ) {
                        continue;
                    }
                    rc = AbsolidReader_SpotReadInfo(self->reader, readId + 1, NULL, &read_label, &read_label_sz, NULL, &read_len);
                    if( read_len == 0 ) {
                        continue;
                    }
                    if( self->is_platform_cs_native ) {
                        uint32_t i;
                        for(i = 0; i < *keys; i++) {
                            if( read_label_sz == self->key_sz[i] && strncmp(read_label, self->keys[i].key, read_label_sz) == 0 ) {
                                set_readmask(self->keys[i].readmask, readId);
                                break;
                            }
                        }
                        if( i >= *keys ) {
                            rc = RC(rcExe, rcNode, rcExecuting, rcTag, rcUnexpected);
                        }
                    } else {
                        /* since prev filter leaves only 2 first bio reads labels F3 and R3 wil be used */
                        set_readmask(self->keys[q].readmask, readId);
                    }
                    q++;
                }
            }
        } else if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound ) {
            SRA_DUMP_DBG (3, ("%s skipped %u row\n", __func__, spot));
            rc = 0;
        }
    }
    return rc;
}

typedef struct AbsolidLabelerFilterFactory_struct {
    const char* accession;
    const SRATable* table;
    bool is_platform_cs_native;
    const AbsolidReader* reader;
} AbsolidLabelerFilterFactory;

static
rc_t AbsolidLabelerFilterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    AbsolidLabelerFilterFactory* self = (AbsolidLabelerFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else {
        rc = AbsolidReaderMake(&self->reader, self->table, self->accession,
                               false, false, 0, 0, 0, false);
    }
    return rc;
}

static
rc_t AbsolidLabelerFilterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    AbsolidLabelerFilterFactory* self = (AbsolidLabelerFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(AbsolidLabelerFilter), NULL, AbsolidLabelerFilter_GetKeySet, NULL, NULL)) == 0 ) {
            ((AbsolidLabelerFilter*)(*splitter))->reader = self->reader;
            ((AbsolidLabelerFilter*)(*splitter))->is_platform_cs_native = self->is_platform_cs_native;
            ((AbsolidLabelerFilter*)(*splitter))->keys[0].key = "F3";
            ((AbsolidLabelerFilter*)(*splitter))->keys[1].key = "R3";
            ((AbsolidLabelerFilter*)(*splitter))->keys[2].key = "F5-P2";
            ((AbsolidLabelerFilter*)(*splitter))->keys[3].key = "F5-BC";
            ((AbsolidLabelerFilter*)(*splitter))->keys[4].key = "F5-RNA";
            ((AbsolidLabelerFilter*)(*splitter))->keys[5].key = "F5-DNA";
            ((AbsolidLabelerFilter*)(*splitter))->keys[6].key = "F3-DNA";
            ((AbsolidLabelerFilter*)(*splitter))->keys[7].key = "BC";
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[0] = 2;
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[1] = 2;
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[2] = 5;
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[3] = 5;
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[4] = 6;
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[5] = 6;
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[6] = 6;
            ((AbsolidLabelerFilter*)(*splitter))->key_sz[7] = 2;
        }
    }
    return rc;
}

static
void AbsolidLabelerFilterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        AbsolidLabelerFilterFactory* self = (AbsolidLabelerFilterFactory*)cself;
        AbsolidReaderWhack(self->reader);
    }
}

static
rc_t AbsolidLabelerFilterFactory_Make(const SRASplitterFactory** cself, const char* accession,
                                      const SRATable* table, bool is_platform_cs_native)
{
    rc_t rc = 0;
    AbsolidLabelerFilterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterRead, sizeof(*obj),
                                             AbsolidLabelerFilterFactory_Init,
                                             AbsolidLabelerFilterFactory_NewObj,
                                             AbsolidLabelerFilterFactory_Release)) == 0 ) {
        obj = (AbsolidLabelerFilterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
        obj->is_platform_cs_native = is_platform_cs_native;
    }
    return rc;
}

/* ============== Absolid min read length splitter ============================ */

char* AbsolidReadLenFilter_key_buf = NULL;

typedef struct AbsolidReadLenFilter_struct {
    const AbsolidReader* reader;
} AbsolidReadLenFilter;


/* skip all reads with len < minreadlen */
static
rc_t AbsolidReadLenFilter_GetKey(const SRASplitter* cself, const char** key, spotid_t spot, readmask_t* readmask)
{
    rc_t rc = 0;
    AbsolidReadLenFilter* self = (AbsolidReadLenFilter*)cself;

    if( self == NULL || key == NULL ) {
        rc = RC(rcExe, rcNode, rcExecuting, rcParam, rcInvalid);
    } else {
        *key = "";

        if( rc == 0 && (rc = AbsolidReaderSeekSpot(self->reader, spot)) == 0 ) {
            uint32_t num_reads = 0;
            if( (rc = AbsolidReader_SpotInfo(self->reader, NULL, NULL, NULL, &num_reads)) == 0 ) {
                uint32_t readId;
                make_readmask(new_mask);
                INSDC_coord_len read_len = 0;

                clear_readmask(new_mask);
                for(readId = 0; rc == 0 && readId < num_reads; readId++) {
                    if( !isset_readmask(readmask, readId) ) {
                        continue;
                    }
                    rc = AbsolidReader_SpotReadInfo(self->reader, readId + 1, NULL, NULL, NULL, NULL, &read_len);
                    if( rc == 0 && read_len > 0 ) {
                        set_readmask(new_mask, readId);
                    }
                }
                copy_readmask(new_mask, readmask);
            }
        } else if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound ) {
            SRA_DUMP_DBG (3, ("%s skipped %u row\n", __func__, spot));
            *key = NULL;
            rc = 0;
        }
    }
    return rc;
}

typedef struct AbsolidReadLenFilterFactory_struct {
    const char* accession;
    const SRATable* table;
    bool clip;
    uint32_t minReadLen;
    const AbsolidReader* reader;
} AbsolidReadLenFilterFactory;

static
rc_t AbsolidReadLenFilterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    AbsolidReadLenFilterFactory* self = (AbsolidReadLenFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else {
        rc = AbsolidReaderMake(&self->reader, self->table, self->accession,
                               false, !self->clip, self->minReadLen, 0, 0, false);
    }
    return rc;
}

static
rc_t AbsolidReadLenFilterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    AbsolidReadLenFilterFactory* self = (AbsolidReadLenFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(AbsolidReadLenFilter), AbsolidReadLenFilter_GetKey, NULL, NULL, NULL)) == 0 ) {
            ((AbsolidReadLenFilter*)(*splitter))->reader = self->reader;
        }
    }
    return rc;
}

static
void AbsolidReadLenFilterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        AbsolidReadLenFilterFactory* self = (AbsolidReadLenFilterFactory*)cself;
        AbsolidReaderWhack(self->reader);
        free(AbsolidReadLenFilter_key_buf);
    }
}

static
rc_t AbsolidReadLenFilterFactory_Make(const SRASplitterFactory** cself, const char* accession, const SRATable* table,
                                      bool clip, uint32_t minReadLen)
{
    rc_t rc = 0;
    AbsolidReadLenFilterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterSpot, sizeof(*obj),
                                             AbsolidReadLenFilterFactory_Init,
                                             AbsolidReadLenFilterFactory_NewObj,
                                             AbsolidReadLenFilterFactory_Release)) == 0 ) {
        obj = (AbsolidReadLenFilterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
        obj->minReadLen = minReadLen;
        obj->clip = clip;
    }
    return rc;
}

/* ============== Absolid quality filter ============================ */

typedef struct AbsolidQFilter_struct {
    const AbsolidReader* reader;
    KDataBuffer* b;
} AbsolidQFilter;


/* filter out reads by leading/trialing quality */
static
rc_t AbsolidQFilter_GetKey(const SRASplitter* cself, const char** key, spotid_t spot, readmask_t* readmask)
{
    rc_t rc = 0;
    AbsolidQFilter* self = (AbsolidQFilter*)cself;

    if( self == NULL || key == NULL ) {
        rc = RC(rcExe, rcNode, rcExecuting, rcParam, rcInvalid);
    } else {
        uint32_t num_reads = 0;

        if( (rc = AbsolidReaderSeekSpot(self->reader, spot)) == 0 ) {
            if( (rc = AbsolidReader_SpotInfo(self->reader, NULL, NULL, NULL, &num_reads)) == 0 ) {
                uint32_t readId;
                make_readmask(new_mask);
                INSDC_coord_len read_len = 0;
                static char const* const colorStr = "..........";
                static const int xLen = 10;

                clear_readmask(new_mask);
                for(readId = 0; rc == 0 && readId < num_reads; readId++) {
                    if( (rc = AbsolidReader_SpotReadInfo(self->reader, readId + 1, NULL, NULL, NULL, NULL, &read_len)) == 0 ) {
                        IF_BUF((AbsolidReaderBase(self->reader, readId + 1, self->b->base, KDataBufferBytes(self->b), NULL)), self->b, read_len) {
                            const char* b = self->b->base;
                            if( strncmp(&b[1], colorStr, xLen) == 0 && strcmp(&b[read_len - xLen + 1], colorStr) == 0 )  {
                                continue;
                            }
                        }
                        set_readmask(new_mask, readId);
                    }
                }
                *key = "";
                copy_readmask(new_mask, readmask);
            }
        } else if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound ) {
            SRA_DUMP_DBG (3, ("%s skipped %u row\n", __func__, spot));
            *key = NULL;
            rc = 0;
        }
    }
    return rc;
}

typedef struct AbsolidQFilterFactory_struct {
    const char* accession;
    const SRATable* table;
    bool clip;
    const AbsolidReader* reader;
    KDataBuffer buf;
} AbsolidQFilterFactory;

static
rc_t AbsolidQFilterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    AbsolidQFilterFactory* self = (AbsolidQFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = KDataBufferMakeBytes(&self->buf, DATABUFFERINITSIZE)) == 0 ) {
        rc = AbsolidReaderMake(&self->reader, self->table, self->accession,
                               false, self->clip, 0, 0, 0, false);
    }
    return rc;
}

static
rc_t AbsolidQFilterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    AbsolidQFilterFactory* self = (AbsolidQFilterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcExe, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(AbsolidQFilter), AbsolidQFilter_GetKey, NULL, NULL, NULL)) == 0 ) {
            ((AbsolidQFilter*)(*splitter))->reader = self->reader;
            ((AbsolidQFilter*)(*splitter))->b = &self->buf;
        }
    }
    return rc;
}

static
void AbsolidQFilterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        AbsolidQFilterFactory* self = (AbsolidQFilterFactory*)cself;
        KDataBufferWhack(&self->buf);
        AbsolidReaderWhack(self->reader);
    }
}

static
rc_t AbsolidQFilterFactory_Make(const SRASplitterFactory** cself, const char* accession, const SRATable* table, bool clip)
{
    rc_t rc = 0;
    AbsolidQFilterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcExe, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterSpot, sizeof(*obj),
                                             AbsolidQFilterFactory_Init,
                                             AbsolidQFilterFactory_NewObj,
                                             AbsolidQFilterFactory_Release)) == 0 ) {
        obj = (AbsolidQFilterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
        obj->clip = clip;
    }
    return rc;
}

/* ============== Absolid formatter object  ============================ */

typedef struct AbsolidFormatterSplitter_struct {
    const AbsolidReader* reader;
    char pfx[10 * 1024];
    int pfx_sz;
    KDataBuffer* hb;
    KDataBuffer* b;
} AbsolidFormatterSplitter;

static
rc_t AbsolidFormatterSplitter_Dump(const SRASplitter* cself, spotid_t spot, const readmask_t* readmask)
{
    rc_t rc = 0;
    AbsolidFormatterSplitter* self = (AbsolidFormatterSplitter*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = AbsolidReaderSeekSpot(self->reader, spot)) == 0 ) {
            const char* prefix;
            size_t head_sz, writ = 0;
            uint32_t num_reads, readId;

            rc = AbsolidReader_SpotInfo(self->reader, NULL, NULL, NULL, &num_reads);

            for(readId = 1; rc == 0 && readId <= num_reads; readId++) {
                if( !isset_readmask(readmask, readId - 1) ) {
                    continue;
                }
                if( (rc = AbsolidReaderSpotName(self->reader, &prefix, &head_sz, NULL, NULL)) == 0 ) {
                    if( head_sz != self->pfx_sz || memcmp(self->pfx, prefix, head_sz) != 0 ) {
                        if( head_sz >= sizeof(self->pfx) ) {
                            rc = RC(rcSRA, rcString, rcCopying, rcBuffer, rcInsufficient);
                        } else {
                            self->pfx_sz = head_sz;
                            memcpy(self->pfx, prefix, head_sz);
                            IF_BUF((string_printf(self->hb->base, KDataBufferBytes(self->hb), &head_sz,
                                   "#\n# Title: %.*s\n#\n", self->pfx_sz, self->pfx)), self->hb, head_sz);
                        }
                    } else {
                        head_sz = 0;
                    }
                }
                if( rc == 0 ) {
                    char* h = self->hb->base;
                    IF_BUF((AbsolidReaderHeader(self->reader, readId, &h[head_sz],
                            KDataBufferBytes(self->hb) - head_sz - 1, &writ)), self->hb, writ + head_sz + 1) {
                        head_sz += writ;
                        h[head_sz++] = '\n';
                    }
                }
                if( rc == 0 && (rc = SRASplitter_FileActivate(cself, ".csfasta")) == 0 ) {
                    IF_BUF((AbsolidReaderBase(self->reader, readId, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                        if( writ > 0 && (rc = SRASplitter_FileWrite(cself, 0, self->hb->base, head_sz)) == 0 ) {
                            ((char*)(self->b->base))[writ] = '\n';
                            rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                        }
                    }
                }
                if( rc == 0 && (rc = SRASplitter_FileActivate(cself, "_QV.qual")) == 0 ) {
                    IF_BUF((AbsolidReaderQuality(self->reader, readId, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                        if( writ > 0 && (rc = SRASplitter_FileWrite(cself, 0, self->hb->base, head_sz)) == 0 ) {
                            ((char*)(self->b->base))[writ] = '\n';
                            rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                        }
                    }
                }
                if( rc == 0 && (rc = SRASplitter_FileActivate(cself, "_intensity.ScaledFTC.fasta")) == 0 ) {
                    IF_BUF((AbsolidReaderSignalFTC(self->reader, readId, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                        if( writ > 0 && (rc = SRASplitter_FileWrite(cself, 0, self->hb->base, head_sz)) == 0 ) {
                            ((char*)(self->b->base))[writ] = '\n';
                            rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                        }
                    }
                }
                if( rc == 0 && (rc = SRASplitter_FileActivate(cself, "_intensity.ScaledCY3.fasta")) == 0 ) {
                    IF_BUF((AbsolidReaderSignalCY3(self->reader, readId, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                        if( writ > 0 && (rc = SRASplitter_FileWrite(cself, 0, self->hb->base, head_sz)) == 0 ) {
                            ((char*)(self->b->base))[writ] = '\n';
                            rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                        }
                    }
                }
                if( rc == 0 && (rc = SRASplitter_FileActivate(cself, "_intensity.ScaledTXR.fasta")) == 0 ) {
                    IF_BUF((AbsolidReaderSignalTXR(self->reader, readId, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                        if( writ > 0 && (rc = SRASplitter_FileWrite(cself, 0, self->hb->base, head_sz)) == 0 ) {
                            ((char*)(self->b->base))[writ] = '\n';
                            rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                        }
                    }
                }
                if( rc == 0 && (rc = SRASplitter_FileActivate(cself, "_intensity.ScaledCY5.fasta")) == 0 ) {
                    IF_BUF((AbsolidReaderSignalCY5(self->reader, readId, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                        if( writ > 0 && (rc = SRASplitter_FileWrite(cself, 0, self->hb->base, head_sz)) == 0 ) {
                            ((char*)(self->b->base))[writ] = '\n';
                            rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                        }
                    }
                }
            }
        } else if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound ) {
            SRA_DUMP_DBG (3, ("%s skipped %u row\n", __func__, spot));
            rc = 0;
        }
    }
    return rc;
}

typedef struct AbsolidFormatterFactory_struct {
    const char* accession;
    const SRATable* table;
    bool orig;
    bool clip;
    const AbsolidReader* reader;
    KDataBuffer hbuf;
    KDataBuffer buf;
} AbsolidFormatterFactory;

static
rc_t AbsolidFormatterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    AbsolidFormatterFactory* self = (AbsolidFormatterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = KDataBufferMakeBytes(&self->hbuf, DATABUFFERINITSIZE)) == 0 &&
               (rc = KDataBufferMakeBytes(&self->buf, DATABUFFERINITSIZE)) == 0 ) {
        rc = AbsolidReaderMake(&self->reader, self->table, self->accession,
                               self->orig, !self->clip, 0, 0, 0, true);
    }
    return rc;
}

static
rc_t AbsolidFormatterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    AbsolidFormatterFactory* self = (AbsolidFormatterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(AbsolidFormatterSplitter), NULL, NULL, AbsolidFormatterSplitter_Dump, NULL)) == 0 ) {
            ((AbsolidFormatterSplitter*)(*splitter))->reader = self->reader;
            ((AbsolidFormatterSplitter*)(*splitter))->pfx[0] = '\0';
            ((AbsolidFormatterSplitter*)(*splitter))->pfx_sz = 0;
            ((AbsolidFormatterSplitter*)(*splitter))->hb = &self->hbuf;
            ((AbsolidFormatterSplitter*)(*splitter))->b = &self->buf;

        }
    }
    return rc;
}

static
void AbsolidFormatterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        AbsolidFormatterFactory* self = (AbsolidFormatterFactory*)cself;
        KDataBufferWhack(&self->hbuf);
        KDataBufferWhack(&self->buf);
        AbsolidReaderWhack(self->reader);
    }
}

static
rc_t AbsolidFormatterFactory_Make(const SRASplitterFactory** cself, const char* accession, const SRATable* table, bool orig, bool clip)
{
    rc_t rc = 0;
    AbsolidFormatterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterFormat, sizeof(*obj),
                                             AbsolidFormatterFactory_Init,
                                             AbsolidFormatterFactory_NewObj,
                                             AbsolidFormatterFactory_Release)) == 0 ) {
        obj = (AbsolidFormatterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
        obj->orig = orig;
        obj->clip = clip;
    }
    return rc;
}

/* ### External entry points ##################################################### */
const char UsageDefaultName[] = "abi-dump";
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

struct AbsolidArgs_struct {
    bool is_platform_cs_native;

    uint32_t minReadLen;
    bool applyClip;
    bool dumpOrigFmt;
    bool qual_filter;
} AbsolidArgs;

rc_t AbsolidDumper_Release(const SRADumperFmt* fmt)
{
    if( fmt == NULL ) {
        return RC(rcExe, rcFormatter, rcDestroying, rcParam, rcInvalid);
    }
    return 0;
}

bool AbsolidDumper_AddArg(const SRADumperFmt* fmt, GetArg* f, int* i, int argc, char *argv[])
{
    const char* arg = NULL;

    if( fmt == NULL || f == NULL || i == NULL || argv == NULL ) {
        LOGERR(klogErr, RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid), "null param");
        return false;
    } else if( f(fmt, "M", "minReadLen", i, argc, argv, &arg) ) {
        AbsolidArgs.minReadLen = AsciiToU32(arg, NULL, NULL);
    } else if( f(fmt, "W", "noclip", i, argc, argv, NULL) ) {
        AbsolidArgs.applyClip = false;
    } else if( f(fmt, "F", "origfmt", i, argc, argv, NULL) ) {
        AbsolidArgs.dumpOrigFmt = true;
    } else if( f(fmt, "B", "noDotReads", i, argc, argv, NULL) ) {
        AbsolidArgs.qual_filter = true;
    } else {
        return false;
    }
    return true;
}

rc_t AbsolidDumper_Factories(const SRADumperFmt* fmt, const SRASplitterFactory** factory)
{
    rc_t rc = 0;
    const SRASplitterFactory* parent = NULL, *child = NULL;

    if( fmt == NULL || factory == NULL ) {
        return RC(rcExe, rcFormatter, rcReading, rcParam, rcInvalid);
    }
    *factory = NULL;

    {
        const SRAColumn* c = NULL;
        if( (rc = SRATableOpenColumnRead(fmt->table, &c, "PLATFORM", sra_platform_id_t)) == 0 ) {
            const INSDC_SRA_platform_id* platform;
            bitsz_t o, z;
            if( (rc = SRAColumnRead(c, 1, (const void **)&platform, &o, &z)) == 0 ) {
                if( *platform == SRA_PLATFORM_ABSOLID ) {
                    AbsolidArgs.is_platform_cs_native = true;
                }
            }
            SRAColumnRelease(c);
        } else if( GetRCState(rc) == rcNotFound && GetRCObject(rc) == ( enum RCObject )rcColumn ) {
            rc = 0;
        }
    }

    if( rc == 0 ) {
        if( (rc = Absolid2BioFilterFactory_Make(&child, fmt->accession, fmt->table)) == 0 ) {
            if( parent != NULL ) {
                if( (rc = SRASplitterFactory_AddNext(parent, child)) != 0 ) {
                    SRASplitterFactory_Release(child);
                } else {
                    parent = child;
                }
            } else {
                parent = child;
                *factory = parent;
            }
        }
    }
    if( rc == 0 ) {
        if( (rc = AbsolidLabelerFilterFactory_Make(&child, fmt->accession, fmt->table, AbsolidArgs.is_platform_cs_native)) == 0 ) {
            if( parent != NULL ) {
                if( (rc = SRASplitterFactory_AddNext(parent, child)) != 0 ) {
                    SRASplitterFactory_Release(child);
                } else {
                    parent = child;
                }
            } else {
                parent = child;
                *factory = parent;
            }
        }
    }
    if( rc == 0 && AbsolidArgs.minReadLen > 0 ) {
        if( (rc = AbsolidReadLenFilterFactory_Make(&child, fmt->accession, fmt->table,
                                                   AbsolidArgs.applyClip, AbsolidArgs.minReadLen)) == 0 ) {
            if( parent != NULL ) {
                if( (rc = SRASplitterFactory_AddNext(parent, child)) != 0 ) {
                    SRASplitterFactory_Release(child);
                } else {
                    parent = child;
                }
            } else {
                parent = child;
                *factory = parent;
            }
        }
    }
    if( rc == 0 && AbsolidArgs.qual_filter ) {
        if( (rc = AbsolidQFilterFactory_Make(&child, fmt->accession, fmt->table, AbsolidArgs.applyClip)) == 0 ) {
            if( parent != NULL ) {
                if( (rc = SRASplitterFactory_AddNext(parent, child)) != 0 ) {
                    SRASplitterFactory_Release(child);
                } else {
                    parent = child;
                }
            } else {
                parent = child;
                *factory = parent;
            }
        }
    }
    if( rc == 0 ) {
        if( (rc = AbsolidFormatterFactory_Make(&child, fmt->accession, fmt->table,
                                               AbsolidArgs.dumpOrigFmt, AbsolidArgs.applyClip)) == 0 ) {
            if( parent != NULL ) {
                if( (rc = SRASplitterFactory_AddNext(parent, child)) != 0 ) {
                    SRASplitterFactory_Release(child);
                }
            } else {
                *factory = child;
            }
        }
    }
    return rc;
}

/* main entry point of the file */
rc_t SRADumper_Init(SRADumperFmt* fmt)
{
    static const SRADumperFmt_Arg arg[] =
        {
            {"M", "minReadLen", "len", {"Minimum read length to output, default is 25", NULL}},
            {"W", "noclip", NULL, {"Do not clip quality left and right for spot", NULL}},
            {"F", "origfmt", NULL, {"Excludes SRR accession on defline", NULL}},
            {"B", "noDotReads", NULL, {"Do not output reads consisting mostly of dots", NULL}},
            {NULL, NULL, NULL, {NULL}}
        };
    AbsolidArgs.is_platform_cs_native = false;
    AbsolidArgs.minReadLen = 25;
    AbsolidArgs.applyClip = true;
    AbsolidArgs.dumpOrigFmt = false;
    AbsolidArgs.qual_filter = false;

    if( fmt == NULL ) {
        return RC(rcExe, rcFileFormat, rcConstructing, rcParam, rcNull);
    }
    fmt->release = AbsolidDumper_Release;
    fmt->arg_desc = arg;
    fmt->add_arg = AbsolidDumper_AddArg;
    fmt->get_factory = AbsolidDumper_Factories;
    fmt->gzip = true;
    fmt->bzip2 = true;

    return 0;
}
