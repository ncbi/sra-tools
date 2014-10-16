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
#ifndef _services_sra_fuser_debug_h_
#define _services_sra_fuser_debug_h_

#include <klib/debug.h>

#if _DEBUGGING
#define DEBUG_MSG_PASTE(a,b) a##b
#define DEBUG_MSG(flag,msg) DBGMSG(DBG_APP,DBG_FLAG(DEBUG_MSG_PASTE(DBG_APP_,flag)), msg)
#define DEBUG_LINE(flag,fmt,...) DEBUG_MSG(flag, ("%s:%u: " fmt "\n", __func__, __LINE__, __VA_ARGS__))

#define MALLOC(ptr, size) ptr = malloc(size); DEBUG_LINE(10, "%p=malloc(%lu)", ptr, size)
#define CALLOC(ptr, qty, size) ptr = calloc(qty, size); DEBUG_LINE(10, "%p=calloc(%lu)", ptr, qty * size)
#define REALLOC(ptr, src, size) ptr = realloc(src, size); DEBUG_LINE(10, "%p=realloc(%p, %lu)", ptr, src, size)
#define FREE(ptr) free(ptr); if(ptr){DEBUG_LINE(10, "0=free(%p)", ptr);}

#else

#define DEBUG_MSG(flag,msg) ((void)0)
#define DEBUG_LINE(flag,fmt,...) ((void)0)

#define MALLOC(ptr, size) ptr = malloc(size)
#define CALLOC(ptr, qty, size) ptr = calloc(qty, size)
#define REALLOC(ptr, src, size) ptr = realloc(src, size)
#define FREE(ptr) free(ptr)

#endif

#endif /* _services_sra_fuser_debug_h_ */
