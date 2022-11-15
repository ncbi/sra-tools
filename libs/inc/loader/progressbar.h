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
#ifndef _h_kapp_progressbar_
#define _h_kapp_progressbar_

#ifndef _h_kapp_extern_
#include <kapp/extern.h>
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct KFile;
struct KDirectory;

typedef struct KLoadProgressbar KLoadProgressbar;

/**
   Create new object in job
 */
KAPP_EXTERN rc_t CC KLoadProgressbar_Make(const KLoadProgressbar** cself, uint64_t size);
/**
   if dir is NULL current directory assumed
 */
KAPP_EXTERN rc_t CC KLoadProgressbar_File(const KLoadProgressbar** cself, const char* filename, struct KDirectory const* dir);
KAPP_EXTERN rc_t CC KLoadProgressbar_KFile(const KLoadProgressbar** cself, struct KFile const* file);

/**
  Release job object
  if exclude than job stats excluded from reports
  */
KAPP_EXTERN void CC KLoadProgressbar_Release(const KLoadProgressbar* cself, bool exclude);

/**
    Add a chunk of smth (bytes, rows, etc) to the job
 */
KAPP_EXTERN rc_t CC KLoadProgressbar_Append(const KLoadProgressbar* cself, uint64_t chunk);


/* Set severity level name
   severity [IN] - default 'status'
 */
KAPP_EXTERN rc_t CC KLoadProgressbar_Severity(const char* severity);

/* mark a chunk of bytes as processed
   report on full percent processed or if forced
 */
KAPP_EXTERN rc_t CC KLoadProgressbar_Process(const KLoadProgressbar* cself, uint64_t chunk, bool force_report);

#ifdef __cplusplus
}
#endif

#endif /* _h_kapp_progressbar_ */
