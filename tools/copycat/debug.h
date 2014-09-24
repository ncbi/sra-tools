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


#ifndef _h_copycat_debug_
#define _h_copycat_debug_

#include <klib/debug.h>

#ifndef _h_kfs_impl_
#include <kfs/impl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_MSG_PASTE(a,b) a##b
#define DEBUG_MSG(flag,msg) DBGMSG(DBG_APP,DBG_FLAG(DEBUG_MSG_PASTE(DBG_APP_,flag)), msg)
#define DEBUG_ENTRY() DEBUG_MSG(10,("Enter: %s\n", __func__))

/*--------------------------------------------------------------------------
 * application debug stuff
 */

/* redefine some application flags to be more specialized rather than just numbers */
#define DBG_CC          DBG_APP
#define DBG_CC_FILE     DBG_APP_1
#define DBG_CC_CCTREE   DBG_APP_2

#define DBG_KFILE(msg)  DBGMSG (DBG_CC, DBG_FLAG(DBG_CC_FILE), msg)

#if _DEBUGGING
#define DBG_KFile(p)    \
    { DBG_KFILE(("%s: KFile * %p\n", __func__, (p)));   \
        if (p) { \
            DBG_KFILE(("  vt:            %p\n",(p)->vt)); \
            DBG_KFILE(("  dir:            %p\n",(p)->dir)); \
            DBG_KFILE(("  ref:           %u\n",*(unsigned*)&(p)->refcount));  \
            DBG_KFILE(("  read_enabled:  %u\n",*(unsigned*)&(p)->read_enabled)); \
            DBG_KFILE(("  write_enabled: %u\n",*(unsigned*)&(p)->write_enabled));}}
#else
#define DBG_KFile(p) 
#endif



#ifdef __cplusplus
}
#endif

#endif /* _h_copycat_debug_ */
