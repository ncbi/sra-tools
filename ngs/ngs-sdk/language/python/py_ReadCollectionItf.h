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

#ifndef _Included_py_ngs_itf_ReadCollectionItf
#define _Included_py_ngs_itf_ReadCollectionItf
#ifdef __cplusplus
extern "C" {
#endif

#include "py_ngs_defs.h"

LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetName           (void* pRef, void** ppNGSStringBuf, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetReadGroups     (void* pRef, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionHasReadGroup      (void* pRef, char const* spec, int* pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetReadGroup      (void* pRef, char const* spec, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetReferences     (void* pRef, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionHasReference      (void* pRef, char const* spec, int* pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetReference      (void* pRef, char const* spec, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetAlignment      (void* pRef, char const* alignmentId, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetAlignments     (void* pRef, uint32_t categories, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetAlignmentCount (void* pRef, uint32_t categories, uint64_t* pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetAlignmentRange (void* pRef, uint64_t first, uint64_t count, uint32_t categories, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetRead           (void* pRef, char const* readId, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetReads          (void* pRef, uint32_t categories, void** pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetReadCount      (void* pRef, uint32_t categories, uint64_t* pRet, void** ppNGSStrError);
LIB_EXPORT PY_RES_TYPE PY_NGS_ReadCollectionGetReadRange      (void* pRef, uint64_t first, uint64_t count, uint32_t categories, void** pRet, void** ppNGSStrError);

#ifdef __cplusplus
}
#endif
#endif /* _Included_py_ngs_itf_ReadCollectionItf */
