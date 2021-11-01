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

#ifndef _h_tbl_join_
#define _h_tbl_join_

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

#ifndef _h_temp_dir_
#include "temp_dir.h"
#endif

#ifndef _h_temp_registry_
#include "temp_registry.h"
#endif

typedef struct execute_tbl_join_args_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * accession_path;
    const char * accession_short;
    const char * seq_defline;           /* NULL for default */
    const char * qual_defline;          /* NULL for default */
    const char * tbl_name;
    join_stats_t * stats;                   /* helper.h */
    const join_options_t * join_options;    /* helper.h */
    const struct temp_dir_t * temp_dir;     /* temp_dir.h */
    struct temp_registry_t * registry;      /* temp_registry.h */
    size_t cursor_cache;
    size_t buf_size;
    uint32_t num_threads;
    uint64_t row_limit;
    bool show_progress;
    format_t fmt;                       /* helper.h */
} execute_tbl_join_args_t;

rc_t execute_tbl_join( const execute_tbl_join_args_t * args );

typedef struct execute_fasta_tbl_join_args_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * accession_path;
    const char * accession_short;
    const char * output_filename;
    const char * seq_defline;           /* NULL for default */
    const char * tbl_name;
    join_stats_t * stats;                   /* helper.h */
    const join_options_t * join_options;    /* helper.h */
    size_t cursor_cache;
    size_t buf_size;
    uint32_t num_threads;
    uint64_t row_limit;
    bool show_progress;
    bool force;
} execute_fasta_tbl_join_args_t;

rc_t execute_unsorted_fasta_tbl_join( const execute_fasta_tbl_join_args_t * args );

#ifdef __cplusplus
}
#endif

#endif
