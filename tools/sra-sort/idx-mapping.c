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

#include "idx-mapping.h"
#include "ctx.h"

#include <klib/sort.h>

FILE_ENTRY ( idx-mapping );


/*--------------------------------------------------------------------------
 * IdxMapping
 *  
 */

#if USE_OLD_KSORT

int CC IdxMappingCmpOld ( const void *a, const void *b, void *data )
{
    const IdxMapping *ap = a;
    const IdxMapping *bp = b;

    if ( ap -> old_id < bp -> old_id )
        return -1;
    return ap -> old_id > bp -> old_id;
}

int CC IdxMappingCmpNew ( const void *a, const void *b, void *data )
{
    const IdxMapping *ap = a;
    const IdxMapping *bp = b;

    if ( ap -> new_id < bp -> new_id )
        return -1;
    return ap -> new_id > bp -> new_id;
}

#else /* USE_OLD_KSORT */

#define T( x ) ( ( const IdxMapping* ) ( x ) )

#define SWAP( a, b, off, size ) KSORT_TSWAP ( IdxMapping, a, b )


void IdxMappingSortOld ( IdxMapping *self, const ctx_t *ctx, size_t count )
{
#define CMP( a, b ) \
    ( ( T ( a ) -> old_id < T ( b ) -> old_id ) ? -1 : ( T ( a ) -> old_id > T ( b ) -> old_id ) )

    KSORT ( self, count, sizeof * self, 0, sizeof * self );

#undef CMP
}

void IdxMappingSortNew ( IdxMapping *self, const ctx_t *ctx, size_t count )
{
#define CMP( a, b ) \
    ( ( T ( a ) -> new_id < T ( b ) -> new_id ) ? -1 : ( T ( a ) -> new_id > T ( b ) -> new_id ) )

    KSORT ( self, count, sizeof * self, 0, sizeof * self );

#undef CMP
}

#undef T
#undef SWAP

#endif /* USE_OLD_KSORT */
