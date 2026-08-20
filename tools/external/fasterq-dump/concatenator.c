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

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_copy_machine_
#include "copy_machine.h"
#endif

#ifndef _h_kfs_buffile_
#include <kfs/buffile.h>
#endif

#ifndef _h_kfs_gzip_
#include <kfs/gzip.h>
#endif

#ifndef _h_kfs_bzip_
#include <kfs/bzip.h>
#endif

/*
#ifndef _h_kfs_szip_
#include <kfs/szip.h>
#endif
*/

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

static rc_t create_compressed_file( struct KFile ** dst, compress_t compress_mode ) {
    rc_t rc = 0;
    struct KFile * tmp = NULL;
    switch( compress_mode ) {
        case compress_t_gzip : rc = KFileMakeGzipForWrite( &tmp, *dst );
                                if ( 0 != rc ) {
                                    ErrMsg( "create_compressed_file() KFileMakeGzipForWrite() -> %R", rc );
                                }
                                break;
        case compress_t_bzip : rc = KFileMakeBzip2ForWrite( &tmp, *dst );
                                if ( 0 != rc ) {
                                    ErrMsg( "create_compressed_file() KFileMakeBzip2ForWrite() -> %R", rc );
                                }
                                break;
        /*
        case compress_t_szip : rc = KFileMakeSzipForWrite( &tmp, *dst );
                                if ( 0 != rc ) {
                                    ErrMsg( "create_compressed_file() KFileMakeSzipForWrite() -> %R", rc );
                                }
                                break;
        */
        case compress_t_none : break;   // do nothing to dst
    }
    if ( 0 == rc && tmp != NULL ) {
        rc = ft_release_file( *dst, "create_compressed_file()" );
        if ( 0 == rc ) {
            *dst = tmp;
        }
    }
    return rc;
}

static rc_t create_buffered_file( struct KFile ** dst, size_t buf_size ) {
    rc_t rc = 0;
    if ( buf_size > 0 ) {
        struct KFile * tmp;
        rc = KBufFileMakeWrite( &tmp, *dst, false, buf_size );
        if ( 0 != rc ) {
            ErrMsg( "create_buffered_file() KBufFileMakeWrite() -> %R", rc );
        } else {
            rc = ft_release_file( *dst, "create_buffered_file()" );
            if ( 0 == rc ) {
                *dst = tmp;
            }
        }
    }
    return rc;
}

static rc_t create_output_name( char * buffer, size_t bufsize,
                                const char * name, compress_t compress_mode ) {
    size_t num_writ;
    rc_t rc = 0;
    switch( compress_mode ) {
        case compress_t_gzip : rc = string_printf( buffer, bufsize, &num_writ, "%s.gz", name );
                               break;
        case compress_t_bzip : rc = string_printf( buffer, bufsize, &num_writ, "%s.bz2", name );
                               break;
/*
        case compress_t_szip : rc = string_printf( buffer, bufsize, &num_writ, "%s.zip", name );
                               break;
*/
        case compress_t_none : rc = string_printf( buffer, bufsize, &num_writ, "%s", name );
                               break;
    }
    if ( 0 != rc ) {
        ErrMsg( "create_output_name() string_printf() -> %R", rc );
    }
    return rc;
}

static rc_t concat_compressed( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files_to_concat,
                    size_t buf_size,
                    struct bg_progress_t * progress,
                    uint32_t count,
                    uint32_t q_wait_time,
                    compress_t compress_mode ) {
    char buffer[ 4096 ];
    rc_t rc = create_output_name( buffer, sizeof buffer, output_filename, compress_mode );
    if ( 0 == rc ) {
        struct KFile * dst;
        rc = KDirectoryCreateFile( dir, &dst, false, 0664, kcmInit, "%s", buffer );
        if ( 0 != rc ) {
            ErrMsg( "concat_compressed() cannot create file '%s' -> %R", buffer, rc );
        } else {
            rc = create_compressed_file( &dst, compress_mode );
        }
        if ( 0 == rc ) {
            rc = create_buffered_file( &dst, buf_size );
        }
        if ( 0 == rc ) {
            rc = cm_make_a_copy( dir, dst, files_to_concat, progress,
                                0, buf_size, 0, q_wait_time ); /* copy_machine.c */
        }
        if ( 0 == rc ) {
            rc = ft_release_file( dst, "concat_compressed()" );
        }
    }
    return rc;
}

static rc_t concat_execute_un_compressed_append( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files_to_concat,
                    size_t buf_size,
                    struct bg_progress_t * progress,
                    uint32_t count,
                    uint32_t q_wait_time ) {
    uint64_t size_of_existing_file;
    rc_t rc = KDirectoryFileSize ( dir, &size_of_existing_file, "%s", output_filename );
    if ( 0 != rc ) {
        ErrMsg( "execute_concat_un_compressed_append() KDirectoryFileSize( '%s' ) -> %R",
                output_filename, rc );
    } else {
        struct KFile * dst;
        rc = KDirectoryOpenFileWrite ( dir, &dst, true, "%s", output_filename );
        if ( 0 != rc ) {
            ErrMsg( "execute_concat_un_compressed_append() KDirectoryOpenFileWrite( '%s' ) -> %R",
                    output_filename, rc );
        } else {
            rc = cm_make_a_copy( dir, dst, files_to_concat, progress,
                                 size_of_existing_file, buf_size, 0, q_wait_time ); /* copy_machine.c */
            {
                rc_t rc2 = ft_release_file( dst, "execute_concat_un_compressed_append()" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static rc_t concat_execute_un_compressed_no_append( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files_to_concat,
                    size_t buf_size,
                    struct bg_progress_t * progress,
                    bool force,
                    uint32_t count,
                    uint32_t q_wait_time ) {
    const char * file1;
    rc_t rc = VNameListGet( files_to_concat, 0, &file1 );
    if ( 0 != rc ) {
        ErrMsg( "execute_concat_un_compressed() VNameListGet( 0 ) -> %R", rc );
    } else {
        uint64_t size_file1;

        /* we need the size of the first file, as an offset later - if KDirectoryRename() was successful */
        rc = KDirectoryFileSize ( dir, &size_file1, "%s", file1 );
        if ( 0 != rc ) {
            ErrMsg( "execute_concat_un_compressed() KDirectoryFileSize( '%s' ) -> %R", file1, rc );
        } else {
            if ( !force && ft_file_exists( dir, "%s", output_filename ) ) {
                rc = RC( rcExe, rcFile, rcPacking, rcName, rcExists );
                ErrMsg( "execute_concat_un_compressed() creating ouput-file '%s' -> %R",
                        output_filename, rc );
            } else {
                /* first try to create the output-file, so that sub-directories that do not exist
                   are created ... */
                struct KFile * dst = NULL;
                uint32_t files_offset = 1;

                /* try to move the first file into the place of the output-file */
                rc = KDirectoryRename( dir, true, file1, output_filename );
                if ( 0 != rc ) {
                    /* this can fail, if file1 and output_filename are on different filesystems ... */
                    files_offset = 0; /* this will make sure that we copy all files ( including the 1st one ) */
                    size_file1 = 0;
                    rc = KDirectoryCreateFile( dir, &dst, false, 0664, kcmInit, "%s", output_filename );
                } else {
                    rc = KDirectoryOpenFileWrite ( dir, &dst, true, "%s", output_filename );
                }

                if ( 0 != rc ) {
                    StdErrMsg( "\n\tError: fasterq-dump cannot create this file: '%s'\n", output_filename );
                }
                
                if ( 0 == rc ) {
                    rc = create_buffered_file( &dst, buf_size );
                }

                if ( 0 == rc ) {
                    bg_progress_update( progress, size_file1 ); /* progress_thread.c */

                    rc = cm_make_a_copy( dir, dst, files_to_concat, progress, size_file1, buf_size,
                                      files_offset, q_wait_time ); /* copy_machine.c */
                }
                if ( 0 == rc && dst != NULL ) {
                    rc = ft_release_file( dst,
                    "execute_concat_un_compressed( '%s' )", output_filename );
                }
            }
        }
    }
    return rc;
}

/* ---------------------------------------------------------------------------------- */

/* used by temp_registry.c */
rc_t concat_execute( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files_to_concat,
                    size_t buf_size,
                    struct bg_progress_t * progress,
                    bool force,
                    bool append,
                    compress_t compress_mode ) {
    uint32_t count;
    rc_t rc = VNameListCount( files_to_concat, &count );
    if ( 0 != rc ) {
        ErrMsg( "concatenator.c execute_concat().VNameListCount() -> %R", rc );
    } else if ( count > 0 ) {
        uint32_t q_wait_time = 500;
        if ( compress_mode == compress_t_none ) {
            bool file_exists = ft_file_exists( dir, "%s", output_filename );
            bool perform_append = ( append && file_exists );
            if ( perform_append ) {
                rc = concat_execute_un_compressed_append( dir, output_filename, files_to_concat,
                                    buf_size, progress, count, q_wait_time );
            } else {
                rc = concat_execute_un_compressed_no_append( dir, output_filename, files_to_concat,
                                    buf_size, progress, force, count, q_wait_time );
            }
        } else {
            rc = concat_compressed( dir, output_filename, files_to_concat,
                                    buf_size, progress, count, q_wait_time, compress_mode );
        }
    } else {
        rc = RC( rcExe, rcFile, rcPacking, rcName, rcEmpty );
        ErrMsg( "concat_execute ... no files to process into %s", output_filename );
    }
    return rc;
}
