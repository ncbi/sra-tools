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

#ifndef _Included_py_ngs_itf_ReferenceItf
#define _Included_py_ngs_itf_ReferenceItf
#ifdef __cplusplus
extern "C" {
#endif

#include "py_ngs_defs.h"

LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetCommonName              ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetCanonicalName           ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetIsCircular              ( void* pRef, int* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetIsLocal                 ( void* pRef, int* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetLength                  ( void* pRef, uint64_t* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetReferenceBases          ( void* pRef, uint64_t offset, uint64_t length, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetReferenceChunk          ( void* pRef, uint64_t offset, uint64_t length, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetAlignment               ( void* pRef, char const* alignmentId, void** pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetAlignments              ( void* pRef, uint32_t categories, void** pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetAlignmentSlice          ( void* pRef, int64_t start, uint64_t length, uint32_t categories, void** pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetFilteredAlignmentSlice  ( void* pRef, int64_t start, uint64_t length, uint32_t categories, uint32_t filters, int32_t map_qual, void** pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetPileups                 ( void* pRef, uint32_t categories, void** pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetFilteredPileups         ( void* pRef, uint32_t categories, uint32_t filters, int32_t map_qual, void** pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetPileupSlice             ( void* pRef, int64_t start, uint64_t length, uint32_t categories, void** pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_ReferenceGetFilteredPileupSlice     ( void* pRef, int64_t start, uint64_t length, uint32_t categories, uint32_t filters, int32_t map_qual, void** pRet, void** ppNGSStrError );

#ifdef __cplusplus
}
#endif
#endif /* _Included_py_ngs_itf_ReferenceItf */
