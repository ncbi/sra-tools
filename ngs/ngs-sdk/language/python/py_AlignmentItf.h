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

#ifndef _Included_py_ngs_itf_AlignmentItf
#define _Included_py_ngs_itf_AlignmentItf
#ifdef __cplusplus
extern "C" {
#endif

#include "py_ngs_defs.h"

/* TODO: it looks like CAST-functions are not actually needed, they are implemented on the python level, see ReadItf as well */
/*LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetFragmentId                ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetFragmentBases             ( void* pRef, uint64_t offset, uint64_t length, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetFragmentQualities         ( void* pRef, uint64_t offset, uint64_t length, void** ppNGSStringBuf, void** ppNGSStrError );
*/

LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetAlignmentId               ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetReferenceSpec             ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetMappingQuality            ( void* pRef, int32_t* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetReferenceBases            ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetReadGroup                 ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetReadId                    ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetClippedFragmentBases      ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetClippedFragmentQualities  ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetAlignedFragmentBases      ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetAlignmentCategory         ( void* pRef, uint32_t* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetAlignmentPosition         ( void* pRef, int64_t* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetAlignmentLength           ( void* pRef, uint64_t* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetIsReversedOrientation     ( void* pRef, int* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetSoftClip                  ( void* pRef, uint32_t edge, int32_t* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetTemplateLength            ( void* pRef, uint64_t* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetShortCigar                ( void* pRef, int clipped, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetLongCigar                 ( void* pRef, int clipped, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetRNAOrientation            ( void* pRef, char* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentHasMate                      ( void* pRef, int* pRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetMateAlignmentId           ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetMateAlignment             ( void* pRef, void** ppRet, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetMateReferenceSpec         ( void* pRef, void** ppNGSStringBuf, void** ppNGSStrError );
LIB_EXPORT PY_RES_TYPE PY_NGS_AlignmentGetMateIsReversedOrientation ( void* pRef, int* pRet, void** ppNGSStrError );


#ifdef __cplusplus
}
#endif
#endif /* _Included_py_ngs_itf_AlignmentItf */
