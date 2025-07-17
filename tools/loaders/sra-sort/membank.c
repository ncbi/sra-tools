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

typedef struct MemBankImpl MemBankImpl;
#define MEMBANK_IMPL MemBankImpl

#include <sysalloc.h>

#include "caps.h"
#include "ctx.h"
#include "mem.h"
#include "except.h"
#include "status.h"

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/mmap.h>
#include <klib/refcount.h>
#include <klib/container.h>
#include <klib/rc.h>
#include <atomic64.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>

FILE_ENTRY ( membank );


/*--------------------------------------------------------------------------
 * MemBankImpl
 *  maintains a quota
 */
struct MemBankImpl
{
    MemBank dad;
    size_t quota;
    atomic64_t avail;
};

static
void MemBankImplWhack ( MemBankImpl *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    Caps *caps;

    MemBankDestroy ( & self -> dad, ctx );

    caps = ( Caps* ) ctx -> caps;
    if ( & self -> dad != caps -> mem )
        MemBankFree ( caps -> mem, ctx, self, sizeof * self );
    else
    {
        free ( self );
        caps -> mem = NULL;
    }
}


/* InUse
 *  report the number of bytes in use
 *  optionally returns quota
 */
static
size_t MemBankImplInUse ( const MemBankImpl *self, const ctx_t *ctx, size_t *quota )
{
    size_t dummy;

    if ( quota == NULL )
        quota = & dummy;

    if ( self != NULL )
    {
        * quota = self -> quota;
        return self -> quota - atomic64_read ( & self -> avail );
    }

    * quota = 0;
    return 0;
}


/* Alloc
 *  allocates some memory from bank
 */
static
void *MemBankImplAlloc ( MemBankImpl *self, const ctx_t *ctx, size_t bytes, bool clear )
{
    assert(bytes <= LONG_MAX); /* can safely be a long */
    if ( self != NULL )
    {
        /* allocation from the raw bank */
        rc_t rc;
        FUNC_ENTRY ( ctx );

        /* update "avail" atomicially */
        long avail = atomic64_read ( & self -> avail );
        while (bytes <= avail) {
            long old = atomic64_test_and_set(&self->avail, avail - (long)bytes, avail);
            if (old == avail) {
                void *mem = calloc(1, bytes);
                if (mem) {
                    if ( bytes > 1024 * 1024 )
                        STATUS ( 3, "allocated %,zu bytes of memory", bytes );
                    else if ( bytes > 256 * 1024 )
                        STATUS ( 4, "allocated %,zu bytes of memory", bytes );
                }
                else {
                    /* failed to get memory */
                    atomic64_add ( & self -> avail, ( long ) bytes );
                    rc = RC ( rcExe, rcMemory, rcAllocating, rcMemory, rcExhausted );
                    ERROR ( rc, "memory exhausted allocating %zu bytes of memory", bytes );
                }
                return mem;
            }
            avail = old;
        }
        /* at this point we could be using a timeout */
        rc = RC ( rcExe, rcMemory, rcAllocating, rcRange, rcExcessive );
        ERROR ( rc, "quota exceeded allocating %zu bytes of memory", bytes );
    }

    return NULL;
}


/* Free
 *  returns memory to bank
 *  ignored by paged bank
 *  burden on caller to remember allocation size
 */
static
void MemBankImplFree ( MemBankImpl *self, const ctx_t *ctx, void *mem, size_t bytes )
{
    assert(bytes <= LONG_MAX); /* can safely be a long */
    if ( mem != NULL )
    {
        FUNC_ENTRY ( ctx );

        if ( self == NULL )
        {
            rc_t rc = RC ( rcExe, rcMemory, rcReleasing, rcSelf, rcNull );
            ERROR ( rc, "attempt to return memory to NULL MemBank" );
        }
        else
        {
            /* release the memory */
            free ( mem );

            /* update the counters */
            if ( bytes == 0 )
                ANNOTATE ( "freed memory with size of 0 bytes" );
            else
            {
                atomic64_add ( & self -> avail, ( long ) bytes );

                if ( bytes > 256 * 1024 )
                {
                    if ( bytes > 1024 * 1024 )
                        STATUS ( 3, "freed %,zu bytes of memory", bytes );
                    else
                        STATUS ( 4, "freed %,zu bytes of memory", bytes );
                }
            }
        }
    }
}

static MemBank_vt MemBankImpl_vt =
{
    MemBankImplWhack,
    MemBankImplInUse,
    MemBankImplAlloc,
    MemBankImplFree
};


/*--------------------------------------------------------------------------
 * MemBank
 *  very, very, very watered down memory bank
 */


/* Make
 *  make a new memory bank
 *  in a real system, this could make a bank linked to
 *  the primordial one. in this system, there is only
 *  a single bank, so this function is overloaded to
 *  create both the primordial and to reset quota on it.
 */
MemBank *MemBankMake ( const ctx_t *ctx, size_t quota )
{
    FUNC_ENTRY ( ctx );

    MemBankImpl *mem = malloc ( sizeof * mem );
    if ( mem == NULL )
        exit ( -1 );

    MemBankInit ( & mem -> dad, ctx, & MemBankImpl_vt, "Process MemBank" );

    if ( quota < 4096 )
        quota = 4096;
    if ( quota > LONG_MAX )
        quota = LONG_MAX;

    mem -> quota = quota;
    atomic64_set ( & mem -> avail, ( long ) ( quota - sizeof * mem ) );

    return & mem -> dad;
}


/* Init
 */
void MemBankInit ( MemBank *self, const ctx_t *ctx, const MemBank_vt *vt, const char *name )
{
    FUNC_ENTRY ( ctx );

    self -> vt = vt;
    KRefcountInit ( & self -> refcount, 1, "MemBank", "Make", name );
    self -> align = 0;
}


/* Duplicate
 * Release
 */
MemBank *MemBankDuplicate ( const MemBank *self, const ctx_t *ctx )
{
    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "MemBank" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative: {
            FUNC_ENTRY ( ctx );
            rc_t rc = RC ( rcExe, rcMemory, rcAttaching, rcRange, rcExcessive );
            ERROR ( rc, "excessive references on MemBank" );
            return NULL;
        }}
    }
    return ( MemBank* ) self;
}

void MemBankRelease ( const MemBank *self, const ctx_t *ctx )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "MemBank" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( MEMBANK_IMPL* ) self, ctx );
            break;
        case krefZero:
            break;
        case krefLimit:
        case krefNegative: {
            FUNC_ENTRY ( ctx );
            rc_t rc = RC ( rcExe, rcMemory, rcReleasing, rcRange, rcExcessive );
            ERROR ( rc, "excessive releases on MemBank" );
        }}
    }
}
