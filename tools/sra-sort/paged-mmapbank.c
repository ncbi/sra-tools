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

typedef struct PagedMMapBank PagedMMapBank;
#define MEMBANK_IMPL PagedMMapBank

#include "caps.h"
#include "ctx.h"
#include "mem.h"
#include "except.h"
#include "status.h"
#include "sra-sort.h"

#include "membank-priv.h"

#include <kfs/mmap.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <klib/container.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <string.h>

FILE_ENTRY ( paged-mmapbank );


/*--------------------------------------------------------------------------
 * MMapPage
 *  a "page" which is really an entire mmap
 */
typedef struct MMapPage MMapPage;
struct MMapPage
{
    SLNode n;
    KMMap *mmap;
    uint8_t *addr;
    size_t size;
    size_t used;
};

static
void CC MMapPageWhack ( SLNode *n, void *data )
{
    MMapPage *self = ( MMapPage* ) n;
    const ctx_t *ctx = ( const void* ) data;

    FUNC_ENTRY ( ctx );

    rc_t rc = KMMapRelease ( self -> mmap );
    if ( rc != 0 )
        SYSTEM_ERROR ( rc, "KMMapRelease failed" );

    MemFree ( ctx, self, sizeof * self );
}


/*--------------------------------------------------------------------------
 * PagedMMapBank
 *  a memory bank based upon system mmap
 */
struct PagedMMapBank
{
    MemBank dad;
    size_t quota;
    size_t used;
    size_t pgsize;
    KFile *backing;
    SLList pages;
};


/* Whack
 */
static
void PagedMMapBankWhack ( PagedMMapBank *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    SLListWhack ( & self -> pages, MMapPageWhack, ( void* ) ctx );

    rc = KFileRelease ( self -> backing );
    if ( rc != 0 )
        SYSTEM_ERROR ( rc, "KFileRelease failed on backing file" );
    self -> backing = NULL;

    MemBankDestroy ( & self -> dad, ctx );

    MemFree ( ctx, self, sizeof * self );
}


/* InUse
 *  report the number of bytes in use
 *  optionally returns quota
 */
static
size_t PagedMMapBankInUse ( const PagedMMapBank *self, const ctx_t *ctx, size_t *quota )
{
    size_t dummy;

    if ( quota == NULL )
        quota = & dummy;

    if ( self != NULL )
    {
        * quota = self -> quota;
        return self -> used;
    }

    * quota = 0;
    return 0;
}


/* CreateBacking
 */
static
void PagedMMapBankCreateBacking ( PagedMMapBank *self, const ctx_t *ctx, KFile **backing )
{
    FUNC_ENTRY ( ctx );

    KDirectory *wd;
    rc_t rc = KDirectoryNativeDir ( & wd );
    if ( rc != 0 )
        ABORT ( rc, "KDirectoryNativeDir failed" );
    else
    {
        static uint32_t file_no;
        const Tool *tp = ctx -> caps -> tool;

        ++ file_no;
        STATUS ( 5, "creating backing file '%s/sra-sort-buffer.%d.%u'", tp -> mmapdir, tp -> pid, file_no );

        rc = KDirectoryCreateFile ( wd, backing, true,
            0600, kcmInit | kcmParents, "%s/sra-sort-buffer.%d.%u",
            tp -> mmapdir, tp -> pid, file_no );
        if ( rc != 0 )
            ABORT ( rc, "KDirectoryCreateFile failed when creating mem-mapped buffer" );
        else
        {
#if ! WINDOWS
            /* unlinking file is a wonderful thing */
            if ( tp -> unlink_idx_files )
            {
                /* unlink KFile */
                STATUS ( 5, "unlinking backing file '%s/sra-sort-buffer.%d.%u'", tp -> mmapdir, tp -> pid, file_no );
                rc = KDirectoryRemove ( wd, false, "%s/sra-sort-buffer.%d.%u",
                    tp -> mmapdir, tp -> pid, file_no );
                if ( rc != 0 )
                    WARN ( "failed to unlink mem-mapped buffer file #%u", file_no );
            }
#endif
            /* set initial size to a page */
            rc = KFileSetSize ( * backing, self -> pgsize );
            if ( rc != 0 )
                SYSTEM_ERROR ( rc, "KFileSetSize failed to set file to %zu bytes", self -> pgsize );
        }

        KDirectoryRelease ( wd );
    }
}


/* ExtendBacking
 *  creates a backing file if not there
 *  extends it otherwise
 */
static
void PagedMMapBankExtendBacking ( PagedMMapBank *self, const ctx_t *ctx )
{
#if USE_SINGLE_BACKING_FILE

    /* this code is only useful if using a single file for all maps */
    FUNC_ENTRY ( ctx );

    /* create the file on first attempt */
    if ( self -> backing == NULL )
    {
        assert ( self -> used == 0 );
        PagedMMapBankCreateBacking ( self, ctx, & self -> backing )l
    }
    else
    {
        /* extend the file */
        rc_t rc;
        STATUS ( 4, "extending common buffer file to %,zu bytes", self -> used + self -> pgsize );
        rc = KFileSetSize ( self -> backing, self -> used + self -> pgsize );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "KFileSetSize failed to extend file size to %zu bytes", self -> used + self -> pgsize );
    }
#endif
}


/* MapPage
 */
static
void PagedMMapBankMapPage ( PagedMMapBank *self, const ctx_t *ctx, MMapPage *pg )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

#if USE_SINGLE_BACKING_FILE
    STATUS ( 4, "allocating new mmap of %,zu bytes onto common file at offset %,zu", self -> pgsize, self -> used );
    rc = KMMapMakeRgnUpdate ( & pg -> mmap, self -> backing, self -> used, self -> pgsize );
#else
    KFile *backing;
    ON_FAIL ( PagedMMapBankCreateBacking ( self, ctx, & backing ) )
        return;

    STATUS ( 4, "allocating new mmap of %,zu bytes onto its own file", self -> pgsize );
    rc = KMMapMakeRgnUpdate ( & pg -> mmap, backing, 0, self -> pgsize );
    KFileRelease ( backing );
#endif

    if ( rc != 0 )
        SYSTEM_ERROR ( rc, "KMMapMakeRgnUpdate failed creating %zu byte region", self -> pgsize );
    else
    {
        rc = KMMapAddrUpdate ( pg -> mmap, ( void** ) & pg -> addr );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "KMMapAddrUpdate failed" );
        else
        {
            rc = KMMapSize ( pg -> mmap, & pg -> size );
            if ( rc != 0 )
                INTERNAL_ERROR ( rc, "KMMapSize failed" );
            else
            {
                pg -> used = 0;
                self -> used += self -> pgsize;
                STATUS ( 4, "total mem-mapped buffer space: %,zu bytes", self -> used );
                return;
            }
        }

        KMMapRelease ( pg -> mmap );
        pg -> mmap = NULL;
    }
}


/* Alloc
 *  allocates some memory from bank
 */
static
void *PagedMMapBankAlloc ( PagedMMapBank *self, const ctx_t *ctx, size_t bytes, bool clear )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    void *mem;
    size_t avail;

    /* check current alloc page */
    MMapPage *pg = ( MMapPage* ) SLListHead ( & self -> pages );
    if ( pg != NULL )
    {
        /* calculate bytes available */
        avail = pg -> size - pg -> used;
        if ( avail >= bytes )
        {
            /* got the memory */
            mem = & pg -> addr [ pg -> used ];
            pg -> used += bytes;

            /* zero it out if so requested */
            if ( clear )
                memset ( mem, 0, bytes );

            return mem;
        }
    }

    /* either didn't have a page or didn't have enough memory
       test for being able to fit in ANY page */
    if ( bytes > self -> pgsize )
    {
        rc = RC ( rcExe, rcMemory, rcAllocating, rcData, rcExcessive );
        ERROR ( rc, "requested memory allocation of %zu bytes is too large to fit into a page", bytes );
        return NULL;
    }

    /* allocate the bytes */
    if ( self -> used + self -> pgsize <= self -> quota )
    {
        /* ask process for memory block */
        TRY ( pg = MemAlloc ( ctx, sizeof * pg, false ) )
        {
            /* increase size of underlying file */
            TRY ( PagedMMapBankExtendBacking ( self, ctx ) )
            {
                TRY ( PagedMMapBankMapPage ( self, ctx, pg ) )
                {
                    /* got it - push it onto stack */
                    SLListPushHead ( & self -> pages, & pg -> n );

                    /* clear memory of asked */
                    mem = pg  -> addr;
                    pg -> used = bytes;
                    if ( clear )
                        memset ( mem, 0, bytes );

                    return mem;
                }
            }

            MemFree ( ctx, pg, sizeof * pg );
        }

        return NULL;
    }

    /* at this point we could be using a timeout */
    rc = RC ( rcExe, rcMemory, rcAllocating, rcRange, rcExcessive );
    ERROR ( rc, "quota exceeded allocating %zu bytes of page memory for a %zu byte block", self -> pgsize, bytes );

    return NULL;
}


/* Free
 *  ignored by paged bank
 */
static
void PagedMMapBankFree ( PagedMMapBank *self, const ctx_t *ctx, void *mem, size_t bytes )
{
}


static MemBank_vt PagedMMapBank_vt =
{
    PagedMMapBankWhack,
    PagedMMapBankInUse,
    PagedMMapBankAlloc,
    PagedMMapBankFree
};


/*--------------------------------------------------------------------------
 * MemBank
 *  very, very, very watered down memory bank
 */

/* MakePagedMMap
 *  make a new paged memory bank backed by memory map
 */
MemBank *MemBankMakePagedMMap ( const ctx_t *ctx, size_t quota, size_t pgsize )
{
    FUNC_ENTRY ( ctx );
    PagedMMapBank *mem;

    if ( pgsize < 256 * 1024 * 1024 )
        pgsize = 256 * 1024 * 1024;

    if ( quota < pgsize )
        quota = pgsize;

    TRY ( mem = MemAlloc ( ctx, sizeof * mem, true ) )
    {
        TRY ( MemBankInit ( & mem -> dad, ctx, & PagedMMapBank_vt, "Memory Mapped Task MemBank" ) )
        {
            mem -> quota = quota;
            mem -> pgsize = pgsize;
            return & mem -> dad;
        }

        MemFree ( ctx, mem, sizeof * mem );
    }

    return NULL;
}
