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

#ifndef _h_file_printer_
#define _h_file_printer_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_kfs_file_
#include <kfs/file.h>
#endif

struct file_printer_t;

void destroy_file_printer( struct file_printer_t * printer );

rc_t make_file_printer_from_file( struct KFile * f, struct file_printer_t ** printer,
                size_t print_buffer_size );

rc_t make_file_printer_from_filename( const struct KDirectory * dir, struct file_printer_t ** printer,
                size_t file_buffer_size, size_t print_buffer_size,
                const char * fmt, ... );

rc_t file_print( struct file_printer_t * printer, const char * fmt, ... );


#ifdef __cplusplus
}
#endif

#endif
