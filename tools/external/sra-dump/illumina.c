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
#include <sra/illumina.h>
#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "core.h"

#define DATABUFFERINITSIZE 10240

typedef enum {
    eRead = 0x01,
    eQual1_S = 0x02,
    eQual1_M = 0x04,
    eQual4 = 0x08,
    eIntensity = 0x10,
    eNoise = 0x20,
    eSignal = 0x40,
    eQSeq_S = 0x80,
    eQSeq_M = 0x100
} EIlluminaOptions;

struct IlluminaArgs_struct {
    EIlluminaOptions opt;
} IlluminaArgs;


/* ============== Illumina tile/lane splitter based on uint32_t pointer ============================ */
typedef enum {
    eIlluminaLaneSplitter = 1,
    eIlluminaTileSplitter = 2
} TIlluminaLaneSplitterType;

typedef struct IlluminaU32Splitter_struct {
    TIlluminaLaneSplitterType type;
    const IlluminaReader* reader;
    char key[128];
} IlluminaU32Splitter;

static
rc_t IlluminaU32Splitter_GetKey(const SRASplitter* cself, const char** key, spotid_t spot, readmask_t* readmask)
{
    rc_t rc = 0;
    IlluminaU32Splitter* self = (IlluminaU32Splitter*)cself;

    if( self == NULL || key == NULL ) {
        rc = RC(rcSRA, rcNode, rcExecuting, rcParam, rcInvalid);
    } else {
        *key = self->key;
        self->key[0] = '\0';
        if( (rc = IlluminaReaderSeekSpot(self->reader, spot)) == 0 ) {
            int w;
            INSDC_coord_val val;
            switch( self->type ) {
                case eIlluminaLaneSplitter:
                    w = 1;
                    rc = IlluminaReader_SpotInfo(self->reader, NULL, NULL, &val, NULL, NULL, NULL, NULL, NULL);
                    break;
                case eIlluminaTileSplitter:
                    w = 4;
                    rc = IlluminaReader_SpotInfo(self->reader, NULL, NULL, NULL, &val, NULL, NULL, NULL, NULL);
                    break;
                default:
                    rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcInvalid);
            }
            if( rc == 0 ) {
                int n = snprintf(self->key, sizeof(self->key), "%0*d", w, val);
                assert(n < sizeof(self->key));
            }
        } else if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound ) {
            SRA_DUMP_DBG (3, ("%s skipped %u row\n", __func__, spot));
            *key = NULL;
            rc = 0;
        }
    }
    return rc;
}

typedef struct IlluminaU32SplitterFactory_struct {
    const char* accession;
    const SRATable* table;
    TIlluminaLaneSplitterType type;
    const IlluminaReader* reader;
} IlluminaU32SplitterFactory;

static
rc_t IlluminaU32SplitterFactory_Init(const SRASplitterFactory* cself)
{
    rc_t rc = 0;
    IlluminaU32SplitterFactory* self = (IlluminaU32SplitterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
    } else {
        rc = IlluminaReaderMake(&self->reader, self->table, self->accession, true, false, false, false, false, false, false, 0, 0);
    }
    return rc;
}

static
rc_t IlluminaU32SplitterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    IlluminaU32SplitterFactory* self = (IlluminaU32SplitterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(IlluminaU32Splitter), IlluminaU32Splitter_GetKey, NULL, NULL, NULL)) == 0 ) {
            ((IlluminaU32Splitter*)(*splitter))->reader = self->reader;
            ((IlluminaU32Splitter*)(*splitter))->type = self->type;
        }
    }
    return rc;
}

static
void IlluminaU32SplitterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        IlluminaU32SplitterFactory* self = (IlluminaU32SplitterFactory*)cself;
        IlluminaReaderWhack(self->reader);
    }
}

static
rc_t IlluminaU32SplitterFactory_Make(const SRASplitterFactory** cself, const char* accession, const SRATable* table, TIlluminaLaneSplitterType type)
{
    rc_t rc = 0;
    IlluminaU32SplitterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterSpot, sizeof(*obj),
                                             IlluminaU32SplitterFactory_Init,
                                             IlluminaU32SplitterFactory_NewObj,
                                             IlluminaU32SplitterFactory_Release)) == 0 ) {
        obj = (IlluminaU32SplitterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
        switch(type) {
            case eIlluminaLaneSplitter:
            case eIlluminaTileSplitter:
                obj->type = type;
                break;
            default:
                rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcInvalid);
        }
    }
    return rc;
}

/* ============== Illumina formatter object  ============================ */

typedef struct IlluminaFormatterSplitter_struct {
    const IlluminaReader* reader;
    KDataBuffer* b;
} IlluminaFormatterSplitter;

static
rc_t IlluminaFormatterSplitter_Dump(const SRASplitter* cself, spotid_t spot, const readmask_t* readmask)
{
    rc_t rc = 0;
    IlluminaFormatterSplitter* self = (IlluminaFormatterSplitter*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = IlluminaReaderSeekSpot(self->reader, spot)) == 0 ) {
            size_t writ = 0;

            if( rc == 0 && (IlluminaArgs.opt & eRead) &&
                (rc = SRASplitter_FileActivate(cself, "seq.txt")) == 0 ) {
                    IF_BUF((IlluminaReaderBase(self->reader, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                    if( writ > 0 ) {
                        ((char*)(self->b->base))[writ] = '\n';
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eQual1_S) &&
                (rc = SRASplitter_FileActivate(cself, "qcal.txt")) == 0 ) {
                IF_BUF((IlluminaReaderQuality1(self->reader, 0, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                    if( writ > 0 ) {
                        ((char*)(self->b->base))[writ] = '\n';
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eQual1_M) ) {
                uint32_t num_reads = 0;
                if( (rc = IlluminaReader_SpotInfo(self->reader, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &num_reads)) == 0 ) {
                    int readId;
                    char key[128];
                    for(readId = 1; rc == 0 && readId <= num_reads; readId++) {
                        int n = snprintf(key, sizeof(key), "%u_qcal.txt", readId);
                        assert(n < sizeof(key));
                        if( (rc = SRASplitter_FileActivate(cself, key)) == 0 ) {
                            IF_BUF((IlluminaReaderQuality1(self->reader, readId, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                                if( writ > 0 ) {
                                    ((char*)(self->b->base))[writ] = '\n';
                                    rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                                }
                            }
                        }
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eQual4) && (rc = SRASplitter_FileActivate(cself, "prb.txt")) == 0 ) {
                IF_BUF((IlluminaReaderQuality4(self->reader, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                    if( writ > 0 ) {
                        ((char*)(self->b->base))[writ] = '\n';
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eIntensity) && (rc = SRASplitter_FileActivate(cself, "int.txt")) == 0 ) {
                IF_BUF((IlluminaReaderIntensity(self->reader, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                    if( writ > 0 ) {
                        ((char*)(self->b->base))[writ] = '\n';
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eNoise) && (rc = SRASplitter_FileActivate(cself, "nse.txt")) == 0 ) {
                IF_BUF((IlluminaReaderNoise(self->reader, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                    if( writ > 0 ) {
                        ((char*)(self->b->base))[writ] = '\n';
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eSignal) && (rc = SRASplitter_FileActivate(cself, "sig2.txt")) == 0 ) {
                IF_BUF((IlluminaReaderSignal(self->reader, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                    if( writ > 0 ) {
                        ((char*)(self->b->base))[writ] = '\n';
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eQSeq_S) && (rc = SRASplitter_FileActivate(cself, "qseq.txt")) == 0 ) {
                IF_BUF((IlluminaReaderQSeq(self->reader, 0, true, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                    if( writ > 0 ) {
                        ((char*)(self->b->base))[writ] = '\n';
                        rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                    }
                }
            }
            if( rc == 0 && (IlluminaArgs.opt & eQSeq_M) ) {
                uint32_t num_reads = 0;
                if( (rc = IlluminaReader_SpotInfo(self->reader, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &num_reads)) == 0 ) {
                    uint32_t readId;
                    char key[128];
                    for(readId = 1; rc == 0 && readId <= num_reads; readId++) {
                        int n = snprintf(key, sizeof(key), "%u_qseq.txt", readId);
                        assert(n < sizeof(key));
                        if( (rc = SRASplitter_FileActivate(cself, key)) == 0 ) {
                            IF_BUF((IlluminaReaderQSeq(self->reader, readId, true, self->b->base, KDataBufferBytes(self->b) - 1, &writ)), self->b, writ) {
                                if( writ > 0 ) {
                                    ((char*)(self->b->base))[writ] = '\n';
                                    rc = SRASplitter_FileWrite(cself, spot, self->b->base, writ + 1);
                                }
                            }
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

typedef struct IlluminaFormatterFactory_struct {
    const char* accession;
    const SRATable* table;
    const IlluminaReader* reader;
    KDataBuffer buf;
} IlluminaFormatterFactory;


/* refactored June 13 2014 by Wolfgang to fix a mysterious bug on windows where b_qseq resolved into false on windows
   and true on posix! The original code was this:

    if( self == NULL ) {
          rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
      } else if( (rc = IlluminaReaderMake(&self->reader, self->table, self->accession,
                                  IlluminaArgs.opt & eRead, IlluminaArgs.opt & (eQual1_S | eQual1_M), IlluminaArgs.opt & eQual4, 
                                  IlluminaArgs.opt & eIntensity, IlluminaArgs.opt & eNoise, IlluminaArgs.opt & eSignal,
                                  IlluminaArgs.opt & (eQSeq_S | eQSeq_M), 0, 0)) == 0 ) {
          rc = KDataBufferMakeBytes(&self->buf, DATABUFFERINITSIZE);
        }

    Anton's old, more dense version should do the same, but it does not on Windows!

    eQSeq_M is an enum, defined at the top of this file to have the value 0x100.
    It will be type-casted into bool because that is the type IlluminaReaderMake() wants.
    ( bool )eQSeq_M is true for GCC/LLVM and false for the MS-compiler!
    Casting enum's directly without comparison into boolean does not work for the MS-compiler.
*/
static rc_t IlluminaFormatterFactory_Init( const SRASplitterFactory * cself )
{
    rc_t rc = 0;
    IlluminaFormatterFactory * self = ( IlluminaFormatterFactory * )cself;

    if ( self == NULL )
        rc = RC( rcSRA, rcType, rcConstructing, rcParam, rcNull );
    else
    {
        bool b_read = ( ( IlluminaArgs.opt & eRead ) != 0 );
        bool b_qual1 = ( ( IlluminaArgs.opt & ( eQual1_S | eQual1_M ) ) != 0 );
        bool b_qual4 = ( ( IlluminaArgs.opt & eQual4 ) != 0 );
        bool b_intensity = ( ( IlluminaArgs.opt & eIntensity ) != 0 );
        bool b_noise = ( ( IlluminaArgs.opt & eNoise ) != 0 );
        bool b_signal = ( ( IlluminaArgs.opt & eSignal ) != 0 );
        bool b_qseq = ( ( IlluminaArgs.opt & ( eQSeq_S | eQSeq_M ) ) != 0 );
        rc = IlluminaReaderMake( &self->reader,
                                self->table,
                                self->accession,
                                b_read,
                                b_qual1,
                                b_qual4,
                                b_intensity,
                                b_noise,
                                b_signal,
                                b_qseq,
                                0,
                                0 );
        if ( rc == 0 )
            rc = KDataBufferMakeBytes( &self->buf, DATABUFFERINITSIZE );
    }
    return rc;
}


static
rc_t IlluminaFormatterFactory_NewObj(const SRASplitterFactory* cself, const SRASplitter** splitter)
{
    rc_t rc = 0;
    IlluminaFormatterFactory* self = (IlluminaFormatterFactory*)cself;

    if( self == NULL ) {
        rc = RC(rcSRA, rcType, rcExecuting, rcParam, rcNull);
    } else {
        if( (rc = SRASplitter_Make(splitter, sizeof(IlluminaFormatterSplitter), NULL, NULL, IlluminaFormatterSplitter_Dump, NULL)) == 0 ) {
            ((IlluminaFormatterSplitter*)(*splitter))->reader = self->reader;
            ((IlluminaFormatterSplitter*)(*splitter))->b = &self->buf;
        }
    }
    return rc;
}

static
void IlluminaFormatterFactory_Release(const SRASplitterFactory* cself)
{
    if( cself != NULL ) {
        IlluminaFormatterFactory* self = (IlluminaFormatterFactory*)cself;
        IlluminaReaderWhack(self->reader);
        KDataBufferWhack(&self->buf);
    }
}

static
rc_t IlluminaFormatterFactory_Make(const SRASplitterFactory** cself, const char* accession, const SRATable* table)
{
    rc_t rc = 0;
    IlluminaFormatterFactory* obj = NULL;

    if( cself == NULL || accession == NULL || table == NULL ) {
        rc = RC(rcSRA, rcType, rcConstructing, rcParam, rcNull);
    } else if( (rc = SRASplitterFactory_Make(cself, eSplitterFormat, sizeof(*obj),
                                             IlluminaFormatterFactory_Init,
                                             IlluminaFormatterFactory_NewObj,
                                             IlluminaFormatterFactory_Release)) == 0 ) {
        obj = (IlluminaFormatterFactory*)*cself;
        obj->accession = accession;
        obj->table = table;
    }
    return rc;
}

/* ### External entry points ##################################################### */
const char UsageDefaultName[] = "illumina-dump";
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

rc_t IlluminaDumper_Release(const SRADumperFmt* fmt)
{
    if( fmt == NULL ) {
        return RC(rcExe, rcFormatter, rcDestroying, rcParam, rcInvalid);
    }
    return 0;
}

bool IlluminaDumper_AddArg(const SRADumperFmt* fmt, GetArg* f, int* i, int argc, char *argv[])
{
    const char* arg = NULL;
    const char* qseq = "2";

    if( fmt == NULL || f == NULL || i == NULL || argv == NULL ) {
        LOGERR(klogErr, RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid), "null param");
        return false;
    } else if( f(fmt, "r", "read", i, argc, argv, NULL) ) {
        IlluminaArgs.opt |= eRead;
    } else if( f(fmt, "x", "qseq", i, argc, argv, &qseq) ) {
        uint32_t q = AsciiToU32(qseq, NULL, NULL);
        if( q == 1 ) {
            IlluminaArgs.opt |= eQSeq_S;
            IlluminaArgs.opt &= ~eQSeq_M;
        } else if( q == 2 ) {
            IlluminaArgs.opt |= eQSeq_M;
            IlluminaArgs.opt &= ~eQSeq_S;
        } else {
            return false;
        }
    } else if( f(fmt, "q", "qual1", i, argc, argv, &arg) ) {
        uint32_t q = AsciiToU32(arg, NULL, NULL);
        if( q == 1 ) {
            IlluminaArgs.opt |= eQual1_S;
            IlluminaArgs.opt &= ~eQual1_M;
        } else if( q == 2 ) {
            IlluminaArgs.opt |= eQual1_M;
            IlluminaArgs.opt &= ~eQual1_S;
        } else {
            return false;
        }
    } else if( f(fmt, "p", "qual4", i, argc, argv, NULL) ) {
        IlluminaArgs.opt |= eQual4;
    } else if( f(fmt, "i", "intensity", i, argc, argv, NULL) ) {
        IlluminaArgs.opt |= eIntensity;
    } else if( f(fmt, "n", "noise", i, argc, argv, NULL) ) {
        IlluminaArgs.opt |= eNoise;
    } else if( f(fmt, "s", "signal", i, argc, argv, NULL) ) {
        IlluminaArgs.opt |= eSignal;
    } else {
        return false;
    }
    return true;
}

rc_t IlluminaDumper_Factories(const SRADumperFmt* fmt, const SRASplitterFactory** factory)
{
    rc_t rc = 0;
    const SRASplitterFactory* f_lane = NULL, *f_tile = NULL, *f_fmt = NULL;

    if( fmt == NULL || factory == NULL ) {
        return RC(rcExe, rcFormatter, rcReading, rcParam, rcInvalid);
    }
    *factory = NULL;

    if( IlluminaArgs.opt == 0 ) {
        IlluminaArgs.opt = eRead | eQual1_S;
    }

    if( (rc = IlluminaU32SplitterFactory_Make(&f_lane, fmt->accession, fmt->table, eIlluminaLaneSplitter)) == 0 ) {
        if( (rc = IlluminaU32SplitterFactory_Make(&f_tile, fmt->accession, fmt->table, eIlluminaTileSplitter)) == 0 ) {
            if( (rc = IlluminaFormatterFactory_Make(&f_fmt, fmt->accession, fmt->table)) == 0 ) {
                if( (rc = SRASplitterFactory_AddNext(f_tile, f_fmt)) == 0 ) {
                    f_fmt = NULL;
                    if( (rc = SRASplitterFactory_AddNext(f_lane, f_tile)) == 0 ) {
                        f_tile = NULL;
                        *factory = f_lane;
                    }
                }
            }
        }
    }
    if( rc != 0 ) {
        SRASplitterFactory_Release(f_lane);
        SRASplitterFactory_Release(f_tile);
        SRASplitterFactory_Release(f_fmt);
    }
    return rc;
}

/* main entry point of the file */
rc_t SRADumper_Init(SRADumperFmt* fmt)
{
    static const SRADumperFmt_Arg arg[] =
        {
            {"r", "read", NULL, {"Output READ: \"seq\", default is on", NULL}},
            {"q", "qual1", "1|2", {"Output QUALITY, whole spot (1) or split by reads (2): \"qcal\", default is 1", NULL}},
            {"p", "qual4", NULL, {"Output full QUALITY: \"prb\", default is off", NULL}},
            {"i", "intensity", NULL, {"Output INTENSITY, if present: \"int\", default is off", NULL}},
            {"n", "noise", NULL, {"Output NOISE, if present: \"nse\", default is off", NULL}},
            {"s", "signal", NULL, {"Output SIGNAL, if present: \"sig2\", default is off", NULL}},
            {"x", "qseq", "1|2", {"Output QSEQ format: whole spot (1) or split by reads: \"qseq\", default is off", NULL}},
            {NULL, NULL, NULL, {NULL}}
        };

    IlluminaArgs.opt = 0;

    if( fmt == NULL ) {
        return RC(rcExe, rcFileFormat, rcConstructing, rcParam, rcNull);
    }
    fmt->release = IlluminaDumper_Release;
    fmt->arg_desc = arg;
    fmt->add_arg = IlluminaDumper_AddArg;
    fmt->get_factory = IlluminaDumper_Factories;
    fmt->gzip = true;
    fmt->bzip2 = true;

    return 0;
}
