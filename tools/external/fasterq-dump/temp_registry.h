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
#ifndef _h_temp_registry_
#define _h_temp_registry_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_fastdump_cleanup_task_
#include "cleanup_task.h"
#endif

struct temp_registry_t;

void destroy_temp_registry( struct temp_registry_t * self );

rc_t make_temp_registry( struct temp_registry_t ** registry,
                         struct CleanupTask_t * cleanup_task,
                         bool keep_tmp_files );

rc_t register_temp_file( struct temp_registry_t * self, uint32_t read_id, const char * filename );

rc_t temp_registry_merge( struct temp_registry_t * self,
                          KDirectory * dir,
                          const char * output_filename,
                          size_t buf_size,
                          bool show_progress,
                          bool force,
                          bool append );

rc_t temp_registry_to_stdout( struct temp_registry_t * self,
                              KDirectory * dir,
                              size_t buf_size,
                              bool keep_tmp_files );

#ifdef __cplusplus
}
#endif

#endif
