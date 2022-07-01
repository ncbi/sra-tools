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

#ifndef _h_kns_ascp_
#define _h_kns_ascp_

#ifndef _h_kns_extern_
#include <kns/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    eAscpStateRunning,
    eAscpStateExitSuccess,
    eAscpStateExitWriteFailure,
    eAscpStateExitFailure
};

typedef rc_t TQuitting ( void );
typedef bool TProgress ( uint64_t id,
    uint64_t state, uint64_t size, uint64_t percentage );

typedef struct AscpOptions AscpOptions;
struct AscpOptions
{
    uint64_t src_size;

    uint64_t heartbeat;       /* in milliseconds */

    uint64_t id; /* to pass to the callback */

    const char *host;
    const char *user;
    char target_rate[512];
       /* -l MAX-RATE Set the target transfer rate in Kbps */

    const char *ascp_options; /* any arbitrary options to pass to ascp */

/* progress logging */
    const char *name;

    TProgress *callback;

    TQuitting *quitting;

    bool status; /* whether to call STSMSG */

    bool cache_key; /* Add the server's host key to PuTTY's cache */

    bool disabled; /* output parameter for aspera_options */

    bool dryRun; /* Dry run: don't download, only print command */
};

/**  status - whether to print STSMSG(1-2) - information messages
    ascp_bin and private_file should be freed by the caller */
KNS_EXTERN rc_t CC ascp_locate ( const char **ascp_bin, const char **private_file,
    bool use_config, bool status );

/** Get a file by running aspera ascp binary */
KNS_EXTERN rc_t CC aspera_get ( const char *ascp_bin, const char *private_file,
    const char *src, const char *dest, AscpOptions *opt );

/** Fill AscpOptions members initialized by ascp library */
KNS_EXTERN rc_t CC aspera_options ( AscpOptions *opt );


#ifdef __cplusplus
}
#endif

#endif
