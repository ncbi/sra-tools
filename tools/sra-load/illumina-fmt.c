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
#include <os-native.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

typedef struct IlluminaLoaderFmt IlluminaLoaderFmt;
#define SRALOADERFMT_IMPL IlluminaLoaderFmt
#include "loader-fmt.h"

#include "writer-illumina.h"
#include "illumina-load.vers.h"
#include "debug.h"

typedef enum EIlluminaNativeFileType_enum {
    eIlluminaNativeFileTypeNotSet = 0,
    eIlluminaNativeFileTypeQuality4 = 0x01,
    eIlluminaNativeFileTypeQSeq = 0x02,
    eIlluminaNativeFileTypeFasta = 0x04,
    eIlluminaNativeFileTypeNoise = 0x08,
    eIlluminaNativeFileTypeIntensity = 0x10,
    eIlluminaNativeFileTypeSignal = 0x20
} EIlluminaNativeFileType;

struct {
    EIlluminaNativeFileType type;
    const char* key[3];
} const file_types[] = {
    /* order important, see WritaData loop */
    { eIlluminaNativeFileTypeSignal,
        { "_sig2.", NULL } },
    { eIlluminaNativeFileTypeIntensity,
        { "_int.", NULL } },
    { eIlluminaNativeFileTypeNoise,
        { "_nse.", NULL } },
    { eIlluminaNativeFileTypeFasta,
        { "_seq.", NULL } },
    { eIlluminaNativeFileTypeQSeq,
        { "_qseq.", ".qseq", NULL } },
    { eIlluminaNativeFileTypeQuality4,
        { "_prb.", NULL } }
};

typedef struct IlluminaFileInfo_struct {
    /* parsed data from file for a single spot */
    bool ready; /* if data is filled from file */
    EIlluminaNativeFileType type;
    struct IlluminaFileInfo_struct* prev;
    struct IlluminaFileInfo_struct* next;

    /* spot data */
    pstring name;
    pstring barcode;
    long coord[4]; /* lane, tile, x, y - sort-merge keys */

    IlluminaRead read;

    const SRALoaderFile* file;
    /* file line buffer */
    const char* line; /* not NULL if contains unprocessed data */
    size_t line_len;
} IlluminaFileInfo;

static
void IlluminaFileInfo_init(IlluminaFileInfo* file)
{
    assert(file);
    file->ready = false;
    pstring_clear(&file->name);
    memset(file->coord, 0, sizeof(file->coord));
    IlluminaRead_Init(&file->read, false);
}

/* reads 1 line from given file
 * if file->line pointer is not NULL line is in buffer already, nothing is read
 * failes on error or if line empty and not optional
 */
static
rc_t file_read_line(IlluminaFileInfo* file, bool optional)
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

/*
 * assumes tab separated file:
 * first 2 postiions concatinated with "_" into spot prefix
 * nextg 4 postiions concatinated with ":" into spot id: lane:tile:x:y
 * 7th (index) ignored
 * 8th is read id
 * 9th fasta
 * 10th quality
 * 11th (optional) read filter
 */
static
rc_t parse_qseq(IlluminaFileInfo* file, const char* data, size_t data_sz)
{
    rc_t rc = 0;
    const char* t, *str = data, *end = data + data_sz;
    int tabs = 0;
    do {
        if( (t = memchr(str, '\t', end - str)) != NULL ) {
            switch(++tabs) {
                case 1:
                    rc = pstring_assign(&file->name, str, t - str);
                    break;
                case 2:
                    if( (rc = pstring_append(&file->name, "_", 1)) == 0 ) {
                        rc = pstring_append(&file->name, str, t - str);
                    }
                    break;
                case 3:
                case 4:
                case 5:
                case 6:
                    errno = 0;
                    file->coord[tabs - 3] = strtol(str, NULL, 10);
                    if( errno != 0 ) {
                        file->coord[tabs - 3] = 0;
                    }
                    if( (rc = pstring_append(&file->name, ":", 1)) == 0 ) {
                        rc = pstring_append(&file->name, str, t - str);
                    }
                    break;
                case 7:
                    if( t - str != 1 || (*str != '0' && *str != '1') ) {
                        rc = pstring_assign(&file->barcode, str, t - str);
                    }
                    break;
                case 8:
                    if( t - str != 1 || !isdigit(*str) ) {
                        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInvalid);
                    } else {
                        file->read.read_id = *str - '0';
                        if( file->read.read_id == 0 ) {
                            file->read.read_id = ILLUMINAWRITER_READID_NONE;
                        }
                    }
                    break;
                case 9:
                    rc = pstring_assign(&file->read.seq, str, t - str);
                    break;
                case 10:
                    file->read.qual_type = ILLUMINAWRITER_COLMASK_QUALITY_PHRED;
                    rc = pstring_assign(&file->read.qual, str, t - str);
                    break;
            }
            str = ++t;
        }
    } while( rc == 0 && t != NULL && str < end );

    if( rc == 0 ) {
        if( tabs == 9 ) {
            file->read.qual_type = ILLUMINAWRITER_COLMASK_QUALITY_PHRED;
            rc = pstring_assign(&file->read.qual, str, end - str);
        } else if( tabs == 10 ) {
            if( end - str != 1 ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInvalid);
            } else if( *str == '1' ) {
                file->read.filter = SRA_READ_FILTER_PASS;
            } else if( *str == '0' ) {
                file->read.filter = SRA_READ_FILTER_REJECT;
            } else {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInvalid);
            }
        } else {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInvalid);
        }
        if( rc == 0 ) {
            if( file->read.seq.len != file->read.qual.len ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInconsistent);
            } else {
                rc = pstring_quality_convert(&file->read.qual, eExperimentQualityEncoding_Ascii, 64, 0, 0x7F);
            }
        }
    }
    return rc;
}

static
rc_t read_quality(const char* data, size_t data_sz, IlluminaRead* read)
{
    rc_t rc = 0;

    if( (rc = pstring_assign(&read->qual, data, data_sz)) == 0 ) {
        if( (rc = pstring_quality_convert(&read->qual, eExperimentQualityEncoding_Decimal, 0, -128, 127)) == 0 ) {
            read->qual_type = ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4;
        }
    }
    return rc;
}

static
rc_t read_spot_coord(IlluminaFileInfo* file, const char* data, size_t data_sz, const char** tail)
{
    rc_t rc = 0;
    const char* t, *str = data, *end = data + data_sz;
    int tabs = 0;

    if( tail ) {
        *tail = NULL;
    }
    do {
        if( (t = memchr(str, '\t', end - str)) != NULL ) {
            switch(++tabs) {
                case 1:
                    errno = 0;
                    file->coord[0] = strtol(str, NULL, 10);
                    if( errno != 0 ) {
                        file->coord[0] = 0;
                    }
                    rc = pstring_assign(&file->name, str, t - str);
                    break;
                case 2:
                case 3:
                case 4:
                    errno = 0;
                    file->coord[tabs - 1] = strtol(str, NULL, 10);
                    if( errno != 0 ) {
                        file->coord[tabs - 1] = 0;
                    }
                    if( (rc = pstring_append(&file->name, ":", 1)) == 0 ) {
                        rc = pstring_append(&file->name, str, t - str);
                    }
                    if( tail ) {
                        *tail = t + 1;
                    }
                    break;
            }
            str = ++t;
        }
    } while( rc == 0 && t != NULL && str < end && tabs < 4 );

    if( tabs < 4 ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcTooShort);
    }
    return rc;
}

static
rc_t read_signal(const char* data, size_t data_sz, pstring* str)
{
    rc_t rc = 0;
    double d;
    const char* c = data, *end = data + data_sz;
    float* f = (float*)str->data;
    float* fend = (float*)(str->data + sizeof(str->data));

    while( rc == 0 && c != end ) {
        char* next;
        errno = 0;
        d = strtod(c, &next);
        if( errno != 0 ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcOutofrange);
        } else if( d == 0 && c == next ) {
            break;
        } else {
            c = next;
            if( f >= fend ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcBuffer, rcInsufficient);
            }
            *f++ = d;
        }
    }
    if( rc == 0 ) {
        str->len = (f - (float*)str->data) * sizeof(*f);
        if( str->len == 0 ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcEmpty);
        }
    }
    return rc;
}

/* reads from a file data for a sinlge spot, data may be partial */
static
rc_t read_next_spot(const char* blk_pfx, IlluminaFileInfo* file)
{
    rc_t rc = 0;
    const char* tail = file->line;

    if( file->ready ) {
        /* data still not used */
        return 0;
    }
    IlluminaFileInfo_init(file);
    if( (rc = file_read_line(file, true)) != 0 ) {
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading more data");
    } else if( file->line == NULL ) {
        return 0; /* eof */
    }
    switch( file->type ) {
        case eIlluminaNativeFileTypeQSeq:
            if( (rc = parse_qseq(file, file->line, file->line_len)) != 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading qseq");
            }
            break;

        case eIlluminaNativeFileTypeFasta:
        case eIlluminaNativeFileTypeNoise:
        case eIlluminaNativeFileTypeIntensity:
        case eIlluminaNativeFileTypeSignal:
            {{
                /* read only common first 4 coords into name and prepend with DATA_BLOCK/@name */
                if( (rc = read_spot_coord(file, file->line, file->line_len, &tail)) == 0 ) {
                    if( blk_pfx != NULL ) {
                        pstring tmp_name;
                        if( (rc = pstring_copy(&tmp_name, &file->name)) == 0 &&
                            (rc = pstring_assign(&file->name, blk_pfx, strlen(blk_pfx))) == 0 &&
                            (rc = pstring_append(&file->name, ":", 1)) == 0 ) {
                            rc = pstring_concat(&file->name, &tmp_name);
                        }
                    }
                }
                if( rc != 0 ) {
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading spot coord");
                }
                break;
            }}

        case eIlluminaNativeFileTypeQuality4:
            if( (rc = read_quality(file->line, file->line_len, &file->read)) != 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading quality");
            } else if( (rc = pstring_assign(&file->name, blk_pfx, strlen(blk_pfx))) != 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=name for quality 4");
            }
            break;

        default:
            rc = RC(rcSRA, rcFormatter, rcReading, rcFileFormat, rcUnknown);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=processing data line");
            break;
    }

    /* process tail (after coords) for some file types */
    file->line_len -= tail - file->line; /* length of tail */
    switch( file->type ) {
        case eIlluminaNativeFileTypeQSeq:
        case eIlluminaNativeFileTypeQuality4:
        default:
            /* completely processed before */
            break;

        case eIlluminaNativeFileTypeFasta:
            if( (rc = pstring_assign(&file->read.seq, tail, file->line_len)) != 0 ||
                !pstring_is_fasta(&file->read.seq) ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcCorrupt);
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading fasta");
            }
            break;

        case eIlluminaNativeFileTypeNoise:
            if( (rc = read_signal(tail, file->line_len, &file->read.noise)) != 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=converting noise");
            }
            break;

        case eIlluminaNativeFileTypeIntensity:
            if( (rc = read_signal(tail, file->line_len, &file->read.intensity)) != 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=converting intensity");
            }
            break;

        case eIlluminaNativeFileTypeSignal:
            if( (rc = read_signal(tail, file->line_len, &file->read.signal)) != 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=converting signal");
            }
            break;
    }
    file->line = NULL;
    file->ready = true;
#if _DEBUGGING
    DEBUG_MSG(3, ("name:'%s' [%li:%li:%li:%li]\n", file->name.data, 
                file->coord[0], file->coord[1], file->coord[2], file->coord[3]));
    if( file->read.seq.len ) {
        DEBUG_MSG(3, ("seq:'%.*s'\n", file->read.seq.len, file->read.seq.data));
    }
    if( file->read.qual.len ) {
        DEBUG_MSG(3, ("qual{0x%x}: %u bytes\n", file->read.qual_type, file->read.qual.len));
    }
#endif
    return 0;
}

struct IlluminaLoaderFmt {
    SRALoaderFmt dad;
    const SRAWriterIllumina* writer;
    bool skip_intensity;
    bool skip_signal;
    bool skip_noise;
    int64_t spots_bad_allowed;
    int64_t spots_bad_count;
};

typedef struct FGroup_struct {
    SLNode dad;
    pstring key; /* part of filename w/o path up to ftypes value above */
    const char* blk_pfx;
    EIlluminaNativeFileType mask;
    IlluminaFileInfo* files;
} FGroup;

typedef struct FGroup_Find_data_struct {
    FGroup* found;
    pstring key;
} FGroup_Find_data;

bool FGroup_Find( SLNode *node, void *data )
{
    FGroup* n = (FGroup*)node;
    FGroup_Find_data* d = (FGroup_Find_data*)data;
    IlluminaFileInfo* file = n->files;

    while( file != NULL ) {
        if( pstring_cmp(&file->name, &d->key) == 0 ) {
            d->found = n;
            return true;
        }
        file = file->next;
    }
    if( pstring_cmp(&d->key, &n->key) == 0 ) {
        d->found = n;
        return true;
    }
    return false;
}

void FGroup_Validate( SLNode *node, void *data )
{
    rc_t* rc = (rc_t*)data;
    static EIlluminaNativeFileType mask = eIlluminaNativeFileTypeNotSet;
    FGroup* n = (FGroup*)node;
    IlluminaFileInfo* file = n->files;

    DEBUG_MSG(3, ("==> group: '%s'\n", n->key.data));
    while( file != NULL ) {
        DEBUG_MSG(3, ("file: type %u '%s'\n", file->type, file->name.data));
        if( mask == eIlluminaNativeFileTypeNotSet ) {
            mask = n->mask;
            if( !(mask & (eIlluminaNativeFileTypeFasta | eIlluminaNativeFileTypeQSeq)) ) {
                *rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcNotFound);
                SRALoaderFile_LOG(file->file, klogErr, *rc, "file group '$(p)*': sequence data", PLOG_S(p) , n->key.data);
            }
            if( (mask & eIlluminaNativeFileTypeFasta) && (mask & eIlluminaNativeFileTypeQSeq) ) {
                *rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcDuplicate);
                SRALoaderFile_LOG(file->file, klogErr, *rc, "file group '$(p)*': _seq and _qseq", PLOG_S(p) , n->key.data);
            }
            if( !(mask & eIlluminaNativeFileTypeQuality4) && !(mask & eIlluminaNativeFileTypeQSeq) ) {
                *rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcNotFound);
                SRALoaderFile_LOG(file->file, klogErr, *rc, "file group '$(p)*': quality data", PLOG_S(p) , n->key.data);
            }
        } else if( mask != n->mask ) {
            *rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInconsistent);
            SRALoaderFile_LOG(file->file, klogErr, *rc, "file group '$(p)*': no match in spot names on 1st lines across files in group",
                              PLOG_S(p) , n->key.data);
        }
        file = file->next;
    }
    DEBUG_MSG(3, ("<== group: '%s'\n", n->key.data));
}

void FGroup_Whack(SLNode *n, void *data )
{
    if( n != NULL ) {
        FGroup* node = (FGroup*)n;
        IlluminaFileInfo* file = node->files;

        while( file != NULL ) {
            IlluminaFileInfo* x = file;
            file = file->next;
            free(x);
        }
        free(node);
    }
}

typedef struct FGroup_Parse_data_struct {
    rc_t rc;
    IlluminaLoaderFmt* self;
    IlluminaSpot spot;
} FGroup_Parse_data;

bool FGroup_Parse( SLNode *n, void *d )
{
    FGroup_Parse_data* data = (FGroup_Parse_data*)d;
    FGroup* g = (FGroup*)n;
    bool done;
    const SRALoaderFile* data_block_ref = NULL;

    data->rc = 0;
    do {
        IlluminaFileInfo* file = g->files;
        done = true;
        while( data->rc == 0 && file != NULL ) {
            if( (data->rc = read_next_spot(g->blk_pfx, file)) == 0 && file->ready ) {
                done = false;
            }
            file = file->next;
        }
        if( data->rc != 0 || done ) {
            break;
        }
        /* collect spot reads, matching by spot name
         * spot data may be split across multiple files
         */
        IlluminaSpot_Init(&data->spot);
        file = g->files;
        while( data->rc == 0 && file != NULL ) {
            if( file->ready ) {
                if( (file->type == eIlluminaNativeFileTypeNoise && data->self->skip_noise) ||
                    (file->type == eIlluminaNativeFileTypeIntensity && data->self->skip_intensity) ||
                    (file->type == eIlluminaNativeFileTypeSignal && data->self->skip_signal) ) {
                    file->ready = false;
                } else {
                    data_block_ref = file->file;
                    if( file->type == eIlluminaNativeFileTypeQSeq && (g->mask & eIlluminaNativeFileTypeQuality4) ) {
                        /* drop quality1 from qseq data */
                        pstring_clear(&file->read.qual);
                    } else if( file->type == eIlluminaNativeFileTypeQuality4 ) {
                        IlluminaFileInfo* neib = file->next ? file->next : file->prev;
                        /* need to fix spotname to be same cause prb do not have any name in it */
                        if( (data->rc = pstring_copy(&file->name, &neib->name)) != 0 ) {
                            SRALoaderFile_LOG(file->file, klogErr, data->rc, "$(msg) '$(n)'", "msg=syncing prb spot name,n=%s", neib->name.data);
                        }
                    }
                    if( data->rc == 0 ) {
                        data->rc = IlluminaSpot_Add(&data->spot, &file->name, &file->barcode, &file->read);
                        if( data->rc == 0 ) {
                            file->ready = false;
                        } else {
                            if( GetRCState(data->rc) == rcIgnored ) {
                                SRALoaderFile_LOG(file->file, klogWarn, data->rc, "$(msg) '$(s1)' <> '$(s2)'",
                                                "msg=spot name mismatch,s1=%.*s,s2=%.*s",
                                                data->spot.name->len, data->spot.name->data, file->name.len, file->name.data);
                                data->self->spots_bad_count++;
                                /* skip spot for all files in a group */
                                file = g->files;
                                while( file != NULL ) {
                                    file->ready = false;
                                    SRALoaderFile_LOG(file->file, klogWarn, data->rc,
                                                      "$(msg) '$(n)'", "msg=skipped spot,n=%s", file->name.data);
                                    file = file->next;
                                }
                                if( data->self->spots_bad_allowed >= 0 &&
                                    data->self->spots_bad_count > data->self->spots_bad_allowed ) {
                                    data->rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcInvalid);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            file = file->next;
        }
        if( GetRCState(data->rc) == rcIgnored ) {
            data->rc = 0;
            continue;
        }
        if( data->rc == 0 ) {
            data->rc = SRAWriterIllumina_Write(data->self->writer, data_block_ref, &data->spot);
        }
    } while( data->rc == 0 );
    return data->rc != 0;
}

static
rc_t IlluminaLoaderFmt_WriteData(IlluminaLoaderFmt* self, uint32_t argc, const SRALoaderFile* const argv[], int64_t* spots_bad_count)
{
    rc_t rc = 0;
    uint32_t t, i, k, ftype_q = sizeof(file_types) / sizeof(file_types[0]);
    SLList files;
    IlluminaFileInfo* file = NULL;

    SLListInit(&files);

    /* group files using spotname, for _prb. file name prefix is used,
       files reviewed by type detected from name and ordered by file_type array */
    for(t = 0; rc == 0 && t < ftype_q; t++) {
        for(i = 0; rc == 0 && i < argc; i++) {
            const char* fname, *blk_pfx;
            int prefix_len = 0;
            ERunFileType ftype;
            EIlluminaNativeFileType type = eIlluminaNativeFileTypeNotSet;
            FGroup_Find_data data;

            if( (rc = SRALoaderFileName(argv[i], &fname)) != 0 ) {
                SRALoaderFile_LOG(argv[i], klogErr, rc, "reading file name", NULL);
                break;
            }
            if( (rc = SRALoaderFile_FileType(argv[i], &ftype)) != 0 ) {
                SRALoaderFile_LOG(argv[i], klogErr, rc, "reading file type", NULL);
                break;
            }
            if( (rc = SRALoaderFileBlockName(argv[i], &blk_pfx)) != 0 ) {
                SRALoaderFile_LOG(argv[i], klogErr, rc, "reading DATA_BLOCK/@name", NULL);
                break;
            }
            if( blk_pfx == NULL ) {
                blk_pfx = "";
            }
            {{
                /* skip path if present */
                const char* p = strrchr(fname, '/');
                fname = p ? p + 1 : fname;
                p = NULL;
                for(k = 0; type == eIlluminaNativeFileTypeNotSet && k < ftype_q; k++) {
                    const char* const* e = file_types[k].key;
                    while( *e != NULL ) {
                        p = strstr(fname, *e++);
                        if( p != NULL ) {
                            type = file_types[k].type;
                            break;
                        } 
                    }
                }
                if( p != NULL ) {
                    prefix_len = p - fname;
                }
            }}
            if( ftype == rft_IlluminaNativeSeq ) {
                type = eIlluminaNativeFileTypeFasta;
            } else if( ftype == rft_IlluminaNativePrb ) {
                type = eIlluminaNativeFileTypeQuality4;
            } else if( ftype == rft_IlluminaNativeInt ) {
                type = eIlluminaNativeFileTypeIntensity;
            } else if( ftype == rft_IlluminaNativeQseq ) {
                type = eIlluminaNativeFileTypeQSeq;
            }
            if( type == eIlluminaNativeFileTypeNotSet ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcUnrecognized);
                SRALoaderFile_LOG(argv[i], klogErr, rc, "detecting file type by file name", NULL);
                break;
            }
            if( type != file_types[t].type ) {
                /* one type at a time */
                continue;
            }
            DEBUG_MSG(3, ("file '%s' type set to %d\n", fname, type));
            file = calloc(1, sizeof(*file));
            if( file == NULL ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcMemory, rcExhausted);
                SRALoaderFile_LOG(argv[i], klogErr, rc, "allocating file object", NULL);
                break;
            }
            IlluminaFileInfo_init(file);
            file->file = argv[i];
            file->type = type;

            if( file->type == eIlluminaNativeFileTypeQuality4 ) {
                /* in _prb there is no spotname inside so use file prefix */
                rc = pstring_assign(&data.key, fname, prefix_len);
            } else {
                /* try to get 1st spot so group can be organized by spot name */
                if( (rc = read_next_spot(blk_pfx, file)) != 0 || !file->ready ) {
                    rc = rc ? rc : RC(rcSRA, rcFormatter, rcReading, rcData, rcNotFound);
                    SRALoaderFile_LOG(argv[i], klogErr, rc, "reading 1st spot", NULL);
                    break;
                }
                rc = pstring_copy(&data.key, &file->name);
            }

            data.found = NULL;
            if( SLListDoUntil(&files, FGroup_Find, &data) && data.found != NULL ) {
                IlluminaFileInfo* ss = data.found->files;

                while( rc == 0 && file != NULL ) {
                    if( ss->type != eIlluminaNativeFileTypeQSeq && ss->type == file->type ) {
                        rc = RC(rcSRA, rcFormatter, rcReading, rcFile, rcDuplicate);
                        SRALoaderFile_LOG(argv[i], klogErr, rc, "type of file for lane", NULL);
                    } else if( ss->next != NULL ) {
                        ss = ss->next;
                    } else {
                        ss->next = file;
                        file->prev = ss;
                        data.found->mask |= file->type;
                        file = NULL;
                    }
                }
            } else {
                data.found = calloc(1, sizeof(*data.found));
                if( data.found == NULL ) {
                    rc = RC(rcSRA, rcFormatter, rcReading, rcMemory, rcInsufficient);
                    SRALoaderFile_LOG(argv[i], klogErr, rc, "preparing file group", NULL);
                    break;
                } else {
                    if( (rc = pstring_assign(&data.found->key, fname, prefix_len)) != 0 ) {
                        SRALoaderFile_LOG(argv[i], klogErr, rc, "setting file group key", NULL);
                        FGroup_Whack(&data.found->dad, NULL);
                        break;
                    } else {
                        FGroup* curr = (FGroup*)SLListHead(&files), *prev = NULL;
                        data.found->blk_pfx = blk_pfx;
                        data.found->files = file;
                        data.found->mask = file->type;
                        /* group inserted into list by coords in 1st spot */
                        while( curr != NULL ) {
                            if( curr->files[0].coord[0] > file->coord[0] ||
                                (curr->files[0].coord[0] == file->coord[0] &&
                                 curr->files[0].coord[1] > file->coord[1]) ) {
                                data.found->dad.next = &curr->dad;
                                if( prev == NULL ) {
                                    files.head = &data.found->dad;
                                } else {
                                    prev->dad.next = &data.found->dad;
                                }
                                break;
                            }
                            prev = curr;
                            curr = (FGroup*)curr->dad.next;
                        }
                        if( curr == NULL ) {
                            SLListPushTail(&files, &data.found->dad);
                        }
                        file = NULL;
                    }
                }
            }
        }
    }
    if( rc == 0 ) {
        SLListForEach(&files, FGroup_Validate, &rc);
    }
    if( rc == 0 ) {
        FGroup_Parse_data data;
        data.self = self;
        if( SLListDoUntil(&files, FGroup_Parse, &data) ) {
            rc = data.rc;
        }
    } else {
        free(file);
    }
    SLListWhack(&files, FGroup_Whack, NULL);
    *spots_bad_count = self->spots_bad_count;
    return rc;
}

static
rc_t IlluminaLoaderFmt_Whack(IlluminaLoaderFmt *self, SRATable** table)
{
    SRAWriterIllumina_Whack(self->writer, table);
    free(self);
    return 0;
}

const char UsageDefaultName[] = "illumina-load";

uint32_t KAppVersion(void)
{
    return ILLUMINA_LOAD_VERS;
}

static
rc_t IlluminaLoaderFmt_Version (const IlluminaLoaderFmt* self, uint32_t *vers, const char** name )
{
    *vers = ILLUMINA_LOAD_VERS;
    *name = "Illumina native";
    return 0;
}

static SRALoaderFmt_vt_v1 vtIlluminaLoaderFmt =
{
    1, 0,
    IlluminaLoaderFmt_Whack,
    IlluminaLoaderFmt_Version,
    NULL,
    IlluminaLoaderFmt_WriteData
};

rc_t SRALoaderFmtMake(SRALoaderFmt **self, const SRALoaderConfig *config)
{
    rc_t rc = 0;
    IlluminaLoaderFmt* fmt;

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
    if( (rc = SRAWriterIllumina_Make(&fmt->writer, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize writer");
    } else if( (rc = SRALoaderFmtInit(&fmt->dad, (const SRALoaderFmt_vt*)&vtIlluminaLoaderFmt)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize parent object");
    }
    if( rc != 0 ) {
        IlluminaLoaderFmt_Whack(fmt, NULL);
    } else {
        fmt->skip_signal = (config->columnFilter & (efltrSIGNAL | efltrDEFAULT));
        fmt->skip_noise = (config->columnFilter & (efltrNOISE | efltrDEFAULT));
        fmt->skip_intensity = (config->columnFilter & (efltrINTENSITY | efltrDEFAULT));
        fmt->spots_bad_allowed = config->spots_bad_allowed;
        fmt->spots_bad_count = 0;
        *self = &fmt->dad;
    }
    return rc;
}
