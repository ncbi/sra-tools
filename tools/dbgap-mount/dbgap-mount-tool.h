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

#ifndef _mount_tool_h_
#define _mount_tool_h_

#include <xfs/xfs-defs.h>

#ifdef __cplusplus 
extern "C" {
#endif /* __cplusplus */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*)))
 |||    Log init/dispose
(((*/

XFS_EXTERN rc_t CC WrapIt (
                        struct Args * TheArgs,
                        rc_t ( CC * runner ) ( struct Args * TheArgs )
                        );

/*)))
  \\\   Argumends ... raznye
  (((*/
#define OPT_DAEMONIZE   "daemonize"
#define ALS_DAEMONIZE   "d"
#define PRM_DAEMONIZE   NULL
static const char * UsgDaemonize [] = { "Run tool as a daemon", NULL };

#define OPT_LOGFILE     "log-file"
#define ALS_LOGFILE     "l"
#define PRM_LOGFILE     "log-file-path"
static const char * UsgLogFile []   = { "Log file", NULL };

#define OPT_UNMOUNT     "unmount"
#define ALS_UNMOUNT     "u"
#define PRM_UNMOUNT     "mount-point"
static const char * UsgUnmount []   = { "Unmount", NULL };

#define PARAM_RO        "ro"
#define PARAM_RW        "rw"

#define OPT_ACCCTRL     "accctrl"
#define ALS_ACCCTRL     "a"
#define PRM_ACCCTRL     NULL
static const char * UsgAccCtrl [] = { "Control user access", NULL };

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _mount_tool_h_ */
