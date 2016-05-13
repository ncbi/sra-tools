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
#include <klib/rc.h>
#include <klib/log.h>
#include <sra/sff-file.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

typedef struct SFFLoaderFmt SFFLoaderFmt;

#define SRALOADERFMT_IMPL SFFLoaderFmt
#include "loader-fmt.h"

#include "writer-454.h"
#include "writer-ion-torrent.h"
#include "debug.h"

#include <byteswap.h>

struct SFFLoaderFmt {
    SRALoaderFmt dad; /* base type -> must be first in struct */ 
    const SRAWriter454* w454;
    const SRAWriterIonTorrent* wIonTorrent;
    bool skip_signal;
    uint32_t curr_read_number;

    /* file buffer data */
    const char* file_name;
    const uint8_t* file_buf;
    size_t file_advance;
    uint64_t index_correction; /* used in case of concatinated files */

    /* spot data */
    SFFCommonHeader header;
    pstring flow_chars;
    pstring key_seq;

    SFFReadHeader read_header;
    pstring name;
    pstring signal;
    pstring position;
    pstring read;
    pstring quality;
};

static
rc_t SFFLoaderFmt_ReadBlock(SFFLoaderFmt* self, const SRALoaderFile* file, size_t size, const char* location, bool silent)
{
    size_t read = 0;
    rc_t rc = SRALoaderFileRead(file, self->file_advance, size, (const void**)&self->file_buf, &read);
    self->file_advance = 0;
    if( rc == 0 && (size > 0 && (self->file_buf == NULL || read < size)) ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInsufficient);
    }
    if( rc != 0 && !silent ) {
        SRALoaderFile_LOG(file, klogErr, rc, "$(l), needed $(needed) got $(got) bytes",
                PLOG_3(PLOG_S(l),PLOG_U32(needed),PLOG_U32(got)), location, size, read);
    }
    return rc;
}

static
rc_t SFFLoaderFmtReadCommonHeader(SFFLoaderFmt* self, const SRALoaderFile *file)
{
    rc_t rc = 0;
    bool skiped_idx_pad = false;
    uint16_t head_sz;
    SFFCommonHeader prev_head;
    pstring prev_flow_chars;
    pstring prev_key_seq;

    if( (rc = SRALoaderFile_Offset(file, &self->index_correction)) != 0 ) {
        SRALoaderFile_LOG(file, klogErr, rc, "Reading initial file position", NULL);
        return rc;
    }
SkipIndexPad:
    self->index_correction += self->file_advance;
    if( (rc = SFFLoaderFmt_ReadBlock(self, file, SFFCommonHeader_size, NULL, true)) != 0) {
        SRALoaderFile_LOG(file, klogErr, rc, "common header, needed $(needed) bytes",
                          PLOG_U32(needed), SFFCommonHeader_size);
        return rc;
    }
    if( self->header.magic_number != 0 ) {
        /* next file in stream, remember prev to sync to each */
        memcpy(&prev_head, &self->header, sizeof(SFFCommonHeader));
        pstring_copy(&prev_flow_chars, &self->flow_chars);
        pstring_copy(&prev_key_seq, &self->key_seq);
    } else {
        prev_head.magic_number = 0;
        prev_head.index_length = 0;
    }

    memcpy(&self->header, self->file_buf, SFFCommonHeader_size);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    self->header.magic_number = bswap_32(self->header.magic_number);
    self->header.version = bswap_32(self->header.version);
    self->header.index_offset = bswap_64(self->header.index_offset);
    self->header.index_length = bswap_32(self->header.index_length);
    self->header.number_of_reads = bswap_32(self->header.number_of_reads);
    self->header.header_length = bswap_16(self->header.header_length);
    self->header.key_length = bswap_16(self->header.key_length);
    self->header.num_flows_per_read = bswap_16(self->header.num_flows_per_read);
#endif

    if( self->header.magic_number != (('.'<<24)|('s'<<16)|('f'<<8)|('f'<<0)) ) {
        if( !skiped_idx_pad && prev_head.magic_number != 0 ) {
            /* possible concatination of 2 files with index at EOF and padded to 8 bytes with header values not padded,
               try skipping padding and reread */
            uint32_t pad = 8 - prev_head.index_length % 8;
            if( pad != 0 ) {
                self->file_advance += pad;
                DEBUG_MSG(5, ("%s: trying to skip over %u bytes index section padding\n", self->file_name, pad));
                skiped_idx_pad = true;
                goto SkipIndexPad;
            }
        }
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnrecognized);
        SRALoaderFile_LOG(file, klogErr, rc, "magic number: $(m)", PLOG_U32(m), self->header.magic_number);
        return rc;
    }
    if( self->header.version != 1 ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcBadVersion);
        SRALoaderFile_LOG(file, klogErr, rc, "format version $(v)", PLOG_U32(v), self->header.version);
        return rc;
    }
    if( self->header.flowgram_format_code != SFFFormatCodeUI16Hundreths ) {
        /* NOTE: add a case here if flowgram coding gets new version to support different */
        rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcUnsupported);
        SRALoaderFile_LOG(file, klogErr, rc, "common header flowgram format code", NULL);
        return rc;
    }
    if( self->header.index_length % 8 != 0 ) {
        DEBUG_MSG(5, ("%s: index_length field value is not 8 byte padded: %u\n", self->file_name, self->header.index_length));
    }
    head_sz = SFFCommonHeader_size + self->header.num_flows_per_read + self->header.key_length;
    head_sz += (head_sz % 8) ? (8 - (head_sz % 8)) : 0;
    if( head_sz != self->header.header_length ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcFormat, rcInvalid);
        SRALoaderFile_LOG(file, klogErr, rc, "header length $(h) <> $(s) ", PLOG_2(PLOG_U16(h),PLOG_U16(s)),
                          self->header.header_length, head_sz);
        return rc;
    }
    /* read flow chars and key */
    self->file_advance = SFFCommonHeader_size;
    if( (rc = SFFLoaderFmt_ReadBlock(self, file, head_sz - SFFCommonHeader_size, "common header", false)) != 0) {
        return rc;
    }
    self->file_advance = head_sz - SFFCommonHeader_size;

    if( (rc = pstring_assign(&self->flow_chars, self->file_buf, self->header.num_flows_per_read)) != 0 ||
        (rc = pstring_assign(&self->key_seq, self->file_buf + self->header.num_flows_per_read, self->header.key_length)) != 0 ) {
        SRALoaderFile_LOG(file, klogErr, rc, "reading flows/key sequence", NULL);
        return rc;
    }
    if( prev_head.magic_number != 0 ) {
        /* next file's common header must match previous file's common header, partially */
        if( prev_head.key_length != self->header.key_length ||
            prev_head.num_flows_per_read != self->header.num_flows_per_read ||
            pstring_cmp(&prev_flow_chars, &self->flow_chars) != 0 ||
            pstring_cmp(&prev_key_seq, &self->key_seq) != 0 ) {
                rc = RC(rcSRA, rcFormatter, rcParsing, rcData, rcInconsistent);
                SRALoaderFile_LOG(file, klogErr, rc, "previous file common header differ in flows/key sequence", NULL);
        }
    }
    if( rc == 0 ) {
        if( self->w454 ) {
            rc = SRAWriter454_WriteHead(self->w454, &self->flow_chars, &self->key_seq);
        } else {
            rc = SRAWriterIonTorrent_WriteHead(self->wIonTorrent, &self->flow_chars, &self->key_seq);
        }
    }
    return rc;
}

static
rc_t SFFLoaderFmtReadDataHeader(SFFLoaderFmt* self, const SRALoaderFile* file)
{
    rc_t rc = 0;
    uint16_t head_sz = 0;

    /* Make sure the entire fixed portion of Read Header section is in the file buffer window */
    if( (rc = SFFLoaderFmt_ReadBlock(self, file, SFFReadHeader_size, "read header", false)) != 0 ) {
        return rc;
    }
    memcpy(&self->read_header, self->file_buf, SFFReadHeader_size);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    self->read_header.header_length = bswap_16(self->read_header.header_length);
    self->read_header.name_length = bswap_16(self->read_header.name_length);
    self->read_header.number_of_bases = bswap_32(self->read_header.number_of_bases);
    self->read_header.clip_quality_left = bswap_16(self->read_header.clip_quality_left);
    self->read_header.clip_quality_right = bswap_16(self->read_header.clip_quality_right);
    self->read_header.clip_adapter_left = bswap_16(self->read_header.clip_adapter_left);
    self->read_header.clip_adapter_right = bswap_16(self->read_header.clip_adapter_right);
#endif

    head_sz = SFFReadHeader_size + self->read_header.name_length;
    head_sz += (head_sz % 8) ? (8 - (head_sz % 8)) : 0;
    if( head_sz != self->read_header.header_length ) {
        rc = RC(rcSRA, rcFormatter, rcParsing, rcFormat, rcInvalid);
        SRALoaderFile_LOG(file, klogErr, rc, "read header length $(h) != $(s)", PLOG_2(PLOG_U16(h),PLOG_U16(s)),
                          self->header.header_length, head_sz);
        return rc;
    }
    /* read name */
    self->file_advance = SFFReadHeader_size;
    if( (rc = SFFLoaderFmt_ReadBlock(self, file, head_sz - SFFReadHeader_size, "read header", false)) != 0) {
        return rc;
    }
    self->file_advance = head_sz - SFFReadHeader_size;

    if( (rc = pstring_assign(&self->name, self->file_buf, self->read_header.name_length)) != 0 ) {
        SRALoaderFile_LOG(file, klogErr, rc, "copying read name", NULL);
    }
    return rc;
}

static
rc_t SFFLoaderFmtReadData(SFFLoaderFmt* self, const SRALoaderFile* file)
{
    rc_t rc = 0;
    uint32_t i;

    /* calc signal chunk size */
    size_t signal_sz = self->header.num_flows_per_read * sizeof(uint16_t);
    /* plus position, read, quality */
    size_t sz = signal_sz + self->read_header.number_of_bases * 3;
    /* + padding */
    sz += (sz % 8) ? (8 - (sz % 8)) : 0;

    /* adjust the buffer window to full data block size */
    if( (rc = SFFLoaderFmt_ReadBlock(self, file, sz, "read data", false)) != 0 ) { 
        return rc;
    }
    self->file_advance = sz;

    if( !self->skip_signal ) {
        rc = pstring_assign(&self->signal, self->file_buf, signal_sz);
#if __BYTE_ORDER == __LITTLE_ENDIAN
        if( rc == 0 ) {
            uint16_t* sig = (uint16_t*)self->signal.data;
            for(i = 0; i < self->header.num_flows_per_read; i++) {
                sig[i] = bswap_16(sig[i]);
            }
        }
#endif
    }

    if( rc == 0 ) {
        const uint8_t* pos = self->file_buf + signal_sz;

        if( !self->skip_signal ) {
            INSDC_coord_one *p;
            /* reset buffer to proper size */
            pstring_clear(&self->position);
            rc = pstring_append_chr(&self->position, 0, self->read_header.number_of_bases * sizeof(*p));
            p = (INSDC_coord_one*)&self->position.data[0];
            p[0] = pos[0];
            for(i = 1; i < self->read_header.number_of_bases; i++) {
                p[i] = p[i - 1] + pos[i];
            }
        }
        if( rc == 0 ) {
            pos += self->read_header.number_of_bases;
            rc = pstring_assign(&self->read, pos, self->read_header.number_of_bases);
            /*for(i = 0; i< self->read.len; i++ ) {
                self->read.data[i] = tolower(self->read.data[i]);
            }*/
        }
        if( rc == 0 ) {
            pos += self->read_header.number_of_bases;
            rc = pstring_assign(&self->quality, pos, self->read_header.number_of_bases);
        }
    }
    if( rc != 0 ) {
        SRALoaderFile_LOG(file, klogErr, rc, "copying read data", NULL);
    }
    return rc;
}

/**
 * SFFLoaderFmtSkipIndex - skip through indexes section if current position in file is indexes offset from header
 */
static
rc_t SFFLoaderFmtSkipIndex(SFFLoaderFmt* self, const SRALoaderFile* file)
{
    rc_t rc = 0;
    uint64_t pos;
    /* advance whatever and read file position */
    if( (rc = SFFLoaderFmt_ReadBlock(self, file, 0, "index", false)) == 0 &&
        (rc = SRALoaderFile_Offset(file, &pos)) == 0 ) {
        if( pos == (self->header.index_offset + self->index_correction) ) {
            /* skip */
            size_t leftover;
            self->file_advance += self->header.index_length;
            DEBUG_MSG(5, ("%s: indexes skipped at 0x%016X, %u bytes\n",
                          self->file_name, pos, self->header.index_length));
            /* try to get more data while skipping index */
            if( (rc = SRALoaderFileRead(file, self->file_advance, 16, (const void**)&self->file_buf, &leftover)) == 0 ) {
                if( self->file_buf != NULL && leftover < 8 ) {
                    /* smth short is read! possible last chunk in file was index and it was padded to 8 bytes,
                       while in header index_length field value was not padded */
                    if( leftover == (8 - self->header.index_length % 8) ) {
                        /* read out padding */
                        if( (rc = SRALoaderFileRead(file, leftover, 0, (const void**)&self->file_buf, &leftover)) == 0 ) {
                            if( self->file_buf == NULL ) {
                                /* ok EOF! */
                                DEBUG_MSG(5, ("%s: ignored extra %u bytes index section padding\n",
                                              self->file_name, 8 - self->header.index_length % 8));
                            }
                        }
                    }
                }
            }
            self->file_advance = 0;
        }
    }
    return rc;
}

static
rc_t SFFLoaderFmtWriteDataFile(SFFLoaderFmt* self, const SRALoaderFile* file)
{
    rc_t rc = 0;

    while( rc == 0 ) {
        if( self->curr_read_number == 0 ) {
            if( (rc = SFFLoaderFmtReadCommonHeader(self, file)) == 0 ) {
                DEBUG_MSG (5, ("%s: Common header ok: %u reads\n", self->file_name, self->header.number_of_reads));
                DEBUG_MSG (8, ("%s: flow_chars: [%hu] %s\n", self->file_name, self->header.num_flows_per_read, self->flow_chars.data));
                DEBUG_MSG (8, ("%s: key_seq: [%hu] %s\n", self->file_name, self->header.key_length, self->key_seq.data));
            } else if( GetRCObject(rc) == (enum RCObject)rcData && GetRCState(rc) == rcIgnored ) {
                rc = 0;
                break;
            }
        }
        if( rc == 0 && self->header.number_of_reads != 0 && 
            (rc = SFFLoaderFmtSkipIndex(self, file)) == 0 &&
            (rc = SFFLoaderFmtReadDataHeader(self, file)) == 0 &&
            (rc = SFFLoaderFmtReadData(self, file)) == 0 ) {
            if( self->w454 ) {
                rc = SRAWriter454_WriteRead(self->w454, file, &self->name, &self->read, &self->quality,
                                         self->skip_signal ? NULL : &self->signal,
                                         self->skip_signal ? NULL : &self->position,
                                         self->read_header.clip_quality_left, self->read_header.clip_quality_right,
                                         self->read_header.clip_adapter_left, self->read_header.clip_adapter_right);
            } else {
                rc = SRAWriterIonTorrent_WriteRead(self->wIonTorrent, file, &self->name, &self->read, &self->quality,
                         self->skip_signal ? NULL : &self->signal,
                         self->skip_signal ? NULL : &self->position,
                         self->read_header.clip_quality_left, self->read_header.clip_quality_right,
                         self->read_header.clip_adapter_left, self->read_header.clip_adapter_right);
            }
            if( rc == 0 ) {
                ++self->curr_read_number;
            }
        }
        if( rc != 0 && (GetRCObject(rc) != rcTransfer && GetRCState(rc) != rcDone) ) {
            SRALoaderFile_LOG(file, klogErr, rc, "on or about read #$(i)", PLOG_U32(i), self->curr_read_number + 1);
        } else if( self->curr_read_number == self->header.number_of_reads ) {
            DEBUG_MSG(5, ("%s: done loading declared %u reads\n", self->file_name, self->curr_read_number));
            self->curr_read_number = 0;
            /* will skip indexes if they are at eof */
            if( (rc = SFFLoaderFmtSkipIndex(self, file)) == 0 ) {
                /* This should be the end of file and/or beginning of next */
                if( (rc = SFFLoaderFmt_ReadBlock(self, file, 0, "EOF", false)) == 0 ) {
                    if( self->file_buf == NULL ) {
                        DEBUG_MSG(5, ("%s: EOF detected\n", self->file_name));
                        self->index_correction = 0;
                        break;
                    }
                }
            }
        }
    }
    return rc;
}

const char UsageDefaultName[] = "sff-load";

static
rc_t SFFLoaderFmtVersion( const SFFLoaderFmt*self, uint32_t *vers, const char** name )
{
    *vers = KAppVersion();
    *name = "SFF";
    return 0;
}

static
rc_t SFFLoaderFmtDestroy(SFFLoaderFmt* self, SRATable** table)
{
    SRAWriter454_Whack(self->w454, table);
    SRAWriterIonTorrent_Whack(self->wIonTorrent, table);
    free(self);
    return 0;
}

static
rc_t SFFLoaderFmtWriteData(SFFLoaderFmt* self, uint32_t argc, const SRALoaderFile* const argv[], int64_t* spots_bad_count)
{
    rc_t rc = 0;
    uint32_t idx = 0;

    for(idx = 0; rc == 0 && idx < argc; idx ++) {
        if( (rc = SRALoaderFileName(argv[idx], &self->file_name)) == 0 ) {
            self->curr_read_number = 0;
            rc = SFFLoaderFmtWriteDataFile(self, argv[idx]);
        }
    }
    return rc;
}

static SRALoaderFmt_vt_v1 vtSFFLoaderFmt = 
{
    1,
    0,
    SFFLoaderFmtDestroy,
    SFFLoaderFmtVersion,
    NULL,
    SFFLoaderFmtWriteData
};

rc_t SFFLoaderFmtMake(SRALoaderFmt** self, const SRALoaderConfig* config)
{
    rc_t rc = 0;
    SFFLoaderFmt* obj = NULL;
    const PlatformXML* platform;

    if( self == NULL || config == NULL ) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcSelf, rcNull);
    }
    *self = NULL;
    if( (rc = Experiment_GetPlatform(config->experiment, &platform)) != 0 ) {
        return rc;
    }
    obj = calloc(1, sizeof(SFFLoaderFmt));
    if(obj == NULL) {
        return RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
    }
    if( (rc = SRALoaderFmtInit(&obj->dad, (const SRALoaderFmt_vt*)&vtSFFLoaderFmt)) != 0 ) {
        LOGERR(klogInt, rc, "failed to initialize parent object");
    } else {
        if( platform->id == SRA_PLATFORM_454 ) {
            if( (rc = SRAWriter454_Make(&obj->w454, config)) != 0 ) {
                LOGERR(klogInt, rc, "failed to initialize writer");
            }
        } else {
            if( (rc = SRAWriterIonTorrent_Make(&obj->wIonTorrent, config)) != 0 ) {
                LOGERR(klogInt, rc, "failed to initialize Ion Torrent writer");
            }
        }
    }
    if(rc == 0) {
        obj->skip_signal = (config->columnFilter & efltrSIGNAL) && !(config->columnFilter & efltrDEFAULT);
        *self = &obj->dad;
    } else {
        free(obj);
    }
    return rc;
}

rc_t SRALoaderFmtMake(SRALoaderFmt **self, const SRALoaderConfig *config)
{
    return SFFLoaderFmtMake(self, config);
}
