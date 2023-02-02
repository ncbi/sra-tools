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

#include "file_tools.h"

#ifndef _h_err_msg_
#include "err_msg.h"     /* ErrMsg */
#endif

#ifndef _h_helper_
#include "helper.h"      /* split_string_r */
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_kfs_buffile_
#include <kfs/buffile.h>
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

rc_t create_this_dir( KDirectory * dir, const String * dir_name, bool force ) {
    KCreateMode create_mode = force ? kcmInit : kcmCreate;
    rc_t rc = KDirectoryCreateDir( dir, 0774, create_mode | kcmParents, "%.*s", dir_name -> len, dir_name -> addr );
    if ( 0 != rc ) {
        ErrMsg( "create_this_dir().KDirectoryCreateDir( '%.*s' ) -> %R", dir_name -> len, dir_name -> addr, rc );
    }
    return rc;
}

rc_t create_this_dir_2( KDirectory * dir, const char * dir_name, bool force ) {
    KCreateMode create_mode = force ? kcmInit : kcmCreate;
    rc_t rc = KDirectoryCreateDir( dir, 0774, create_mode | kcmParents, "%s", dir_name );
    if ( 0 != rc ) {
        ErrMsg( "create_this_dir_2().KDirectoryCreateDir( '%s' ) -> %R", dir_name, rc );
    }
    return rc;
}

static bool check_expected_path_type( const KDirectory * dir, uint32_t expected, const char * fmt, va_list args ) {
    bool res = false;
    char buffer[ 4096 ];
    size_t num_writ;

    rc_t rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, args );
    if ( 0 == rc ) {
        uint32_t pt = KDirectoryPathType( dir, "%s", buffer );
        res = ( ( pt & expected ) == expected );
    }
    return res;
}

bool file_exists( const KDirectory * dir, const char * fmt, ... ) {
    bool res = false;
    va_list args;
    va_start( args, fmt );
    res = check_expected_path_type( dir, kptFile, fmt, args ); /* because KDirectoryPathType() uses vsnprintf */
    va_end( args );
    return res;
}

bool dir_exists( const KDirectory * dir, const char * fmt, ... ) {
    bool res = false;
    va_list args;
    va_start( args, fmt );
    res = check_expected_path_type( dir, kptDir, fmt, args ); /* because KDirectoryPathType() uses vsnprintf */
    va_end( args );
    return res;
}

rc_t delete_files( KDirectory * dir, const VNamelist * files, bool details ) {
    uint32_t count;
    rc_t rc = VNameListCount( files, &count );
    if ( 0 != rc ) {
        ErrMsg( "delete_files().VNameListCount() -> %R", rc );
    } else {
        uint32_t idx;
        for ( idx = 0; 0 == rc && idx < count; ++idx ) {
            const char * filename;
            rc = VNameListGet( files, idx, &filename );
            if ( 0 != rc ) {
                ErrMsg( "delete_files.VNameListGet( #%d ) -> %R", idx, rc );
            } else {
                if ( file_exists( dir, "%s", filename ) ) {
                    rc = KDirectoryRemove( dir, true, "%s", filename );
                    if ( 0 != rc ) {
                        ErrMsg( "delete_files.KDirectoryRemove( '%s' ) -> %R", filename, rc );
                    } else if ( details ) {
                        InfoMsg( "file deleted: '%s'", filename );
                    }
                }
            }
        }
    }
    return rc;
}

rc_t delete_dirs( KDirectory * dir, const VNamelist * dirs, bool details ) {
    uint32_t count;
    rc_t rc = VNameListCount( dirs, &count );
    if ( 0 != rc ) {
        ErrMsg( "delete_dirs().VNameListCount() -> %R", rc );
    } else {
        uint32_t idx;
        for ( idx = 0; 0 == rc && idx < count; ++idx ) {
            const char * dirname;
            rc = VNameListGet( dirs, idx, &dirname );
            if ( 0 != rc ) {
                ErrMsg( "delete_dirs().VNameListGet( #%d ) -> %R", idx, rc );
            } else if ( dir_exists( dir, "%s", dirname ) ) {
                rc = KDirectoryClearDir ( dir, true, "%s", dirname );
                if ( 0 != rc ) {
                    ErrMsg( "delete_dirs().KDirectoryClearDir( %s ) -> %R", dirname, rc );
                } else {
                    rc = KDirectoryRemove ( dir, true, "%s", dirname );
                    if ( 0 != rc ) {
                        ErrMsg( "delete_dirs().KDirectoryRemove( %s ) -> %R", dirname, rc );
                    } else if ( details ) {
                        InfoMsg( "dir deleted: '%s'", dirname );
                    }
                }
            }
        }
    }
    return rc;
}

uint64_t file_size( const KDirectory * dir, const char * fmt, ... ) {
    uint64_t res = 0;
    if ( NULL != dir && NULL != fmt ) {
        va_list args;
        va_start( args, fmt );
        KDirectoryVFileSize( dir, &res, fmt, args );
        va_end( args );
    }
    return res;
}

uint64_t total_size_of_files_in_list( KDirectory * dir, const VNamelist * files ) {
    uint64_t res = 0;
    if ( NULL != dir && NULL != files ) {
        uint32_t idx, count;
        rc_t rc = VNameListCount( files, &count );
        if ( 0 != rc ) {
            ErrMsg( "total_size_of_files_in_list().VNameListCount() -> %R", rc );
        } else {
            for ( idx = 0; 0 == rc && idx < count; ++idx ) {
                const char * filename;
                rc = VNameListGet( files, idx, &filename );
                if ( 0 != rc ) {
                    ErrMsg( "total_size_of_files_in_list().VNameListGet( #%d ) -> %R", idx, rc );
                } else {
                    uint64_t size;
                    rc_t rc1 = KDirectoryFileSize( dir, &size, "%s", filename );
                    if ( 0 != rc1 ) {
                        ErrMsg( "total_size_of_files_in_list().KDirectoryFileSize( %s ) -> %R", filename, rc );
                    } else {
                        res += size;
                    }
                }
            }
        }
    }
    return res;
}

rc_t make_buffered_for_read( KDirectory * dir, const struct KFile ** f,
                             const char * filename, size_t buf_size ) {
    const struct KFile * fr;
    rc_t rc = KDirectoryOpenFileRead( dir, &fr, "%s", filename );
    if ( 0 != rc ) {
        ErrMsg( "make_buffered_for_read().KDirectoryOpenFileRead( '%s' ) -> %R", filename, rc );
    } else {
        if ( buf_size > 0 ) {
            const struct KFile * fb;
            rc = KBufFileMakeRead( &fb, fr, buf_size );
            if ( 0 != rc ) {
                ErrMsg( "make_buffered_for_read( '%s' ).KBufFileMakeRead() -> %R", filename, rc );
            } else {
                rc = release_file( fr, "make_buffered_for_read( '%s' ).1", filename );
                if ( 0 == rc ) { fr = fb; }
            }
        }
        if ( 0 == rc ) {
            *f = fr;
        } else {
            release_file( fr, "make_buffered_for_read( '%s' ).2", filename );
        }
    }
    return rc;
}

rc_t release_file( const struct KFile * f, const char * err_msg, ... ) {
    rc_t rc = KFileRelease( f );
    if ( 0 != rc ) {
        char buffer[ 4096 ];
        size_t num_writ;
        va_list list;
        rc_t rc2;

        va_start( list, err_msg );
        rc2 = string_vprintf( buffer, sizeof buffer, &num_writ, err_msg, list );
        if ( 0 == rc2 ) {
            PLOGERR( klogErr, ( klogErr, rc, "$(E) . KFileRelease()", "E=%s", buffer ) );
        }
        va_end( list );
    }
    return rc;
}

rc_t wrap_file_in_buffer( struct KFile ** f, size_t buffer_size, const char * err_msg ) {
    struct KFile * temp_file = *f;
    rc_t rc = KBufFileMakeWrite( &temp_file, *f, false, buffer_size );
    if ( 0 != rc ) {
        ErrMsg( "%s KBufFileMakeWrite() -> %R", err_msg, rc );
    } else {
        rc = release_file( *f, err_msg );
        if ( 0 == rc ) { *f = temp_file; }
    }
    return rc;
}

static rc_t available_space_dir_space( const KDirectory * dir, size_t * res ) {
    uint64_t free_space, total_space;
    rc_t rc = KDirectoryGetDiskFreeSpace( dir, &free_space, &total_space );
    if ( 0 != rc ) {
        ErrMsg( "available_space_dir_space.KDirectoryGetDiskFreeSpace() -> %R", rc );
    } else {
        *res = free_space;
    }
    return rc;
}

static rc_t extract_path_part( const char * p, char * dst, size_t dst_size ) {
    rc_t rc;
    String S_in, S_path, S_leaf;

    StringInitCString( &S_in, p );
    rc = split_string_r( &S_in, &S_path, &S_leaf, '/' ); /* helper.c */
    if ( 0 == rc ) {
        size_t l = string_copy( dst, dst_size, S_path . addr, S_path . size );
        if ( l < dst_size - 2 ) {
            dst[ l ] = '/';
            dst[ l + 1 ] = 0;
        } else {
            rc = RC( rcApp, rcArgv, rcAccessing, rcParam, rcInvalid );
            ErrMsg( "extract_path_part.KDirectoryOpenDirRead( '%s' ) -> %R", p, rc );
        }
    }
    return rc;
}

rc_t available_space_disk_space( const KDirectory * dir, const char * path, size_t * res, bool is_file ) {
    rc_t rc;
    const KDirectory * sub = NULL;
    if ( is_file ) {
        char tmp[ 4096 ];
        rc = extract_path_part( path, tmp, sizeof tmp );
        if ( 0 == rc ) {
            rc = KDirectoryOpenDirRead( dir, &sub, false, "%s", tmp );
            if ( 0 != rc ) {
                ErrMsg( "available_space_disk_space.KDirectoryOpenDirRead( '%s' ).1 -> %R", tmp, rc );
            }
        }
    } else {
        rc = KDirectoryOpenDirRead( dir, &sub, false, "%s", path );
        if ( 0 != rc ) {
            ErrMsg( "available_space_disk_space.KDirectoryOpenDirRead( '%s' ).2 -> %R", path, rc );
        }
    }
    if ( 0 == rc && NULL != sub ) {
        rc = available_space_dir_space( sub, res );
        {
            rc_t rc2 = KDirectoryRelease( sub );
            if ( 0 != rc2 ) {
                ErrMsg( "available_space_disk_space.KDirectoryRelease( '%s' ) -> %R", path, rc );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}
