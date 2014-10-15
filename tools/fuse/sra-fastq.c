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
#include <kfs/file.h>
#include <kproc/lock.h>
#include <kdb/table.h>
#include <kdb/index.h>

#include <sra/sradb-priv.h>
#include <sra/fastq.h>

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "xml.h"
#include "sra-list.h"
#include "sra-fastq.h"
#include "zlib-simple.h"

typedef struct SRAFastqFile SRAFastqFile;
#define KFILE_IMPL SRAFastqFile
#include <kfs/impl.h>

struct SRAFastqFile {
    KFile dad;
    uint32_t buffer_sz;
    uint64_t file_sz;
    char* gzipped; /* serves as flag and a buffer */
    KLock* lock;
    const SRATable* stbl;
    const KTable* ktbl;
    const KIndex* kidx;
    const FastqReader* reader;
    /* current buf content */
    uint64_t from;
    uint64_t size;
    char* buf;
};

static
rc_t SRAFastqFile_Destroy(SRAFastqFile *self)
{
    if( KLockAcquire(self->lock) == 0 ) {
        ReleaseComplain(FastqReaderWhack, self->reader);
        ReleaseComplain(KIndexRelease, self->kidx);
        ReleaseComplain(KTableRelease, self->ktbl);
        ReleaseComplain(SRATableRelease, self->stbl);
        FREE(self->buf < self->gzipped ? self->buf : self->gzipped);
        ReleaseComplain(KLockUnlock, self->lock);
        ReleaseComplain(KLockRelease, self->lock);
        FREE(self);
    }
    return 0;
}

static
struct KSysFile* SRAFastqFile_GetSysFile(const SRAFastqFile *self, uint64_t *offset)
{
    *offset = 0;
    return NULL;
}

static
rc_t SRAFastqFile_RandomAccess(const SRAFastqFile *self)
{
    return 0;
}

static
uint32_t SRAFastqFile_Type(const SRAFastqFile *self)
{
    return kfdFile;
}

static
rc_t SRAFastqFile_Size(const SRAFastqFile *self, uint64_t *size)
{
    *size = self->file_sz;
    return 0;
}

static
rc_t SRAFastqFile_SetSize(SRAFastqFile *self, uint64_t size)
{
    return RC(rcExe, rcFile, rcUpdating, rcInterface, rcUnsupported);
}

static
rc_t SRAFastqFile_Read(const SRAFastqFile* self, uint64_t pos, void *buffer, size_t size, size_t *num_read)
{
    rc_t rc = 0;

    if( pos >= self->file_sz ) {
        *num_read = 0;
    } else if( (rc = KLockAcquire(self->lock)) == 0 ) {
        do {
            if( pos < self->from || pos >= (self->from + self->size) ) {
                int64_t id = 0;
                uint64_t id_qty = 0;
                DEBUG_MSG(10, ("Caching for pos %lu %lu bytes\n", pos, size - *num_read));
                if( (rc = KIndexFindU64(self->kidx, pos, &((SRAFastqFile*)self)->from, &((SRAFastqFile*)self)->size, &id, &id_qty)) == 0 ) {
                    DEBUG_MSG(10, ("Caching from %lu:%lu, %lu bytes\n", self->from, self->from + self->size - 1, self->size));
                    DEBUG_MSG(10, ("Caching spot %ld, %lu spots\n", id, id_qty));
                    if( (rc = FastqReaderSeekSpot(self->reader, id)) == 0 ) {
                        size_t inbuf = 0, w = 0;
                        char* b = self->buf;
                        uint64_t left = self->buffer_sz;
                        do {
                            if( (rc = FastqReader_GetCurrentSpotSplitData(self->reader, b, left, &w)) != 0 ) {
                                break;
                            }
                            b += w; left -= w; inbuf += w; --id_qty;
                        } while( id_qty > 0 && (rc = FastqReaderNextSpot(self->reader)) == 0);
                        if( GetRCObject(rc) == rcRow && GetRCState(rc) == rcExhausted ) {
                            DEBUG_MSG(10, ("No more rows\n"));
                            rc = 0;
                        }
                        DEBUG_MSG(8, ("Cached %u bytes\n", inbuf));
                        if( self->gzipped != NULL ) {
                            size_t compressed = 0;
                            if( (rc = ZLib_DeflateBlock(self->buf, inbuf, self->gzipped, self->buffer_sz, &compressed)) == 0 ) {
                                char* b = self->buf;
                                ((SRAFastqFile*)self)->buf = self->gzipped;
                                ((SRAFastqFile*)self)->gzipped = b;
                                ((SRAFastqFile*)self)->size = compressed;
                                DEBUG_MSG(10, ("gzipped %lu bytes\n", self->size));
                            }
                        }
                    }
                }
            }
            if( rc == 0 ) {
                off_t from = pos - self->from;
                size_t q = (self->size - from) > (size - *num_read) ? (size - *num_read) : (self->size - from);
                DEBUG_MSG(10, ("Copying from %lu %u bytes\n", from, q));
                memcpy(&((char*)buffer)[*num_read], &self->buf[from], q);
                *num_read = *num_read + q;
                pos += q;
            }
        } while( rc == 0 && *num_read < size && pos < self->file_sz );
        ReleaseComplain(KLockUnlock, self->lock);
    }
    return rc;
}

static
rc_t SRAFastqFile_Write(SRAFastqFile *self, uint64_t pos, const void *buffer, size_t size, size_t *num_writ)
{
    return RC(rcExe, rcFile, rcWriting, rcInterface, rcUnsupported);
}

static KFile_vt_v1 SRAFastqFile_vtbl = {
    1, 1,
    SRAFastqFile_Destroy,
    SRAFastqFile_GetSysFile,
    SRAFastqFile_RandomAccess,
    SRAFastqFile_Size,
    SRAFastqFile_SetSize,
    SRAFastqFile_Read,
    SRAFastqFile_Write,
    SRAFastqFile_Type
};

rc_t SRAFastqFile_Open(const KFile** cself, const SRAListNode* sra, const FileOptions* opt)
{
    rc_t rc = 0;
    SRAFastqFile* self;
    CALLOC( self, 1, sizeof( *self ) );
    if( self == NULL )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcMemory, rcExhausted );
    }
    else
    {
        if ( ( rc = KFileInit( &self->dad, (const KFile_vt*)&SRAFastqFile_vtbl, "SRAFastqFile", "no-name", true, false ) ) == 0 )
        {
            if ( ( rc = SRAListNode_TableOpen( sra, &self->stbl ) ) == 0 )
            {
                if ( ( rc = SRATableGetKTableRead( self->stbl, &self->ktbl ) ) == 0 )
                {
                    if ( ( rc = KTableOpenIndexRead( self->ktbl, &self->kidx, opt->index ) ) == 0 )
                    {
                        if ( ( rc = KLockMake( &self->lock ) ) == 0 )
                        {
                            MALLOC( self->buf, opt->buffer_sz * ( opt->f.fastq.gzip ? 2 : 1 ) );
                            if ( self->buf == NULL )
                            {
                                rc = RC( rcExe, rcFile, rcOpening, rcMemory, rcExhausted );
                            }
                            else
                            {
                                self->file_sz = opt->file_sz;
                                self->buffer_sz = opt->buffer_sz;
                                if ( opt->f.fastq.gzip )
                                {
                                    self->gzipped = &self->buf[ opt->buffer_sz ];
                                }
                                self->from = ~0; /* reset position beyond file end */
                                rc = FastqReaderMake( &self->reader, self->stbl,
                                                      opt->f.fastq.accession, opt->f.fastq.colorSpace,
                                                      opt->f.fastq.origFormat, false, opt->f.fastq.printLabel,
                                                      opt->f.fastq.printReadId, !opt->f.fastq.clipQuality, false,
                                                      opt->f.fastq.minReadLen, opt->f.fastq.qualityOffset,
                                                      opt->f.fastq.colorSpaceKey,
                                                      opt->f.fastq.minSpotId, opt->f.fastq.maxSpotId );
                            }
                        }
                    }
                }
            }
            if ( rc == 0 )
            {
                *cself = &self->dad;
            }
            else
            {
                KFileRelease( &self->dad );
            }
        }
    }
    return rc;
}
