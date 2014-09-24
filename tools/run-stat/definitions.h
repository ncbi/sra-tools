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

#ifndef _h_definitions_
#define _h_definitions_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#define DISP_RC(rc,err) (void)((rc == 0) ? 0 : LogErr( klogInt, rc, err ))

#define DISP_RC2(rc,err,succ) \
    (void)((rc != 0)? 0 : (succ) ? LOGMSG( klogInfo, succ ) : LogErr( klogInt, rc, err ))

#define RUN_STAT_COLS "READ,QUALITY,READ_LEN,READ_START,READ_TYPE"

#define SRA_PF_UNDEF 0
#define SRA_PF_454 1
#define SRA_PF_ILLUMINA 2
#define SRA_PF_ABSOLID 3
#define SRA_PF_COMPLETE_GENOMICS 4
#define SRA_PF_HELICOS 5
#define SRA_PF_UNKNOWN 6

#define SRA_READ_FILTER_PASS 0
#define SRA_READ_FILTER_REJECT 1
#define SRA_READ_FILTER_CRITERIA 2
#define SRA_READ_FILTER_REDACTED 3

/* report output formats */
#define RT_TXT 0
#define RT_CSV 1
#define RT_XML 2
#define RT_JSO 3

#define DEFAULT_REPORT_PREFIX "report"

#ifdef __cplusplus
}
#endif

#endif
