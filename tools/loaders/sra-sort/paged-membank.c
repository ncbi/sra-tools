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

typedef struct PagedMemBank PagedMemBank;
#define MEMBANK_IMPL PagedMemBank

#include "caps.h"
#include "ctx.h"
#include "mem.h"
#include "except.h"
#include "status.h"
#include "sra-sort.h"

#include "membank-priv.h"

#include <klib/container.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <string.h>

FILE_ENTRY ( paged-membank );


/*--------------------------------------------------------------------------
 * MemPage
 */
typedef struct MemPage MemPage;
struct MemPage
{
    SLNode n;
    size_t used;
};

/*--------------------------------------------------------------------------
 * PagedMemBank
 *  designed to allocate a lot of little bits of memory
 *  and then free them all at once
 */
struct PagedMemBank
{
    MemBank dad;
    size_t quota;
    size_t avail;
    size_t pgsize;
    SLList pages;
};


/* Whack
 */
static
void PagedMemBankWhack ( PagedMemBank *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    MemPage *pg = ( MemPage* ) SLListPopHead ( & self -> pages );
    while ( pg != NULL )
    {
        MemFree ( ctx, pg, self -> pgsize );
        pg = ( MemPage* ) SLListPopHead ( & self -> pages );
    }

    MemBankDestroy ( & self -> dad, ctx );

    MemFree ( ctx, self, sizeof * self );
}


/* InUse
 *  report the number of bytes in use
 *  optionally returns quota
 */
static
size_t PagedMemBankInUse ( const PagedMemBank *self, const ctx_t *ctx, size_t *quota )
{
    size_t dummy;

    if ( quota == NULL )
        quota = & dummy;

    if ( self != NULL )
    {
        * quota = self -> quota;
        return self -> quota - self -> avail;
    }

    * quota = 0;
    return 0;
}


/* Alloc
 *  allocates some memory from bank
 */
static
void *PagedMemBankAlloc ( PagedMemBank *self, const ctx_t *ctx, size_t bytes, bool clear )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    void *mem;
    size_t avail;

    /* check current alloc page */
    MemPage *pg = ( MemPage* ) SLListHead ( & self -> pages );
    if ( pg != NULL )
    {
        /* calculate bytes available */
        avail = self -> pgsize - pg -> used;
        if ( avail >= bytes )
        {
            /* got the memory */
            mem = & ( ( uint8_t* ) pg ) [ pg -> used ];
            pg -> used += bytes;

            /* zero it out if so requested */
            if ( clear )
                memset ( mem, 0, bytes );

            return mem;
        }
    }

    /* either didn't have a page or didn't have enough memory
       test for being able to fit in ANY page */
    if ( bytes > self -> pgsize - sizeof * pg )
    {
        rc = RC ( rcExe, rcMemory, rcAllocating, rcData, rcExcessive );
        ERROR ( rc, "requested memory allocation of %zu bytes is too large to fit into a page", bytes );
        return NULL;
    }

    /* allocate the bytes */
    if ( self -> avail >= self -> pgsize )
    {
        /* going to allocate a full page */
        self -> avail -= self -> pgsize;

        /* ask process for memory block */
        TRY ( pg = MemAlloc ( ctx, self -> pgsize, false ) )
        {
            /* got it - push it onto stack */
            pg -> used = sizeof * pg + bytes;
            SLListPushHead ( & self -> pages, & pg -> n );

            /* clear memory of asked */
            mem = pg + 1;
            if ( clear )
                memset ( mem, 0, bytes );

            return mem;
        }

        /* return with error from main MemBank */
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
void PagedMemBankFree ( PagedMemBank *self, const ctx_t *ctx, void *mem, size_t bytes )
{
}


static MemBank_vt PagedMemBank_vt =
{
    PagedMemBankWhack,
    PagedMemBankInUse,
    PagedMemBankAlloc,
    PagedMemBankFree
};


/*--------------------------------------------------------------------------
 * MemBank
 *  very, very, very watered down memory bank
 */

/* MakePaged
 *  make a new paged memory bank
 */
MemBank *MemBankMakePaged ( const ctx_t *ctx, size_t quota, size_t pgsize )
{
    FUNC_ENTRY ( ctx );
    PagedMemBank *mem;

    /* special case for using memory mapped banks */
    if ( ctx -> caps -> tool -> mmapdir != NULL )
        return MemBankMakePagedMMap ( ctx, quota, pgsize );

    if ( pgsize < 4096 )
        pgsize = 4096;

    if ( quota < pgsize )
        quota = pgsize;

    TRY ( mem = MemAlloc ( ctx, sizeof * mem, true ) )
    {
        TRY ( MemBankInit ( & mem -> dad, ctx, & PagedMemBank_vt, "Task MemBank" ) )
        {
            mem -> quota = mem -> avail = quota;
            mem -> pgsize = pgsize;
            return & mem -> dad;
        }

        MemFree ( ctx, mem, sizeof * mem );
    }

    return NULL;
}
