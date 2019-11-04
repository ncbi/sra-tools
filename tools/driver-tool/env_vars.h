/* ===========================================================================
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
 * Project:
 *  sratools command line tool
 *
 * Purpose:
 *  Define environment variable names (shared with library)
 *
 */

/* Tool Session ID */
/*   Also defined in ncbi-vdb/libs/kns/manager.c */
#define ENV_VAR_SESSION_ID "VDB_SESSION_ID"

/* Compute Environment Token (aka CE Token) */
/*   Also defined in ncbi-vdb/interfaces/klib/strings.h */
#define ENV_VAR_CE_TOKEN "VDB_CE_TOKEN"


/* MARK: .sra file */

/* Remote location of .sra file */
#define ENV_VAR_REMOTE_URL "VDB_REMOTE_URL"

/* Local path of .sra file */
#define ENV_VAR_LOCAL_URL "VDB_LOCAL_URL"

/* Local path for caching .sra file */
#define ENV_VAR_CACHE_URL "VDB_CACHE_URL"

/* Size of .sra file */
#define ENV_VAR_SIZE_URL "VDB_SIZE_URL"

/* CE Token is required for .sra file if set to '1' */
#define ENV_VAR_REMOTE_NEED_CE "VDB_REMOTE_NEED_CE"

/* Payment is required for remote .sra file if set to '1' */
#define ENV_VAR_REMOTE_NEED_PMT "VDB_REMOTE_NEED_PMT"


/* MARK: .vdbcache file */
/* INFO: These are set ONLY if there is .vdbcache file */

/* Remote location of .vdbcache file */
#define ENV_VAR_REMOTE_VDBCACHE "VDB_REMOTE_VDBCACHE"

/* Local path of .vdbcache file */
#define ENV_VAR_LOCAL_VDBCACHE "VDB_LOCAL_VDBCACHE"

/* Local path for caching .vdbcache file */
#define ENV_VAR_CACHE_VDBCACHE "VDB_CACHE_VDBCACHE"

/* Size of .vdbcache file */
#define ENV_VAR_SIZE_VDBCACHE "VDB_SIZE_VDBCACHE"

/* CE Token is required for remote .vdbcache file if set to '1' */
#define ENV_VAR_CACHE_NEED_CE "VDB_CACHE_NEED_CE"

/* Payment is required for remote .vdbcache file if set to '1' */
#define ENV_VAR_CACHE_NEED_PMT "VDB_CACHE_NEED_PMT"
