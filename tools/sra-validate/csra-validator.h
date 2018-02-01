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

#ifndef _h_csra_validator_
#define _h_csra_validator_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_cmn_iter_
#include "cmn-iter.h"
#endif

#ifndef _h_logger_
#include "logger.h"
#endif

#ifndef _h_result_
#include "result.h"
#endif

#ifndef _h_thread_runner_
#include "thread-runner.h"
#endif

#ifndef _h_progress_
#include "progress.h"
#endif

rc_t run_csra_validator( const KDirectory * dir,
                         const acc_info_t * acc_info,
                         struct logger * log,
                         struct validate_result * v_res,
                         struct thread_runner * threads,
                         struct progress * progress,
                         uint32_t num_slices,
                         size_t cursor_cache );

#ifdef __cplusplus
}
#endif

#endif