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

#ifndef _h_tool_ctx_
#define _h_tool_ctx_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_inspector_
#include "inspector.h"
#endif

#ifndef _h_cmn_iter_
#include "cmn_iter.h"
#endif
    
#define DFLT_PATH_LEN 4096

typedef struct tool_ctx_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;     /* created, but unused to avoid race-condition in threads */

    const char * requested_temp_path;
    const char * accession_path;
    const char * accession_short;
    const char * output_filename;
    const char * output_dirname;
    const char * requested_seq_tbl_name;
    const char * seq_defline;
    const char * qual_defline;

    VNamelist * ref_name_filter;

    struct temp_dir_t * temp_dir;

    char lookup_filename[ DFLT_PATH_LEN ];
    char index_filename[ DFLT_PATH_LEN ];
    char dflt_output[ DFLT_PATH_LEN ];

    struct CleanupTask_t * cleanup_task;

    size_t cursor_cache, buf_size, mem_limit;
    size_t estimated_output_size;
    size_t disk_limit_out_cmdl;
    size_t disk_limit_tmp_cmdl;
    size_t disk_limit_out_os;
    size_t disk_limit_tmp_os;

    uint32_t num_threads;
    uint32_t stop_after_step;
    uint64_t total_ram;
    uint64_t row_limit;

    format_t fmt; /* helper.h */
    check_mode_t check_mode; /* helper.h */

    bool force, show_progress, show_details, append, use_stdout, split_file;
    bool only_unaligned, only_aligned;
    bool out_and_tmp_on_same_fs;
    bool only_internal_refs;
    bool only_external_refs;
    bool use_name;
    bool keep_tmp_files;

    join_options_t join_options; /* helper.h */

    insp_input_t insp_input;       /* inspector.h */
    insp_output_t insp_output;     /* inspector.h */
} tool_ctx_t;

bool tctx_populate_cmn_iter_params( const tool_ctx_t * tool_ctx,
                                    cmn_iter_params_t * params );
    
rc_t tctx_populate_and_call_inspector( tool_ctx_t * tool_ctx );

rc_t tctx_release( const tool_ctx_t * tool_ctx, rc_t rc_in );

#ifdef __cplusplus
}
#endif

#endif
