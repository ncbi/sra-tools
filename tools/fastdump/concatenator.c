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

static rc_t make_buffered_for_read( KDirectory * dir, const struct KFile ** f,
                                    const char * filename, size_t buf_size )
{
    const struct KFile * fr;
    rc_t rc = KDirectoryOpenFileRead( dir, &fr, "%s", filename );
    if ( rc == 0 )
    {
        if ( buf_size > 0 )
        {
            const struct KFile * fb;
            rc = KBufFileMakeRead( &fb, fr, buf_size );
            if ( rc == 0 )
            {
                KFileRelease( fr );
                fr = fb;
            }
        }
        if ( rc == 0 )
            *f = fr;
        else
            KFileRelease( fr );
    }
    return rc;
}

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


typedef struct cf_progress
{
    struct progressbar * progressbar;
    uint64_t total_size;
    uint64_t current_size;
    uint32_t current_percent;
} cf_progress;

static rc_t copy_file( KFile * dst, const KFile * src, uint64_t * dst_pos,
                       size_t buf_size, cf_progress * cfp )
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
        size_t num_trans = 1;
        while ( rc == 0 && num_trans > 0 )
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
                    rc = KFileWrite( dst, *dst_pos, buffer, num_read, &num_trans );
                    if ( rc != 0 )
                        ErrMsg( "copy_file.KFileWrite( at %lu ) -> %R", *dst_pos, rc );
                    else
                    {
                        *dst_pos += num_trans;
                        src_pos += num_trans;
                        if ( cfp != NULL && cfp->progressbar != NULL )
                        {
                            uint32_t percent;
                            
                            cfp->current_size += num_trans;
                            percent = calc_percent( cfp->total_size, cfp->current_size, 2 );
                            if ( percent > cfp->current_percent )
                            {
                                uint32_t i;
                                for ( i = cfp->current_percent + 1; i <= percent; ++i )
                                    update_progressbar( cfp->progressbar, i );
                                cfp->current_percent = percent;
                            }
                        }
                    }
                }
                else
                    num_trans = 0;
            }
        }
        free( buffer );
    }
    return rc;
}

static rc_t total_filesize( const KDirectory * dir, const VNamelist * files, uint64_t *total )
{
    uint32_t count;
    rc_t rc = VNameListCount( files, &count );
    *total = 0;
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
                uint64_t size;
                rc_t rc1 = KDirectoryFileSize( dir, &size, "%s", filename );
                if ( rc1 == 0 )
                    *total += size;
            }
        }
    }
    return rc;
}


static rc_t simple_concat_loop( KDirectory * dir,
                        const struct VNamelist * files,
                        size_t buf_size,
                        bool show_progress,
                        struct KFile * dst )
{
    rc_t rc;
    cf_progress cfp;

    if ( show_progress )
    {
        cfp . current_size = 0;
        cfp . current_percent = 0;
        rc = make_progressbar( &( cfp . progressbar ), 2 );
        if ( rc == 0 )
            rc = total_filesize( dir, files, &( cfp . total_size ) );
    }
    else
        cfp . progressbar = NULL;

    if ( rc == 0 )
    {
        uint32_t count;
        rc = VNameListCount( files, &count );
        if ( rc != 0 )
            ErrMsg( "VNameListCount() -> %R", rc );
        else
        {
            uint32_t idx;
            uint64_t dst_pos = 0;
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
                        rc = copy_file( dst, src, &dst_pos, buf_size, &cfp );
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
        
        if ( cfp . progressbar != NULL )
            destroy_progressbar( cfp . progressbar );
    }
    return rc;
}

rc_t execute_concat( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    bool show_progress,
                    bool print_to_stdout,
                    bool force,
                    compress_t compress )
{
    rc_t rc = 0;
    
    if ( show_progress )
        rc = KOutMsg( "concat :" );

    if ( rc == 0 )
    {
        if ( print_to_stdout )
            rc = print_files( dir, files, buf_size ); /* helper.c */
        else
        {
            struct KFile * dst;
            rc_t rc =  make_compressed( dir,
                                    output_filename,
                                    buf_size,
                                    compress,
                                    force,
                                    & dst );
            if ( rc == 0 )
            {
                rc = simple_concat_loop( dir,
                                  files,
                                  buf_size,
                                  show_progress,
                                  dst );
                KFileRelease( dst );
            }
        }
    }
    return rc;
}
