/*==============================================================================
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
#ifndef _h_kapp_loader_meta_
#define _h_kapp_loader_meta_

#ifndef _h_kapp_extern_
#include <kapp/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct KMDataNode;

KAPP_EXTERN rc_t CC KLoaderMeta_Write(struct KMDataNode* root,
                                      const char* argv0, const char* argv0_date,
                                      const char* app_name, ver_t app_version);

KAPP_EXTERN rc_t CC KLoaderMeta_WriteWithVersion(struct KMDataNode* root,
                                                 const char* argv0, const char* argv0_date, ver_t argv0_version,
                                                 const char* app_name, ver_t app_version);

#ifdef __cplusplus
}
#endif

#endif /* _h_kapp_loader_meta_*/
