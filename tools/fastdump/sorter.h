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

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

struct sorter;

void release_sorter( struct sorter * sorter );

typedef struct sorter_params
{
    KDirectory * dir;
    const char * output_filename;
    const char * temp_path;
    uint64_t store_limit;
    size_t buf_size;
    bool use_subfiles;
} sorter_params;

rc_t make_sorter( sorter_params * params, struct sorter ** sorter );

rc_t write_to_sorter( struct sorter * sorter, int64_t seq_spot_id, uint32_t seq_read_id,
        const String * unpacked_bases );

rc_t finalize_sorter( struct sorter * sorter );

#ifdef __cplusplus
}
#endif

#endif
