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
#ifndef _h_search_debug_
#define _h_search_debug_

#include <klib/debug.h>

#if _DEBUGGING
#define SEARCH_DBGF(msg) DBGMSG(DBG_SEARCH, DBG_FLAG(DBG_SEARCH_METHOD), msg)
#define SEARCH_DBG(fmt, ...) SEARCH_DBGF(("%s:%u: " fmt "\n", __func__, __LINE__, __VA_ARGS__))
#define SEARCH_DBGERR(rc) SEARCH_DBGF(("%s:%u: %R\n", __func__, __LINE__, rc))
#define SEARCH_DBGERRP(fmt, rc, ...) SEARCH_DBGF(("%s:%u: %R " fmt "\n", __func__, __LINE__, rc, __VA_ARGS__))
#else
#define SEARCH_DBG(fmt, ...) ((void)0)
#define SEARCH_DBGERR(rc) ((void)0)
#define SEARCH_DBGERRP(fmt, rc, ...) ((void)0)
#define SEARCH_DBGF(msg) ((void)0)
#endif

#endif /* _h_search_debug_ */
