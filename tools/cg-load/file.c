/*==============================================================================
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
*/
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/status.h>
#include <kfs/directory.h>
#include <kapp/loader-file.h>
#include <strtol.h>

#include "defs.h"
#include "file.h"
#include "factory-file.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

rc_t CGLoaderFile_IsEof(const CGLoaderFile* cself, bool* eof)
{
    return KLoaderFile_IsEof(cself ? cself->file : NULL, eof);
}

rc_t CGLoaderFile_Close(const CGLoaderFile* cself)
{
    const char* nm;

    if( KLoaderFile_FullName(cself ? cself->file : NULL, &nm) == 0 ) {
        PLOGMSG(klogInfo, (klogInfo, "File $(file) done", "severity=status,file=%s", nm));
    }
    return KLoaderFile_Close(cself ? cself->file : NULL);
}

rc_t CGLoaderFile_Line(const CGLoaderFile* cself, uint64_t* line)
{
    return KLoaderFile_Line(cself ? cself->file : NULL, line);
}


rc_t CGLoaderFile_Readline(const CGLoaderFile* cself, const void** buffer, size_t* length)
{
    return KLoaderFile_Readline(cself ? cself->file : NULL, buffer, length);
}

rc_t CGLoaderFile_Filename(const CGLoaderFile* cself, const char** name)
{
    return KLoaderFile_Name(cself ? cself->file : NULL, name);
}

rc_t CGLoaderFile_LOG(const CGLoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, ...)
{
    if( cself != NULL ) {
        va_list args;
        va_start(args, fmt);
        rc = KLoaderFile_VLOG(cself->file, lvl, rc, msg, fmt, args);
        va_end(args);
    }
    return rc;
}

rc_t CGLoaderFile_Release(const CGLoaderFile* cself, bool ignored)
{
    rc_t rc = 0;

    if( cself ) {
        CGLoaderFile* self = (CGLoaderFile*)cself;
        /* may return md5 check error here */
        if( self->cg_file && self->cg_file->vt->destroy ) {
            uint64_t recs = 0;
            self->cg_file->vt->destroy(self->cg_file, &recs);
            if( rc == 0 && !ignored ) {
                const char* name = NULL;
                CGLoaderFile_Filename(cself, &name);
                STSMSG(0, ("file %s %lu records", name, recs));
            }
        }
        rc = KLoaderFile_Release(self->file, ignored);
        free(self);
    }
    return rc;
}

static
rc_t parse_version(const char* buf, size_t len, uint32_t* v)
{
    rc_t rc = 0;
    int i = 0;
    int64_t q;
    char* end = (char*)buf;
    const size_t sz = sizeof(*v) / sizeof(char);

    *v = 0;
    do {
        if( i == sz ) {
            rc = RC(rcRuntime, rcHeader, rcConstructing, rcFile, rcBadVersion);
        } else {
            q = strtoi64(end, &end, 10);
            if( q < 0 || q > 255 ) {
                rc = RC(rcRuntime, rcHeader, rcConstructing, rcFile, rcBadVersion);
            } else if( *end == '.' ) {
                end++;
            } else if( end - buf < len ) {
                rc = RC(rcRuntime, rcHeader, rcConstructing, rcFile, rcBadVersion);
            }
            *v = *v | ((uint8_t)q << ((sz - ++i) * 8));
        }
    } while( rc == 0 && end - buf < len);
    return rc;
}

static
rc_t CGLoaderFile_header(const CGLoaderFile* cself)
{
    rc_t rc = 0;

    if( cself->cg_file == NULL ) {
        CGLoaderFile* self = (CGLoaderFile*)cself;
        const char* buf;
        size_t len;
        uint32_t fver = 0;
        char type[64];

        do {
            if( (rc = CGLoaderFile_Readline(self, (const void**)&buf, &len)) == 0 ) {
                if( buf == NULL ) {
                    rc = RC(rcRuntime, rcFile, rcConstructing, rcData, rcTooShort);
                } else if( len == 0 ) {
                    /* empty line: skip */
                } else if( buf[0] == '>' ) {
                    /* start of records */
                    break;
                } else if( buf[0] == '#' ) {
                    len--; buf++;
                    if( strncmp("FORMAT_VERSION\t", buf, 15) == 0 ) {
                        rc = parse_version(&buf[15], len - 15, &fver);
                    } else if( strncmp("TYPE\t", buf, 5) == 0 ) {
                        rc = str2buf(&buf[5], len - 5, type, sizeof(type));
                    }
                } else {
                    rc = RC(rcRuntime, rcFile, rcConstructing, rcData, rcUnrecognized);
                }
            }
        } while( rc == 0 && self->cg_file == NULL );
        if( rc == 0 ) {
            rc = CGLoaderFile_CreateCGFile(self, fver, type);

            if( rc == 0 ) {
                CGFileType* f = (CGFileType*)self->cg_file;
                f->format_version = fver;
                if( f->type == cg_eFileType_Unknown || !f->vt || !f->vt->header ) {
                    rc = RC(rcRuntime, rcFile, rcConstructing, rcInterface, rcIncomplete);
                }
            }
            if( rc == 0 ) {
                /* we need to restart file for loading all header by sub class */
                KLoaderFile_Reset(self->file);
            }
            while( rc == 0 ) {
                if( (rc = CGLoaderFile_Readline(self, (const void**)&buf, &len)) == 0 ) {
                    if( buf == NULL ) {
                        rc = RC(rcRuntime, rcFile, rcConstructing, rcData, rcTooShort);
                    } else if( len == 0 ) {
                        /* empty line: skip */
                    } else if( buf[0] == '>' ) {
                        /* start of records */
                        break;
                    } else if( buf[0] == '#' ) {
                        if( strncmp("FORMAT_VERSION\t", &buf[1], 15) != 0 && strncmp("TYPE\t", &buf[1], 5) != 0 ) {
                            rc = cself->cg_file->vt->header(cself->cg_file, &buf[1], len - 1);
                        }
                    } else {
                        rc = RC(rcRuntime, rcFile, rcConstructing, rcData, rcUnrecognized);
                    }
                }
            }
        }
        /* close file after file is processed, but stay at data start */
        KLoaderFile_SetReadAhead(self->file, self->read_ahead);
        KLoaderFile_Close(self->file);
    }

    return rc;
}

rc_t CGLoaderFile_Make(const CGLoaderFile** cself, const KDirectory* dir, const char* filename,
                       const uint8_t* md5_digest, bool read_ahead)
{
    rc_t rc = 0;
    CGLoaderFile* obj = NULL;

    if( cself == NULL ) {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcSelf, rcNull);
    } else if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcMemory, rcExhausted);
    } else if( (rc = KLoaderFile_Make(&obj->file, dir, filename, md5_digest, false)) == 0 ) {
        obj->read_ahead = read_ahead;
    }
    if( rc == 0 ) {
        *cself = obj;
    } else {
        CGLoaderFile_Release(obj, true);
    }
    return rc;
}

rc_t CGLoaderFile_GetType(const CGLoaderFile* cself, CG_EFileType* type)
{
    rc_t rc = 0;

    if( cself == NULL || type == NULL ) {
        rc = RC(rcRuntime, rcFile, rcClassifying, rcParam, rcNull);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        *type = cself->cg_file->type;
    }
    return rc;
}

rc_t CGLoaderFile_GetRead(const CGLoaderFile* cself, TReadsData* data)
{
    rc_t rc = 0;

    if( cself == NULL || data == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->type != cg_eFileType_READS ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( cself->cg_file->vt->reads == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcIncomplete);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->reads(cself->cg_file, data);
    }
    return rc;
}

rc_t CGLoaderFile_GetTagLfr(const CGLoaderFile* cself, TReadsData* data)
{
    rc_t rc = 0;

    if( cself == NULL || data == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->type != cg_eFileType_TAG_LFR ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( cself->cg_file->vt->tag_lfr == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcIncomplete);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->tag_lfr(cself->cg_file, data);
    }
    return rc;
}

rc_t CGLoaderFile_GetStartRow(const CGLoaderFile* cself, int64_t* rowid)
{
    rc_t rc = 0;

    if( cself == NULL || rowid == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->type != cg_eFileType_READS ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( cself->cg_file->vt->get_start_row == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcIncomplete);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->get_start_row(cself->cg_file, rowid);
    }
    return rc;
}

rc_t CGLoaderFile_GetMapping(const CGLoaderFile* cself, TMappingsData* data)
{
    rc_t rc = 0;

    if( cself == NULL || data == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->type != cg_eFileType_MAPPINGS ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( cself->cg_file->vt->mappings == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcIncomplete);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->mappings(cself->cg_file, data);
    }
    return rc;
}

rc_t CGLoaderFile_GetEvidenceIntervals(const CGLoaderFile* cself, TEvidenceIntervalsData* data)
{
    rc_t rc = 0;

    if( cself == NULL || data == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->type != cg_eFileType_EVIDENCE_INTERVALS ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( cself->cg_file->vt->evidence_intervals == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcIncomplete);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->evidence_intervals(cself->cg_file, data);
    }
    return rc;
}

rc_t CGLoaderFile_GetEvidenceDnbs(const CGLoaderFile* cself, const char* interval_id, TEvidenceDnbsData* data)
{
    rc_t rc = 0;

    if( cself == NULL || interval_id == NULL || data == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->type != cg_eFileType_EVIDENCE_DNBS ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( cself->cg_file->vt->evidence_dnbs == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcIncomplete);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->evidence_dnbs(cself->cg_file, interval_id, data);
    }
    return rc;
}

rc_t CGLoaderFile_GetAssemblyId(const CGLoaderFile* cself, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id)
{
    rc_t rc = 0;

    if( cself == NULL || assembly_id == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->vt->assembly_id == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->assembly_id(cself->cg_file, assembly_id);
    }
    return rc;
}

rc_t CGLoaderFile_GetSlide(const CGLoaderFile* cself, const CGFIELD_SLIDE_TYPE** slide)
{
    rc_t rc = 0;

    if( cself == NULL || slide == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->vt->slide == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->slide(cself->cg_file, slide);
    }
    return rc;
}

rc_t CGLoaderFile_GetLane(const CGLoaderFile* cself, const CGFIELD_LANE_TYPE** lane)
{
    rc_t rc = 0;

    if( cself == NULL || lane == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->vt->lane == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->lane(cself->cg_file, lane);
    }
    return rc;
}

rc_t CGLoaderFile_GetBatchFileNumber(const CGLoaderFile* cself, const CGFIELD_BATCH_FILE_NUMBER_TYPE** batch_file_number)
{
    rc_t rc = 0;

    if( cself == NULL || batch_file_number == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->vt->batch_file_number == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->batch_file_number(cself->cg_file, batch_file_number);
    }
    return rc;
}

rc_t CGLoaderFile_GetSample(const CGLoaderFile* cself, const CGFIELD_SAMPLE_TYPE** sample)
{
    rc_t rc = 0;

    if( cself == NULL || sample == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->vt->sample == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->sample(cself->cg_file, sample);
    }
    return rc;
}

rc_t CGLoaderFile_GetChromosome(const CGLoaderFile* cself, const CGFIELD_CHROMOSOME_TYPE** chromosome)
{
    rc_t rc = 0;

    if( cself == NULL || chromosome == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcParam, rcNull);
    } else if( cself->cg_file->vt->chromosome == NULL ) {
        rc = RC(rcRuntime, rcFile, rcReading, rcInterface, rcUnsupported);
    } else if( (rc = CGLoaderFile_header(cself)) == 0 ) {
        rc = cself->cg_file->vt->chromosome(cself->cg_file, chromosome);
    }
    return rc;
}
