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
#include <klib/container.h>
#include <klib/refcount.h>
#include <kproc/lock.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include "log.h"
#include "tar-list.h"

#include <string.h>
#include <stdlib.h>

typedef enum ETar_Format {
    eTar_Unknown = 0,
    eTar_Legacy  = 1,
    eTar_OldGNU  = 2,
    eTar_Ustar   = 4,
    eTar_Posix   = 5
} ETar_Format;

/* POSIX "ustar" tar archive member header */
typedef struct SHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    union {
        char prefix[155];    /* not valid with old GNU format */
        struct {             /* old GNU format only */
            char atime[12];
            char ctime[12];
        } gt;
    } x;
} SHeader;

#define TAR_BLOCK_SIZE (512)
#define TAR_TAIL_PAD (TAR_BLOCK_SIZE * 2)
#define TAR_BLOCK_PAD (TAR_BLOCK_SIZE * 20)

union TarBlock {
    char buffer[TAR_BLOCK_SIZE];
    SHeader header;
};

typedef struct TarEntry {
    BSTNode node;
    const char* path;
    const char* name;
    uint64_t size;
    KTime_t mtime;
    bool executable;
    int16_t name_fmt; /* 0 - fits, +n - split, -n n-blocks */
    /* file chunks offsets within archive */
    /* file_header block(s) | file itself | padding to 512 block size */
    uint64_t start, data, pad, end;
} TarEntry;


static
void TarEntry_Init(const TarEntry* entry, uint64_t offset, const char* path, const char* name, uint64_t size, KTime_t mtime, bool executable)
{
    SHeader* h = NULL;
    size_t nlen = strlen(name), alloc = 0;
    TarEntry* e = (TarEntry*)entry;

    e->name = name;
    e->path = path;
    e->size = size;
    e->mtime = mtime;
    e->executable = executable;

    if(nlen <= sizeof(h->name)) {
        /* Name fits! */
        alloc = 1;
        e->name_fmt = 0;
    } else if( nlen <= sizeof(h->x.prefix) + 1 + sizeof(h->name) ) {
        /* Try to split the long name into a prefix and a short name (POSIX) */
        size_t split = nlen;
        if(split > sizeof(h->x.prefix)) {
            split = sizeof(h->x.prefix);
        }
        while(split > 0 && e->name[--split] != '/');
        if(split && nlen - split <= sizeof(h->name) + 1) {
            alloc = 1;
            e->name_fmt = split;
        }
    }
    if( alloc == 0 ) {
        nlen++;
        /* 2 pieces: 1st long file name header block + name block(s) and second actual file header block */
        alloc = nlen / TAR_BLOCK_SIZE;
        alloc += (nlen % TAR_BLOCK_SIZE) ? 1 : 0;
        alloc += 2;
        e->name_fmt = -alloc;
    }
    e->start = offset;
    e->data = e->start + alloc * TAR_BLOCK_SIZE;
    e->pad = e->data + e->size;
    e->end = e->size % TAR_BLOCK_SIZE;
    if( e->end != 0 ) {
        e->end = e->pad + (TAR_BLOCK_SIZE - e->end) - 1;
    } else {
        e->end = e->pad - 1;
    }
}

/* Convert a number to an octal string padded to the left
   with [leading] zeros ('0') and having _no_ trailing '\0'. */
static
bool TarEntry_NumToOctal(unsigned long val, char* ptr, size_t len)
{
    do {
        ptr[--len] = '0' + (char)(val & 7);
        val >>= 3;
    } while(len);
    return val ? false : true;
}

static
bool TarEntry_NumToBase256(uint64_t val, char* ptr, size_t len)
{
    do {
        ptr[--len] = (unsigned char)(val & 0xFF);
        val >>= 8;
    } while(len);
    *ptr |= '\x80';  /* set base-256 encoding flag */
    return val ? false : true;
}


/* Return 0 (false) if conversion failed; 1 if the value converted to
   conventional octal representation (perhaps, with terminating '\0'
   sacrificed), or -1 if the value converted using base-256. */
static
int TarEntry_EncodeUint64(uint64_t val, char* ptr, size_t len)
{
    if((unsigned long) val == val) {
        /* Max file size: */
        if(TarEntry_NumToOctal((unsigned long) val, ptr, len)) {
            /* 8GB - 1 */
            return 1;
        }
        if(TarEntry_NumToOctal((unsigned long) val, ptr, ++len)) {
            /* 64GB - 1  */
            return 1;
        }
    }
    if(TarEntry_NumToBase256(val, ptr, len)) {
        /* up to 2^94-1  */
        return -1;
    }
    return 0;
}

static
bool TarEntry_TarChecksum(TarBlock* block, bool isgnu)
{
    SHeader* h = &block->header;
    size_t i, len = sizeof(h->checksum) - (isgnu ? 2 : 1);
    unsigned long checksum = 0;
    const unsigned char* p = (const unsigned char*)block->buffer;

    /* Compute the checksum */
    memset(h->checksum, ' ', sizeof(h->checksum));
    for(i = 0; i < sizeof(block->buffer); i++) {
        checksum += *p++;
    }
    /* ustar: '\0'-terminated checksum
       GNU special: 6 digits, then '\0', then a space [already in place] */
    if(!TarEntry_NumToOctal(checksum, h->checksum, len)) {
        return false;
    }
    h->checksum[len] = '\0';
    return true;
}

static
rc_t TarEntry_PackName(const TarEntry* e, TarBlock* block)
{
    SHeader* h = &block[0].header;

    if( e->name_fmt == 0 ) {
        strcpy(h->name, e->name);
    } else if( e->name_fmt > 0 ) {
        /* to split the long name into a prefix and a short name (POSIX) */
        memcpy(h->x.prefix, e->name, e->name_fmt);
        memcpy(h->name, e->name + e->name_fmt + 1, strlen(e->name) - e->name_fmt - 1);
    } else {
        strcpy(h->name, "././@LongLink");
        TarEntry_NumToOctal(0, h->mode, sizeof(h->mode) - 1);
        TarEntry_NumToOctal(0, h->uid, sizeof(h->uid) - 1);
        TarEntry_NumToOctal(0, h->gid, sizeof(h->gid) - 1);
        /* write terminating '\0' as it can always be made to fit in */
        if(!TarEntry_EncodeUint64(strlen(e->name) + 1, h->size, sizeof(h->size) - 1)) {
            return RC(rcExe, rcArc, rcConstructing, rcId, rcOutofrange);
        }
        TarEntry_NumToOctal(0, h->mtime, sizeof(h->mtime)- 1);
        h->typeflag[0] = 'L';
        /* Old GNU magic protrudes into adjacent version field */
        memcpy(h->magic, "ustar  ", 8); /* 2 spaces and '\0'-terminated */
        TarEntry_TarChecksum(block, true);
        strcpy((char*)(&block[1]), e->name);

        /* Still, store the initial part in the original header */
        h = &block[-e->name_fmt - 1].header;
        memcpy(h->name, e->name, sizeof(h->name));
    }
    return 0;
}

static
rc_t TarEntry_Header(const TarEntry* e, TarBlock* block)
{
    rc_t rc = 0;
    /* Update format as we go */

    ETar_Format fmt = eTar_Ustar;
    int ok;
    do {
        uint32_t size =  e->name_fmt < 0 ? -e->name_fmt : 1;
        SHeader* h = &block[size - 1].header;
        memset(block, 0, sizeof(*block) * size);
        if( (rc = TarEntry_PackName(e, block)) != 0 ) {
            break;
        }
        if( !TarEntry_NumToOctal(e->executable ? 00755 : 00644, h->mode, sizeof(h->mode) - 1) ) {
            rc = RC(rcExe, rcArc, rcConstructing, rcData, rcInvalid);
            break;
        }
        TarEntry_EncodeUint64(0, h->uid, sizeof(h->uid) - 1);
        TarEntry_EncodeUint64(0, h->gid, sizeof(h->gid) - 1);

        ok = TarEntry_EncodeUint64(e->size, h->size, sizeof(h->size) - 1);
        if(!ok) {
            rc = RC(rcExe, rcArc, rcConstructing, rcFile, rcTooBig);
            break;
        }
        if(ok < 0) {
            fmt = eTar_OldGNU;
        }
        if(fmt != eTar_Ustar && h->x.prefix[0]) {
            /* cannot downgrade to reflect encoding */
            fmt  = eTar_Ustar;
        }
        /* Modification time */
        if (!TarEntry_NumToOctal(e->mtime, h->mtime, sizeof(h->mtime) - 1)) {
            rc = RC(rcExe, rcArc, rcConstructing, rcId, rcOutofrange);
            break;
        }
        /* limited version: expect only normal files !!! */
        h->typeflag[0] = '0';
        /* User and group */
        memcpy(h->uname, "anyone", 6);
        /* memcpy(h->gname, "?????", 5); */

        /* Device nos to complete the ustar header protocol (all fields ok) */
        if( fmt != eTar_OldGNU ) {
            TarEntry_NumToOctal(0, h->devmajor, sizeof(h->devmajor) - 1);
            TarEntry_NumToOctal(0, h->devminor, sizeof(h->devminor) - 1);
        }
        if(fmt != eTar_OldGNU) {
            /* Magic */
            strcpy(h->magic, "ustar");
            /* Version (EXCEPTION:  not '\0' terminated) */
            memcpy(h->version, "00", 2);
        } else {
            /* Old GNU magic protrudes into adjacent version field */
            memcpy(h->magic, "ustar  ", 8); /* 2 spaces and '\0'-terminated */
        }

        if( !TarEntry_TarChecksum(&block[size - 1], fmt == eTar_OldGNU ? true : false) ) {
            rc = RC(rcExe, rcArc, rcConstructing, rcId, rcOutofrange);
        }
    } while(false);
    return rc;
}

static
rc_t TarEntry_Read(const TarEntry* entry, TarBlock* header_buf, const KFile* kfile, uint64_t pos, uint8_t* buffer, const size_t size, size_t* num_read)
{
    rc_t rc = 0;

    if( pos < entry->data ) {
        /* insert tar header */
        if( (rc = TarEntry_Header(entry, header_buf)) == 0 ) {
            size_t q = entry->data - pos;
            if( q > (size - *num_read) ) {
                q = size - *num_read;
            }
            memcpy(&buffer[*num_read], &header_buf->buffer[pos - entry->start], q);
            *num_read = *num_read + q;
            pos += q;
            DEBUG_LINE(8, "header: %u bytes", q);
        }
    }
    if( rc == 0 && *num_read < size && pos < entry->pad ) {
        /* read file data */
        size_t rd;
        if( (rc = KFileRead(kfile, pos - entry->data, &buffer[*num_read], size - *num_read, &rd)) == 0 ) {
            DEBUG_LINE(8, "file: from %lu %u bytes", pos - entry->data, rd);
            *num_read = *num_read + rd;
            pos += rd;
        }
    }
    if( rc == 0 && *num_read < size && pos <= entry->end ) {
        /* pad last block with 0 */
        size_t q = entry->end - pos + 1;
        if( q > (size - *num_read) ) {
            q = size - *num_read;
        }
        memset(&buffer[*num_read], 0, q);
        *num_read = *num_read + q;
        DEBUG_LINE(8, "padding: %u bytes", q);
    }
    return rc;
}

struct TarFileList {
    KRefcount refcount;
    const TarEntry* files;
    uint32_t count;
    uint32_t max_count;
    uint64_t tar_sz;
    uint16_t tar_10k_pad;
    char* strings;
    uint64_t strings_avail;
    /* used to setup cache in file */
    uint16_t max_header_blocks;
    /* current file data/cache */
    KLock* lock;
    BSTree btree;
    TarBlock* header_buf;
    const TarEntry* cur_entry;
    const KFile* cur_file;
};

rc_t TarFileList_Make(const TarFileList** cself, uint32_t max_file_count, const char* name)
{
    rc_t rc = 0;

    if( cself == NULL || max_file_count == 0 ) {
        rc = RC(rcExe, rcArc, rcConstructing, rcParam, rcInvalid);
    } else {
        TarFileList* l = NULL;
        uint64_t strings_sz = max_file_count * 2048;
        *cself = NULL;
        CALLOC(l, 1, sizeof(*l) + max_file_count * sizeof(*(l->files)) + strings_sz);
        if( l == NULL ) {
            /* allocate self + array of entries + 1024 for path and name */
            rc = RC(rcExe, rcArc, rcConstructing, rcMemory, rcExhausted);
        } else if( (rc = KLockMake(&l->lock)) == 0 ) {
            KRefcountInit(&l->refcount, 1, "TarFileList", "Make", name);
            l->max_count = max_file_count;
            l->tar_sz = TAR_TAIL_PAD;
            l->files = (TarEntry*)&l[1]; /* files array follows self */
            l->strings_avail = strings_sz;
            l->strings = (char*)&l->files[l->max_count]; /* string table follows last entry */
            l->max_header_blocks = 1;
            *cself = l;
        }
    }
    return rc;
}

void TarFileList_Release(const TarFileList* cself)
{
    if( cself != NULL ) {
        TarFileList* self = (TarFileList*)cself;
        if( KLockAcquire(self->lock) == 0 ) {
            int x = KRefcountDropDep(&self->refcount, "TarFileList");
            if( atomic32_read(&self->refcount) < 2 ) {
                self->cur_entry = NULL;
                ReleaseComplain(KFileRelease, self->cur_file);
                self->cur_file = NULL;
            }
            if( x == krefWhack ) {
                KRefcountWhack(&self->refcount, "TarFileList");
                FREE(self->header_buf);
                BSTreeWhack(&self->btree, NULL, NULL);
                ReleaseComplain(KLockUnlock, self->lock);
                ReleaseComplain(KLockRelease, self->lock);
                FREE(self);
            } else {
                ReleaseComplain(KLockUnlock, self->lock);
            }
        }
    }
}

rc_t TarFileList_Add(const TarFileList* cself, const char* path, const char* name, uint64_t size, KTime_t mtime, bool executable)
{
    rc_t rc = 0;

    if( cself == NULL || path == NULL || name == NULL ) {
        rc = RC(rcExe, rcArc, rcRegistering, rcParam, rcInvalid);
    } else if( (rc = KLockAcquire(cself->lock)) == 0 ) {
        if( cself->count >= cself->max_count ) {
            rc = RC(rcExe, rcArc, rcRegistering, rcDirEntry, rcExcessive);
        } else if( cself->header_buf != NULL ) {
            rc = RC(rcExe, rcArc, rcRegistering, rcDirEntry, rcReadonly);
        } else {
            TarFileList* self = (TarFileList*)cself;
            size_t lp = strlen(path) + 1, ln = strlen(name) + 1;

            if( lp + ln  >= cself->strings_avail ) {
                rc = RC(rcExe, rcArc, rcRegistering, rcName, rcExcessive);
            } else {
                const TarEntry* e = &cself->files[cself->count];

                /* copy path/name to internal buffer */
                strncpy(cself->strings, path, lp);
                path = cself->strings;
                self->strings += lp;
                self->strings_avail -= lp;
                strncpy(cself->strings, name, ln);
                name = cself->strings;
                self->strings += ln;
                self->strings_avail -= ln;

                TarEntry_Init(e, cself->tar_sz - TAR_TAIL_PAD, path, name, size, mtime, executable);
                self->tar_sz += e->end - e->start + 1;
                self->tar_10k_pad = self->tar_sz % TAR_BLOCK_PAD;
                if( self->tar_10k_pad != 0 ) {
                    self->tar_10k_pad = TAR_BLOCK_PAD - self->tar_10k_pad;
                }
                if( e->name_fmt < 0 && cself->max_header_blocks < -e->name_fmt ) {
                    self->max_header_blocks = -e->name_fmt;
                }
                self->count++;
            }
        }
        ReleaseComplain(KLockUnlock, cself->lock);
    }
    return rc;
}

rc_t TarFileList_Size(const TarFileList* cself, uint64_t* file_sz)
{
    if( cself == NULL || file_sz == NULL ) {
        return RC(rcExe, rcArc, rcAccessing, rcParam, rcInvalid);
    }
    *file_sz = cself->tar_sz + cself->tar_10k_pad;
    return 0;
}

static
int TarFileList_Sort(const BSTNode *item, const BSTNode *node)
{
    const TarEntry* i = (const TarEntry*)item;
    const TarEntry* n = (const TarEntry*)node;

    if( i->start > n->end ) {
        return 1;
    } else if( i->end < n->start ) {
        return -1;
    }
    /* fails on insert! */
    return 0;
}

rc_t TarFileList_Open(const TarFileList* cself)
{
    rc_t rc = 0;
    if( cself == NULL ) {
        rc = RC(rcExe, rcArc, rcOpening, rcParam, rcInvalid);
    } else if( (rc = KLockAcquire(cself->lock)) == 0 ) {
        TarFileList* self = (TarFileList*)cself;
        if( self->header_buf == NULL ) {
            MALLOC(self->header_buf, self->max_header_blocks * TAR_BLOCK_SIZE);
            if( self->header_buf == NULL ) {
                rc = RC(rcExe, rcArc, rcOpening, rcMemory, rcExhausted);
            } else {
                /* build file offset search tree */
                uint32_t i;
                BSTreeInit(&self->btree);
                /* adjust last file size to include archive-end of 2 512 blocks + 10k block padding */
                ((TarEntry*)(self->files + self->count - 1))->end += TAR_TAIL_PAD + self->tar_10k_pad;
                self->tar_sz += self->tar_10k_pad;
                self->tar_10k_pad = 0;
                for(i = 0; rc == 0 && i < self->count; i++) {
                    rc = BSTreeInsertUnique(&self->btree, (BSTNode*)&self->files[i].node, NULL, TarFileList_Sort);
                }
            }
        }
        if( rc == 0 && KRefcountAddDep(&self->refcount, "TarFileList") != krefOkay ) {
            rc = RC(rcExe, rcArc, rcOpening, rcRefcount, rcFailed);
        }
        ReleaseComplain(KLockUnlock, cself->lock);
    }
    return rc;
}

static
int TarFileList_Find(const void *item, const BSTNode *node)
{
    uint64_t pos = *(uint64_t*)item;
    const TarEntry* n = (const TarEntry*)node;
    if( pos < n->start ) {
        return -1;
    } else if( pos > n->end ) {
        return 1;
    }
    return 0;
}

rc_t TarFileList_Read(const TarFileList* cself, uint64_t pos, uint8_t* buffer, size_t size, size_t* num_read)
{
    rc_t rc = 0;

    if( cself == NULL ) {
        rc = RC(rcExe, rcArc, rcReading, rcParam, rcInvalid);
    } else if( (rc = KLockAcquire(cself->lock)) == 0 ) {
        *num_read = 0;
        if( cself->header_buf == NULL ) {
            rc = RC(rcExe, rcArc, rcReading, rcBuffer, rcNull);
        } else if( pos < cself->tar_sz ) {
            while( rc == 0 && *num_read < size && pos < cself->tar_sz ) {
                if( cself->cur_entry == NULL || pos < cself->cur_entry->start || pos > cself->cur_entry->end ) {
                    KFileRelease(cself->cur_file);
                    ((TarFileList*)cself)->cur_entry = (const TarEntry*)BSTreeFind(&cself->btree, &pos, TarFileList_Find);
                    if( cself->cur_entry != NULL ) {
                        KDirectory* dir;
                        DEBUG_LINE(8, "Open: '%s'", cself->cur_entry->path);
                        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
                            if( (rc = KDirectoryOpenFileRead(dir, &((TarFileList*)cself)->cur_file, "%s", cself->cur_entry->path)) == 0 ) {
                                uint64_t size = 0;
                                if( (rc = KFileSize(cself->cur_file, &size)) == 0 ) {
                                    if( cself->cur_entry->size != size ) {
                                        rc = RC(rcExe, rcArc, rcReading, rcFileDesc, rcInvalid);
                                        PLOGERR(klogErr, (klogErr, rc, "Source file size differ in XML $(p)", PLOG_S(p), cself->cur_entry->path));
                                    }
                                }
                            } else {
                                PLOGERR(klogErr, (klogErr, rc, "$(p)", PLOG_S(p), cself->cur_entry->path));
                            }
                            ReleaseComplain(KDirectoryRelease, dir);
                        }
                    } else {
                        rc = RC(rcExe, rcArc, rcReading, rcOffset, rcInvalid);
                    }
                }
                if( rc == 0 ) {
                    size_t rd = *num_read;
                    DEBUG_LINE(8, "Reading: '%s'", cself->cur_entry->path);
                    if( (rc = TarEntry_Read(cself->cur_entry, cself->header_buf, cself->cur_file, pos, buffer, size, num_read)) == 0 ) {
                        DEBUG_LINE(8, "read: %u bytes", *num_read);
                        pos += *num_read - rd;
                    }
                }
            }
        }
        ReleaseComplain(KLockUnlock, cself->lock);
#if _DEBUGGING
        if( rc == 0 && size < *num_read ) {
            PLOGERR(klogErr, (klogErr, RC(rcExe, rcArc, rcReading, rcData, rcTooLong),
                "file $(f) at pos $(p): $(s) < $(n)", "p=%lu,s=%lu,n=%lu,f=%s", pos, size, *num_read, cself->cur_entry->path));
        }
#endif
    }
    return rc;
}
