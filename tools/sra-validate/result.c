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
#include "result.h"

/* 
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */

 #ifndef _h_atomic_
#include <atomic.h>
#endif

#ifndef _h_atomic64_
#include <atomic64.h>
#endif

#ifndef _h_cmn_
#include "cmn.h"
#endif

typedef struct validate_result
{
    atomic64_t records;
    atomic64_t errors;
} validate_result;

void destroy_validate_result( validate_result * self )
{
    if ( self != NULL )
    {
        free( ( void * ) self );
    }
}

rc_t make_validate_result( validate_result ** obj )
{
    rc_t rc = 0;
    validate_result * vr = calloc( 1, sizeof * vr );
    if ( vr == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_validate_result().calloc( %d ) -> %R", ( sizeof * vr ), rc );
    }
    else
    {
        *obj = vr;
    }
    return rc;
}

rc_t update_validate_result( struct validate_result * self, uint32_t errors )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "update_validate_result() -> %R", rc );
    }
    else
    {
        atomic64_inc( &self -> records );
        atomic64_read_and_add( &self -> errors, errors );
    }
    return rc;
}

rc_t print_validate_result( struct validate_result * self, struct logger * log )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "update_validate_result() -> %R", rc );
    }
    else
    {
        uint64_t records = atomic64_read( &self -> records );
        uint64_t errors  = atomic64_read( &self -> errors );
        
        log_write( log, "rows   = %,lu", records );
        log_write( log, "errors = %,lu", errors );
    }
    return rc;
}
