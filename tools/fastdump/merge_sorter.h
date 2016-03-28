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

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

struct merge_sorter;

typedef struct merge_sorter_params
{
    KDirectory *dir;
    const char * output_filename;
    const char * index_filename;
    uint32_t count;
    size_t buf_size;
} merge_sorter_params;


rc_t make_merge_sorter( struct merge_sorter ** ms, const merge_sorter_params * params );

rc_t add_merge_sorter_src( struct merge_sorter *ms, const char * filename, uint32_t id );

void release_merge_sorter( struct merge_sorter *ms );

rc_t run_merge_sorter( struct merge_sorter *ms );

#ifdef __cplusplus
}
#endif

#endif
