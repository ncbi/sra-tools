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

#include "vdb-dump-redir.h"

#include <kfs/directory.h>
#include <kfs/buffile.h>
#include <kfs/bzip.h>
#include <kfs/gzip.h>
#include <kfs/appendfile.h>
#include <sysalloc.h>

#include "vdb-dump-helper.h"

static rc_t CC out_redir_callback( void * self, const char * buffer, size_t bufsize, size_t * num_writ )
{
    out_redir * redir = ( out_redir * )self;
    rc_t rc = KFileWriteAll( redir -> kfile, redir -> pos, buffer, bufsize, num_writ );
    if ( 0 == rc )
    {
        redir -> pos += *num_writ;
    }
    return rc;
}

rc_t init_out_redir( out_redir * self, out_redir_mode_t mode,
                     const char * filename, size_t bufsize,
                     bool append )
{
    rc_t rc;
    KFile *output_file;

    if ( NULL != filename )
    {
        KDirectory *dir;
        rc = KDirectoryNativeDir( &dir );
        DISP_RC( rc, "KDirectoryNativeDir() failed" );
        if ( 0 == rc )
        {
            rc = KDirectoryCreateFile( dir, &output_file, false, 0664, kcmOpen, "%s", filename );
            DISP_RC( rc, "KDirectoryCreateFile() failed" );
            {
                rc_t rc2 = KDirectoryRelease( dir );
                DISP_RC( rc2, "KDirectoryRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        
        if ( 0 == rc && append )
        {
            KFile *temp_file;
            if ( mode != orm_uncompressed )
            {
                mode = orm_uncompressed;
            }
            rc = KFileMakeAppend ( &temp_file, output_file );
            DISP_RC( rc, "KFileMakeAppend() failed" );
            if ( 0 == rc )
            {
                rc_t rc2 = KFileRelease( output_file );
                DISP_RC( rc2, "KFileRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
                output_file = temp_file;
            }
        }
    }
    else
    {
        rc = KFileMakeStdOut ( &output_file );
        DISP_RC( rc, "KFileMakeStdOut() failed" );
    }

    if ( 0 == rc )
    {
        KFile *temp_file;

        /* wrap the output-file in compression, if requested */
        switch ( mode )
        {
            case orm_gzip  : rc = KFileMakeGzipForWrite( &temp_file, output_file );
                             DISP_RC( rc, "KFileMakeGzipForWrite() failed" );
                             break;
            case orm_bzip2 : rc = KFileMakeBzip2ForWrite( &temp_file, output_file );
                             DISP_RC( rc, "KFileMakeBzip2ForWrite() failed" );
                             break;
            case orm_uncompressed : break;
        }
        if ( 0 == rc )
        {
            if ( mode != orm_uncompressed )
            {
                rc_t rc2 = KFileRelease( output_file );
                DISP_RC( rc2, "KFileRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
                output_file = temp_file;
            }

            /* wrap the output/compressed-file in buffering, if requested */
            if ( 0 == rc && 0 != bufsize )
            {
                rc = KBufFileMakeWrite( &temp_file, output_file, false, bufsize );
                DISP_RC( rc, "KBufFileMakeWrite() failed" );
                if ( 0 == rc )
                {
                    rc_t rc2 = KFileRelease( output_file );
                    DISP_RC( rc2, "KFileRelease() failed" );
                    rc = ( 0 == rc ) ? rc2 : rc;
                    output_file = temp_file;
                }
            }

            if ( 0 == rc )
            {
                self -> kfile = output_file;
                self -> org_writer = KOutWriterGet();
                self -> org_data = KOutDataGet();
                self -> pos = 0;
                rc = KOutHandlerSet( out_redir_callback, self );
                DISP_RC( rc, "KOutHandlerSet() failed" );
            }
        }
    }
    return rc;
}


rc_t release_out_redir( out_redir * self )
{
    rc_t rc = KFileRelease( self -> kfile );
    DISP_RC( rc, "KFileRelease() failed" );
    if ( NULL != self->org_writer )
    {
        rc_t rc2 = KOutHandlerSet( self -> org_writer, self -> org_data );
        DISP_RC( rc2, "KOutHandlerSet() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    self -> org_writer = NULL;
    return rc;
}

