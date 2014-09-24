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

#include "out_redir.h"

#include <kfs/directory.h>
#include <kfs/buffile.h>
#include <kfs/bzip.h>
#include <kfs/gzip.h>
#include <sysalloc.h>

static rc_t CC out_redir_callback( void * self, const char * buffer, size_t bufsize, size_t * num_writ )
{
    out_redir * redir = ( out_redir * )self;
    rc_t rc = KFileWriteAll( redir->kfile, redir->pos, buffer, bufsize, num_writ );
    if ( rc == 0 )
        redir->pos += *num_writ;
    return rc;
}


rc_t init_out_redir( out_redir * self, enum out_redir_mode mode, const char * filename, size_t bufsize )
{
    rc_t rc;
    KFile *output_file;

    if ( filename != NULL )
    {
        KDirectory *dir;
        rc = KDirectoryNativeDir( &dir );
        if ( rc != 0 )
            LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
        else
        {
            rc = KDirectoryCreateFile ( dir, &output_file, false, 0664, kcmInit, "%s", filename );
            KDirectoryRelease( dir );
        }
    }
    else
        rc = KFileMakeStdOut ( &output_file );

    if ( rc == 0 )
    {
        KFile *temp_file;

        /* wrap the output-file in compression, if requested */
        switch ( mode )
        {
            case orm_gzip  : rc = KFileMakeGzipForWrite( &temp_file, output_file ); break;
            case orm_bzip2 : rc = KFileMakeBzip2ForWrite( &temp_file, output_file ); break;
            case orm_uncompressed : break;
        }
        if ( rc == 0 )
        {
            if ( mode != orm_uncompressed )
            {
                KFileRelease( output_file );
                output_file = temp_file;
            }

            /* wrap the output/compressed-file in buffering, if requested */
            if ( bufsize != 0 )
            {
                rc = KBufFileMakeWrite( &temp_file, output_file, false, bufsize );
                if ( rc == 0 )
                {
                    KFileRelease( output_file );
                    output_file = temp_file;
                }
            }

            if ( rc == 0 )
            {
                self->kfile = output_file;
                self->org_writer = KOutWriterGet();
                self->org_data = KOutDataGet();
                self->pos = 0;
                rc = KOutHandlerSet( out_redir_callback, self );
                if ( rc != 0 )
                    LOGERR( klogInt, rc, "KOutHandlerSet() failed" );
            }
        }
    }
    return rc;
}


void release_out_redir( out_redir * self )
{
    KFileRelease( self->kfile );
    if( self->org_writer != NULL )
    {
        KOutHandlerSet( self->org_writer, self->org_data );
    }
    self->org_writer = NULL;
}

