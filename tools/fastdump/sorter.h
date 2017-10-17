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

#ifndef _h_atomic_
#include <atomic.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_cmn_iter_
#include "cmn_iter.h"
#endif

typedef struct lookup_production_params
{
    KDirectory * dir;
    const char * accession;
    cmn_params * cmn;                               /* cmn_iter.h */
    struct background_vector_merger * merger;       /* merge_sorter.h */
    size_t buf_size, mem_limit, num_threads;
} lookup_production_params;

rc_t execute_lookup_production( const lookup_production_params * lp );

#ifdef __cplusplus
}
#endif

#endif
