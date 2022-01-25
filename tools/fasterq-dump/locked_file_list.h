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

#ifndef _h_locked_file_list_
#define _h_locked_file_list_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#ifndef _h_kproc_lock_
#include <kproc/lock.h>
#endif

typedef struct locked_file_list
{
    KLock * lock;
    VNamelist * files;
} locked_file_list_t;

rc_t locked_file_list_init( locked_file_list_t * self, uint32_t alloc_blocksize );
rc_t locked_file_list_release( locked_file_list_t * self, KDirectory * dir, bool details );
rc_t locked_file_list_append( const locked_file_list_t * self, const char * filename );
rc_t locked_file_list_delete_files( KDirectory * dir, locked_file_list_t * self, bool details );
rc_t locked_file_list_delete_dirs( KDirectory * dir, locked_file_list_t * self, bool details );
rc_t locked_file_list_count( const locked_file_list_t * self, uint32_t * count );
rc_t locked_file_list_pop( locked_file_list_t * self, const String ** item );

#ifdef __cplusplus
}
#endif

#endif
