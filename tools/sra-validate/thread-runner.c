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
#include "thread-runner.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

typedef struct thread_runner
{
    Vector ThreadVector;
} thread_runner;

rc_t destroy_thread_runner( thread_runner * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        uint32_t len = VectorLength( &self -> ThreadVector );
        uint32_t idx;
        rc_t status = 0;
        for ( idx = 0; rc == 0 && idx < len; ++idx )
        {
            KThread *t = VectorGet ( &self -> ThreadVector, idx );
            if ( t != NULL )
            {
                rc_t thread_rc; 
                rc = KThreadWait ( t, &thread_rc );
                if ( rc == 0 )
                {
                    if ( status == 0 )
                        status = thread_rc;
                    KThreadRelease ( t );
                }
            }
        }
        if ( rc == 0 )
            rc = status;
            
        free( ( void * ) self );
    }
    return rc;
}

rc_t make_thread_runner( thread_runner ** runner )
{
    rc_t rc = 0;
    thread_runner * r = calloc( 1, sizeof * r );
    if ( r == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_thread_runner().calloc( %d ) -> %R", ( sizeof * r ), rc );
    }
    else
    {
        VectorInit ( & r -> ThreadVector, 0, 10 );
        *runner = r;
    }
    return rc;
}

rc_t thread_runner_add( thread_runner * self,
                        rc_t ( CC * run_thread ) ( const KThread *self, void *data ),
                        void *data )
{
    rc_t rc = 0;
    if ( self == NULL || run_thread == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
        ErrMsg( "thread_runner_add() -> %R", rc );
    }
    else
    {
        KThread *t;
        rc = KThreadMake ( &t, run_thread, data );
        if ( rc != 0 )
            ErrMsg( "thread_runner_add().KThreadMake() -> %R", rc );
        else
        {
            rc = VectorAppend ( &self -> ThreadVector, NULL, t );
            if ( rc != 0 )
            {
                ErrMsg( "thread_runner_add().VectorAppend() -> %R", rc );
                KThreadRelease ( t );
            }
        }
    }
    return rc;
}
