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

#ifndef _h_merge_sorter_
#define _h_merge_sorter_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_fastdump_cleanup_task_
#include "cleanup_task.h"
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_temp_dir_
#include "temp_dir.h"
#endif

#ifndef _h_progress_thread_
#include "progress_thread.h"
#endif

struct background_vector_merger_t;
struct background_file_merger_t;

/* ================================================================================= */

typedef struct vector_merger_args_t {
    KDirectory * dir;
    const struct temp_dir_t * temp_dir;
    struct CleanupTask_t * cleanup_task;
    struct background_file_merger_t * file_merger;
    uint32_t batch_size;
    uint32_t q_wait_time;
    size_t buf_size;
    struct bg_update_t * gap;
    bool details;
    bool keep_tmp_files;
} vector_merger_args_t;


rc_t make_background_vector_merger( struct background_vector_merger_t ** merger,
                                    vector_merger_args_t * args );

void tell_total_rowcount_to_vector_merger( struct background_vector_merger_t * self, uint64_t value );

rc_t push_to_background_vector_merger( struct background_vector_merger_t * self, KVector * store );

rc_t seal_background_vector_merger( struct background_vector_merger_t * self );

rc_t wait_for_and_release_background_vector_merger( struct background_vector_merger_t * self );


/* ================================================================================= */

typedef struct file_merger_args_t {
    KDirectory * dir;
    const struct temp_dir_t * temp_dir;
    struct CleanupTask_t * cleanup_task;
    const char * lookup_filename;
    const char * index_filename;
    uint32_t batch_size;
    uint32_t wait_time;
    size_t buf_size;
    struct bg_update_t * gap;
    bool details;
    bool keep_tmp_files;
} file_merger_args_t;

rc_t make_background_file_merger( struct background_file_merger_t ** merger,
                                  file_merger_args_t * args );

void tell_total_rowcount_to_file_merger( struct background_file_merger_t * self, uint64_t value );

rc_t push_to_background_file_merger( struct background_file_merger_t * self, const char * filename );

rc_t seal_background_file_merger( struct background_file_merger_t * self );

rc_t wait_for_and_release_background_file_merger( struct background_file_merger_t * self );

#ifdef __cplusplus
}
#endif

#endif
