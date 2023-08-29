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
#include <kapp/extern.h>
#include <klib/debug.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/rc.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/md5.h>
#include <kfs/bzip.h>
#include <kfs/gzip.h>

#include <loader/queue-file.h>
#include <loader/progressbar.h>
#include <loader/loader-file.h>
#define KFILE_IMPL KLoaderFile
#include <kfs/impl.h>

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <os-native.h> /* for strdup on Windows */

#define DBG(msg) DBGMSG(DBG_LOADLIB,DBG_FLAG(DBG_LOADLIB_FILE), msg)

#if _DEBUGGING
#   ifndef KLoaderFile_BUFFERSIZE
#       define KLoaderFile_BUFFERSIZE (64 * 1024 * 1024)
#   endif
#else
#   undef KLoaderFile_BUFFERSIZE
#   define KLoaderFile_BUFFERSIZE (64 * 1024 * 1024)
#endif

struct KLoaderFile
{
    KFile dad;
    /* physical file */
    uint64_t kfile_pos;
    const KFile* kfile;
    const KLoadProgressbar* job;

    const KFile* file;
    const KDirectory* dir;
    char* filename;
    char* realname;
    bool has_md5;
    bool ahead;
    uint8_t md5_digest[16];

    /* current file */
    enum {
        compress_none = 0,
        compress_gzip,
        compress_bzip2
    } compress_type;
    uint64_t pos;
    bool eof;
    uint32_t eol; /* next line start in buffer (next symbol after previously detected eol) */
    uint64_t line_no;

    /* file buffer */
    uint8_t *buffer_pos;
    uint32_t avail;
    uint8_t *buffer;
    size_t buffer_size;
#if _DEBUGGING
    uint32_t small_reads; /* used to detect ineffective reads from file */
#endif
};

static
rc_t CC KLoaderFile_Destroy(KLoaderFile* self)
{
    if( self != NULL ) {
        DBG(("%s %s\n", __func__, self->realname));
        KFileRelease(self->kfile);
        self->kfile = NULL;
    }
    return 0;
}

static
struct KSysFile* CC KLoaderFile_GetSysFile(const KLoaderFile *self, uint64_t *offset)
{
    return KFileGetSysFile(self ? self->kfile : NULL, offset);
}

static
rc_t CC KLoaderFile_RandomAccess(const KLoaderFile *self)
{
    return KFileRandomAccess(self ? self->kfile : NULL);
}

static
uint32_t CC KLoaderFile_Type(const KLoaderFile *self)
{
    if( self && self->kfile ) {
        return KFileType(self->kfile);
    }
    return KDirectoryPathType(self ? self->dir : NULL, "%s", self->realname);
}

static
rc_t CC KLoaderFile_Size(const KLoaderFile *self, uint64_t *size)
{
    if( self && self->kfile ) {
        return KFileSize(self->kfile, size);
    }
    return KDirectoryFileSize(self ? self->dir : NULL, size, "%s", self->realname);
}

static
rc_t CC KLoaderFile_SetSize(KLoaderFile *self, uint64_t size)
{
    return RC(rcApp, rcFile, rcUpdating, rcInterface, rcUnsupported);
}

static
rc_t CC KLoaderFile_ReadKFile(const KLoaderFile* cself, uint64_t pos, void *buffer, size_t size, size_t *num_read)
{
    rc_t rc = KFileRead(cself ? cself->kfile : NULL, pos, buffer, size, num_read);
    DBG(("%s: %s: %lu\n", __func__, cself->realname, *num_read));
    if( rc == 0 && cself->job != NULL ) {
        if( pos > cself->kfile_pos ) {
            rc = KLoadProgressbar_Process(cself->job, pos - cself->kfile_pos, false);
            ((KLoaderFile*)cself)->kfile_pos = pos;
        }
        pos += *num_read;
        if( pos > cself->kfile_pos ) {
            rc = KLoadProgressbar_Process(cself->job, pos - cself->kfile_pos, false);
            ((KLoaderFile*)cself)->kfile_pos= pos;
        }
    }
    return rc;
}

static
rc_t CC KLoaderFile_WriteKFile(KLoaderFile *self, uint64_t pos, const void *buffer, size_t size, size_t *num_writ)
{
    return RC(rcApp, rcFile, rcWriting, rcInterface, rcUnsupported);
}

static KFile_vt_v1 KLoaderFile_vtbl = {
    1, 1,
    KLoaderFile_Destroy,
    KLoaderFile_GetSysFile,
    KLoaderFile_RandomAccess,
    KLoaderFile_Size,
    KLoaderFile_SetSize,
    KLoaderFile_ReadKFile,
    KLoaderFile_WriteKFile,
    KLoaderFile_Type
};

static
rc_t KLoaderFile_Open(KLoaderFile* self)
{
    rc_t rc = 0;

    DBG(("%s opening %s\n", __func__, self->realname));
    if( (rc = KDirectoryOpenFileRead(self->dir, &self->kfile, "%s", self->realname)) == 0 ) {
        if( self->has_md5 ) {
            const KFile *md5File = NULL;
            DBG(("%s opening as md5 wrapped %s\n", __func__, self->realname));
            if( (rc = KFileMakeMD5Read(&md5File, self->file, self->md5_digest)) == 0) {
                self->file = md5File;
            }
        }
        if( rc == 0 ) {
            const KFile *z = NULL;
            switch(self->compress_type) {
                case compress_none:
                    break;
                case compress_gzip:
                    DBG(("%s opening as gzip wrapped %s\n", __func__, self->realname));
                    if( (rc = KFileMakeGzipForRead(&z, self->file)) == 0 ) {
                        KFileRelease(self->file);
                        self->file = z;
                    }
                    break;
                case compress_bzip2:
                    DBG(("%s opening as bzip2 wrapped %s\n", __func__, self->realname));
                    if( (rc = KFileMakeBzip2ForRead(&z, self->file)) == 0 ) {
                        KFileRelease(self->file);
                        self->file = z;
                    }
                    break;
                default:
                    rc = RC(rcApp, rcFile, rcOpening, rcType, rcUnexpected);
                    break;
            }
#if ! WINDOWS
            if( rc == 0 && self->ahead ) {
                const KFile *z = NULL;
                if( (rc = KQueueFileMakeRead(&z, self->pos, self->file,
                                             self->buffer_size * 10,  self->buffer_size, 0)) == 0 ) {
                    KFileRelease(self->file);
                    self->file = z;
                }
            }
#endif
        }
    }
    if( rc != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "opening $(file)", PLOG_S(file), self->filename));
        KFileRelease(self->file);
    }
    return rc;
}

/* Fill
 *  fill buffer as far as possible, shift unread data in buffer to buffer start
 */
static
rc_t KLoaderFile_Fill(KLoaderFile *self)
{
    rc_t rc = 0;

    if (self->kfile == NULL) {
        rc = KLoaderFile_Open(self);
    }
    if( rc == 0 ) {
        /* determine space in buffer available */
        size_t to_read = self->buffer_size - self->avail;
        if( to_read > 0 ) {
#if _DEBUGGING
            if( to_read < self->buffer_size * 0.5 ) {
                self->small_reads++;
                if( self->small_reads > 10 ) {
                    PLOGMSG(klogWarn, (klogWarn, "$(filename) INEFFECTIVE READING: $(times) times, now $(bytes) bytes",
                        PLOG_3(PLOG_S(filename),PLOG_U32(times),PLOG_U32(bytes)), self->filename, self->small_reads, to_read));
                }
            }
#endif
            /* shift left unread data */
            memmove(self->buffer, self->buffer_pos, self->avail);
            /* skip read chunk in buffer */
            self->pos += self->buffer_pos - self->buffer;
            /* reset pointer */
            self->buffer_pos = self->buffer;
            do { /* fill buffer up to eof */
                size_t num_read = 0;
                if( (rc = KFileRead(self->file, self->pos + self->avail,
                                    &self->buffer[self->avail], to_read, &num_read)) == 0 ) {
                    self->eof = (num_read == 0);
                    self->avail += (uint32_t) num_read;
                    to_read -= num_read;
                    DBG(("KLoaderFile read %s from %lu %u bytes%s\n",
                         self->filename, self->pos + self->avail - num_read, num_read, self->eof ? " EOF" : ""));
                }
            } while( rc == 0 && to_read > 0 && !self->eof );
        }
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_IsEof(const KLoaderFile* cself, bool* eof)
{
    if( cself == NULL || eof == NULL ) {
        return RC(rcApp, rcFile, rcConstructing, rcParam, rcNull);
    }
    /* end of file is when file is at eof and nothing in buffer or last readline returned the buffer */
    *eof = cself->eof && ((cself->avail - cself->eol) == 0);
    return 0;
}

LIB_EXPORT rc_t CC KLoaderFile_Close(const KLoaderFile* cself)
{
    rc_t rc = 0;

    if( cself == NULL ) {
        rc = RC(rcApp, rcFile, rcConstructing, rcParam, rcNull);
    } else {
        /* TBD possible delays if file has md5 set */
        DBG(("%s closing %s\n", __func__, cself->realname));
        rc = KFileRelease(cself->file);
        KFileAddRef(&cself->dad);
        ((KLoaderFile*)cself)->file = &cself->dad;
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_Reset(const KLoaderFile* cself)
{
    rc_t rc = 0;

    if( cself == NULL ) {
        rc = RC(rcApp, rcFile, rcConstructing, rcParam, rcNull);
    } else {
        KLoaderFile* self = (KLoaderFile*)cself;
        if( cself->pos != 0 && (cself->ahead || cself->compress_type != compress_none) ) {
            /* threaded buffering || data in buffer is not first portion of the file */
            rc = KLoaderFile_Close(cself);
            self->avail = 0;
            self->buffer[0] = 0;
            self->eof = false;
        } else {
            self->avail += (uint32_t) ( self->buffer_pos - self->buffer );
        }
        self->pos = 0;
        self->eol = 0;
        self->line_no = 0;
        self->buffer_pos = self->buffer;
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_SetReadAhead(const KLoaderFile* cself, bool read_ahead)
{
    if( cself == NULL ) {
        return RC(rcApp, rcFile, rcConstructing, rcParam, rcNull);
    }
#if ! WINDOWS
    ((KLoaderFile*)cself)->ahead = read_ahead;
#endif
    return 0;
}

LIB_EXPORT rc_t CC KLoaderFile_LOG(const KLoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    rc = KLoaderFile_VLOG(cself, lvl, rc, msg, fmt, args);
    va_end(args);
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_VLOG(const KLoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, va_list args)
{
    if( cself == NULL || (msg == NULL && rc == 0) ) {
        rc = RC(rcApp, rcFile, rcAccessing, rcParam, rcInvalid);
        LOGERR(klogErr, rc, __func__);
    }  else if( msg == NULL && rc != 0 ) {
        if( cself->line_no == 0 ) {
            PLOGERR(lvl, (lvl, rc, "$(file):$(offset)", "file=%s,offset=%lu", cself->filename, cself->pos));
        } else {
            PLOGERR(lvl, (lvl, rc, "$(file):$(line)", "file=%s,line=%lu", cself->filename, cself->line_no));
        }
    } else {
        rc_t fmt_rc;
        char xfmt[4096];
        const char* f = fmt ? fmt : "";
        const char* c = fmt ? "," : "";

        if( cself->line_no == 0 ) {
            fmt_rc = string_printf(xfmt, sizeof xfmt, NULL, "file=%s,offset=%lu%s%s", cself->filename, cself->pos, c, f);
        } else {
            fmt_rc = string_printf(xfmt, sizeof xfmt, NULL, "file=%s,line=%lu%s%s", cself->filename, cself->line_no, c, f);
        }
        if ( fmt_rc == 0 ) {
            fmt = xfmt;
        }
        if( rc == 0 ) {
            VLOGMSG(lvl, (lvl, msg, fmt, args));
        } else {
            VLOGERR(lvl, (lvl, rc, msg, fmt, args));
        }
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_Offset(const KLoaderFile* cself, uint64_t* offset)
{
    if( cself == NULL || offset == NULL ) {
        return RC(rcApp, rcFile, rcConstructing, rcParam, rcNull);
    }
    *offset = cself->pos + (cself->buffer_pos - cself->buffer);
    return 0;
}

LIB_EXPORT rc_t CC KLoaderFile_Line(const KLoaderFile* cself, uint64_t* line)
{
    if( cself == NULL || line == NULL ) {
        return RC(rcApp, rcFile, rcConstructing, rcParam, rcNull);
    }
    *line = cself->line_no;
    return 0;
}

static
rc_t KLoaderFile_AllocateBuffer(KLoaderFile *self)
{
    self->buffer_size = KLoaderFile_BUFFERSIZE;
    if( (self->buffer = malloc(self->buffer_size)) == NULL ) {
        self->buffer_size = 0;
        return RC(rcApp, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    self->buffer[0] = 0;
    self->buffer_pos = self->buffer;
    return 0;
}

LIB_EXPORT rc_t CC KLoaderFile_Readline(const KLoaderFile* cself, const void** buffer, size_t* length)
{
    rc_t rc = 0;

    if(cself == NULL || buffer == NULL || length == NULL) {
        rc = RC( rcApp, rcFile, rcAccessing, rcParam, rcNull);
    } else {
        KLoaderFile *self = (KLoaderFile*)cself;
        uint8_t* nl;
        bool refilled = false;
        bool eof;

        rc = KLoaderFile_IsEof(cself, &eof);
        if( rc == 0 && eof ) {
            *buffer = NULL;
            return 0;
        }

        while( rc == 0 ) {
            bool CR_last = false;
            int i, cnt = self->avail - self->eol;
            uint8_t* buf = &self->buffer_pos[self->eol];
            *buffer = buf;
            /* find first eol from current position */
            for(nl = NULL, i = 0; i < cnt && nl == NULL; i++) {
                if(buf[i] == '\n' || buf[i] == '\r') {
                    nl = &buf[i];
                }
            }
            if( nl != NULL || refilled ) {
                break;
            }
            refilled = true;
            /* none found we need to push out processed portion and load full buffer */
            if( self->eol > 0 ) {
                /* mark that line ended on buffer end and last char in buffer is \r */
                CR_last = self->eol == self->avail && self->buffer_pos[self->eol - 1] == '\r';
                self->buffer_pos += self->eol;
                self->avail -= self->eol;
                self->eol = 0;
            }
            if( (rc = KLoaderFile_Fill(self)) == 0 ) {
                if( CR_last && self->buffer_pos[0] == '\n' ) {
                    /* in previous chunk last char was \r and in next chunk 1st char is \n
                    this is \r\n seq split by buffer, need to ignore \n */
                    self->eol++;
                }
            }
        }
        if( rc == 0 ) {
            if( nl == NULL ) {
                self->eol = self->avail;
                *length = self->avail;
                if( self->buffer_size == self->avail ) {
                    /* buffer could be copied and next call will provide tail of line */
                    rc = RC( rcApp, rcFile, rcReading, rcString, rcTooLong);
                }
            } else {
                *length = nl - (uint8_t*)*buffer;
                self->eol = (uint32_t) ( nl - self->buffer_pos + 1 );
                if( *nl == '\r' && nl < &self->buffer[self->buffer_size - 1] && *(nl + 1) == '\n' ) {
                    /* \r\n */
                    self->eol++;
                }
                self->line_no++;
            }
        }
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_Read(const KLoaderFile* cself, size_t advance, size_t size, const void** buffer, size_t* length)
{
    rc_t rc = 0;

    if (cself == NULL || buffer == NULL || length == NULL ) {
        return RC(rcApp, rcFile, rcPositioning, rcSelf, rcNull);
    } else {
        KLoaderFile* self = (KLoaderFile*)cself;
        if( advance > 0 ) {
            if(advance >= self->avail) {
                self->pos += advance;
                self->avail = 0;
                self->eol = 0;
            } else {
                self->buffer_pos += advance;
                self->avail -= (uint32_t) advance;
                self->eol = (uint32_t) ( self->eol > advance ? self->eol - advance : 0 );
            }
        }
        if( size > self->avail || self->avail == 0 ) {
            rc = KLoaderFile_Fill(self);
        }
        if( rc == 0 ) {
            *buffer = self->buffer_pos;
            *length = self->avail;
            if( self->avail == 0 && self->eof ) {
                *buffer = NULL;
            } else if( size > self->avail ) {
                if( !self->eof ) {
                    rc = RC( rcApp, rcFile, rcReading, rcBuffer, rcInsufficient);
                }
            } else if( size > 0 ) {
                *length = size;
            }
        }
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_Name(const KLoaderFile *self, const char **name)
{
    if( self == NULL || name == NULL ) {
        return RC(rcApp, rcFile, rcAccessing, rcParam, rcNull);
    }
    *name = self->filename;
    return 0;
}

LIB_EXPORT rc_t CC KLoaderFile_FullName(const KLoaderFile *self, const char **name)
{
    if( self == NULL || name == NULL ) {
        return RC(rcApp, rcFile, rcAccessing, rcParam, rcNull);
    } else {
        *name = self->realname;
    }
    return 0;
}

LIB_EXPORT rc_t CC KLoaderFile_ResolveName(const KLoaderFile *self, char *resolved, size_t rsize)
{
    if( self == NULL || resolved == NULL ) {
        return RC(rcApp, rcFile, rcAccessing, rcParam, rcNull);
    }
    return KDirectoryResolvePath(self->dir, true, resolved, rsize, "%s", self->realname);
}

LIB_EXPORT rc_t CC KLoaderFile_Release(const KLoaderFile* cself, bool exclude_from_progress)
{
    rc_t rc = 0;

    if( cself != NULL ) {
        KLoaderFile* self = (KLoaderFile*)cself;
        DBG(("%s: '%s'%s\n", __func__, cself->filename, exclude_from_progress ? " excluded" : ""));
        /* may return md5 check error here */
        if( (rc = KFileRelease(self->file)) == 0 && !exclude_from_progress ) {
            PLOGMSG(klogInt, (klogInfo, "done file $(file) $(bytes) bytes",
                    "severity=file,file=%s,bytes=%lu",
                    self->filename, self->pos + (self->buffer_pos - self->buffer)));
        }
        KLoadProgressbar_Release(self->job, exclude_from_progress);
        KDirectoryRelease(self->dir);
        free(self->filename);
        free(self->realname);
        free(self->buffer);
        free(self);
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoaderFile_Make(const KLoaderFile **file, const KDirectory* dir, const char* filename,
                                    const uint8_t* md5_digest, bool read_ahead)
{
    rc_t rc = 0;
    KLoaderFile* obj = NULL;

    if( file == NULL || dir == NULL || filename == NULL ) {
        rc = RC(rcApp, rcFile, rcConstructing, rcParam, rcNull);
    } else {
        *file = NULL;
        if( (obj = calloc(1, sizeof(*obj))) == NULL ||
            (obj->filename = string_dup(filename, string_size(filename))) == NULL ) {
            rc = RC(rcApp, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else {
            if( (rc = KDirectoryAddRef(dir)) == 0 ) {
                obj->dir = dir;
                if( md5_digest != NULL ) {
                    obj->has_md5 = true;
                    memmove(obj->md5_digest, md5_digest, sizeof(obj->md5_digest));
                }
                if( (rc = KLoaderFile_AllocateBuffer(obj)) == 0 ) {
                    char resolved[4096];
                    char* ext = NULL;

                    strcpy(resolved, filename);
                    ext = strrchr(resolved, '.');
                    DBG(("%s adding %s\n", __func__, resolved));
                    rc = KLoadProgressbar_File(&obj->job, resolved, obj->dir);
                    if( ext != NULL && strcmp(ext, ".gz") == 0 ) {
                        obj->compress_type = compress_gzip;
                    } else if( ext != NULL && strcmp(ext, ".bz2") == 0 ) {
                        obj->compress_type = compress_bzip2;
                    }
                    if( rc != 0 && obj->compress_type != compress_none ) {
                        *ext = '\0';
                        DBG(("%s retry adding as %s\n", __func__, resolved));
                        if( (rc = KLoadProgressbar_File(&obj->job, resolved, obj->dir)) == 0 ) {
                            obj->has_md5 = false;
                            obj->compress_type = compress_none;
                        }
                    }
                    if( rc == 0 &&
                        (rc = KFileInit(&obj->dad, (const KFile_vt*)&KLoaderFile_vtbl, "KLoaderFile", filename, true, false)) == 0 ) {
                        obj->file = &obj->dad;
#if WINDOWS
                        obj->ahead = false;
#else
                        obj->ahead = read_ahead;
#endif
                        obj->realname = string_dup(resolved, string_size(resolved));
                        if( obj->realname == NULL ) {
                            rc = RC(rcApp, rcFile, rcConstructing, rcMemory, rcExhausted);
                        }
                    }
                }
            }
        }
    }
    if( rc == 0 ) {
        *file = obj;
    } else {
        *file = NULL;
        KLoaderFile_Release(obj, false);
    }
    return rc;
}
