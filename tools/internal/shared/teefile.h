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

#ifndef _h_kfs_teefile_
#define _h_kfs_teefile_

#ifndef _h_kfs_extern_
#include <kfs/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct KFile;

typedef struct KTeeFile KTeeFile;

/* -----
 * Copy can be a serialized type KFile for a KTeeFile opened for Read but
 * not when opened for write.
 *
 * Specifically this means copy can be a KMD5File for read but not for write
 * other KFile subtypes might have the same restriction.
 *
 * A seekless update KTeefile can be created but does not now exist.
 */
KFS_EXTERN rc_t CC KFileMakeTeeRead (const struct KFile ** self, 
                                     const struct KFile * original,
                                     struct KFile * copy);
KFS_EXTERN rc_t CC KFileMakeTeeUpdate (struct KFile ** self,
                                       struct KFile * original,
                                       struct KFile * copy);

#ifdef __cplusplus
}
#endif

#endif /* _h_kfs_teefile_ */
