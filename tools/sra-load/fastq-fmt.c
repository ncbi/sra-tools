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

typedef struct FastqLoaderFmt FastqLoaderFmt;
#define SRALOADERFMT_IMPL FastqLoaderFmt
#include "loader-fmt.h"

#include "writer-illumina.h"
#include "writer-454.h"
#include "writer-ion-torrent.h"
#include "experiment-xml.h"
#include "debug.h"

typedef struct FileReadData_struct {
    bool ready; /* if data is filled from file */
    pstring name;
    pstring barcode;
    IlluminaRead read;
} FileReadData;

typedef struct FastqFileInfo_struct {
    const SRALoaderFile* file;
    /* parsed data from file for single spot */
    FileReadData spot[2]; /* 2nd element is used for 8 line format file only */

    int qualType;
    ExperimentQualityEncoding qualEnc;
    uint8_t qualOffset;
    int8_t qualMin;
    int8_t qualMax;

    /* file line buffer */
    const char* line; /* not NULL if contains unprocessed data */
    size_t line_len;
} FastqFileInfo;

struct FastqLoaderFmt {
    SRALoaderFmt dad;
    const SRAWriterIllumina* wIllumina;
    const SRAWriter454* w454;
    const SRAWriterIonTorrent* wIonTorrent;

    const ProcessingXML* processing;
    int64_t spots_bad_allowed;
    int64_t spots_bad_count;
};

static
void FileReadData_init(FileReadData* read, bool name_only)
{
    pstring_clear(&read->name);
    pstring_clear(&read->barcode);
    IlluminaRead_Init(&read->read, name_only);
    if( !name_only ) {
        read->ready = false;
    }
}

/* reads 1 line from given file
 * if file->line pointer is not NULL line is in buffer already, nothing is read
 * failes on error or if line empty and not optional
 */
static
rc_t file_read_line(FastqFileInfo* file, bool optional)
{
    rc_t rc = 0;
static unsigned long lineNo=0;

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
                        if( *e!=0 && !isspace(*e) ) {
                            break;
                        }
                        file->line_len--;
                    }
                }
            }
        }
        ++lineNo;
    }
    return rc;
}

/*
 * compare seq to quality length ignoring traling spaces, no leading spaces expected
 */
static
bool match_seq_to_qual(const char* seq, size_t seq_len, const char* qual, size_t qual_len)
{
    char *space = NULL;

    /* cut trailing spaces */
    while( seq_len > 0 && seq[seq_len - 1] == ' ') {
        seq_len--;
    }
    while( qual_len > 0 && qual[qual_len - 1] == ' ') {
        qual_len--;
    }
    space = memchr(qual, ' ', qual_len);
    if( space != NULL ) {
        /* spaced numbers form: count numbers */
        int spaceCount = 1;
        do {
            if( (space = memchr(space, ' ', qual_len - (space - qual))) != NULL ) {
                spaceCount++;
                space++;
            }
        } while( space != NULL );
        qual_len = spaceCount;
    }
    return seq_len == qual_len;
}

/* parses name as a given word number (1-based) in a str of size len
 * looks for name(#barcode)?([\/.]\d)?
 * returns score of found parts
 * score == 0 word not found
 */ 
static
uint8_t parse_spot_name(const SRALoaderFile* file, FileReadData* spot, const char* str, size_t len, uint8_t word_number)
{
    uint8_t w, score = 0;
    const char* name, *name_end;

    name = name_end = str;
    /* set name_end to end of word_number-th word */
    for(w = 1; w <= word_number || name_end == NULL; w++ ) {
        /* skip consecutive spaces */
        while( *name_end == ' ' && name_end != &str[len] ) {
            name_end++;
        }
        name = name_end;
        name_end = memchr(name, ' ', len - (name_end - str));
        if( name_end == NULL ) {
            if( w == word_number ) {
                name_end = &str[len];
            }
            break;
        }
    }
    if( name != name_end && name_end != NULL ) {
        char* x;
        rc_t rc;

        /* init only name portion */
        FileReadData_init(spot, true);
        --name_end; /* goto last char */
        if( isdigit(name_end[0])&& (name_end[-1] == '\\' || name_end[-1] == '/' )) {
            score++;
            spot->read.read_id = name_end[0] - '0';
            name_end -= 2;
        } else if( isdigit(*name_end) && name_end[-1] == '.' ) {
            int q = 0;
            if( memrchr(name, '#', name_end - name) != NULL ) {
                /* have barode -> this is read id */
                q = 4;
            } else {
                /* may a read id, check to see if 4 coords follow */
                const char* end = name_end - 1;
                while( --end >= name ) {
                    if( strchr(":|_", *end) != NULL ) {
                        q++;
                    } else if( !isdigit(*end) ) {
                        break;
                    }
                }
            }
            if( q == 4 ) {
                score++;
                spot->read.read_id = name_end[0] - '0';
                name_end -= 2;
            }
        }
        if( (x = memrchr(name, '#', name_end - name)) != NULL ) {
            score++;
            if( (rc = pstring_assign(&spot->barcode, x + 1, name_end - x)) != 0 ) {
                SRALoaderFile_LOG(file, klogErr, rc, "barcode $(b)", "b=%.*s", name_end - x, x + 1);
                return 0;
            }
            if( pstring_strcmp(&spot->barcode, "0") == 0 ) {
                pstring_clear(&spot->barcode);
            } else if( spot->barcode.len >= 4 &&
                       (strncmp(spot->barcode.data, "0/1_", 4) == 0 || strncmp(spot->barcode.data, "0/2_", 4) == 0) ) {
                spot->read.read_id = spot->barcode.data[2] - '0';
                pstring_assign(&spot->barcode, &spot->barcode.data[4], spot->barcode.len - 4);
            }
            name_end = --x;
        }
        score++;
        if( (rc = pstring_assign(&spot->name, name, name_end - name + 1)) != 0 ) {
            SRALoaderFile_LOG(file, klogErr, rc, "spot name $(n)", "n=%.*s", name_end - name + 1, name);
            return 0;
        }
        /* search for _R\d\D in name and use it as read id, remove from name or spot won't assemble */
        x = spot->name.data;
        while( (x = strrchr(x, 'R')) != NULL ) {
            if( x != spot->name.data && *(x - 1) == '_' && isdigit(*(x + 1)) && !isalnum(*(x + 2)) ) {
                score++;
		if(spot->read.read_id == -1){
			spot->read.read_id = *(x + 1) - '0';
		}
                strcpy(x - 1, x + 2);
                spot->name.len -= 4;
                break;
            }
            x++;
        }
        /* find last '=' and use only whatever is to the left of it */
        if( (x = memrchr(spot->name.data, '=', spot->name.len)) != NULL ) {
            rc = pstring_assign(&spot->name, spot->name.data, (x - spot->name.data) );
        }
    }
    return score;
}

/*
 * in a single line form tries to grab last to chunks defined by sep into seq and qual
 * ignores spaces adjucent to sep
 * normally line would look like "name sep seq sep sep qual"
 */
static
bool find_seq_qual_by_sep(FastqLoaderFmt* self, FastqFileInfo* file, const char sep)
{
    const char* seq = NULL, *qual = NULL;
    size_t seq_len = 0, qual_len = 0;

    FileReadData_init(file->spot, false);
    qual = memrchr(file->line, sep, file->line_len);
    if( qual != NULL ) {
        seq = memrchr(file->line, sep, qual - file->line);
        if( seq != NULL ) {
            if( parse_spot_name(file->file, file->spot, file->line, seq - file->line, 1) != 0 ) {
                /* skip leading spaces */
                do {
                    seq = seq + 1;
                } while( *seq == ' ' && seq < (file->line + file->line_len) );
                seq_len = qual - seq;
                do {
                    qual = qual + 1;
                } while( *qual == ' ' && qual < (file->line + file->line_len)  );
                qual_len = file->line_len - (qual - file->line);
                if( *seq != sep && *seq != ' ' && seq_len != 0 &&
                    *qual != sep && *qual != ' ' && qual_len != 0 ) {
                    if( match_seq_to_qual(seq, seq_len, qual, qual_len) ) {
                        rc_t rc;
                        if( (rc = pstring_assign(&file->spot->read.seq, seq, seq_len)) == 0 ) {
                            if( pstring_is_fasta(&file->spot->read.seq) ) {
                                if( (rc = pstring_assign(&file->spot->read.qual, qual, qual_len)) == 0 ) {
                                    file->spot->read.qual_type = file->qualType;
                                    return true;
                                }
                            }
                            file->spot->read.seq.len = 0;
                        }
                        if( rc != 0 ) {
                            SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=storing read data");
                        }
                    }
                }
            }
        }
    }
    return false;
}

/*
 * read fasta or quality, which maybe wrapped on 70th column width,
 * into asciiZ buffer
 */
static
rc_t read_multiline_seq_or_qual(FastqFileInfo* file, const char stop, pstring* str)
{
    rc_t rc = 0;
    bool append = false, optional = false;

    while( rc == 0 ) {
        if( (rc = file_read_line(file, optional)) == 0 ) {
            if( optional && (file->line == NULL || (file->line_len > 0 && file->line[0] == stop)) ) {
                /* eof or next line is defline -> stop, line stays in buffer */
                break;
            }
            if( append && memchr(str->data, ' ', str->len) != NULL ) {
                rc = pstring_append(str, " ", 1);
            }
            if( rc == 0 && (rc = pstring_append(str, file->line, file->line_len)) == 0 ) {
                file->line = NULL; /* line processed */
                optional = true;
            }
            append = true;
        }
    }
    return rc;
}

static
rc_t read_spot_data_3lines(FastqFileInfo* file, FileReadData* sd, uint8_t best_word, uint8_t best_score, int qualType)
{
    rc_t rc = 0;

    file->line = NULL; /* discard defline */
    /* read sequence */
    if( (rc = read_multiline_seq_or_qual(file, '+', &sd->read.seq)) != 0 ) {
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading sequence data");
    }
    if( !pstring_is_fasta(&sd->read.seq) ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcCorrupt);
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=expected sequence data");
    }
    /* next defline */
    if( (rc = file_read_line(file, false)) != 0 ) {
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading quality defline");
    }
    if( file->line[0] != '+' ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcCorrupt);
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=expected '+' on quality defline");
    }
    if( file->line_len != 1 ) { /* there may be just '+' on quality defline */
        FileReadData d;
        uint8_t score = parse_spot_name(file->file, &d, &file->line[1], file->line_len - 1, best_word);
        /* sometimes quality defline may NOT contain barcode and readid, so score will be lower than bestscore,
           but must be at least == 1 with none empty line, which means that name was found */
        if( score < 1 ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcCorrupt);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=spot name not found");
        }
        if( pstring_cmp(&sd->name, &d.name) != 0 ||
            (score == best_score && (pstring_cmp(&sd->barcode, &d.barcode) != 0 || sd->read.read_id != d.read.read_id)) ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcCorrupt);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=quality defline do not match sequence defline");
        }
    }
    file->line = NULL; /* discard defline */
    if( (rc = read_multiline_seq_or_qual(file, '@', &sd->read.qual)) != 0 ) {
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=failed to read quality");
    }
    if( sd->read.qual.len <= 0 ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcEmpty);
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=quality");
    }
    sd->read.qual_type = qualType;
    sd->ready = true;
    return 0;
}

/* reads from a file data for a sinlge spot, data may be partial */
static
rc_t read_next_spot(FastqLoaderFmt* self, FastqFileInfo* file)
{
    rc_t rc = 0;

    if( file->spot->ready ) {
        /* data still not used */
        return 0;
    }
    FileReadData_init(file->spot, false);
    FileReadData_init(&file->spot[1], false);
    if( (rc = file_read_line(file, true)) != 0 ) {
        return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading more data");
    } else if( file->line == NULL ) {
        return 0; /* eof */
    }
    if( find_seq_qual_by_sep(self, file, ':') || find_seq_qual_by_sep(self, file, ' ') ) {
        /* single line forms */
        file->line = NULL; /* line consumed */
        file->spot->ready = true;
    } else  if( file->line[0] == '>' || file->line[0] == '@' ) {
        /* 4 or 8 line format */
        FileReadData sd;
        uint8_t word = 0, best_word = 0;
        uint8_t score = 0, best_score = 0;
        /* find and parse spot name on defline */
        do {
            score = parse_spot_name(file->file, &sd, &file->line[1], file->line_len - 1, ++word);
            if( score > best_score ) {
                if( (rc = pstring_copy(&file->spot->name, &sd.name)) != 0 ||
                    (rc = pstring_copy(&file->spot->barcode, &sd.barcode)) != 0 ) {
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=copying read name");
                }
                file->spot->read.read_id = sd.read.read_id;
                best_score = score;
                best_word = word; /* used below for quality defline parsing */
            }

        } while(score != 0);
        if( best_score == 0 ) {
            rc = RC(rcSRA, rcFormatter, rcReading, rcId, rcNotFound);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=spot name not detected");
        }
        if( file->line[0] == '@' ) {
            if( (rc = read_spot_data_3lines(file, file->spot, best_word, best_score, file->qualType)) != 0 ) {
                return rc;
            }
            /* now we MAY have 5th line in buffer so we can check if it's 2nd read for 8 lines format */
            if( file->line_len != 0 && file->line != NULL && file->line[0] == '@' ) {
                /* try to find read id on next line */
                FileReadData_init(&file->spot[1], false);
                if( parse_spot_name(file->file, &file->spot[1], &file->line[1], file->line_len - 1, best_word) == best_score ) {
                    if( pstring_cmp(&file->spot->name, &file->spot[1].name) == 0 &&
                        pstring_cmp(&file->spot->barcode, &file->spot[1].barcode) == 0 &&
                        file->spot->read.read_id != file->spot[1].read.read_id ) {
                        /* since it is different read id with same name and barcode, fill up second read */
                        if( (rc = read_spot_data_3lines(file, &file->spot[1], best_word, best_score, file->qualType)) != 0 ) {
                            return rc;
                        }
                    }
                }
            }
        } else {
            /* 2 line seq or quality form */
            file->line = NULL; /* line consumed */
            /* read sequence/quality */
            if( (rc = read_multiline_seq_or_qual(file, '>', &file->spot->read.seq)) != 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=reading seq/qual data");
            }
            if( file->spot->read.seq.len == 0 ) {
                return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=empty string reading seq/qual data");
            } else if( !pstring_is_fasta(&file->spot->read.seq) ) {
                /* swap */
                if( (rc = pstring_copy(&file->spot->read.qual, &file->spot->read.seq)) == 0 ) {
                    file->spot->read.qual_type = file->qualType;
                    pstring_clear(&file->spot->read.seq);
                }
            }
            file->spot->ready = true;
        }
    } else {
            rc = RC(rcSRA, rcFormatter, rcReading, rcFile, rcInvalid);
            return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=file corrupt or format unknown");
    }
    if( rc == 0 ) {
        int k;
        for(k = 0; k < 2; k++) {
            FileReadData* rd = &file->spot[k];
            if( rd->ready && rd->read.qual_type != ILLUMINAWRITER_COLMASK_NOTSET ) {
                if( file->qualOffset == 0 ) {
                    /* detect and remember */
                    file->qualOffset = 33;
		    file->qualMax = 94;
                    rc = pstring_quality_convert(&rd->read.qual, file->qualEnc, file->qualOffset, file->qualMin, file->qualMax);
                    if( GetRCState(rc) == rcOutofrange ) {
                        file->qualOffset = 64;
			file->qualMax = 61;
                        rc = pstring_quality_convert(&rd->read.qual, file->qualEnc, file->qualOffset, file->qualMin, file->qualMax);
                    }
                } else {
		    if(file->qualOffset == 33) file->qualMax = 94;
                    rc = pstring_quality_convert(&rd->read.qual, file->qualEnc, file->qualOffset, file->qualMin, file->qualMax);
                }
                if( rc != 0 ) {
                    return SRALoaderFile_LOG(file->file, klogErr, rc, "$(msg)", "msg=converting quality");
                }
            }
        }
    }
    return 0;
}

static
rc_t FastqLoaderFmt_WriteData(FastqLoaderFmt* self, uint32_t argc, const SRALoaderFile* const argv[], int64_t* spots_bad_count)
{
    rc_t rc = 0;
    uint32_t i, g = 0;
    FastqFileInfo* files = NULL;
    bool done;
    static IlluminaSpot spot;
 
    if( (files = calloc(argc, sizeof(*files))) == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcMemory, rcInsufficient);
    }

    for(i = 0; rc == 0 && i < argc; i++) {
        ExperimentQualityType qType;
        FastqFileInfo* file = &files[i];

        file->file = argv[i];
        FileReadData_init(file->spot, false);
        FileReadData_init(&file->spot[1], false);
        if( (rc = SRALoaderFile_QualityScoringSystem(file->file, &qType)) == 0 &&
            (rc = SRALoaderFile_QualityEncoding(file->file, &file->qualEnc)) == 0 &&
            (rc = SRALoaderFile_AsciiOffset(file->file, &file->qualOffset)) == 0 ) {

            file->qualType = ILLUMINAWRITER_COLMASK_NOTSET;

            if( qType == eExperimentQualityType_Undefined ) {
                qType = self->processing->quality_type;
                file->qualOffset = self->processing->quality_offset;
            }
            switch(qType) {
                case eExperimentQualityType_LogOdds:
                case eExperimentQualityType_Other:
                    if( self->w454 != NULL || self->wIonTorrent != NULL ) {
                        rc = RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcInvalid);
                        LOGERR(klogInt, rc, "quality type other than Phred is not supported for this PLATFORM");
                    }
                    file->qualMin = -40;
                    file->qualMax = 41;
                    file->qualType = ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS1;
                    break;
                default:
                    SRALoaderFile_LOG(file->file, klogWarn, rc, 
                        "quality_scoring_system attribute not set for this file, using Phred as default", NULL);
                case eExperimentQualityType_Phred:
                    file->qualType = ILLUMINAWRITER_COLMASK_QUALITY_PHRED;
                    file->qualMin = 0;
                    file->qualMax = (self->wIllumina) ? 61: 127;
                    break;
            }
        }
    }
    do {
        done = true;
        for(i = 0; rc == 0 && i < argc; i++) {
            FastqFileInfo* file = &files[i];
            if( (rc = read_next_spot(self, file)) != 0 || !file->spot->ready ) {
                continue;
            }
            done = false;
#if _DEBUGGING
            {{
                FileReadData* ss = file->spot;
                do {
                    DEBUG_MSG(3, ("file-%u: name:'%s', bc:%s, rd:%i, flt:%hu, seq '%.*s', qual %u bytes\n",
                                  i + 1, ss->name.data, ss->barcode.data, ss->read.read_id, ss->read.filter,
                                  ss->read.seq.len, ss->read.seq.data, ss->read.qual.len));
                    if( ss == &file->spot[1]){ break; }
                    ss = file->spot[1].ready ? &file->spot[1] : NULL;
                } while( ss != NULL );
            }}
#endif
        }
        if( rc != 0 || done ) {
            break;
        }
        /* collect spot reads, matching by spot name
         * spot data may be split across multiple files
         */
        IlluminaSpot_Init(&spot);
        for(i = 0; rc == 0 && i < argc; i++) {
            FileReadData* fspot = files[i].spot[0].ready ? &files[i].spot[0] : NULL;
            while(rc == 0 && fspot != NULL ) {
                rc = IlluminaSpot_Add(&spot, &fspot->name, &fspot->barcode, &fspot->read);
                if( rc == 0 ) {
                    g = i;
                    fspot->ready = false;
                } else if( GetRCState(rc) == rcIgnored ) {
                    rc = 0;
                } else {
                    SRALoaderFile_LOG(files[i].file, klogErr, rc, "$(msg)", "msg=adding data to spot");
                }
                if( fspot == &files[i].spot[1]) { break; }
                fspot = files[i].spot[1].ready ? &files[i].spot[1] : NULL;
            }
        }
        if( rc == 0 ) {
            if( self->wIllumina != NULL ) {
                if( (rc = SRAWriterIllumina_Write(self->wIllumina, argv[0], &spot)) != 0 &&
                    GetRCTarget(rc) == rcFormatter && GetRCContext(rc) == rcValidating ) {
                    SRALoaderFile_LOG(files[g].file, klogWarn, rc, "$(msg) $(spot_name)", "msg=bad spot,spot_name=%.*s",
                                                spot.name->len, spot.name->data);
                    self->spots_bad_count++;
                    if( self->spots_bad_allowed < 0 ||
                        self->spots_bad_count <= self->spots_bad_allowed ) {
                        rc = 0;
                    }
                }
            } else if( spot.nreads != 1 ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcUnsupported);
                SRALoaderFile_LOG(files[g].file, klogErr, rc, "$(msg)", "msg=multiple reads for this platform");
            } else if( self->wIonTorrent != NULL ) {
                rc = SRAWriterIonTorrent_WriteRead(self->wIonTorrent, argv[0], spot.name,
                                                   spot.reads[0].seq, spot.reads[0].qual, NULL, NULL, 0, 0, 0, 0);
            } else {
                rc = SRAWriter454_WriteRead(self->w454, argv[0], spot.name,
                                            spot.reads[0].seq, spot.reads[0].qual, NULL, NULL, 0, 0, 0, 0);
            }
        }
    } while( rc == 0 );
    free(files);
    *spots_bad_count = self->spots_bad_count;
    return rc;
}

static
rc_t FastqLoaderFmt_Whack(FastqLoaderFmt *self, SRATable** table)
{
    SRAWriterIllumina_Whack(self->wIllumina, table);
    SRAWriter454_Whack(self->w454, table);
    SRAWriterIonTorrent_Whack(self->wIonTorrent, table);
    free(self);
    return 0;
}

const char UsageDefaultName[] = "fastq-load";

static
rc_t FastqLoaderFmt_Version (const FastqLoaderFmt* self, uint32_t *vers, const char** name )
{
    *vers = KAppVersion();
    *name = "Fastq";
    return 0;
}

static SRALoaderFmt_vt_v1 vtFastqLoaderFmt =
{
    1, 0,
    FastqLoaderFmt_Whack,
    FastqLoaderFmt_Version,
    NULL,
    FastqLoaderFmt_WriteData
};

rc_t SRALoaderFmtMake(SRALoaderFmt **self, const SRALoaderConfig *config)
{
    rc_t rc = 0;
    FastqLoaderFmt* fmt;
    const PlatformXML* platform;
    const ProcessingXML* processing;

    if( self == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    }
    *self = NULL;

    if( (rc = Experiment_GetProcessing(config->experiment, &processing)) != 0 ||
        (rc = Experiment_GetPlatform(config->experiment, &platform)) != 0 ) {
        return rc;
    }
    fmt = calloc(1, sizeof(*fmt));
    if(fmt == NULL) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        LOGERR(klogInt, rc, "failed to initialize; out of memory");
        return rc;
    }
    if( platform->id == SRA_PLATFORM_454 ) {
        if( rc == 0 && (rc = SRAWriter454_Make(&fmt->w454, config)) != 0 ) {
            LOGERR(klogInt, rc, "failed to initialize 454 writer");
        }
    } else if( platform->id == SRA_PLATFORM_ION_TORRENT ) {
        if( rc == 0 && (rc = SRAWriterIonTorrent_Make(&fmt->wIonTorrent, config)) != 0 ) {
            LOGERR(klogInt, rc, "failed to initialize Ion Torrent writer");
        }
    } else if(   (rc = SRAWriterIllumina_Make(&fmt->wIllumina, config)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize Illumina writer");
    }
    if( rc == 0 && (rc = SRALoaderFmtInit(&fmt->dad, (const SRALoaderFmt_vt*)&vtFastqLoaderFmt)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize parent object");
    }
    if( rc != 0 ) {
        FastqLoaderFmt_Whack(fmt, NULL);
    } else {
        fmt->processing = processing;
        fmt->spots_bad_allowed = config->spots_bad_allowed;
        fmt->spots_bad_count = 0;
        *self = &fmt->dad;
    }
    return rc;
}
