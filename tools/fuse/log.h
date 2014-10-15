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
#ifndef _h_services_sra_fuser_log_
#define _h_services_sra_fuser_log_

#include <klib/rc.h>
#include <klib/log.h>

#include "debug.h"

#define ReleaseComplain(release, obj) \
    {{ \
       rc_t t_rc_ = release(obj); \
       if( t_rc_ != 0 ) { \
       PLOGERR(klogWarn, (klogWarn, t_rc_, "$(file):$(line):$(func): " #release "(" #obj ")", \
                                    "file=%s,line=%u,func=%s", __FILE__, __LINE__, __func__)); \
       } \
    }}

rc_t StrDup(const char* src, char** dst);

/*
 * Set path to logfile and optional (0) log reopen thread timeout
 * if path is NULL (re)starts watch thread if needed (sync value ignored)
 */
rc_t LogFile_Init(const char* path, unsigned int sync, bool foreground, int* log_fd);

void LogFile_Fini(void);

#endif /* _h_services_sra_fuser_log_ */
