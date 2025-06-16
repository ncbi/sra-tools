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

#ifndef _h_sorter_
#define _h_sorter_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_cmn_iter_
#include "cmn_iter.h"
#endif

#ifndef _h_merge_sorter_
#include "merge_sorter.h"
#endif

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif

typedef struct lookup_production_args_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * accession_path;
    const char * accession_short;
    struct background_vector_merger_t * merger; /*merge_sorter.h */
    uint64_t align_row_count;
    size_t cursor_cache;
    size_t buf_size;
    size_t mem_limit;
    uint32_t num_threads;
    bool show_progress;
    bool keep_tmp_files;
} lookup_production_args_t;

rc_t execute_lookup_production( const lookup_production_args_t * args );

#ifdef __cplusplus
}
#endif

#endif
