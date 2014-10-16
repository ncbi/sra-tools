/*===========================================================================
 *
 *                            Public DOMAIN NOTICE
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
#include <klib/rc.h>
#include <klib/checksum.h>
#include <klib/printf.h>
#include <kfs/file.h>
#include <kdb/table.h>
#include <kdb/index.h>

#include <sra/sradb-priv.h>

#include "log.h"
#include "zlib-simple.h"
#include "sra-list.h"
#include "formats.h"
#include "text-file.h"
#include "sra-fastq.h"
#include "sra-sff.h"

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

static
rc_t FileOptions_MakeSuffix(FileOptions* self, const char* suffix, KTime_t ts)
{
    rc_t rc = 0;

    if( suffix == NULL ) {
        rc = RC(rcExe, rcFormat, rcReading, rcParam, rcNull);
    } else if( strlen(suffix) > FILEOPTIONS_BUFFER - 2 ) {
        rc = RC(rcExe, rcFormat, rcReading, rcBuffer, rcInsufficient);
    } else {
        strcpy(self->suffix, (isalnum(suffix[0]) && suffix[0] != '_') ? "." : "");
        strcat(self->suffix, suffix);
    }
    return rc;
}

rc_t FileOptions_Make(FileOptions** self, uint32_t count)
{
    assert(self != NULL);
    CALLOC(*self, count, sizeof(**self));
    if( *self == NULL ) {
        return RC(rcExe, rcTable, rcAllocating, rcMemory, rcExhausted);
    }
    return 0;
}

void FileOptions_Release(FileOptions* self)
{
    if( self != NULL ) {
        FREE(self);
    }
}

rc_t FileOptions_Clone(FileOptions** self, const FileOptions* src, uint32_t count)
{
    rc_t rc = 0;

    if( self == NULL || src == NULL ) {
        rc = RC(rcExe, rcTable, rcCopying, rcParam, rcNull);
    } else if( (rc = FileOptions_Make(self, count)) == 0 ) {
        memcpy(*self, src, sizeof(**self) * count);
    }
    return rc;
}

rc_t FileOptions_SRAArchive(FileOptions* self, const SRATable* tbl, KTime_t ts, bool lite)
{
    rc_t rc = 0;

    if( tbl == NULL || self == NULL ) {
        rc = RC(rcExe, rcFormat, rcReading, rcParam, rcNull);
    } else {
        const KFile* sfa = NULL;
        const char* ext;
        if( (rc = SRATableMakeSingleFileArchive(tbl, &sfa, lite, &ext)) == 0 ) {
            if( (rc = KFileSize(sfa, &self->file_sz)) == 0 &&
                (rc = FileOptions_MakeSuffix(self, ext, ts)) == 0 ) {
                self->type = lite ? eSRAFuseFmtArcLite : eSRAFuseFmtArc;
                self->f.sra.lite = lite;
            }
            ReleaseComplain(KFileRelease, sfa);
        }
    }
    return rc;
}

rc_t FileOptions_SRAArchiveInstant(FileOptions* self, FileOptions* fmd5,
                                   const SRAMgr* mgr, const char* accession, const char* path,
                                   const bool lite, KTime_t ts, uint64_t size, char md5[32])
{
    rc_t rc = 0;

    if( self == NULL || mgr == NULL || accession == NULL || fmd5 == NULL ) {
        rc = RC(rcExe, rcFormat, rcReading, rcParam, rcNull);
    } else {
        const char* ext;

        /* Only support non-lite files, if lite is needed fix this*/
        /*if (lite)
        {
            PLOGERR(klogErr, (klogErr, 0, "lite flag is not supported, get rid of lite flag in xml, in FileOptions_SRAArchiveInstant for path or accession = $(a)", PLOG_S(a), path ? path : accession));
            abort();
        }
        assert(!lite);*/
        ext = lite ? ".lite.sra" : ".sra";

        if( (rc = FileOptions_MakeSuffix(self, ext, ts)) == 0 ) {
            self->file_sz = size;
            self->type = lite ? eSRAFuseFmtArcLite : eSRAFuseFmtArc;
            self->f.sra.lite = lite;
            memcpy(self->md5, md5, sizeof(self->md5));
            if( (rc = FileOptions_AttachMD5(self, accession, fmd5)) == 0 ) {
                rc = FileOptions_UpdateMD5(self, accession);
            }
        }
    }
    return rc;
}

rc_t FileOptions_SRAArchiveUpdate(FileOptions* self, const char* name,
                                  KTime_t ts, uint64_t size, char md5[32])
{
    rc_t rc = 0;

    if( self == NULL || name == NULL ) {
        rc = RC(rcExe, rcFormat, rcReading, rcParam, rcNull);
    } else {
        self->file_sz = size;
        if( md5 == NULL ) {
            memset(self->md5, 0, sizeof(self->md5));
        } else {
            memcpy(self->md5, md5, sizeof(self->md5));
        }
        rc = FileOptions_UpdateMD5(self, name);
    }
    return rc;
}

rc_t FileOptions_AttachMD5(FileOptions* self, const char* name, FileOptions* md5)
{
    rc_t rc = 0;
    if( name == NULL || self == NULL || md5 == NULL ) {
        rc = RC(rcExe, rcFormat, rcAttaching, rcParam, rcInvalid);
    } else {
        switch(self->type) {
            case eSRAFuseFmtFastq:
            case eSRAFuseFmtFastqGz:
                md5->type = eSRAFuseFmtFastqMD5;
                break;
            case eSRAFuseFmtSFF:
            case eSRAFuseFmtSFFGz:
                md5->type = eSRAFuseFmtSFFMD5;
                break;
            case eSRAFuseFmtArc:
                md5->type = eSRAFuseFmtArcMD5;
                break;
            case eSRAFuseFmtArcLite:
                md5->type = eSRAFuseFmtArcLiteMD5;
                break;
            default:
                rc = RC(rcExe, rcFormat, rcReading, rcType, rcUnexpected);
        }
        if( rc == 0 ) {
            char buf[sizeof(md5->suffix)];

            strcpy(buf, self->suffix);
            strcat(buf, ".md5");
            if( (rc = FileOptions_MakeSuffix(md5, buf, 0)) == 0 ) {
                self->md5_file = md5 - self;
            }
        }
    }
    return rc;
}

rc_t FileOptions_CalcMD5(FileOptions* self, const char* name, const SRAListNode* sra)
{
    rc_t rc = 0;

    if( self == NULL || sra == NULL || name == NULL ) {
        rc = RC(rcExe, rcFormat, rcProcessing, rcParam, rcInvalid);
    } else if( self->md5[0] == '\0' ) {
        const KFile* kfile;
        if( (rc = FileOptions_OpenFile(self, sra, &kfile)) == 0 ) {
            MD5State md5;
            uint64_t pos = 0;
            uint8_t buffer[256 * 1024];
            size_t num_read = 0, x;

            MD5StateInit(&md5);
            do {
                if( (rc = KFileRead(kfile, pos, buffer, sizeof(buffer), &num_read)) == 0 ) {
                    MD5StateAppend(&md5, buffer, num_read);
                    pos += num_read;
                }
            } while(rc == 0 && num_read != 0);
            if( rc == 0 ) {
                uint8_t digest[16];
                char smd5[sizeof(self->md5) + 1];
                MD5StateFinish(&md5, digest);
                for(pos = 0, x = 0; rc == 0 && pos < sizeof(digest); pos++) {
                    rc = string_printf(&smd5[x], sizeof(smd5) - x, &num_read, "%02x", digest[pos]);
                    x += num_read;
                }
                memcpy(self->md5, smd5, sizeof(self->md5));
                DEBUG_LINE(10, "%s %s %.*s", self->suffix, name, sizeof(self->md5), self->md5);
            }
            KFileRelease(kfile);
        }
    }
    return rc;
}

rc_t FileOptions_UpdateMD5(FileOptions* self, const char* name)
{
    rc_t rc = 0;
    
    if( self == NULL || name == NULL ) {
        rc = RC(rcExe, rcFormat, rcUpdating, rcParam, rcInvalid);
    } else if( self->md5_file != 0 ) {
        FileOptions* md5 = &self[self->md5_file];
        if( self->md5[0] != '\0' ) {
            size_t nw;
            rc = string_printf(md5->f.txt64b, sizeof(md5->f.txt64b), &nw,
                "%.*s *%s%s\n", sizeof(self->md5), self->md5, name, self->suffix);
            md5->file_sz = rc ? 0 : nw;
        } else {
            md5->f.txt64b[0] = '\0';
            md5->file_sz = 0;
        }
    }
    return rc;
}

rc_t FileOptions_OpenFile(const FileOptions* cself, const struct SRAListNode* sra, const KFile** kfile)
{
    rc_t rc = 0;

    assert(cself != NULL);
    assert(kfile != NULL);
    *kfile = NULL;
    switch(cself->type) {
        case eSRAFuseFmtArc:
        case eSRAFuseFmtArcLite:
            {{
            const SRATable* tbl = NULL;
            if( (rc = SRAListNode_TableOpen(sra, &tbl)) == 0 ) {
                rc = SRATableMakeSingleFileArchive(tbl, kfile, cself->f.sra.lite, NULL);
                ReleaseComplain(SRATableRelease, tbl);
            }
            }}
            break;

        case eSRAFuseFmtFastqMD5:
        case eSRAFuseFmtSFFMD5:
        case eSRAFuseFmtArcMD5:
        case eSRAFuseFmtArcLiteMD5:
            rc = TextFile_Open(kfile, cself);
            break;

        case eSRAFuseFmtFastq:
        case eSRAFuseFmtFastqGz:
            rc = SRAFastqFile_Open(kfile, sra, cself);
            break;

        case eSRAFuseFmtSFF:
        case eSRAFuseFmtSFFGz:
            rc = SRASFFFile_Open(kfile, sra, cself);
            break;
    }
    return rc;
}

rc_t FileOptions_ParseMeta(FileOptions* self, const KMDataNode* file, const SRATable* tbl, KTime_t ts, const char* suffix)
{
    rc_t rc = 0;

    if( file == NULL || self == NULL || suffix == NULL ) {
        rc = RC(rcExe, rcFormat, rcReading, rcParam, rcNull);
    } else if( strlen(suffix) > FILEOPTIONS_BUFFER - 2 ) {
        rc = RC(rcExe, rcFormat, rcReading, rcBuffer, rcInsufficient);
    } else if( (rc = FileOptions_MakeSuffix(self, suffix, ts)) == 0 ) {
        const KMDataNode* format = NULL;
        if( (rc = KMDataNodeOpenNodeRead(file, &format, "Format")) == 0 ) {
            char fmt_name[32];
            size_t read = 0;
            if( (rc = KMDataNodeReadCString(format, fmt_name, sizeof(fmt_name), &read)) == 0 ) {
                const KMDataNode* tmp = NULL;
                fmt_name[read] = '\0';
                if( rc == 0 && (rc = KMDataNodeOpenNodeRead(file, &tmp, "Size")) == 0 ) {
                    rc = KMDataNodeReadB64(tmp, &self->file_sz);
                    ReleaseComplain(KMDataNodeRelease, tmp);
                }
                if( rc == 0 && (rc = KMDataNodeOpenNodeRead(file, &tmp, "Buffer")) == 0 ) {
                    rc = KMDataNodeReadB32(tmp, &self->buffer_sz);
                    ReleaseComplain(KMDataNodeRelease, tmp);
                }
                if( rc == 0 && (rc = KMDataNodeOpenNodeRead(file, &tmp, "Index")) == 0 ) {
                    if( (rc = KMDataNodeReadCString(tmp, self->index, sizeof(self->index) - 1, &read)) == 0 && tbl != NULL ) {
                        const KTable* ktbl = NULL;
                        if( (rc = SRATableGetKTableRead(tbl, &ktbl)) == 0 ) {
                            const KIndex* kidx = NULL;
                            if( (rc = KTableOpenIndexRead(ktbl, &kidx, self->index)) == 0 ) {
                                ReleaseComplain(KIndexRelease, kidx);
                            }
                            ReleaseComplain(KTableRelease, ktbl);
                        }
                    }
                    ReleaseComplain(KMDataNodeRelease, tmp);
                }
                if( rc == 0 && KMDataNodeOpenNodeRead(file, &tmp, "md5") == 0 ) {
                    rc = KMDataNodeReadCString(tmp, self->md5, sizeof(self->md5), &read);
                    ReleaseComplain(KMDataNodeRelease, tmp);
                }
                if( rc == 0 ) {
                    if(strcmp(fmt_name, "fastq") == 0 || strcmp(fmt_name, "fastq-gzip") == 0 ) {
                        if( strcmp(fmt_name, "fastq-gzip") == 0 ) {
                            self->type = eSRAFuseFmtFastqGz;
                            self->f.fastq.gzip = true;
                            if( (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/ZlibVersion")) == 0 ) {
                                uint16_t v = 0;
                                rc = KMDataNodeReadB16(tmp, &v);
                                ReleaseComplain(KMDataNodeRelease, tmp);
                                if( rc == 0 && v > ZLIB_VERNUM ) {
                                    PLOGMSG(klogWarn, (klogWarn, "ZLib version old: $(v) < $(mj).$(mn).$(b)",
                                        PLOG_4(PLOG_S(v),PLOG_U8(mj),PLOG_U8(mn),PLOG_U8(b)),
                                        ZLIB_VERSION, v & 0xF000, v & 0x0F00, v & 0x00FF));
                                }
                            }
                        } else {
                            self->type = eSRAFuseFmtFastq;
                        }

                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/accession")) == 0 ) {
                            rc = KMDataNodeReadCString(tmp, self->f.fastq.accession, sizeof(self->f.fastq.accession) - 1, &read);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/minSpotId")) == 0 ) {
                            rc = KMDataNodeReadB64(tmp, &self->f.fastq.minSpotId);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/maxSpotId")) == 0 ) {
                            rc = KMDataNodeReadB64(tmp, &self->f.fastq.maxSpotId);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/colorSpace")) == 0 ) {
                            rc = KMDataNodeReadB8(tmp, &self->f.fastq.colorSpace);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/colorSpaceKey")) == 0 ) {
                            rc = KMDataNodeRead(tmp, 0, &self->f.fastq.colorSpaceKey, 1, &read, NULL);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/origFormat")) == 0 ) {
                            rc = KMDataNodeReadB8(tmp, &self->f.fastq.origFormat);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/printLabel")) == 0 ) {
                            rc = KMDataNodeReadB8(tmp, &self->f.fastq.printLabel);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/printReadId")) == 0 ) {
                            rc = KMDataNodeReadB8(tmp, &self->f.fastq.printReadId);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/clipQuality")) == 0 ) {
                            rc = KMDataNodeReadB8(tmp, &self->f.fastq.clipQuality);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/minReadLen")) == 0 ) {
                            rc = KMDataNodeReadB32(tmp, &self->f.fastq.minReadLen);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/qualityOffset")) == 0 ) {
                            rc = KMDataNodeReadB16(tmp, &self->f.fastq.qualityOffset);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                    } else if(strcmp(fmt_name, "SFF") == 0 || strcmp(fmt_name, "SFF-gzip") == 0 ) {
                        if( strcmp(fmt_name, "SFF-gzip") == 0 ) {
                            self->type = eSRAFuseFmtSFFGz;
                            self->f.sff.gzip = true;
                            if( (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/ZlibVersion")) == 0 ) {
                                uint16_t v = 0;
                                rc = KMDataNodeReadB16(tmp, &v);
                                ReleaseComplain(KMDataNodeRelease, tmp);
                                if( rc == 0 && v > ZLIB_VERNUM ) {
                                    PLOGMSG(klogWarn, (klogWarn, "ZLib version old: $(v) < $(mj).$(mn).$(b)",
                                        PLOG_4(PLOG_S(v),PLOG_U8(mj),PLOG_U8(mn),PLOG_U8(b)),
                                        ZLIB_VERSION, v & 0xF000, v & 0x0F00, v & 0x00FF));
                                }
                            }
                        } else {
                            self->type = eSRAFuseFmtSFF;
                        }

                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/accession")) == 0 ) {
                            rc = KMDataNodeReadCString(tmp, self->f.sff.accession, sizeof(self->f.sff.accession) - 1, &read);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/minSpotId")) == 0 ) {
                            rc = KMDataNodeReadB64(tmp, &self->f.sff.minSpotId);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                        if( rc == 0 && (rc = KMDataNodeOpenNodeRead(format, &tmp, "Options/maxSpotId")) == 0 ) {
                            rc = KMDataNodeReadB64(tmp, &self->f.sff.maxSpotId);
                            ReleaseComplain(KMDataNodeRelease, tmp);
                        }
                    } else {
                        rc = RC(rcExe, rcFormat, rcReading, rcFormat, rcUnrecognized);
                    }
                }
            }
            ReleaseComplain(KMDataNodeRelease, format);
        }
    }
    return rc;
}
