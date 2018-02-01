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

#include "logger.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_kproc_lock_
#include <kproc/lock.h>
#endif

#ifndef _h_kfs_file_
#include <kfs/file.h>
#endif

#ifndef _h_kfs_defs_
#include <kfs/defs.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

typedef struct logger
{
    KFile * f;
    KLock * lock;
    uint64_t pos;
    char buffer[ 4096 ];
} logger;

void destroy_logger( logger * self )
{
    if ( self != NULL )
    {
        if ( self -> lock != NULL )
            KLockRelease( self -> lock );
        if ( self -> f != NULL )
            KFileRelease( self -> f );
        free( ( void * ) self );
    }
}

rc_t make_logger( KDirectory * dir, logger ** log, const char * filename )
{
    rc_t rc = 0;
    logger * l = calloc( 1, sizeof * l );
    if ( l == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_logger().calloc( %d ) -> %R", ( sizeof * l ), rc );
    }
    else
    {
        rc = KLockMake ( & l -> lock );
        if ( rc != 0 )
            ErrMsg( "make_logger().KLockMake() -> %R", rc );
        else if ( filename != NULL )
        {
            rc = KDirectoryCreateFile ( dir, & l -> f, true, 0644, kcmCreate, "%s", filename );
            if ( rc != 0 )
                ErrMsg( "make_logger().KDirectoryCreateFile( '%s' ) -> %R", filename, rc );
        }
        
        if ( rc == 0 )
            *log = l;
        else
            destroy_logger( l );
    }
    return rc;
}

rc_t log_write( struct logger * self, const char * fmt, ... )
{
    rc_t rc = 0;
    if ( self == NULL || fmt == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "log_write() -> %R", rc );
    }
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc != 0 )
            ErrMsg( "log_write().KLockAcquire -> %R", rc );
        else
        {
            size_t num_writ;
            va_list list;
            va_start( list, fmt );
            rc = string_vprintf( self -> buffer, sizeof self -> buffer, &num_writ, fmt, list );
            va_end( list );

            if ( rc != 0 )
                ErrMsg( "write_2_logger().string_vprintf -> %R", rc );
            else
            {
                self -> buffer[ num_writ++ ] = '\n';
                if ( self -> f == NULL )
                {
                    self -> buffer[ num_writ ] = 0;
                    rc = KOutMsg( "%s", self -> buffer );
                }
                else
                {
                    size_t num_writ_file;
                    rc = KFileWriteAll( self -> f, self -> pos, self -> buffer, num_writ, &num_writ_file );
                    if ( rc != 0 )
                        ErrMsg( "write_2_logger().KFileWriteAll() -> %R", rc );
                    else
                        self -> pos += num_writ_file;
                }
            }
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}
