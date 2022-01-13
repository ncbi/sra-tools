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

#include "locked_file_list.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

rc_t locked_file_list_init( locked_file_list_t * self, uint32_t alloc_blocksize ) {
    rc_t rc;
    if ( NULL == self || 0 == alloc_blocksize ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_file_list_init() -> %R", rc );
    } else {
        rc = KLockMake ( &( self -> lock ) );
        if ( 0 != rc ) {
            ErrMsg( "locked_file_list_init().KLockMake() -> %R", rc );
        } else {
            rc = VNamelistMake ( & self -> files, alloc_blocksize );
            if ( 0 != rc ) {
                ErrMsg( "locked_file_list_init().VNamelistMake() -> %R", rc );
            }
        }
    }
    return rc;
}

rc_t locked_file_list_release( locked_file_list_t * self, KDirectory * dir ) {
    rc_t rc = 0;
    /* tolerates to be called with self == NULL */
    if ( NULL != self ) {
        rc = KLockRelease ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_file_list_release().KLockRelease() -> %R", rc );
        } else {
            /* tolerates to be called with dir == NULL */
            if ( NULL != dir ) {
                rc = delete_files( dir, self -> files );
            }
        }
        {
            rc_t rc2 = VNamelistRelease ( self -> files );
            if ( 0 != rc2 ) {
                ErrMsg( "locked_file_list_release().VNamelistRelease() -> %R", rc );
            }
        }
    }
    return rc;
}

static rc_t locked_file_list_unlock( const locked_file_list_t * self, const char * function, rc_t rc ) {
    rc_t rc2 = KLockUnlock ( self -> lock );
    if ( 0 != rc2 ) {
        ErrMsg( "%s().KLockUnlock() -> %R", function, rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t locked_file_list_append( const locked_file_list_t * self, const char * filename ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == filename ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_file_list_append() -> %R", rc );
    } else {
        rc = KLockAcquire ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_file_list_append( '%s' ).KLockAcquire() -> %R", filename, rc );
        } else {
            rc = VNamelistAppend ( self -> files, filename );
            if ( 0 != rc ) {
                ErrMsg( "locked_file_list_append( '%s' ).VNamelistAppend() -> %R", filename, rc );
            }
            rc = locked_file_list_unlock( self, "locked_file_list_append", rc );
        }
    }
    return rc;
}

rc_t locked_file_list_delete_files( KDirectory * dir, locked_file_list_t * self ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_file_list_delete_files() -> %R", rc );
    } else {
        rc = KLockAcquire ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_file_list_delete_files().KLockAcquire() -> %R", rc );
        } else {
            rc = delete_files( dir, self -> files );
            if ( 0 != rc ) {
                ErrMsg( "locked_file_list_delete_files().delete_files() -> %R", rc );
            }
            rc = locked_file_list_unlock( self, "locked_file_list_delete_files", rc );
        }
    }
    return rc;
}

rc_t locked_file_list_delete_dirs( KDirectory * dir, locked_file_list_t * self ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == dir ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_file_list_delete_dirs() -> %R", rc );
    } else {
        rc = KLockAcquire ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_file_list_delete_dirs().KLockAcquire() -> %R", rc );
        } else {
            rc = delete_dirs( dir, self -> files );
            if ( 0 != rc ) {
                ErrMsg( "locked_file_list_delete_dirs().delete_dirs() -> %R", rc );
            }
            rc = locked_file_list_unlock( self, "locked_file_list_delete_dirs", rc );
        }
    }
    return rc;
}

rc_t locked_file_list_count( const locked_file_list_t * self, uint32_t * count ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == count ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_file_list_delete_dirs() -> %R", rc );
    } else {
        rc = KLockAcquire ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_file_list_count().KLockAcquire() -> %R", rc );
        } else {
            rc = VNameListCount( self -> files, count );
            if ( 0 != rc ) {
                ErrMsg( "locked_file_list_count().VNameListCount() -> %R", rc );
            }
            rc = locked_file_list_unlock( self, "locked_file_list_count", rc );
        }
    }
    return rc;
}

rc_t locked_file_list_pop( locked_file_list_t * self, const String ** item ) {
    rc_t rc = 0;
    if ( NULL == self || NULL == item ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "locked_file_list_pop() -> %R", rc );
    } else {
        *item = NULL;
        rc = KLockAcquire ( self -> lock );
        if ( 0 != rc ) {
            ErrMsg( "locked_file_list_pop().KLockAcquire() -> %R", rc );
        } else {
            const char * s;
            rc = VNameListGet ( self -> files, 0, &s );
            if ( 0 != rc ) {
                ErrMsg( "locked_file_list_pop().VNameListGet() -> %R", rc );
            } else {
                String S;
                StringInitCString( &S, s );
                rc = StringCopy ( item, &S );
                if ( 0 != rc ) {
                    ErrMsg( "locked_file_list_pop().StringCopy() -> %R", rc );
                } else {
                    rc = VNamelistRemoveIdx( self -> files, 0 );
                    if ( 0 != rc ) {
                        ErrMsg( "locked_file_list_pop().VNamelistRemoveIdx() -> %R", rc );
                    }
                }
            }
            rc = locked_file_list_unlock( self, "locked_file_list_pop", rc );
        }
    }
    return rc;
}
