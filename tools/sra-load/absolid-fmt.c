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

typedef struct AbiLoaderFmt AbiLoaderFmt;
#define SRALOADERFMT_IMPL AbiLoaderFmt
#include "loader-fmt.h"

#include "pstring.h"
#include "writer-absolid.h"
#include "abi-load.vers.h"
#include "debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

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

typedef struct AbiFileInfo_struct {
    /* parsed data from file for a single spot */
    bool ready; /* if data is filled from file */
    /* header */
    EFileType file_type;
    pstring name_prefix;
    /* spot data */
    EAbisolidReadType read_type;
    pstring name; /* name suffix */
    long coord[3]; /* panel, x, y - sort-merge keys */
    pstring data;
    char cs_key; /* only for csfasta file type */

    const SRALoaderFile* file;
    /* file line buffer */
    const char* line; /* not NULL if contains unprocessed data */
    size_t line_len;
} AbiFileInfo;

struct AbiLoaderFmt {
    SRALoaderFmt dad;
    bool skip_signal;
    const SRAWriteAbsolid* writer;
    AbsolidRead read[ABSOLID_FMT_MAX_NUM_READS];
};

static
void AbiFileInfo_init(AbiFileInfo* file)
{
    file->ready = false;
    file->read_type = eAbisolidReadType_Unknown;
    pstring_clear(&file->name);
    memset(file->coord, 0, sizeof(file->coord));
    pstring_clear(&file->data);
    file->cs_key = '\0';
}

/* reads 1 line from given file
 * if file->line pointer is not NULL line is in buffer already, nothing is read
 * failes on error or if line empty and not optional
 */
static
rc_t file_read_line(AbiFileInfo* file, bool optional)
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

static
bool is_csfasta(const char* str, const size_t len)
{
    const char* s = str, *end = s + len;

    if( s == end || strchr("ACGTacgt", *s++) == NULL ) {
        return false;
    }
    while( s != end ) {
        if( strchr("0123.", *s++) == NULL ) {
            return false;
        }
    }
    return true;
}

static
bool is_quality(const char* str, const size_t len)
{
    const char* s = str, *end = s + len;

    while( s != end ) {
        if( strchr("-+0123456789 ", *s++) == NULL ) {
            return false;
        }
    }
    return len != 0;
}

static
bool is_signal(const char* str, const size_t len)
{
    const char* s = str, *end = s + len;

    while( s != end ) {
        if( strchr("-+.0123456789Ee ", *s++) == NULL ) {
            return false;
        }
    }
    return len != 0;
}

static
rc_t read_signal(const char* data, size_t data_sz, pstring* str)
{
    rc_t rc = 0;
    double d;
    const char* c = data, *end = data + data_sz;

    pstring_clear(str);
    while( rc == 0 && c != end ) {
        char* next;
        errno = 0;
        d = strtod(c, &next);
        if( errno != 0 ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcOutofrange);
        } else if( d == 0 && c == next ) {
            break;
        } else {
            float f = d;
            c = next;
            rc = pstring_append(str, &f, sizeof(f));
        }
    }
    return rc;
}

static
rc_t parse_suffix(const char* s, size_t len, AbiFileInfo* file)
{
    rc_t rc = 0;
    int i;
    const char* p = s, *end = p + len;
    
    assert(s && file);
    while( isspace(*p) && p < end ) { ++p; }
    if( (rc = pstring_assign(&file->name, p, end - p)) != 0 ) {
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading name");
    }
    if( file->name.len <= 0 ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcFile, rcInvalid);
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=expected not empty name after '>'");
    }
    p = strrchr(file->name.data, '_');
    if( p != NULL ) {
        file->read_type = AbsolidRead_Suffix2ReadType(p + 1);
    } else {
        file->read_type = eAbisolidReadType_Unknown;
    }
    /* in native form each file has single read: 1st or 2nd. Can't be whole spot, so label must be always found */
    if( file->read_type <= eAbisolidReadType_SPOT ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcUnrecognized);
        return SRALoaderFile_LOG(file->file, klogErr, rc, "read name suffix is not recognized: '$(name)'", "name=%.*s",
                                 file->name.len, file->name.data);
    }
    file->name.len -= strlen(AbisolidReadType2ReadLabel[file->read_type]) + 1;
    /* try to get panel, x, y from name, format: \D*(\d+)\D+(\d+)\D+(\d+) */
    p = file->name.data + file->name.len;
    for(i = 2; rc == 0 && i >= 0; i--) {
        /* reverse parse */
        while( !isdigit(*--p) && p >= file->name.data );
        while( isdigit(*--p) && p >= file->name.data );
        errno = 0;
        file->coord[i] = strtol(p + 1, NULL, 10);
        if( errno != 0 ) {
            file->coord[i] =  0;
        }
    }
    return rc;
}

/* reads from a file data for a sinlge spot, data may be partial */
static
rc_t read_next_spot(AbiLoaderFmt* self, AbiFileInfo* file)
{
    rc_t rc = 0;

    if( file->ready ) {
        /* data still not used */
        return 0;
    }
    AbiFileInfo_init(file);
    do {
        if( (rc = file_read_line(file, true)) != 0 ) {
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading more data");
        } else if( file->line == NULL ) {
            return 0; /* eof */
        }
        if( file->line[0] != '#' ) {
            break;
        } else {
            const char* p = file->line, *end = p + file->line_len;
            while( isspace(*++p) && p < end );
            if( strncasecmp(p, "Title:", 6) == 0 ) {
                p += 5;
                while( isspace(*++p) && p < end );
                if( p < end ) {
                    if( (rc = pstring_assign(&file->name_prefix, p, end - p)) != 0 ){
                        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading Title");
                    }
                }
            }
            file->line = NULL;
        }
    } while( true );
    if( file->line[0] != '>' ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcFile, rcInvalid);
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=expected '>'");
    } else {
        if( (rc = parse_suffix(&file->line[1], file->line_len - 1, file)) != 0 ) {
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading name suffix");
        }
        file->line = NULL;
        if( (rc = file_read_line(file, false)) != 0 ) {
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading data line");
        }
        if( file->file_type == eNotSet ) {
            if( is_csfasta(file->line, file->line_len) ) {
                file->file_type = eCSFasta;
            } else if( is_quality(file->line, file->line_len) ) {
                file->file_type = eQuality;
            } else if( is_signal(file->line, file->line_len) ) {
                const char* fname, *p;
                if( (rc = SRALoaderFileName(file->file, &fname)) != 0 ) {
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=file name");
                }
                /* skip path if present */
                p = strrchr(fname, '/');
                p = p ? p : fname;
                if( strstr(p, "FTC") != NULL ) {
                    file->file_type = eSignalFTC;
                } else if( strstr(p, "CY3") != NULL ) {
                    file->file_type = eSignalCY3;
                } else if( strstr(p, "TXR") != NULL ) {
                    file->file_type = eSignalTXR;
                } else if( strstr(p, "CY5") != NULL ) {
                    file->file_type = eSignalCY5;
                } else {
                    rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcUnrecognized);
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=signal type by file name");
                }
            } else {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcUnrecognized);
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading data line");
            }
        }
        switch( file->file_type ) {
            case eCSFasta:
                if( !is_csfasta(file->line, file->line_len) ) {
                    rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcCorrupt);
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading csfasta");
                }
                file->cs_key = file->line[0];
                if( (rc = pstring_assign(&file->data, &file->line[1], file->line_len - 1)) != 0 ) {
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=copying csfasta");
                }
                break;
            case eQuality:
                if( (rc = pstring_assign(&file->data, file->line, file->line_len) != 0 ) ||
                    (rc = pstring_quality_convert(&file->data, eExperimentQualityEncoding_Decimal, 0, -1, 0x7F)) != 0 ) {
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=converting quality");
                }
                break;
            case eSignalFTC:
            case eSignalCY3:
            case eSignalTXR:
            case eSignalCY5:
                if( (rc = read_signal(file->line, file->line_len, &file->data)) != 0 ) {
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=converting signal");
                }
                break;
            default:
                rc = RC(rcSRA, rcFormatter, rcReading, rcFileFormat, rcUnknown);
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=processing data line");
                break;
        }
        file->line = NULL;
        file->ready = true;
    }
#if _DEBUGGING
    DEBUG_MSG(3, ("READ: type:%i, pfx:%s, rd_typ:%s, name:'%s', %li:%li:%li",
                  file->file_type, file->name_prefix.data, AbisolidReadType2ReadLabel[file->read_type],
                  file->name.data, file->coord[0], file->coord[1], file->coord[2]));
    if( file->cs_key ) {
        DEBUG_MSG(3, (", cs_key %c, data [%u] '%.*s'\n",
                      file->cs_key, file->data.len, file->data.len, file->data.data));
    } else {
        DEBUG_MSG(3, (", data length: %u\n", file->data.len));
    }
#endif
    return 0;
}

static
rc_t AbiLoaderFmt_WriteData(AbiLoaderFmt* self, uint32_t argc, const SRALoaderFile* const argv[], int64_t* spots_bad_count)
{
    rc_t rc = 0;
    uint32_t i, origin;
    AbiFileInfo* files = NULL;
    bool done;
    pstring spotname;

    if( (files = calloc(argc, sizeof(*files))) == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcMemory, rcInsufficient);
    }
    for(i = 0; rc == 0 && i < argc; i++) {
        AbiFileInfo* file = &files[i];
        AbiFileInfo_init(file);
        file->file = argv[i];
        file->file_type = eNotSet;
    }
    do {
        done = true;
        for(i = 0; rc == 0 && i < argc; i++) {
            AbiFileInfo* file = &files[i];
            if( (rc = read_next_spot(self, file)) != 0 || !file->ready ) {
                continue;
            }
            done = false;
        }
        if( rc != 0 || done ) {
            break;
        }
        /* find read with min coords */
        origin = argc;
        for(i = 0; i < argc; i++) {
            AbiFileInfo* file = &files[i];
            if( file->ready && (origin == argc ||
                                file->coord[0] < files[origin].coord[0] ||
                                (file->coord[0] == files[origin].coord[0] &&
                                 file->coord[1] < files[origin].coord[1]) ||
                                (file->coord[0] == files[origin].coord[0] &&
                                 file->coord[1] == files[origin].coord[1] &&
                                 file->coord[2] < files[origin].coord[2]) ) ) {
                origin = i;
            }
        }
        assert(origin != argc);
        /* collect spot reads, matching by spot name
         * spot data may be split across multiple files
         */
        for(i = 0; i < sizeof(self->read) / sizeof(self->read[0]); i++) {
            AbsolidRead_Init(&self->read[i]);
        }
        rc = SRAWriteAbsolid_MakeName(&files[origin].name_prefix, &files[origin].name, &spotname);
        for(i = 0; rc == 0 && i < argc; i++) {
            AbiFileInfo* file = &files[i];
            if( file->ready ) {
                pstring tmpname;
                if( (rc = SRAWriteAbsolid_MakeName(&file->name_prefix, &file->name, &tmpname)) == 0 ) {
                    if( pstring_cmp(&spotname, &tmpname) == 0 ) {
                        int read_number = AbisolidReadType2ReadNumber[file->read_type];
                        switch( file->file_type ) {
                            case eNotSet:
                            case eFileTypeMax:
                                rc = RC(rcSRA, rcFormatter, rcReading, rcFileFormat, rcUnknown);
                                SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=merging spot data");
                                break;
                            case eCSFasta:
                                if( self->read[read_number].seq.len > 0 ) {
                                    rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
                                    SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=sequence");
                                } else {
                                    self->read[read_number].cs_key = file->cs_key;
                                    rc = pstring_copy(&self->read[read_number].seq, &file->data);
                                }
                                break;
                            case eQuality:
                                if( self->read[read_number].qual.len > 0 ) {
                                    rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
                                    SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=quality");
                                } else {
                                    rc = pstring_copy(&self->read[read_number].qual, &file->data);
                                }
                                break;
                            case eSignalFTC:
                                if( !self->skip_signal ) {
                                    if( self->read[read_number].fxx.len > 0 ) {
                                        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
                                        SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=FTC");
                                    } else {
                                        self->read[read_number].fs_type = eAbisolidFSignalType_FTC;
                                        rc = pstring_copy(&self->read[read_number].fxx, &file->data);
                                    }
                                }
                                break;
                            case eSignalCY3:
                                if( !self->skip_signal ) {
                                    if( self->read[read_number].cy3.len > 0 ) {
                                        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
                                        SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=CY3");
                                    } else {
                                        rc = pstring_copy(&self->read[read_number].cy3, &file->data);
                                    }
                                }
                                break;
                            case eSignalTXR:
                                if( !self->skip_signal ) {
                                    if( self->read[read_number].txr.len > 0 ) {
                                        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
                                        SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=TXR");
                                    } else {
                                        rc = pstring_copy(&self->read[read_number].txr, &file->data);
                                    }
                                }
                                break;
                            case eSignalCY5:
                                if( !self->skip_signal ) {
                                    if( self->read[read_number].cy5.len > 0 ) {
                                        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
                                        SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=CY5");
                                    } else {
                                        rc = pstring_copy(&self->read[read_number].cy5, &file->data);
                                    }
                                }
                                break;
                        }
                        if( rc == 0 ) {
                            const char* l = AbisolidReadType2ReadLabel[file->read_type];
                            rc = pstring_assign(&self->read[read_number].label, l, strlen(l));
                        }
                        file->ready = false;
                    }
                }
            }
        }
        /* replace -1 quality for '.' to 0 */
        for(i = 0; rc == 0 && i < ABSOLID_FMT_MAX_NUM_READS; i++) {
            int j;
            for(j = 0; rc == 0 && j < self->read[i].qual.len; j++ ) {
                if( self->read[i].qual.data[j] == -1 ) {
                    if( self->read[i].seq.data[j] == '.' ) {
                        self->read[i].qual.data[j] = 0;
                    } else {
                        SRALoaderFile_LOG(files[0].file, klogErr, rc, "$(msg)", "msg=quality is -1 not for '.'");
                        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInvalid);
                    }
                }
            }
        }
        if( rc == 0 ) {
            rc = SRAWriteAbsolid_Write(self->writer, argv[0], &spotname, NULL,
                                       self->read[0].label.len > 0 ? &self->read[0] : NULL,
                                       self->read[1].label.len > 0 ? &self->read[1] : NULL);
        }
    } while( rc == 0 );
    free(files);
    return rc;
}

static
rc_t AbiLoaderFmt_Whack(AbiLoaderFmt *self, SRATable** table)
{
    SRAWriteAbsolid_Whack(self->writer, table);
    free(self);
    return 0;
}

const char UsageDefaultName[] = "abi-load";

uint32_t KAppVersion(void)
{
    return ABI_LOAD_VERS;
}

static
rc_t AbiLoaderFmt_Version (const AbiLoaderFmt* self, uint32_t *vers, const char** name )
{
    *vers = ABI_LOAD_VERS;
    *name = "AB SOLiD native";
    return 0;
}

static SRALoaderFmt_vt_v1 vtAbiLoaderFmt =
{
    1, 0,
    AbiLoaderFmt_Whack,
    AbiLoaderFmt_Version,
    NULL,
    AbiLoaderFmt_WriteData
};

rc_t SRALoaderFmtMake(SRALoaderFmt **self, const SRALoaderConfig *config)
{
    rc_t rc = 0;
    AbiLoaderFmt* fmt;

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
    if( (rc = SRAWriteAbsolid_Make(&fmt->writer, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize writer");
    } else if( (rc = SRALoaderFmtInit(&fmt->dad, (const SRALoaderFmt_vt*)&vtAbiLoaderFmt)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize parent object");
    }
    /* Set result or free obj */
    if( rc != 0 ) {
        AbiLoaderFmt_Whack(fmt, NULL);
    } else {
        fmt->skip_signal = (config->columnFilter & (efltrSIGNAL | efltrDEFAULT));
        *self = &fmt->dad;
    }
    return rc;
}
