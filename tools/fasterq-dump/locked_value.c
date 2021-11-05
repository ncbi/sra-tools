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

#include "locked_value.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

rc_t locked_value_init( locked_value_t * self, uint64_t init_value ) {
    rc_t rc;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_value_init() -> %R", rc );
    } else {
        rc = KLockMake ( &( self -> lock ) );
        if ( 0 != rc ) {
            ErrMsg( "locked_value_init().KLockMake() -> %R", rc );
        } else {
            self -> value = init_value;
        }
    }
    return rc;
}

void locked_value_release( locked_value_t * self ) {
    if ( NULL != self ) {
        rc_t rc = KLockRelease ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_value_init().KLockRelease() -> %R", rc );
        }
    }
}

rc_t locked_value_get( locked_value_t * self, uint64_t * value ) {
    rc_t rc;
    if ( NULL == self || NULL == value ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_value_get() -> %R", rc );
    } else {
        rc = KLockAcquire ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_value_get().KLockAcquire -> %R", rc );
        } else {
            *value = self -> value;
            rc = KLockUnlock ( self -> lock );
            if ( 0 != rc ) {
                ErrMsg( "locked_value_get().KLockUnlock -> %R", rc );
            }
        }
    }
    return rc;
}

rc_t locked_value_set( locked_value_t * self, uint64_t value ) {
    rc_t rc;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_value_set() -> %R", rc );
    } else {
        rc = KLockAcquire ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_value_set().KLockAcquire -> %R", rc );
        } else {
            self -> value = value;
            rc = KLockUnlock ( self -> lock );
            if ( 0 != rc ) {
                ErrMsg( "locked_value_set().KLockUnlock -> %R", rc );
            }
        }
    }
    return rc;
}
