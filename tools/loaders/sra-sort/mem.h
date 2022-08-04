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

#ifndef _h_sra_sort_mem_
#define _h_sra_sort_mem_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif


/*--------------------------------------------------------------------------
 * Mem
 *  memory allocation via MemBank in Caps
 */
void *MemAlloc ( const ctx_t *ctx, size_t bytes, bool clear );
void MemFree ( const ctx_t *ctx, void *mem, size_t bytes );
size_t MemInUse ( const ctx_t *ctx, size_t *opt_quota );



/*--------------------------------------------------------------------------
 * MemBank
 *  very, very, very watered down memory bank
 */
typedef struct MemBank_vt MemBank_vt;

typedef struct MemBank MemBank;
struct MemBank
{
    const MemBank_vt *vt;
    KRefcount refcount;
    uint32_t align;
};

#ifndef MEMBANK_IMPL
#define MEMBANK_IMPL MemBank
#endif

struct MemBank_vt
{
    void ( * whack ) ( MEMBANK_IMPL *self, const ctx_t *ctx );
    size_t ( * in_use ) ( const MEMBANK_IMPL *self, const ctx_t *ctx, size_t *opt_quota );
    void* ( * alloc ) ( MEMBANK_IMPL *self, const ctx_t *ctx, size_t bytes, bool clear );
    void ( * free ) ( MEMBANK_IMPL *self, const ctx_t *ctx, void *mem, size_t bytes );
};


/* Make
 *  make a new memory bank
 */
MemBank *MemBankMake ( const ctx_t *ctx, size_t quota );


/* MakePaged
 *  make a new paged memory bank
 */
MemBank *MemBankMakePaged ( const ctx_t *ctx, size_t quota, size_t pgsize );


/* Duplicate
 * Release
 */
MemBank *MemBankDuplicate ( const MemBank *self, const ctx_t *ctx );
void MemBankRelease ( const MemBank *self, const ctx_t *ctx );


/* InUse
 *  report the number of bytes in use
 *  optionally returns quota
 */
#define MemBankInUse( self, ctx, opt_quota ) \
    POLY_DISPATCH_INT ( in_use, self, const MEMBANK_IMPL, ctx, opt_quota )


/* Alloc
 *  allocates some memory from bank
 */
#define MemBankAlloc( self, ctx, bytes, clear ) \
    POLY_DISPATCH_PTR ( alloc, self, MEMBANK_IMPL, ctx, bytes, clear )


/* Free
 *  returns memory to bank
 *  ignored by paged bank
 *  burden on caller to remember allocation size
 */
#define MemBankFree( self, ctx, mem, bytes ) \
    POLY_DISPATCH_VOID ( free, self, MEMBANK_IMPL, ctx, mem, bytes )


/* Init
 */
void MemBankInit ( MemBank *self, const ctx_t *ctx, const MemBank_vt *vt, const char *name );

/* Destroy
 */
#define MemBankDestroy( self, ctx ) \
    ( ( void ) 0 )

#endif
