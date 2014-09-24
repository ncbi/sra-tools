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
#include <klib/log.h>
#include <klib/rc.h>
#include <os-native.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

typedef struct HelicosLoaderFmt HelicosLoaderFmt;
#define SRALOADERFMT_IMPL HelicosLoaderFmt
#include "loader-fmt.h"

#include "pstring.h"
#include "writer-helicos.h"
#include "helicos-load.vers.h"
#include "debug.h"

typedef enum EFileType_enum {
    eNotSet = -1,
    eCSFasta,
    eQuality,
    eSignalFTC,
    eSignalCY3,
    eSignalTXR,
    eSignalCY5,
    eFileTypeMax
} EFileType;

typedef struct HelicosFileInfo_struct {
    /* parsed data from file for a single spot */
    bool ready; /* if data is filled from file */
    /* spot data */
    pstring name;
    pstring sequence;
    pstring quality;

    const SRALoaderFile* file;
    /* file line buffer */
    const char* line; /* not NULL if contains unprocessed data */
    size_t line_len;
} HelicosFileInfo;

struct HelicosLoaderFmt {
    SRALoaderFmt dad;
    const SRAWriterHelicos* writer;
};

static
void HelicosFileInfo_init(HelicosFileInfo* file)
{
    file->ready = false;
    pstring_clear(&file->name);
    pstring_clear(&file->sequence);
    pstring_clear(&file->quality);
}

/* reads 1 line from given file
 * if file->line pointer is not NULL line is in buffer already, nothing is read
 * failes on error or if line empty and not optional
 */
static
rc_t file_read_line(HelicosFileInfo* file, bool optional)
{
    rc_t rc = 0;

    if( file->line == NULL ) {
        if( (rc = SRALoaderFileReadline(file->file, (const void**)&file->line, &file->line_len)) == 0 ) {
            if( file->line == NULL || file->line_len == 0 ) {
                if( !optional ) {
                    rc = RC(rcSRA, rcFormatter, rcReading, rcString, rcInsufficient);
                }
            }
            if( rc == 0 && file->line != NULL ) {
                if( file->line_len > 0 ) {
                    const char* e = file->line + file->line_len;
                    /* right trim */
                    while( --e >= file->line ) {
                        if( !isspace(*e) ) {
                            break;
                        }
                        file->line_len--;
                    }
                }
            }
        }
    }
    return rc;
}

/* reads from a file data for a sinlge spot, data may be partial */
static
rc_t read_next_spot(HelicosLoaderFmt* self, HelicosFileInfo* file)
{
    rc_t rc = 0;

    if( file->ready ) {
        /* data still not used */
        return 0;
    }
    HelicosFileInfo_init(file);
    if( (rc = file_read_line(file, true)) != 0 ) {
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading more data");
    } else if( file->line == NULL ) {
        return 0; /* eof */
    }
    if( file->line[0] == '@' ) { /*** fastq format **/
        if( (rc = pstring_assign(&file->name, &file->line[1], file->line_len - 1)) != 0 ) {
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading name");
        }
        file->line = NULL;
        if( (rc = file_read_line(file, false)) != 0 || file->line_len > sizeof(file->sequence.data)-1 ||
            (rc = pstring_assign(&file->sequence, file->line, file->line_len)) != 0 ||
            !pstring_is_fasta(&file->sequence) ) {
            rc = rc ? rc : RC(rcSRA, rcFormatter, rcReading, rcData, rcUnrecognized);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading sequence");
        }
        file->line = NULL;
        if( (rc = file_read_line(file, false)) != 0 ||
            file->line[0] != '+' || file->line_len != 1 ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcCorrupt);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading quality defline");
        }
        file->line = NULL;
        if( (rc = file_read_line(file, false)) != 0 || file->line_len > sizeof(file->quality.data)-1 ||
            (rc = pstring_assign(&file->quality, file->line, file->line_len)) != 0 ||
            (rc = pstring_quality_convert(&file->quality, eExperimentQualityEncoding_Ascii, 33, 0, 0x7F)) != 0 ) {
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading quality");
        }
        file->line = NULL;
        file->ready = true;
    } else if( file->line[0] == '>' ) { /** fasta format **/
	if( (rc = pstring_assign(&file->name, &file->line[1], file->line_len - 1)) != 0 ) {
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading name");
        }
        file->line = NULL;
	if( (rc = file_read_line(file, false)) != 0 || file->line_len > sizeof(file->sequence.data)-1 ||
            (rc = pstring_assign(&file->sequence, file->line, file->line_len)) != 0 ||
            !pstring_is_fasta(&file->sequence) ) {
            rc = rc ? rc : RC(rcSRA, rcFormatter, rcReading, rcData, rcUnrecognized);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading sequence");
        }
	file->line = NULL;
	file->quality.len = file->sequence.len;
	memset(file->quality.data,14,file->quality.len);
	file->ready = true;
    } else {
        rc = RC(rcSRA, rcFormatter, rcReading, rcFile, rcInvalid);
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=expected '@'");
    }
#if _DEBUGGING
 DEBUG_MSG(3, ("READ: name:'%s', seq[%u]:'%s', qual[%u]\n", file->name.data,
                file->sequence.len, file->sequence.data, file->quality.len)); /*
    DEBUG_MSG(3, ("READ: name:'%s', seq[%u]:'%s', qual[%u]:'%s'\n", file->name.data,
                file->sequence.len, file->sequence.data, file->quality.len, file->quality.data));*/
#endif
    return 0;
}

static
rc_t HelicosLoaderFmt_WriteData(HelicosLoaderFmt* self, uint32_t argc, const SRALoaderFile* const argv[], int64_t* spots_bad_count)
{
    rc_t rc = 0;
    uint32_t i;
    HelicosFileInfo* files = NULL;
    bool done;

    if( (files = calloc(argc, sizeof(*files))) == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcMemory, rcInsufficient);
    }
    for(i = 0; rc == 0 && i < argc; i++) {
        HelicosFileInfo* file = &files[i];
        HelicosFileInfo_init(file);
        file->file = argv[i];
    }
    do {
        done = true;
        for(i = 0; rc == 0 && i < argc; i++) {
            HelicosFileInfo* file = &files[i];
            if( (rc = read_next_spot(self, file)) == 0 && file->ready ) {
                done = false;
                rc = SRAWriterHelicos_Write(self->writer, argv[0], &file->name, &file->sequence, &file->quality);
                file->ready = false;
            }
        }
    } while( rc == 0 && !done );
    free(files);
    return rc;
}

static
rc_t HelicosLoaderFmt_Whack(HelicosLoaderFmt *self, SRATable** table)
{
    SRAWriterHelicos_Whack(self->writer, table);
    free(self);
    return 0;
}

const char UsageDefaultName[] = "helicos-load";

uint32_t KAppVersion(void)
{
    return HELICOS_LOAD_VERS;
}

static
rc_t HelicosLoaderFmt_Version (const HelicosLoaderFmt* self, uint32_t *vers, const char** name )
{
    *vers = HELICOS_LOAD_VERS;
    *name = "Helicos";
    return 0;
}

static SRALoaderFmt_vt_v1 vtHelicosLoaderFmt =
{
    1, 0,
    HelicosLoaderFmt_Whack,
    HelicosLoaderFmt_Version,
    NULL,
    HelicosLoaderFmt_WriteData
};

rc_t SRALoaderFmtMake(SRALoaderFmt **self, const SRALoaderConfig *config)
{
    rc_t rc = 0;
    HelicosLoaderFmt* fmt;

    if( self == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    *self = NULL;
    fmt = calloc(1, sizeof(*fmt));
    if(fmt == NULL) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        LOGERR(klogInt, rc, "failed to initialize; out of memory");
        return rc;
    }
    if( (rc = SRAWriterHelicos_Make(&fmt->writer, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize writer");
    } else if( (rc = SRALoaderFmtInit(&fmt->dad, (const SRALoaderFmt_vt*)&vtHelicosLoaderFmt)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize parent object");
    }
    /* Set result or free obj */
    if( rc != 0 ) {
        HelicosLoaderFmt_Whack(fmt, NULL);
    } else {
        *self = &fmt->dad;
    }
    return rc;
}
