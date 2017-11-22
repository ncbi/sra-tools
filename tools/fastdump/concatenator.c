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

#include "concatenator.h"
#include "helper.h"
#include "copy_machine.h"

#include <klib/out.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/progressbar.h>

#include <kproc/thread.h>

#include <kfs/defs.h>
#include <kfs/file.h>
#include <kfs/buffile.h>
#include <kfs/gzip.h>
#include <kfs/bzip.h>

static rc_t print_file( const KFile * src, size_t buf_size )
{
    rc_t rc = 0;
    char * buffer = malloc( buf_size );
    if ( buffer == NULL )
    {
        rc = RC( rcExe, rcFile, rcPacking, rcMemory, rcExhausted );
        ErrMsg( "copy_file.malloc( %d ) -> %R", buf_size, rc );
    }
    else
    {
        uint64_t src_pos = 0;
        size_t num_read = 1;
        while ( rc == 0 && num_read > 0 )
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                size_t num_read;
                rc = KFileRead( src, src_pos, buffer, buf_size, &num_read );
                if ( rc != 0 )
                    ErrMsg( "copy_file.KFileRead( at %lu ) -> %R", src_pos, rc );
                else if ( num_read > 0 )
                {
                    rc = KOutMsg( "%.*s", num_read, buffer );
                    src_pos += num_read;
                }
            }
        }
        free( buffer );
    }
    return rc;
}


static rc_t print_files( KDirectory * dir,
                         const struct VNamelist * files,
                         size_t buf_size )
{
    uint32_t count;
    rc_t rc = VNameListCount( files, &count );
    if ( rc != 0 )
        ErrMsg( "VNameListCount() -> %R", rc );
    else
    {
        uint32_t idx;
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            const char * filename;
            rc = VNameListGet( files, idx, &filename );
            if ( rc != 0 )
                ErrMsg( "VNameListGet( #%d) -> %R", idx, rc );
            else
            {
                const struct KFile * src;
                rc = make_buffered_for_read( dir, &src, filename, buf_size );
                if ( rc == 0 )
                {
                    rc = print_file( src, buf_size );
                    KFileRelease( src );
                }

                if ( rc == 0 )
                {
                    rc = KDirectoryRemove( dir, true, "%s", filename );
                    if ( rc != 0 )
                        ErrMsg( "KDirectoryRemove( '%s' ) -> %R", filename, rc );
                }
            }
        }
    }
    return rc;
}


static const char * ct_none_fmt  = "%s";
static const char * ct_gzip_fmt  = "%s.gz";
static const char * ct_bzip2_fmt = "%s.bz2";

static rc_t make_compressed( KDirectory * dir,
                             const char * output_filename,
                             size_t buf_size,
                             compress_t compress,
                             bool force,
                             struct KFile ** dst )
{
    rc_t rc = 0;
    if ( dst != NULL )
        *dst = NULL;
    if ( dir == NULL || dst == NULL || output_filename == NULL)
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        struct KFile * f;
        const char * fmt;
        KCreateMode create_mode = force ? kcmInit : kcmCreate;
        
        switch( compress )
        {
            case ct_none  : fmt = ct_none_fmt; break;
            case ct_gzip  : fmt = ct_gzip_fmt; break;
            case ct_bzip2 : fmt = ct_bzip2_fmt; break;
        }
        rc = KDirectoryCreateFile( dir, &f, false, 0664, create_mode | kcmParents, fmt, output_filename );
        if ( rc != 0 )
            ErrMsg( "concatenator.make_compressed() KDirectoryCreateFile( '%s' ) -> %R", output_filename, rc );
        else
        {
            if ( buf_size > 0 )
            {
                struct KFile * tmp;
                rc = KBufFileMakeWrite( &tmp, f, false, buf_size );
                if ( rc != 0 )
                    ErrMsg( "KBufFileMakeWrite( '%s' ) -> %R", output_filename, rc );
                else
                {
                    KFileRelease( f );
                    f = tmp;
                }
            }
            if ( rc == 0 && compress != ct_none )
            {
                struct KFile * tmp;
                if ( compress == ct_gzip )
                {
                    rc = KFileMakeGzipForWrite ( &tmp, f );
                    if ( rc != 0 )
                        ErrMsg( "KFileMakeGzipForWrite( '%s' ) -> %R", output_filename, rc );
                }
                else if ( compress == ct_bzip2 )
                {
                    rc = KFileMakeBzip2ForWrite ( &tmp, f );
                    if ( rc != 0 )
                        ErrMsg( "KFileMakeBzip2ForWrite( '%s' ) -> %R", output_filename, rc );
                }
                else
                    rc = RC( rcExe, rcFile, rcPacking, rcMode, rcInvalid );
                
                if ( rc == 0 )
                {
                    KFileRelease( f );
                    f = tmp;
                }
            }
        
            if ( rc == 0 )
                *dst = f;
            else
                KFileRelease( f );
        }
    }
    return rc;
}

/* ---------------------------------------------------------------------------------- */

rc_t execute_concat_compressed( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    bool force,
                    compress_t compress,
                    uint32_t count )
{
    struct KFile * dst;
    rc_t rc =  make_compressed( dir, output_filename, buf_size,
                           compress, force, &dst );
    if ( rc == 0 )
    {
        rc = make_a_copy( dir, dst, files, progress, 0, buf_size, 0, 500 ); /* copy_machine.c */
        KFileRelease( dst );
    }
    return rc;
}

static rc_t create_this_file( KDirectory * dir, const char * filename, bool force )
{
    struct KFile * f;
    KCreateMode create_mode = force ? kcmInit : kcmCreate;
    rc_t rc = KDirectoryCreateFile( dir, &f, false, 0664, create_mode | kcmParents, "%s", filename );
    if ( rc != 0 )
        ErrMsg( "KDirectoryCreateFile( '%s' ) -> %R", filename, rc );
    else
        KFileRelease( f );
    return rc;
}

rc_t execute_concat_un_compressed( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    bool force,
                    uint32_t count )
{
    const char * file1;
    rc_t rc = VNameListGet( files, 0, &file1 );
    if ( rc != 0 )
        ErrMsg( "VNameListGet( 0 ) -> %R", rc );
    else
    {
        uint64_t size_file1;
        uint32_t files_offset = 1;
        
        rc = KDirectoryFileSize ( dir, &size_file1, "%s", file1 );
        if ( rc != 0 )
            ErrMsg( "KDirectoryFileSize( '%s' ) -> %R", file1, rc );

        if ( rc == 0 && !force )
        {
            if ( file_exists( dir, "%s", output_filename ) )
            {
                rc = RC( rcExe, rcFile, rcPacking, rcName, rcExists );
                ErrMsg( "creating ouput-file '%s' -> %R", output_filename, rc );
            }
        }
            
        /* first try to create the output-file, so that sub-directories that do not exist
           are created ... */
        if ( rc == 0 )
            rc = create_this_file( dir, output_filename, force );
            
        if ( rc == 0 )
        {
            rc = KDirectoryRename ( dir, true, file1, output_filename );
            if ( rc != 0 )
            {
                /* this can fail, if file1 and output_filename are on different filesystems ... */
                files_offset = 0;
                size_file1 = 0;
                rc = create_this_file( dir, output_filename, force );
            }
        }
            
        if ( rc == 0 )
        {
            struct KFile * dst;
            rc = KDirectoryOpenFileWrite ( dir, &dst, true, "%s", output_filename );
            if ( rc != 0 )
                ErrMsg( "KDirectoryOpenFileWrite( '%s' ) -> %R", output_filename, rc );
            else
            {
                if ( buf_size > 0 )
                {
                    struct KFile * tmp;
                    rc = KBufFileMakeWrite( &tmp, dst, false, buf_size );
                    if ( rc != 0 )
                        ErrMsg( "KBufFileMakeWrite( '%s' ) -> %R", output_filename, rc );
                    else
                    {
                        KFileRelease( dst );
                        dst = tmp;
                    }
                }

                bg_progress_update( progress, size_file1 );

                rc = make_a_copy( dir, dst, files, progress, size_file1, buf_size, files_offset, 500 ); /* copy_machine.c */

                KFileRelease( dst );
            }
        }
    }
    return rc;
}
                    
rc_t execute_concat( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    bool print_to_stdout,
                    bool force,
                    compress_t compress )
{
    rc_t rc;
    if ( print_to_stdout )
    {
        rc = print_files( dir, files, buf_size ); /* helper.c */
    }
    else
    {
        uint32_t count;
        rc = VNameListCount( files, &count );
        if ( rc != 0 )
            ErrMsg( "VNameListCount() -> %R", rc );
        else if ( count > 0 )
        {
            if ( compress != ct_none )
            {
                rc = execute_concat_compressed( dir, output_filename, files, buf_size,
                            progress, force, compress, count );
            }
            else
            {
                rc = execute_concat_un_compressed( dir, output_filename, files, buf_size,
                            progress, force, count );
            }
        }
    }
    return rc;
}
