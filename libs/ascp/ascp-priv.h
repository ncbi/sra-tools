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

#ifndef _h_libs_ascp_ascp_priv_
#define _h_libs_ascp_ascp_priv_

#include <ascp/ascp.h> /* EAscpState */
#include <klib/text.h> /* String */

#ifdef __cplusplus
extern "C" {
#endif

#define STS_INFO 1
#define STS_DBG 2

typedef enum {
      eUnknown
    , eChild
    , eStart
    , eLog
    , eKeyStart
    , eKeyMayBeIn
    , eKeyIn
    , eKeyEnd
    , eProgress
    , eWriteFailed
    , eFailed
    , eFailedAuthenticate
    , eFailedInitiation
    , eCompleted
    , eEnd
} EAscpState;

rc_t ascpParse(const char *buf, size_t len, const char *filename,
    EAscpState *state, String *line);

bool ascp_path(const char **cmd, const char **key);

rc_t run_ascp(const char *ascp_bin, const char *private_file,
    const char *src, const char *dest, const AscpOptions *opt);

rc_t mkAscpCmd(const char *ascp_bin, const char *private_file,
    const char *src, const char *dest, const AscpOptions *opt,
    char *const argv[], size_t argvSz);

int silent_system(const char *command);

#ifdef __cplusplus
}
#endif

#endif
