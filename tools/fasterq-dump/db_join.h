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

#ifndef _h_db_join_
#define _h_db_join_

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

#ifndef _h_inspector_
#include "inspector.h"
#endif

typedef struct execute_db_join_args_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * accession_path;
    const char * accession_short;
    const char * seq_defline;           /* NULL for default */
    const char * qual_defline;          /* NULL for default */
    const char * lookup_filename;
    const char * index_filename;
    join_stats_t * stats;                   /* helper.h */
    const join_options_t * join_options;    /* helper.h */
    const inspector_output_t * insp_output; /* inspector.h */
    const struct temp_dir_t * temp_dir;
    struct temp_registry_t * registry;
    size_t cursor_cache;
    size_t buf_size;
    uint32_t num_threads;
    uint64_t row_limit;
    bool show_progress;
    format_t fmt;
} execute_db_join_args_t;

rc_t execute_db_join( const execute_db_join_args_t * args );

/*
rc_t check_lookup( const KDirectory * dir,
                   size_t buf_size,
                   size_t cursor_cache,
                   const char * lookup_filename,
                   const char * index_filename,
                   const char * accession_short,
                   const char * accession_path );

rc_t check_lookup_this( const KDirectory * dir,
                        size_t buf_size,
                        size_t cursor_cache,
                        const char * lookup_filename,
                        const char * index_filename,
                        uint64_t seq_spot_id,
                        uint32_t seq_read_id );
*/

typedef struct execute_unsorted_fasta_db_join_args_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * accession_short;           /* accession-name to be used for output-file/error-reports */
    const char * accession_path;            /* full path to accession for opening it */
    const char * output_filename;           /* NULL for stdout! */
    const char * seq_defline;               /* NULL for default, we need only seq-defline here ( FASTA!) */
    join_stats_t * stats;                   /* helper.h */
    const join_options_t * join_options;    /* helper.h */
    const inspector_output_t * insp_output; /* inspector.h */
    size_t cur_cache;                       /* size of cursor-cache for vdb-cursor */
    size_t buf_size;                        /* size of buffer-file for output-writing */
    uint32_t num_threads;                   /* how many threads to use */
    uint64_t row_limit;
    bool show_progress;                     /* display progressbar */
    bool force;                             /* overwrite output-file if it exists */
    bool only_unaligned;                    /* process only un-aligned reads */
    bool only_aligned;                      /* process only aligned reads */
} execute_unsorted_fasta_db_join_args_t;

rc_t execute_unsorted_fasta_db_join( const execute_unsorted_fasta_db_join_args_t * args );

#ifdef __cplusplus
}
#endif

#endif
